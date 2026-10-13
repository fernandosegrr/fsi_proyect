/*
 * NavegadorNativa.kt — Declara las funciones en C del navegador del modo semiautónomo.
 *
 * GUÍA DE LECTURA
 *  1. `external` significa "la implementación está en código nativo (C)": está en
 *     app/src/main/cpp/puente.c y llama a nucleo_c/navegador.c.
 *  2. Hay UN solo navegador (en C). Se usa en este orden:
 *       cargarRuta() → comenzar() → tick() cada 50 ms → (cancelar() si hace falta).
 *  3. tick() devuelve el comando que el carro debe mantener (un número de 0 a 4).
 *  4. estado() devuelve 9 enteros para mostrar en pantalla (índices más abajo).
 *  Todo se llama desde el hilo principal.
 */
package com.fsi.carro

object NavegadorNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    // Comandos que devuelve tick() (el mismo orden que ComandoNav en navegador.h).
    const val CMD_STOP = 0
    const val CMD_FORWARD = 1
    const val CMD_REVERSE = 2
    const val CMD_LEFT = 3
    const val CMD_RIGHT = 4

    // Estados (el mismo orden que EstadoNav en navegador.h).
    const val ESPERANDO = 0
    const val SIGUIENDO = 1
    const val DETENIDO_OBSTACULO = 2
    const val ESQUIVANDO = 3
    const val REINCORPORANDO = 4
    const val TERMINADO = 5
    const val ERROR = 6

    // Posiciones dentro del arreglo que devuelve estado().
    const val E_ESTADO = 0
    const val E_X_MM = 1
    const val E_Y_MM = 2
    const val E_RUMBO = 3
    const val E_OBJETIVO = 4
    const val E_PUNTOS = 5
    const val E_ESQUIVADAS = 6
    const val E_MOTIVO_ERROR = 7
    const val E_COMANDO = 8

    /** Le dice al navegador qué calibración del carro usar (8 enteros, ver CalibracionNativa). Se aplica a la próxima ruta que se cargue. Devuelve 0 o un código de error. */
    external fun ponerCalibracion(calibracion: IntArray): Int

    /** Carga la ruta [columna0, fila0, columna1, fila1, ...]. Devuelve 0 si está bien o un código negativo. */
    external fun cargarRuta(puntos: IntArray): Int

    /** Empieza (o retoma) la ruta. `ahoraMs` es un reloj en milisegundos. Devuelve 0 o un código negativo. */
    external fun comenzar(ahoraMs: Long): Int

    /** Cancela: el siguiente tick devuelve STOP y el estado vuelve a ESPERANDO. */
    external fun cancelar()

    /**
     * Una vuelta del navegador. `obstaculos` tiene el mismo formato que ObstaculosNativa.analizar
     * (10 enteros; se usan hay/cercanía de izquierda, centro y derecha) y solo se mira si
     * `hayCuadro` es verdadero. Devuelve el comando (CMD_*).
     */
    external fun tick(ahoraMs: Long, hayCuadro: Boolean, obstaculos: IntArray): Int

    /** Estado para mostrar en pantalla (ver los índices E_*). */
    external fun estado(): IntArray

    /** Texto del comando tal como lo entiende el firmware ("FORWARD", "STOP"...). */
    external fun textoComando(comando: Int): String

    /** Texto del estado ("SIGUIENDO", "ESQUIVANDO"...). */
    external fun textoEstado(estado: Int): String
}
