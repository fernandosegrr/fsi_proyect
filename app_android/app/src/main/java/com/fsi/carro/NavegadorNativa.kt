/*
 * NavegadorNativa.kt — Declara las funciones en C del navegador del modo semiautónomo.
 *
 * GUÍA DE LECTURA
 *  1. `external` significa "la implementación está en código nativo (C)": está en
 *     app/src/main/cpp/puente.c y llama a nucleo_c/navegador.c.
 *  2. Hay UN solo navegador (en C). Se usa en este orden:
 *       cargarRuta() → comenzar() → tick() cada 50 ms → (cancelar() si hace falta).
 *  3. tick() devuelve el comando que el carro debe mantener (un número de 0 a 4).
 *  4. estado() devuelve 11 enteros para mostrar en pantalla (índices más abajo).
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
    const val E_REINTENTOS = 9   // reintentos de esquive de la esquivada actual (retroceder + girar más)
    const val E_FASE = 10        // fase de la maniobra de esquivar (NAV_FASE_* en navegador.h)

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

    /**
     * Dice cuánto se atrasa la cámara en ms (0 con la cámara del teléfono; ~300 con la del carro por WiFi). Los cuadros que todavía
     * muestran al carro sin girar no cuentan y el pedazo de giro que la cámara aún no ve se completa por tiempo. Devuelve 0 o un código de error.
     */
    external fun ponerLatenciaVisual(ms: Int): Int

    /** 1 = además de lo de siempre, un obstáculo LEJANO (cercanía 1) en el centro también frena, tras 2 cuadros y 60 ms. Para el video del carro. */
    external fun frenarLejosCentro(activo: Int): Int

    /**
     * Corredor de trayectoria: dice qué zonas tienen un obstáculo FUERA del corredor (1 = solo avisa, no frena) y la cercanía de lo que está DENTRO
     * (0 = la de siempre) para el siguiente tick. Con todo en 0 el corredor no hace nada.
     */
    external fun ponerCorredor(fueraIzq: Int, fueraCen: Int, fueraDer: Int, cercaniaIzq: Int, cercaniaCen: Int, cercaniaDer: Int)

    /** Dice cuántas celdas de obstáculo tiene cada zona en el cuadro que va al siguiente tick (-1 = no se sabe). El navegador elige el lado con menos celdas. */
    external fun ponerCeldas(izquierda: Int, centro: Int, derecha: Int)

    /**
     * Avisa cuánto vio girar la cámara entre dos cuadros consecutivos: `giroX10` en grados por 10 (positivo a la derecha), `dtMs` el tiempo
     * entre esos cuadros y `confiable` si la cámara estaba segura. Solo cuenta mientras el carro gira; el giro se corta cuando la cámara ve
     * el ángulo pedido (con el tiempo como respaldo). Ver docs/giro_visual.md.
     */
    external fun aporteVisual(giroX10: Int, dtMs: Int, confiable: Boolean)

    /** Estado para mostrar en pantalla (ver los índices E_*). */
    external fun estado(): IntArray

    /** Texto del comando tal como lo entiende el firmware ("FORWARD", "STOP"...). */
    external fun textoComando(comando: Int): String

    /** Texto del estado ("SIGUIENDO", "ESQUIVANDO"...). */
    external fun textoEstado(estado: Int): String
}
