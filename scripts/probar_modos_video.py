# -*- coding: utf-8 -*-
"""
probar_modos_video.py — Prueba en el teléfono (por adb, SIN el carro) los tres modos de video y sus fallos con una cámara falsa.

GUÍA DE LECTURA
 1. Qué prueba: la cadena de la app (lectura, decodificación y análisis con cola de 1), los modos jpeg / raw / celdas de VideoCarro, el registro MODO_STATS y la vuelta a jpeg
    cuando un modo falla. NO prueba la cámara real: las imágenes son sintéticas (scripts/camara_falsa.py) y las celdas las manda scripts/camara_falsa_udp.c corriendo en el propio teléfono.
 2. Cómo: por cada escenario se guarda la configuración de la app (vision.xml, se restaura al final), se pone en modo «fuente = video del carro» apuntando a http://127.0.0.1:8085/stream
    (adb reverse), se abre la pantalla de visión ~22 s y se lee de logcat MODO_STATS y los avisos de VIDEO.
 3. Necesita: el teléfono conectado por adb, el NDK de Android (para compilar camara_falsa_udp), numpy y Pillow en el Python de la PC (ya están) y que la app ya esté instalada
    (scripts/instalar_app_adb.py). No toca nada más del teléfono ni de la PC.
 4. Uso:   python scripts/probar_modos_video.py [--segundos 22]
"""
import argparse
import glob
import os
import subprocess
import sys
import time

ADB = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk", "platform-tools", "adb.exe")
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAQ = "com.fsi.carro"
PREFS = "shared_prefs/vision.xml"


def adb(*args, timeout=60):
    env = dict(os.environ, MSYS_NO_PATHCONV="1")
    r = subprocess.run([ADB] + list(args), capture_output=True, timeout=timeout, env=env)
    return r.stdout.decode("utf-8", "replace")


def compilar_udp():
    ndk = sorted(glob.glob(os.path.join(os.environ["LOCALAPPDATA"], "Android", "Sdk", "ndk", "*", "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin")))
    if not ndk:
        print("No se encontró el NDK de Android")
        return None
    salida = os.path.join(os.environ.get("TEMP", "."), "camara_falsa_udp")
    cc = os.path.join(ndk[-1], "aarch64-linux-android24-clang.cmd")
    nuc = os.path.join(RAIZ, "nucleo_c")
    fuentes = [os.path.join(RAIZ, "scripts", "camara_falsa_udp.c")] + [os.path.join(nuc, f) for f in ("obstaculos.c", "paquete_celdas.c", "pisos_sinteticos.c")]
    r = subprocess.run([cc, "-std=gnu99", "-O2", "-I" + nuc, "-o", salida] + fuentes, capture_output=True)
    if r.returncode != 0:
        print(r.stderr.decode("utf-8", "replace"))
        return None
    return salida


def escribir_prefs(base_xml, modo):
    """La configuración del usuario más: fuente = video del carro, la URL de la cámara falsa y el modo de video."""
    lineas = [l for l in base_xml.splitlines() if "fuente_carro" not in l and "url_video_carro" not in l and "modo_video" not in l and "</map>" not in l]
    lineas += ['    <boolean name="fuente_carro" value="true" />', '    <string name="url_video_carro">http://127.0.0.1:8085/stream</string>',
               '    <string name="modo_video">%s</string>' % modo, "</map>"]
    local = os.path.join(os.environ.get("TEMP", "."), "vision_prueba.xml")
    with open(local, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lineas) + "\n")
    adb("push", local, "/data/local/tmp/vision_prueba.xml")
    adb("shell", "run-as", PAQ, "cp", "/data/local/tmp/vision_prueba.xml", PREFS)


def escenario(nombre, modo, opciones_camara, con_udp, segundos, base_xml):
    print("\n=== %s (modo pedido: %s) ===" % (nombre, modo), flush=True)
    camara = subprocess.Popen([sys.executable, os.path.join(RAIZ, "scripts", "camara_falsa.py"), "--puerto", "8085"] + opciones_camara, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    udp = None
    if con_udp:
        udp = subprocess.Popen([ADB, "shell", "/data/local/tmp/camara_falsa_udp", "4210", "10", str(segundos + 15)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.5)
    adb("shell", "am", "force-stop", PAQ)
    escribir_prefs(base_xml, modo)
    adb("logcat", "-c")
    adb("shell", "input", "keyevent", "KEYCODE_WAKEUP")
    adb("shell", "am", "start", "-n", PAQ + "/.VisionActivity")
    time.sleep(segundos)
    log = adb("logcat", "-d", "-s", "MODO_STATS:I", "VIDEO:I", "VIDEO:W", "VISION:W", "AndroidRuntime:E")
    adb("shell", "am", "force-stop", PAQ)
    camara.terminate()
    if udp is not None:
        udp.terminate()
        adb("shell", "pkill", "camara_falsa_udp")
    interesantes = [l for l in log.splitlines() if any(k in l for k in ("MODO_STATS", "vuelvo a jpeg", "llegó el primer", "entregando cuadros", "FATAL", "Exception", "modo de video"))]
    for l in interesantes[-14:]:
        print(l.split(": ", 1)[-1] if "MODO_STATS" in l else l[-170:])
    return log


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--segundos", type=int, default=22)
    args = p.parse_args()
    ejecutable = compilar_udp()
    if not ejecutable:
        return 1
    adb("push", ejecutable, "/data/local/tmp/camara_falsa_udp")
    adb("shell", "chmod", "755", "/data/local/tmp/camara_falsa_udp")
    adb("reverse", "tcp:8085", "tcp:8085")
    base_xml = adb("shell", "run-as", PAQ, "cat", PREFS)
    if "<map>" not in base_xml:
        print("No se pudo leer la configuración de la app (¿está instalada y es depurable?)")
        return 1
    copia = os.path.join(os.environ.get("TEMP", "."), "vision_original.xml")
    with open(copia, "w", encoding="utf-8", newline="\n") as f:
        f.write(base_xml)
    print("Configuración original guardada en", copia)
    try:
        s = args.segundos
        escenario("jpeg (de siempre)", "jpeg", [], False, s, base_xml)
        escenario("raw", "raw", [], False, s, base_xml)
        escenario("celdas por UDP", "celdas", [], True, s, base_xml)
        escenario("raw pero la cámara no lo acepta (400)", "raw", ["--sin-modo", "raw"], False, 12, base_xml)
        escenario("raw pero /raw no manda nada", "raw", ["--raw-mudo"], False, 14, base_xml)
        escenario("celdas pero no llega ningún paquete UDP", "celdas", [], False, 14, base_xml)
    finally:
        adb("shell", "am", "force-stop", PAQ)
        with open(copia, encoding="utf-8") as f:
            original = f.read()
        local = os.path.join(os.environ.get("TEMP", "."), "vision_restaurar.xml")
        with open(local, "w", encoding="utf-8", newline="\n") as f:
            f.write(original)
        adb("push", local, "/data/local/tmp/vision_restaurar.xml")
        adb("shell", "run-as", PAQ, "cp", "/data/local/tmp/vision_restaurar.xml", PREFS)
        adb("shell", "rm", "-f", "/data/local/tmp/vision_prueba.xml", "/data/local/tmp/vision_restaurar.xml")
        print("\nConfiguración de la app restaurada.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
