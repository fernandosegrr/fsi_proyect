#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
revisar_manual.py — herramientas del manual v1.2 (solo leen o escriben Markdown; no tocan el PDF).

    python docs/manual/revisar_manual.py plantilla    comprueba que los capítulos de subsistema (A a I)
                                                      tienen la plantilla completa y bien formada
    python docs/manual/revisar_manual.py pendientes   reúne todos los «CONFIRMAR CON EL EQUIPO» en
                                                      docs/pendientes_confirmar.md
    python docs/manual/revisar_manual.py glosario     arma el glosario general alfabético
                                                      (docs/manual/capitulos/01b_glosario_general.md)
                                                      con los «Términos clave» (sección 6) de A a I

Orden recomendado después de editar capítulos: plantilla, glosario, pendientes y luego
python docs/manual/generar_manual.py --sin-figuras
"""
from pathlib import Path
import re
import sys
import unicodedata

RAIZ = Path(__file__).resolve().parents[2]
CAPS = RAIZ / "docs" / "manual" / "capitulos"
GUIA = RAIZ / "docs" / "guia_exposicion.md"
PENDIENTES = RAIZ / "docs" / "pendientes_confirmar.md"
GLOSARIO = CAPS / "01b_glosario_general.md"

ENCABEZADOS = [
    "Qué es y para qué sirve",
    "Cómo funciona, paso a paso",
    "Cómo lo hicimos",
    "Cómo lo probamos y con qué resultado",
    "Límites conocidos",
    "Términos clave",
    "Preguntas probables del profesor",
]
ETIQUETAS = ["VALIDADO EN EL CARRO", "SIMULADO", "NO MEDIDO", "PENDIENTE"]
MAX_ANCHO_CODIGO = 84
MARCA = "CONFIRMAR CON EL EQUIPO"


def subsistemas():
    """Archivos de subsistema ordenados por letra: {letra: ruta}."""
    out = {}
    for ruta in sorted(CAPS.glob("01[d-l]_subsistema_*.md")):
        m = re.match(r"^01[d-l]_subsistema_([A-I])_", ruta.name)
        if m:
            out[m.group(1)] = ruta
    return out


def secciones(texto):
    """Devuelve {n: texto de la sección ## X.n} (n entero) y la lista de encabezados ## en orden."""
    lineas = texto.splitlines()
    encabezados, cuerpos, actual, buf = [], {}, None, []
    en_codigo = False
    for ln in lineas:
        if ln.strip().startswith("```"):
            en_codigo = not en_codigo
        m = None if en_codigo else re.match(r"^## ([A-I])\.(\d+) (.+?)\s*$", ln)
        if m:
            if actual is not None:
                cuerpos[actual] = "\n".join(buf)
            actual = int(m.group(2))
            buf = []
            encabezados.append((m.group(1), int(m.group(2)), m.group(3)))
        elif not en_codigo and ln.startswith("## "):
            if actual is not None:
                cuerpos[actual] = "\n".join(buf)
            actual, buf = None, []
            encabezados.append((None, None, ln[3:].strip()))
        else:
            buf.append(ln)
    if actual is not None:
        cuerpos[actual] = "\n".join(buf)
    return cuerpos, encabezados


def bullets_termino(cuerpo):
    """Viñetas «- **término**: definición» de una sección (con líneas de continuación)."""
    items, actual = [], None
    for ln in cuerpo.splitlines():
        m = re.match(r"^\s*[-*]\s+\*\*(.+?)\*\*\s*[:：—–-]\s*(.*)$", ln)
        if m:
            if actual:
                items.append(actual)
            actual = [m.group(1).strip(), m.group(2).strip()]
        elif actual and ln.strip() and not re.match(r"^\s*([-*]|\d+\.)\s", ln) and not ln.startswith("#"):
            actual[1] += " " + ln.strip()
        elif not ln.strip() and actual:
            items.append(actual)
            actual = None
    if actual:
        items.append(actual)
    return [(t, d) for t, d in items]


def preguntas(cuerpo):
    return [ln for ln in cuerpo.splitlines() if re.match(r"^\s*\d+\.\s+\*\*", ln)]


def palabras(texto):
    sin_codigo = re.sub(r"```.*?```", " ", texto, flags=re.S)
    return len(re.findall(r"[\wÁÉÍÓÚÜÑáéíóúüñ]+", sin_codigo))


def marcas_confirmar(texto):
    """Preguntas «CONFIRMAR CON EL EQUIPO: ...» de un texto (una por aparición)."""
    out = []
    for ln in texto.splitlines():
        for m in re.finditer(MARCA + r"\s*:\s*(.*)", ln):
            q = m.group(1).strip().rstrip("*`_ ").strip()
            out.append(q)
    return out


def cmd_plantilla():
    subs = subsistemas()
    problemas, filas = [], []
    faltan = [l for l in "ABCDEFGHI" if l not in subs]
    for l in faltan:
        problemas.append(f"falta el capítulo del subsistema {l}")
    for letra, ruta in subs.items():
        texto = ruta.read_text(encoding="utf-8")
        p = []
        h1 = [ln for ln in texto.splitlines() if ln.startswith("# ")]
        if len(h1) != 1 or not re.match(rf"^# Subsistema {letra} · .+", h1[0]):
            p.append(f"el título debe ser «# Subsistema {letra} · Nombre» (único): {h1[:2]}")
        cuerpos, enc = secciones(texto)
        esperados = [(letra, i + 1, t) for i, t in enumerate(ENCABEZADOS)]
        if enc != esperados:
            p.append("encabezados ## distintos de la plantilla: " + " | ".join(
                f"{a or '?'}.{b or '?'} {c}" for a, b, c in enc))
        # enlaces Markdown (el PDF no los sigue y el verificador los revisa)
        en_codigo, sec_codigo = False, []
        for i, ln in enumerate(texto.splitlines(), 1):
            if ln.strip().startswith("```"):
                en_codigo = not en_codigo
                continue
            if en_codigo:
                if len(ln.expandtabs(4).rstrip()) > MAX_ANCHO_CODIGO:
                    sec_codigo.append(i)
            elif re.search(r"\[[^\]]+\]\([^)]+\)", ln) or "[FIGURA:" in ln:
                p.append(f"línea {i}: enlace Markdown o [FIGURA:] (usa rutas en backticks)")
        if sec_codigo:
            p.append(f"líneas de código de más de {MAX_ANCHO_CODIGO} caracteres: {sec_codigo[:8]}")
        if en_codigo:
            p.append("bloque de código sin cerrar")
        n_pal = palabras(texto)
        if n_pal < 1500:
            p.append(f"muy corto: {n_pal} palabras (mínimo 1,500)")
        if n_pal > 3600:
            p.append(f"muy largo: {n_pal} palabras (máximo 3,600)")
        terminos = bullets_termino(cuerpos.get(6, ""))
        if not 8 <= len(terminos) <= 14:
            p.append(f"X.6 debe tener de 8 a 14 términos «- **término**: definición» (tiene {len(terminos)})")
        preg = preguntas(cuerpos.get(7, ""))
        if not 3 <= len(preg) <= 5:
            p.append(f"X.7 debe tener de 3 a 5 preguntas «1. **¿...?** respuesta» (tiene {len(preg)})")
        s4 = cuerpos.get(4, "")
        if "|" not in s4:
            p.append("X.4 debe incluir la tabla «Qué se probó | Cómo | Resultado | Etiqueta»")
        if not any(e in s4 for e in ETIQUETAS):
            p.append("X.4 no usa ninguna etiqueta de evidencia (" + ", ".join(ETIQUETAS) + ")")
        marcas = marcas_confirmar(texto)
        for q in marcas:
            if len(q) < 15:
                p.append(f"«{MARCA}» sin pregunta concreta: «{q}»")
        externas = [u for u in re.findall(r"https?://[^\s)`'\"]+", texto)
                    if not re.match(r"https?://(192\.168\.|127\.0\.0\.1|localhost|10\.)", u)]
        if externas:
            p.append("contiene una URL externa (no cites fuentes que no se hayan abierto): " + externas[0])
        filas.append((letra, ruta.name, n_pal, len(marcas), len(terminos), len(preg), "OK" if not p else f"{len(p)} problema(s)"))
        for x in p:
            problemas.append(f"{letra} ({ruta.name}): {x}")
    print(f"{'Sub':<4}{'Archivo':<44}{'Palabras':>9}{'CONFIRMAR':>11}{'Términos':>10}{'Preguntas':>11}  Estado")
    for f in filas:
        print(f"{f[0]:<4}{f[1]:<44}{f[2]:>9}{f[3]:>11}{f[4]:>10}{f[5]:>11}  {f[6]}")
    if problemas:
        print("\nPROBLEMAS:")
        for x in problemas:
            print(" -", x)
        return 1
    print("\nPlantilla completa en los 9 subsistemas (A a I).")
    return 0


def cmd_pendientes():
    fuentes = []
    for ruta in sorted(CAPS.glob("*.md")):
        fuentes.append(ruta)
    if GUIA.exists():
        fuentes.append(GUIA)
    filas, conteo = [], {}
    for ruta in fuentes:
        texto = ruta.read_text(encoding="utf-8")
        if ruta.parent == CAPS and re.match(r"^01[d-l]_subsistema_([A-I])_", ruta.name):
            letra = re.match(r"^01[d-l]_subsistema_([A-I])_", ruta.name).group(1)
            titulo = re.search(r"^# (.+)$", texto, flags=re.M).group(1).strip()
            etiqueta = titulo
        elif ruta == GUIA:
            etiqueta = "Guía de exposición"
        else:
            m = re.search(r"^# (.+)$", texto, flags=re.M)
            etiqueta = m.group(1).strip() if m else ruta.stem
        for q in marcas_confirmar(texto):
            filas.append((etiqueta, str(ruta.relative_to(RAIZ)).replace("\\", "/"), q))
            conteo[etiqueta] = conteo.get(etiqueta, 0) + 1
    lineas = [
        "# Pendientes por confirmar con el equipo (v1.2)",
        "",
        f"Generado con `python docs/manual/revisar_manual.py pendientes`. Cada fila es una pregunta que NO se pudo "
        f"responder con el código ni con los documentos del repositorio y que quedó marcada en el texto como "
        f"`{MARCA}`. Cuando el equipo la responda: corregir el capítulo, borrar la marca y volver a correr el comando.",
        "",
        f"Total: **{len(filas)}** preguntas en {len(conteo)} documentos.",
        "",
        "## Resumen por capítulo",
        "",
        "| Capítulo | Preguntas |",
        "|---|---|",
    ]
    for etiqueta, n in conteo.items():
        lineas.append(f"| {etiqueta.replace('|', '/')} | {n} |")
    lineas += ["", "## Lista completa", "", "| # | Capítulo | Archivo | Pregunta |", "|---|---|---|---|"]
    for i, (etiqueta, archivo, q) in enumerate(filas, 1):
        lineas.append(f"| {i} | {etiqueta.replace('|', '/')} | `{archivo}` | {q.replace('|', '/')} |")
    if not filas:
        lineas.append("| – | – | – | No quedó ninguna pregunta pendiente. |")
    PENDIENTES.write_text("\n".join(lineas) + "\n", encoding="utf-8")
    print(f"escrito {PENDIENTES.relative_to(RAIZ)}: {len(filas)} preguntas")
    for etiqueta, n in conteo.items():
        print(f"  {n:>3}  {etiqueta}")
    return 0


def clave_orden(termino):
    t = termino.replace("`", "").strip()
    t = unicodedata.normalize("NFKD", t).encode("ascii", "ignore").decode("ascii").lower()
    return re.sub(r"^[^a-z0-9]+", "", t) or t


def cmd_glosario():
    subs = subsistemas()
    if not subs:
        print("no hay capítulos de subsistema")
        return 1
    entradas = {}   # clave normalizada -> {"termino", "definicion", "donde": [letras]}
    for letra in "ABCDEFGHI":
        ruta = subs.get(letra)
        if not ruta:
            continue
        cuerpos, _ = secciones(ruta.read_text(encoding="utf-8"))
        for termino, definicion in bullets_termino(cuerpos.get(6, "")):
            k = clave_orden(re.sub(r"\s*\(.*?\)", "", termino))   # «AYUDA (NAV_AYUDA)» y «AYUDA» son el mismo término
            if k not in entradas:
                entradas[k] = {"termino": termino, "definicion": definicion, "donde": [letra]}
            elif letra not in entradas[k]["donde"]:
                entradas[k]["donde"].append(letra)
    orden = sorted(entradas, key=lambda k: (not k[:1].isalpha(), k))
    lineas = [
        "# Glosario general",
        "",
        "Todos los términos clave de los subsistemas A a I en orden alfabético. Al final de cada término "
        "se indica la sección donde se explica y la página del manual (el número es un enlace dentro del PDF); "
        "si aparece en varios subsistemas, la definición es la del primero.",
        "",
        "## Términos",
        "",
    ]
    grupo = None
    for k in orden:
        e = entradas[k]
        g = k[0].upper() if k[:1].isalpha() else "0-9"
        if g != grupo:
            grupo = g
            lineas += ["", f"### {g}", ""]
        donde = ", ".join(f"{l}.6 p. {{{{pag:{l}.6}}}}" for l in e["donde"])
        lineas.append(f"- **{e['termino']}**: {e['definicion']} *({donde})*")
    GLOSARIO.write_text("\n".join(lineas) + "\n", encoding="utf-8")
    print(f"escrito {GLOSARIO.relative_to(RAIZ)}: {len(entradas)} términos distintos "
          f"({sum(len(e['donde']) for e in entradas.values())} apariciones en A a I)")
    return 0


def main():
    cmds = {"plantilla": cmd_plantilla, "pendientes": cmd_pendientes, "glosario": cmd_glosario}
    if len(sys.argv) != 2 or sys.argv[1] not in cmds:
        print(__doc__)
        return 2
    return cmds[sys.argv[1]]()


if __name__ == "__main__":
    sys.exit(main())
