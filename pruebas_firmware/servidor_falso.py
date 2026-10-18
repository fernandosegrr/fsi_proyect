"""
servidor_falso.py - Servidor WebSocket FALSO que imita al carro real (solo para pruebas en PC).

GUIA DE LECTURA
 1. Escucha en 0.0.0.0:8080, ruta /ws (como el carro: servidor en el puerto 80, ruta /ws).
 2. Solo acepta los comandos del firmware: FORWARD, REVERSE, LEFT, RIGHT, STOP, EBRAKE, PING, MODO_MANUAL y MODO_AUTO
    (texto plano, sin parametros). Cualquier otro texto se imprime y se ignora.
 3. PING se contesta con PONG. OJO: el carro REAL no contesta nada; el PONG se agrego aqui
    para poder medir la latencia desde la app (ver docs/pruebas_control.md).
 4. Watchdog de 1000 ms como el firmware: si hay motores en marcha y no llega un comando real
    en 1000 ms, imprime "WATCHDOG: motores detenidos". PING NO renueva el reloj (igual que en el carro).
 4b. PERFILES de velocidad como el firmware: MODO_MANUAL se contesta con ACK_MANUAL y MODO_AUTO con ACK_AUTO. Si el carro se mueve y el perfil
    cambia, primero se detiene (imprime "STOP por cambio de perfil"). Al conectarse un cliente y tras el watchdog el perfil vuelve a AUTO.
 4c. BALANCE de los lados (equilibrio al ir recto), como el firmware: BAL_AUTO:NNN y BAL_MANUAL:NNN (800 a 1000) se contestan ACK_BAL_AUTO:NNN / ACK_BAL_MANUAL:NNN y se guardan (el
    servidor los conserva aunque el cliente se desconecte, como la memoria no volatil del carro); fuera de rango o mal escritos se contestan NAK_BAL; BAL? se contesta BAL:AAA:MMM.
 5. Un solo cliente a la vez: si se conecta otro, se cierra el anterior (como el carro).
 6. Cada mensaje se imprime con la hora y los ms desde el mensaje anterior.

 7. SENSORES (J11, opcional con --sensores): imita al firmware con el giroscopio y el laser. CAPS? -> CAPS:IMU=1,TOF=1; SENS:<angulo x10>,<mm>,<banderas> cada 100 ms con perfil AUTO; GIRAR:<grados> ->
    GIRO_OK:<objetivo>:<real x10>:<ms> (o GIRO_ERROR:angulo / sin_imu / ocupado; un STOP o un movimiento lo aborta con GIRO_ERROR:abortado); IMU?, TOF?, IMU_CERO, IMU_CAL (-> ACK_IMU_CAL y,
    un momento despues, IMU_CAL_OK:-47), TOF_USO:<0|1> (apaga o enciende el freno del laser) y TOF_PARO:<mm>. Para probar el laser sin objetos: el mensaje SIM_TOF:<mm> (solo de pruebas) fija la distancia que
    "ve" el laser; con FORWARD en AUTO y menos de 120 mm dos lecturas seguidas el servidor frena y manda TOF_PARO. --sin-giroscopio y --sin-laser hacen que CAPS? diga 0. SIN --sensores el servidor se
    porta como un firmware viejo: esos mensajes se tratan como desconocidos (sin respuesta).

Uso (desde la carpeta pruebas_firmware, con el entorno virtual local):
    .venv\\Scripts\\python.exe servidor_falso.py            (puerto 8080)
    .venv\\Scripts\\python.exe servidor_falso.py --puerto 9000
    .venv\\Scripts\\python.exe servidor_falso.py --sensores   (con giroscopio y laser)
"""

import argparse
import asyncio
import datetime
import http
import re
import time

from websockets.asyncio.server import serve

COMANDOS_VALIDOS = {"FORWARD", "REVERSE", "LEFT", "RIGHT", "STOP", "EBRAKE", "PING", "MODO_MANUAL", "MODO_AUTO"}
COMANDOS_MOVIMIENTO = {"FORWARD", "REVERSE", "LEFT", "RIGHT"}
BALANCE_MIN = 800          # igual que BALANCE_MIN_MIL y BALANCE_MAX_MIL de config.h
BALANCE_MAX = 1000
PATRON_BALANCE = re.compile(r"^BAL_(AUTO|MANUAL):([1-9][0-9]{2,3})$")   # solo cifras, 3 o 4, sin cero a la izquierda
RUTA = "/ws"
WATCHDOG_S = 1.0          # igual que TIEMPO_WATCHDOG_MS del firmware (1000 ms)
REVISION_WATCHDOG_S = 0.02

# Estado del "carro" falso (un solo hilo de asyncio: no hace falta protegerlo).
estado = {
    "cliente": None,             # la conexion activa
    "motores_en_marcha": False,
    "ultimo_comando_real": 0.0,  # reloj del ultimo comando que NO es PING
    "ultimo_mensaje": None,      # para los ms desde el mensaje anterior
    "perfil": "AUTO",            # "MANUAL" (255) o "AUTO" (210); siempre arranca y se reconecta en AUTO
    "sens": {"activo": False, "imu": True, "tof": True, "angulo_x10": 0, "sesgo_x100": -47, "mm": 1200, "tof_uso": True, "giro": None, "cerca": 0, "cal": None},   # sensores (J11), solo con --sensores
    "balance": {"AUTO": 986, "MANUAL": 988},   # factor del lado izquierdo de cada perfil (milesimas); como la memoria no volatil del carro, no se pierde al desconectar
}


def hora():
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


def imprimir(texto):
    print("%s  %s" % (hora(), texto), flush=True)


def ms_desde_anterior():
    """Milisegundos desde el mensaje anterior (texto vacio si es el primero)."""
    ahora = time.monotonic()
    anterior = estado["ultimo_mensaje"]
    estado["ultimo_mensaje"] = ahora
    if anterior is None:
        return "      -   "
    return "+%6d ms" % round((ahora - anterior) * 1000)


PATRON_GIRAR = re.compile(r"^GIRAR:([+-]?[0-9]{1,4})$")


async def mandar(conexion, texto):
    """Manda un mensaje y lo imprime (para ver en el servidor lo que contesta)."""
    imprimir("             -> %s" % texto)
    await conexion.send(texto)


def abortar_giro():
    """Un STOP, EBRAKE, otro movimiento o un cambio de perfil cortan un giro por angulo en curso."""
    giro = estado["sens"]["giro"]
    if giro is not None:
        giro.cancel()


async def hacer_giro(conexion, grados):
    """El giro por angulo: dura ~250 ms + 5.5 ms por grado; termina con el angulo real (1 % de mas) y GIRO_OK. Si lo cortan informa lo girado hasta ahi."""
    s = estado["sens"]
    total_ms = 250 + 5.5 * abs(grados)
    inicio = time.monotonic()
    estado["motores_en_marcha"] = True
    try:
        await asyncio.sleep(total_ms / 1000.0)
        ms = int((time.monotonic() - inicio) * 1000)
        real_x10 = int(grados * 10 * 1.01)
        s["angulo_x10"] += real_x10
        estado["motores_en_marcha"] = False
        await mandar(conexion, "GIRO_OK:%d:%d:%d" % (grados, real_x10, ms))
    except asyncio.CancelledError:
        ms = int((time.monotonic() - inicio) * 1000)
        real_x10 = int(grados * 10 * 1.01 * min(1.0, ms / total_ms))
        s["angulo_x10"] += real_x10
        estado["motores_en_marcha"] = False
        try:
            await mandar(conexion, "GIRO_ERROR:abortado:%d:%d:%d" % (grados, real_x10, ms))
        except Exception:
            pass
    finally:
        s["giro"] = None


async def procesar_sensores(conexion, mensaje):
    """Los mensajes del giroscopio y del laser (solo con --sensores). Devuelve True si el mensaje era de los sensores."""
    s = estado["sens"]
    if mensaje == "CAPS?":
        await mandar(conexion, "CAPS:IMU=%d,TOF=%d,TOFR=0" % (1 if s["imu"] else 0, 1 if s["tof"] else 0))
    elif mensaje == "ARR?":
        await mandar(conexion, "ARR:5,0,00000,0")   # los ultimos 5 arranques sin reintentos del laser
    elif mensaje == "IMU?":
        await mandar(conexion, "IMU:%d,%d,%d,104" % (s["angulo_x10"], s["sesgo_x100"], 2 if s["imu"] else 3))
    elif mensaje == "TOF?":
        await mandar(conexion, "TOF:%d,%d" % (s["mm"] if s["tof"] else 0, (1 if s["tof"] else 0) | (2 if s["tof"] and s["mm"] < 1200 else 0)))
    elif mensaje == "IMU_CERO":
        s["angulo_x10"] = 0
        await mandar(conexion, "ACK_IMU_CERO")
    elif mensaje == "IMU_CAL":
        if estado["motores_en_marcha"] or s["giro"] is not None or not s["imu"]:
            await mandar(conexion, "NAK_IMU_CAL")
        else:
            await mandar(conexion, "ACK_IMU_CAL")

            async def terminar():
                await asyncio.sleep(0.8)
                await mandar(conexion, "IMU_CAL_OK:%d" % s["sesgo_x100"])
            s["cal"] = asyncio.create_task(terminar())
    elif mensaje.startswith("TOF_USO:"):
        s["tof_uso"] = mensaje[8:] != "0"
        await mandar(conexion, "ACK_TOF_USO:%d" % (1 if s["tof_uso"] else 0))
    elif mensaje.startswith("SIM_TOF:"):          # solo de pruebas: la distancia que "ve" el laser
        s["mm"] = max(0, min(1200, int(mensaje[8:] or 1200)))
    elif mensaje.startswith("GIRAR:"):
        coincide = PATRON_GIRAR.match(mensaje)
        if coincide is None or not 5 <= abs(int(coincide.group(1))) <= 200:
            await mandar(conexion, "GIRO_ERROR:angulo")
        elif not s["imu"]:
            await mandar(conexion, "GIRO_ERROR:sin_imu")
        elif s["giro"] is not None or estado["motores_en_marcha"]:
            await mandar(conexion, "GIRO_ERROR:ocupado")
        else:
            estado["ultimo_comando_real"] = time.monotonic()
            s["giro"] = asyncio.create_task(hacer_giro(conexion, int(coincide.group(1))))
    else:
        return False
    return True


async def emisor_sens():
    """Cada 100 ms: SENS al cliente (con perfil AUTO) y el freno de emergencia del laser (2 lecturas seguidas menores de 120 mm, AUTO, avanzando)."""
    while True:
        await asyncio.sleep(0.1)
        s = estado["sens"]
        conexion = estado["cliente"]
        if not s["activo"] or conexion is None:
            continue
        try:
            if s["tof"] and s["tof_uso"] and estado["perfil"] == "AUTO" and estado["motores_en_marcha"] and estado.get("ultimo_movimiento") == "FORWARD":
                s["cerca"] = s["cerca"] + 1 if s["mm"] < 120 else 0
                if s["cerca"] >= 2:
                    s["cerca"] = 0
                    estado["motores_en_marcha"] = False
                    await mandar(conexion, "TOF_PARO:%d" % s["mm"])
            else:
                s["cerca"] = 0
            if estado["perfil"] == "AUTO":
                banderas = (1 if s["imu"] else 0) | (2 if s["tof"] else 0) | (4 if s["tof"] and s["mm"] < 1200 else 0) | (16 if s["giro"] is not None else 0)
                await conexion.send("SENS:%d,%d,%d" % (s["angulo_x10"] if s["imu"] else 0, s["mm"] if s["tof"] else 0, banderas))
        except Exception:
            pass


async def procesar(conexion, mensaje):
    """Interpreta un mensaje igual que el firmware."""
    if isinstance(mensaje, bytes):
        imprimir("%s  BINARIO ignorado (%d bytes)" % (ms_desde_anterior(), len(mensaje)))
        return

    intervalo = ms_desde_anterior()
    if estado["sens"]["activo"] and (mensaje in ("CAPS?", "ARR?", "IMU?", "TOF?", "IMU_CERO", "IMU_CAL") or mensaje.startswith(("TOF_USO:", "SIM_TOF:", "GIRAR:"))):
        imprimir("%s  %s" % (intervalo, mensaje))
        await procesar_sensores(conexion, mensaje)
        return
    if mensaje == "BAL?" or mensaje.startswith(("BAL_AUTO:", "BAL_MANUAL:")):
        imprimir("%s  %s" % (intervalo, mensaje))
        if mensaje == "BAL?":
            await conexion.send("BAL:%d:%d" % (estado["balance"]["AUTO"], estado["balance"]["MANUAL"]))
            return
        estado["ultimo_comando_real"] = time.monotonic()      # como cualquier comando real, renueva el watchdog (pero no mueve nada ni cambia el perfil)
        coincide = PATRON_BALANCE.match(mensaje)
        if coincide and BALANCE_MIN <= int(coincide.group(2)) <= BALANCE_MAX:
            estado["balance"][coincide.group(1)] = int(coincide.group(2))
            imprimir("             Balance %s: %s" % (coincide.group(1), coincide.group(2)))
            await conexion.send("ACK_BAL_%s:%s" % (coincide.group(1), coincide.group(2)))
        else:
            imprimir("             Balance no valido (NAK_BAL)")
            await conexion.send("NAK_BAL")
        return
    if mensaje not in COMANDOS_VALIDOS:
        # El firmware real guarda el desconocido y renueva el watchdog (pendiente, ver
        # docs/pendientes_firmware.md); aqui se ignora como pidio la prueba.
        imprimir("%s  DESCONOCIDO (ignorado): %r" % (intervalo, mensaje))
        return

    imprimir("%s  %s" % (intervalo, mensaje))
    if mensaje == "PING":
        await conexion.send("PONG")        # no renueva el watchdog
        return

    estado["ultimo_comando_real"] = time.monotonic()      # un MODO_* tambien cuenta como comando real (renueva el watchdog)
    if mensaje in COMANDOS_MOVIMIENTO or mensaje in ("STOP", "EBRAKE") or mensaje in ("MODO_MANUAL", "MODO_AUTO"):
        if mensaje not in ("MODO_MANUAL", "MODO_AUTO") or ("MANUAL" if mensaje == "MODO_MANUAL" else "AUTO") != estado["perfil"]:
            abortar_giro()
    if mensaje in ("MODO_MANUAL", "MODO_AUTO"):
        nuevo = "MANUAL" if mensaje == "MODO_MANUAL" else "AUTO"
        if nuevo != estado["perfil"]:
            if estado["motores_en_marcha"]:
                imprimir("             STOP por cambio de perfil")
                estado["motores_en_marcha"] = False
            estado["perfil"] = nuevo
            imprimir("             Perfil: %s" % nuevo)
        await conexion.send("ACK_" + nuevo)
        return
    estado["motores_en_marcha"] = mensaje in COMANDOS_MOVIMIENTO
    estado["ultimo_movimiento"] = mensaje


async def atender(conexion):
    """Atiende a un cliente hasta que se desconecta."""
    anterior = estado["cliente"]
    if anterior is not None:
        imprimir("Cliente nuevo: se cierra el anterior (el carro solo acepta uno)")
        await anterior.close(1000, "otro cliente")
    estado["cliente"] = conexion
    estado["motores_en_marcha"] = False     # el firmware hace hardStop al conectar
    estado["perfil"] = "AUTO"               # y deja el perfil AUTO
    estado["ultimo_mensaje"] = None
    imprimir("CONEXION desde %s:%s" % conexion.remote_address[:2])
    try:
        async for mensaje in conexion:
            await procesar(conexion, mensaje)
    finally:
        imprimir("DESCONEXION: motores detenidos")
        abortar_giro()
        if estado["cliente"] is conexion:
            estado["cliente"] = None
            estado["motores_en_marcha"] = False
            estado["perfil"] = "AUTO"


async def vigilar_watchdog():
    """Watchdog de software: igual que carro_vigilar_comunicacion() del firmware."""
    while True:
        await asyncio.sleep(REVISION_WATCHDOG_S)
        if estado["motores_en_marcha"] and estado["sens"]["giro"] is None and time.monotonic() - estado["ultimo_comando_real"] > WATCHDOG_S:   # un giro por angulo no depende del watchdog
            estado["motores_en_marcha"] = False
            estado["perfil"] = "AUTO"            # tras el watchdog el perfil vuelve a AUTO
            imprimir("WATCHDOG: motores detenidos")


def revisar_ruta(conexion, peticion):
    """Solo se acepta /ws; cualquier otra ruta responde 404 (como el carro)."""
    if peticion.path != RUTA:
        return conexion.respond(http.HTTPStatus.NOT_FOUND, "Not found\n")
    return None


async def main(puerto):
    tarea = asyncio.create_task(vigilar_watchdog())
    tarea_sens = asyncio.create_task(emisor_sens())
    async with serve(atender, "0.0.0.0", puerto, process_request=revisar_ruta):
        imprimir("Servidor falso del carro escuchando en 0.0.0.0:%d%s (Ctrl+C para salir)" % (puerto, RUTA))
        await asyncio.get_running_loop().create_future()   # espera para siempre
    tarea.cancel()
    tarea_sens.cancel()


if __name__ == "__main__":
    lector = argparse.ArgumentParser(description="Servidor WebSocket falso del carro")
    lector.add_argument("--puerto", type=int, default=8080)
    lector.add_argument("--sensores", action="store_true", help="imita al firmware con el giroscopio y el laser (J11)")
    lector.add_argument("--sin-giroscopio", action="store_true")
    lector.add_argument("--sin-laser", action="store_true")
    argumentos = lector.parse_args()
    if argumentos.sensores:
        estado["sens"]["activo"] = True
        estado["sens"]["imu"] = not argumentos.sin_giroscopio
        estado["sens"]["tof"] = not argumentos.sin_laser
    try:
        asyncio.run(main(argumentos.puerto))
    except KeyboardInterrupt:
        imprimir("Servidor detenido")
