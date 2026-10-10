/*
 * ConfigVision.kt — Los parámetros del detector de obstáculos que se ajustan en vivo.
 *
 * GUÍA DE LECTURA
 *  1. Guarda UMBRAL_COLOR, MIN_CELDAS y FILA_MINIMA en SharedPreferences (el almacenamiento de preferencias
 *     de Android), para que lo que se ajustó en VisionActivity también lo use el modo
 *     semiautónomo de ControlActivity y se conserve al cerrar la app.
 *  2. Los valores por defecto vienen del código en C (ObstaculosNativa.configDefecto).
 *  3. comoArreglo() da los 7 enteros que espera ObstaculosNativa.analizarConfig.
 */
package com.fsi.carro

import android.content.Context
import android.content.SharedPreferences

class ConfigVision(contexto: Context) {

    companion object {
        const val UMBRAL_COLOR_MAXIMO = 300   // tope de la barra deslizable (el máximo del detector es 765)
        const val MIN_CELDAS_MAXIMO = 40
        const val FILA_MINIMA_MAXIMA = ObstaculosNativa.FILAS - 1   // 23: la última fila
    }

    private val preferencias: SharedPreferences = contexto.getSharedPreferences("vision", Context.MODE_PRIVATE)
    private val defecto = ObstaculosNativa.configDefecto()

    var umbralColor: Int
        get() = preferencias.getInt("umbral_color", defecto[ObstaculosNativa.CFG_UMBRAL_COLOR])
        set(valor) = preferencias.edit().putInt("umbral_color", valor).apply()

    var minCeldas: Int
        get() = preferencias.getInt("min_celdas", defecto[ObstaculosNativa.CFG_MIN_CELDAS])
        set(valor) = preferencias.edit().putInt("min_celdas", valor).apply()

    var filaMinima: Int
        get() = preferencias.getInt("fila_minima", defecto[ObstaculosNativa.CFG_FILA_MINIMA]).coerceIn(0, FILA_MINIMA_MAXIMA)
        set(valor) = preferencias.edit().putInt("fila_minima", valor).apply()

    /** Vuelve a los valores por defecto del código en C. */
    fun restablecer() {
        umbralColor = defecto[ObstaculosNativa.CFG_UMBRAL_COLOR]
        minCeldas = defecto[ObstaculosNativa.CFG_MIN_CELDAS]
        filaMinima = defecto[ObstaculosNativa.CFG_FILA_MINIMA]
    }

    /** Los 7 enteros de configuración (la franja de referencia queda con el valor por defecto). */
    fun comoArreglo(): IntArray {
        val config = defecto.copyOf()
        config[ObstaculosNativa.CFG_UMBRAL_COLOR] = umbralColor
        config[ObstaculosNativa.CFG_MIN_CELDAS] = minCeldas
        config[ObstaculosNativa.CFG_FILA_MINIMA] = filaMinima
        return config
    }
}
