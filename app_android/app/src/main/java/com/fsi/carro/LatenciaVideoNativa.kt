/*
 * LatenciaVideoNativa.kt — Declara las funciones en C que comparan el PONG del control con y sin video (nucleo_c/latencia_video.c).
 *
 * GUÍA DE LECTURA
 *  1. iniciar() empieza una medición nueva; muestra(conVideo, pongMs) anota cada PONG; resultado() devuelve 6 enteros (índices R_*).
 *  2. La regla (más de 30 ms de aumento = dejar el video de Manual apagado por defecto) está en el código en C y sus pruebas.
 */
package com.fsi.carro

object LatenciaVideoNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    const val R_PROM_SIN_X10 = 0
    const val R_PROM_CON_X10 = 1
    const val R_MAX_SIN = 2
    const val R_MAX_CON = 3
    const val R_DIFERENCIA_X10 = 4
    const val R_VEREDICTO = 5    // -1 faltan medidas, 0 se mantiene, 1 empeora

    external fun iniciar()
    external fun muestra(conVideo: Int, pongMs: Int)
    external fun resultado(): IntArray
}
