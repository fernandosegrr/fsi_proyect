# -*- coding: utf-8 -*-
"""
qa_captura.py — Abre una pantalla de la app en el teléfono y guarda una captura de la pantalla (para revisar el diseño; docs/diseno.md, sección 7).

GUÍA DE LECTURA
 1. Usa SOLO adb: `am start` con los extras de prueba de MainActivity (pantalla, tema, orientación, video_url, ws_url) y `exec-out screencap -p`.
 2. El video es la CÁMARA FALSA (scripts/camara_falsa.py, imágenes sintéticas) por `adb reverse tcp:8085`; el carro es el servidor falso
    (pruebas_firmware/servidor_falso.py) por `adb reverse tcp:8080`. NUNCA se muestra el video real del carro.
 3. No toca ningún ajuste del sistema: la orientación y el tema son de la app (extras), no del teléfono.
 4. Uso:   python scripts/qa_captura.py manual --tema oscuro --orientacion vertical --salida C:/ruta/captura.png [--espera 4]
 5. Las capturas son temporales: se borran al terminar la revisión.
"""
import argparse
import os
import subprocess
import sys
import time

ADB = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk", "platform-tools", "adb.exe")


def adb(*args, entrada=None):
    return subprocess.run([ADB] + list(args), capture_output=True, input=entrada, timeout=60)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("pantalla")
    p.add_argument("--tema", default="oscuro")
    p.add_argument("--orientacion", default="vertical")
    p.add_argument("--espera", type=float, default=4.0)
    p.add_argument("--salida", required=True)
    p.add_argument("--plantilla", type=int, default=-1, help="pone una plantilla (0 a 3) en el editor de rutas")
    p.add_argument("--sin-servidores", action="store_true", help="no usa el carro ni la cámara falsos (para ver los estados de error)")
    a = p.parse_args()

    adb("shell", "am", "force-stop", "com.fsi.carro")
    time.sleep(0.5)
    comando = ["shell", "am", "start", "-n", "com.fsi.carro/.MainActivity", "--es", "pantalla", a.pantalla, "--es", "tema", a.tema,
               "--es", "orientacion", a.orientacion, "--ez", "asistente_visto", "true"]
    if a.plantilla >= 0:
        comando += ["--ei", "ruta_plantilla", str(a.plantilla)]
    if a.sin_servidores:
        comando += ["--es", "video_url", "http://127.0.0.1:9/stream", "--es", "ws_url", "ws://127.0.0.1:9/ws"]
    else:
        comando += ["--es", "video_url", "http://127.0.0.1:8085/stream", "--es", "ws_url", "ws://127.0.0.1:8080/ws"]
    if a.pantalla in ("vision", "calibracion"):   # son Activity aparte (su orientación la fija el manifiesto)
        componente = "com.fsi.carro/.VisionActivity" if a.pantalla == "vision" else "com.fsi.carro/.CalibracionActivity"
        adb("shell", "am", "start", "-n", "com.fsi.carro/.MainActivity", "--es", "tema", a.tema, "--ez", "asistente_visto", "true", "--es", "video_url", "http://127.0.0.1:8085/stream", "--es", "ws_url", "ws://127.0.0.1:8080/ws")
        time.sleep(2.5)
        comando = ["shell", "am", "start", "-n", componente]
    r = adb(*comando)
    if b"Error" in r.stdout or b"Error" in r.stderr:
        print(r.stdout.decode("utf-8", "replace"), r.stderr.decode("utf-8", "replace"))
        return 1
    time.sleep(a.espera)
    # Seguridad: solo se captura si la app es la que está al frente (si el usuario está usando otra pantalla del teléfono, NO se captura nada).
    foco = adb("shell", "dumpsys", "window").stdout.decode("utf-8", "replace")
    linea = [l for l in foco.splitlines() if "mCurrentFocus" in l]
    if not linea or "com.fsi.carro" not in linea[0]:
        print("NO se captura: la app no está al frente (%s)" % (linea[0].strip() if linea else "sin foco"))
        return 2
    captura = adb("exec-out", "screencap", "-p").stdout
    os.makedirs(os.path.dirname(os.path.abspath(a.salida)), exist_ok=True)
    with open(a.salida, "wb") as f:
        f.write(captura)
    print("captura: %s (%d bytes)" % (a.salida, len(captura)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
