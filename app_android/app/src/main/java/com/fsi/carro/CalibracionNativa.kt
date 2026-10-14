/*
 * CalibracionNativa.kt — Declara las funciones en C de la calibración del carro (nucleo_c/calibracion.c).
 *
 * GUÍA DE LECTURA
 *  1. La calibración viaja como un IntArray de 9 enteros (índices CAL_* más abajo). Las funciones que
 *     la cambian devuelven 10 enteros: [código, calibración...]; usa desempaquetar() para separarlos.
 *  2. Los decimales van multiplicados por 10 (sufijo _x10): 420 = 42.0 ms por cm.
 *  3. Cómo se mide (lo hace CalibracionActivity): arranque, avance (varios tiempos), giro a la
 *     izquierda y giro a la derecha. Ver docs/calibracion.md.
 */
package com.fsi.carro

object CalibracionNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    // Posiciones dentro de la calibración (8 enteros).
    const val CAL_MEDIDOS = 0
    const val CAL_ARRANQUE_MS = 1
    const val CAL_MS_POR_CM_X10 = 2
    const val CAL_FRENADO_MM = 3
    const val CAL_MS_GRADO_IZQ_X10 = 4
    const val CAL_MS_GRADO_DER_X10 = 5
    const val CAL_FRENADO_GRADOS_IZQ_X10 = 6
    const val CAL_FRENADO_GRADOS_DER_X10 = 7
    const val CAL_FOV_GRADOS_X10 = 8    // campo de visión horizontal de la cámara, grados por 10 (600 = 60.0)
    const val ENTEROS = 9

    // Bits de "medidos" / "faltantes".
    const val MEDIDO_ARRANQUE = 1
    const val MEDIDO_AVANCE = 2
    const val MEDIDO_GIRO_IZQ = 4
    const val MEDIDO_GIRO_DER = 8
    const val MEDIDO_TODO = 15          // los cuatro obligatorios: arranque, avance y los dos giros
    const val MEDIDO_FOV = 16           // opcional: el campo de visión de la cámara (giro visual)

    const val LADO_IZQUIERDA = -1
    const val LADO_DERECHA = 1

    // Códigos de error (calibracion.h).
    const val OK = 0
    const val ERR_ARGUMENTO = -1
    const val ERR_DATOS = -2
    const val ERR_RANGO = -3
    const val ERR_TEXTO = -4

    /** Calibración por defecto (la de ejemplo): [código, 8 enteros]. */
    external fun defecto(): IntArray

    /** Promedio de los valores sin los datos atípicos (los que se alejan mucho de la mediana). */
    external fun promedioSinAtipicos(valores: IntArray): Int

    /** Guarda el retardo de arranque medido (ms): [código, calibración]. */
    external fun ponerArranque(calibracion: IntArray, arranqueMs: Int): IntArray

    /** Ajusta el avance con los tiempos (ms) y lo que avanzó el carro (mm): [código, calibración]. */
    external fun ajustarAvance(calibracion: IntArray, ms: IntArray, mm: IntArray): IntArray

    /** Ajusta el giro de un lado con los tiempos (ms) y los grados girados por 10: [código, calibración]. */
    external fun ajustarGiro(calibracion: IntArray, lado: Int, ms: IntArray, gradosX10: IntArray): IntArray

    /** Calibra el campo de visión de la cámara con un giro medido: grados reales por 10 y grados que midió la cámara por 10: [código, calibración]. */
    external fun ajustarFov(calibracion: IntArray, realX10: Int, visualX10: Int): IntArray

    /** Qué falta medir (bits MEDIDO_*; 0 = todo). */
    external fun faltantes(calibracion: IntArray): Int

    /** Cuántos ms de motor se piden para avanzar `cm`. */
    external fun cmAMs(calibracion: IntArray, cm: Int): Int

    /** Cuántos ms de motor se piden para girar `grados` (negativo = izquierda). */
    external fun gradosAMs(calibracion: IntArray, grados: Int): Int

    /** La calibración como texto "clave=valor" (vacío si no es válida). */
    external fun aTexto(calibracion: IntArray): String

    /** Lee el texto de aTexto: [código, calibración]. */
    external fun deTexto(texto: String): IntArray

    /** Separa el resultado [código, calibración...] en el código y la calibración de 8 enteros. */
    fun desempaquetar(resultado: IntArray): Pair<Int, IntArray> =
        Pair(resultado[0], resultado.copyOfRange(1, 1 + ENTEROS))

    /** Texto sencillo para un código de error. */
    fun textoError(codigo: Int): String = when (codigo) {
        OK -> "Bien"
        ERR_DATOS -> "Los datos no tienen sentido (por ejemplo, avanzó menos con más tiempo). Repite la prueba."
        ERR_RANGO -> "El resultado sale de lo razonable. Revisa los números que anotaste."
        ERR_TEXTO -> "El texto no se puede leer."
        else -> "Datos no válidos (código $codigo)."
    }
}
