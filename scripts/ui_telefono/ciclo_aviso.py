# -*- coding: utf-8 -*-
"""
ciclo_aviso.py - ¿el aviso «La cámara no responde» de Manual aparece y desaparece solo con cada reintento (cada 3 s)?
Muestrea la jerarquia de vistas (visibilidad y alto del aviso, y lugar de la cruceta) durante ~20 s en dos casos:
  - "rechazada": nadie escucha en el puerto de la camara (el rechazo es inmediato)
  - "apagada": la IP no responde (la conexion se queda esperando, como con la placa de la camara apagada)
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prueba_telefono as p  # noqa: E402

CASOS = [("rechazada", "http://127.0.0.1:8099/stream"), ("apagada", "http://10.255.255.1:81/stream")]


def main():
    etq = sys.argv[1] if len(sys.argv) > 1 else "x"
    p.shell("input keyevent KEYCODE_WAKEUP")
    for nombre, url in CASOS:
        p.lanzar("manual", url)
        t0 = time.time()
        print("\n=== camara %s (%s) ===" % (nombre, url), flush=True)
        linea = []
        while time.time() - t0 < 22:
            if not p.app_al_frente():
                print("  la app no esta al frente:", p.foco())
                break
            h = p.jerarquia()
            a, adel = h.get("manual_aviso"), h.get("boton_adelante")
            t = time.time() - t0
            linea.append("%4.1fs aviso=%s alto=%4d  adelante_y=%s" % (
                t, a["vis"] if a else "-", (a["y1"] - a["y0"]) if a else -1, adel["y0"] if adel else "-"))
            time.sleep(0.2)
        print("\n".join(linea), flush=True)
        vis = [l.split("aviso=")[1][0] for l in linea]
        cambios = sum(1 for i in range(1, len(vis)) if vis[i] != vis[i - 1])
        print("  -> el aviso cambio de visible a oculto (o al reves) %d veces en %d muestras" % (cambios, len(vis)), flush=True)


if __name__ == "__main__":
    main()
