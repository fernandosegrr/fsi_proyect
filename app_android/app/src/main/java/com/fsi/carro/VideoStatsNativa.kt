/*
 * VideoStatsNativa.kt — Estadística de la llegada de cuadros del video del carro (nucleo_c/video_stats.c).
 *
 * GUÍA DE LECTURA
 *  1. VideoCarro llama a cuadro() cada vez que llega un cuadro. La pantalla de visión y el log VIDEO_STATS llaman a resumen().
 *  2. resumen() devuelve 5 enteros de los últimos 10 segundos (índices de abajo): fps, el hueco más largo entre cuadros, cuántos huecos
 *     pasaron de 500 ms ("tropiezos"), los cuadros y el hueco que está pasando ahora.
 *  3. Varios hilos la usan (el del video escribe, la pantalla y el log leen): un candado las serializa.
 */
package com.fsi.carro

object VideoStatsNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    const val FPS_X10 = 0
    const val HUECO_MAX_MS = 1
    const val HUECOS_LARGOS = 2
    const val CUADROS = 3
    const val HUECO_ACTUAL_MS = 4

    private val candado = Any()

    private external fun nIniciar()
    private external fun nCuadro(ahoraMs: Long)
    private external fun nResumen(ahoraMs: Long): IntArray

    /** Empieza de cero (al abrir el video). */
    fun reiniciar() = synchronized(candado) { nIniciar() }

    /** Anota que llegó un cuadro ahora (reloj SystemClock.elapsedRealtime). */
    fun cuadro(ahoraMs: Long) = synchronized(candado) { nCuadro(ahoraMs) }

    /** Los 5 números de los últimos 10 s vistos desde `ahoraMs`. */
    fun resumen(ahoraMs: Long): IntArray = synchronized(candado) { nResumen(ahoraMs) }
}
