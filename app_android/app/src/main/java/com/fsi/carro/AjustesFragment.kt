/*
 * AjustesFragment.kt — La pantalla AJUSTES: red, velocidad y video de Manual, tema y vibración.
 *
 * GUÍA DE LECTURA
 *  1. Todo lo que se elige se guarda en Ajustes (SharedPreferences) y sobrevive al cerrar la app. Las opciones y los interruptores se guardan al tocarlos.
 *  2. RED: la dirección del carro (Ajustes.direccionCarro, debe empezar con ws://) y la del video (ConfigVision.urlVideoCarro, http:// o https://) se guardan
 *     con «Guardar direcciones» (el único botón primario). Si una no es válida el campo dice por qué y no se guarda nada. «Restablecer direcciones» vuelve
 *     a las originales. Las direcciones nuevas se usan la próxima vez que la app abre la conexión (Ajustes no mantiene ninguna conexión abierta).
 *  3. VELOCIDAD EN MANUAL: Rápido (perfil MANUAL, 255) o Lento (perfil AUTO, 210). Manual la lee al entrar. Las rutas siempre van en Lento.
 *  4. VIDEO EN MANUAL: el mismo ajuste que el interruptor de la pantalla Manual (Ajustes.videoEnManual).
 *  5. TEMA: Oscuro, Claro o Sistema. Al cambiar se aplica con Diseno.aplicarTema() y se vuelve a dibujar la pantalla (recreate()).
 *  6. VIBRACIÓN: al encenderla vibra una vez, como ejemplo.
 */
package com.fsi.carro

import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.widget.RadioGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton
import com.google.android.material.materialswitch.MaterialSwitch
import com.google.android.material.textfield.TextInputEditText
import com.google.android.material.textfield.TextInputLayout

class AjustesFragment : Fragment(R.layout.fragment_ajustes) {

    private lateinit var ajustes: Ajustes
    private lateinit var config: ConfigVision
    private lateinit var capaCarro: TextInputLayout
    private lateinit var capaVideo: TextInputLayout
    private lateinit var campoCarro: TextInputEditText
    private lateinit var campoVideo: TextInputEditText
    private lateinit var aviso: AvisoBanda

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        val ctx = requireContext()
        ajustes = Ajustes(ctx)
        config = ConfigVision(ctx)

        vista.findViewById<TextView>(R.id.barra_titulo).setText(R.string.mas_ajustes)
        vista.findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { (activity as? MainActivity)?.volver() }

        // ── Red ──
        capaCarro = vista.findViewById<TextInputLayout>(R.id.aj_capa_carro)
        capaVideo = vista.findViewById<TextInputLayout>(R.id.aj_capa_video)
        campoCarro = vista.findViewById<TextInputEditText>(R.id.aj_campo_carro)
        campoVideo = vista.findViewById<TextInputEditText>(R.id.aj_campo_video)
        aviso = vista.findViewById<AvisoBanda>(R.id.aj_aviso)
        campoCarro.setText(ajustes.direccionCarro)
        campoVideo.setText(config.urlVideoCarro)
        // Al volver a escribir se quita el error de ese campo.
        campoCarro.addTextChangedListener(quitarError(capaCarro))
        campoVideo.addTextChangedListener(quitarError(capaVideo))

        val botonGuardar = vista.findViewById<MaterialButton>(R.id.aj_guardar)
        val botonRestablecer = vista.findViewById<MaterialButton>(R.id.aj_restablecer)
        Movimiento.respuestaAlTocar(botonGuardar)
        Movimiento.respuestaAlTocar(botonRestablecer)
        botonGuardar.setOnClickListener { guardarDirecciones() }
        botonRestablecer.setOnClickListener { restablecerDirecciones() }

        // ── Velocidad en Manual ──
        val grupoVelocidad = vista.findViewById<RadioGroup>(R.id.aj_grupo_velocidad)
        grupoVelocidad.check(if (ajustes.perfilManual == Ajustes.PERFIL_LENTO) R.id.aj_velocidad_lento else R.id.aj_velocidad_rapido)
        grupoVelocidad.setOnCheckedChangeListener { _, id ->
            ajustes.perfilManual = if (id == R.id.aj_velocidad_lento) Ajustes.PERFIL_LENTO else Ajustes.PERFIL_RAPIDO
        }

        // ── Video en Manual ──
        val interruptorVideo = vista.findViewById<MaterialSwitch>(R.id.aj_switch_video)
        interruptorVideo.isChecked = ajustes.videoEnManual
        interruptorVideo.setOnCheckedChangeListener { _, encendido -> ajustes.videoEnManual = encendido }
        val filaVideo = vista.findViewById<View>(R.id.aj_fila_video)
        Movimiento.respuestaAlTocar(filaVideo)
        filaVideo.setOnClickListener { interruptorVideo.toggle() }

        // ── Tema ──
        val grupoTema = vista.findViewById<RadioGroup>(R.id.aj_grupo_tema)
        grupoTema.check(
            when (ajustes.tema) {
                Ajustes.TEMA_CLARO -> R.id.aj_tema_claro
                Ajustes.TEMA_SISTEMA -> R.id.aj_tema_sistema
                else -> R.id.aj_tema_oscuro
            }
        )
        grupoTema.setOnCheckedChangeListener { _, id ->
            val nuevo = when (id) {
                R.id.aj_tema_claro -> Ajustes.TEMA_CLARO
                R.id.aj_tema_sistema -> Ajustes.TEMA_SISTEMA
                else -> Ajustes.TEMA_OSCURO
            }
            if (nuevo != ajustes.tema) {
                ajustes.tema = nuevo
                Diseno.aplicarTema(requireContext())
                requireActivity().recreate()
            }
        }

        // ── Vibración ──
        val interruptorVibracion = vista.findViewById<MaterialSwitch>(R.id.aj_switch_vibracion)
        interruptorVibracion.isChecked = ajustes.vibracion
        interruptorVibracion.setOnCheckedChangeListener { _, encendido ->
            ajustes.vibracion = encendido
            if (encendido) {
                Haptica.corta(requireContext())   // un ejemplo de cómo se siente
            }
        }
        val filaVibracion = vista.findViewById<View>(R.id.aj_fila_vibracion)
        Movimiento.respuestaAlTocar(filaVibracion)
        filaVibracion.setOnClickListener { interruptorVibracion.toggle() }
    }

    override fun onStart() {
        super.onStart()
        PantallasLog.abierta("AJUSTES")
    }

    /** Un TextWatcher que borra el error del campo cuando el usuario vuelve a escribir. */
    private fun quitarError(capa: TextInputLayout) = object : TextWatcher {
        override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
        override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
            capa.error = null
        }
        override fun afterTextChanged(s: Editable?) {}
    }

    /** Valida las dos direcciones y, si las dos están bien, las guarda. Si una falla, solo marca el campo (no guarda nada). */
    private fun guardarDirecciones() {
        val carro = campoCarro.text?.toString().orEmpty().trim()
        val video = campoVideo.text?.toString().orEmpty().trim()
        val carroValido = carro.startsWith("ws://") && carro.length > "ws://".length
        val videoValido = (video.startsWith("http://") && video.length > "http://".length) || (video.startsWith("https://") && video.length > "https://".length)
        capaCarro.error = if (carroValido) null else getString(R.string.aj_error_carro)
        capaVideo.error = if (videoValido) null else getString(R.string.aj_error_video)
        if (!carroValido || !videoValido) {
            aviso.ocultar()
            return
        }
        ajustes.direccionCarro = carro
        config.urlVideoCarro = video
        Haptica.corta(requireContext())
        aviso.mostrar(Nivel.OK, getString(R.string.aj_guardado_titulo), getString(R.string.aj_guardado_detalle))
    }

    /** Vuelve a las direcciones originales del carro y de su cámara. */
    private fun restablecerDirecciones() {
        ajustes.direccionCarro = Ajustes.DIRECCION_CARRO_DEFECTO
        config.urlVideoCarro = VideoCarro.URL_DEFECTO
        campoCarro.setText(Ajustes.DIRECCION_CARRO_DEFECTO)
        campoVideo.setText(VideoCarro.URL_DEFECTO)
        capaCarro.error = null
        capaVideo.error = null
        aviso.mostrar(Nivel.OK, getString(R.string.aj_restablecido_titulo), getString(R.string.aj_restablecido_detalle))
    }
}
