/*
 * ModeloEnHilo.kt — El modelo de objetos (EfficientDet-Lite0) corriendo en su PROPIO hilo, sin frenar al
 * detector de color.
 *
 * GUÍA DE LECTURA
 *  1. El detector de color (~7 ms) corre en el hilo de la cámara con cada cuadro. El modelo tarda ~45 ms, así que NO
 *     puede correr ahí: aquí tiene un hilo aparte ("modelo-objetos") que abre el intérprete, lo usa y lo cierra
 *     (TensorFlow Lite exige usarlo siempre desde el mismo hilo).
 *  2. ofrecer() se llama desde el hilo de la cámara con cada cuadro y NO espera: solo deja el cuadro como "el más reciente
 *     pendiente" (si ya había uno sin procesar, lo reemplaza: nunca se acumulan cuadros viejos). Cuesta microsegundos.
 *  3. El hilo del modelo toma el pendiente, como máximo INTERVALO_MIN_MS (100 ms = 10 veces por segundo), corre las tres etapas
 *     (preparar, inferir, postprocesar; se miden por separado) y publica el resultado en `ultimo` (un solo objeto
 *     inmutable, así leerlo desde otro hilo es seguro).
 *  4. La MARCA de tiempo del resultado es la hora del CUADRO (cuando se ofreció), no la de cuando terminó el modelo: así la
 *     "edad" que mira la fusión (caduca a 300 ms) ya incluye lo que tardó el modelo. La fusión (fusion.c) ignora lo que
 *     esté caducado; si el modelo se cuelga, el detector de color sigue decidiendo solo.
 *  5. Si el modelo no se puede abrir (falta el archivo, error del intérprete) queda `error` y el hilo termina: el resto
 *     de la app sigue funcionando solo con el detector de color.
 */
package com.fsi.carro

import android.content.Context
import android.graphics.Bitmap
import android.os.SystemClock
import android.util.Log
import java.util.concurrent.atomic.AtomicReference
import java.util.concurrent.locks.LockSupport

/** Lo que dijo el modelo sobre UN cuadro. Es inmutable: se publica de un hilo a otro sin candados. */
class ResultadoModelo(
    val numero: Long,
    val marcaMs: Long,              // hora (SystemClock.elapsedRealtime) del cuadro que se analizó
    val cajas: IntArray,            // [n, (clase, confianza_mil, xmin, ymin, xmax, ymax) * n]: el formato de FusionNativa
    val nombres: List<String>,      // el nombre de la clase de cada caja
    val msPreparar: Double,
    val msInferir: Double,
    val msPostprocesar: Double
) {
    val msTotal: Double get() = msPreparar + msInferir + msPostprocesar
    val cantidad: Int get() = nombres.size
}

class ModeloEnHilo(private val contexto: Context) {

    companion object {
        const val ETIQUETA = "MODELO"
        const val INTERVALO_MIN_MS = 100L          // como máximo 10 veces por segundo
        private const val ESPERA_SIN_CUADRO_NS = 10_000_000L   // 10 ms de espera cuando no hay cuadro pendiente
    }

    private class Pendiente(val pixeles: IntArray, val ancho: Int, val alto: Int, val marcaMs: Long)

    private val pendiente = AtomicReference<Pendiente?>(null)
    private var hilo: Thread? = null

    @Volatile
    private var corriendo = false

    /** Lo último que dijo el modelo (null si todavía no terminó ninguna). */
    @Volatile
    var ultimo: ResultadoModelo? = null
        private set
    private var ultimoLogModeloMs = 0L

    /** Confianza mínima de las cajas que devuelve el modelo (0.6 por defecto; el modo «cualquier objeto» pide 0.01 y filtra en C). */
    @Volatile
    var umbralCajas = DetectorTflite.UMBRAL_CAJAS

    /** Mensaje de error (vacío si todo va bien). */
    @Volatile
    var error = ""
        private set

    /** Las etiquetas del modelo (se llenan cuando el hilo abre el modelo). */
    @Volatile
    var etiquetas: List<String> = emptyList()
        private set

    /** Cuántas inferencias se han hecho y los ms medios de la última ventana, para la pantalla. */
    @Volatile
    var inferencias = 0L
        private set

    @Volatile
    var msEntreResultados = 0.0
        private set

    val activo: Boolean get() = corriendo

    /** Arranca el hilo (si no estaba corriendo). */
    fun iniciar() {
        if (corriendo) {
            return
        }
        error = ""
        corriendo = true
        pendiente.set(null)
        hilo = Thread({ bucle() }, "modelo-objetos").also { it.start() }
    }

    /** Detiene el hilo y espera a que cierre el intérprete. */
    fun detener() {
        corriendo = false
        val h = hilo
        hilo = null
        if (h != null) {
            LockSupport.unpark(h)
            try { h.join(2000) } catch (e: InterruptedException) { /* seguimos */ }
        }
        pendiente.set(null)
        ultimo = null
    }

    /**
     * Ofrece un cuadro al modelo. NO espera: solo lo deja como el más reciente pendiente (reemplaza al anterior si
     * todavía no se había tomado). `pixeles` son colores ARGB de ancho x alto; el arreglo no se modifica.
     */
    fun ofrecer(pixeles: IntArray, ancho: Int, alto: Int, marcaMs: Long) {
        if (!corriendo) {
            return
        }
        pendiente.set(Pendiente(pixeles, ancho, alto, marcaMs))
        hilo?.let { LockSupport.unpark(it) }
    }

    // ── El hilo del modelo ────────────────────────────────────────────────

    private fun bucle() {
        val detector = DetectorTflite(contexto)
        try {
            detector.abrir(Configuracion("CPU 1 hilo", Delegado.CPU, 1))
            etiquetas = detector.etiquetas
            Log.i(ETIQUETA, "modelo abierto: ${detector.descripcionTensores}")
        } catch (e: Exception) {
            error = "No se pudo abrir el modelo: ${e.javaClass.simpleName}: ${e.message}"
            Log.w(ETIQUETA, error)
            corriendo = false
            return
        }
        var ultimaInferenciaMs = 0L
        var contador = 0L
        var ultimoResultadoMs = 0L
        try {
            while (corriendo) {
                val ahora = SystemClock.elapsedRealtime()
                val espera = INTERVALO_MIN_MS - (ahora - ultimaInferenciaMs)
                if (espera > 0) {
                    LockSupport.parkNanos(espera * 1_000_000L)   // no se pasa de 10 veces por segundo
                    continue
                }
                val cuadro = pendiente.getAndSet(null)
                if (cuadro == null) {
                    LockSupport.parkNanos(ESPERA_SIN_CUADRO_NS)
                    continue
                }
                ultimaInferenciaMs = SystemClock.elapsedRealtime()
                try {
                    val resultado = inferir(detector, cuadro, ++contador)
                    ultimo = resultado
                    inferencias = contador
                    val fin = SystemClock.elapsedRealtime()
                    if (ultimoResultadoMs != 0L) {
                        msEntreResultados = (fin - ultimoResultadoMs).toDouble()
                    }
                    ultimoResultadoMs = fin
                } catch (e: Exception) {
                    error = "Error del modelo: ${e.javaClass.simpleName}: ${e.message}"
                    Log.w(ETIQUETA, error)
                    corriendo = false
                }
            }
        } finally {
            detector.cerrar()
            Log.i(ETIQUETA, "modelo cerrado")
        }
    }

    /** Las tres etapas, medidas por separado, y el resultado en el formato de la fusión. */
    private fun inferir(detector: DetectorTflite, cuadro: Pendiente, numero: Long): ResultadoModelo {
        val t0 = System.nanoTime()
        val bitmap = Bitmap.createBitmap(cuadro.pixeles, cuadro.ancho, cuadro.alto, Bitmap.Config.ARGB_8888)
        detector.preparar(bitmap)
        bitmap.recycle()
        val t1 = System.nanoTime()
        detector.inferir()
        val t2 = System.nanoTime()
        detector.umbralCajas = umbralCajas
        val detecciones = detector.postprocesarCajas()
        val ahoraLog = android.os.SystemClock.elapsedRealtime()
        if (ahoraLog - ultimoLogModeloMs >= 1000L) {   // una línea por segundo: lo mejor que vio el modelo (aunque no pase el umbral)
            ultimoLogModeloMs = ahoraLog
            Log.i("MODELO", "mejor=" + (if (detector.mejorSinUmbral.isEmpty()) "nada (menos de 20%)" else detector.mejorSinUmbral) + " | cajas_que_pasan_${Math.round(umbralCajas * 100f)}%=${detecciones.size}")
        }
        val t3 = System.nanoTime()

        val n = Math.min(detecciones.size, 10)   // FUS_MAX_CAJAS
        val cajas = IntArray(1 + n * FusionNativa.ENTEROS_POR_CAJA)
        cajas[0] = n
        val nombres = ArrayList<String>(n)
        for (i in 0 until n) {
            val d = detecciones[i]
            val base = 1 + i * FusionNativa.ENTEROS_POR_CAJA
            cajas[base] = d.clase
            cajas[base + 1] = Math.round(d.confianza * 1000f)
            cajas[base + 2] = d.xmin
            cajas[base + 3] = d.ymin
            cajas[base + 4] = d.xmax
            cajas[base + 5] = d.ymax
            nombres.add(d.nombre)
        }
        return ResultadoModelo(numero, cuadro.marcaMs, cajas, nombres,
            (t1 - t0) / 1.0e6, (t2 - t1) / 1.0e6, (t3 - t2) / 1.0e6)
    }
}
