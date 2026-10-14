/*
 * VideoCarro.kt — Lee el video de la cámara del carro (la Freenove) por WiFi.
 *
 * GUÍA DE LECTURA
 *  1. La cámara del carro (camara_prueba.ino) sirve http://192.168.4.2/stream: un MJPEG, o sea una conexión que no se cierra y
 *     manda un JPEG tras otro. Cada uno viene así:
 *         --cuadro\r\nContent-Type: image/jpeg\r\nContent-Length: N\r\n\r\n<N bytes de JPEG>\r\n
 *  2. Esta clase abre esa conexión en su propio hilo, saca cada JPEG y se lo pasa a `alJpeg` (en ese mismo hilo: mientras la función
 *     devuelve no se lee el cuadro siguiente, así que si el análisis fuera lento se perderían cuadros en vez de acumular retraso en
 *     la conexión... lo que sí pasa a nivel TCP es que el carro los va descartando por su lado).
 *  3. Si la conexión se cae o no responde, avisa por `alEstado` y reintenta cada segundo hasta que se llame a detener().
 *  4. No decodifica ni analiza nada: eso lo hace CamaraObstaculos.
 */
package com.fsi.carro

import android.util.Log
import java.io.BufferedInputStream
import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL

class VideoCarro(
    private val direccion: String,
    private val alJpeg: (ByteArray, Int) -> Unit,    // (bytes, cuántos bytes del arreglo son el JPEG)
    private val alEstado: (String) -> Unit           // "" = bien, otro texto = problema
) {

    companion object {
        const val ETIQUETA = "VIDEO"
        const val URL_DEFECTO = "http://192.168.4.2/stream"
        private const val TIEMPO_CONEXION_MS = 3000
        private const val TIEMPO_LECTURA_MS = 3000     // sin datos en este tiempo: se da por caída y se reintenta
        private const val ESPERA_REINTENTO_MS = 1000L
        private const val MAX_JPEG = 200_000           // un cuadro más grande que esto no es de esta cámara: se corta la conexión
    }

    @Volatile
    private var activo = false
    private var hilo: Thread? = null

    @Volatile
    private var conexion: HttpURLConnection? = null

    fun iniciar() {
        if (activo) {
            return
        }
        activo = true
        hilo = Thread({ ciclo() }, "video-carro").also { it.start() }
    }

    fun detener() {
        activo = false
        try { conexion?.disconnect() } catch (e: Exception) { /* ya estaba cerrada */ }
        try { hilo?.join(1000) } catch (e: InterruptedException) { /* seguimos */ }
        hilo = null
    }

    private fun ciclo() {
        while (activo) {
            try {
                leerUnaConexion()
            } catch (e: Exception) {
                if (!activo) {
                    break
                }
                Log.w(ETIQUETA, "video del carro: ${e.javaClass.simpleName}: ${e.message}")
                alEstado("Sin video del carro (${e.message ?: e.javaClass.simpleName}); reintentando")
            }
            if (activo) {
                try { Thread.sleep(ESPERA_REINTENTO_MS) } catch (e: InterruptedException) { break }
            }
        }
    }

    /** Le pide a la cámara del carro el tamaño de cuadro HQVGA 240x176 (más rápido: ~12 cuadros por segundo contra ~7 en QVGA). */
    private fun pedirTamanoChico() {
        try {
            val control = direccion.substringBeforeLast("/stream") + "/control?size=hqvga"
            val c = URL(control).openConnection() as HttpURLConnection
            c.connectTimeout = TIEMPO_CONEXION_MS
            c.readTimeout = TIEMPO_CONEXION_MS
            try {
                Log.i(ETIQUETA, "tamaño HQVGA pedido a la cámara del carro: respuesta ${c.responseCode}")
            } finally {
                c.disconnect()
            }
        } catch (e: Exception) {
            Log.w(ETIQUETA, "no se pudo pedir el tamaño a la cámara del carro: ${e.message}")
        }
    }

    private fun leerUnaConexion() {
        pedirTamanoChico()
        val c = URL(direccion).openConnection() as HttpURLConnection
        conexion = c
        c.connectTimeout = TIEMPO_CONEXION_MS
        c.readTimeout = TIEMPO_LECTURA_MS
        c.useCaches = false
        try {
            if (c.responseCode != 200) {
                throw IOException("respuesta HTTP ${c.responseCode}")
            }
            val entrada = BufferedInputStream(c.inputStream, 32 * 1024)
            var cuadros = 0
            var buffer = ByteArray(32 * 1024)
            while (activo) {
                val largo = leerEncabezado(entrada)
                if (largo <= 0 || largo > MAX_JPEG) {
                    throw IOException("largo de cuadro no válido ($largo)")
                }
                if (buffer.size < largo) {
                    buffer = ByteArray(largo + 4096)
                }
                leerExacto(entrada, buffer, largo)
                if (cuadros == 0) {
                    alEstado("")
                    Log.i(ETIQUETA, "llegó el primer cuadro del carro ($largo bytes)")
                }
                cuadros++
                alJpeg(buffer, largo)
            }
        } finally {
            try { c.disconnect() } catch (e: Exception) { /* nada */ }
            conexion = null
        }
    }

    /** Lee las líneas del encabezado de una parte hasta la línea vacía y devuelve el Content-Length (o -1 si no vino). */
    private fun leerEncabezado(entrada: InputStream): Int {
        var largo = -1
        var lineasVacias = 0
        while (activo) {
            val linea = leerLinea(entrada) ?: throw IOException("la conexión se cerró")
            if (linea.isEmpty()) {
                // La línea vacía que cierra el encabezado; las que sobran del cuadro anterior (\r\n final) se saltan.
                if (largo > 0) {
                    return largo
                }
                lineasVacias++
                if (lineasVacias > 4) {
                    throw IOException("encabezado sin Content-Length")
                }
                continue
            }
            if (linea.startsWith("Content-Length:", ignoreCase = true)) {
                largo = linea.substring(15).trim().toIntOrNull() ?: -1
            }
        }
        return -1
    }

    private fun leerLinea(entrada: InputStream): String? {
        val sb = StringBuilder()
        while (true) {
            val b = entrada.read()
            if (b < 0) {
                return null
            }
            if (b == '\n'.code) {
                break
            }
            if (b != '\r'.code) {
                sb.append(b.toChar())
                if (sb.length > 200) {
                    throw IOException("línea demasiado larga en el encabezado")
                }
            }
        }
        return sb.toString()
    }

    private fun leerExacto(entrada: InputStream, destino: ByteArray, largo: Int) {
        var leidos = 0
        while (leidos < largo) {
            val n = entrada.read(destino, leidos, largo - leidos)
            if (n < 0) {
                throw IOException("la conexión se cerró a mitad de un cuadro")
            }
            leidos += n
        }
    }
}
