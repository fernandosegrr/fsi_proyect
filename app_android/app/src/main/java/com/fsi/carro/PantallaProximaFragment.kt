/*
 * PantallaProximaFragment.kt — Una pantalla con barra de título y un ESTADO VACÍO («esto llega pronto»).
 *
 * GUÍA DE LECTURA
 *  1. Sirve de ejemplo del componente «estado vacío» (view_estado_vacio.xml) y de relleno de las pantallas que se arman más adelante.
 */
package com.fsi.carro

import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton

class PantallaProximaFragment : Fragment(R.layout.fragment_estado_vacio) {

    companion object {
        fun nueva(titulo: String): PantallaProximaFragment = PantallaProximaFragment().also { it.arguments = Bundle().apply { putString("titulo", titulo) } }
    }

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        val titulo = arguments?.getString("titulo").orEmpty()
        vista.findViewById<TextView>(R.id.barra_titulo).text = titulo
        vista.findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { (activity as? MainActivity)?.volver() }
        vista.findViewById<TextView>(R.id.vacio_texto).setText(R.string.proximamente)
    }

    override fun onStart() {
        super.onStart()
        PantallasLog.abierta("PROXIMA:" + arguments?.getString("titulo").orEmpty())
    }
}
