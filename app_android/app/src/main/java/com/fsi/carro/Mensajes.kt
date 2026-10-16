/*
 * Mensajes.kt — Los mensajes de error y de aviso de la app, en un solo lugar.
 *
 * GUÍA DE LECTURA
 *  1. Regla (docs/diseno.md): todo error dice QUÉ PASÓ y QUÉ HACER, en una frase cada cosa, sin códigos crudos.
 *  2. Cada función devuelve un Mensaje que la pantalla muestra con AvisoBanda.mostrar(nivel, m.quePaso, m.queHacer, ...).
 *  3. Los códigos y detalles técnicos NO se muestran: se quedan en el registro (logcat) y en docs/.
 *  4. RedCarro.enRedDelCarro() ayuda a elegir entre «el teléfono no está en la red del carro» y «el carro no responde».
 */
package com.fsi.carro

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import java.net.Inet4Address

data class Mensaje(val quePaso: String, val queHacer: String)

object RedCarro {

    /** true si el teléfono tiene una conexión WiFi con dirección 192.168.4.x (la red del carro). Solo mira las redes; no pide permiso de ubicación. */
    @Suppress("DEPRECATION")
    fun enRedDelCarro(contexto: Context): Boolean {
        val cm = contexto.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager ?: return false
        for (red in cm.allNetworks) {
            val capacidades = cm.getNetworkCapabilities(red) ?: continue
            if (!capacidades.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
                continue
            }
            val propiedades = cm.getLinkProperties(red) ?: continue
            for (direccion in propiedades.linkAddresses) {
                val ip = direccion.address
                if (ip is Inet4Address) {
                    val b = ip.address
                    if ((b[0].toInt() and 0xFF) == 192 && (b[1].toInt() and 0xFF) == 168 && (b[2].toInt() and 0xFF) == 4) {
                        return true
                    }
                }
            }
        }
        return false
    }
}

object Mensajes {

    fun sinRedDelCarro() = Mensaje(
        "El teléfono no está en la red del carro",
        "Conéctate al WiFi «CarroFSI» en los ajustes del teléfono, vuelve a la app y toca Conectar."
    )

    fun carroNoResponde() = Mensaje(
        "El carro no responde",
        "Revisa que el carro esté encendido y cerca, y toca Conectar."
    )

    fun conexionPerdida() = Mensaje(
        "Se perdió la conexión con el carro",
        "El carro se detuvo. La app intenta volver sola; si no, toca Conectar."
    )

    fun perfilSinConfirmar() = Mensaje(
        "El carro no confirmó su velocidad",
        "Puede tener un firmware viejo. Carga el firmware nuevo en el carro y vuelve a intentar."
    )

    /** La cámara no contestó. Dice la dirección que se intentó (la de ConfigVision) y que la app reintenta sola cada 3 s (SES_REINTENTO_MS en sesion_compartida.h). */
    fun sinVideo(contexto: Context) = Mensaje(
        "La cámara no responde",
        "Se intentó con ${direccionDeLaCamara(contexto)} y se reintenta sola cada 3 s. Puedes seguir manejando sin video; si sigue igual, apaga y enciende la cámara."
    )

    /** La IP (o nombre) de la cámara a la que intenta conectarse el video, sin puerto ni ruta. */
    fun direccionDeLaCamara(contexto: Context): String {
        val url = ConfigVision(contexto).urlVideoCarro
        return try {
            java.net.URL(url).host.ifEmpty { url }
        } catch (e: java.net.MalformedURLException) {
            url
        }
    }

    fun videoApagado() = Mensaje(
        "El video está apagado",
        "Enciende el interruptor «Video» si quieres ver lo que ve el carro."
    )

    fun videoSinImagenEnModo(modo: String) = Mensaje(
        "El modo de video «$modo» no manda imagen",
        "Cambia el modo a jpeg en «Visión y diagnóstico» para ver el video aquí."
    )

    fun sinCamaraEnRuta() = Mensaje(
        "La cámara dejó de mandar imágenes",
        "La ruta se detuvo por seguridad. Revisa la cámara del carro y vuelve a iniciar."
    )

    fun rutaNoValida() = Mensaje(
        "La ruta no se puede recorrer",
        "Revisa que tenga al menos dos puntos y que cada punto esté pegado al anterior."
    )

    fun sinCalibrar(falta: String) = Mensaje(
        "El carro no está calibrado del todo",
        "$falta Puedes calibrarlo en Más → Calibración; mientras tanto se usan valores de ejemplo."
    )
}
