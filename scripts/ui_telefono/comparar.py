# -*- coding: utf-8 -*-
"""comparar.py <antes_medidas.json> <despues_medidas.json> - pone lado a lado las dos corridas de prueba_telefono.py medir."""
import json
import sys

sys.stdout.reconfigure(encoding="utf-8")


def cargar(ruta):
    with open(ruta, encoding="utf-8") as f:
        return {r["escena"]: r for r in json.load(f)}


def fila(nombre, a, b, fmt="%s", menor_es_mejor=True):
    def f(v):
        return "-" if v is None else (fmt % v)
    marca = ""
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        if b < a:
            marca = "  mejor" if menor_es_mejor else "  peor"
        elif b > a:
            marca = "  peor" if menor_es_mejor else "  mejor"
    print("    %-34s antes %-9s despues %-9s%s" % (nombre, f(a), f(b), marca))


def main():
    A, B = cargar(sys.argv[1]), cargar(sys.argv[2])
    for esc in ("manual_video_ok", "manual_video_falla", "ruta_en_marcha"):
        if esc not in A or esc not in B:
            print("\n[%s] falta en una de las corridas" % esc)
            continue
        a, b = A[esc], B[esc]
        if "gfxinfo" not in a or "gfxinfo" not in b:
            print("\n[%s] sin medidas (%s | %s)" % (esc, a.get("error"), b.get("error")))
            continue
        ga, gb = a["gfxinfo"], b["gfxinfo"]
        print("\n[%s]" % esc)
        fila("cuadros dibujados en la ventana", ga.get("cuadros_total"), gb.get("cuadros_total"), "%d", menor_es_mejor=False)
        fila("cuadros lentos (gfxinfo) %", ga.get("janky_pct"), gb.get("janky_pct"), "%.1f")
        pa = 100.0 * ga["hilo_ui_lento"] / ga["cuadros_total"] if ga.get("cuadros_total") else None
        pb = 100.0 * gb["hilo_ui_lento"] / gb["cuadros_total"] if gb.get("cuadros_total") else None
        fila("hilo principal lento %", pa, pb, "%.1f")
        fila("tiempo de cuadro p50 (ms)", ga.get("p50"), gb.get("p50"), "%d")
        fila("tiempo de cuadro p90 (ms)", ga.get("p90"), gb.get("p90"), "%d")
        fila("tiempo de cuadro p99 (ms)", ga.get("p99"), gb.get("p99"), "%d")
        ma, mb = a["muestra"], b["muestra"]
        fila("muestra: cuadros leidos", ma["muestreados"], mb["muestreados"], "%d", menor_es_mejor=False)
        fila("muestra: % con acomodar > 1 ms", ma["pct_acomodar_mas_de_1ms"], mb["pct_acomodar_mas_de_1ms"], "%.1f")
        fila("muestra: % con acomodar > 2 ms", ma["pct_acomodar_mas_de_2ms"], mb["pct_acomodar_mas_de_2ms"], "%.1f")
        fila("muestra: % con acomodar > 4 ms", ma["pct_acomodar_mas_de_4ms"], mb["pct_acomodar_mas_de_4ms"], "%.1f")
        fila("muestra: acomodar maximo (ms)", ma["acomodar_ms_max"], mb["acomodar_ms_max"], "%.1f")
    if "punto_conexion" in A and "punto_conexion" in B and "lum_media" in A["punto_conexion"] and "lum_media" in B["punto_conexion"]:
        a, b = A["punto_conexion"], B["punto_conexion"]
        print("\n[punto de conexion: brillo 0-255 en %s y %s capturas]" % (a["muestras"], b["muestras"]))
        fila("brillo minimo", a["lum_min"], b["lum_min"], "%.1f", menor_es_mejor=False)
        fila("brillo maximo", a["lum_max"], b["lum_max"], "%.1f", menor_es_mejor=False)
        fila("rango (max-min)", a["lum_max"] - a["lum_min"], b["lum_max"] - b["lum_min"], "%.1f", menor_es_mejor=False)
        fila("desviacion", a["lum_desv"], b["lum_desv"], "%.1f", menor_es_mejor=False)
    print("\n[geometria: botones de manejo con el video bien -> con el video fallando]")
    for nombre, R in (("antes", A), ("despues", B)):
        ok, mal = R.get("manual_video_ok", {}).get("vistas"), R.get("manual_video_falla", {}).get("vistas")
        if not ok or not mal:
            print("    %s: sin datos" % nombre)
            continue
        movidas = []
        for k in ("boton_adelante", "boton_izquierda", "boton_paro", "boton_derecha", "boton_atras", "manual_switch_video", "manual_switch_deteccion"):
            if ok.get(k) != mal.get(k):
                movidas.append(k)
        print("    %-8s botones/interruptores que cambian de lugar: %s" % (nombre, movidas if movidas else "NINGUNO"))
    for nombre, R in (("antes", A), ("despues", B)):
        lg = R.get("logcat")
        if lg:
            print("    logcat %-8s crash=%s fatales=%s" % (nombre, lg.get("crash_buffer_lineas"), lg.get("fatal_lineas")))


if __name__ == "__main__":
    main()
