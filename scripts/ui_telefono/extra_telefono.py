# -*- coding: utf-8 -*-
"""
extra_telefono.py <etiqueta> <carpeta> - dos revisiones mas con el carro y la camara FALSOS:
  1. Manual en HORIZONTAL con el video bien y con el video fallando: el video no debe encogerse ni la cruceta correrse.
  2. Carro apagado (la direccion no responde): el aviso global baja bajo la cabecera; capturas a 1, 3, 6, 9 y 12 s y lugar de los botones antes/despues.
"""
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prueba_telefono as p  # noqa: E402

WS_MUERTO = "ws://127.0.0.1:8098/ws"   # nadie escucha: es como tener el carro apagado
CLAVES = ["manual_video", "manual_aviso", "manual_switch_video", "manual_switch_deteccion", "manual_aviso_retraso", "boton_adelante",
          "boton_izquierda", "boton_paro", "boton_derecha", "boton_atras", "aviso_global", "contenedor", "punto_conexion"]


def fmt(v):
    return "-" if not v else "%s %d,%d-%d,%d" % (v["vis"], v["x0"], v["y0"], v["x1"], v["y1"])


def horizontal(carpeta, etq):
    r = {"escena": "horizontal_manual"}
    for nombre, video in (("ok", p.CAM_OK), ("falla", p.CAM_MAL)):
        p.lanzar("manual", video, orient="horizontal")
        time.sleep(7)
        if not p.app_al_frente():
            r["error"] = "la app no esta al frente: " + p.foco()
            return r
        r[nombre] = p.jerarquia()
        r["captura_" + nombre] = p.capturar(os.path.join(carpeta, "%s_horizontal_manual_video_%s.png" % (etq, nombre)))
    print("\n=== MANUAL HORIZONTAL: video bien (A) vs video fallando (B) ===")
    cambian = []
    for k in CLAVES:
        a, b = r["ok"].get(k), r["falla"].get(k)
        print("  %-24s A=%-26s B=%s" % (k, fmt(a), fmt(b)))
        if a != b and k not in ("manual_aviso",):
            cambian.append(k)
    print("  -> cambian de lugar (sin contar el aviso mismo):", cambian if cambian else "NINGUNA")
    r["cambian"] = cambian
    return r


def aviso_global(carpeta, etq):
    r = {"escena": "aviso_global", "capturas": []}
    p.lanzar("manual", p.CAM_OK, ws=WS_MUERTO)
    t0 = time.time()
    r["antes"] = None
    for t in (1, 3, 6, 9, 12):
        while time.time() - t0 < t:
            time.sleep(0.1)
        if not p.app_al_frente():
            r["error"] = "la app no esta al frente: " + p.foco()
            return r
        ok = p.capturar(os.path.join(carpeta, "%s_aviso_global_%02ds.png" % (etq, t)))
        r["capturas"].append((t, ok))
        if t == 1:
            r["antes"] = p.jerarquia()
    r["despues"] = p.jerarquia()
    print("\n=== AVISO GLOBAL (carro apagado): lugar de las cosas a ~2 s (A) y a ~13 s (B) ===")
    for k in CLAVES:
        a, b = r["antes"].get(k), r["despues"].get(k)
        print("  %-24s A=%-26s B=%s%s" % (k, fmt(a), fmt(b), "" if a == b else "   <- cambia"))
    return r


def main():
    etq, carpeta = sys.argv[1], sys.argv[2]
    os.makedirs(carpeta, exist_ok=True)
    p.shell("input keyevent KEYCODE_WAKEUP")
    res = [horizontal(carpeta, etq), aviso_global(carpeta, etq)]
    crash = p.adb("logcat", "-d", "-b", "crash")
    res.append({"escena": "logcat", "crash_buffer_lineas": len([l for l in crash.split("\n") if p.PKG in l])})
    with open(os.path.join(carpeta, etq + "_extra.json"), "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=1)
    print("\nlogcat crash:", res[-1]["crash_buffer_lineas"])


if __name__ == "__main__":
    main()
