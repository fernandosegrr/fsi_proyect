/*
 * TrayectoriaNativa.kt — Declara las dos funciones que están escritas en C.
 *
 * GUÍA DE LECTURA
 *  1. `external` significa "la implementación está en código nativo (C)".
 *  2. Las implementaciones están en app/src/main/cpp/puente.c; los nombres
 *     deben coincidir con las reglas de JNI (Java_com_fsi_carro_TrayectoriaNativa_...).
 *  3. System.loadLibrary("carro_nativo") carga libcarro_nativo.so, que CMake
 *     compila a partir de puente.c y de ../nucleo_c/trayectoria.c.
 */
package com.fsi.carro

object TrayectoriaNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    /** Recibe [columna0, fila0, columna1, fila1, ...] y devuelve el texto con los pasos o el error. */
    external fun calcular(puntos: IntArray): String

    /** Máximo de puntos que acepta el módulo en C. */
    external fun maxPuntos(): Int
}
