# -*- coding: utf-8 -*-
"""
hoja.py <salida.png> <ancho_baldosa> <columnas> <recorte_arriba> <archivo1.png> [archivo2.png ...]
Junta varias capturas en una sola hoja (para revisarlas de un vistazo). Recorta la barra de estado (recorte_arriba px) y rotula cada baldosa.
"""
import os
import sys

from PIL import Image, ImageDraw

salida, ancho, cols, recorte = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
archivos = sys.argv[5:]
baldosas = []
for a in archivos:
    im = Image.open(a).convert("RGB")
    im = im.crop((0, recorte, im.width, im.height))
    esc = ancho / im.width
    im = im.resize((ancho, int(im.height * esc)), Image.LANCZOS)
    baldosas.append((os.path.basename(a).replace(".png", ""), im))
filas = (len(baldosas) + cols - 1) // cols
alto_fila = max(b[1].height for b in baldosas) + 22
hoja = Image.new("RGB", (cols * (ancho + 8) + 8, filas * alto_fila + 8), (40, 40, 40))
d = ImageDraw.Draw(hoja)
for i, (nombre, im) in enumerate(baldosas):
    x = 8 + (i % cols) * (ancho + 8)
    y = 8 + (i // cols) * alto_fila
    d.text((x + 2, y + 2), nombre, fill=(255, 255, 0))
    hoja.paste(im, (x, y + 18))
hoja.save(salida)
print("hoja:", salida, hoja.size)
