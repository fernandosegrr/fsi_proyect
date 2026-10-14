/*
 * FlujoNativa.kt — Declara las funciones en C del flujo del piso (nucleo_c/movimiento_piso.c; ver docs/flujo_piso.md).
 *
 * GUÍA DE LECTURA
 *  1. El flujo compara dos cuadros seguidos (separados ~200 ms) mientras el carro AVANZA y mira cuánto se movió cada bloque de 16x16 contra lo que se
 *     movería si fuera piso plano (con la altura, la inclinación y el campo de visión del corredor y la velocidad del perfil AUTO). Un bloque con textura que se
 *     movió MÁS que el piso está más cerca que el piso (candidato a obstáculo); uno sin textura es «sin dato» y nunca cuenta.
 *  2. Está APAGADO por defecto (ConfigVision.flujoActivo). Con el flujo encendido, CamaraObstaculos escribe el registro FLUJO_DIAG y le pasa a la fusión
 *     FusionNativa.aplicarFlujo (el color manda: el flujo solo puede BAJAR un falso positivo de color lejano en un piso con textura consistente, o avisar).
 *  3. analizar() recibe los dos cuadros en RGB (ancho x alto x 3 bytes) y devuelve 31 enteros (posiciones R_*).
 */
package com.fsi.carro

object FlujoNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    // Posiciones de la configuración (13 enteros).
    const val CFG_ACTIVO = 0
    const val CFG_VELOCIDAD_MM_S = 1
    const val CFG_DT_MIN_MS = 9
    const val CFG_DT_MAX_MS = 10
    const val ENTEROS_CONFIG = 13

    // Posiciones del resultado de analizar() (31 enteros).
    const val R_CODIGO = 0
    const val R_EVALUADO = 1
    const val R_MOTIVO = 2            // 0 evaluado, 1 apagado, 2 el carro no avanza, 3 tiempo entre cuadros fuera de rango
    const val R_AVANCE_MM = 3
    const val R_PISO = 4
    const val R_CERCA = 5
    const val R_DUDOSO = 6
    const val R_SIN_DATO = 7
    const val R_FILA_INICIAL = 8
    const val R_FILA_FINAL = 9
    const val R_ZONAS = 10            // 7 enteros por zona (izquierda, centro, derecha): piso, cerca, dudoso, sin dato, piso consistente, hay cerca, cercanía
    const val ENTEROS_POR_ZONA = 7
    const val ENTEROS_RESULTADO = 31

    /** Cuántos ms debe haber entre el cuadro de referencia y el actual (el desplazamiento del piso en 100 ms es de 1 a 2 píxeles: muy poco para medirlo bien). */
    const val SEPARACION_OBJETIVO_MS = 180

    /** Los valores por defecto del código en C (13 enteros, el primero —activo— en 0). */
    external fun configDefecto(): IntArray

    /**
     * Compara el cuadro anterior con el actual. `config` son los 13 enteros de arriba y `corredor` los 7 de CorredorNativa (de ahí salen la altura, la inclinación y el
     * campo de visión de la cámara). `avanzando` = el carro iba hacia adelante; `dtMs` = tiempo entre los dos cuadros.
     */
    external fun analizar(config: IntArray, corredor: IntArray, anterior: ByteArray, actual: ByteArray, ancho: Int, alto: Int, avanzando: Boolean, dtMs: Int): IntArray

    /** Los 9 enteros (piso consistente, hay cerca, cercanía de izquierda, centro y derecha) que pide FusionNativa.aplicarFlujo. */
    fun zonasDe(r: IntArray): IntArray {
        val salida = IntArray(9)
        for (z in 0..2) {
            val base = R_ZONAS + ENTEROS_POR_ZONA * z
            salida[3 * z] = r[base + 4]
            salida[3 * z + 1] = r[base + 5]
            salida[3 * z + 2] = r[base + 6]
        }
        return salida
    }

    /** El texto de la línea FLUJO_DIAG (solo números). */
    fun textoDiagnostico(r: IntArray, dtMs: Int, ms: Double): String {
        val sb = StringBuilder("FLUJO_DIAG ")
        sb.append("evaluado=").append(r[R_EVALUADO]).append(" motivo=").append(r[R_MOTIVO]).append(" dt_ms=").append(dtMs).append(" avance_mm=").append(r[R_AVANCE_MM])
        sb.append(" piso=").append(r[R_PISO]).append(" cerca=").append(r[R_CERCA]).append(" dudoso=").append(r[R_DUDOSO]).append(" sin_dato=").append(r[R_SIN_DATO])
        sb.append(" filas=").append(r[R_FILA_INICIAL]).append('-').append(r[R_FILA_FINAL])
        val nombres = arrayOf("I", "C", "D")
        for (z in 0..2) {
            val b = R_ZONAS + ENTEROS_POR_ZONA * z
            sb.append(' ').append(nombres[z]).append("=p").append(r[b]).append("/c").append(r[b + 1]).append("/d").append(r[b + 2]).append("/s").append(r[b + 3])
                .append("/consistente").append(r[b + 4]).append("/cerc").append(r[b + 6])
        }
        sb.append(String.format(java.util.Locale.US, " ms=%.1f", ms))
        return sb.toString()
    }
}
