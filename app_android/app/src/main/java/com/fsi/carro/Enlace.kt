/*
 * Enlace.kt — La conexión WebSocket con el carro (o con el servidor falso).
 *
 * GUÍA DE LECTURA
 *  1. Enlace es la interfaz mínima: conectar, enviar un texto y cerrar. EscuchaEnlace recibe
 *     lo que pasa (se abrió, llegó un texto, se cerró).
 *  2. EnlaceOkHttp la implementa con OkHttp. Sus avisos se pasan al hilo principal, así la
 *     lógica (ControlLogica) nunca se ocupa de hilos.
 *  3. Cada conexión nueva lleva un número; los avisos de una conexión vieja se descartan.
 */
package com.fsi.carro

import android.os.Handler
import android.os.Looper
import android.util.Log
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import java.util.concurrent.TimeUnit

/** Lo que el enlace le avisa a quien lo usa. Siempre se llama en el hilo principal. */
interface EscuchaEnlace {
    fun alAbrir()
    fun alMensaje(texto: String)
    fun alCerrar()   // se cerró o falló: la lógica decide si reintenta
}

interface Enlace {
    var escucha: EscuchaEnlace?
    fun conectar(url: String)
    fun enviar(texto: String): Boolean   // false si no hay conexión abierta
    fun cerrar()
}

class EnlaceOkHttp : Enlace {

    override var escucha: EscuchaEnlace? = null

    private val principal = Handler(Looper.getMainLooper())

    // readTimeout 0 = no cortar por silencio: el carro REAL nunca contesta nada, y con el
    // tiempo de espera normal (10 s) OkHttp cerraría la conexión sin motivo.
    private val cliente = OkHttpClient.Builder()
        .connectTimeout(3, TimeUnit.SECONDS)
        .readTimeout(0, TimeUnit.MILLISECONDS)
        .build()

    private var socket: WebSocket? = null
    private var numeroConexion = 0   // solo se toca en el hilo principal

    override fun conectar(url: String) {
        cerrar()
        val peticion = try {
            Request.Builder().url(url).build()
        } catch (e: IllegalArgumentException) {
            Log.w(ETIQUETA, "Dirección no válida: $url")
            principal.post { escucha?.alCerrar() }
            return
        }
        val numero = numeroConexion
        Log.i(ETIQUETA, "conectando a $url")
        socket = cliente.newWebSocket(peticion, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                avisar(numero) { escucha?.alAbrir() }
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                avisar(numero) { escucha?.alMensaje(text) }
            }

            override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
                webSocket.close(code, reason)
                avisar(numero) { escucha?.alCerrar() }
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                Log.w(ETIQUETA, "falla de conexión: ${t.javaClass.simpleName}: ${t.message}")
                avisar(numero) { escucha?.alCerrar() }
            }
        })
    }

    override fun enviar(texto: String): Boolean = socket?.send(texto) ?: false

    /** Cierra con gusto: lo que ya se envió (por ejemplo un STOP) sale antes del cierre. */
    override fun cerrar() {
        socket?.close(1000, "adios")
        socket = null
        numeroConexion++   // los avisos pendientes de la conexión anterior ya no valen
    }

    /** Pasa el aviso al hilo principal, solo si la conexión sigue siendo la actual. */
    private fun avisar(numero: Int, accion: () -> Unit) {
        principal.post {
            if (numero == numeroConexion) {
                accion()
            }
        }
    }

    companion object {
        const val ETIQUETA = "CONTROL"
    }
}
