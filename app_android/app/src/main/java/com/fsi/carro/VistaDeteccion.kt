/*
 * VistaDeteccion.kt — Dibuja el cuadro de la cámara con lo que detecta el detector de obstáculos.
 *
 * GUÍA DE LECTURA
 *  1. Es una vista propia (View). mostrar() recibe el último CuadroAnalizado y pide redibujar.
 *  2. onDraw() dibuja, en este orden: la imagen de 320x240 (lo que realmente ve el detector,
 *     así que sirve de "vista previa"), la cuadrícula de celdas, las celdas "no piso" en rojo
 *     semitransparente, la franja de referencia del piso en azul, y las tres zonas (izquierda,
 *     centro, derecha) con su estado y su cercanía.
 *  3. La zona es verde si está libre y roja si tiene obstáculo; el número es la cercanía
 *     (1 = lejos, 2 = media, 3 = muy cerca).
 */
package com.fsi.carro

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View

class VistaDeteccion(contexto: Context, atributos: AttributeSet?) : View(contexto, atributos) {

    private val imagen = Bitmap.createBitmap(ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO, Bitmap.Config.ARGB_8888)
    private var cuadro: CuadroAnalizado? = null
    private var mensaje = "Sin imagen todavía"

    private val pincelImagen = Paint().apply { isFilterBitmap = true }
    private val pincelRojo = Paint().apply { color = Color.argb(120, 255, 0, 0) }
    private val pincelAzul = Paint().apply { color = Color.argb(110, 0, 90, 255) }
    private val pincelRejilla = Paint().apply { color = Color.argb(70, 255, 255, 255); strokeWidth = 1f }
    private val pincelBorde = Paint().apply { style = Paint.Style.STROKE; strokeWidth = 4f }
    private val pincelFondoTexto = Paint().apply { color = Color.argb(170, 0, 0, 0) }
    private val pincelTexto = Paint().apply { color = Color.WHITE; textAlign = Paint.Align.CENTER; isFakeBoldText = true }

    /** Muestra un cuadro nuevo (hay que llamarlo en el hilo principal). null = sin cuadro. */
    fun mostrar(nuevo: CuadroAnalizado?, texto: String = "") {
        cuadro = nuevo
        mensaje = texto
        if (nuevo != null) {
            imagen.setPixels(nuevo.pixeles, 0, ObstaculosNativa.ANCHO, 0, 0, ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO)
        }
        invalidate()
    }

    override fun onDraw(lienzo: Canvas) {
        lienzo.drawColor(Color.BLACK)
        val cuadroActual = cuadro
        if (cuadroActual == null) {
            pincelTexto.textSize = 36f
            lienzo.drawText(mensaje, width / 2f, height / 2f, pincelTexto)
            return
        }

        // La imagen ocupa todo lo que pueda manteniendo la proporción 4:3.
        val escala = Math.min(width.toFloat() / ObstaculosNativa.ANCHO, height.toFloat() / ObstaculosNativa.ALTO)
        val ancho = ObstaculosNativa.ANCHO * escala
        val alto = ObstaculosNativa.ALTO * escala
        val izquierda = (width - ancho) / 2f
        val arriba = (height - alto) / 2f
        val celdaAncho = ancho / ObstaculosNativa.COLUMNAS
        val celdaAlto = alto / ObstaculosNativa.FILAS

        lienzo.drawBitmap(imagen, Rect(0, 0, ObstaculosNativa.ANCHO, ObstaculosNativa.ALTO),
            RectF(izquierda, arriba, izquierda + ancho, arriba + alto), pincelImagen)

        for (c in 0..ObstaculosNativa.COLUMNAS) {
            lienzo.drawLine(izquierda + c * celdaAncho, arriba, izquierda + c * celdaAncho, arriba + alto, pincelRejilla)
        }
        for (f in 0..ObstaculosNativa.FILAS) {
            lienzo.drawLine(izquierda, arriba + f * celdaAlto, izquierda + ancho, arriba + f * celdaAlto, pincelRejilla)
        }

        val resultado = cuadroActual.resultado
        val config = cuadroActual.config

        // Franja de referencia del piso (azul).
        val colRef = config[ObstaculosNativa.CFG_REF_COLUMNA_INICIAL]
        val filaRef = config[ObstaculosNativa.CFG_REF_FILA_INICIAL]
        lienzo.drawRect(izquierda + colRef * celdaAncho, arriba + filaRef * celdaAlto,
            izquierda + (colRef + config[ObstaculosNativa.CFG_REF_COLUMNAS]) * celdaAncho,
            arriba + (filaRef + config[ObstaculosNativa.CFG_REF_FILAS]) * celdaAlto, pincelAzul)

        // Celdas "no piso" (rojo).
        if (resultado[ObstaculosNativa.CODIGO] == 0) {
            for (f in 0 until ObstaculosNativa.FILAS) {
                for (c in 0 until ObstaculosNativa.COLUMNAS) {
                    if (resultado[ObstaculosNativa.INICIO_MASCARA + f * ObstaculosNativa.COLUMNAS + c] != 0) {
                        lienzo.drawRect(izquierda + c * celdaAncho, arriba + f * celdaAlto,
                            izquierda + (c + 1) * celdaAncho, arriba + (f + 1) * celdaAlto, pincelRojo)
                    }
                }
            }
        }

        // Las tres zonas, en la mitad inferior.
        val nombres = arrayOf("IZQ", "CENTRO", "DER")
        val hay = intArrayOf(resultado[ObstaculosNativa.HAY_IZQUIERDA], resultado[ObstaculosNativa.HAY_CENTRO], resultado[ObstaculosNativa.HAY_DERECHA])
        val cercania = intArrayOf(resultado[ObstaculosNativa.CERCANIA_IZQUIERDA], resultado[ObstaculosNativa.CERCANIA_CENTRO], resultado[ObstaculosNativa.CERCANIA_DERECHA])
        val mitad = arriba + alto / 2f
        // El texto cabe en el ancho de una zona (la etiqueta más larga tiene unos 20 caracteres).
        pincelTexto.textSize = Math.max(12f, Math.min(alto / 14f, ancho / 3f / 11f))
        for (zona in 0..2) {
            val x0 = izquierda + ancho * zona / 3f
            val x1 = izquierda + ancho * (zona + 1) / 3f
            pincelBorde.color = if (hay[zona] != 0) Color.RED else Color.GREEN
            lienzo.drawRect(x0 + 3f, mitad + 3f, x1 - 3f, arriba + alto - 3f, pincelBorde)
            val etiqueta = if (resultado[ObstaculosNativa.CODIGO] != 0) "${nombres[zona]}: ?" else
                if (hay[zona] != 0) "${nombres[zona]}: OBSTACULO ${cercania[zona]}" else "${nombres[zona]}: libre"
            val yTexto = mitad + pincelTexto.textSize * 1.3f
            val mitadAncho = pincelTexto.measureText(etiqueta) / 2f + 8f
            lienzo.drawRect((x0 + x1) / 2f - mitadAncho, yTexto - pincelTexto.textSize, (x0 + x1) / 2f + mitadAncho, yTexto + 8f, pincelFondoTexto)
            lienzo.drawText(etiqueta, (x0 + x1) / 2f, yTexto, pincelTexto)
        }

        if (resultado[ObstaculosNativa.CODIGO] != 0) {
            val texto = when (resultado[ObstaculosNativa.CODIGO]) {
                ObstaculosNativa.ERR_REFERENCIA -> "El piso de referencia (azul) no es uniforme"
                ObstaculosNativa.ERR_CONFIG -> "Configuración no válida"
                else -> "Error ${resultado[ObstaculosNativa.CODIGO]}"
            }
            val mitadAncho = pincelTexto.measureText(texto) / 2f + 10f
            lienzo.drawRect(izquierda + ancho / 2f - mitadAncho, arriba + 10f, izquierda + ancho / 2f + mitadAncho,
                arriba + 10f + pincelTexto.textSize * 1.4f, pincelFondoTexto)
            lienzo.drawText(texto, izquierda + ancho / 2f, arriba + 10f + pincelTexto.textSize * 1.1f, pincelTexto)
        }
    }
}
