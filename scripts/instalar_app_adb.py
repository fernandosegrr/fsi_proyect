# -*- coding: utf-8 -*-
"""
instalar_app_adb.py — Instala la app en el Huawei por adb y acepta con el teclado los avisos de instalación del teléfono (sin tocar la pantalla).

GUÍA DE LECTURA
 1. El Huawei pide confirmar cada instalación hecha por adb: primero un aviso de «aplicaciones de origen desconocido» (CANCELAR / CONTINUAR) y luego la pantalla del instalador
    (BUSCAR APLICACIONES SIMILARES / INSTALAR / CANCELAR). Los toques simulados por adb se ignoran en esas pantallas, pero el teclado sí funciona.
 2. Este programa lanza `adb install -r -g` y, mientras no termine, mira la pantalla con `uiautomator dump`, busca cuál botón tiene el foco y:
      - si el foco está en CONTINUAR o INSTALAR, manda ENTER;
      - si no, mueve el foco con TAB (y, en el instalador, con las flechas hacia abajo) hasta llegar a uno de esos dos.
    Nunca manda ENTER sobre CANCELAR, BUSCAR APLICACIONES SIMILARES ni los botones de otras aplicaciones («Puede gustarle»).
 3. Solo mira los textos y los foco de los botones (no guarda ni lee imágenes).
 4. Uso:   python scripts/instalar_app_adb.py [ruta_del_apk]      (por defecto app_android/app/build/outputs/apk/debug/app-debug.apk)
"""
import os
import re
import subprocess
import sys
import time

ADB = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk", "platform-tools", "adb.exe")
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OK_TEXTOS = {"CONTINUAR", "INSTALAR"}
PROHIBIDOS = {"CANCELAR", "BUSCAR APLICACIONES SIMILARES"}


def adb(*args, entrada=None, timeout=60):
    r = subprocess.run([ADB] + list(args), capture_output=True, timeout=timeout, input=entrada)
    return r.stdout.decode("utf-8", "replace")


def botones():
    """Devuelve la lista de (texto, enfocado, (x0, y0, x1, y1)) de los nodos clicables de la pantalla."""
    adb("shell", "uiautomator", "dump", "/sdcard/ui_instalar.xml")
    xml = adb("shell", "cat", "/sdcard/ui_instalar.xml")
    lista = []
    for m in re.finditer(r"<node [^>]*>", xml):
        nodo = m.group(0)
        texto = re.search(r'text="([^"]*)"', nodo)
        enfocado = re.search(r'focused="(\w+)"', nodo)
        limites = re.search(r'bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"', nodo)
        clicable = 'clickable="true"' in nodo
        if texto and limites and (clicable or (enfocado and enfocado.group(1) == "true")):
            lista.append((texto.group(1).strip(), bool(enfocado and enfocado.group(1) == "true"), tuple(int(v) for v in limites.groups())))
    return lista


def main():
    apk = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "app_android", "app", "build", "outputs", "apk", "debug", "app-debug.apk")
    if not os.path.exists(apk):
        print("No existe el APK:", apk)
        return 1
    adb("shell", "input", "keyevent", "KEYCODE_WAKEUP")
    adb("shell", "wm", "dismiss-keyguard")
    proceso = subprocess.Popen([ADB, "install", "-r", "-g", apk], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    inicio = time.time()
    ultimo_enter = 0
    while proceso.poll() is None and time.time() - inicio < 180:
        time.sleep(1.5)
        try:
            lista = botones()
        except Exception as e:  # noqa: BLE001
            print("(no se pudo leer la pantalla:", e, ")")
            continue
        textos = [t.upper() for t, _, _ in lista]
        enfocado = next(((t.upper(), lim) for t, f, lim in lista if f), None)
        if not any(t in OK_TEXTOS for t in textos):
            continue   # todavía no hay un aviso de instalación en pantalla
        if enfocado and enfocado[0] in OK_TEXTOS:
            if time.time() - ultimo_enter > 3:
                print("ENTER sobre", enfocado[0])
                adb("shell", "input", "keyevent", "KEYCODE_ENTER")
                ultimo_enter = time.time()
            continue
        # El foco está en otro lado: se mueve. En el instalador (hay INSTALAR y CANCELAR) las flechas hacia abajo llegan primero a los botones de abajo.
        if "INSTALAR" in textos and "CANCELAR" in textos:
            adb("shell", "input", "keyevent", "KEYCODE_DPAD_DOWN")
        else:
            adb("shell", "input", "keyevent", "KEYCODE_TAB")
    salida = proceso.communicate(timeout=30)[0].decode("utf-8", "replace") if proceso.poll() is None else proceso.stdout.read().decode("utf-8", "replace")
    print(salida.strip().splitlines()[-1] if salida.strip() else "(sin salida)")
    return 0 if "Success" in salida else 2


if __name__ == "__main__":
    sys.exit(main())
