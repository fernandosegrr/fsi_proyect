/*
 * BenchmarkObstaculos.kt — Cuánto tarda el detector de obstáculos por color de piso (en C).
 *
 * GUÍA DE LECTURA
 *  1. Se generan con código 4 imágenes de 320x240 (piso con ruido, baldosas con una caja,
 *     degradado de luz con dos objetos, y una pared) y se comprimen a JPEG calidad 70,
 *     como las mandaría la ESP32-CAM. No se descarga nada.
 *  2. Si existe la foto bench_pasillo.jpg en la carpeta de archivos de la app (copiada con
 *     adb), se prueba también con ella (se reduce a 320x240). Esa foto no se sube a git.
 *  3. Por cada imagen se repite (20 pasadas de calentamiento y 200 mediciones):
 *       decodificar el JPEG → convertir a bytes RGB → analizar en C.
 *     Cada etapa tiene su cronómetro, igual que el benchmark del modelo (BenchmarkActivity).
 *  4. También se comprueba que el resultado sea el esperado en las imágenes generadas.
 */
package com.fsi.carro

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.Locale
import java.util.Random

class BenchmarkObstaculos(private val contexto: Context, private val anotar: (String) -> Unit) {

    companion object {
        const val CALENTAMIENTO = 20
        const val MEDICIONES = 200
        const val ARCHIVO_PASILLO = "bench_pasillo.jpg"
    }

    /** Una imagen de prueba ya comprimida; "esperado" = cercanía esperada [izquierda, centro, derecha] o null. */
    private class Prueba(val nombre: String, val jpeg: ByteArray, val esperado: IntArray?)

    private fun f(valor: Double): String = String.format(Locale.US, "%.2f", valor)

    // ── Generación de imágenes ────────────────────────────────────────────

    private val azar = Random(42)

    /** Límite de un canal de color a 0..255. */
    private fun limitar(v: Int): Int = if (v < 0) 0 else if (v > 255) 255 else v

    private fun color(r: Int, g: Int, b: Int, ruido: Int): Int {
        val dr = if (ruido > 0) azar.nextInt(2 * ruido + 1) - ruido else 0
        val dg = if (ruido > 0) azar.nextInt(2 * ruido + 1) - ruido else 0
        val db = if (ruido > 0) azar.nextInt(2 * ruido + 1) - ruido else 0
        return (0xFF shl 24) or (limitar(r + dr) shl 16) or (limitar(g + dg) shl 8) or limitar(b + db)
    }

    /** Pinta un rectángulo de píxeles [x0, x1) x [y0, y1) de un color con ruido. */
    private fun rectangulo(pixeles: IntArray, x0: Int, y0: Int, x1: Int, y1: Int, r: Int, g: Int, b: Int, ruido: Int) {
        for (y in y0 until y1) {
            for (x in x0 until x1) {
                pixeles[y * ObstaculosNativa.ANCHO + x] = color(r, g, b, ruido)
            }
        }
    }

    private fun comoJpeg(pixeles: IntArray): ByteArray {
        val bitmap = Bitmap.createBitmap(pixeles, ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO, Bitmap.Config.ARGB_8888)
        val salida = ByteArrayOutputStream()
        bitmap.compress(Bitmap.CompressFormat.JPEG, ImagenesPrueba.CALIDAD_JPEG, salida)
        return salida.toByteArray()
    }

    /** Imagen 1: piso liso con ruido. No debe haber obstáculos. */
    private fun pisoConRuido(): Prueba {
        val p = IntArray(ObstaculosNativa.ANCHO * ObstaculosNativa.ALTO)
        rectangulo(p, 0, 0, ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO, 120, 105, 90, 8)
        return Prueba("piso-con-ruido", comoJpeg(p), intArrayOf(0, 0, 0))
    }

    /** Imagen 2: baldosas de dos tonos y una caja roja al centro (llega a la fila 19: cercanía 2). */
    private fun baldosasConCaja(): Prueba {
        val p = IntArray(ObstaculosNativa.ANCHO * ObstaculosNativa.ALTO)
        for (y in 0 until ObstaculosNativa.ALTO step 40) {
            for (x in 0 until ObstaculosNativa.ANCHO step 40) {
                val claro = ((x / 40) + (y / 40)) % 2 == 0
                rectangulo(p, x, y, x + 40, y + 40, if (claro) 118 else 126, if (claro) 103 else 111, if (claro) 88 else 96, 6)
            }
        }
        rectangulo(p, 130, 130, 200, 200, 190, 60, 50, 6)
        return Prueba("baldosas-caja-centro", comoJpeg(p), intArrayOf(0, 2, 0))
    }

    /** Imagen 3: piso con degradado de luz (más claro arriba), una caja a la izquierda y un círculo a la derecha. */
    private fun degradadoConDosObjetos(): Prueba {
        val p = IntArray(ObstaculosNativa.ANCHO * ObstaculosNativa.ALTO)
        for (y in 0 until ObstaculosNativa.ALTO) {
            val extra = (ObstaculosNativa.ALTO - 1 - y) * 50 / (ObstaculosNativa.ALTO - 1)
            rectangulo(p, 0, y, ObstaculosNativa.ANCHO, y + 1, 115 + extra, 100 + extra, 85 + extra, 6)
        }
        rectangulo(p, 30, 150, 80, 210, 200, 150, 40, 6)   // caja amarilla a la izquierda
        for (y in 130 until 210) {                         // círculo azul a la derecha
            for (x in 235 until 305) {
                val dx = x - 270
                val dy = y - 170
                if (dx * dx + dy * dy <= 35 * 35) {
                    p[y * ObstaculosNativa.ANCHO + x] = color(60, 120, 200, 6)
                }
            }
        }
        return Prueba("degradado-dos-objetos", comoJpeg(p), intArrayOf(3, 0, 3))
    }

    /** Imagen 4: una pared que llena todo menos las dos últimas filas de celdas (el piso de la franja). */
    private fun pared(): Prueba {
        val p = IntArray(ObstaculosNativa.ANCHO * ObstaculosNativa.ALTO)
        rectangulo(p, 0, 0, ObstaculosNativa.ANCHO, 220, 170, 175, 185, 5)
        rectangulo(p, 0, 220, ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO, 120, 105, 90, 5)
        return Prueba("pared", comoJpeg(p), intArrayOf(3, 3, 3))
    }

    /** Imagen 5 (opcional): foto real copiada con adb; se reduce a 320x240. Sin resultado esperado. */
    private fun fotoDePasillo(): Prueba? {
        val archivo = File(contexto.getExternalFilesDir(null), ARCHIVO_PASILLO)
        if (!archivo.exists()) {
            return null
        }
        val original = BitmapFactory.decodeFile(archivo.absolutePath) ?: return null
        val reducida = Bitmap.createScaledBitmap(original, ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO, true)
        val salida = ByteArrayOutputStream()
        reducida.compress(Bitmap.CompressFormat.JPEG, ImagenesPrueba.CALIDAD_JPEG, salida)
        return Prueba("foto-de-pasillo-real", salida.toByteArray(), null)
    }

    // ── Medición ──────────────────────────────────────────────────────────

    fun ejecutar() {
        val pruebas = ArrayList<Prueba>()
        pruebas.add(pisoConRuido())
        pruebas.add(baldosasConCaja())
        pruebas.add(degradadoConDosObjetos())
        pruebas.add(pared())
        val foto = fotoDePasillo()
        if (foto != null) {
            pruebas.add(foto)
        }
        anotar("BENCH|obstaculos|imagenes=${pruebas.size}|foto_real=${foto != null}|calentamiento=$CALENTAMIENTO|mediciones=$MEDICIONES|jpeg_calidad=${ImagenesPrueba.CALIDAD_JPEG}")
        for (prueba in pruebas) {
            medir(prueba)
        }
    }

    private fun medir(prueba: Prueba) {
        val pixeles = IntArray(ObstaculosNativa.ANCHO * ObstaculosNativa.ALTO)
        val rgb = ByteArray(ObstaculosNativa.ANCHO * ObstaculosNativa.ALTO * 3)
        val decodificar = DoubleArray(MEDICIONES)
        val convertir = DoubleArray(MEDICIONES)
        val analizar = DoubleArray(MEDICIONES)
        val total = DoubleArray(MEDICIONES)
        var resultado = IntArray(0)

        for (pasada in 0 until CALENTAMIENTO + MEDICIONES) {
            val t0 = System.nanoTime()
            val imagen = BitmapFactory.decodeByteArray(prueba.jpeg, 0, prueba.jpeg.size)
                ?: throw IllegalStateException("No se pudo decodificar el JPEG")
            val t1 = System.nanoTime()
            imagen.getPixels(pixeles, 0, ObstaculosNativa.ANCHO, 0, 0, ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO)
            var posicion = 0
            for (pixel in pixeles) {
                rgb[posicion] = (pixel shr 16).toByte()
                rgb[posicion + 1] = (pixel shr 8).toByte()
                rgb[posicion + 2] = pixel.toByte()
                posicion += 3
            }
            val t2 = System.nanoTime()
            resultado = ObstaculosNativa.analizar(rgb, ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO)
            val t3 = System.nanoTime()
            imagen.recycle()

            val indice = pasada - CALENTAMIENTO
            if (indice >= 0) {
                decodificar[indice] = (t1 - t0) / 1.0e6
                convertir[indice] = (t2 - t1) / 1.0e6
                analizar[indice] = (t3 - t2) / 1.0e6
                total[indice] = (t3 - t0) / 1.0e6
            }
        }

        val base = "BENCH|obstaculos|imagen=${prueba.nombre}|jpeg_bytes=${prueba.jpeg.size}|n=$MEDICIONES"
        anotar(linea(base, "decodificar", decodificar))
        anotar(linea(base, "convertir_rgb", convertir))
        anotar(linea(base, "analizar_en_c", analizar))
        anotar(linea(base, "total", total) + "|fps_posibles=${f(1000.0 / resumir(total).media)}")

        val cercanias = intArrayOf(resultado[ObstaculosNativa.CERCANIA_IZQUIERDA], resultado[ObstaculosNativa.CERCANIA_CENTRO], resultado[ObstaculosNativa.CERCANIA_DERECHA])
        val esperado = prueba.esperado
        val veredicto = if (esperado == null) "sin_esperado" else if (cercanias.contentEquals(esperado)) "OK" else "FALLO"
        anotar("$base|resultado|codigo=${resultado[ObstaculosNativa.CODIGO]}|cercania_izq_centro_der=${cercanias.joinToString(",")}|esperado=${esperado?.joinToString(",") ?: "-"}|veredicto=$veredicto|piso_rgb=${resultado[ObstaculosNativa.REF_R]},${resultado[ObstaculosNativa.REF_G]},${resultado[ObstaculosNativa.REF_B]}")
    }

    private fun linea(base: String, etapa: String, muestras: DoubleArray): String {
        val r = resumir(muestras)
        return "$base|etapa=$etapa|media_ms=${f(r.media)}|mediana_ms=${f(r.mediana)}|p95_ms=${f(r.p95)}|min_ms=${f(r.minimo)}|max_ms=${f(r.maximo)}"
    }
}
