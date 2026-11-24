#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Evaluacion CIEGA de v6 con semilla externa.  SIMULADO / NO MEDIDO EN EL CARRO.

Compara v3 (e37f77b), v5-carbon-fix (freeze/v5-carbon-fix, la referencia) y v6 (el arbol de trabajo con AV6_BITS=master) con el MISMO banco, escenarios nuevos
generados con AV5_CIEGA=<semilla> (SplitMix64: parametros, colores, posiciones y pisos que nadie ajusto), en laser normal y pesimista y con ruido perceptivo 0 y 1.
Entran los escenarios de siempre y la familia sin eco (objetos oscuros, manchas, reflejos saturados).

Uso:  python scripts/evaluacion_ciega_v6.py --semilla <uint64> [--corridas 120] [--ruido 0,1]
      (o scripts/evaluacion_ciega.ps1 -Semilla <uint64>)

La salida es texto plano y se guarda TAL CUAL; no se edita despues.  Lleva las lineas crudas del banco de cada version para poder auditar cualquier numero.
"""
import argparse
import concurrent.futures
import datetime
import os
import subprocess
import sys
import tempfile

import compuertas_v6 as cv


def correr_ciega(exe, ruido, laser, corridas, semilla, extra_env=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("AV")}
    env["AV5_CIEGA"] = str(semilla)
    env["AV6_RUIDO"] = str(ruido)
    env["AV5_LASER"] = str(laser)
    env.update(extra_env or {})
    p = subprocess.run([exe, str(corridas)], env=env, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if p.returncode != 0:
        raise RuntimeError("Fallo la ejecucion ciega (%s, ruido=%s, laser=%s): codigo %d\n%s" % (exe, ruido, laser, p.returncode, p.stderr))
    filas, crudas = {}, []
    for linea in p.stdout.splitlines():
        d = cv.parsear(linea)
        if d:
            filas[d["esc"]] = d
            crudas.append(linea.strip())
    return {"filas": filas, "crudas": crudas, "cabecera": p.stdout.splitlines()[0] if p.stdout else ""}


def git(*a):
    return subprocess.check_output(["git", "-C", cv.ROOT] + list(a), text=True).strip()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--semilla", type=int, required=True, help="semilla uint64 para AV5_CIEGA")
    ap.add_argument("--corridas", type=int, default=120, help="corridas por escenario (10 a 5000)")
    ap.add_argument("--ruido", default="0,1", help="niveles de ruido perceptivo (0 = banco de siempre, 1 = calibrado con las cajas negras)")
    ap.add_argument("--workers", type=int, default=os.cpu_count() or 4)
    args = ap.parse_args()
    if args.semilla <= 0:
        raise SystemExit("Error: --semilla debe ser un entero positivo.")

    rama, commit = git("rev-parse", "--abbrev-ref", "HEAD"), git("rev-parse", "HEAD")
    sucio = git("status", "--porcelain", "--untracked-files=no") != ""
    ruidos = [int(x) for x in args.ruido.split(",")]
    print("=== EVALUACION CIEGA v3 vs v5-carbon-fix vs v6 (SIMULADO / NO MEDIDO EN EL CARRO) ===")
    print("semilla=%d  corridas=%d  ruido=%s  rama=%s  commit_v6=%s  arbol_con_cambios_sin_confirmar=%s" % (args.semilla, args.corridas, args.ruido, rama, commit, "SI" if sucio else "no"))
    print("base_v3=%s  referencia_v5fix=%s (%s)  inicio=%s" % (cv.REFS["v3"], cv.REFS["v5fix"], git("rev-parse", cv.REFS["v5fix"]), datetime.datetime.now().astimezone().isoformat(timespec="seconds")))

    with tempfile.TemporaryDirectory(prefix="fsi_v6_ciega_") as tmp:
        exes = {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as ex:
            fut = {"actual": ex.submit(cv.compilar, "actual", None, tmp), "v3": ex.submit(cv.compilar, "v3", cv.REFS["v3"], tmp), "v5fix": ex.submit(cv.compilar, "v5fix", cv.REFS["v5fix"], tmp)}
            for k, f in fut.items():
                exes[k] = f.result()
        todo, crudo = {}, {}
        mats = [("CIEGA", r, l) for r in ruidos for l in (0, 1)]
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as ex:
            futs = {}
            for m in mats:
                futs[m] = {"v3": ex.submit(correr_ciega, exes["v3"], m[1], m[2], args.corridas, args.semilla),
                           "v5fix": ex.submit(correr_ciega, exes["v5fix"], m[1], m[2], args.corridas, args.semilla),
                           "v6": ex.submit(correr_ciega, exes["actual"], m[1], m[2], args.corridas, args.semilla, {"AV6_BITS": "master"})}
            for m, d in futs.items():
                res = {k: f.result() for k, f in d.items()}
                todo[m] = {k: v["filas"] for k, v in res.items()}
                crudo[m] = {k: v["crudas"] for k, v in res.items()}

    resumen = cv.reporte(todo, args)
    for m in mats:
        for ver, titulo in (("v3", "v3"), ("v5fix", "v5-carbon-fix"), ("v6", "v6")):
            print("\n--- SALIDA CRUDA %s (semilla=%d, ruido=%d, laser=%s) ---" % (titulo, args.semilla, m[1], "pesimista" if m[2] else "normal"))
            for linea in crudo[m][ver]:
                print("  " + linea)
    print("\nfin=%s" % datetime.datetime.now().astimezone().isoformat(timespec="seconds"))
    sys.exit(0 if not any(x == "NO CUMPLE" for v in resumen.values() for x in v.values()) else 1)


if __name__ == "__main__":
    main()
