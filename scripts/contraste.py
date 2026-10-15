#!/usr/bin/env python3
"""
contraste.py - Comprueba el contraste (WCAG) de la paleta de la app.

Lee los colores de app_android/app/src/main/res/values/colors.xml (tema claro) y de
values-night/colors.xml (tema oscuro) y comprueba cada pareja "texto sobre fondo" de la
lista PAREJAS. El minimo es 4.5:1 (texto normal) y 3:1 para los iconos y bordes que
solo acompanan (lista PAREJAS_GRAFICAS).

Uso:
    python scripts/contraste.py             # imprime la tabla y sale con 1 si algo falla
    python scripts/contraste.py --markdown  # imprime la tabla en Markdown (para docs/diseno.md)

Solo usa la libreria estandar de Python.
"""
import re
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
RES = RAIZ / "app_android" / "app" / "src" / "main" / "res"

# (nombre de la pareja, color del texto, color del fondo)
PAREJAS = [
    ("Texto principal sobre el fondo", "texto", "fondo"),
    ("Texto principal sobre una tarjeta", "texto", "superficie"),
    ("Texto principal sobre una tarjeta elevada", "texto", "superficie_alta"),
    ("Texto suave sobre el fondo", "texto_suave", "fondo"),
    ("Texto suave sobre una tarjeta", "texto_suave", "superficie"),
    ("Texto suave sobre una tarjeta elevada", "texto_suave", "superficie_alta"),
    ("Texto del boton primario", "sobre_acento", "acento"),
    ("Acento (enlaces, iconos) sobre el fondo", "acento_texto", "fondo"),
    ("Acento sobre una tarjeta", "acento_texto", "superficie"),
    ("Acento sobre una tarjeta elevada", "acento_texto", "superficie_alta"),
    ("Chip OK", "ok", "ok_fondo"),
    ("Chip aviso", "aviso", "aviso_fondo"),
    ("Chip error", "peligro", "peligro_fondo"),
    ("OK sobre una tarjeta", "ok", "superficie"),
    ("Aviso sobre una tarjeta", "aviso", "superficie"),
    ("Peligro sobre una tarjeta", "peligro", "superficie"),
    ("Texto del boton de PARO", "sobre_paro", "paro"),
    ("Texto sobre el chip neutro", "texto_suave", "neutro_fondo"),
    ("Texto del aviso en banda (aviso)", "texto", "aviso_fondo"),
    ("Texto del aviso en banda (error)", "texto", "peligro_fondo"),
    ("Texto de la barra inferior (activo)", "acento_texto", "superficie"),
    ("Texto de la barra inferior (inactivo)", "texto_suave", "superficie"),
]
# Bordes e iconos decorativos: 3:1.
PAREJAS_GRAFICAS = [
    ("Borde de una tarjeta sobre el fondo", "borde_fuerte", "fondo"),
    ("Paro sobre el fondo", "paro", "fondo"),
]


def leer_colores(ruta):
    texto = ruta.read_text(encoding="utf-8")
    return {m.group(1): m.group(2).upper() for m in re.finditer(r'<color name="([a-z_0-9]+)">(#[0-9A-Fa-f]{6})</color>', texto)}


def luminancia(hexa):
    canales = [int(hexa[i:i + 2], 16) / 255.0 for i in (1, 3, 5)]
    lineales = [c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4 for c in canales]
    return 0.2126 * lineales[0] + 0.7152 * lineales[1] + 0.0722 * lineales[2]


def contraste(a, b):
    la, lb = luminancia(a), luminancia(b)
    claro, oscuro = max(la, lb), min(la, lb)
    return (claro + 0.05) / (oscuro + 0.05)


def main():
    claro = leer_colores(RES / "values" / "colors.xml")
    oscuro = dict(claro)
    oscuro.update(leer_colores(RES / "values-night" / "colors.xml"))
    markdown = "--markdown" in sys.argv
    fallos = 0
    if markdown:
        print("| Pareja | Tema | Texto | Fondo | Contraste | Minimo | Resultado |")
        print("|---|---|---|---|---|---|---|")
    for lista, minimo in ((PAREJAS, 4.5), (PAREJAS_GRAFICAS, 3.0)):
        for nombre, fg, bg in lista:
            for tema, paleta in (("claro", claro), ("oscuro", oscuro)):
                if fg not in paleta or bg not in paleta:
                    print("FALTA el color %s o %s en el tema %s" % (fg, bg, tema))
                    fallos += 1
                    continue
                valor = contraste(paleta[fg], paleta[bg])
                bien = valor >= minimo
                fallos += 0 if bien else 1
                if markdown:
                    print("| %s | %s | `%s` %s | `%s` %s | %.2f:1 | %.1f:1 | %s |" % (nombre, tema, fg, paleta[fg], bg, paleta[bg], valor, minimo, "pasa" if bien else "FALLA"))
                else:
                    print("%-46s %-7s %s sobre %s  %5.2f:1  (minimo %.1f)  %s" % (nombre, tema, paleta[fg], paleta[bg], valor, minimo, "ok" if bien else "FALLA"))
    if not markdown:
        print("\n%s" % ("TODAS LAS PAREJAS PASAN" if fallos == 0 else "%d PAREJAS FALLAN" % fallos))
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
