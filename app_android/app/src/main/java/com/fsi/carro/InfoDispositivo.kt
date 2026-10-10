/*
 * InfoDispositivo.kt — Datos del teléfono para el reporte del benchmark.
 *
 * GUÍA DE LECTURA
 *  1. texto() junta modelo, Android, procesador (SoC) y núcleos en líneas "clave=valor".
 *  2. Los núcleos salen de /proc/cpuinfo y de las frecuencias máximas de cada núcleo
 *     (si el teléfono deja leerlas).
 *  3. estadoTermico() sirve para ver si el teléfono se calentó durante la prueba.
 */
package com.fsi.carro

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.os.PowerManager
import java.io.File

object InfoDispositivo {

    /** Líneas "clave=valor" con los datos del equipo. */
    fun texto(contexto: Context): List<String> {
        val lineas = ArrayList<String>()
        lineas.add("fabricante=${Build.MANUFACTURER}")
        lineas.add("modelo=${Build.MODEL}")
        lineas.add("dispositivo=${Build.DEVICE}")
        lineas.add("android=${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
        lineas.add("hardware=${Build.HARDWARE}")
        lineas.add("placa=${Build.BOARD}")
        if (Build.VERSION.SDK_INT >= 31) {
            lineas.add("soc=${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}")
        }
        lineas.add("nucleos=${Runtime.getRuntime().availableProcessors()}")
        lineas.add("abi=${Build.SUPPORTED_ABIS.joinToString(",")}")
        lineas.addAll(datosCpu())
        lineas.add("estado_inicial=${estadoTermico(contexto)}")
        return lineas
    }

    /** Procesador y frecuencias máximas por núcleo, si se pueden leer. */
    private fun datosCpu(): List<String> {
        val lineas = ArrayList<String>()
        try {
            val cpuinfo = File("/proc/cpuinfo").readLines()
            cpuinfo.firstOrNull { it.startsWith("Hardware") }?.let { lineas.add("cpuinfo_" + it.replace("\\s*:\\s*".toRegex(), "=")) }
            val partes = cpuinfo.filter { it.startsWith("CPU part") }.map { it.substringAfter(":").trim() }
            if (partes.isNotEmpty()) {
                lineas.add("cpu_part_por_nucleo=${partes.joinToString(",")}")
            }
        } catch (e: Exception) {
            lineas.add("cpuinfo=no_legible")
        }
        val frecuencias = ArrayList<String>()
        for (n in 0 until Runtime.getRuntime().availableProcessors()) {
            try {
                val khz = File("/sys/devices/system/cpu/cpu$n/cpufreq/cpuinfo_max_freq").readText().trim().toLong()
                frecuencias.add("${khz / 1000}")
            } catch (e: Exception) {
                frecuencias.add("?")
            }
        }
        lineas.add("frecuencia_max_mhz_por_nucleo=${frecuencias.joinToString(",")}")
        return lineas
    }

    /** Estado térmico del sistema (0 = normal) y temperatura de la batería en grados. */
    fun estadoTermico(contexto: Context): String {
        var estado = "?"
        if (Build.VERSION.SDK_INT >= 29) {
            val energia = contexto.getSystemService(Context.POWER_SERVICE) as PowerManager
            estado = energia.currentThermalStatus.toString()
        }
        val bateria: Intent? = contexto.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val decimas = bateria?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, -1) ?: -1
        val cargando = bateria?.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0) ?: 0
        val temperatura = if (decimas >= 0) "${decimas / 10.0}C" else "?"
        return "termico=$estado bateria=$temperatura enchufado=${cargando != 0}"
    }
}
