"""Compuertas del modo autónomo v2: el banco realista (nucleo_c/pruebas_autonomo_v2.c) contra tres versiones del núcleo.

SIMULADO / NO MEDIDO en el carro. Solo PC: no conecta con el carro ni con el teléfono, no lee secretos.h,
no cambia de rama y no genera binarios de firmware. Compila en una carpeta temporal.

Versiones (el MISMO banco, el MISMO mundo y el MISMO simulador físico; solo cambia el núcleo de decisión:
navegador, fusión, clasificador de color, corredor, calibración y trayectoria):
  v11     bd1727b  (versión final publicada 1.1)
  partida 6bea71e  (rama fix/navegacion-esquive-confirmado)
  actual  el árbol de trabajo

Uso:
  python scripts/compuertas_autonomo.py                    1000 corridas por escenario, las tres versiones
  python scripts/compuertas_autonomo.py --corridas 200     más rápido, para iterar
  python scripts/compuertas_autonomo.py --solo actual      solo el árbol de trabajo
Salida: docs/precision/autonomo_v2_mc_<version>.txt, docs/precision/autonomo_v2_comparacion.txt y código de
salida 1 si la versión actual no pasa las compuertas.
"""
import argparse
import concurrent.futures
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VERSIONES = [("v11", "bd1727b", ["-DAV2_BASE_V11", "-DSIM_SIN_FUENTES"]),
             ("partida", "6bea71e", ["-DAV2_BASE_6BEA"]),
             ("actual", None, [])]
# El núcleo de decisión (sale de cada versión) y el banco con su mundo (siempre del árbol de trabajo).
NUCLEO = ["navegador.c", "navegador.h", "trayectoria.c", "trayectoria.h", "obstaculos.c", "obstaculos.h",
          "mascara_vision.c", "mascara_vision.h", "calibracion.c", "calibracion.h", "fusion.c", "fusion.h",
          "corredor.c", "corredor.h", "movimiento_piso.h"]
BANCO = ["pruebas_autonomo_v2.c", "mundo_v2.c", "mundo_v2.h", "simulador_navegador.c", "simulador_navegador.h"]
OPCIONALES = ["vision_tele.c", "vision_tele.h", "odometria.c", "odometria.h"]   # módulos nuevos: solo existen en la versión actual
N_ESCENARIOS = 21


def exportar(destino, referencia):
    destino.mkdir(parents=True)
    for nombre in NUCLEO + (OPCIONALES if referencia is None else []):
        if referencia:
            datos = subprocess.run(["git", "show", f"{referencia}:nucleo_c/{nombre}"], cwd=ROOT,
                                   capture_output=True, check=True).stdout
            (destino / nombre).write_bytes(datos)
        elif (ROOT / "nucleo_c" / nombre).exists():
            shutil.copyfile(ROOT / "nucleo_c" / nombre, destino / nombre)
    for nombre in BANCO:
        shutil.copyfile(ROOT / "nucleo_c" / nombre, destino / nombre)


def compilar(destino, banderas):
    fuentes = sorted(p.name for p in destino.glob("*.c"))
    comando = ["gcc", "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2"] + banderas + \
              ["-o", "banco.exe"] + fuentes + ["-lm"]
    subprocess.run(comando, cwd=destino, check=True)


def correr(destino, corridas, escenario):
    r = subprocess.run([str(destino / "banco.exe"), str(corridas), str(escenario)], cwd=destino,
                       capture_output=True, check=True)
    lineas = [l for l in r.stdout.decode("utf-8", "replace").replace("\r\n", "\n").split("\n") if l.startswith("AV2 ")]
    return escenario, lineas


def campos(linea):
    d = {"causas": ""}
    m = re.search(r"\| causas(.*)$", linea)
    if m:
        d["causas"] = m.group(1).strip()
    d["escenario"] = re.search(r"escenario=(\S+)", linea).group(1)
    d["tipo"] = re.search(r"tipo=(\S+)", linea).group(1)
    for clave in ["n", "exito", "contactos", "ayuda", "error", "terminado_falso", "sin_terminar", "llegan", "esquivas",
                  "paradas", "corridas_con_parada", "sin_piso"]:
        d[clave] = int(re.search(rf"\b{clave}=(\d+)", linea).group(1))
    for clave, patron in [("t", r"tiempo_ms media=(\d+) p95=(\d+)"), ("lat", r"lateral media=(\d+) p95=(\d+)"),
                          ("lon", r"longitudinal media=(\d+) p95=(\d+)")]:
        m = re.search(patron, linea)
        d[clave + "_media"], d[clave + "_p95"] = int(m.group(1)), int(m.group(2))
    d["desvio_p95"] = int(re.search(r"desvio_max p95=(\d+)", linea).group(1))
    return d


def escribir(ruta, texto):
    with open(ruta, "w", encoding="utf-8", newline="\n") as archivo:
        archivo.write(texto)


def compuertas(actual, partida):
    """Las metas medibles del encargo. Devuelve la lista de fallos (vacía = pasa)."""
    fallos = []
    for nombre, a in actual.items():
        n = a["n"]
        if a["tipo"] == "limite":
            continue   # límite conocido de la percepción: se informa, no es compuerta
        if a["contactos"] != 0:
            fallos.append(f"{nombre}: {a['contactos']} contactos (meta 0)")
        if a["terminado_falso"] != 0:
            fallos.append(f"{nombre}: {a['terminado_falso']} Terminado sin dejar atrás el objeto (meta 0)")
        if a["tipo"] == "obstaculo":
            if a["exito"] * 100 < n * 99:
                fallos.append(f"{nombre}: éxito {a['exito']}/{n} (meta >= 99 %)")
            if a["ayuda"] * 100 >= n:
                fallos.append(f"{nombre}: AYUDA {a['ayuda']}/{n} (meta < 1 %)")
        if a["tipo"] == "falso":
            if a["esquivas"] != 0 or a["ayuda"] != 0:
                fallos.append(f"{nombre}: falso positivo con {a['esquivas']} esquives y {a['ayuda']} ayudas (meta 0)")
            if a["exito"] * 100 < n * 99:
                fallos.append(f"{nombre}: éxito {a['exito']}/{n} (meta >= 99 %)")
        if a["tipo"] == "imposible" and a["exito"] * 100 < n * 99:
            fallos.append(f"{nombre}: paró sin tocar y pidió ayuda {a['exito']}/{n} (meta >= 99 %)")
        if partida and nombre in partida:
            p = partida[nombre]
            if a["exito"] < p["exito"]:
                fallos.append(f"{nombre}: éxito {a['exito']} peor que la rama de partida ({p['exito']})")
            if a["contactos"] > p["contactos"]:
                fallos.append(f"{nombre}: contactos {a['contactos']} peor que la rama de partida ({p['contactos']})")
            if a["tipo"] != "imposible" and a["ayuda"] > p["ayuda"]:
                fallos.append(f"{nombre}: AYUDA {a['ayuda']} peor que la rama de partida ({p['ayuda']})")
    return fallos


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--corridas", type=int, default=1000)
    parser.add_argument("--solo", choices=[v[0] for v in VERSIONES])
    parser.add_argument("--procesos", type=int, default=max(2, (os.cpu_count() or 4) - 2))
    parser.add_argument("--sin-guardar", action="store_true", help="no escribe en docs/precision (para iterar)")
    args = parser.parse_args()
    salida = ROOT / "docs/precision"
    resultados = {}
    with tempfile.TemporaryDirectory(prefix="fsi-autonomo-v2-") as temporal:
        raiz = Path(temporal)
        trabajos = []
        for nombre, ref, banderas in VERSIONES:
            if args.solo and nombre != args.solo:
                continue
            destino = raiz / nombre
            exportar(destino, ref)
            compilar(destino, banderas)
            trabajos += [(nombre, destino, e) for e in range(N_ESCENARIOS)]
        print(f"Simulando {len(trabajos)} escenarios x {args.corridas} corridas en {args.procesos} procesos...", flush=True)
        lineas = {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.procesos) as grupo:
            futuros = {grupo.submit(correr, destino, args.corridas, e): nombre for nombre, destino, e in trabajos}
            for futuro in concurrent.futures.as_completed(futuros):
                e, ls = futuro.result()
                lineas.setdefault(futuros[futuro], {})[e] = ls
        for nombre in lineas:
            texto = [f"-- Banco autonomo v2 (SIMULADO / NO MEDIDO en el carro), version {nombre}, {args.corridas} corridas por escenario, "
                     "semillas 170000 + indice * 10007 + corrida --"]
            resultados[nombre] = {}
            for e in sorted(lineas[nombre]):
                for l in lineas[nombre][e]:
                    texto.append(l)
                    d = campos(l)
                    resultados[nombre][d["escenario"]] = d
            if not args.sin_guardar:
                escribir(salida / f"autonomo_v2_mc_{nombre}.txt", "\n".join(texto) + "\n")
    orden = [v[0] for v in VERSIONES if v[0] in resultados]
    informe = ["SIMULADO / NO MEDIDO en el carro. Banco nucleo_c/pruebas_autonomo_v2.c; "
               f"{args.corridas} corridas por escenario; semillas 170000 + indice * 10007 + corrida.",
               "Por escenario y versión: éxito / contactos / AYUDA / Terminado falso | esquives | paradas | tiempo medio (s) | lateral p95 (mm)", ""]
    nombres = list(resultados[orden[-1]].keys())
    for esc in nombres:
        informe.append(f"{esc}  [{resultados[orden[-1]][esc]['tipo']}]")
        for v in orden:
            d = resultados[v].get(esc)
            if d:
                informe.append(f"   {v:8s} {d['exito']:5d}/{d['n']} ok  contactos {d['contactos']:4d}  ayuda {d['ayuda']:4d}  falso_fin {d['terminado_falso']:3d}"
                               f" | esquives {d['esquivas']:5d} | paradas {d['paradas']:5d} | t {d['t_media'] / 1000:5.1f} s | lat p95 {d['lat_p95']:4d}"
                               f" | causas {d['causas']}")
    fallos = []
    if "actual" in resultados:
        fallos = compuertas(resultados["actual"], resultados.get("partida"))
        informe += ["", f"COMPUERTAS de la versión actual: {'PASAN' if not fallos else str(len(fallos)) + ' fallos'}"] + ["   FALLO " + f for f in fallos]
    texto = "\n".join(informe) + "\n"
    if not args.sin_guardar:
        escribir(salida / "autonomo_v2_comparacion.txt", texto)
    print(texto)
    return 1 if fallos else 0


if __name__ == "__main__":
    raise SystemExit(main())
