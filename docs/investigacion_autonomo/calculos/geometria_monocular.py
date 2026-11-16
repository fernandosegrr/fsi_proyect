#!/usr/bin/env python3
"""Geometría monocular del carro FSI (área 05): relación fila-distancia, incertidumbre y tamaño de una pata de mueble.

TODO es CÁLCULO / SIMULADO con valores SUPUESTOS (altura 115 mm, inclinación 12 grados hacia abajo, campo horizontal 56 grados): ninguno está medido en el carro.
Modelo: cámara de agujero (sin distorsión), eje óptico inclinado `incl` grados hacia abajo, imagen de W x H píxeles, f = (W/2) / tan(FOV/2).
Un punto del piso a distancia horizontal d (mm, desde la vertical de la lente) cae en la fila v = H/2 + f * tan(atan(h/d) - incl).
Es la misma geometría de nucleo_c/corredor.c (A(v) = f sen(incl) + (v - cy) cos(incl)); en v = cy da d = h / tan(incl).

Uso:  python calculos/geometria_monocular.py
"""
import math

W, H = 240, 176          # HQVGA, modo por defecto de la cámara
H_MM, INCL, FOV = 115.0, 12.0, 56.0
LADO_CELDA = 10          # celdas de 10x10 del detector de color (obstaculos.h)
FILA_MIN_MIL = 440       # OBS_FILA_MINIMA_MIL


def foco_px(fov=FOV, ancho=W):
    return (ancho / 2.0) / math.tan(math.radians(fov / 2.0))


def fila_de_distancia(d, h=H_MM, incl=INCL, fov=FOV):
    """Fila (con decimales) del punto del piso a d mm."""
    return H / 2.0 + foco_px(fov) * math.tan(math.atan(h / d) - math.radians(incl))


def distancia_de_fila(v, h=H_MM, incl=INCL, fov=FOV):
    """Distancia (mm) del piso visto en la fila v; inf si está en el horizonte o arriba."""
    ang = math.atan((v - H / 2.0) / foco_px(fov)) + math.radians(incl)
    return h / math.tan(ang) if ang > 0 else float("inf")


def fila_de_punto(d, z, h=H_MM, incl=INCL, fov=FOV):
    """Fila de un punto a d mm de distancia horizontal y z mm de altura sobre el piso."""
    return H / 2.0 + foco_px(fov) * math.tan(math.atan((h - z) / d) - math.radians(incl))


def celdas_con_cobertura(d, ancho_mm, minimo=0.25, fila_min_px=None):
    """Celdas de 10x10 que cubre (al menos `minimo` de su área) una pata vertical de `ancho_mm` de ancho, entera (alta), en la zona de avance.
    Devuelve (min, max) sobre 20 desplazamientos horizontales. Solo cuenta las filas >= fila_min_px (el detector ignora las de arriba)."""
    if fila_min_px is None:
        fila_min_px = (H // LADO_CELDA) * FILA_MIN_MIL // 1000 * LADO_CELDA
    ancho_px = ancho_mm * foco_px() / d
    base = min(fila_de_distancia(d), H)
    cuentas = []
    for k in range(20):
        x0 = W / 2.0 - ancho_px / 2.0 + 0.5 * k
        x1 = x0 + ancho_px
        n = 0
        for f in range(fila_min_px // LADO_CELDA, H // LADO_CELDA):
            for c in range(W // LADO_CELDA):
                ox = max(0.0, min(c * 10 + 10, x1) - max(c * 10, x0))
                oy = max(0.0, min(f * 10 + 10, base) - max(f * 10, fila_min_px))
                if ox * oy / 100.0 >= minimo:
                    n += 1
        cuentas.append(n)
    return min(cuentas), max(cuentas)


if __name__ == "__main__":
    print(f"f = {foco_px():.1f} px; horizonte en la fila {H / 2 - foco_px() * math.tan(math.radians(INCL)):.1f}; "
          f"el eje óptico toca el piso a {H_MM / math.tan(math.radians(INCL)):.0f} mm")
    print("\nfila -> distancia (mm) y mm por fila de píxel")
    for v in (70, 77, 88, 100, 110, 120, 130, 140, 150, 160, 175):
        print(f"  fila {v:3d}: {distancia_de_fila(v):6.0f} mm   ({distancia_de_fila(v - .5) - distancia_de_fila(v + .5):5.1f} mm/px)")
    print("\ncelda (fila de celdas): borde de arriba y de abajo, en mm")
    for c in range(5, 17):
        print(f"  celda {c:2d}: {distancia_de_fila(c * 10):6.0f} -> {distancia_de_fila(c * 10 + 10):5.0f} mm")
    print("\nIncertidumbre de la distancia de una fila (nominal h=115, incl=12, FOV=56)")
    casos = (("h+10 mm", dict(h=125)), ("h-10 mm", dict(h=105)), ("incl+3", dict(incl=15)), ("incl-3", dict(incl=9)),
             ("FOV 48", dict(fov=48)), ("FOV 64", dict(fov=64)), ("peor cerca", dict(h=105, incl=15, fov=64)),
             ("peor lejos", dict(h=125, incl=9, fov=48)), ("h=110", dict(h=110)), ("h=120", dict(h=120)))
    for v in (110, 125, 140):
        print(f"  fila {v}: nominal {distancia_de_fila(v):4.0f} mm; " + "; ".join(f"{n} {distancia_de_fila(v, **kw):4.0f}" for n, kw in casos))
    print("\nPata de mueble: ancho en píxeles y celdas del detector (zona de avance desde la fila 70)")
    for w in (25, 30, 35):
        for d in (300, 500, 800):
            px = w * foco_px() / d
            a25 = celdas_con_cobertura(d, w, 0.25)
            a50 = celdas_con_cobertura(d, w, 0.50)
            print(f"  {w} mm a {d} mm: {px:5.1f} px ({px / 10:4.2f} celdas de ancho), base en la fila {fila_de_distancia(d):5.1f}; "
                  f"celdas con >=25 % cubierto: {a25[0]}-{a25[1]}; con >=50 %: {a50[0]}-{a50[1]}")
    print("\nAmpliación (looming) de una pata de 30 mm con el carro a 193 mm/s (AUTO) en 100 ms")
    for d in (800, 500, 300):
        w1 = 30 * foco_px() / d
        w2 = 30 * foco_px() / (d - 19.3)
        print(f"  d={d} mm: tau = d/v = {d / 193.0:4.2f} s; ancho {w1:5.2f} -> {w2:5.2f} px (+{w2 - w1:4.2f} px)")
    print("\nSi el modo HQVGA recorta el campo (ventana 480/640 = 75 % del ancho del sensor, ver 06_gc0308_camara.md): campo efectivo")
    for fov_completo in (56.0, 70.6):
        print(f"  campo del sensor completo {fov_completo:4.1f} grados -> HQVGA {2 * math.degrees(math.atan(0.75 * math.tan(math.radians(fov_completo / 2)))):4.1f} grados")
