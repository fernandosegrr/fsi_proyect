# -*- coding: utf-8 -*-
"""
analizar_obj_any.py — Lee el registro OBJ_ANY (modo «cualquier objeto» del modelo de objetos) que la app escribe en logcat y calcula, por umbral de confianza
y por tipo de pasada:
  - falsos obstáculos por minuto (en las pasadas SIN obstáculo),
  - el porcentaje de pasadas donde se detecta el objeto ANTES de 40 cm.

GUÍA DE LECTURA
 1. De dónde salen los datos: de logcat. Antes de la sesión: `adb logcat -G 16M` y `adb logcat -c`. Después: `adb logcat -d -v time > sesion.txt`.
    Este programa lo puede hacer solo con --adb (baja el log del teléfono por adb con `logcat -d -v time`).
 2. Cada línea OBJ_ANY (una por cuadro analizado) trae, para los 6 umbrales 0.01, 0.05, 0.10, 0.15, 0.20 y 0.30, cuántas cajas pasan los filtros (f), cuántas además
    persisten (p) y la zona y cercanía de la mejor. Una caja CUENTA en ese cuadro si p > 0.
 3. Las pasadas se sacan de las líneas `NAV ... ruta iniciada` (empieza), `NAV ... comando=FORWARD` (el carro arranca) y `NAV ... fin:` o `NAV ... cancelado:` (termina).
    Se asignan EN ORDEN a los tipos que se pasan con --pasadas. El tipo es `vacio` (sin obstáculo) o el nombre del objeto (termo, caja, mochila...).
 4. La distancia al objeto en cada instante es  d0 - v * (t - t_arranque)  con d0 = --distancia-inicial (cm al objeto cuando el carro arranca; por defecto 120) y
    v = --velocidad (cm/s; por defecto 19.3, la del perfil AUTO a 210). La distancia a la que se detecta es la del PRIMER cuadro en que la caja cuenta. «Antes de 40 cm»
    quiere decir que a esa hora el objeto todavía estaba a 40 cm o más del carro. Es una cuenta aproximada: el video llega con ~0.3 s de retraso y no se descuenta.
 5. Falsos obstáculos por minuto: en las pasadas `vacio`, los EPISODIOS (cuadros seguidos con p > 0 cuentan como uno; un hueco de más de --hueco ms los separa)
    divididos entre los minutos que duró la pasada (del inicio de la ruta a su fin).
 6. --autoprueba corre una prueba interna con un log inventado (no necesita nada).

Uso:
    python scripts/analizar_obj_any.py sesion.txt --pasadas vacio,vacio,termo,termo,caja,caja,mochila,mochila --distancia-inicial 120
    python scripts/analizar_obj_any.py --adb --pasadas vacio,termo,caja
    python scripts/analizar_obj_any.py --autoprueba
Solo números: no mira ni guarda ninguna imagen.
"""

import argparse
import re
import subprocess
import sys
from datetime import datetime

UMBRALES = ["0.01", "0.05", "0.10", "0.15", "0.20", "0.30"]
RE_HORA = re.compile(r"^(\d\d-\d\d) (\d\d:\d\d:\d\d\.\d{3})")
RE_UMBRAL = re.compile(r"u(\d\.\d\d)=f(-?\d+),p(-?\d+),z(-?\d+),c(-?\d+)")


def segundos(texto_hora):
    """«MM-DD HH:MM:SS.mmm» -> segundos (el día cuenta: 86400 s)."""
    m = RE_HORA.match(texto_hora)
    if not m:
        return None
    fecha = datetime.strptime("2026-" + m.group(1) + " " + m.group(2), "%Y-%m-%d %H:%M:%S.%f")
    return fecha.timestamp()


def leer_log(lineas):
    """Devuelve (cuadros, eventos): los cuadros OBJ_ANY [(t, {umbral: (f, p, z, c)})] y los eventos NAV [(t, texto)]."""
    cuadros = []
    eventos = []
    for linea in lineas:
        t = segundos(linea)
        if t is None:
            continue
        if "OBJ_ANY" in linea and "u0.01=" in linea:
            por_umbral = {}
            for m in RE_UMBRAL.finditer(linea):
                por_umbral[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)))
            if por_umbral:
                cuadros.append((t, por_umbral))
        elif "NAV" in linea and ("ruta iniciada" in linea or "comando=FORWARD" in linea or "fin:" in linea or "cancelado:" in linea):
            eventos.append((t, linea))
    return cuadros, eventos


def sacar_pasadas(eventos):
    """Agrupa los eventos NAV en pasadas [(inicio, arranque, fin)] en orden."""
    pasadas = []
    actual = None
    for t, texto in eventos:
        if "ruta iniciada" in texto:
            if actual is not None and actual["fin"] is None:
                actual["fin"] = t   # la anterior no avisó que terminó
            actual = {"inicio": t, "arranque": None, "fin": None}
            pasadas.append(actual)
        elif actual is not None and "comando=FORWARD" in texto and actual["arranque"] is None:
            actual["arranque"] = t
        elif actual is not None and ("fin:" in texto or "cancelado:" in texto) and actual["fin"] is None:
            actual["fin"] = t
    return [p for p in pasadas if p["fin"] is not None and p["fin"] > p["inicio"]]


def episodios(tiempos, hueco_s):
    """Cuántos grupos de tiempos hay si un hueco mayor a hueco_s separa dos grupos."""
    cuenta = 0
    anterior = None
    for t in tiempos:
        if anterior is None or t - anterior > hueco_s:
            cuenta += 1
        anterior = t
    return cuenta


def analizar(cuadros, pasadas, tipos, d0_cm, v_cm_s, hueco_ms=500):
    """Devuelve una tabla por umbral y tipo: pasadas, detectadas antes de 40 cm, falsos por minuto."""
    resultado = {u: {} for u in UMBRALES}
    for indice, pasada in enumerate(pasadas):
        if indice >= len(tipos):
            break
        tipo = tipos[indice]
        arranque = pasada["arranque"] if pasada["arranque"] is not None else pasada["inicio"]
        propios = [(t, d) for t, d in cuadros if pasada["inicio"] <= t <= pasada["fin"]]
        minutos = max((pasada["fin"] - pasada["inicio"]) / 60.0, 1e-9)
        for u in UMBRALES:
            fila = resultado[u].setdefault(tipo, {"pasadas": 0, "antes_40": 0, "falsos": 0, "minutos": 0.0, "dist_detecta": []})
            cuentan = [t for t, d in propios if u in d and d[u][1] > 0]
            fila["pasadas"] += 1
            fila["minutos"] += minutos
            if tipo == "vacio":
                fila["falsos"] += episodios(cuentan, hueco_ms / 1000.0)
            else:
                if cuentan:
                    distancia = d0_cm - v_cm_s * (cuentan[0] - arranque)
                    fila["dist_detecta"].append(distancia)
                    if distancia >= 40.0:
                        fila["antes_40"] += 1
    return resultado


def texto_tabla(resultado):
    tipos = []
    for u in UMBRALES:
        for tipo in resultado[u]:
            if tipo not in tipos:
                tipos.append(tipo)
    objetos = [t for t in tipos if t != "vacio"]
    lineas = []
    cabecera = "umbral | falsos/min (vacio)"
    for tipo in objetos:
        cabecera += " | %s: antes de 40 cm (distancia media)" % tipo
    lineas.append(cabecera)
    for u in UMBRALES:
        partes = [u]
        v = resultado[u].get("vacio")
        partes.append("%.1f (%d en %.1f min)" % (v["falsos"] / v["minutos"], v["falsos"], v["minutos"]) if v and v["minutos"] > 0 else "-")
        for tipo in objetos:
            f = resultado[u].get(tipo)
            if not f or f["pasadas"] == 0:
                partes.append("-")
                continue
            dist = ("%.0f cm" % (sum(f["dist_detecta"]) / len(f["dist_detecta"]))) if f["dist_detecta"] else "no se detecto"
            partes.append("%d de %d = %.0f %% (%s)" % (f["antes_40"], f["pasadas"], 100.0 * f["antes_40"] / f["pasadas"], dist))
        lineas.append(" | ".join(partes))
    return "\n".join(lineas)


def log_inventado():
    """Un log de mentira: 2 pasadas vacias (una con ruido a 0.01) y 3 con un termo (se ve a 70, 50 y 30 cm segun el umbral). Para --autoprueba."""
    lineas = []

    def hora(s):
        total = int(s)
        ms = int(round((s - total) * 1000))
        return "10-14 10:%02d:%02d.%03d" % (total // 60, total % 60, ms)

    def obj_any(s, p01=0, p05=0, p10=0, p15=0, p20=0, p30=0):
        valores = [p01, p05, p10, p15, p20, p30]
        partes = ["u%s=f%d,p%d,z%d,c%d" % (UMBRALES[i], 1 if valores[i] else 0, valores[i], 1 if valores[i] else -1, 2 if valores[i] else 0) for i in range(6)]
        return "%s D/OBJ_ANY( 1): OBJ_ANY ms_modelo=60.0 color=0/0,0/0,0/0 decidio=nadie severidad=0 desmentidos=0 %s" % (hora(s), " ".join(partes))

    # Pasada 1 (vacia): empieza en 0 s, arranca a 0.1 s, dura 60 s; el umbral 0.01 tiene 2 episodios de ruido y 0.05 uno.
    lineas.append("%s I/NAV( 1): ruta iniciada (6 puntos)" % hora(0))
    lineas.append("%s I/NAV( 1): comando=FORWARD estado=SIGUIENDO" % hora(0.1))
    for decima in range(10, 600):   # un cuadro cada 0.1 s
        s = decima / 10.0
        ruido01 = 1 if (10 <= s < 13 or 40 <= s < 42) else 0
        ruido05 = 1 if 20 <= s < 22 else 0
        lineas.append(obj_any(s, p01=ruido01, p05=ruido05))
    lineas.append("%s I/NAV( 1): fin: TERMINADO" % hora(60))
    # Pasadas 2 a 4 (termo): empiezan en 100, 200 y 300 s, arrancan 0.1 s despues; el carro avanza a 19.3 cm/s con d0 = 120 cm.
    # Se ve a 70 cm (2.6 s), 50 cm (3.6 s) y 30 cm (4.7 s) segun el umbral: 0.01 y 0.10 a 70 cm; 0.20 a 50 cm; 0.30 no ve nada.
    for numero, base in enumerate((100, 200, 300)):
        lineas.append("%s I/NAV( 1): ruta iniciada (6 puntos)" % hora(base))
        lineas.append("%s I/NAV( 1): comando=FORWARD estado=SIGUIENDO" % hora(base + 0.1))
        for paso in range(0, 80):
            t = paso * 0.1
            p01 = 1 if t >= 2.7 else 0
            p10 = 1 if t >= 2.7 else 0
            p20 = 1 if t >= 3.7 else 0
            p30 = 1 if (numero == 2 and t >= 4.8) else 0
            lineas.append(obj_any(base + t + 0.1, p01=p01, p10=p10, p15=p10, p20=p20, p30=p30))
        lineas.append("%s I/NAV( 1): fin: ERROR" % hora(base + 8))
    return lineas


def autoprueba():
    cuadros, eventos = leer_log(log_inventado())
    pasadas = sacar_pasadas(eventos)
    tipos = ["vacio", "termo", "termo", "termo"]
    resultado = analizar(cuadros, pasadas, tipos, 120.0, 19.3)
    errores = []
    if len(pasadas) != 4:
        errores.append("se esperaban 4 pasadas y salieron %d" % len(pasadas))
    v01 = resultado["0.01"]["vacio"]
    v05 = resultado["0.05"]["vacio"]
    v10 = resultado["0.10"]["vacio"]
    if v01["falsos"] != 2 or v05["falsos"] != 1 or v10["falsos"] != 0:
        errores.append("falsos esperados 2/1/0 y salieron %d/%d/%d" % (v01["falsos"], v05["falsos"], v10["falsos"]))
    if abs(v01["falsos"] / v01["minutos"] - 2.0) > 0.05:
        errores.append("falsos por minuto esperados 2.0 y salieron %.2f" % (v01["falsos"] / v01["minutos"]))
    t01 = resultado["0.01"]["termo"]
    t20 = resultado["0.20"]["termo"]
    t30 = resultado["0.30"]["termo"]
    if t01["antes_40"] != 3 or t01["pasadas"] != 3:
        errores.append("con 0.01 el termo debia verse antes de 40 cm en 3 de 3 y salio %d de %d" % (t01["antes_40"], t01["pasadas"]))
    if t20["antes_40"] != 3:
        errores.append("con 0.20 el termo (a ~50 cm) debia verse antes de 40 cm en 3 de 3 y salio %d" % t20["antes_40"])
    if t30["antes_40"] != 0 or len(t30["dist_detecta"]) != 1:
        errores.append("con 0.30 solo la 3.a pasada lo ve, y tarde: antes de 40 cm = 0 y 1 deteccion; salio %d y %d" % (t30["antes_40"], len(t30["dist_detecta"])))
    print(texto_tabla(resultado))
    if errores:
        print("\nAUTOPRUEBA: FALLO")
        for e in errores:
            print("  -", e)
        return 1
    print("\nAUTOPRUEBA: bien (4 pasadas, falsos 2/1/0 por umbral, termo visto a ~70, ~50 y ~28 cm segun el umbral)")
    return 0


def main():
    lector = argparse.ArgumentParser(description="Calcula falsos obstaculos por minuto y % de pasadas detectadas antes de 40 cm por umbral, desde el registro OBJ_ANY.")
    lector.add_argument("archivo", nargs="?", help="log de `adb logcat -d -v time` (omitir con --adb o --autoprueba)")
    lector.add_argument("--adb", action="store_true", help="bajar el log del telefono con adb (logcat -d -v time)")
    lector.add_argument("--pasadas", default="", help="tipos de las pasadas EN ORDEN, separados por coma: vacio,termo,caja,mochila...")
    lector.add_argument("--distancia-inicial", type=float, default=120.0, help="cm al objeto cuando el carro arranca (120)")
    lector.add_argument("--velocidad", type=float, default=19.3, help="cm/s del carro (19.3 = perfil AUTO a 210)")
    lector.add_argument("--hueco", type=int, default=500, help="ms de hueco que separan dos episodios de falso obstaculo (500)")
    lector.add_argument("--autoprueba", action="store_true", help="correr la prueba interna con un log inventado")
    a = lector.parse_args()
    if a.autoprueba:
        return autoprueba()
    if a.adb:
        adb = "adb"
        import os
        ruta = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk", "platform-tools", "adb.exe")
        if os.path.exists(ruta):
            adb = ruta
        texto = subprocess.run([adb, "logcat", "-d", "-v", "time"], capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
        lineas = texto.splitlines()
    elif a.archivo:
        with open(a.archivo, encoding="utf-8", errors="replace") as f:
            lineas = f.read().splitlines()
    else:
        lector.error("falta el archivo del log, --adb o --autoprueba")
        return 2
    tipos = [t.strip() for t in a.pasadas.split(",") if t.strip()]
    cuadros, eventos = leer_log(lineas)
    pasadas = sacar_pasadas(eventos)
    print("cuadros OBJ_ANY: %d, pasadas encontradas: %d, tipos dados: %d" % (len(cuadros), len(pasadas), len(tipos)))
    if len(pasadas) != len(tipos):
        print("OJO: las pasadas encontradas y los tipos dados no son los mismos: se usan los primeros %d en orden" % min(len(pasadas), len(tipos)))
    print(texto_tabla(analizar(cuadros, pasadas, tipos, a.distancia_inicial, a.velocidad, a.hueco)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
