/*
 * MasFragment.kt — La pestaña MÁS: Calibración, Visión y diagnóstico, Prueba final, Ajustes, Ayuda y Acerca de.
 *
 * GUÍA DE LECTURA
 *  1. Una lista de tarjetas (item_menu.xml). Calibración y Visión son pantallas aparte (Activity); las demás se abren aquí encima, con la flecha de volver.
 *  2. abrirPorNombre() sirve para abrir una directo con adb (extra «pantalla» de MainActivity) y para las pruebas de pantallas.
 */
package com.fsi.carro

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.fragment.app.Fragment

class MasFragment : Fragment(R.layout.fragment_mas) {

    private class Apartado(val clave: String, val icono: Int, val titulo: Int, val detalle: Int)

    private val apartados = listOf(
        Apartado("calibracion", R.drawable.ic_straighten, R.string.mas_calibracion, R.string.mas_calibracion_detalle),
        Apartado("vision", R.drawable.ic_tune, R.string.mas_vision, R.string.mas_vision_detalle),
        Apartado("prueba", R.drawable.ic_fact_check, R.string.mas_prueba_final, R.string.mas_prueba_final_detalle),
        Apartado("ajustes", R.drawable.ic_settings, R.string.mas_ajustes, R.string.mas_ajustes_detalle),
        Apartado("ayuda", R.drawable.ic_help, R.string.mas_ayuda, R.string.mas_ayuda_detalle),
        Apartado("acerca", R.drawable.ic_info, R.string.mas_acerca, R.string.mas_acerca_detalle)
    )

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        val lista = vista.findViewById<LinearLayout>(R.id.mas_lista)
        val inflador = LayoutInflater.from(requireContext())
        for (a in apartados) {
            val fila = inflador.inflate(R.layout.item_menu, lista, false)
            fila.findViewById<ImageView>(R.id.item_icono).setImageResource(a.icono)
            fila.findViewById<TextView>(R.id.item_titulo).setText(a.titulo)
            fila.findViewById<TextView>(R.id.item_detalle).setText(a.detalle)
            Movimiento.respuestaAlTocar(fila)
            fila.setOnClickListener { abrirPorNombre(a.clave) }
            lista.addView(fila)
        }
    }

    override fun onStart() {
        super.onStart()
        SesionCarro.entrar(requireContext(), SesionNativa.USUARIO_MAS)
        PantallasLog.abierta("MAS")
    }

    override fun onStop() {
        SesionCarro.salir(SesionNativa.USUARIO_MAS)
        super.onStop()
    }

    /** Abre un apartado por su nombre: calibracion, vision, prueba, ajustes, ayuda o acerca. */
    fun abrirPorNombre(clave: String) {
        val principal = activity as? MainActivity ?: return
        when (clave) {
            "calibracion" -> startActivity(Intent(requireContext(), CalibracionActivity::class.java))
            "vision" -> startActivity(Intent(requireContext(), VisionActivity::class.java))
            "prueba" -> principal.abrirSubpantalla(PruebaFinalFragment())
            "ajustes" -> principal.abrirSubpantalla(AjustesFragment())
            "ayuda" -> principal.abrirSubpantalla(AyudaFragment())
            "acerca" -> principal.abrirSubpantalla(AcercaFragment())
        }
    }
}
