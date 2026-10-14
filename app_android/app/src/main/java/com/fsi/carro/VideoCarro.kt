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
 *  7. MODOS DE VIDEO (Trabajo I, docs/latencia_modos.md): jpeg (/stream, el de siempre y el predeterminado), raw (/raw: el cuadro RGB565 sin comprimir con su encabezado: ancho, alto, número y ms
 *     entre captura y envío) y celdas (la cámara manda por UDP el promedio RGB de cada celda de 10x10; el teléfono manda HOLA al puerto 4210 y la cámara le contesta a esa dirección; el JPEG
 *     sigue llegando por /stream en paralelo). Antes de abrir el video se le pide el modo a la cámara con /control?modo=. Si el modo falla (la cámara no lo acepta o no llega nada en 3 s),
 *     se vuelve SOLO a jpeg y se avisa (alFalloModo y el texto de estado). El protocolo de los motores no se toca.
 *  8. El teléfono manda también un PING por UDP cada segundo (en cualquier modo) y la cámara contesta PONG: de ahí sale el ida y vuelta de la red (alRtt).
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
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.HttpURLConnection
import java.net.Inet4Address
import java.net.InetAddress
import java.net.SocketTimeoutException
import java.net.URL

class VideoCarro(
    private val contexto: Context,
    private val direccion: String,
    private val alJpeg: (ByteArray, Int) -> Unit,    // (bytes, cuántos bytes del arreglo son el JPEG)
    private val alEstado: (String) -> Unit,          // "" = bien, otro texto = problema
    modoInicial: String = ModoVideo.JPEG,
    private val alCrudo: (ByteArray, Int, Int, Int, Int, Int) -> Unit = { _, _, _, _, _, _ -> },   // modo raw: (bytes RGB565, largo, ancho, alto, número, ms de la cámara)
    private val alCeldas: (PaqueteCeldas) -> Unit = { },                                            // modo celdas: un paquete ya leído
    private val alRtt: (Double) -> Unit = { },                                                      // ida y vuelta de la red por UDP (PING/PONG), en ms
    private val alFalloModo: (String, String) -> Unit = { _, _ -> }                                 // (modo que falló, motivo): ya se volvió a jpeg
) {

    companion object {
        const val ETIQUETA = "VIDEO"
        const val URL_DEFECTO = "http://192.168.4.2/stream"
        private const val TIEMPO_CONEXION_MS = 3000
        private const val TIEMPO_LECTURA_MS = 3000     // sin datos en este tiempo: se da por caída y se reintenta
        private const val ESPERA_REINTENTO_MS = 1000L
        private const val PERIODO_ESTADISTICA_MS = 5000L
        private const val MAX_JPEG = 200_000           // un cuadro más grande que esto no es de esta cámara: se corta la conexión
        private const val MAX_CRUDO = 700_000          // 640x480 RGB565 son 614 400 bytes
        private const val ESPERA_PRIMER_DATO_MS = 3000L   // un modo que no entrega nada en este tiempo se da por fallido
        private const val PERIODO_UDP_MS = 1000L
    }

    /** El modo de video en uso. Si el pedido falla vuelve a jpeg. */
    @Volatile
    var modo: String = ModoVideo.valido(modoInicial)
        private set

    @Volatile
    private var udpActivo = false
    private var hiloUdp: Thread? = null
    @Volatile
    private var socketUdp: DatagramSocket? = null
    @Volatile
    private var paquetesCeldas = 0
    @Volatile
    private var ultimoPaqueteMs = 0L
    private var inicioModoMs = 0L

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
        iniciarUdp()
    }

    fun detener() {
        activo = false
        try { conexion?.disconnect() } catch (e: Exception) { /* ya estaba cerrada */ }
        try { hilo?.join(1000) } catch (e: InterruptedException) { /* seguimos */ }
        hilo = null
        hiloEstadistica?.interrupt()
        try { hiloEstadistica?.join(500) } catch (e: InterruptedException) { /* seguimos */ }
        hiloEstadistica = null
        udpActivo = false
        try { socketUdp?.close() } catch (e: Exception) { /* ya estaba cerrado */ }
        try { hiloUdp?.join(500) } catch (e: InterruptedException) { /* seguimos */ }
        hiloUdp = null
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

    /** Le pide el modo de video a la cámara (/control?modo=). Devuelve true si la cámara contestó 200. */
    private fun pedirModo(modoPedido: String): Boolean {
        return try {
            val c = abrirConexion(direccion.substringBeforeLast("/stream") + "/control?modo=" + modoPedido)
            c.connectTimeout = TIEMPO_CONEXION_MS
            c.readTimeout = TIEMPO_CONEXION_MS
            try {
                val codigo = c.responseCode
                Log.i(ETIQUETA, "modo de video $modoPedido pedido a la cámara: respuesta $codigo")
                codigo == 200
            } finally {
                c.disconnect()
            }
        } catch (e: Exception) {
            Log.w(ETIQUETA, "no se pudo pedir el modo $modoPedido a la cámara: ${e.message}")
            false
        }
    }

    /** Un modo falló: se anota, se avisa y se sigue en jpeg (el modo de siempre). */
    private fun volverAJpeg(modoFallido: String, motivo: String) {
        Log.w(ETIQUETA, "el modo de video $modoFallido falló ($motivo): vuelvo a jpeg")
        modo = ModoVideo.JPEG
        udpPedirCeldas(false)
        alFalloModo(modoFallido, motivo)
    }

    private fun leerUnaConexion() {
        leerStatsDeLaCamara()
        pedirTamanoChico()
        inicioModoMs = SystemClock.elapsedRealtime()
        paquetesCeldas = 0
        ultimoPaqueteMs = 0L
        if (modo != ModoVideo.JPEG) {
            if (!pedirModo(modo)) {
                volverAJpeg(modo, "la cámara no aceptó el modo (firmware sin ese modo, o sin red)")
                pedirModo(ModoVideo.JPEG)
            }
        } else {
            pedirModo(ModoVideo.JPEG)   // por si la cámara se quedó en otro modo de una sesión anterior
        }
        udpPedirCeldas(modo == ModoVideo.CELDAS)
        if (modo == ModoVideo.RAW) {
            leerCrudo()
            if (modo == ModoVideo.RAW) {
                return   // se cerró bien (se detuvo el video)
            }
            pedirModo(ModoVideo.JPEG)
        }
        leerJpeg()
    }

    /** Modo jpeg (y también el JPEG paralelo del modo celdas): el MJPEG de /stream. */
    private fun leerJpeg() {
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
                if (modo != ModoVideo.CELDAS) {
                    VideoStatsNativa.cuadro(SystemClock.elapsedRealtime())   // en el modo celdas lo que cuenta son los paquetes de celdas, no el JPEG de adorno
                } else {
                    revisarCeldas()
                }
                alJpeg(buffer, largo)
            }
        } finally {
            try { c.disconnect() } catch (e: Exception) { /* nada */ }
            conexion = null
        }
    }

    /** En el modo celdas: si no llega ningún paquete en 3 s, o se dejan de recibir 3 s, se vuelve a jpeg. Se llama con cada cuadro JPEG paralelo. */
    private fun revisarCeldas() {
        val ahora = SystemClock.elapsedRealtime()
        if (paquetesCeldas == 0 && ahora - inicioModoMs > ESPERA_PRIMER_DATO_MS) {
            volverAJpeg(ModoVideo.CELDAS, "no llegó ningún paquete UDP en ${ESPERA_PRIMER_DATO_MS / 1000} s (¿firewall o cámara sin el modo?)")
            pedirModo(ModoVideo.JPEG)
        } else if (paquetesCeldas > 0 && ahora - ultimoPaqueteMs > ESPERA_PRIMER_DATO_MS) {
            volverAJpeg(ModoVideo.CELDAS, "se dejaron de recibir paquetes UDP")
            pedirModo(ModoVideo.JPEG)
        }
    }

    /** Modo raw: /raw manda partes con el cuadro RGB565 sin comprimir y X-Ancho, X-Alto, X-Numero y X-Ms-Captura-Envio. Si no llega nada en 3 s, o hay un error antes del primer cuadro, vuelve a jpeg. */
    private fun leerCrudo() {
        val c = abrirConexion(direccion.substringBeforeLast("/stream") + "/raw")
        conexion = c
        c.connectTimeout = TIEMPO_CONEXION_MS
        c.readTimeout = ESPERA_PRIMER_DATO_MS.toInt()
        c.useCaches = false
        var recibioAlguno = false
        try {
            if (c.responseCode != 200) {
                throw IOException("respuesta HTTP ${c.responseCode}")
            }
            val entrada = BufferedInputStream(c.inputStream, 64 * 1024)
            var buffer = ByteArray(96 * 1024)
            while (activo) {
                val e = leerEncabezadoCrudo(entrada)
                if (e.largo <= 0 || e.largo > MAX_CRUDO || e.ancho <= 0 || e.alto <= 0 || e.ancho * e.alto * 2 != e.largo) {
                    throw IOException("cuadro crudo no válido (largo ${e.largo}, ${e.ancho}x${e.alto})")
                }
                if (buffer.size < e.largo) {
                    buffer = ByteArray(e.largo + 4096)
                }
                leerExacto(entrada, buffer, e.largo)
                if (!recibioAlguno) {
                    recibioAlguno = true
                    c.readTimeout = TIEMPO_LECTURA_MS
                    alEstado("")
                    Log.i(ETIQUETA, "llegó el primer cuadro crudo del carro (${e.ancho}x${e.alto}, ${e.largo} bytes)")
                }
                VideoStatsNativa.cuadro(SystemClock.elapsedRealtime())
                alCrudo(buffer, e.largo, e.ancho, e.alto, e.numero, e.msCamara)
            }
        } catch (e: IOException) {
            if (!activo) {
                return
            }
            if (!recibioAlguno) {
                volverAJpeg(ModoVideo.RAW, e.message ?: e.javaClass.simpleName)
                return
            }
            throw e
        } finally {
            try { c.disconnect() } catch (e: Exception) { /* nada */ }
            conexion = null
        }
    }

    private class EncabezadoCrudo(var largo: Int = -1, var ancho: Int = 0, var alto: Int = 0, var numero: Int = 0, var msCamara: Int = 0)

    /** Como leerEncabezado pero guarda también las líneas X-Ancho, X-Alto, X-Numero y X-Ms-Captura-Envio. */
    private fun leerEncabezadoCrudo(entrada: InputStream): EncabezadoCrudo {
        val e = EncabezadoCrudo()
        var lineasVacias = 0
        while (activo) {
            val linea = leerLinea(entrada) ?: throw IOException("la conexión se cerró")
            if (linea.isEmpty()) {
                if (e.largo > 0) {
                    return e
                }
                lineasVacias++
                if (lineasVacias > 4) {
                    throw IOException("encabezado sin Content-Length")
                }
                continue
            }
            val dos = linea.indexOf(':')
            if (dos < 0) {
                continue
            }
            val clave = linea.substring(0, dos).trim().lowercase()
            val valor = linea.substring(dos + 1).trim()
            when (clave) {
                "content-length" -> e.largo = valor.toIntOrNull() ?: -1
                "x-ancho" -> e.ancho = valor.toIntOrNull() ?: 0
                "x-alto" -> e.alto = valor.toIntOrNull() ?: 0
                "x-numero" -> e.numero = valor.toLongOrNull()?.toInt() ?: 0
                "x-ms-captura-envio" -> e.msCamara = valor.toIntOrNull() ?: 0
            }
        }
        return e
    }

    // ── UDP: HOLA (pedir las celdas) y PING/PONG (ida y vuelta de la red) ─────────────────────────

    @Volatile
    private var pedirCeldasUdp = false

    private fun udpPedirCeldas(si: Boolean) {
        pedirCeldasUdp = si
    }

    private fun iniciarUdp() {
        val destino = try { InetAddress.getByName(URL(direccion).host) } catch (e: Exception) { null } ?: return
        udpActivo = true
        hiloUdp = Thread({ cicloUdp(destino) }, "video-udp").also { it.start() }
    }

    private fun cicloUdp(destino: InetAddress) {
        val socket: DatagramSocket
        try {
            socket = DatagramSocket()
            redCarro?.bindSocket(socket)   // por la red del carro, no por los datos móviles
            socket.soTimeout = 100
        } catch (e: Exception) {
            Log.w(ETIQUETA, "udp: no se pudo abrir el socket: ${e.message}")
            return
        }
        socketUdp = socket
        val buf = ByteArray(2048)
        val celdasBuf = ByteArray(PaqueteCeldasNativa.MAX_CELDAS_BYTES)
        val envios = LongArray(16)
        var proximoHola = 0L
        var proximoPing = 0L
        var numeroPing = 0
        try {
            while (activo && udpActivo) {
                val ahora = SystemClock.elapsedRealtime()
                try {
                    if (pedirCeldasUdp && ahora >= proximoHola) {
                        val hola = "HOLA".toByteArray()
                        socket.send(DatagramPacket(hola, hola.size, destino, PaqueteCeldasNativa.PUERTO))
                        proximoHola = ahora + PERIODO_UDP_MS
                    }
                    if (ahora >= proximoPing) {
                        numeroPing++
                        envios[numeroPing % envios.size] = System.nanoTime()
                        val ping = "PING $numeroPing".toByteArray()
                        socket.send(DatagramPacket(ping, ping.size, destino, PaqueteCeldasNativa.PUERTO))
                        proximoPing = ahora + PERIODO_UDP_MS
                    }
                    val paquete = DatagramPacket(buf, buf.size)
                    socket.receive(paquete)
                    val llegada = SystemClock.elapsedRealtime()
                    val largo = paquete.length
                    if (largo >= 4 && buf[0] == 'C'.code.toByte() && buf[1] == 'E'.code.toByte() && buf[2] == 'L'.code.toByte()) {
                        val r = PaqueteCeldasNativa.leer(buf, largo, celdasBuf)
                        if (r[PaqueteCeldasNativa.R_CODIGO] == 0 && modo == ModoVideo.CELDAS) {
                            val n = r[PaqueteCeldasNativa.R_COLUMNAS] * r[PaqueteCeldasNativa.R_FILAS] * 3
                            paquetesCeldas++
                            ultimoPaqueteMs = llegada
                            VideoStatsNativa.cuadro(llegada)
                            alCeldas(PaqueteCeldas(celdasBuf.copyOf(n), r[PaqueteCeldasNativa.R_COLUMNAS], r[PaqueteCeldasNativa.R_FILAS], r[PaqueteCeldasNativa.R_ANCHO],
                                r[PaqueteCeldasNativa.R_ALTO], r[PaqueteCeldasNativa.R_NUMERO], r[PaqueteCeldasNativa.R_MS_CAMARA], largo, llegada))
                        } else if (r[PaqueteCeldasNativa.R_CODIGO] != 0) {
                            Log.w(ETIQUETA, "udp: paquete de celdas mal formado (código ${r[PaqueteCeldasNativa.R_CODIGO]}, $largo bytes)")
                        }
                    } else if (largo >= 4 && buf[0] == 'P'.code.toByte() && buf[1] == 'O'.code.toByte() && buf[2] == 'N'.code.toByte() && buf[3] == 'G'.code.toByte()) {
                        val n = String(buf, 4, largo - 4).trim().toIntOrNull()
                        if (n != null) {
                            val rttMs = (System.nanoTime() - envios[n % envios.size]) / 1.0e6
                            if (rttMs in 0.0..5000.0) {
                                alRtt(rttMs)
                            }
                        }
                    }
                } catch (e: SocketTimeoutException) {
                    // sin paquetes en 100 ms: se vuelve a revisar si toca mandar HOLA o PING
                }
            }
        } catch (e: Exception) {
            if (activo && udpActivo) {
                Log.w(ETIQUETA, "udp: ${e.javaClass.simpleName}: ${e.message}")
            }
        } finally {
            try { socket.close() } catch (e: Exception) { /* nada */ }
            socketUdp = null
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
