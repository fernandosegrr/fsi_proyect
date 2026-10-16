/*
 * Diseno.kt — Los componentes visuales reutilizables de la app (docs/diseno.md).
 *
 * GUÍA DE LECTURA
 *  1. Nivel: el significado fijo de los colores. OK = verde (bien), AVISO = ámbar, ERROR = rojo (paro o peligro), NEUTRO = gris.
 *  2. ChipEstado: una píldora con un punto de color y un texto corto ("Conectado", "Reintentando"...).
 *  3. PuntoConexion: un punto de color que "respira" suavemente (fundido lento) mientras hay conexión.
 *  4. AvisoBanda: un aviso en banda, con QUÉ PASÓ y QUÉ HACER (y un botón opcional, por ejemplo "Reintentar").
 *  5. Haptica: vibración muy corta en acciones importantes y en obstáculo (se puede apagar en Ajustes).
 *  6. Dialogos: diálogo de confirmación con el estilo de la app.
 *  7. Movimiento: las duraciones (150 a 250 ms) y la respuesta al tocar (la vista se achica un poco mientras se mantiene el dedo).
 *  8. Diseno: el tema (oscuro por defecto), el color de los iconos de la barra de estado y medidas en dp.
 */
package com.fsi.carro

import android.animation.ObjectAnimator
import android.animation.ValueAnimator
import android.app.Activity
import android.content.Context
import android.content.res.ColorStateList
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.util.AttributeSet
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.animation.AccelerateDecelerateInterpolator
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatDelegate
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder

/** El significado fijo de los colores: verde = bien, ámbar = aviso, rojo = paro o peligro, gris = sin información. */
enum class Nivel { OK, AVISO, ERROR, NEUTRO }

object Diseno {

    /** Medida en dp pasada a píxeles. */
    fun dp(contexto: Context, valor: Int): Int = (valor * contexto.resources.displayMetrics.density + 0.5f).toInt()

    /** Aplica el tema guardado en Ajustes: oscuro (predeterminado), claro o el del sistema. Se llama al arrancar la app. */
    fun aplicarTema(contexto: Context) {
        AppCompatDelegate.setDefaultNightMode(
            when (Ajustes(contexto).tema) {
                Ajustes.TEMA_CLARO -> AppCompatDelegate.MODE_NIGHT_NO
                Ajustes.TEMA_SISTEMA -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
                else -> AppCompatDelegate.MODE_NIGHT_YES
            }
        )
    }

    /** Iconos de la barra de estado claros u oscuros según el tema (el fondo de la barra ya es el de la app). */
    fun barras(actividad: Activity) {
        val noche = (actividad.resources.configuration.uiMode and android.content.res.Configuration.UI_MODE_NIGHT_MASK) == android.content.res.Configuration.UI_MODE_NIGHT_YES
        val controlador = WindowCompat.getInsetsController(actividad.window, actividad.window.decorView)
        controlador.isAppearanceLightStatusBars = !noche
        controlador.isAppearanceLightNavigationBars = !noche
    }

    /** El color principal de un nivel (texto, punto, icono). */
    fun colorDe(contexto: Context, nivel: Nivel): Int = ContextCompat.getColor(contexto, when (nivel) {
        Nivel.OK -> R.color.ok
        Nivel.AVISO -> R.color.aviso
        Nivel.ERROR -> R.color.peligro
        Nivel.NEUTRO -> R.color.texto_suave
    })

    /** El color de fondo (suave) de un nivel. */
    fun fondoDe(contexto: Context, nivel: Nivel): Int = ContextCompat.getColor(contexto, when (nivel) {
        Nivel.OK -> R.color.ok_fondo
        Nivel.AVISO -> R.color.aviso_fondo
        Nivel.ERROR -> R.color.peligro_fondo
        Nivel.NEUTRO -> R.color.neutro_fondo
    })
}

/** Duraciones del movimiento (150 a 250 ms) y la respuesta al tocar. Nada recargado. */
object Movimiento {
    const val RAPIDO = 150L
    const val NORMAL = 200L
    const val LENTO = 250L

    /** Mientras el dedo está sobre la vista se achica un 3 %; al soltar vuelve. No consume el toque. */
    fun respuestaAlTocar(vista: View) {
        vista.setOnTouchListener { v, evento ->
            when (evento.actionMasked) {
                MotionEvent.ACTION_DOWN -> escala(v, true)
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> escala(v, false)
            }
            false
        }
    }

    /** Achica (true) o devuelve (false) la vista; sirve también desde los botones que manejan su propio toque (la cruceta). */
    fun escala(vista: View, presionada: Boolean) {
        vista.animate().scaleX(if (presionada) 0.97f else 1f).scaleY(if (presionada) 0.97f else 1f).setDuration(RAPIDO).start()
    }

    /** Aparece con un fundido corto. */
    fun mostrar(vista: View) {
        if (vista.visibility == View.VISIBLE && vista.alpha == 1f) {
            return
        }
        vista.alpha = 0f
        vista.visibility = View.VISIBLE
        vista.animate().alpha(1f).setDuration(NORMAL).start()
    }

    /** Desaparece con un fundido corto (queda GONE). */
    fun ocultar(vista: View) {
        if (vista.visibility != View.VISIBLE) {
            return
        }
        vista.animate().alpha(0f).setDuration(RAPIDO).withEndAction { vista.visibility = View.GONE; vista.alpha = 1f }.start()
    }
}

/** Vibración corta. Si la vibración está apagada en Ajustes, no hace nada. */
object Haptica {
    /** Una vibración de 18 ms: acciones importantes (conectar, iniciar una ruta, terminar). */
    fun corta(contexto: Context) = vibrar(contexto, longArrayOf(0, 18))

    /** Dos pulsos cortos: aparece un obstáculo o hay un problema. */
    fun obstaculo(contexto: Context) = vibrar(contexto, longArrayOf(0, 30, 60, 30))

    private fun vibrar(contexto: Context, patron: LongArray) {
        if (!Ajustes(contexto).vibracion) {
            return
        }
        val vibrador = contexto.getSystemService(Vibrator::class.java) ?: return
        if (!vibrador.hasVibrator()) {
            return
        }
        if (Build.VERSION.SDK_INT >= 26) {
            vibrador.vibrate(VibrationEffect.createWaveform(patron, -1))
        } else {
            @Suppress("DEPRECATION")
            vibrador.vibrate(patron, -1)
        }
    }
}

/** Diálogo de confirmación con el estilo de la app. */
object Dialogos {
    fun confirmar(contexto: Context, titulo: String, mensaje: String, textoSi: String, alSi: () -> Unit) {
        MaterialAlertDialogBuilder(contexto)
            .setTitle(titulo)
            .setMessage(mensaje)
            .setPositiveButton(textoSi) { _, _ -> alSi() }
            .setNegativeButton(R.string.cancelar, null)
            .show()
    }

    /** Una decisión con tres botones (no se cierra tocando fuera: hay que elegir una). `textoSi` es la opción recomendada. */
    fun tresOpciones(contexto: Context, titulo: String, mensaje: String, textoSi: String, alSi: () -> Unit, textoOtra: String, alOtra: () -> Unit, textoNo: String, alNo: () -> Unit) {
        MaterialAlertDialogBuilder(contexto)
            .setTitle(titulo)
            .setMessage(mensaje)
            .setCancelable(false)
            .setPositiveButton(textoSi) { _, _ -> alSi() }
            .setNeutralButton(textoOtra) { _, _ -> alOtra() }
            .setNegativeButton(textoNo) { _, _ -> alNo() }
            .show()
    }

    /** Un aviso con un solo botón "Entendido". */
    fun avisar(contexto: Context, titulo: String, mensaje: String) {
        MaterialAlertDialogBuilder(contexto)
            .setTitle(titulo)
            .setMessage(mensaje)
            .setPositiveButton(R.string.entendido, null)
            .show()
    }
}

/** Píldora de estado: un punto de color y un texto corto. No se toca; solo informa. */
class ChipEstado @JvmOverloads constructor(contexto: Context, atributos: AttributeSet? = null) : AppCompatTextView(contexto, atributos) {

    init {
        setTextAppearance(R.style.Texto_Etiqueta)
        gravity = Gravity.CENTER_VERTICAL
        val h = Diseno.dp(contexto, 12)
        val v = Diseno.dp(contexto, 4)
        setPadding(h, v, h, v)
        minHeight = Diseno.dp(contexto, 28)
        compoundDrawablePadding = Diseno.dp(contexto, 8)
        maxLines = 1
        poner(Nivel.NEUTRO, text?.toString().orEmpty())
    }

    /** Pone el nivel (color) y el texto. */
    fun poner(nivel: Nivel, texto: String) {
        this.text = texto
        val fondo = GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            cornerRadius = Diseno.dp(context, 16).toFloat()
            setColor(Diseno.fondoDe(context, nivel))
        }
        background = fondo
        setTextColor(Diseno.colorDe(context, nivel))
        val punto = GradientDrawable().apply {
            shape = GradientDrawable.OVAL
            setColor(Diseno.colorDe(context, nivel))
            setSize(Diseno.dp(context, 8), Diseno.dp(context, 8))
        }
        setCompoundDrawablesRelativeWithIntrinsicBounds(punto, null, null, null)
    }
}

/** Punto de color de la conexión. Con el nivel OK "respira" (el brillo sube y baja despacio, ~1.6 s). */
class PuntoConexion @JvmOverloads constructor(contexto: Context, atributos: AttributeSet? = null) : View(contexto, atributos) {

    private val pincel = Paint(Paint.ANTI_ALIAS_FLAG)
    private var animador: ObjectAnimator? = null
    var nivel = Nivel.NEUTRO
        set(valor) {
            field = valor
            pincel.color = Diseno.colorDe(context, valor)
            invalidate()
            actualizarAnimacion()
        }

    init {
        pincel.color = Diseno.colorDe(contexto, Nivel.NEUTRO)
    }

    override fun onMeasure(anchoSpec: Int, altoSpec: Int) {
        val lado = Diseno.dp(context, 12)
        setMeasuredDimension(lado, lado)
    }

    override fun onDraw(lienzo: Canvas) {
        lienzo.drawCircle(width / 2f, height / 2f, width / 2f, pincel)
    }

    private fun actualizarAnimacion() {
        animador?.cancel()
        animador = null
        alpha = 1f
        if (nivel == Nivel.OK && isAttachedToWindow) {
            animador = ObjectAnimator.ofFloat(this, ALPHA, 1f, 0.35f).apply {
                duration = 1600
                repeatMode = ValueAnimator.REVERSE
                repeatCount = ValueAnimator.INFINITE
                interpolator = AccelerateDecelerateInterpolator()
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
}

/** Aviso en banda: QUÉ PASÓ (en negrita) y QUÉ HACER, con un botón opcional. Se oculta solo con ocultar(). */
class AvisoBanda @JvmOverloads constructor(contexto: Context, atributos: AttributeSet? = null) : LinearLayout(contexto, atributos) {

    private val icono = ImageView(contexto)
    private val titulo = TextView(contexto)
    private val detalle = TextView(contexto)
    private val accion = MaterialButton(contexto, null, com.google.android.material.R.attr.materialButtonStyle)

    init {
        orientation = HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        val e2 = Diseno.dp(contexto, 16)
        setPadding(e2, Diseno.dp(contexto, 12), Diseno.dp(contexto, 8), Diseno.dp(contexto, 12))

        icono.layoutParams = LayoutParams(Diseno.dp(contexto, 24), Diseno.dp(contexto, 24)).apply { marginEnd = e2 }
        addView(icono)

        val textos = LinearLayout(contexto).apply { orientation = VERTICAL }
        titulo.setTextAppearance(R.style.Texto_Cuerpo)
        titulo.typeface = androidx.core.content.res.ResourcesCompat.getFont(contexto, R.font.inter_semibold)
        detalle.setTextAppearance(R.style.Texto_Suave)
        detalle.setTextColor(ContextCompat.getColor(contexto, R.color.texto))
        textos.addView(titulo)
        textos.addView(detalle)
        addView(textos, LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))

        accion.setTextAppearance(R.style.TextoBoton)
        accion.visibility = View.GONE
        addView(accion, LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, Diseno.dp(contexto, 48)))
        visibility = View.GONE
    }

    /** Muestra el aviso. `quePaso` y `queHacer` son frases cortas y claras (sin códigos crudos). `textoAccion` agrega un botón (por ejemplo "Reintentar"). */
    fun mostrar(nivel: Nivel, quePaso: String, queHacer: String, textoAccion: String? = null, alAccion: (() -> Unit)? = null) {
        val color = Diseno.colorDe(context, nivel)
        background = GradientDrawable().apply {
            cornerRadius = Diseno.dp(context, 16).toFloat()
            setColor(Diseno.fondoDe(context, nivel))
            setStroke(Diseno.dp(context, 1), color)
        }
        icono.setImageResource(if (nivel == Nivel.ERROR) R.drawable.ic_error else if (nivel == Nivel.AVISO) R.drawable.ic_warning else R.drawable.ic_info)
        icono.imageTintList = ColorStateList.valueOf(color)
        titulo.text = quePaso
        detalle.text = queHacer
        detalle.visibility = if (queHacer.isEmpty()) View.GONE else View.VISIBLE
        if (textoAccion != null && alAccion != null) {
            accion.text = textoAccion
            accion.setTextColor(color)
            accion.backgroundTintList = ColorStateList.valueOf(0)
            accion.setOnClickListener { alAccion() }
            accion.visibility = View.VISIBLE
        } else {
            accion.visibility = View.GONE
        }
        Movimiento.mostrar(this)
    }

    fun ocultar() = Movimiento.ocultar(this)

    /** El texto de "qué pasó" actual (para las pruebas y el registro). */
    fun textoQuePaso(): String = if (visibility == View.VISIBLE) titulo.text.toString() else ""
}
