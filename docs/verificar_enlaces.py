#!/usr/bin/env python3
"""
verificar_enlaces_docs.py
Verifica que no existan enlaces relativos o absolutos rotos entre los archivos Markdown (.md).
"""
import os
import re
import sys
import urllib.parse

def extraer_enlaces(texto):
    # Detecta patrones [texto](url)
    patron = r'\[([^\]]+)\]\(([^)]+)\)'
    return re.findall(patron, texto)

def verificar_enlaces():
    errores = []
    total_enlaces = 0
    archivos_md = []

    for root, dirs, files in os.walk('.'):
        # Ignorar carpetas no pertinentes
        if any(ign in root for ign in ['original_backup', '.git', '__pycache__', '.gradle', 'build', '.idea', 'node_modules']):
            continue
        for f in files:
            if f.endswith('.md'):
                archivos_md.append(os.path.normpath(os.path.join(root, f)))

    print(f"Analizando {len(archivos_md)} archivos Markdown...")

    for ruta_md in archivos_md:
        with open(ruta_md, 'r', encoding='utf-8', errors='ignore') as fh:
            contenido = fh.read()

        enlaces = extraer_enlaces(contenido)
        directorio_base = os.path.dirname(ruta_md)

        for texto, destino in enlaces:
            total_enlaces += 1
            destino_limpio = destino.strip()

            # Ignorar enlaces web externos (http, https, mailto, badges)
            if destino_limpio.startswith(('http://', 'https://', 'mailto:', '#')):
                continue

            # Manejar esquema file:///
            if destino_limpio.startswith('file:///'):
                # Extraer ruta local tras file:///
                ruta_local = destino_limpio[8:]
                # En Windows, puede ser e:/... o E:/...
                ruta_local = urllib.parse.unquote(ruta_local)
                if not os.path.exists(ruta_local):
                    errores.append((ruta_md, destino_limpio, f"Ruta absoluta file:/// no existe: {ruta_local}"))
                continue

            # Separar posible ancla interna (#seccion)
            partes = destino_limpio.split('#', 1)
            archivo_destino = partes[0]

            if not archivo_destino:
                # Es solo un ancla interna dentro del mismo archivo
                continue

            # Resolver ruta relativa
            ruta_resuelta = os.path.normpath(os.path.join(directorio_base, archivo_destino))

            if not os.path.exists(ruta_resuelta):
                errores.append((ruta_md, destino_limpio, f"Ruta relativa no encontrada: {ruta_resuelta}"))

    print(f"Total de enlaces analizados: {total_enlaces}")
    if errores:
        print(f"\n[ALERTA] Se encontraron {len(errores)} enlaces rotos:")
        for origen, dest, motivo in errores:
            print(f"  En {origen}: '{dest}' -> {motivo}")
        return 1
    else:
        print("[OK] Todos los enlaces internos entre documentos Markdown son validos.")
        return 0

if __name__ == '__main__':
    sys.exit(verificar_enlaces())
