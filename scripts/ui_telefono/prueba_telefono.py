# -*- coding: utf-8 -*-
"""
prueba_telefono.py - mide y fotografia la interfaz de la app Android en el telefono, con el carro y la camara FALSOS.

El carro NO se mueve: la app habla con el servidor falso del PC (adb reverse tcp:8080) y el video es la camara falsa
(adb reverse tcp:8085, imagenes sinteticas). Las capturas solo se hacen con la app al frente.

  python prueba_telefono.py medir   <etiqueta> <carpeta> [--segundos 20] [--sin-ruta]
  python prueba_telefono.py galeria <etiqueta> <carpeta>

Que mide (medir):
  1. Cuadros dibujados por la app en una ventana de N segundos (dumpsys gfxinfo framestats, que guarda los ultimos 120 cuadros:
     se lee cada ~0.8 s y se juntan sin repetir). Por cada cuadro: cuanto tardo la parte de medir+acomodar (PerformTraversalsStart -> DrawStart)
     y cuanto tardo el cuadro entero (IntendedVsync -> FrameCompleted). Un cuadro con "acomodar" largo es un cuadro en el que alguien pidio
     un requestLayout (cambio de texto, de visibilidad, de alto...).
  2. La posicion en pantalla de los botones de manejo con el video bien y con el video fallando (jerarquia de vistas, dumpsys activity top).
  3. El brillo del punto de conexion de la cabecera en N capturas (solo se guarda el numero, no la imagen).
"""
import argparse
import io
import json
import os
import re
import statistics
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding="utf-8")

ADB = os.path.join(os.environ["LOCALAPPDATA"], "Android", "Sdk", "platform-tools", "adb.exe")
SER = os.environ.get("ANDROID_SERIAL", "")   # vacio = el unico telefono conectado
PKG = "com.fsi.carro"
MAIN = PKG + "/.MainActivity"
WS = "ws://127.0.0.1:8080/ws"
CAM_OK = "http://127.0.0.1:8085/stream"
CAM_MAL = "http://127.0.0.1:8099/stream"  # nadie escucha ahi: el video falla al instante

IDS_MANEJO = ["boton_adelante", "boton_izquierda", "boton_paro", "boton_derecha", "boton_atras",
              "manual_video", "manual_switch_video", "manual_switch_deteccion", "manual_aviso_retraso", "manual_aviso",
              "punto_conexion"]

PANTALLAS = [("inicio", ()), ("manual", ()), ("rutas", ()), ("ejecutar", ("--es", "ruta_predefinida", "l")), ("mas", ()),
             ("asistente", ()), ("prueba", ()), ("ajustes", ()), ("ayuda", ()), ("acerca", ())]


# ---------------------------------------------------------------- adb
def adb(*args, timeout=90, binario=False):
    try:
        r = subprocess.run([ADB] + (["-s", SER] if SER else []) + list(args), capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return b"" if binario else ""
    return r.stdout if binario else r.stdout.decode("utf-8", "replace").replace("\r", "")


def shell(cmd, timeout=90):
    return adb("shell", cmd, timeout=timeout)


def foco():
    for linea in shell("dumpsys window | grep mCurrentFocus").split("\n"):
        if "mCurrentFocus" in linea:
            return linea.strip()
    return ""


def app_al_frente():
    return PKG in foco()


def bateria():
    s = shell("dumpsys battery")
    n = re.search(r"level: (\d+)", s)
    return int(n.group(1)) if n else -1


def lanzar(pantalla, video=CAM_OK, orient="vertical", tema="oscuro", extras=(), ws=WS):
    shell("am force-stop " + PKG)
    time.sleep(1.0)
    cmd = ["shell", "am", "start", "-W", "-n", MAIN, "--es", "pantalla", pantalla, "--es", "ws_url", ws,
           "--es", "tema", tema, "--ez", "asistente_visto", "true", "--ez", "video_en_manual", "true",
           "--es", "orientacion", orient]
    if video:
        cmd += ["--es", "video_url", video]
    cmd += list(extras)
    salida = adb(*cmd)
    t0 = time.time()
    while time.time() - t0 < 15 and not app_al_frente():
        time.sleep(0.4)
    return ("Status: ok" in salida) and app_al_frente()


def capturar(ruta):
    """Captura la pantalla SOLO si la app esta al frente. Devuelve True si guardo el archivo."""
    if not app_al_frente():
        return False
    datos = adb("exec-out", "screencap", "-p", binario=True)
    if not datos.startswith(b"\x89PNG"):
        return False
    with open(ruta, "wb") as f:
        f.write(datos)
    return True


# ---------------------------------------------------------------- jerarquia de vistas
PATRON_VISTA = re.compile(r"\{([0-9a-f]+) (\S+) (\S+) (-?\d+),(-?\d+)-(-?\d+),(-?\d+)(?: #[0-9a-f]+ (?:app|android):id/(\w+))?")


def jerarquia():
    """Igual que _jerarquia(), pero reintenta (si la app esta muy ocupada el volcado puede venir vacio)."""
    for _ in range(4):
        h = _jerarquia()
        if h:
            return h
        time.sleep(1.5)
    return {}


def _jerarquia():
    """{id: {vis, x0, y0, x1, y1}} de la actividad de la app (posiciones absolutas en la ventana; sin contar scroll)."""
    lineas = adb("shell", "dumpsys activity top", timeout=60).split("\n")
    ini = None
    for i, l in enumerate(lineas):
        if re.search(r"DecorView@\w+\[\w*Activity\]", l):
            ini = i
            break
    if ini is None:
        return {}
    base = len(lineas[ini]) - len(lineas[ini].lstrip(" "))
    pila = [(base, 0, 0)]
    salida = {}
    for l in lineas[ini + 1:]:
        if not l.strip():
            continue
        sangria = len(l) - len(l.lstrip(" "))
        if sangria <= base:
            break
        m = PATRON_VISTA.search(l)
        if not m:
            continue
        while pila and pila[-1][0] >= sangria:
            pila.pop()
        px, py = pila[-1][1], pila[-1][2]
        x0, y0 = px + int(m.group(4)), py + int(m.group(5))
        x1, y1 = px + int(m.group(6)), py + int(m.group(7))
        pila.append((sangria, x0, y0))
        nombre = m.group(8)
        if nombre:
            salida[nombre] = {"vis": m.group(2)[0], "x0": x0, "y0": y0, "x1": x1, "y1": y1}
    return salida


def diferencias(a, b):
    """Vistas con id que cambiaron de lugar o de visibilidad entre dos jerarquias."""
    dif = []
    for k in sorted(set(a) | set(b)):
        va, vb = a.get(k), b.get(k)
        if va is None or vb is None:
            dif.append((k, "solo en " + ("A" if vb is None else "B")))
        elif va != vb:
            dif.append((k, "A=%s %d,%d-%d,%d  B=%s %d,%d-%d,%d" % (va["vis"], va["x0"], va["y0"], va["x1"], va["y1"],
                                                                  vb["vis"], vb["x0"], vb["y0"], vb["x1"], vb["y1"])))
    return dif


# ---------------------------------------------------------------- cuadros
def tomar_cuadros(vistos):
    """Lee los ultimos ~120 cuadros y agrega los nuevos a 'vistos' {IntendedVsync: (acomodar_ms, total_ms)}."""
    s = shell("dumpsys gfxinfo " + PKG + " framestats")
    for bloque in re.findall(r"---PROFILEDATA---\n(.*?)---PROFILEDATA---", s, re.S):
        filas = [l for l in bloque.strip().split("\n") if l.strip()]
        if len(filas) < 2:
            continue
        cab = filas[0].rstrip(",").split(",")
        ix = {n: i for i, n in enumerate(cab)}
        for f in filas[1:]:
            v = f.rstrip(",").split(",")
            if len(v) < len(cab):
                continue
            try:
                intendido = int(v[ix["IntendedVsync"]])
                trav = int(v[ix["PerformTraversalsStart"]])
                dibujo = int(v[ix["DrawStart"]])
                fin = int(v[ix["FrameCompleted"]])
            except (ValueError, KeyError):
                continue
            if trav <= 0 or dibujo < trav or fin < intendido:
                continue
            vistos[intendido] = ((dibujo - trav) / 1e6, (fin - intendido) / 1e6)


def pct(v, p):
    return round(v[min(len(v) - 1, int(len(v) * p))], 2) if v else None


def resumen_cuadros(cuadros):
    """Resumen de una MUESTRA de cuadros (cada lectura de framestats trae solo los 10 ultimos cuadros)."""
    n = len(cuadros)
    lay = sorted(c[0] for c in cuadros)
    tot = sorted(c[1] for c in cuadros)
    def porc(k):
        return round(100.0 * k / n, 1) if n else None
    return {
        "muestreados": n,
        "pct_acomodar_mas_de_1ms": porc(sum(1 for x in lay if x > 1.0)),
        "pct_acomodar_mas_de_2ms": porc(sum(1 for x in lay if x > 2.0)),
        "pct_acomodar_mas_de_4ms": porc(sum(1 for x in lay if x > 4.0)),
        "acomodar_ms_p50": pct(lay, .50), "acomodar_ms_p95": pct(lay, .95), "acomodar_ms_max": round(lay[-1], 2) if lay else None,
        "total_ms_p50": pct(tot, .50), "total_ms_p90": pct(tot, .90), "total_ms_p99": pct(tot, .99),
        "total_ms_max": round(tot[-1], 2) if tot else None,
        "pct_cuadros_mas_de_16ms": porc(sum(1 for x in tot if x > 16.7)),
    }


def gfx_leer():
    s = shell("dumpsys gfxinfo " + PKG)
    d = {}
    for clave, patron in [("cuadros_total", r"Total frames rendered: (\d+)"), ("janky", r"Janky frames: (\d+)"),
                          ("janky_pct", r"Janky frames: \d+ \(([\d.]+)%\)"), ("p50", r"50th percentile: (\d+)ms"),
                          ("p90", r"90th percentile: (\d+)ms"), ("p95", r"95th percentile: (\d+)ms"),
                          ("p99", r"99th percentile: (\d+)ms"), ("vsync_perdidos", r"Number Missed Vsync: (\d+)"),
                          ("hilo_ui_lento", r"Number Slow UI thread: (\d+)"), ("bitmap_lento", r"Number Slow bitmap uploads: (\d+)"),
                          ("dibujo_lento", r"Number Slow issue draw commands: (\d+)")]:
        m = re.search(patron, s)
        d[clave] = float(m.group(1)) if m else None
    return d


def ventana_de_medida(seg, seg_muestreo=10):
    """A: 'seg' segundos SIN tocar nada y se lee el resumen de gfxinfo (todos los cuadros). B: 'seg_muestreo' segundos leyendo framestats."""
    shell("dumpsys gfxinfo " + PKG + " reset")
    time.sleep(seg)
    r = {"gfxinfo": gfx_leer(), "ventana_s": seg}
    vistos = {}
    fin = time.time() + seg_muestreo
    while time.time() < fin:
        tomar_cuadros(vistos)
        time.sleep(0.03)
    r["muestra"] = resumen_cuadros(list(vistos.values()))
    return r


# ---------------------------------------------------------------- escenas de medida
def escena(nombre, carpeta, etq, pantalla, video, seg, extras=(), espera=6.0, jer=True):
    r = {"escena": nombre, "bateria_pct": bateria()}
    r["lanzo"] = lanzar(pantalla, video, extras=extras)
    time.sleep(espera)
    r["foco_ok"] = app_al_frente()
    if not r["foco_ok"]:
        r["error"] = "la app no esta al frente: " + foco()
        return r
    r.update(ventana_de_medida(seg))
    if jer:
        r["vistas"] = jerarquia()
    r["captura"] = capturar(os.path.join(carpeta, "%s_%s.png" % (etq, nombre)))
    return r


def tocar_vista(clave, jer):
    """Toca el centro de una vista visible (por su id). Solo se usa con el carro FALSO."""
    v = jer.get(clave)
    if not v or v["vis"] != "V":
        return False
    shell("input tap %d %d" % ((v["x0"] + v["x1"]) // 2, (v["y0"] + v["y1"]) // 2))
    return True


def escena_ruta(carpeta, etq, seg):
    """Ruta en L contra el carro FALSO: abre Ejecutar ruta, toca 'Confirmar y salir' (cuenta 3, 2, 1) y mide mientras corre."""
    r = {"escena": "ruta_en_marcha", "bateria_pct": bateria()}
    r["lanzo"] = lanzar("ejecutar", CAM_OK, extras=("--es", "ruta_predefinida", "l", "--ez", "auto_iniciar", "true"))
    time.sleep(8)
    if not app_al_frente():
        r["error"] = "la app no esta al frente: " + foco()
        return r
    jer = jerarquia()
    shell("dumpsys gfxinfo " + PKG + " reset")
    r["toco_confirmar"] = tocar_vista("ej_iniciar", jer)
    time.sleep(11)          # cuenta 3, 2, 1 y la ruta corre (contra el carro falso dura unos 7 s)
    r["foco_ok"] = app_al_frente()
    if not r["foco_ok"]:
        r["error"] = "la app no esta al frente despues de confirmar: " + foco()
        return r
    r["gfxinfo"] = gfx_leer()
    r["ventana_s"] = 11
    r["muestra"] = resumen_cuadros([])
    r["captura"] = capturar(os.path.join(carpeta, "%s_ruta_en_marcha.png" % etq))
    return r


def luminancia(png, cx, cy, rad=5):
    from PIL import Image
    im = Image.open(io.BytesIO(png)).convert("RGB")
    caja = im.crop((cx - rad, cy - rad, cx + rad, cy + rad))
    px = list(caja.getdata())
    n = len(px)
    r = sum(p[0] for p in px) / n
    g = sum(p[1] for p in px) / n
    b = sum(p[2] for p in px) / n
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def escena_punto(n=30):
    r = {"escena": "punto_conexion", "bateria_pct": bateria()}
    r["lanzo"] = lanzar("inicio", CAM_OK)
    time.sleep(5)
    h = jerarquia()
    p = h.get("punto_conexion")
    if not p or not app_al_frente():
        r["error"] = "no encontre el punto o la app no esta al frente"
        return r
    cx, cy = (p["x0"] + p["x1"]) // 2, (p["y0"] + p["y1"]) // 2
    r["centro"] = [cx, cy]
    muestras = []
    t0 = time.time()
    for _ in range(n):
        png = adb("exec-out", "screencap", "-p", binario=True)
        if png.startswith(b"\x89PNG"):
            muestras.append((round(time.time() - t0, 2), round(luminancia(png, cx, cy), 1)))
    r["al_frente_al_final"] = app_al_frente()
    lum = [m[1] for m in muestras]
    if lum:
        r.update({"muestras": len(lum), "lum_min": min(lum), "lum_max": max(lum), "lum_media": round(statistics.mean(lum), 1),
                  "lum_desv": round(statistics.pstdev(lum), 1), "lum_serie": lum})
    return r


def medir(etq, carpeta, seg, con_ruta):
    resultados = []
    shell("input keyevent KEYCODE_WAKEUP")
    adb("logcat", "-c")
    resultados.append(escena("manual_video_ok", carpeta, etq, "manual", CAM_OK, seg))
    resultados.append(escena("manual_video_falla", carpeta, etq, "manual", CAM_MAL, seg))
    if con_ruta:
        resultados.append(escena_ruta(carpeta, etq, seg))
    resultados.append(escena_punto())
    # revision de fallos de la app durante toda la medida
    crash = adb("logcat", "-d", "-b", "crash")
    fatales = adb("logcat", "-d", "AndroidRuntime:E", "*:S")
    resultados.append({"escena": "logcat", "crash_buffer_lineas": len([l for l in crash.split("\n") if PKG in l]),
                       "fatal_lineas": len([l for l in fatales.split("\n") if "FATAL" in l and PKG in l or PKG in l and "Process:" in l])})
    return resultados


def imprimir_medidas(res):
    print("\n=== RESUMEN (A: gfxinfo de %s s sin tocar nada | B: muestra de cuadros con framestats) ===" % next((r.get("ventana_s") for r in res if "gfxinfo" in r), "?"))
    print("%-20s | %7s %7s %6s %5s %5s %5s %7s | %6s %9s %9s %9s %9s" % ("escena", "cuadros", "janky%", "UIlent", "p50", "p90", "p99", "slowdr", "mues.", "acom>1ms%", "acom>2ms%", "acom>4ms%", "acom_max"))
    for r in res:
        if "gfxinfo" not in r:
            continue
        g, m = r["gfxinfo"], r["muestra"]
        ui = g.get("hilo_ui_lento")
        tot = g.get("cuadros_total")
        print("%-20s | %7s %7s %6s %5s %5s %5s %7s | %6s %9s %9s %9s %9s" % (
            r["escena"], int(tot) if tot is not None else None, g.get("janky_pct"),
            ("%d" % ui) if ui is not None else None, g.get("p50"), g.get("p90"), g.get("p99"), g.get("dibujo_lento"),
            m["muestreados"], m["pct_acomodar_mas_de_1ms"], m["pct_acomodar_mas_de_2ms"], m["pct_acomodar_mas_de_4ms"], m["acomodar_ms_max"]))
    a = next((r for r in res if r["escena"] == "manual_video_ok"), None)
    b = next((r for r in res if r["escena"] == "manual_video_falla"), None)
    if a and b and a.get("vistas") and b.get("vistas"):
        print("\n=== MANEJO: video bien (A) vs video fallando (B) ===")
        for k in IDS_MANEJO:
            va, vb = a["vistas"].get(k), b["vistas"].get(k)
            print("  %-24s A=%s  B=%s" % (k, ("%s %d,%d-%d,%d" % (va["vis"], va["x0"], va["y0"], va["x1"], va["y1"])) if va else "-",
                                          ("%s %d,%d-%d,%d" % (vb["vis"], vb["x0"], vb["y0"], vb["x1"], vb["y1"])) if vb else "-"))
        dif = [d for d in diferencias(a["vistas"], b["vistas"]) if d[0] in IDS_MANEJO]
        print("  -> vistas de manejo que cambian entre A y B:", dif if dif else "NINGUNA (los botones no se mueven)")
    p = next((r for r in res if r["escena"] == "punto_conexion"), None)
    if p and "lum_media" in p:
        print("\n=== PUNTO DE CONEXION (brillo 0-255 en %d capturas) ===" % p["muestras"])
        print("  min %.1f  max %.1f  media %.1f  desv %.1f" % (p["lum_min"], p["lum_max"], p["lum_media"], p["lum_desv"]))
        print("  serie:", p["lum_serie"])
    for r in res:
        if "error" in r:
            print("\n!! %s: %s" % (r["escena"], r["error"]))
        if r["escena"] == "logcat":
            print("\n=== LOGCAT === crash_buffer=%s fatales=%s" % (r["crash_buffer_lineas"], r["fatal_lineas"]))


# ---------------------------------------------------------------- galeria
def galeria(etq, carpeta):
    res = []
    shell("input keyevent KEYCODE_WAKEUP")
    for orient in ("vertical", "horizontal"):
        for nombre, extras in PANTALLAS:
            ok = lanzar(nombre, CAM_OK, orient=orient, extras=extras)
            time.sleep(4.5)
            ruta = os.path.join(carpeta, "%s_%s_%s.png" % (etq, orient, nombre))
            cap = capturar(ruta)
            res.append({"pantalla": nombre, "orientacion": orient, "lanzo": ok, "captura": cap, "foco": "" if cap else foco()})
            print("  %-10s %-10s lanzo=%s captura=%s" % (nombre, orient, ok, cap), flush=True)
    for nombre, extras in [p for p in PANTALLAS if p[0] in ("inicio", "manual", "ejecutar", "mas")]:
        ok = lanzar(nombre, CAM_OK, orient="vertical", tema="claro", extras=extras)
        time.sleep(4.5)
        ruta = os.path.join(carpeta, "%s_claro_%s.png" % (etq, nombre))
        cap = capturar(ruta)
        res.append({"pantalla": nombre, "orientacion": "vertical", "tema": "claro", "lanzo": ok, "captura": cap, "foco": "" if cap else foco()})
        print("  %-10s %-10s lanzo=%s captura=%s" % (nombre, "claro", ok, cap), flush=True)
    lanzar("inicio", CAM_OK, orient="auto", tema="oscuro")      # deja el tema como estaba
    crash = adb("logcat", "-d", "-b", "crash")
    res.append({"escena": "logcat", "crash_buffer_lineas": len([l for l in crash.split("\n") if PKG in l])})
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("modo", choices=["medir", "galeria"])
    ap.add_argument("etiqueta")
    ap.add_argument("carpeta")
    ap.add_argument("--segundos", type=int, default=20)
    ap.add_argument("--sin-ruta", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.carpeta, exist_ok=True)
    if a.modo == "medir":
        res = medir(a.etiqueta, a.carpeta, a.segundos, not a.sin_ruta)
        with open(os.path.join(a.carpeta, a.etiqueta + "_medidas.json"), "w", encoding="utf-8") as f:
            json.dump(res, f, ensure_ascii=False, indent=1)
        imprimir_medidas(res)
    else:
        res = galeria(a.etiqueta, a.carpeta)
        with open(os.path.join(a.carpeta, a.etiqueta + "_galeria.json"), "w", encoding="utf-8") as f:
            json.dump(res, f, ensure_ascii=False, indent=1)
        print("listo: %d pantallas" % len([r for r in res if "pantalla" in r]))


if __name__ == "__main__":
    main()
