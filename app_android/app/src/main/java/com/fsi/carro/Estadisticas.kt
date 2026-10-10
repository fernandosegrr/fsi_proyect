/*
 * Estadisticas.kt — Resumen de una lista de tiempos (en milisegundos).
 *
 * GUÍA DE LECTURA
 *  1. resumir() ordena las muestras y calcula media, mediana, p95, mínimo y máximo.
 *  2. p95 = el valor por debajo del cual queda el 95 % de las mediciones
 *     (cuánto tarda "casi siempre", sin contar los casos más raros).
 */
package com.fsi.carro

class Resumen(
    val media: Double,
    val mediana: Double,
    val p95: Double,
    val minimo: Double,
    val maximo: Double
)

/** Calcula el resumen de las muestras. Debe haber al menos una. */
fun resumir(muestras: DoubleArray): Resumen {
    val ordenadas = muestras.sortedArray()
    val n = ordenadas.size
    val media = ordenadas.sum() / n
    val mediana = if (n % 2 == 1) ordenadas[n / 2] else (ordenadas[n / 2 - 1] + ordenadas[n / 2]) / 2.0
    val posicionP95 = Math.ceil(0.95 * n).toInt() - 1
    return Resumen(media, mediana, ordenadas[posicionP95.coerceIn(0, n - 1)], ordenadas[0], ordenadas[n - 1])
}
