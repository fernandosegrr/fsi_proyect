"""Monte Carlo pareado de esquive contra v1.1. Solo PC; no conecta con el carro.

Exporta exclusivamente fuentes enumeradas, compila en temporal y conserva texto.
No lee secretos.h, no cambia ramas y no genera binarios de firmware.
Uso: python scripts/comparar_esquives.py --corridas 1000
"""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = "bd1727b"
CORE = ["pruebas_precision_j16.c", "sim_ruta.c", "sim_ruta.h", "ruta_editor.c", "ruta_editor.h",
        "simulador_navegador.c", "simulador_navegador.h", "navegador.c", "navegador.h",
        "trayectoria.c", "trayectoria.h", "obstaculos.c", "obstaculos.h", "mascara_vision.c",
        "mascara_vision.h", "calibracion.c", "calibracion.h"]
FW = ["rumbo.c", "rumbo.h", "config.h", "secretos.ejemplo.h"]  # solo plantilla publica


def exportar(destino, referencia):
    for carpeta, nombres in [("nucleo_c", CORE), ("firmware_carro", FW)]:
        (destino / carpeta).mkdir(parents=True)
        for nombre in nombres:
            ruta = f"{carpeta}/{nombre}"
            if referencia:
                datos = subprocess.run(["git", "show", f"{referencia}:{ruta}"], cwd=ROOT,
                                       capture_output=True, check=True).stdout
                (destino / ruta).write_bytes(datos)
            else:
                shutil.copyfile(ROOT / ruta, destino / ruta)
    if referencia:
        # Mismo mundo: fondo valido a 1200 mm. v1.1 devolvia 0 cuando el rayo
        # no tocaba una caja; no tocar el algoritmo ni constantes de navegacion.
        banco = destino / "nucleo_c/pruebas_precision_j16.c"
        texto = banco.read_text(encoding="utf-8")
        a = texto.index("static int laser(")
        b = texto.index("\n}", a) + 2
        funcion = texto[a:b].replace("return 0;", "return 1200;")
        banco.write_text(texto[:a] + funcion + texto[b:], encoding="utf-8")


def ejecutar(destino, corridas):
    fuentes = [n for n in CORE if n.endswith(".c")]
    comando = ["gcc", "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
               "-I../firmware_carro", "-o", "precision.exe"] + fuentes + ["../firmware_carro/rumbo.c"]
    subprocess.run(comando, cwd=destino / "nucleo_c", check=True)
    resultado = subprocess.run([str(destino / "nucleo_c/precision.exe"), str(corridas)],
                               cwd=destino / "nucleo_c", capture_output=True, check=True)
    return resultado.stdout.decode("utf-8").replace("\r\n", "\n")


def filas(texto):
    patron = (r'^J16 escenario=(esquive_20x20|esquive_ancha) config="(DESPUES[^"]+)" n=(\d+) '
              r'error_mm media=(\d+) p50=\d+ p95=(\d+).*?llegan=(\d+) toques=(\d+)')
    return {(m[0], m[1]): tuple(map(int, m[2:])) for m in re.findall(patron, texto, re.M)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corridas", type=int, default=1000, choices=range(20, 5001), metavar="20..5000")
    args = parser.parse_args()
    salida = ROOT / "docs/precision"
    with tempfile.TemporaryDirectory(prefix="fsi-mc-autonomia-") as temporal:
        raiz = Path(temporal)
        for nombre, ref in [("v11", BASE), ("actual", None)]:
            print(f"Compilando y simulando {nombre}: {args.corridas} semillas por configuracion", flush=True)
            destino = raiz / nombre
            exportar(destino, ref)
            texto = ejecutar(destino, args.corridas)
            (salida / f"esquive_autonomia_mc_{nombre}.txt").write_text(texto, encoding="utf-8", newline="\n")
    antes = filas((salida / "esquive_autonomia_mc_v11.txt").read_text(encoding="utf-8"))
    despues = filas((salida / "esquive_autonomia_mc_actual.txt").read_text(encoding="utf-8"))
    if len(antes) != 6 or antes.keys() != despues.keys():
        raise RuntimeError("Faltan filas de los dos escenarios y tres configuraciones actuales")
    lineas = ["SIMULADO / NO MEDIDO. Referencia bd1727b; fondo pareado a 1200 mm.",
              "Semillas: 160000 + indice_escenario * 10007 + corrida (indices 8 y 9).",
              "Criterio estricto: media/p95/contactos no suben; llegadas no bajan. Sin tolerancia."]
    fallos = 0
    for clave, a in antes.items():
        d = despues[clave]
        ok = a[0] == d[0] == args.corridas and d[1] <= a[1] and d[2] <= a[2] and d[3] >= a[3] and d[4] <= a[4]
        fallos += not ok
        lineas.append(f"{'OK' if ok else 'FALLO'} {clave}: media/p95 {a[1]}/{a[2]} -> {d[1]}/{d[2]} mm; llegadas {a[3]} -> {d[3]}; contactos {a[4]} -> {d[4]}")
    lineas.append(f"6 comparaciones, {fallos} fallos")
    texto = "\n".join(lineas) + "\n"
    (salida / "esquive_autonomia_comparacion.txt").write_text(texto, encoding="utf-8", newline="\n")
    print(texto)
    return 1 if fallos else 0


if __name__ == "__main__":
    raise SystemExit(main())
