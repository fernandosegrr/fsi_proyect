/*
 * AlmacenRutas.kt — Guarda en el teléfono las rutas con nombre, el borrador y el historial de ejecuciones.
 *
 * GUÍA DE LECTURA
 *  1. RUTAS CON NOMBRE: un archivo de texto por ruta en «rutas/» dentro de la carpeta privada de la app (el contenido es el texto de re_a_texto, por ejemplo
 *     "RUTA1;12x8;1,6;1,4"). El nombre del archivo es el nombre de la ruta (ya validado por re_nombre_valido).
 *  2. BORRADOR: la ruta que se está dibujando se guarda sola en «borrador.txt» cada vez que cambia, y se recupera al volver a abrir la pantalla.
 *  3. HISTORIAL: una línea por ejecución en «historial.txt» (los últimos 30): fecha en segundos, resultado (TERMINADO, ERROR o CANCELADO), duración en segundos,
 *     obstáculos esquivados y nombre de la ruta, separados por «|».
 *  4. Todo se lee y se escribe en el hilo principal (son archivos muy chicos). Si algo no se puede leer, se ignora: nunca debe tumbar la pantalla.
 */
package com.fsi.carro

import android.content.Context
import java.io.File

class AlmacenRutas(contexto: Context) {

    private val carpeta = File(contexto.filesDir, "rutas").also { it.mkdirs() }
    private val borrador = File(contexto.filesDir, "borrador.txt")
    private val historial = File(contexto.filesDir, "historial.txt")

    /** Una ejecución del historial. */
    class Ejecucion(val epochS: Long, val resultado: String, val duracionS: Int, val esquivadas: Int, val ruta: String)

    // ── Rutas con nombre ──────────────────────────────────────────────────

    /** Los nombres de las rutas guardadas, ordenados. */
    fun nombres(): List<String> = (carpeta.listFiles() ?: emptyArray()).filter { it.isFile && it.name.endsWith(".txt") }.map { it.name.removeSuffix(".txt") }.sorted()

    /** Guarda la ruta del editor con ese nombre. Devuelve false si el nombre no sirve o no se pudo escribir. */
    fun guardar(nombre: String): Boolean {
        if (RutaEditorNativa.nombreValido(nombre) == 0) {
            return false
        }
        return try {
            File(carpeta, "$nombre.txt").writeText(RutaEditorNativa.aTexto())
            true
        } catch (e: Exception) {
            false
        }
    }

    /** Abre la ruta con ese nombre en el editor. Devuelve 0 o el código de error (RE_ERR_*). */
    fun abrir(nombre: String): Int {
        val archivo = File(carpeta, "$nombre.txt")
        return try {
            RutaEditorNativa.deTexto(archivo.readText())
        } catch (e: Exception) {
            -7   // RE_ERR_TEXTO: no se pudo leer
        }
    }

    fun borrar(nombre: String): Boolean = File(carpeta, "$nombre.txt").delete()

    fun existe(nombre: String): Boolean = File(carpeta, "$nombre.txt").exists()

    // ── Borrador ──────────────────────────────────────────────────────────

    fun guardarBorrador() {
        try {
            borrador.writeText(RutaEditorNativa.aTexto())
        } catch (e: Exception) {
            // sin borrador: no pasa nada
        }
    }

    /** Recupera el borrador en el editor (si hay uno y se puede leer). */
    fun recuperarBorrador(): Boolean {
        return try {
            borrador.exists() && RutaEditorNativa.deTexto(borrador.readText()) == 0
        } catch (e: Exception) {
            false
        }
    }

    // ── Historial ─────────────────────────────────────────────────────────

    fun agregarEjecucion(resultado: String, duracionMs: Long, esquivadas: Int, ruta: String) {
        val limpia = ruta.replace("|", " ").replace("\n", " ")
        val linea = "${System.currentTimeMillis() / 1000}|$resultado|${duracionMs / 1000}|$esquivadas|$limpia"
        try {
            val anteriores = if (historial.exists()) historial.readLines().filter { it.isNotBlank() } else emptyList()
            historial.writeText((anteriores + linea).takeLast(30).joinToString("\n") + "\n")
        } catch (e: Exception) {
            // sin historial: no pasa nada
        }
    }

    /** Las ejecuciones, de la más nueva a la más vieja. */
    fun ejecuciones(): List<Ejecucion> {
        if (!historial.exists()) {
            return emptyList()
        }
        return try {
            historial.readLines().mapNotNull { linea ->
                val p = linea.split("|", limit = 5)
                if (p.size < 5) null else Ejecucion(p[0].toLongOrNull() ?: 0L, p[1], p[2].toIntOrNull() ?: 0, p[3].toIntOrNull() ?: 0, p[4])
            }.reversed()
        } catch (e: Exception) {
            emptyList()
        }
    }

    fun borrarHistorial() {
        historial.delete()
    }
}
