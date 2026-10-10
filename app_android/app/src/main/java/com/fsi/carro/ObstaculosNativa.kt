/*
 * ObstaculosNativa.kt — Declara las funciones en C del detector de obstáculos por color de piso.
 *
 * GUÍA DE LECTURA
 *  1. `external` significa "la implementación está en código nativo (C)": está en
 *     app/src/main/cpp/puente.c y llama a nucleo_c/obstaculos.c.
 *  2. La imagen entra como bytes RGB (R, G, B, R, G, B...) de 320x240, fila por fila.
 *  3. analizar() usa la configuración por defecto y devuelve 10 enteros.
 *     analizarConfig() usa una configuración que se puede cambiar en vivo y devuelve además la
 *     máscara de celdas "no piso" (para dibujarlas). Los índices están más abajo.
 */
package com.fsi.carro

object ObstaculosNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    const val ANCHO = 320
    const val ALTO = 240
    const val COLUMNAS = 32     // celdas de 10x10 píxeles
    const val FILAS = 24

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

    /** analizarConfig() devuelve los 10 de arriba y después la máscara: 1 = celda "no piso". */
    const val INICIO_MASCARA = 10
    const val ENTEROS_DETALLE = INICIO_MASCARA + FILAS * COLUMNAS

    // Posiciones dentro de la configuración (6 enteros).
    const val CFG_UMBRAL_COLOR = 0
    const val CFG_MIN_CELDAS = 1
    const val CFG_REF_COLUMNAS = 2
    const val CFG_REF_FILAS = 3
    const val CFG_REF_COLUMNA_INICIAL = 4
    const val CFG_REF_FILA_INICIAL = 5

    // Códigos de error de obstaculos.h.
    const val ERR_IMAGEN_NULA = -1
    const val ERR_TAMANO = -2
    const val ERR_REFERENCIA = -3
    const val ERR_CONFIG = -4

    /** Analiza una imagen RGB de 320x240 (230400 bytes) con la configuración por defecto: 10 enteros. */
    external fun analizar(rgb: ByteArray, ancho: Int, alto: Int): IntArray

    /** Igual, pero con una configuración propia (6 enteros) y con la máscara de celdas: ENTEROS_DETALLE enteros. */
    external fun analizarConfig(rgb: ByteArray, ancho: Int, alto: Int, config: IntArray): IntArray

    /** Configuración por defecto del código en C (6 enteros). */
    external fun configDefecto(): IntArray
}
