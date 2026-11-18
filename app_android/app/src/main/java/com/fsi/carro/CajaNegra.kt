/*
 * CajaNegra.kt — La CAJA NEGRA de cada corrida del modo autónomo (autónomo v2).
 *
 * GUÍA DE LECTURA
 *  1. Para qué: el registro del sistema (logcat) del teléfono descarta líneas y se pierde al desconectar el cable. Aquí cada ruta deja UN archivo
 *     de texto con todo lo que hizo falta para entender después qué pasó: la configuración con la que salió, el estado del navegador en cada tick
 *     (línea «CN ...»), cada decisión del navegador (las líneas *_DIAG), lo que mandó el carro (láser, giroscopio, frenos) y la telemetría NUMÉRICA
 *     de la visión de cada cuadro (línea «VTEL ...»: histograma de distancias al piso, brillo, celdas por zona y fila). Solo números y palabras:
 *     NUNCA se guarda una imagen.
 *  2. Dónde: en la carpeta propia de la app, `Android/data/com.fsi.carro/files/cajas_negras/cn_<fecha>_<hora>.txt`. Se sacan con el cable
 *     (`adb pull /sdcard/Android/data/com.fsi.carro/files/cajas_negras`) o con el explorador de archivos. Se conservan las últimas MAX_ARCHIVOS.
 *  3. Cómo: se escribe en un hilo propio (una cola): el tick del navegador nunca espera al disco. Si el disco falla, la ruta sigue igual.
 *  4. Formato de cada línea: `<ms desde que empezó la ruta> <texto>`. La primera palabra del texto dice qué es: CAB (cabecera), CN, VTEL, EV (evento:
 *     comando enviado, TOF_PARO, WD...), las etiquetas *_DIAG del navegador y FIN.
 */
package com.fsi.carro

import android.content.Context
import android.os.SystemClock
import java.io.BufferedWriter
import java.io.File
import java.io.FileWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

object CajaNegra {
    private const val CARPETA = "cajas_negras"
    private const val MAX_ARCHIVOS = 40
    private const val MAX_LINEAS = 60000          // unos 6 MB: una ruta de 5 minutos queda muy por debajo

    private var raiz: File? = null
    private val hilo: ExecutorService = Executors.newSingleThreadExecutor { tarea -> Thread(tarea, "caja-negra").apply { isDaemon = true } }
    private var escritor: BufferedWriter? = null   // solo se toca desde `hilo`
    @Volatile private var abierta = false
    @Volatile private var inicioMs = 0L
    @Volatile private var lineas = 0
    @Volatile var ultimoArchivo: String = ""       // nombre del archivo de la última corrida (para mostrarlo)
        private set

    /** Se llama una vez al abrir la app: recuerda la carpeta donde se escriben los archivos. */
    fun iniciar(contexto: Context) {
        raiz = contexto.applicationContext.getExternalFilesDir(null)?.let { File(it, CARPETA) }
    }

    /** La carpeta de las cajas negras como texto (para decirle al usuario dónde están), o "" si no hay almacenamiento. */
    fun carpeta(): String = raiz?.absolutePath ?: ""

    /** Cuántos archivos hay guardados. */
    fun cuantas(): Int = raiz?.listFiles { f -> f.isFile && f.name.startsWith("cn_") }?.size ?: 0

    /** Empieza el archivo de una corrida. `cabecera` son líneas «clave=valor» (se escriben como CAB). Si ya había una abierta, se cierra primero. */
    fun abrir(cabecera: List<String>) {
        val carpeta = raiz ?: return
        val nombre = "cn_" + SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date()) + ".txt"
        inicioMs = SystemClock.elapsedRealtime()
        lineas = 0
        abierta = true
        ultimoArchivo = nombre
        hilo.execute {
            try {
                escritor?.close()
                carpeta.mkdirs()
                carpeta.listFiles { f -> f.isFile && f.name.startsWith("cn_") }?.sortedBy { it.name }?.dropLast(MAX_ARCHIVOS - 1)?.forEach { it.delete() }
                val w = BufferedWriter(FileWriter(File(carpeta, nombre)), 16 * 1024)
                w.write("0 CAB caja_negra=1 formato=ms_desde_inicio+texto\n")
                for (l in cabecera) {
                    w.write("0 CAB " + l.replace('\n', ' ') + "\n")
                }
                escritor = w
            } catch (e: Exception) {
                escritor = null
            }
        }
    }

    /** Agrega una línea a la corrida en curso (no hace nada si no hay ninguna abierta o si el texto está vacío). Se puede llamar desde cualquier hilo. */
    fun linea(texto: String) {
        if (!abierta || texto.isEmpty() || lineas >= MAX_LINEAS) {
            return
        }
        lineas++
        val t = SystemClock.elapsedRealtime() - inicioMs
        hilo.execute {
            try {
                escritor?.write("$t ${texto.replace('\n', ' ')}\n")
            } catch (e: Exception) {
                escritor = null
            }
        }
    }

    /** Termina la corrida: escribe la línea FIN con `resultado` y cierra el archivo. */
    fun cerrar(resultado: String) {
        if (!abierta) {
            return
        }
        val t = SystemClock.elapsedRealtime() - inicioMs
        val n = lineas
        abierta = false
        hilo.execute {
            try {
                escritor?.write("$t FIN lineas=$n ${resultado.replace('\n', ' ')}\n")
                escritor?.close()
            } catch (e: Exception) {
                // el archivo queda como esté: la ruta ya terminó
            }
            escritor = null
        }
    }
}
