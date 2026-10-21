/*
 * VisionActivity.kt — Pantalla VISIÓN Y DIAGNÓSTICO: apuntar la cámara al piso y ver qué detecta el detector de obstáculos, ajustando los umbrales sin recompilar.
 *
 * GUÍA DE LECTURA
 *  1. Usar con el teléfono en HORIZONTAL (la pantalla está fija en horizontal): el piso cerca del carro debe quedar en la parte baja de la imagen.
 *     Izquierda: el cuadro con la detección (VistaDeteccion) y un chip con el estado de la cámara. Derecha: lo esencial arriba (estado del detector, de dónde viene
 *     la cámara y el botón «Guardar cuadro») y, plegada, la sección «Avanzado» con todos los ajustes finos.
 *  2. onResume abre la cámara (CamaraObstaculos) y pide el permiso si falta; onPause la cierra. Si la cámara falla, un aviso en banda dice qué pasó y qué hacer.
 *  3. Cada cuadro analizado llega en el hilo de la cámara; aquí solo se guarda el último y se pide al hilo principal que lo dibuje (si llegan más rápido de lo que
 *     se dibuja, se descartan los intermedios).
 *  4. Los deslizadores cambian UMBRAL_COLOR, MIN_CELDAS, fila mínima, grupos del piso (K) y aprendizaje en vivo y se guardan en SharedPreferences (ConfigVision); el
 *     modo semiautónomo usa los mismos valores. Los interruptores (cámara del carro, modelo de objetos, «cualquier objeto», corredor, flujo) cambian en ConfigVision
 *     lo mismo que antes cambiaban los botones de «tocar para cambiar».
 *  5. «Avanzado» se abre y se cierra tocando su encabezado (animación de 200 ms, cerrada al abrir la pantalla). «Restablecer» llama a ConfigVision.restablecer y a
 *     ObstaculosNativa.reiniciarModelo (también se olvida el piso aprendido) y refresca los deslizadores.
 *  6. «Guardar cuadro» escribe el cuadro actual como JPEG en la carpeta privada de la app (se saca con adb pull; nunca va al repositorio).
 *  7. Los textos de estado (detector, modelo, video, giro, corredor, piso) se refrescan cada medio segundo (actualizarTextos). PantallasLog.abierta("VISION") se
 *     escribe al terminar de armar la pantalla.
 */
package com.fsi.carro

import android.Manifest
import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.animation.ValueAnimator
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import com.google.android.material.button.MaterialButton
import com.google.android.material.materialswitch.MaterialSwitch
import java.io.File
import java.io.FileOutputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.atomic.AtomicInteger

class VisionActivity : androidx.appcompat.app.AppCompatActivity() {

    companion object {
        const val PEDIDO_PERMISO_CAMARA = 21
    }

    private lateinit var configuracion: ConfigVision
    private lateinit var vista: VistaDeteccion
    private lateinit var chipCamara: ChipEstado
    private lateinit var aviso: AvisoBanda
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
    private lateinit var botonTamano: MaterialButton
    private lateinit var interruptorModelo: MaterialSwitch
    private lateinit var interruptorFuente: MaterialSwitch        // cámara del carro (WiFi) o del teléfono
    private lateinit var textoVideo: TextView
    private lateinit var textoFusion: TextView
    private lateinit var textoGiro: TextView
    private lateinit var botonModoVideo: MaterialButton   // modo de video del carro: jpeg, raw o celdas (Trabajo I)
    private lateinit var interruptorFlujo: MaterialSwitch  // «Flujo del piso» (movimiento_piso.h): apagado por defecto
    private lateinit var interruptorCorredor: MaterialSwitch
    private lateinit var campoCorredor: EditText
    private lateinit var textoCorredor: TextView
    private lateinit var botonAprenderPiso: MaterialButton   // «Aprender piso»: con el carro quieto, aprende los colores de toda la zona de abajo
    private lateinit var textoPisoAprendido: TextView
    private lateinit var interruptorCualquiera: MaterialSwitch    // «Modelo: cualquier objeto» y su umbral de confianza (0.01 a 0.50)
    private lateinit var barraCualquiera: SeekBar
    private lateinit var textoCualquiera: TextView
    private lateinit var avanzadoEncabezado: View
    private lateinit var avanzadoIcono: ImageView
    private lateinit var avanzadoContenido: LinearLayout
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
    private var refrescando = false      // true mientras el código (no la persona) pone los valores en los controles: los escuchas no hacen nada
    private var avanzadoAbierto = false
    private var animadorAvanzado: ValueAnimator? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_vision)
        Diseno.barras(this)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        configuracion = ConfigVision(this)
        vista = findViewById(R.id.vista_deteccion)
        chipCamara = findViewById(R.id.vision_chip_camara)
        aviso = findViewById(R.id.vision_aviso)
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
        botonAprenderPiso = findViewById(R.id.boton_aprender_piso)
        textoPisoAprendido = findViewById(R.id.texto_piso_aprendido)
        interruptorModelo = findViewById(R.id.switch_modelo)
        interruptorFuente = findViewById(R.id.switch_fuente_video)
        textoVideo = findViewById(R.id.texto_video)
        botonTamano = findViewById(R.id.boton_tamano)
        textoFusion = findViewById(R.id.texto_fusion)
        textoGiro = findViewById(R.id.texto_giro)
        interruptorCualquiera = findViewById(R.id.switch_cualquiera)
        textoCualquiera = findViewById(R.id.texto_cualquiera)
        barraCualquiera = findViewById(R.id.barra_cualquiera)
        interruptorCorredor = findViewById(R.id.switch_corredor)
        campoCorredor = findViewById(R.id.campo_corredor)
        textoCorredor = findViewById(R.id.texto_corredor)
        interruptorFlujo = findViewById(R.id.switch_flujo)
        botonModoVideo = findViewById(R.id.boton_modo_video)
        avanzadoEncabezado = findViewById(R.id.avanzado_encabezado)
        avanzadoIcono = findViewById(R.id.avanzado_icono)
        avanzadoContenido = findViewById(R.id.avanzado_contenido)

        findViewById<TextView>(R.id.barra_titulo).setText(R.string.vision_titulo_corto)
        if (ConfigVision.camaraDePrueba) findViewById<TextView>(R.id.barra_titulo).apply {
            maxLines = 3
            append("\n" + getString(R.string.camara_prueba))
        }
        findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { finish() }

        barraUmbral.max = ConfigVision.UMBRAL_COLOR_MAXIMO
        barraMinCeldas.max = ConfigVision.MIN_CELDAS_MAXIMO - 1   // el progreso 0 equivale a 1 celda
        barraFilaMinima.max = ConfigVision.PORCENTAJE_FILA_MAXIMO   // el progreso es el por ciento del alto (0 = toda la imagen)
        barraGrupos.max = ConfigVision.GRUPOS_MAXIMO - 1             // el progreso 0 equivale a 1 color
        barraAprendizaje.max = ConfigVision.APRENDIZAJE_MAXIMO_MIL / 5   // pasos de 5 milésimas
        barraCualquiera.max = CualquieraNativa.UMBRAL_MAX_MIL - CualquieraNativa.UMBRAL_MIN_MIL   // 0 = 0.01 ... 490 = 0.50
        mostrarValoresDeBarras()

        barraUmbral.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.umbralColor = valor })
        barraMinCeldas.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.minCeldas = valor + 1 })
        barraFilaMinima.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.filaMinimaMil = valor * 10 })
        barraGrupos.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.gruposPiso = valor + 1 })
        barraAprendizaje.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.aprendizajeMil = valor * 5 })
        barraCualquiera.setOnSeekBarChangeListener(alMoverBarra { valor -> configuracion.umbralCualquieraMil = valor + CualquieraNativa.UMBRAL_MIN_MIL })
        findViewById<MaterialButton>(R.id.boton_giro_cero).setOnClickListener {
            anguloAcumuladoX10.set(0)
            cuadrosSinGiro.set(0)
        }
        interruptorModelo.setOnCheckedChangeListener { _, marcado ->
            if (!refrescando) {
                configuracion.usarModelo = marcado
                camara.activarModelo(configuracion.usarModelo && camara.abierta)
                mostrarValoresDeBarras()
            }
        }
        interruptorFuente.setOnCheckedChangeListener { _, marcado ->
            if (!refrescando) {
                configuracion.fuenteCarro = marcado
                camara.cerrar()
                abrirCamara()   // se vuelve a abrir con la fuente nueva (teléfono o video del carro)
                mostrarValoresDeBarras()
            }
        }
        interruptorCualquiera.setOnCheckedChangeListener { _, marcado ->
            if (!refrescando) {
                configuracion.cualquierObjeto = marcado
                if (configuracion.cualquierObjeto && !configuracion.usarModelo) {
                    configuracion.usarModelo = true   // el modo necesita el modelo de objetos encendido
                    camara.activarModelo(camara.abierta)
                }
                CualquieraNativa.reiniciar()
                mostrarValoresDeBarras()
            }
        }
        interruptorCorredor.setOnCheckedChangeListener { _, marcado ->
            if (!refrescando) {
                configuracion.corredorActivo = marcado
                mostrarValoresDeBarras()
            }
        }
        interruptorFlujo.setOnCheckedChangeListener { _, marcado ->
            if (!refrescando) {
                configuracion.flujoActivo = marcado
                mostrarValoresDeBarras()
            }
        }
        botonModoVideo.setOnClickListener {
            configuracion.modoVideo = ModoVideo.siguiente(configuracion.modoVideo)
            if (configuracion.fuenteCarro) {
                camara.cerrar()
                SesionVideo.cambiarModo(configuracion.modoVideo)   // el video es una sesión compartida: se reabre con el modo nuevo (la cámara del carro lo cambia con /control?modo=)
                abrirCamara()
            }
            mostrarValoresDeBarras()
        }
        botonAprenderPiso.setOnClickListener {
            if (camara.abierta && !camara.aprendiendoPiso) {
                camara.aprenderPiso()
            } else {
                Toast.makeText(this, getString(R.string.vision_camara_no_lista), Toast.LENGTH_SHORT).show()
            }
        }
        campoCorredor.setOnEditorActionListener { campo, accion, _ ->
            if (accion == EditorInfo.IME_ACTION_DONE) {
                campo.clearFocus()
                (getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager).hideSoftInputFromWindow(campo.windowToken, 0)
                aplicarCampoCorredor(campo.text.toString())
                true
            } else {
                false
            }
        }
        botonTamano.setOnClickListener {
            configuracion.indiceTamano = (configuracion.indiceTamano + 1) % ConfigVision.TAMANOS.size   // la cámara lo toma en el siguiente cuadro
            mostrarValoresDeBarras()
        }
        findViewById<MaterialButton>(R.id.boton_guardar_cuadro).setOnClickListener { guardarCuadro() }
        findViewById<MaterialButton>(R.id.boton_restablecer).setOnClickListener {
            configuracion.restablecer()
            ObstaculosNativa.reiniciarModelo()   // también se olvida el piso aprendido
            mostrarValoresDeBarras()
        }
        avanzadoEncabezado.setOnClickListener { ponerAvanzado(!avanzadoAbierto, true) }
        ponerAvanzado(savedInstanceState?.getBoolean("avanzado") ?: false, false)

        // La cámara lee la configuración actual en cada cuadro: los cambios de los controles se notan al instante.
        camara = CamaraObstaculos(this, { configuracion.comoArreglo() }, { intArrayOf(configuracion.anchoCuadro, configuracion.altoCuadro) },
            { false }, { configuracion.usarModelo }, { configuracion.corredorComoArreglo() }, { configuracion.cualquieraComoArreglo() }, { configuracion.flujoComoArreglo() }) { cuadro -> alLlegarCuadro(cuadro) }
        camara.fovX10 = CalibracionGuardada(this).fovCalibradoX10()   // campo de visión calibrado (0 = no calibrado: se usa el de la cámara)
        vista.mostrar(null, "Abriendo cámara...")
        chipCamara.poner(Nivel.AVISO, getString(R.string.vision_chip_abriendo))
        principal.post(actualizarTextos)
        PantallasLog.abierta("VISION")
    }

    override fun onSaveInstanceState(salida: Bundle) {
        super.onSaveInstanceState(salida)
        salida.putBoolean("avanzado", avanzadoAbierto)
    }

    // ── Sección «Avanzado» (plegable) ─────────────────────────────────────

    /** Abre o cierra «Avanzado». Con `animar` el alto cambia en 200 ms (Movimiento.NORMAL); sin animar se pone directo (al armar la pantalla). */
    private fun ponerAvanzado(abrir: Boolean, animar: Boolean) {
        avanzadoAbierto = abrir
        avanzadoIcono.setImageResource(if (abrir) R.drawable.ic_expand_less else R.drawable.ic_expand_more)
        avanzadoEncabezado.contentDescription = getString(if (abrir) R.string.vision_avanzado_abierto else R.string.vision_avanzado_cerrado)
        animadorAvanzado?.cancel()
        val contenido = avanzadoContenido
        if (!animar) {
            contenido.layoutParams.height = ViewGroup.LayoutParams.WRAP_CONTENT
            contenido.visibility = if (abrir) View.VISIBLE else View.GONE
            contenido.requestLayout()
            return
        }
        if (abrir) {
            // Se mide el alto que tendrá abierto para animar de 0 hasta él.
            val ancho = (contenido.parent as View).width
            contenido.measure(View.MeasureSpec.makeMeasureSpec(ancho, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
            contenido.layoutParams.height = 0
            contenido.visibility = View.VISIBLE
            animarAlto(0, contenido.measuredHeight) {
                contenido.layoutParams.height = ViewGroup.LayoutParams.WRAP_CONTENT
                contenido.requestLayout()
            }
        } else {
            animarAlto(contenido.height, 0) {
                contenido.visibility = View.GONE
                contenido.layoutParams.height = ViewGroup.LayoutParams.WRAP_CONTENT
            }
        }
    }

    private fun animarAlto(desde: Int, hasta: Int, alTerminar: () -> Unit) {
        val contenido = avanzadoContenido
        animadorAvanzado = ValueAnimator.ofInt(desde, hasta).apply {
            duration = Movimiento.NORMAL
            addUpdateListener {
                contenido.layoutParams.height = it.animatedValue as Int
                contenido.requestLayout()
            }
            addListener(object : AnimatorListenerAdapter() {
                override fun onAnimationEnd(animacion: Animator) {
                    alTerminar()
                }
            })
            start()
        }
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
        animadorAvanzado?.cancel()
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
            principal.removeCallbacks(actualizarTextos)
            principal.post(actualizarTextos)
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

    /** Cada medio segundo actualiza los números de la derecha, el chip de la cámara y el aviso si la cámara falla. */
    private val actualizarTextos = object : Runnable {
        override fun run() {
            val cuadro = ultimoMostrado
            val estado = if (camara.abierta) "abierta" else if (camara.error.isNotEmpty()) "con problema (ver el aviso)" else "abriendo..."
            textoStats.text = String.format(Locale.US,
                "Cámara: %s\nTamaño de la cámara: %s\nTamaño del cuadro: %s\nCuadros por segundo: %.1f\nms por cuadro: %.2f%s",
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
            textoPisoAprendido.text = when {
                camara.aprendiendoPiso -> "Aprendiendo el piso... deja el carro quieto y sin objetos adelante."
                camara.errorAprender.isNotEmpty() -> camara.errorAprender
                camara.informePiso != null -> camara.informePiso!!.texto()
                else -> "Piso: todavía no se aprendió con el botón (se usa lo que ve la tira de abajo)."
            } + (cuadro?.let { "\nBrillo medio del cuadro: ${it.resultado[ObstaculosNativa.BRILLO_MEDIO]}  motivo: ${ObstaculosNativa.textoMotivo(it.resultado[ObstaculosNativa.MOTIVO])}" } ?: "")
            actualizarEstadoCamara()
            registrosPendientes++
            if (registrosPendientes % 4 == 0) {   // cada ~2 s se anota en logcat lo que muestra la pantalla (sirve para probar sin mirar la imagen)
                Log.i(CamaraObstaculos.ETIQUETA, "pantalla| piso=" + (cuadro?.resultado?.let { "${it[ObstaculosNativa.REF_R]},${it[ObstaculosNativa.REF_G]},${it[ObstaculosNativa.REF_B]}" } ?: "-") + " | " + (textoFusion.text.toString() + " " + textoGiro.text.toString()).replace("\n", " / "))
            }
            principal.postDelayed(this, 500)
        }
    }

    /** El chip sobre el video y el aviso en banda: qué pasa con la cámara y qué hacer (sin códigos crudos; esos se quedan en logcat). */
    private fun actualizarEstadoCamara() {
        val sinPermiso = checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED
        when {
            camara.abierta -> {
                chipCamara.poner(Nivel.OK, getString(R.string.vision_chip_abierta, String.format(Locale.US, "%.0f", camara.fps)))
                aviso.ocultar()
            }
            sinPermiso -> {
                chipCamara.poner(Nivel.ERROR, getString(R.string.vision_chip_sin_permiso))
                aviso.mostrar(Nivel.ERROR, getString(R.string.vision_aviso_permiso_paso), getString(R.string.vision_aviso_permiso_hacer), getString(R.string.reintentar)) {
                    pidiendoPermiso = false
                    abrirCamara()
                }
            }
            camara.error.isNotEmpty() -> {
                chipCamara.poner(Nivel.ERROR, getString(R.string.vision_chip_sin_camara))
                if (configuracion.fuenteCarro) {
                    val m = Mensajes.sinVideo(this)
                    aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer, getString(R.string.reintentar)) {
                        SesionVideo.reintentar()
                        camara.cerrar()
                        abrirCamara()
                    }
                } else {
                    aviso.mostrar(Nivel.ERROR, getString(R.string.vision_aviso_telefono_paso), getString(R.string.vision_aviso_telefono_hacer), getString(R.string.reintentar)) {
                        camara.cerrar()
                        abrirCamara()
                    }
                }
            }
            else -> {
                chipCamara.poner(Nivel.AVISO, getString(R.string.vision_chip_abriendo))
                aviso.ocultar()
            }
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

    // ── Corredor de trayectoria (medidas escritas) ────────────────────────

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

    private fun textoUmbralCualquiera(): String = String.format(Locale.US, "Umbral de «cualquier objeto»: la caja debe SUPERAR %.2f de confianza (de 0.01 a 0.50)", configuracion.umbralCualquieraMil / 1000.0)

    // ── Valores de los controles ──────────────────────────────────────────

    /** Pone en cada control (deslizadores, interruptores, botones y sus textos) lo que dice ConfigVision. Los escuchas se callan mientras tanto. */
    private fun mostrarValoresDeBarras() {
        refrescando = true
        try {
            interruptorCualquiera.isChecked = configuracion.cualquierObjeto
            barraCualquiera.progress = configuracion.umbralCualquieraMil - CualquieraNativa.UMBRAL_MIN_MIL
            textoCualquiera.text = textoUmbralCualquiera()
            botonModoVideo.text = if (::camara.isInitialized && camara.esVideoCarro && camara.modoVideo != configuracion.modoVideo)
                getString(R.string.vision_modo_video_en_uso, configuracion.modoVideo, camara.modoVideo)
            else getString(R.string.vision_modo_video, configuracion.modoVideo)
            interruptorFlujo.isChecked = configuracion.flujoActivo
            interruptorCorredor.isChecked = configuracion.corredorActivo
            if (!campoCorredor.hasFocus()) {
                campoCorredor.setText(String.format(Locale.US, "%d %d %.1f %.1f %d", configuracion.corredorAnchoCm, configuracion.corredorAlturaCm,
                    configuracion.corredorInclinacionX10 / 10.0, configuracion.corredorFovX10 / 10.0, configuracion.corredorMargenCm))
            }
            barraUmbral.progress = configuracion.umbralColor.coerceIn(0, ConfigVision.UMBRAL_COLOR_MAXIMO)
            barraMinCeldas.progress = (configuracion.minCeldas - 1).coerceIn(0, ConfigVision.MIN_CELDAS_MAXIMO - 1)
            textoUmbral.text = getString(R.string.vision_umbral, configuracion.umbralColor)
            textoMinCeldas.text = getString(R.string.vision_min_celdas, configuracion.minCeldas)
            barraFilaMinima.progress = configuracion.filaMinimaMil / 10
            val filasDelCuadro = ObstaculosNativa.filasDe(configuracion.altoCuadro)
            barraGrupos.progress = configuracion.gruposPiso - 1
            textoGrupos.text = getString(R.string.vision_grupos, configuracion.gruposPiso)
            barraAprendizaje.progress = configuracion.aprendizajeMil / 5
            textoAprendizaje.text = getString(R.string.vision_aprendizaje, configuracion.aprendizajeMil)
            textoFilaMinima.text = getString(R.string.vision_fila_minima, configuracion.filaMinimaMil / 10,
                filasDelCuadro * configuracion.filaMinimaMil / 1000, filasDelCuadro)
            botonTamano.text = getString(R.string.vision_tamano, configuracion.anchoCuadro, configuracion.altoCuadro)
            interruptorFuente.isChecked = configuracion.fuenteCarro
            interruptorModelo.isChecked = configuracion.usarModelo
        } finally {
            refrescando = false
        }
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
            Toast.makeText(this, "No se pudo guardar el cuadro. Revisa que haya espacio en el teléfono y vuelve a intentar.", Toast.LENGTH_LONG).show()
        }
    }
}
