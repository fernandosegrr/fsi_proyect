/*
 * VistaDeteccion.kt — Dibuja el cuadro de la cámara con lo que detecta el detector de obstáculos.
 *
 * GUÍA DE LECTURA
 *  1. Es una vista propia (View). mostrar() recibe el último CuadroAnalizado y pide redibujar.
 *  2. El tamaño del cuadro NO es fijo: cada CuadroAnalizado trae su ancho, alto, columnas y filas
 *     (320x240, 240x176, 160x120...). Si cambia, el bitmap interno se rehace.
 *  3. onDraw() dibuja, en este orden: la imagen (lo que realmente ve el detector, así que sirve de
 *     "vista previa"), la cuadrícula de celdas, las celdas "no piso" en rojo semitransparente, las
 *     filas IGNORADAS en gris (arriba de la fila mínima) con una línea blanca en el límite, la
 *     franja de referencia del piso en azul (el detector dice dónde cayó), y las tres zonas
 *     (izquierda, centro, derecha, de la fila mínima hacia abajo) con su estado y su cercanía.
 *  4. La zona es verde si está libre y roja si tiene obstáculo; el número es la cercanía
 *     (1 = lejos, 2 = media, 3 = muy cerca). Si se usa el modelo de objetos, las zonas son las FUSIONADAS
 *     (detector + modelo) y encima se dibujan en celeste las cajas del modelo con su nombre y confianza.
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

    private var imagen = Bitmap.createBitmap(ObstaculosNativa.ANCHO_CAMARA, ObstaculosNativa.ALTO_CAMARA, Bitmap.Config.ARGB_8888)
    private var cuadro: CuadroAnalizado? = null
    private var mensaje = "Sin imagen todavía"

    private val pincelImagen = Paint().apply { isFilterBitmap = true }
    private val pincelRojo = Paint().apply { color = Color.argb(120, 255, 0, 0) }
    private val pincelGris = Paint().apply { color = Color.argb(170, 110, 110, 110) }
    private val pincelLimite = Paint().apply { color = Color.WHITE; strokeWidth = 5f }
    private val pincelAzul = Paint().apply { color = Color.argb(110, 0, 90, 255) }
    private val pincelRejilla = Paint().apply { color = Color.argb(70, 255, 255, 255); strokeWidth = 1f }
    private val pincelBorde = Paint().apply { style = Paint.Style.STROKE; strokeWidth = 4f }
    private val pincelFondoTexto = Paint().apply { color = Color.argb(170, 0, 0, 0) }
    private val pincelTexto = Paint().apply { color = Color.WHITE; textAlign = Paint.Align.CENTER; isFakeBoldText = true }
    private val pincelCaja = Paint().apply { color = Color.CYAN; style = Paint.Style.STROKE; strokeWidth = 3f }
    private val pincelNombreCaja = Paint().apply { color = Color.CYAN; textAlign = Paint.Align.LEFT; isFakeBoldText = true; textSize = 18f }

    /** Muestra un cuadro nuevo (hay que llamarlo en el hilo principal). null = sin cuadro. */
    fun mostrar(nuevo: CuadroAnalizado?, texto: String = "") {
        cuadro = nuevo
        mensaje = texto
        if (nuevo != null) {
            if (imagen.width != nuevo.ancho || imagen.height != nuevo.alto) {
                imagen = Bitmap.createBitmap(nuevo.ancho, nuevo.alto, Bitmap.Config.ARGB_8888)   // el tamaño del cuadro cambió
            }
            imagen.setPixels(nuevo.pixeles, 0, nuevo.ancho, 0, 0, nuevo.ancho, nuevo.alto)
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

        val resultado = cuadroActual.resultado
        val columnas = cuadroActual.columnas
        val filas = cuadroActual.filas

        // La imagen ocupa todo lo que pueda manteniendo su proporción.
        val escala = Math.min(width.toFloat() / cuadroActual.ancho, height.toFloat() / cuadroActual.alto)
        val ancho = cuadroActual.ancho * escala
        val alto = cuadroActual.alto * escala
        val izquierda = (width - ancho) / 2f
        val arriba = (height - alto) / 2f
        // Las celdas son de 10x10 píxeles del cuadro; los píxeles que sobran (ancho no múltiplo de 10) no se miran.
        val celdaAncho = ObstaculosNativa.LADO_CELDA * escala
        val celdaAlto = ObstaculosNativa.LADO_CELDA * escala
        val anchoRejilla = columnas * celdaAncho
        val altoRejilla = filas * celdaAlto

        lienzo.drawBitmap(imagen, Rect(0, 0, cuadroActual.ancho, cuadroActual.alto),
            RectF(izquierda, arriba, izquierda + ancho, arriba + alto), pincelImagen)

        for (c in 0..columnas) {
            lienzo.drawLine(izquierda + c * celdaAncho, arriba, izquierda + c * celdaAncho, arriba + altoRejilla, pincelRejilla)
        }
        for (f in 0..filas) {
            lienzo.drawLine(izquierda, arriba + f * celdaAlto, izquierda + anchoRejilla, arriba + f * celdaAlto, pincelRejilla)
        }

        val hayAnalisis = resultado[ObstaculosNativa.CODIGO] == 0

        // Celdas "no piso" (rojo).
        if (hayAnalisis) {
            for (f in 0 until filas) {
                for (c in 0 until columnas) {
                    if (resultado[ObstaculosNativa.INICIO_MASCARA + f * columnas + c] != 0) {
                        lienzo.drawRect(izquierda + c * celdaAncho, arriba + f * celdaAlto,
                            izquierda + (c + 1) * celdaAncho, arriba + (f + 1) * celdaAlto, pincelRojo)
                    }
                }
            }
        }

        // Filas ignoradas (arriba de la fila mínima) en gris y una línea blanca en el límite.
        val filaMinima = if (hayAnalisis) resultado[ObstaculosNativa.FILA_MINIMA_USADA].coerceIn(0, filas)
                         else (cuadroActual.config[ObstaculosNativa.CFG_FILA_MINIMA_MIL] * filas / 1000).coerceIn(0, filas)
        val yLimite = arriba + filaMinima * celdaAlto
        if (filaMinima > 0) {
            lienzo.drawRect(izquierda, arriba, izquierda + anchoRejilla, yLimite, pincelGris)
            lienzo.drawLine(izquierda, yLimite, izquierda + anchoRejilla, yLimite, pincelLimite)
        }

        // Franja de referencia del piso (azul), tal como la ubicó el detector.
        if (hayAnalisis) {
            val colRef = resultado[ObstaculosNativa.REF_COLUMNA]
            val filaRef = resultado[ObstaculosNativa.REF_FILA]
            lienzo.drawRect(izquierda + colRef * celdaAncho, arriba + filaRef * celdaAlto,
                izquierda + (colRef + resultado[ObstaculosNativa.REF_NCOLUMNAS]) * celdaAncho,
                arriba + (filaRef + resultado[ObstaculosNativa.REF_NFILAS]) * celdaAlto, pincelAzul)
        }

        // Las tres zonas, de la fila mínima hacia abajo.
        val nombres = arrayOf("IZQ", "CENTRO", "DER")
        val zonasDecididas = cuadroActual.zonas   // las fusionadas si se usa el modelo de objetos
        val hay = intArrayOf(zonasDecididas[ObstaculosNativa.HAY_IZQUIERDA], zonasDecididas[ObstaculosNativa.HAY_CENTRO], zonasDecididas[ObstaculosNativa.HAY_DERECHA])
        val cercania = intArrayOf(zonasDecididas[ObstaculosNativa.CERCANIA_IZQUIERDA], zonasDecididas[ObstaculosNativa.CERCANIA_CENTRO], zonasDecididas[ObstaculosNativa.CERCANIA_DERECHA])
        val mitad = yLimite
        // El texto cabe en el ancho de una zona (la etiqueta más larga tiene unos 20 caracteres).
        pincelTexto.textSize = Math.max(12f, Math.min(altoRejilla / 14f, anchoRejilla / 3f / 11f))
        for (zona in 0..2) {
            val x0 = izquierda + anchoRejilla * zona / 3f
            val x1 = izquierda + anchoRejilla * (zona + 1) / 3f
            pincelBorde.color = if (hay[zona] != 0) Color.RED else Color.GREEN
            lienzo.drawRect(x0 + 3f, mitad + 3f, x1 - 3f, arriba + altoRejilla - 3f, pincelBorde)
            val etiqueta = if (!hayAnalisis) "${nombres[zona]}: ?" else
                if (hay[zona] != 0) "${nombres[zona]}: OBSTACULO ${cercania[zona]}" else "${nombres[zona]}: libre"
            // La etiqueta va arriba de la zona, pero sin salirse por abajo si la zona es muy baja.
            val yTexto = Math.min(mitad + pincelTexto.textSize * 1.3f, arriba + altoRejilla - 12f)
            val mitadAncho = pincelTexto.measureText(etiqueta) / 2f + 8f
            lienzo.drawRect((x0 + x1) / 2f - mitadAncho, yTexto - pincelTexto.textSize, (x0 + x1) / 2f + mitadAncho, yTexto + 8f, pincelFondoTexto)
            lienzo.drawText(etiqueta, (x0 + x1) / 2f, yTexto, pincelTexto)
        }

        // Cajas del modelo de objetos (celeste), si la fusión las usó.
        val modelo = cuadroActual.modelo
        val fusion = cuadroActual.fusion
        if (hayAnalisis && modelo != null && fusion != null && fusion[FusionNativa.R_MODELO_VIGENTE] == 1) {
            val n = modelo.cajas[0]
            for (i in 0 until n) {
                val base = 1 + i * FusionNativa.ENTEROS_POR_CAJA
                val x0 = izquierda + ancho * modelo.cajas[base + 2] / 1000f
                val y0 = arriba + alto * modelo.cajas[base + 3] / 1000f
                val x1 = izquierda + ancho * modelo.cajas[base + 4] / 1000f
                val y1 = arriba + alto * modelo.cajas[base + 5] / 1000f
                lienzo.drawRect(x0, y0, x1, y1, pincelCaja)
                lienzo.drawText("${modelo.nombres.getOrElse(i) { "?" }} ${modelo.cajas[base + 1] / 10}%", x0 + 4f, y0 + 20f, pincelNombreCaja)
            }
        }

        if (!hayAnalisis) {
            val texto = when (resultado[ObstaculosNativa.CODIGO]) {
                ObstaculosNativa.ERR_REFERENCIA -> "El piso de referencia (azul) no es uniforme"
                ObstaculosNativa.ERR_CONFIG -> "Configuración no válida"
                ObstaculosNativa.ERR_TAMANO -> "Tamaño de imagen no válido (${cuadroActual.ancho}x${cuadroActual.alto})"
                else -> "Error ${resultado[ObstaculosNativa.CODIGO]}"
            }
            val mitadAncho = pincelTexto.measureText(texto) / 2f + 10f
            lienzo.drawRect(izquierda + ancho / 2f - mitadAncho, arriba + 10f, izquierda + ancho / 2f + mitadAncho,
                arriba + 10f + pincelTexto.textSize * 1.4f, pincelFondoTexto)
            lienzo.drawText(texto, izquierda + ancho / 2f, arriba + 10f + pincelTexto.textSize * 1.1f, pincelTexto)
        } else if (resultado[ObstaculosNativa.REJILLA_CHICA] != 0) {
            val texto = "Rejilla chica (${columnas}x${filas} celdas): menos precisa"
            val mitadAncho = pincelTexto.measureText(texto) / 2f + 10f
            lienzo.drawRect(izquierda + ancho / 2f - mitadAncho, arriba + 10f, izquierda + ancho / 2f + mitadAncho,
                arriba + 10f + pincelTexto.textSize * 1.4f, pincelFondoTexto)
            lienzo.drawText(texto, izquierda + ancho / 2f, arriba + 10f + pincelTexto.textSize * 1.1f, pincelTexto)
        }
    }
}
