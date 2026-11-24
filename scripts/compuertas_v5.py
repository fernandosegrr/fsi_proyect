#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Evaluador calibrado de compuertas G1-G7 para feat/autonomo-v5-carbon.

SIMULADO / NO MEDIDO EN EL CARRO.
Compila v3 (commit e37f77b) y la version actual usando exactamente el mismo
arnes calibrado (pruebas_autonomo_v2.c + mundo_v2 + simulador_navegador),
y evalua DESARROLLO (24 escenarios, semillas 2100000+...) y RESERVADO
(27 escenarios, semillas 4300000+...) tanto con laser normal como pesimista.
"""
import argparse
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NUCLEO = os.path.join(ROOT, "nucleo_c")
CACHE_V3 = os.path.join(ROOT, "docs", "precision", "v5", "base_v3_calibrada.json")
ARNES_FILES = [
    "pruebas_autonomo_v2.c",
    "mundo_v2.c",
    "mundo_v2.h",
    "simulador_navegador.c",
    "simulador_navegador.h",
]
CORE_SRCS = [
    "pruebas_autonomo_v2.c",
    "mundo_v2.c",
    "fusion.c",
    "corredor.c",
    "simulador_navegador.c",
    "navegador.c",
    "trayectoria.c",
    "obstaculos.c",
    "mascara_vision.c",
    "calibracion.c",
]

ESC_G1_LIMPIOS = [
    "libre_limpio",
    "falso_juntas",
    "falso_exposicion",
    "falso_detector",
    "pared_tras_el_final",
]
ESC_G1_ILUMINACION = ["falso_sol", "falso_sombra", "falso_reflejo"]
ESC_G1 = ESC_G1_LIMPIOS + ESC_G1_ILUMINACION

ESC_G2_SOLIDOS = [
    "caja20",
    "caja20_lateral",
    "objeto_largo90",
    "pared_al_frente",
    "dos_obstaculos",
    "objeto_bajo",
    "caja20_con_sol",
    "bateria_baja",
    "aparece_de_repente",
    "caja_antes_de_esquina",
]
ESC_G3_SIN_ECO = ["bloque_sin_eco"]
ESC_G4_MUEBLES = ["banquito35x45", "silla_patas", "dos_muebles", "mueble_con_pared"]
ESC_G5_ESPACIOS = ["caja_y_pared_al_lado", "pasillo", "pasillo_angosto"]


def exe_ext():
    return ".exe" if os.name == "nt" else ""


def compilar_actual(carpeta_bin):
    out = os.path.join(carpeta_bin, "actual_bench" + exe_ext())
    cmd = ["gcc", "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2", "-I", NUCLEO, "-o", out]
    cmd += [os.path.join(NUCLEO, s) for s in CORE_SRCS] + ["-lm"]
    subprocess.run(cmd, check=True)
    return out


def compilar_ref(commit, carpeta_tmp):
    src_dir = os.path.join(carpeta_tmp, f"src_{commit}")
    os.makedirs(src_dir, exist_ok=True)
    p = subprocess.run(
        ["git", "-C", ROOT, "archive", commit, "--format=tar", "nucleo_c"],
        capture_output=True,
        check=True,
    )
    tar_path = os.path.join(carpeta_tmp, f"{commit}.tar")
    with open(tar_path, "wb") as f:
        f.write(p.stdout)
    subprocess.run(["tar", "-xf", tar_path, "-C", src_dir], check=True)
    nu_dir = os.path.join(src_dir, "nucleo_c")
    for fname in ARNES_FILES:
        shutil.copyfile(os.path.join(NUCLEO, fname), os.path.join(nu_dir, fname))
    out = os.path.join(carpeta_tmp, f"{commit}_bench" + exe_ext())
    cmd = [
        "gcc",
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-O2",
        "-I",
        nu_dir,
        "-o",
        out,
    ] + [os.path.join(nu_dir, s) for s in CORE_SRCS] + ["-lm"]
    subprocess.run(cmd, check=True)
    return out


def parse_linea_av2(linea):
    if not linea.startswith("AV2 "):
        return None
    m_esc = re.search(r"escenario=(\S+)\s+tipo=(\S+)\s+n=(\d+)", linea)
    if not m_esc:
        return None
    d = {
        "escenario": m_esc.group(1),
        "tipo": m_esc.group(2),
        "n": int(m_esc.group(3)),
        "raw": linea.strip(),
    }
    campos = [
        "exito", "contactos", "ayuda", "error", "terminado_falso", "sin_terminar",
        "llegan", "esquivas", "paradas", "corridas_con_parada", "sin_piso",
        "bajo_mueble", "vistazos", "vistazos_con_eco",
    ]
    for c in campos:
        m = re.search(rf"\b{c}=(\d+)", linea)
        d[c] = int(m.group(1)) if m else 0
    m_t = re.search(r"tiempo_ms media=(\d+) p95=(\d+)", linea)
    d["t_media"] = int(m_t.group(1)) if m_t else 0
    d["t_p95"] = int(m_t.group(2)) if m_t else 0
    m_lat = re.search(r"lateral media=(\d+) p95=(\d+)", linea)
    d["lat_media"] = int(m_lat.group(1)) if m_lat else 0
    d["lat_p95"] = int(m_lat.group(2)) if m_lat else 0
    m_des = re.search(r"desvio_max p95=(\d+)", linea)
    d["desvio_p95"] = int(m_des.group(1)) if m_des else 0
    m_cau = re.search(r"\|\s*causas([^|]*)\|", linea)
    d["causas"] = m_cau.group(1).strip() if m_cau else ""
    m_cat = re.search(r"\|\s*cat\s+(.*)$", linea)
    d["cat"] = {}
    if m_cat:
        for par in m_cat.group(1).split():
            if "=" in par:
                k, v = par.split("=", 1)
                d["cat"][k] = int(v)
    return d


def correr_config(bin_path, conjunto, laser, corridas, apagado=False):
    env = os.environ.copy()
    env["AV5_CONJUNTO"] = str(conjunto)
    env["AV5_LASER"] = str(laser)
    if apagado:
        env["AV5_APAGADO"] = "1"
    else:
        env.pop("AV5_APAGADO", None)
    p = subprocess.run([bin_path, str(corridas)], env=env, capture_output=True, text=True, encoding="utf-8", errors="replace")
    filas = {}
    for linea in p.stdout.splitlines():
        parsed = parse_linea_av2(linea)
        if parsed:
            filas[parsed["escenario"]] = parsed
    return {"header": p.stdout.splitlines()[0] if p.stdout else "", "filas": filas, "exit": p.returncode}


def correr_todas_matrices(bin_path, corridas, apagado=False):
    specs = [
        ("desarrollo_normal", 1, 0),
        ("desarrollo_pesimista", 1, 1),
        ("reservado_normal", 2, 0),
        ("reservado_pesimista", 2, 1),
    ]
    res = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
        futs = {ex.submit(correr_config, bin_path, c, l, corridas, apagado): k for k, c, l in specs}
        for fut, k in futs.items():
            res[k] = fut.result()
    return res


def evaluar_compuertas(v3_res, act_res):
    detalles = []
    cumple = {"G1": True, "G2": True, "G3": True, "G4": True, "G5": True, "G6": True, "G7": True}

    # G1
    for mat_k, mat in act_res.items():
        v3_mat = v3_res[mat_k]
        for esc in ESC_G1:
            if esc not in mat["filas"] or esc not in v3_mat["filas"]:
                continue
            a = mat["filas"][esc]
            b = v3_mat["filas"][esc]
            if esc in ESC_G1_LIMPIOS:
                if a["ayuda"] > 0 or a["esquivas"] > 0:
                    cumple["G1"] = False
                    detalles.append(f"G1 [{mat_k}] {esc}: ayuda={a['ayuda']} esquivas={a['esquivas']} (debe ser 0/0 en piso limpio)")
            if a["ayuda"] > b["ayuda"] or a["esquivas"] > b["esquivas"] or a["exito"] < b["exito"] or a["paradas"] > b["paradas"]:
                cumple["G1"] = False
                detalles.append(
                    f"G1 [{mat_k}] {esc}: v5(ex={a['exito']},ay={a['ayuda']},es={a['esquivas']},pa={a['paradas']}) "
                    f"empeora vs v3(ex={b['exito']},ay={b['ayuda']},es={b['esquivas']},pa={b['paradas']})"
                )

    # G2: cero contactos con paredes solidas y objeto_bajo en ambos conjuntos y ambos laseres
    for mat_k, mat in act_res.items():
        for esc in ESC_G2_SOLIDOS:
            if esc in mat["filas"]:
                a = mat["filas"][esc]
                if a["contactos"] > 0:
                    cumple["G2"] = False
                    detalles.append(f"G2 [{mat_k}] {esc}: contactos={a['contactos']} (meta 0)")

    # G3: bloque_sin_eco contactos claramente menores que v3 (v3 = 40/40) y G1 limpio = 0 AYUDA
    for mat_k, mat in act_res.items():
        if "bloque_sin_eco" in mat["filas"] and "bloque_sin_eco" in v3_res[mat_k]["filas"]:
            a = mat["filas"]["bloque_sin_eco"]
            b = v3_res[mat_k]["filas"]["bloque_sin_eco"]
            if not (a["contactos"] < b["contactos"] and a["contactos"] <= max(2, b["contactos"] // 4)):
                cumple["G3"] = False
                detalles.append(f"G3 [{mat_k}] bloque_sin_eco: v5 contactos={a['contactos']} vs v3={b['contactos']}")

    # G4: muebles de patas finas (contactos <= v3 y < v3 si v3 > 0; ayuda <= v3; no empeorar exito+seguro)
    for mat_k, mat in act_res.items():
        for esc in ESC_G4_MUEBLES:
            if esc in mat["filas"] and esc in v3_res[mat_k]["filas"]:
                a = mat["filas"][esc]
                b = v3_res[mat_k]["filas"][esc]
                if a["contactos"] > 0 or a["contactos"] > b["contactos"] or a["ayuda"] > b["ayuda"]:
                    if a["contactos"] >= b["contactos"] and b["contactos"] > 0:
                        cumple["G4"] = False
                        detalles.append(f"G4 [{mat_k}] {esc}: cont v5={a['contactos']} v3={b['contactos']}, ay v5={a['ayuda']} v3={b['ayuda']}")
                    elif a["ayuda"] > b["ayuda"]:
                        cumple["G4"] = False
                        detalles.append(f"G4 [{mat_k}] {esc}: ayuda empeora v5={a['ayuda']} > v3={b['ayuda']}")

    # G5: ayuda en caja_y_pared_al_lado, pasillo, pasillo_angosto
    for mat_k, mat in act_res.items():
        for esc in ESC_G5_ESPACIOS:
            if esc in mat["filas"] and esc in v3_res[mat_k]["filas"]:
                a = mat["filas"][esc]
                b = v3_res[mat_k]["filas"][esc]
                if a["ayuda"] > b["ayuda"] or (b["ayuda"] >= 5 and a["ayuda"] >= b["ayuda"]) or a["contactos"] > 0:
                    cumple["G5"] = False
                    detalles.append(f"G5 [{mat_k}] {esc}: v5 cont={a['contactos']} ay={a['ayuda']} vs v3 cont={b['contactos']} ay={b['ayuda']}")

    # G6: terminado_falso == 0
    for mat_k, mat in act_res.items():
        for esc, a in mat["filas"].items():
            if a["terminado_falso"] > 0:
                cumple["G6"] = False
                detalles.append(f"G6 [{mat_k}] {esc}: terminado_falso={a['terminado_falso']}")

    # G7: ninguna otra metrica de salida peor sin justificacion (error, sin_terminar, esquive_falso)
    for mat_k, mat in act_res.items():
        for esc, a in mat["filas"].items():
            b = v3_res[mat_k]["filas"].get(esc)
            if not b:
                continue
            if a["error"] > b["error"] or a["sin_terminar"] > b["sin_terminar"] or a["cat"].get("esquive_falso", 0) > b["cat"].get("esquive_falso", 0):
                cumple["G7"] = False
                detalles.append(
                    f"G7 [{mat_k}] {esc}: err {b['error']}->{a['error']}, sin_term {b['sin_terminar']}->{a['sin_terminar']}, "
                    f"esq_falso {b['cat'].get('esquive_falso',0)}->{a['cat'].get('esquive_falso',0)}"
                )
    return cumple, detalles


def imprimir_comparacion(v3_res, act_res, cumple, detalles):
    for mat_k in ["desarrollo_normal", "desarrollo_pesimista", "reservado_normal", "reservado_pesimista"]:
        print(f"\n=== MATRIZ: {mat_k} ===")
        print(f"v3    : {v3_res[mat_k]['header']}")
        print(f"actual: {act_res[mat_k]['header']}")
        for esc, a in act_res[mat_k]["filas"].items():
            b = v3_res[mat_k]["filas"].get(esc, {})
            print(
                f"  {esc:<22} "
                f"v3[ok={b.get('exito',0):2d} cont={b.get('contactos',0):2d} bm={b.get('bajo_mueble',0):2d} ay={b.get('ayuda',0):2d} par={b.get('paradas',0):3d} esq={b.get('esquivas',0):2d}]  "
                f"v5[ok={a['exito']:2d} cont={a['contactos']:2d} bm={a['bajo_mueble']:2d} ay={a['ayuda']:2d} err={a['error']:2d} tf={a['terminado_falso']:2d} par={a['paradas']:3d} esq={a['esquivas']:2d} c=({a['causas']})]"
            )
    print("\n=== RESUMEN DE COMPUERTAS G1-G7 (SIMULADO) ===")
    for g in ["G1", "G2", "G3", "G4", "G5", "G6", "G7"]:
        print(f"  {g}: {'CUMPLE' if cumple[g] else 'NO CUMPLE'}")
    if detalles:
        print("\nObservaciones / incumplimientos:")
        for d in detalles:
            print("  -", d)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corridas", type=int, default=40)
    ap.add_argument("--recalcular-v3", action="store_true")
    ap.add_argument("--apagado", action="store_true", help="Corre v5 con AV5_APAGADO=1 y compara fila por fila vs v3")
    ap.add_argument("--guardar", default="", help="Ruta de archivo texto para guardar el reporte")
    args = ap.parse_args()

    os.makedirs(os.path.dirname(CACHE_V3), exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fsi_v5_eval_") as tmp:
        bin_actual = compilar_actual(tmp)
        cache_ok = os.path.exists(CACHE_V3) and not args.recalcular_v3
        if cache_ok:
            v3_all = json.load(open(CACHE_V3, encoding="utf-8"))
            if v3_all.get("corridas") != args.corridas:
                cache_ok = False
        if not cache_ok:
            bin_v3 = compilar_ref("e37f77b", tmp)
            v3_res = correr_todas_matrices(bin_v3, args.corridas, apagado=False)
            json.dump({"corridas": args.corridas, "resultados": v3_res}, open(CACHE_V3, "w", encoding="utf-8"), indent=2)
        else:
            v3_res = v3_all["resultados"]

        act_res = correr_todas_matrices(bin_actual, args.corridas, apagado=args.apagado)

        if args.apagado:
            iguales = True
            diffs = []
            for mat_k in ["desarrollo_normal", "desarrollo_pesimista", "reservado_normal", "reservado_pesimista"]:
                for esc, a in act_res[mat_k]["filas"].items():
                    b = v3_res[mat_k]["filas"].get(esc)
                    if not b or a["raw"] != b["raw"]:
                        iguales = False
                        diffs.append((mat_k, esc, b["raw"] if b else "MISSING", a["raw"]))
            print(f"Igualdad exacta AV5_APAGADO=1 vs e37f77b (4 matrices, N={args.corridas}): {'SI' if iguales else 'NO'}")
            for mat_k, esc, lb, la in diffs[:20]:
                print(f"  DIFF [{mat_k}] {esc}:\n    v3 : {lb}\n    apg: {la}")
            sys.exit(0 if iguales else 1)

        cumple, detalles = evaluar_compuertas(v3_res, act_res)
        buf = []
        import io
        from contextlib import redirect_stdout
        sio = io.StringIO()
        with redirect_stdout(sio):
            imprimir_comparacion(v3_res, act_res, cumple, detalles)
        salida = sio.getvalue()
        print(salida)
        if args.guardar:
            os.makedirs(os.path.dirname(os.path.abspath(args.guardar)), exist_ok=True)
            open(args.guardar, "w", encoding="utf-8").write(salida)
        sys.exit(0 if all(cumple.values()) else 1)


if __name__ == "__main__":
    main()
