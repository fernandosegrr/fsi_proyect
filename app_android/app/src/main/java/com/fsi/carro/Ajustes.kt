/*
 * Ajustes.kt — Las preferencias de la app que NO son del detector (las del detector están en ConfigVision).
 *
 * GUÍA DE LECTURA
 *  1. Todo se guarda en SharedPreferences ("ajustes"): sobrevive al cerrar la app.
 *  2. tema: "oscuro" (predeterminado), "claro" o "sistema" (sigue al teléfono). Lo aplica Diseno.aplicarTema().
 *  3. direccionCarro: el WebSocket del carro (la DevKit); la dirección del video está en ConfigVision.urlVideoCarro.
 *  4. perfilManual: cómo va el carro en la pantalla Manual: "rapido" (perfil MANUAL, 255; el predeterminado) o "lento" (perfil AUTO, 210).
 *     Las rutas siempre van en AUTO.
 *  5. videoEnManual: si la pantalla Manual abre el video del carro. Si el video estorba al control, se apaga aquí o con el interruptor de Manual
 *     (docs/manual_video.md).
 *  6. verDeteccion: dibujar encima del video los números de la detección (zona, corredor y cercanía), sin tocar la imagen.
 *  7. asistenteVisto: el asistente de primera vez ya se mostró (o se saltó).
 *  8. pruebaFinal: qué pasos de la prueba final ya se marcaron como hechos (una cadena con los números separados por coma).
 *  9. usarGiroscopio / usarSensorDistancia (J11): los interruptores de la pantalla Sensores. Encendidos por defecto; solo valen si el carro avisa que el sensor funciona.
 */
package com.fsi.carro

import android.content.Context
import android.content.SharedPreferences

class Ajustes(contexto: Context) {

    companion object {
        const val TEMA_OSCURO = "oscuro"
        const val TEMA_CLARO = "claro"
        const val TEMA_SISTEMA = "sistema"
        const val PERFIL_RAPIDO = "rapido"
        const val PERFIL_LENTO = "lento"
        const val DIRECCION_CARRO_DEFECTO = "ws://192.168.4.1/ws"
    }

    private val preferencias: SharedPreferences = contexto.getSharedPreferences("ajustes", Context.MODE_PRIVATE)

    var tema: String
        get() = when (val t = preferencias.getString("tema", TEMA_OSCURO)) {
            TEMA_CLARO, TEMA_SISTEMA -> t
            else -> TEMA_OSCURO
        }
        set(valor) = preferencias.edit().putString("tema", valor).apply()

    var direccionCarro: String
        get() = preferencias.getString("direccion_carro", DIRECCION_CARRO_DEFECTO) ?: DIRECCION_CARRO_DEFECTO
        set(valor) = preferencias.edit().putString("direccion_carro", valor.trim().ifEmpty { DIRECCION_CARRO_DEFECTO }).apply()

    var perfilManual: String
        get() = if (preferencias.getString("perfil_manual", PERFIL_RAPIDO) == PERFIL_LENTO) PERFIL_LENTO else PERFIL_RAPIDO
        set(valor) = preferencias.edit().putString("perfil_manual", valor).apply()

    /** El perfil del carro que corresponde a la pantalla Manual según el ajuste. */
    val perfilDeManual: ControlLogica.Perfil
        get() = if (perfilManual == PERFIL_LENTO) ControlLogica.Perfil.AUTO else ControlLogica.Perfil.MANUAL

    var videoEnManual: Boolean
        get() = preferencias.getBoolean("video_en_manual", true)
        set(valor) = preferencias.edit().putBoolean("video_en_manual", valor).apply()

    var verDeteccion: Boolean
        get() = preferencias.getBoolean("ver_deteccion", false)
        set(valor) = preferencias.edit().putBoolean("ver_deteccion", valor).apply()

    var usarGiroscopio: Boolean
        get() = preferencias.getBoolean("usar_giroscopio", true)
        set(valor) = preferencias.edit().putBoolean("usar_giroscopio", valor).apply()

    var usarSensorDistancia: Boolean
        get() = preferencias.getBoolean("usar_sensor_distancia", true)
        set(valor) = preferencias.edit().putBoolean("usar_sensor_distancia", valor).apply()

    var autoridadLaser: Boolean
        // J14: el dueño decidió que la Autoridad del láser quede siempre encendida.
        // El valor guardado en el teléfono manda; este es solo el valor de una instalación nueva.
        get() = preferencias.getBoolean("autoridad_laser", true)
        set(valor) = preferencias.edit().putBoolean("autoridad_laser", valor).apply()

    var vibracion: Boolean
        get() = preferencias.getBoolean("vibracion", true)
        set(valor) = preferencias.edit().putBoolean("vibracion", valor).apply()

    var asistenteVisto: Boolean
        get() = preferencias.getBoolean("asistente_visto", false)
        set(valor) = preferencias.edit().putBoolean("asistente_visto", valor).apply()

    /** Pasos de la prueba final marcados como hechos (números separados por coma). */
    var pruebaFinal: Set<Int>
        get() = (preferencias.getString("prueba_final", "") ?: "").split(",").mapNotNull { it.trim().toIntOrNull() }.toSet()
        set(valor) = preferencias.edit().putString("prueba_final", valor.sorted().joinToString(",")).apply()
}
