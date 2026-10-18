/*
 * CalGiroNativa.kt — Puente a nucleo_c/calgiro.c: la calibración del giro con el giroscopio (J12).
 *
 * GUÍA DE LECTURA
 *  1. iniciar() con la calibración guardada, y después tick() cada ~50 ms con el ángulo que dijo SENS. tick() devuelve el comando que el carro debe mantener (CMD_*).
 *  2. estado() dice en qué va (giro actual de 6) y si hubo un error; resultado() da la calibración nueva y, para cada lado, cuánto cambió contra lo guardado.
 *  3. La pantalla es CalibrarGiroActivity; la lógica (cuándo girar, cuándo medir) está toda en el código en C, con sus pruebas en nucleo_c/pruebas_calgiro.c.
 */
package com.fsi.carro

object CalGiroNativa {
    init {
        System.loadLibrary("carro_nativo")
    }

    // Lo que devuelve tick().
    const val CMD_STOP = 0
    const val CMD_IZQ = 1
    const val CMD_DER = 2

    // Estados (E_ESTADO de estado()).
    const val PARADO = 0
    const val PREPARANDO = 1
    const val GIRANDO = 2
    const val ASENTANDO = 3
    const val LISTO = 4
    const val ERROR = 5
    const val CANCELADO = 6

    // Índices de estado().
    const val E_ESTADO = 0
    const val E_ERROR = 1
    const val E_GIRO_ACTUAL = 2
    const val E_CODIGO_AJUSTE = 3
    const val E_COMANDO = 4

    const val TOTAL_GIROS = 6

    // Índices de resultado(): [0] = 1 si hay resultado; [1..16] = la calibración nueva; después, para cada lado (izquierda: 17, derecha: 32), 15 enteros.
    const val R_HAY = 0
    const val R_CALIBRACION = 1
    const val R_LADO_IZQ = 17
    const val R_LADO_DER = 32
    const val L_MS_ANTES = 0
    const val L_MS_DESPUES = 1
    const val L_FRENADO_ANTES = 2
    const val L_FRENADO_DESPUES = 3
    const val L_CAMBIO_MS_PCT = 4
    const val L_CAMBIO_FRENADO_PCT = 5
    const val L_CAMBIO_GRANDE = 6
    const val L_ERROR_MAX = 7
    const val L_DURACIONES = 8   // 3 tiempos mandados (ms)
    const val L_GRADOS = 11      // 3 ángulos medidos (grados x10)

    /** Empieza con la calibración guardada (16 enteros, ver CalibracionNativa). Devuelve 0 o -1. */
    external fun iniciar(calibracion: IntArray, ahoraMs: Long): Int

    /** Una vuelta: devuelve CMD_STOP, CMD_IZQ o CMD_DER. `anguloX10` es el del giroscopio (POSITIVO = izquierda) y `hayAngulo` si es reciente. */
    external fun tick(ahoraMs: Long, hayAngulo: Boolean, anguloX10: Int): Int

    external fun cancelar()

    /** [estado, error, giro actual, código del ajuste, comando] */
    external fun estado(): IntArray

    /** Ver los índices R_* y L_*. Solo hay datos (R_HAY = 1) en el estado LISTO. */
    external fun resultado(): IntArray

    /** Frase corta del error (vacía si no hay). */
    external fun textoError(error: Int): String
}
