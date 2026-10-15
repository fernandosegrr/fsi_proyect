/*
 * VisorJpeg.kt — Muestra en un ImageView los cuadros JPEG de la sesión de video (ConsumidorVideo).
 *
 * GUÍA DE LECTURA
 *  1. alJpeg() corre en el hilo del video: copia el JPEG a una "casilla única" (si hay uno viejo sin mostrar, se tira; nunca se acumula retraso)
 *     y despierta al hilo decodificador.
 *  2. El hilo decodificador toma el JPEG más nuevo, lo decodifica a Bitmap y lo pasa al hilo principal, que lo pone en el ImageView.
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
import java.util.concurrent.atomic.AtomicLong

class VisorJpeg(private val vista: ImageView) : ConsumidorVideo {

    private val principal = Handler(Looper.getMainLooper())
    private val candado = Object()
    private var pendiente: ByteArray? = null
    private var largoPendiente = 0
    @Volatile
    private var activo = true
    private val mostrados = AtomicLong(0)

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
                principal.post {
                    if (activo) {
                        vista.setImageBitmap(mapa)
                        mostrados.incrementAndGet()
                    }
                }
            }
        }
    }

    /** Detiene el hilo y deja de dibujar. */
    fun cerrar() {
        activo = false
        synchronized(candado) { candado.notifyAll() }
        try { hilo.join(300) } catch (e: InterruptedException) { /* seguimos */ }
    }
}
