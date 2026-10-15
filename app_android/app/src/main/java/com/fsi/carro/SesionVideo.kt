/*
 * SesionVideo.kt — La sesión de video ÚNICA y compartida de la cámara del carro.
 *
 * GUÍA DE LECTURA
 *  1. La cámara del carro atiende UN solo cliente de video a la vez. Por eso hay un solo VideoCarro en toda la app y lo comparten las
 *     pantallas que lo necesitan: Inicio, Manual, Visión y Ejecutar ruta. Cada una ENTRA con su número de usuario (SesionNativa.USUARIO_*)
 *     y, si quiere los cuadros, con un ConsumidorVideo; al irse SALE. El video se abre con el primero que entra y se cierra al salir el último
 *     (con 1.5 s de gracia para que cambiar de pantalla o girar el teléfono no lo corte).
 *  2. Las decisiones (abrir, compartir, cerrar, fallo, reintento) las toma el código en C (nucleo_c/sesion_compartida.c, con sus pruebas);
 *     aquí solo se ejecutan las acciones: crear el VideoCarro, detenerlo o volverlo a crear.
 *  3. Si el video falla, las pantallas lo ven en `mensajeFallo` y pueden mostrar «Reintentar» (reintentar()); además, la sesión reabre sola cada 5 s.
 *     El manejo del carro NO depende del video: la conexión de control es otra (SesionCarro).
 *  4. Todas las llamadas se hacen desde el hilo principal. Los cuadros llegan en el hilo del video y se reparten tal cual: cada consumidor copia lo
 *     que necesite (el arreglo se reutiliza).
 */
package com.fsi.carro

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.CopyOnWriteArrayList

/** Lo que una pantalla quiere recibir del video. Todo es opcional. Se llama en el hilo del video. */
interface ConsumidorVideo {
    fun alJpeg(bytes: ByteArray, largo: Int) {}
    fun alCrudo(bytes: ByteArray, largo: Int, ancho: Int, alto: Int, numero: Int, msCamara: Int) {}
    fun alCeldas(paquete: PaqueteCeldas) {}
    fun alRtt(ms: Double) {}
    fun alFalloModo(modoFallido: String, motivo: String) {}
}

object SesionVideo {

    const val ETIQUETA = "SESION_VIDEO"
    private const val PERIODO_TICK_MS = 250L

    private val principal = Handler(Looper.getMainLooper())
    private val consumidores = ConcurrentHashMap<Int, ConsumidorVideo>()
    private val ninguno = object : ConsumidorVideo {}
    private val escuchas = CopyOnWriteArrayList<() -> Unit>()
    private var contexto: Context? = null
    private var tickProgramado = false
    private var reabriendo = false
    private var cerrando = false          // un VideoCarro se está deteniendo (en otro hilo)
    private var abrirAlTerminar = false   // alguien entró mientras se cerraba el anterior: se abre apenas termine

    /** El VideoCarro que está abierto ahora (null si no hay). Cambia si la sesión se reabre. */
    @Volatile
    var actual: VideoCarro? = null
        private set

    /** Lo último que dijo el video si falló ("" = bien). */
    @Volatile
    var mensajeFallo = ""
        private set

    /** Último ida y vuelta de la red por UDP (PING/PONG), en ms; 0 si todavía no hay. */
    @Volatile
    var rttMs = 0.0
        private set

    /** Cuántos cuadros por segundo llegan (de los últimos 10 s); 0 si no hay video. */
    val fps: Double
        get() = if (actual == null) 0.0 else VideoStatsNativa.resumen(SystemClock.elapsedRealtime())[VideoStatsNativa.FPS_X10] / 10.0

    /** El estado de la sesión (SesionNativa.CERRADA, ABRIENDO, ABIERTA o FALLO). */
    val estado: Int get() = SesionNativa.estado(SesionNativa.RECURSO_VIDEO)[SesionNativa.E_ESTADO]

    val usuarios: Int get() = SesionNativa.estado(SesionNativa.RECURSO_VIDEO)[SesionNativa.E_USUARIOS]

    /** Pide que se avise (en el hilo principal) cuando cambie el estado del video. */
    fun escuchar(alCambiar: () -> Unit) {
        escuchas.addIfAbsent(alCambiar)
    }

    fun dejarDeEscuchar(alCambiar: () -> Unit) {
        escuchas.remove(alCambiar)
    }

    /** Una pantalla entra a usar el video. `consumidor` recibe los cuadros (null = solo quiere saber el estado). */
    fun entrar(ctx: Context, usuario: Int, consumidor: ConsumidorVideo? = null) {
        contexto = ctx.applicationContext
        consumidores[usuario] = consumidor ?: ninguno
        val accion = SesionNativa.entrar(SesionNativa.RECURSO_VIDEO, usuario, SystemClock.elapsedRealtime())
        Log.i(ETIQUETA, "entra usuario=$usuario accion=$accion usuarios=$usuarios estado=$estado")
        if (accion == SesionNativa.ACCION_ABRIR) {
            if (cerrando) {
                abrirAlTerminar = true   // la cámara atiende un solo cliente: se espera a que el viejo termine de cerrarse
            } else {
                abrir()
            }
        }
        programarTick()
        avisar()
    }

    /** Una pantalla deja de usar el video. Si era la última, se cierra tras la gracia. */
    fun salir(usuario: Int) {
        consumidores.remove(usuario)
        SesionNativa.salir(SesionNativa.RECURSO_VIDEO, usuario, SystemClock.elapsedRealtime())
        Log.i(ETIQUETA, "sale usuario=$usuario usuarios=$usuarios estado=$estado")
        programarTick()
        avisar()
    }

    /** El usuario tocó «Reintentar»: se cierra y se vuelve a abrir el video ya. */
    fun reintentar() {
        val accion = SesionNativa.reintentar(SesionNativa.RECURSO_VIDEO, SystemClock.elapsedRealtime())
        Log.i(ETIQUETA, "reintentar accion=$accion")
        if (accion == SesionNativa.ACCION_REABRIR) {
            reabrir()
        }
        avisar()
    }

    /** Cambia el modo de video (jpeg, raw o celdas): se guarda y, si el video está abierto, se vuelve a abrir con el modo nuevo. */
    fun cambiarModo(modo: String) {
        val ctx = contexto
        if (ctx != null) {
            ConfigVision(ctx).modoVideo = modo
        }
        if (actual != null) {
            reabrir()
        }
    }

    // ── Ejecutar las acciones ─────────────────────────────────────────────

    private fun abrir() {
        val ctx = contexto ?: return
        val config = ConfigVision(ctx)
        mensajeFallo = ""
        rttMs = 0.0
        val video = VideoCarro(ctx, config.urlVideoCarro,
            { bytes, largo -> for (c in consumidores.values) c.alJpeg(bytes, largo) },
            { texto -> principal.post { alEstadoDelVideo(texto) } },
            config.modoVideo,
            { bytes, largo, w, h, numero, msCamara -> for (c in consumidores.values) c.alCrudo(bytes, largo, w, h, numero, msCamara) },
            { paquete -> for (c in consumidores.values) c.alCeldas(paquete) },
            { rtt -> rttMs = rtt; for (c in consumidores.values) c.alRtt(rtt) },
            { modoFallido, motivo -> for (c in consumidores.values) c.alFalloModo(modoFallido, motivo) })
        actual = video
        Log.i(ETIQUETA, "abriendo el video (${config.urlVideoCarro}, modo ${config.modoVideo})")
        video.iniciar()
    }

    /** Detener un VideoCarro tarda hasta ~1.5 s (espera sus hilos): se hace fuera del hilo principal. */
    private fun cerrarViejo(viejo: VideoCarro?, alTerminar: () -> Unit) {
        if (viejo == null) {
            alTerminar()
            return
        }
        Thread({
            viejo.detener()
            principal.post(alTerminar)
        }, "video-cierre").start()
    }

    private fun cerrar() {
        val viejo = actual
        actual = null
        Log.i(ETIQUETA, "cerrando el video")
        cerrando = true
        cerrarViejo(viejo) {
            cerrando = false
            if (abrirAlTerminar) {
                abrirAlTerminar = false
                if (estado != SesionNativa.CERRADA && usuarios > 0) {
                    abrir()
                }
            }
            avisar()
        }
    }

    private fun reabrir() {
        if (reabriendo) {
            return
        }
        reabriendo = true
        val viejo = actual
        actual = null
        Log.i(ETIQUETA, "reabriendo el video")
        cerrarViejo(viejo) {
            reabriendo = false
            if (estado != SesionNativa.CERRADA && usuarios > 0) {
                abrir()
            }
            avisar()
        }
    }

    /** El video avisó: "" = llegó un cuadro (funciona); otro texto = problema. */
    private fun alEstadoDelVideo(texto: String) {
        if (actual == null) {
            return   // aviso atrasado de un video que ya se cerró
        }
        if (texto.isEmpty()) {
            SesionNativa.funciona(SesionNativa.RECURSO_VIDEO)
            mensajeFallo = ""
        } else {
            SesionNativa.fallo(SesionNativa.RECURSO_VIDEO, SystemClock.elapsedRealtime())
            mensajeFallo = texto
        }
        Log.i(ETIQUETA, "video estado=${if (texto.isEmpty()) "OK" else "FALLO: $texto"} sesion=$estado")
        avisar()
    }

    // ── El reloj de la sesión ─────────────────────────────────────────────

    private val tick = object : Runnable {
        override fun run() {
            tickProgramado = false
            when (SesionNativa.tick(SesionNativa.RECURSO_VIDEO, SystemClock.elapsedRealtime())) {
                SesionNativa.ACCION_CERRAR -> cerrar()
                SesionNativa.ACCION_REABRIR -> reabrir()
            }
            if (estado != SesionNativa.CERRADA) {
                programarTick()
            }
            avisar()
        }
    }

    private fun programarTick() {
        if (!tickProgramado) {
            tickProgramado = true
            principal.postDelayed(tick, PERIODO_TICK_MS)
        }
    }

    private fun avisar() {
        for (e in escuchas) {
            e()
        }
    }
}
