#!/usr/bin/env python3
"""
verificar_pagina.py - Prueba de hash de firmware_carro/pagina.h.

Comprueba tres cosas:
  1. El bloque <script>...</script> (la logica de la pagina: heartbeat, PING, perfiles) sigue IDENTICO al de siempre
     (SHA-256 fijo en JS_SHA256). Si se cambia a proposito, hay que actualizar esa constante.
  2. El archivo completo coincide con firmware_carro/pagina.sha256 (detecta ediciones accidentales de la capa visual).
     Si cambiaste el HTML o el CSS a proposito:  python scripts/verificar_pagina.py --actualizar
  3. No aparece ninguna referencia al nombre del proyecto anterior (busqueda sin distinguir mayusculas).

Uso:  python scripts/verificar_pagina.py   (sale con 1 si algo falla)
"""
import hashlib
import re
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
PAGINA = RAIZ / "firmware_carro" / "pagina.h"
SHA_COMPLETO = RAIZ / "firmware_carro" / "pagina.sha256"

# SHA-256 del bloque <script> tal como estaba en el firmware original (no cambio en el rediseno).
JS_SHA256 = "864c4e0ad293603e487d482ba9868a27d69eab781331fcadfa88816ac01d3cc5"

# Las palabras prohibidas se arman con trozos para que este archivo no se delate a si mismo en la busqueda.
PROHIBIDA = re.compile("w" + "all", re.IGNORECASE)


def sha(texto):
    return hashlib.sha256(texto.encode("utf-8")).hexdigest()


def main():
    texto = PAGINA.read_text(encoding="utf-8")   # modo texto: los saltos de linea ya vienen como \n
    fallos = 0

    bloque = re.search(r"<script>.*?</script>", texto, re.S)
    if not bloque:
        print("FALLA: no se encontro el bloque <script>")
        return 1
    js = sha(bloque.group(0))
    if js == JS_SHA256:
        print("ok     el <script> sigue identico (%s...)" % js[:12])
    else:
        print("FALLA  el <script> cambio: %s (esperado %s)" % (js, JS_SHA256))
        fallos += 1

    completo = sha(texto)
    if "--actualizar" in sys.argv:
        SHA_COMPLETO.write_text(completo + "\n", encoding="utf-8")
        print("actualizado firmware_carro/pagina.sha256 -> %s" % completo)
    elif SHA_COMPLETO.exists() and SHA_COMPLETO.read_text(encoding="utf-8").strip() == completo:
        print("ok     el archivo completo coincide con pagina.sha256 (%s...)" % completo[:12])
    else:
        print("FALLA  pagina.h no coincide con pagina.sha256 (si el cambio fue a proposito: --actualizar)")
        fallos += 1

    encontradas = [(n, l.strip()) for n, l in enumerate(texto.splitlines(), 1) if PROHIBIDA.search(l)]
    if encontradas:
        print("FALLA  quedan referencias al nombre anterior: %s" % encontradas[:3])
        fallos += 1
    else:
        print("ok     sin referencias al nombre anterior")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
