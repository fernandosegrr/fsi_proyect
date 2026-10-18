# -*- coding: utf-8 -*-
"""
probar_pantallas.py — Comprueba POR REGISTRO que cada pantalla de la app abre sin errores.

GUÍA DE LECTURA
 1. Para cada pantalla de la lista: limpia logcat, abre la pantalla con adb (extra «pantalla» de MainActivity, o la Activity directa), espera unos segundos y revisa:
      - que aparezca la línea  PANTALLA: abierta=<NOMBRE> ok  (la escribe PantallasLog cuando la pantalla termina de armarse);
      - que NO haya «FATAL EXCEPTION» ni errores de AndroidRuntime de la app.
 2. Cada pantalla se prueba en vertical y en horizontal (la orientación es un extra de la app; no se toca ningún ajuste del teléfono).
 3. Usa el carro y la cámara FALSOS por `adb reverse` (pruebas_firmware/servidor_falso.py en 8080 y scripts/camara_falsa.py en 8085): las pantallas se prueban con
    «conexión» y «video» de mentira, nunca con el video real del carro. Si no están encendidos, la prueba igual corre (verá los avisos de «sin conexión»).
 4. Uso:   python scripts/probar_pantallas.py            (todas)
           python scripts/probar_pantallas.py manual rutas
    Sale con 1 si alguna pantalla falla.
"""
import os
import subprocess
import sys
import time

ADB = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk", "platform-tools", "adb.exe")

# (nombre del extra «pantalla», línea que debe salir en el registro)
PANTALLAS = [
    ("inicio", "abierta=INICIO ok"),
    ("manual", "abierta=MANUAL ok"),
    ("rutas", "abierta=RUTAS ok"),
    ("ejecutar", "abierta=EJECUTAR_RUTA ok"),
    ("mas", "abierta=MAS ok"),
    ("asistente", "abierta=ASISTENTE ok"),
    ("prueba", "abierta=PRUEBA_FINAL ok"),
    ("ajustes", "abierta=AJUSTES ok"),
    ("ayuda", "abierta=AYUDA ok"),
    ("acerca", "abierta=ACERCA ok"),
]
# Pantallas que son Activity aparte: (nombre, componente, línea esperada)
ACTIVIDADES = [
    ("vision", "com.fsi.carro/.VisionActivity", "abierta=VISION ok"),
    ("calibracion", "com.fsi.carro/.CalibracionActivity", "abierta=CALIBRACION ok"),
    ("sensores", "com.fsi.carro/.SensoresActivity", "abierta=SENSORES ok"),
]


def adb(*args):
    return subprocess.run([ADB] + list(args), capture_output=True, timeout=60).stdout.decode("utf-8", "replace")


def al_frente():
    foco = [l for l in adb("shell", "dumpsys", "window").splitlines() if "mCurrentFocus" in l]
    return bool(foco) and "com.fsi.carro" in foco[0]


def probar(nombre, orientacion, esperada, comando):
    adb("shell", "am", "force-stop", "com.fsi.carro")
    adb("logcat", "-c")
    time.sleep(0.4)
    adb(*comando)
    time.sleep(3.5)
    registro = adb("logcat", "-d", "-v", "time", "PANTALLA:I", "AndroidRuntime:E", "*:S")
    fallos = []
    if esperada not in registro:
        fallos.append("no salió «%s»" % esperada)
    if "FATAL EXCEPTION" in registro:
        fallos.append("FATAL EXCEPTION: " + " | ".join(l.strip()[:140] for l in registro.splitlines() if "Caused by" in l or "Exception" in l)[:300])
    if not al_frente():
        fallos.append("la app no quedó al frente")
    print("%-8s %-11s %s" % ("ok" if not fallos else "FALLA", nombre + "/" + orientacion[:4], "; ".join(fallos)))
    return not fallos


def main():
    pedidas = sys.argv[1:]
    todo_bien = True
    base = ["--es", "tema", "oscuro", "--ez", "asistente_visto", "true", "--es", "video_url", "http://127.0.0.1:8085/stream", "--es", "ws_url", "ws://127.0.0.1:8080/ws"]
    for nombre, esperada in PANTALLAS:
        if pedidas and nombre not in pedidas:
            continue
        for orientacion in ("vertical", "horizontal"):
            comando = ["shell", "am", "start", "-n", "com.fsi.carro/.MainActivity", "--es", "pantalla", nombre, "--es", "orientacion", orientacion] + base
            todo_bien &= probar(nombre, orientacion, esperada, comando)
    for nombre, componente, esperada in ACTIVIDADES:
        if pedidas and nombre not in pedidas:
            continue
        # La Activity se abre desde MainActivity para que se carguen los extras de prueba (video y carro falsos).
        comando = ["shell", "am", "start", "-n", componente] + ["--es", "direccion", "ws://127.0.0.1:8080/ws"]
        todo_bien &= probar(nombre, "auto", esperada, comando)
    print("\n%s" % ("TODAS LAS PANTALLAS ABREN SIN ERRORES" if todo_bien else "HAY PANTALLAS CON ERRORES"))
    return 0 if todo_bien else 1


if __name__ == "__main__":
    sys.exit(main())
