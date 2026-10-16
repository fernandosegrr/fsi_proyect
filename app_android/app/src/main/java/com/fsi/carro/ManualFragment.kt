/*
 * ManualFragment.kt — La pantalla MANUAL: lo que ve el carro y la cruceta para manejarlo.
 *
 * GUÍA DE LECTURA
 *  1. Los botones de la cruceta mandan su comando mientras se mantienen presionados y STOP al soltar (ControlLogica.presionar / soltar). El PARO
 *     manda STOP ya. NINGUNO depende del video: si el video falla, se maneja igual.
 *  2. El video es la sesión compartida (SesionVideo, usuario MANUAL): el VisorJpeg decodifica cada JPEG y lo pone en la imagen con esquinas redondeadas.
 *     Dos chips encima: el retraso (≈ 0.3 s, medido en docs/latencia.md) y los cuadros por segundo. Si el video falla, un aviso en banda dice qué pasó y
 *     ofrece «Reintentar».
 *  3. «Ver detección» abre además la detección de obstáculos sobre el mismo video (CamaraObstaculos, usuario DETECCION) y dibuja encima SOLO los números:
 *     zona, corredor y cercanía (VistaNumerosDeteccion). La imagen no se modifica. En Manual la detección NO frena el carro: manda el usuario.
 *  4. El interruptor «Video» apaga el video (se recuerda en Ajustes). docs/manual_video.md explica cuándo conviene dejarlo apagado por defecto.
 *  5. MANUAL_VIDEO: cada 5 s se escribe en logcat si el video está encendido, los cuadros por segundo y el PONG del control (medio y máximo de esos 5 s),
 *     para comprobar que el video no empeora la respuesta del manejo.
 *  6. El perfil de velocidad es el de Ajustes (rápido = MANUAL, 255; lento = AUTO, 210). Con la pantalla en Manual no se apaga.
 */
package com.fsi.carro

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.view.MotionEvent
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.android.material.materialswitch.MaterialSwitch

class ManualFragment : Fragment(R.layout.fragment_manual) {

    private val principal = Handler(Looper.getMainLooper())
    private lateinit var ajustes: Ajustes
    private lateinit var imagen: ImageView
    private lateinit var overlay: VistaNumerosDeteccion
    private lateinit var placeholder: View
    private lateinit var placeholderTexto: TextView
    private lateinit var chipRetraso: ChipEstado
    private lateinit var chipFps: ChipEstado
    private lateinit var aviso: AvisoBanda
    private lateinit var avisoRetraso: TextView
    private lateinit var interruptorVideo: MaterialSwitch
    private lateinit var interruptorDeteccion: MaterialSwitch

    private var visor: VisorJpeg? = null
    private var camaraDeteccion: CamaraObstaculos? = null
    private var videoEncendido = true
    private var deteccionEncendida = false

    // Registro MANUAL_VIDEO (cada 5 s)
    private var inicioVentanaMs = 0L
    private var sumaPong = 0L
    private var cuentaPong = 0
    private var maxPong = 0L

    private val alCambiar: () -> Unit = { actualizar() }
    private val reloj = object : Runnable {
        override fun run() {
            actualizar()
            registrarSiToca()
            principal.postDelayed(this, 250)
        }
    }

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        ajustes = Ajustes(requireContext())
        imagen = vista.findViewById(R.id.manual_video)
        overlay = vista.findViewById(R.id.manual_overlay)
        placeholder = vista.findViewById(R.id.manual_placeholder)
        placeholderTexto = vista.findViewById(R.id.manual_placeholder_texto)
        chipRetraso = vista.findViewById(R.id.manual_chip_retraso)
        chipFps = vista.findViewById(R.id.manual_chip_fps)
        aviso = vista.findViewById(R.id.manual_aviso)
        avisoRetraso = vista.findViewById(R.id.manual_aviso_retraso)
        interruptorVideo = vista.findViewById(R.id.manual_switch_video)
        interruptorDeteccion = vista.findViewById(R.id.manual_switch_deteccion)

        enlazarMovimiento(vista.findViewById(R.id.boton_adelante), "FORWARD")
        enlazarMovimiento(vista.findViewById(R.id.boton_atras), "REVERSE")
        enlazarMovimiento(vista.findViewById(R.id.boton_izquierda), "LEFT")
        enlazarMovimiento(vista.findViewById(R.id.boton_derecha), "RIGHT")
        val paro = vista.findViewById<View>(R.id.boton_paro)
        paro.setOnTouchListener { v, evento ->
            if (evento.actionMasked == MotionEvent.ACTION_DOWN) {
                Movimiento.escala(v, true)
                Haptica.corta(requireContext())
                SesionCarro.pararTodo("PARO (Manual)")
            } else if (evento.actionMasked == MotionEvent.ACTION_UP || evento.actionMasked == MotionEvent.ACTION_CANCEL) {
                Movimiento.escala(v, false)
            }
            true
        }

        videoEncendido = ajustes.videoEnManual
        deteccionEncendida = ajustes.verDeteccion
        interruptorVideo.isChecked = videoEncendido
        interruptorDeteccion.isChecked = deteccionEncendida
        interruptorVideo.setOnCheckedChangeListener { _, encendido ->
            videoEncendido = encendido
            ajustes.videoEnManual = encendido
            if (isResumed || isStarted()) {
                if (encendido) abrirVideo() else cerrarVideo()
            }
            actualizar()
        }
        interruptorDeteccion.setOnCheckedChangeListener { _, encendido ->
            deteccionEncendida = encendido
            ajustes.verDeteccion = encendido
            if (isStarted()) {
                if (encendido) abrirDeteccion() else cerrarDeteccion()
            }
            actualizar()
        }
        chipRetraso.poner(Nivel.NEUTRO, getString(R.string.chip_retraso))
    }

    private fun isStarted() = lifecycle.currentState.isAtLeast(androidx.lifecycle.Lifecycle.State.STARTED)

    override fun onStart() {
        super.onStart()
        val ctx = requireContext()
        SesionCarro.entrar(ctx, SesionNativa.USUARIO_MANUAL)
        val logica = SesionCarro.logica
        logica.pedirPerfil(ajustes.perfilDeManual)
        logica.alPong = { ms ->
            sumaPong += ms
            cuentaPong++
            if (ms > maxPong) maxPong = ms
        }
        avisoRetraso.setText(if (ajustes.perfilManual == Ajustes.PERFIL_LENTO) R.string.manual_aviso_retraso_lento else R.string.manual_aviso_retraso)
        inicioVentanaMs = SystemClock.elapsedRealtime()
        if (videoEncendido) abrirVideo()
        if (deteccionEncendida) abrirDeteccion()
        SesionCarro.escuchar(alCambiar)
        SesionVideo.escuchar(alCambiar)
        principal.post(reloj)
        PantallasLog.abierta("MANUAL")
    }

    override fun onStop() {
        principal.removeCallbacks(reloj)
        SesionCarro.dejarDeEscuchar(alCambiar)
        SesionVideo.dejarDeEscuchar(alCambiar)
        // Al irse de la pantalla el carro se detiene (STOP) y se sueltan las pantallas de la sesión.
        SesionCarro.logica.parar()
        SesionCarro.logica.alPong = null
        cerrarDeteccion()
        cerrarVideo()
        SesionCarro.salir(SesionNativa.USUARIO_MANUAL)
        super.onStop()
    }

    // ── Video y detección ─────────────────────────────────────────────────

    private fun abrirVideo() {
        if (visor != null) {
            return
        }
        visor = VisorJpeg(imagen)
        SesionVideo.entrar(requireContext(), SesionNativa.USUARIO_MANUAL, visor)
    }

    private fun cerrarVideo() {
        SesionVideo.salir(SesionNativa.USUARIO_MANUAL)
        visor?.cerrar()
        visor = null
        imagen.setImageDrawable(null)
    }

    private fun abrirDeteccion() {
        if (camaraDeteccion != null) {
            return
        }
        val config = ConfigVision(requireContext())
        val camara = CamaraObstaculos(requireActivity(), { config.comoArreglo() }, { intArrayOf(config.anchoCuadro, config.altoCuadro) },
            { false }, { config.usarModelo }, { config.corredorComoArreglo() }, { config.cualquieraComoArreglo() }, { config.flujoComoArreglo() })
        camara.forzarVideoCarro = true
        camara.usuarioVideo = SesionNativa.USUARIO_DETECCION
        camara.abrir()
        camaraDeteccion = camara
    }

    private fun cerrarDeteccion() {
        camaraDeteccion?.cerrar()
        camaraDeteccion = null
        overlay.visibility = View.GONE
    }

    // ── Pantalla ──────────────────────────────────────────────────────────

    private fun actualizar() {
        if (!isAdded || view == null) {
            return
        }
        val estado = SesionVideo.estado
        val hayVideo = videoEncendido && estado == SesionNativa.ABIERTA && SesionVideo.fps > 0.5
        val modo = ConfigVision(requireContext()).modoVideo

        // Placeholder y chips
        placeholder.visibility = if (hayVideo && (visor?.cuadrosMostrados ?: 0L) > 0L) View.GONE else View.VISIBLE
        placeholderTexto.setText(
            when {
                !videoEncendido -> R.string.manual_video_apagado
                estado == SesionNativa.FALLO -> R.string.manual_camara_no_responde
                else -> R.string.manual_sin_video
            }
        )
        chipRetraso.visibility = if (hayVideo) View.VISIBLE else View.GONE
        chipFps.visibility = if (hayVideo) View.VISIBLE else View.GONE
        if (hayVideo) {
            chipFps.poner(Nivel.NEUTRO, String.format(java.util.Locale.US, "%.0f fps", SesionVideo.fps))
        }

        // Aviso en banda: el video falló (se sigue manejando sin video)
        if (videoEncendido && (estado == SesionNativa.FALLO)) {
            val m = Mensajes.sinVideo(requireContext())
            aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer, getString(R.string.reintentar)) { SesionVideo.reintentar() }
        } else if (videoEncendido && modo == ModoVideo.RAW && estado == SesionNativa.ABIERTA) {
            val m = Mensajes.videoSinImagenEnModo(modo)
            aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer)
        } else {
            aviso.ocultar()
        }

        // Detección encima del video
        val camara = camaraDeteccion
        if (deteccionEncendida && camara != null) {
            overlay.visibility = View.VISIBLE
            overlay.poner(camara.ultimoCuadro)
        } else {
            overlay.visibility = View.GONE
        }
    }

    /** MANUAL_VIDEO: una línea cada 5 s con el video, los cuadros y el PONG del control (para comprobar que el video no empeora el manejo). */
    private fun registrarSiToca() {
        val ahora = SystemClock.elapsedRealtime()
        if (ahora - inicioVentanaMs < 5000) {
            return
        }
        val promedio = if (cuentaPong > 0) sumaPong.toDouble() / cuentaPong else -1.0
        Log.i("MANUAL_VIDEO", String.format(java.util.Locale.US,
            "video=%s sesion=%s fps=%.1f modo=%s deteccion=%s pong_medio_ms=%.1f pong_max_ms=%d pongs=%d conexion=%s",
            if (videoEncendido) "on" else "off", ses(), SesionVideo.fps, ConfigVision(requireContext()).modoVideo, if (deteccionEncendida) "on" else "off",
            promedio, maxPong, cuentaPong, SesionCarro.logica.conexion))
        inicioVentanaMs = ahora
        sumaPong = 0
        cuentaPong = 0
        maxPong = 0
    }

    private fun ses(): String = when (SesionVideo.estado) {
        SesionNativa.CERRADA -> "CERRADA"
        SesionNativa.ABRIENDO -> "ABRIENDO"
        SesionNativa.ABIERTA -> "ABIERTA"
        else -> "FALLO"
    }

    /** Un botón de la cruceta: presionado = comando repetido; soltado (o cancelado) = STOP. */
    private fun enlazarMovimiento(boton: View, comando: String) {
        boton.setOnTouchListener { v, evento ->
            when (evento.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    v.isPressed = true
                    Movimiento.escala(v, true)
                    SesionCarro.logica.presionar(comando)
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    v.isPressed = false
                    Movimiento.escala(v, false)
                    SesionCarro.logica.soltar(comando)
                }
            }
            true
        }
    }
}
