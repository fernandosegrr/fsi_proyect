#!/usr/bin/env python3
"""
frenado.py - Distancia de frenado del carro FSI con el freno local del laser VL53L0X.

Solo biblioteca estandar. Reproduce las tablas de docs/investigacion_autonomo/10_seguridad_funcional.md
(seccion 2.x). Ejecutar:  python frenado.py        (determinista; la parte Monte Carlo usa semilla fija)

QUE ES MEDIDO Y QUE NO
  MEDIDO en el carro (docs/calibracion.md, docs/latencia.md, 13-14 oct 2026):
      velocidad 38 cm/s (PWM 255) y 19.3 cm/s (PWM 210/207); "frenado" 0.9 cm y 1.4 cm.
  TODO LO DEMAS es SUPUESTO / SIMULADO: la edad de la medida, la fase del reloj del sensor, la distancia
  sensor-frente del chasis, el sesgo del sensor, la incertidumbre del "frenado" y la tasa de lecturas invalidas.
  Nada de lo que imprime este script se midio como "distancia de parada del freno laser" (el manual lo declara
  NO MEDIDO: docs/manual/capitulos/12_pruebas.md, fila "Freno autonomo aislado por laser").

MODELO (distancias en cm, tiempos en ms)
  Sea t_cruce el instante en que la parte delantera del sensor cruza la distancia TOF_PARO_MM (la verdadera).
  El firmware corta los motores en t_corte = t_cruce + t_sense + t_lazo. Despues el carro aun recorre d_frenado.
      t_sense = a + (N-1)*T + U[0,T) + p        a: edad efectiva de la medida (0..T), T: periodo (33 ms),
                                                 N: TOF_PARO_LECTURAS (2), p: retraso de sondeo (0..TOF_SONDEO_MS=4)
      distancia_total = v * (t_sense + t_lazo) + d_frenado
      holgura = TOF_PARO_MM/10 - sesgo - distancia_total - e
        e: distancia del sensor al punto mas delantero del carro (>0 si el sensor esta "metido")
        sesgo: cuanto de mas "ve de lejos" el sensor (error de rango en la direccion peligrosa)
"""
import random

# ---------------- constantes del proyecto (config.h) ----------------
T_MS = 33.0            # TOF_PERIODO_MS
SONDEO_MS = 4.0        # TOF_SONDEO_MS
N_LECT = 2             # TOF_PARO_LECTURAS
PARO_MM = 120.0        # TOF_PARO_MM
BLOQUEO_MM = 150.0     # TOF_BLOQUEO_MM
LOOP_DISENO_US = 5000  # LOOP_MAX_US
WATCHDOG_MS = 1000.0   # TIEMPO_WATCHDOG_MS
LATIDO_MS = 250.0      # heartbeat de la app y de pagina.h

# ---------------- medidas del proyecto (docs/calibracion.md, docs/latencia.md) ----------------
PERFILES = {
    # nombre: (v cm/s MEDIDA, frenado cm MEDIDO)
    "AUTO 210/207":    (19.3, 1.4),
    "MANUAL 255/252":  (38.0, 0.9),
}

# ---------------- escenarios de latencia (SUPUESTOS salvo donde se indica) ----------------
# a = edad efectiva de la medida; fase = fraccion de T que espera hasta que termina la medida k (0..1)
ESCENARIOS = {
    #  nombre            a_ms       fase  sondeo_ms  lazo_ms   nota
    "optimista":     (0.0,        0.0,   0.0,       0.7,   "medida sin edad, fase favorable; lazo = promedio medido 0.69 ms (manual 12_pruebas)"),
    "central":       (T_MS / 2,   0.5,   2.0,       1.0,   "edad media T/2, fase media, sondeo medio, lazo ~1 ms"),
    "conservador":   (T_MS,       1.0,   SONDEO_MS, LOOP_DISENO_US / 1000.0, "edad = T, fase peor, sondeo 4 ms, lazo = LOOP_MAX_US (5 ms)"),
    "peor medido":   (T_MS,       1.0,   SONDEO_MS, 41.1,  "como conservador pero lazo = maximo visto 41.1 ms (manual 12_pruebas, corrida 1)"),
}


def t_sense_ms(a, fase, sondeo):
    return a + (N_LECT - 1) * T_MS + fase * T_MS + sondeo


def distancia_total_cm(v, frenado, esc, escalar_frenado_v2=None):
    a, fase, sondeo, lazo, _ = ESCENARIOS[esc]
    t = t_sense_ms(a, fase, sondeo) + lazo
    d_fren = frenado
    if escalar_frenado_v2 is not None:
        # INFERENCIA: deceleracion constante => d = v^2/(2a). Se calibra con el perfil AUTO.
        v_ref, f_ref = escalar_frenado_v2
        d_fren = f_ref * (v / v_ref) ** 2
    return v * t / 1000.0 + d_fren, t, d_fren


def tabla_basica():
    print("=== 1. Distancia total desde que el objeto cruza TOF_PARO_MM hasta que el carro queda quieto ===")
    print("    (frenado = el MEDIDO del proyecto; ver tabla 2 para el escalado por v^2)")
    print(f"{'perfil':16s} {'escenario':12s} {'t_sense+lazo ms':>16s} {'v*t cm':>8s} {'frenado cm':>11s} {'TOTAL cm':>9s}")
    for perfil, (v, fren) in PERFILES.items():
        for esc in ESCENARIOS:
            tot, t, d = distancia_total_cm(v, fren, esc)
            print(f"{perfil:16s} {esc:12s} {t:16.1f} {v*t/1000:8.2f} {d:11.2f} {tot:9.2f}")
    print()


def tabla_frenado_v2():
    print("=== 2. Igual, pero con el frenado de MANUAL escalado por v^2 desde AUTO (INFERENCIA, cota alta) ===")
    ref = PERFILES["AUTO 210/207"]
    a_dec = ref[0] ** 2 / (2 * ref[1])
    print(f"    deceleracion implicita en AUTO: a = v^2/(2d) = {ref[0]:.1f}^2/(2*{ref[1]}) = {a_dec:.0f} cm/s^2")
    print(f"{'perfil':16s} {'escenario':12s} {'frenado cm':>11s} {'TOTAL cm':>9s}")
    for perfil, (v, fren) in PERFILES.items():
        for esc in ("central", "conservador", "peor medido"):
            tot, t, d = distancia_total_cm(v, fren, esc, escalar_frenado_v2=ref)
            print(f"{perfil:16s} {esc:12s} {d:11.2f} {tot:9.2f}")
    print()


def tabla_holgura(sesgos_cm=(1.0, 3.0), e_cm=(0.0, 2.0, 4.0)):
    print("=== 3. Holgura con TOF_PARO_MM = 120 mm: holgura = 12.0 - sesgo - TOTAL - e  (cm; negativo = CHOCA) ===")
    print("    sesgo 1.0 cm = offset tipico +-10 mm (hoja de datos, Tabla 15); 3.0 cm = maximo por temperatura +-30 mm")
    print("    e = distancia sensor -> punto mas delantero del carro (NO MEDIDO; la rueda puede sobresalir)")
    ref = PERFILES["AUTO 210/207"]
    for perfil, (v, fren) in PERFILES.items():
        for esc in ("central", "conservador", "peor medido"):
            for modo, kw in (("frenado medido", {}), ("frenado v^2", {"escalar_frenado_v2": ref})):
                if perfil.startswith("AUTO") and modo == "frenado v^2":
                    continue
                tot, t, d = distancia_total_cm(v, fren, esc, **kw)
                cols = []
                for s in sesgos_cm:
                    for e in e_cm:
                        cols.append(f"{PARO_MM/10 - s - tot - e:6.1f}")
                print(f"{perfil:15s} {esc:11s} {modo:14s} TOTAL={tot:5.2f} | sesgo/e: " +
                      "  ".join(f"[{s:.0f}/{e:.0f}]" for s in sesgos_cm for e in e_cm))
                print(f"{'':44s}   holgura: " + " ".join(cols))
    print()


def umbral_recomendado(v, frenado, esc, sesgo_cm, e_cm, margen_cm=1.0):
    tot, _, _ = distancia_total_cm(v, frenado, esc)
    return tot + sesgo_cm + e_cm + margen_cm          # cm  (P >= D_total + sesgo + e + margen)


def tabla_umbral():
    print("=== 4. TOF_PARO_MM minimo para dejar margen de 1 cm:  P = TOTAL + sesgo + e + 1 cm  (en mm) ===")
    print(f"{'perfil':16s} {'escenario':12s} {'sesgo':>6s} {'e=0':>7s} {'e=2':>7s} {'e=4':>7s}")
    ref = PERFILES["AUTO 210/207"]
    for perfil, (v, fren) in PERFILES.items():
        for esc in ("central", "conservador", "peor medido"):
            for sesgo in (1.0, 3.0):
                vals = []
                for e in (0.0, 2.0, 4.0):
                    f = fren
                    if perfil.startswith("MANUAL"):
                        f = ref[1] * (v / ref[0]) ** 2   # cota alta por v^2
                    tot = v * (t_sense_ms(*ESCENARIOS[esc][:3]) + ESCENARIOS[esc][3]) / 1000.0 + f
                    vals.append(10 * (tot + sesgo + e + 1.0))
                print(f"{perfil:16s} {esc:12s} {sesgo:6.1f} " + " ".join(f"{x:7.0f}" for x in vals))
    print("    (MANUAL usa el frenado escalado por v^2 = cota alta; el freno laser NO actua en MANUAL en el firmware actual)")
    print()


def tabla_arranque_pleno():
    print("=== 5. AUTO durante el pulso de arranque (250 ms a PWM 255): velocidad hasta 38 cm/s (COTA, INFERENCIA) ===")
    ref = PERFILES["AUTO 210/207"]
    v = 38.0
    for esc in ("central", "conservador", "peor medido"):
        tot, t, d = distancia_total_cm(v, ref[1], esc, escalar_frenado_v2=ref)
        print(f"  {esc:12s}: TOTAL={tot:5.2f} cm (v*t={v*t/1000:4.2f} + frenado v^2={d:4.2f})  holgura(sesgo 1, e 0)={PARO_MM/10-1-tot:5.1f} cm")
    print(f"    distancia maxima recorrida en el pulso de 250 ms a 38 cm/s: {38.0*0.25:.1f} cm (cota: asume velocidad plena desde t=0)")
    print()


def tabla_watchdog():
    print("=== 6. Distancia por perdida de enlace (watchdog 1000 ms) ===")
    print("    Peor caso: el ultimo comando llega justo antes de que se pierda el enlace.")
    for perfil, (v, fren) in PERFILES.items():
        base = v * WATCHDOG_MS / 1000.0
        print(f"  {perfil:16s}: v*1000 ms = {base:5.1f} cm  + frenado {fren:.1f} cm = {base+fren:5.1f} cm  (+ lazo hasta ~41 ms = {v*0.041:.1f} cm)")
    print(f"    Latido {LATIDO_MS:.0f} ms: hasta 3 latidos seguidos perdidos (750 ms) no frenan; el 4.o hueco (>1000 ms) si.")
    print("    Con el enlace sano, un latido tardio 750 ms deja 250 ms de margen antes del watchdog.")
    print()


def prob_dos_seguidas(p):
    """Espera media (en lecturas) hasta N=2 lecturas validas y cercanas seguidas con prob. p cada una (iid)."""
    return (1.0 + p) / (p * p)


def tabla_baja_reflectancia():
    print("=== 7. Efecto de lecturas invalidas ('sin eco' = 8190, tratadas como LIBRE y rompen la racha) ===")
    print("    p = probabilidad (iid, SUPUESTO) de que una lectura sea valida y < 120 mm cuando el objeto ya esta dentro")
    print(f"{'p':>5s} {'lecturas esperadas':>19s} {'retraso extra ms':>17s} {'extra cm AUTO':>14s} {'extra cm MANUAL':>16s}")
    for p in (1.0, 0.9, 0.7, 0.5, 0.3):
        n = prob_dos_seguidas(p)
        extra_ms = (n - N_LECT) * T_MS
        print(f"{p:5.1f} {n:19.2f} {extra_ms:17.0f} {19.3*extra_ms/1000:14.2f} {38.0*extra_ms/1000:16.2f}")
    print("    OJO: el carro se acerca y la senal sube ~1/d^2, asi que p sube con el avance; esta tabla es una cota para p fija.")
    print()


def monte_carlo(n=200000, semilla=20261116):
    """Simulacion de la distancia total y la holgura con distribuciones SUPUESTAS (SIMULADO, no medido)."""
    rnd = random.Random(semilla)
    print("=== 8. Monte Carlo (SIMULADO, semilla fija; distribuciones supuestas) ===")
    print("    a~U(0,T), fase~U(0,T), sondeo~U(0,4), lazo: 85% ~0.7-2 ms, 14% 2-8 ms, 1% 8-41 ms;")
    print("    frenado~N(media medida, 1.0 cm) truncado a >=0 (incertidumbre ~2 cm segun docs/calibracion.md);")
    print("    sesgo~N(0, 0.8 cm) en la direccion peligrosa solo si >0 (offset +-10 mm, hoja de datos Tabla 15); e=0.")
    ref = PERFILES["AUTO 210/207"]
    for perfil, (v, fren) in PERFILES.items():
        for modo in ("frenado medido", "frenado v^2"):
            if perfil.startswith("AUTO") and modo == "frenado v^2":
                continue
            tot = []
            hol = []
            for _ in range(n):
                a = rnd.uniform(0, T_MS)
                fase = rnd.uniform(0, T_MS)
                sondeo = rnd.uniform(0, SONDEO_MS)
                r = rnd.random()
                if r < 0.85:
                    lazo = rnd.uniform(0.7, 2.0)
                elif r < 0.99:
                    lazo = rnd.uniform(2.0, 8.0)
                else:
                    lazo = rnd.uniform(8.0, 41.0)
                media = fren if modo == "frenado medido" else ref[1] * (v / ref[0]) ** 2
                d_fren = max(0.0, rnd.gauss(media, 1.0))
                sesgo = max(0.0, rnd.gauss(0.0, 0.8))
                t = a + (N_LECT - 1) * T_MS + fase + sondeo + lazo
                total = v * t / 1000.0 + d_fren
                tot.append(total)
                hol.append(PARO_MM / 10 - sesgo - total)
            tot.sort()
            hol.sort()
            q = lambda xs, p: xs[int(p * (len(xs) - 1))]
            print(f"  {perfil:15s} {modo:14s} TOTAL cm  p50={q(tot,.5):5.2f} p95={q(tot,.95):5.2f} p99={q(tot,.99):5.2f} max={tot[-1]:5.2f}"
                  f" | holgura(e=0) p1={q(hol,.01):5.2f} p5={q(hol,.05):5.2f} p50={q(hol,.5):5.2f}  P(<0)={sum(1 for h in hol if h < 0)/n:.4f}")
    print()


if __name__ == "__main__":
    tabla_basica()
    tabla_frenado_v2()
    tabla_holgura()
    tabla_umbral()
    tabla_arranque_pleno()
    tabla_watchdog()
    tabla_baja_reflectancia()
    monte_carlo()
