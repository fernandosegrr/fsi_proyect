/*
 * CuadriculaView.kt — Vista propia: cuadrícula donde se dibuja la ruta con el dedo.
 *
 * GUÍA DE LECTURA
 *  1. La cuadrícula tiene COLUMNAS x FILAS celdas; la fila 0 es la de ARRIBA
 *     (igual que en el módulo en C: la fila crece hacia abajo).
 *  2. La ruta se guarda en un arreglo fijo [columna0, fila0, columna1, fila1, ...].
 *  3. onTouchEvent(): al tocar empieza una ruta nueva; al mover el dedo se
 *     agregan celdas.
 *  4. agregarHasta(): si el dedo salta varias celdas, se rellenan una por una
 *     (en pasos de 8 vecinos), así cada punto es vecino del anterior.
 *  5. onDraw(): dibuja celdas, líneas, la ruta, el inicio (I) y la meta (M).
 */
package com.fsi.carro

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View

class CuadriculaView(contexto: Context, atributos: AttributeSet?) : View(contexto, atributos) {

    companion object {
        const val COLUMNAS = 12
        const val FILAS = 8
    }

    /** Se llama cada vez que cambia la ruta (para actualizar el contador). */
    var alCambiarRuta: (() -> Unit)? = null

    private val maxPuntos = TrayectoriaNativa.maxPuntos()
    private val ruta = IntArray(2 * maxPuntos)   // [columna, fila, columna, fila, ...]
    private var cantidad = 0                     // cuántos puntos tiene la ruta

    private val pincelRelleno = Paint().apply { color = Color.rgb(187, 222, 251) }
    private val pincelLinea = Paint().apply { color = Color.LTGRAY; strokeWidth = 2f }
    private val pincelRuta = Paint().apply { color = Color.rgb(25, 118, 210); strokeWidth = 8f }
    private val pincelInicio = Paint().apply { color = Color.rgb(46, 125, 50) }
    private val pincelMeta = Paint().apply { color = Color.rgb(198, 40, 40) }
    private val pincelLetra = Paint().apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }

    /** Número de puntos de la ruta. */
    fun cantidadPuntos(): Int = cantidad

    /** Copia de la ruta como [columna0, fila0, ...] lista para pasar al código en C. */
    fun puntosComoArreglo(): IntArray = ruta.copyOf(2 * cantidad)

    /** Borra la ruta. */
    fun borrar() {
        cantidad = 0
        invalidate()
        alCambiarRuta?.invoke()
    }

    // ── Tamaño: la cuadrícula ocupa todo el ancho y mantiene la proporción ──

    override fun onMeasure(anchoSpec: Int, altoSpec: Int) {
        val ancho = MeasureSpec.getSize(anchoSpec)
        setMeasuredDimension(ancho, ancho * FILAS / COLUMNAS)
    }

    // ── Dedo ────────────────────────────────────────────────────────────────

    override fun onTouchEvent(evento: MotionEvent): Boolean {
        val columna = (evento.x * COLUMNAS / width).toInt().coerceIn(0, COLUMNAS - 1)
        val fila = (evento.y * FILAS / height).toInt().coerceIn(0, FILAS - 1)

        if (evento.actionMasked == MotionEvent.ACTION_DOWN) {
            cantidad = 0
            agregarPunto(columna, fila)
        } else if (evento.actionMasked == MotionEvent.ACTION_MOVE) {
            agregarHasta(columna, fila)
        }
        invalidate()
        alCambiarRuta?.invoke()
        return true
    }

    /** Agrega un punto al final si todavía hay lugar. */
    private fun agregarPunto(columna: Int, fila: Int) {
        if (cantidad < maxPuntos) {
            ruta[2 * cantidad] = columna
            ruta[2 * cantidad + 1] = fila
            cantidad++
        }
    }

    /** Avanza desde el último punto hasta la celda indicada, una celda vecina a la vez. */
    private fun agregarHasta(columna: Int, fila: Int) {
        while (cantidad in 1 until maxPuntos) {
            val ultimaColumna = ruta[2 * cantidad - 2]
            val ultimaFila = ruta[2 * cantidad - 1]
            if (ultimaColumna == columna && ultimaFila == fila) {
                return
            }
            agregarPunto(
                ultimaColumna + Integer.signum(columna - ultimaColumna),
                ultimaFila + Integer.signum(fila - ultimaFila)
            )
        }
    }

    // ── Dibujo ──────────────────────────────────────────────────────────────

    override fun onDraw(lienzo: Canvas) {
        val celdaAncho = width.toFloat() / COLUMNAS
        val celdaAlto = height.toFloat() / FILAS

        for (i in 0 until cantidad) {
            val x = ruta[2 * i] * celdaAncho
            val y = ruta[2 * i + 1] * celdaAlto
            lienzo.drawRect(x, y, x + celdaAncho, y + celdaAlto, pincelRelleno)
        }
        for (c in 0..COLUMNAS) {
            lienzo.drawLine(c * celdaAncho, 0f, c * celdaAncho, height.toFloat(), pincelLinea)
        }
        for (f in 0..FILAS) {
            lienzo.drawLine(0f, f * celdaAlto, width.toFloat(), f * celdaAlto, pincelLinea)
        }
        for (i in 1 until cantidad) {
            lienzo.drawLine(
                (ruta[2 * i - 2] + 0.5f) * celdaAncho, (ruta[2 * i - 1] + 0.5f) * celdaAlto,
                (ruta[2 * i] + 0.5f) * celdaAncho, (ruta[2 * i + 1] + 0.5f) * celdaAlto,
                pincelRuta
            )
        }
        if (cantidad > 0) {
            marcar(lienzo, 0, pincelInicio, "I", celdaAncho, celdaAlto)
            if (cantidad > 1) {
                marcar(lienzo, cantidad - 1, pincelMeta, "M", celdaAncho, celdaAlto)
            }
        }
    }

    /** Dibuja un círculo con una letra sobre el punto número `indice` de la ruta. */
    private fun marcar(lienzo: Canvas, indice: Int, pincel: Paint, letra: String,
                       celdaAncho: Float, celdaAlto: Float) {
        val centroX = (ruta[2 * indice] + 0.5f) * celdaAncho
        val centroY = (ruta[2 * indice + 1] + 0.5f) * celdaAlto
        val radio = minOf(celdaAncho, celdaAlto) * 0.35f
        pincelLetra.textSize = radio * 1.4f
        lienzo.drawCircle(centroX, centroY, radio, pincel)
        lienzo.drawText(letra, centroX, centroY + pincelLetra.textSize * 0.35f, pincelLetra)
    }
}
