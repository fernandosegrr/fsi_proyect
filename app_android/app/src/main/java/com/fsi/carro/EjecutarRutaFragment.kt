/*
 * EjecutarRutaFragment.kt — EJECUTAR RUTA: el carro recorre la ruta dibujada, con el video y el estado en palabras.
 *
 * GUÍA DE LECTURA
 *  1. Usa el MISMO ciclo de siempre (ModoSemiautonomo, el navegador en C): obstáculos de la CÁMARA DEL CARRO (CamaraObstaculos sobre la sesión de video compartida),
 *     perfil AUTO confirmado, giro por tiempo corregido con la cámara. Esta pantalla solo lo muestra y lo maneja.
 *  2. SALIR EN 3 TOQUES (elegir la ruta, «Iniciar ruta» y «Confirmar y salir»). «Iniciar ruta» hace solo lo que antes eran varios pasos: (a) comprueba que hay conexión
 *     con el carro y que el video llega, (b) pide el perfil AUTO y espera su ACK, (c) APRENDE EL PISO con el carro quieto (~1 s; si el piso se ve «muy variado» o falla, dice
 *     «Despeja el frente del carro» o qué revisar en vez de aprender mal) y (d) queda en «Listo para salir». Al tocar «Confirmar y salir» cuenta 3, 2, 1 y sale (el PARO
 *     cancela la cuenta). Cada ruta que sale queda como «la última ruta» (AlmacenRutas) para repetirla desde Inicio. «Aprender piso» a mano sigue en «Avanzado».
 *  3. Lo grande es el ESTADO EN PALABRAS («Avanzando», «Obstáculo: esquivando», «Terminado»), con el progreso por puntos. El PARO rojo ocupa el lugar del botón de iniciar
 *     mientras corre (y también está en la cabecera).
 *  3b. TOMA EL CONTROL: si el carro no puede esquivar (los dos lados ocupados, esquivadas o reintentos agotados) el navegador pasa a AYUDA en vez de dar ERROR: el carro se queda
 *     quieto, el teléfono vibra y la pantalla dice «Necesito ayuda: toma el control» con la causa. Una cruceta de mantener apretado manda el comando manual al navegador
 *     (NavegadorNativa.ayudaComando; soltar = STOP), que sigue estimando la posición. «Continuar la ruta» (ayudaContinuar) hace que vuelva solo a la línea y siga. El PARO cancela la ruta.
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
        private const val ESPERA_VIDEO_MS = 6000L     // cuánto se espera al primer cuadro del video antes de decir que no hay video
        private const val ESPERA_PISO_MS = 6000L      // cuánto se espera a que termine de aprender el piso
        private const val PERFIL_ESPERA_MS = 1000L    // cuánto se espera el ACK del perfil AUTO
        private const val CONFIRMAR_MAX_MS = 60000L   // «Confirmar y salir» caduca: pasado esto se vuelve a comprobar todo (el piso, el video y el perfil pudieron cambiar)

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
    private lateinit var resumenGirosTitulo: TextView
    private lateinit var resumenGiros: TextView
    private lateinit var detalles: TextView
    private lateinit var imagen: ImageView
    private lateinit var placeholder: View
    private lateinit var chipFps: ChipEstado
    private lateinit var chipRetraso: ChipEstado
    private lateinit var botonCancelar: MaterialButton
    private lateinit var ayudaConfirmar: TextView
    private lateinit var avanzado: View
    private lateinit var tarjetaAyuda: View
    private lateinit var textoAyudaCausa: TextView
    private var ayudaAvisada = false   // ya se vibró por la ayuda actual

    /** Lo que pasa entre tocar «Iniciar ruta» y que el carro salga. */
    private enum class Fase { OCIOSA, COMPROBANDO, CONFIRMAR, CUENTA }
    private var fase = Fase.OCIOSA
    private var subpaso = 0            // dentro de COMPROBANDO: 0 espera el video, 1 espera el perfil AUTO, 2 aprende el piso
    private var faseDesdeMs = 0L
    private var cuenta = 0             // el número que se ve en la cuenta regresiva
    private var avisoDelFlujo = false  // true mientras el aviso en banda es de la preparación (el reloj de la pantalla no lo oculta)

    private var estabaActiva = false
    private var inicioMs = 0L
    private var ultimaEsquivadas = 0
    private var terminada = false
    private var resultadoFinal = ""

    /** Cada 200 ms mientras se comprueba todo: espera el video, el perfil AUTO y el piso aprendido, y pasa a «Listo para salir» o dice qué falló. */
    private val vigilarPreparacion = object : Runnable {
        override fun run() {
            if (fase != Fase.COMPROBANDO || !isAdded) {
                return
            }
            val ahora = SystemClock.elapsedRealtime()
            if (SesionCarro.logica.conexion != ControlLogica.Conexion.CONECTADO) {
                fallarPreparacion(Mensajes.conexionPerdida())
                return
            }
            when (subpaso) {
                0 -> if (camara.abierta && camara.ultimoCuadro != null) {
                    subpaso = 1
                    faseDesdeMs = ahora
                    pedirPerfilAuto()
                } else if (ahora - faseDesdeMs > ESPERA_VIDEO_MS) {
                    val m = Mensajes.sinVideo(requireContext())
                    fallarPreparacion(Mensaje(m.quePaso, getString(R.string.ejecutar_sin_video_ruta, Mensajes.direccionDeLaCamara(requireContext()))), getString(R.string.reintentar)) { SesionVideo.reintentar() }
                    return
                }
                2 -> if (!camara.aprendiendoPiso) {
                    evaluarPiso()
                    return
                } else if (ahora - faseDesdeMs > ESPERA_PISO_MS) {
                    fallarPreparacion(Mensaje(getString(R.string.ejecutar_piso_error), getString(R.string.ejecutar_piso_error_hacer)))
                    return
                }
            }
            principal.postDelayed(this, 200)
        }
    }

    /** La cuenta regresiva 3, 2, 1 y salir. */
    private val tareaCuenta = object : Runnable {
        override fun run() {
            if (fase != Fase.CUENTA || !isAdded) {
                return
            }
            if (cuenta > 0) {
                Haptica.corta(requireContext())
                actualizar()
                cuenta--
                principal.postDelayed(this, 1000)
            } else {
                fase = Fase.OCIOSA
                SesionCarro.semi.iniciar(celdas)   // la comprobación de arriba ya dejó el piso aprendido y el perfil AUTO: arranca sin pedir nada más
                actualizar()
            }
        }
    }

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
        resumenGirosTitulo = vista.findViewById(R.id.ej_resumen_giros_titulo)
        resumenGiros = vista.findViewById(R.id.ej_resumen_giros)
        detalles = vista.findViewById(R.id.ej_detalles)
        imagen = vista.findViewById(R.id.manual_video)
        placeholder = vista.findViewById(R.id.manual_placeholder)
        chipFps = vista.findViewById(R.id.manual_chip_fps)
        chipRetraso = vista.findViewById(R.id.manual_chip_retraso)
        chipRetraso.poner(Nivel.NEUTRO, getString(R.string.chip_retraso))
        botonCancelar = vista.findViewById(R.id.ej_cancelar)
        ayudaConfirmar = vista.findViewById(R.id.ej_ayuda_confirmar)
        avanzado = vista.findViewById(R.id.ej_avanzado)
        botonCancelar.setOnClickListener { cancelarSalida() }
        tarjetaAyuda = vista.findViewById(R.id.ej_ayuda)
        textoAyudaCausa = vista.findViewById(R.id.ej_ayuda_causa)
        enlazarCrucetaAyuda(vista.findViewById(R.id.ej_ayuda_adelante), NavegadorNativa.CMD_FORWARD)
        enlazarCrucetaAyuda(vista.findViewById(R.id.ej_ayuda_atras), NavegadorNativa.CMD_REVERSE)
        enlazarCrucetaAyuda(vista.findViewById(R.id.ej_ayuda_izquierda), NavegadorNativa.CMD_LEFT)
        enlazarCrucetaAyuda(vista.findViewById(R.id.ej_ayuda_derecha), NavegadorNativa.CMD_RIGHT)
        vista.findViewById<MaterialButton>(R.id.ej_ayuda_continuar).setOnClickListener {
            NavegadorNativa.ayudaComando(NavegadorNativa.CMD_STOP)
            NavegadorNativa.ayudaContinuar(SystemClock.elapsedRealtime())
            Haptica.corta(requireContext())
        }
        for (id in listOf(R.id.ej_iniciar, R.id.ej_aprender)) {
            Movimiento.respuestaAlTocar(vista.findViewById(id))
        }

        botonIniciar.setOnClickListener { iniciar() }
        botonAprender.setOnClickListener { aprenderPiso() }
        botonParo.setOnTouchListener { v, evento ->
            if (evento.actionMasked == android.view.MotionEvent.ACTION_DOWN) {
                Movimiento.escala(v, true)
                Haptica.corta(ctx)
                cancelarSalida()   // si todavía se estaba comprobando o contando, no sale
                SesionCarro.pararTodo("PARO (ruta)")
            } else if (evento.actionMasked == android.view.MotionEvent.ACTION_UP || evento.actionMasked == android.view.MotionEvent.ACTION_CANCEL) {
                Movimiento.escala(v, false)
            }
            true
        }
        botonVolver.setOnClickListener { (activity as? MainActivity)?.volver() }
        val botonDetalles = vista.findViewById<MaterialButton>(R.id.ej_boton_detalles)
        botonDetalles.setOnClickListener {
            val abrir = avanzado.visibility != View.VISIBLE
            avanzado.visibility = if (abrir) View.VISIBLE else View.GONE
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
        semi.leerCalibracion = { calibracion.leerAjustada() }
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
        cancelarSalida()
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

    /** Un botón de la cruceta del modo «toma el control»: apretado = ese comando; soltado o cancelado = STOP. El navegador lo ejecuta en su siguiente vuelta (cada 50 ms). */
    private fun enlazarCrucetaAyuda(boton: View, comando: Int) {
        boton.setOnTouchListener { v, evento ->
            when (evento.actionMasked) {
                android.view.MotionEvent.ACTION_DOWN -> {
                    v.isPressed = true
                    Movimiento.escala(v, true)
                    NavegadorNativa.ayudaComando(comando)
                }
                android.view.MotionEvent.ACTION_UP, android.view.MotionEvent.ACTION_CANCEL -> {
                    v.isPressed = false
                    Movimiento.escala(v, false)
                    NavegadorNativa.ayudaComando(NavegadorNativa.CMD_STOP)
                }
            }
            true
        }
    }

    /** El botón grande: «Iniciar ruta» (comprueba todo) y, cuando está listo, «Confirmar y salir» (cuenta 3, 2, 1 y sale). */
    private fun iniciar() {
        if (fase == Fase.CONFIRMAR) {
            if (SystemClock.elapsedRealtime() - faseDesdeMs <= CONFIRMAR_MAX_MS) {
                confirmarSalida()
                return
            }
            fase = Fase.OCIOSA   // pasó mucho tiempo desde la comprobación: se repite entera antes de salir
        }
        if (fase != Fase.OCIOSA) {
            return
        }
        terminada = false
        resumen.visibility = View.GONE
        aviso.ocultar()
        avisoDelFlujo = false
        Haptica.corta(requireContext())
        if (SesionCarro.logica.conexion != ControlLogica.Conexion.CONECTADO) {
            val m = if (RedCarro.enRedDelCarro(requireContext())) Mensajes.carroNoResponde() else Mensajes.sinRedDelCarro()
            aviso.mostrar(Nivel.ERROR, m.quePaso, m.queHacer, getString(R.string.conectar)) { SesionCarro.conectarAhora() }
            avisoDelFlujo = true
            return
        }
        fase = Fase.COMPROBANDO
        subpaso = 0
        faseDesdeMs = SystemClock.elapsedRealtime()
        principal.removeCallbacks(vigilarPreparacion)
        principal.post(vigilarPreparacion)
        actualizar()
    }

    /** Pide el perfil AUTO y espera su ACK (1 s). Con el ACK sigue con el piso. */
    private fun pedirPerfilAuto() {
        SesionCarro.logica.confirmarPerfil(ControlLogica.Perfil.AUTO, PERFIL_ESPERA_MS) { confirmado ->
            if (fase != Fase.COMPROBANDO) {
                return@confirmarPerfil   // se canceló mientras se esperaba
            }
            if (!confirmado) {
                fallarPreparacion(Mensajes.perfilSinConfirmar())
                return@confirmarPerfil
            }
            subpaso = 2
            faseDesdeMs = SystemClock.elapsedRealtime()
            camara.aprenderPiso()   // con el carro quieto, ~1 s
        }
    }

    /** El piso ya se aprendió: si salió bien pasa a «Listo para salir»; si se ve muy variado o falló, dice qué hacer en vez de seguir con un piso mal aprendido. */
    private fun evaluarPiso() {
        val informe = camara.informePiso
        if (camara.errorAprender.isNotEmpty() || informe == null) {
            fallarPreparacion(Mensaje(getString(R.string.ejecutar_piso_error), getString(R.string.ejecutar_piso_error_hacer)))
        } else if (informe.muyVariado) {
            fallarPreparacion(Mensaje(getString(R.string.ejecutar_piso_variado), getString(R.string.ejecutar_piso_variado_hacer)))
        } else {
            fase = Fase.CONFIRMAR
            faseDesdeMs = SystemClock.elapsedRealtime()   // desde aquí cuenta CONFIRMAR_MAX_MS
            Haptica.corta(requireContext())
            actualizar()
        }
    }

    private fun fallarPreparacion(m: Mensaje, textoAccion: String? = null, accion: (() -> Unit)? = null) {
        fase = Fase.OCIOSA
        principal.removeCallbacks(vigilarPreparacion)
        SesionCarro.logica.cancelarConfirmacion()
        if (textoAccion != null && accion != null) {
            aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer, textoAccion, accion)
        } else {
            aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer)
        }
        avisoDelFlujo = true
        actualizar()
    }

    private fun confirmarSalida() {
        fase = Fase.CUENTA
        cuenta = 3
        avisoDelFlujo = false
        aviso.ocultar()
        almacen.guardarUltimaRuta(nombreRuta, celdas)   // queda como «la última ruta» para repetirla desde Inicio
        principal.removeCallbacks(tareaCuenta)
        principal.post(tareaCuenta)
        actualizar()
    }

    /** Cancela la comprobación, la espera de confirmación o la cuenta regresiva (no sale). */
    private fun cancelarSalida() {
        if (fase == Fase.OCIOSA) {
            return
        }
        fase = Fase.OCIOSA
        principal.removeCallbacks(vigilarPreparacion)
        principal.removeCallbacks(tareaCuenta)
        SesionCarro.logica.cancelarConfirmacion()
        if (isAdded && view != null) {
            actualizar()
        }
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
        val corriendo = semi.activo || semi.esperandoPerfil || fase == Fase.COMPROBANDO || fase == Fase.CUENTA
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

        // Modo «toma el control»: la tarjeta con la cruceta, y una vibración la primera vez
        val enAyuda = semi.activo && e[NavegadorNativa.E_ESTADO] == NavegadorNativa.AYUDA
        tarjetaAyuda.visibility = if (enAyuda) View.VISIBLE else View.GONE
        if (enAyuda) {
            textoAyudaCausa.text = NavegadorNativa.textoError().replaceFirstChar { it.uppercase() }
            if (!ayudaAvisada) {
                ayudaAvisada = true
                Haptica.obstaculo(requireContext())
            }
        } else {
            ayudaAvisada = false
        }

        // Lo grande: el estado en palabras
        val palabra = when {
            fase == Fase.CUENTA -> "$cuenta"
            fase == Fase.COMPROBANDO -> getString(R.string.ejecutar_estado_comprobando)
            fase == Fase.CONFIRMAR -> getString(R.string.ejecutar_estado_confirmar)
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
        botonIniciar.setText(if (fase == Fase.CONFIRMAR) R.string.ejecutar_confirmar else if (terminada) R.string.ejecutar_repetir else R.string.ejecutar_iniciar)
        botonCancelar.visibility = if (fase == Fase.CONFIRMAR) View.VISIBLE else View.GONE
        ayudaConfirmar.visibility = if (fase == Fase.CONFIRMAR) View.VISIBLE else View.GONE
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
        if (!corriendo && !terminada && !avisoDelFlujo && semi.mensaje.isNotEmpty()) {
            aviso.mostrar(Nivel.AVISO, "No se pudo iniciar la ruta", semi.mensaje)
        } else if (SesionVideo.estado == SesionNativa.FALLO && !corriendo) {
            val m = Mensajes.sinVideo(requireContext())
            aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer, getString(R.string.reintentar)) { SesionVideo.reintentar() }
        } else if (semi.activo && semi.mensaje.startsWith("Aviso")) {
            aviso.mostrar(Nivel.AVISO, "Aviso antes de empezar", semi.mensaje)
        } else if (!terminada && !avisoDelFlujo) {
            aviso.ocultar()
        }
        detalles.text = textoDetalles(e)
    }

    private fun palabraEstado(e: IntArray): String = when (e[NavegadorNativa.E_ESTADO]) {
        NavegadorNativa.AYUDA -> getString(R.string.ejecutar_estado_ayuda)
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
        // «Giro pedido / Giro hecho (estimado)»: lo que el programa pidió en cada giro y lo que el navegador estima que giró (por tiempo, o con la cámara si era confiable). Si el programa pidió
        // 90° y el carro no giró, aquí se ve que el comando fue correcto y que el problema es del carro (contacto eléctrico), no del programa.
        val giros = NavegadorNativa.textoGiros()
        resumenGirosTitulo.visibility = if (giros.isEmpty()) View.GONE else View.VISIBLE
        resumenGiros.visibility = resumenGirosTitulo.visibility
        resumenGiros.text = giros
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
