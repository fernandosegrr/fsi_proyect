"""Banco pareado de v4. Ejecutar desde PC. Código 1 indica compuertas rojas."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import subprocess
import tempfile

import compuertas_autonomo as banco


def evaluar(tarea):
    directorio, conjunto, perfil, indice, n, experimento = tarea
    entorno = {k: v for k, v in os.environ.items() if not k.startswith(("AV2_", "AV3_", "AV4_"))}
    entorno.update(AV4_CONJUNTO=conjunto, AV4_PESIMISTA=str(int(perfil == "pesimista")))
    if experimento:
        entorno["AV4_EXPERIMENTO"] = "1"
    r = subprocess.run([str(directorio / "banco.exe"), str(n), str(indice)], env=entorno,
                       capture_output=True, check=True, encoding="utf-8")
    linea, = [l for l in r.stdout.splitlines() if l.startswith("AV2 ")]
    d = banco.campos(linea)
    if set(d["cat"]) != set(banco.CATEGORIAS) or sum(d["cat"].values()) != n or d["cat"]["ok"] != d["exito"]:
        raise ValueError(f"Contabilidad inválida: {linea}")
    return conjunto, perfil, directorio.name, linea, d


def compuertas(filas, candidato):
    fallos = []
    for (conjunto, perfil, escena), versiones in filas.items():
        a = versiones[candidato]
        motivos = []
        for k in ("contactos", "terminado_falso", "sin_terminar", "error"):
            if a[k]:
                motivos.append(f"{k}={a[k]}")
        if a["tipo"] == "falso" and (a["ayuda"] or a["esquivas"]):
            motivos.append(f"FP ayudas={a['ayuda']} esquives={a['esquivas']}")
        if a["tipo"] == "imposible" and a["exito"] != a["n"]:
            motivos.append("paso imposible sin parada segura")
        for ref in ("v11", "v3"):
            b = versiones[ref]
            claves = ["contactos", "bajo_mueble", "error", "sin_terminar", "terminado_falso", "sin_piso",
                      "paradas", "corridas_con_parada", "t_media", "t_p95", "lat_media", "lat_p95",
                      "lon_media", "lon_p95", "desvio_p95"]
            if a["tipo"] != "imposible":
                claves.append("ayuda")
            if a["tipo"] == "falso":
                claves.append("esquivas")
            for k in claves:
                if a[k] > b[k]:
                    motivos.append(f"{k} {a[k]}>{ref} {b[k]}")
            if a["exito"] < b["exito"]:
                motivos.append(f"ok {a['exito']}<{ref} {b['exito']}")
        if escena in ("banquito35x45", "silla_patas"):
            for k in ("contactos", "ayuda"):
                if a[k] >= versiones["v3"][k] and a[k] > 0:
                    motivos.append(f"sin reducción de {k}")
        if escena == "caja_y_pared_al_lado" and a["ayuda"] and a["ayuda"] >= versiones["v3"]["ayuda"]:
            motivos.append("sin reducción de AYUDA")
        if a["tipo"] == "falso" and a["paradas"] and a["paradas"] >= versiones["v3"]["paradas"]:
            motivos.append("sin reducción de paradas")
        if motivos:
            fallos.append(f"{conjunto} {perfil} {escena}: " + ", ".join(motivos))
    return fallos


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--salida", type=Path, required=True)
    p.add_argument("--corridas", type=int, default=40, choices=range(10, 5001), metavar="10..5000")
    p.add_argument("--conjunto", choices=("desarrollo", "reservado", "ambos"), default="ambos")
    p.add_argument("--experimento", action="store_true")
    p.add_argument("--procesos", type=int, default=6)
    args = p.parse_args()
    conjuntos = ("desarrollo", "reservado") if args.conjunto == "ambos" else (args.conjunto,)
    versiones = [("v11", "bd1727b", ["-DAV2_BASE_V11", "-DSIM_SIN_FUENTES"]),
                 ("v3", "5ac174e", []), ("actual", None, [])]
    filas = {}
    args.salida.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fsi-v4-") as tmp, args.salida.open("w", encoding="utf-8", newline="\n") as out:
        tareas = []
        for nombre, ref, flags in versiones:
            dest = Path(tmp) / nombre
            banco.exportar(dest, ref)
            banco.compilar(dest, flags)
            for conjunto in conjuntos:
                for perfil in ("normal", "pesimista"):
                    for indice in range(26 if conjunto == "reservado" else 23):
                        tareas.append((dest, conjunto, perfil, indice, args.corridas, nombre == "actual" and args.experimento))
        print(f"SIMULADO n={args.corridas}/escena experimento={int(args.experimento)}", file=out)
        with ThreadPoolExecutor(max_workers=args.procesos) as pool:
            for conjunto, perfil, nombre, linea, d in pool.map(evaluar, tareas):
                print(conjunto, perfil, nombre, linea, file=out)
                filas.setdefault((conjunto, perfil, d["escenario"]), {})[nombre] = d
        fallos = compuertas(filas, "actual")
        for fallo in fallos:
            print("FALLO", fallo, file=out)
        resumen = f"{len(tareas)*args.corridas} corridas. {len(fallos)}/{len(filas)} combinaciones con alguna compuerta roja."
        print(resumen, file=out)
        print(resumen)
    return int(bool(fallos))


if __name__ == "__main__":
    raise SystemExit(main())
