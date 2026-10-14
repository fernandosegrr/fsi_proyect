/*
 * MovimientoVisualNativa.kt — Declara las funciones en C que miden cuánto giró el carro entre dos cuadros de la
 * cámara (nucleo_c/movimiento_visual.c; ver docs/giro_visual.md).
 *
 * GUÍA DE LECTURA
 *  1. procesar() recibe cada cuadro (bytes RGB, ancho y alto) y el campo de visión de la cámara. Lo compara con el cuadro
 *     anterior (el código en C lo recuerda) y devuelve 7 enteros (índices de abajo).
 *  2. Solo se mira la mitad de arriba del cuadro (el horizonte). El giro es positivo a la derecha, como en el navegador.
 *  3. Si la escena casi no tiene detalles, o hay varias coincidencias parecidas, el resultado trae `valido` = 0 con el motivo:
 *     ese giro NO se debe sumar.
 *  4. Solo se llama desde un hilo (el de la cámara).
 */
package com.fsi.carro

object MovimientoVisualNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    // Posiciones del arreglo que devuelve procesar().
    const val CODIGO = 0            // 0 = bien; negativo = error de argumentos o de tamaño
    const val VALIDO = 1            // 1 = el corrimiento es confiable
    const val MOTIVO = 2            // 0 bien; 1 sin cuadro previo; 2 escena uniforme; 3 coincidencia débil; 4 ambiguo; 5 fuera de rango
    const val CORRIMIENTO_X16 = 3   // píxeles que se movió el contenido, por 16
    const val CONFIANZA_MIL = 4     // de 0 a 1000
    const val GIRO_X10 = 5          // giro del carro en grados por 10 (+ = a la derecha; 0 si no es confiable)
    const val ENERGIA = 6           // energía de bordes de la huella (para depurar)

    const val FOV_DEFECTO_X10 = 600 // campo de visión horizontal por defecto: 60.0 grados

    /** Procesa un cuadro RGB y lo compara con el anterior. `fovX10` es el campo de visión horizontal en grados por 10. */
    external fun procesar(rgb: ByteArray, ancho: Int, alto: Int, fovX10: Int): IntArray

    /** Olvida el cuadro anterior: el siguiente no se compara con nada. */
    external fun reiniciar()

    /** Texto sencillo del motivo por el que un giro no es confiable. */
    fun textoMotivo(motivo: Int): String = when (motivo) {
        0 -> "bien"
        1 -> "primer cuadro"
        2 -> "escena sin detalles (pared lisa)"
        3 -> "no se parece a nada"
        4 -> "textura repetida (ambiguo)"
        5 -> "giro demasiado grande entre cuadros"
        else -> "motivo $motivo"
    }
}
