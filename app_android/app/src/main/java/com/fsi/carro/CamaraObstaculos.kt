/*
 * CamaraObstaculos.kt — La cámara trasera del teléfono como fuente de imágenes para el
 * detector de obstáculos (en C). Sustituye a la ESP32-CAM mientras se prueba.
 *
 * GUÍA DE LECTURA
 *  1. Usa Camera2 directo (la API de cámara de Android, sin librerías y sin servicios de
 *     Google, que el Huawei no tiene). abrir() abre la cámara y cerrar() la libera.
 *  2. La cámara entrega cuadros YUV (un formato de video) en un ImageReader. Se elige el
 *     tamaño 4:3 más pequeño que sea al menos 320x240.
 *  3. alLlegarImagen() procesa cada cuadro en un hilo propio:
 *       convertir()  → lo reduce al tamaño pedido (por defecto 240x176, el de la cámara real
 *                      del carro) y lo pasa a RGB (bytes para el detector y colores ARGB para
 *                      dibujarlo)
 *       ObstaculosNativa.analizarConfig() → el detector en C, con la configuración en vivo
 *     y avisa con un CuadroAnalizado, que lleva su propio ancho y alto. El tamaño se puede
 *     cambiar con la función `tamanoPedido` y se nota en el siguiente cuadro. Si llegan cuadros
 *     más rápido de lo que se procesan, se descartan los viejos (solo importa el último).
 *  4. SUPUESTO DE ORIENTACIÓN: el detector espera una imagen "de pie" en horizontal (el piso
 *     cerca del carro abajo). Se asume el teléfono sostenido en HORIZONTAL con la pantalla
 *     hacia ti; en esa posición la imagen del sensor ya sale derecha (o al revés, que se
 *     corrige con la orientación del sensor).
 *  5. Cada segundo se escribe en logcat (etiqueta VISION) los cuadros por segundo y los ms
 *     medios por cuadro.
 *  5b. MODELO DE OBJETOS (opcional, ModeloEnHilo): si está activo, cada cuadro se le ofrece al modelo, que corre
 *     en su propio hilo a ~10 veces por segundo sin frenar al detector; después FusionNativa.decidir() une el
 *     resultado del detector con lo último del modelo (si no está caducado). El resumen para el navegador sale de
 *     la fusión; el detector de color siempre decide solo si el modelo falla o está caducado.
 *  6. tomarCuadroNuevo() es lo que usa el modo semiautónomo: devuelve el resumen del último
 *     cuadro solo una vez. Si el detector no pudo analizar el cuadro (error), se devuelve
 *     "todo bloqueado": ante la duda, el carro se detiene.
 */
package com.fsi.carro

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.ImageFormat
import android.hardware.camera2.CameraAccessException
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CaptureRequest
import android.media.Image
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.os.SystemClock
import android.util.Log
import android.util.Size

/** Un cuadro ya convertido y analizado. `pixeles` son colores ARGB de ancho x alto para dibujar.
 *  El tamaño viaja con el cuadro: la pantalla no supone ningún tamaño fijo. */
class CuadroAnalizado(
    val numero: Long,
    val ancho: Int,
    val alto: Int,
    val pixeles: IntArray,
    val resultado: IntArray,        // ObstaculosNativa.INICIO_MASCARA + filas * columnas enteros
    val config: IntArray,           // la configuración con la que se analizó
    val msConversion: Double,
    val msAnalisis: Double,
    val fusion: IntArray? = null,          // FusionNativa.ENTEROS_RESULTADO enteros si se usó el modelo de objetos
    val modelo: ResultadoModelo? = null,   // lo último que dijo el modelo cuando se fusionó este cuadro
    val msFusion: Double = 0.0
) {
    /** Los 7 primeros enteros de las zonas que DECIDEN: las fusionadas si hubo fusión, si no las del detector. */
    val zonas: IntArray get() = if (fusion != null && fusion[FusionNativa.R_CODIGO] == 0) fusion else resultado

    /** Columnas y filas de celdas de ESTE cuadro (0 si el análisis falló). */
    val columnas: Int get() = if (resultado[ObstaculosNativa.CODIGO] == 0) resultado[ObstaculosNativa.COLUMNAS_USADAS] else ObstaculosNativa.columnasDe(ancho)
    val filas: Int get() = if (resultado[ObstaculosNativa.CODIGO] == 0) resultado[ObstaculosNativa.FILAS_USADAS] else ObstaculosNativa.filasDe(alto)
}

class CamaraObstaculos(
    private val contexto: Activity,
    private val configActual: () -> IntArray,
    private val tamanoPedido: () -> IntArray = { intArrayOf(ObstaculosNativa.ANCHO_CAMARA, ObstaculosNativa.ALTO_CAMARA) },   // [ancho, alto] de los cuadros que se analizan
    private val avanzando: () -> Boolean = { false },   // true si el carro está avanzando ahora (solo entonces el modelo del piso aprende)
    private val usarModelo: () -> Boolean = { false },  // true si se quiere usar también el modelo de objetos (fusión)
    private val alCuadro: ((CuadroAnalizado) -> Unit)? = null   // se llama en el hilo de la cámara
) {

    companion object {
        const val ETIQUETA = "VISION"
        private const val MAX_IMAGENES = 2
    }

    // Estado de la cámara.
    private var hilo: HandlerThread? = null
    private var manejador: Handler? = null
    private var dispositivo: CameraDevice? = null
    private var sesion: CameraCaptureSession? = null
    private var lector: ImageReader? = null
    private var rotar180 = false

    /** true cuando la cámara está entregando cuadros. */
    @Volatile
    var abierta = false
        private set

    /** Último mensaje de error (vacío si no hay). */
    @Volatile
    var error = ""
        private set

    /** Tamaño real de la cámara elegido, para mostrarlo en pantalla. */
    @Volatile
    var tamanoCamara = ""
        private set

    @Volatile
    var ultimoCuadro: CuadroAnalizado? = null
        private set

    /** Estadística del último segundo. */
    @Volatile
    var fps = 0.0
        private set

    @Volatile
    var msPorCuadro = 0.0
        private set

    private var ultimoTomado = -1L          // solo se usa desde el hilo principal
    private var contadorCuadros = 0L

    // Estadística por ventana de un segundo.
    private var inicioVentanaMs = 0L
    private var cuadrosVentana = 0
    private var sumaMsVentana = 0.0

    // Buffers reutilizados para no pedir memoria en cada cuadro (se rehacen si cambia el tamaño pedido).
    private var bufY = ByteArray(0)
    private var bufU = ByteArray(0)
    private var bufV = ByteArray(0)
    private var anchoSalida = ObstaculosNativa.ANCHO_CAMARA
    private var altoSalida = ObstaculosNativa.ALTO_CAMARA
    private var rgb = ByteArray(anchoSalida * altoSalida * 3)
    private var tablaX = IntArray(anchoSalida)
    private var tablaY = IntArray(altoSalida)
    private var tamanoSensor = Size(ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO)

    // Modelo de objetos (opcional) y su fusión con el detector de color.
    @Volatile
    var modelo: ModeloEnHilo? = null
        private set
    private var configFusion: IntArray? = null
    private val sinCajas = intArrayOf(0)

    // ── Abrir y cerrar ────────────────────────────────────────────────────

    /** Abre la cámara trasera. Devuelve false (y deja `error`) si no se pudo empezar. */
    fun abrir(): Boolean {
        if (hilo != null) {
            return true   // ya está abierta o abriéndose
        }
        if (contexto.checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            return fallar("Falta el permiso de cámara")
        }
        error = ""
        val administrador = contexto.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        try {
            val id = administrador.cameraIdList.firstOrNull {
                administrador.getCameraCharacteristics(it).get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_BACK
            } ?: return fallar("No hay cámara trasera")
            val caracteristicas = administrador.getCameraCharacteristics(id)
            val mapa = caracteristicas.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
                ?: return fallar("La cámara no informa sus tamaños")
            val tamano = elegirTamano(mapa.getOutputSizes(ImageFormat.YUV_420_888))
                ?: return fallar("La cámara no entrega YUV")
            rotar180 = caracteristicas.get(CameraCharacteristics.SENSOR_ORIENTATION) == 270
            tamanoCamara = "${tamano.width}x${tamano.height}"
            tamanoSensor = tamano
            ajustarTamanoSalida()

            hilo = HandlerThread("camara-obstaculos").also { it.start() }
            manejador = Handler(hilo!!.looper)
            lector = ImageReader.newInstance(tamano.width, tamano.height, ImageFormat.YUV_420_888, MAX_IMAGENES).also {
                it.setOnImageAvailableListener({ lectorLleno -> alLlegarImagen(lectorLleno) }, manejador)
            }
            Log.i(ETIQUETA, "abriendo cámara id=$id tamaño=$tamanoCamara girar180=$rotar180")
            administrador.openCamera(id, estadoDispositivo, manejador)
            activarModelo(usarModelo())
            return true
        } catch (e: CameraAccessException) {
            cerrar()
            return fallar("No se pudo abrir la cámara: ${e.message}")
        } catch (e: SecurityException) {
            cerrar()
            return fallar("Sin permiso de cámara: ${e.message}")
        }
    }

    /** Libera la cámara. Se puede llamar varias veces. */
    fun cerrar() {
        val habiaCamara = hilo != null
        abierta = false
        activarModelo(false)
        try { sesion?.close() } catch (e: Exception) { Log.w(ETIQUETA, "cerrando sesión: ${e.message}") }
        try { dispositivo?.close() } catch (e: Exception) { Log.w(ETIQUETA, "cerrando cámara: ${e.message}") }
        sesion = null
        dispositivo = null
        hilo?.quitSafely()
        try { hilo?.join(500) } catch (e: InterruptedException) { /* seguimos */ }
        try { lector?.close() } catch (e: Exception) { Log.w(ETIQUETA, "cerrando lector: ${e.message}") }
        lector = null
        hilo = null
        manejador = null
        if (habiaCamara) {
            Log.i(ETIQUETA, "cámara cerrada")
        }
    }

    /** Enciende o apaga el modelo de objetos (su hilo) sin tocar la cámara. */
    fun activarModelo(activar: Boolean) {
        if (activar) {
            if (modelo == null) {
                modelo = ModeloEnHilo(contexto).also { it.iniciar() }
                configFusion = null
                Log.i(ETIQUETA, "modelo de objetos encendido")
            }
        } else {
            val m = modelo
            modelo = null
            if (m != null) {
                m.detener()
                Log.i(ETIQUETA, "modelo de objetos apagado")
            }
        }
    }

    private fun fallar(mensaje: String): Boolean {
        error = mensaje
        Log.w(ETIQUETA, mensaje)
        return false
    }

    /** El tamaño 4:3 más pequeño que sea de al menos 320x240 (o, si no hay 4:3, el más pequeño de 320 de ancho o más). */
    private fun elegirTamano(tamanos: Array<Size>?): Size? {
        if (tamanos == null) {
            return null
        }
        val grandes = tamanos.filter { it.width >= ObstaculosNativa.ANCHO && it.height >= ObstaculosNativa.ALTO }
        val cuatroTercios = grandes.filter { it.width * 3 == it.height * 4 }
        return (if (cuatroTercios.isNotEmpty()) cuatroTercios else grandes).minByOrNull { it.width * it.height }
    }

    /**
     * Mira qué tamaño de cuadro se pide ahora y, si cambió, rehace los buffers y las tablas de
     * "qué columna y fila de la cámara corresponde a cada píxel de la salida" (reducción por muestreo).
     * Solo se llama desde el hilo de la cámara (y una vez al abrir).
     */
    private fun ajustarTamanoSalida() {
        val pedido = tamanoPedido()
        val ancho = pedido[0].coerceIn(30, 640)
        val alto = pedido[1].coerceIn(30, 480)
        if (ancho == anchoSalida && alto == altoSalida && tablaX.size == ancho && tablaY.size == alto && ancho * alto * 3 == rgb.size) {
            return
        }
        anchoSalida = ancho
        altoSalida = alto
        rgb = ByteArray(ancho * alto * 3)
        tablaX = IntArray(ancho)
        tablaY = IntArray(alto)
        for (x in 0 until ancho) {
            val destino = if (rotar180) ancho - 1 - x else x
            tablaX[x] = destino * tamanoSensor.width / ancho
        }
        for (y in 0 until alto) {
            val destino = if (rotar180) alto - 1 - y else y
            tablaY[y] = destino * tamanoSensor.height / alto
        }
        Log.i(ETIQUETA, "tamaño de análisis: ${ancho}x${alto}")
    }

    // ── Callbacks de la cámara ────────────────────────────────────────────

    private val estadoDispositivo = object : CameraDevice.StateCallback() {
        override fun onOpened(camara: CameraDevice) {
            dispositivo = camara
            crearSesion(camara)
        }

        override fun onDisconnected(camara: CameraDevice) {
            Log.w(ETIQUETA, "cámara desconectada")
            error = "Cámara desconectada"
            camara.close()
            abierta = false
        }

        override fun onError(camara: CameraDevice, codigo: Int) {
            Log.w(ETIQUETA, "error de cámara $codigo")
            error = "Error de cámara ($codigo)"
            camara.close()
            abierta = false
        }
    }

    @Suppress("DEPRECATION")
    private fun crearSesion(camara: CameraDevice) {
        val superficie = lector?.surface ?: return
        try {
            camara.createCaptureSession(listOf(superficie), object : CameraCaptureSession.StateCallback() {
                override fun onConfigured(nueva: CameraCaptureSession) {
                    sesion = nueva
                    try {
                        val peticion = camara.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)
                        peticion.addTarget(superficie)
                        peticion.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                        nueva.setRepeatingRequest(peticion.build(), null, manejador)
                        inicioVentanaMs = SystemClock.elapsedRealtime()
                        abierta = true
                        Log.i(ETIQUETA, "cámara abierta y entregando cuadros")
                    } catch (e: Exception) {
                        fallar("No se pudo iniciar la captura: ${e.message}")
                    }
                }

                override fun onConfigureFailed(fallida: CameraCaptureSession) {
                    fallar("No se pudo configurar la sesión de cámara")
                }
            }, manejador)
        } catch (e: Exception) {
            fallar("No se pudo crear la sesión de cámara: ${e.message}")
        }
    }

    // ── Cada cuadro ───────────────────────────────────────────────────────

    private fun alLlegarImagen(origen: ImageReader) {
        val imagen = try { origen.acquireLatestImage() } catch (e: Exception) { null } ?: return
        try {
            procesar(imagen)
        } catch (e: Exception) {
            Log.w(ETIQUETA, "error procesando un cuadro: ${e.javaClass.simpleName}: ${e.message}")
        } finally {
            imagen.close()
        }
    }

    private fun procesar(imagen: Image) {
        val inicio = System.nanoTime()
        ajustarTamanoSalida()
        val ancho = anchoSalida
        val alto = altoSalida
        val pixeles = IntArray(ancho * alto)
        convertir(imagen, pixeles, ancho, alto)
        val tras_conversion = System.nanoTime()
        val config = configActual()
        val resultado = ObstaculosNativa.analizarConfig(rgb, ancho, alto, config, avanzando(), SystemClock.elapsedRealtime())
        val fin = System.nanoTime()

        // Modelo de objetos: se le ofrece el cuadro (sin esperar) y se fusiona con lo último que dijo.
        var fusion: IntArray? = null
        var ultimoModelo: ResultadoModelo? = null
        var msFusion = 0.0
        val m = modelo
        if (m != null && m.activo && resultado[ObstaculosNativa.CODIGO] == 0) {
            val ahoraMs = SystemClock.elapsedRealtime()
            m.ofrecer(pixeles, ancho, alto, ahoraMs)
            ultimoModelo = m.ultimo
            if (configFusion == null && m.etiquetas.isNotEmpty()) {
                configFusion = FusionNativa.configConClases(m.etiquetas)
            }
            val inicioFusion = System.nanoTime()
            fusion = FusionNativa.decidir(configFusion ?: FusionNativa.configDefecto(), resultado,
                ultimoModelo?.cajas ?: sinCajas, ultimoModelo?.marcaMs ?: 0L, ahoraMs)
            msFusion = (System.nanoTime() - inicioFusion) / 1.0e6
        } else if (m != null && !m.activo && m.error.isNotEmpty()) {
            error = m.error   // el modelo falló: la cámara y el detector de color siguen funcionando
        }

        contadorCuadros++
        val cuadro = CuadroAnalizado(contadorCuadros, ancho, alto, pixeles, resultado, config,
            (tras_conversion - inicio) / 1.0e6, (fin - tras_conversion) / 1.0e6, fusion, ultimoModelo, msFusion)
        ultimoCuadro = cuadro
        actualizarEstadistica(cuadro)
        alCuadro?.invoke(cuadro)
    }

    /** Copia los tres planos del cuadro a arreglos, reduce a ancho x alto y llena `rgb` (bytes) y `pixeles` (ARGB). */
    private fun convertir(imagen: Image, pixeles: IntArray, ancho: Int, alto: Int) {
        val planoY = imagen.planes[0]
        val planoU = imagen.planes[1]
        val planoV = imagen.planes[2]
        bufY = copiarPlano(planoY.buffer, bufY)
        bufU = copiarPlano(planoU.buffer, bufU)
        bufV = copiarPlano(planoV.buffer, bufV)
        val pasoY = planoY.rowStride
        val pasoFilaUV = planoU.rowStride
        val pasoPixelUV = planoU.pixelStride

        var posicion = 0
        var indice = 0
        for (y in 0 until alto) {
            val fy = tablaY[y]
            val filaY = fy * pasoY
            val filaUV = (fy shr 1) * pasoFilaUV
            for (x in 0 until ancho) {
                val fx = tablaX[x]
                val luz = bufY[filaY + fx].toInt() and 0xFF
                val indiceUV = filaUV + (fx shr 1) * pasoPixelUV
                val u = (bufU[indiceUV].toInt() and 0xFF) - 128
                val v = (bufV[indiceUV].toInt() and 0xFF) - 128
                val r = limitar(luz + ((1436 * v) shr 10))
                val g = limitar(luz - ((352 * u + 731 * v) shr 10))
                val b = limitar(luz + ((1814 * u) shr 10))
                rgb[posicion] = r.toByte()
                rgb[posicion + 1] = g.toByte()
                rgb[posicion + 2] = b.toByte()
                posicion += 3
                pixeles[indice++] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
            }
        }
    }

    private fun copiarPlano(origen: java.nio.ByteBuffer, destino: ByteArray): ByteArray {
        val tamano = origen.remaining()
        val arreglo = if (destino.size >= tamano) destino else ByteArray(tamano)
        origen.get(arreglo, 0, tamano)
        return arreglo
    }

    private fun limitar(valor: Int): Int = if (valor < 0) 0 else if (valor > 255) 255 else valor

    // ── Estadística y uso desde el modo semiautónomo ──────────────────────

    private fun actualizarEstadistica(cuadro: CuadroAnalizado) {
        cuadrosVentana++
        sumaMsVentana += cuadro.msConversion + cuadro.msAnalisis
        val ahora = SystemClock.elapsedRealtime()
        if (ahora - inicioVentanaMs >= 1000) {
            fps = cuadrosVentana * 1000.0 / (ahora - inicioVentanaMs)
            msPorCuadro = sumaMsVentana / cuadrosVentana
            Log.i(ETIQUETA, String.format(java.util.Locale.US, "fps=%.1f ms_medio=%.2f (conversion %.2f + analisis %.2f) cuadros=%d tamano=%dx%d umbral=%d min_celdas=%d fila_minima_mil=%d codigo=%d",
                fps, msPorCuadro, cuadro.msConversion, cuadro.msAnalisis, contadorCuadros, cuadro.ancho, cuadro.alto,
                cuadro.config[ObstaculosNativa.CFG_UMBRAL_COLOR], cuadro.config[ObstaculosNativa.CFG_MIN_CELDAS], cuadro.config[ObstaculosNativa.CFG_FILA_MINIMA_MIL],
                cuadro.resultado[ObstaculosNativa.CODIGO]))
            inicioVentanaMs = ahora
            cuadrosVentana = 0
            sumaMsVentana = 0.0
        }
    }

    /**
     * Devuelve el resumen (10 enteros, como ObstaculosNativa.analizar) del último cuadro si es
     * nuevo desde la última vez, o null si no hay cuadro nuevo. Si el detector no pudo analizar
     * el cuadro, devuelve "todo bloqueado" (cercanía 3 en las tres zonas) con el código del error.
     * Solo se llama desde el hilo principal.
     */
    fun tomarCuadroNuevo(): IntArray? {
        val cuadro = ultimoCuadro ?: return null
        if (cuadro.numero == ultimoTomado) {
            return null
        }
        ultimoTomado = cuadro.numero
        val resumen = cuadro.zonas.copyOf(10)   // las zonas fusionadas si se usa el modelo
        if (cuadro.resultado[ObstaculosNativa.CODIGO] != 0) {
            resumen[ObstaculosNativa.CODIGO] = cuadro.resultado[ObstaculosNativa.CODIGO]
            for (zona in 0..2) {
                resumen[1 + 2 * zona] = 1
                resumen[2 + 2 * zona] = 3
            }
        }
        return resumen
    }
}
