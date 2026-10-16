/*
 * CalibracionActivity.kt — Asistente paso a paso para MEDIR el carro y que el navegador convierta
 * distancias y giros en tiempos reales (el carro no tiene encoders ni IMU).
 *
 * GUÍA DE LECTURA
 *  1. Pasos (enum Paso): INTRO → REACCION → ARRANQUE → AVANCE → GIRO_IZQ → GIRO_DER → RETROCESO → GIRO_VISUAL → RESUMEN → GIRO_90. UNA prueba por pantalla.
 *       - REACCION: mide tu tiempo de reacción (toca cuando el botón se pone verde).
 *       - ARRANQUE: manda FORWARD y tú tocas "YA SE MOVIÓ" cuando ves moverse las ruedas; el
 *         arranque es ese tiempo menos tu reacción. 5 repeticiones, se descartan los datos raros.
 *       - AVANCE: manda FORWARD durante 1000, 2000 y 3000 ms (3 veces cada uno) y tú escribes cuántos
 *         cm avanzó (el carro frena solo después del STOP: mídelo hasta que se detenga del todo).
 *       - GIRO_IZQ / GIRO_DER: igual con LEFT y RIGHT (500, 1000 y 1500 ms), anotando los grados
 *         aproximados (con marcas de 90 grados en el piso). Cada lado por separado.
 *       - RETROCESO (opcional): igual que el AVANCE pero con REVERSE; mide los ms por cm hacia atrás. Lo usan los reintentos de esquive
 *         (el carro retrocede 15 cm) y, si no se mide, se usa el valor del avance con un aviso.
 *       - GIRO_VISUAL (opcional): gira 90 grados a la derecha con la calibración ya medida mientras la cámara (la que alimenta al
 *         detector) mide cuánto vio girar; tú escribes cuántos grados giró de verdad. Con eso se ajusta el campo de visión (FOV)
 *         de la cámara, que convierte píxeles en grados (docs/giro_visual.md).
 *       - RESUMEN: calcula con el código en C (CalibracionNativa) y muestra los valores; "Usar estos
 *         valores" los guarda (CalibracionGuardada) y el navegador los usa en la próxima ruta.
 *       - GIRO_90 (AJUSTE FINO, después del resumen o desde el atajo de la primera pantalla; parte de la calibración GUARDADA): el carro gira lo que la calibración dice que son
 *         90° y tú dices cómo giró («se pasó mucho / un poco / justo / se quedó corto») o escribes los grados. Con eso se corrige un FACTOR del giro por lado (izquierda y derecha) y
 *         por perfil (AUTO y MANUAL), que se guarda en CalibracionGuardada y lo usan las rutas y el simulador. Se repite hasta que quede justo. «Restaurar valor por defecto»
 *         lo vuelve a 1000. Con pilas nuevas hay que repetirlo (docs/calibracion.md).
 *  2. Toda la cuenta (promedios, atípicos, rectas, frenado) está en C y probada en PC
 *     (nucleo_c/calibracion.c). Aquí solo hay pantalla y temporizadores.
 *  3. LA PANTALLA (diseño nuevo, docs/diseno.md): «Paso X de 9» con una barra de avance, el título, una ilustración dibujada por código
 *     (IlustracionCalibracion), una instrucción corta y, abajo y siempre visibles, la acción del paso, Atrás / Siguiente y el PARO.
 *     Solo UN botón se ve como primario a la vez (el que toca tocar ahora; ver actualizarBotones).
 *  4. LA CONEXIÓN es la compartida de toda la app (SesionCarro): se entra en onResume y se sale en onPause; la sesión abre y cierra el WebSocket
 *     (aquí ya no se llama a iniciar/detener). La dirección del carro es la de Ajustes. Sin conexión sale un aviso en banda con «Conectar».
 *  5. Seguridad: el botón PARO siempre está abajo; al salir de la pantalla se manda STOP (logica.parar) y se sueltan las pruebas.
 *     Hace falta tener ~2 m libres delante del carro.
 *  6. Los mensajes importantes también van a logcat con la etiqueta CALIBRACION. PantallasLog.abierta("CALIBRACION") sale al armar la pantalla.
 */
package com.fsi.carro

import android.Manifest
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.pm.PackageManager
import android.content.res.ColorStateList
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.text.Editable
import android.text.TextWatcher
import android.util.Log
import android.view.View
import android.view.WindowManager
import android.widget.EditText
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.core.content.ContextCompat
import com.google.android.material.button.MaterialButton
import com.google.android.material.progressindicator.LinearProgressIndicator
import com.google.android.material.textfield.TextInputLayout
import java.util.Locale
import kotlin.random.Random

class CalibracionActivity : androidx.appcompat.app.AppCompatActivity() {

    companion object {
        val TIEMPOS_AVANCE = intArrayOf(1000, 2000, 3000)   // ms de FORWARD en cada prueba de avance
        val TIEMPOS_GIRO = intArrayOf(500, 1000, 1500)      // ms de LEFT / RIGHT en cada prueba de giro
        const val REPETICIONES = 3                          // por cada tiempo
        const val TOQUES_REACCION = 3
        const val TOQUES_ARRANQUE = 5
        const val REACCION_POR_DEFECTO_MS = 180             // si no se midió la reacción
        const val ESPERA_MAXIMA_ARRANQUE_MS = 3000L         // si no tocas en este tiempo, se detiene el carro
        private const val LADO_ATRAS = 2                    // en ajustarGrupo: 0 = avance, -1 / 1 = giros, 2 = retroceso
    }

    private enum class Paso { INTRO, REACCION, ARRANQUE, AVANCE, GIRO_IZQ, GIRO_DER, RETROCESO, GIRO_VISUAL, RESUMEN, GIRO_90 }

    private val principal = Handler(Looper.getMainLooper())   // las pruebas con tiempo (se cancelan todas con pararTodo)
    private val reloj = Handler(Looper.getMainLooper())       // solo refresca el chip y el aviso de conexión (no se cancela con pararTodo)
    private lateinit var logica: ControlLogica               // la conexión COMPARTIDA (SesionCarro.lograr)
    private lateinit var guardada: CalibracionGuardada

    private lateinit var aviso: AvisoBanda
    private lateinit var chipConexion: ChipEstado
    private lateinit var textoPaso: TextView
    private lateinit var progreso: LinearProgressIndicator
    private lateinit var ilustracion: IlustracionCalibracion
    private lateinit var scroll: ScrollView
    private lateinit var textoTitulo: TextView
    private lateinit var textoInstruccion: TextView
    private lateinit var textoNota: TextView
    private lateinit var textoDatos: TextView
    private lateinit var campoLayout: TextInputLayout
    private lateinit var campoResultado: EditText
    private lateinit var botonProbar: MaterialButton
    private lateinit var botonYa: MaterialButton
    private lateinit var botonGuardar: MaterialButton
    private lateinit var botonRepetir: MaterialButton
    private lateinit var botonAtras: MaterialButton
    private lateinit var botonSiguiente: MaterialButton
    private lateinit var botonUsar: MaterialButton
    private lateinit var botonCopiar: MaterialButton
    private lateinit var botonBorrarCal: MaterialButton
    private lateinit var botonParo: MaterialButton
    private lateinit var botonAjuste: MaterialButton            // atajo de la primera pantalla al ajuste fino
    private lateinit var panelAjuste: View                      // perfil, lado y «¿cómo giró?» del ajuste fino
    private lateinit var botonPerfilAuto: MaterialButton
    private lateinit var botonPerfilManual: MaterialButton
    private lateinit var botonLadoIzq: MaterialButton
    private lateinit var botonLadoDer: MaterialButton
    private lateinit var bloqueGiro: View
    private lateinit var botonesOpcionGiro: List<MaterialButton>   // mucho, un poco, justo, corto (en el orden de CalibracionNativa.GIRO_*)
    private lateinit var botonRestaurar: MaterialButton

    private var paso = Paso.INTRO
    private var soloGiroVisual = false   // se saltó directo al paso 8 (giro con la cámara): el resumen parte de la calibración guardada, no de los valores por defecto
    private var soloAjusteFino = false   // se saltó directo al ajuste fino: el resumen también parte de la calibración guardada
    private var perfilAjuste = ControlLogica.Perfil.AUTO       // el perfil con el que se prueba y al que se le guarda el factor (ajuste fino)
    private var ladoAjuste = CalibracionNativa.LADO_DERECHA    // el lado del giro de 90° que se prueba
    private var resumenAjuste = ""                              // lo que pasó con el último ajuste (se muestra en la tarjeta de datos)

    // Datos anotados.
    private val reacciones = ArrayList<Int>()
    private val toquesArranque = ArrayList<Int>()
    private val avance = Array(TIEMPOS_AVANCE.size) { ArrayList<Int>() }      // mm por prueba
    private val giroIzq = Array(TIEMPOS_GIRO.size) { ArrayList<Int>() }       // grados x10
    private val giroDer = Array(TIEMPOS_GIRO.size) { ArrayList<Int>() }
    private val retroceso = Array(TIEMPOS_AVANCE.size) { ArrayList<Int>() }   // mm hacia atrás por prueba (opcional)
    private val fovReal = ArrayList<Int>()      // grados x10 que el usuario midió en el piso en cada giro de la prueba visual
    private val fovVisual = ArrayList<Int>()    // grados x10 que la cámara vio girar en ese mismo giro
    private var visualActualX10 = 0             // lo que la cámara va viendo en el giro de esta prueba
    private lateinit var camara: CamaraObstaculos

    // Estado de la prueba en curso.
    private var corriendo = false            // el carro se está moviendo por una prueba de tiempo fijo
    private var esperandoDato = false        // la prueba terminó: falta anotar el resultado
    private var estadoReaccion = 0           // 0 = parado, 1 = esperando que se ponga verde, 2 = verde (toca ya)
    private var tVerdeMs = 0L
    private var arrancando = false           // prueba de arranque en curso
    private var t0ArranqueMs = 0L
    private var datoMostrado = false         // para desplazar la pantalla hasta el campo solo la primera vez que aparece
    private var conectadoDesdeMs = 0L        // desde cuándo hay conexión (para avisar si el carro no confirma el perfil AUTO)

    private var resultado: IntArray? = null  // calibración calculada en el RESUMEN
    private var textoResumen = ""

    /** La sesión compartida avisa (conexión, perfil, latencia): se redibuja el chip y se habilitan o no los botones. */
    private val alCambiar: () -> Unit = { mostrarConexion() }

    /** Cada medio segundo: el aviso de «sin conexión» depende del tiempo que lleva sin conectar. */
    private val tareaReloj = object : Runnable {
        override fun run() {
            actualizarChipYAviso()
            reloj.postDelayed(this, 500)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_calibracion)
        Diseno.barras(this)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        guardada = CalibracionGuardada(this)

        aviso = findViewById(R.id.calib_aviso)
        chipConexion = findViewById(R.id.calib_chip_conexion)
        textoPaso = findViewById(R.id.texto_paso)
        progreso = findViewById(R.id.calib_progreso)
        ilustracion = findViewById(R.id.calib_ilustracion)
        scroll = findViewById(R.id.calib_scroll)
        textoTitulo = findViewById(R.id.texto_titulo_paso)
        textoInstruccion = findViewById(R.id.texto_instruccion)
        textoNota = findViewById(R.id.texto_nota)
        textoDatos = findViewById(R.id.texto_datos)
        campoLayout = findViewById(R.id.calib_campo_layout)
        campoResultado = findViewById(R.id.campo_resultado)
        botonProbar = findViewById(R.id.boton_probar)
        botonYa = findViewById(R.id.boton_ya)
        botonGuardar = findViewById(R.id.boton_guardar)
        botonRepetir = findViewById(R.id.boton_repetir)
        botonAtras = findViewById(R.id.boton_atras)
        botonSiguiente = findViewById(R.id.boton_siguiente)
        botonUsar = findViewById(R.id.boton_usar)
        botonCopiar = findViewById(R.id.boton_copiar)
        botonBorrarCal = findViewById(R.id.boton_borrar_cal)
        botonParo = findViewById(R.id.boton_paro)
        botonAjuste = findViewById(R.id.boton_ajuste)
        panelAjuste = findViewById(R.id.aj_panel)
        botonPerfilAuto = findViewById(R.id.aj_perfil_auto)
        botonPerfilManual = findViewById(R.id.aj_perfil_manual)
        botonLadoIzq = findViewById(R.id.aj_lado_izq)
        botonLadoDer = findViewById(R.id.aj_lado_der)
        bloqueGiro = findViewById(R.id.aj_bloque_giro)
        botonesOpcionGiro = listOf(R.id.aj_op_mucho, R.id.aj_op_poco, R.id.aj_op_justo, R.id.aj_op_corto).map { findViewById<MaterialButton>(it) }
        botonRestaurar = findViewById(R.id.aj_restaurar)
        findViewById<TextView>(R.id.barra_titulo).setText(R.string.cal_titulo)
        findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { finish() }
        // Para pruebas con adb: la dirección del carro (el servidor falso) se guarda en Ajustes, como hace MainActivity con ws_url.
        intent.getStringExtra("direccion")?.let { Ajustes(this).direccionCarro = it }

        logica = SesionCarro.lograr(this)   // la conexión compartida: la abre y la cierra la sesión (SesionCarro.entrar / salir)
        val configVision = ConfigVision(this)
        camara = CamaraObstaculos(this, { configVision.comoArreglo() }, { intArrayOf(configVision.anchoCuadro, configVision.altoCuadro) })

        botonParo.setOnClickListener {
            Haptica.corta(this)
            pararTodo("PARO")
        }
        botonProbar.setOnClickListener { alProbar() }
        botonYa.setOnClickListener { alTocar() }
        botonGuardar.setOnClickListener { guardarDato() }
        botonRepetir.setOnClickListener { borrarUltimoDato() }
        botonAtras.setOnClickListener { cambiarPaso(-1) }
        botonSiguiente.setOnClickListener { cambiarPaso(1) }
        botonUsar.setOnClickListener { usarValores() }
        botonCopiar.setOnClickListener { copiarTexto() }
        botonBorrarCal.setOnClickListener { borrarCalibracion() }
        botonAjuste.setOnClickListener {
            soloAjusteFino = true
            cambiarPaso(Paso.GIRO_90.ordinal - Paso.INTRO.ordinal)
        }
        botonPerfilAuto.setOnClickListener { elegirPerfilAjuste(ControlLogica.Perfil.AUTO) }
        botonPerfilManual.setOnClickListener { elegirPerfilAjuste(ControlLogica.Perfil.MANUAL) }
        botonLadoIzq.setOnClickListener { elegirLadoAjuste(CalibracionNativa.LADO_IZQUIERDA) }
        botonLadoDer.setOnClickListener { elegirLadoAjuste(CalibracionNativa.LADO_DERECHA) }
        for (opcion in botonesOpcionGiro.indices) {
            botonesOpcionGiro[opcion].setOnClickListener { aplicarGradosGiro(CalibracionNativa.gradosDeOpcionGiro(opcion)) }
        }
        botonRestaurar.setOnClickListener { restaurarAjuste() }
        campoResultado.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(texto: CharSequence?, inicio: Int, cuenta: Int, despues: Int) {}
            override fun onTextChanged(texto: CharSequence?, inicio: Int, antes: Int, cuenta: Int) {
                campoLayout.error = null   // al volver a escribir se quita el aviso de «número no válido»
            }
            override fun afterTextChanged(texto: Editable?) {}
        })
        mostrarPaso()
        PantallasLog.abierta("CALIBRACION")
    }

    override fun onResume() {
        super.onResume()
        SesionCarro.escuchar(alCambiar)
        SesionCarro.entrar(this, SesionNativa.USUARIO_CALIBRACION)
        logica.pedirPerfil(perfilEsperado())   // la calibración en el perfil AUTO (210), que es la velocidad a la que andan las rutas; el ajuste fino puede usar también MANUAL
        reloj.removeCallbacks(tareaReloj)
        reloj.post(tareaReloj)
        if (paso == Paso.GIRO_VISUAL) {
            abrirCamaraSiHayPermiso()
        }
        mostrarConexion()
    }

    /** Abre la cámara (pidiendo el permiso si falta) para la prueba del giro visual. */
    private fun abrirCamaraSiHayPermiso() {
        if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.CAMERA), VisionActivity.PEDIDO_PERMISO_CAMARA)
            return
        }
        camara.fovX10 = guardada.fovCalibradoX10()
        camara.abrir()
    }

    override fun onRequestPermissionsResult(codigo: Int, permisos: Array<out String>, resultados: IntArray) {
        super.onRequestPermissionsResult(codigo, permisos, resultados)
        if (codigo == VisionActivity.PEDIDO_PERMISO_CAMARA && resultados.isNotEmpty() && resultados[0] == PackageManager.PERMISSION_GRANTED && paso == Paso.GIRO_VISUAL) {
            abrirCamaraSiHayPermiso()
            mostrarPaso()
        }
    }

    override fun onPause() {
        camara.cerrar()   // la cámara se libera al salir
        pararTodo("pantalla en segundo plano")   // STOP (logica.parar) y se cancelan las pruebas en curso
        reloj.removeCallbacks(tareaReloj)
        SesionCarro.dejarDeEscuchar(alCambiar)
        SesionCarro.salir(SesionNativa.USUARIO_CALIBRACION)   // la sesión cierra la conexión si nadie más la usa
        super.onPause()
    }

    override fun onDestroy() {
        principal.removeCallbacksAndMessages(null)
        reloj.removeCallbacksAndMessages(null)
        super.onDestroy()
    }

    // ── Conexión y seguridad ──────────────────────────────────────────────

    private fun conectado() = logica.conexion == ControlLogica.Conexion.CONECTADO

    /** El perfil que debe tener el carro en el paso actual: AUTO siempre, salvo en el ajuste fino, donde es el que se eligió. */
    private fun perfilEsperado() = if (paso == Paso.GIRO_90) perfilAjuste else ControlLogica.Perfil.AUTO

    /** Conectado Y con el perfil esperado confirmado por el carro: sin eso las pruebas medirían otra velocidad (firmware viejo o perfil equivocado). */
    private fun listoParaProbar() = conectado() && logica.perfilConfirmado == perfilEsperado()

    private fun mostrarConexion() {
        actualizarChipYAviso()
        actualizarBotones()
    }

    /** El chip de conexión (arriba a la derecha) y el aviso en banda: sin conexión dice qué pasó y qué hacer, con el botón «Conectar». */
    private fun actualizarChipYAviso() {
        val ahora = SystemClock.elapsedRealtime()
        if (conectado()) {
            if (conectadoDesdeMs == 0L) {
                conectadoDesdeMs = ahora
            }
        } else {
            conectadoDesdeMs = 0L
        }
        when (logica.conexion) {
            ControlLogica.Conexion.CONECTADO -> {
                if (logica.perfilConfirmado == perfilEsperado()) {
                    chipConexion.poner(Nivel.OK, getString(R.string.conexion_conectado) + " · " + (if (perfilEsperado() == ControlLogica.Perfil.AUTO) getString(R.string.cal_chip_auto) else "MANUAL") + (logica.latenciaMs?.let { " · $it ms" } ?: ""))
                } else {
                    chipConexion.poner(Nivel.AVISO, getString(R.string.cal_chip_sin_confirmar))
                }
            }
            ControlLogica.Conexion.RECONECTANDO -> chipConexion.poner(Nivel.AVISO, getString(R.string.conexion_reconectando))
            else -> if (logica.msSinConexion > 0) {
                chipConexion.poner(Nivel.AVISO, getString(R.string.conexion_conectando))
            } else {
                chipConexion.poner(Nivel.NEUTRO, getString(R.string.conexion_sin))
            }
        }
        if (!conectado()) {
            if (logica.msSinConexion > InicioFragment.ESPERA_ANTES_DE_AVISAR_MS) {
                val m = if (RedCarro.enRedDelCarro(this)) Mensajes.carroNoResponde() else Mensajes.sinRedDelCarro()
                aviso.mostrar(Nivel.ERROR, m.quePaso, m.queHacer, getString(R.string.conectar)) { SesionCarro.conectarAhora() }
            } else {
                aviso.ocultar()
            }
        } else if (logica.perfilConfirmado != perfilEsperado() && conectadoDesdeMs != 0L && ahora - conectadoDesdeMs > 5000L) {
            val m = Mensajes.perfilSinConfirmar()
            aviso.mostrar(Nivel.AVISO, m.quePaso, m.queHacer)
        } else {
            aviso.ocultar()
        }
    }

    /** STOP inmediato y cancela cualquier prueba o temporizador en curso. */
    private fun pararTodo(motivo: String) {
        principal.removeCallbacksAndMessages(null)
        logica.parar()
        if (corriendo || arrancando || estadoReaccion != 0) {
            Log.i(CalibracionGuardada.ETIQUETA, "prueba cancelada: $motivo")
        }
        corriendo = false
        arrancando = false
        estadoReaccion = 0
        mostrarPaso()
    }

    // ── Pasos ─────────────────────────────────────────────────────────────

    private fun cambiarPaso(delta: Int) {
        if (corriendo || arrancando || estadoReaccion != 0) {
            return
        }
        val pasos = Paso.values()
        val nuevo = (paso.ordinal + delta).coerceIn(0, pasos.size - 1)
        if (pasos[nuevo] == Paso.REACCION) {   // empieza una calibración completa: parte de los valores por defecto, no de la guardada
            soloGiroVisual = false
            soloAjusteFino = false
        }
        if (pasos[nuevo] == Paso.RESUMEN || pasos[nuevo] == Paso.GIRO_VISUAL) {
            calcularResumen()   // la prueba visual usa la calibración medida hasta ahora
        }
        if (pasos[nuevo] == Paso.GIRO_VISUAL) {
            paso = pasos[nuevo]
            abrirCamaraSiHayPermiso()
        } else {
            camara.cerrar()
        }
        paso = pasos[nuevo]
        esperandoDato = false
        resumenAjuste = ""
        campoResultado.setText("")
        logica.pedirPerfil(perfilEsperado())   // al entrar o salir del ajuste fino el carro cambia de perfil si hace falta
        mostrarPaso()
        scroll.scrollTo(0, 0)   // cada paso empieza arriba
    }

    /** Cuántas pruebas se han anotado en la lista de listas. */
    private fun cuantas(listas: Array<ArrayList<Int>>) = listas.sumOf { it.size }

    private fun totalPruebas() = TIEMPOS_AVANCE.size * REPETICIONES

    private fun pasoCompleto(): Boolean = when (paso) {
        Paso.INTRO -> true
        Paso.REACCION -> reacciones.size >= TOQUES_REACCION
        Paso.ARRANQUE -> toquesArranque.size >= TOQUES_ARRANQUE
        Paso.AVANCE -> cuantas(avance) >= totalPruebas()
        Paso.GIRO_IZQ -> cuantas(giroIzq) >= totalPruebas()
        Paso.GIRO_DER -> cuantas(giroDer) >= totalPruebas()
        Paso.RETROCESO -> true     // es opcional: si no se mide, el retroceso usa el valor del avance
        Paso.GIRO_VISUAL -> true   // es opcional
        Paso.RESUMEN -> true
        Paso.GIRO_90 -> true
    }

    /** La lista de listas del paso actual (avance o giro) o null. */
    private fun datosDelPaso(): Array<ArrayList<Int>>? = when (paso) {
        Paso.AVANCE -> avance
        Paso.GIRO_IZQ -> giroIzq
        Paso.GIRO_DER -> giroDer
        Paso.RETROCESO -> retroceso
        else -> null
    }

    /** Tiempo (ms) y comando de la prueba que toca ahora en un paso de avance o giro. */
    private fun tiempoActual(): Int {
        val datos = datosDelPaso() ?: return 0
        val indice = (cuantas(datos) / REPETICIONES).coerceAtMost(TIEMPOS_AVANCE.size - 1)
        return if (esPasoLineal()) TIEMPOS_AVANCE[indice] else TIEMPOS_GIRO[indice]
    }

    /** true en los pasos donde se mide una distancia en cm (avance y retroceso); false en los de giro. */
    private fun esPasoLineal() = paso == Paso.AVANCE || paso == Paso.RETROCESO

    private fun comandoDelPaso() = when (paso) {
        Paso.AVANCE -> "FORWARD"
        Paso.RETROCESO -> "REVERSE"
        Paso.GIRO_IZQ -> "LEFT"
        else -> "RIGHT"
    }

    /** La escena de la ilustración que corresponde al paso actual. */
    private fun escenaDelPaso(): IlustracionCalibracion.Escena = when (paso) {
        Paso.INTRO -> IlustracionCalibracion.Escena.INTRO
        Paso.REACCION -> IlustracionCalibracion.Escena.REACCION
        Paso.ARRANQUE -> IlustracionCalibracion.Escena.ARRANQUE
        Paso.AVANCE -> IlustracionCalibracion.Escena.AVANCE
        Paso.GIRO_IZQ -> IlustracionCalibracion.Escena.GIRO_IZQ
        Paso.GIRO_DER -> IlustracionCalibracion.Escena.GIRO_DER
        Paso.RETROCESO -> IlustracionCalibracion.Escena.RETROCESO
        Paso.GIRO_VISUAL -> IlustracionCalibracion.Escena.GIRO_VISUAL
        Paso.RESUMEN -> IlustracionCalibracion.Escena.RESUMEN
        Paso.GIRO_90 -> IlustracionCalibracion.Escena.GIRO_90
    }

    /** Pone la pista del campo de resultado solo si cambió (cambiarla en cada cuadro de la prueba visual haría parpadear el campo). */
    private fun ponerPista(texto: String) {
        if (campoLayout.hint?.toString() != texto) {
            campoLayout.hint = texto
        }
    }

    private fun mostrarPaso() {
        val datos = datosDelPaso()
        botonProbar.visibility = View.GONE
        botonYa.visibility = View.GONE
        campoLayout.visibility = View.GONE
        botonGuardar.visibility = View.GONE
        botonRepetir.visibility = View.GONE
        botonUsar.visibility = View.GONE
        botonCopiar.visibility = View.GONE
        botonBorrarCal.visibility = View.GONE
        textoNota.visibility = View.GONE
        botonAjuste.visibility = View.GONE
        panelAjuste.visibility = View.GONE
        bloqueGiro.visibility = View.GONE

        textoPaso.text = getString(R.string.cal_paso, paso.ordinal + 1, Paso.values().size)
        if (progreso.max != Paso.values().size) {
            progreso.max = Paso.values().size
        }
        if (progreso.progress != paso.ordinal + 1) {
            progreso.setProgressCompat(paso.ordinal + 1, true)
        }
        ilustracion.escena = escenaDelPaso()
        ilustracion.enMarcha = corriendo || arrancando || estadoReaccion != 0

        when (paso) {
            Paso.INTRO -> {
                textoTitulo.setText(R.string.cal_t_intro)
                textoInstruccion.setText(R.string.cal_i_intro)
                textoNota.text = getString(R.string.cal_n_intro) + "\n" + getString(R.string.cal_n_intro_giro)
                textoNota.visibility = View.VISIBLE
                textoDatos.text = (guardada.avisoSiFalta() ?: "Ya hay una calibración completa guardada.") +
                    (guardada.avisoRetroceso()?.let { "\n" + it } ?: "")
                botonProbar.visibility = View.VISIBLE
                botonProbar.setText(R.string.cal_b_solo_giro)
                botonAjuste.visibility = View.VISIBLE
            }
            Paso.REACCION -> {
                textoTitulo.setText(R.string.cal_t_reaccion)
                textoInstruccion.setText(R.string.cal_i_reaccion)
                textoNota.setText(R.string.cal_n_reaccion)
                textoNota.visibility = View.VISIBLE
                botonProbar.visibility = View.VISIBLE
                botonProbar.setText(R.string.cal_b_empezar)
                botonYa.visibility = View.VISIBLE
                botonYa.setText(if (estadoReaccion == 2) R.string.cal_b_ya_toca else if (estadoReaccion == 1) R.string.cal_b_ya_espera else R.string.cal_b_ya_empezar)
                pintarYa(estadoReaccion == 2)
                textoDatos.text = "Reacciones (ms): " + reacciones.joinToString(", ") +
                    "\nPromedio: " + (if (reacciones.isEmpty()) "-" else "${CalibracionNativa.promedioSinAtipicos(reacciones.toIntArray())} ms")
            }
            Paso.ARRANQUE -> {
                textoTitulo.setText(R.string.cal_t_arranque)
                textoInstruccion.setText(R.string.cal_i_arranque)
                textoNota.setText(R.string.cal_n_arranque)
                textoNota.visibility = View.VISIBLE
                botonProbar.visibility = View.VISIBLE
                botonProbar.setText(R.string.cal_b_probar_arranque)
                botonYa.visibility = View.VISIBLE
                botonYa.setText(R.string.cal_b_ya_movio)
                pintarYa(arrancando)
                botonRepetir.visibility = View.VISIBLE
                textoDatos.text = "Tiempos (ms, con tu reacción dentro): " + toquesArranque.joinToString(", ") +
                    "\nFaltan: ${(TOQUES_ARRANQUE - toquesArranque.size).coerceAtLeast(0)}"
            }
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER, Paso.RETROCESO -> {
                val hechas = cuantas(datos!!)
                textoTitulo.setText(when (paso) {
                    Paso.AVANCE -> R.string.cal_t_avance
                    Paso.GIRO_IZQ -> R.string.cal_t_giro_izq
                    Paso.GIRO_DER -> R.string.cal_t_giro_der
                    else -> R.string.cal_t_retroceso
                })
                textoInstruccion.setText(when (paso) {
                    Paso.RETROCESO -> R.string.cal_i_retroceso
                    Paso.AVANCE -> R.string.cal_i_avance
                    else -> R.string.cal_i_giro
                })
                textoNota.setText(when (paso) {
                    Paso.RETROCESO -> R.string.cal_n_retroceso
                    Paso.AVANCE -> R.string.cal_n_avance
                    else -> R.string.cal_n_giro
                })
                textoNota.visibility = View.VISIBLE
                botonProbar.visibility = View.VISIBLE
                botonProbar.text = if (hechas >= totalPruebas()) getString(R.string.cal_b_todas) else
                    getString(R.string.cal_b_probar_n, tiempoActual(), hechas + 1, totalPruebas())
                if (esperandoDato) {
                    campoLayout.visibility = View.VISIBLE
                    ponerPista(getString(when (paso) {
                        Paso.AVANCE -> R.string.cal_h_cm
                        Paso.RETROCESO -> R.string.cal_h_cm_atras
                        else -> R.string.cal_h_grados
                    }))
                    botonGuardar.visibility = View.VISIBLE
                }
                botonRepetir.visibility = View.VISIBLE
                textoDatos.text = armarTabla(datos)
            }
            Paso.GIRO_VISUAL -> {
                textoTitulo.setText(R.string.cal_t_giro_visual)
                textoInstruccion.setText(R.string.cal_i_giro_visual)
                textoNota.setText(R.string.cal_n_giro_visual)
                textoNota.visibility = View.VISIBLE
                botonProbar.visibility = View.VISIBLE
                botonProbar.text = if (fovReal.size >= 3) getString(R.string.cal_b_visual_listo) else getString(R.string.cal_b_visual_n, fovReal.size + 1)
                if (esperandoDato) {
                    campoLayout.visibility = View.VISIBLE
                    ponerPista(getString(R.string.cal_h_grados_reales))
                    botonGuardar.visibility = View.VISIBLE
                }
                botonRepetir.visibility = View.VISIBLE
                val sb = StringBuilder()
                sb.append("Cámara: " + (if (camara.abierta) "abierta" else if (camara.error.isNotEmpty()) "no se pudo abrir (revisa el permiso de cámara y cierra otras apps que la usen)" else "abriendo...") + "\n")
                sb.append(String.format(Locale.US, "Campo de visión actual: %.1f°\n", camara.fovEfectivoX10 / 10.0))
                if (corriendo || esperandoDato) {
                    sb.append(String.format(Locale.US, "La cámara ve que giró: %.1f°\n", visualActualX10 / 10.0))
                }
                for (i in fovReal.indices) {
                    sb.append(String.format(Locale.US, "Prueba %d: real %.1f°, cámara %.1f°\n", i + 1, fovReal[i] / 10.0, fovVisual[i] / 10.0))
                }
                textoDatos.text = sb.toString()
            }
            Paso.RESUMEN -> {
                textoTitulo.setText(R.string.cal_t_resumen)
                textoInstruccion.setText(R.string.cal_i_resumen)
                textoDatos.text = textoResumen
                botonUsar.visibility = View.VISIBLE
                botonCopiar.visibility = View.VISIBLE
                botonBorrarCal.visibility = View.VISIBLE
            }
            Paso.GIRO_90 -> mostrarGiro90()
        }
        // Cuando aparece el campo del resultado, la pantalla baja hasta el botón «Guardar este resultado» (solo esa primera vez).
        if (esperandoDato && !datoMostrado) {
            scroll.post { scroll.smoothScrollTo(0, (botonGuardar.bottom - scroll.height).coerceAtLeast(0)) }
        }
        datoMostrado = esperandoDato
        actualizarBotones()
    }

    /** Tabla de lo anotado: una línea por tiempo con sus repeticiones. */
    private fun armarTabla(datos: Array<ArrayList<Int>>): String {
        val tiempos = if (esPasoLineal()) TIEMPOS_AVANCE else TIEMPOS_GIRO
        val sb = StringBuilder()
        for (i in tiempos.indices) {
            val valores = datos[i].map {
                if (esPasoLineal()) String.format(Locale.US, "%.1f cm", it / 10.0) else String.format(Locale.US, "%.0f°", it / 10.0)
            }
            sb.append(String.format(Locale.US, "%d ms: %s\n", tiempos[i], if (valores.isEmpty()) "-" else valores.joinToString("   ")))
        }
        return sb.toString()
    }

    // ── Aspecto de los botones ────────────────────────────────────────────

    /** Habilita o deshabilita un botón. Como los estilos de la app usan un color fijo, el botón deshabilitado se atenúa con la transparencia (38 %). */
    private fun habilitar(boton: View, activo: Boolean) {
        boton.isEnabled = activo
        boton.alpha = if (activo) 1f else 0.38f
    }

    /** Un botón se ve como PRIMARIO (relleno con el acento) o como SECUNDARIO (solo borde y texto del acento). Solo uno es primario a la vez. */
    private fun resaltar(boton: MaterialButton, primario: Boolean) {
        if (primario) {
            boton.backgroundTintList = ColorStateList.valueOf(ContextCompat.getColor(this, R.color.acento))
            boton.setTextColor(ContextCompat.getColor(this, R.color.sobre_acento))
            boton.strokeWidth = 0
        } else {
            boton.backgroundTintList = ColorStateList.valueOf(Color.TRANSPARENT)
            boton.setTextColor(ContextCompat.getColor(this, R.color.acento_texto))
            boton.strokeColor = ColorStateList.valueOf(ContextCompat.getColor(this, R.color.borde_fuerte))
            boton.strokeWidth = Diseno.dp(this, 1)
        }
    }

    /** El botón grande de reacción y arranque: verde (con su borde) cuando hay que tocar YA; gris cuando todavía no. */
    private fun pintarYa(activo: Boolean) {
        if (activo) {
            botonYa.backgroundTintList = ColorStateList.valueOf(Diseno.fondoDe(this, Nivel.OK))
            botonYa.setTextColor(Diseno.colorDe(this, Nivel.OK))
            botonYa.strokeColor = ColorStateList.valueOf(Diseno.colorDe(this, Nivel.OK))
            botonYa.strokeWidth = Diseno.dp(this, 2)
        } else {
            botonYa.backgroundTintList = ColorStateList.valueOf(Diseno.fondoDe(this, Nivel.NEUTRO))
            botonYa.setTextColor(Diseno.colorDe(this, Nivel.NEUTRO))
            botonYa.strokeColor = ColorStateList.valueOf(ContextCompat.getColor(this, R.color.borde_fuerte))
            botonYa.strokeWidth = Diseno.dp(this, 1)
        }
    }

    private fun actualizarBotones() {
        val ocupado = corriendo || arrancando || estadoReaccion != 0
        val probarActivo = when (paso) {
            Paso.INTRO -> true
            Paso.REACCION -> estadoReaccion == 0 && reacciones.size < TOQUES_REACCION
            Paso.ARRANQUE -> listoParaProbar() && !arrancando && toquesArranque.size < TOQUES_ARRANQUE
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER, Paso.RETROCESO ->
                listoParaProbar() && !corriendo && !esperandoDato && cuantas(datosDelPaso()!!) < totalPruebas()
            Paso.GIRO_VISUAL -> listoParaProbar() && camara.abierta && !corriendo && !esperandoDato && fovReal.size < 3
            Paso.GIRO_90 -> listoParaProbar() && !corriendo && !esperandoDato && guardada.estaCompleta()
            else -> false
        }
        habilitar(botonProbar, probarActivo)
        habilitar(botonYa, (paso == Paso.REACCION && estadoReaccion != 0) || (paso == Paso.ARRANQUE && arrancando))
        habilitar(botonRepetir, !ocupado && when (paso) {
            Paso.ARRANQUE -> toquesArranque.isNotEmpty()
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER, Paso.RETROCESO -> cuantas(datosDelPaso()!!) > 0
            Paso.GIRO_VISUAL -> fovReal.isNotEmpty()
            else -> false
        })
        habilitar(botonAtras, !ocupado && paso != Paso.INTRO)
        habilitar(botonSiguiente, !ocupado && paso != Paso.GIRO_90 && pasoCompleto() && !esperandoDato)
        botonSiguiente.visibility = if (paso == Paso.GIRO_90) View.GONE else View.VISIBLE
        botonSiguiente.setText(when (paso) {
            Paso.INTRO -> R.string.cal_b_empezar
            Paso.GIRO_VISUAL -> R.string.cal_b_ver_resumen
            Paso.RESUMEN -> R.string.cal_b_ajuste_fino
            else -> R.string.cal_siguiente
        })

        // Solo UN botón se ve como primario: el que toca tocar ahora. Con una prueba en marcha, ninguno (lo importante es el PARO y el botón grande).
        val destacado: MaterialButton? = when {
            ocupado -> null
            paso == Paso.RESUMEN -> botonUsar
            esperandoDato -> if (paso == Paso.GIRO_90) null else botonGuardar   // en el ajuste fino las opciones de «¿cómo giró?» son las que se tocan
            paso != Paso.INTRO && probarActivo -> botonProbar
            else -> botonSiguiente
        }
        for (boton in arrayOf(botonProbar, botonGuardar, botonSiguiente, botonUsar)) {
            resaltar(boton, boton === destacado)
        }
    }

    // ── Pruebas ───────────────────────────────────────────────────────────

    private fun alProbar() {
        when (paso) {
            Paso.INTRO -> {
                soloGiroVisual = true
                cambiarPaso(Paso.GIRO_VISUAL.ordinal - Paso.INTRO.ordinal)
            }
            Paso.REACCION -> empezarReaccion()
            Paso.ARRANQUE -> empezarArranque()
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER, Paso.RETROCESO -> empezarPruebaDeTiempo()
            Paso.GIRO_VISUAL -> empezarGiroVisual()
            Paso.GIRO_90 -> empezarGiro90()
            else -> {}
        }
    }

    private fun alTocar() {
        when (paso) {
            Paso.REACCION -> tocarReaccion()
            Paso.ARRANQUE -> tocarArranque()
            else -> {}
        }
    }

    private fun empezarReaccion() {
        estadoReaccion = 1
        mostrarPaso()
        principal.postDelayed({
            estadoReaccion = 2
            tVerdeMs = SystemClock.elapsedRealtime()
            mostrarPaso()
        }, 1000L + Random.nextLong(2000L))
    }

    private fun tocarReaccion() {
        if (estadoReaccion == 1) {   // tocó antes de tiempo: se repite
            principal.removeCallbacksAndMessages(null)
            estadoReaccion = 0
            Toast.makeText(this, "Muy pronto: espera a que se ponga verde", Toast.LENGTH_SHORT).show()
        } else if (estadoReaccion == 2) {
            val ms = (SystemClock.elapsedRealtime() - tVerdeMs).toInt()
            estadoReaccion = 0
            reacciones.add(ms)
            Log.i(CalibracionGuardada.ETIQUETA, "reaccion_ms=$ms")
        }
        mostrarPaso()
    }

    private fun reaccionMs(): Int =
        if (reacciones.isEmpty()) REACCION_POR_DEFECTO_MS else CalibracionNativa.promedioSinAtipicos(reacciones.toIntArray())

    private fun empezarArranque() {
        if (!conectado()) {
            return
        }
        arrancando = true
        t0ArranqueMs = SystemClock.elapsedRealtime()
        logica.presionar("FORWARD")
        principal.postDelayed({
            if (arrancando) {
                arrancando = false
                logica.parar()
                Toast.makeText(this, "No tocaste a tiempo: se detuvo el carro. Repite la prueba.", Toast.LENGTH_LONG).show()
                mostrarPaso()
            }
        }, ESPERA_MAXIMA_ARRANQUE_MS)
        mostrarPaso()
    }

    private fun tocarArranque() {
        if (!arrancando) {
            return
        }
        val ms = (SystemClock.elapsedRealtime() - t0ArranqueMs).toInt()
        arrancando = false
        principal.removeCallbacksAndMessages(null)
        logica.parar()
        toquesArranque.add(ms)
        Log.i(CalibracionGuardada.ETIQUETA, "arranque_toque_ms=$ms reaccion_ms=${reaccionMs()}")
        mostrarPaso()
    }

    /** Manda el comando del paso durante el tiempo fijo y luego STOP. */
    private fun empezarPruebaDeTiempo() {
        if (!conectado() || corriendo) {
            return
        }
        val ms = tiempoActual()
        val comando = comandoDelPaso()
        corriendo = true
        Log.i(CalibracionGuardada.ETIQUETA, "prueba $comando durante $ms ms")
        logica.presionar(comando)
        mostrarPaso()
        principal.postDelayed({
            logica.parar()
            corriendo = false
            esperandoDato = true
            Log.i(CalibracionGuardada.ETIQUETA, "prueba terminada: STOP")
            mostrarPaso()
        }, ms.toLong())
    }

    /** Gira a la derecha lo que la calibración dice que son 90 grados mientras se suma lo que la cámara ve girar. */
    private fun empezarGiroVisual() {
        if (!conectado() || !camara.abierta || corriendo) {
            return
        }
        val ms = CalibracionNativa.gradosAMs(resultado ?: CalibracionNativa.desempaquetar(CalibracionNativa.defecto()).second, 90)
        corriendo = true
        visualActualX10 = 0
        camara.olvidarGiroPendiente()
        Log.i(CalibracionGuardada.ETIQUETA, "prueba de giro visual: RIGHT durante $ms ms")
        logica.presionar("RIGHT")
        mostrarPaso()
        val finMovimiento = SystemClock.elapsedRealtime() + ms
        val tarea = object : Runnable {
            override fun run() {
                camara.tomarGiroNuevo()?.let { visualActualX10 += it.giroX10 }
                val ahora = SystemClock.elapsedRealtime()
                if (ahora < finMovimiento) {
                    mostrarPaso()
                    principal.postDelayed(this, 40)
                } else if (ahora < finMovimiento + 800) {   // el carro sigue girando un poco al frenar: se sigue midiendo
                    logica.parar()
                    mostrarPaso()
                    principal.postDelayed(this, 40)
                } else {
                    corriendo = false
                    esperandoDato = true
                    Log.i(CalibracionGuardada.ETIQUETA, "giro visual terminado: la cámara vio ${visualActualX10 / 10.0} grados")
                    mostrarPaso()
                }
            }
        }
        principal.postDelayed(tarea, 40)
    }

    // ── Ajuste fino: giro de 90° ──────────────────────────────────────────

    /** Un botón de selección (perfil o lado): el elegido lleva un borde del acento y fondo de tarjeta elevada; los demás se ven como secundarios. No es «el primario». */
    private fun marcarSeleccion(boton: MaterialButton, elegido: Boolean) {
        if (elegido) {
            boton.backgroundTintList = ColorStateList.valueOf(ContextCompat.getColor(this, R.color.superficie_alta))
            boton.setTextColor(ContextCompat.getColor(this, R.color.texto))
            boton.strokeColor = ColorStateList.valueOf(ContextCompat.getColor(this, R.color.acento))
            boton.strokeWidth = Diseno.dp(this, 2)
        } else {
            boton.backgroundTintList = ColorStateList.valueOf(Color.TRANSPARENT)
            boton.setTextColor(ContextCompat.getColor(this, R.color.acento_texto))
            boton.strokeColor = ColorStateList.valueOf(ContextCompat.getColor(this, R.color.borde_fuerte))
            boton.strokeWidth = Diseno.dp(this, 1)
        }
    }

    private fun elegirPerfilAjuste(perfil: ControlLogica.Perfil) {
        if (corriendo || esperandoDato || perfil == perfilAjuste) {
            return
        }
        perfilAjuste = perfil
        logica.pedirPerfil(perfilEsperado())
        resumenAjuste = ""
        mostrarPaso()
    }

    private fun elegirLadoAjuste(lado: Int) {
        if (corriendo || esperandoDato || lado == ladoAjuste) {
            return
        }
        ladoAjuste = lado
        resumenAjuste = ""
        mostrarPaso()
    }

    private fun nombreLado(lado: Int) = if (lado < 0) "izquierda" else "derecha"

    /** La pantalla del giro de 90°: panel de perfil y lado, las opciones de «¿cómo giró?» cuando termina la prueba, el campo de grados y lo que se ha ajustado. */
    private fun mostrarGiro90() {
        textoTitulo.setText(R.string.cal_t_giro_90)
        textoInstruccion.setText(R.string.cal_i_giro_90)
        textoNota.text = getString(R.string.cal_n_giro_90) + "\n" + getString(R.string.cal_aj_pilas)
        textoNota.visibility = View.VISIBLE
        panelAjuste.visibility = View.VISIBLE
        marcarSeleccion(botonPerfilAuto, perfilAjuste == ControlLogica.Perfil.AUTO)
        marcarSeleccion(botonPerfilManual, perfilAjuste == ControlLogica.Perfil.MANUAL)
        marcarSeleccion(botonLadoIzq, ladoAjuste == CalibracionNativa.LADO_IZQUIERDA)
        marcarSeleccion(botonLadoDer, ladoAjuste == CalibracionNativa.LADO_DERECHA)
        botonProbar.visibility = View.VISIBLE
        botonProbar.setText(if (ladoAjuste < 0) R.string.cal_b_probar_giro_izq else R.string.cal_b_probar_giro_der)
        if (esperandoDato) {
            bloqueGiro.visibility = View.VISIBLE
            campoLayout.visibility = View.VISIBLE
            ponerPista(getString(R.string.cal_h_grados_giro_90))
            botonGuardar.visibility = View.VISIBLE
            botonGuardar.setText(R.string.cal_b_aplicar_grados)
        } else {
            botonGuardar.setText(R.string.cal_guardar)
        }
        textoDatos.text = textoAjusteGiro()
    }

    private fun textoAjusteGiro(): String {
        if (!guardada.estaCompleta()) {
            return getString(R.string.cal_aj_sin_cal)
        }
        val cal = guardada.leerAjustada(perfilAjuste)
        val sb = StringBuilder()
        sb.append(String.format(Locale.US, "Factor del giro (%s): izquierda %d, derecha %d (1000 = sin corrección)\n", perfilAjuste.texto,
            guardada.factorGiro(perfilAjuste, CalibracionNativa.LADO_IZQUIERDA), guardada.factorGiro(perfilAjuste, CalibracionNativa.LADO_DERECHA)))
        sb.append(String.format(Locale.US, "Un giro de 90° dura: izquierda %d ms, derecha %d ms\n",
            CalibracionNativa.gradosAMs(cal, -90), CalibracionNativa.gradosAMs(cal, 90)))
        if (resumenAjuste.isNotEmpty()) {
            sb.append("\n").append(resumenAjuste)
        }
        return sb.toString()
    }

    /** El carro gira lo que la calibración (con el factor de ese lado y perfil) dice que son 90° y se detiene; después se pregunta cómo giró. */
    private fun empezarGiro90() {
        if (!conectado() || corriendo) {
            return
        }
        val cal = guardada.leerAjustada(perfilAjuste)
        val ms = CalibracionNativa.gradosAMs(cal, ladoAjuste * 90)
        val comando = if (ladoAjuste < 0) "LEFT" else "RIGHT"
        corriendo = true
        resumenAjuste = ""
        Log.i(CalibracionGuardada.ETIQUETA, "ajuste del giro de 90°: $comando durante $ms ms (perfil ${perfilAjuste.name}, factor ${guardada.factorGiro(perfilAjuste, ladoAjuste)})")
        logica.presionar(comando)
        mostrarPaso()
        principal.postDelayed({
            logica.parar()
            corriendo = false
            esperandoDato = true
            mostrarPaso()
        }, ms.toLong())
    }

    /** Corrige el factor del lado probado con los grados que giró de verdad (los de la opción elegida o los que escribió el usuario). */
    private fun aplicarGradosGiro(reales: Int) {
        if (!esperandoDato) {
            return
        }
        val antes = guardada.factorGiro(perfilAjuste, ladoAjuste)
        val msAntes = CalibracionNativa.gradosAMs(guardada.leerAjustada(perfilAjuste), ladoAjuste * 90)
        val (codigo, factor) = CalibracionNativa.factorGiroNuevo(antes, 90, reales).let { Pair(it[0], it[1]) }
        if (codigo != CalibracionNativa.OK) {
            campoLayout.error = getString(R.string.cal_error_numero)
            return
        }
        guardada.guardarFactorGiro(perfilAjuste, ladoAjuste, factor)
        guardada.aplicarAlNavegador()
        val msDespues = CalibracionNativa.gradosAMs(guardada.leerAjustada(perfilAjuste), ladoAjuste * 90)
        resumenAjuste = if (factor == antes) {
            "Giro a la ${nombreLado(ladoAjuste)}: quedó justo, no se cambia nada. Prueba el otro lado si falta."
        } else {
            "Giro a la ${nombreLado(ladoAjuste)}: giró ~$reales° en vez de 90°. El factor pasó de $antes a $factor y el giro de 90° dura $msDespues ms (antes $msAntes ms). Vuelve a probar para comprobar."
        }
        Log.i(CalibracionGuardada.ETIQUETA, "ajuste giro 90°: lado=${nombreLado(ladoAjuste)} perfil=${perfilAjuste.name} reales=$reales factor $antes -> $factor ms $msAntes -> $msDespues")
        esperandoDato = false
        campoResultado.setText("")
        mostrarPaso()
    }

    /** «Restaurar valor por defecto»: los dos lados del perfil vuelven a 1000 (sin corrección). */
    private fun restaurarAjuste() {
        if (corriendo) {
            return
        }
        guardada.restaurarFactoresGiro(perfilAjuste)
        guardada.aplicarAlNavegador()
        resumenAjuste = "Se restauró el valor por defecto del giro en el perfil ${perfilAjuste.texto}."
        esperandoDato = false
        campoResultado.setText("")
        mostrarPaso()
    }

    // ── Datos ─────────────────────────────────────────────────────────────

    private fun guardarDato() {
        if (paso == Paso.GIRO_VISUAL) {
            guardarGiroVisual()
            return
        }
        if (paso == Paso.GIRO_90) {
            val grados = campoResultado.text.toString().trim().replace(',', '.').toDoubleOrNull()
            if (grados == null || grados < 10.0 || grados > 360.0) {
                campoLayout.error = getString(R.string.cal_error_numero)
                return
            }
            aplicarGradosGiro(Math.round(grados).toInt())
            return
        }
        val datos = datosDelPaso() ?: return
        val numero = campoResultado.text.toString().trim().replace(',', '.').toDoubleOrNull()
        if (numero == null || numero <= 0.0 || numero > 1000.0) {
            campoLayout.error = getString(R.string.cal_error_numero)
            return
        }
        val indice = (cuantas(datos) / REPETICIONES).coerceAtMost(TIEMPOS_AVANCE.size - 1)
        val entero = Math.round(numero * 10).toInt()   // mm para avance, grados x10 para giro
        datos[indice].add(entero)
        Log.i(CalibracionGuardada.ETIQUETA, "dato paso=$paso tiempo_ms=${tiempoActual()} valor_x10=$entero")
        esperandoDato = false
        campoResultado.setText("")
        mostrarPaso()
    }

    /** Guarda los grados reales de la prueba visual junto con lo que la cámara vio. */
    private fun guardarGiroVisual() {
        val numero = campoResultado.text.toString().trim().replace(',', '.').toDoubleOrNull()
        if (numero == null || numero <= 0.0 || numero > 720.0) {
            campoLayout.error = getString(R.string.cal_error_numero)
            return
        }
        if (visualActualX10 <= 0) {
            Toast.makeText(this, "La cámara no vio girar: la escena no tiene detalles o no hubo giro. Repite la prueba.", Toast.LENGTH_LONG).show()
            esperandoDato = false
            mostrarPaso()
            return
        }
        fovReal.add(Math.round(numero * 10).toInt())
        fovVisual.add(visualActualX10)
        Log.i(CalibracionGuardada.ETIQUETA, "giro visual: real=${numero} camara=${visualActualX10 / 10.0}")
        esperandoDato = false
        campoResultado.setText("")
        mostrarPaso()
    }

    private fun borrarUltimoDato() {
        if (corriendo || arrancando) {
            return
        }
        when (paso) {
            Paso.GIRO_VISUAL -> {
                if (esperandoDato) {
                    esperandoDato = false
                } else if (fovReal.isNotEmpty()) {
                    fovReal.removeAt(fovReal.size - 1)
                    fovVisual.removeAt(fovVisual.size - 1)
                }
            }
            Paso.ARRANQUE -> if (toquesArranque.isNotEmpty()) toquesArranque.removeAt(toquesArranque.size - 1)
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER, Paso.RETROCESO -> {
                val datos = datosDelPaso()!!
                if (esperandoDato) {
                    esperandoDato = false   // se cancela la prueba que esperaba su dato: se repite
                } else {
                    val ultimo = datos.indexOfLast { it.isNotEmpty() }
                    if (ultimo >= 0) {
                        datos[ultimo].removeAt(datos[ultimo].size - 1)
                    }
                }
            }
            else -> {}
        }
        mostrarPaso()
    }

    // ── Cálculo y resumen ─────────────────────────────────────────────────

    /** Calcula la calibración con lo anotado (en C) y arma el texto del resumen. */
    private fun calcularResumen() {
        val sb = StringBuilder()
        // Si se saltó al paso del giro visual solo para el campo de visión, se parte de lo guardado; si no, de los valores por defecto.
        val partirDeGuardada = soloGiroVisual || soloAjusteFino
        var cal = if (partirDeGuardada) guardada.leer() else CalibracionNativa.desempaquetar(CalibracionNativa.defecto()).second
        if (partirDeGuardada) {
            sb.append("Se conservan el arranque, el avance y los giros de la calibración guardada" + (if (soloGiroVisual) " (solo se ajusta el campo de visión).\n" else ".\n"))
        }

        if (toquesArranque.size >= 3) {
            val medio = CalibracionNativa.promedioSinAtipicos(toquesArranque.toIntArray())
            val arranque = (medio - reaccionMs()).coerceAtLeast(0)
            val (codigo, nueva) = CalibracionNativa.desempaquetar(CalibracionNativa.ponerArranque(cal, arranque))
            if (codigo == CalibracionNativa.OK) {
                cal = nueva
                sb.append("Arranque: $arranque ms (toque ${medio} ms menos reacción ${reaccionMs()} ms)\n")
            } else {
                sb.append("Arranque: ${CalibracionNativa.textoError(codigo)}\n")
            }
        } else {
            sb.append("Arranque: sin medir (se usa 0 ms)\n")
        }

        cal = ajustarGrupo(sb, cal, "Avance", avance, TIEMPOS_AVANCE, 0)
        cal = ajustarGrupo(sb, cal, "Giro izquierda", giroIzq, TIEMPOS_GIRO, CalibracionNativa.LADO_IZQUIERDA)
        cal = ajustarGrupo(sb, cal, "Giro derecha", giroDer, TIEMPOS_GIRO, CalibracionNativa.LADO_DERECHA)
        if (cuantas(retroceso) > 0) {
            cal = ajustarGrupo(sb, cal, "Retroceso", retroceso, TIEMPOS_AVANCE, LADO_ATRAS)
        } else if (cal[CalibracionNativa.CAL_MEDIDOS] and CalibracionNativa.MEDIDO_ATRAS == 0) {
            sb.append("Retroceso: sin medir (los reintentos de esquive usan el valor del avance; el carro retrocede 15 cm a ciegas)\n")
        }

        if (fovReal.isNotEmpty()) {
            // El campo de visión con el que se midió la prueba (el de la cámara si no estaba calibrado) es el que se ajusta.
            cal[CalibracionNativa.CAL_FOV_GRADOS_X10] = camara.fovEfectivoX10
            val (codigo, nueva) = CalibracionNativa.desempaquetar(CalibracionNativa.ajustarFov(cal, fovReal.sum(), fovVisual.sum()))
            if (codigo == CalibracionNativa.OK) {
                cal = nueva
                sb.append(String.format(Locale.US, "Giro visual: campo de visión %.1f° (real %.1f° contra %.1f° que vio la cámara en %d pruebas)\n",
                    cal[CalibracionNativa.CAL_FOV_GRADOS_X10] / 10.0, fovReal.sum() / 10.0, fovVisual.sum() / 10.0, fovReal.size))
            } else {
                sb.append("Giro visual: ${CalibracionNativa.textoError(codigo)}\n")
            }
        } else {
            sb.append("Giro visual: sin medir (se usa el campo de visión por defecto)\n")
        }

        val faltan = CalibracionNativa.faltantes(cal)
        sb.append("\n")
        sb.append(String.format(Locale.US, "ms por cm: %.1f   frenado: %.1f cm\n",
            cal[CalibracionNativa.CAL_MS_POR_CM_X10] / 10.0, cal[CalibracionNativa.CAL_FRENADO_MM] / 10.0))
        sb.append(String.format(Locale.US, "ms por cm hacia atrás: %.1f %s\n", cal[CalibracionNativa.CAL_MS_POR_CM_ATRAS_X10] / 10.0,
            if (cal[CalibracionNativa.CAL_MEDIDOS] and CalibracionNativa.MEDIDO_ATRAS != 0) "(medido)" else "(sin medir: se usa el del avance, ${cal[CalibracionNativa.CAL_MS_POR_CM_X10] / 10.0})"))
        sb.append(String.format(Locale.US, "ms por grado: izq %.1f, der %.1f   frenado: izq %.1f°, der %.1f°\n",
            cal[CalibracionNativa.CAL_MS_GRADO_IZQ_X10] / 10.0, cal[CalibracionNativa.CAL_MS_GRADO_DER_X10] / 10.0,
            cal[CalibracionNativa.CAL_FRENADO_GRADOS_IZQ_X10] / 10.0, cal[CalibracionNativa.CAL_FRENADO_GRADOS_DER_X10] / 10.0))
        sb.append("\nEjemplo: para avanzar 40 cm el carro recibirá ${CalibracionNativa.cmAMs(cal, 40)} ms; " +
            "para girar 90°: izquierda ${CalibracionNativa.gradosAMs(cal, -90)} ms, derecha ${CalibracionNativa.gradosAMs(cal, 90)} ms.\n")
        if (faltan != 0) {
            sb.append("\nOJO: faltan partes por medir; esas usan los valores por defecto.")
        }
        resultado = cal
        textoResumen = sb.toString()
        Log.i(CalibracionGuardada.ETIQUETA, "resumen: " + CalibracionNativa.aTexto(cal).replace('\n', ' '))
    }

    /** Promedia cada tiempo (sin atípicos) y ajusta avance (lado 0) o giro de un lado. */
    private fun ajustarGrupo(sb: StringBuilder, cal: IntArray, nombre: String, datos: Array<ArrayList<Int>>,
                             tiempos: IntArray, lado: Int): IntArray {
        val ms = ArrayList<Int>()
        val valores = ArrayList<Int>()
        for (i in tiempos.indices) {
            if (datos[i].isNotEmpty()) {
                ms.add(tiempos[i])
                valores.add(CalibracionNativa.promedioSinAtipicos(datos[i].toIntArray()))
            }
        }
        if (ms.size < 2) {
            sb.append("$nombre: sin medir (faltan datos de al menos 2 tiempos)\n")
            return cal
        }
        val respuesta = when (lado) {
            0 -> CalibracionNativa.ajustarAvance(cal, ms.toIntArray(), valores.toIntArray())
            LADO_ATRAS -> CalibracionNativa.ajustarRetroceso(cal, ms.toIntArray(), valores.toIntArray())
            else -> CalibracionNativa.ajustarGiro(cal, lado, ms.toIntArray(), valores.toIntArray())
        }
        val (codigo, nueva) = CalibracionNativa.desempaquetar(respuesta)
        if (codigo != CalibracionNativa.OK) {
            sb.append("$nombre: ${CalibracionNativa.textoError(codigo)}\n")
            return cal
        }
        sb.append("$nombre: bien (${ms.size} tiempos)\n")
        return nueva
    }

    // ── Botones del resumen ───────────────────────────────────────────────

    private fun usarValores() {
        val cal = resultado ?: return
        if (guardada.guardar(cal)) {
            if (cuantas(giroIzq) > 0 || cuantas(giroDer) > 0) {
                for (perfil in ControlLogica.Perfil.values()) {
                    guardada.restaurarFactoresGiro(perfil)   // los giros se acaban de medir: el factor de antes ya no aplica
                }
            }
            guardada.aplicarAlNavegador()
            Toast.makeText(this, "Calibración guardada. El navegador la usará en la próxima ruta.", Toast.LENGTH_LONG).show()
        } else {
            Toast.makeText(this, "Los valores no son válidos: repite alguna prueba", Toast.LENGTH_LONG).show()
        }
    }

    private fun copiarTexto() {
        val cal = resultado ?: return
        val texto = CalibracionNativa.aTexto(cal)
        if (texto.isEmpty()) {
            Toast.makeText(this, "Los valores no son válidos", Toast.LENGTH_SHORT).show()
            return
        }
        val portapapeles = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        portapapeles.setPrimaryClip(ClipData.newPlainText("calibracion", texto))
        Toast.makeText(this, "Texto copiado", Toast.LENGTH_SHORT).show()
    }

    private fun borrarCalibracion() {
        guardada.borrar()
        guardada.aplicarAlNavegador()
        Toast.makeText(this, "Calibración borrada: se usan los valores por defecto", Toast.LENGTH_LONG).show()
    }
}
