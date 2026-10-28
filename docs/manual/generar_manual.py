#!/usr/bin/env python3
"""
generar_manual.py
Genera el manual consolidado en PDF: docs/manual/Manual_Proyecto_Carro_FSI.pdf
Utiliza ReportLab y los recursos existentes (Markdown, catálogo de figuras, lista de fotos).
"""

import os
import sys
import re
import html
from reportlab.lib.pagesizes import letter
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, PageBreak, Spacer, Table,
    TableStyle, Image as RLImage, KeepTogether, HRFlowable, Preformatted, Flowable
)
from reportlab.pdfgen import canvas

class PageTracker(Flowable):
    """Flowable invisible que registra en qué página física se ubica cada capítulo."""
    def __init__(self, key, tracker_dict):
        super().__init__()
        self.key = str(key)
        self.tracker_dict = tracker_dict
        self.width = 0
        self.height = 0

    def draw(self):
        self.tracker_dict[self.key] = self.canv._pageNumber

# Dimensiones en puntos (letter: 612 x 792 pt, margen 40 pt)
MARGEN = 40
ANCHO_UTIL = 612 - 2 * MARGEN  # 532 pt
ALTO_UTIL = 792 - 2 * MARGEN   # 712 pt

COLOR_PRIMARIO = colors.HexColor('#0d3b66')     # Azul institucional profundo
COLOR_SECUNDARIO = colors.HexColor('#008080')   # Teal técnico
COLOR_TEXTO = colors.HexColor('#212529')        # Gris oscuro carbón
COLOR_FONDO_CLARO = colors.HexColor('#f8f9fa')  # Fondo grisáceo suave
COLOR_BORDE = colors.HexColor('#ced4da')        # Borde sutil
COLOR_ALERTA = colors.HexColor('#d90429')       # Rojo para advertencias
COLOR_AVISO = colors.HexColor('#f77f00')        # Naranja de aviso
COLOR_VERDE = colors.HexColor('#2a9d8f')        # Verde de verificación

class NumberedCanvas(canvas.Canvas):
    """
    Canvas de doble pasada para calcular el total exacto de páginas
    y dibujar encabezados y pies consistentes con números de página reales.
    """
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            if self._pageNumber > 1:
                self.dibujar_encabezado_pie(num_pages)
            canvas.Canvas.showPage(self)
        canvas.Canvas.save(self)

    def dibujar_encabezado_pie(self, total_paginas):
        self.saveState()
        # Encabezado (superior)
        self.setFont('Helvetica', 8)
        self.setFillColor(colors.HexColor('#6c757d'))
        self.drawString(MARGEN, 792 - 28, 'Manual y Documentación Completa del Carro FSI — Materia FSI')
        self.setStrokeColor(COLOR_BORDE)
        self.setLineWidth(0.5)
        self.line(MARGEN, 792 - 32, 612 - MARGEN, 792 - 32)

        # Pie de página (inferior)
        self.line(MARGEN, 34, 612 - MARGEN, 34)
        self.drawString(MARGEN, 22, 'Entrega final: 1 de diciembre de 2026 · Dr. Horacio Rostro González')
        self.drawRightString(612 - MARGEN, 22, f'Página {self._pageNumber} de {total_paginas}')
        self.restoreState()

def crear_estilos():
    base = getSampleStyleSheet()

    estilos = {
        'PortadaTitulo': ParagraphStyle(
            'PortadaTitulo',
            parent=base['Title'],
            fontName='Helvetica-Bold',
            fontSize=24,
            leading=30,
            textColor=COLOR_PRIMARIO,
            alignment=1, # Centrado
            spaceAfter=15
        ),
        'PortadaSubtitulo': ParagraphStyle(
            'PortadaSubtitulo',
            parent=base['Normal'],
            fontName='Helvetica',
            fontSize=13,
            leading=18,
            textColor=COLOR_SECUNDARIO,
            alignment=1,
            spaceAfter=25
        ),
        'PortadaMetadatos': ParagraphStyle(
            'PortadaMetadatos',
            parent=base['Normal'],
            fontName='Helvetica',
            fontSize=10,
            leading=16,
            textColor=COLOR_TEXTO,
            alignment=1
        ),
        'TituloCapitulo': ParagraphStyle(
            'TituloCapitulo',
            parent=base['Heading1'],
            fontName='Helvetica-Bold',
            fontSize=17,
            leading=22,
            textColor=COLOR_PRIMARIO,
            spaceBefore=12,
            spaceAfter=10,
            keepWithNext=True
        ),
        'TituloSubseccion': ParagraphStyle(
            'TituloSubseccion',
            parent=base['Heading2'],
            fontName='Helvetica-Bold',
            fontSize=13,
            leading=17,
            textColor=COLOR_SECUNDARIO,
            spaceBefore=10,
            spaceAfter=6,
            keepWithNext=True
        ),
        'TituloSubsub': ParagraphStyle(
            'TituloSubsub',
            parent=base['Heading3'],
            fontName='Helvetica-Bold',
            fontSize=10.5,
            leading=14,
            textColor=COLOR_PRIMARIO,
            spaceBefore=8,
            spaceAfter=4,
            keepWithNext=True
        ),
        'Cuerpo': ParagraphStyle(
            'Cuerpo',
            parent=base['Normal'],
            fontName='Helvetica',
            fontSize=9.2,
            leading=13.5,
            textColor=COLOR_TEXTO,
            spaceAfter=6
        ),
        'CuerpoNegrita': ParagraphStyle(
            'CuerpoNegrita',
            parent=base['Normal'],
            fontName='Helvetica-Bold',
            fontSize=9.2,
            leading=13.5,
            textColor=COLOR_TEXTO,
            spaceAfter=6
        ),
        'ItemLista': ParagraphStyle(
            'ItemLista',
            parent=base['Normal'],
            fontName='Helvetica',
            fontSize=9.0,
            leading=13.0,
            textColor=COLOR_TEXTO,
            leftIndent=15,
            firstLineIndent=-10,
            spaceAfter=3
        ),
        'CodigoBloque': ParagraphStyle(
            'CodigoBloque',
            parent=base['Code'],
            fontName='Courier',
            fontSize=7.8,
            leading=10.5,
            textColor=colors.HexColor('#1b263b'),
            backColor=colors.HexColor('#f1f3f5'),
            borderColor=COLOR_BORDE,
            borderWidth=0.5,
            borderPadding=6,
            spaceBefore=6,
            spaceAfter=8,
            keepWithNext=False
        ),
        'PieFigura': ParagraphStyle(
            'PieFigura',
            parent=base['Italic'],
            fontName='Helvetica-Oblique',
            fontSize=8.2,
            leading=11.5,
            textColor=colors.HexColor('#495057'),
            alignment=1, # Centrado
            spaceBefore=4,
            spaceAfter=10
        ),
        'CajaAviso': ParagraphStyle(
            'CajaAviso',
            parent=base['Normal'],
            fontName='Helvetica',
            fontSize=8.8,
            leading=12.5,
            textColor=colors.HexColor('#495057'),
            backColor=colors.HexColor('#e8f4f8'),
            borderColor=COLOR_SECUNDARIO,
            borderWidth=0.8,
            borderPadding=8,
            spaceBefore=6,
            spaceAfter=8
        ),
        'CajaAlerta': ParagraphStyle(
            'CajaAlerta',
            parent=base['Normal'],
            fontName='Helvetica-Bold',
            fontSize=8.8,
            leading=12.5,
            textColor=COLOR_ALERTA,
            backColor=colors.HexColor('#ffebee'),
            borderColor=COLOR_ALERTA,
            borderWidth=0.8,
            borderPadding=8,
            spaceBefore=6,
            spaceAfter=8
        ),
        'CeldaTabla': ParagraphStyle(
            'CeldaTabla',
            parent=base['Normal'],
            fontName='Helvetica',
            fontSize=8.0,
            leading=10.8,
            textColor=COLOR_TEXTO
        ),
        'CeldaEncabezado': ParagraphStyle(
            'CeldaEncabezado',
            parent=base['Normal'],
            fontName='Helvetica-Bold',
            fontSize=8.2,
            leading=11.2,
            textColor=colors.white
        ),
        'IndiceLinea': ParagraphStyle(
            'IndiceLinea',
            parent=base['Normal'],
            fontName='Helvetica',
            fontSize=9.5,
            leading=15,
            textColor=COLOR_TEXTO
        )
    }
    return estilos

def md_a_xml(texto):
    """Convierte marcas markdown básicas a etiquetas XML ReportLab asegurando anidación correcta."""
    if not texto:
        return ""
    # 1. Extraer bloques de código inline `...` antes de procesar asteriscos
    tokens_codigo = []
    def guardar_codigo(m):
        tokens_codigo.append(html.escape(m.group(1)))
        return f"___CODIGO_{len(tokens_codigo)-1}___"

    # Escapar caracteres especiales pero proteger enlaces
    txt = re.sub(r'`([^`\n]+?)`', guardar_codigo, texto)
    txt = html.escape(txt)

    # 2. Enlaces Markdown [texto](url)
    txt = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'<u>\1</u>', txt)

    # 3. Negrita: **texto**
    txt = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', txt)

    # 4. Cursiva: *texto* (solo palabras bien acotadas, no asteriscos sueltos o multiplicaciones)
    txt = re.sub(r'(?<![a-zA-Z0-9_\*])\*([a-zA-Z0-9_ÁÉÍÓÚáéíóúñÑ][^\*\n]+?[a-zA-Z0-9_ÁÉÍÓÚáéíóúñÑ])\*(?![a-zA-Z0-9_\*])', r'<i>\1</i>', txt)

    # 5. Restaurar tokens de código inline
    for idx, c in enumerate(tokens_codigo):
        txt = txt.replace(f"___CODIGO_{idx}___", f'<font name="Courier" color="#b02a37"><b>{c}</b></font>')

    return txt

def crear_caja_foto_pendiente(foto_id, titulo, encuadre, archivo_sugerido, estilos):
    """Inserta una foto disponible con escalado proporcional y pie de figura."""
    ruta_real = os.path.join('docs', 'imagenes', archivo_sugerido)
    if os.path.exists(ruta_real):
        try:
            from PIL import Image as PILImage
            with PILImage.open(ruta_real) as pimg:
                iw, ih = pimg.size
            aspect = ih / float(iw)
            if aspect > 1.2:
                render_h = 240
                render_w = render_h / aspect
            else:
                render_w = 380
                render_h = render_w * aspect
                if render_h > 240:
                    render_h = 240
                    render_w = render_h / aspect
            img = RLImage(ruta_real, width=render_w, height=render_h)
            caption = Paragraph(f"<b>Fotografía:</b> {titulo}. Archivo: <code>{archivo_sugerido}</code>", estilos['PieFigura'])
            return [Spacer(1, 4), img, caption, Spacer(1, 6)]
        except Exception:
            pass
    # Si no existe, no genera marcadores vacíos ni huecos en la maquetación
    return []

def crear_figura_flujo(fig_id, catalogo_dict, estilos):
    """Genera un bloque con la figura numerada y su pie."""
    if fig_id not in catalogo_dict:
        return []
    info = catalogo_dict[fig_id]
    nombre_archivo = info['archivo'] + '.png'
    ruta_png = os.path.join('docs', 'figuras', nombre_archivo)
    if not os.path.exists(ruta_png):
        return []

    # Ajuste de tamaño: ancho proporcional a 460 pt (alto ~258 pt para relación 16:9)
    img = RLImage(ruta_png, width=460, height=258)
    pie_txt = f"**Figura {info.get('num', '')}:** {info['titulo']}. {info['resumen']} *({info['nota']})*"
    pie_par = Paragraph(md_a_xml(pie_txt), estilos['PieFigura'])

    return [Spacer(1, 4), img, pie_par, Spacer(1, 6)]

def parsear_tabla_md(lineas_tabla, estilos):
    """Convierte un bloque de líneas Markdown de tabla en un objeto Table de ReportLab."""
    filas = []
    for l in lineas_tabla:
        c = l.strip()
        if not c or set(c).issubset({'-', '|', ':', ' '}):
            # Separador de cabecera |---|---|
            continue
        celdas = [celda.strip() for celda in c.split('|')]
        # Remover bordes vacíos creados por split('|')
        if celdas and celdas[0] == '':
            celdas.pop(0)
        if celdas and celdas[-1] == '':
            celdas.pop()
        filas.append(celdas)

    if not filas:
        return None

    # Normalizar cantidad de columnas
    num_cols = max(len(r) for r in filas)
    filas_normalizadas = []
    for i, r in enumerate(filas):
        r_pad = r + [''] * (num_cols - len(r))
        estilo_c = estilos['CeldaEncabezado'] if i == 0 else estilos['CeldaTabla']
        filas_normalizadas.append([Paragraph(md_a_xml(val), estilo_c) for val in r_pad])

    ancho_col = ANCHO_UTIL / num_cols
    t = Table(filas_normalizadas, colWidths=[ancho_col] * num_cols)
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), COLOR_PRIMARIO),
        ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('GRID', (0, 0), (-1, -1), 0.5, COLOR_BORDE),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, COLOR_FONDO_CLARO]),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 5),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
    ]))
    return t

def procesar_markdown_a_story(contenido_md, catalogo_figuras, estilos):
    """Transforma texto Markdown en una lista de Flowables."""
    story = []
    lineas = contenido_md.splitlines()
    i = 0
    n = len(lineas)

    while i < n:
        linea = lineas[i].rstrip()
        linea_strip = linea.strip()

        # Línea vacía
        if not linea_strip:
            i += 1
            continue

        # Bloques de código ```
        if linea_strip.startswith('```'):
            i += 1
            codigo_lineas = []
            while i < n and not lineas[i].strip().startswith('```'):
                codigo_lineas.append(lineas[i])
                i += 1
            i += 1 # Consumir el cierre ```
            texto_codigo = '\n'.join(codigo_lineas)
            # Truncar líneas muy largas para que no desborden
            lineas_trunc = [l[:95] for l in texto_codigo.splitlines()]
            story.append(Preformatted('\n'.join(lineas_trunc), estilos['CodigoBloque']))
            continue

        # Tablas Markdown (| col | col |)
        if linea_strip.startswith('|') and '|' in linea_strip[1:]:
            lineas_tabla = []
            while i < n and lineas[i].strip().startswith('|'):
                lineas_tabla.append(lineas[i])
                i += 1
            tabla_obj = parsear_tabla_md(lineas_tabla, estilos)
            if tabla_obj:
                story.append(Spacer(1, 4))
                story.append(tabla_obj)
                story.append(Spacer(1, 6))
            continue

        # Encabezados
        if linea_strip.startswith('# '):
            texto = linea_strip[2:].strip()
            story.append(Paragraph(md_a_xml(texto), estilos['TituloCapitulo']))
            i += 1
            continue
        elif linea_strip.startswith('## '):
            texto = linea_strip[3:].strip()
            story.append(Paragraph(md_a_xml(texto), estilos['TituloSubseccion']))
            i += 1
            continue
        elif linea_strip.startswith('### '):
            texto = linea_strip[4:].strip()
            story.append(Paragraph(md_a_xml(texto), estilos['TituloSubsub']))
            i += 1
            continue

        # Separador horizontal
        if linea_strip in ('---', '***', '___'):
            story.append(Spacer(1, 3))
            story.append(HRFlowable(width="100%", thickness=0.6, color=COLOR_BORDE, spaceAfter=6, spaceBefore=4))
            i += 1
            continue

        # Citas / Avisos (> ...)
        if linea_strip.startswith('>'):
            caja_lineas = []
            while i < n and lineas[i].strip().startswith('>'):
                caja_lineas.append(lineas[i].strip()[1:].strip())
                i += 1
            texto_caja = ' '.join(caja_lineas)
            if 'ADVERTENCIA' in texto_caja or 'CRÍTICA' in texto_caja:
                story.append(Paragraph(md_a_xml(texto_caja), estilos['CajaAlerta']))
            else:
                story.append(Paragraph(md_a_xml(texto_caja), estilos['CajaAviso']))
            continue

        # Listas con viñeta (- o *)
        if re.match(r'^[\*\-]\s+', linea_strip):
            texto_item = re.sub(r'^[\*\-]\s+', '• ', linea_strip)
            story.append(Paragraph(md_a_xml(texto_item), estilos['ItemLista']))
            i += 1
            continue

        # Listas numeradas (1. 2.)
        m_num = re.match(r'^(\d+)\.\s+(.+)$', linea_strip)
        if m_num:
            num, rest = m_num.groups()
            story.append(Paragraph(f"<b>{num}.</b> {md_a_xml(rest)}", estilos['ItemLista']))
            i += 1
            continue

        # Párrafo ordinario
        # Puede ocupar varias líneas continuas
        parr_lineas = [linea_strip]
        i += 1
        while i < n and lineas[i].strip() and not lineas[i].strip().startswith(('#', '|', '```', '>', '-', '*', '---')):
            parr_lineas.append(lineas[i].strip())
            i += 1
        texto_parrafo = ' '.join(parr_lineas)
        story.append(Paragraph(md_a_xml(texto_parrafo), estilos['Cuerpo']))

    return story

def cargar_catalogo_figuras():
    """Lee docs/figuras/catalogo.py y devuelve el diccionario numerado."""
    sys.path.insert(0, os.path.join('docs', 'figuras'))
    try:
        import catalogo
        cat = {}
        for idx, fig in enumerate(catalogo.FIGURAS, start=1):
            fig_copy = dict(fig)
            fig_copy['num'] = idx
            cat[fig['id']] = fig_copy
        return cat
    except Exception as e:
        print(f"Aviso al cargar catalogo.py: {e}")
        return {}

def generar_portada(estilos, commit_hash):
    """Genera los Flowables de la portada formal."""
    story = [
        Spacer(1, 40),
        Paragraph("UNIVERSIDAD DE GUANAJUATO", ParagraphStyle('Inst', fontName='Helvetica-Bold', fontSize=12, leading=15, alignment=1, textColor=COLOR_PRIMARIO)),
        Paragraph("División de Ingenierías · Campus Irapuato-Salamanca", ParagraphStyle('Div', fontName='Helvetica', fontSize=10, leading=13, alignment=1, textColor=colors.HexColor('#6c757d'))),
        Spacer(1, 15),
        HRFlowable(width="60%", thickness=1.5, color=COLOR_SECUNDARIO, spaceAfter=25, spaceBefore=5),
        Paragraph("Manual y documentación completa del proyecto:<br/>el carro con navegación asistida", estilos['PortadaTitulo']),
        Paragraph("Control distribuido en tres nodos, percepción multimodal, cinemática C99 portátil y protocolo de tiempo real", estilos['PortadaSubtitulo']),
        Spacer(1, 40),
        HRFlowable(width="80%", thickness=0.8, color=COLOR_BORDE, spaceAfter=20, spaceBefore=10),
        Paragraph("<b>Materia:</b> Fundamentos de los Sistemas de Información (FSI)", estilos['PortadaMetadatos']),
        Paragraph("<b>Profesor Titular:</b> Dr. Horacio Rostro González", estilos['PortadaMetadatos']),
        Paragraph("<b>Fecha de entrega y exposición:</b> 1 de diciembre de 2026", estilos['PortadaMetadatos']),
        Paragraph("<b>Integrantes del equipo:</b>", estilos['PortadaMetadatos']),
        Paragraph("&nbsp;&nbsp;• Jaime Fernando Guerrero Segoviano", estilos['PortadaMetadatos']),
        Paragraph("&nbsp;&nbsp;• Melanie Contreras Solis", estilos['PortadaMetadatos']),
        Paragraph("&nbsp;&nbsp;• Vitto Eduardo Rangel Fortuna", estilos['PortadaMetadatos']),
        Paragraph("&nbsp;&nbsp;• Azul Abril Cadena Campos", estilos['PortadaMetadatos']),
        Spacer(1, 15),
        Paragraph(f"<b>Versión del documento:</b> 1.0 · <b>Commit:</b> <code>{commit_hash}</code> · <b>Rama:</b> <code>main</code>", estilos['PortadaMetadatos']),
        HRFlowable(width="80%", thickness=0.8, color=COLOR_BORDE, spaceAfter=15, spaceBefore=20),
        Spacer(1, 30),
        Paragraph("<i>Documentación técnica de ingeniería — Prohibida su alteración sin verificación de código</i>", ParagraphStyle('AvisoP', fontName='Helvetica-Oblique', fontSize=8, leading=11, alignment=1, textColor=colors.HexColor('#6c757d'))),
        PageBreak()
    ]
    return story

def generar_indice(capitulos, estilos, paginas_dict=None):
    """Genera una página de índice general tabulada con números de página reales."""
    story = [
        Paragraph("Índice General del Manual", estilos['TituloCapitulo']),
        Paragraph("Estructura temática y paginación del documento consolidado:", estilos['Cuerpo']),
        HRFlowable(width="100%", thickness=1.0, color=COLOR_PRIMARIO, spaceAfter=8, spaceBefore=4),
    ]

    filas_indice = []
    for num, titulo, desc in capitulos:
        pag_num = paginas_dict.get(str(num), "") if paginas_dict else ""
        pag_txt = f"Pág. {pag_num}" if pag_num else ""
        col1 = Paragraph(f"<b>{num}. {titulo}</b>" + (f" — <i>{desc}</i>" if desc else ""), estilos['CeldaTabla'])
        col2 = Paragraph(f"<b>{pag_txt}</b>", ParagraphStyle('PagR', parent=estilos['CeldaTabla'], alignment=2, textColor=COLOR_PRIMARIO))
        filas_indice.append([col1, col2])

    t = Table(filas_indice, colWidths=[ANCHO_UTIL - 65, 65])
    t.setStyle(TableStyle([
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 2),
        ('TOPPADDING', (0, 0), (-1, -1), 2),
        ('LEFTPADDING', (0, 0), (-1, -1), 0),
        ('RIGHTPADDING', (0, 0), (-1, -1), 0),
        ('LINEBELOW', (0, 0), (-1, -1), 0.3, colors.HexColor('#dee2e6')),
    ]))
    story.append(t)
    story.append(Spacer(1, 10))
    story.append(HRFlowable(width="100%", thickness=0.5, color=COLOR_BORDE, spaceAfter=15, spaceBefore=10))
    story.append(PageBreak())
    return story

def armar_historia(capitulos, estilos, catalogo_fig, commit_actual, paginas_tracker, paginas_dict=None):
    """Construye la lista completa de Flowables para el documento."""
    story = []

    # 1. Portada
    story.extend(generar_portada(estilos, commit_actual))

    # 2. Índice
    story.extend(generar_indice(capitulos, estilos, paginas_dict))

    def leer_doc(subruta):
        p = os.path.join('docs', subruta)
        if os.path.exists(p):
            with open(p, 'r', encoding='utf-8', errors='ignore') as f:
                return f.read()
        return ""

    # Helper para registrar y agregar capítulo
    def agregar_capitulo(num, titulo, story_cap):
        story.append(PageTracker(num, paginas_tracker))
        story.append(Paragraph(f"{num}. {titulo}", estilos['TituloCapitulo']))
        story.extend(story_cap)
        story.append(PageBreak())

    # ── CAPÍTULO 1 ──
    cap1_txt = """
El proyecto **Carro FSI** aborda el diseño, construcción y programación de un vehículo robótico móvil capaz de ejecutar misiones de navegación terrestre asistida empleando exclusivamente componentes de bajo costo y microcontroladores embebidos accesibles en el ámbito universitario.

El sistema demuestra que no se requieren costosos sensores LiDAR industriales ni procesadores de gama alta para lograr comportamientos autónomos seguros: mediante una cuidadosa **arquitectura distribuida en tres nodos**, el desacoplamiento estricto de la lógica cinemática en C99 puro, el control de orientación por giroscopio digital y la supervisión de proximidad por láser ToF, el carro planifica trayectorias sobre una cuadrícula de 20 cm, sortea obstáculos en su trayectoria y mantiene informado al operador humano en tiempo real.

### Objetivos principales del proyecto
- **Objetivo 1 (Arquitectónico):** Demostrar la viabilidad de separar la computación de visión (cámara) del control de tracción en tiempo real (motores y giroscopio) en placas microcontroladoras ESP32 independientes, evitando latencias de procesamiento.
- **Objetivo 2 (Portabilidad y rigor):** Desarrollar el núcleo de navegación en C estándar (C99) libre de dependencias externas, permitiendo compilarlo tanto en la app Android (vía JNI) como en PC para validación automatizada mediante suites de pruebas unitarias.
- **Objetivo 3 (Seguridad robusta):** Diseñar mecanismos de protección multinivel ante fallos, incluyendo un watchdog de comunicación de 1000 ms, un watchdog de hardware de 5000 ms y la autoridad prioritaria del sensor láser ToF para detención instantánea.
- **Objetivo 4 (Honestidad metrológica):** Caracterizar experimentalmente los límites físicos del chasis, reportando con total transparencia los datos verificados en pista física, los resultados obtenidos en simulación y los aspectos no medidos.
"""
    agregar_capitulo(1, "Resumen ejecutivo y objetivos", procesar_markdown_a_story(cap1_txt, catalogo_fig, estilos))

    # ── CAPÍTULO 2 ──
    cap2_txt = """
El vehículo ofrece dos modos principales de interacción operativa:

1. **Teleoperación manual supervisada:** El operador conduce el vehículo desde la pantalla táctil de su teléfono móvil mediante una cruceta direccional sensible. El teléfono transmite órdenes repetidas de avance cada 250 ms por un canal WebSocket. Mientras el usuario mantenga el botón pulsado, el vehículo avanza aplicando el factor de balance calibrado. Al soltar el control, el carro frena de inmediato. La pantalla exhibe el video en vivo emitido por la cámara a ~12.4 cuadros por segundo.
2. **Navegación asistida semiautónoma:** El operador traza una ruta sobre una cuadrícula métrica de 20 cm en la app Android. Al ordenar la ejecución, el núcleo en C toma el mando:
   - Convierte los segmentos en pulsos temporizados calculados con la constante de milisegundos por centímetro.
   - En cada cambio de dirección, orienta el chasis utilizando el ángulo medido por el giroscopio MPU6050.
   - Analiza continuamente el corredor de seguridad frontal: si la cámara o el láser detectan una caja de 20×20 cm obstruyendo la marcha, el carro se detiene, evalúa el espacio lateral, se aparta 40 cm hacia el lado despejado, supera la caja y busca un punto de reincorporación 30 cm por delante antes de continuar la ruta trazada.
"""
    agregar_capitulo(2, "Qué hace el sistema (Descripción general)", procesar_markdown_a_story(cap2_txt, catalogo_fig, estilos))

    # ── CAPÍTULO 3 ──
    c3_elems = []
    c3_elems.extend(crear_figura_flujo('fig_01', catalogo_fig, estilos))
    c3_elems.extend(procesar_markdown_a_story(leer_doc('arquitectura.md'), catalogo_fig, estilos))
    agregar_capitulo(3, "Arquitectura general del sistema", c3_elems)

    # ── CAPÍTULO 4 ──
    c4_elems = []
    c4_elems.extend(crear_figura_flujo('fig_10', catalogo_fig, estilos))
    c4_elems.extend(crear_caja_foto_pendiente('FOTO_01', 'Vista frontal del carro', 'Encuadre frontal mostrando ruedas Mecanum delanteras, sensor láser ToF VL53L0X al centro y cámara ESP32-WROVER vertical', 'foto_01_carro_frontal.jpg', estilos))
    c4_elems.extend(crear_caja_foto_pendiente('FOTO_02', 'Vista lateral en tres cuartos', 'Perspectiva lateral derecha mostrando chasis de acrílico de dos niveles, ruedas Mecanum, pack de baterías y luz interna', 'foto_02_carro_lateral.jpg', estilos))
    c4_elems.extend(crear_caja_foto_pendiente('FOTO_03', 'Vista superior cenital (planta)', 'Vista cenital completa mostrando las cuatro ruedas Mecanum, powerbank trasera, portapilas central, ESP32 DevKit y sensor MPU6050', 'foto_03_carro_superior.jpg', estilos))
    c4_elems.extend(crear_caja_foto_pendiente('FOTO_04', 'Vista en perspectiva de tres cuartos', 'Perspectiva exterior frontal izquierda del vehículo ensamblado con cámara frontal, sensor ToF y sensores inerciales', 'foto_04_carro_perspectiva.jpg', estilos))
    c4_elems.extend(procesar_markdown_a_story(leer_doc(os.path.join('referencia', 'hardware.md')), catalogo_fig, estilos))
    agregar_capitulo(4, "Hardware, componentes y conexiones", c4_elems)

    # ── CAPÍTULO 5 ──
    c5_elems = []
    c5_elems.extend(crear_figura_flujo('fig_02', catalogo_fig, estilos))
    c5_elems.extend(crear_figura_flujo('fig_03', catalogo_fig, estilos))
    c5_elems.extend(procesar_markdown_a_story(leer_doc(os.path.join('referencia', 'firmware.md')), catalogo_fig, estilos))
    agregar_capitulo(5, "Firmware del carro (ESP32 DevKit)", c5_elems)

    # ── CAPÍTULO 6 ──
    c6_elems = []
    c6_elems.extend(crear_figura_flujo('fig_09', catalogo_fig, estilos))
    c6_elems.extend(crear_caja_foto_pendiente('FOTO_06', 'Placa de la cámara Freenove', 'Detalle de lente GC0308, alimentación micro-USB independiente y antena', 'foto_06_camara_freenove.jpg', estilos))
    c6_elems.extend(procesar_markdown_a_story(leer_doc(os.path.join('referencia', 'camara.md')), catalogo_fig, estilos))
    agregar_capitulo(6, "Firmware de la cámara (Freenove ESP32-WROVER)", c6_elems)

    # ── CAPÍTULO 7 ──
    c7_elems = []
    c7_elems.extend(crear_figura_flujo('fig_11', catalogo_fig, estilos))
    c7_elems.extend(procesar_markdown_a_story(leer_doc('protocolo.md'), catalogo_fig, estilos))
    agregar_capitulo(7, "Protocolo de comunicación", c7_elems)

    # ── CAPÍTULO 8 ──
    c8_elems = []
    c8_elems.extend(crear_figura_flujo('fig_04', catalogo_fig, estilos))
    c8_elems.extend(crear_figura_flujo('fig_05', catalogo_fig, estilos))
    c8_elems.extend(crear_figura_flujo('fig_06', catalogo_fig, estilos))
    c8_elems.extend(crear_figura_flujo('fig_12', catalogo_fig, estilos))
    c8_elems.extend(procesar_markdown_a_story(leer_doc(os.path.join('referencia', 'nucleo.md')), catalogo_fig, estilos))
    agregar_capitulo(8, "Núcleo de navegación en C (nucleo_c)", c8_elems)

    # ── CAPÍTULO 9 ──
    c9_elems = []
    c9_elems.extend(crear_figura_flujo('fig_08', catalogo_fig, estilos))
    c9_elems.extend(crear_caja_foto_pendiente('FOTO_10', 'Pestaña Inicio de la app (conectada)', 'Estado conectado con carro (latencia 14 ms, perfil Lento) y cámara (latencia 40 ms, 12.5 fps), con accesos rápidos de ruta', 'foto_10_app_conexion.png', estilos))
    c9_elems.extend(crear_caja_foto_pendiente('FOTO_12', 'Pestaña Rutas de la app', 'Lienzo táctil con cuadrícula cartesiana de 20 cm, plantilla cuadrada de 5 waypoints y herramientas de edición', 'foto_12_app_editor_rutas.png', estilos))
    c9_elems.extend(crear_caja_foto_pendiente('FOTO_15', 'Pantalla Calibrar giro (giroscopio)', 'Interfaz inicial de CalibrarGiroActivity con instrucciones de los 8 giros, botón de inicio y paro de emergencia', 'foto_15_app_calibracion_giro.png', estilos))
    c9_elems.extend(crear_caja_foto_pendiente('FOTO_17', 'Pantalla Ajustes / Red', 'Configuración de direcciones WebSocket ws://192.168.4.1/ws y streaming de cámara http://192.168.4.2/stream', 'foto_17_app_ajustes_red.png', estilos))
    c9_elems.extend(procesar_markdown_a_story(leer_doc(os.path.join('referencia', 'app.md')), catalogo_fig, estilos))
    agregar_capitulo(9, "Aplicación móvil Android", c9_elems)

    # ── CAPÍTULO 10 ──
    c10_elems = []
    c10_elems.extend(crear_figura_flujo('fig_07', catalogo_fig, estilos))
    c10_elems.extend(crear_caja_foto_pendiente('FOTO_13', 'Modo semiautónomo asistido', 'Carro avanzando sobre ruta con detección de obstáculos activa', 'foto_13_app_semiautonomo.png', estilos))
    c10_elems.extend(crear_caja_foto_pendiente('FOTO_16', 'Pantalla VisionActivity', 'Corredor trapezoidal superpuesto, máscara de piso y cuadro TFLite', 'foto_16_app_vision_deteccion.png', estilos))
    c10_elems.extend(procesar_markdown_a_story(leer_doc(os.path.join('codigo', 'corredor.md')) + "\n\n" + leer_doc(os.path.join('codigo', 'obstaculos.md')), catalogo_fig, estilos))
    agregar_capitulo(10, "Visión por computadora y percepción", c10_elems)

    # ── CAPÍTULO 11 ──
    agregar_capitulo(11, "Compilación, instalación y verificación de versiones", procesar_markdown_a_story(leer_doc(os.path.join('referencia', 'compilacion.md')), catalogo_fig, estilos))

    # ── CAPÍTULO 12 ──
    c12_elems = procesar_markdown_a_story(leer_doc(os.path.join('referencia', 'pruebas.md')) + "\n\n" + leer_doc(os.path.join('referencia', 'simulador.md')), catalogo_fig, estilos)
    agregar_capitulo(12, "Pruebas, calidad, integración continua y simulador", c12_elems)

    # ── CAPÍTULO 13 ──
    agregar_capitulo(13, "Historia del proyecto y decisiones de diseño", procesar_markdown_a_story(leer_doc('historia.md'), catalogo_fig, estilos))

    # ── CAPÍTULO 14 ──
    c14_elems = []
    c14_elems.extend(crear_caja_foto_pendiente('FOTO_09', 'Montaje de pruebas de obstáculo', 'Pista con cinta métrica extendida y caja de 20x20 cm en la ruta', 'foto_09_escenario_obstaculo.jpg', estilos))
    c14_elems.extend(procesar_markdown_a_story(leer_doc('resultados.md'), catalogo_fig, estilos))
    agregar_capitulo(14, "Resultados y estado de validación", c14_elems)

    # ── CAPÍTULO 15 ──
    cap15_txt = """
### Límites conocidos del prototipo
1. **Ausencia de encoders ópticos:** El carro carece de medición directa del giro de las ruedas. Toda estimación de desplazamiento lineal depende del modelo temporal ($d = t / \text{ms/cm}$). Si las ruedas patinan por exceso de cera en el suelo, el odómetro acumula error de avance.
2. **Dependencia de la carga de las pilas AA:** Conforme las pilas alcalinas se descargan, la velocidad lineal cae paulatinamente. Se mitiga mediante la regla de los 90 minutos y 20 corridas.
3. **Compresión de video por software:** El sensor GC0308 no tiene silicio JPEG; el límite físico de salida es de ~12.4 fps en HQVGA.
4. **Campo estrecho del sensor láser ToF:** El cono de visión del VL53L0X cubre ~25°; objetos angostos ubicados en los extremos del parachoques requieren ser detectados por visión.

### Líneas de trabajo futuro
- Sustitución de la placa de cámara por un módulo con sensor **OV2640 o ESP32-S3**, habilitando compresión JPEG por hardware a 30 fps en resolución VGA.
- Incorporación de una batería LiPo 2S de 7.4 V con circuito regulador step-down conmutado para alimentar motores con voltaje constante independiente del tiempo de prueba.
- Migración hacia algoritmos de fusión EKF (Filtro de Kalman Extendido) integrando acelerometría lineal filtrada.
"""
    agregar_capitulo(15, "Límites, riesgos y trabajo futuro", procesar_markdown_a_story(cap15_txt, catalogo_fig, estilos))

    # ── CAPÍTULO 16 ──
    c16_elems = procesar_markdown_a_story(leer_doc('operacion.md') + "\n\n" + leer_doc('seguridad.md'), catalogo_fig, estilos)
    agregar_capitulo(16, "Guía de operación y seguridad física", c16_elems)

    # ── CAPÍTULO 17 ──
    agregar_capitulo(17, "Preguntas frecuentes de la exposición oral", procesar_markdown_a_story(leer_doc('exposicion.md'), catalogo_fig, estilos))

    # ── CAPÍTULO 18 ──
    c18_elems = procesar_markdown_a_story(leer_doc('solucion_problemas.md') + "\n\n" + leer_doc('mantenimiento.md'), catalogo_fig, estilos)
    agregar_capitulo(18, "Solución de problemas y mantenimiento", c18_elems)

    # ── CAPÍTULO 19 ──
    agregar_capitulo(19, "Glosario de términos y unidades", procesar_markdown_a_story(leer_doc('glosario.md'), catalogo_fig, estilos))

    # ── CAPÍTULO 20 ──
    cap20_txt = """
### Apéndice A: Estructura del repositorio y mapa de archivos
El repositorio oficial se hospeda en GitHub (`github.com/fernandosegrr/fsi_proyect`). La rama oficial de entrega documental es `docs/documentacion-completa`.

### Apéndice B: Tabla maestra de comandos y respuestas WebSocket
| Comando | Parámetros | Respuesta típica | Propósito |
|---|---|---|---|
| `FORWARD` | Ninguno | — | Avance con rampa y balance activo |
| `REVERSE` | Ninguno | — | Retroceso con balance |
| `LEFT` / `RIGHT` | Ninguno | — | Pivote en el lugar (un motor activo) |
| `STOP` / `EBRAKE` | Ninguno | — | Freno inmediato a los cuatro motores |
| `PING` | Ninguno | `PONG` | Sondeo de latencia (no renueva watchdog) |
| `MODO_MANUAL` | Ninguno | `ACK_MANUAL` | Potencia 255/252 |
| `MODO_AUTO` | Ninguno | `ACK_AUTO` | Potencia 210/207 |
| `GIRAR:<deg>` | 5 a 200 | `GIRO_OK` / `GIRO_TIMEOUT` | Giro en bucle cerrado con giroscopio |
| `BAL_AUTO:NNN` | 800 a 1000 | `ACK_BAL_AUTO:NNN` | Calibra factor de balance AUTO |
| `CAPS?` | Ninguno | `CAPS:IMU=1,TOF=1` | Consulta de periféricos I2C |
| `IMU_CAL` | Ninguno | `ACK_IMU_CAL` | Calibra sesgo en reposo |
| `TOF_USO:<0|1>` | 0 o 1 | `ACK_TOF_USO:<n>` | Activa/desactiva autoridad láser |

### Apéndice C: Asignación de pines físicos (ESP32 DevKit)
- **Motor Izquierdo INA / INB:** GPIO 27 / GPIO 26 (`PIN_MOTOR_IZQ_A`, `PIN_MOTOR_IZQ_B`).
- **Motor Derecho INC / IND:** GPIO 25 / GPIO 33 (`PIN_MOTOR_DER_A`, `PIN_MOTOR_DER_B`).
- **Bus I2C Sensores:** SDA = GPIO 21, SCL = GPIO 22 (400 kHz).
- **Keep-alive Powerbank:** GPIO 2 (señal de mantenimiento periódica en firmware; no verificado físicamente).

### Apéndice D: Sumas de verificación criptográficas (SHA-256)
- APK Oficial: `36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA` (`dist/carro_fsi_0.4-074c9fd.apk`, 14,852,386 bytes).
- Firmware Esperado: `9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E` (`dist/firmware_carro_453ed20-20261027.bin`, 1,031,712 bytes).

### Apéndice E: Observaciones y puntos pendientes
Para la lista de 17 fotografías por tomar, consúltese `docs/lista_de_fotos.md`. Para las aclaraciones de contradicciones históricas y aspectos pendientes de confirmación, consúltese `docs/pendientes_documentacion.md`.
"""
    agregar_capitulo(20, "Apéndices técnicos", procesar_markdown_a_story(cap20_txt, catalogo_fig, estilos))

    return story

def compilar_manual():
    print("=== Iniciando compilación del Manual Completo en PDF ===")
    os.makedirs(os.path.join('docs', 'manual'), exist_ok=True)
    pdf_salida = os.path.join('docs', 'manual', 'Manual_Proyecto_Carro_FSI.pdf')

    try:
        import subprocess
        commit_actual = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], text=True).strip()
    except Exception:
        commit_actual = '203892c'

    estilos = crear_estilos()
    catalogo_fig = cargar_catalogo_figuras()

    capitulos = [
        ("1", "Resumen ejecutivo y objetivos", "Propósito pedagógico, justificación y alcance"),
        ("2", "Qué hace el sistema", "Comportamiento funcional y capacidades del vehículo"),
        ("3", "Arquitectura general del sistema", "Topología de tres nodos, enlaces de red y jerarquía"),
        ("4", "Hardware, componentes y conexiones", "Placas, motores, I2C, cableado y lista de materiales"),
        ("5", "Firmware del carro (ESP32 DevKit)", "Capas, cola protegida, watchdog y temporizaciones"),
        ("6", "Firmware de la cámara (Freenove ESP32-WROVER)", "Sensor GC0308, compresión JPEG, PSRAM y modos"),
        ("7", "Protocolo de comunicación", "Comandos WebSocket, telemetría SENS y respuestas"),
        ("8", "Núcleo de navegación en C (nucleo_c)", "Máquina de estados, cinemática, pose y calibración"),
        ("9", "Aplicación móvil Android", "Interfaz de usuario, sesiones persistentes y puente JNI"),
        ("10", "Visión por computadora y percepción", "Corredor trapezoidal, segmentación e inferencia TFLite"),
        ("11", "Compilación, instalación y verificación", "Cadenas de compilación, flasheo y comprobación SHA-256"),
        ("12", "Pruebas, calidad y simulador", "2045 casos en PC, pruebas de estrés y modelo virtual"),
        ("13", "Historia del proyecto y decisiones de diseño", "Evolución por jornadas (J1-J15) y lo descartado"),
        ("14", "Resultados y estado de validación", "Desglose estricto: medido en hardware, simulado y NO MEDIDO"),
        ("15", "Límites, riesgos y trabajo futuro", "Restricciones operativas y líneas de desarrollo"),
        ("16", "Guía de operación y seguridad física", "Regla de 90 min / 20 corridas, protocolo de encendido"),
        ("17", "Preguntas frecuentes de la exposición oral", "Banco de 9 preguntas críticas del sínodo con respuestas"),
        ("18", "Solución de problemas y mantenimiento", "Matriz de síntomas, fallas comunes y conservación"),
        ("19", "Glosario técnico de términos y unidades", "Definiciones formales y vocabulario de ingeniería"),
        ("20", "Apéndices técnicos", "Comandos, pines, sumas criptográficas y pendientes"),
    ]

    doc = SimpleDocTemplate(
        pdf_salida,
        pagesize=letter,
        leftMargin=MARGEN,
        rightMargin=MARGEN,
        topMargin=MARGEN,
        bottomMargin=MARGEN
    )

    # Pasada 1: Medir la paginación de los capítulos
    print("Pasada 1: Calculando paginación exacta de cada capítulo...")
    paginas_tracker = {}
    story_pasada1 = armar_historia(capitulos, estilos, catalogo_fig, commit_actual, paginas_tracker, paginas_dict=None)
    doc.build(story_pasada1, canvasmaker=NumberedCanvas)
    print(f"Páginas detectadas: {paginas_tracker}")

    # Pasada 2: Renderizar el documento final con el índice paginado
    print("Pasada 2: Renderizando documento final con Índice General paginado...")
    paginas_tracker_final = {}
    story_pasada2 = armar_historia(capitulos, estilos, catalogo_fig, commit_actual, paginas_tracker_final, paginas_dict=paginas_tracker)
    doc.build(story_pasada2, canvasmaker=NumberedCanvas)

    tam_bytes = os.path.getsize(pdf_salida)
    print(f"Manual generado con éxito: {pdf_salida} ({tam_bytes:,} bytes)")
    return pdf_salida, tam_bytes

if __name__ == '__main__':
    compilar_manual()
