#!/usr/bin/env python3
"""
buscar_nombre_anterior.py - Busca (sin distinguir mayusculas) las referencias al nombre anterior del proyecto en
codigo, recursos, cadenas, comentarios, documentacion, firmware y scripts. Sale con 1 si encuentra alguna.

Reglas de la busqueda:
  - Recorre los archivos que git conoce (git ls-files) mas los nuevos sin ignorar, asi no mira secretos.h ni compilados.
  - NO mira original_backup/ (la regla 1 del proyecto: no se toca) ni los mensajes originales guardados tal cual en
    docs/trabajos/rediseno_*.md (se guardan sin cambios por orden del usuario).
  - La palabra debe empezar en una letra suelta: asi "firewall" no cuenta, y la opcion del compilador "-W" + "all" tampoco.
Uso:  python scripts/buscar_nombre_anterior.py
"""
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
# Armado con trozos para que este archivo no se encuentre a si mismo.
PATRON = re.compile(r"(?<![A-Za-z])" + "wa" + "ll", re.IGNORECASE)
OPCION_COMPILADOR = re.compile(r"-W" + "all")
EXCLUIDOS = ("original_backup/", "docs/trabajos/rediseno_")
EXTENSIONES_BINARIAS = {".png", ".jpg", ".jpeg", ".ttf", ".tflite", ".so", ".jar", ".apk", ".exe", ".pdf", ".zip", ".onnx", ".bin", ".webp"}


def archivos():
    salida = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard"], cwd=RAIZ, capture_output=True, text=True, check=True).stdout
    for linea in salida.splitlines():
        if linea.startswith(EXCLUIDOS) or Path(linea).suffix.lower() in EXTENSIONES_BINARIAS:
            continue
        yield linea


def main():
    hallazgos = []
    for nombre in archivos():
        ruta = RAIZ / nombre
        if not ruta.is_file():
            continue
        if PATRON.search(nombre):
            hallazgos.append((nombre, 0, "(el NOMBRE del archivo)"))
        try:
            texto = ruta.read_text(encoding="utf-8", errors="strict")
        except (UnicodeDecodeError, OSError):
            continue
        for numero, linea in enumerate(texto.splitlines(), 1):
            limpia = OPCION_COMPILADOR.sub("", linea)
            if PATRON.search(limpia):
                hallazgos.append((nombre, numero, linea.strip()[:120]))
    for nombre, numero, linea in hallazgos:
        print("%s:%d: %s" % (nombre, numero, linea))
    print("\n%s" % ("SIN REFERENCIAS al nombre anterior" if not hallazgos else "%d REFERENCIAS por quitar" % len(hallazgos)))
    return 1 if hallazgos else 0


if __name__ == "__main__":
    sys.exit(main())
