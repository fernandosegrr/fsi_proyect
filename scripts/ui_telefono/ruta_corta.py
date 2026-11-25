# -*- coding: utf-8 -*-
"""
ruta_corta.py - corre UNA ruta corta (la «Ruta en L», unos 6 a 7 s) contra el carro FALSO y deja capturas de la ruta en marcha y del resumen final.

  python ruta_corta.py <carpeta_de_capturas>

Para que sirve: ver en el telefono la cabecera con el PARO mientras corre la ruta y el resumen «Terminado» (que el chip no salga cortado).

Por que espera un momento antes de lanzar: la comprobacion previa de la ruta mira el piso con la camara y se niega («Hay algo frente al carro») si ve
la «caja» de la camara falsa, que scripts/camara_falsa.py pone a partir del segundo 5 de cada ciclo de 12 s del reloj de la PC. Aqui se lanza la app
para que la comprobacion caiga en la parte limpia del ciclo, se toca «Confirmar y salir» y se espera al PARO de la cabecera (visible = ruta en
marcha, oculto = ruta terminada). Hasta 3 intentos. El carro NO se mueve: es el servidor falso del PC. Las capturas se hacen solo con la app al frente.
"""
import os
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import prueba_telefono as p  # noqa: E402


def esperar(clave, vis, seg, paso=0.7):
    """Espera a que la vista `clave` tenga la visibilidad `vis` ('V' visible, 'G' oculta). Devuelve la jerarquia o None."""
    t0 = time.time()
    while time.time() - t0 < seg:
        j = p.jerarquia()
        v = j.get(clave)
        if v and v["vis"] == vis:
            return j
        time.sleep(paso)
    return None


def alinear_con_la_camara(objetivo=10.3, tolerancia=0.4):
    """Espera a que el reloj de la PC (modulo 12 s) este cerca de `objetivo`: la caja de la camara falsa aparece despues del segundo 5."""
    while abs((time.time() % 12.0) - objetivo) > tolerancia:
        time.sleep(0.05)
    return time.time() % 12.0


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    carpeta = sys.argv[1]
    os.makedirs(carpeta, exist_ok=True)
    corrio = False
    for intento in range(1, 4):
        print("== intento", intento)
        print("fase de la camara al lanzar: %.1f s" % alinear_con_la_camara())
        print("lanzo:", p.lanzar("ejecutar", p.CAM_OK, extras=("--es", "ruta_predefinida", "l", "--ez", "auto_iniciar", "true")))
        time.sleep(8)                      # la comprobacion previa de la ruta
        j = p.jerarquia()
        if "ej_iniciar" not in j or j["ej_iniciar"]["vis"] != "V":
            print("  no hay boton para confirmar; foco:", p.foco())
            continue
        p.capturar(os.path.join(carpeta, "antes_de_confirmar_%d.png" % intento))
        print("  toque:", p.tocar_vista("ej_iniciar", j))
        if not esperar("boton_paro_cabecera", "V", 14):
            print("  la ruta no arranco (mira la captura antes_de_confirmar_%d.png: puede ser el aviso «Hay algo frente al carro»)" % intento)
            continue
        print("  ruta en marcha (PARO de la cabecera visible): True")
        p.capturar(os.path.join(carpeta, "ruta_en_marcha.png"))
        terminada = esperar("boton_paro_cabecera", "G", 40)
        print("  ruta terminada (PARO de la cabecera oculto):", bool(terminada))
        time.sleep(1.5)
        print("  captura del resumen:", p.capturar(os.path.join(carpeta, "ruta_resumen.png")))
        corrio = True
        break
    print("corrio completa:", corrio, "| foco:", p.foco())
    return 0 if corrio else 1


if __name__ == "__main__":
    sys.exit(main())
