from pathlib import Path
import html, re, subprocess, sys
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak, Image, Table, TableStyle, KeepTogether
from reportlab.platypus.tableofcontents import TableOfContents
from PIL import Image as PILImage
import matplotlib
import pymupdf

ROOT = Path(__file__).resolve().parents[2]
MANUAL = ROOT / 'docs' / 'manual'
FIG = ROOT / 'docs' / 'figuras' / 'v2'
FOTOS = ROOT / 'docs' / 'imagenes'
fonts = Path(matplotlib.get_data_path()) / 'fonts' / 'ttf'
for name, filename in [('DejaVu','DejaVuSans.ttf'), ('DejaVu-Bold','DejaVuSans-Bold.ttf'), ('DejaVu-Oblique','DejaVuSans-Oblique.ttf'), ('DejaVuMono','DejaVuSansMono.ttf')]:
    pdfmetrics.registerFont(TTFont(name, str(fonts / filename)))
pdfmetrics.registerFontFamily('DejaVu', normal='DejaVu', bold='DejaVu-Bold', italic='DejaVu-Oblique', boldItalic='DejaVu-Bold')
BLUE = colors.HexColor('#163c60')
TEAL = colors.HexColor('#00877e')
styles = {
 'body': ParagraphStyle('body', fontName='DejaVu', fontSize=10.2, leading=15, textColor=colors.HexColor('#26323b'), spaceAfter=7, allowWidows=0, allowOrphans=0),
 'h1': ParagraphStyle('h1', fontName='DejaVu-Bold', fontSize=16, leading=20, textColor=BLUE, spaceBefore=16, spaceAfter=9, keepWithNext=True),
 'h2': ParagraphStyle('h2', fontName='DejaVu-Bold', fontSize=12, leading=16, textColor=TEAL, spaceBefore=10, spaceAfter=5, keepWithNext=True),
 'cover': ParagraphStyle('cover', fontName='DejaVu-Bold', fontSize=24, leading=31, textColor=BLUE, alignment=TA_CENTER, spaceAfter=18),
 'center': ParagraphStyle('center', fontName='DejaVu', fontSize=11, leading=17, alignment=TA_CENTER, spaceAfter=8),
 'caption': ParagraphStyle('caption', fontName='DejaVu', fontSize=8.7, leading=12, textColor=BLUE, alignment=TA_CENTER, spaceBefore=4, spaceAfter=11),
}

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True, stderr=subprocess.DEVNULL).strip()

def html_text(s):
    s = html.escape(s)
    s = re.sub(r'\*\*(.*?)\*\*', r'<b>\1</b>', s)
    return re.sub(r'`([^`]+)`', r'<font name="DejaVuMono">\1</font>', s)

def sized(path, maxw, maxh):
    with PILImage.open(path) as im:
        w,h=im.size
    scale=min(maxw/w,maxh/h)
    return Image(str(path), width=w*scale, height=h*scale, hAlign='CENTER')

class Manual(SimpleDocTemplate):
    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and hasattr(flowable, '_outline'):
            level,key = flowable._outline
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(flowable.getPlainText(), key, level=level, closed=False)
            self.notify('TOCEntry', (level, flowable.getPlainText(), self.page, key))

def heading(text, level, key):
    p=Paragraph(html_text(text),styles['h1' if level == 0 else 'h2'])
    p._outline=(level,key)
    return p

def header(canvas, doc):
    canvas.setTitle('Manual técnico del Carro FSI')
    canvas.setAuthor('Jaime Fernando Guerrero Segoviano; Melanie Contreras Solis; Vitto Eduardo Rangel Fortuna; Azul Abril Cadena Campos')
    if doc.page > 1:
        canvas.setFont('DejaVu', 8)
        canvas.setFillColor(BLUE)
        canvas.drawString(doc.leftMargin, A4[1]-38, 'Carro FSI · Manual técnico')
        canvas.line(doc.leftMargin, A4[1]-43, A4[0]-doc.rightMargin, A4[1]-43)
        canvas.drawRightString(A4[0]-doc.rightMargin, 34, str(doc.page))

figures = {
 2:[('arquitectura','Arquitectura de tres nodos y alimentación independiente de las placas.'),('alimentacion','Alimentación y referencia común entre DevKit, driver y pilas.'),('seguridad','Capas de supervisión y condiciones de parada.')],
 4:[('comandos','Recorrido general de una orden de movimiento.')],
 7:[('navegacion','Ciclo general de navegación y estimación de pose.'),('esquive','Etapas conceptuales del esquive y la reincorporación.')],
 8:[('percepcion','Señales de percepción y parada frontal condicionada.')],
 9:[('pantallas','Pantallas principales de la aplicación.')],
 14:[('desarrollo','Etapas del desarrollo del sistema.')],
}

def add_figure(story, name, caption, number):
    story.append(Paragraph(f'La Figura {number} presenta: {html.escape(caption)}', styles['body']))
    pic=sized(FIG/(name+'.png'), 450, 250)
    story.append(KeepTogether([pic, Paragraph(f'Figura {number}. {html.escape(caption)}',styles['caption'])]))

def photo_grid(story, paths, labels, number, caption, height):
    items=[]
    for path,label in zip(paths,labels):
        items.append([sized(FOTOS/path, 220 if len(paths)==4 else 140,height),Paragraph(html.escape(label),styles['caption'])])
    cols=2 if len(paths)==4 else 3
    rows=[]
    for i in range(0,len(items),cols):
        rows.append([Table([[item[0]], [item[1]]], colWidths=[225 if cols==2 else 145], style=TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'),('ALIGN',(0,0),(-1,-1),'CENTER')])) for item in items[i:i+cols]])
    table=Table(rows, colWidths=[228 if cols==2 else 148]*cols, hAlign='CENTER')
    story.append(KeepTogether([Paragraph(f'La Figura {number} muestra las vistas correspondientes.', styles['body']),table, Paragraph(f'Figura {number}. {html.escape(caption)}', styles['caption'])]))

def tabla(story, numero, titulo, filas):
    data=[[Paragraph(html.escape(str(c)), styles['body']) for c in row] for row in filas]
    t=Table(data,colWidths=[145,310],repeatRows=1,hAlign='CENTER')
    t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),colors.HexColor('#d9efeb')),('GRID',(0,0),(-1,-1),.4,colors.HexColor('#b8cbc9')),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),6),('RIGHTPADDING',(0,0),(-1,-1),6)]))
    story.extend([t,Paragraph(f'Tabla {numero}. {html.escape(titulo)}.',styles['caption'])])

def main():
    subprocess.run([sys.executable, str(ROOT/'docs'/'figuras'/'generar_figuras_v2.py')], cwd=ROOT, check=True)
    out=MANUAL/'Manual_Proyecto_Carro_FSI.pdf'
    doc=Manual(str(out), pagesize=A4, leftMargin=63, rightMargin=63, topMargin=60, bottomMargin=58, title='Manual técnico del Carro FSI')
    story=[Spacer(1,100),Paragraph('Manual técnico<br/>del Carro FSI',styles['cover']),Paragraph('Arquitectura, operación y evidencia de un vehículo de navegación asistida',styles['center']),Spacer(1,45),Paragraph('Fundamentos de los Sistemas de Información',styles['center']),Paragraph('Profesor titular: Dr. Horacio Rostro González',styles['center']),Paragraph('Jaime Fernando Guerrero Segoviano<br/>Melanie Contreras Solis<br/>Vitto Eduardo Rangel Fortuna<br/>Azul Abril Cadena Campos',styles['center']),Spacer(1,20),Paragraph('Entrega: 1 de diciembre de 2026',styles['center']),Paragraph('Versión de fuentes: '+html.escape(git('describe','--tags','--always'))+' · Commit: '+git('rev-parse','--short','HEAD'),styles['center']),PageBreak(),Paragraph('Contenido',styles['h1'])]
    toc=TableOfContents()
    toc.levelStyles=[ParagraphStyle('toc0',fontName='DejaVu-Bold',fontSize=9.5,leading=15,leftIndent=0,firstLineIndent=0),ParagraphStyle('toc1',fontName='DejaVu',fontSize=8.5,leading=13,leftIndent=16,firstLineIndent=0)]
    story.extend([toc,Spacer(1,14)])
    fig_num=0
    for n,path in enumerate(sorted((MANUAL/'capitulos').glob('*.md')),1):
        sections=0
        code_line=None
        blocks=re.split(r'\n\s*\n',path.read_text(encoding='utf-8').strip())
        for block in blocks:
            t=block.strip()
            if t.startswith('# '):
                story.append(heading(f'{n}. {t[2:]}',0,f'cap{n}'))
            elif t.startswith('## '):
                sections+=1
                story.append(heading(f'{n}.{sections} {t[3:]}',1,f'cap{n}s{sections}'))
            else:
                if t.startswith('Dónde está en el código:'):
                    code_line=t
                else:
                    story.append(Paragraph(html_text(' '.join(t.splitlines())),styles['body']))
                    if n==16 and t.startswith('La Tabla 1 resume'):
                        tabla(story,1,'Comandos de consulta y control', [('Comando','Función'),('FORWARD / REVERSE','Avanzar / retroceder'),('LEFT / RIGHT','Pivote izquierdo / derecho'),('STOP / EBRAKE','Detener motores'),('PING','Sondeo con respuesta PONG; no renueva el temporizador'),('MODO_AUTO / MODO_MANUAL','Seleccionar perfil'),('GIRAR:<grados>','Solicitar giro por ángulo'),('CAPS? / IMU? / TOF?','Consultar estado de sensores'),('IMU_CAL / IMU_CERO','Calibrar sesgo / fijar referencia angular')])
                    elif n==16 and t.startswith('La Tabla 2 reúne'):
                        tabla(story,2,'Asignación de pines', [('GPIO','Función'),('27 / 26','Entradas del driver, lado izquierdo'),('25 / 33','Entradas del driver, lado derecho'),('21 / 22','I2C: SDA / SCL'),('2','Pulso de mantenimiento configurado; efecto físico no medido')])
        for name,caption in figures.get(n,[]):
            fig_num+=1; add_figure(story,name,caption,fig_num)
        if n==3:
            fig_num+=1
            photo_grid(story,['foto_01_carro_frontal.jpg','foto_02_carro_lateral.jpg','foto_03_carro_superior.jpg','foto_04_carro_perspectiva.jpg'],['Vista frontal','Vista lateral','Vista superior','Vista en perspectiva'],fig_num,'Cuatro vistas del carro.',145)
        if n==9:
            fig_num+=1
            photo_grid(story,['foto_10_app_conexion.png','foto_12_app_editor_rutas.png','foto_17_app_ajustes_red.png'],['Conectado','Reconectando: teléfono fuera de la red','Sin conexión'],fig_num,'Pantallas capturadas en distintos estados de conexión.',205)
        if code_line:
            story.append(Paragraph(html_text(code_line),styles['body']))
    doc.multiBuild(story, onFirstPage=header, onLaterPages=header)
    # ReportLab agrega una selección inicial de Helvetica sin texto asociado.
    # Retirarla y quitar el recurso evita declarar una fuente que no se usa.
    pdf=pymupdf.open(out)
    helvetica=next(font[0] for font in pdf[0].get_fonts() if font[3]=='Helvetica' and font[4]=='F1')
    for page in pdf:
        for xref in page.get_contents():
            data=pdf.xref_stream(xref)
            data=re.sub(rb'BT /F1 [\d.]+ Tf [\d.]+ TL ET',b'',data)
            if re.search(rb'/F1(?:\s|$)', data):
                raise RuntimeError('Helvetica tiene uso de texto; no se elimina')
            pdf.update_stream(xref,data)
    for xref in range(1,pdf.xref_length()):
        if pdf.xref_get_key(xref,'F1') == ('xref',f'{helvetica} 0 R'):
            obj=pdf.xref_object(xref)
            obj=re.sub(r'/F1\s+'+str(helvetica)+r'\s+0\s+R\b','',obj)
            pdf.update_object(xref,obj)
    tmp=out.with_suffix('.tmp.pdf')
    pdf.save(tmp,garbage=4,deflate=True)
    pdf.close()
    tmp.replace(out)
    print(out)

if __name__ == '__main__': main()
