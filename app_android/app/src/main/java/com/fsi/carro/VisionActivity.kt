/*
 * VisionActivity.kt — Pantalla para apuntar la cámara del teléfono al piso y ver qué detecta el
 * detector de obstáculos, ajustando los umbrales sin recompilar.
 *
 * GUÍA DE LECTURA
 *  1. Usar con el teléfono en HORIZONTAL (la pantalla está fija en horizontal): el piso cerca
 *     del carro debe quedar en la parte baja de la imagen.
 *  2. onResume abre la cámara (CamaraObstaculos) y pide el permiso si falta; onPause la cierra.
 *  3. Cada cuadro analizado llega en el hilo de la cámara; aquí solo se guarda el último y se
 *     pide al hilo principal que lo dibuje (si llegan más rápido de lo que se dibuja, se
 *     descartan los intermedios).
 *  4. Las dos barras cambian UMBRAL_COLOR y MIN_CELDAS en vivo y se guardan en
 *     SharedPreferences (ConfigVision); el modo semiautónomo usa los mismos valores.
 *  5. "Guardar cuadro" escribe el cuadro actual como JPEG en la carpeta privada de la app (se saca con
 *     adb pull; nunca va al repositorio).
 */
package com.fsi.carro

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.WindowManager
import android.widget.Button
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import java.io.File
import java.io.FileOutputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class VisionActivity : Activity() {

    companion object {
        const val PEDIDO_PERMISO_CAMARA = 21
    }

    private lateinit var configuracion: ConfigVision
    private lateinit var vista: VistaDeteccion
    private lateinit var textoStats: TextView
    private lateinit var textoUmbral: TextView
    private lateinit var textoMinCeldas: TextView
    private lateinit var textoFilaMinima: TextView
    private lateinit var textoGrupos: TextView
    private lateinit var textoAprendizaje: TextView
    private lateinit var barraGrupos: SeekBar
    private lateinit var barraAprendizaje: SeekBar
    private lateinit var barraUmbral: SeekBar
    private lateinit var barraMinCeldas: SeekBar
    private lateinit var barraFilaMinima: SeekBar
    private lateinit var camara: CamaraObstaculos
    private lateinit var botonTamano: Button

    private val principal = Handler(Looper.getMainLooper())
    @Volatile
    private var pendiente: CuadroAnalizado? = null
    private var dibujando = false
    private var ultimoMostrado: CuadroAnalizado? = null
    private var pidiendoPermiso = false
    private var ultimoAvisoPisoMs = 0L   // para mostrar unos segundos el aviso "piso nuevo aprendido"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_vision)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        configuracion = ConfigVision(this)
        vista = findViewById(R.id.vista_deteccion)
        textoStats = findViewById(R.id.texto_stats)
        textoUmbral = findViewById(R.id.texto_umbral)
        textoMinCeldas = findViewById(R.id.texto_min_celdas)
        barraUmbral = findViewById(R.id.barra_umbral)
        barraMinCeldas = findViewById(R.id.barra_min_celdas)
        textoFilaMinima = findViewById(R.id.texto_fila_minima)
        barraFilaMinima = findViewById(R.id.barra_fila_minima)
        textoGrupos = findViewById(R.id.texto_grupos)
        barraGrupos = findViewById(R.id.barra_grupos)
        textoAprendizaje = findViewById(R.id.texto_aprendizaje)
        barraAprendizaje = findViewById(R.id.barra_aprendizaje)

        barraUmbral.max = ConfigVision.UMBRAL_COLOR_MAXIMO
        barraMinCeldas.max = ConfigVision.MIN_CELDAS_MAXIMO - 1   // el progreso 0 equivale a 1 celda
        barraFilaMinima.max = ConfigVision.PORCENTAJE_FILA_MAXIMO   // el progreso es el por ciento del alto (0 = toda la imagen)
        barraGrupos.max = ConfigVision.GRUPOS_MAXIMO - 1             // el progreso 0 equivale a 1 color
        barraAprendizaje.max = ConfigVision.APRENDIZAJE_MAXIMO_MIL / 5   // pasos de 5 milésimas
        mostrarValoresDeBarras()

        barraUmbral.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.umbralColor = valor })
        barraMinCeldas.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.minCeldas = valor + 1 })
        barraFilaMinima.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.filaMinimaMil = valor * 10 })
        barraGrupos.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.gruposPiso = valor + 1 })
        barraAprendizaje.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.aprendizajeMil = valor * 5 })
        botonTamano = findViewById(R.id.boton_tamano)
        botonTamano.setOnClickListener {
            configuracion.indiceTamano = (configuracion.indiceTamano + 1) % ConfigVision.TAMANOS.size   // la cámara lo toma en el siguiente cuadro
            mostrarValoresDeBarras()
        }
        findViewById<Button>(R.id.boton_guardar_cuadro).setOnClickListener { guardarCuadro() }
        findViewById<Button>(R.id.boton_restablecer).setOnClickListener {
            configuracion.restablecer()
            ObstaculosNativa.reiniciarModelo()   // también se olvida el piso aprendido
            mostrarValoresDeBarras()
        }

        // La cámara lee la configuración actual en cada cuadro: los cambios de las barras se notan al instante.
        camara = CamaraObstaculos(this, { configuracion.comoArreglo() }, { intArrayOf(configuracion.anchoCuadro, configuracion.altoCuadro) }) { cuadro -> alLlegarCuadro(cuadro) }
        vista.mostrar(null, "Abriendo cámara...")
        principal.post(actualizarTextos)
    }

    override fun onResume() {
        super.onResume()
        abrirCamara()
    }

    override fun onPause() {
        camara.cerrar()   // se libera la cámara al salir (segundo plano incluido)
        principal.removeCallbacks(actualizarTextos)
        super.onPause()
    }

    override fun onDestroy() {
        principal.removeCallbacksAndMessages(null)
        super.onDestroy()
    }

    // ── Permiso y cámara ──────────────────────────────────────────────────

    private fun abrirCamara() {
        if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            if (!pidiendoPermiso) {
                pidiendoPermiso = true
                Log.i(CamaraObstaculos.ETIQUETA, "pidiendo permiso de cámara")
                requestPermissions(arrayOf(Manifest.permission.CAMERA), PEDIDO_PERMISO_CAMARA)
            }
            vista.mostrar(null, "Se necesita el permiso de cámara")
            return
        }
        if (!camara.abrir()) {
            vista.mostrar(null, camara.error)
        }
        principal.removeCallbacks(actualizarTextos)
        principal.post(actualizarTextos)
    }

    override fun onRequestPermissionsResult(codigo: Int, permisos: Array<out String>, resultados: IntArray) {
        super.onRequestPermissionsResult(codigo, permisos, resultados)
        if (codigo == PEDIDO_PERMISO_CAMARA) {
            pidiendoPermiso = false
            if (resultados.isNotEmpty() && resultados[0] == PackageManager.PERMISSION_GRANTED) {
                Log.i(CamaraObstaculos.ETIQUETA, "permiso de cámara concedido")
                abrirCamara()
            } else {
                Log.w(CamaraObstaculos.ETIQUETA, "permiso de cámara negado")
                vista.mostrar(null, "Permiso de cámara negado")
            }
        }
    }

    // ── Dibujo ────────────────────────────────────────────────────────────

    /** Llega en el hilo de la cámara: guarda el cuadro y pide dibujarlo en el hilo principal. */
    private fun alLlegarCuadro(cuadro: CuadroAnalizado) {
        pendiente = cuadro
        if (!dibujando) {
            dibujando = true
            principal.post {
                dibujando = false
                val ultimo = pendiente
                if (ultimo != null) {
                    ultimoMostrado = ultimo
                    vista.mostrar(ultimo)
                }
            }
        }
    }

    /** Cada medio segundo actualiza los números de la derecha. */
    private val actualizarTextos = object : Runnable {
        override fun run() {
            val cuadro = ultimoMostrado
            val estado = if (camara.abierta) "ABIERTA" else if (camara.error.isNotEmpty()) camara.error else "abriendo..."
            textoStats.text = String.format(Locale.US,
                "Cámara: %s\nTamaño cámara: %s\nTamaño del cuadro: %s\nCuadros por segundo: %.1f\nms por cuadro: %.2f%s",
                estado, camara.tamanoCamara.ifEmpty { "-" },
                if (cuadro != null) "${cuadro.ancho}x${cuadro.alto} (${cuadro.columnas}x${cuadro.filas} celdas)" else "-",
                camara.fps, camara.msPorCuadro,
                if (cuadro != null) String.format(Locale.US, "\n  conversión %.2f + análisis %.2f\nPiso (RGB): %d,%d,%d\nGrupos de color del piso: %d%s\nCódigo: %d",
                    cuadro.msConversion, cuadro.msAnalisis,
                    cuadro.resultado[ObstaculosNativa.REF_R], cuadro.resultado[ObstaculosNativa.REF_G], cuadro.resultado[ObstaculosNativa.REF_B],
                    cuadro.resultado[ObstaculosNativa.GRUPOS_PISO], avisosDelPiso(cuadro),
                    cuadro.resultado[ObstaculosNativa.CODIGO]) else "")
            principal.postDelayed(this, 500)
        }
    }

    /** Avisos del modelo del piso para mostrar en pantalla (franja tapada, piso nuevo aprendido). */
    private fun avisosDelPiso(cuadro: CuadroAnalizado): String {
        if (cuadro.resultado[ObstaculosNativa.CODIGO] != 0) {
            return ""
        }
        if (cuadro.resultado[ObstaculosNativa.PISO_REAPRENDIDO] != 0) {
            ultimoAvisoPisoMs = System.currentTimeMillis()
        }
        val sb = StringBuilder()
        if (System.currentTimeMillis() - ultimoAvisoPisoMs < 5000) {
            sb.append("\n¡PISO NUEVO APRENDIDO!")
        }
        if (cuadro.resultado[ObstaculosNativa.FRANJA_TAPADA] != 0) {
            sb.append("\nFranja tapada: se usa el piso aprendido")
        }
        return sb.toString()
    }

    // ── Barras ────────────────────────────────────────────────────────────

    private fun mostrarValoresDeBarras() {
        barraUmbral.progress = configuracion.umbralColor.coerceIn(0, ConfigVision.UMBRAL_COLOR_MAXIMO)
        barraMinCeldas.progress = (configuracion.minCeldas - 1).coerceIn(0, ConfigVision.MIN_CELDAS_MAXIMO - 1)
        textoUmbral.text = getString(R.string.texto_umbral, configuracion.umbralColor)
        textoMinCeldas.text = getString(R.string.texto_min_celdas, configuracion.minCeldas)
        barraFilaMinima.progress = configuracion.filaMinimaMil / 10
        val filasDelCuadro = ObstaculosNativa.filasDe(configuracion.altoCuadro)
        barraGrupos.progress = configuracion.gruposPiso - 1
        textoGrupos.text = getString(R.string.texto_grupos, configuracion.gruposPiso)
        barraAprendizaje.progress = configuracion.aprendizajeMil / 5
        textoAprendizaje.text = getString(R.string.texto_aprendizaje, configuracion.aprendizajeMil)
        textoFilaMinima.text = getString(R.string.texto_fila_minima, configuracion.filaMinimaMil / 10,
            filasDelCuadro * configuracion.filaMinimaMil / 1000, filasDelCuadro)
        botonTamano.text = getString(R.string.boton_tamano, configuracion.anchoCuadro, configuracion.altoCuadro)
    }

    /** Un escuchador de barra que guarda el valor al moverla (solo si lo movió la persona). */
    private fun alMoverBarra(guardar: (Int) -> Unit) = object : SeekBar.OnSeekBarChangeListener {
        override fun onProgressChanged(barra: SeekBar, valor: Int, delUsuario: Boolean) {
            if (delUsuario) {
                guardar(valor)
                mostrarValoresDeBarras()
            }
        }

        override fun onStartTrackingTouch(barra: SeekBar) {}
        override fun onStopTrackingTouch(barra: SeekBar) {}
    }

    // ── Guardar cuadro ────────────────────────────────────────────────────

    /** Guarda el cuadro actual como JPEG en la carpeta de archivos de la app (para sacarlo con adb pull). */
    private fun guardarCuadro() {
        val cuadro = ultimoMostrado
        if (cuadro == null) {
            Toast.makeText(this, getString(R.string.sin_cuadro), Toast.LENGTH_SHORT).show()
            return
        }
        val nombre = "cuadro_" + SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date()) + ".jpg"
        val archivo = File(getExternalFilesDir(null), nombre)
        try {
            val bitmap = Bitmap.createBitmap(cuadro.pixeles, cuadro.ancho, cuadro.alto, Bitmap.Config.ARGB_8888)
            FileOutputStream(archivo).use { bitmap.compress(Bitmap.CompressFormat.JPEG, 90, it) }
            Log.i(CamaraObstaculos.ETIQUETA, "cuadro guardado: ${archivo.absolutePath}")
            Toast.makeText(this, getString(R.string.cuadro_guardado, archivo.name), Toast.LENGTH_LONG).show()
        } catch (e: Exception) {
            Log.w(CamaraObstaculos.ETIQUETA, "no se pudo guardar el cuadro: ${e.message}")
            Toast.makeText(this, "No se pudo guardar: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }
}
