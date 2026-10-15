/*
 * RutasFragment.kt — La pestaña RUTAS (se completa en la parte 3 del rediseño: editor, simulador, ejecución e historial).
 */
package com.fsi.carro

import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.fragment.app.Fragment

class RutasFragment : Fragment(R.layout.view_estado_vacio) {

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        vista.findViewById<TextView>(R.id.vacio_texto).setText(R.string.proximamente)
    }

    override fun onStart() {
        super.onStart()
        SesionCarro.entrar(requireContext(), SesionNativa.USUARIO_RUTAS)
        PantallasLog.abierta("RUTAS")
    }

    override fun onStop() {
        SesionCarro.salir(SesionNativa.USUARIO_RUTAS)
        super.onStop()
    }
}
