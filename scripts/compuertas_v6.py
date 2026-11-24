#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Compuertas G1-G7 de v6 (rama feat/autonomo-v6).  SIMULADO / NO MEDIDO EN EL CARRO.

Compila TRES nucleos con EXACTAMENTE el mismo banco (pruebas_autonomo_v2.c + mundo_v2 + simulador_navegador del arbol de trabajo):
  v3     = commit e37f77b                  (la base segura)
  v5fix  = etiqueta freeze/v5-carbon-fix   (la referencia inmediata)
  v6     = el arbol de trabajo con AV6_BITS=master (el paquete v6 tal como lo enciende la app)
y los corre en las MISMAS semillas, en las matrices
  DES  (24 escenarios, semillas 2100000+...)     RES  (27 escenarios, semillas 4300000+...)
  FDES (familia sin eco, 12 escenarios, 6100000+...)   FRES (familia sin eco, 8300000+...)
por ruido perceptivo (0 = el banco de siempre; 1 = calibrado con las cajas negras; 2 = esfuerzo, NO calibrado) y por laser (0 normal, 1 pesimista).

Uso:
  python scripts/compuertas_v6.py                          # 120 corridas, ruido 0 y 1, laser 0 y 1 (unos 15 min)
  python scripts/compuertas_v6.py --corridas 60 --ruido 0,1,2 --guardar docs/precision/v6/compuertas.txt
  python scripts/compuertas_v6.py --apagado                # v6 apagado == v5-fix exacto y v5 apagado == v3 exacto (compara linea por linea)

Veredicto por compuerta y por matriz: CUMPLE / AVISO (falla segura, con aviso) / NO CUMPLE.  El criterio es ESTRICTO (una sola corrida de mas cuenta);
las diferencias que caben en el ruido estadistico se explican aparte en docs/autonomo_v6_diseno.md y en el informe, nunca se esconden.
"""
import argparse
import concurrent.futures
import io
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from contextlib import redirect_stdout

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NUCLEO = os.path.join(ROOT, "nucleo_c")
ARNES = ["pruebas_autonomo_v2.c", "mundo_v2.c", "mundo_v2.h", "simulador_navegador.c", "simulador_navegador.h"]
CORE = ["pruebas_autonomo_v2.c", "mundo_v2.c", "fusion.c", "corredor.c", "simulador_navegador.c", "navegador.c", "trayectoria.c",
        "obstaculos.c", "mascara_vision.c", "calibracion.c"]
REFS = {"v3": "e37f77b", "v5fix": "freeze/v5-carbon-fix"}

G1 = ["libre_limpio", "falso_juntas", "falso_exposicion", "falso_sol", "falso_sombra", "falso_reflejo", "falso_detector", "pared_tras_el_final"]
G1_FAMILIA = ["se_piso_manchas", "se_reflejo_saturado"]          # pisos con manchas y reflejos saturados: tambien son falsos positivos
ESFUERZO = ["se_charco_oscuro"]                                  # mancha oscura DENTRO del carril: indistinguible de un objeto oscuro de 3 cm sin eco (se informa, no es compuerta)
G2_NUCLEO = ["caja20", "objeto_largo90", "dos_obstaculos", "pared_al_frente"]
G2_AMPLIADO = ["caja20_lateral", "aparece_de_repente", "objeto_bajo", "caja20_con_sol", "bateria_baja", "caja_antes_de_esquina", "bajo_solo_color"]
G3 = ["bloque_sin_eco", "se_negro_mate", "se_pequeno_borde", "se_alto_con_sombra", "se_bajo_oscuro", "se_rayado", "se_piso_oscuro", "se_piso_claro"]
LIMITES = ["se_transparente", "se_poco_contraste"]               # ni laser ni color los ven: no cuentan
G4 = ["banquito35x45", "silla_patas", "dos_muebles", "mueble_con_pared"]
G5 = ["caja_y_pared_al_lado", "pasillo", "pasillo_angosto"]

CAMPOS = ["exito", "contactos", "ayuda", "error", "terminado_falso", "sin_terminar", "llegan", "esquivas", "paradas", "corridas_con_parada",
          "sin_piso", "bajo_mueble", "vistazos", "vistazos_con_eco"]


def ext():
    return ".exe" if os.name == "nt" else ""


def compilar(nombre, referencia, carpeta):
    """referencia = None -> arbol de trabajo (con -Werror); si no, commit o etiqueta (con el banco del arbol de trabajo copiado encima)."""
    salida = os.path.join(carpeta, nombre + "_bench" + ext())
    if referencia is None:
        nu, extra = NUCLEO, ["-Werror"]
    else:
        src = os.path.join(carpeta, "src_" + nombre)
        os.makedirs(src, exist_ok=True)
        datos = subprocess.run(["git", "-C", ROOT, "archive", referencia, "--format=tar", "nucleo_c"], capture_output=True, check=True).stdout
        tar_ruta = os.path.join(carpeta, nombre + ".tar")
        with open(tar_ruta, "wb") as f:
            f.write(datos)
        with tarfile.open(tar_ruta) as tf:          # tarfile y no el comando tar: GNU tar se confunde con rutas de Windows tipo C:\...
            tf.extractall(src)
        nu = os.path.join(src, "nucleo_c")
        for f in ARNES:
            shutil.copyfile(os.path.join(NUCLEO, f), os.path.join(nu, f))
        extra = []
    cmd = ["gcc", "-std=c99", "-Wall", "-Wextra", "-Wpedantic"] + extra + ["-O2", "-I", nu, "-o", salida] + [os.path.join(nu, s) for s in CORE] + ["-lm"]
    subprocess.run(cmd, check=True)
    return salida


def parsear(linea):
    m = re.search(r"escenario=(\S+)\s+tipo=(\S+)\s+n=(\d+)", linea)
    if not linea.startswith("AV2 ") or not m:
        return None
    d = {"esc": m.group(1), "tipo": m.group(2), "n": int(m.group(3)), "raw": linea.strip()}
    for c in CAMPOS:
        mm = re.search(r"\b%s=(\d+)" % c, linea)
        d[c] = int(mm.group(1)) if mm else 0
    mc = re.search(r"\|\s*cat\s+(.*)$", linea)
    d["cat"] = {}
    if mc:
        for par in mc.group(1).split():
            if "=" in par:
                k, v = par.split("=", 1)
                d["cat"][k] = int(v)
    d["esqf"] = d["cat"].get("esquive_falso", 0)
    return d


def correr(exe, matriz, ruido, laser, n, extra_env=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("AV")}
    if matriz == "DES":
        env["AV5_CONJUNTO"] = "1"
    elif matriz == "RES":
        env["AV5_CONJUNTO"] = "2"
    elif matriz == "FDES":
        env["AV6_FAMILIA"] = "1"
    elif matriz == "FRES":
        env["AV6_FAMILIA"] = "2"
    env["AV6_RUIDO"] = str(ruido)
    env["AV5_LASER"] = str(laser)
    env.update(extra_env or {})
    p = subprocess.run([exe, str(n)], env=env, capture_output=True, text=True, encoding="utf-8", errors="replace")
    filas = {}
    for linea in p.stdout.splitlines():
        d = parsear(linea)
        if d:
            filas[d["esc"]] = d
    return filas


def suma(filas, nombres, campo):
    return sum(filas[e][campo] for e in nombres if e in filas)


# ── veredictos ────────────────────────────────────────────────────────────────

def comparar_g1(r, ref, etiqueta, nombres, notas):
    """Estricto: en cada escenario de piso limpio v6 no puede tener menos exitos ni mas AYUDA, esquives o paradas que la referencia, ni contactos."""
    mal = 0
    for e in nombres:
        if e not in r or e not in ref:
            continue
        a, b = r[e], ref[e]
        peor = []
        if a["exito"] < b["exito"]:
            peor.append("exito %d<%d" % (a["exito"], b["exito"]))
        for c in ("ayuda", "esquivas", "paradas"):
            if a[c] > b[c]:
                peor.append("%s %d>%d" % (c, a[c], b[c]))
        if a["contactos"] > 0:
            peor.append("contactos %d" % a["contactos"])
        if peor:
            mal += 1
            notas.append("  G1 vs %s  %-22s %s" % (etiqueta, e, "; ".join(peor)))
    return mal


def veredictos(mat, tit, V, notas):
    """Devuelve {G: 'CUMPLE'|'AVISO'|'NO CUMPLE'|'NO MEDIDO'} para una matriz. V = {'v3': filas, 'v5fix': filas, 'v6': filas}."""
    v3, v5, v6 = V["v3"], V["v5fix"], V["v6"]
    out = {}
    esc = set(v6)
    # G1
    nombres = [e for e in G1 + G1_FAMILIA if e in esc]
    if nombres:
        n0 = len(notas)
        comparar_g1(v6, v3, "v3   ", nombres, notas)
        comparar_g1(v6, v5, "v5fix", nombres, notas)
        out["G1"] = "CUMPLE" if len(notas) == n0 else "NO CUMPLE"
    # G2
    nombres = [e for e in G2_NUCLEO if e in esc]
    if nombres:
        malos = [e for e in nombres if v6[e]["contactos"] > 0]
        peor_v3 = [e for e in nombres + [x for x in G2_AMPLIADO if x in esc] if v6[e]["contactos"] > v3[e]["contactos"]]
        for e in malos:
            notas.append("  G2 %-22s contactos v3=%d v5fix=%d v6=%d (meta 0)" % (e, v3[e]["contactos"], v5[e]["contactos"], v6[e]["contactos"]))
        for e in peor_v3:
            notas.append("  G2 %-22s v6 PEOR que v3: contactos v3=%d v6=%d" % (e, v3[e]["contactos"], v6[e]["contactos"]))
        out["G2"] = "CUMPLE" if not malos and not peor_v3 else ("AVISO" if not peor_v3 and sum(v6[e]["contactos"] for e in malos) <= max(1, sum(v3[e]["contactos"] for e in malos) // 4) else "NO CUMPLE")
    # G3
    nombres = [e for e in G3 if e in esc]
    if nombres:
        c3, c5, c6 = suma(v3, nombres, "contactos"), suma(v5, nombres, "contactos"), suma(v6, nombres, "contactos")
        claro = c6 * 2 <= c3 or c3 == 0
        no_peor = c6 <= c5
        if not claro:
            notas.append("  G3 contactos v6=%d no son claramente menores que v3=%d" % (c6, c3))
        if not no_peor:
            notas.append("  G3 contactos v6=%d peores que v5fix=%d (v3=%d)" % (c6, c5, c3))
            for e in nombres:
                if v6[e]["contactos"] > v5[e]["contactos"]:
                    notas.append("      %-22s v3=%d v5fix=%d v6=%d" % (e, v3[e]["contactos"], v5[e]["contactos"], v6[e]["contactos"]))
        out["G3"] = "CUMPLE" if claro and no_peor else ("AVISO" if claro else "NO CUMPLE")
    # G4
    nombres = [e for e in G4 if e in esc]
    if nombres:
        estado = "CUMPLE"
        for e in nombres:
            a, b = v6[e], v3[e]
            menos = a["contactos"] < b["contactos"] or (a["contactos"] == 0 and b["contactos"] == 0)
            ayuda_ok = a["ayuda"] <= b["ayuda"]
            if not menos:
                estado = "NO CUMPLE"
                notas.append("  G4 %-22s contactos v6=%d no menores que v3=%d" % (e, a["contactos"], b["contactos"]))
            elif not ayuda_ok:
                if estado == "CUMPLE":
                    estado = "AVISO"
                notas.append("  G4 %-22s falla SEGURA: contactos v6=%d < v3=%d pero AYUDA v6=%d > v3=%d" % (e, a["contactos"], b["contactos"], a["ayuda"], b["ayuda"]))
        out["G4"] = estado
    # G5
    nombres = [e for e in G5 if e in esc]
    if nombres:
        estado = "CUMPLE"
        for e in nombres:
            a, b = v6[e], v3[e]
            if a["ayuda"] > b["ayuda"] or (b["ayuda"] > 0 and a["ayuda"] == b["ayuda"]):
                estado = "NO CUMPLE"
                notas.append("  G5 %-22s AYUDA v6=%d no es menor que v3=%d (v5fix=%d)" % (e, a["ayuda"], b["ayuda"], v5[e]["ayuda"]))
            if a["contactos"] > b["contactos"]:
                estado = "NO CUMPLE"
                notas.append("  G5 %-22s contactos v6=%d > v3=%d" % (e, a["contactos"], b["contactos"]))
        out["G5"] = estado
    # G6
    tf = [e for e in esc if v6[e]["terminado_falso"] > 0]
    for e in tf:
        notas.append("  G6 %-22s terminado_falso v3=%d v5fix=%d v6=%d" % (e, v3[e]["terminado_falso"], v5[e]["terminado_falso"], v6[e]["terminado_falso"]))
    out["G6"] = "CUMPLE" if not tf else "NO CUMPLE"
    # G7: nada peor que v5fix sin justificacion (en los escenarios que cuentan; los limites fisicos no)
    estado = "CUMPLE"
    for e in sorted(esc):
        if e in LIMITES or e in ESFUERZO:
            continue
        a, b = v6[e], v5[e]
        peor = []
        for c in ("contactos", "error", "sin_terminar", "terminado_falso", "ayuda", "paradas", "esquivas"):
            if a[c] > b[c]:
                peor.append("%s %d>%d" % (c, a[c], b[c]))
        if a["esqf"] > b["esqf"]:
            peor.append("esquive_falso %d>%d" % (a["esqf"], b["esqf"]))
        if a["exito"] < b["exito"]:
            peor.append("exito %d<%d" % (a["exito"], b["exito"]))
        if peor:
            graves = [p for p in peor if p.split()[0] in ("contactos", "error", "sin_terminar", "terminado_falso", "esquive_falso", "exito")]
            if graves:
                estado = "NO CUMPLE"
            elif estado == "CUMPLE":
                estado = "AVISO"
            notas.append("  G7 %-22s v6 vs v5fix: %s" % (e, "; ".join(peor)))
    out["G7"] = estado
    return out


def fila(d):
    if not d:
        return "%-37s" % "(sin dato)"
    return "ok=%3d c=%3d ay=%3d e=%3d p=%3d er=%d tf=%d" % (d["exito"], d["contactos"], d["ayuda"], d["esquivas"], d["paradas"], d["error"], d["terminado_falso"])


def reporte(todo, args):
    resumen = {}
    for (mat, ruido, laser), V in todo.items():
        tit = "%s ruido=%d laser=%s" % (mat, ruido, "pesimista" if laser else "normal")
        print("\n=== MATRIZ %s  (corridas por escenario: %d) ===" % (tit, args.corridas))
        print("%-22s | %-37s | %-37s | %-37s" % ("escenario", "v3", "v5-fix", "v6"))
        for e in V["v6"]:
            print("%-22s | %s | %s | %s" % (e, fila(V["v3"].get(e)), fila(V["v5fix"].get(e)), fila(V["v6"][e])))
        for nombre, lista in (("G1 piso limpio y falsos", G1 + G1_FAMILIA), ("G2 solidos (nucleo)", G2_NUCLEO), ("G3 sin eco", G3), ("G4 muebles", G4), ("G5 espacios", G5)):
            presentes = [x for x in lista if x in V["v6"]]
            if presentes:
                print("  suma %-24s v3[ok=%4d c=%4d ay=%4d e=%4d p=%4d]  v5fix[ok=%4d c=%4d ay=%4d e=%4d p=%4d]  v6[ok=%4d c=%4d ay=%4d e=%4d p=%4d]" % (
                    nombre,
                    suma(V["v3"], presentes, "exito"), suma(V["v3"], presentes, "contactos"), suma(V["v3"], presentes, "ayuda"), suma(V["v3"], presentes, "esquivas"), suma(V["v3"], presentes, "paradas"),
                    suma(V["v5fix"], presentes, "exito"), suma(V["v5fix"], presentes, "contactos"), suma(V["v5fix"], presentes, "ayuda"), suma(V["v5fix"], presentes, "esquivas"), suma(V["v5fix"], presentes, "paradas"),
                    suma(V["v6"], presentes, "exito"), suma(V["v6"], presentes, "contactos"), suma(V["v6"], presentes, "ayuda"), suma(V["v6"], presentes, "esquivas"), suma(V["v6"], presentes, "paradas")))
        solo_info = [x for x in ESFUERZO + LIMITES if x in V["v6"]]
        if solo_info:
            print("  (solo informativos, no son compuerta: %s)" % ", ".join(solo_info))
        notas = []
        v = veredictos(mat, tit, V, notas)
        resumen[(mat, ruido, laser)] = v
        print("  veredicto: " + "  ".join("%s=%s" % (g, v[g]) for g in sorted(v)))
        for n in notas:
            print(n)
    print("\n=== RESUMEN DE COMPUERTAS (SIMULADO, criterio estricto, corridas=%d) ===" % args.corridas)
    gates = ["G1", "G2", "G3", "G4", "G5", "G6", "G7"]
    print("%-28s %s" % ("matriz", " ".join("%-9s" % g for g in gates)))
    for k, v in resumen.items():
        print("%-28s %s" % ("%s r%d L%d" % k, " ".join("%-9s" % v.get(g, "-") for g in gates)))
    print("%-28s %s" % ("AGREGADO (peor caso)", " ".join("%-9s" % peor_caso([v.get(g) for v in resumen.values()]) for g in gates)))
    return resumen


def peor_caso(lista):
    lista = [x for x in lista if x]
    if not lista:
        return "NO MEDIDO"
    for x in ("NO CUMPLE", "AVISO"):
        if x in lista:
            return x
    return "CUMPLE"


def modo_apagado(args, exes):
    """v6 apagado debe ser v5-fix EXACTO y v5 apagado debe ser v3 EXACTO: se comparan las lineas del banco una por una."""
    mats = []
    for mat in ("DES", "RES", "FDES", "FRES"):
        for ruido in [int(x) for x in args.ruido.split(",")]:
            for laser in (0, 1):
                mats.append((mat, ruido, laser))
    total = bien = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as ex:
        tareas = []
        for m in mats:
            tareas.append((m, "v6 apagado vs v5-fix", ex.submit(correr, exes["actual"], m[0], m[1], m[2], args.corridas), ex.submit(correr, exes["v5fix"], m[0], m[1], m[2], args.corridas)))
            tareas.append((m, "v5 apagado vs v3   ", ex.submit(correr, exes["actual"], m[0], m[1], m[2], args.corridas, {"AV5_APAGADO": "1"}), ex.submit(correr, exes["v3"], m[0], m[1], m[2], args.corridas)))
        for m, que, fa, fb in tareas:
            A, B = fa.result(), fb.result()
            iguales = bool(A) and A.keys() == B.keys() and all(A[e]["raw"] == B[e]["raw"] for e in A)
            total += 1
            bien += 1 if iguales else 0
            print("%s  %-5s ruido=%d laser=%d  %s (%d escenarios)" % (que, m[0], m[1], m[2], "IDENTICO" if iguales else "DIFIERE", len(A)))
            if not iguales:
                for e in A:
                    if e not in B or A[e]["raw"] != B[e]["raw"]:
                        print("    A: %s\n    B: %s" % (A[e]["raw"][:240], B[e]["raw"][:240] if e in B else "(falta)"))
                        break
    print("RESULTADO: %d de %d comparaciones identicas" % (bien, total))
    return 0 if bien == total else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--corridas", type=int, default=120)
    ap.add_argument("--ruido", default="0,1", help="niveles de ruido perceptivo a correr (0,1 o 0,1,2)")
    ap.add_argument("--laser", default="0,1", help="0 normal, 1 pesimista")
    ap.add_argument("--matrices", default="DES,RES,FDES,FRES")
    ap.add_argument("--workers", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--apagado", action="store_true", help="comprueba v6 apagado == v5-fix y v5 apagado == v3 (linea por linea)")
    ap.add_argument("--guardar", default="", help="archivo de texto donde guardar el reporte")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory(prefix="fsi_v6_") as tmp:
        exes = {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as ex:
            fut = {"actual": ex.submit(compilar, "actual", None, tmp), "v3": ex.submit(compilar, "v3", REFS["v3"], tmp), "v5fix": ex.submit(compilar, "v5fix", REFS["v5fix"], tmp)}
            for k, f in fut.items():
                exes[k] = f.result()
        if args.apagado:
            sys.exit(modo_apagado(args, exes))
        mats = [(m, int(r), int(l)) for m in args.matrices.split(",") for r in args.ruido.split(",") for l in args.laser.split(",")]
        todo = {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as ex:
            futs = {}
            for m in mats:
                futs[m] = {"v3": ex.submit(correr, exes["v3"], m[0], m[1], m[2], args.corridas),
                           "v5fix": ex.submit(correr, exes["v5fix"], m[0], m[1], m[2], args.corridas),
                           "v6": ex.submit(correr, exes["actual"], m[0], m[1], m[2], args.corridas, {"AV6_BITS": "master"})}
            for m, d in futs.items():
                todo[m] = {k: f.result() for k, f in d.items()}
        buf = io.StringIO()
        with redirect_stdout(buf):
            resumen = reporte(todo, args)
        texto = buf.getvalue()
        print(texto)
        if args.guardar:
            os.makedirs(os.path.dirname(os.path.abspath(args.guardar)), exist_ok=True)
            with open(args.guardar, "w", encoding="utf-8") as f:
                f.write(texto)
        malos = any(x == "NO CUMPLE" for v in resumen.values() for x in v.values())
        sys.exit(1 if malos else 0)


if __name__ == "__main__":
    main()
