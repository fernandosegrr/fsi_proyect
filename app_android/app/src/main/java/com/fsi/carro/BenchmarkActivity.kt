/*
 * BenchmarkActivity.kt — Cuánto tarda el detector de objetos en este teléfono.
 *
 * GUÍA DE LECTURA
 *  1. Se abre con el botón de la pantalla principal o con adb:
 *       adb shell am start -n com.fsi.carro/.BenchmarkActivity --ez bench_auto true
 *     (con bench_auto corre solo al abrirse).
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
        const val ARCHIVO_RESULTADOS = "bench_resultados.txt"
        const val CALENTAMIENTO = 20   // pasadas que no cuentan
        const val MEDICIONES = 200     // pasadas que sí cuentan
        const val CALENTAMIENTO_NNAPI = 5   // NNAPI solo se prueba para anotar si funciona
        const val MEDICIONES_NNAPI = 20
    }

    private class Entrada(val nombre: String, val ancho: Int, val alto: Int, val jpeg: ByteArray)

    private lateinit var boton: Button
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
        estado = findViewById(R.id.estado)
        registro = findViewById(R.id.registro)
        desplazamiento = findViewById(R.id.desplazamiento)

        boton.setOnClickListener { iniciar() }
        if (intent.getBooleanExtra(EXTRA_AUTO, false)) {
            iniciar()
        }
    }

    /** Arranca el benchmark en un hilo aparte (si no hay uno corriendo). */
    private fun iniciar() {
        if (corriendo) {
            return
        }
        corriendo = true
        boton.isEnabled = false
        registro.text = ""
        Thread { ejecutar() }.start()
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
            runOnUiThread { boton.isEnabled = true }
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
