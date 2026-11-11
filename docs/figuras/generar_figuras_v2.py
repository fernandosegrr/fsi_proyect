#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generar_figuras_v2.py — Genera las figuras y láminas del Manual Técnico v1.1.

Diseñado para ejecutarse de forma independiente o invocado desde
docs/figuras/generar_figuras.py y docs/manual/generar_manual.py.
"""
from pathlib import Path
import io
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
import matplotlib.image as mpimg

AQUI = Path(__file__).resolve().parent
DESTINO = AQUI / "v2"
IMAGENES = AQUI.parent / "imagenes"

# Paleta institucional unificada con el PDF
AZUL = "#163c60"
AZUL_CLARO = "#e9f1f7"
VERDE = "#00877e"
VERDE_CLARO = "#e6f5f3"
BOSQUE = "#2d6a4f"
BOSQUE_CLARO = "#e8f5ee"
CORAL = "#d95f43"
CORAL_CLARO = "#fdeee9"
AMBAR = "#d9822b"
AMBAR_CLARO = "#fef4e8"
GRIS = "#52606d"
GRIS_CLARO = "#f4f6f8"
ROJO = "#b00020"
ROJO_CLARO = "#fbe9eb"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 8.2,
    "text.color": AZUL,
    "pdf.fonttype": 42,
    "svg.fonttype": "none",
})


def _lienzo(w=11.0, h=5.3):
    fig, ax = plt.subplots(figsize=(w, h), facecolor="white")
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis("off")
    return fig, ax


def _caja(ax, x, y, w, h, texto, fc=AZUL_CLARO, ec=AZUL, fs=8.0, bold=False,
          color=AZUL, lw=1.4, pad=0.6):
    ax.add_patch(FancyBboxPatch(
        (x, y), w, h,
        boxstyle=f"round,pad={pad},rounding_size=1.3",
        facecolor=fc, edgecolor=ec, linewidth=lw, zorder=2
    ))
    ax.text(
        x + w / 2, y + h / 2, texto,
        ha="center", va="center", fontsize=fs,
        fontweight="bold" if bold else "normal",
        color=color, linespacing=1.22, zorder=3
    )


def _flecha(ax, p1, p2, color=AZUL, lw=1.6, estilo="-|>", ls="-", rad=0.0):
    ax.add_patch(FancyArrowPatch(
        p1, p2, arrowstyle=estilo, mutation_scale=12,
        color=color, linewidth=lw, linestyle=ls,
        connectionstyle=f"arc3,rad={rad}", shrinkA=2, shrinkB=2, zorder=4
    ))


def _etiqueta(ax, x, y, texto, color=AZUL, fs=7.7, bold=False, fc="white", ec="none"):
    bbox = dict(boxstyle="round,pad=0.22", facecolor=fc, edgecolor=ec, alpha=0.96) if fc else None
    ax.text(
        x, y, texto, ha="center", va="center", fontsize=fs,
        color=color, fontweight="bold" if bold else "normal",
        bbox=bbox, linespacing=1.18, zorder=5
    )


def _guardar(fig, nombres):
    DESTINO.mkdir(parents=True, exist_ok=True)
    if isinstance(nombres, str):
        nombres = [nombres]
    buf_png = io.BytesIO()
    fig.savefig(buf_png, format="png", dpi=220, bbox_inches="tight", pad_inches=0.08, facecolor="white")
    datos_png = buf_png.getvalue()
    buf_pdf = io.BytesIO()
    fig.savefig(buf_pdf, format="pdf", bbox_inches="tight", pad_inches=0.08, facecolor="white", metadata={"CreationDate": None})
    datos_pdf = buf_pdf.getvalue()
    for nombre in nombres:
        if nombre.startswith("fig_"):
            (AQUI / f"{nombre}.png").write_bytes(datos_png)
            buf_svg = io.BytesIO()
            fig.savefig(buf_svg, format="svg", bbox_inches="tight", pad_inches=0.08, facecolor="white")
            (AQUI / f"{nombre}.svg").write_bytes(buf_svg.getvalue())
        else:
            (DESTINO / f"{nombre}.png").write_bytes(datos_png)
            (DESTINO / f"{nombre}.pdf").write_bytes(datos_pdf)
    plt.close(fig)


def fig_arquitectura():
    fig, ax = _lienzo(11.2, 5.5)
    # Tres nodos principales arriba
    _caja(ax, 2, 60, 27, 34,
          "Teléfono Android\nApp LIDIA (Kotlin + JNI)\n• Núcleo de navegación C99\n• Visión YCbCr + TFLite\n• Supervisión activa J16",
          AZUL_CLARO, AZUL, fs=8.0, bold=True)
    _caja(ax, 37, 60, 26, 34,
          "ESP32 DevKit (Carro)\nAP WiFi CarroFSI\n192.168.4.1 (:80 /ws)\n• Cola 32 órdenes + WDT\n• Lazo PI RUMBO y GIRO",
          VERDE_CLARO, VERDE, fs=8.0, bold=True)
    _caja(ax, 71, 60, 27, 34,
          "Freenove ESP32-WROVER\nCámara GC0308 (STA)\n192.168.4.2\n• HQVGA 240×176 @ 20 MHz\n• HTTP :80/:81 y UDP :4210",
          AMBAR_CLARO, AMBAR, fs=8.0, bold=True)

    # Enlaces inalámbricos
    _flecha(ax, (29.8, 80), (36.2, 80), AZUL, lw=1.8, estilo="<|-|>")
    _etiqueta(ax, 33, 88, "WebSocket /ws\nÓrdenes + SENS (10 Hz)", AZUL, fs=7.4, bold=True)
    _flecha(ax, (70.2, 76), (63.8, 76), AMBAR, lw=1.6, estilo="<|-|>", ls="--")
    _etiqueta(ax, 67, 84, "Se une a AP\nCarroFSI", AMBAR, fs=7.4)
    _flecha(ax, (70.5, 62), (29.5, 62), VERDE, lw=1.7, rad=0.14)
    _etiqueta(ax, 50, 50.5, "Flujo de video directo a la app (/stream MJPEG, /raw RGB565 o UDP :4210 celdas)", VERDE, fs=7.6, bold=True)

    # Sensores y etapa de potencia abajo
    _caja(ax, 37, 23, 26, 20,
          "Sensores en bus I2C\nSDA=GPIO 21 · SCL=GPIO 22\n• Giroscopio MPU6050 (0x68)\n• Láser ToF VL53L0X (0x29)",
          BOSQUE_CLARO, BOSQUE, fs=7.8)
    _caja(ax, 71, 23, 27, 20,
          "Etapa de tracción\nPuente H doble TC1508A\n• GPIO 27/26 (izq), 25/33 (der)\n• 4 motores TT con rodillos",
          CORAL_CLARO, CORAL, fs=7.8)
    _flecha(ax, (50, 59), (50, 44), BOSQUE, lw=1.7, estilo="<|-|>")
    _etiqueta(ax, 44.0, 47.0, "I2C 400 kHz", BOSQUE, fs=7.3)
    _flecha(ax, (63.5, 63), (71.5, 44), CORAL, lw=1.7)
    _etiqueta(ax, 68.5, 47.0, "PWM 1 kHz", CORAL, fs=7.3)

    # Tres fuentes de energía abajo del todo
    _caja(ax, 2, 3, 27, 14,
          "Powerbank USB #1 (5 V)\nAlimenta ESP32 DevKit + I2C",
          GRIS_CLARO, GRIS, fs=7.7)
    _caja(ax, 37, 3, 26, 14,
          "Portapilas 4×AA (~4.8–6 V)\nVDD de TC1508A (GND común)",
          GRIS_CLARO, GRIS, fs=7.7)
    _caja(ax, 71, 3, 27, 14,
          "Powerbank USB #2 (5 V)\nExclusiva cámara Freenove",
          GRIS_CLARO, GRIS, fs=7.7)
    _guardar(fig, ["arquitectura", "fig_01_sistema"])


def fig_alimentacion():
    fig, ax = _lienzo(11.2, 5.3)
    # Circuito 1: Lógica y sensores
    _caja(ax, 2, 72, 24, 21, "Powerbank USB #1\nSalida 5 V por cable USB\n(electrónica de control)", VERDE_CLARO, VERDE, fs=7.9, bold=True)
    _caja(ax, 35, 72, 26, 21, "ESP32 DevKit\nRegulador interno 3.3 V\nControl y red CarroFSI", AZUL_CLARO, AZUL, fs=7.9, bold=True)
    _caja(ax, 70, 72, 28, 21, "Sensores I2C (3.3 V)\nMPU6050 (0x68) + VL53L0X (0x29)\nSDA=GPIO 21 · SCL=GPIO 22", BOSQUE_CLARO, BOSQUE, fs=7.8)
    _flecha(ax, (26.6, 82.5), (34.4, 82.5), VERDE, lw=1.8)
    _etiqueta(ax, 30.5, 87.5, "5 V USB", VERDE, fs=7.4, bold=True)
    _flecha(ax, (61.6, 82.5), (69.4, 82.5), BOSQUE, lw=1.8)
    _etiqueta(ax, 65.5, 87.5, "3.3 V + I2C", BOSQUE, fs=7.4, bold=True)

    # Circuito 2: Potencia de motores
    _caja(ax, 2, 38, 24, 21, "Portapilas 4×AA\n4.8 V (NiMH) a 6.0 V (alc.)\ncon interruptor físico", AMBAR_CLARO, AMBAR, fs=7.9, bold=True)
    _caja(ax, 35, 38, 26, 21, "Puente H TC1508A\nEntrada de potencia VDD\nIN1..IN4 (GPIO 27/26/25/33)", CORAL_CLARO, CORAL, fs=7.8, bold=True)
    _caja(ax, 70, 38, 28, 21, "4 Motores DC TT\n2 canal izquierdo + 2 derecho\n(sin unión con 5 V / 3.3 V)", GRIS_CLARO, GRIS, fs=7.8)
    _flecha(ax, (26.6, 48.5), (34.4, 48.5), CORAL, lw=1.8)
    _etiqueta(ax, 30.5, 53.5, "VDD motor", CORAL, fs=7.4, bold=True)
    _flecha(ax, (61.6, 48.5), (69.4, 48.5), CORAL, lw=1.8)
    _etiqueta(ax, 65.5, 53.5, "Salidas M1/M2", CORAL, fs=7.4, bold=True)

    # Unión de tierra común y señales PWM
    _flecha(ax, (48, 71.5), (48, 59.5), AZUL, lw=1.6, estilo="<|-|>")
    _etiqueta(ax, 66, 65.5, "GND común obligatorio (DevKit ↔ TC1508A ↔ negativo 4×AA)\n+ 4 líneas lógicas PWM (GPIO 27, 26, 25, 33)", AZUL, fs=7.5, bold=True)

    # Circuito 3: Cámara aislada
    _caja(ax, 2, 5, 24, 21, "Powerbank USB #2\nSalida 5 V independiente\n(exclusiva de cámara)", VERDE_CLARO, VERDE, fs=7.9, bold=True)
    _caja(ax, 35, 5, 26, 21, "Freenove ESP32-WROVER\nCámara GC0308 (192.168.4.2)\nAislada del chasis", AMBAR_CLARO, AMBAR, fs=7.8, bold=True)
    _flecha(ax, (26.6, 15.5), (34.4, 15.5), VERDE, lw=1.8)
    _etiqueta(ax, 30.5, 20.5, "5 V USB", VERDE, fs=7.4, bold=True)
    _caja(ax, 66, 5, 32, 21, "Regla de protección eléctrica:\nEl positivo de las 4 pilas AA va SOLO\na VDD del TC1508A; jamás a la ESP32.", ROJO_CLARO, ROJO, fs=7.8, bold=True, color=ROJO)
    _guardar(fig, ["alimentacion", "fig_10_alimentacion"])


def fig_seguridad():
    fig, ax = _lienzo(11.2, 5.0)
    capas = [
        (76, "Capa 1 — Operador humano y entorno físico",
         "Supervisión visual directa, botón rojo PARO en pantalla e interruptor físico del portapilas 4×AA.",
         AZUL_CLARO, AZUL),
        (53, "Capa 2 — Supervisión activa en la aplicación Android (cierre J16)",
         "Corta latido FORWARD si llega TOF_PARO; aborta ruta ante WD:<n>, silencio de SENS > 1.5 s o freno láser no atendido.",
         VERDE_CLARO, VERDE),
        (30, "Capa 3 — Vigilancia temporal en el firmware (ESP32 DevKit)",
         "Cola de 32 ranuras donde STOP/EBRAKE nunca se descartan; watchdog de 1000 ms (emite WD:<n>) y WDT de hardware de 5000 ms.",
         AMBAR_CLARO, AMBAR),
        (7, "Capa 4 — Paro reflejo de hardware por sensor láser frontal (VL53L0X)",
         "En avance AUTO, si la distancia frontal cae por debajo de 120 mm, frena los motores en el propio ciclo local y emite TOF_PARO:<mm>.",
         CORAL_CLARO, CORAL),
    ]
    for y, titulo, desc, fc, ec in capas:
        _caja(ax, 3, y, 94, 18, f"{titulo}\n{desc}", fc, ec, fs=8.0)
    for y1, y2 in [(75.5, 71.5), (52.5, 48.5), (29.5, 25.5)]:
        _flecha(ax, (50, y1), (50, y2), AZUL, lw=1.5)
    _guardar(fig, ["seguridad", "fig_02_capas_firmware"])


def fig_comandos():
    fig, ax = _lienzo(11.2, 5.1)
    _caja(ax, 2, 58, 21, 32,
          "App Android\nControlLogica /\nModoSemiautonomo\n• Latido cada 250 ms\n• PING cada 800 ms",
          AZUL_CLARO, AZUL, fs=7.8, bold=True)
    _caja(ax, 27, 58, 21, 32,
          "Enlace WebSocket\nws://192.168.4.1/ws\nCola de 32 ranuras\n• Prioridad STOP\n• Descarta PING viejo",
          VERDE_CLARO, VERDE, fs=7.8, bold=True)
    _caja(ax, 52, 58, 21, 32,
          "Lazo loop() en C99\ncarro.c + sensores.c\n• Watchdog 1000 ms\n• Lazo PI RUMBO (J16)\n• Giro cerrado GIRO",
          AMBAR_CLARO, AMBAR, fs=7.8, bold=True)
    _caja(ax, 77, 58, 21, 32,
          "Tracción PWM\nTC1508A (1 kHz)\n• Arranque 255 (250 ms)\n• MANUAL 252/255\n• AUTO 207/210 ± Δu",
          CORAL_CLARO, CORAL, fs=7.8, bold=True)

    _flecha(ax, (23.5, 74), (26.5, 74), AZUL, lw=1.8)
    _flecha(ax, (48.5, 74), (51.5, 74), VERDE, lw=1.8)
    _flecha(ax, (73.5, 74), (76.5, 74), CORAL, lw=1.8)

    # Retorno de telemetría abajo
    _caja(ax, 8, 10, 84, 30,
          "Canal de retorno en tiempo real (ESP32 → App Android)\n"
          "• SENS:<ang_x10>,<mm>,<flags> cada 100 ms en AUTO (décimas de grado + distancia láser + banderas bit 0..4)\n"
          "• Avisos asíncronos de seguridad: WD:<n> (vencimiento de watchdog), TOF_PARO:<mm>, GIRO_FIN:<ang> y CAPS",
          BOSQUE_CLARO, BOSQUE, fs=7.9)
    _flecha(ax, (62.5, 57.5), (62.5, 40.5), BOSQUE, lw=1.6)
    _flecha(ax, (20, 40.5), (12.5, 57.5), BOSQUE, lw=1.6)
    _guardar(fig, ["comandos", "fig_03_flujo_comando", "fig_11_protocolo"])


def fig_modos_camara():
    fig, ax = _lienzo(11.4, 5.0)
    _caja(ax, 2, 16, 24, 72,
          "Cámara Freenove\nESP32-WROVER\nSensor GC0308\n• HQVGA 240×176\n• RGB565 @ 20 MHz\n• Buzón de último\n  cuadro (sin cola)\n• IP 192.168.4.2",
          AMBAR_CLARO, AMBAR, fs=7.8, bold=True)

    modos = [
        (68, "Modo 1 — MJPEG (/stream en puerto 81 y /capture en puerto 80)",
         "JPEG por software (JPEGENC calidad 50) · 12.4 fps · Edad media 137 ms · Vista humana y TFLite.",
         VERDE_CLARO, VERDE),
        (42, "Modo 2 — Cuadro crudo RGB565 (/raw en puerto 80)",
         "84,480 bytes por cuadro sin compresión JPEG · Verificación cromática exacta en laboratorio.",
         AZUL_CLARO, AZUL),
        (16, "Modo 3 — Rejilla compacta de celdas (UDP puerto 4210)",
         "Paquete CEL1 de 1,244 bytes (cabecera 20 B + 24×17 celdas RGB) · Mínima latencia para piso.",
         CORAL_CLARO, CORAL),
    ]
    for y, titulo, detalle, fc, ec in modos:
        _caja(ax, 33, y, 65, 20, f"{titulo}\n{detalle}", fc, ec, fs=7.5)
        _flecha(ax, (26.6, y + 10), (32.4, y + 10), ec, lw=1.7)
    _guardar(fig, ["camara_modos", "fig_09_camara_modos"])


def fig_navegacion():
    fig, ax = _lienzo(11.2, 5.1)
    _caja(ax, 2, 54, 21, 36,
          "1. Planificar ruta\ntrayectoria.c\n• Cuadrícula 20 cm\n• Convierte nodos a\n  pasos de giro y\n  avance en mm",
          AZUL_CLARO, AZUL, fs=7.8, bold=True)
    _caja(ax, 27, 54, 21, 36,
          "2. Guiar en recta\nRUMBO:<décimas> (J16)\n• Punto de mira 400 mm\n• Tope ±12° sobre recta\n• Suprime giros ≤ 8°\n• Lazo PI a 50 Hz",
          VERDE_CLARO, VERDE, fs=7.8, bold=True)
    _caja(ax, 52, 54, 21, 36,
          "3. Cerrar giros\nGIRO:<grados> + IMU\n• MPU6050 a 200 Hz\n• Escala en ppm (NVS)\n• Freno proporcional\n• Respaldo por tiempo",
          AMBAR_CLARO, AMBAR, fs=7.8, bold=True)
    _caja(ax, 77, 54, 21, 36,
          "4. Integrar pose\nrumbo_x10 (J15/J16)\n• Décimas de grado\n• Cuerda de pivote\n  2R·sin(Δθ/2)\n• Descarte dt en fallo",
          BOSQUE_CLARO, BOSQUE, fs=7.8, bold=True)

    _flecha(ax, (23.5, 72), (26.5, 72), AZUL, lw=1.7)
    _flecha(ax, (48.5, 72), (51.5, 72), VERDE, lw=1.7)
    _flecha(ax, (73.5, 72), (76.5, 72), AMBAR, lw=1.7)

    _caja(ax, 6, 10, 88, 28,
          "Realimentación continua cada 100 ms (navegador_aportar_imu + navegador_aportar_tof + visión)\n"
          "La pose integrada (x_mm, y_mm, rumbo_x10) corrige la referencia RUMBO en el siguiente ciclo sin detener el carro\n"
          "y sirve de base geométrica para el esquive en 3 tramos si aparece un obstáculo en el corredor.",
          GRIS_CLARO, GRIS, fs=7.9)
    _flecha(ax, (87.5, 53.5), (87.5, 38.5), BOSQUE, lw=1.6)
    _flecha(ax, (37.5, 38.5), (37.5, 53.5), VERDE, lw=1.6)
    _guardar(fig, ["navegacion", "fig_04_estados_navegador", "fig_05_pose_j15", "fig_12_calibracion"])


def fig_esquive():
    fig, ax = _lienzo(11.2, 5.3)
    # Cuadrícula suave de fondo
    for gx in range(8, 95, 8):
        ax.plot([gx, gx], [16, 88], color="#edf2f7", lw=0.8, zorder=0)
    for gy in range(18, 88, 10):
        ax.plot([8, 92], [gy, gy], color="#edf2f7", lw=0.8, zorder=0)

    # Línea de ruta original (horizontal en y=34)
    ax.plot([10, 90], [34, 34], "--", color=AZUL, lw=1.6, zorder=1)
    _etiqueta(ax, 78, 26, "Ruta original planificada (y = 0 mm)", AZUL, fs=7.8, bold=True)

    # Obstáculo en la ruta
    _caja(ax, 41, 25, 16, 18, "Obstáculo\n20×20 cm\n(prof. 250 mm)", CORAL_CLARO, CORAL, fs=7.8, bold=True)

    # Trayectoria de esquive en 3 tramos
    pts_x = [12, 26, 26, 66, 66, 88]
    pts_y = [34, 34, 72, 72, 34, 34]
    ax.plot(pts_x, pts_y, color=VERDE, lw=2.8, solid_capstyle="round", zorder=3)
    _flecha(ax, (14, 34), (24, 34), VERDE, lw=2.0)
    _flecha(ax, (26, 38), (26, 68), VERDE, lw=2.0)
    _flecha(ax, (30, 72), (62, 72), VERDE, lw=2.0)
    _flecha(ax, (66, 68), (66, 38), VERDE, lw=2.0)
    _flecha(ax, (68, 34), (86, 34), VERDE, lw=2.0)

    _etiqueta(ax, 16, 54, "Tramo 1:\nDesvío lateral\n400 mm (40 cm)", VERDE, fs=7.7, bold=True, fc=VERDE_CLARO, ec=VERDE)
    _etiqueta(ax, 46, 82, "Tramo 2: Avance paralelo con RUMBO\ndist_obs + 250 mm + margen J16 (150 mm)", VERDE, fs=7.7, bold=True, fc=VERDE_CLARO, ec=VERDE)
    _etiqueta(ax, 80, 55, "Tramo 3: Reincorporación\nhacia punto 300 mm adelante\nusando pose real integrada", BOSQUE, fs=7.7, bold=True, fc=BOSQUE_CLARO, ec=BOSQUE)

    # Cintillo inferior con estado de evidencia real J16
    _caja(ax, 6, 2, 88, 12,
          "Estado J16 — VALIDADO EN EL CARRO (escenario B6, 10-nov-2026): 6 corridas físicas, 0/6 toques de caja,\n"
          "5/6 completas autónomas con error lateral final aproximado entre ~0 y ~10 cm (1/6 en AYUDA sin contacto).",
          BOSQUE_CLARO, BOSQUE, fs=7.8, bold=True, color=BOSQUE)
    _guardar(fig, ["esquive", "fig_06_esquive"])


def fig_percepcion():
    fig, ax = _lienzo(11.2, 5.2)
    _caja(ax, 2, 69, 27, 24,
          "1. Clasificador de piso\nobstaculos.c (YCbCr/RGB)\n• Hasta 3 clústeres de suelo\n• Regla J10c en filas < 7\n• Máscara de artefactos",
          VERDE_CLARO, VERDE, fs=7.8, bold=True)
    _caja(ax, 2, 37, 27, 24,
          "2. Red neuronal de objetos\nDetectorTflite + ModeloEnHilo\n• EfficientDet-Lite0 (90 clases)\n• Hilo propio (vigencia 300 ms)\n• Modo opcional obj-any",
          AZUL_CLARO, AZUL, fs=7.8, bold=True)
    _caja(ax, 2, 5, 27, 24,
          "3. Sensor láser frontal\nVL53L0X (tof.c / navegador.c)\n• Paro hardware a 120 mm\n• Autoridad opcional en ruta\n  a 400 mm (columnas 3 a 6)",
          CORAL_CLARO, CORAL, fs=7.8, bold=True)

    _caja(ax, 36, 54, 26, 28,
          "Corredor de perspectiva\ncorredor.c\nProyecta el ancho físico del\ncarro (150 mm + margen)\nsegún la fila vertical",
          AMBAR_CLARO, AMBAR, fs=7.8, bold=True)
    _caja(ax, 36, 16, 26, 28,
          "Fusión sincronizada\nfusion.c + puente.c\nProtegida por mutex JNI\ncandado_vision (J16)\nCombina color, red y láser",
          BOSQUE_CLARO, BOSQUE, fs=7.8, bold=True)

    _caja(ax, 69, 30, 29, 42,
          "Decisión del navegador\nnavegador.c\n• LIBRE → sigue con RUMBO\n• FUERA DEL CORREDOR → avisa\n• EN EL CORREDOR → detiene,\n  evalúa lado libre (izq/der)\n  y ejecuta esquive en 3 tramos",
          AZUL_CLARO, AZUL, fs=7.8, bold=True)

    _flecha(ax, (29.6, 81), (35.4, 70), VERDE, lw=1.6)
    _flecha(ax, (29.6, 74), (35.4, 36), VERDE, lw=1.6)
    _flecha(ax, (29.6, 49), (35.4, 32), AZUL, lw=1.6)
    _flecha(ax, (29.6, 17), (35.4, 24), CORAL, lw=1.6)
    _flecha(ax, (62.6, 68), (68.4, 58), AMBAR, lw=1.7)
    _flecha(ax, (62.6, 30), (68.4, 44), BOSQUE, lw=1.7)
    _guardar(fig, ["percepcion", "fig_07_corredor"])


def fig_pantallas():
    fig, ax = _lienzo(11.4, 5.1)
    _caja(ax, 20, 80, 60, 14,
          "MainActivity — Contenedor principal con barra de estado y 4 pestañas",
          AZUL_CLARO, AZUL, fs=8.2, bold=True)

    tabs = [
        (2, "Pestaña 1: Inicio", "Estado de red CarroFSI,\nvideo MJPEG, resumen\nde sensores y batería"),
        (27, "Pestaña 2: Manual", "Cruceta de mando,\nperfiles MANUAL/AUTO,\ngiro por ángulo y PARO"),
        (52, "Pestaña 3: Rutas", "Cuadrícula 20 cm, aviso\nde calibración (J16),\nejecutar y simular"),
        (77, "Pestaña 4: Más", "Ajustes, acceso a las\n5 pantallas completas,\nayuda y versión"),
    ]
    for x, titulo, desc in tabs:
        _caja(ax, x, 47, 21, 23, f"{titulo}\n{desc}", VERDE_CLARO, VERDE, fs=7.6, bold=True)
        _flecha(ax, (50, 79.5), (x + 10.5, 70.5), AZUL, lw=1.3)

    _caja(ax, 2, 21, 96, 18,
          "5 Actividades de pantalla completa:  VisionActivity (modelo de piso y corredor)  ·  "
          "CalibracionActivity (avance ms/cm)\n"
          "CalibrarGiroActivity (giro y cuerda)  ·  "
          "SensoresActivity (telemetría SENS, RUMBO y escala IMU en 3 pasos J16)  ·  BenchmarkActivity",
          AMBAR_CLARO, AMBAR, fs=7.5, bold=True)
    _flecha(ax, (87.5, 46.5), (87.5, 39.5), AMBAR, lw=1.5)

    _caja(ax, 2, 4, 96, 12,
          "Sesiones únicas compartidas (SesionCarro y SesionVideo): conservan vivo el WebSocket /ws y el flujo de cámara entre pantallas.",
          BOSQUE_CLARO, BOSQUE, fs=7.5, bold=True)
    _guardar(fig, ["pantallas", "fig_08_pantallas_app"])


def fig_desarrollo():
    fig, ax = _lienzo(11.2, 4.7)
    etapas = [
        (2, "Etapa 1\nControl WiFi\ny seguridad",
         "AP CarroFSI,\ncola 32 órdenes,\nwatchdog 1 s",
         AZUL_CLARO, AZUL),
        (22, "Etapa 2\nNúcleo C99\ny puente JNI",
         "Cuadrícula 20 cm,\ncalibración y\nsuite en PC",
         VERDE_CLARO, VERDE),
        (42, "Etapa 3\nCámara HQVGA\ny visión",
         "GC0308 a 12.4 fps,\npiso YCbCr, TFLite\ny corredor",
         AMBAR_CLARO, AMBAR),
        (62, "Etapa 4\nSensores I2C\ny pose en C",
         "Giro MPU6050,\nfreno láser 120 mm\ne integración",
         CORAL_CLARO, CORAL),
        (82, "Etapa 5\nCierre J16 y\nesquive B6",
         "Lazo PI RUMBO,\nescala IMU ppm y\n6 corridas B6",
         BOSQUE_CLARO, BOSQUE),
    ]
    for x, titulo, detalle, fc, ec in etapas:
        _caja(ax, x, 33, 16, 54, f"{titulo}\n\n{detalle}", fc, ec, fs=7.7, bold=True)
    for x1, x2 in [(18.5, 21.5), (38.5, 41.5), (58.5, 61.5), (78.5, 81.5)]:
        _flecha(ax, (x1, 60), (x2, 60), AZUL, lw=1.7)

    _caja(ax, 3, 5, 94, 19,
          "Resultado acumulado en v1.1: 22 programas / 1,655 casos en nucleo_c + 5 programas / 540 casos en pruebas_firmware\n"
          "+ 50,000 corridas Monte Carlo + validación física en el carro del esquive B6 (5/6 autónomas, 0/6 toques).",
          GRIS_CLARO, GRIS, fs=7.8, bold=True)
    _guardar(fig, ["desarrollo"])


def lamina_carro():
    fotos = [
        (IMAGENES / "foto_01_carro_frontal.jpg", "(a) Frente: cámara y láser"),
        (IMAGENES / "foto_02_carro_lateral.jpg", "(b) Lateral: chasis y ruedas"),
        (IMAGENES / "foto_03_carro_superior.jpg", "(c) Superior: placas y cables"),
        (IMAGENES / "foto_04_carro_perspectiva.jpg", "(d) Perspectiva del vehículo"),
    ]
    if not all(p.exists() for p, _ in fotos):
        raise FileNotFoundError("Faltan fotografías del carro en docs/imagenes")
    fig, axs = plt.subplots(2, 2, figsize=(9.2, 6.2), facecolor="white")
    for ax, (ruta, pie_txt) in zip(axs.flat, fotos):
        img = mpimg.imread(str(ruta))
        ax.imshow(img)
        ax.set_xticks([])
        ax.set_yticks([])
        for sp in ax.spines.values():
            sp.set_edgecolor(AZUL)
            sp.set_linewidth(1.0)
        ax.set_xlabel(pie_txt, fontsize=8.0, color=AZUL, fontweight="bold", labelpad=5)
    plt.subplots_adjust(wspace=0.22, hspace=0.24)
    DESTINO.mkdir(parents=True, exist_ok=True)
    buf = io.BytesIO()
    fig.savefig(buf, format="png", dpi=220, bbox_inches="tight", pad_inches=0.06, facecolor="white")
    (DESTINO / "lamina_carro.png").write_bytes(buf.getvalue())
    plt.close(fig)


def lamina_app():
    capturas = [
        (IMAGENES / "foto_10_app_conexion.png", "(a) Pestaña Inicio:\nConectado a CarroFSI"),
        (IMAGENES / "foto_12_app_editor_rutas.png", "(b) Pestaña Rutas:\nReconectando fuera de la red"),
        (IMAGENES / "foto_17_app_ajustes_red.png", "(c) Pantalla Ajustes:\nEstado sin conexión activa"),
    ]
    if not all(p.exists() for p, _ in capturas):
        raise FileNotFoundError("Faltan capturas de la aplicación en docs/imagenes")
    fig, axs = plt.subplots(1, 3, figsize=(8.8, 5.0), facecolor="white")
    for ax, (ruta, pie_txt) in zip(axs.flat, capturas):
        img = mpimg.imread(str(ruta))
        ax.imshow(img)
        ax.set_xticks([])
        ax.set_yticks([])
        for sp in ax.spines.values():
            sp.set_edgecolor(AZUL)
            sp.set_linewidth(1.0)
        ax.set_xlabel(pie_txt, fontsize=8.4, color=AZUL, fontweight="bold", labelpad=6)
    plt.subplots_adjust(wspace=0.16)
    DESTINO.mkdir(parents=True, exist_ok=True)
    buf = io.BytesIO()
    fig.savefig(buf, format="png", dpi=220, bbox_inches="tight", pad_inches=0.06, facecolor="white")
    (DESTINO / "lamina_app.png").write_bytes(buf.getvalue())
    plt.close(fig)


def main():
    fig_arquitectura()
    fig_alimentacion()
    fig_seguridad()
    fig_comandos()
    fig_modos_camara()
    fig_navegacion()
    fig_esquive()
    fig_percepcion()
    fig_pantallas()
    fig_desarrollo()
    lamina_carro()
    lamina_app()
    print(f"Figuras v1.1 generadas en {DESTINO}")


if __name__ == "__main__":
    main()
