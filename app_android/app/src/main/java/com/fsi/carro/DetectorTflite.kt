/*
 * DetectorTflite.kt — Detector de objetos EfficientDet-Lite0 con TensorFlow Lite.
 *
 * GUÍA DE LECTURA
 *  1. El modelo está en assets (efficientdet_lite0.tflite, de los ejemplos oficiales de
 *     TensorFlow Lite) y se lee a un buffer directo. Ya trae dentro el decodificador de
 *     cajas y la supresión de duplicados (NMS): eso se mide dentro de "inferir()".
 *  2. abrir(configuracion) crea el intérprete con CPU (1, 2 o 4 hilos), GPU o NNAPI.
 *  3. Una detección son TRES etapas que se miden por separado desde afuera:
 *       preparar()      → imagen reducida a la entrada del modelo (320x320, RGB)
 *       inferir()       → la red neuronal
 *       postprocesar()  → leer las 4 salidas (cajas, clases, confianzas, cantidad) y filtrar
 *  4. Se usa TensorFlow Lite directo (y no Task Vision / MediaPipe) porque esas
 *     librerías hacen las tres etapas dentro de una sola llamada y no dejan medirlas.
 *  5. Hay que abrir, usar y cerrar el intérprete en el MISMO hilo (lo exige la GPU).
 */
package com.fsi.carro

import android.content.Context
import android.graphics.Bitmap
import org.tensorflow.lite.DataType
import org.tensorflow.lite.Interpreter
import org.tensorflow.lite.gpu.GpuDelegate
import org.tensorflow.lite.nnapi.NnApiDelegate
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.ZipFile

enum class Delegado { CPU, GPU, NNAPI }

/** Una forma de ejecutar el modelo: nombre para el reporte, delegado y número de hilos (solo CPU). */
class Configuracion(val nombre: String, val delegado: Delegado, val hilos: Int)

/** Un objeto detectado: clase (texto) y confianza de 0 a 1. */
class Deteccion(val clase: String, val confianza: Float)

/** Un objeto detectado CON su caja: número de clase, nombre, confianza de 0 a 1 y la caja en milésimas del cuadro (0 a 1000, origen arriba a la izquierda). */
class DeteccionCaja(
    val clase: Int,
    val nombre: String,
    val confianza: Float,
    val xmin: Int,
    val ymin: Int,
    val xmax: Int,
    val ymax: Int
)

/**
 * El detector de objetos con TensorFlow Lite. `archivoModelo` es el nombre del .tflite: se busca primero en la carpeta de archivos de la app (modelos/<nombre>, para
 * probar otros modelos sin meterlos al repositorio: se copian con adb) y si no está, en assets (el EfficientDet-Lite0 que viene empaquetado).
 */
class DetectorTflite(private val contexto: Context, private val archivoModelo: String = ARCHIVO_MODELO) {

    companion object {
        const val ARCHIVO_MODELO = "efficientdet_lite0.tflite"
        const val UMBRAL = 0.4f   // por debajo de esta confianza no se cuenta como detección
        const val UMBRAL_DIAGNOSTICO = 0.2f   // el registro de diagnóstico anota lo mejor que vio el modelo si pasa de esto
        const val UMBRAL_CAJAS = 0.6f   // para las cajas que se fusionan con el detector de color: 60 % (un "refrigerator 43 %" sobre una pared lisa debe ignorarse)
    }

    private val modelo: ByteBuffer = cargarModelo()
    val etiquetas: List<String> = leerEtiquetas()

    private var interprete: Interpreter? = null
    private var delegadoGpu: GpuDelegate? = null
    private var delegadoNnapi: NnApiDelegate? = null

    // Entrada del modelo.
    private var anchoEntrada = 320
    private var altoEntrada = 320
    private var entradaFlotante = false
    private var entrada: ByteBuffer = ByteBuffer.allocateDirect(0)
    private var pixeles = IntArray(0)

    // Salidas del modelo (se reservan una vez para no medir la reserva de memoria).
    private val salidas = HashMap<Int, Any>()
    private var indiceCajas = -1
    private var indiceCantidad = -1
    private var indicesPlanos = ArrayList<Int>()   // las dos salidas [1, N]: clases y confianzas
    private var indiceClases = -1
    private var indiceConfianzas = -1

    /** Descripción de las entradas y salidas del modelo, para el reporte. */
    var descripcionTensores: String = ""
        private set

    // ── Carga del modelo ──────────────────────────────────────────────────

    /** El archivo del modelo: de la carpeta de archivos de la app (modelos/<nombre>) si está ahí, y si no de assets. */
    private fun abrirModelo(): java.io.InputStream {
        val externo = File(contexto.getExternalFilesDir(null), "modelos/" + archivoModelo)
        return if (externo.exists()) externo.inputStream() else contexto.assets.open(archivoModelo)
    }

    /** Lee el modelo de assets a un buffer directo (lo que pide el intérprete). */
    private fun cargarModelo(): ByteBuffer {
        val bytes = abrirModelo().use { it.readBytes() }
        val buffer = ByteBuffer.allocateDirect(bytes.size).order(ByteOrder.nativeOrder())
        buffer.put(bytes)
        buffer.rewind()
        return buffer
    }

    /** Las etiquetas (labels.txt) vienen empaquetadas dentro del propio archivo .tflite. */
    private fun leerEtiquetas(): List<String> {
        return try {
            val copia = File(contexto.cacheDir, archivoModelo)
            if (!copia.exists() || copia.length() != modelo.capacity().toLong()) {
                abrirModelo().use { entrada -> copia.outputStream().use { entrada.copyTo(it) } }
            }
            ZipFile(copia).use { zip ->
                val entrada = zip.getEntry("labelmap.txt") ?: zip.getEntry("labels.txt")
                zip.getInputStream(entrada).bufferedReader().readLines().filter { it.isNotBlank() }
            }
        } catch (e: Exception) {
            emptyList()
        }
    }

    // ── Abrir y cerrar ────────────────────────────────────────────────────

    /** Crea el intérprete con la configuración dada. Devuelve cuántos ms tardó (primera carga). */
    fun abrir(configuracion: Configuracion): Double {
        cerrar()
        val inicio = System.nanoTime()

        val opciones = Interpreter.Options()
        when (configuracion.delegado) {
            Delegado.CPU -> opciones.setNumThreads(configuracion.hilos)
            Delegado.GPU -> {
                val gpu = GpuDelegate()
                delegadoGpu = gpu
                opciones.addDelegate(gpu)
            }
            Delegado.NNAPI -> {
                val nnapi = NnApiDelegate()
                delegadoNnapi = nnapi
                opciones.addDelegate(nnapi)
            }
        }
        modelo.rewind()
        val nuevo = Interpreter(modelo, opciones)
        interprete = nuevo

        reservarEntradaYSalidas(nuevo)
        return (System.nanoTime() - inicio) / 1.0e6
    }

    /** Libera el intérprete y los delegados. */
    fun cerrar() {
        interprete?.close()
        interprete = null
        delegadoGpu?.close()
        delegadoGpu = null
        delegadoNnapi?.close()
        delegadoNnapi = null
    }

    /** Lee las formas del modelo y reserva los buffers de entrada y de salida. */
    private fun reservarEntradaYSalidas(modeloAbierto: Interpreter) {
        val tensorEntrada = modeloAbierto.getInputTensor(0)
        val forma = tensorEntrada.shape()                  // [1, alto, ancho, 3]
        altoEntrada = forma[1]
        anchoEntrada = forma[2]
        entradaFlotante = tensorEntrada.dataType() == DataType.FLOAT32
        val bytesPorValor = if (entradaFlotante) 4 else 1
        entrada = ByteBuffer.allocateDirect(altoEntrada * anchoEntrada * 3 * bytesPorValor).order(ByteOrder.nativeOrder())
        pixeles = IntArray(altoEntrada * anchoEntrada)

        salidas.clear()
        indicesPlanos.clear()
        indiceCajas = -1
        indiceCantidad = -1
        indiceClases = -1
        indiceConfianzas = -1
        val texto = StringBuilder("entrada: ${tensorEntrada.dataType()} ${forma.joinToString("x")}")
        for (i in 0 until modeloAbierto.outputTensorCount) {
            val tensor = modeloAbierto.getOutputTensor(i)
            val f = tensor.shape()
            texto.append("; salida $i: ${tensor.name()} ${tensor.dataType()} ${f.joinToString("x")}")
            when (f.size) {
                3 -> { salidas[i] = Array(f[0]) { Array(f[1]) { FloatArray(f[2]) } }; indiceCajas = i }
                2 -> { salidas[i] = Array(f[0]) { FloatArray(f[1]) }; indicesPlanos.add(i) }
                else -> { salidas[i] = FloatArray(f[0]); indiceCantidad = i }
            }
        }
        descripcionTensores = texto.toString()
    }

    // ── Las tres etapas ───────────────────────────────────────────────────

    /** Etapa 1 — Prepara la imagen: la reduce a la entrada del modelo y la pasa a bytes RGB. */
    fun preparar(imagen: Bitmap): ByteBuffer {
        val reducida = Bitmap.createScaledBitmap(imagen, anchoEntrada, altoEntrada, true)
        reducida.getPixels(pixeles, 0, anchoEntrada, 0, 0, anchoEntrada, altoEntrada)
        if (reducida !== imagen) {
            reducida.recycle()
        }
        entrada.rewind()
        for (pixel in pixeles) {
            val rojo = (pixel shr 16) and 0xFF
            val verde = (pixel shr 8) and 0xFF
            val azul = pixel and 0xFF
            if (entradaFlotante) {
                entrada.putFloat((rojo - 127.5f) / 127.5f)
                entrada.putFloat((verde - 127.5f) / 127.5f)
                entrada.putFloat((azul - 127.5f) / 127.5f)
            } else {
                entrada.put(rojo.toByte())
                entrada.put(verde.toByte())
                entrada.put(azul.toByte())
            }
        }
        entrada.rewind()
        return entrada
    }

    /** Etapa 2 — Inferencia: la red neuronal. */
    fun inferir() {
        interprete!!.runForMultipleInputsOutputs(arrayOf<Any>(entrada), salidas)
    }

    /** Etapa 3 — Postproceso: lee las salidas y devuelve los objetos con confianza suficiente. */
    @Suppress("UNCHECKED_CAST")
    fun postprocesar(): List<Deteccion> {
        decidirClasesYConfianzas()
        val resultado = ArrayList<Deteccion>()
        if (indiceClases < 0 || indiceConfianzas < 0 || indiceCantidad < 0) {
            return resultado
        }
        val clases = (salidas[indiceClases] as Array<FloatArray>)[0]
        val confianzas = (salidas[indiceConfianzas] as Array<FloatArray>)[0]
        val cantidad = Math.min((salidas[indiceCantidad] as FloatArray)[0].toInt(), confianzas.size)
        for (i in 0 until cantidad) {
            if (confianzas[i] >= UMBRAL) {
                val numero = clases[i].toInt()
                resultado.add(Deteccion(etiquetas.getOrNull(numero) ?: "clase $numero", confianzas[i]))
            }
        }
        return resultado
    }

    /**
     * Igual que postprocesar(), pero devolviendo también la CAJA de cada objeto (el modelo la entrega como
     * [ymin, xmin, ymax, xmax] entre 0 y 1; aquí se pasa a milésimas). Lo usa la fusión con el detector de color.
     */
    @Suppress("UNCHECKED_CAST")
    fun postprocesarCajas(): List<DeteccionCaja> {
        decidirClasesYConfianzas()
        val resultado = ArrayList<DeteccionCaja>()
        mejorSinUmbral = ""
        if (indiceClases < 0 || indiceConfianzas < 0 || indiceCantidad < 0 || indiceCajas < 0) {
            return resultado
        }
        val clases = (salidas[indiceClases] as Array<FloatArray>)[0]
        val confianzas = (salidas[indiceConfianzas] as Array<FloatArray>)[0]
        val cajas = (salidas[indiceCajas] as Array<Array<FloatArray>>)[0]
        val cantidad = Math.min(Math.min((salidas[indiceCantidad] as FloatArray)[0].toInt(), confianzas.size), cajas.size)
        // Diagnóstico (solo clase y porcentaje): lo mejor que vio el modelo, aunque no llegue al umbral de las cajas.
        var mejor = -1
        for (i in 0 until cantidad) {
            if (mejor < 0 || confianzas[i] > confianzas[mejor]) {
                mejor = i
            }
        }
        if (mejor >= 0 && confianzas[mejor] >= UMBRAL_DIAGNOSTICO) {
            val nombre = etiquetas.getOrNull(clases[mejor].toInt()) ?: "clase ${clases[mejor].toInt()}"
            val c = cajas[mejor]
            mejorSinUmbral = String.format(java.util.Locale.US, "%s %.0f%% caja x=%d..%d y=%d..%d", nombre, confianzas[mejor] * 100f,
                milesimas(c[1]), milesimas(c[3]), milesimas(c[0]), milesimas(c[2]))
        }
        for (i in 0 until cantidad) {
            if (confianzas[i] >= umbralCajas) {
                val numero = clases[i].toInt()
                val caja = cajas[i]
                resultado.add(DeteccionCaja(numero, etiquetas.getOrNull(numero) ?: "clase $numero", confianzas[i],
                    milesimas(caja[1]), milesimas(caja[0]), milesimas(caja[3]), milesimas(caja[2])))
            }
        }
        return resultado
    }

    /** Confianza mínima para devolver una caja (0.6 por defecto; el modo «cualquier objeto» la baja hasta 0.01 y filtra después en C). */
    @Volatile
    var umbralCajas = UMBRAL_CAJAS

    /** Lo mejor que vio el modelo en el último cuadro (clase, porcentaje y caja), aunque no pasara el umbral; vacío si no vio nada. Solo texto. */
    @Volatile
    var mejorSinUmbral = ""
        private set

    private fun milesimas(valor: Float): Int = Math.round(valor * 1000f).coerceIn(0, 1000)

    /**
     * De las dos salidas [1, N], las clases son números enteros (0, 1, 2...) y las
     * confianzas tienen decimales entre 0 y 1. Se decide una vez, con la primera
     * ejecución que tenga alguna detección.
     */
    @Suppress("UNCHECKED_CAST")
    private fun decidirClasesYConfianzas() {
        if (indiceClases >= 0 || indicesPlanos.size != 2) {
            return
        }
        val primera = (salidas[indicesPlanos[0]] as Array<FloatArray>)[0]
        val segunda = (salidas[indicesPlanos[1]] as Array<FloatArray>)[0]
        val primeraEntera = primera.all { it == Math.floor(it.toDouble()).toFloat() }
        val segundaEntera = segunda.all { it == Math.floor(it.toDouble()).toFloat() }
        if (primeraEntera && !segundaEntera) {
            indiceClases = indicesPlanos[0]
            indiceConfianzas = indicesPlanos[1]
        } else if (segundaEntera && !primeraEntera) {
            indiceClases = indicesPlanos[1]
            indiceConfianzas = indicesPlanos[0]
        }
    }
}
