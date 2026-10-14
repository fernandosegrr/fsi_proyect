/*
 * CalibracionGuardada.kt — Guarda y lee la calibración del carro en el teléfono.
 *
 * GUÍA DE LECTURA
 *  1. Se guarda como TEXTO ("clave=valor", una por línea; el mismo que produce el código en C) en
 *     SharedPreferences ("calibracion"). Es el mismo texto que se exporta: se puede copiar,
 *     pegar en otro teléfono o leer a mano.
 *  2. guardar() además escribe el archivo calibracion.txt en la carpeta de archivos de la app, para
 *     sacarlo con adb (ver docs/calibracion.md). Nunca va al repositorio.
 *  3. leer() devuelve la calibración guardada o, si no hay (o el texto no es válido), la de por
 *     defecto. estaCompleta() dice si todo se midió (si no, la pantalla de control avisa).
 *  4. aplicarAlNavegador() se la pasa al navegador en C (se usa al cargar la siguiente ruta).
 */
package com.fsi.carro

import android.content.Context
import android.util.Log
import java.io.File

class CalibracionGuardada(contexto: Context) {

    companion object {
        const val ETIQUETA = "CALIBRACION"
        const val NOMBRE_ARCHIVO = "calibracion.txt"
    }

    private val aplicacion = contexto.applicationContext
    private val preferencias = aplicacion.getSharedPreferences("calibracion", Context.MODE_PRIVATE)

    /** La calibración guardada (8 enteros) o la de por defecto. */
    fun leer(): IntArray {
        val texto = preferencias.getString("texto", null)
        if (texto != null) {
            val (codigo, cal) = CalibracionNativa.desempaquetar(CalibracionNativa.deTexto(texto))
            if (codigo == CalibracionNativa.OK) {
                return cal
            }
            Log.w(ETIQUETA, "el texto guardado no es válido (código $codigo): se usa la calibración por defecto")
        }
        return CalibracionNativa.desempaquetar(CalibracionNativa.defecto()).second
    }

    /** El campo de visión horizontal de la cámara guardado, en grados por 10 (600 si no se calibró). */
    fun fovX10(): Int = leer()[CalibracionNativa.CAL_FOV_GRADOS_X10]

    /** true si ya se midió todo (arranque, avance y los dos giros). */
    fun estaCompleta(): Boolean = CalibracionNativa.faltantes(leer()) == 0

    /** Guarda la calibración y escribe el archivo de texto. Devuelve false si no es válida. */
    fun guardar(calibracion: IntArray): Boolean {
        val texto = CalibracionNativa.aTexto(calibracion)
        if (texto.isEmpty()) {
            return false
        }
        preferencias.edit().putString("texto", texto).apply()
        try {
            File(aplicacion.getExternalFilesDir(null), NOMBRE_ARCHIVO).writeText(texto)
        } catch (e: Exception) {
            Log.w(ETIQUETA, "no se pudo escribir $NOMBRE_ARCHIVO: ${e.message}")
        }
        Log.i(ETIQUETA, "guardada: " + texto.replace('\n', ' '))
        return true
    }

    /** Vuelve a los valores por defecto (borra lo guardado y el archivo). */
    fun borrar() {
        preferencias.edit().remove("texto").apply()
        try {
            File(aplicacion.getExternalFilesDir(null), NOMBRE_ARCHIVO).delete()
        } catch (e: Exception) {
            Log.w(ETIQUETA, "no se pudo borrar $NOMBRE_ARCHIVO: ${e.message}")
        }
        Log.i(ETIQUETA, "calibración borrada: se usan los valores por defecto")
    }

    /** Le entrega la calibración guardada al navegador (para la próxima ruta). */
    fun aplicarAlNavegador() {
        val codigo = NavegadorNativa.ponerCalibracion(leer())
        if (codigo != 0) {
            Log.w(ETIQUETA, "el navegador rechazó la calibración (código $codigo)")
        }
    }

    /** Texto del aviso si falta calibrar algo, o null si todo está calibrado. */
    fun avisoSiFalta(): String? {
        val falta = CalibracionNativa.faltantes(leer())
        if (falta == 0) {
            return null
        }
        val partes = ArrayList<String>()
        if (falta and CalibracionNativa.MEDIDO_ARRANQUE != 0) partes.add("arranque")
        if (falta and CalibracionNativa.MEDIDO_AVANCE != 0) partes.add("avance")
        if (falta and CalibracionNativa.MEDIDO_GIRO_IZQ != 0) partes.add("giro izquierda")
        if (falta and CalibracionNativa.MEDIDO_GIRO_DER != 0) partes.add("giro derecha")
        return "Carro sin calibrar (${partes.joinToString(", ")}): se usan valores por defecto y la ruta puede salir chueca. Mide con «Calibración del carro»."
    }
}
