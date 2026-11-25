/*
 * RefrescoAcotado.kt — Junta una ráfaga de avisos en UN solo refresco de pantalla (como mucho uno cada `periodoMs`).
 *
 * GUÍA DE LECTURA
 *  1. Para qué: las sesiones avisan MUCHAS veces por segundo (el modo semiautónomo cada 50 ms, los sensores del carro cada 100 ms, cada PONG, cada tick del video)
 *     y cada aviso rehacía la pantalla entera (textos, chips, avisos). Con el teléfono ocupado en eso el video se entrecortaba y lo animado se reiniciaba:
 *     eran los «parpadeos». Aquí se limita CUÁNTAS veces se redibuja, no QUÉ se dibuja.
 *  2. Cómo: pedir() ejecuta la acción YA si pasó el periodo desde la última vez (un cambio aislado, por ejemplo el PARO, se ve al instante). Si no, deja UN solo
 *     refresco pendiente para cuando se cumpla el periodo: los avisos que lleguen mientras tanto no suman nada (el refresco lee el estado de ESE momento).
 *  3. Todo corre en el hilo principal. cancelar() quita el refresco pendiente (al salir de la pantalla).
 *  4. No cambia ninguna lógica del carro ni del navegador: solo cuándo se redibuja. Las llamadas directas a la acción (un toque de botón) siguen siendo inmediatas.
 */
package com.fsi.carro

import android.os.Handler
import android.os.Looper
import android.os.SystemClock

class RefrescoAcotado(private val periodoMs: Long, private val accion: () -> Unit) {

    private val manejador = Handler(Looper.getMainLooper())
    private var ultimoMs = Long.MIN_VALUE / 2   // «hace mucho»: el primer pedir() corre de inmediato
    private var pendiente = false

    private val diferido = Runnable {
        pendiente = false
        correr()
    }

    /** Pide un refresco: inmediato si ya pasó el periodo; si no, uno solo cuando se cumpla (los pedidos intermedios se funden en ese). */
    fun pedir() {
        if (pendiente) {
            return
        }
        val espera = ultimoMs + periodoMs - SystemClock.uptimeMillis()
        if (espera <= 0L) {
            correr()
        } else {
            pendiente = true
            manejador.postDelayed(diferido, espera)
        }
    }

    /** Quita el refresco pendiente (la pantalla se va). */
    fun cancelar() {
        manejador.removeCallbacks(diferido)
        pendiente = false
    }

    private fun correr() {
        ultimoMs = SystemClock.uptimeMillis()
        accion()
    }
}
