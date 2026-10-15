/*
 * AppCarro.kt — La aplicación: se crea una vez al abrir la app, antes de cualquier pantalla.
 *
 * GUÍA DE LECTURA
 *  1. Aplica el tema guardado (oscuro por defecto, claro o el del sistema) para que TODAS las pantallas nazcan con el tema correcto.
 */
package com.fsi.carro

import android.app.Application

class AppCarro : Application() {
    override fun onCreate() {
        super.onCreate()
        Diseno.aplicarTema(this)
    }
}
