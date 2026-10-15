/*
 * EditorRutaView.kt — La cuadrícula donde se dibuja la ruta (y donde se ve la simulación).
 *
 * GUÍA DE LECTURA
 *  1. La cuadrícula es RutaEditorNativa.COLUMNAS x FILAS celdas de 20 cm. La escala en cm se ve en los bordes (cada 40 cm) y en la leyenda «1 cuadro = 20 cm».
 *  2. MODO RUTA: tocar una celda vacía AGREGA un punto; tocar un punto lo SELECCIONA y arrastrarlo lo MUEVE (un arrastre entero es un solo paso de «deshacer»).
 *     MODO OBSTÁCULOS: tocar una celda pone o quita un obstáculo virtual (para probar la simulación).
 *     Toda la lógica del editor está en C (RutaEditorNativa): aquí solo se dibuja y se traducen los toques a celdas.
 *  3. Dibujo: la ruta (con los pasos intermedios), los puntos numerados, la huella del carro al inicio (18 x 28 cm), los obstáculos, y durante la simulación el carro
 *     virtual con su rastro. Después de varias corridas con ruido se dibujan los puntos finales.
 *  4. Colores con significado fijo de los tokens (acento = ruta; ámbar = obstáculo; verde/rojo = final que llegó / no llegó).
 */
package com.fsi.carro

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import android.view.animation.LinearInterpolator
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat

class EditorRutaView @JvmOverloads constructor(contexto: Context, atributos: AttributeSet? = null) : View(contexto, atributos) {

    enum class Modo { RUTA, OBSTACULOS }

    /** Se llama cuando el usuario cambió la ruta o los obstáculos (para actualizar el resumen y el borrador). */
    var alCambiar: (() -> Unit)? = null

    /** Se llama con el código de error cuando una acción no cabe (fuera, repetido, ruta demasiado larga...). */
    var alError: ((Int) -> Unit)? = null

    /** Se llama en cada paso de la animación con la muestra actual (t, x, y, rumbo, comando, estado) o null al terminar. */
    var alAnimar: ((IntArray?) -> Unit)? = null

    var modo = Modo.RUTA
    var seleccionado = -1
        private set

    private val obstaculos = ArrayList<IntArray>()   // cada uno [columna, fila]
    private var arrastrando = -1
    private var ultimaCelda = intArrayOf(-1, -1)

    // Animación de la simulación
    private var muestras: IntArray? = null
    private var animador: ValueAnimator? = null
    private var muestraActual = 0
    private var finales: List<IntArray> = emptyList()   // cada uno [x_mm, y_mm, llegó]

    private val dp = resources.displayMetrics.density
    private val pincel = Paint(Paint.ANTI_ALIAS_FLAG)
    private val letra = Paint(Paint.ANTI_ALIAS_FLAG)
    private val camino = Path()
    private val caja = RectF()

    init {
        letra.typeface = ResourcesCompat.getFont(contexto, R.font.inter_medium)
        letra.textAlign = Paint.Align.CENTER
    }

    private fun color(id: Int) = ContextCompat.getColor(context, id)

    val hayObstaculos: Boolean get() = obstaculos.isNotEmpty()

    /** Los obstáculos como [columna0, fila0, ...] para el simulador. */
    fun obstaculosComoArreglo(): IntArray {
        val salida = IntArray(obstaculos.size * 2)
        for ((i, o) in obstaculos.withIndex()) {
            salida[2 * i] = o[0]
            salida[2 * i + 1] = o[1]
        }
        return salida
    }

    fun quitarObstaculos() {
        obstaculos.clear()
        invalidate()
    }

    /** Borra el punto seleccionado (si hay). Devuelve el código del editor (0 = bien). */
    fun borrarSeleccionado(): Int {
        if (seleccionado < 0) {
            return -5   // RE_ERR_INDICE: no hay punto elegido
        }
        val codigo = RutaEditorNativa.borrarPunto(seleccionado)
        seleccionado = -1
        invalidate()
        return codigo
    }

    fun deseleccionar() {
        seleccionado = -1
        invalidate()
    }

    // ── Medidas ───────────────────────────────────────────────────────────

    override fun onMeasure(anchoSpec: Int, altoSpec: Int) {
        val ancho = minOf(MeasureSpec.getSize(anchoSpec), (400 * dp).toInt())   // en una pantalla ancha (horizontal) la cuadrícula no crece sin límite
        setMeasuredDimension(ancho, ancho * RutaEditorNativa.FILAS / RutaEditorNativa.COLUMNAS)
    }

    private fun celdaAncho() = width.toFloat() / RutaEditorNativa.COLUMNAS
    private fun celdaAlto() = height.toFloat() / RutaEditorNativa.FILAS

    // ── Toques ────────────────────────────────────────────────────────────

    override fun onTouchEvent(evento: MotionEvent): Boolean {
        if (animador != null) {
            return true   // mientras se anima la simulación no se edita
        }
        val columna = (evento.x / celdaAncho()).toInt().coerceIn(0, RutaEditorNativa.COLUMNAS - 1)
        val fila = (evento.y / celdaAlto()).toInt().coerceIn(0, RutaEditorNativa.FILAS - 1)
        when (evento.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)   // la cuadrícula no debe desplazar la pantalla
                ultimaCelda = intArrayOf(columna, fila)
                if (modo == Modo.OBSTACULOS) {
                    alternarObstaculo(columna, fila)
                } else {
                    val v = RutaEditorNativa.vertices()
                    var encontrado = -1
                    for (i in 0 until v.size / 2) {
                        if (v[2 * i] == columna && v[2 * i + 1] == fila) {
                            encontrado = i
                        }
                    }
                    if (encontrado >= 0) {
                        seleccionado = encontrado
                        arrastrando = encontrado
                    } else {
                        val codigo = RutaEditorNativa.agregar(columna, fila)
                        if (codigo != 0) {
                            alError?.invoke(codigo)
                        } else {
                            seleccionado = RutaEditorNativa.vertices().size / 2 - 1
                            alCambiar?.invoke()
                        }
                    }
                }
                invalidate()
            }
            MotionEvent.ACTION_MOVE -> {
                if (modo == Modo.RUTA && arrastrando >= 0 && (columna != ultimaCelda[0] || fila != ultimaCelda[1])) {
                    ultimaCelda = intArrayOf(columna, fila)
                    val codigo = RutaEditorNativa.mover(arrastrando, columna, fila)
                    if (codigo != 0) {
                        alError?.invoke(codigo)
                    } else {
                        alCambiar?.invoke()
                    }
                    invalidate()
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (arrastrando >= 0) {
                    RutaEditorNativa.finArrastre()
                    arrastrando = -1
                }
            }
        }
        return true
    }

    private fun alternarObstaculo(columna: Int, fila: Int) {
        val i = obstaculos.indexOfFirst { it[0] == columna && it[1] == fila }
        if (i >= 0) {
            obstaculos.removeAt(i)
        } else if (obstaculos.size < SimRutaMaxObstaculos) {
            obstaculos.add(intArrayOf(columna, fila))
        }
        finales = emptyList()
        alCambiar?.invoke()
    }

    // ── Animación de la simulación ────────────────────────────────────────

    /** Anima el recorrido (muestras de SimRutaNativa.muestras()) al doble de la velocidad real. */
    fun animar(datos: IntArray) {
        detenerAnimacion()
        finales = emptyList()
        muestras = datos
        val n = datos.size / SimRutaNativa.M_PASO
        if (n < 2) {
            muestras = null
            invalidate()
            return
        }
        val duracionMs = datos[(n - 1) * SimRutaNativa.M_PASO].toLong()
        animador = ValueAnimator.ofFloat(0f, (n - 1).toFloat()).apply {
            duration = (duracionMs / 2).coerceAtLeast(1200)
            interpolator = LinearInterpolator()
            addUpdateListener {
                muestraActual = (it.animatedValue as Float).toInt()
                alAnimar?.invoke(muestraEn(muestraActual))
                invalidate()
            }
            addListener(object : android.animation.AnimatorListenerAdapter() {
                override fun onAnimationEnd(animation: android.animation.Animator) {
                    animador = null
                    alAnimar?.invoke(null)
                    invalidate()
                }
            })
            start()
        }
    }

    private fun muestraEn(i: Int): IntArray? {
        val d = muestras ?: return null
        if (i < 0 || (i + 1) * SimRutaNativa.M_PASO > d.size) {
            return null
        }
        return d.copyOfRange(i * SimRutaNativa.M_PASO, (i + 1) * SimRutaNativa.M_PASO)
    }

    fun detenerAnimacion() {
        animador?.cancel()
        animador = null
    }

    /** Quita el carro virtual y su rastro. */
    fun limpiarSimulacion() {
        detenerAnimacion()
        muestras = null
        finales = emptyList()
        invalidate()
    }

    /** Dibuja los puntos finales de varias corridas (cada uno [x_mm, y_mm, llegó]). */
    fun mostrarFinales(puntos: List<IntArray>) {
        detenerAnimacion()
        finales = puntos
        invalidate()
    }

    override fun onDetachedFromWindow() {
        detenerAnimacion()
        super.onDetachedFromWindow()
    }

    // ── Dibujo ────────────────────────────────────────────────────────────

    private fun px(columnaMm: Int) = (columnaMm / (RutaEditorNativa.CELDA_CM * 10f) + 0.5f) * celdaAncho()
    private fun py(filaMm: Int) = (filaMm / (RutaEditorNativa.CELDA_CM * 10f) + 0.5f) * celdaAlto()

    override fun onDraw(lienzo: Canvas) {
        val cw = celdaAncho()
        val ch = celdaAlto()
        val columnas = RutaEditorNativa.COLUMNAS
        val filas = RutaEditorNativa.FILAS

        // Fondo y cuadrícula
        caja.set(0f, 0f, width.toFloat(), height.toFloat())
        pincel.style = Paint.Style.FILL
        pincel.color = color(R.color.superficie)
        lienzo.drawRoundRect(caja, 16 * dp, 16 * dp, pincel)
        pincel.style = Paint.Style.STROKE
        pincel.strokeWidth = 1f * dp
        pincel.color = color(R.color.borde)
        for (c in 1 until columnas) {
            lienzo.drawLine(c * cw, 0f, c * cw, height.toFloat(), pincel)
        }
        for (f in 1 until filas) {
            lienzo.drawLine(0f, f * ch, width.toFloat(), f * ch, pincel)
        }
        pincel.color = color(R.color.borde_fuerte)
        lienzo.drawRoundRect(caja, 16 * dp, 16 * dp, pincel)

        // Escala en cm: cada 40 cm en los dos bordes
        letra.textSize = 10 * dp
        letra.color = color(R.color.texto_suave)
        letra.textAlign = Paint.Align.LEFT
        for (c in 2 until columnas step 2) {
            lienzo.drawText("${c * RutaEditorNativa.CELDA_CM}", c * cw + 3 * dp, height - 5 * dp, letra)
        }
        for (f in 2 until filas step 2) {
            lienzo.drawText("${f * RutaEditorNativa.CELDA_CM}", 4 * dp, f * ch + 12 * dp, letra)
        }
        letra.textAlign = Paint.Align.RIGHT
        lienzo.drawText("1 cuadro = ${RutaEditorNativa.CELDA_CM} cm", width - 10 * dp, 16 * dp, letra)
        letra.textAlign = Paint.Align.CENTER

        // Obstáculos virtuales
        for (o in obstaculos) {
            caja.set(o[0] * cw + 3 * dp, o[1] * ch + 3 * dp, (o[0] + 1) * cw - 3 * dp, (o[1] + 1) * ch - 3 * dp)
            pincel.style = Paint.Style.FILL
            pincel.color = color(R.color.aviso_fondo)
            lienzo.drawRoundRect(caja, 6 * dp, 6 * dp, pincel)
            pincel.style = Paint.Style.STROKE
            pincel.strokeWidth = 2 * dp
            pincel.color = color(R.color.aviso)
            lienzo.drawRoundRect(caja, 6 * dp, 6 * dp, pincel)
            lienzo.drawLine(caja.left + 8 * dp, caja.top + 8 * dp, caja.right - 8 * dp, caja.bottom - 8 * dp, pincel)
            lienzo.drawLine(caja.right - 8 * dp, caja.top + 8 * dp, caja.left + 8 * dp, caja.bottom - 8 * dp, pincel)
        }

        val v = RutaEditorNativa.vertices()
        val celdas = RutaEditorNativa.celdas()

        // La ruta (con los pasos intermedios)
        if (celdas.size >= 4) {
            camino.reset()
            for (i in 0 until celdas.size / 2) {
                val x = (celdas[2 * i] + 0.5f) * cw
                val y = (celdas[2 * i + 1] + 0.5f) * ch
                if (i == 0) camino.moveTo(x, y) else camino.lineTo(x, y)
            }
            pincel.style = Paint.Style.STROKE
            pincel.strokeWidth = 4 * dp
            pincel.strokeCap = Paint.Cap.ROUND
            pincel.strokeJoin = Paint.Join.ROUND
            pincel.color = color(R.color.acento)
            lienzo.drawPath(camino, pincel)
        }

        // La huella del carro al inicio (18 x 28 cm, mirando hacia arriba)
        if (v.size >= 2) {
            val cx = (v[0] + 0.5f) * cw
            val cy = (v[1] + 0.5f) * ch
            val medioAncho = 9f / RutaEditorNativa.CELDA_CM * cw
            val medioAlto = 14f / RutaEditorNativa.CELDA_CM * ch
            caja.set(cx - medioAncho, cy - medioAlto, cx + medioAncho, cy + medioAlto)
            pincel.style = Paint.Style.FILL
            pincel.color = color(R.color.superficie_alta)
            lienzo.drawRoundRect(caja, 4 * dp, 4 * dp, pincel)
            pincel.style = Paint.Style.STROKE
            pincel.strokeWidth = 1.5f * dp
            pincel.color = color(R.color.texto_suave)
            lienzo.drawRoundRect(caja, 4 * dp, 4 * dp, pincel)
            pincel.style = Paint.Style.FILL
            lienzo.drawLine(cx, cy - medioAlto, cx, cy - medioAlto - 3 * dp, pincel)   // la punta: hacia dónde mira
        }

        // Los puntos numerados
        for (i in 0 until v.size / 2) {
            val cx = (v[2 * i] + 0.5f) * cw
            val cy = (v[2 * i + 1] + 0.5f) * ch
            val radio = 11 * dp
            pincel.style = Paint.Style.FILL
            pincel.color = if (i == 0) color(R.color.ok) else color(R.color.acento)
            lienzo.drawCircle(cx, cy, radio, pincel)
            if (i == seleccionado) {
                pincel.style = Paint.Style.STROKE
                pincel.strokeWidth = 2.5f * dp
                pincel.color = color(R.color.texto)
                lienzo.drawCircle(cx, cy, radio + 3 * dp, pincel)
            }
            letra.textSize = 12 * dp
            letra.color = color(if (i == 0) R.color.fondo else R.color.sobre_acento)
            lienzo.drawText("${i + 1}", cx, cy + 4.2f * dp, letra)
        }

        // La simulación: rastro y carro virtual
        val d = muestras
        if (d != null && d.size >= 2 * SimRutaNativa.M_PASO) {
            val hasta = if (animador != null) muestraActual else d.size / SimRutaNativa.M_PASO - 1
            camino.reset()
            for (i in 0..hasta) {
                val x = px(d[i * SimRutaNativa.M_PASO + 1])
                val y = py(d[i * SimRutaNativa.M_PASO + 2])
                if (i == 0) camino.moveTo(x, y) else camino.lineTo(x, y)
            }
            pincel.style = Paint.Style.STROKE
            pincel.strokeWidth = 2 * dp
            pincel.color = color(R.color.ok)
            lienzo.drawPath(camino, pincel)
            val m = muestraEn(hasta)
            if (m != null) {
                lienzo.save()
                lienzo.rotate(m[3].toFloat(), px(m[1]), py(m[2]))
                val medioAncho = 9f / RutaEditorNativa.CELDA_CM * cw
                val medioAlto = 14f / RutaEditorNativa.CELDA_CM * ch
                caja.set(px(m[1]) - medioAncho, py(m[2]) - medioAlto, px(m[1]) + medioAncho, py(m[2]) + medioAlto)
                pincel.style = Paint.Style.FILL
                pincel.color = color(R.color.ok)
                lienzo.drawRoundRect(caja, 4 * dp, 4 * dp, pincel)
                pincel.color = color(R.color.fondo)
                lienzo.drawRect(caja.left + 2 * dp, caja.top, caja.right - 2 * dp, caja.top + 4 * dp, pincel)   // el frente del carro
                lienzo.restore()
            }
        }

        // Puntos finales de varias corridas
        for (f in finales) {
            pincel.style = Paint.Style.FILL
            pincel.color = color(if (f[2] == 1) R.color.ok else R.color.peligro)
            lienzo.drawCircle(px(f[0]), py(f[1]), 5 * dp, pincel)
        }
    }

    companion object {
        const val SimRutaMaxObstaculos = 16
    }
}
