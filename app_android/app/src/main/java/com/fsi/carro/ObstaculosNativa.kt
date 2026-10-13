/*
 * ObstaculosNativa.kt — Declara las funciones en C del detector de obstáculos por color de piso.
 *
 * GUÍA DE LECTURA
 *  1. `external` significa "la implementación está en código nativo (C)": está en
 *     app/src/main/cpp/puente.c y llama a nucleo_c/obstaculos.c.
 *  2. La imagen entra como bytes RGB (R, G, B, R, G, B...), fila por fila, de CUALQUIER tamaño
 *     razonable (de 30x30 a 640x480): el ancho y el alto se pasan en cada llamada. El detector
 *     la divide en celdas de 10x10 píxeles; con 240x176 (el modo de la cámara real) son 24x17.
 *  3. analizar() usa la configuración por defecto y devuelve 10 enteros.
 *     analizarConfig() usa una configuración que se puede cambiar en vivo y devuelve además el
 *     tamaño de la rejilla, la franja de referencia y la máscara de celdas "no piso" (para
 *     dibujarlas). Los índices están más abajo.
 *  4. Todo lo que depende del tamaño (franja de referencia, fila mínima) va como proporción en
 *     MILÉSIMAS (500 = 0.5), así la misma configuración vale para cualquier tamaño de imagen.
 */
package com.fsi.carro

object ObstaculosNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    /** Tamaño de REFERENCIA (320x240): lo usan el benchmark y las imágenes de prueba. */
    const val ANCHO = 320
    const val ALTO = 240

    /** Tamaño de los cuadros de la cámara real del carro (Freenove en HQVGA). Es el de partida de la pantalla de visión. */
    const val ANCHO_CAMARA = 240
    const val ALTO_CAMARA = 176

    const val LADO_CELDA = 10   // cada celda mide 10x10 píxeles

    /** Columnas y filas de celdas de una imagen de ese tamaño (igual que obstaculos_columnas_de / obstaculos_filas_de). */
    fun columnasDe(ancho: Int) = ancho / LADO_CELDA
    fun filasDe(alto: Int) = alto / LADO_CELDA

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

    // analizarConfig() devuelve los 10 de arriba y después este encabezado y la máscara.
    const val COLUMNAS_USADAS = 10      // columnas de la rejilla de ESTE análisis
    const val FILAS_USADAS = 11
    const val FILA_MINIMA_USADA = 12    // fila donde empieza la zona de avance
    const val REJILLA_CHICA = 13        // 1 = rejilla chica: funciona pero es más tosca
    const val REF_COLUMNA = 14          // franja de referencia del piso (en celdas)
    const val REF_FILA = 15
    const val REF_NCOLUMNAS = 16
    const val REF_NFILAS = 17

    /** Desde aquí viene la máscara: 1 = celda "no piso", fila por fila (índice INICIO_MASCARA + fila * columnas + columna). */
    const val INICIO_MASCARA = 18

    // Posiciones dentro de la configuración (7 enteros). Las proporciones van en milésimas.
    const val CFG_UMBRAL_COLOR = 0
    const val CFG_MIN_CELDAS = 1
    const val CFG_REF_ANCHO_MIL = 2        // ancho de la franja de referencia
    const val CFG_REF_ALTO_MIL = 3         // alto de la franja de referencia
    const val CFG_REF_CENTRO_X_MIL = 4     // centro de la franja a lo ancho (500 = al centro)
    const val CFG_REF_BORDE_INF_MIL = 5    // borde de abajo de la franja (1000 = pegada abajo)
    const val CFG_FILA_MINIMA_MIL = 6      // fila mínima de la zona de avance (500 = mitad inferior); las de arriba se ignoran

    // Códigos de error de obstaculos.h.
    const val ERR_IMAGEN_NULA = -1
    const val ERR_TAMANO = -2
    const val ERR_REFERENCIA = -3
    const val ERR_CONFIG = -4

    /** Analiza una imagen RGB (ancho x alto x 3 bytes) con la configuración por defecto: 10 enteros. */
    external fun analizar(rgb: ByteArray, ancho: Int, alto: Int): IntArray

    /** Igual, pero con una configuración propia (7 enteros) y con el detalle de celdas: INICIO_MASCARA + filas*columnas enteros. */
    external fun analizarConfig(rgb: ByteArray, ancho: Int, alto: Int, config: IntArray): IntArray

    /** Configuración por defecto del código en C (7 enteros). */
    external fun configDefecto(): IntArray
}
