/*
 * EjecutarRutaFragment.kt — EJECUTAR RUTA: el carro recorre la ruta dibujada, con el video y el estado en palabras.
 *
 * GUÍA DE LECTURA
 *  1. Usa el MISMO ciclo de siempre (ModoSemiautonomo, el navegador en C): obstáculos de la CÁMARA DEL CARRO (CamaraObstaculos sobre la sesión de video compartida),
 *     perfil AUTO confirmado, giro por tiempo corregido con la cámara. Esta pantalla solo lo muestra y lo maneja.
 *  2. Antes de cada ruta hay que APRENDER EL PISO (botón «Aprender piso», con el carro quieto): así el detector sabe qué color es el piso. Si falta, «Iniciar ruta»
 *     lo explica (ModoSemiautonomo.mensaje) y se puede tocar otra vez en 10 s para usar el último piso aprendido.
 *  3. Lo grande es el ESTADO EN PALABRAS («Avanzando», «Obstáculo: esquivando», «Terminado»), con el progreso por puntos. El PARO rojo ocupa el lugar del botón de iniciar
 *     mientras corre (y también está en la cabecera).
 *  4. Al terminar (o al cancelar) se muestra un RESUMEN (resultado, duración y obstáculos esquivados) y se anota en el historial (AlmacenRutas).
 *  5. Al salir de la pantalla la ruta se cancela (STOP) y se suelta el video. Los detalles técnicos (posición estimada, cuadros, la prueba de obstáculos) están
 *     detrás del botón «Detalles técnicos».
 */
package com.fsi.carro

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import com.google.android.material.progressindicator.LinearProgressIndicator
import java.util.Locale

class EjecutarRutaFragment : Fragment(R.layout.fragment_ejecutar) {

    companion object {
        fun nueva(nombre: String, celdas: IntArray): EjecutarRutaFragment = EjecutarRutaFragment().also {
            it.arguments = Bundle().apply {
                putString("nombre", nombre)
                putIntArray("celdas", celdas)
            }
        }
    }

    private val principal = Handler(Looper.getMainLooper())
    private lateinit var almacen: AlmacenRutas
    private lateinit var calibracion: CalibracionGuardada
    private lateinit var camara: CamaraObstaculos
    private var visor: VisorJpeg? = null
    private var celdas = IntArray(0)
    private var nombreRuta = ""

    private lateinit var estado: TextView
    private lateinit var progreso: TextView
    private lateinit var barra: LinearProgressIndicator
    private lateinit var aviso: AvisoBanda
    private lateinit var piso: TextView
    private lateinit var botonAprender: MaterialButton
    private lateinit var ayudaAprender: TextView
    private lateinit var botonIniciar: MaterialButton
    private lateinit var botonParo: MaterialButton
    private lateinit var botonVolver: MaterialButton
    private lateinit var resumen: MaterialCardView
    private lateinit var resumenChip: ChipEstado
    private lateinit var resumenDuracion: TextView
    private lateinit var resumenEsquivadas: TextView
    private lateinit var detalles: TextView
    private lateinit var imagen: ImageView
    private lateinit var placeholder: View
    private lateinit var chipFps: ChipEstado
    private lateinit var chipRetraso: ChipEstado

    private var estabaActiva = false
    private var inicioMs = 0L
    private var ultimaEsquivadas = 0
    private var terminada = false
    private var resultadoFinal = ""

    private val alCambiar: () -> Unit = { actualizar() }
    private val reloj = object : Runnable {
        override fun run() {
            actualizar()
            principal.postDelayed(this, 250)
        }
    }

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        val ctx = requireContext()
        almacen = AlmacenRutas(ctx)
        calibracion = CalibracionGuardada(ctx)
        celdas = arguments?.getIntArray("celdas") ?: IntArray(0)
        nombreRuta = arguments?.getString("nombre").orEmpty()

        vista.findViewById<TextView>(R.id.barra_titulo).setText(R.string.ejecutar_titulo)
        vista.findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { intentarSalir() }
        estado = vista.findViewById(R.id.ej_estado)
        vista.findViewById<TextView>(R.id.ej_ruta_nombre).text = nombreRuta
        progreso = vista.findViewById(R.id.ej_progreso)
        barra = vista.findViewById(R.id.ej_barra)
        aviso = vista.findViewById(R.id.ej_aviso)
        piso = vista.findViewById(R.id.ej_piso)
        botonAprender = vista.findViewById(R.id.ej_aprender)
        ayudaAprender = vista.findViewById(R.id.ej_aprender_ayuda)
        botonIniciar = vista.findViewById(R.id.ej_iniciar)
        botonParo = vista.findViewById(R.id.ej_paro)
        botonVolver = vista.findViewById(R.id.ej_volver)
        resumen = vista.findViewById(R.id.ej_resumen)
        resumenChip = vista.findViewById(R.id.ej_resumen_chip)
        resumenDuracion = vista.findViewById(R.id.ej_resumen_duracion)
        resumenEsquivadas = vista.findViewById(R.id.ej_resumen_esquivadas)
        detalles = vista.findViewById(R.id.ej_detalles)
        imagen = vista.findViewById(R.id.manual_video)
        placeholder = vista.findViewById(R.id.manual_placeholder)
        chipFps = vista.findViewById(R.id.manual_chip_fps)
        chipRetraso = vista.findViewById(R.id.manual_chip_retraso)
        chipRetraso.poner(Nivel.NEUTRO, getString(R.string.chip_retraso))
        for (id in listOf(R.id.ej_iniciar, R.id.ej_aprender)) {
            Movimiento.respuestaAlTocar(vista.findViewById(id))
        }

        botonIniciar.setOnClickListener { iniciar() }
        botonAprender.setOnClickListener { aprenderPiso() }
        botonParo.setOnTouchListener { v, evento ->
            if (evento.actionMasked == android.view.MotionEvent.ACTION_DOWN) {
                Movimiento.escala(v, true)
                Haptica.corta(ctx)
                SesionCarro.pararTodo("PARO (ruta)")
            } else if (evento.actionMasked == android.view.MotionEvent.ACTION_UP || evento.actionMasked == android.view.MotionEvent.ACTION_CANCEL) {
                Movimiento.escala(v, false)
            }
            true
        }
        botonVolver.setOnClickListener { (activity as? MainActivity)?.volver() }
        val botonDetalles = vista.findViewById<MaterialButton>(R.id.ej_boton_detalles)
        botonDetalles.setOnClickListener {
            val abrir = detalles.visibility != View.VISIBLE
            detalles.visibility = if (abrir) View.VISIBLE else View.GONE
            botonDetalles.setIconResource(if (abrir) R.drawable.ic_expand_less else R.drawable.ic_expand_more)
        }
    }

    override fun onStart() {
        super.onStart()
        val ctx = requireContext()
        SesionCarro.entrar(ctx, SesionNativa.USUARIO_RUTA)
        val logica = SesionCarro.logica
        logica.pedirPerfil(ControlLogica.Perfil.AUTO)
        val semi = SesionCarro.semi
        calibracion.aplicarAlNavegador()
        semi.leerCalibracion = { calibracion.leer() }
        PruebaObstaculoNativa.iniciarResumen()

        visor = VisorJpeg(imagen)
        SesionVideo.entrar(ctx, SesionNativa.USUARIO_RUTA, visor)

        val config = ConfigVision(ctx)
        camara = CamaraObstaculos(requireActivity(), { config.comoArreglo().also { it[ObstaculosNativa.CFG_PISO_FIJO] = if (semi.activo) 1 else 0 } },
            { intArrayOf(config.anchoCuadro, config.altoCuadro) }, { semi.avanzando }, { config.usarModelo }, { config.corredorComoArreglo() },
            { config.cualquieraComoArreglo() }, { config.flujoComoArreglo() })
        camara.forzarVideoCarro = true
        camara.usuarioVideo = SesionNativa.USUARIO_DETECCION
        camara.fovX10 = calibracion.fovCalibradoX10()
        camara.abrir()
        semi.camara = camara

        SesionCarro.escuchar(alCambiar)
        SesionVideo.escuchar(alCambiar)
        principal.post(reloj)
        PantallasLog.abierta("EJECUTAR_RUTA")
    }

    override fun onStop() {
        principal.removeCallbacks(reloj)
        SesionCarro.dejarDeEscuchar(alCambiar)
        SesionVideo.dejarDeEscuchar(alCambiar)
        val semi = SesionCarro.semi
        if (semi.activo || semi.esperandoPerfil) {
            semi.cancelar("Pantalla cerrada")
            anotarFinal()
        }
        semi.camara = null
        camara.cerrar()
        SesionVideo.salir(SesionNativa.USUARIO_RUTA)
        visor?.cerrar()
        visor = null
        SesionCarro.salir(SesionNativa.USUARIO_RUTA)
        super.onStop()
    }

    // ── Acciones ──────────────────────────────────────────────────────────

    private fun intentarSalir() {
        val semi = SesionCarro.semi
        if (semi.activo || semi.esperandoPerfil) {
            Dialogos.confirmar(requireContext(), getString(R.string.ejecutar_salir_titulo), getString(R.string.ejecutar_salir_texto), getString(R.string.ejecutar_salir_si)) {
                SesionCarro.pararTodo("Salida de la pantalla")
                (activity as? MainActivity)?.volver()
            }
        } else {
            (activity as? MainActivity)?.volver()
        }
    }

    private fun iniciar() {
        terminada = false
        resumen.visibility = View.GONE
        aviso.ocultar()
        Haptica.corta(requireContext())
        val semi = SesionCarro.semi
        if (!camara.abierta) {
            val m = Mensajes.sinVideo()
            aviso.mostrar(Nivel.AVISO, m.quePaso, getString(R.string.ejecutar_sin_video_ruta), getString(R.string.reintentar)) { SesionVideo.reintentar() }
            return
        }
        semi.iniciar(celdas)
    }

    private fun aprenderPiso() {
        val semi = SesionCarro.semi
        if (!camara.abierta) {
            aviso.mostrar(Nivel.AVISO, getString(R.string.ejecutar_esperando_video), getString(R.string.ejecutar_aprender_ayuda))
        } else if (semi.activo) {
            aviso.mostrar(Nivel.AVISO, "No se puede aprender el piso con la ruta en marcha", "Detén la ruta y vuelve a intentarlo.")
        } else {
            aviso.ocultar()
            camara.aprenderPiso()
        }
    }

    // ── Pantalla ──────────────────────────────────────────────────────────

    private fun actualizar() {
        if (!isAdded || view == null) {
            return
        }
        val semi = SesionCarro.semi
        val e = NavegadorNativa.estado()
        val corriendo = semi.activo || semi.esperandoPerfil
        val hayVideo = SesionVideo.estado == SesionNativa.ABIERTA && SesionVideo.fps > 0.5

        // El fin de la ruta: pasó de «corriendo» a «detenida»
        if (estabaActiva && !semi.activo && !semi.esperandoPerfil) {
            anotarFinal()
        }
        estabaActiva = semi.activo
        if (semi.activo) {
            if (inicioMs == 0L || terminada) {
                inicioMs = SystemClock.elapsedRealtime()
                terminada = false
                resumen.visibility = View.GONE
            }
            ultimaEsquivadas = e[NavegadorNativa.E_ESQUIVADAS]
        }

        // Lo grande: el estado en palabras
        val palabra = when {
            semi.esperandoPerfil -> getString(R.string.ejecutar_estado_preparando)
            semi.activo -> palabraEstado(e)
            terminada -> when (resultadoFinal) {
                "TERMINADO" -> getString(R.string.ejecutar_estado_terminado)
                "ERROR" -> getString(R.string.ejecutar_estado_error)
                else -> getString(R.string.ejecutar_estado_cancelado)
            }
            else -> getString(R.string.ejecutar_estado_listo)
        }
        if (estado.text != palabra) {
            estado.text = palabra
        }
        val total = (e[NavegadorNativa.E_PUNTOS] - 1).coerceAtLeast(1)
        val objetivo = if (semi.activo) e[NavegadorNativa.E_OBJETIVO].coerceIn(1, total) else if (terminada && resultadoFinal == "TERMINADO") total else 0
        barra.setProgressCompat(if (semi.activo || terminada) objetivo * 100 / total else 0, true)
        progreso.text = if (semi.activo || terminada) getString(R.string.ejecutar_progreso, objetivo, total) else ""
        progreso.visibility = if (progreso.text.isEmpty()) View.GONE else View.VISIBLE

        // Botones
        botonIniciar.visibility = if (corriendo) View.GONE else View.VISIBLE
        botonIniciar.setText(if (terminada) R.string.ejecutar_repetir else R.string.ejecutar_iniciar)
        botonParo.visibility = if (corriendo) View.VISIBLE else View.GONE
        botonAprender.visibility = if (corriendo) View.GONE else View.VISIBLE
        ayudaAprender.visibility = if (corriendo) View.GONE else View.VISIBLE
        botonVolver.visibility = if (terminada && !corriendo) View.VISIBLE else View.GONE
        piso.text = if (corriendo) "" else textoPiso()

        // Video
        placeholder.visibility = if (hayVideo && (visor?.cuadrosMostrados ?: 0L) > 0L) View.GONE else View.VISIBLE
        chipFps.visibility = if (hayVideo) View.VISIBLE else View.GONE
        if (hayVideo) {
            chipFps.poner(Nivel.NEUTRO, String.format(Locale.US, "%.0f fps", SesionVideo.fps))
        }

        // Avisos: el mensaje del ciclo de la ruta (por qué no arrancó o por qué se detuvo)
        if (!corriendo && !terminada && semi.mensaje.isNotEmpty()) {
            aviso.mostrar(Nivel.AVISO, "No se pudo iniciar la ruta", semi.mensaje)
        } else if (SesionVideo.estado == SesionNativa.FALLO && !corriendo) {
            val m = Mensajes.sinVideo()
            aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer, getString(R.string.reintentar)) { SesionVideo.reintentar() }
        } else if (corriendo && semi.mensaje.startsWith("Aviso")) {
            aviso.mostrar(Nivel.AVISO, "Aviso antes de empezar", semi.mensaje)
        } else if (!terminada) {
            aviso.ocultar()
        }
        detalles.text = textoDetalles(e)
    }

    private fun palabraEstado(e: IntArray): String = when (e[NavegadorNativa.E_ESTADO]) {
        NavegadorNativa.DETENIDO_OBSTACULO -> getString(R.string.ejecutar_estado_obstaculo)
        NavegadorNativa.ESQUIVANDO -> getString(R.string.ejecutar_estado_esquivando)
        NavegadorNativa.REINCORPORANDO -> getString(R.string.ejecutar_estado_volviendo)
        NavegadorNativa.TERMINADO -> getString(R.string.ejecutar_estado_terminado)
        NavegadorNativa.ERROR -> getString(R.string.ejecutar_estado_error)
        else -> when (e[NavegadorNativa.E_COMANDO]) {
            NavegadorNativa.CMD_FORWARD -> getString(R.string.ejecutar_estado_avanzando)
            NavegadorNativa.CMD_REVERSE -> "Retrocediendo"
            NavegadorNativa.CMD_LEFT, NavegadorNativa.CMD_RIGHT -> getString(R.string.ejecutar_estado_girando)
            else -> getString(R.string.ejecutar_estado_detenido)
        }
    }

    /** Lo que se sabe del piso (aprendiendo, lo aprendido o que falta aprenderlo), en palabras cortas. */
    private fun textoPiso(): String = when {
        camara.aprendiendoPiso -> "Aprendiendo el piso… deja el carro quieto y sin objetos adelante."
        camara.errorAprender.isNotEmpty() -> camara.errorAprender
        camara.informePiso != null -> "Piso aprendido: " + camara.informePiso!!.texto()
        else -> "Piso: todavía sin aprender."
    }

    /** Cierra la ejecución: resultado, resumen, historial y vibración. */
    private fun anotarFinal() {
        if (terminada) {
            return
        }
        val e = NavegadorNativa.estado()
        val estadoFinal = e[NavegadorNativa.E_ESTADO]
        resultadoFinal = when (estadoFinal) {
            NavegadorNativa.TERMINADO -> "TERMINADO"
            NavegadorNativa.ERROR -> "ERROR"
            else -> "CANCELADO"
        }
        terminada = true
        val duracion = if (inicioMs == 0L) 0L else SystemClock.elapsedRealtime() - inicioMs
        almacen.agregarEjecucion(resultadoFinal, duracion, ultimaEsquivadas, nombreRuta)
        resumen.visibility = View.VISIBLE
        resumenChip.poner(when (resultadoFinal) {
            "TERMINADO" -> Nivel.OK
            "ERROR" -> Nivel.ERROR
            else -> Nivel.AVISO
        }, when (resultadoFinal) {
            "TERMINADO" -> getString(R.string.hist_terminado)
            "ERROR" -> getString(R.string.hist_error)
            else -> getString(R.string.hist_cancelado)
        })
        resumenDuracion.text = String.format(Locale.US, "%d s", duracion / 1000)
        resumenEsquivadas.text = "$ultimaEsquivadas"
        if (resultadoFinal == "ERROR") {
            Haptica.obstaculo(requireContext())
            val m = mensajeDeError(e[NavegadorNativa.E_MOTIVO_ERROR])
            aviso.mostrar(Nivel.ERROR, m.quePaso, m.queHacer)
        } else {
            Haptica.corta(requireContext())
        }
        inicioMs = 0L
    }

    /** Qué pasó y qué hacer según el motivo del error del navegador (MotivoErrorNav). */
    private fun mensajeDeError(motivo: Int): Mensaje = when (motivo) {
        1 -> Mensaje(conCausa("Había obstáculos a los dos lados"), "Quita lo que estorba y vuelve a iniciar.")
        2 -> Mensaje(conCausa("Ya esquivó las 3 veces permitidas y el obstáculo seguía enfrente"), "Quita el obstáculo o cambia la ruta y vuelve a iniciar.")
        3 -> Mensaje(conCausa("No pudo esquivar el obstáculo"), "Quita el obstáculo o deja más espacio libre y vuelve a iniciar.")
        4 -> Mensaje("No encontró cómo volver a la ruta", "Pon el carro de vuelta en el inicio y vuelve a iniciar.")
        5 -> Mensajes.sinCamaraEnRuta()
        else -> Mensaje("La ruta se detuvo", "Revisa el carro y vuelve a iniciar.")
    }

    /** La causa concreta que dice el navegador (con los números), con la primera letra en mayúscula; si no hay, el texto general. */
    private fun conCausa(general: String): String {
        val causa = NavegadorNativa.textoError()
        return if (causa.isEmpty()) general else causa.replaceFirstChar { it.uppercase() }
    }

    private fun textoDetalles(e: IntArray): String {
        val semi = SesionCarro.semi
        return "Estado: ${NavegadorNativa.textoEstado(e[NavegadorNativa.E_ESTADO])}\n" +
            "Comando: ${NavegadorNativa.textoComando(e[NavegadorNativa.E_COMANDO])}\n" +
            "Posición estimada: x=${e[NavegadorNativa.E_X_MM] / 10} cm  y=${e[NavegadorNativa.E_Y_MM] / 10} cm  rumbo=${e[NavegadorNativa.E_RUMBO]}°\n" +
            "Cámara: ${if (camara.abierta) "abierta" else "cerrada"}  ${String.format(Locale.US, "%.1f", camara.fps)} fps\n" +
            (if (camara.esVideoCarro) camara.textoVideo() + "\n" else "") +
            (if (semi.textoPrueba.isNotEmpty()) semi.textoPrueba else "")
    }
}
