#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Evaluacion ciega lado a lado (v3 e37f77b vs actual v5-carbon) con semilla externa.

SIMULADO / NO MEDIDO EN EL CARRO.
Usa exactamente el mismo arnes calibrado actual (pruebas_autonomo_v2 + mundo_v2 +
simulador_navegador) para ambos commits y genera escenarios nuevos mediante
AV5_CIEGA=<semilla> con SplitMix64, en laser normal (AV5_LASER=0) y pesimista
(AV5_LASER=1). Salida en texto plano, sin tablas Markdown.
"""
import argparse
import concurrent.futures
import os
import subprocess
import sys
import tempfile

import compuertas_v5 as cv5


def correr_ciega(bin_path, laser, corridas, semilla):
    env = os.environ.copy()
    env["AV5_CIEGA"] = str(semilla)
    env["AV5_LASER"] = str(laser)
    env.pop("AV5_APAGADO", None)
    env.pop("AV5_CONJUNTO", None)
    p = subprocess.run(
        [bin_path, str(corridas)],
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if p.returncode != 0:
        raise RuntimeError(f"Fallo ejecucion ciega ({bin_path}, laser={laser}): rc={p.returncode}\n{p.stderr}")
    filas = {}
    raw_lines = []
    for linea in p.stdout.splitlines():
        if linea.startswith("AV2 "):
            raw_lines.append(linea.strip())
            parsed = cv5.parse_linea_av2(linea)
            if parsed:
                filas[parsed["escenario"]] = parsed
    return {
        "header": p.stdout.splitlines()[0] if p.stdout else "",
        "filas": filas,
        "raw_lines": raw_lines,
        "exit": p.returncode,
    }


def detectar_peores(v3_res, act_res):
    peores = []
    for mod_k, label in [("ciega_normal", "normal"), ("ciega_pesimista", "pesimista")]:
        for esc, a in act_res[mod_k]["filas"].items():
            b = v3_res[mod_k]["filas"].get(esc)
            if not b:
                continue
            razones = []
            if a["exito"] < b["exito"]:
                razones.append(f"ok {b['exito']}->{a['exito']}")
            if a["contactos"] > b["contactos"]:
                razones.append(f"contacto {b['contactos']}->{a['contactos']}")
            if a["ayuda"] > b["ayuda"]:
                razones.append(f"ayuda {b['ayuda']}->{a['ayuda']}")
            if a["error"] > b["error"]:
                razones.append(f"error {b['error']}->{a['error']}")
            if a["terminado_falso"] > b["terminado_falso"]:
                razones.append(f"terminado_falso {b['terminado_falso']}->{a['terminado_falso']}")
            if a["sin_terminar"] > b["sin_terminar"]:
                razones.append(f"sin_terminar {b['sin_terminar']}->{a['sin_terminar']}")
            if esc in cv5.ESC_G1:
                if a["paradas"] > b["paradas"]:
                    razones.append(f"paradas {b['paradas']}->{a['paradas']}")
                if a["esquivas"] > b["esquivas"]:
                    razones.append(f"esquivas {b['esquivas']}->{a['esquivas']}")
            if razones:
                peores.append(f"[{label}] {esc}: " + ", ".join(razones))
    return peores


def evaluar_g1_g7_ciego(v3_res, act_res):
    cumple, detalles = cv5.evaluar_compuertas(v3_res, act_res)
    # Asegurar que un aumento de contactos en G4 cuando v3 tuvo 0 tambien marque G4 NO CUMPLE
    for mat_k, mat in act_res.items():
        for esc in cv5.ESC_G4_MUEBLES:
            a = mat["filas"].get(esc)
            b = v3_res[mat_k]["filas"].get(esc)
            if a and b and a["contactos"] > b["contactos"] and b["contactos"] == 0:
                cumple["G4"] = False
                detalles.append(f"G4 [{mat_k}] {esc}: cont v5={a['contactos']} > v3={b['contactos']}")
    return cumple, detalles


def git_head_info():
    commit = subprocess.check_output(
        ["git", "-C", cv5.ROOT, "rev-parse", "HEAD"],
        text=True,
    ).strip()
    rama = subprocess.check_output(
        ["git", "-C", cv5.ROOT, "rev-parse", "--abbrev-ref", "HEAD"],
        text=True,
    ).strip()
    return rama, commit


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--semilla", type=int, required=True, help="Semilla uint64 para AV5_CIEGA")
    ap.add_argument("--corridas", type=int, default=40, help="Corridas por escenario (10..5000)")
    args = ap.parse_args()
    if args.semilla <= 0:
        raise SystemExit("Error: --semilla debe ser un entero positivo.")

    rama, commit = git_head_info()
    with tempfile.TemporaryDirectory(prefix="fsi_v5_ciega_") as tmp:
        bin_v3 = cv5.compilar_ref("e37f77b", tmp)
        bin_v5 = cv5.compilar_actual(tmp)
        specs = [
            ("v3", "ciega_normal", bin_v3, 0),
            ("v3", "ciega_pesimista", bin_v3, 1),
            ("v5", "ciega_normal", bin_v5, 0),
            ("v5", "ciega_pesimista", bin_v5, 1),
        ]
        res = {"v3": {}, "v5": {}}
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
            futs = {
                ex.submit(correr_ciega, bpath, laser, args.corridas, args.semilla): (ver, mat_k)
                for ver, mat_k, bpath, laser in specs
            }
            for fut, (ver, mat_k) in futs.items():
                res[ver][mat_k] = fut.result()

    v3_res = res["v3"]
    v5_res = res["v5"]
    cumple, detalles = evaluar_g1_g7_ciego(v3_res, v5_res)
    peores = detectar_peores(v3_res, v5_res)

    print("=== EVALUACION CIEGA v3 (e37f77b) vs v5-carbon (SIMULADO / NO MEDIDO EN EL CARRO) ===")
    print(f"semilla={args.semilla}  corridas={args.corridas}  rama={rama}  commit_v5={commit}  base_v3=e37f77b")

    for mat_k, titulo in [("ciega_normal", "LASER NORMAL"), ("ciega_pesimista", "LASER PESIMISTA")]:
        print(f"\n--- {titulo} ({mat_k}, N={args.corridas}, semilla={args.semilla}) ---")
        print(f"v3: {v3_res[mat_k]['header']}")
        print(f"v5: {v5_res[mat_k]['header']}")
        print("Comparacion lado a lado (ok/contacto/bajo_mueble/ayuda):")
        tot_v3 = {"ok": 0, "cont": 0, "bm": 0, "ay": 0, "err": 0, "tf": 0}
        tot_v5 = {"ok": 0, "cont": 0, "bm": 0, "ay": 0, "err": 0, "tf": 0}
        for esc, a in v5_res[mat_k]["filas"].items():
            b = v3_res[mat_k]["filas"].get(esc, {})
            for d, src in [(tot_v3, b), (tot_v5, a)]:
                d["ok"] += src.get("exito", 0)
                d["cont"] += src.get("contactos", 0)
                d["bm"] += src.get("bajo_mueble", 0)
                d["ay"] += src.get("ayuda", 0)
                d["err"] += src.get("error", 0)
                d["tf"] += src.get("terminado_falso", 0)
            print(
                f"  {esc:<22} "
                f"v3: {b.get('exito',0):2d}/{b.get('contactos',0):2d}/{b.get('bajo_mueble',0):2d}/{b.get('ayuda',0):2d} "
                f"(err={b.get('error',0)} tf={b.get('terminado_falso',0)} par={b.get('paradas',0):3d} esq={b.get('esquivas',0):2d})  |  "
                f"v5: {a['exito']:2d}/{a['contactos']:2d}/{a['bajo_mueble']:2d}/{a['ayuda']:2d} "
                f"(err={a['error']} tf={a['terminado_falso']} par={a['paradas']:3d} esq={a['esquivas']:2d} causas=({a['causas']}))"
            )
        print(
            f"  TOTAL ({mat_k})      "
            f"v3: {tot_v3['ok']:4d}/{tot_v3['cont']:3d}/{tot_v3['bm']:3d}/{tot_v3['ay']:3d} (err={tot_v3['err']} tf={tot_v3['tf']})  |  "
            f"v5: {tot_v5['ok']:4d}/{tot_v5['cont']:3d}/{tot_v5['bm']:3d}/{tot_v5['ay']:3d} (err={tot_v5['err']} tf={tot_v5['tf']})"
        )
        print(f"Salida cruda v3 ({mat_k}):")
        for ln in v3_res[mat_k]["raw_lines"]:
            print("  " + ln)
        print(f"Salida cruda v5 ({mat_k}):")
        for ln in v5_res[mat_k]["raw_lines"]:
            print("  " + ln)

    print(f"\n--- VEREDICTO COMPUERTAS G1-G7 (semilla={args.semilla}, SIMULADO) ---")
    for g in ["G1", "G2", "G3", "G4", "G5", "G6", "G7"]:
        print(f"  {g}: {'CUMPLE' if cumple[g] else 'NO CUMPLE'}")

    print(f"\n--- CASOS DONDE v5 ES PEOR QUE v3 (semilla={args.semilla}) ---")
    if peores:
        for p_txt in peores:
            print("  -", p_txt)
    else:
        print("  - Ninguno")

    if detalles:
        print(f"\n--- DETALLE DE COMPUERTAS / INCUMPLIMIENTOS (semilla={args.semilla}) ---")
        for d in detalles:
            print("  -", d)


if __name__ == "__main__":
    main()
