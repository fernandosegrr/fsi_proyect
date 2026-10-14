/*
 * BenchmarkActivity.kt — Cuánto tarda el detector de objetos en este teléfono.
 *
 * GUÍA DE LECTURA
 *  1. Se abre con el botón de la pantalla principal o con adb:
 *       adb shell am start -n com.fsi.carro/.BenchmarkActivity --ez bench_auto true
 *     (con bench_auto corre solo al abrirse). Con --es bench_modo obstaculos mide el
 *     detector de obstáculos por color de piso (BenchmarkObstaculos) en vez del modelo.
 *  2. ejecutar() es lo principal: para cada configuración (CPU 1/2/4 hilos, GPU, NNAPI)
 *     abre el detector y mide con dos imágenes JPEG (640x480 y 320x240).
 *  3. medirEntrada() repite: decodificar → preparar → inferir → postprocesar, cada una
 *     con su cronómetro. Las primeras pasadas son de calentamiento y no cuentan.
 *  4. anotar() escribe cada línea en logcat (etiqueta BENCH), en pantalla y en el archivo
 *     bench_resultados.txt de la carpeta de archivos de la app.
 */
package com.fsi.carro

import android.app.Activity
import android.graphics.BitmapFactory
import android.os.Bundle
import android.util.Log
import android.view.WindowManager
import android.widget.Button
import android.widget.ScrollView
import android.widget.TextView
import java.io.File
import java.util.Locale

class BenchmarkActivity : Activity() {

    companion object {
        const val ETIQUETA = "BENCH"
        const val EXTRA_AUTO = "bench_auto"
        const val EXTRA_MODO = "bench_modo"            // "detector" (por defecto) u "obstaculos"
        const val MODO_DETECTOR = "detector"
        const val MODO_OBSTACULOS = "obstaculos"
        const val MODO_GENERICO = "generico"         // mide cualquier .tflite o .onnx de la carpeta modelos/ solo por tiempo (BenchmarkGenerico, registro DET_MS)
        const val EXTRA_ACELERADOR = "bench_acelerador"   // con modo generico y un .tflite: además prueba NNAPI fijado a ese acelerador (nombre de ANeuralNetworks)
        const val MODO_TERMICO = "termico"           // 60 s seguidos por configuración: ms al principio y al final, y temperatura (con bench_modelo = nombre del .tflite)
        const val EXTRA_MODELO = "bench_modelo"      // el .tflite a medir (en la carpeta modelos/ de los archivos de la app, o en assets); por defecto EfficientDet-Lite0
        const val SEGUNDOS_TERMICO = 60
        const val ARCHIVO_OBSTACULOS = "bench_obstaculos.txt"
        const val ARCHIVO_RESULTADOS = "bench_resultados.txt"
        const val CALENTAMIENTO = 20   // pasadas que no cuentan
        const val MEDICIONES = 200     // pasadas que sí cuentan
        const val CALENTAMIENTO_NNAPI = 5   // NNAPI solo se prueba para anotar si funciona
        const val MEDICIONES_NNAPI = 20
    }

    private class Entrada(val nombre: String, val ancho: Int, val alto: Int, val jpeg: ByteArray)

    private lateinit var boton: Button
    private lateinit var botonObstaculos: Button
    private lateinit var estado: TextView
    private lateinit var registro: TextView
    private lateinit var desplazamiento: ScrollView
    private var archivo: File? = null
    private var corriendo = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_benchmark)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        boton = findViewById(R.id.boton_iniciar)
        botonObstaculos = findViewById(R.id.boton_iniciar_obstaculos)
        estado = findViewById(R.id.estado)
        registro = findViewById(R.id.registro)
        desplazamiento = findViewById(R.id.desplazamiento)

        boton.setOnClickListener { iniciar(MODO_DETECTOR) }
        botonObstaculos.setOnClickListener { iniciar(MODO_OBSTACULOS) }
        if (intent.getBooleanExtra(EXTRA_AUTO, false)) {
            iniciar(intent.getStringExtra(EXTRA_MODO) ?: MODO_DETECTOR)
        }
    }

    /** Arranca el benchmark del modo dado en un hilo aparte (si no hay uno corriendo). */
    private fun iniciar(modo: String) {
        if (corriendo) {
            return
        }
        corriendo = true
        boton.isEnabled = false
        botonObstaculos.isEnabled = false
        registro.text = ""
        val modelo = intent.getStringExtra(EXTRA_MODELO) ?: DetectorTflite.ARCHIVO_MODELO
        Thread { when (modo) { MODO_OBSTACULOS -> ejecutarObstaculos(); MODO_TERMICO -> ejecutarTermico(modelo); MODO_GENERICO -> ejecutarGenerico(modelo); else -> ejecutar() } }.start()
    }

    // ── Salida ────────────────────────────────────────────────────────────

    /** Escribe una línea en logcat, en pantalla y en el archivo de resultados. */
    private fun anotar(linea: String) {
        Log.i(ETIQUETA, linea)
        archivo?.appendText(linea + "\n")
        runOnUiThread {
            registro.append(linea + "\n")
            desplazamiento.post { desplazamiento.fullScroll(ScrollView.FOCUS_DOWN) }
        }
    }

    private fun mostrarEstado(texto: String) {
        runOnUiThread { estado.text = texto }
    }

    private fun f(valor: Double): String = String.format(Locale.US, "%.2f", valor)

    // ── Benchmark ─────────────────────────────────────────────────────────

    private fun ejecutar() {
        try {
            archivo = File(getExternalFilesDir(null), ARCHIVO_RESULTADOS).also { it.writeText("") }
            anotar("BENCH|INICIO|modelo_archivo=${DetectorTflite.ARCHIVO_MODELO}|calentamiento=$CALENTAMIENTO|mediciones=$MEDICIONES|jpeg_calidad=${ImagenesPrueba.CALIDAD_JPEG}")
            for (linea in InfoDispositivo.texto(this)) {
                anotar("BENCH|dispositivo|$linea")
            }

            val entradas = prepararEntradas()
            val detector = DetectorTflite(this)
            anotar("BENCH|modelo|etiquetas=${detector.etiquetas.size}")

            val configuraciones = listOf(
                Configuracion("CPU-1-hilo", Delegado.CPU, 1),
                Configuracion("CPU-2-hilos", Delegado.CPU, 2),
                Configuracion("CPU-4-hilos", Delegado.CPU, 4),
                Configuracion("GPU", Delegado.GPU, 0),
                Configuracion("NNAPI", Delegado.NNAPI, 0)
            )
            for (configuracion in configuraciones) {
                mostrarEstado("Midiendo ${configuracion.nombre}...")
                try {
                    medirConfiguracion(detector, configuracion, entradas)
                } catch (e: Throwable) {
                    anotar("BENCH|FALLO|config=${configuracion.nombre}|error=${e.javaClass.simpleName}: ${e.message?.replace('\n', ' ')}")
                } finally {
                    detector.cerrar()
                }
            }
            anotar("BENCH|FIN")
            mostrarEstado("Terminado. Resultados en $ARCHIVO_RESULTADOS")
        } catch (e: Throwable) {
            anotar("BENCH|ERROR_GENERAL|${e.javaClass.simpleName}: ${e.message?.replace('\n', ' ')}")
            mostrarEstado("Error: ${e.message}")
        } finally {
            corriendo = false
            runOnUiThread { boton.isEnabled = true; botonObstaculos.isEnabled = true }
        }
    }

    /**
     * Prueba de 60 s seguidos por configuración (CPU 1 hilo, GPU y NNAPI) con UN modelo: ms de inferencia al principio (primeros 10 s) y al final (últimos 10 s), la media de
     * los 60 s y la temperatura de la batería al empezar y al terminar (es lo que el teléfono deja leer; ver InfoDispositivo.estadoTermico). Solo números.
     */
    private fun ejecutarTermico(archivoModelo: String) {
        try {
            val nombreArchivo = "bench_termico_" + archivoModelo.removeSuffix(".tflite") + ".txt"
            archivo = File(getExternalFilesDir(null), nombreArchivo).also { it.writeText("") }
            anotar("BENCH|TERMICO|INICIO|modelo=$archivoModelo|segundos=$SEGUNDOS_TERMICO")
            for (linea in InfoDispositivo.texto(this)) {
                anotar("BENCH|dispositivo|$linea")
            }
            val entradas = prepararEntradas()
            val entrada = entradas.first { it.nombre == "320x240" }
            val configuraciones = listOf(Configuracion("CPU-1-hilo", Delegado.CPU, 1), Configuracion("GPU", Delegado.GPU, 0), Configuracion("NNAPI", Delegado.NNAPI, 0))
            for (configuracion in configuraciones) {
                mostrarEstado("Midiendo $archivoModelo con ${configuracion.nombre} durante $SEGUNDOS_TERMICO s...")
                val detector = DetectorTflite(this, archivoModelo)
                try {
                    val aperturaMs = detector.abrir(configuracion)
                    val imagen = BitmapFactory.decodeByteArray(entrada.jpeg, 0, entrada.jpeg.size)
                    val antes = InfoDispositivo.estadoTermico(this)
                    val tIni = System.nanoTime()
                    val tiempos = ArrayList<Double>()
                    val marcas = ArrayList<Double>()
                    var rojo = 0
                    while ((System.nanoTime() - tIni) / 1.0e9 < SEGUNDOS_TERMICO) {
                        val t0 = System.nanoTime()
                        detector.preparar(imagen)
                        detector.inferir()
                        detector.postprocesar()
                        val t1 = System.nanoTime()
                        tiempos.add((t1 - t0) / 1.0e6)
                        marcas.add((t1 - tIni) / 1.0e9)
                        rojo++
                    }
                    imagen.recycle()
                    val despues = InfoDispositivo.estadoTermico(this)
                    val primeros = tiempos.filterIndexed { i, _ -> marcas[i] <= 10.0 }
                    val ultimos = tiempos.filterIndexed { i, _ -> marcas[i] >= SEGUNDOS_TERMICO - 10.0 }
                    val media = { l: List<Double> -> if (l.isEmpty()) 0.0 else l.sum() / l.size }
                    val mediana = { l: List<Double> -> if (l.isEmpty()) 0.0 else l.sorted()[l.size / 2] }
                    anotar("BENCH|TERMICO|modelo=$archivoModelo|config=${configuracion.nombre}|apertura_ms=${f(aperturaMs)}|inferencias=$rojo|ms_primeros_10s=${f(media(primeros))}|" +
                        "ms_ultimos_10s=${f(media(ultimos))}|ms_media_60s=${f(media(tiempos))}|ms_mediana_60s=${f(mediana(tiempos))}|inicio=[$antes]|fin=[$despues]")
                } catch (e: Throwable) {
                    anotar("BENCH|TERMICO|FALLO|modelo=$archivoModelo|config=${configuracion.nombre}|error=${e.javaClass.simpleName}: ${e.message?.replace('\n', ' ')}")
                } finally {
                    detector.cerrar()
                }
            }
            anotar("BENCH|TERMICO|FIN|modelo=$archivoModelo")
            mostrarEstado("Terminado. Resultados en $nombreArchivo")
        } catch (e: Throwable) {
            anotar("BENCH|ERROR_GENERAL|${e.javaClass.simpleName}: ${e.message?.replace('\n', ' ')}")
            mostrarEstado("Error: ${e.message}")
        } finally {
            corriendo = false
            runOnUiThread { boton.isEnabled = true; botonObstaculos.isEnabled = true }
        }
    }

    /** Comparativa de detectores: mide el modelo dado (.tflite u .onnx de la carpeta modelos/) con BenchmarkGenerico. */
    private fun ejecutarGenerico(archivoModelo: String) {
        try {
            archivo = File(getExternalFilesDir(null), "det_ms_" + archivoModelo.substringBeforeLast('.') + ".txt").also { it.writeText("") }
            anotar("DET_MS|INICIO|modelo=$archivoModelo")
            mostrarEstado("Midiendo $archivoModelo (60 s por configuración)...")
            BenchmarkGenerico(this, { anotar(it) }).medir(archivoModelo, intent.getStringExtra(EXTRA_ACELERADOR))
            anotar("DET_MS|FIN|modelo=$archivoModelo")
            mostrarEstado("Terminado.")
        } catch (e: Throwable) {
            anotar("DET_MS|ERROR_GENERAL|${e.javaClass.simpleName}: ${e.message?.replace('\n', ' ')}")
        } finally {
            corriendo = false
            runOnUiThread { boton.isEnabled = true; botonObstaculos.isEnabled = true }
        }
    }

    /** Benchmark del detector de obstáculos por color de piso (en C). */
    private fun ejecutarObstaculos() {
        try {
            archivo = File(getExternalFilesDir(null), ARCHIVO_OBSTACULOS).also { it.writeText("") }
            anotar("BENCH|INICIO|modo=obstaculos")
            for (linea in InfoDispositivo.texto(this)) {
                anotar("BENCH|dispositivo|$linea")
            }
            mostrarEstado("Midiendo obstáculos...")
            BenchmarkObstaculos(this) { anotar(it) }.ejecutar()
            anotar("BENCH|FIN")
            mostrarEstado("Terminado. Resultados en $ARCHIVO_OBSTACULOS")
        } catch (e: Throwable) {
            anotar("BENCH|ERROR_GENERAL|${e.javaClass.simpleName}: ${e.message?.replace('\n', ' ')}")
            mostrarEstado("Error: ${e.message}")
        } finally {
            corriendo = false
            runOnUiThread { boton.isEnabled = true; botonObstaculos.isEnabled = true }
        }
    }

    /** Genera los dos JPEG de prueba a partir de la foto (o de la imagen sintética). */
    private fun prepararEntradas(): List<Entrada> {
        val fuente = ImagenesPrueba.cargarFuente(this)
        anotar("BENCH|imagen|origen=${if (fuente != null) "foto copiada con adb (${ImagenesPrueba.NOMBRE_FUENTE})" else "sintetica (no se encontro la foto)"}")
        val base = fuente ?: ImagenesPrueba.sintetica()
        return listOf(
            Entrada("640x480", 640, 480, ImagenesPrueba.generarJpeg(base, 640, 480)),
            Entrada("320x240", 320, 240, ImagenesPrueba.generarJpeg(base, 320, 240))
        )
    }

    private fun medirConfiguracion(detector: DetectorTflite, configuracion: Configuracion, entradas: List<Entrada>) {
        val aperturaMs = detector.abrir(configuracion)
        anotar("BENCH|config=${configuracion.nombre}|apertura_ms=${f(aperturaMs)}|tensores=${detector.descripcionTensores}")
        anotar("BENCH|config=${configuracion.nombre}|inicio|${InfoDispositivo.estadoTermico(this)}")
        for (entrada in entradas) {
            medirEntrada(detector, configuracion, entrada)
        }
        anotar("BENCH|config=${configuracion.nombre}|fin|${InfoDispositivo.estadoTermico(this)}")
    }

    /** Calentamiento y mediciones de una configuración con una de las dos imágenes. */
    private fun medirEntrada(detector: DetectorTflite, configuracion: Configuracion, entrada: Entrada) {
        val soloAnotar = configuracion.delegado == Delegado.NNAPI
        val calentamiento = if (soloAnotar) CALENTAMIENTO_NNAPI else CALENTAMIENTO
        val mediciones = if (soloAnotar) MEDICIONES_NNAPI else MEDICIONES

        val decodificar = DoubleArray(mediciones)
        val preparar = DoubleArray(mediciones)
        val inferencia = DoubleArray(mediciones)
        val postproceso = DoubleArray(mediciones)
        val total = DoubleArray(mediciones)
        var ultimas: List<Deteccion> = emptyList()

        for (pasada in 0 until calentamiento + mediciones) {
            val t0 = System.nanoTime()
            val imagen = BitmapFactory.decodeByteArray(entrada.jpeg, 0, entrada.jpeg.size)
                ?: throw IllegalStateException("No se pudo decodificar el JPEG")
            val t1 = System.nanoTime()
            detector.preparar(imagen)
            val t2 = System.nanoTime()
            detector.inferir()
            val t3 = System.nanoTime()
            ultimas = detector.postprocesar()
            val t4 = System.nanoTime()
            imagen.recycle()

            val indice = pasada - calentamiento
            if (indice >= 0) {
                decodificar[indice] = (t1 - t0) / 1.0e6
                preparar[indice] = (t2 - t1) / 1.0e6
                inferencia[indice] = (t3 - t2) / 1.0e6
                postproceso[indice] = (t4 - t3) / 1.0e6
                total[indice] = (t4 - t0) / 1.0e6
            }
        }

        val base = "BENCH|config=${configuracion.nombre}|entrada=${entrada.nombre}|jpeg_bytes=${entrada.jpeg.size}|n=$mediciones"
        anotar(resumenEtapa(base, "decodificar", decodificar))
        anotar(resumenEtapa(base, "preparar", preparar))
        anotar(resumenEtapa(base, "inferencia", inferencia))
        anotar(resumenEtapa(base, "postproceso", postproceso))
        val resumenTotal = resumir(total)
        anotar(resumenEtapa(base, "total", total) + "|fps_posibles=${f(1000.0 / resumenTotal.media)}")

        anotar("BENCH|config=${configuracion.nombre}|entrada=${entrada.nombre}|detecciones=${ultimas.size}|umbral=${DetectorTflite.UMBRAL}")
        for (d in ultimas) {
            anotar("BENCH|deteccion|config=${configuracion.nombre}|entrada=${entrada.nombre}|clase=${d.clase}|confianza=${f(d.confianza.toDouble())}")
        }
    }

    private fun resumenEtapa(base: String, etapa: String, muestras: DoubleArray): String {
        val r = resumir(muestras)
        return "$base|etapa=$etapa|media_ms=${f(r.media)}|mediana_ms=${f(r.mediana)}|p95_ms=${f(r.p95)}|min_ms=${f(r.minimo)}|max_ms=${f(r.maximo)}"
    }
}
