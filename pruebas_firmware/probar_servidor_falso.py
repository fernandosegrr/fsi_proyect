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
