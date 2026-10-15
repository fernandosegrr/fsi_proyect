/*
 * AcercaFragment.kt — La pantalla ACERCA DE: el nombre de la app, su versión, una frase del proyecto y las licencias de lo incluido.
 *
 * GUÍA DE LECTURA
 *  1. La versión sale del propio paquete instalado (PackageManager): versionName y versionCode, así nunca queda desactualizada.
 *  2. Las licencias son una lista fija (nombre, para qué sirve y licencia). Los textos completos de Inter y de Material Symbols están en docs/licencias/.
 *  3. Lo grande de la pantalla es el nombre de la app.
 */
package com.fsi.carro

import android.content.pm.PackageManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.content.pm.PackageInfoCompat
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton

class AcercaFragment : Fragment(R.layout.fragment_acerca) {

    /** Una pieza incluida: su nombre y, en una línea, para qué sirve y con qué licencia viene. */
    private class Licencia(val nombre: Int, val detalle: Int)

    private val licencias = listOf(
        Licencia(R.string.ac_inter, R.string.ac_inter_d),
        Licencia(R.string.ac_symbols, R.string.ac_symbols_d),
        Licencia(R.string.ac_components, R.string.ac_components_d),
        Licencia(R.string.ac_tflite, R.string.ac_tflite_d),
        Licencia(R.string.ac_okhttp, R.string.ac_okhttp_d),
        Licencia(R.string.ac_jpegenc, R.string.ac_jpegenc_d)
    )

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        vista.findViewById<TextView>(R.id.barra_titulo).setText(R.string.mas_acerca)
        vista.findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { (activity as? MainActivity)?.volver() }
        vista.findViewById<TextView>(R.id.ac_version).text = textoVersion()

        val lista = vista.findViewById<LinearLayout>(R.id.ac_lista)
        val inflador = LayoutInflater.from(requireContext())
        for ((indice, licencia) in licencias.withIndex()) {
            val fila = inflador.inflate(R.layout.item_licencia, lista, false)
            fila.findViewById<TextView>(R.id.ac_nombre).setText(licencia.nombre)
            fila.findViewById<TextView>(R.id.ac_detalle).setText(licencia.detalle)
            if (indice == 0) {
                fila.findViewById<View>(R.id.ac_divisor).visibility = View.GONE   // la primera fila no lleva divisor arriba
            }
            lista.addView(fila)
        }
    }

    override fun onStart() {
        super.onStart()
        PantallasLog.abierta("ACERCA")
    }

    /** «Versión 0.3 (2)»: versionName y versionCode del paquete instalado. */
    private fun textoVersion(): String {
        val ctx = requireContext()
        return try {
            val info = ctx.packageManager.getPackageInfo(ctx.packageName, 0)
            getString(R.string.ac_version, info.versionName ?: "?", PackageInfoCompat.getLongVersionCode(info))
        } catch (e: PackageManager.NameNotFoundException) {
            getString(R.string.ac_version_desconocida)
        }
    }
}
