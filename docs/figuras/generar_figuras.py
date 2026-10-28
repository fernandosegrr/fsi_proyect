#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generar_figuras.py — Dibuja las figuras del manual del carro FSI.

Uso:
    python docs/figuras/generar_figuras.py

Genera, para cada figura del catálogo:
    docs/figuras/fig_NN_slug.png   (170 dpi, para el PDF)
    docs/figuras/fig_NN_slug.svg   (vectorial, para editar o ampliar)

También reescribe docs/figuras/README.md con la tabla de figuras.
Las figuras son esquemas: no se inventan mediciones. Cuando un dato es
simulado o no medido, se dice en el pie o en la propia figura.
"""
from pathlib import Path
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Polygon, Rectangle, Circle, Arc

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
from catalogo import FIGURAS  # noqa: E402

# ── Paleta y estilo ────────────────────────────────────────────────────────
AZUL = "#1d3557"
AZUL_CLARO = "#e8f0f7"
TEAL = "#2a9d8f"
TEAL_CLARO = "#e6f4f1"
NARANJA = "#e76f51"
NARANJA_CLARO = "#fdeee9"
AMARILLO = "#f4a261"
AMARILLO_CLARO = "#fdf3e6"
GRIS = "#6c757d"
GRIS_CLARO = "#f1f3f5"
VERDE = "#2d6a4f"
VERDE_CLARO = "#e8f5ee"
ROJO = "#b00020"
ROJO_CLARO = "#fbe9eb"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 10,
    "axes.edgecolor": GRIS,
    "axes.labelcolor": AZUL,
    "text.color": AZUL,
    "svg.fonttype": "none",
})


def nueva(titulo, subtitulo, dos=False, alto=6.75):
    fig = plt.figure(figsize=(12, alto), facecolor="white")
    fig.suptitle(titulo, fontsize=18, fontweight="bold", color=AZUL, y=0.965)
    fig.text(0.5, 0.915, subtitulo, ha="center", va="top", fontsize=10, color=GRIS)
    if dos:
        axs = [fig.add_axes([0.045, 0.08, 0.42, 0.78]),
               fig.add_axes([0.535, 0.08, 0.42, 0.78])]
    else:
        axs = [fig.add_axes([0.035, 0.075, 0.93, 0.80])]
    for ax in axs:
        ax.set_xlim(0, 100)
        ax.set_ylim(0, 100)
        ax.axis("off")
    return fig, axs


def caja(ax, x, y, w, h, texto, relleno=AZUL_CLARO, borde=AZUL, tam=10.5,
         color=AZUL, bold=False, radio=1.2, lw=1.5, z=2):
    p = FancyBboxPatch((x, y), w, h,
                       boxstyle=f"round,pad=0,rounding_size={radio}",
                       linewidth=lw, edgecolor=borde, facecolor=relleno, zorder=z)
    ax.add_patch(p)
    ax.text(x + w / 2, y + h / 2, texto, ha="center", va="center",
            fontsize=tam, color=color, fontweight="bold" if bold else "normal",
            zorder=z + 1, linespacing=1.3)


def flecha(ax, p1, p2, color=AZUL, lw=1.8, estilo="-|>", rad=0.0, ls="-",
           mut=12, z=1):
    ax.add_patch(FancyArrowPatch(p1, p2, arrowstyle=estilo, mutation_scale=mut,
                                 color=color, lw=lw, linestyle=ls,
                                 connectionstyle=f"arc3,rad={rad}",
                                 shrinkA=2, shrinkB=2, zorder=z))


def etiqueta(ax, x, y, texto, color=GRIS, tam=9, ha="center", va="center",
             fondo=True, bold=False, rot=0):
    bbox = dict(boxstyle="round,pad=0.22", fc="white", ec="none", alpha=0.95) if fondo else None
    ax.text(x, y, texto, ha=ha, va=va, fontsize=tam, color=color,
            fontweight="bold" if bold else "normal", bbox=bbox,
            zorder=5, rotation=rot, linespacing=1.25)


def pie(fig, texto):
    fig.text(0.5, 0.022, texto, ha="center", va="bottom", fontsize=7.6, color=GRIS)


def guardar(_fig, archivo):
    # Las funciones de dibujo dejan su figura como figura actual.
    fig = plt.gcf()
    png = AQUI / f"{archivo}.png"
    svg = AQUI / f"{archivo}.svg"
    fig.savefig(png, dpi=170, facecolor="white")
    fig.savefig(svg, format="svg", facecolor="white")
    plt.close(fig)
    return png, svg


# ── Figura 1: sistema completo ─────────────────────────────────────────────
def fig_01():
    fig, (ax,) = nueva(
        "El sistema completo",
        "Un teléfono manda órdenes por WiFi; la ESP32 las traduce en movimiento y la cámara va aparte.",
    )
    caja(ax, 3, 69, 20, 16, "Teléfono Android\napp FSI\no navegador web", AZUL_CLARO)
    caja(ax, 38, 65, 24, 22, "ESP32 DevKit\nAP 192.168.4.1\nHTTP :80\nWebSocket /ws", TEAL_CLARO, TEAL)
    caja(ax, 72, 70, 25, 16, "Freenove\nESP32-WROVER\ncámara GC0308\n192.168.4.2", AMARILLO_CLARO, AMARILLO)
    caja(ax, 38, 35, 24, 14, "MPU6050\nVL53L0X\nI2C 21/22", VERDE_CLARO, VERDE)
    caja(ax, 72, 39, 25, 14, "TC1508A\npuente H doble", NARANJA_CLARO, NARANJA)
    caja(ax, 72, 12, 25, 14, "4 motores TT\ntracción", GRIS_CLARO, GRIS)
    caja(ax, 4, 45, 20, 12, "Powerbank 5 V\nUSB", GRIS_CLARO, GRIS)
    caja(ax, 4, 12, 20, 12, "4 pilas AA\n~6 V", GRIS_CLARO, GRIS)

    flecha(ax, (23, 77), (38, 77))
    etiqueta(ax, 30.5, 81, "WiFi 2.4 GHz", AZUL, 8.6)
    flecha(ax, (62, 79), (72, 79))
    etiqueta(ax, 67, 83, "WiFi", TEAL, 8.6)
    flecha(ax, (50, 65), (50, 49), color=VERDE)
    etiqueta(ax, 55.5, 57, "I2C", VERDE, 8.6)
    flecha(ax, (62, 70), (72, 52), color=NARANJA)
    etiqueta(ax, 69.5, 60, "GPIO\n27/26/25/33", NARANJA, 8.2)
    flecha(ax, (84.5, 39), (84.5, 26), color=NARANJA)
    etiqueta(ax, 90, 32.5, "potencia", NARANJA, 8.4)
    flecha(ax, (24, 52), (38, 66))
    etiqueta(ax, 29.5, 61, "USB 5 V", GRIS, 8.2)
    ax.plot([24, 68], [18, 18], color=GRIS, lw=1.8, solid_capstyle="round")
    ax.plot([68, 68], [18, 39], color=GRIS, lw=1.8, solid_capstyle="round")
    flecha(ax, (68, 39), (72, 39), color=GRIS)
    etiqueta(ax, 45, 21, "VDD ~6 V", GRIS, 8.4)

    ax.text(4, 5, "Las tierras de los dos circuitos van unidas: sin tierra común no hay referencia.",
            fontsize=8.4, color=GRIS, ha="left", va="bottom")
    pie(fig, "Esquema lógico, no a escala. Fuente: docs/referencia/hardware.md y firmware_carro/config.h.")


# ── Figura 2: capas del firmware ───────────────────────────────────────────
def fig_02():
    fig, (ax,) = nueva(
        "Capas del firmware",
        "La lógica en C no toca los pines: pide las cosas por funciones hw_... y así se puede probar en un PC.",
    )
    capas = [
        (80, 11, "App Android / navegador web", "El usuario manda órdenes y ve el estado", AZUL_CLARO, AZUL),
        (63, 12, "firmware_carro.ino\nWiFi · HTTP · WebSocket · setup()/loop()", "Recibe mensajes y los pasa a la lógica", TEAL_CLARO, TEAL),
        (44, 14, "carro.c · sensores.c · imu.c · tof.c · giro.c", "C puro: decide, vigila y no enciende pines", NARANJA_CLARO, NARANJA),
        (26, 12, "firmware_carro.ino · funciones hw_...\nPWM · I2C · millis · NVS · keep-alive", "Traduce la lógica al hardware real", GRIS_CLARO, GRIS),
        (7, 12, "Hardware: motores, TC1508A, MPU6050, VL53L0X, WiFi", "Lo que existe físicamente", GRIS_CLARO, GRIS),
    ]
    for y, h, texto, _sub, relleno, borde in capas:
        caja(ax, 6, y, 64, h, texto, relleno, borde, tam=9.8)

    for y1, y2 in [(80, 75), (63, 58), (44, 38), (26, 19)]:
        flecha(ax, (38, y1), (38, y2), color=AZUL, lw=1.6, estilo="<|-|>")
    ax.text(35, 77.5, "órdenes / estado", fontsize=7.4, color=GRIS, ha="right")
    ax.text(35, 60.5, "mensajes", fontsize=7.4, color=GRIS, ha="right")
    ax.text(35, 41.5, "hw_...", fontsize=7.4, color=GRIS, ha="right")
    ax.text(35, 22.5, "señales", fontsize=7.4, color=GRIS, ha="right")

    caja(ax, 77, 46, 19, 14, "Pruebas en PC\nhardware falso", VERDE_CLARO, VERDE, tam=9.0)
    flecha(ax, (77, 53), (70, 53), color=VERDE, lw=1.6)
    ax.text(86.5, 41, "La misma lógica\ncorre en el PC\ncon hardware falso.",
            fontsize=8.0, color=VERDE, ha="center", va="top", linespacing=1.35)
    ax.text(6, 3, "El .ino no decide: solo traduce órdenes y sensores.",
            fontsize=8.4, color=GRIS, ha="left", va="bottom")
    pie(fig, "Esquema de capas, no a escala. Fuente: docs/referencia/firmware.md y carro.h.")


# ── Figura 3: flujo de un comando ─────────────────────────────────────────
def fig_03():
    fig, (ax,) = nueva(
        "Qué pasa cuando llega un comando",
        "La app manda texto; el WebSocket lo encola y solo loop() lo ejecuta, para no tocar los motores desde dos tareas.",
    )
    xs = [14, 38, 62, 86]
    cabeceras = ["App", "WebSocket", "Cola", "loop() + carro.c"]
    for x, t in zip(xs, cabeceras):
        caja(ax, x - 9, 87, 18, 7, t, AZUL_CLARO, AZUL, tam=10, bold=True)
        ax.plot([x, x], [10, 87], color=GRIS, lw=1.0, ls=(0, (3, 3)), zorder=0)

    flecha(ax, (14, 79), (38, 79))
    etiqueta(ax, 26, 82, "FORWARD", AZUL, 9, bold=True)
    flecha(ax, (38, 71), (62, 71))
    etiqueta(ax, 50, 74, "carro_encolar_comando()", AZUL, 8.4)
    flecha(ax, (62, 62), (86, 62), color=TEAL, ls="--")
    etiqueta(ax, 74, 65, "carro_procesar_cola()", TEAL, 8.4)
    ax.plot([86], [54], marker="o", color=TEAL, ms=6)
    ax.text(88, 54, "carro_recibir_comando()\nejecutar_comando()", fontsize=8.2,
            color=TEAL, ha="left", va="center", linespacing=1.3)
    caja(ax, 77, 8, 18, 7, "Motores", NARANJA_CLARO, NARANJA, tam=9.5)
    flecha(ax, (86, 47), (86, 15), color=NARANJA)
    etiqueta(ax, 92, 31, "poner_motores()\n→ PWM", NARANJA, 8.2)

    flecha(ax, (14, 36), (38, 36), ls="--", color=GRIS)
    flecha(ax, (38, 32), (14, 32), ls="--", color=GRIS)
    etiqueta(ax, 26, 38.5, "PING cada 800 ms", GRIS, 8.2)
    etiqueta(ax, 26, 30.5, "PONG", GRIS, 8.2)

    flecha(ax, (14, 23), (62, 23), color=ROJO)
    etiqueta(ax, 38, 26, "STOP (siempre se ejecuta)", ROJO, 8.2)
    ax.text(2, 57, "La cola tiene 32 espacios.\nSi se llena, tira un PING viejo,\nnunca un STOP.",
            fontsize=8.0, color=GRIS, ha="left", va="center", linespacing=1.35)
    ax.text(2, 4, "Watchdog: si pasan 1000 ms sin un comando real, el carro frena solo.",
            fontsize=8.2, color=ROJO, ha="left", va="bottom")
    pie(fig, "Diagrama de secuencia simplificado. Fuente: firmware_carro.ino, carro.c y docs/protocolo.md.")


# ── Figura 4: estados del navegador ───────────────────────────────────────
def fig_04():
    fig, (ax,) = nueva(
        "Estados del navegador",
        "El cerebro siempre sabe en qué estado está; cada flecha es una regla del código, no una improvisación.",
    )

    def ruta(puntos, color=AZUL, lw=1.7, ls="-"):
        """Poli línea con una sola punta de flecha en el último tramo."""
        xs = [p[0] for p in puntos[:-1]]
        ys = [p[1] for p in puntos[:-1]]
        ax.plot(xs, ys, color=color, lw=lw, ls=ls, solid_capstyle="round",
                solid_joinstyle="round", zorder=1)
        flecha(ax, puntos[-2], puntos[-1], color=color, lw=lw, estilo="-|>", ls=ls)

    # Columna central: el recorrido normal, de arriba abajo.
    caja(ax, 36, 82, 22, 10, "ESPERANDO", AZUL_CLARO, AZUL, tam=9.2, bold=True)
    caja(ax, 36, 64, 22, 10, "SIGUIENDO", TEAL_CLARO, TEAL, tam=9.2, bold=True)
    caja(ax, 36, 46, 22, 10, "DETENIDO\nPOR OBSTÁCULO", AMARILLO_CLARO, AMARILLO, tam=8.4, bold=True)
    caja(ax, 36, 28, 22, 10, "ESQUIVANDO", NARANJA_CLARO, NARANJA, tam=9.0, bold=True)
    caja(ax, 36, 10, 22, 10, "REINCORPORANDO", VERDE_CLARO, VERDE, tam=8.4, bold=True)

    # Salidas laterales.
    caja(ax, 72, 64, 22, 10, "TERMINADO", GRIS_CLARO, GRIS, tam=9.2, bold=True)
    caja(ax, 72, 46, 22, 10, "ERROR", ROJO_CLARO, ROJO, tam=9.2, bold=True)
    caja(ax, 72, 28, 22, 10, "AYUDA\n(la persona maneja)", AMARILLO_CLARO, AMARILLO, tam=8.2, bold=True)

    # Recorrido normal.
    flecha(ax, (47, 82), (47, 74))
    etiqueta(ax, 49, 78, "comenzar", GRIS, 7.8, ha="left")
    flecha(ax, (47, 64), (47, 56))
    etiqueta(ax, 49, 60, "ve obstáculo", GRIS, 7.8, ha="left")
    flecha(ax, (47, 46), (47, 38))
    etiqueta(ax, 49, 42, "hay espacio", GRIS, 7.8, ha="left")
    flecha(ax, (47, 28), (47, 20))
    etiqueta(ax, 49, 24, "pasó la caja", GRIS, 7.8, ha="left")

    # Salidas.
    flecha(ax, (58, 69), (72, 69))
    etiqueta(ax, 65, 71.5, "fin de ruta", GRIS, 7.8)
    flecha(ax, (58, 51), (72, 51), color=ROJO)
    etiqueta(ax, 65, 53.5, "no puede esquivar", ROJO, 7.8)
    flecha(ax, (58, 33), (72, 33), color=AMARILLO)
    etiqueta(ax, 65, 35.5, "no puede seguir\n(si pedir ayuda)", AMARILLO, 7.4)

    # AYUDA: el usuario maneja y decide continuar; si no, se agota el tiempo.
    flecha(ax, (83, 38), (83, 46), color=AMARILLO)
    etiqueta(ax, 81, 42, "3 min sin\ncontinuar", AMARILLO, 7.2, ha="right")
    ruta([(83, 28), (83, 15), (58, 15)], color=AMARILLO)
    etiqueta(ax, 70.5, 16.8, "el usuario continúa", AMARILLO, 7.6, va="bottom")

    # Vuelta a la ruta por la izquierda.
    ruta([(36, 15), (16, 15), (16, 69), (36, 69)], color=VERDE, lw=1.9)
    etiqueta(ax, 18.5, 42, "volvió a la línea", VERDE, 7.8, rot=90)

    ax.text(3, 8.0, "En ESPERANDO, TERMINADO y ERROR el comando es siempre STOP.",
            fontsize=8.8, color=ROJO, ha="left", va="bottom", fontweight="bold")
    ax.text(3, 1.5, "Si el obstáculo se va antes de decidir, DETENIDO vuelve a SIGUIENDO.\n"
                    "Para empezar otra ruta se reinicia el navegador: vuelve a ESPERANDO.",
            fontsize=7.6, color=GRIS, ha="left", va="bottom", linespacing=1.35)
    pie(fig, "Fuente: nucleo_c/navegador.h y navegador.c. Esquema de estados, no a escala.")


# ── Figura 5: pose antes/después de J15 ───────────────────────────────────
def fig_05():
    import numpy as np
    fig, (ax1, ax2) = nueva(
        "Cómo se estima la pose",
        "El carro no tiene encoders ni GPS: la distancia sale del tiempo y la dirección, del giroscopio.",
        dos=True,
    )
    t = np.linspace(0, 1, 240)
    x = 20 + 52 * t + 10 * np.sin(2 * np.pi * t)
    y = 8 + 84 * t

    for ax in (ax1, ax2):
        ax.set_xlim(0, 100)
        ax.set_ylim(0, 100)
        ax.set_xticks([])
        ax.set_yticks([])
        ax.set_aspect("equal")
        ax.grid(color="#e9ecef", lw=0.6, zorder=0)
        ax.plot(x, y, color=TEAL, lw=3.0, solid_capstyle="round", zorder=2)

    # Antes: todo el avance con el rumbo inicial (hacia arriba).
    ax1.plot([20, 20], [8, 92], color=NARANJA, lw=2.4, ls="--", zorder=3)
    ax1.annotate("", xy=(20, 93), xytext=(20, 88),
                 arrowprops=dict(arrowstyle="-|>", color=NARANJA, lw=2))
    ax1.text(23, 94, "error lateral", color=ROJO, fontsize=9, fontweight="bold")
    ax1.annotate("", xy=(20, 86), xytext=(x[-1], 88),
                 arrowprops=dict(arrowstyle="<|-|>", color=ROJO, lw=1.2, ls="--"))
    ax1.set_title("Antes de J15", fontsize=13, color=AZUL, fontweight="bold", pad=10)
    ax1.text(2, 2, "Todo el avance se proyectaba con el rumbo inicial.\nUn giro a mitad de camino inventaba un desplazamiento.",
             fontsize=8.2, color=GRIS, ha="left", va="bottom", linespacing=1.35)

    # J15: cada incremento se integra con el rumbo observado.
    x_est = x - 1.4 * np.sin(3 * np.pi * t)
    ax2.plot(x_est, y, color=NARANJA, lw=2.2, ls="--", zorder=3)
    for k in range(1, 8):
        i = int(k * len(t) / 9)
        j = min(i + 8, len(t) - 1)
        ax2.annotate("", xy=(x[j], y[j]), xytext=(x[i], y[i]),
                     arrowprops=dict(arrowstyle="-|>", color=AZUL, lw=1.0, alpha=0.65))
    ax2.set_title("Con J15", fontsize=13, color=AZUL, fontweight="bold", pad=10)
    ax2.text(2, 2, "Cada tramo usa el rumbo del giroscopio.\nLa distancia sigue siendo por tiempo.",
             fontsize=8.2, color=GRIS, ha="left", va="bottom", linespacing=1.35)
    ax2.text(96, 92, "Simulación: 1.9 cm medio\np95 4.6 cm (400 rutas B6)",
             fontsize=8.0, color=AZUL, ha="right", va="top", linespacing=1.3,
             bbox=dict(boxstyle="round,pad=0.35", fc=AMARILLO_CLARO, ec=AMARILLO, lw=1))

    fig.text(0.5, 0.062,
             "Línea verde continua: camino real simulado.   Línea naranja discontinua: pose estimada.",
             ha="center", va="center", fontsize=8.2, color=GRIS)
    pie(fig, "Concepto, no a escala. Cifras simuladas en docs/reporte_J15.md; la prueba física es NO MEDIDA.")


# ── Figura 6: esquive y reincorporación ───────────────────────────────────
def fig_06():
    fig, (ax,) = nueva(
        "Esquivar y volver a la línea",
        "El carro se aparta 40 cm, pasa la caja y busca un punto 30 cm por delante para reincorporarse.",
    )
    # Cuadrícula: 1 celda = 20 cm = 10 unidades.
    for k in range(0, 101, 10):
        ax.plot([k, k], [0, 100], color="#e9ecef", lw=0.7, zorder=0)
        ax.plot([0, 100], [k, k], color="#e9ecef", lw=0.7, zorder=0)

    ax.plot([50, 50], [4, 96], color=AZUL, lw=2.0, ls=(0, (6, 4)), zorder=1)
    ax.text(51, 4, "ruta original", fontsize=8.0, color=AZUL, ha="left", va="bottom")

    caja(ax, 45, 60, 10, 10, "caja\n20×20 cm", NARANJA_CLARO, NARANJA, tam=8.2, bold=True)
    ax.text(58, 65, "obstáculo", fontsize=8.2, color=NARANJA, ha="left", va="center")

    # Carro inicial
    caja(ax, 47.5, 14, 5, 7, "", AZUL_CLARO, AZUL, radio=0.8)
    ax.annotate("", xy=(50, 24), xytext=(50, 21),
                arrowprops=dict(arrowstyle="-|>", color=AZUL, lw=1.6))
    ax.text(50, 11, "carro", fontsize=8.2, color=AZUL, ha="center", va="top")

    # Desvío: izquierda 40 cm, subir, volver.
    puntos = [(50, 40), (30, 40), (30, 80), (50, 80)]
    ax.plot([p[0] for p in puntos], [p[1] for p in puntos], color=TEAL, lw=3.0,
            solid_capstyle="round", zorder=2)
    for p1, p2 in zip(puntos[:-1], puntos[1:]):
        ax.annotate("", xy=p2, xytext=p1,
                    arrowprops=dict(arrowstyle="-|>", color=TEAL, lw=1.4,
                                    shrinkA=4, shrinkB=4))
    ax.text(30, 31, "40 cm", fontsize=9.0, color=TEAL, ha="center", va="top", fontweight="bold")
    ax.annotate("", xy=(30, 34), xytext=(50, 34),
                arrowprops=dict(arrowstyle="<|-|>", color=TEAL, lw=1.1))
    ax.text(31, 44, "salida de la línea", fontsize=7.8, color=TEAL, ha="left", va="bottom")
    ax.text(52, 78, "vuelta a la línea", fontsize=8.0, color=VERDE, ha="left", va="bottom")
    ax.text(48, 84, "30 cm por delante\nde la caja", fontsize=8.0, color=VERDE,
            ha="right", va="bottom", linespacing=1.3)

    ax.text(2, 2, "1 celda = 20 cm. Esquema, no a escala; medidas de navegador.h.",
            fontsize=7.8, color=GRIS, ha="left", va="bottom")
    pie(fig, "Fuente: nucleo_c/navegador.h (NAV_TRAMO_ESQUIVAR_CM = 40, NAV_REINC_ADELANTE_CM = 30).")


# ── Figura 7: corredor de trayectoria ─────────────────────────────────────
def fig_07():
    fig, (ax1, ax2) = nueva(
        "El corredor de trayectoria",
        "No es lo mismo un objeto al lado del camino que uno dentro de la franja por la que el carro va a pasar.",
        dos=True,
    )
    # Vista lateral.
    ax1.set_xlim(0, 100)
    ax1.set_ylim(0, 100)
    ax1.axis("off")
    ax1.plot([4, 96], [15, 15], color=GRIS, lw=2.0)
    ax1.text(95, 11, "piso", fontsize=8.0, color=GRIS, ha="right", va="top")
    caja(ax1, 10, 62, 12, 8, "cámara", AZUL_CLARO, AZUL, tam=8.2)
    ax1.plot([16, 70], [62, 15], color=TEAL, lw=1.2, ls="--")
    ax1.plot([16, 94], [62, 15], color=TEAL, lw=1.2, ls="--")
    ax1.add_patch(Polygon([(16, 62), (70, 15), (94, 15)], closed=True,
                          facecolor=TEAL, alpha=0.12, edgecolor="none"))
    ax1.plot([16, 16], [15, 62], color=GRIS, lw=1.0, ls=":")
    ax1.annotate("", xy=(16, 40), xytext=(11, 40),
                 arrowprops=dict(arrowstyle="<|-|>", color=GRIS, lw=1.0))
    ax1.text(13.5, 43, "altura", fontsize=7.6, color=GRIS, ha="center", va="bottom")
    ax1.annotate("", xy=(21, 61), xytext=(24, 55),
                 arrowprops=dict(arrowstyle="-", color=AMARILLO, lw=1.4))
    ax1.text(25.5, 58, "inclinación", fontsize=7.6, color=AMARILLO, ha="left", va="center")
    ax1.text(55, 75, "el ancho que ve la cámara\ncrece con la distancia",
             fontsize=8.2, color=TEAL, ha="center", va="center", linespacing=1.3)
    ax1.text(4, 4, "Vista de lado", fontsize=10.5, color=AZUL, fontweight="bold", ha="left", va="bottom")

    # Vista de la imagen.
    ax2.set_xlim(0, 100)
    ax2.set_ylim(0, 100)
    ax2.axis("off")
    ax2.add_patch(Rectangle((8, 8), 84, 84, fill=False, edgecolor=GRIS, lw=1.5))
    ax2.plot([8, 92], [62, 62], color=GRIS, lw=1.0, ls=":")
    ax2.text(10, 64, "horizonte", fontsize=7.4, color=GRIS, ha="left", va="bottom")
    ax2.add_patch(Polygon([(30, 8), (70, 8), (56, 62), (44, 62)], closed=True,
                          facecolor=TEAL, alpha=0.18, edgecolor=TEAL, lw=1.2))
    ax2.plot([30, 44], [8, 62], color=TEAL, lw=1.0)
    ax2.plot([70, 56], [8, 62], color=TEAL, lw=1.0)
    ax2.text(50, 70, "corredor: ancho del carro + margen",
             fontsize=8.0, color=TEAL, ha="center", va="bottom")
    caja(ax2, 40, 28, 10, 10, "caja", NARANJA_CLARO, NARANJA, tam=8.0, bold=True)
    ax2.text(52.5, 33, "dentro: frena", fontsize=8.0, color=NARANJA, ha="left", va="center")
    caja(ax2, 74, 30, 9, 9, "caja", GRIS_CLARO, GRIS, tam=7.6)
    ax2.text(73, 24, "fuera: solo avisa", fontsize=8.0, color=GRIS, ha="right", va="top")
    ax2.text(4, 4, "Vista de la imagen", fontsize=10.5, color=AZUL, fontweight="bold", ha="left", va="bottom")
    pie(fig, "Esquema geométrico, no a escala. Fuente: nucleo_c/corredor.h y docs/obstaculos.md.")


# ── Figura 8: mapa de pantallas ───────────────────────────────────────────
def fig_08():
    fig, (ax,) = nueva(
        "Mapa de pantallas de la app",
        "Cuatro pestañas para el uso diario y pantallas completas para tareas que necesitan más espacio.",
    )
    caja(ax, 30, 84, 40, 10, "MainActivity · cabecera y barra inferior", AZUL_CLARO, AZUL, tam=10.2, bold=True)
    tabs = [
        (4, "Inicio", "estado, conexión,\nsensores, batería"),
        (26, "Manual", "botones, perfil,\nvideo, giro, PARO"),
        (48, "Rutas", "editor, ejecutar,\nsimular"),
        (70, "Más", "ajustes, calibración,\nsensores, visión,\nayuda, acerca"),
    ]
    for x, nombre, detalle in tabs:
        caja(ax, x, 66, 18, 9, nombre, TEAL_CLARO, TEAL, tam=10, bold=True)
        flecha(ax, (50, 84), (x + 9, 75), color=AZUL, lw=1.1)
        caja(ax, x, 48, 18, 12, detalle, GRIS_CLARO, GRIS, tam=7.8)

    caja(ax, 4, 16, 92, 24, "", GRIS_CLARO, GRIS, radio=1.5)
    ax.text(50, 36, "Pantallas completas", fontsize=10, color=AZUL, fontweight="bold",
            ha="center", va="center")
    pantallas = ["VisionActivity", "CalibracionActivity", "CalibrarGiroActivity",
                 "SensoresActivity", "BenchmarkActivity"]
    for i, nombre in enumerate(pantallas):
        x = 7 + i * 17.8
        caja(ax, x, 21, 16, 9, nombre, "white", GRIS, tam=7.2, radio=0.8)
    flecha(ax, (79, 48), (79, 40), color=GRIS, lw=1.4)
    ax.text(50, 8, "SesionCarro y SesionVideo son únicas y compartidas: cambiar de pantalla no corta la conexión ni el video.",
            fontsize=8.2, color=GRIS, ha="center", va="bottom")
    pie(fig, "Esquema de navegación, no a escala. Fuente: app_android y docs/referencia/app.md.")


# ── Figura 9: modos de la cámara ──────────────────────────────────────────
def fig_09():
    fig, (ax,) = nueva(
        "Los tres modos de la cámara",
        "La misma placa puede mandar video, un cuadro sin comprimir o solo los colores del piso.",
    )
    filas = [
        (72, "JPEG", "HTTP /stream · MJPEG", "ver en vivo en el teléfono", TEAL_CLARO, TEAL),
        (46, "RAW", "HTTP /raw · RGB565", "procesar sin pérdida de calidad", AZUL_CLARO, AZUL),
        (20, "CELDAS", "UDP 4210 · celdas de 10×10", "detectar obstáculos sin mandar video", NARANJA_CLARO, NARANJA),
    ]
    for y, nombre, transporte, uso, relleno, borde in filas:
        ax.add_patch(FancyBboxPatch((3, y - 4), 94, 18, boxstyle="round,pad=0,rounding_size=1.2",
                                    facecolor=relleno, edgecolor="none", alpha=0.55, zorder=0))
        caja(ax, 18, y, 20, 10, "Cámara\n192.168.4.2", AMARILLO_CLARO, AMARILLO, tam=8.4)
        caja(ax, 74, y, 20, 10, "Teléfono\napp FSI", AZUL_CLARO, AZUL, tam=8.4)
        flecha(ax, (38, y + 5), (74, y + 5), color=borde, lw=2.0)
        etiqueta(ax, 56, y + 8, transporte, borde, 8.8, bold=True)
        ax.text(56, y - 1.2, uso, fontsize=8.0, color=GRIS, ha="center", va="top")
        ax.text(4, y + 5, nombre, fontsize=9.0, color=borde, ha="left", va="center", fontweight="bold")

    ax.text(6, 8, "HQVGA: 240×176 px → 24×17 celdas = 1244 bytes por paquete.",
            fontsize=8.2, color=NARANJA, ha="left", va="bottom")
    ax.text(6, 4, "La cámara atiende un solo cliente de video a la vez; la app comparte ese cliente entre pantallas.",
            fontsize=8.0, color=GRIS, ha="left", va="bottom")
    pie(fig, "Esquema de datos. Fuente: camara_prueba/camara_prueba.ino y nucleo_c/paquete_celdas.h.")


# ── Figura 10: alimentación ───────────────────────────────────────────────
def fig_10():
    fig, (ax,) = nueva(
        "Los dos circuitos de alimentación",
        "La electrónica y los motores van separados; las tierras se unen para que todos hablen el mismo idioma.",
    )
    caja(ax, 4, 70, 20, 12, "Powerbank\n5 V USB", GRIS_CLARO, GRIS, tam=9.0)
    caja(ax, 34, 70, 24, 12, "ESP32 DevKit", TEAL_CLARO, TEAL, tam=9.5)
    caja(ax, 70, 70, 24, 12, "Freenove\n(cámara)\nUSB propio", AMARILLO_CLARO, AMARILLO, tam=8.4)
    caja(ax, 4, 35, 20, 12, "4 pilas AA\n~6 V", GRIS_CLARO, GRIS, tam=9.0)
    caja(ax, 34, 35, 24, 12, "TC1508A\nVDD", NARANJA_CLARO, NARANJA, tam=9.2)
    caja(ax, 70, 35, 24, 12, "4 motores TT", GRIS_CLARO, GRIS, tam=9.2)

    flecha(ax, (24, 76), (34, 76))
    etiqueta(ax, 29, 79, "5 V", GRIS, 8.4)
    flecha(ax, (58, 76), (70, 76), color=GRIS, ls="--")
    etiqueta(ax, 64, 79, "WiFi (enlace)", GRIS, 8.0)
    flecha(ax, (24, 41), (34, 41))
    etiqueta(ax, 29, 44, "VDD", NARANJA, 8.4)
    flecha(ax, (58, 41), (70, 41))
    etiqueta(ax, 64, 44, "potencia", NARANJA, 8.2)

    # Bus de tierra común: une la electrónica y el circuito de los motores.
    ax.plot([8, 90], [57, 57], color=GRIS, lw=1.6, ls=(0, (4, 3)), zorder=1)
    for x, y0, y1 in [(14, 47, 70), (46, 47, 70)]:
        ax.plot([x, x], [y0, y1], color=GRIS, lw=1.2, zorder=0)
    ax.text(90, 59, "GND común", fontsize=8.2, color=GRIS, ha="right", va="bottom")
    ax.text(30, 54, "tierra común de los dos circuitos", fontsize=7.6, color=GRIS,
            ha="center", va="top")

    ax.add_patch(FancyBboxPatch((4, 8), 92, 14, boxstyle="round,pad=0,rounding_size=1.2",
                                facecolor=ROJO_CLARO, edgecolor=ROJO, lw=1.5))
    ax.text(50, 17.5, "El positivo de las pilas va solo al VDD del driver.",
            fontsize=10.0, color=ROJO, ha="center", va="center", fontweight="bold")
    ax.text(50, 12.0, "Nunca a un pin de la ESP32. No se deben mover los motores con las pilas desconectadas.",
            fontsize=8.2, color=ROJO, ha="center", va="center")
    pie(fig, "Esquema eléctrico simplificado, no a escala. Fuente: docs/conexiones.md y docs/referencia/hardware.md.")


# ── Figura 11: protocolo ──────────────────────────────────────────────────
def fig_11():
    fig, (ax,) = nueva(
        "Mensajes entre la app y el carro",
        "Órdenes, respuestas, latido y sensores: todo viaja como texto por el mismo WebSocket.",
    )
    xs = [22, 78]
    for x, t in zip(xs, ["App Android", "ESP32 del carro"]):
        caja(ax, x - 14, 88, 28, 8, t, AZUL_CLARO, AZUL, tam=10, bold=True)
        ax.plot([x, x], [26, 88], color=GRIS, lw=1.0, ls=(0, (3, 3)), zorder=0)

    def evento(y, texto, respuesta=False):
        color = TEAL if respuesta else AZUL
        if respuesta:
            flecha(ax, (78, y), (22, y), color=color, ls="--")
        else:
            flecha(ax, (22, y), (78, y), color=color)
        etiqueta(ax, 50, y + 3, texto, color, 8.6)

    evento(80, "MODO_AUTO")
    evento(74, "ACK_AUTO", respuesta=True)
    evento(66, "PING")
    evento(60, "PONG", respuesta=True)
    evento(52, "FORWARD  (se repite cada 250 ms)")
    evento(44, "SENS:ángulo,mm,banderas  (cada 100 ms en AUTO)", respuesta=True)
    evento(36, "STOP")

    ax.add_patch(FancyBboxPatch((4, 8), 92, 16, boxstyle="round,pad=0,rounding_size=1.2",
                                facecolor=AMARILLO_CLARO, edgecolor=AMARILLO, lw=1.3))
    ax.text(50, 20, "Watchdog de comunicación: 1000 ms sin un comando real → STOP y vuelve a AUTO.",
            fontsize=9.2, color=AZUL, ha="center", va="center", fontweight="bold")
    ax.text(50, 14, "PING no renueva el watchdog. La app repite el movimiento cada 250 ms mientras el botón está apretado.",
            fontsize=8.2, color=GRIS, ha="center", va="center")
    ax.annotate("", xy=(4, 30), xytext=(4, 84),
                arrowprops=dict(arrowstyle="-|>", color=GRIS, lw=1.0))
    ax.text(5.5, 57, "tiempo", fontsize=8.0, color=GRIS, rotation=90, va="center")
    pie(fig, "Diagrama de secuencia simplificado. Fuente: app_android/ControlLogica.kt, firmware_carro/carro.c y docs/protocolo.md.")


# ── Figura 12: navegar por tiempo ─────────────────────────────────────────
def fig_12():
    fig, (ax1, ax2) = nueva(
        "Navegar por tiempo",
        "El carro no mide centímetros: mide milisegundos. La calibración convierte unos en otros.",
        dos=True,
    )
    # Panel izquierdo: perfil de un avance.
    ax1.set_xlim(0, 100)
    ax1.set_ylim(0, 100)
    ax1.axis("off")
    ax1.plot([8, 94], [12, 12], color=GRIS, lw=1.4)
    ax1.plot([8, 8], [12, 94], color=GRIS, lw=1.4)
    ax1.text(94, 8, "tiempo", fontsize=8.2, color=GRIS, ha="right", va="top")
    ax1.text(5, 53, "distancia", fontsize=8.2, color=GRIS, ha="center", va="center", rotation=90)
    ax1.plot([10, 24], [18, 18], color=AZUL, lw=3.0)
    ax1.plot([24, 72], [18, 76], color=TEAL, lw=3.0)
    ax1.plot([72, 86], [76, 84], color=NARANJA, lw=3.0)
    ax1.plot([24, 24], [18, 30], color=GRIS, lw=1.0, ls=":")
    ax1.plot([72, 72], [76, 60], color=GRIS, lw=1.0, ls=":")
    ax1.annotate("", xy=(24, 22), xytext=(10, 22),
                 arrowprops=dict(arrowstyle="<|-|>", color=AZUL, lw=1.2))
    ax1.text(17, 26, "arranque_ms", fontsize=8.2, color=AZUL, ha="center", va="bottom")
    ax1.text(40, 62, "avance = tiempo × ms/cm", fontsize=8.6, color=TEAL,
             ha="center", va="center",
             bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="none", alpha=0.95))
    ax1.annotate("", xy=(40, 38), xytext=(40, 59),
                 arrowprops=dict(arrowstyle="-", color=TEAL, lw=0.8, ls=":"))
    ax1.plot([72, 88], [76, 76], color=GRIS, lw=0.8, ls=":")
    ax1.plot([86, 88], [84, 84], color=GRIS, lw=0.8, ls=":")
    ax1.annotate("", xy=(88, 84), xytext=(88, 76),
                 arrowprops=dict(arrowstyle="<|-|>", color=NARANJA, lw=1.2))
    ax1.text(90, 80, "frenado_mm", fontsize=8.2, color=NARANJA, ha="left", va="center")
    ax1.text(8, 3, "Un avance: primero espera, luego corre y al final se desliza.",
             fontsize=8.2, color=GRIS, ha="left", va="bottom")
    ax1.text(4, 99, "Perfil de un avance", fontsize=11, color=AZUL, fontweight="bold", ha="left", va="top")

    # Panel derecho: recta de calibración.
    ax2.set_xlim(0, 100)
    ax2.set_ylim(0, 100)
    ax2.set_xlabel("distancia (cm)", fontsize=9, color=GRIS)
    ax2.set_ylabel("tiempo (ms)", fontsize=9, color=GRIS)
    ax2.set_xticks([])
    ax2.set_yticks([])
    ax2.plot([10, 90], [18, 86], color=TEAL, lw=2.6)
    for x, y in [(26, 31), (50, 54), (74, 77)]:
        ax2.plot([x], [y], "o", color=NARANJA, ms=8, zorder=4)
    ax2.text(62, 30, "pendiente = ms/cm", fontsize=8.6, color=TEAL, rotation=40,
             ha="center", va="center")
    ax2.annotate("", xy=(10, 18), xytext=(10, 10),
                 arrowprops=dict(arrowstyle="<|-|>", color=AZUL, lw=1.2))
    ax2.text(12, 13, "arranque", fontsize=8.0, color=AZUL, ha="left", va="center")
    ax2.text(6, 95, "Recta de calibración", fontsize=11, color=AZUL, fontweight="bold", ha="left", va="top")
    ax2.text(52, 5, "Se mide con varias distancias y se ajusta una recta;\nla pendiente es el tiempo por centímetro.",
             fontsize=8.0, color=GRIS, ha="center", va="bottom", linespacing=1.3)
    pie(fig, "Ejemplo ilustrativo: 50 ms/cm y 10 ms/grado son valores de partida. Los reales se miden con la app (docs/calibracion.md).")


DIBUJOS = {
    "fig_01_sistema": fig_01,
    "fig_02_capas_firmware": fig_02,
    "fig_03_flujo_comando": fig_03,
    "fig_04_estados_navegador": fig_04,
    "fig_05_pose_j15": fig_05,
    "fig_06_esquive": fig_06,
    "fig_07_corredor": fig_07,
    "fig_08_pantallas_app": fig_08,
    "fig_09_camara_modos": fig_09,
    "fig_10_alimentacion": fig_10,
    "fig_11_protocolo": fig_11,
    "fig_12_calibracion": fig_12,
}


def escribir_readme():
    lineas = [
        "# Figuras del manual",
        "",
        "**Generado por** `docs/figuras/generar_figuras.py` a partir de `docs/figuras/catalogo.py`.",
        "No se editan a mano: se cambia el dibujo y se vuelve a ejecutar el script.",
        "",
        "Cada figura existe en PNG (170 dpi, para el PDF) y SVG (vectorial, para ampliar o editar).",
        "Los pies y las notas viven en `catalogo.py`, para que el texto y la imagen no se separen.",
        "",
        "| N.º | Título | Archivo | Capítulo | Qué muestra |",
        "|---|---|---|---|---|",
    ]
    for f in FIGURAS:
        lineas.append(
            f"| {f['id'].replace('fig_', '')} | {f['titulo']} | `{f['archivo']}.png` / `{f['archivo']}.svg` | {f['capitulo']} | {f['resumen']} |"
        )
    lineas += [
        "",
        "## Cómo regenerar",
        "",
        "```bash",
        "python docs/figuras/generar_figuras.py",
        "```",
        "",
        "Requiere Python 3.9 o más nuevo y `matplotlib` 3.9.4 (el entorno usado en este repositorio).",
        "El script no necesita conexión a internet ni el carro.",
        "",
        "## Reglas",
        "",
        "- Ninguna figura inventa una medición. Si un valor es simulado o no medido, se dice en la figura o en el pie.",
        "- Las figuras son esquemas: no son planos de montaje ni sustituyen una foto.",
        "- Las fotos reales van en `docs/imagenes/` y el manual las incluye aparte.",
        "",
    ]
    (AQUI / "README.md").write_text("\n".join(lineas), encoding="utf-8")


def main():
    print(f"Generando {len(FIGURAS)} figuras en {AQUI}")
    for f in FIGURAS:
        archivo = f["archivo"]
        if archivo not in DIBUJOS:
            raise SystemExit(f"Falta el dibujo de {archivo}")
        png, svg = guardar(DIBUJOS[archivo](), archivo)
        print(f"  {png.name:34s} {png.stat().st_size/1024:7.1f} KB   {svg.name}")
    escribir_readme()
    print("Listo. También se reescribió docs/figuras/README.md")


if __name__ == "__main__":
    main()
