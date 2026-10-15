/*
 * VistaNumerosDeteccion.kt — Dibuja ENCIMA del video solo los números de la detección: una píldora por zona (izquierda, centro, derecha)
 * con su cercanía (0 = nada, 1 = lejos, 2 = media, 3 = muy cerca) y, si el corredor está encendido, si el obstáculo está FUERA de la huella del carro.
 *
 * GUÍA DE LECTURA
 *  1. poner(cuadro) recibe lo último que analizó el detector (CuadroAnalizado) o null si no hay.
 *  2. Colores con significado fijo: gris = sin obstáculo, ámbar = lejos o fuera del corredor (solo avisa), rojo = cerca o dentro del camino del carro.
 *  3. No toca la imagen del video: es otra vista transparente puesta encima.
 */
package com.fsi.carro

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import androidx.core.content.res.ResourcesCompat

class VistaNumerosDeteccion @JvmOverloads constructor(contexto: Context, atributos: AttributeSet? = null) : View(contexto, atributos) {

    private val cercania = IntArray(3)
    private val fuera = BooleanArray(3)
    private var hayDatos = false
    private var corredorActivo = false
    private val relleno = Paint(Paint.ANTI_ALIAS_FLAG)
    private val letra = Paint(Paint.ANTI_ALIAS_FLAG)
    private val caja = RectF()

    init {
        letra.typeface = ResourcesCompat.getFont(contexto, R.font.inter_semibold)
        letra.textAlign = Paint.Align.CENTER
        letra.textSize = Diseno.dp(contexto, 14).toFloat()
    }

    /** Pone lo último que vio el detector (null = no hay nada todavía). */
    fun poner(cuadro: CuadroAnalizado?) {
        if (cuadro == null || cuadro.resultado[ObstaculosNativa.CODIGO] != 0) {
            hayDatos = false
        } else {
            val z = cuadro.zonas
            hayDatos = true
            cercania[0] = if (z[ObstaculosNativa.HAY_IZQUIERDA] == 1) z[ObstaculosNativa.CERCANIA_IZQUIERDA] else 0
            cercania[1] = if (z[ObstaculosNativa.HAY_CENTRO] == 1) z[ObstaculosNativa.CERCANIA_CENTRO] else 0
            cercania[2] = if (z[ObstaculosNativa.HAY_DERECHA] == 1) z[ObstaculosNativa.CERCANIA_DERECHA] else 0
            val c = cuadro.corredor
            corredorActivo = c != null && c[CorredorNativa.CODIGO] == 0
            for (i in 0..2) {
                fuera[i] = corredorActivo && c!![CorredorNativa.FUERA + i] == 1
            }
        }
        invalidate()
    }

    override fun onDraw(lienzo: Canvas) {
        val margen = Diseno.dp(context, 8).toFloat()
        val alto = Diseno.dp(context, 32).toFloat()
        val ancho = (width - 4 * margen) / 3f
        val nombres = arrayOf("IZQ", "CEN", "DER")
        for (i in 0..2) {
            val nivel = when {
                !hayDatos || cercania[i] == 0 -> Nivel.NEUTRO
                fuera[i] || cercania[i] == 1 -> Nivel.AVISO
                else -> Nivel.ERROR
            }
            val izquierda = margen + i * (ancho + margen)
            caja.set(izquierda, height - margen - alto, izquierda + ancho, height - margen)
            relleno.color = Diseno.fondoDe(context, nivel)
            lienzo.drawRoundRect(caja, alto / 2, alto / 2, relleno)
            letra.color = Diseno.colorDe(context, nivel)
            val texto = if (!hayDatos) "${nombres[i]} —" else nombres[i] + " " + cercania[i] + (if (fuera[i]) " fuera" else "")
            lienzo.drawText(texto, caja.centerX(), caja.centerY() - (letra.descent() + letra.ascent()) / 2, letra)
        }
        if (corredorActivo) {
            letra.color = Diseno.colorDe(context, Nivel.NEUTRO)
            lienzo.drawText("corredor", width / 2f, height - margen - alto - margen / 2, letra)
        }
    }
}
