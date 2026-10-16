/*
 * SimRutaNativa.kt — Declara las funciones en C que PRUEBAN LA RUTA SIN EL CARRO (nucleo_c/sim_ruta.c): el navegador real con un carro virtual.
 *
 * GUÍA DE LECTURA
 *  1. simular() corre la ruta del editor (RutaEditorNativa) con la calibración dada (null = valores por defecto) y los obstáculos virtuales
 *     ([columna0, fila0, ...]). Devuelve 14 enteros (índices S_*). Después muestras() y cambios() dan el recorrido para animarlo.
 *  2. ruido = cuántos grados puede errar un giro de 90 (0 = perfecto); semilla distinta = error distinto (0 = sin ruido).
 *  3. dispersion() repite la ruta varias veces con ruido distinto y devuelve el resumen y el punto final de cada corrida.
 *  4. Las coordenadas del carro virtual son milímetros desde el centro de la celda (0,0) de la cuadrícula; la fila crece hacia abajo.
 */
package com.fsi.carro

object SimRutaNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    const val S_CODIGO = 0
    const val S_CODIGO_RUTA = 1
    const val S_LLEGO = 2
    const val S_ESTADO_FINAL = 3
    const val S_MOTIVO_ERROR = 4
    const val S_DURACION_MS = 5
    const val S_COMANDOS = 6
    const val S_ESQUIVADAS = 7
    const val S_ADVERTENCIAS = 8
    const val S_X_FINAL_MM = 9
    const val S_Y_FINAL_MM = 10
    const val S_ERROR_FINAL_MM = 11
    const val S_MUESTRAS = 12
    const val S_CAMBIOS = 13

    const val ADV_SALE_CUADRICULA = 1
    const val ADV_NO_LLEGO = 2
    const val ADV_CORTADA = 4
    const val ADV_CALIBRACION = 8

    // Cada muestra: t_ms, x_mm, y_mm, rumbo, comando, estado.
    const val M_PASO = 6

    // dispersion(): [código, corridas, llegaron, error medio, error máximo, dispersión] y luego, por corrida, [llegó, x, y, error, duración].
    const val D_CODIGO = 0
    const val D_CORRIDAS = 1
    const val D_LLEGARON = 2
    const val D_ERROR_MEDIO_MM = 3
    const val D_ERROR_MAX_MM = 4
    const val D_DISPERSION_MM = 5
    const val D_ENCABEZADO = 6
    const val D_PASO = 5

    external fun simular(calibracion: IntArray?, obstaculos: IntArray, ruido: Int, semilla: Int): IntArray
    /** El equilibrio de los lados del carro virtual para las simulaciones que vengan: cuánto más rápido empuja el izquierdo y el balance con que se reduce (milésimas; 1000 y 1000 = sin desvío). */
    external fun ponerBalance(desbalanceMil: Int, balanceMil: Int)

    external fun muestras(): IntArray
    external fun cambios(): IntArray
    external fun dispersion(calibracion: IntArray?, obstaculos: IntArray, ruido: Int, corridas: Int): IntArray
    external fun textoAdvertencia(bit: Int): String

    /** Por qué no llegó la última simulación, en una frase ("" si llegó o no hay una causa que contar). */
    external fun textoCausa(): String
}
