/*
 * ObstaculosNativa.kt — Declara la función en C del detector de obstáculos por color de piso.
 *
 * GUÍA DE LECTURA
 *  1. `external` significa "la implementación está en código nativo (C)": está en
 *     app/src/main/cpp/puente.c y llama a nucleo_c/obstaculos.c.
 *  2. La imagen entra como bytes RGB (R, G, B, R, G, B...) de 320x240, fila por fila.
 *  3. El resultado son 10 enteros (ver los índices más abajo).
 */
package com.fsi.carro

object ObstaculosNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    const val ANCHO = 320
    const val ALTO = 240

    // Posiciones dentro del arreglo que devuelve analizar().
    const val CODIGO = 0            // 0 = bien; negativo = error
    const val HAY_IZQUIERDA = 1
    const val CERCANIA_IZQUIERDA = 2   // 0 = nada, 1 = lejos, 2 = media, 3 = muy cerca
    const val HAY_CENTRO = 3
    const val CERCANIA_CENTRO = 4
    const val HAY_DERECHA = 5
    const val CERCANIA_DERECHA = 6
    const val REF_R = 7             // color de referencia del piso
    const val REF_G = 8
    const val REF_B = 9

    /** Analiza una imagen RGB de 320x240 (230400 bytes) y devuelve los 10 enteros. */
    external fun analizar(rgb: ByteArray, ancho: Int, alto: Int): IntArray
}
