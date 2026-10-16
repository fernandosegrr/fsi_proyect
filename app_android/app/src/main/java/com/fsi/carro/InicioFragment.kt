/*
 * InicioFragment.kt — La pantalla de INICIO: estado del carro y de la cámara, botón Conectar y accesos rápidos.
 *
 * GUÍA DE LECTURA
 *  1. Lo grande es el estado en palabras («Listo para manejar», «Sin conexión»...). Debajo, dos tarjetas con chips de color (carro y cámara).
 *  2. Entra a la conexión del carro (SesionCarro) y al video (SesionVideo) como usuario INICIO: la tarjeta de la cámara necesita el video abierto
 *     para saber los cuadros por segundo y la latencia de la red (la cámara atiende un solo cliente, por eso se comparte la sesión).
 *  3. «Conectar» abre la conexión de nuevo ya. Si falla, el aviso en banda dice qué pasó y qué hacer (Mensajes.kt).
 *  4. Se refresca con los avisos de las sesiones y con un reloj de 500 ms (latencia y cuadros cambian solos).
 *  5. SALIR RÁPIDO: «Repetir la última ruta» y las plantillas «Recta 1 m», «En L» e «Ida y vuelta» abren Ejecutar ruta con esa ruta ya cargada: 1 toque aquí, «Iniciar ruta» (2) y
 *     «Confirmar y salir» (3). Las plantillas se piden al código en C sin tocar el borrador del editor de Rutas.
 */
package com.fsi.carro

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton

class InicioFragment : Fragment(R.layout.fragment_inicio) {

    private val principal = Handler(Looper.getMainLooper())
    private lateinit var estadoGrande: TextView
    private lateinit var estadoDetalle: TextView
    private lateinit var aviso: AvisoBanda
    private lateinit var chipCarro: ChipEstado
    private lateinit var chipCamara: ChipEstado
    private lateinit var carroLatencia: TextView
    private lateinit var carroPerfil: TextView
    private lateinit var camaraLatencia: TextView
    private lateinit var camaraFps: TextView
    private lateinit var botonConectar: MaterialButton
    private var intentoManualMs = 0L   // hora en que se tocó «Conectar» (para esperar un poco antes de decir que falló)

    private val alCambiar: () -> Unit = { actualizar() }
    private val reloj = object : Runnable {
        override fun run() {
            actualizar()
            principal.postDelayed(this, 500)
        }
    }

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        estadoGrande = vista.findViewById(R.id.inicio_estado_grande)
        estadoDetalle = vista.findViewById(R.id.inicio_estado_detalle)
        aviso = vista.findViewById(R.id.inicio_aviso)
        chipCarro = vista.findViewById(R.id.inicio_chip_carro)
        chipCamara = vista.findViewById(R.id.inicio_chip_camara)
        carroLatencia = vista.findViewById(R.id.inicio_carro_latencia)
        carroPerfil = vista.findViewById(R.id.inicio_carro_perfil)
        camaraLatencia = vista.findViewById(R.id.inicio_camara_latencia)
        camaraFps = vista.findViewById(R.id.inicio_camara_fps)
        botonConectar = vista.findViewById(R.id.inicio_boton_conectar)
        Movimiento.respuestaAlTocar(botonConectar)
        botonConectar.setOnClickListener {
            Haptica.corta(requireContext())
            intentoManualMs = android.os.SystemClock.elapsedRealtime()
            SesionCarro.conectarAhora()
            SesionVideo.reintentar()
            actualizar()
        }
        val almacen = AlmacenRutas(requireContext())
        val ultima = almacen.ultimaRuta()
        val botonUltima = vista.findViewById<MaterialButton>(R.id.inicio_rapida_ultima)
        if (ultima != null) {
            botonUltima.text = getString(R.string.rapida_ultima_nombre, ultima.first)
        } else {
            botonUltima.isEnabled = false
            botonUltima.alpha = 0.38f
            botonUltima.setText(R.string.rapida_sin_ultima)
        }
        botonUltima.setOnClickListener { almacen.ultimaRuta()?.let { salirRapido(it.first, it.second) } }
        for ((id, plantilla, nombre) in listOf(Triple(R.id.inicio_rapida_recta, RutaEditorNativa.PLANTILLA_RECTA, R.string.plantilla_recta),
            Triple(R.id.inicio_rapida_l, RutaEditorNativa.PLANTILLA_L, R.string.plantilla_l), Triple(R.id.inicio_rapida_ida, RutaEditorNativa.PLANTILLA_IDA_VUELTA, R.string.plantilla_ida_vuelta))) {
            val boton = vista.findViewById<MaterialButton>(id)
            Movimiento.respuestaAlTocar(boton)
            boton.setOnClickListener { salirRapido(getString(nombre), RutaEditorNativa.celdasDePlantilla(plantilla)) }
        }
        Movimiento.respuestaAlTocar(botonUltima)
        for ((id, pestana) in listOf(R.id.inicio_acceso_manual to R.id.pestana_manual, R.id.inicio_acceso_rutas to R.id.pestana_rutas)) {
            val tarjeta = vista.findViewById<View>(id)
            Movimiento.respuestaAlTocar(tarjeta)
            tarjeta.setOnClickListener { (activity as? MainActivity)?.irA(pestana) }
        }
    }

    override fun onStart() {
        super.onStart()
        val ctx = requireContext()
        SesionCarro.entrar(ctx, SesionNativa.USUARIO_INICIO)
        SesionVideo.entrar(ctx, SesionNativa.USUARIO_INICIO)
        SesionCarro.logica.pedirPerfil(ControlLogica.Perfil.AUTO)   // en Inicio el carro queda en el perfil lento (el seguro)
        SesionCarro.escuchar(alCambiar)
        SesionVideo.escuchar(alCambiar)
        principal.post(reloj)
        PantallasLog.abierta("INICIO")
    }

    override fun onStop() {
        principal.removeCallbacks(reloj)
        SesionCarro.dejarDeEscuchar(alCambiar)
        SesionVideo.dejarDeEscuchar(alCambiar)
        SesionVideo.salir(SesionNativa.USUARIO_INICIO)
        SesionCarro.salir(SesionNativa.USUARIO_INICIO)
        super.onStop()
    }

    private fun actualizar() {
        if (!isAdded || view == null) {
            return
        }
        val logica = SesionCarro.logica
        val conectado = logica.conexion == ControlLogica.Conexion.CONECTADO
        val estadoVideo = SesionVideo.estado
        val videoBien = estadoVideo == SesionNativa.ABIERTA && SesionVideo.fps > 0.5

        // Tarjeta del carro
        when (logica.conexion) {
            ControlLogica.Conexion.CONECTADO -> chipCarro.poner(Nivel.OK, getString(R.string.conexion_conectado))
            ControlLogica.Conexion.RECONECTANDO -> chipCarro.poner(Nivel.AVISO, getString(R.string.chip_reintentando))
            ControlLogica.Conexion.DESCONECTADO -> chipCarro.poner(if (logica.msSinConexion > 0) Nivel.AVISO else Nivel.ERROR,
                getString(if (logica.msSinConexion > 0) R.string.conexion_conectando else R.string.conexion_sin))
        }
        carroLatencia.text = logica.latenciaMs?.let { "$it ms" } ?: getString(R.string.valor_sin_dato)
        carroPerfil.text = if (conectado) textoPerfil(logica.perfilConfirmado) else getString(R.string.valor_sin_dato)

        // Tarjeta de la cámara
        when {
            videoBien -> chipCamara.poner(Nivel.OK, getString(R.string.camara_conectada))
            estadoVideo == SesionNativa.FALLO -> chipCamara.poner(Nivel.ERROR, getString(R.string.camara_sin_video))
            estadoVideo == SesionNativa.ABRIENDO -> chipCamara.poner(Nivel.AVISO, getString(R.string.camara_abriendo))
            else -> chipCamara.poner(Nivel.NEUTRO, getString(R.string.camara_en_espera))
        }
        camaraFps.text = if (videoBien) String.format(java.util.Locale.US, "%.1f", SesionVideo.fps) else getString(R.string.valor_sin_dato)
        camaraLatencia.text = if (videoBien && SesionVideo.rttMs > 0) String.format(java.util.Locale.US, "%.0f ms", SesionVideo.rttMs) else getString(R.string.valor_sin_dato)

        // Lo grande
        when {
            conectado && videoBien -> poner(R.string.inicio_listo, R.string.inicio_listo_detalle)
            conectado -> poner(R.string.inicio_solo_carro, R.string.inicio_solo_carro_detalle)
            else -> poner(R.string.inicio_sin_carro, R.string.inicio_sin_carro_detalle)
        }
        botonConectar.setText(if (conectado) R.string.boton_reconectar_inicio else R.string.boton_conectar_inicio)

        // Aviso en banda si el carro no responde (una frase de qué pasó y una de qué hacer)
        val esperaMs = logica.msSinConexion
        if (!conectado && esperaMs > ESPERA_ANTES_DE_AVISAR_MS) {
            val m = if (RedCarro.enRedDelCarro(requireContext())) Mensajes.carroNoResponde() else Mensajes.sinRedDelCarro()
            aviso.mostrar(Nivel.ERROR, m.quePaso, m.queHacer)
        } else if (conectado && estadoVideo == SesionNativa.FALLO) {
            val m = Mensajes.sinVideo(requireContext())
            aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer, getString(R.string.reintentar)) { SesionVideo.reintentar() }
        } else {
            aviso.ocultar()
        }
    }

    /** Abre Ejecutar ruta con esa ruta ya cargada (el primer toque de los tres). */
    private fun salirRapido(nombre: String, celdas: IntArray) {
        if (celdas.size < 4) {
            return
        }
        Haptica.corta(requireContext())
        (activity as? MainActivity)?.abrirSubpantalla(EjecutarRutaFragment.nueva(nombre, celdas))
    }

    private fun poner(grande: Int, detalle: Int) {
        if (estadoGrande.text != getString(grande)) {
            estadoGrande.setText(grande)
        }
        estadoDetalle.setText(detalle)
    }

    private fun textoPerfil(perfil: ControlLogica.Perfil?): String = when (perfil) {
        ControlLogica.Perfil.MANUAL -> getString(R.string.perfil_rapido) + " (255)"
        ControlLogica.Perfil.AUTO -> getString(R.string.perfil_lento) + " (210)"
        null -> getString(R.string.valor_sin_dato)
    }

    companion object {
        /** Cuánto se espera sin conexión antes de mostrar el aviso de error (el primer intento tarda un poco). */
        const val ESPERA_ANTES_DE_AVISAR_MS = 5000L
    }
}
