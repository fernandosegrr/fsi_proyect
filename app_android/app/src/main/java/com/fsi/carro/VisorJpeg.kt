/*
 * VisorJpeg.kt — Muestra en un ImageView los cuadros JPEG de la sesión de video (ConsumidorVideo).
 *
 * GUÍA DE LECTURA
 *  1. alJpeg() corre en el hilo del video: copia el JPEG a una "casilla única" (si hay uno viejo sin mostrar, se tira; nunca se acumula retraso)
 *     y despierta al hilo decodificador.
 *  2. El hilo decodificador toma el JPEG más nuevo, lo decodifica a Bitmap y lo pasa al hilo principal, que lo pone en el ImageView. Entre los dos también hay una casilla
 *     única: si el hilo principal tarda y se acumulan varios cuadros, solo se pinta el más nuevo (nunca una fila de cuadros viejos a toda prisa).
 *  3. Solo lee y dibuja: NO modifica la imagen (la detección se dibuja en otra vista encima).
 *  4. cerrar() detiene el hilo. Si el modo de video no manda JPEG (raw), simplemente no llega nada.
 *  5. cuadrosMostrados cuenta los cuadros que de verdad se dibujaron (sirve para el registro MANUAL_VIDEO).
 */
package com.fsi.carro

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Handler
import android.os.Looper
import android.widget.ImageView
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference

class VisorJpeg(private val vista: ImageView) : ConsumidorVideo {

    private val principal = Handler(Looper.getMainLooper())
    private val candado = Object()
    private var pendiente: ByteArray? = null
    private var largoPendiente = 0
    @Volatile
    private var activo = true
    private val mostrados = AtomicLong(0)

    // Segunda casilla única (decodificador → hilo principal): si el hilo principal anda ocupado (rehaciendo la pantalla, un aviso animándose) y se acumulan cuadros,
    // solo se pinta el más nuevo. Antes cada cuadro encolaba su propio pintado y, tras un tropiezo, se veían varios seguidos a toda prisa («tirones» y retraso).
    // Estas tres van ANTES de `hilo`: el hilo arranca al crearse y las usa enseguida.
    private val ultimoMapa = AtomicReference<Bitmap?>(null)   // el cuadro decodificado más nuevo que todavía no se pinta
    private val envio = AtomicBoolean(false)                   // true = ya hay un pintado en la cola del hilo principal
    private val pintar = Runnable {
        envio.set(false)
        val mapa = ultimoMapa.getAndSet(null)
        if (mapa != null && activo) {
            vista.setImageBitmap(mapa)
            mostrados.incrementAndGet()
        }
    }

    /** Cuántos cuadros se han dibujado desde que se creó el visor. */
    val cuadrosMostrados: Long get() = mostrados.get()

    private val hilo = Thread({ ciclo() }, "visor-jpeg").also { it.start() }

    override fun alJpeg(bytes: ByteArray, largo: Int) {
        synchronized(candado) {
            val destino = pendiente?.takeIf { it.size >= largo } ?: ByteArray(largo + 4096)
            System.arraycopy(bytes, 0, destino, 0, largo)
            pendiente = destino
            largoPendiente = largo
            candado.notifyAll()
        }
    }

    private fun ciclo() {
        while (activo) {
            var datos: ByteArray?
            var largo: Int
            synchronized(candado) {
                while (activo && pendiente == null) {
                    try { candado.wait(500) } catch (e: InterruptedException) { return }
                }
                datos = pendiente
                largo = largoPendiente
                pendiente = null
            }
            val bytes = datos ?: continue
            val mapa: Bitmap? = try {
                BitmapFactory.decodeByteArray(bytes, 0, largo)
            } catch (e: Exception) {
                null
            }
            if (mapa != null && activo) {
                ultimoMapa.set(mapa)
                if (envio.compareAndSet(false, true)) {
                    principal.post(pintar)
                }
            }
        }
    }

    /** Detiene el hilo y deja de dibujar. */
    fun cerrar() {
        activo = false
        principal.removeCallbacks(pintar)
        envio.set(false)
        ultimoMapa.set(null)
        synchronized(candado) { candado.notifyAll() }
        try { hilo.join(300) } catch (e: InterruptedException) { /* seguimos */ }
    }
}
