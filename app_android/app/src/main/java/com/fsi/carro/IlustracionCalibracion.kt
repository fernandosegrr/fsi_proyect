/*
 * IlustracionCalibracion.kt — El dibujo simple de cada paso de la calibración (el carro visto desde arriba).
 *
 * GUÍA DE LECTURA
 *  1. Es una View propia: todo se dibuja con Canvas y Paint, sin imágenes. Los colores salen de los tokens del tema (colors.xml y values-night/colors.xml),
 *     así que se ve bien en claro y en oscuro y no hay hexadecimales sueltos.
 *  2. La pantalla solo le dice dos cosas: `escena` (qué paso es) y `enMarcha` (la prueba está corriendo ahora).
 *  3. Escenas: INTRO (el carro con su espacio libre), REACCION y ARRANQUE (cronómetro y dedo), AVANCE (flecha hacia adelante y una regla en cm),
 *     FRENADO (el carro pasa la raya de STOP y sigue un poco más), GIRO_IZQ y GIRO_DER (arco de 90 grados), RETROCESO (flecha hacia atrás), GIRO_VISUAL (el carro girando y un ojo/cámara),
 *     DESPLAZAMIENTO (el carro donde empezó, girado donde terminó y una flecha entre las marcas de cinta de su centro) y RESUMEN (una palomita).
 *  4. El carro se dibuja de frente hacia ARRIBA: un rectángulo redondeado con 4 ruedas y una franja del acento al frente (ahí va la cámara).
 *  5. Con `enMarcha` en true corre una animación suave (fase de 0 a 1 cada 1.4 s): el carro avanza, retrocede o gira, el cronómetro da vueltas.
 *     Solo anima mientras la prueba corre y se detiene al salir de la pantalla (onDetachedFromWindow).
 *  6. Todas las medidas son proporcionales al alto de la vista (u): se ve igual en cualquier tamaño de pantalla.
 */
package com.fsi.carro

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.util.AttributeSet
import android.util.TypedValue
import android.view.View
import android.view.animation.LinearInterpolator
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat

class IlustracionCalibracion @JvmOverloads constructor(contexto: Context, atributos: AttributeSet? = null) : View(contexto, atributos) {

    /** Una escena por paso del asistente (mismos nombres que los pasos de CalibracionActivity). */
    enum class Escena { INTRO, REACCION, ARRANQUE, AVANCE, FRENADO, GIRO_IZQ, GIRO_DER, RETROCESO, GIRO_VISUAL, RESUMEN, GIRO_90, RECTA, DESPLAZAMIENTO }

    /** Qué paso se dibuja. */
    var escena: Escena = Escena.INTRO
        set(valor) {
            if (field == valor) {
                return
            }
            field = valor
            contentDescription = descripcion(valor)
            invalidate()
        }

    /** true mientras la prueba corre: el dibujo se anima (el carro se mueve o gira). */
    var enMarcha: Boolean = false
        set(valor) {
            if (field != valor) {
                field = valor
                actualizarAnimacion()
                invalidate()
            }
        }

    private var fase = 0f
    private var animador: ValueAnimator? = null

    init {
        contentDescription = descripcion(escena)
    }

    /** La frase que lee el lector de pantalla para cada escena (en textos_vision_cal.xml). */
    private fun descripcion(valor: Escena): String = resources.getString(
        when (valor) {
            Escena.INTRO -> R.string.cal_ilus_intro
            Escena.REACCION -> R.string.cal_ilus_reaccion
            Escena.ARRANQUE -> R.string.cal_ilus_arranque
            Escena.AVANCE -> R.string.cal_ilus_avance
            Escena.FRENADO -> R.string.cal_ilus_frenado
            Escena.DESPLAZAMIENTO -> R.string.cal_ilus_desplazamiento
            Escena.GIRO_IZQ -> R.string.cal_ilus_giro_izq
            Escena.GIRO_DER -> R.string.cal_ilus_giro_der
            Escena.RETROCESO -> R.string.cal_ilus_retroceso
            Escena.GIRO_VISUAL -> R.string.cal_ilus_giro_visual
            Escena.RESUMEN -> R.string.cal_ilus_resumen
            Escena.GIRO_90 -> R.string.cal_ilus_giro_90
            Escena.RECTA -> R.string.cal_ilus_recta
        }
    )

    // ── Pinceles (colores = tokens del tema) ──────────────────────────────

    private val colorFondoPanel = ContextCompat.getColor(contexto, R.color.superficie)
    private val colorBordePanel = ContextCompat.getColor(contexto, R.color.borde)
    private val colorCarro = ContextCompat.getColor(contexto, R.color.superficie_alta)
    private val colorContorno = ContextCompat.getColor(contexto, R.color.borde_fuerte)
    private val colorRueda = ContextCompat.getColor(contexto, R.color.texto_suave)
    private val colorAcento = ContextCompat.getColor(contexto, R.color.acento_texto)
    private val colorOk = ContextCompat.getColor(contexto, R.color.ok)
    private val colorOkFondo = ContextCompat.getColor(contexto, R.color.ok_fondo)

    private val pincelPanel = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; color = colorFondoPanel }
    private val pincelBordePanel = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = colorBordePanel; strokeWidth = dp(1f) }
    private val pincelRelleno = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val pincelLinea = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND }
    private val pincelTexto = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = colorRueda
        textAlign = Paint.Align.CENTER
        textSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 14f, resources.displayMetrics)
        typeface = ResourcesCompat.getFont(contexto, R.font.inter_medium)
    }
    private val camino = Path()
    private val rect = RectF()

    private fun dp(valor: Float): Float = valor * resources.displayMetrics.density

    // ── Animación ─────────────────────────────────────────────────────────

    private fun actualizarAnimacion() {
        animador?.cancel()
        animador = null
        fase = 0f
        if (enMarcha && isAttachedToWindow) {
            animador = ValueAnimator.ofFloat(0f, 1f).apply {
                duration = 1400
                repeatCount = ValueAnimator.INFINITE
                interpolator = LinearInterpolator()
                addUpdateListener {
                    fase = it.animatedValue as Float
                    invalidate()
                }
                start()
            }
        }
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        actualizarAnimacion()
    }

    override fun onDetachedFromWindow() {
        animador?.cancel()
        animador = null
        super.onDetachedFromWindow()
    }

    // ── Dibujo ────────────────────────────────────────────────────────────

    override fun onDraw(lienzo: Canvas) {
        val ancho = width.toFloat()
        val alto = height.toFloat()
        val u = alto                      // unidad: el alto de la vista
        val cx = ancho / 2f
        val cy = alto / 2f

        // Panel de fondo con esquinas de 16 dp y borde fino (como las tarjetas).
        val mitad = pincelBordePanel.strokeWidth / 2f
        rect.set(mitad, mitad, ancho - mitad, alto - mitad)
        lienzo.drawRoundRect(rect, dp(16f), dp(16f), pincelPanel)
        lienzo.drawRoundRect(rect, dp(16f), dp(16f), pincelBordePanel)

        when (escena) {
            Escena.INTRO -> escenaIntro(lienzo, cx, cy, u)
            Escena.REACCION -> escenaReaccion(lienzo, cx, cy, u)
            Escena.ARRANQUE -> escenaArranque(lienzo, cx, cy, u)
            Escena.AVANCE -> escenaAvance(lienzo, cx, u)
            Escena.FRENADO -> escenaFrenado(lienzo, cx, u)
            Escena.DESPLAZAMIENTO -> escenaDesplazamiento(lienzo, cx, cy, u)
            Escena.RETROCESO -> escenaRetroceso(lienzo, cx, u)
            Escena.GIRO_IZQ -> escenaGiro(lienzo, cx, cy, u, -1f)
            Escena.GIRO_DER -> escenaGiro(lienzo, cx, cy, u, 1f)
            Escena.GIRO_VISUAL -> escenaGiroVisual(lienzo, cx, cy, u)
            Escena.RESUMEN -> escenaResumen(lienzo, cx, cy, u)
            Escena.GIRO_90 -> escenaGiro(lienzo, cx, cy, u, 1f)
            Escena.RECTA -> escenaAvance(lienzo, cx, u)
        }
    }

    // ── Las escenas ───────────────────────────────────────────────────────

    /** INTRO: el carro en el centro y, a su alrededor, un círculo de rayas: el espacio libre que hace falta. */
    private fun escenaIntro(c: Canvas, cx: Float, cy: Float, u: Float) {
        pincelLinea.color = colorContorno
        pincelLinea.strokeWidth = dp(2f)
        pincelLinea.pathEffect = DashPathEffect(floatArrayOf(dp(6f), dp(6f)), 0f)
        c.drawCircle(cx, cy, 0.44f * u, pincelLinea)
        pincelLinea.pathEffect = null
        dibujarCarro(c, cx, cy, u, 1f)
    }

    /** REACCION: un cronómetro grande y el dedo (círculo con ondas) que lo toca. */
    private fun escenaReaccion(c: Canvas, cx: Float, cy: Float, u: Float) {
        dibujarCronometro(c, cx - 0.12f * u, cy + 0.03f * u, 0.30f * u)
        dibujarDedo(c, cx + 0.34f * u, cy + 0.18f * u, u)
    }

    /** ARRANQUE: el carro con rayas de movimiento detrás, un cronómetro chico y el dedo. */
    private fun escenaArranque(c: Canvas, cx: Float, cy: Float, u: Float) {
        val carroX = cx - 0.42f * u
        val avance = if (enMarcha) fase * 0.06f * u else 0f
        // Rayas de movimiento (detrás = abajo del carro).
        pincelLinea.color = colorContorno
        pincelLinea.strokeWidth = dp(2.5f)
        for (i in -1..1) {
            val x = carroX + i * 0.07f * u
            c.drawLine(x, cy + 0.30f * u, x, cy + 0.42f * u, pincelLinea)
        }
        dibujarCarro(c, carroX, cy - avance, u, 0.9f)
        dibujarCronometro(c, cx + 0.14f * u, cy - 0.02f * u, 0.22f * u)
        dibujarDedo(c, cx + 0.48f * u, cy + 0.22f * u, u * 0.8f)
    }

    /** AVANCE: el carro, una flecha hacia adelante hasta una raya de meta y una regla con marcas de «cm». */
    private fun escenaAvance(c: Canvas, cx: Float, u: Float) {
        val carroX = cx - 0.22f * u
        val carroY = 0.70f * u
        val frente = carroY - 0.25f * u
        val metaY = 0.12f * u
        dibujarRegla(c, cx + 0.22f * u, metaY, 0.88f * u, u)
        // Raya de meta (de rayas) hasta la regla.
        pincelLinea.color = colorContorno
        pincelLinea.strokeWidth = dp(2f)
        pincelLinea.pathEffect = DashPathEffect(floatArrayOf(dp(5f), dp(5f)), 0f)
        c.drawLine(carroX - 0.2f * u, metaY, cx + 0.2f * u, metaY, pincelLinea)
        pincelLinea.pathEffect = null
        dibujarFlecha(c, carroX, frente - 0.03f * u, carroX, metaY, u)
        val corrimiento = if (enMarcha) fase * 0.24f * u else 0f
        dibujarCarro(c, carroX, carroY - corrimiento, u, 1f)
    }

    /** FRENADO: el carro avanza, pasa una raya de «STOP» (donde se le ordena detenerse) y sigue un poco más: esa distancia extra, entre la raya y la punta, es el frenado. */
    private fun escenaFrenado(c: Canvas, cx: Float, u: Float) {
        val carroX = cx - 0.22f * u
        val rayaY = 0.46f * u
        val frenadoY = 0.26f * u                       // hasta aquí llega el frente del carro después de la raya (con el carro quieto)
        val reposo = frenadoY + 0.25f * u              // centro del carro cuando se detiene del todo
        val salida = 0.80f * u                         // centro del carro al salir
        pincelLinea.color = colorContorno
        pincelLinea.strokeWidth = dp(2f)
        pincelLinea.pathEffect = DashPathEffect(floatArrayOf(dp(5f), dp(5f)), 0f)
        c.drawLine(carroX - 0.24f * u, rayaY, cx + 0.06f * u, rayaY, pincelLinea)
        pincelLinea.pathEffect = null
        pincelTexto.color = colorRueda
        c.drawText("STOP", carroX - 0.34f * u, rayaY + pincelTexto.textSize / 3f, pincelTexto)
        dibujarFlecha(c, cx + 0.04f * u, rayaY, cx + 0.04f * u, frenadoY, u)               // el frenado: de la raya a donde queda el frente
        val posicion = if (enMarcha) salida + (reposo - salida) * fase else reposo
        dibujarCarro(c, carroX, posicion, u, 1f)
        dibujarRegla(c, cx + 0.26f * u, 0.10f * u, 0.90f * u, u)
    }

    /** DESPLAZAMIENTO: el carro (fantasma) donde empezó, el carro girado donde terminó, y una flecha entre las dos marcas de cinta del centro: ese es el desplazamiento del centro. */
    private fun escenaDesplazamiento(c: Canvas, cx: Float, cy: Float, u: Float) {
        val x0 = cx - 0.30f * u
        val y0 = cy + 0.12f * u
        val x1 = cx + 0.24f * u
        val y1 = cy - 0.10f * u
        // El carro donde empezó, atenuado.
        c.saveLayerAlpha(0f, 0f, width.toFloat(), height.toFloat(), 90)
        dibujarCarro(c, x0, y0, u, 0.8f)
        c.restore()
        // El carro girado 90° donde terminó (si la prueba corre, se mueve y gira de una posición a la otra).
        val avance = if (enMarcha) fase else 1f
        val x = x0 + (x1 - x0) * avance
        val y = y0 + (y1 - y0) * avance
        c.save()
        c.rotate(90f * avance, x, y)
        dibujarCarro(c, x, y, u, 0.8f)
        c.restore()
        // Las marcas de cinta del centro y la flecha entre ellas.
        dibujarFlecha(c, x0 + 0.04f * u, y0 - 0.03f * u, x1 - 0.05f * u, y1 + 0.02f * u, u)
        pincelRelleno.color = colorAcento
        c.drawCircle(x0, y0, dp(5f), pincelRelleno)
        c.drawCircle(x1, y1, dp(5f), pincelRelleno)
    }

    /** RETROCESO: igual que el avance pero la flecha sale de atrás del carro, hacia abajo. */
    private fun escenaRetroceso(c: Canvas, cx: Float, u: Float) {
        val carroX = cx - 0.22f * u
        val carroY = 0.32f * u
        val atras = carroY + 0.25f * u
        val metaY = 0.90f * u
        dibujarRegla(c, cx + 0.22f * u, 0.10f * u, metaY, u)
        pincelLinea.color = colorContorno
        pincelLinea.strokeWidth = dp(2f)
        pincelLinea.pathEffect = DashPathEffect(floatArrayOf(dp(5f), dp(5f)), 0f)
        c.drawLine(carroX - 0.2f * u, metaY, cx + 0.2f * u, metaY, pincelLinea)
        pincelLinea.pathEffect = null
        dibujarFlecha(c, carroX, atras + 0.03f * u, carroX, metaY, u)
        val corrimiento = if (enMarcha) fase * 0.20f * u else 0f
        dibujarCarro(c, carroX, carroY + corrimiento, u, 1f)
    }

    /** GIRO_IZQ / GIRO_DER: el carro en el centro y un arco de 90 grados con flecha hacia el lado que gira. lado = -1 izquierda, +1 derecha. */
    private fun escenaGiro(c: Canvas, cx: Float, cy: Float, u: Float, lado: Float) {
        dibujarGiro(c, cx, cy, u, lado, 1f)
        // La etiqueta «90°» al lado de la punta de la flecha.
        val r = 0.40f * u
        val y = cy + (pincelTexto.textSize / 3f)
        pincelTexto.color = colorRueda
        c.drawText("90°", cx + lado * (r + 0.16f * u), y, pincelTexto)
    }

    /** Dibuja el carro (que gira si hay prueba en marcha) con su arco de 90 grados y la flecha. `escala` achica toda la figura. */
    private fun dibujarGiro(c: Canvas, cx: Float, cy: Float, u: Float, lado: Float, escala: Float) {
        val r = 0.40f * u * escala
        // Referencia de salida (hacia adelante) y de llegada (hacia el lado), de rayas.
        pincelLinea.color = colorContorno
        pincelLinea.strokeWidth = dp(2f)
        pincelLinea.pathEffect = DashPathEffect(floatArrayOf(dp(5f), dp(5f)), 0f)
        c.drawLine(cx, cy - 0.26f * u * escala, cx, cy - r - 0.04f * u, pincelLinea)
        c.drawLine(cx + lado * 0.20f * u * escala, cy, cx + lado * (r + 0.04f * u), cy, pincelLinea)
        pincelLinea.pathEffect = null
        // El arco: sale hacia arriba (-90°) y gira 90° hacia el lado.
        pincelLinea.color = colorAcento
        pincelLinea.strokeWidth = dp(4f)
        rect.set(cx - r, cy - r, cx + r, cy + r)
        c.drawArc(rect, -90f, 90f * lado, false, pincelLinea)
        // Punta de la flecha al final del arco, apuntando hacia abajo (el sentido del giro en ese punto).
        punta(c, cx + lado * r, cy, 0f, 1f, dp(12f))
        // El carro: si la prueba corre, gira hasta 90° y repite.
        c.save()
        if (enMarcha) {
            c.rotate(90f * lado * fase, cx, cy)
        }
        dibujarCarro(c, cx, cy, u, escala)
        c.restore()
    }

    /** GIRO_VISUAL: el carro girando a la derecha y, al lado, un ojo (la cámara que mide cuánto giró). */
    private fun escenaGiroVisual(c: Canvas, cx: Float, cy: Float, u: Float) {
        val carroX = cx - 0.34f * u
        dibujarGiro(c, carroX, cy, u, 1f, 0.8f)
        // El ojo.
        val ex = cx + 0.46f * u
        val ey = cy - 0.02f * u
        val ew = 0.30f * u
        val eh = 0.17f * u
        // Línea de rayas del carro (su cámara) al ojo.
        pincelLinea.color = colorContorno
        pincelLinea.strokeWidth = dp(2f)
        pincelLinea.pathEffect = DashPathEffect(floatArrayOf(dp(5f), dp(5f)), 0f)
        c.drawLine(carroX + 0.12f * u, cy - 0.20f * u, ex - ew - 0.02f * u, ey - 0.02f * u, pincelLinea)
        pincelLinea.pathEffect = null
        camino.reset()
        camino.moveTo(ex - ew, ey)
        camino.quadTo(ex, ey - 2f * eh, ex + ew, ey)
        camino.quadTo(ex, ey + 2f * eh, ex - ew, ey)
        camino.close()
        pincelRelleno.color = colorCarro
        c.drawPath(camino, pincelRelleno)
        pincelLinea.color = colorRueda
        pincelLinea.strokeWidth = dp(3f)
        c.drawPath(camino, pincelLinea)
        pincelLinea.color = colorAcento
        pincelLinea.strokeWidth = dp(3f)
        c.drawCircle(ex, ey, eh * 0.62f, pincelLinea)
        pincelRelleno.color = colorAcento
        c.drawCircle(ex, ey, eh * 0.28f, pincelRelleno)
    }

    /** RESUMEN: una palomita verde grande y el carro chico a su lado. */
    private fun escenaResumen(c: Canvas, cx: Float, cy: Float, u: Float) {
        dibujarCarro(c, cx - 0.52f * u, cy, u, 0.7f)
        val r = 0.34f * u
        val px = cx + 0.12f * u
        pincelRelleno.color = colorOkFondo
        c.drawCircle(px, cy, r, pincelRelleno)
        pincelLinea.color = colorOk
        pincelLinea.strokeWidth = dp(3f)
        c.drawCircle(px, cy, r, pincelLinea)
        pincelLinea.strokeWidth = dp(7f)
        camino.reset()
        camino.moveTo(px - 0.14f * u, cy + 0.01f * u)
        camino.lineTo(px - 0.04f * u, cy + 0.11f * u)
        camino.lineTo(px + 0.16f * u, cy - 0.10f * u)
        c.drawPath(camino, pincelLinea)
    }

    // ── Piezas ────────────────────────────────────────────────────────────

    /** El carro visto desde arriba, centrado en (cx, cy), con el frente hacia arriba: cuerpo redondeado, 4 ruedas y la franja de la cámara. */
    private fun dibujarCarro(c: Canvas, cx: Float, cy: Float, u: Float, escala: Float) {
        val cw = 0.30f * u * escala          // ancho del cuerpo
        val ch = 0.50f * u * escala          // largo del cuerpo
        val radio = 0.06f * u * escala
        // Ruedas (detrás del cuerpo, asomando por los lados).
        val rw = 0.075f * u * escala
        val rh = 0.15f * u * escala
        pincelRelleno.color = colorRueda
        for (lado in intArrayOf(-1, 1)) {
            for (fila in intArrayOf(-1, 1)) {
                val rx = cx + lado * (cw / 2f)
                val ry = cy + fila * ch * 0.28f
                rect.set(rx - rw / 2f, ry - rh / 2f, rx + rw / 2f, ry + rh / 2f)
                c.drawRoundRect(rect, radio / 2f, radio / 2f, pincelRelleno)
            }
        }
        // Cuerpo.
        rect.set(cx - cw / 2f + rw * 0.25f, cy - ch / 2f, cx + cw / 2f - rw * 0.25f, cy + ch / 2f)
        pincelRelleno.color = colorCarro
        c.drawRoundRect(rect, radio, radio, pincelRelleno)
        pincelLinea.color = colorContorno
        pincelLinea.strokeWidth = dp(2f)
        c.drawRoundRect(rect, radio, radio, pincelLinea)
        // Franja del frente (la cámara).
        val franjaAncho = cw * 0.46f
        rect.set(cx - franjaAncho / 2f, cy - ch / 2f + ch * 0.08f, cx + franjaAncho / 2f, cy - ch / 2f + ch * 0.20f)
        pincelRelleno.color = colorAcento
        c.drawRoundRect(rect, radio / 3f, radio / 3f, pincelRelleno)
    }

    /** Una flecha del acento de (x1, y1) a (x2, y2) con la punta en el segundo punto. */
    private fun dibujarFlecha(c: Canvas, x1: Float, y1: Float, x2: Float, y2: Float, u: Float) {
        val largo = Math.hypot((x2 - x1).toDouble(), (y2 - y1).toDouble()).toFloat()
        if (largo < 1f) {
            return
        }
        val dx = (x2 - x1) / largo
        val dy = (y2 - y1) / largo
        val tam = dp(12f)
        pincelLinea.color = colorAcento
        pincelLinea.strokeWidth = dp(4f)
        c.drawLine(x1, y1, x2 - dx * tam * 0.6f, y2 - dy * tam * 0.6f, pincelLinea)
        punta(c, x2, y2, dx, dy, tam)
    }

    /** Un triángulo lleno del acento con la punta en (x, y), apuntando hacia (dx, dy) (vector unitario). */
    private fun punta(c: Canvas, x: Float, y: Float, dx: Float, dy: Float, tam: Float) {
        val nx = -dy
        val ny = dx
        camino.reset()
        camino.moveTo(x, y)
        camino.lineTo(x - dx * tam + nx * tam * 0.6f, y - dy * tam + ny * tam * 0.6f)
        camino.lineTo(x - dx * tam - nx * tam * 0.6f, y - dy * tam - ny * tam * 0.6f)
        camino.close()
        pincelRelleno.color = colorAcento
        c.drawPath(camino, pincelRelleno)
    }

    /** Una regla vertical en x, de arriba (y1) a abajo (y2), con marcas y la palabra «cm». */
    private fun dibujarRegla(c: Canvas, x: Float, y1: Float, y2: Float, u: Float) {
        val ancho = 0.11f * u
        rect.set(x, y1, x + ancho, y2)
        pincelRelleno.color = colorCarro
        c.drawRoundRect(rect, dp(4f), dp(4f), pincelRelleno)
        pincelLinea.color = colorContorno
        pincelLinea.strokeWidth = dp(2f)
        c.drawRoundRect(rect, dp(4f), dp(4f), pincelLinea)
        // Marcas: una larga cada dos cortas.
        pincelLinea.color = colorRueda
        pincelLinea.strokeWidth = dp(1.5f)
        var y = y1 + 0.06f * u
        var n = 0
        while (y < y2 - 0.03f * u) {
            val largo = if (n % 2 == 0) ancho * 0.6f else ancho * 0.35f
            c.drawLine(x, y, x + largo, y, pincelLinea)
            y += 0.075f * u
            n++
        }
        pincelTexto.color = colorRueda
        c.drawText("cm", x + ancho + 0.10f * u, (y1 + y2) / 2f + pincelTexto.textSize / 3f, pincelTexto)
    }

    /** Un cronómetro: círculo con botoncito arriba y una manecilla (que da vueltas si la prueba corre). */
    private fun dibujarCronometro(c: Canvas, x: Float, y: Float, radio: Float) {
        pincelRelleno.color = colorCarro
        c.drawCircle(x, y, radio, pincelRelleno)
        pincelLinea.color = colorRueda
        pincelLinea.strokeWidth = dp(4f)
        c.drawCircle(x, y, radio, pincelLinea)
        // Botón de arriba.
        pincelLinea.strokeWidth = dp(5f)
        c.drawLine(x, y - radio - radio * 0.28f, x, y - radio, pincelLinea)
        c.drawLine(x - radio * 0.22f, y - radio - radio * 0.28f, x + radio * 0.22f, y - radio - radio * 0.28f, pincelLinea)
        // Manecilla.
        val angulo = Math.toRadians((-60.0 + 360.0 * fase)).toFloat()
        pincelLinea.color = colorAcento
        pincelLinea.strokeWidth = dp(4f)
        c.drawLine(x, y, x + Math.cos(angulo.toDouble()).toFloat() * radio * 0.7f, y + Math.sin(angulo.toDouble()).toFloat() * radio * 0.7f, pincelLinea)
        pincelRelleno.color = colorAcento
        c.drawCircle(x, y, radio * 0.1f, pincelRelleno)
    }

    /** El «dedo»: un círculo lleno con dos ondas que se abren (se animan si la prueba corre). */
    private fun dibujarDedo(c: Canvas, x: Float, y: Float, u: Float) {
        val base = 0.07f * u
        pincelRelleno.color = colorAcento
        c.drawCircle(x, y, base, pincelRelleno)
        pincelLinea.color = colorAcento
        pincelLinea.strokeWidth = dp(2.5f)
        val onda1 = base * (1.7f + (if (enMarcha) fase * 0.4f else 0f))
        val onda2 = base * (2.6f + (if (enMarcha) fase * 0.4f else 0f))
        pincelLinea.alpha = if (enMarcha) (255 * (1f - fase)).toInt().coerceIn(60, 255) else 200
        c.drawCircle(x, y, onda1, pincelLinea)
        pincelLinea.alpha = if (enMarcha) (255 * (1f - fase) * 0.6f).toInt().coerceIn(40, 255) else 110
        c.drawCircle(x, y, onda2, pincelLinea)
        pincelLinea.alpha = 255
    }
}
