/*
 * RutaEditorNativa.kt — Declara las funciones en C del editor de rutas (nucleo_c/ruta_editor.c).
 *
 * GUÍA DE LECTURA
 *  1. Hay UN editor para toda la app: la ruta que se está dibujando en la pantalla Rutas. Todas las acciones devuelven 0 (bien) o un código negativo
 *     (RE_ERR_* de ruta_editor.h); textoError(codigo) da la frase en español para mostrar.
 *  2. vertices() son los puntos que puso el usuario; celdas() es la ruta ya con los pasos intermedios (la que entiende el navegador).
 *  3. resumen() devuelve 7 enteros (índices R_*): largo en cm, tiempo estimado en ms, giros, vértices, celdas, advertencias (bits ADV_*) y el código.
 *  4. aTexto() / deTexto() sirven para guardar y abrir rutas con nombre.
 *  Todo se llama desde el hilo principal.
 */
package com.fsi.carro

object RutaEditorNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    const val COLUMNAS = 12
    const val FILAS = 8
    const val CELDA_CM = 20

    const val PLANTILLA_RECTA = 0
    const val PLANTILLA_CUADRADO = 1
    const val PLANTILLA_OCHO = 2
    const val PLANTILLA_ZIGZAG = 3

    const val R_LARGO_CM = 0
    const val R_TIEMPO_MS = 1
    const val R_GIROS = 2
    const val R_VERTICES = 3
    const val R_CELDAS = 4
    const val R_ADVERTENCIAS = 5
    const val R_CODIGO = 6

    const val ADV_POCOS_PUNTOS = 1
    const val ADV_GIRO_CERRADO = 2

    external fun iniciar()
    external fun agregar(columna: Int, fila: Int): Int
    external fun mover(indice: Int, columna: Int, fila: Int): Int
    external fun borrarPunto(indice: Int): Int
    external fun limpiar()
    external fun deshacer(): Int
    external fun rehacer(): Int
    external fun puedeDeshacer(): Int
    external fun puedeRehacer(): Int
    external fun plantilla(plantilla: Int): Int
    external fun vertices(): IntArray
    external fun celdas(): IntArray
    external fun resumen(calibracion: IntArray?): IntArray
    external fun aTexto(): String
    external fun deTexto(texto: String): Int
    external fun nombreValido(nombre: String): Int
    external fun textoError(codigo: Int): String
}
