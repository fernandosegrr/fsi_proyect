# -*- coding: utf-8 -*-
"""
verificar_sensores.py - Pruebas FISICAS del giroscopio MPU6050 y del sensor laser VL53L0X por el cable USB.

GUIA DE LECTURA
 1. La PC NO se conecta al WiFi del carro: todo va por el puerto serie (115200) con los mensajes de diagnostico del firmware
    (IMU?, TOF?, CAPS?, LOOP?, IMU_CAL, IMU_CERO, SENS_SERIE:1). Por serie NUNCA se mueven los motores.
 2. Abrir el puerto NO reinicia la placa (DTR y RTS quedan apagados); con --reiniciar si se reinicia para ver el arranque completo.
 3. Modos:
      estado   : reinicia (opcional), muestra el arranque, CAPS?, IMU?, TOF?, LOOP? y el escaneo del bus.
      mano     : "giro a mano": pone el angulo en cero y muestra en vivo angulo, sesgo y distancia. Gira el carro a mano 90 grados
                 a la IZQUIERDA y dice si marco +90 +-5 (si sale negativo hay que cambiar IMU_SIGNO en config.h).
      deriva   : carro quieto N segundos (60 por defecto): el angulo no debe moverse mas de 1 grado.
      vacio    : "piso vacio": el laser mirando al frente sin nada a 1.5 m, 5 s: debe salir libre (mas de 800 mm).
      distancia: muestra en vivo la distancia del laser (para la caja a 30 cm, etc.).
      calibrar : manda IMU_CAL (carro quieto) y espera el resultado.
 4. Entorno virtual del proyecto (con pyserial):
      pruebas_firmware\\.venv\\Scripts\\python.exe scripts\\verificar_sensores.py estado --puerto COM6
"""

import argparse
import re
import sys
import time

import serial

BAUDIOS = 115200
PATRON_SENS = re.compile(r"^SENS:(-?\d+),(-?\d+),(\d+)$")


def abrir(puerto, reiniciar):
    """Abre el puerto sin reiniciar la placa (a menos que se pida)."""
    p = serial.Serial()
    p.port = puerto
    p.baudrate = BAUDIOS
    p.timeout = 0.2
    p.dtr = False
    p.rts = False
    p.open()
    if reiniciar:
        p.rts = True      # pulso en EN: reinicia el ESP32
        time.sleep(0.15)
        p.rts = False
    return p


def leer_lineas(p, segundos):
    """Lee lineas durante `segundos` y las devuelve."""
    fin = time.time() + segundos
    salida = []
    while time.time() < fin:
        linea = p.readline().decode("utf-8", "replace").strip()
        if linea:
            salida.append(linea)
    return salida


def preguntar(p, texto, prefijo, espera=1.5):
    """Manda un mensaje y devuelve la primera linea que empieza con `prefijo` (o None)."""
    p.reset_input_buffer()
    p.write((texto + "\n").encode())
    fin = time.time() + espera
    while time.time() < fin:
        linea = p.readline().decode("utf-8", "replace").strip()
        if linea.startswith(prefijo):
            return linea
    return None


def modo_estado(p, args):
    if args.reiniciar:
        print("--- arranque (6 s) ---")
        for linea in leer_lineas(p, 6.0):
            print("  | " + linea)
    for texto, prefijo in (("CAPS?", "CAPS:"), ("IMU?", "IMU:"), ("TOF?", "TOF:"), ("LOOP?", "LOOP:"), ("ANT?", "ANT:")):
        print("%-6s -> %s" % (texto, preguntar(p, texto, prefijo)))
    imu = preguntar(p, "IMU?", "IMU:")
    if imu:
        ang, sesgo, estado, quien = [int(x) for x in imu[4:].split(",")]
        print("giroscopio: estado %d (2 = lista), WHO_AM_I=0x%02X, sesgo %.2f grados/s, angulo %.1f" % (estado, quien, sesgo / 100.0, ang / 10.0))


def flujo(p, segundos, mostrar):
    """Activa SENS por serie y devuelve la lista de (t, angulo_grados, mm, banderas)."""
    preguntar(p, "SENS_SERIE:1", "ACK_SENS_SERIE")
    datos = []
    t0 = time.time()
    while time.time() - t0 < segundos:
        linea = p.readline().decode("utf-8", "replace").strip()
        m = PATRON_SENS.match(linea)
        if m:
            reg = (time.time() - t0, int(m.group(1)) / 10.0, int(m.group(2)), int(m.group(3)))
            datos.append(reg)
            if mostrar:
                print("\r  t=%5.1f s  angulo=%+8.1f grados  distancia=%4d mm  banderas=%d   " % reg, end="", flush=True)
    if mostrar:
        print()
    preguntar(p, "SENS_SERIE:0", "ACK_SENS_SERIE")
    return datos


def modo_mano(p, args):
    print("IMU_CERO y giro a mano. Deja el carro quieto 2 s, luego giralo 90 grados a la IZQUIERDA (antihorario visto desde arriba) y dejalo quieto.")
    print(preguntar(p, "IMU_CERO", "ACK_IMU_CERO"))
    datos = flujo(p, args.segundos, True)
    if not datos:
        print("NO llego ningun SENS: el giroscopio no esta listo (revisa CAPS?).")
        return 1
    ang = datos[-1][1]
    pico = max(datos, key=lambda d: abs(d[1]))[1]
    print("angulo final: %+.1f   (el mayor fue %+.1f)" % (ang, pico))
    if 85 <= ang <= 95:
        print("OK: +90 +-5. La polaridad es correcta (izquierda = positivo).")
    elif -95 <= ang <= -85:
        print("AL REVES: salio negativo. Pon IMU_SIGNO -1 en firmware_carro/config.h y vuelve a cargar.")
    else:
        print("Fuera de +-5 de 90: repite el giro con calma o revisa que el eje Z quede hacia arriba.")
    return 0


def modo_deriva(p, args):
    print("Carro QUIETO %d s (no lo toques)..." % args.segundos)
    preguntar(p, "IMU_CERO", "ACK_IMU_CERO")
    datos = flujo(p, args.segundos, True)
    if not datos:
        print("NO llego ningun SENS.")
        return 1
    peor = max(abs(d[1]) for d in datos)
    print("mayor desviacion del angulo en %d s: %.1f grados -> %s" % (args.segundos, peor, "OK (menos de 1)" if peor < 1.0 else "MAL (1 o mas)"))
    return 0 if peor < 1.0 else 2


def modo_vacio(p, args):
    print("Piso vacio: laser al frente, nada a menos de 1.5 m. Leyendo %d s..." % args.segundos)
    datos = flujo(p, args.segundos, True)
    if not datos:
        print("NO llego ningun SENS.")
        return 1
    mm = [d[2] for d in datos]
    libres = sum(1 for d in datos if not (d[3] & 4))
    print("lecturas: %d, minimo %d mm, maximo %d mm, marcadas libres: %d" % (len(mm), min(mm), max(mm), libres))
    if min(mm) > 800:
        print("OK: libre (mas de 800 mm). El laser no ve el piso.")
        return 0
    if 150 <= min(mm) <= 350:
        print("El laser esta viendo el PISO (150-350 mm): subelo o inclinalo mas hacia arriba y repite.")
    else:
        print("Hay algo a menos de 800 mm delante del laser: revisa que este despejado.")
    return 2


def modo_distancia(p, args):
    datos = flujo(p, args.segundos, True)
    if datos:
        mm = [d[2] for d in datos if d[3] & 4]
        if mm:
            print("lecturas validas: %d, minimo %d, maximo %d, promedio %.0f mm" % (len(mm), min(mm), max(mm), sum(mm) / len(mm)))
    return 0


def modo_calibrar(p, args):

    p.reset_input_buffer()
    p.write(b"IMU_CAL\n")
    fin = time.time() + 12
    while time.time() < fin:
        linea = p.readline().decode("utf-8", "replace").strip()
        if linea:
            print("  | " + linea)
        if linea.startswith("IMU_CAL_OK") or linea.startswith("IMU_CAL_FALLO") or linea.startswith("NAK_IMU_CAL"):
            return 0 if linea.startswith("IMU_CAL_OK") else 2
    print("sin respuesta")
    return 1


MODOS = {"estado": modo_estado, "mano": modo_mano, "deriva": modo_deriva, "vacio": modo_vacio, "distancia": modo_distancia, "calibrar": modo_calibrar}


def main():
    ap = argparse.ArgumentParser(description="Pruebas fisicas del giroscopio y del laser por USB")
    ap.add_argument("modo", choices=sorted(MODOS))
    ap.add_argument("--puerto", default="COM6")
    ap.add_argument("--segundos", type=int, default=None, help="duracion (mano 25, deriva 60, vacio 5, distancia 15)")
    ap.add_argument("--reiniciar", action="store_true", help="reinicia la placa al abrir el puerto (para ver el arranque)")
    args = ap.parse_args()
    if args.segundos is None:
        args.segundos = {"mano": 25, "deriva": 60, "vacio": 5, "distancia": 15}.get(args.modo, 10)
    p = abrir(args.puerto, args.reiniciar)
    try:
        r = MODOS[args.modo](p, args)
    finally:
        p.close()
    sys.exit(r or 0)


if __name__ == "__main__":
    main()
