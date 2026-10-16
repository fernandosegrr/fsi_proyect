/*
 * CalibracionNativa.kt — Declara las funciones en C de la calibración del carro (nucleo_c/calibracion.c).
 *
 * GUÍA DE LECTURA
 *  1. La calibración viaja como un IntArray de 10 enteros (índices CAL_* más abajo). Las funciones que
 *     la cambian devuelven 11 enteros: [código, calibración...]; usa desempaquetar() para separarlos.
 *  2. Los decimales van multiplicados por 10 (sufijo _x10): 420 = 42.0 ms por cm.
 *  3. Cómo se mide (lo hace CalibracionActivity): arranque, avance (varios tiempos), giro a la
 *     izquierda y giro a la derecha. Ver docs/calibracion.md.
 */
package com.fsi.carro

object CalibracionNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    // Posiciones dentro de la calibración (10 enteros).
    const val CAL_MEDIDOS = 0
    const val CAL_ARRANQUE_MS = 1
    const val CAL_MS_POR_CM_X10 = 2
    const val CAL_FRENADO_MM = 3
    const val CAL_MS_GRADO_IZQ_X10 = 4
    const val CAL_MS_GRADO_DER_X10 = 5
    const val CAL_FRENADO_GRADOS_IZQ_X10 = 6
    const val CAL_FRENADO_GRADOS_DER_X10 = 7
    const val CAL_FOV_GRADOS_X10 = 8    // campo de visión horizontal de la cámara, grados por 10 (600 = 60.0)
    const val CAL_MS_POR_CM_ATRAS_X10 = 9   // ms por cm RETROCEDIENDO por 10; vale solo si el bit MEDIDO_ATRAS está puesto (si no, se usa el del avance)
    const val ENTEROS = 10

    // Bits de "medidos" / "faltantes".
    const val MEDIDO_ARRANQUE = 1
    const val MEDIDO_AVANCE = 2
    const val MEDIDO_GIRO_IZQ = 4
    const val MEDIDO_GIRO_DER = 8
    const val MEDIDO_TODO = 15          // los cuatro obligatorios: arranque, avance y los dos giros
    const val MEDIDO_FOV = 16           // opcional: el campo de visión de la cámara (giro visual)
    const val MEDIDO_ATRAS = 32         // opcional: los ms por cm al retroceder (si falta, se usa el del avance)

    const val LADO_IZQUIERDA = -1
    const val LADO_DERECHA = 1

    // Códigos de error (calibracion.h).
    const val OK = 0
    const val ERR_ARGUMENTO = -1
    const val ERR_DATOS = -2
    const val ERR_RANGO = -3
    const val ERR_TEXTO = -4

    /** Calibración por defecto (la de ejemplo): [código, 10 enteros]. */
    external fun defecto(): IntArray

    /** Promedio de los valores sin los datos atípicos (los que se alejan mucho de la mediana). */
    external fun promedioSinAtipicos(valores: IntArray): Int

    /** Guarda el retardo de arranque medido (ms): [código, calibración]. */
    external fun ponerArranque(calibracion: IntArray, arranqueMs: Int): IntArray

    /** Ajusta el avance con los tiempos (ms) y lo que avanzó el carro (mm): [código, calibración]. */
    external fun ajustarAvance(calibracion: IntArray, ms: IntArray, mm: IntArray): IntArray

    /** Ajusta el retroceso con los tiempos (ms de REVERSE) y lo que retrocedió el carro (mm): [código, calibración]. Solo cambia los ms por cm hacia atrás. */
    external fun ajustarRetroceso(calibracion: IntArray, ms: IntArray, mm: IntArray): IntArray

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

    // ── Ajuste fino: giro de 90° y recta (ver docs/calibracion.md y docs/perfiles_velocidad.md) ──
    const val FACTOR_GIRO_DEFECTO = 1000   // milésimas: 1000 = sin corrección
    const val GIRO_SE_PASO_MUCHO = 0
    const val GIRO_SE_PASO_UN_POCO = 1
    const val GIRO_JUSTO = 2
    const val GIRO_QUEDO_CORTO = 3
    const val BALANCE_MIN = 800
    const val BALANCE_MAX = 1000
    const val EJES_MM_DEFECTO = 120
    const val RECTA_OK_MM = 10
    const val RECTA_MAX_ITERACIONES = 4

    /** Los grados que se suponen girados en cada opción de «¿cómo giró?» con 90 pedidos (0 si no existe la opción). */
    external fun gradosDeOpcionGiro(opcion: Int): Int

    /** Factor nuevo del giro (milésimas) si se pidieron `pedidos` grados y el carro giró `reales`: [código, factor]. */
    external fun factorGiroNuevo(factorMil: Int, pedidos: Int, reales: Int): IntArray

    /** La calibración con el factor de giro de cada lado aplicado (los ms por grado cambian): [código, calibración]. */
    external fun conFactorGiro(calibracion: IntArray, factorIzqMil: Int, factorDerMil: Int): IntArray

    /** Balance nuevo (milésimas, de 800 a 1000) tras una recta: desvío en mm (+ derecha), recorrido y ancho entre ruedas en mm: [código, balance]. */
    external fun balanceNuevo(balanceMil: Int, desvioMm: Int, recorridoMm: Int, ejesMm: Int): IntArray

    /** La calibración como texto "clave=valor" (vacío si no es válida). */
    external fun aTexto(calibracion: IntArray): String

    /** Lee el texto de aTexto: [código, calibración]. */
    external fun deTexto(texto: String): IntArray

    /** Separa el resultado [código, calibración...] en el código y la calibración de 10 enteros. */
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
