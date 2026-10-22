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
import android.graphics.Bitmap
import android.graphics.BitmapFactory
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

/** Lo que la cámara vio girar al carro entre dos cuadros consecutivos (giro visual). */
class GiroVisual(
    val numero: Long,
    val giroX10: Int,          // grados por 10 (+ = a la derecha); 0 si no es confiable
    val dtMs: Int,             // tiempo entre los dos cuadros comparados
    val confiable: Boolean,
    val motivo: Int,           // MovimientoVisualNativa.MOTIVO
    val confianzaMil: Int,
    val energia: Int = 0       // energía de bordes de la huella (la escena tiene detalles si pasa de 60)
)

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
    val msFusion: Double = 0.0,
    val giro: GiroVisual? = null,          // lo que se vio girar respecto al cuadro anterior
    val marcaMs: Long = 0L,                // hora (SystemClock.elapsedRealtime) en que el cuadro LLEGÓ al teléfono
    val msHastaResultado: Double = 0.0,    // ms desde que llegó hasta que el detector (y la fusión) terminaron
    val corredor: IntArray? = null,        // CorredorNativa.aplicar de este cuadro (null = corredor apagado)
    val cualquiera: IntArray? = null,      // CualquieraNativa.actualizar de este cuadro (null = modo «cualquier objeto» apagado)
    val flujo: IntArray? = null            // FlujoNativa.analizar de este cuadro (null = flujo apagado o sin cuadro de referencia)
) {
    /** Los 7 primeros enteros de las zonas que DECIDEN: las fusionadas si hubo fusión, si no las del detector. */
    val zonas: IntArray get() = if (fusion != null && fusion[FusionNativa.R_CODIGO] == 0) fusion else resultado

    /** Columnas y filas de celdas de ESTE cuadro (0 si el análisis falló). */
    val columnas: Int get() = if (resultado[ObstaculosNativa.CODIGO] == 0) resultado[ObstaculosNativa.COLUMNAS_USADAS] else ObstaculosNativa.columnasDe(ancho)
    val filas: Int get() = if (resultado[ObstaculosNativa.CODIGO] == 0) resultado[ObstaculosNativa.FILAS_USADAS] else ObstaculosNativa.filasDe(alto)
}

/** Lo que se aprendió con el botón «Aprender piso» (ver obstaculos.h: InformePiso). */
class InformePisoAprendido(
    val grupos: Int,
    val brilloMedio: Int,
    val muyOscuro: Boolean,
    val muyVariado: Boolean,
    val descartadoPct: Int,
    val cuadros: Int,
    val colores: List<IntArray>,    // R, G, B de cada color aprendido
    val pesosPct: List<Int>         // % de la zona de avance que respalda cada color
) {
    /** El texto para la pantalla: cuántos colores aprendió, su brillo medio y los avisos. */
    fun texto(): String {
        val sb = StringBuilder("Piso aprendido: $grupos color(es), brillo medio $brilloMedio de 255 ($cuadros cuadros)")
        for (i in colores.indices) {
            sb.append("\n  color ${i + 1}: RGB ${colores[i][0]},${colores[i][1]},${colores[i][2]} (${pesosPct[i]} % de la zona)")
        }
        if (muyOscuro) {
            sb.append("\n⚠ El piso es MUY OSCURO: un obstáculo oscuro casi no se distingue de él.")
        }
        if (muyVariado) {
            sb.append("\n⚠ El piso es MUY VARIADO ($descartadoPct % de la zona quedó en colores de poco apoyo): revisa que no haya objetos y que la luz sea pareja.")
        }
        return sb.toString()
    }
}

class CamaraObstaculos(
    private val contexto: Activity,
    private val configActual: () -> IntArray,
    private val tamanoPedido: () -> IntArray = { intArrayOf(ObstaculosNativa.ANCHO_CAMARA, ObstaculosNativa.ALTO_CAMARA) },   // [ancho, alto] de los cuadros que se analizan
    private val avanzando: () -> Boolean = { false },   // true si el carro está avanzando ahora (solo entonces el modelo del piso aprende)
    private val usarModelo: () -> Boolean = { false },  // true si se quiere usar también el modelo de objetos (fusión)
    private val configCorredor: () -> IntArray = { IntArray(0) },   // CorredorNativa.CFG_*: vacío o activo = 0 = corredor apagado
    private val configCualquiera: () -> IntArray = { IntArray(0) }, // CualquieraNativa.CFG_*: vacío o activo = 0 = modo «cualquier objeto» apagado
    private val configFlujo: () -> IntArray = { IntArray(0) },     // FlujoNativa.CFG_*: vacío o activo = 0 = flujo del piso apagado
    private val alCuadro: ((CuadroAnalizado) -> Unit)? = null   // se llama en el hilo de la cámara
) {

    companion object {
        const val ETIQUETA = "VISION"
        private const val MAX_IMAGENES = 2
        const val LATENCIA_VIDEO_CARRO_MS = 320   // retraso del video del carro hasta el teléfono (docs/latencia.md)
        const val APRENDER_PISO_MS = 1000L        // cuánto dura el aprendizaje del piso con el carro quieto
    }

    // Estado de la cámara.
    private var hilo: HandlerThread? = null
    private var manejador: Handler? = null
    private var dispositivo: CameraDevice? = null
    private var sesion: CameraCaptureSession? = null
    private var lector: ImageReader? = null
    private var rotar180 = false
    /** true si los cuadros salen del video del carro (la sesión compartida, SesionVideo) y no de la cámara del teléfono. */
    private var enSesionVideo = false
    private val videoCarro: VideoCarro? get() = if (enSesionVideo) SesionVideo.actual else null   // el VideoCarro abierto ahora (null mientras se reabre)

    /** true = usar siempre el video del carro (sesión compartida), aunque en la configuración de visión esté elegida la cámara del teléfono. Lo usa Manual para «Ver detección». */
    var forzarVideoCarro = false

    /** Qué usuario de la sesión de video es esta cámara (SesionNativa.USUARIO_*): Visión, Ejecutar ruta... Hay que ponerlo ANTES de abrir(). */
    var usuarioVideo = SesionNativa.USUARIO_VISION

    /** El video de la sesión compartida cambió de estado: se copia el error para que lo vean las pantallas. */
    private val alCambiarSesionVideo: () -> Unit = {
        if (enSesionVideo) {
            val fallo = SesionVideo.mensajeFallo
            error = fallo
            if (fallo.isNotEmpty()) {
                abierta = false
            }
        }
    }

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

    /** El cuadro que devolvió la última llamada a tomarCuadroNuevo() (con su hora de llegada y sus tiempos): lo lee la prueba de obstáculos. */
    @Volatile
    var cuadroTomado: CuadroAnalizado? = null
        private set
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
    private var tablasListas = false   // las tablas de muestreo ya se llenaron para el sensor y el tamaño actuales
    private var firmaCeldasLog = 0     // para escribir en el log solo cuando cambian las celdas marcadas (diagnóstico)

    // Aprender el piso (botón «Aprender piso»): con el carro QUIETO, durante ~1 s los cuadros se acumulan (en C) y al final se agrupan los colores de toda la
    // zona de avance. Todo ocurre en el hilo de la cámara: la pantalla solo lo pide con aprenderPiso() y lee el informe.
    @Volatile
    private var pedirAprender = false
    private var aprendiendoHasta = 0L

    /** true desde que se pidió aprender el piso hasta que termina (~1 s). */
    @Volatile
    var aprendiendoPiso = false
        private set

    /** Lo que se aprendió la última vez (null = nada todavía, o falló: ver errorAprender). */
    @Volatile
    var informePiso: InformePisoAprendido? = null
        private set

    @Volatile
    var errorAprender = ""
        private set

    /** Hora (SystemClock.elapsedRealtime) en que terminó de aprender el piso por última vez; 0 = nunca. */
    @Volatile
    var pisoAprendidoMs = 0L
        private set

    /** Pide aprender el piso: el carro debe estar QUIETO sobre el piso por el que va a pasar y sin objetos en la zona de abajo. */
    fun aprenderPiso() {
        errorAprender = ""
        informePiso = null
        aprendiendoPiso = true
        pedirAprender = true
    }

    // Giro visual: campo de visión CALIBRADO (lo pone la pantalla desde la calibración; 0 = no calibrado) y el de la cámara según el
    // teléfono (distancia focal y tamaño del sensor, se calcula al abrir). Se usa el calibrado y, si no hay, el de la cámara.
    @Volatile
    var fovX10 = 0
    @Volatile
    var fovCamaraX10 = MovimientoVisualNativa.FOV_DEFECTO_X10
    val fovEfectivoX10: Int get() = if (fovX10 > 0) fovX10 else fovCamaraX10
    @Volatile
    var ultimoGiro: GiroVisual? = null
        private set
    private val candadoGiro = Any()
    private var giroPendienteX10 = 0             // suma de los giros confiables que nadie ha tomado todavía
    private var dtPendienteMs = 0                // y el tiempo que cubren
    // Hora de llegada de los últimos cuadros (anillo): si el estimador rechazó cuadros y comparó contra uno más viejo, el tiempo del giro
    // abarca ese hueco (MovimientoVisualNativa.CUADROS cuadros hacia atrás).
    private val marcasCuadrosMs = LongArray(8)
    private var cuentaMarcas = 0

    // Modelo de objetos (opcional) y su fusión con el detector de color.
    @Volatile
    var modelo: ModeloEnHilo? = null
        private set
    private var configFusion: IntArray? = null
    private val sinCajas = intArrayOf(0)

    // ── Abrir y cerrar ────────────────────────────────────────────────────

    /** Abre la cámara trasera. Devuelve false (y deja `error`) si no se pudo empezar. */
    fun abrir(): Boolean {
        if (hilo != null || enSesionVideo) {
            return true   // ya está abierta o abriéndose
        }
        CalibracionGuardada(contexto).aplicarMascaraAlDetector()   // los artefactos fijos de la imagen que se calibraron en «Calibrar visión» se ignoran en cualquier pantalla con cámara
        val configVision = ConfigVision(contexto)
        if (forzarVideoCarro || configVision.fuenteCarro) {
            return abrirVideoCarro(configVision.urlVideoCarro)
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
            fovCamaraX10 = calcularFovX10(caracteristicas, tamano)
            tamanoCamara = "${tamano.width}x${tamano.height}"
            tamanoSensor = tamano
            tablasListas = false   // otro sensor: hay que rehacer las tablas aunque el tamaño de salida sea el mismo
            MovimientoVisualNativa.reiniciar()
            cuentaMarcas = 0
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

    /**
     * Retraso (ms) entre lo que pasa y el cuadro que llega: 0 con la cámara del teléfono; con el video del carro ~320 ms (medido,
     * docs/latencia.md). El navegador lo usa para no contar el giro de los cuadros que todavía muestran al carro sin girar.
     */
    val latenciaVisualMs: Int get() {
        if (!enSesionVideo) {
            return 0
        }
        // jpeg: la medida con cronómetro (docs/latencia.md). raw y celdas: ms de la cámara (captura a envío) + la mitad del ida y vuelta de la red; es una ESTIMACIÓN (no incluye
        // lo que tarda en bajar un cuadro entero por WiFi en raw) hasta que se mida con cronómetro (docs/latencia_modos.md).
        if (modoVideo == ModoVideo.JPEG || cuentaCamara.get() == 0L) {
            return LATENCIA_VIDEO_CARRO_MS
        }
        return (sumaCamaraMs.get() / cuentaCamara.get() + ultimoRttMs.get() / 2).toInt()
    }

    /** Texto para la pantalla de visión: los números del video del carro y un aviso si NO va por la red del carro. Vacío con la cámara del teléfono. */
    fun textoVideo(): String {
        val v = videoCarro ?: return ""
        return "Video del carro, modo ${v.modo} (últimos 10 s): " + v.textoEstadistica() + (if (!v.redEsDelCarro) "\n⚠ EL VIDEO NO VA POR LA RED DEL CARRO: " + v.red else "") +
            (if (avisoModoVideo.isNotEmpty()) "\n⚠ $avisoModoVideo" else "")
    }

    /** true si los cuadros salen del video del carro y no de la cámara del teléfono. */
    val esVideoCarro: Boolean get() = enSesionVideo

    /** Empieza a leer el video de la cámara del carro (no necesita el permiso de cámara). Devuelve false si la dirección no es válida. */
    private fun abrirVideoCarro(url: String): Boolean {
        error = ""
        rotar180 = false
        tamanoCamara = "carro"
        tamanoSensor = Size(ObstaculosNativa.ANCHO_CAMARA, ObstaculosNativa.ALTO_CAMARA)   // se corrige con el primer cuadro
        tablasListas = false
        fovCamaraX10 = MovimientoVisualNativa.FOV_DEFECTO_X10   // no se conoce el de esta cámara: 60 hasta calibrar (paso 6)
        MovimientoVisualNativa.reiniciar()
        cuentaMarcas = 0
        ajustarTamanoSalida()
        inicioVentanaMs = SystemClock.elapsedRealtime()
        avisoModoVideo = ""
        sumaCamaraMs.set(0)
        cuentaCamara.set(0)
        val consumidor = object : ConsumidorVideo {
            override fun alJpeg(bytes: ByteArray, largo: Int) = alJpegDelCarro(bytes, largo)
            override fun alCrudo(bytes: ByteArray, largo: Int, ancho: Int, alto: Int, numero: Int, msCamara: Int) = alCrudoDelCarro(bytes, largo, ancho, alto, numero, msCamara)
            override fun alCeldas(paquete: PaqueteCeldas) = alCeldasDelCarro(paquete)
            override fun alRtt(ms: Double) {
                ultimoRttMs.set(ms.toLong())
                estadisticaModo.rtt(ms)
            }
            override fun alFalloModo(modoFallido: String, motivo: String) {
                avisoModoVideo = "El modo de video «$modoFallido» falló ($motivo): sigo con JPEG."
                Log.w(ETIQUETA, avisoModoVideo)
            }
        }
        enSesionVideo = true
        SesionVideo.escuchar(alCambiarSesionVideo)
        Log.i(ETIQUETA, "usando el video del carro de la sesión compartida ($url)")
        iniciarCadena()
        SesionVideo.entrar(contexto, usuarioVideo, consumidor)
        activarModelo(usarModelo())
        return true
    }

    // ── La cadena del video del carro: lectura → decodificación → análisis ──
    // Tres hilos con una COLA DE 1 entre cada etapa: si una etapa va más lenta, el cuadro viejo se TIRA y la siguiente toma el más nuevo (nunca se acumula retraso,
    // ni en la cola ni en el búfer del socket: el hilo de lectura siempre lo deja vacío). Los objetos de cada cuadro se reutilizan (sin crear basura en cada cuadro).

    /** Un JPEG recién llegado. */
    private class CuadroJpeg {
        var bytes = ByteArray(32 * 1024)
        var largo = 0
        var llegadaMs = 0L
        var inicioNs = 0L
    }

    /** Un cuadro ya decodificado a pixeles ARGB. */
    private class CuadroDecodificado {
        var pixeles = IntArray(0)
        var ancho = 0
        var alto = 0
        var llegadaMs = 0L
        var inicioNs = 0L
        var bytesJpeg = 0
    }

    /** Una cola con lugar para UN solo elemento: poner() deja el nuevo y devuelve el que estaba (ya viejo) para reciclarlo. */
    private class ColaDeUno<T : Any> {
        private val candado = Object()
        private var valor: T? = null
        private var cerrada = false

        fun poner(nuevo: T): T? = synchronized(candado) {
            val viejo = valor
            valor = nuevo
            candado.notifyAll()
            viejo
        }

        /** Espera hasta `esperaMs` un elemento. Devuelve null si pasó el tiempo o si la cola se cerró. */
        fun tomar(esperaMs: Long): T? = synchronized(candado) {
            val limite = SystemClock.elapsedRealtime() + esperaMs
            while (valor == null && !cerrada) {
                val resta = limite - SystemClock.elapsedRealtime()
                if (resta <= 0) {
                    return null
                }
                try { candado.wait(resta) } catch (e: InterruptedException) { return null }
            }
            val v = valor
            valor = null
            v
        }

        fun cerrar() = synchronized(candado) {
            cerrada = true
            valor = null
            candado.notifyAll()
        }

        /** Vuelve a dejarla lista para usarse (después de cerrar). */
        fun abrir() = synchronized(candado) {
            cerrada = false
            valor = null
        }
    }

    private val colaJpeg = ColaDeUno<CuadroJpeg>()
    private val colaDecodificada = ColaDeUno<CuadroDecodificado>()
    private val librosJpeg = java.util.concurrent.ConcurrentLinkedQueue<CuadroJpeg>()
    private val librosDecodificados = java.util.concurrent.ConcurrentLinkedQueue<CuadroDecodificado>()
    private var hiloDecodificador: Thread? = null
    private var hiloAnalisis: Thread? = null
    @Volatile
    private var cadenaActiva = false
    private var mapaReutilizable: Bitmap? = null
    private val estadisticaModo = EstadisticaModo { modoVideo }

    /** El modo de video en uso (ModoVideo): jpeg, raw o celdas. Si el que se pidió falló, es jpeg. */
    val modoVideo: String get() = videoCarro?.modo ?: ModoVideo.JPEG

    /** Si el modo de video pedido falló y la app volvió a jpeg: el aviso para la pantalla ("" si no pasó nada). */
    @Volatile
    var avisoModoVideo: String = ""
        private set

    /** Lo que se calcula como retraso de las cámaras de los modos raw y celdas: ms de la cámara (captura a envío) + la mitad del ida y vuelta de la red. */
    private val sumaCamaraMs = java.util.concurrent.atomic.AtomicLong(0)
    private val cuentaCamara = java.util.concurrent.atomic.AtomicLong(0)
    private val ultimoRttMs = java.util.concurrent.atomic.AtomicLong(0)

    private fun iniciarCadena() {
        colaJpeg.abrir()
        colaDecodificada.abrir()
        colaCeldas.abrir()
        ultimoNumeroCeldas = -1
        cadenaActiva = true
        hiloCeldas = Thread({ cicloCeldas() }, "video-celdas").also { it.start() }
        hiloDecodificador = Thread({ cicloDecodificador() }, "video-decodifica").also { it.start() }
        hiloAnalisis = Thread({ cicloAnalisisVideo() }, "video-analiza").also { it.start() }
    }

    private fun detenerCadena() {
        cadenaActiva = false
        colaJpeg.cerrar()
        colaDecodificada.cerrar()
        colaCeldas.cerrar()
        try { hiloCeldas?.join(500) } catch (e: InterruptedException) { /* seguimos */ }
        hiloCeldas = null
        try { hiloDecodificador?.join(500) } catch (e: InterruptedException) { /* seguimos */ }
        try { hiloAnalisis?.join(500) } catch (e: InterruptedException) { /* seguimos */ }
        hiloDecodificador = null
        hiloAnalisis = null
        mapaReutilizable = null
    }

    /** Etapa 1 (hilo de lectura de VideoCarro): copia el JPEG a un objeto libre y lo deja en la cola; el hilo vuelve ya a leer el socket. */
    private fun alJpegDelCarro(bytes: ByteArray, largo: Int) {
        if (modoVideo == ModoVideo.CELDAS) {
            estadisticaModo.jpegParalelo()   // el JPEG paralelo del modo celdas solo se cuenta (fps): no se decodifica ni se analiza
            return
        }
        val c = librosJpeg.poll() ?: CuadroJpeg()
        if (c.bytes.size < largo) {
            c.bytes = ByteArray(largo + 4096)
        }
        System.arraycopy(bytes, 0, c.bytes, 0, largo)
        c.largo = largo
        c.llegadaMs = SystemClock.elapsedRealtime()
        c.inicioNs = System.nanoTime()
        val viejo = colaJpeg.poner(c)
        if (viejo != null) {
            estadisticaModo.descartado()
            librosJpeg.offer(viejo)
        }
    }

    /** Modo raw: el cuadro RGB565 (primero el byte alto de cada píxel) pasa a ARGB y entra directo a la etapa de análisis, sin decodificar. */
    private fun alCrudoDelCarro(bytes: ByteArray, largo: Int, w: Int, h: Int, numero: Int, msCamara: Int) {
        val llegadaMs = SystemClock.elapsedRealtime()
        val inicioNs = System.nanoTime()
        estadisticaModo.camara(msCamara)
        sumaCamaraMs.addAndGet(msCamara.toLong())
        cuentaCamara.incrementAndGet()
        val d = librosDecodificados.poll() ?: CuadroDecodificado()
        if (d.pixeles.size != w * h) {
            d.pixeles = IntArray(w * h)
        }
        var o = 0
        for (i in 0 until w * h) {
            val alto = bytes[o].toInt() and 0xFF
            val bajo = bytes[o + 1].toInt() and 0xFF
            o += 2
            val r5 = alto shr 3
            val g6 = ((alto and 7) shl 3) or (bajo shr 5)
            val b5 = bajo and 0x1F
            val r = (r5 shl 3) or (r5 shr 2)
            val g = (g6 shl 2) or (g6 shr 4)
            val b = (b5 shl 3) or (b5 shr 2)
            d.pixeles[i] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
        }
        d.ancho = w
        d.alto = h
        d.llegadaMs = llegadaMs
        d.inicioNs = inicioNs
        d.bytesJpeg = largo
        val viejo = colaDecodificada.poner(d)
        if (viejo != null) {
            estadisticaModo.descartado()
            librosDecodificados.offer(viejo)
        }
    }

    // ── Modo celdas: los paquetes UDP con el promedio de cada celda entran a su propia etapa de análisis (cola de 1) ──
    private val colaCeldas = ColaDeUno<PaqueteCeldas>()
    private var hiloCeldas: Thread? = null
    private var ultimoNumeroCeldas = -1

    /** Etapa 1 de las celdas (hilo UDP): cuenta lo que falta y deja el paquete en la cola. */
    private fun alCeldasDelCarro(p: PaqueteCeldas) {
        if (ultimoNumeroCeldas != -1 && p.numero - ultimoNumeroCeldas > 1) {
            estadisticaModo.perdidos(p.numero - ultimoNumeroCeldas - 1)
        }
        ultimoNumeroCeldas = p.numero
        estadisticaModo.camara(p.msCamara)
        sumaCamaraMs.addAndGet(p.msCamara.toLong())
        cuentaCamara.incrementAndGet()
        if (colaCeldas.poner(p) != null) {
            estadisticaModo.descartado()
        }
    }

    /** Etapa 2 de las celdas: dibuja las celdas como cuadro (cada celda de 10x10 de un color) y corre el detector sobre ellas. */
    private fun cicloCeldas() {
        while (cadenaActiva) {
            val p = colaCeldas.tomar(1000) ?: continue
            try {
                val ancho = p.columnas * ObstaculosNativa.LADO_CELDA
                val alto = p.filas * ObstaculosNativa.LADO_CELDA
                val pixeles = IntArray(ancho * alto)
                for (f in 0 until p.filas) {
                    for (c in 0 until p.columnas) {
                        val k = (f * p.columnas + c) * 3
                        val argb = (0xFF shl 24) or ((p.celdas[k].toInt() and 0xFF) shl 16) or ((p.celdas[k + 1].toInt() and 0xFF) shl 8) or (p.celdas[k + 2].toInt() and 0xFF)
                        for (y in 0 until ObstaculosNativa.LADO_CELDA) {
                            val fila = (f * ObstaculosNativa.LADO_CELDA + y) * ancho + c * ObstaculosNativa.LADO_CELDA
                            java.util.Arrays.fill(pixeles, fila, fila + ObstaculosNativa.LADO_CELDA, argb)
                        }
                    }
                }
                if (!abierta) {
                    abierta = true
                    inicioVentanaMs = SystemClock.elapsedRealtime()
                    Log.i(ETIQUETA, "celdas del carro entregando cuadros (${p.columnas}x${p.filas} celdas de una imagen ${p.ancho}x${p.alto})")
                }
                val inicioNs = System.nanoTime()
                analizar(ancho, alto, pixeles, inicioNs, System.nanoTime(), p.llegadaMs, p.celdas, p.columnas, p.filas)
                estadisticaModo.cuadro(p.llegadaMs, SystemClock.elapsedRealtime(), p.bytes)
            } catch (e: Exception) {
                Log.w(ETIQUETA, "error procesando celdas del carro: ${e.javaClass.simpleName}: ${e.message}")
            }
        }
    }

    /** Etapa 2: decodifica el JPEG más nuevo (reutilizando el mapa de bits) a pixeles y se lo deja a la etapa de análisis. */
    private fun cicloDecodificador() {
        while (cadenaActiva) {
            val jpeg = colaJpeg.tomar(1000) ?: continue
            try {
                val opciones = BitmapFactory.Options()
                opciones.inMutable = true
                opciones.inBitmap = mapaReutilizable
                var mapa: Bitmap? = try {
                    BitmapFactory.decodeByteArray(jpeg.bytes, 0, jpeg.largo, opciones)
                } catch (e: IllegalArgumentException) {
                    opciones.inBitmap = null   // el mapa guardado no es del tamaño de este cuadro: se hace uno nuevo
                    BitmapFactory.decodeByteArray(jpeg.bytes, 0, jpeg.largo, opciones)
                }
                if (mapa == null) {
                    Log.w(ETIQUETA, "un cuadro del carro no se pudo decodificar (${jpeg.largo} bytes)")
                    continue
                }
                mapaReutilizable = mapa
                val w = mapa.width
                val h = mapa.height
                val d = librosDecodificados.poll() ?: CuadroDecodificado()
                if (d.pixeles.size != w * h) {
                    d.pixeles = IntArray(w * h)
                }
                mapa.getPixels(d.pixeles, 0, w, 0, 0, w, h)
                d.ancho = w
                d.alto = h
                d.llegadaMs = jpeg.llegadaMs
                d.inicioNs = jpeg.inicioNs
                d.bytesJpeg = jpeg.largo
                mapa = null
                val viejo = colaDecodificada.poner(d)
                if (viejo != null) {
                    estadisticaModo.descartado()
                    librosDecodificados.offer(viejo)
                }
            } catch (e: Exception) {
                Log.w(ETIQUETA, "error decodificando un cuadro del carro: ${e.javaClass.simpleName}: ${e.message}")
            } finally {
                librosJpeg.offer(jpeg)
            }
        }
    }

    /** Etapa 3: reduce al tamaño de análisis y entra a la misma cadena que los cuadros del teléfono (detector, giro visual, modelo y fusión). */
    private fun cicloAnalisisVideo() {
        while (cadenaActiva) {
            val d = colaDecodificada.tomar(1000)
            estadisticaModo.revisar(SystemClock.elapsedRealtime())   // también sin cuadros: así se ve cuándo el video se detiene
            if (d == null) {
                continue
            }
            try {
                val w = d.ancho
                val h = d.alto
                if (w != tamanoSensor.width || h != tamanoSensor.height) {
                    tamanoSensor = Size(w, h)
                    tablasListas = false
                }
                ajustarTamanoSalida()
                val fuente = d.pixeles
                val ancho = anchoSalida
                val alto = altoSalida
                val pixeles = IntArray(ancho * alto)
                var posicion = 0
                var indice = 0
                for (y in 0 until alto) {
                    val fila = tablaY[y] * w
                    for (x in 0 until ancho) {
                        val p = fuente[fila + tablaX[x]]
                        val r = (p shr 16) and 0xFF
                        val g = (p shr 8) and 0xFF
                        val b = p and 0xFF
                        rgb[posicion] = r.toByte()
                        rgb[posicion + 1] = g.toByte()
                        rgb[posicion + 2] = b.toByte()
                        posicion += 3
                        pixeles[indice++] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
                    }
                }
                if (!abierta) {
                    abierta = true
                    inicioVentanaMs = SystemClock.elapsedRealtime()
                    Log.i(ETIQUETA, "video del carro entregando cuadros (${w}x${h})")
                }
                analizar(ancho, alto, pixeles, d.inicioNs, System.nanoTime(), d.llegadaMs)
                estadisticaModo.cuadro(d.llegadaMs, SystemClock.elapsedRealtime(), d.bytesJpeg)
            } catch (e: Exception) {
                Log.w(ETIQUETA, "error procesando un cuadro del carro: ${e.javaClass.simpleName}: ${e.message}")
            } finally {
                librosDecodificados.offer(d)
            }
        }
    }

    /** Libera la cámara. Se puede llamar varias veces. */
    fun cerrar() {
        val habiaCamara = hilo != null || enSesionVideo
        abierta = false
        activarModelo(false)
        if (enSesionVideo) {
            enSesionVideo = false
            SesionVideo.dejarDeEscuchar(alCambiarSesionVideo)
            SesionVideo.salir(usuarioVideo)
        }
        detenerCadena()
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
                try {
                    modelo = ModeloEnHilo(contexto).also { it.iniciar() }
                    configFusion = null
                    Log.i(ETIQUETA, "modelo de objetos encendido")
                } catch (e: Exception) {
                    Log.w(ETIQUETA, "No se pudo iniciar el modelo de objetos: ${e.message}")
                    error = "No se pudo iniciar el modelo: ${e.message}"
                }
            }
        } else {
            val m = modelo
            modelo = null
            if (m != null) {
                try { m.detener() } catch (e: Exception) { Log.w(ETIQUETA, "deteniendo modelo: ${e.message}") }
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
        if (tablasListas && ancho == anchoSalida && alto == altoSalida && tablaX.size == ancho && tablaY.size == alto && ancho * alto * 3 == rgb.size) {
            return
        }
        tablasListas = true
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
        MovimientoVisualNativa.reiniciar()   // otro tamaño: la huella anterior ya no sirve
        cuentaMarcas = 0
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

    /**
     * Campo de visión horizontal (grados por 10) a partir de la distancia focal y el ancho del sensor: 2 * atan(ancho / (2 * focal)).
     * Si la imagen que se pide tiene otra forma que el sensor (por ejemplo 4:3 contra 3:2) se recorta el ancho para esa forma.
     * Si el teléfono no informa esos datos, 60 grados. Con el Huawei ELS-NX9 (focal 4.4 mm, sensor 7.3 mm) sale 79 grados, no 60.
     */
    private fun calcularFovX10(caracteristicas: CameraCharacteristics, tamano: Size): Int {
        val focales = caracteristicas.get(CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS)
        val sensorMm = caracteristicas.get(CameraCharacteristics.SENSOR_INFO_PHYSICAL_SIZE)
        val activa = caracteristicas.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE)
        if (focales == null || focales.isEmpty() || sensorMm == null || activa == null || focales[0] <= 0f || sensorMm.width <= 0f || activa.width() <= 0 || activa.height() <= 0) {
            return MovimientoVisualNativa.FOV_DEFECTO_X10
        }
        var anchoMm = sensorMm.width.toDouble()
        // El tamaño pedido se recorta del sensor sin deformar: si es más "cuadrado" que el sensor se pierde ancho.
        val formaSensor = activa.width().toDouble() / activa.height()
        val formaImagen = tamano.width.toDouble() / tamano.height
        if (formaImagen < formaSensor) {
            anchoMm *= formaImagen / formaSensor
        }
        val grados = Math.toDegrees(2.0 * Math.atan(anchoMm / (2.0 * focales[0])))
        val x10 = Math.round(grados * 10).toInt()
        return if (x10 in 300..1400) x10 else MovimientoVisualNativa.FOV_DEFECTO_X10
    }

    private fun procesar(imagen: Image) {
        val llegadaMs = SystemClock.elapsedRealtime()
        val inicio = System.nanoTime()
        ajustarTamanoSalida()
        val ancho = anchoSalida
        val alto = altoSalida
        val pixeles = IntArray(ancho * alto)
        convertir(imagen, pixeles, ancho, alto)
        analizar(ancho, alto, pixeles, inicio, System.nanoTime(), llegadaMs)
    }

    /** Todo lo que se hace con un cuadro ya convertido a RGB (en `rgb`) y a ARGB (`pixeles`): detector, giro visual, modelo y fusión. */
    private fun analizar(ancho: Int, alto: Int, pixeles: IntArray, inicio: Long, tras_conversion: Long, llegadaMs: Long,
                         celdas: ByteArray? = null, columnasCeldas: Int = 0, filasCeldas: Int = 0) {
        val config = configActual()
        if (pedirAprender) {   // el aprendizaje empieza justo antes de este cuadro: así este cuadro ya se suma
            pedirAprender = false
            ObstaculosNativa.aprenderPisoIniciar()
            aprendiendoHasta = SystemClock.elapsedRealtime() + APRENDER_PISO_MS
        }
        val resultado = if (celdas != null) ObstaculosNativa.analizarCeldasConfig(celdas, columnasCeldas, filasCeldas, config, avanzando(), SystemClock.elapsedRealtime())
            else ObstaculosNativa.analizarConfig(rgb, ancho, alto, config, avanzando(), SystemClock.elapsedRealtime())
        if (aprendiendoHasta != 0L && SystemClock.elapsedRealtime() >= aprendiendoHasta) {
            terminarDeAprenderPiso(config)
        }
        // Registro PISO_DIAG: una línea por cuadro, solo números y el motivo.
        if (resultado[ObstaculosNativa.CODIGO] == 0) {
            Log.d("PISO_DIAG", "brillo=${resultado[ObstaculosNativa.BRILLO_MEDIO]} ganancia=${resultado[ObstaculosNativa.GANANCIA_MIL]} " +
                "grupos=${resultado[ObstaculosNativa.GRUPOS_PISO]} celdas_marcadas=${resultado[ObstaculosNativa.CELDAS_MARCADAS]} " +
                "zonas=${resultado[ObstaculosNativa.HAY_IZQUIERDA]}${resultado[ObstaculosNativa.HAY_CENTRO]}${resultado[ObstaculosNativa.HAY_DERECHA]} " +
                "motivo=${ObstaculosNativa.textoMotivo(resultado[ObstaculosNativa.MOTIVO])} piso_fijo=${config.getOrElse(ObstaculosNativa.CFG_PISO_FIJO) { 0 }}")
        } else {
            Log.d("PISO_DIAG", "error_detector=${resultado[ObstaculosNativa.CODIGO]}")
        }
        val fin = System.nanoTime()

        // Giro visual: cuánto giró el carro respecto al cuadro anterior (se compara la mitad de arriba).
        val ahoraCuadroMs = SystemClock.elapsedRealtime()
        // Con las celdas no hay imagen: no se puede medir el giro visual (el giro se hace por tiempo, con la calibración) ni correr el modelo de objetos.
        val mv = if (celdas == null) MovimientoVisualNativa.procesar(rgb, ancho, alto, fovEfectivoX10) else IntArray(32)
        val cuadrosAbarcados = mv[MovimientoVisualNativa.CUADROS].coerceIn(1, marcasCuadrosMs.size - 1)
        val dtMs = if (cuentaMarcas < cuadrosAbarcados) 0 else (ahoraCuadroMs - marcasCuadrosMs[(cuentaMarcas - cuadrosAbarcados) % marcasCuadrosMs.size]).toInt()
        marcasCuadrosMs[cuentaMarcas % marcasCuadrosMs.size] = ahoraCuadroMs
        cuentaMarcas++
        val giroVisual = GiroVisual(contadorCuadros + 1, if (mv[MovimientoVisualNativa.VALIDO] == 1) mv[MovimientoVisualNativa.GIRO_X10] else 0, dtMs,
            mv[MovimientoVisualNativa.CODIGO] == 0 && mv[MovimientoVisualNativa.VALIDO] == 1 && dtMs > 0,
            mv[MovimientoVisualNativa.MOTIVO], mv[MovimientoVisualNativa.CONFIANZA_MIL], mv[MovimientoVisualNativa.ENERGIA])
        ultimoGiro = giroVisual
        Log.d(ETIQUETA, "giro| valido=${mv[MovimientoVisualNativa.VALIDO]} motivo=${mv[MovimientoVisualNativa.MOTIVO]} corr_x16=${mv[MovimientoVisualNativa.CORRIMIENTO_X16]} giro_x10=${mv[MovimientoVisualNativa.GIRO_X10]} conf=${mv[MovimientoVisualNativa.CONFIANZA_MIL]} dt=$dtMs cuadros=$cuadrosAbarcados pred=${mv[MovimientoVisualNativa.POR_PREDICCION]}")
        if (giroVisual.confiable) {
            synchronized(candadoGiro) {
                giroPendienteX10 += giroVisual.giroX10   // se SUMAN todos: la pantalla lee más lento que la cámara y no puede perder ninguno
                dtPendienteMs += giroVisual.dtMs
            }
        }

        // Modelo de objetos: se le ofrece el cuadro (sin esperar) y se fusiona con lo último que dijo.
        var fusion: IntArray? = null
        var ultimoModelo: ResultadoModelo? = null
        var msFusion = 0.0
        var cualquiera: IntArray? = null
        val m = modelo
        if (celdas == null && m != null && m.activo && resultado[ObstaculosNativa.CODIGO] == 0) {
            val ahoraMs = SystemClock.elapsedRealtime()
            // Modo «cualquier objeto»: el modelo entrega las cajas desde 0.01 de confianza y el filtro (umbral, tamaño, posición, persistencia) se hace en C.
            val cfgCual = configCualquiera()
            val modoCualquiera = cfgCual.size >= CualquieraNativa.ENTEROS_CONFIG && cfgCual[CualquieraNativa.CFG_ACTIVO] == 1
            m.umbralCajas = if (modoCualquiera) CualquieraNativa.UMBRAL_MIN_MIL / 1000f else DetectorTflite.UMBRAL_CAJAS
            m.ofrecer(pixeles, ancho, alto, ahoraMs)
            ultimoModelo = m.ultimo
            if (configFusion == null && m.etiquetas.isNotEmpty()) {
                configFusion = FusionNativa.configConClases(m.etiquetas)
            }
            val inicioFusion = System.nanoTime()
            if (modoCualquiera) {
                val cajasModelo = ultimoModelo?.cajas ?: sinCajas
                val marcaModelo = ultimoModelo?.marcaMs ?: 0L
                val cual = CualquieraNativa.actualizar(cfgCual, cajasModelo, marcaModelo, ahoraMs)
                val barrido = CualquieraNativa.barrido(cfgCual, cajasModelo, marcaModelo, ahoraMs)
                fusion = FusionNativa.decidirCualquiera(configFusion ?: FusionNativa.configDefecto(), resultado, CualquieraNativa.zonasDe(cual),
                    cual[CualquieraNativa.R_CODIGO] == 0 && cual[CualquieraNativa.R_VIGENTE] == 1)
                cualquiera = cual
                registrarObjAny(resultado, fusion, ultimoModelo, barrido)
            } else {
                fusion = FusionNativa.decidir(configFusion ?: FusionNativa.configDefecto(), resultado, ultimoModelo?.cajas ?: sinCajas, ultimoModelo?.marcaMs ?: 0L, ahoraMs)
            }
            msFusion = (System.nanoTime() - inicioFusion) / 1.0e6
        } else if (m != null && !m.activo && m.error.isNotEmpty()) {
            error = m.error   // el modelo falló: la cámara y el detector de color siguen funcionando
        }

        // Diagnóstico (solo números): cada vez que cambian las celdas marcadas como obstáculo en alguna zona se escribe una línea.
        // Con el carro quieto y un objeto a distintas distancias se ve desde qué distancia lo empieza a ver el detector.
        if (resultado[ObstaculosNativa.CODIGO] == 0) {
            val ci = resultado[ObstaculosNativa.CELDAS_ZONA]
            val cc = resultado[ObstaculosNativa.CELDAS_ZONA + 1]
            val cd = resultado[ObstaculosNativa.CELDAS_ZONA + 2]
            val firma = ((ci * 131 + cc) * 131 + cd) * 16 + resultado[ObstaculosNativa.CERCANIA_CENTRO]
            if (firma != firmaCeldasLog) {
                firmaCeldasLog = firma
                Log.i(ETIQUETA, "celdas| IZQ(celdas=$ci,hay=${resultado[ObstaculosNativa.HAY_IZQUIERDA]},cerc=${resultado[ObstaculosNativa.CERCANIA_IZQUIERDA]}) " +
                    "CEN(celdas=$cc,hay=${resultado[ObstaculosNativa.HAY_CENTRO]},cerc=${resultado[ObstaculosNativa.CERCANIA_CENTRO]}) " +
                    "DER(celdas=$cd,hay=${resultado[ObstaculosNativa.HAY_DERECHA]},cerc=${resultado[ObstaculosNativa.CERCANIA_DERECHA]}) " +
                    "min_celdas=${config[ObstaculosNativa.CFG_MIN_CELDAS]} umbral=${config[ObstaculosNativa.CFG_UMBRAL_COLOR]}")
            }
        }

        // Corredor de trayectoria: qué zonas tienen el obstáculo FUERA de por donde pasa el carro (solo si está activado).
        var corredor: IntArray? = null
        val cfgCorredor = configCorredor()
        if (cfgCorredor.size > CorredorNativa.CFG_ACTIVO && cfgCorredor[CorredorNativa.CFG_ACTIVO] == 1 && resultado[ObstaculosNativa.CODIGO] == 0) {
            corredor = CorredorNativa.aplicar(cfgCorredor, resultado, ancho, alto)
            if (corredor[CorredorNativa.CODIGO] != 0) {
                Log.w(ETIQUETA, "corredor| error codigo=${corredor[CorredorNativa.CODIGO]}: se ignora el corredor en este cuadro")
                corredor = null
            }
        }

        // Flujo del piso (solo si está activado): se compara con un cuadro de ~200 ms antes y, si el color marcó algo lejano en un piso con textura consistente, se baja.
        var flujo: IntArray? = null
        val cfgFlujo = configFlujo()
        if (celdas == null && cfgFlujo.size >= FlujoNativa.ENTEROS_CONFIG && cfgFlujo[FlujoNativa.CFG_ACTIVO] == 1 && resultado[ObstaculosNativa.CODIGO] == 0) {
            flujo = analizarFlujo(cfgFlujo, ancho, alto, llegadaMs)
            if (flujo != null && flujo[FlujoNativa.R_EVALUADO] == 1) {
                val base = fusion ?: if (hayObstaculo(resultado)) FusionNativa.decidir(configFusion ?: FusionNativa.configDefecto(), resultado, sinCajas, 0L, SystemClock.elapsedRealtime()) else null
                if (base != null && base[FusionNativa.R_CODIGO] == 0) {
                    val bajada = FusionNativa.aplicarFlujo(base, FlujoNativa.zonasDe(flujo), true)
                    if (bajada[FusionNativa.R_CODIGO] == 0) {
                        fusion = bajada
                        if (bajada[FusionNativa.R_BAJADOS_FLUJO] > 0) {
                            Log.d("FLUJO_DIAG", "bajados_por_flujo=${bajada[FusionNativa.R_BAJADOS_FLUJO]} (el color marcó obstáculo lejano en piso con textura consistente)")
                        }
                    }
                }
            }
        } else {
            anilloCuenta = 0   // apagado: no se guarda nada
        }

        contadorCuadros++
        val cuadro = CuadroAnalizado(contadorCuadros, ancho, alto, pixeles, resultado, config,
            (tras_conversion - inicio) / 1.0e6, (fin - tras_conversion) / 1.0e6, fusion, ultimoModelo, msFusion, giroVisual,
            llegadaMs, (SystemClock.elapsedRealtime() - llegadaMs).toDouble(), corredor, cualquiera, flujo)
        ultimoCuadro = cuadro
        actualizarEstadistica(cuadro)
        alCuadro?.invoke(cuadro)
    }

    /**
     * Registro OBJ_ANY (una línea por cuadro, solo números): lo que decidió el color por zona (hay/cercanía), quién decidió la fusión, cuántas cajas desmintió y, para cada umbral
     * (0.01, 0.05, 0.10, 0.15, 0.20 y 0.30), cuántas cajas pasan los filtros (f), cuántas además persisten (p) y la zona (z, -1 = ninguna) y cercanía (c) de la mejor.
     */
    private fun registrarObjAny(detector: IntArray, fusion: IntArray, modelo: ResultadoModelo?, barrido: IntArray) {
        val sb = StringBuilder("OBJ_ANY ")
        sb.append("ms_modelo=").append(String.format(java.util.Locale.US, "%.1f", modelo?.msTotal ?: 0.0))
        sb.append(" color=")
        for (z in 0..2) {
            sb.append(detector[ObstaculosNativa.HAY_IZQUIERDA + 2 * z]).append('/').append(detector[ObstaculosNativa.CERCANIA_IZQUIERDA + 2 * z])
            if (z < 2) sb.append(',')
        }
        sb.append(" decidio=").append(FusionNativa.textoFuente(fusion[FusionNativa.R_FUENTE]))
        sb.append(" severidad=").append(fusion[FusionNativa.R_SEVERIDAD])
        sb.append(" desmentidos=").append(fusion[FusionNativa.R_DESMENTIDOS])
        for (u in 0 until CualquieraNativa.UMBRALES_BARRIDO) {
            val b = 5 * u
            sb.append(String.format(java.util.Locale.US, " u%.2f=f%d,p%d,z%d,c%d", barrido[b] / 1000.0, barrido[b + 1], barrido[b + 2], barrido[b + 3], barrido[b + 4]))
        }
        Log.d("OBJ_ANY", sb.toString())
    }

    // ── Flujo del piso: los últimos cuadros RGB con su hora, para compararlos con uno de ~200 ms antes ──
    private val anilloFlujo = Array(8) { ByteArray(0) }
    private val anilloAncho = IntArray(8)
    private val anilloAlto = IntArray(8)
    private val anilloMarcaMs = LongArray(8)
    private var anilloCuenta = 0

    private fun hayObstaculo(resultado: IntArray): Boolean =
        resultado[ObstaculosNativa.HAY_IZQUIERDA] == 1 || resultado[ObstaculosNativa.HAY_CENTRO] == 1 || resultado[ObstaculosNativa.HAY_DERECHA] == 1

    /**
     * Guarda este cuadro y lo compara con el más reciente que tenga al menos SEPARACION_OBJETIVO_MS de edad (el piso se mueve solo 1 o 2 píxeles en 100 ms: con ~200 ms se mide mejor).
     * Escribe el registro FLUJO_DIAG. Devuelve null si todavía no hay un cuadro de referencia.
     */
    private fun analizarFlujo(cfgFlujo: IntArray, ancho: Int, alto: Int, ahoraMs: Long): IntArray? {
        val inicio = System.nanoTime()
        var elegido = -1
        var dtMs = 0
        for (k in 1..minOf(anilloCuenta, anilloFlujo.size)) {   // del más nuevo al más viejo
            val i = (anilloCuenta - k).mod(anilloFlujo.size)
            if (anilloAncho[i] != ancho || anilloAlto[i] != alto) {
                break   // cambió el tamaño de los cuadros: lo anterior no sirve
            }
            val edad = (ahoraMs - anilloMarcaMs[i]).toInt()
            if (edad >= FlujoNativa.SEPARACION_OBJETIVO_MS) {
                elegido = i
                dtMs = edad
                break
            }
        }
        var r: IntArray? = null
        if (elegido >= 0) {
            r = FlujoNativa.analizar(cfgFlujo, configCorredor(), anilloFlujo[elegido], rgb, ancho, alto, avanzando(), dtMs)
            if (r[FlujoNativa.R_CODIGO] != 0) {
                Log.w("FLUJO_DIAG", "error codigo=${r[FlujoNativa.R_CODIGO]}")
                r = null
            } else {
                Log.d("FLUJO_DIAG", FlujoNativa.textoDiagnostico(r, dtMs, (System.nanoTime() - inicio) / 1.0e6))
            }
        }
        // Se guarda una copia de este cuadro (el arreglo `rgb` se reutiliza en el cuadro siguiente).
        val siguiente = anilloCuenta % anilloFlujo.size
        if (anilloFlujo[siguiente].size != rgb.size) {
            anilloFlujo[siguiente] = ByteArray(rgb.size)
        }
        System.arraycopy(rgb, 0, anilloFlujo[siguiente], 0, rgb.size)
        anilloAncho[siguiente] = ancho
        anilloAlto[siguiente] = alto
        anilloMarcaMs[siguiente] = ahoraMs
        anilloCuenta++
        return r
    }

    /** Cierra el aprendizaje del piso (en C) y guarda el informe. */
    private fun terminarDeAprenderPiso(config: IntArray) {
        aprendiendoHasta = 0L
        val r = ObstaculosNativa.aprenderPisoTerminar(config)
        aprendiendoPiso = false
        if (r[ObstaculosNativa.APR_CODIGO] != 0) {
            errorAprender = "No se pudo aprender el piso (código ${r[ObstaculosNativa.APR_CODIGO]}): llegaron muy pocos cuadros; revisa la cámara y vuelve a intentarlo."
            informePiso = null
            Log.w(ETIQUETA, "piso| $errorAprender")
            return
        }
        val grupos = r[ObstaculosNativa.APR_GRUPOS]
        val colores = (0 until grupos).map { g ->
            intArrayOf(r[ObstaculosNativa.APR_COLORES + 3 * g], r[ObstaculosNativa.APR_COLORES + 3 * g + 1], r[ObstaculosNativa.APR_COLORES + 3 * g + 2])
        }
        val pesos = (0 until grupos).map { g -> r[ObstaculosNativa.APR_PESOS_PCT + g] }
        val informe = InformePisoAprendido(grupos, r[ObstaculosNativa.APR_BRILLO_MEDIO], r[ObstaculosNativa.APR_MUY_OSCURO] == 1,
            r[ObstaculosNativa.APR_MUY_VARIADO] == 1, r[ObstaculosNativa.APR_DESCARTADO_PCT], r[ObstaculosNativa.APR_CUADROS], colores, pesos)
        informePiso = informe
        pisoAprendidoMs = SystemClock.elapsedRealtime()
        Log.i(ETIQUETA, "piso| aprendido: grupos=$grupos brillo_medio=${informe.brilloMedio} muy_oscuro=${informe.muyOscuro} " +
            "muy_variado=${informe.muyVariado} descartado_pct=${informe.descartadoPct} cuadros=${informe.cuadros}")
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
            // Depuración: números (nunca la imagen) de la mitad de arriba del cuadro y del giro visual.
            val sbBandas = StringBuilder()
            for (banda in 0 until 4) {
                var minimo = 255; var maximo = 0; var suma = 0L; var cuenta = 0
                val desde = (cuadro.alto * banda / 4) * cuadro.ancho
                val hasta = (cuadro.alto * (banda + 1) / 4) * cuadro.ancho
                for (p in desde until hasta) { val v = rgb[p * 3 + 1].toInt() and 0xFF; suma += v; cuenta++; if (v < minimo) minimo = v; if (v > maximo) maximo = v }
                sbBandas.append(" banda$banda(verde medio=${suma / Math.max(1, cuenta)} min=$minimo max=$maximo)")
            }
            Log.i(ETIQUETA, "cuadro por bandas de arriba a abajo:$sbBandas | giro: valido=${cuadro.giro?.confiable} motivo=${cuadro.giro?.motivo} energia=${cuadro.giro?.energia}")
            // Diagnóstico de falsos obstáculos (solo números): lo que decidió el detector, qué piso aprendió y cómo cambia el brillo de arriba a
            // abajo en la parte central de la imagen (8 bandas): sirve para ver si la franja de referencia del piso está tomando algo oscuro.
            val res = cuadro.resultado
            val sbPerfil = StringBuilder()
            val x0 = cuadro.ancho * 3 / 10
            val x1 = cuadro.ancho * 7 / 10
            for (b in 0 until 8) {
                var suma = 0L
                var cuenta = 0
                for (y in (cuadro.alto * b / 8) until (cuadro.alto * (b + 1) / 8)) {
                    for (x in x0 until x1) {
                        suma += rgb[(y * cuadro.ancho + x) * 3 + 1].toInt() and 0xFF
                        cuenta++
                    }
                }
                sbPerfil.append(suma / Math.max(1, cuenta)).append(if (b < 7) "," else "")
            }
            Log.i(ETIQUETA, "deteccion| IZQ(hay=${res[ObstaculosNativa.HAY_IZQUIERDA]},cerc=${res[ObstaculosNativa.CERCANIA_IZQUIERDA]},celdas=${res[ObstaculosNativa.CELDAS_ZONA]}) " +
                "CEN(hay=${res[ObstaculosNativa.HAY_CENTRO]},cerc=${res[ObstaculosNativa.CERCANIA_CENTRO]},celdas=${res[ObstaculosNativa.CELDAS_ZONA + 1]}) " +
                "DER(hay=${res[ObstaculosNativa.HAY_DERECHA]},cerc=${res[ObstaculosNativa.CERCANIA_DERECHA]},celdas=${res[ObstaculosNativa.CELDAS_ZONA + 2]}) | " +
                "piso=${res[ObstaculosNativa.REF_R]},${res[ObstaculosNativa.REF_G]},${res[ObstaculosNativa.REF_B]} grupos=${res[ObstaculosNativa.GRUPOS_PISO]} " +
                "franja(col=${res[ObstaculosNativa.REF_COLUMNA]},fila=${res[ObstaculosNativa.REF_FILA]},${res[ObstaculosNativa.REF_NCOLUMNAS]}x${res[ObstaculosNativa.REF_NFILAS]}) " +
                "tapada=${res[ObstaculosNativa.FRANJA_TAPADA]} reaprendido=${res[ObstaculosNativa.PISO_REAPRENDIDO]} aprendio=${res[ObstaculosNativa.APRENDIO]} " +
                "fila_min=${res[ObstaculosNativa.FILA_MINIMA_USADA]} filas=${res[ObstaculosNativa.FILAS_USADAS]} cols=${res[ObstaculosNativa.COLUMNAS_USADAS]} | " +
                "perfil_centro_verde_8bandas_arriba_a_abajo=$sbPerfil")
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
     * Devuelve lo que la cámara vio girar desde la última vez que se llamó: la SUMA de los giros confiables de todos los cuadros
     * que llegaron y el tiempo que cubren (un cuadro sin confianza no suma nada: ese pedazo lo completa el navegador por tiempo), o
     * null si no llegó nada. Lo usa el modo semiautónomo para corregir el giro por tiempo. Se puede llamar desde cualquier hilo.
     */
    fun tomarGiroNuevo(): GiroVisual? {
        synchronized(candadoGiro) {
            if (dtPendienteMs <= 0) {
                return null
            }
            val giro = GiroVisual(contadorCuadros, giroPendienteX10, dtPendienteMs, true, 0, 1000)
            giroPendienteX10 = 0
            dtPendienteMs = 0
            return giro
        }
    }

    /** Descarta lo que se haya acumulado del giro (por ejemplo, antes de empezar una prueba). */
    fun olvidarGiroPendiente() {
        synchronized(candadoGiro) {
            giroPendienteX10 = 0
            dtPendienteMs = 0
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
        cuadroTomado = cuadro
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
