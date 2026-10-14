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
import android.text.InputType
import android.view.inputmethod.EditorInfo
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import java.io.File
import java.io.FileOutputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.atomic.AtomicInteger

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
    private lateinit var botonModelo: Button
    private lateinit var botonFuenteVideo: Button
    private lateinit var textoVideo: TextView
    private lateinit var textoFusion: TextView
    private lateinit var textoGiro: TextView
    private lateinit var botonCorredor: Button     // controles que se agregan por código debajo de «Fuente del video» (ver agregarControlesExtra)
    private lateinit var campoCorredor: EditText
    private lateinit var textoCorredor: TextView
    private val anguloAcumuladoX10 = AtomicInteger(0)   // suma de los giros confiables vistos por la cámara desde que se puso en 0
    private val cuadrosSinGiro = AtomicInteger(0)        // cuadros en que la cámara no tuvo confianza
    private var ultimoNumeroGiro = -1L

    private val principal = Handler(Looper.getMainLooper())
    @Volatile
    private var pendiente: CuadroAnalizado? = null
    private var dibujando = false
    private var ultimoMostrado: CuadroAnalizado? = null
    private var pidiendoPermiso = false
    private var registrosPendientes = 0
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
        botonModelo = findViewById(R.id.boton_modelo)
        botonFuenteVideo = findViewById(R.id.boton_fuente_video)
        textoVideo = findViewById(R.id.texto_video)
        botonTamano = findViewById(R.id.boton_tamano)
        textoFusion = findViewById(R.id.texto_fusion)
        textoGiro = findViewById(R.id.texto_giro)
        agregarControlesExtra()

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
        findViewById<Button>(R.id.boton_giro_cero).setOnClickListener {
            anguloAcumuladoX10.set(0)
            cuadrosSinGiro.set(0)
        }
        botonModelo.setOnClickListener {
            configuracion.usarModelo = !configuracion.usarModelo
            camara.activarModelo(configuracion.usarModelo && camara.abierta)
            mostrarValoresDeBarras()
        }
        botonFuenteVideo.setOnClickListener {
            configuracion.fuenteCarro = !configuracion.fuenteCarro
            camara.cerrar()
            abrirCamara()   // se vuelve a abrir con la fuente nueva (teléfono o video del carro)
            mostrarValoresDeBarras()
        }
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
        camara = CamaraObstaculos(this, { configuracion.comoArreglo() }, { intArrayOf(configuracion.anchoCuadro, configuracion.altoCuadro) },
            { false }, { configuracion.usarModelo }, { configuracion.corredorComoArreglo() }) { cuadro -> alLlegarCuadro(cuadro) }
        camara.fovX10 = CalibracionGuardada(this).fovCalibradoX10()   // campo de visión calibrado (0 = no calibrado: se usa el de la cámara)
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
        cuadro.giro?.let { giro ->
            if (giro.numero != ultimoNumeroGiro) {
                ultimoNumeroGiro = giro.numero
                if (giro.confiable) {
                    anguloAcumuladoX10.addAndGet(giro.giroX10)
                } else if (giro.dtMs > 0) {
                    cuadrosSinGiro.incrementAndGet()
                }
            }
        }
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
            textoFusion.text = textoDeFusion(cuadro)
            textoVideo.text = camara.textoVideo()
            textoGiro.text = textoDeGiro(cuadro)
            textoCorredor.text = textoDeCorredor(cuadro)
            registrosPendientes++
            if (registrosPendientes % 4 == 0) {   // cada ~2 s se anota en logcat lo que muestra la pantalla (sirve para probar sin mirar la imagen)
                Log.i(CamaraObstaculos.ETIQUETA, "pantalla| piso=" + (cuadro?.resultado?.let { "${it[ObstaculosNativa.REF_R]},${it[ObstaculosNativa.REF_G]},${it[ObstaculosNativa.REF_B]}" } ?: "-") + " | " + (textoFusion.text.toString() + " " + textoGiro.text.toString()).replace("\n", " / "))
            }
            principal.postDelayed(this, 500)
        }
    }

    /** El giro que la cámara ve (giro visual): el del último cuadro y el ángulo acumulado desde que se puso en 0. */
    private fun textoDeGiro(cuadro: CuadroAnalizado?): String {
        val giro = cuadro?.giro ?: return "Giro visual: esperando cuadros..."
        val acumulado = anguloAcumuladoX10.get()
        return String.format(Locale.US, "Giro visual (campo de visión %.1f°): %s\nÁngulo acumulado: %+.1f° (%d cuadros sin confianza)",
            camara.fovEfectivoX10 / 10.0,
            if (giro.confiable) String.format(Locale.US, "%+.1f° en este cuadro (confianza %d)", giro.giroX10 / 10.0, giro.confianzaMil / 10)
            else "sin confianza: " + MovimientoVisualNativa.textoMotivo(giro.motivo) + " (energía ${giro.energia}, mínimo 60)",
            acumulado / 10.0, cuadrosSinGiro.get())
    }

    /** Quién decidió y cuánto tarda cada etapa (detector de color, modelo de objetos y fusión). */
    private fun textoDeFusion(cuadro: CuadroAnalizado?): String {
        val m = camara.modelo
        if (!configuracion.usarModelo) {
            return "Modelo de objetos: apagado (decide solo el detector de color)"
        }
        if (m == null) {
            return "Modelo de objetos: esperando a la cámara..."
        }
        if (m.error.isNotEmpty()) {
            return "Modelo de objetos: ${m.error}\nDecide solo el detector de color."
        }
        if (cuadro == null) {
            return "Modelo de objetos: iniciando..."
        }
        val f = cuadro.fusion
        val sb = StringBuilder()
        sb.append(String.format(Locale.US, "Etapas: detector %.1f ms (conversión %.1f + análisis %.1f)\n", cuadro.msConversion + cuadro.msAnalisis, cuadro.msConversion, cuadro.msAnalisis))
        val r = m.ultimo
        if (r != null) {
            sb.append(String.format(Locale.US, "       modelo %.1f ms (preparar %.1f + inferir %.1f + postproceso %.1f), un resultado cada %.0f ms\n",
                r.msTotal, r.msPreparar, r.msInferir, r.msPostprocesar, m.msEntreResultados))
        } else {
            sb.append("       modelo: todavía sin resultados\n")
        }
        if (f != null && f[FusionNativa.R_CODIGO] == 0) {
            sb.append(String.format(Locale.US, "       fusión %.3f ms\nDecide: %s (severidad %d)%s\n", cuadro.msFusion,
                FusionNativa.textoFuente(f[FusionNativa.R_FUENTE]).uppercase(), f[FusionNativa.R_SEVERIDAD],
                if (f[FusionNativa.R_DETECTOR_SOLO] == 1) " — el detector frena solo, sin esperar al modelo" else ""))
            sb.append("Modelo: " + if (f[FusionNativa.R_MODELO_VIGENTE] == 1) "vigente (edad ${f[FusionNativa.R_EDAD_MODELO_MS]} ms)" else "caducado o sin objetos").append("\n")
            if (f[FusionNativa.R_AVISO] == 1) {
                sb.append("AVISO: " + (m.etiquetas.getOrNull(f[FusionNativa.R_AVISO_CLASE]) ?: "objeto") +
                    " (zona ${f[FusionNativa.R_AVISO_ZONA]}, cercanía ${f[FusionNativa.R_AVISO_CERCANIA]}): solo se avisa\n")
            }
        }
        return sb.toString()
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

    // ── Controles agregados por código (debajo de «Fuente del video») ─────────

    /** Agrega los controles del corredor de trayectoria (botón, campo de medidas y texto del estado). */
    private fun agregarControlesExtra() {
        val padre = botonFuenteVideo.parent as LinearLayout
        var lugar = padre.indexOfChild(botonFuenteVideo) + 1
        val separacion = (8 * resources.displayMetrics.density).toInt()

        botonCorredor = Button(this)
        botonCorredor.setOnClickListener {
            configuracion.corredorActivo = !configuracion.corredorActivo
            mostrarValoresDeBarras()
        }
        padre.addView(botonCorredor, lugar++, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).also { it.topMargin = separacion })

        campoCorredor = EditText(this)
        campoCorredor.inputType = InputType.TYPE_CLASS_TEXT
        campoCorredor.imeOptions = EditorInfo.IME_ACTION_DONE
        campoCorredor.setSingleLine(true)
        campoCorredor.textSize = 12f
        campoCorredor.hint = "ancho_cm altura_cm inclinación° campo_de_visión° margen_cm"
        campoCorredor.setOnEditorActionListener { vista, accion, _ ->
            if (accion == EditorInfo.IME_ACTION_DONE) {
                aplicarCampoCorredor(vista.text.toString())
                true
            } else {
                false
            }
        }
        padre.addView(campoCorredor, lugar++)

        textoCorredor = TextView(this)
        textoCorredor.typeface = android.graphics.Typeface.MONOSPACE
        textoCorredor.textSize = 12f
        padre.addView(textoCorredor, lugar)
    }

    /** Lee «ancho altura inclinación campo_de_visión margen» (cm, cm, grados, grados, cm) y los guarda. */
    private fun aplicarCampoCorredor(texto: String) {
        val numeros = texto.trim().split(Regex("[ ,;]+")).mapNotNull { it.replace(',', '.').toDoubleOrNull() }
        if (numeros.size != 5) {
            Toast.makeText(this, "Escribe 5 números: ancho_cm altura_cm inclinación° campo_de_visión° margen_cm", Toast.LENGTH_LONG).show()
            return
        }
        configuracion.corredorAnchoCm = Math.round(numeros[0]).toInt()
        configuracion.corredorAlturaCm = Math.round(numeros[1]).toInt()
        configuracion.corredorInclinacionX10 = Math.round(numeros[2] * 10).toInt()
        configuracion.corredorFovX10 = Math.round(numeros[3] * 10).toInt()
        configuracion.corredorMargenCm = Math.round(numeros[4]).toInt()
        mostrarValoresDeBarras()
    }

    /** El estado del corredor: lo configurado y, con el último cuadro, qué zonas quedan fuera. */
    private fun textoDeCorredor(cuadro: CuadroAnalizado?): String {
        val config = "Corredor ${if (configuracion.corredorActivo) "ACTIVO" else "apagado"}: carro ${configuracion.corredorAnchoCm} cm, cámara a " +
            "${configuracion.corredorAlturaCm} cm, ${configuracion.corredorInclinacionX10 / 10.0}° hacia abajo, campo de visión ${configuracion.corredorFovX10 / 10.0}°, margen ${configuracion.corredorMargenCm} cm"
        val c = cuadro?.corredor ?: return config
        val nombres = arrayOf("izq", "cen", "der")
        val partes = (0..2).joinToString("  ") { z ->
            "${nombres[z]}: " + (if (cuadro.resultado[1 + 2 * z] == 0) "libre" else if (c[CorredorNativa.FUERA + z] == 1) "obstáculo FUERA (solo avisa)"
                else "obstáculo DENTRO (cercanía ${c[CorredorNativa.CERCANIA + z]})")
        }
        return config + "\n" + partes
    }

    private fun mostrarValoresDeBarras() {
        botonCorredor.text = "Corredor de trayectoria: ${if (configuracion.corredorActivo) "SÍ" else "NO"} (tocar para cambiar)"
        campoCorredor.setText(String.format(Locale.US, "%d %d %.1f %.1f %d", configuracion.corredorAnchoCm, configuracion.corredorAlturaCm,
            configuracion.corredorInclinacionX10 / 10.0, configuracion.corredorFovX10 / 10.0, configuracion.corredorMargenCm))
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
        botonFuenteVideo.text = getString(R.string.boton_fuente_video, getString(if (configuracion.fuenteCarro) R.string.fuente_video_carro else R.string.fuente_video_telefono))
        botonModelo.text = getString(R.string.boton_modelo, getString(if (configuracion.usarModelo) R.string.si else R.string.no))
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
