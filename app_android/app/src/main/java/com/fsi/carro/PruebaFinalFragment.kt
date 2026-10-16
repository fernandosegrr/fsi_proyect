/*
 * PruebaFinalFragment.kt — La pantalla PRUEBA FINAL: la lista guiada de las pruebas pendientes con el carro real.
 *
 * GUÍA DE LECTURA
 *  1. Arriba, lo grande: «3 de 9 hechos» con su barra de avance. Cada paso es una tarjeta con casilla, título, instrucciones cortas y, si aplica, un botón
 *     que abre la pantalla relacionada (Inicio, Rutas o Visión y diagnóstico). Las casillas se guardan en Ajustes.pruebaFinal (los números de los pasos hechos).
 *  2. Los pasos salen de docs/reporte_lote4.md (sección 4), docs/modelo_objetos.md (sección 7), docs/latencia_modos.md (sección 4) y docs/latencia.md.
 *  3. PASO 2 «Medir ahora»: mide el PONG del control (el ida y vuelta del WebSocket) 8 s SIN video y 8 s CON video, y dice en palabras si el video estorba.
 *     Usa la conexión del carro como usuario USUARIO_DIAGNOSTICO (SesionCarro) y, para la parte «con video», abre el video con el mismo usuario (SesionVideo).
 *     La regla de los 30 ms la decide el código en C (LatenciaVideoNativa, nucleo_c/latencia_video.c); aquí solo se anotan las medidas y se muestra el resultado.
 *  4. Las fases de la medida (las avanza un reloj de 250 ms):
 *       ESPERA_CERRAR → se espera a que el video esté cerrado (la sesión cierra 1.5 s después de que sale la última pantalla) y 1.5 s más de calma;
 *       SIN_VIDEO     → 8 s anotando cada PONG como «sin video»;
 *       ESPERA_ABRIR  → se abre el video y se espera a que llegue el primer cuadro (hasta 15 s) y 1 s más de calma;
 *       CON_VIDEO     → 8 s anotando cada PONG como «con video». Al final se cierra el video y se muestra el resultado.
 *  5. Si otra pantalla ya usa el video (SesionVideo.usuarios != 0) no se puede medir «sin video»: se avisa. Sin conexión con el carro tampoco se mide.
 *  6. Si el veredicto es EMPEORA (el PONG con video sube más de 30 ms) se apaga «Video en Manual» (Ajustes.videoEnManual) y se dice en el texto.
 *     Cada medida deja una línea MANUAL_VIDEO en logcat: «prueba sin=..ms con=..ms diferencia=..ms veredicto=..».
 *  7. Al salir de la pantalla (onStop) cualquier medida se cancela y el video y la conexión se sueltan.
 */
package com.fsi.carro

import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton
import com.google.android.material.checkbox.MaterialCheckBox
import com.google.android.material.progressindicator.LinearProgressIndicator
import java.util.Locale

class PruebaFinalFragment : Fragment(R.layout.fragment_prueba_final) {

    companion object {
        private const val TOTAL_PASOS = 9
        private const val DURACION_MEDIDA_MS = 8000L      // cada lado de la medida: 8 s
        private const val ESPERA_CERRAR_MAX_MS = 6000L    // cuánto se espera a que el video se cierre
        private const val CALMA_MS = 1500L                // tras cerrarse el video, la cámara tarda en soltar la conexión
        private const val ESPERA_ABRIR_MAX_MS = 15000L    // cuánto se espera al primer cuadro del video
        private const val CALMA_VIDEO_MS = 1000L          // tras llegar el primer cuadro, se deja asentar antes de medir
    }

    private enum class Fase { OCIOSA, ESPERA_CERRAR, SIN_VIDEO, ESPERA_ABRIR, CON_VIDEO }

    /** Un paso de la lista: su número, título, instrucciones y, si aplica, el texto del botón y lo que hace. */
    private class Paso(val numero: Int, val titulo: Int, val instruccion: Int, val textoBoton: Int?, val accion: (() -> Unit)?)

    private val pasos = listOf(
        Paso(1, R.string.pf_p1_t, R.string.pf_p1_i, R.string.pf_boton_inicio) { irAInicio() },
        Paso(2, R.string.pf_p2_t, R.string.pf_p2_i, null, null),
        Paso(3, R.string.pf_p3_t, R.string.pf_p3_i, R.string.pf_boton_vision) { abrirVision() },
        Paso(4, R.string.pf_p4_t, R.string.pf_p4_i, R.string.pf_boton_rutas) { irARutas() },
        Paso(5, R.string.pf_p5_t, R.string.pf_p5_i, R.string.pf_boton_vision) { abrirVision() },
        Paso(6, R.string.pf_p6_t, R.string.pf_p6_i, R.string.pf_boton_vision) { abrirVision() },
        Paso(7, R.string.pf_p7_t, R.string.pf_p7_i, R.string.pf_boton_vision) { abrirVision() },
        Paso(8, R.string.pf_p8_t, R.string.pf_p8_i, R.string.pf_boton_calibracion) { abrirCalibracion() },
        Paso(9, R.string.pf_p9_t, R.string.pf_p9_i, R.string.pf_boton_inicio) { irAInicio() }
    )

    private val principal = Handler(Looper.getMainLooper())
    private lateinit var ajustes: Ajustes
    private lateinit var contador: TextView
    private lateinit var progreso: LinearProgressIndicator
    private lateinit var aviso: AvisoBanda

    // La medida del paso 2
    private var botonMedir: MaterialButton? = null
    private var textoEstado: TextView? = null
    private var textoResultado: TextView? = null
    private var fase = Fase.OCIOSA
    private var faseDesdeMs = 0L
    private var cerradoDesdeMs = 0L      // desde cuándo el video está cerrado (0 = todavía no)
    private var abiertoDesdeMs = 0L      // desde cuándo el video está abierto con imagen (0 = todavía no)
    private var videoEntrado = false     // true si esta pantalla abrió el video (hay que soltarlo al terminar)
    private var entradoEnCarro = false   // true entre onStart y onStop

    private val reloj = object : Runnable {
        override fun run() {
            if (fase == Fase.OCIOSA) {
                return
            }
            avanzar()
            if (fase != Fase.OCIOSA) {
                principal.postDelayed(this, 250)
            }
        }
    }

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        ajustes = Ajustes(requireContext())
        vista.findViewById<TextView>(R.id.barra_titulo).setText(R.string.mas_prueba_final)
        vista.findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { (activity as? MainActivity)?.volver() }
        contador = vista.findViewById<TextView>(R.id.pf_contador)
        progreso = vista.findViewById<LinearProgressIndicator>(R.id.pf_progreso)
        aviso = vista.findViewById<AvisoBanda>(R.id.pf_aviso)
        progreso.max = TOTAL_PASOS

        val lista = vista.findViewById<LinearLayout>(R.id.pf_lista)
        val inflador = LayoutInflater.from(requireContext())
        val hechos = ajustes.pruebaFinal
        for (paso in pasos) {
            val tarjeta = inflador.inflate(R.layout.item_paso_prueba, lista, false)
            val titulo = getString(R.string.pf_titulo_paso, paso.numero, getString(paso.titulo))
            tarjeta.findViewById<TextView>(R.id.pf_titulo).text = titulo
            tarjeta.findViewById<TextView>(R.id.pf_instruccion).setText(paso.instruccion)

            val casilla = tarjeta.findViewById<MaterialCheckBox>(R.id.pf_casilla)
            casilla.contentDescription = getString(R.string.pf_cd_casilla, getString(paso.titulo))
            casilla.isChecked = paso.numero in hechos
            casilla.setOnCheckedChangeListener { _, marcada -> marcarPaso(paso.numero, marcada) }

            val boton = tarjeta.findViewById<MaterialButton>(R.id.pf_boton)
            val textoBoton = paso.textoBoton
            val accion = paso.accion
            if (textoBoton != null && accion != null) {
                boton.setText(textoBoton)
                boton.visibility = View.VISIBLE
                Movimiento.respuestaAlTocar(boton)
                boton.setOnClickListener { accion() }
            }
            if (paso.numero == 2) {
                armarMedida(inflador, tarjeta.findViewById<FrameLayout>(R.id.pf_extra))
            }
            lista.addView(tarjeta)
        }
        actualizarContador(false)
    }

    override fun onStart() {
        super.onStart()
        SesionCarro.entrar(requireContext(), SesionNativa.USUARIO_DIAGNOSTICO)
        entradoEnCarro = true
        PantallasLog.abierta("PRUEBA_FINAL")
    }

    override fun onStop() {
        cerrarMedida()
        if (entradoEnCarro) {
            SesionCarro.salir(SesionNativa.USUARIO_DIAGNOSTICO)
            entradoEnCarro = false
        }
        super.onStop()
    }

    override fun onDestroyView() {
        botonMedir = null
        textoEstado = null
        textoResultado = null
        super.onDestroyView()
    }

    // ── La lista ──────────────────────────────────────────────────────────

    private fun marcarPaso(numero: Int, hecho: Boolean) {
        val nuevos = ajustes.pruebaFinal.toMutableSet()
        if (hecho) nuevos.add(numero) else nuevos.remove(numero)
        ajustes.pruebaFinal = nuevos
        actualizarContador(true)
    }

    /** «3 de 7 hechos» y la barra de avance. */
    private fun actualizarContador(animar: Boolean) {
        val hechos = ajustes.pruebaFinal.count { it in 1..TOTAL_PASOS }
        contador.text = getString(R.string.pf_contador, hechos, TOTAL_PASOS)
        progreso.setProgressCompat(hechos, animar)
    }

    private fun abrirCalibracion() {
        startActivity(Intent(requireContext(), CalibracionActivity::class.java))
    }

    private fun abrirVision() {
        startActivity(Intent(requireContext(), VisionActivity::class.java))
    }

    private fun irARutas() {
        (activity as? MainActivity)?.irA(R.id.pestana_rutas)
    }

    private fun irAInicio() {
        (activity as? MainActivity)?.irA(R.id.pestana_inicio)
    }

    // ── Paso 2: medir el PONG sin y con video ────────────────────────────

    private fun armarMedida(inflador: LayoutInflater, contenedor: FrameLayout) {
        val medida = inflador.inflate(R.layout.view_prueba_medida, contenedor, false)
        contenedor.addView(medida)
        contenedor.visibility = View.VISIBLE
        textoEstado = medida.findViewById<TextView>(R.id.pf_medida_estado)
        textoResultado = medida.findViewById<TextView>(R.id.pf_medida_resultado)
        val boton = medida.findViewById<MaterialButton>(R.id.pf_medir)
        botonMedir = boton
        Movimiento.respuestaAlTocar(boton)
        boton.setOnClickListener {
            if (fase != Fase.OCIOSA) {
                cerrarMedida()
                aviso.ocultar()
            } else {
                empezarMedida()
            }
        }
    }

    /** Revisa lo necesario y arranca la medida (fase ESPERA_CERRAR). */
    private fun empezarMedida() {
        val ctx = requireContext()
        aviso.ocultar()
        textoResultado?.visibility = View.GONE
        if (SesionCarro.logica.conexion != ControlLogica.Conexion.CONECTADO) {
            avisarSinCarro()
            return
        }
        if (SesionVideo.usuarios != 0) {
            // Otra pantalla usa el video: no se puede cerrar desde aquí, así que no hay forma de medir «sin video».
            aviso.mostrar(Nivel.AVISO, getString(R.string.pf_aviso_en_uso), getString(R.string.pf_aviso_cierra_otras))
            return
        }
        Haptica.corta(ctx)
        LatenciaVideoNativa.iniciar()
        SesionCarro.logica.alPong = { ms -> alPong(ms) }
        cerradoDesdeMs = 0L
        abiertoDesdeMs = 0L
        pasarA(Fase.ESPERA_CERRAR)
        view?.keepScreenOn = true
        botonMedir?.setText(R.string.pf_cancelar)
        textoEstado?.visibility = View.VISIBLE
        textoEstado?.setText(R.string.pf_fase_preparando)
        principal.removeCallbacks(reloj)
        principal.postDelayed(reloj, 250)
    }

    private fun pasarA(nueva: Fase) {
        fase = nueva
        faseDesdeMs = SystemClock.elapsedRealtime()
    }

    /** Cada PONG que llega se anota en el lado que toca (el código en C hace los promedios). */
    private fun alPong(ms: Long) {
        when (fase) {
            Fase.SIN_VIDEO -> LatenciaVideoNativa.muestra(0, ms.toInt())
            Fase.CON_VIDEO -> LatenciaVideoNativa.muestra(1, ms.toInt())
            else -> {}
        }
    }

    /** El reloj de la medida (cada 250 ms): decide cuándo pasar a la fase siguiente. */
    private fun avanzar() {
        if (!isAdded) {
            cerrarMedida()
            return
        }
        val ahora = SystemClock.elapsedRealtime()
        if (SesionCarro.logica.conexion != ControlLogica.Conexion.CONECTADO) {
            cerrarMedida()
            avisarSinCarro()
            return
        }
        when (fase) {
            Fase.ESPERA_CERRAR -> {
                textoEstado?.setText(R.string.pf_fase_preparando)
                if (SesionVideo.estado == SesionNativa.CERRADA) {
                    if (cerradoDesdeMs == 0L) {
                        cerradoDesdeMs = ahora
                    }
                    if (ahora - cerradoDesdeMs >= CALMA_MS) {
                        pasarA(Fase.SIN_VIDEO)
                    }
                } else {
                    cerradoDesdeMs = 0L
                    if (ahora - faseDesdeMs > ESPERA_CERRAR_MAX_MS) {
                        cerrarMedida()
                        aviso.mostrar(Nivel.AVISO, getString(R.string.pf_aviso_no_cierra), getString(R.string.pf_aviso_cierra_otras))
                    }
                }
            }
            Fase.SIN_VIDEO -> {
                val restante = DURACION_MEDIDA_MS - (ahora - faseDesdeMs)
                if (restante <= 0L) {
                    // Terminó el lado «sin video»: se abre el video con el mismo usuario.
                    SesionVideo.entrar(requireContext(), SesionNativa.USUARIO_DIAGNOSTICO)
                    videoEntrado = true
                    pasarA(Fase.ESPERA_ABRIR)
                    textoEstado?.setText(R.string.pf_fase_abriendo)
                } else {
                    textoEstado?.text = getString(R.string.pf_fase_sin, segundos(restante))
                }
            }
            Fase.ESPERA_ABRIR -> {
                if (SesionVideo.estado == SesionNativa.ABIERTA) {
                    if (abiertoDesdeMs == 0L) {
                        abiertoDesdeMs = ahora
                    }
                    if (ahora - abiertoDesdeMs >= CALMA_VIDEO_MS) {
                        pasarA(Fase.CON_VIDEO)
                    }
                } else {
                    abiertoDesdeMs = 0L
                    if (ahora - faseDesdeMs > ESPERA_ABRIR_MAX_MS) {
                        abortarSinVideo()
                    }
                }
            }
            Fase.CON_VIDEO -> {
                if (SesionVideo.estado != SesionNativa.ABIERTA) {
                    abortarSinVideo()
                    return
                }
                val restante = DURACION_MEDIDA_MS - (ahora - faseDesdeMs)
                if (restante <= 0L) {
                    terminarMedida()
                } else {
                    textoEstado?.text = getString(R.string.pf_fase_con, segundos(restante))
                }
            }
            Fase.OCIOSA -> {}
        }
    }

    /** Milisegundos a segundos enteros hacia arriba (para mostrar «8 s, 7 s... 1 s»). */
    private fun segundos(ms: Long): Int = ((ms + 999L) / 1000L).toInt()

    private fun avisarSinCarro() {
        val m = if (RedCarro.enRedDelCarro(requireContext())) Mensajes.carroNoResponde() else Mensajes.sinRedDelCarro()
        aviso.mostrar(Nivel.ERROR, m.quePaso, m.queHacer, getString(R.string.conectar)) { SesionCarro.conectarAhora() }
    }

    private fun abortarSinVideo() {
        cerrarMedida()
        val m = Mensajes.sinVideo(requireContext())
        aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer, getString(R.string.reintentar)) { empezarMedida() }
    }

    /** Fin normal: se lee el resultado del código en C, se suelta el video y se muestra en palabras. */
    private fun terminarMedida() {
        val r = LatenciaVideoNativa.resultado()
        cerrarMedida()
        Haptica.corta(requireContext())
        mostrarResultado(r)
    }

    /** Detiene la medida (si hay una), suelta el video y deja el botón como al principio. Se puede llamar siempre. */
    private fun cerrarMedida() {
        principal.removeCallbacks(reloj)
        if (fase != Fase.OCIOSA) {
            SesionCarro.logica.alPong = null
        }
        if (videoEntrado) {
            SesionVideo.salir(SesionNativa.USUARIO_DIAGNOSTICO)
            videoEntrado = false
        }
        fase = Fase.OCIOSA
        cerradoDesdeMs = 0L
        abiertoDesdeMs = 0L
        view?.keepScreenOn = false
        botonMedir?.setText(R.string.pf_medir)
        textoEstado?.visibility = View.GONE
    }

    /** El resultado en palabras, en el registro (MANUAL_VIDEO) y, si el video estorba, apaga «Video en Manual». */
    private fun mostrarResultado(r: IntArray) {
        val sin = r[LatenciaVideoNativa.R_PROM_SIN_X10] / 10.0
        val con = r[LatenciaVideoNativa.R_PROM_CON_X10] / 10.0
        val diferencia = r[LatenciaVideoNativa.R_DIFERENCIA_X10] / 10.0
        val veredicto = r[LatenciaVideoNativa.R_VEREDICTO]
        val nombre = when (veredicto) {
            1 -> "EMPEORA"
            0 -> "SE_MANTIENE"
            else -> "FALTAN_MEDIDAS"
        }
        Log.i("MANUAL_VIDEO", String.format(Locale.US, "prueba sin=%.1fms con=%.1fms diferencia=%.1fms veredicto=%s", sin, con, diferencia, nombre))

        val texto = textoResultado ?: return
        val sinMs = Math.round(sin)
        val conMs = Math.round(con)
        when (veredicto) {
            1 -> {
                ajustes.videoEnManual = false
                texto.text = getString(R.string.pf_res_mal, sinMs, conMs, Math.round(diferencia))
                texto.setTextColor(Diseno.colorDe(requireContext(), Nivel.AVISO))
            }
            0 -> {
                texto.text = getString(R.string.pf_res_bien, sinMs, conMs)
                texto.setTextColor(Diseno.colorDe(requireContext(), Nivel.OK))
            }
            else -> {
                texto.setText(R.string.pf_res_pocas)
                texto.setTextColor(Diseno.colorDe(requireContext(), Nivel.AVISO))
            }
        }
        texto.visibility = View.VISIBLE
    }
}
