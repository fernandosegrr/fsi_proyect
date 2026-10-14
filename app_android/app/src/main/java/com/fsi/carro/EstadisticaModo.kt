/*
 * EstadisticaModo.kt — Los números del video del carro por modo (registro MODO_STATS), para comparar jpeg, raw y celdas.
 *
 * GUÍA DE LECTURA
 *  1. Cada cuadro que llega y se analiza se anota con dos horas: cuándo LLEGÓ al teléfono y cuándo se tomó la decisión (el detector terminó). Su resta es
 *     «ms de llegada a decisión»: incluye lo que esperó en las colas, decodificar y analizar.
 *  2. Cada 5 s se escribe UNA línea (etiqueta MODO_STATS) con: el modo, fps de llegada, hueco máximo entre cuadros, huecos de más de 300 ms, ms de llegada a decisión
 *     (promedio y máximo), bytes por segundo y cuántos cuadros se descartaron por el camino (cola de 1). Solo números, nunca imágenes.
 *  3. Si el video se detiene, la línea sigue saliendo cada 5 s con cuadros=0 (se llama a revisar() aunque no llegue nada).
 *  Se usa desde varios hilos (análisis, celdas y UDP): todos los métodos son sincronizados.
 */
package com.fsi.carro

import android.util.Log

class EstadisticaModo(private val modo: () -> String) {

    companion object {
        const val ETIQUETA = "MODO_STATS"
        const val PERIODO_MS = 5000L
        const val HUECO_LARGO_MS = 300L
    }

    private var inicioMs = 0L
    private var cuadros = 0
    private var sumaDecisionMs = 0L
    private var maxDecisionMs = 0L
    private var ultimaLlegadaMs = 0L
    private var maxHuecoMs = 0L
    private var huecosLargos = 0
    private var bytes = 0L
    private var descartados = 0
    private var perdidos = 0                 // paquetes de celdas que faltaron (el número de cuadro saltó)
    private var jpegParalelo = 0             // cuadros JPEG que llegaron mientras el modo era celdas
    private var sumaRttMs = 0.0
    private var maxRttMs = 0.0
    private var nRtt = 0
    private var sumaCamaraMs = 0L            // ms entre captura y paquete/envío que dice la cámara
    private var nCamara = 0

    /** Anota un cuadro analizado: hora de llegada, hora de la decisión y bytes que ocupó en la red. */
    @Synchronized
    fun cuadro(llegadaMs: Long, decisionMs: Long, bytesDelCuadro: Int) {
        if (inicioMs == 0L) {
            inicioMs = llegadaMs
        }
        cuadros++
        val tarda = (decisionMs - llegadaMs).coerceAtLeast(0L)
        sumaDecisionMs += tarda
        if (tarda > maxDecisionMs) {
            maxDecisionMs = tarda
        }
        if (ultimaLlegadaMs != 0L) {
            val hueco = llegadaMs - ultimaLlegadaMs
            if (hueco > maxHuecoMs) {
                maxHuecoMs = hueco
            }
            if (hueco > HUECO_LARGO_MS) {
                huecosLargos++
            }
        }
        ultimaLlegadaMs = llegadaMs
        bytes += bytesDelCuadro
    }

    /** Anota cuadros que se tiraron en una cola porque llegó uno más nuevo. */
    @Synchronized
    fun descartado(cuantos: Int = 1) {
        descartados += cuantos
    }

    /** Paquetes de celdas que se perdieron por el camino. */
    @Synchronized
    fun perdidos(cuantos: Int) {
        if (cuantos > 0) {
            perdidos += cuantos
        }
    }

    /** Un cuadro JPEG que llegó mientras el modo era celdas (el JPEG «para ver» que va en paralelo). */
    @Synchronized
    fun jpegParalelo() {
        jpegParalelo++
    }

    /** Un ida y vuelta de la red medido con PING/PONG por UDP, en ms. */
    @Synchronized
    fun rtt(ms: Double) {
        nRtt++
        sumaRttMs += ms
        if (ms > maxRttMs) {
            maxRttMs = ms
        }
    }

    /** Lo que dice la cámara que tardó entre capturar el cuadro y mandarlo (ms). */
    @Synchronized
    fun camara(ms: Int) {
        nCamara++
        sumaCamaraMs += ms
    }

    /** Si ya pasaron 5 s desde la última línea, la escribe en el log y empieza otra ventana. Se llama seguido (con o sin cuadros). */
    @Synchronized
    fun revisar(ahoraMs: Long) {
        if (inicioMs == 0L) {
            inicioMs = ahoraMs
            return
        }
        val duracion = ahoraMs - inicioMs
        if (duracion < PERIODO_MS) {
            return
        }
        val segundos = duracion / 1000.0
        val huecoActual = if (ultimaLlegadaMs == 0L) 0L else ahoraMs - ultimaLlegadaMs
        Log.i(ETIQUETA, String.format(java.util.Locale.US,
            "modo=%s fps=%.1f cuadros=%d hueco_max_ms=%d huecos_mayores_300ms=%d hueco_actual_ms=%d llegada_a_decision_ms=%.1f llegada_a_decision_max_ms=%d bytes_s=%d descartados=%d perdidos=%d " +
                "jpeg_paralelo_fps=%.1f rtt_medio_ms=%.1f rtt_mitad_ms=%.1f rtt_max_ms=%.1f camara_captura_a_envio_ms=%.1f",
            modo(), cuadros / segundos, cuadros, maxOf(maxHuecoMs, huecoActual), huecosLargos + (if (huecoActual > HUECO_LARGO_MS) 1 else 0), huecoActual,
            if (cuadros > 0) sumaDecisionMs.toDouble() / cuadros else 0.0, maxDecisionMs, (bytes / segundos).toLong(), descartados, perdidos,
            jpegParalelo / segundos, if (nRtt > 0) sumaRttMs / nRtt else 0.0, if (nRtt > 0) sumaRttMs / nRtt / 2 else 0.0, maxRttMs,
            if (nCamara > 0) sumaCamaraMs.toDouble() / nCamara else 0.0))
        inicioMs = ahoraMs
        cuadros = 0
        sumaDecisionMs = 0
        maxDecisionMs = 0
        maxHuecoMs = 0
        huecosLargos = 0
        bytes = 0
        descartados = 0
        perdidos = 0
        jpegParalelo = 0
        sumaRttMs = 0.0
        maxRttMs = 0.0
        nRtt = 0
        sumaCamaraMs = 0
        nCamara = 0
    }
}
