#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generar_manual.py — Genera el Manual Técnico del Carro FSI (Base v1.1 · revisión pendiente).

Uso reproducible en un solo comando:
    python docs/manual/generar_manual.py

Produce:
    docs/manual/Manual_Proyecto_Carro_FSI.pdf
"""
from pathlib import Path
import html
import re
import subprocess
import sys

from PIL import Image as PILImage
import matplotlib
import pymupdf
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    Image,
    KeepTogether,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = Path(__file__).resolve().parents[2]
MANUAL = ROOT / "docs" / "manual"
CAPITULOS = MANUAL / "capitulos"
FIG = ROOT / "docs" / "figuras" / "v2"

# Registro de fuentes DejaVu con soporte completo de acentos y símbolos matemáticos Unicode
FONTS_DIR = Path(matplotlib.get_data_path()) / "fonts" / "ttf"
for font_name, ttf_file in [
    ("DejaVu", "DejaVuSans.ttf"),
    ("DejaVu-Bold", "DejaVuSans-Bold.ttf"),
    ("DejaVu-Oblique", "DejaVuSans-Oblique.ttf"),
    ("DejaVu-BoldOblique", "DejaVuSans-BoldOblique.ttf"),
    ("DejaVuMono", "DejaVuSansMono.ttf"),
    ("DejaVuMono-Bold", "DejaVuSansMono-Bold.ttf"),
]:
    pdfmetrics.registerFont(TTFont(font_name, str(FONTS_DIR / ttf_file)))

pdfmetrics.registerFontFamily(
    "DejaVu",
    normal="DejaVu",
    bold="DejaVu-Bold",
    italic="DejaVu-Oblique",
    boldItalic="DejaVu-BoldOblique",
)
pdfmetrics.registerFontFamily(
    "DejaVuMono",
    normal="DejaVuMono",
    bold="DejaVuMono-Bold",
    italic="DejaVuMono",
    boldItalic="DejaVuMono-Bold",
)

# Paleta institucional (coincide con docs/figuras/generar_figuras_v2.py)
AZUL = colors.HexColor("#163c60")
VERDE = colors.HexColor("#00877e")
BOSQUE = colors.HexColor("#2d6a4f")
TEXTO = colors.HexColor("#222f38")
GRIS = colors.HexColor("#52606d")
FONDO_TABLA_CAB = colors.HexColor("#163c60")
FONDO_TABLA_PAR = colors.HexColor("#f4f8fa")
BORDE_TABLA = colors.HexColor("#c4d4de")
FONDO_NOTA = colors.HexColor("#edf6f5")
FONDO_CODIGO = colors.HexColor("#f3f5f7")

# Márgenes de ~2.1 cm (60 pt) en hoja A4 (595.27 x 841.89 pt) -> ancho útil ~475 pt
MARGEN_X = 60
MARGEN_TOP = 50
MARGEN_BOT = 46
ANCHO_UTIL = A4[0] - 2 * MARGEN_X

STYLES = {
    "body": ParagraphStyle(
        "body",
        fontName="DejaVu",
        fontSize=9.6,
        leading=13.0,
        textColor=TEXTO,
        alignment=TA_JUSTIFY,
        spaceAfter=4.4,
        allowWidows=0,
        allowOrphans=0,
    ),
    "bullet": ParagraphStyle(
        "bullet",
        fontName="DejaVu",
        fontSize=9.4,
        leading=12.6,
        textColor=TEXTO,
        alignment=TA_LEFT,
        leftIndent=13,
        firstLineIndent=-8,
        spaceAfter=3.1,
        allowWidows=0,
        allowOrphans=0,
    ),
    "h1": ParagraphStyle(
        "h1",
        fontName="DejaVu-Bold",
        fontSize=15.0,
        leading=18.8,
        textColor=AZUL,
        spaceBefore=0,
        spaceAfter=6.5,
        keepWithNext=True,
    ),
    "h2": ParagraphStyle(
        "h2",
        fontName="DejaVu-Bold",
        fontSize=11.5,
        leading=14.6,
        textColor=VERDE,
        spaceBefore=7.2,
        spaceAfter=3.6,
        keepWithNext=True,
    ),
    "h3": ParagraphStyle(
        "h3",
        fontName="DejaVu-Bold",
        fontSize=10.0,
        leading=13.2,
        textColor=BOSQUE,
        spaceBefore=5.5,
        spaceAfter=2.6,
        keepWithNext=True,
    ),
    "formula": ParagraphStyle(
        "formula",
        fontName="DejaVuMono",
        fontSize=8.9,
        leading=12.6,
        textColor=AZUL,
        alignment=TA_CENTER,
        backColor=FONDO_CODIGO,
        borderPadding=4,
        spaceBefore=4,
        spaceAfter=5.5,
        keepWithNext=False,
    ),
    "codeblock": ParagraphStyle(
        "codeblock",
        fontName="DejaVuMono",
        fontSize=8.2,
        leading=11.4,
        textColor=AZUL,
        alignment=TA_LEFT,
        backColor=FONDO_CODIGO,
        leftIndent=8,
        rightIndent=8,
        borderPadding=4,
        spaceBefore=3,
        spaceAfter=5.0,
    ),
    "quote": ParagraphStyle(
        "quote",
        fontName="DejaVu",
        fontSize=9.1,
        leading=12.8,
        textColor=AZUL,
        alignment=TA_LEFT,
        backColor=FONDO_NOTA,
        leftIndent=9,
        rightIndent=9,
        borderPadding=5,
        spaceBefore=4,
        spaceAfter=5.5,
    ),
    "code_ref": ParagraphStyle(
        "code_ref",
        fontName="DejaVu-Oblique",
        fontSize=8.7,
        leading=12.0,
        textColor=GRIS,
        alignment=TA_LEFT,
        spaceBefore=5,
        spaceAfter=2,
    ),
    "caption": ParagraphStyle(
        "caption",
        fontName="DejaVu-Oblique",
        fontSize=8.4,
        leading=11.5,
        textColor=AZUL,
        alignment=TA_CENTER,
        spaceBefore=3,
        spaceAfter=6.5,
    ),
    "th": ParagraphStyle(
        "th",
        fontName="DejaVu-Bold",
        fontSize=8.2,
        leading=10.8,
        textColor=colors.white,
        alignment=TA_LEFT,
    ),
    "td": ParagraphStyle(
        "td",
        fontName="DejaVu",
        fontSize=8.0,
        leading=10.7,
        textColor=TEXTO,
        alignment=TA_LEFT,
    ),
    "cover_inst": ParagraphStyle(
        "cover_inst",
        fontName="DejaVu-Bold",
        fontSize=12.5,
        leading=17,
        textColor=AZUL,
        alignment=TA_CENTER,
        spaceAfter=4,
    ),
    "cover_div": ParagraphStyle(
        "cover_div",
        fontName="DejaVu",
        fontSize=10.5,
        leading=15,
        textColor=GRIS,
        alignment=TA_CENTER,
        spaceAfter=28,
    ),
    "cover_title": ParagraphStyle(
        "cover_title",
        fontName="DejaVu-Bold",
        fontSize=24,
        leading=30,
        textColor=AZUL,
        alignment=TA_CENTER,
        spaceAfter=12,
    ),
    "cover_sub": ParagraphStyle(
        "cover_sub",
        fontName="DejaVu",
        fontSize=12.0,
        leading=17,
        textColor=VERDE,
        alignment=TA_CENTER,
        spaceAfter=16,
    ),
    "cover_ver": ParagraphStyle(
        "cover_ver",
        fontName="DejaVu-Bold",
        fontSize=13.0,
        leading=18,
        textColor=BOSQUE,
        alignment=TA_CENTER,
        spaceAfter=26,
    ),
    "cover_meta": ParagraphStyle(
        "cover_meta",
        fontName="DejaVu",
        fontSize=10.5,
        leading=16,
        textColor=TEXTO,
        alignment=TA_CENTER,
        spaceAfter=8,
    ),
}


def git(*args):
    try:
        return subprocess.check_output(
            ["git", *args], cwd=ROOT, text=True, stderr=subprocess.DEVNULL
        ).strip()
    except Exception:
        return "v1.1"


def limpiar_latex_inline(expr):
    """Convierte expresiones matemáticas simples de LaTeX a notación Unicode limpia."""
    s = expr.strip()
    reemplazos_literales = [
        (r"\left(", "("),
        (r"\right)", ")"),
        (r"\left[", "["),
        (r"\right]", "]"),
        (r"\leftrightarrow", "↔"),
        (r"\leftarrow", "←"),
        (r"\rightarrow", "→"),
        (r"\Rightarrow", "⇒"),
        (r"\qquad", "    "),
        (r"\quad", "  "),
        (r"\cdot", "·"),
        (r"\times", "×"),
        (r"\pm", "±"),
        (r"\neq", "≠"),
        (r"\approx", "≈"),
        (r"\le", "≤"),
        (r"\ge", "≥"),
        (r"\in", "∈"),
        (r"\Delta", "Δ"),
        (r"\theta", "θ"),
        (r"\omega", "ω"),
        (r"\sigma", "σ"),
        (r"\pi", "π"),
        (r"\mu", "μ"),
        (r"\mathcal{N}", "N"),
        (r"\hat{d}", "d̂"),
        (r"\sin", "sin"),
        (r"\cos", "cos"),
        (r"\max", "max"),
        (r"\min", "min"),
        ("{,}", ","),
        (r"\,", " "),
        (r"\ ", " "),
        (r"\_", "_"),
        (r"^\circ", "°"),
    ]
    for pat, rep in reemplazos_literales:
        s = s.replace(pat, rep)
    s = re.sub(r"\\text\{([^}]*)\}", r"\1", s)
    s = re.sub(r"\\sqrt\{([^}]*)\}", r"√(\1)", s)
    # Subíndices y superíndices simples (primero para desanidar llaves dentro de \frac)
    s = re.sub(r"_\{([^{}]+)\}", r"_\1", s)
    s = re.sub(r"\^\{([^{}]+)\}", r"^\1", s)
    # Fracciones simples \frac{a}{b} -> (a) / (b)
    while r"\frac{" in s:
        s_new = re.sub(r"\\frac\{([^{}]+)\}\{([^{}]+)\}", r"(\1) / (\2)", s)
        if s_new == s:
            break
        s = s_new
    s = s.replace("{", "").replace("}", "")
    return s


def formatear_inline(texto):
    """Convierte Markdown inline (**negrita**, *cursiva*, `código`, $math$) a XML de ReportLab."""
    # Primero sustituir bloques matemáticos inline $...$
    partes_math = []

    def _guardar_math(m):
        idx = len(partes_math)
        partes_math.append(limpiar_latex_inline(m.group(1)))
        return f"@@MATH{idx}@@"

    texto = re.sub(r"\$([^$]+)\$", _guardar_math, texto)

    # Extraer spans de código `...` para no alterar guiones bajos ni asteriscos dentro de ellos
    partes_code = []

    def _guardar_code(m):
        idx = len(partes_code)
        partes_code.append(html.escape(m.group(1)))
        return f"@@CODE{idx}@@"

    texto = re.sub(r"`([^`]+)`", _guardar_code, texto)

    # Escapar HTML del resto
    s = html.escape(texto)

    # Saltos de línea explícitos <br> escritos en tablas
    s = s.replace("&lt;br&gt;", "<br/>").replace("&lt;br/&gt;", "<br/>")

    # Negrita + cursiva al final de negrita (**...*...***), negrita, cursiva
    s = re.sub(r"\*\*\*([^*]+?)\*\*\*", r"<b><i>\1</i></b>", s)
    s = re.sub(r"\*\*([^*]+?)\*([^*]+?)\*\*\*", r"<b>\1<i>\2</i></b>", s)
    s = re.sub(r"\*\*(.+?)\*\*", lambda m: "<b>" + re.sub(r"\*([^*]+?)\*", r"<i>\1</i>", m.group(1)) + "</b>", s)
    s = re.sub(r"(?<!\*)\*([^*]+?)\*(?!\*)", r"<i>\1</i>", s)

    # Restaurar código y matemáticas
    for idx, c in enumerate(partes_code):
        s = s.replace(f"@@CODE{idx}@@", f'<font name="DejaVuMono" size="9.1">{c}</font>')
    for idx, m_str in enumerate(partes_math):
        s = s.replace(
            f"@@MATH{idx}@@",
            f'<font name="DejaVuMono" color="#163c60">{html.escape(m_str)}</font>',
        )
    return s


def imagen_escalada(ruta, max_w, max_h):
    with PILImage.open(ruta) as im:
        w, h = im.size
    escala = min(max_w / w, max_h / h)
    return Image(str(ruta), width=w * escala, height=h * escala, hAlign="CENTER")


class ManualDocTemplate(SimpleDocTemplate):
    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and hasattr(flowable, "_outline"):
            level, key, in_toc = flowable._outline
            texto = flowable.getPlainText()
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(texto, key, level=level, closed=(level > 0))
            if in_toc and level <= 1:
                self.notify("TOCEntry", (level, texto, self.page, key))


def crear_encabezado(texto, nivel, key, in_toc=True):
    estilo = STYLES["h1"] if nivel == 0 else (STYLES["h2"] if nivel == 1 else STYLES["h3"])
    p = Paragraph(formatear_inline(texto), estilo)
    p._outline = (nivel, key, in_toc)
    return p


def dibujar_marco_pagina(canvas, doc):
    canvas.setTitle("Manual técnico del Carro FSI (LIDIA) — Base v1.1 · revisión pendiente")
    canvas.setAuthor(
        "Jaime Fernando Guerrero Segoviano; Melanie Contreras Solis; "
        "Vitto Eduardo Rangel Fortuna; Azul Abril Cadena Campos"
    )
    canvas.setSubject(
        "Proyecto final FSI — Arquitectura, firmware, visión, navegación en C99 y validación J16"
    )
    canvas.setCreator("docs/manual/generar_manual.py — Universidad de Guanajuato")
    if doc.page > 1:
        canvas.saveState()
        canvas.setFont("DejaVu", 8.0)
        canvas.setFillColor(AZUL)
        canvas.drawString(
            doc.leftMargin,
            A4[1] - 36,
            "Carro FSI (LIDIA) · Manual técnico · Base v1.1 · revisión pendiente",
        )
        canvas.setStrokeColor(BORDE_TABLA)
        canvas.setLineWidth(0.6)
        canvas.line(doc.leftMargin, A4[1] - 41, A4[0] - doc.rightMargin, A4[1] - 41)
        canvas.drawRightString(A4[0] - doc.rightMargin, 32, f"Página {doc.page}")
        canvas.restoreState()


def construir_portada(story):
    rama = git("branch", "--show-current")
    story.append(Spacer(1, 28))
    story.append(Paragraph("UNIVERSIDAD DE GUANAJUATO", STYLES["cover_inst"]))
    story.append(
        Paragraph(
            "División de Ingenierías · Campus Irapuato-Salamanca (DICIS)",
            STYLES["cover_div"],
        )
    )
    # Línea decorativa superior
    linea_sup = Table([[""]], colWidths=[ANCHO_UTIL * 0.82], rowHeights=[2])
    linea_sup.setStyle(
        TableStyle([("BACKGROUND", (0, 0), (-1, -1), AZUL), ("ALIGN", (0, 0), (-1, -1), "CENTER")])
    )
    story.append(linea_sup)
    story.append(Spacer(1, 22))
    story.append(
        Paragraph("Manual Técnico del<br/>Proyecto Carro FSI (LIDIA)", STYLES["cover_title"])
    )
    story.append(
        Paragraph(
            "Arquitectura distribuida, firmware de control y visión, núcleo de navegación en C99,<br/>"
            "aplicación Android y evidencia de cierre J16",
            STYLES["cover_sub"],
        )
    )
    story.append(Paragraph("Base v1.1 · revisión pendiente", STYLES["cover_ver"]))
    story.append(Paragraph("11-nov-2026: esquive confirmado, NO MEDIDO en el carro.\nNo constituye una publicación v1.2.", STYLES["cover_sub"]))
    linea_inf = Table([[""]], colWidths=[ANCHO_UTIL * 0.55], rowHeights=[1.5])
    linea_inf.setStyle(
        TableStyle([("BACKGROUND", (0, 0), (-1, -1), VERDE), ("ALIGN", (0, 0), (-1, -1), "CENTER")])
    )
    story.append(linea_inf)
    story.append(Spacer(1, 28))

    # Caja institucional de metadatos
    filas_meta = [
        [
            Paragraph("<b>Materia:</b>", STYLES["td"]),
            Paragraph("Fundamentos de los Sistemas de Información (FSI)", STYLES["td"]),
        ],
        [
            Paragraph("<b>Profesor titular:</b>", STYLES["td"]),
            Paragraph("Dr. Horacio Rostro González", STYLES["td"]),
        ],
        [
            Paragraph("<b>Integrantes del equipo:</b>", STYLES["td"]),
            Paragraph(
                "Jaime Fernando Guerrero Segoviano<br/>"
                "Melanie Contreras Solis<br/>"
                "Vitto Eduardo Rangel Fortuna<br/>"
                "Azul Abril Cadena Campos",
                STYLES["td"],
            ),
        ],
        [
            Paragraph("<b>Cierre físico histórico:</b>", STYLES["td"]),
            Paragraph("10 de noviembre de 2026", STYLES["td"]),
        ],
        [
            Paragraph("<b>Artefactos históricos:</b>", STYLES["td"]),
            Paragraph(
                '<font name="DejaVuMono" size="8.2">APP:0.4-6d1b213</font> · '
                '<font name="DejaVuMono" size="8.2">FW:6d1b213-20261110</font>',
                STYLES["td"],
            ),
        ],
        [
            Paragraph("<b>Revisión del repositorio:</b>", STYLES["td"]),
            Paragraph(
                '<font name="DejaVuMono" size="8.2">Base v1.1: bd1727b; revisión 11-nov-2026</font><br/>'
                f"{html.escape(rama)} (pendiente de validación física)",
                STYLES["td"],
            ),
        ],
    ]
    tabla_meta = Table(filas_meta, colWidths=[135, ANCHO_UTIL - 165], hAlign="CENTER")
    tabla_meta.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), FONDO_TABLA_PAR),
                ("BOX", (0, 0), (-1, -1), 0.8, BORDE_TABLA),
                ("INNERGRID", (0, 0), (-1, -1), 0.4, BORDE_TABLA),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 8),
                ("RIGHTPADDING", (0, 0), (-1, -1), 8),
                ("TOPPADDING", (0, 0), (-1, -1), 6),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
            ]
        )
    )
    story.append(tabla_meta)
    story.append(PageBreak())


def agregar_tabla_markdown(story, lineas_tabla, num_tabla):
    filas_crudas = []
    for ln in lineas_tabla:
        ln_s = ln.strip()
        if not ln_s.startswith("|"):
            continue
        celdas = [c.strip() for c in ln_s.strip("|").split("|")]
        # Ignorar fila separadora |---|---|
        if all(re.match(r"^:?-{2,}:?$", c) for c in celdas):
            continue
        filas_crudas.append(celdas)
    if len(filas_crudas) < 2:
        return num_tabla

    ncols = len(filas_crudas[0])
    # Calcular pesos de ancho de columna según la longitud máxima de texto en cada columna
    pesos = []
    for c_idx in range(ncols):
        max_len = max(
            (len(re.sub(r"[*`]", "", fila[c_idx])) if c_idx < len(fila) else 8)
            for fila in filas_crudas
        )
        # Raíz cuadrada amortigua columnas muy largas para que no aplasten a las cortas
        pesos.append(max(10.0, (max_len ** 0.62)))
    suma_pesos = sum(pesos)
    col_widths = [ANCHO_UTIL * (p / suma_pesos) for p in pesos]

    datos = []
    for r_idx, fila in enumerate(filas_crudas):
        estilo_celda = STYLES["th"] if r_idx == 0 else STYLES["td"]
        fila_platypus = []
        for c_idx in range(ncols):
            txt = fila[c_idx] if c_idx < len(fila) else ""
            fila_platypus.append(Paragraph(formatear_inline(txt), estilo_celda))
        datos.append(fila_platypus)

    t = Table(datos, colWidths=col_widths, repeatRows=1, splitByRow=1, hAlign="CENTER")
    estilos_t = [
        ("BACKGROUND", (0, 0), (-1, 0), FONDO_TABLA_CAB),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("GRID", (0, 0), (-1, -1), 0.45, BORDE_TABLA),
        ("LEFTPADDING", (0, 0), (-1, -1), 4.5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4.5),
        ("TOPPADDING", (0, 0), (-1, -1), 3.2),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3.6),
    ]
    for r_idx in range(1, len(datos)):
        if r_idx % 2 == 0:
            estilos_t.append(("BACKGROUND", (0, r_idx), (-1, r_idx), FONDO_TABLA_PAR))
    t.setStyle(TableStyle(estilos_t))

    num_tabla += 1
    titulo_cab = " · ".join(c.replace("*", "").replace("`", "") for c in filas_crudas[0][:2])
    story.append(Spacer(1, 2))
    story.append(t)
    story.append(
        Paragraph(
            f"<b>Tabla {num_tabla}.</b> Resumen estructurado ({html.escape(titulo_cab)}).",
            STYLES["caption"],
        )
    )
    return num_tabla


def agregar_figura(story, slug, pie_txt, num_fig):
    ruta_img = FIG / f"{slug}.png"
    if not ruta_img.exists():
        raise FileNotFoundError(f"No existe la figura requerida: {ruta_img}")
    num_fig += 1
    max_h = 220 if slug.startswith("lamina_") else 156
    img = imagen_escalada(ruta_img, ANCHO_UTIL - 6, max_h)
    cap = Paragraph(
        f"<b>Figura {num_fig}.</b> {formatear_inline(pie_txt)}",
        STYLES["caption"],
    )
    story.append(Spacer(1, 2.0))
    story.append(KeepTogether([img, cap]))
    return num_fig


def procesar_capitulo(story, idx_cap, ruta_md, num_fig, num_tabla):
    # Cada capítulo inicia siempre en página nueva
    story.append(PageBreak())

    lineas = ruta_md.read_text(encoding="utf-8").splitlines()
    i = 0
    n = len(lineas)
    num_sec = 0
    num_subsec = 0

    while i < n:
        ln = lineas[i].rstrip()
        ln_strip = ln.strip()

        if not ln_strip:
            i += 1
            continue

        # Encabezados
        if ln_strip.startswith("# "):
            titulo = ln_strip[2:].strip()
            story.append(crear_encabezado(titulo, 0, f"cap{idx_cap}"))
            i += 1
            continue
        if ln_strip.startswith("## "):
            num_sec += 1
            num_subsec = 0
            titulo = ln_strip[3:].strip()
            story.append(crear_encabezado(titulo, 1, f"cap{idx_cap}_s{num_sec}"))
            i += 1
            continue
        if ln_strip.startswith("### "):
            num_subsec += 1
            titulo = ln_strip[4:].strip()
            story.append(crear_encabezado(titulo, 2, f"cap{idx_cap}_s{num_sec}_{num_subsec}"))
            i += 1
            continue
        if ln_strip.startswith("#### "):
            num_subsec += 1
            titulo = ln_strip[5:].strip()
            story.append(crear_encabezado(titulo, 2, f"cap{idx_cap}_s{num_sec}_{num_subsec}"))
            i += 1
            continue

        # Marcador inline de figura: [FIGURA:slug|Pie de figura]
        m_fig = re.match(r"^\[FIGURA:([^|\]]+)\|([^\]]+)\]$", ln_strip)
        if m_fig:
            slug, pie_txt = m_fig.group(1).strip(), m_fig.group(2).strip()
            num_fig = agregar_figura(story, slug, pie_txt, num_fig)
            i += 1
            continue

        # Bloque de código ```...```
        if ln_strip.startswith("```"):
            i += 1
            cod_lineas = []
            while i < n and not lineas[i].strip().startswith("```"):
                cod_lineas.append(html.escape(lineas[i].rstrip()))
                i += 1
            if i < n:
                i += 1
            if cod_lineas:
                story.append(Paragraph("<br/>".join(cod_lineas), STYLES["codeblock"]))
            continue

        # Fórmulas en bloque $$...$$
        if ln_strip.startswith("$$"):
            expr_lineas = [ln_strip]
            if not (ln_strip.endswith("$$") and len(ln_strip) > 4):
                i += 1
                while i < n:
                    expr_lineas.append(lineas[i].strip())
                    if lineas[i].strip().endswith("$$"):
                        break
                    i += 1
            i += 1
            expr_bruta = " ".join(expr_lineas).strip("$").strip()
            expr_limpia = limpiar_latex_inline(expr_bruta)
            story.append(Paragraph(html.escape(expr_limpia), STYLES["formula"]))
            continue

        # Tablas Markdown |...|
        if ln_strip.startswith("|"):
            lineas_tabla = []
            while i < n and lineas[i].strip().startswith("|"):
                lineas_tabla.append(lineas[i])
                i += 1
            num_tabla = agregar_tabla_markdown(story, lineas_tabla, num_tabla)
            continue

        # Citas / Notas > ...
        if ln_strip.startswith(">"):
            cita_lineas = []
            while i < n and lineas[i].strip().startswith(">"):
                cita_lineas.append(lineas[i].strip().lstrip(">").strip())
                i += 1
            story.append(Paragraph(formatear_inline(" ".join(cita_lineas)), STYLES["quote"]))
            continue

        # Viñetas y listas numeradas
        m_ul = re.match(r"^[-*]\s+(.+)$", ln_strip)
        m_ol = re.match(r"^(\d+\.)\s+(.+)$", ln_strip)
        if m_ul or m_ol:
            prefijo = "• " if m_ul else f"<b>{m_ol.group(1)}</b> "
            contenido_item = [m_ul.group(1) if m_ul else m_ol.group(2)]
            i += 1
            while i < n:
                sig = lineas[i].strip()
                if (
                    not sig
                    or sig.startswith("#")
                    or sig.startswith("|")
                    or sig.startswith("```")
                    or sig.startswith("$$")
                    or sig.startswith(">")
                    or sig.startswith("[FIGURA:")
                    or re.match(r"^[-*]\s+", sig)
                    or re.match(r"^\d+\.\s+", sig)
                ):
                    break
                contenido_item.append(sig)
                i += 1
            story.append(
                Paragraph(prefijo + formatear_inline(" ".join(contenido_item)), STYLES["bullet"])
            )
            continue

        # Línea final "Dónde está en el código:"
        if ln_strip.startswith("Dónde está en el código:"):
            story.append(Paragraph(formatear_inline(ln_strip), STYLES["code_ref"]))
            i += 1
            continue

        # Párrafo normal (acumular líneas contiguas del mismo párrafo)
        par_lineas = [ln_strip]
        i += 1
        while i < n:
            sig = lineas[i].strip()
            if (
                not sig
                or sig.startswith("#")
                or sig.startswith("|")
                or sig.startswith("```")
                or sig.startswith("$$")
                or sig.startswith(">")
                or sig.startswith("[FIGURA:")
                or sig.startswith("Dónde está en el código:")
                or re.match(r"^[-*]\s+", sig)
                or re.match(r"^\d+\.\s+", sig)
            ):
                break
            par_lineas.append(sig)
            i += 1
        story.append(Paragraph(formatear_inline(" ".join(par_lineas)), STYLES["body"]))

    return num_fig, num_tabla


def limpiar_helvetica_sin_uso(ruta_pdf):
    """Retira el recurso inicial /F1 Helvetica que ReportLab declara por defecto sin usar."""
    pdf = pymupdf.open(ruta_pdf)
    fuentes_p0 = pdf[0].get_fonts()
    helv = [f[0] for f in fuentes_p0 if f[3] == "Helvetica" and f[4] == "F1"]
    if helv:
        h_xref = helv[0]
        for page in pdf:
            for xref in page.get_contents():
                data = pdf.xref_stream(xref)
                data = re.sub(rb"BT /F1 [\d.]+ Tf [\d.]+ TL ET", b"", data)
                if re.search(rb"/F1(?:\s|$)", data):
                    raise RuntimeError("Se encontró texto en Helvetica no incrustada (/F1)")
                pdf.update_stream(xref, data)
        for xref in range(1, pdf.xref_length()):
            if pdf.xref_get_key(xref, "F1") == ("xref", f"{h_xref} 0 R"):
                obj = pdf.xref_object(xref)
                obj = re.sub(r"/F1\s+" + str(h_xref) + r"\s+0\s+R\b", "", obj)
                pdf.update_object(xref, obj)
    tmp = ruta_pdf.with_suffix(".tmp.pdf")
    pdf.save(tmp, garbage=4, deflate=True)
    pdf.close()
    tmp.replace(ruta_pdf)


def main():
    # --sin-figuras conserva las láminas existentes en revisiones solo de texto.
    if "--sin-figuras" not in sys.argv:
        subprocess.run(
            [sys.executable, str(ROOT / "docs" / "figuras" / "generar_figuras.py")],
            cwd=ROOT,
            check=True,
        )

    out_pdf = MANUAL / "Manual_Proyecto_Carro_FSI.pdf"
    doc = ManualDocTemplate(
        str(out_pdf),
        pagesize=A4,
        leftMargin=MARGEN_X,
        rightMargin=MARGEN_X,
        topMargin=MARGEN_TOP,
        bottomMargin=MARGEN_BOT,
        title="Manual técnico del Carro FSI (LIDIA) — Base v1.1 · revisión pendiente",
    )

    story = []
    construir_portada(story)

    # Índice general automático (dos pasadas con multiBuild)
    story.append(crear_encabezado("Índice general", 0, "indice_general", in_toc=False))
    toc = TableOfContents()
    toc.levelStyles = [
        ParagraphStyle(
            "toc0",
            fontName="DejaVu-Bold",
            fontSize=8.8,
            leading=10.6,
            textColor=AZUL,
            leftIndent=0,
            firstLineIndent=0,
            spaceBefore=1.0,
        ),
        ParagraphStyle(
            "toc1",
            fontName="DejaVu",
            fontSize=8.1,
            leading=9.3,
            textColor=TEXTO,
            leftIndent=12,
            firstLineIndent=0,
        ),
    ]
    story.append(toc)

    num_fig = 0
    num_tabla = 0
    archivos_cap = sorted(CAPITULOS.glob("*.md"))
    for idx_cap, ruta_md in enumerate(archivos_cap, 1):
        num_fig, num_tabla = procesar_capitulo(story, idx_cap, ruta_md, num_fig, num_tabla)

    doc.multiBuild(story, onFirstPage=dibujar_marco_pagina, onLaterPages=dibujar_marco_pagina)
    limpiar_helvetica_sin_uso(out_pdf)

    pdf_check = pymupdf.open(out_pdf)
    num_paginas = len(pdf_check)
    pdf_check.close()
    print(
        f"PDF generado: {out_pdf} ({num_paginas} páginas, {num_fig} figuras, "
        f"{num_tabla} tablas, {out_pdf.stat().st_size / 1024:.1f} KB)"
    )


if __name__ == "__main__":
    main()
