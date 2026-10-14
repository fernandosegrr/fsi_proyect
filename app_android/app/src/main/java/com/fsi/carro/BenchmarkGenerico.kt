/*
 * BenchmarkGenerico.kt — Mide CUALQUIER modelo de detección (.tflite o .onnx) solo por tiempo, para la comparativa de detectores (docs/comparativa_detectores.md).
 *
 * GUÍA DE LECTURA
 *  1. Se lanza desde BenchmarkActivity (modo «generico»): adb shell am start -n com.fsi.carro/.BenchmarkActivity --ez bench_auto true --es bench_modo generico --es bench_modelo yolo11n_320.onnx
 *     El modelo se busca en la carpeta modelos/ de los archivos de la app (adb push al teléfono; no va en el repositorio).
 *  2. NO decodifica las salidas del modelo ni dibuja nada: solo le da una entrada fija (un patrón repetible, NO una foto) y cronometra la inferencia. Sirve para comparar ms,
 *     memoria y temperatura entre modelos; la calidad de la detección se compara aparte, con las pasadas del protocolo «cualquier objeto».
 *  3. TFLite: CPU 1 hilo, CPU 4 hilos, GPU y NNAPI (con o sin nombre de acelerador). ONNX Runtime: CPU 1 hilo, CPU 4 hilos y NNAPI.
 *  4. Cada configuración corre 60 s seguidos y escribe UNA línea DET_MS (en logcat y en el archivo de resultados): modelo, tamaño del archivo, entrada, apertura, inferencias, ms de los
 *     primeros 10 s, de los últimos 10 s, media, mediana y p95, memoria (PSS en MB antes y después) y estado térmico y temperatura de la batería al empezar y al terminar.
 *  5. Si una configuración falla (el delegado no soporta alguna operación del modelo), la línea dice FALLO y el motivo: eso también es un resultado.
 */
package com.fsi.carro

import android.content.Context
import android.os.Debug
import android.util.Log
import java.io.File
import java.io.FileInputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.channels.FileChannel
import java.util.Locale
import org.tensorflow.lite.DataType
import org.tensorflow.lite.Interpreter
import org.tensorflow.lite.gpu.GpuDelegate
import org.tensorflow.lite.nnapi.NnApiDelegate
import ai.onnxruntime.OnnxTensor
import ai.onnxruntime.OrtEnvironment
import ai.onnxruntime.OrtSession
import java.nio.FloatBuffer

class BenchmarkGenerico(private val contexto: Context, private val anotar: (String) -> Unit, private val segundos: Int = 60) {

    companion object {
        const val ETIQUETA = "DET_MS"
    }

    private fun f(v: Double) = String.format(Locale.US, "%.2f", v)

    private fun memoriaMb(): Double {
        val info = Debug.MemoryInfo()
        Debug.getMemoryInfo(info)
        return info.totalPss / 1024.0
    }

    /** Mide el modelo `nombre` (carpeta modelos/ de los archivos de la app) con todas las configuraciones que corresponden a su formato. */
    fun medir(nombre: String, acelerador: String? = null) {
        val archivo = File(contexto.getExternalFilesDir(null), "modelos/$nombre")
        if (!archivo.exists()) {
            anotar("$ETIQUETA|modelo=$nombre|FALLO|error=no existe $archivo (adb push al teléfono)")
            return
        }
        for (linea in InfoDispositivo.texto(contexto)) {
            anotar("$ETIQUETA|dispositivo|$linea")
        }
        if (nombre.endsWith(".onnx")) {
            medirOnnx(archivo)
        } else {
            medirTflite(archivo, acelerador)
        }
    }

    private class Estadistica {
        val tiempos = ArrayList<Double>()
        val marcas = ArrayList<Double>()
        fun linea(segundos: Int): String {
            val primeros = tiempos.filterIndexed { i, _ -> marcas[i] <= 10.0 }
            val ultimos = tiempos.filterIndexed { i, _ -> marcas[i] >= segundos - 10.0 }
            val media = { l: List<Double> -> if (l.isEmpty()) 0.0 else l.sum() / l.size }
            val ordenados = tiempos.sorted()
            val mediana = if (ordenados.isEmpty()) 0.0 else ordenados[ordenados.size / 2]
            val p95 = if (ordenados.isEmpty()) 0.0 else ordenados[(ordenados.size * 95 / 100).coerceAtMost(ordenados.size - 1)]
            return String.format(Locale.US, "inferencias=%d|ms_primeros_10s=%.2f|ms_ultimos_10s=%.2f|ms_media=%.2f|ms_mediana=%.2f|ms_p95=%.2f", tiempos.size, media(primeros), media(ultimos),
                media(tiempos), mediana, p95)
        }
    }

    // ── TensorFlow Lite ───────────────────────────────────────────────────

    private fun medirTflite(archivo: File, acelerador: String?) {
        val nombre = archivo.name
        val configs = ArrayList<Triple<String, Int, String>>()   // (nombre, hilos, delegado)
        configs.add(Triple("CPU-1-hilo", 1, "cpu"))
        configs.add(Triple("CPU-4-hilos", 4, "cpu"))
        configs.add(Triple("GPU", 0, "gpu"))
        configs.add(Triple("NNAPI", 0, "nnapi"))
        for (a in (acelerador ?: "").split(',').filter { it.isNotBlank() }) {   // nombres de dispositivos NNAPI separados por coma (ver scripts/nnapi_dispositivos.c)
            configs.add(Triple("NNAPI-$a", 0, "nnapi:$a"))
        }
        for ((nombreConfig, hilos, delegado) in configs) {
            var interprete: Interpreter? = null
            var delegadoObj: AutoCloseable? = null
            try {
                val memAntes = memoriaMb()
                val t0 = System.nanoTime()
                val opciones = Interpreter.Options()
                if (hilos > 0) {
                    opciones.setNumThreads(hilos)
                }
                if (delegado == "gpu") {
                    val d = GpuDelegate()
                    delegadoObj = AutoCloseable { d.close() }
                    opciones.addDelegate(d)
                } else if (delegado.startsWith("nnapi")) {
                    val o = NnApiDelegate.Options()
                    if (delegado.contains(":")) {
                        o.setAcceleratorName(delegado.substringAfter(":"))
                    }
                    val d = NnApiDelegate(o)
                    delegadoObj = AutoCloseable { d.close() }
                    opciones.addDelegate(d)
                }
                val mapa = FileInputStream(archivo).channel.use { it.map(FileChannel.MapMode.READ_ONLY, 0, archivo.length()) }
                interprete = Interpreter(mapa, opciones)
                val entradas = Array<Any>(interprete.inputTensorCount) { i -> entradaFija(interprete.getInputTensor(i).dataType(), interprete.getInputTensor(i).numBytes()) }
                val salidas = HashMap<Int, Any>()
                for (i in 0 until interprete.outputTensorCount) {
                    salidas[i] = ByteBuffer.allocateDirect(interprete.getOutputTensor(i).numBytes()).order(ByteOrder.nativeOrder())
                }
                val forma = interprete.getInputTensor(0).shape().joinToString("x") + ":" + interprete.getInputTensor(0).dataType()
                val aperturaMs = (System.nanoTime() - t0) / 1.0e6
                val estad = Estadistica()
                val antes = InfoDispositivo.estadoTermico(contexto)
                val tIni = System.nanoTime()
                while ((System.nanoTime() - tIni) / 1.0e9 < segundos) {
                    val a = System.nanoTime()
                    for (b in salidas.values) {
                        (b as ByteBuffer).rewind()
                    }
                    interprete.runForMultipleInputsOutputs(entradas, salidas)
                    val b = System.nanoTime()
                    estad.tiempos.add((b - a) / 1.0e6)
                    estad.marcas.add((b - tIni) / 1.0e9)
                }
                val despues = InfoDispositivo.estadoTermico(contexto)
                anotar("$ETIQUETA|modelo=$nombre|formato=tflite|bytes=${archivo.length()}|entrada=$forma|config=$nombreConfig|apertura_ms=${f(aperturaMs)}|${estad.linea(segundos)}|" +
                    "memoria_mb_antes=${f(memAntes)}|memoria_mb_despues=${f(memoriaMb())}|inicio=[$antes]|fin=[$despues]")
            } catch (e: Throwable) {
                anotar("$ETIQUETA|modelo=$nombre|formato=tflite|bytes=${archivo.length()}|config=$nombreConfig|FALLO|error=${e.javaClass.simpleName}: ${e.message?.replace('\n', ' ')?.take(200)}")
            } finally {
                try { interprete?.close() } catch (e: Throwable) { /* nada */ }
                try { delegadoObj?.close() } catch (e: Throwable) { /* nada */ }
            }
        }
    }

    /** Una entrada fija y repetible (un patrón de rampas, NO una foto): bytes para uint8 y flotantes de 0 a 1 para float32. */
    private fun entradaFija(tipo: DataType, bytes: Int): ByteBuffer {
        val b = ByteBuffer.allocateDirect(bytes).order(ByteOrder.nativeOrder())
        if (tipo == DataType.FLOAT32) {
            for (i in 0 until bytes / 4) {
                b.putFloat(((i * 7) % 256) / 255f)
            }
        } else {
            for (i in 0 until bytes) {
                b.put(((i * 7) % 256).toByte())
            }
        }
        b.rewind()
        return b
    }

    // ── ONNX Runtime ──────────────────────────────────────────────────────

    private fun medirOnnx(archivo: File) {
        val nombre = archivo.name
        val entorno = OrtEnvironment.getEnvironment()
        for ((nombreConfig, hilos, nnapi) in listOf(Triple("CPU-1-hilo", 1, false), Triple("CPU-4-hilos", 4, false), Triple("NNAPI", 0, true))) {
            var sesion: OrtSession? = null
            try {
                val memAntes = memoriaMb()
                val t0 = System.nanoTime()
                val opciones = OrtSession.SessionOptions()
                if (hilos > 0) {
                    opciones.setIntraOpNumThreads(hilos)
                }
                if (nnapi) {
                    opciones.addNnapi()
                }
                sesion = entorno.createSession(archivo.absolutePath, opciones)
                val nombreEntrada = sesion.inputNames.first()
                val info = sesion.inputInfo[nombreEntrada]!!.info as ai.onnxruntime.TensorInfo
                val forma = info.shape.map { if (it <= 0) 1L else it }.toLongArray()
                var total = 1
                for (d in forma) {
                    total *= d.toInt()
                }
                val datos = FloatArray(total) { ((it * 7) % 256) / 255f }
                val aperturaMs = (System.nanoTime() - t0) / 1.0e6
                val estad = Estadistica()
                val antes = InfoDispositivo.estadoTermico(contexto)
                val tIni = System.nanoTime()
                while ((System.nanoTime() - tIni) / 1.0e9 < segundos) {
                    val tensor = OnnxTensor.createTensor(entorno, FloatBuffer.wrap(datos), forma)
                    val a = System.nanoTime()
                    sesion.run(mapOf(nombreEntrada to tensor)).use { }
                    val b = System.nanoTime()
                    tensor.close()
                    estad.tiempos.add((b - a) / 1.0e6)
                    estad.marcas.add((b - tIni) / 1.0e9)
                }
                val despues = InfoDispositivo.estadoTermico(contexto)
                anotar("$ETIQUETA|modelo=$nombre|formato=onnx|bytes=${archivo.length()}|entrada=${forma.joinToString("x")}:float32|config=$nombreConfig|apertura_ms=${f(aperturaMs)}|" +
                    "${estad.linea(segundos)}|memoria_mb_antes=${f(memAntes)}|memoria_mb_despues=${f(memoriaMb())}|inicio=[$antes]|fin=[$despues]")
            } catch (e: Throwable) {
                anotar("$ETIQUETA|modelo=$nombre|formato=onnx|bytes=${archivo.length()}|config=$nombreConfig|FALLO|error=${e.javaClass.simpleName}: ${e.message?.replace('\n', ' ')?.take(200)}")
            } finally {
                try { sesion?.close() } catch (e: Throwable) { /* nada */ }
            }
        }
    }
}
