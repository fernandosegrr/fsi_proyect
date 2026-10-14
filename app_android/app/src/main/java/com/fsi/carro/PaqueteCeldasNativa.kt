/*
 * PaqueteCeldasNativa.kt — Lee el paquete UDP del modo de video CELDAS (nucleo_c/paquete_celdas.c) y define los nombres de los modos de video.
 *
 * GUÍA DE LECTURA
 *  1. La cámara del carro puede mandar, en vez de la imagen, el promedio RGB de cada celda de 10x10 píxeles en un solo paquete UDP por cuadro (modo «celdas»).
 *     El teléfono lo lee con leer() (el mismo código C que lo armó en la cámara) y analiza las celdas con ObstaculosNativa.analizarCeldasConfig.
 *  2. ModoVideo tiene los tres modos y su orden: jpeg (de siempre, el predeterminado), raw (imagen sin comprimir) y celdas.
 */
package com.fsi.carro

/** Los tres modos de video de la cámara del carro. El predeterminado es jpeg. */
object ModoVideo {
    const val JPEG = "jpeg"
    const val RAW = "raw"
    const val CELDAS = "celdas"
    val TODOS = listOf(JPEG, RAW, CELDAS)

    /** El modo que sigue al actual (para el botón que los cambia). */
    fun siguiente(actual: String): String = TODOS[(TODOS.indexOf(actual).coerceAtLeast(0) + 1) % TODOS.size]

    /** Un modo guardado que no se reconoce vale jpeg. */
    fun valido(modo: String?): String = if (modo != null && modo in TODOS) modo else JPEG
}

/** Un paquete de celdas ya leído. `celdas` trae columnas * filas * 3 bytes (R, G, B de cada celda, fila por fila). */
class PaqueteCeldas(
    val celdas: ByteArray,
    val columnas: Int,
    val filas: Int,
    val ancho: Int,
    val alto: Int,
    val numero: Int,        // número de cuadro de la cámara (sube de uno en uno; un salto = se perdieron paquetes)
    val msCamara: Int,      // ms entre la captura y el armado del paquete en la cámara
    val bytes: Int,         // lo que pesó el paquete en la red
    val llegadaMs: Long     // SystemClock.elapsedRealtime() al llegar al teléfono
)

object PaqueteCeldasNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    const val PUERTO = 4210
    const val MAX_CELDAS_BYTES = 64 * 48 * 3

    // Posiciones del resultado de leer() (7 enteros).
    const val R_CODIGO = 0
    const val R_NUMERO = 1
    const val R_MS_CAMARA = 2
    const val R_ANCHO = 3
    const val R_ALTO = 4
    const val R_COLUMNAS = 5
    const val R_FILAS = 6

    /** Lee un paquete de `largo` bytes y deja las celdas en `celdas`. El código (R_CODIGO) vale 0 si estaba bien formado. */
    external fun leer(paquete: ByteArray, largo: Int, celdas: ByteArray): IntArray
}
