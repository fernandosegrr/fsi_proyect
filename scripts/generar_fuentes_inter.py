# -*- coding: utf-8 -*-
"""
generar_fuentes_inter.py — Saca de Inter (archivo variable, licencia OFL 1.1) tres fuentes ESTÁTICAS (400, 500 y 600) recortadas al alfabeto latino.

GUÍA DE LECTURA
 1. Por qué: en el Huawei el archivo variable no aplicaba los pesos (todo salía en peso normal). Con archivos estáticos el peso es el mismo en cualquier teléfono.
 2. Qué hace: abre el archivo variable de Inter (Google Fonts, ofl/inter), fija el peso (wght) con fontTools.varLib.instancer (y el tamaño óptico opsz=14) y recorta
    los símbolos (pyftsubset) a: latín básico, latín-1, latín extendido A y B, puntuación general, flechas y los signos que usa la app (≈ → … · —). Se CONSERVAN las
    funciones OpenType (tnum = cifras tabulares).
 3. Requiere fonttools (no está en el Python de la PC): se instala en un entorno virtual aparte, nunca en el global:
        python -m venv <carpeta>   y   <carpeta>\\Scripts\\python.exe -m pip install fonttools
 4. Uso:   <python del entorno> scripts/generar_fuentes_inter.py <Inter-variable.ttf> <carpeta de salida>
    Deja inter_400.ttf, inter_500.ttf e inter_600.ttf. El resultado se copia a app_android/app/src/main/res/font/.
 5. Licencia: las fuentes derivadas siguen siendo OFL 1.1 (docs/licencias/Inter-OFL-1.1.txt). El nombre reservado «Inter» no aplica: la OFL del proyecto no declara nombre reservado.
"""
import os
import sys

from fontTools.subset import Options, Subsetter, load_font, save_font
from fontTools.varLib import instancer

RANGOS = list(range(0x20, 0x7F)) + list(range(0xA0, 0x250)) + list(range(0x2010, 0x2030)) + [0x2026, 0x2032, 0x2033, 0x20AC, 0x2122, 0x2190, 0x2191, 0x2192, 0x2193, 0x2248, 0x2264, 0x2265, 0x25CF, 0x2022, 0x00B7, 0x2212, 0x00D7]


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    origen, salida = sys.argv[1], sys.argv[2]
    os.makedirs(salida, exist_ok=True)
    for nombre, peso in (("inter_400", 400), ("inter_500", 500), ("inter_600", 600)):
        fuente = load_font(origen, Options())
        estatica = instancer.instantiateVariableFont(fuente, {"wght": peso, "opsz": 14})
        opciones = Options()
        opciones.layout_features = ["*"]
        opciones.notdef_outline = True
        opciones.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]
        sub = Subsetter(opciones)
        sub.populate(unicodes=RANGOS)
        sub.subset(estatica)
        destino = os.path.join(salida, nombre + ".ttf")
        save_font(estatica, destino, opciones)
        print("%s: %d bytes" % (destino, os.path.getsize(destino)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
