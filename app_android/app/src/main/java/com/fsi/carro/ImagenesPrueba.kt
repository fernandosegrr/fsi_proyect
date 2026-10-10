/*
 * ImagenesPrueba.kt — Las imágenes JPEG que se usan en el benchmark.
 *
 * GUÍA DE LECTURA
 *  1. cargarFuente() busca la foto de prueba que se copia al teléfono con adb
 *     (no está en git). Si no existe, sintetica() dibuja una imagen de formas.
 *  2. generarJpeg() la reduce a 640x480 o 320x240 y la comprime a calidad media
 *     (70), como la mandaría la ESP32-CAM. Eso da los bytes que se "decodifican".
 */
package com.fsi.carro

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import java.io.ByteArrayOutputStream
import java.io.File

object ImagenesPrueba {

    const val NOMBRE_FUENTE = "bench_fuente.jpg"
    const val CALIDAD_JPEG = 70   // calidad media

    /** La foto de prueba copiada al teléfono, o null si no está. */
    fun cargarFuente(contexto: Context): Bitmap? {
        val archivo = File(contexto.getExternalFilesDir(null), NOMBRE_FUENTE)
        if (!archivo.exists()) {
            return null
        }
        return android.graphics.BitmapFactory.decodeFile(archivo.absolutePath)
    }

    /** Imagen sintética de 640x480 (fondo, rectángulos y círculos); no tiene objetos reales. */
    fun sintetica(): Bitmap {
        val imagen = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888)
        val lienzo = Canvas(imagen)
        val pincel = Paint()
        lienzo.drawColor(Color.rgb(200, 190, 170))
        pincel.color = Color.rgb(60, 90, 160)
        lienzo.drawRect(80f, 120f, 260f, 420f, pincel)
        pincel.color = Color.rgb(180, 50, 50)
        lienzo.drawRect(300f, 200f, 560f, 400f, pincel)
        pincel.color = Color.rgb(40, 140, 80)
        lienzo.drawCircle(420f, 120f, 70f, pincel)
        return imagen
    }

    /** Reduce la imagen al tamaño pedido y la comprime a JPEG de calidad media. */
    fun generarJpeg(fuente: Bitmap, ancho: Int, alto: Int): ByteArray {
        val reducida = Bitmap.createScaledBitmap(fuente, ancho, alto, true)
        val salida = ByteArrayOutputStream()
        reducida.compress(Bitmap.CompressFormat.JPEG, CALIDAD_JPEG, salida)
        return salida.toByteArray()
    }
}
