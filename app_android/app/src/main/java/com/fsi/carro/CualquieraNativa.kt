/*
 * CualquieraNativa.kt — Declara las funciones en C del modo «cualquier objeto» del modelo de objetos (nucleo_c/cualquiera.c).
 *
 * GUÍA DE LECTURA
 *  1. El modelo de objetos (EfficientDet) casi nunca acierta la CLASE con la cámara del carro, pero suele dibujar una caja sobre lo que está en el piso. Este modo
 *     ignora la clase: filtra las cajas por confianza (mayor al umbral, 0.15 por defecto), tamaño y posición y pide que aparezcan en el mismo sitio en 2 cuadros
 *     seguidos. Apagado por defecto (ConfigVision.cualquierObjeto).
 *  2. actualizar() devuelve las zonas que cuentan (posiciones R_*); FusionNativa.decidirCualquiera las une con el color (el color manda en seguridad).
 *  3. barrido() es solo para el registro OBJ_ANY: corre el modo con 6 umbrales a la vez (0.01 a 0.30) para ver qué umbral conviene.
 *  Todo se llama desde el hilo de la cámara (la historia de persistencia es única).
 */
package com.fsi.carro

object CualquieraNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    // Posiciones de la configuración (9 enteros).
    const val CFG_ACTIVO = 0
    const val CFG_UMBRAL_MIL = 1
    const val CFG_AREA_MAX_MIL = 2
    const val CFG_BORDE_INFERIOR_MIN_MIL = 3
    const val CFG_AREA_MIN_MIL = 4
    const val CFG_ANCHO_MIN_MIL = 5
    const val CFG_PERSISTENCIA = 6
    const val CFG_SOLAPE_MIL = 7
    const val CFG_VIGENCIA_MS = 8
    const val ENTEROS_CONFIG = 9

    // Valores por defecto (los mismos de cualquiera.h).
    const val UMBRAL_DEFECTO_MIL = 150
    const val UMBRAL_MIN_MIL = 10
    const val UMBRAL_MAX_MIL = 500

    // Posiciones del resultado de actualizar().
    const val R_CODIGO = 0
    const val R_VIGENTE = 1
    const val R_EDAD_MS = 2
    const val R_CAJAS_MODELO = 3
    const val R_PASAN_FILTROS = 4
    const val R_PERSISTENTES = 5
    const val R_N_CUENTAN = 6
    const val R_ZONAS = 7           // 7 a 12: hay y cercanía de izquierda, centro y derecha
    const val R_MEJOR_ZONA = 13
    const val R_MEJOR_CERCANIA = 14
    const val R_NUEVO_CUADRO = 15
    const val R_CAJAS = 16          // desde aquí 6 enteros por caja que cuenta: xmin, ymin, xmax, ymax, zona, cercanía

    const val UMBRALES_BARRIDO = 6
    val UMBRALES_BARRIDO_MIL = intArrayOf(10, 50, 100, 150, 200, 300)

    /** Procesa lo último que dijo el modelo: `cajas` = [n, (clase, confianza_mil, xmin, ymin, xmax, ymax) * n]. */
    external fun actualizar(config: IntArray, cajas: IntArray, marcaMs: Long, ahoraMs: Long): IntArray

    /** Registro OBJ_ANY: con los 6 umbrales; por umbral [umbral_mil, pasan filtros, cuentan, mejor zona, mejor cercanía] (30 enteros). */
    external fun barrido(config: IntArray, cajas: IntArray, marcaMs: Long, ahoraMs: Long): IntArray

    /** Olvida la historia de persistencia. */
    external fun reiniciar()

    /** Las zonas (hay, cercanía de izquierda, centro y derecha) que cuentan, en el formato que pide FusionNativa.decidirCualquiera. */
    fun zonasDe(resultado: IntArray): IntArray = intArrayOf(
        resultado[R_ZONAS], resultado[R_ZONAS + 1], resultado[R_ZONAS + 2], resultado[R_ZONAS + 3], resultado[R_ZONAS + 4], resultado[R_ZONAS + 5])
}
