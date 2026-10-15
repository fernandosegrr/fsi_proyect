/*
 * AyudaFragment.kt — La pantalla AYUDA: preguntas frecuentes, qué hacer si algo falla y volver a ver el asistente.
 *
 * GUÍA DE LECTURA
 *  1. Cada pregunta es una tarjeta (item_pregunta_ayuda.xml). Al tocarla se abre o se cierra la respuesta con una animación corta (200 ms,
 *     Movimiento.NORMAL) y el icono cambia entre expand_more y expand_less. Las preguntas abiertas se recuerdan al girar el teléfono.
 *  2. La respuesta dice QUÉ PASÓ (o «Qué es», si solo explica algo) y QUÉ HACER, en frases cortas. Los textos están en textos_mas.xml (ay_*).
 *  3. «Si algo falla» son 4 pasos numerados, en este orden: reiniciar, reconectar, probar en Inicio y, si sigue, manejar sin video.
 *  4. El botón secundario «Ver el asistente de nuevo» abre el asistente de primera vez (MainActivity.abrirAsistente()).
 */
package com.fsi.carro

import android.os.Bundle
import android.transition.AutoTransition
import android.transition.TransitionManager
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.view.ViewCompat
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton

class AyudaFragment : Fragment(R.layout.fragment_ayuda) {

    /** Una pregunta: su título, la etiqueta de la primera parte («Qué pasó» o «Qué es»), esa primera parte y «Qué hacer». */
    private class Pregunta(val pregunta: Int, val etiqueta: Int, val primera: Int, val hacer: Int)

    private val preguntas = listOf(
        Pregunta(R.string.ay_p1_q, R.string.ay_et_paso, R.string.ay_p1_a, R.string.ay_p1_h),
        Pregunta(R.string.ay_p2_q, R.string.ay_et_paso, R.string.ay_p2_a, R.string.ay_p2_h),
        Pregunta(R.string.ay_p3_q, R.string.ay_et_paso, R.string.ay_p3_a, R.string.ay_p3_h),
        Pregunta(R.string.ay_p4_q, R.string.ay_et_paso, R.string.ay_p4_a, R.string.ay_p4_h),
        Pregunta(R.string.ay_p5_q, R.string.ay_et_es, R.string.ay_p5_a, R.string.ay_p5_h),
        Pregunta(R.string.ay_p6_q, R.string.ay_et_es, R.string.ay_p6_a, R.string.ay_p6_h),
        Pregunta(R.string.ay_p7_q, R.string.ay_et_paso, R.string.ay_p7_a, R.string.ay_p7_h),
        Pregunta(R.string.ay_p8_q, R.string.ay_et_paso, R.string.ay_p8_a, R.string.ay_p8_h),
        Pregunta(R.string.ay_p9_q, R.string.ay_et_paso, R.string.ay_p9_a, R.string.ay_p9_h),
        Pregunta(R.string.ay_p10_q, R.string.ay_et_paso, R.string.ay_p10_a, R.string.ay_p10_h)
    )

    private val pasosSiFalla = listOf(R.string.ay_falla_1, R.string.ay_falla_2, R.string.ay_falla_3, R.string.ay_falla_4)

    /** Los números de las preguntas que están abiertas. */
    private val abiertas = mutableSetOf<Int>()
    private lateinit var contenido: ViewGroup

    override fun onCreate(guardado: Bundle?) {
        super.onCreate(guardado)
        guardado?.getIntArray("abiertas")?.let { abiertas.addAll(it.toList()) }
    }

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        vista.findViewById<TextView>(R.id.barra_titulo).setText(R.string.mas_ayuda)
        vista.findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { (activity as? MainActivity)?.volver() }
        contenido = vista.findViewById<ViewGroup>(R.id.ay_contenido)
        val inflador = LayoutInflater.from(requireContext())

        // Preguntas frecuentes
        val lista = vista.findViewById<LinearLayout>(R.id.ay_lista)
        for ((indice, p) in preguntas.withIndex()) {
            val tarjeta = inflador.inflate(R.layout.item_pregunta_ayuda, lista, false)
            tarjeta.findViewById<TextView>(R.id.ay_pregunta).setText(p.pregunta)
            tarjeta.findViewById<TextView>(R.id.ay_etiqueta_1).setText(p.etiqueta)
            tarjeta.findViewById<TextView>(R.id.ay_texto_1).setText(p.primera)
            tarjeta.findViewById<TextView>(R.id.ay_texto_2).setText(p.hacer)
            mostrarEstado(tarjeta, indice in abiertas)
            Movimiento.respuestaAlTocar(tarjeta)
            tarjeta.setOnClickListener { alternar(indice, tarjeta) }
            lista.addView(tarjeta)
        }

        // Si algo falla: 4 pasos numerados
        val pasos = vista.findViewById<LinearLayout>(R.id.ay_pasos)
        for ((indice, texto) in pasosSiFalla.withIndex()) {
            val fila = inflador.inflate(R.layout.item_paso_ayuda, pasos, false)
            fila.findViewById<TextView>(R.id.ay_numero).text = (indice + 1).toString()
            fila.findViewById<TextView>(R.id.ay_paso_texto).setText(texto)
            pasos.addView(fila)
        }

        // Ver el asistente de nuevo
        val botonAsistente = vista.findViewById<MaterialButton>(R.id.ay_asistente)
        Movimiento.respuestaAlTocar(botonAsistente)
        botonAsistente.setOnClickListener { (activity as? MainActivity)?.abrirAsistente() }
    }

    override fun onStart() {
        super.onStart()
        PantallasLog.abierta("AYUDA")
    }

    override fun onSaveInstanceState(salida: Bundle) {
        super.onSaveInstanceState(salida)
        salida.putIntArray("abiertas", abiertas.toIntArray())
    }

    /** Abre o cierra una pregunta con una animación corta (200 ms). El resto de la pantalla se desliza con ella. */
    private fun alternar(indice: Int, tarjeta: View) {
        val abrir = indice !in abiertas
        if (abrir) abiertas.add(indice) else abiertas.remove(indice)
        TransitionManager.beginDelayedTransition(contenido, AutoTransition().setDuration(Movimiento.NORMAL))
        mostrarEstado(tarjeta, abrir)
    }

    /** Muestra u oculta la respuesta y pone el icono y la descripción para el lector de pantalla. */
    private fun mostrarEstado(tarjeta: View, abierta: Boolean) {
        tarjeta.findViewById<View>(R.id.ay_cuerpo).visibility = if (abierta) View.VISIBLE else View.GONE
        tarjeta.findViewById<ImageView>(R.id.ay_icono).setImageResource(if (abierta) R.drawable.ic_expand_less else R.drawable.ic_expand_more)
        ViewCompat.setStateDescription(tarjeta, getString(if (abierta) R.string.ay_abierta else R.string.ay_cerrada))
    }
}
