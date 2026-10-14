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
 *  5. RED: busca entre las redes del teléfono una WiFi que esté en la misma subred que la cámara (192.168.4.x) y abre las conexiones POR ESA
 *     red (Network.openConnection): así el video no se va por los datos móviles aunque Android prefiera esos. Si no hay una WiFi así, avisa
 *     (red / redEsDelCarro) y lo anota en el log con la etiqueta VIDEO. El protocolo es el mismo: HTTP y MJPEG.
 *  6. ESTADÍSTICA: cada cuadro se anota en VideoStatsNativa (nucleo_c/video_stats.c) y cada 5 s se escribe en el log con la etiqueta
 *     VIDEO_STATS: fps reales, hueco máximo entre cuadros, cuántos huecos pasaron de 500 ms y la red. Solo números, nunca imágenes.
 *     Al abrir y en cada reintento también se lee /stats de la cámara (reconexiones y señal mínima de la cámara): con el video
 *     abierto la cámara no atiende otra petición, por eso solo se lee antes de abrirlo.
 */
package com.fsi.carro

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.os.SystemClock
import android.util.Log
import java.io.BufferedInputStream
import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.Inet4Address
import java.net.InetAddress
import java.net.URL

class VideoCarro(
    private val contexto: Context,
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
        private const val PERIODO_ESTADISTICA_MS = 5000L
        private const val MAX_JPEG = 200_000           // un cuadro más grande que esto no es de esta cámara: se corta la conexión
    }

    @Volatile
    private var activo = false
    private var hilo: Thread? = null

    @Volatile
    private var conexion: HttpURLConnection? = null

    /** Por qué red va el video (texto para la pantalla y el log) y si es la del carro. */
    @Volatile
    var red = "sin comprobar"
        private set
    @Volatile
    var redEsDelCarro = true
        private set

    /** Lo último que dijo la cámara de su WiFi (/stats): reconexiones y señal mínima de sus últimos 10 s. */
    @Volatile
    var textoCamara = ""
        private set

    private var redCarro: Network? = null
    private var hiloEstadistica: Thread? = null

    fun iniciar() {
        if (activo) {
            return
        }
        activo = true
        VideoStatsNativa.reiniciar()
        elegirRed()
        hilo = Thread({ ciclo() }, "video-carro").also { it.start() }
        hiloEstadistica = Thread({ cicloEstadistica() }, "video-stats").also { it.start() }
    }

    fun detener() {
        activo = false
        try { conexion?.disconnect() } catch (e: Exception) { /* ya estaba cerrada */ }
        try { hilo?.join(1000) } catch (e: InterruptedException) { /* seguimos */ }
        hilo = null
        hiloEstadistica?.interrupt()
        try { hiloEstadistica?.join(500) } catch (e: InterruptedException) { /* seguimos */ }
        hiloEstadistica = null
    }

    // ── Red ───────────────────────────────────────────────────────────────

    /** Busca la red WiFi que está en la misma subred que la cámara y anota en el log por cuál red irá el video. */
    private fun elegirRed() {
        redCarro = null
        val cm = contexto.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
        if (cm == null) {
            red = "no se pudo consultar la red"
            redEsDelCarro = false
            Log.w(ETIQUETA, "red_video: no hay ConnectivityManager")
            return
        }
        val activa = cm.activeNetwork
        val activaTexto = tipoDeRed(cm, activa)
        try {
            val destino = InetAddress.getByName(URL(direccion).host)   // la dirección es un número (192.168.4.2): no consulta DNS
            for (n in cm.allNetworks) {
                val caps = cm.getNetworkCapabilities(n) ?: continue
                if (!caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
                    continue
                }
                val propiedades = cm.getLinkProperties(n) ?: continue
                if (propiedades.linkAddresses.any { mismaSubred(it.address, it.prefixLength, destino) }) {
                    redCarro = n
                }
            }
        } catch (e: Exception) {
            Log.w(ETIQUETA, "red_video: no se pudo revisar las redes (${e.message})")
        }
        if (redCarro != null) {
            red = "WiFi del carro"
            redEsDelCarro = true
            Log.i(ETIQUETA, "red_video=WIFI_DEL_CARRO (conexiones vinculadas a esa red) red_activa_del_telefono=$activaTexto")
        } else {
            red = "NO es la red del carro (red activa: $activaTexto)"
            redEsDelCarro = false
            Log.w(ETIQUETA, "red_video=NO_ES_LA_DEL_CARRO red_activa_del_telefono=$activaTexto: no hay una WiFi en la subred de la cámara")
        }
    }

    private fun tipoDeRed(cm: ConnectivityManager, red: Network?): String {
        if (red == null) {
            return "ninguna"
        }
        val caps = cm.getNetworkCapabilities(red) ?: return "desconocida"
        return when {
            caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> "WiFi"
            caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) -> "datos móviles"
            else -> "otra"
        }
    }

    /** ¿La dirección local `propia` (con su prefijo) está en la misma subred que `destino`? Solo IPv4. */
    private fun mismaSubred(propia: InetAddress, prefijo: Int, destino: InetAddress): Boolean {
        if (propia !is Inet4Address || destino !is Inet4Address || prefijo !in 1..32) {
            return false
        }
        val a = propia.address
        val b = destino.address
        var bits = prefijo
        var i = 0
        while (bits > 0 && i < 4) {
            val mascara = if (bits >= 8) 0xFF else (0xFF shl (8 - bits)) and 0xFF
            if ((a[i].toInt() and mascara) != (b[i].toInt() and mascara)) {
                return false
            }
            bits -= 8
            i++
        }
        return true
    }

    /** Abre una conexión HTTP por la red del carro si se encontró; si no, por la red por defecto del teléfono. */
    private fun abrirConexion(url: String): HttpURLConnection {
        val destino = URL(url)
        val n = redCarro
        return (if (n != null) n.openConnection(destino) else destino.openConnection()) as HttpURLConnection
    }

    // ── Estadística ───────────────────────────────────────────────────────

    /** Cada 5 s escribe en el log (VIDEO_STATS) los números del video de los últimos 10 s. */
    private fun cicloEstadistica() {
        while (activo) {
            try { Thread.sleep(PERIODO_ESTADISTICA_MS) } catch (e: InterruptedException) { break }
            if (!activo) {
                break
            }
            Log.i("VIDEO_STATS", textoEstadistica())
        }
    }

    /** Una línea con los números del video (para el log y la pantalla de visión). */
    fun textoEstadistica(): String {
        val r = VideoStatsNativa.resumen(SystemClock.elapsedRealtime())
        return String.format(java.util.Locale.US,
            "fps=%.1f hueco_max_ms=%d huecos_mayores_500ms=%d cuadros_10s=%d hueco_actual_ms=%d red=%s%s",
            r[VideoStatsNativa.FPS_X10] / 10.0, r[VideoStatsNativa.HUECO_MAX_MS], r[VideoStatsNativa.HUECOS_LARGOS],
            r[VideoStatsNativa.CUADROS], r[VideoStatsNativa.HUECO_ACTUAL_MS], red,
            if (textoCamara.isEmpty()) "" else " camara[$textoCamara]")
    }

    /** Lee /stats de la cámara (antes de abrir el video) y se queda con las reconexiones y la señal mínima. Solo números. */
    private fun leerStatsDeLaCamara() {
        try {
            val c = abrirConexion(direccion.substringBeforeLast("/stream") + "/stats")
            c.connectTimeout = 1500
            c.readTimeout = 1500
            try {
                val cuerpo = c.inputStream.bufferedReader().readText()
                fun numero(clave: String): String = Regex("\"" + clave + "\":(-?[0-9.]+)").find(cuerpo)?.groupValues?.get(1) ?: "?"
                textoCamara = "reconexiones=" + numero("reconexiones") + " rssi_min_10s=" + numero("rssi_min_10s") + " fps_envio=" + numero("fps_envio") + " edad_max_ms=" + numero("edad_max_ms")
                Log.i("VIDEO_STATS", "camara: $textoCamara")
            } finally {
                c.disconnect()
            }
        } catch (e: Exception) {
            Log.w(ETIQUETA, "no se pudo leer /stats de la cámara: ${e.message}")
        }
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
            val c = abrirConexion(control)
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
        leerStatsDeLaCamara()
        pedirTamanoChico()
        val c = abrirConexion(direccion)
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
                VideoStatsNativa.cuadro(SystemClock.elapsedRealtime())
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
