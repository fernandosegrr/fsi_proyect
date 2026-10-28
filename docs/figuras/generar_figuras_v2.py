#!/usr/bin/env python3
"""Genera diagramas vectoriales para el manual v2."""
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

DESTINO = Path(__file__).parent / 'v2'
AZUL = '#163c60'
VERDE = '#00877e'
FONDO = '#edf6f4'
plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 9, 'pdf.fonttype': 42, 'svg.fonttype': 'none'})


def diagrama(nombre, cajas, enlaces, notas=()):
    fig, ax = plt.subplots(figsize=(6.3, 3.5))
    ax.set(xlim=(0, 100), ylim=(0, 100))
    ax.axis('off')
    for x, y, w, h, texto in cajas:
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle='round,pad=1.2',
                                    facecolor=FONDO, edgecolor=VERDE, linewidth=1.2))
        ax.text(x+w/2, y+h/2, texto, ha='center', va='center', color=AZUL,
                fontsize=9, linespacing=1.2)
    for x1, y1, x2, y2 in enlaces:
        ax.annotate('', xy=(x2, y2), xytext=(x1, y1),
                    arrowprops={'arrowstyle': '->', 'color': AZUL, 'lw': 1.3})
    for x, y, texto in notas:
        ax.text(x, y, texto, ha='center', va='center', color=AZUL, fontsize=8)
    DESTINO.mkdir(exist_ok=True)
    fig.savefig(DESTINO / (nombre + '.pdf'), bbox_inches='tight', pad_inches=.08)
    fig.savefig(DESTINO / (nombre + '.png'), dpi=200, bbox_inches='tight', pad_inches=.08)
    plt.close(fig)


def esquive():
    fig, ax = plt.subplots(figsize=(6.3, 3.5))
    ax.set(xlim=(-1, 10), ylim=(-1, 10))
    ax.axis('off')
    ax.plot([5,5], [0,9], '--', color=AZUL, lw=1.4)
    ax.plot([5,5,2,2,5,5], [0,2,2,7,7,9], color=VERDE, lw=2)
    ax.add_patch(FancyBboxPatch((4.3,3.6),1.4,1.4,boxstyle='round,pad=.1',edgecolor='#cb5a3a',facecolor='#fff0e7'))
    ax.text(5,4.3,'Obstáculo',ha='center',va='center',fontsize=8,color=AZUL)
    ax.text(1.7,2.6,'Desvío lateral',ha='right',fontsize=9,color=AZUL)
    ax.text(5.4,7.2,'Reincorporación',fontsize=9,color=AZUL)
    ax.text(5.3,.2,'Ruta original',fontsize=9,color=AZUL)
    ax.text(5,9.6,'Esquema conceptual, no a escala · J15: NO MEDIDO en el carro',ha='center',fontsize=8,color=AZUL)
    fig.savefig(DESTINO/'esquive.pdf',bbox_inches='tight',pad_inches=.08)
    fig.savefig(DESTINO/'esquive.png',dpi=200,bbox_inches='tight',pad_inches=.08)
    plt.close(fig)


def main():
    diagrama('arquitectura', [
        (2, 61, 26, 19, 'Teléfono Android\nApp y navegación'),
        (38, 61, 25, 19, 'ESP32 DevKit\nControl y sensores'),
        (73, 61, 25, 19, 'Cámara Freenove\nVideo'),
        (37, 15, 26, 16, 'Powerbank\nde la DevKit'),
        (73, 15, 25, 16, 'Powerbank\nde la cámara'),
    ], [(28, 70, 38, 70), (73, 70, 63, 70), (50, 31, 50, 61), (85, 31, 85, 61)],
        [(33, 76, 'WiFi'), (68, 76, 'WiFi')])
    diagrama('alimentacion', [
        (2, 70, 27, 17, 'Powerbank\nDevKit'), (37, 70, 25, 17, 'ESP32 DevKit'),
        (70, 70, 28, 17, 'Sensores I2C'), (2, 34, 27, 17, 'Cuatro pilas AA'),
        (37, 34, 25, 17, 'Driver TC1508A'), (70, 34, 28, 17, 'Cuatro motores'),
        (2, 3, 27, 17, 'Powerbank\ncámara'), (37, 3, 25, 17, 'Cámara Freenove')
    ], [(29, 78, 37, 78), (62, 78, 70, 78), (29, 42, 37, 42),
        (62, 42, 70, 42), (50, 70, 50, 51), (29, 11, 37, 11)],
        [(60, 61, 'Señales'), (80, 11, 'GND común:\npilas, driver\ny DevKit')])
    diagrama('seguridad', [
        (4, 72, 92, 16, 'Operador: supervisión y parada física'),
        (4, 51, 92, 16, 'Aplicación: control y cancelación de ruta'),
        (4, 30, 92, 16, 'Firmware: parada y vigilancia de comunicación'),
        (4, 9, 92, 16, 'Láser: parada condicionada en avance AUTO')
    ], [])
    diagrama('comandos', [
        (2, 42, 20, 20, 'Aplicación'), (27, 42, 20, 20, 'WebSocket'),
        (52, 42, 20, 20, 'Cola y\nlógica'), (77, 42, 20, 20, 'Motores')
    ], [(22, 52, 27, 52), (47, 52, 52, 52), (72, 52, 77, 52)],
        [(50, 22, 'Estado y telemetría regresan a la aplicación')])
    diagrama('navegacion', [
        (2, 42, 21, 20, 'Planificar\nruta'), (27, 42, 21, 20, 'Avanzar\npor tiempo'),
        (52, 42, 21, 20, 'Girar con\ngiroscopio'), (77, 42, 21, 20, 'Estimar\npose')
    ], [(23, 52, 27, 52), (48, 52, 52, 52), (73, 52, 77, 52)],
        [(50, 23, 'La posición es una estimación')])
    esquive()
    diagrama('percepcion', [
        (2, 74, 23, 15, 'Color del piso'), (2, 48, 23, 15, 'Modelo de objetos'),
        (36, 62, 23, 15, 'Fusión'), (36, 35, 23, 15, 'Corredor'),
        (72, 49, 26, 15, 'Decisión'), (2, 6, 23, 15, 'Láser frontal'),
        (72, 6, 26, 15, 'Parada AUTO')
    ], [(25, 81, 36, 70), (25, 55, 36, 67), (25, 79, 36, 43),
        (59, 70, 72, 57), (59, 43, 72, 55), (25, 14, 72, 14)])
    diagrama('pantallas', [
        (36, 70, 28, 17, 'Aplicación'), (2, 30, 21, 17, 'Inicio'),
        (27, 30, 21, 17, 'Manual'), (52, 30, 21, 17, 'Rutas'), (77, 30, 21, 17, 'Más')
    ], [(50, 70, 12, 47), (50, 70, 37, 47), (50, 70, 62, 47), (50, 70, 87, 47)])
    diagrama('desarrollo', [
        (2, 42, 21, 20, 'Control\ninicial'), (27, 42, 21, 20, 'Núcleo C\ny pruebas'),
        (52, 42, 21, 20, 'Sensores\ny calibración'), (77, 42, 21, 20, 'Corrección\nde pose')
    ], [(23, 52, 27, 52), (48, 52, 52, 52), (73, 52, 77, 52)],
        [(50, 22, 'La validación física de la corrección de pose sigue pendiente')])


if __name__ == '__main__':
    main()
