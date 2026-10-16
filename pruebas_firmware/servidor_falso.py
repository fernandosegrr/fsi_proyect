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

Uso (desde la carpeta pruebas_firmware, con el entorno virtual local):
    .venv\\Scripts\\python.exe servidor_falso.py            (puerto 8080)
    .venv\\Scripts\\python.exe servidor_falso.py --puerto 9000
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


async def procesar(conexion, mensaje):
    """Interpreta un mensaje igual que el firmware."""
    if isinstance(mensaje, bytes):
        imprimir("%s  BINARIO ignorado (%d bytes)" % (ms_desde_anterior(), len(mensaje)))
        return

    intervalo = ms_desde_anterior()
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
        if estado["cliente"] is conexion:
            estado["cliente"] = None
            estado["motores_en_marcha"] = False
            estado["perfil"] = "AUTO"


async def vigilar_watchdog():
    """Watchdog de software: igual que carro_vigilar_comunicacion() del firmware."""
    while True:
        await asyncio.sleep(REVISION_WATCHDOG_S)
        if estado["motores_en_marcha"] and time.monotonic() - estado["ultimo_comando_real"] > WATCHDOG_S:
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
    async with serve(atender, "0.0.0.0", puerto, process_request=revisar_ruta):
        imprimir("Servidor falso del carro escuchando en 0.0.0.0:%d%s (Ctrl+C para salir)" % (puerto, RUTA))
        await asyncio.get_running_loop().create_future()   # espera para siempre
    tarea.cancel()


if __name__ == "__main__":
    lector = argparse.ArgumentParser(description="Servidor WebSocket falso del carro")
    lector.add_argument("--puerto", type=int, default=8080)
    argumentos = lector.parse_args()
    try:
        asyncio.run(main(argumentos.puerto))
    except KeyboardInterrupt:
        imprimir("Servidor detenido")
