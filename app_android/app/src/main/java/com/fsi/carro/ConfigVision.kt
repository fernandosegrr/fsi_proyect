/*
 * ConfigVision.kt — Los parámetros del detector de obstáculos que se ajustan en vivo.
 *
 * GUÍA DE LECTURA
 *  1. Guarda UMBRAL_COLOR, MIN_CELDAS y la fila mínima en SharedPreferences (el almacenamiento de
 *     preferencias de Android), para que lo que se ajustó en VisionActivity también lo use el modo
 *     semiautónomo de ControlActivity y se conserve al cerrar la app.
 *  2. Los valores por defecto vienen del código en C (ObstaculosNativa.configDefecto).
 *  3. La fila mínima es una PROPORCIÓN del alto (en milésimas: 500 = la mitad inferior), no un
 *     número de fila: así vale para cualquier tamaño de cuadro. En pantalla se muestra en por ciento.
 *  4. También guarda el tamaño de los cuadros que se piden a la cámara del teléfono (por defecto
 *     240x176, el de la cámara real del carro).
 *  5. También guarda cuántos colores puede tener el piso (K, 1 a 4) y la velocidad de aprendizaje del modelo.
 *  6. usarModelo dice si además del detector de color se usa el modelo de objetos (fusión); por defecto NO hasta probarlo.
 *  7. comoArreglo() da los 10 enteros que espera ObstaculosNativa.analizarConfig.
 */
package com.fsi.carro

import android.content.Context
import android.content.SharedPreferences

class ConfigVision(contexto: Context) {

    companion object {
        const val UMBRAL_COLOR_MAXIMO = 300   // tope de la barra deslizable (el máximo del detector es 765)
        const val MIN_CELDAS_MAXIMO = 40
        const val PORCENTAJE_FILA_MAXIMO = 100   // la barra de fila mínima va de 0 a 100 %
        const val GRUPOS_MAXIMO = 4              // K máximo (OBS_MAX_GRUPOS en el código en C)
        const val APRENDIZAJE_MAXIMO_MIL = 100   // tope de la barra de aprendizaje (el máximo del detector es 1000)

        /** Tamaños de cuadro que se pueden pedir a la cámara del teléfono (ancho, alto). */
        val TAMANOS = arrayOf(
            intArrayOf(240, 176),   // HQVGA: el modo de la cámara real del carro
            intArrayOf(320, 240),   // QVGA
            intArrayOf(160, 120)    // QQVGA
        )
    }

    private val preferencias: SharedPreferences = contexto.getSharedPreferences("vision", Context.MODE_PRIVATE)
    private val defecto = ObstaculosNativa.configDefecto()

    var umbralColor: Int
        get() = preferencias.getInt("umbral_color", defecto[ObstaculosNativa.CFG_UMBRAL_COLOR])
        set(valor) = preferencias.edit().putInt("umbral_color", valor).apply()

    var minCeldas: Int
        get() = preferencias.getInt("min_celdas", defecto[ObstaculosNativa.CFG_MIN_CELDAS])
        set(valor) = preferencias.edit().putInt("min_celdas", valor).apply()

    /** Fila mínima de la zona de avance como proporción del alto, en milésimas (500 = la mitad inferior). */
    var filaMinimaMil: Int
        get() = preferencias.getInt("fila_minima_mil", defecto[ObstaculosNativa.CFG_FILA_MINIMA_MIL]).coerceIn(0, 1000)
        set(valor) = preferencias.edit().putInt("fila_minima_mil", valor.coerceIn(0, 1000)).apply()

    /** K: cuántos colores distintos puede tener el piso (baldosas, vetas...). */
    var gruposPiso: Int
        get() = preferencias.getInt("grupos_piso", defecto[ObstaculosNativa.CFG_GRUPOS_PISO]).coerceIn(1, GRUPOS_MAXIMO)
        set(valor) = preferencias.edit().putInt("grupos_piso", valor.coerceIn(1, GRUPOS_MAXIMO)).apply()

    /** Velocidad de aprendizaje del modelo del piso, en milésimas por cuadro (0 = no aprende). */
    var aprendizajeMil: Int
        get() = preferencias.getInt("aprendizaje_mil", defecto[ObstaculosNativa.CFG_APRENDIZAJE_MIL]).coerceIn(0, 1000)
        set(valor) = preferencias.edit().putInt("aprendizaje_mil", valor.coerceIn(0, 1000)).apply()

    /** true = se usa también el modelo de objetos (en su propio hilo) y se fusiona con el detector de color. */
    var usarModelo: Boolean
        get() = preferencias.getBoolean("usar_modelo", false)
        set(valor) = preferencias.edit().putBoolean("usar_modelo", valor).apply()

    /** Índice en TAMANOS del tamaño de cuadro que se pide a la cámara del teléfono. */
    var indiceTamano: Int
        get() = preferencias.getInt("indice_tamano", 0).coerceIn(0, TAMANOS.size - 1)
        set(valor) = preferencias.edit().putInt("indice_tamano", valor.coerceIn(0, TAMANOS.size - 1)).apply()

    val anchoCuadro: Int get() = TAMANOS[indiceTamano][0]
    val altoCuadro: Int get() = TAMANOS[indiceTamano][1]

    /** Vuelve a los valores por defecto del código en C. */
    fun restablecer() {
        umbralColor = defecto[ObstaculosNativa.CFG_UMBRAL_COLOR]
        minCeldas = defecto[ObstaculosNativa.CFG_MIN_CELDAS]
        filaMinimaMil = defecto[ObstaculosNativa.CFG_FILA_MINIMA_MIL]
        gruposPiso = defecto[ObstaculosNativa.CFG_GRUPOS_PISO]
        aprendizajeMil = defecto[ObstaculosNativa.CFG_APRENDIZAJE_MIL]
    }

    /** Los 10 enteros de configuración (la franja de referencia queda con el valor por defecto). */
    fun comoArreglo(): IntArray {
        val config = defecto.copyOf()
        config[ObstaculosNativa.CFG_UMBRAL_COLOR] = umbralColor
        config[ObstaculosNativa.CFG_MIN_CELDAS] = minCeldas
        config[ObstaculosNativa.CFG_FILA_MINIMA_MIL] = filaMinimaMil
        config[ObstaculosNativa.CFG_GRUPOS_PISO] = gruposPiso
        config[ObstaculosNativa.CFG_APRENDIZAJE_MIL] = aprendizajeMil
        return config
    }
}
