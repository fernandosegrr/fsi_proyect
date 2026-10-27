"""
probar_servidor_falso.py - Comprueba que servidor_falso.py se comporta como el carro.

GUIA DE LECTURA
 1. Arranca el servidor en el puerto 8099 (subproceso) y se conecta como cliente.
 2. Cada caso imprime OK o FALLO, igual que las otras pruebas del proyecto.
 3. Lee la salida del servidor para comprobar el watchdog, los desconocidos, etc.

Uso (desde pruebas_firmware):
    .venv\\Scripts\\python.exe probar_servidor_falso.py
"""

import asyncio
import subprocess
import sys
import time

from websockets.asyncio.client import connect

PUERTO = 8099
URL = "ws://127.0.0.1:%d/ws" % PUERTO

casos = 0
fallos = 0


def caso(nombre, ok):
    global casos, fallos
    casos += 1
    if not ok:
        fallos += 1
    print("%s  %s" % ("OK   " if ok else "FALLO", nombre))


async def hablar():
    async with connect(URL) as ws:
        await ws.send("PING")
        caso("PING se contesta con PONG", await asyncio.wait_for(ws.recv(), 2) == "PONG")

        await ws.send("FORWARD")
        await asyncio.sleep(0.5)
        await ws.send("PING")                       # el PING no debe renovar el watchdog
        await asyncio.wait_for(ws.recv(), 2)
        await asyncio.sleep(0.8)                    # 1.3 s desde FORWARD: el watchdog debe saltar

        await ws.send("HOLA")                       # desconocido: se ignora, sin respuesta
        await ws.send("MOVE:FWD:50")                # el protocolo planeado NO existe todavia
        await ws.send("STOP")
        await asyncio.sleep(0.2)
        await ws.send("PING")
        caso("los comandos desconocidos no se contestan", await asyncio.wait_for(ws.recv(), 2) == "PONG")

        # Perfiles: ACK, STOP primero si se mueve, el mismo perfil no frena, el watchdog y la reconexion vuelven a AUTO.
        await ws.send("MODO_MANUAL")
        caso("MODO_MANUAL se contesta con ACK_MANUAL", await asyncio.wait_for(ws.recv(), 2) == "ACK_MANUAL")
        await ws.send("FORWARD")
        await ws.send("MODO_MANUAL")
        caso("MODO_MANUAL repetido se contesta igual (idempotente)", await asyncio.wait_for(ws.recv(), 2) == "ACK_MANUAL")
        await ws.send("MODO_AUTO")
        caso("MODO_AUTO se contesta con ACK_AUTO", await asyncio.wait_for(ws.recv(), 2) == "ACK_AUTO")
        await ws.send("MODO_MANUAL")
        await asyncio.wait_for(ws.recv(), 2)
        await ws.send("FORWARD")
        await asyncio.sleep(1.3)                    # sin heartbeat: salta el watchdog y el perfil vuelve a AUTO
        await ws.send("STOP")
        await asyncio.sleep(0.1)
        await ws.send("REVERSE")                    # con heartbeat de 250 ms no hay watchdog
        for _ in range(6):
            await asyncio.sleep(0.25)
            await ws.send("REVERSE")
        await ws.send("STOP")
        await asyncio.sleep(0.3)

        # Balance de los lados: ACK, NAK, BAL? y que se conserva aunque el cliente se desconecte.
        await ws.send("BAL?")
        caso("BAL? se contesta con los valores por defecto BAL:986:988", await asyncio.wait_for(ws.recv(), 2) == "BAL:986:988")
        await ws.send("BAL_AUTO:900")
        caso("BAL_AUTO:900 se contesta ACK_BAL_AUTO:900", await asyncio.wait_for(ws.recv(), 2) == "ACK_BAL_AUTO:900")
        await ws.send("BAL_MANUAL:1000")
        caso("BAL_MANUAL:1000 se contesta ACK_BAL_MANUAL:1000", await asyncio.wait_for(ws.recv(), 2) == "ACK_BAL_MANUAL:1000")
        for malo in ("BAL_AUTO:799", "BAL_AUTO:1001", "BAL_AUTO:", "BAL_AUTO:abc", "BAL_MANUAL:0900", "BAL_AUTO:-900", "BAL_AUTO: 900"):
            await ws.send(malo)
            caso("%s se contesta NAK_BAL" % malo, await asyncio.wait_for(ws.recv(), 2) == "NAK_BAL")
        await ws.send("BAL?")
        caso("BAL? despues de los cambios: BAL:900:1000 (los NAK no cambiaron nada)", await asyncio.wait_for(ws.recv(), 2) == "BAL:900:1000")

    async with connect(URL) as ws:
        await ws.send("BAL?")
        caso("el balance se conserva al reconectar (como la memoria no volatil del carro)", await asyncio.wait_for(ws.recv(), 2) == "BAL:900:1000")
        await ws.send("BAL_AUTO:986")
        await asyncio.wait_for(ws.recv(), 2)
        await ws.send("BAL_MANUAL:988")
        await asyncio.wait_for(ws.recv(), 2)


PUERTO_SENSORES = 8098


async def siguiente(ws, prefijo, espera=3.0):
    """Lee mensajes hasta uno que empiece con `prefijo` (ignora los SENS que llegan solos); None si no llega en `espera` s."""
    fin = time.monotonic() + espera
    while time.monotonic() < fin:
        try:
            m = await asyncio.wait_for(ws.recv(), max(0.05, fin - time.monotonic()))
        except asyncio.TimeoutError:
            return None
        if m.startswith(prefijo):
            return m
    return None


async def probar_sensores():
    """El servidor con --sensores (giroscopio y laser, J11) y sin ellos (firmware viejo)."""
    sensores = subprocess.Popen([sys.executable, "-u", "servidor_falso.py", "--puerto", str(PUERTO_SENSORES), "--sensores"],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8")
    time.sleep(1.0)
    try:
        async with connect("ws://127.0.0.1:%d/ws" % PUERTO_SENSORES) as ws:
            await ws.send("CAPS?")
            caso("sensores: CAPS? incluye capacidades y version simulada", await siguiente(ws, "CAPS:") == "CAPS:IMU=1,TOF=1,TOFR=0,IMUR=0,IMUM=0,IMUE=2,FW:simulado")
            await ws.send("ANT?")
            caso("sensores: ANT? informa los cuatro valores", await siguiente(ws, "ANT:") == "ANT:100,100,20,20")
            sens = [m for m in [await siguiente(ws, "SENS:", 0.4) for _ in range(8)] if m]
            caso("sensores: SENS llega solo, cada ~100 ms, con perfil AUTO (8 en menos de 3.2 s)", len(sens) == 8 and sens[-1].count(",") == 2)
            await ws.send("GIRAR:90")
            ok = await siguiente(ws, "GIRO_", 3)
            caso("sensores: GIRAR:90 incluye angulo, duracion y ANT", ok is not None and ok.startswith("GIRO_OK:90:") and len(ok.split(":")) == 5 and ok.endswith(":100") and 880 <= int(ok.split(":")[2]) <= 930)
            for malo in ("GIRAR:4", "GIRAR:201", "GIRAR:abc"):
                await ws.send(malo)
                caso("sensores: %s -> GIRO_ERROR:angulo" % malo, await siguiente(ws, "GIRO_") == "GIRO_ERROR:angulo")
            await ws.send("GIRAR:-180")
            await asyncio.sleep(0.4)
            await ws.send("GIRAR:45")
            caso("sensores: un GIRAR con otro en curso -> GIRO_ERROR:ocupado", await siguiente(ws, "GIRO_") == "GIRO_ERROR:ocupado")
            await ws.send("STOP")
            caso("sensores: STOP informa ANT_IGNORADO", await siguiente(ws, "ANT_IGNORADO:") == "ANT_IGNORADO:interrumpido")
            abortado = await siguiente(ws, "GIRO_")
            caso("sensores: STOP a mitad de un giro informa tambien ANT", abortado is not None and abortado.startswith("GIRO_ERROR:abortado:-180:") and len(abortado.split(":")) == 6 and abortado.endswith(":100"))
            await ws.send("ANT?")
            caso("sensores: la interrupcion no cambia ANT", await siguiente(ws, "ANT:") == "ANT:100,100,20,20")
            await ws.send("ANT_BORRAR")
            caso("sensores: ANT_BORRAR confirma", await siguiente(ws, "ACK_ANT_BORRAR") == "ACK_ANT_BORRAR")
            await ws.send("IMU_CERO")
            caso("sensores: IMU_CERO -> ACK_IMU_CERO", await siguiente(ws, "ACK_IMU_CERO") == "ACK_IMU_CERO")
            await ws.send("IMU_CAL")
            caso("sensores: IMU_CAL -> ACK_IMU_CAL y despues IMU_CAL_OK:-47", await siguiente(ws, "ACK_IMU_CAL") == "ACK_IMU_CAL" and await siguiente(ws, "IMU_CAL_OK:") == "IMU_CAL_OK:-47")
            # Laser: TOF_PARO con FORWARD en AUTO y menos de 120 mm; con TOF_USO:0 no.
            await ws.send("MODO_AUTO")
            await siguiente(ws, "ACK_AUTO")
            await ws.send("SIM_TOF:80")
            await ws.send("FORWARD")
            paro = None
            for _ in range(6):
                await ws.send("FORWARD")
                paro = await siguiente(ws, "TOF_PARO:", 0.3)
                if paro:
                    break
            caso("sensores: FORWARD con algo a 80 mm -> TOF_PARO:80", paro == "TOF_PARO:80")
            await ws.send("STOP")
            await ws.send("TOF_USO:0")
            caso("sensores: TOF_USO:0 -> ACK_TOF_USO:0", await siguiente(ws, "ACK_TOF_USO") == "ACK_TOF_USO:0")
            paro = None
            for _ in range(6):
                await ws.send("FORWARD")
                paro = await siguiente(ws, "TOF_PARO:", 0.3)
            caso("sensores: con TOF_USO:0 el laser ya no frena", paro is None)
            await ws.send("STOP")
            await ws.send("TOF?")
            caso("sensores: TOF? informa la distancia (80 mm, medida valida)", await siguiente(ws, "TOF:") == "TOF:80,3")
            await ws.send("MODO_MANUAL")
            await siguiente(ws, "ACK_MANUAL")
            viejo = await siguiente(ws, "SENS:", 0.6)
            caso("sensores: en perfil MANUAL no se manda SENS", viejo is None)
    finally:
        sensores.terminate()
        sensores.communicate(timeout=5)
    # Firmware viejo: sin --sensores no contesta a CAPS?
    sin = subprocess.Popen([sys.executable, "-u", "servidor_falso.py", "--puerto", str(PUERTO_SENSORES)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8")
    time.sleep(1.0)
    try:
        async with connect("ws://127.0.0.1:%d/ws" % PUERTO_SENSORES) as ws:
            await ws.send("CAPS?")
            await ws.send("PING")
            caso("firmware viejo (sin --sensores): CAPS? no se contesta (el siguiente mensaje es el PONG)", await asyncio.wait_for(ws.recv(), 2) == "PONG")
    finally:
        sin.terminate()
        sin.communicate(timeout=5)


async def principal():
    servidor = subprocess.Popen([sys.executable, "-u", "servidor_falso.py", "--puerto", str(PUERTO)],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8")
    time.sleep(1.0)
    try:
        await hablar()
        # Un segundo cliente cierra al primero (solo uno a la vez).
        async with connect(URL) as primero:
            async with connect(URL) as segundo:
                await segundo.send("PING")
                await asyncio.wait_for(segundo.recv(), 2)
                try:
                    await asyncio.wait_for(primero.recv(), 2)
                    cerrado = False
                except Exception:
                    cerrado = True
                caso("un cliente nuevo cierra al anterior", cerrado)
        await asyncio.sleep(0.3)
    finally:
        servidor.terminate()
        salida = servidor.communicate(timeout=5)[0]

    await probar_sensores()
    caso("el watchdog salta tras FORWARD + solo PING (y otra vez tras el FORWARD sin heartbeat de los perfiles)", salida.count("WATCHDOG: motores detenidos") == 2)
    caso("se imprime el comando desconocido y se ignora", "DESCONOCIDO (ignorado): 'HOLA'" in salida)
    caso("MOVE:FWD:50 se trata como desconocido", "DESCONOCIDO (ignorado): 'MOVE:FWD:50'" in salida)
    caso("con heartbeat de 250 ms el watchdog no salta de nuevo", salida.count("WATCHDOG") == 2)
    caso("cambiar de perfil con el carro en marcha imprime STOP por cambio de perfil (una vez: MODO_AUTO en marcha)", salida.count("STOP por cambio de perfil") == 1)
    caso("el perfil cambia a MANUAL y a AUTO", "Perfil: MANUAL" in salida and "Perfil: AUTO" in salida)
    caso("se imprimen los ms desde el mensaje anterior", " ms  REVERSE" in salida)
    caso("se imprime la desconexion", "DESCONEXION: motores detenidos" in salida)
    print("\n--- salida del servidor ---\n" + salida)


asyncio.run(principal())
print("\n%d casos, %d fallos" % (casos, fallos))
sys.exit(0 if fallos == 0 else 1)
