/*
 * AsistenteFragment.kt — El asistente de PRIMERA VEZ: 3 pantallas (conectarse a la red del carro, revisar la cámara y qué hace cada modo).
 *
 * GUÍA DE LECTURA
 *  1. Se muestra solo la primera vez (Ajustes.asistenteVisto) y se vuelve a ver desde Más → Ayuda. «Omitir» lo cierra en cualquier paso.
 *  2. Ocupa toda la pantalla: MainActivity oculta la cabecera y la barra mientras está abierto (modoCompleto).
 *  3. Al cerrarse (Omitir o Empezar) se marca como visto y se vuelve a la pantalla de antes.
 */
package com.fsi.carro

import android.os.Bundle
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton

class AsistenteFragment : Fragment(R.layout.fragment_asistente) {

    private class Pagina(val icono: Int, val titulo: Int, val texto: Int)

    private val paginas = listOf(
        Pagina(R.drawable.ic_wifi, R.string.asistente_1_titulo, R.string.asistente_1_texto),
        Pagina(R.drawable.ic_videocam, R.string.asistente_2_titulo, R.string.asistente_2_texto),
        Pagina(R.drawable.ic_sports_esports, R.string.asistente_3_titulo, R.string.asistente_3_texto)
    )
    private var indice = 0
    private lateinit var paso: TextView
    private lateinit var icono: ImageView
    private lateinit var titulo: TextView
    private lateinit var texto: TextView
    private lateinit var puntos: LinearLayout
    private lateinit var siguiente: MaterialButton
    private lateinit var contenido: View

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        indice = guardado?.getInt("indice") ?: 0
        paso = vista.findViewById(R.id.asistente_paso)
        icono = vista.findViewById(R.id.asistente_icono)
        titulo = vista.findViewById(R.id.asistente_titulo)
        texto = vista.findViewById(R.id.asistente_texto)
        puntos = vista.findViewById(R.id.asistente_puntos)
        siguiente = vista.findViewById(R.id.asistente_siguiente)
        contenido = vista.findViewById<View>(R.id.asistente_titulo).parent as View
        vista.findViewById<MaterialButton>(R.id.asistente_omitir).setOnClickListener { cerrar() }
        siguiente.setOnClickListener {
            if (indice < paginas.size - 1) {
                indice++
                mostrar(true)
            } else {
                cerrar()
            }
        }
        for (i in paginas.indices) {
            val punto = View(requireContext())
            val lado = Diseno.dp(requireContext(), 8)
            punto.layoutParams = LinearLayout.LayoutParams(lado, lado).also { it.marginEnd = Diseno.dp(requireContext(), 8) }
            puntos.addView(punto)
        }
        mostrar(false)
    }

    override fun onSaveInstanceState(salida: Bundle) {
        super.onSaveInstanceState(salida)
        salida.putInt("indice", indice)
    }

    override fun onStart() {
        super.onStart()
        (activity as? MainActivity)?.modoCompleto(true)
        PantallasLog.abierta("ASISTENTE")
    }

    private fun mostrar(conFundido: Boolean) {
        val p = paginas[indice]
        paso.text = getString(R.string.asistente_paso, indice + 1, paginas.size)
        icono.setImageResource(p.icono)
        titulo.setText(p.titulo)
        texto.setText(p.texto)
        siguiente.setText(if (indice == paginas.size - 1) R.string.asistente_empezar else R.string.asistente_siguiente)
        for (i in 0 until puntos.childCount) {
            val fondo = android.graphics.drawable.GradientDrawable().apply {
                shape = android.graphics.drawable.GradientDrawable.OVAL
                setColor(androidx.core.content.ContextCompat.getColor(requireContext(), if (i == indice) R.color.acento else R.color.borde_fuerte))
            }
            puntos.getChildAt(i).background = fondo
        }
        if (conFundido) {
            contenido.alpha = 0f
            contenido.animate().alpha(1f).setDuration(Movimiento.NORMAL).start()
        }
    }

    private fun cerrar() {
        Ajustes(requireContext()).asistenteVisto = true
        (activity as? MainActivity)?.let {
            it.modoCompleto(false)
            it.volver()
        }
    }
}
