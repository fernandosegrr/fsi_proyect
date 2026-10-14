/*
 * PruebaObstaculoNativa.kt — Cuentas de la prueba "Obstáculos: cámara" (nucleo_c/prueba_obstaculo.c): retraso obstáculo → STOP y cm recorridos.
 *
 * GUÍA DE LECTURA
 *  1. retrasoMs() suma el retraso del video, el tiempo hasta enviar el STOP y la mitad del ida y vuelta al carro.
 *  2. cmX10() dice cuántas décimas de cm avanza el carro durante ese retraso (más el frenado) con la calibración guardada.
 *  3. agregar() + resumen() llevan el promedio y el máximo de las pasadas de la prueba.
 */
package com.fsi.carro

object PruebaObstaculoNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    const val R_PASADAS = 0
    const val R_RETRASO_PROM_MS = 1
    const val R_RETRASO_MAX_MS = 2
    const val R_CM_PROM_X10 = 3
    const val R_CM_MAX_X10 = 4

    external fun retrasoMs(msStop: Int, msRespuesta: Int, latenciaVideoMs: Int): Int
    external fun cmX10(retrasoMs: Int, msPorCmX10: Int, frenadoMm: Int): Int
    external fun iniciarResumen()
    external fun agregar(retrasoMs: Int, cmX10: Int)
    external fun resumen(): IntArray
}
