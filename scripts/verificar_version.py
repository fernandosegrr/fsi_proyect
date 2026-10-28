"""Compara versiones de FW y APK con Git; solo consultas, sin instalar ni mover.

Uso: pruebas_firmware\\.venv\\Scripts\\python.exe scripts\\verificar_version.py --puerto COM6
Se puede indicar --adb, --apk (para comparar también bytes) y --revision.
Con --solo-apk no abre ningún puerto serie y deja el FW pendiente para Sensores.
Abrir el puerto no cambia DTR/RTS deliberadamente ni escribe NVS. Solo manda CAPS?.
"""
import argparse
import hashlib
import io
import os
from pathlib import Path
import re
import subprocess
import time
import zipfile

RAIZ = Path(__file__).resolve().parent.parent


def ejecutar(argumentos, binario=False):
    return subprocess.check_output(argumentos, cwd=str(RAIZ), timeout=45,
                                   **({} if binario else {"text": True, "encoding": "utf-8"}))


def version_fw(texto):
    hallado = re.search(r"FW:([0-9a-f]{7,40})-(\d{8})", texto)
    if not hallado:
        raise ValueError("CAPS no contiene FW:<hash>-<aaaammdd>")
    return hallado.group(0), hallado.group(1)


def version_apk(datos):
    with zipfile.ZipFile(io.BytesIO(datos)) as apk:
        texto = apk.read("assets/version_app.txt").decode("utf-8").strip()
    hallado = re.fullmatch(r"APP:([\w.]+)-([0-9a-f]{7,40})", texto)
    if not hallado:
        raise ValueError("APK sin metadatos válidos de versión")
    return texto, hallado.group(2)


def coincide_revision(hash_corto, revision):
    """Tolera commits posteriores de documentación, entrega y del propio verificador, sin ocultar cambios de producto."""
    construido = ejecutar(["git", "rev-parse", hash_corto]).strip()
    objetivo = ejecutar(["git", "rev-parse", revision]).strip()
    if construido == objetivo:
        return True
    # No basta un árbol igual en otra rama: la revisión debe ser un antecesor.
    if subprocess.run(["git", "merge-base", "--is-ancestor", construido, objetivo],
                      cwd=str(RAIZ), capture_output=True).returncode != 0:
        return False
    nombres = ejecutar(["git", "diff", "--name-only", construido, objetivo]).splitlines()
    return all(_solo_documentacion(n) for n in nombres)


def _solo_documentacion(nombre):
    """README, docs/, dist/ y los dos archivos del verificador no cambian el APK ni el firmware."""
    prefijos = ("docs/", "dist/")
    exactos = {"README.md", "scripts/verificar_version.py", "scripts/probar_version.py"}
    return nombre.startswith(prefijos) or nombre in exactos


def consultar_caps(puerto):
    import serial
    conexion = serial.Serial()
    conexion.port = puerto
    conexion.baudrate = 115200
    conexion.timeout = 0.2
    conexion.dtr = False
    conexion.rts = False
    conexion.open()
    try:
        limite = time.monotonic() + 8
        proxima = 0
        while time.monotonic() < limite:
            if time.monotonic() >= proxima:
                conexion.write(b"CAPS?\n")
                proxima = time.monotonic() + 1
            linea = conexion.readline().decode("utf-8", errors="replace").strip()
            if linea.startswith("CAPS:") and "FW:" in linea:
                return linea
    finally:
        conexion.close()
    raise ValueError("El DevKit no respondió con una versión FW")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    conexion = p.add_mutually_exclusive_group(required=True)
    conexion.add_argument("--puerto")
    conexion.add_argument("--solo-apk", action="store_true")
    p.add_argument("--adb", default=str(Path(os.environ.get("LOCALAPPDATA", "")) / "Android/Sdk/platform-tools/adb.exe"))
    p.add_argument("--apk", type=Path)
    p.add_argument("--revision", default="HEAD")
    a = p.parse_args()
    try:
        caps = consultar_caps(a.puerto) if a.puerto else ""
        fw, hash_fw = version_fw(caps) if caps else ("FW PENDIENTE: comprobar en Sensores con el carro conectado", "")
        ruta = ejecutar([a.adb, "shell", "pm", "path", "com.fsi.carro"]).strip().splitlines()
        if len(ruta) != 1 or not ruta[0].startswith("package:"):
            raise ValueError("No hay un APK único instalado de com.fsi.carro")
        instalado = ejecutar([a.adb, "exec-out", "cat", ruta[0][8:]], binario=True)
        app, hash_app = version_apk(instalado)
        paquete = ejecutar([a.adb, "shell", "dumpsys", "package", "com.fsi.carro"])
        nombre = re.search(r"versionName=([^\s]+)", paquete)
        nombre_ok = nombre is not None and app.startswith("APP:" + nombre.group(1) + "-")
        bytes_ok = a.apk is None or hashlib.sha256(a.apk.read_bytes()).digest() == hashlib.sha256(instalado).digest()
        fuentes_limpias = not ejecutar(["git", "status", "--porcelain", "--untracked-files=no", "--", "firmware_carro", "app_android", "nucleo_c", "scripts"]).strip()
        fw_ok = a.solo_apk or hash_fw == hash_app
        ok = (fw_ok and coincide_revision(hash_app, a.revision) and nombre_ok and bytes_ok and fuentes_limpias)
        if caps:
            print(caps)
        print(fw)
        print(app)
        print("versionName=" + (nombre.group(1) if nombre else "desconocida"))
        print("APK idéntica al build=" + str(bytes_ok) if a.apk else "Sin comparación binaria de APK (usar --apk)")
        print("Fuentes versionadas sin cambios locales=" + str(fuentes_limpias))
        print("Comparación con " + a.revision + ": " + ("PASA" if ok else "NO COINCIDE"))
        return 0 if ok else 1
    except (OSError, ValueError, subprocess.SubprocessError, KeyError, zipfile.BadZipFile) as e:
        print("No se pudo verificar: " + str(e))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
