/*
 * PantallasLog.kt — El registro de «esta pantalla abrió sin errores».
 *
 * GUÍA DE LECTURA
 *  1. Cada pantalla llama a PantallasLog.abierta("NOMBRE") cuando termina de armarse. Escribe una línea en logcat con la etiqueta PANTALLA.
 *  2. scripts/probar_pantallas.py abre cada pantalla con adb (extra «pantalla» de MainActivity) y comprueba que salga esa línea y que no haya
 *     «FATAL EXCEPTION» en el registro.
 */
package com.fsi.carro

import android.util.Log

object PantallasLog {
    const val ETIQUETA = "PANTALLA"

    fun abierta(nombre: String) {
        Log.i(ETIQUETA, "abierta=$nombre ok")
    }
}
