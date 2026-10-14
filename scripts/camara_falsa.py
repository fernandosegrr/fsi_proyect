# -*- coding: utf-8 -*-
"""
camara_falsa.py — Una "cámara del carro" falsa para probar la app SIN el carro (solo números: las imágenes son sintéticas, nunca fotos de verdad).

GUÍA DE LECTURA
 1. Sirve por HTTP lo mismo que camara_prueba.ino: /stream (MJPEG), /raw (cuadros RGB565 con X-Ancho, X-Alto, X-Numero y X-Ms-Captura-Envio), /control?modo=jpeg|raw|celdas
    y /control?size=..., y /stats (JSON). El modo celdas (UDP, puerto 4210) lo atiende scripts/camara_falsa_udp.c (se compila para el teléfono con el NDK) porque el teléfono
    y la PC no comparten una red UDP por el cable USB: ese programa corre en el propio teléfono, y este de aquí por `adb reverse tcp:8085 tcp:8085`.
 2. La escena: un piso gris cálido con un poco de ruido y, a partir del segundo 5 de cada ciclo de 12 s, una "caja" oscura que crece desde el horizonte hacia abajo (como un
    obstáculo al que se acerca el carro). Todo calculado con numpy; el JPEG lo hace Pillow.
 3. Opciones para probar los fallos: --sin-modo raw|celdas hace que /control?modo=... conteste 400 (la app debe volver a jpeg sola); --raw-mudo hace que /raw acepte la conexión
    pero no mande nada (la app debe volver a jpeg a los 3 s).
 4. Uso:   python scripts/camara_falsa.py --puerto 8085 [--fps 10] [--sin-modo celdas] [--raw-mudo]
    En el teléfono: `adb reverse tcp:8085 tcp:8085` y en la app la URL del video del carro http://127.0.0.1:8085/stream.
Requiere numpy y Pillow (ya están en el Python de esta PC; no se instala nada).
"""
import argparse
import io
import json
import struct
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

import numpy as np
from PIL import Image

ANCHO, ALTO = 240, 176
ESTADO = {"modo": "jpeg", "tam": "hqvga", "enviados": 0, "inicio": time.time(), "peticiones": 0}
OPCIONES = None


def escena(t):
    """Un cuadro sintético (RGB uint8, ALTO x ANCHO x 3) para el instante t (segundos)."""
    rng = np.random.default_rng(int(t * 1000) % 100000)
    base = np.zeros((ALTO, ANCHO, 3), dtype=np.int16)
    base[:, :, 0] = 150
    base[:, :, 1] = 132
    base[:, :, 2] = 112
    # un degradado suave: más claro hacia abajo
    base += (np.linspace(-10, 12, ALTO, dtype=np.int16)[:, None, None])
    base += rng.integers(-3, 4, size=(ALTO, ANCHO, 1), dtype=np.int16)
    fase = (t % 12.0)
    if fase > 5.0:
        k = (fase - 5.0) / 7.0                   # 0 a 1: la caja crece al acercarse
        alto = int(10 + 90 * k)
        ancho = int(30 + 110 * k)
        y1 = int(80 + 85 * k)
        y0 = max(0, y1 - alto)
        x0 = ANCHO // 2 - ancho // 2
        base[y0:y1, max(0, x0):min(ANCHO, x0 + ancho), :] = np.array([40, 60, 140], dtype=np.int16)
    return np.clip(base, 0, 255).astype(np.uint8)


def a_jpeg(rgb):
    b = io.BytesIO()
    Image.fromarray(rgb).save(b, format="JPEG", quality=50)
    return b.getvalue()


def a_rgb565(rgb):
    r = (rgb[:, :, 0].astype(np.uint16) >> 3) << 11
    g = (rgb[:, :, 1].astype(np.uint16) >> 2) << 5
    b = rgb[:, :, 2].astype(np.uint16) >> 3
    v = r | g | b
    alto = (v >> 8).astype(np.uint8)
    bajo = (v & 0xFF).astype(np.uint8)
    return np.stack([alto, bajo], axis=2).tobytes()   # primero el byte alto de cada píxel, como la cámara


class Manejador(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, formato, *args):
        sys.stderr.write("[camara_falsa] %s\n" % (formato % args))

    def _texto(self, codigo, texto, tipo="text/plain"):
        cuerpo = texto.encode("utf-8")
        self.send_response(codigo)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(cuerpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(cuerpo)

    def do_GET(self):
        url = urlparse(self.path)
        consulta = parse_qs(url.query)
        ESTADO["peticiones"] += 1
        if url.path == "/control":
            if "modo" in consulta:
                modo = consulta["modo"][0]
                if modo not in ("jpeg", "raw", "celdas") or (OPCIONES.sin_modo == modo):
                    self._texto(400, "modo no disponible")
                    return
                ESTADO["modo"] = modo
                self._texto(200, modo)
                return
            if "size" in consulta:
                ESTADO["tam"] = consulta["size"][0]
                self._texto(200, ESTADO["tam"])
                return
            self._texto(400, "uso: /control?modo=jpeg|raw|celdas")
        elif url.path == "/stats":
            seg = time.time() - ESTADO["inicio"]
            datos = {"fps_captura": OPCIONES.fps, "fps_envio": OPCIONES.fps, "edad_ms": 90, "edad_max_ms": 140, "reconexiones": 0, "rssi": -50, "rssi_min_10s": -55,
                     "modo": ESTADO["modo"], "uptime_s": int(seg)}
            self._texto(200, json.dumps(datos), "application/json")
        elif url.path in ("/stream", "/raw"):
            self._stream(url.path == "/raw")
        else:
            self._texto(404, "no existe")

    def _stream(self, crudo):
        if crudo and OPCIONES.raw_mudo:
            self.send_response(200)
            self.send_header("Content-Type", "multipart/x-mixed-replace;boundary=cuadro")
            self.send_header("Connection", "close")
            self.end_headers()
            try:
                time.sleep(30)   # acepta la conexión y no manda nada
            except Exception:
                pass
            return
        self.send_response(200)
        self.send_header("Content-Type", "multipart/x-mixed-replace;boundary=cuadro")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "close")
        self.end_headers()
        numero = 0
        periodo = 1.0 / OPCIONES.fps
        try:
            while True:
                t0 = time.time()
                rgb = escena(t0)
                numero += 1
                if crudo:
                    datos = a_rgb565(rgb)
                    ms = int((time.time() - t0) * 1000)
                    cab = ("--cuadro\r\nContent-Type: application/x-rgb565\r\nContent-Length: %d\r\nX-Ancho: %d\r\nX-Alto: %d\r\nX-Numero: %d\r\nX-Ms-Captura-Envio: %d\r\n\r\n"
                           % (len(datos), ANCHO, ALTO, numero, ms)).encode()
                else:
                    datos = a_jpeg(rgb)
                    cab = ("--cuadro\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n" % len(datos)).encode()
                self.wfile.write(cab + datos + b"\r\n")
                self.wfile.flush()
                ESTADO["enviados"] += 1
                resto = periodo - (time.time() - t0)
                if resto > 0:
                    time.sleep(resto)
        except (BrokenPipeError, ConnectionResetError, OSError):
            return


def main():
    global OPCIONES
    p = argparse.ArgumentParser(description="Cámara del carro falsa (imágenes sintéticas)")
    p.add_argument("--puerto", type=int, default=8085)
    p.add_argument("--fps", type=float, default=10.0)
    p.add_argument("--sin-modo", choices=["raw", "celdas"], default="", help="el modo indicado contesta 400")
    p.add_argument("--raw-mudo", action="store_true", help="/raw acepta la conexión pero no manda nada")
    OPCIONES = p.parse_args()
    servidor = ThreadingHTTPServer(("127.0.0.1", OPCIONES.puerto), Manejador)
    servidor.daemon_threads = True
    print("camara_falsa escuchando en 127.0.0.1:%d (modo inicial jpeg, %.0f fps, escena sintética)" % (OPCIONES.puerto, OPCIONES.fps), flush=True)
    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
