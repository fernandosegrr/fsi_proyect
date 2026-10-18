/*
 * CalibracionActivity.kt — Pantalla para MEDIR el carro y que el navegador convierta distancias y giros en tiempos reales (el carro no tiene encoders ni IMU).
 *
 * GUÍA DE LECTURA
 *  1. LA PANTALLA TIENE DOS FORMAS (docs/calibracion.md):
 *       - LA LISTA (Paso.LISTA): una tarjeta por PARTE de la calibración (enum Parte, en CalibracionGuardada.kt) con su valor actual, la fecha y hora de la última calibración y el botón
 *         «Calibrar solo esto». Cada parte es INDEPENDIENTE: no exige haber hecho otra antes, no borra los valores de las demás, y guarda al terminar (una prueba cancelada no cambia nada).
 *         Opcional: «Calibrar todo, en orden» las recorre una tras otra (cada una se guarda sola al terminar; «Omitir esta parte» salta una).
 *       - UNA PRUEBA, una por pantalla (los demás Paso): «Paso X de N», título, ilustración dibujada por código (IlustracionCalibracion), instrucción corta y, abajo y siempre visibles,
 *         la acción del paso, Atrás / Siguiente y el PARO. Solo UN botón se ve como primario a la vez (ver actualizarBotones).
 *  2. LAS PARTES Y SUS PASOS (función secuencia):
 *       - ARRANQUE: REACCION (toca cuando el botón se pone verde; 3 veces) → ARRANQUE (manda FORWARD y tocas «YA SE MOVIÓ»; 5 veces) → RESUMEN.
 *       - VELOCIDAD (de avance): AVANCE (FORWARD 1000, 2000 y 3000 ms, 3 veces cada uno; anotas los cm) → RESUMEN. Solo cambia los ms por cm; el arranque y el frenado guardados se conservan.
 *       - FRENADO (de avance): FRENADO (FORWARD 2000 ms, 3 veces; anotas los cm TOTALES) → RESUMEN. Con la velocidad y el arranque guardados, lo que sobra es el frenado.
 *       - GIRO_IZQ y GIRO_DER: un paso cada uno con el GIRO DE PRUEBA elegible: «Giro de 90° real» (por defecto: el comando que usa el navegador, con la calibración y el ajuste fino guardados),
 *         180°, duración libre de 500 a 1500 ms, o la prueba completa de tres tiempos (500, 1000 y 1500 ms, 3 veces cada uno; mide también cuánto sigue girando al frenar). Con un solo giro
 *         de prueba son 3 repeticiones y los ms por grado salen de los grados que TÚ dices que giró. → RESUMEN. Los ms por grado dependen de la duración de la prueba (el arranque pesa más
 *         en un giro corto): la app anota con qué giro de prueba se midió cada lado y AVISA si se van a mezclar dos distintos.
 *       - RETROCESO: igual que el avance pero con REVERSE (los ms por cm hacia atrás que usan los reintentos de esquive) → RESUMEN.
 *       - GIRO_VISUAL: gira 90° a la derecha con la calibración guardada mientras la cámara mide cuánto vio girar; tú escribes cuánto giró de verdad; ajusta el campo de visión (FOV) → RESUMEN.
 *       - DESPLAZ (desplazamiento del centro al girar): una pantalla: perfil, lado y giro de prueba (90° real, 180° o libre); «Girar»; mides con cinta cuánto se movió el centro (adelante o atrás,
 *         izquierda o derecha, de 0.5 cm en 0.5 cm) y el ángulo final (botones de 45°); 2 repeticiones por lado, se promedian y se rechazan si difieren más de 30 % (en C). Se guarda por lado y perfil.
 *       - GIRO_90 (ajuste fino del giro de 90°): el carro gira lo que la calibración dice que son 90°; dices cómo giró y se corrige un FACTOR por lado y perfil. Se repite hasta que quede justo.
 *       - RECTA (ajuste fino de la recta): «Avanzar 4 s» sobre una línea de cinta; dices hacia dónde se desvió y cuántos cm; el BALANCE nuevo se guarda y se manda al carro (BAL_AUTO / BAL_MANUAL).
 *     RESUMEN muestra lo que cambiaría (antes → después) y «Guardar esta calibración» lo guarda; antes de guardar el código en C revisa que los ms por grado de cada lado estén entre 3 y 20
 *     (CalibracionGuardada.guardar): si no, no guarda nada y dice por qué.
 *  3. Toda la cuenta (promedios, atípicos, rectas, frenado, geometría) está en C y probada en PC (nucleo_c/calibracion.c). Aquí solo hay pantalla y temporizadores.
 *  4. LA CONEXIÓN es la compartida de toda la app (SesionCarro): se entra en onResume y se sale en onPause. Sin conexión sale un aviso en banda con «Conectar».
 *  5. Seguridad: el botón PARO siempre está abajo; al salir de la pantalla se manda STOP (logica.parar) y se sueltan las pruebas. Hace falta tener ~2 m libres delante del carro.
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
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.core.content.ContextCompat
import com.google.android.material.button.MaterialButton
import com.google.android.material.snackbar.Snackbar
import com.google.android.material.progressindicator.LinearProgressIndicator
import com.google.android.material.textfield.TextInputLayout
import java.util.Locale
import kotlin.random.Random

class CalibracionActivity : androidx.appcompat.app.AppCompatActivity() {

    companion object {
        val TIEMPOS_AVANCE = intArrayOf(1000, 2000, 3000)   // ms de FORWARD en cada prueba de velocidad de avance (y de retroceso)
        val TIEMPOS_GIRO = intArrayOf(500, 1000, 1500)      // ms de LEFT / RIGHT en la prueba de giro de tres tiempos
        const val REPETICIONES = 3                          // por cada tiempo (y repeticiones de las pruebas de un solo tiempo)
        const val REPETICIONES_DESP = 2                     // repeticiones por lado del desplazamiento al girar
        const val TOQUES_REACCION = 3
        const val TOQUES_ARRANQUE = 5
        const val REACCION_POR_DEFECTO_MS = 180             // si no se midió la reacción
        const val ESPERA_MAXIMA_ARRANQUE_MS = 3000L         // si no tocas en este tiempo, se detiene el carro
        private const val LADO_ATRAS = 2                    // en ajustarGrupo: 0 = velocidad, -1 / 1 = giros, 2 = retroceso
        const val TIEMPO_RECTA_MS = 4000                    // la prueba de recta avanza 4 s (con 2 s el ruido de arranque de ±2 a 3 cm era mayor que la meta de 1 cm)
        const val TIEMPO_FRENADO_MS = 2000                  // la prueba de frenado avanza 2 s
        const val TIEMPO_MAXIMO_PRUEBA_MS = 4000            // ninguna prueba de giro manda un motor más tiempo que esto
        const val MS_POR_CM_MANUAL_X10 = 263                // 26.3 ms por cm en el perfil MANUAL (255), medido el 13 oct (docs/calibracion.md)
        const val ARRANQUE_CAMBIO_AVISO_MS = 40             // si el arranque cambia más que esto, se avisa que lo medido con el anterior puede estar desfasado
        private const val DESP_PASO_MM = 5                  // el desplazamiento se mide de 0.5 cm en 0.5 cm
        private const val DESP_MAX_MM = 400
        private const val ANGULO_PASO = 45
    }

    /** Las pantallas: la lista y cada paso de una prueba. */
    private enum class Paso { LISTA, REACCION, ARRANQUE, AVANCE, FRENADO, GIRO_IZQ, GIRO_DER, RETROCESO, GIRO_VISUAL, DESPLAZ, RESUMEN, GIRO_90, RECTA, VISION }

    /** Los pasos de cada parte, en orden. Las partes que se ajustan de a poco (desplazamiento, giro de 90°, recta) guardan en cada repetición y no tienen RESUMEN. */
    private fun secuencia(p: Parte): List<Paso> = when (p) {
        Parte.ARRANQUE -> listOf(Paso.REACCION, Paso.ARRANQUE, Paso.RESUMEN)
        Parte.VELOCIDAD -> listOf(Paso.AVANCE, Paso.RESUMEN)
        Parte.FRENADO -> listOf(Paso.FRENADO, Paso.RESUMEN)
        Parte.GIRO_IZQ -> listOf(Paso.GIRO_IZQ, Paso.RESUMEN)
        Parte.GIRO_DER -> listOf(Paso.GIRO_DER, Paso.RESUMEN)
        Parte.RETROCESO -> listOf(Paso.RETROCESO, Paso.RESUMEN)
        Parte.GIRO_VISUAL -> listOf(Paso.GIRO_VISUAL, Paso.RESUMEN)
        Parte.DESPLAZAMIENTO -> listOf(Paso.DESPLAZ)
        Parte.VISION -> listOf(Paso.VISION)
        Parte.GIRO_90 -> listOf(Paso.GIRO_90)
        Parte.RECTA -> listOf(Paso.RECTA)
    }

    /** «Calibrar todo, en orden»: el orden importa (el arranque entra en las cuentas de la velocidad y de los giros; el ajuste fino parte de los giros). El giro con la cámara no entra: pide la cámara abierta. */
    private val ORDEN_TODO = listOf(Parte.ARRANQUE, Parte.VELOCIDAD, Parte.FRENADO, Parte.GIRO_IZQ, Parte.GIRO_DER, Parte.RETROCESO, Parte.DESPLAZAMIENTO, Parte.GIRO_90, Parte.RECTA)

    /** Un contador con − y + (view_stepper.xml). */
    private class Contador(val raiz: View) {
        val etiqueta: TextView = raiz.findViewById(R.id.stepper_etiqueta)
        val valor: TextView = raiz.findViewById(R.id.stepper_valor)
        val menos: MaterialButton = raiz.findViewById(R.id.stepper_menos)
        val mas: MaterialButton = raiz.findViewById(R.id.stepper_mas)
    }

    /** Lo que se muestra de una parte en la lista. */
    private class EstadoParte(val medido: Boolean, val valor: String, val fecha: String, val pocaPrecision: Boolean = false)

    private val principal = Handler(Looper.getMainLooper())   // las pruebas con tiempo (se cancelan todas con pararTodo)
    private val reloj = Handler(Looper.getMainLooper())       // solo refresca el chip y el aviso de conexión (no se cancela con pararTodo)
    private lateinit var logica: ControlLogica               // la conexión COMPARTIDA (SesionCarro.lograr)
    private lateinit var guardada: CalibracionGuardada

    private lateinit var aviso: AvisoBanda
    private lateinit var avisoDesp: AvisoBanda   // «desplazamiento grande», dentro del bloque de contadores del desplazamiento
    private lateinit var chipConexion: ChipEstado
    private lateinit var textoPaso: TextView
    private lateinit var progreso: LinearProgressIndicator
    private lateinit var ilustracion: IlustracionCalibracion
    private lateinit var scroll: ScrollView
    private lateinit var listaContenedor: View
    private lateinit var listaPartes: LinearLayout
    private lateinit var textoListaAviso: TextView
    private lateinit var contenidoPaso: View
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
    private lateinit var botonOmitir: MaterialButton            // solo en «Calibrar todo, en orden»
    private lateinit var filaNav: View
    private lateinit var panelAjuste: View                      // perfil y lado (giro de 90°, recta y desplazamiento)
    private lateinit var botonPerfilAuto: MaterialButton
    private lateinit var botonPerfilManual: MaterialButton
    private lateinit var botonLadoIzq: MaterialButton
    private lateinit var botonLadoDer: MaterialButton
    private lateinit var bloqueGiro: View
    private lateinit var botonesOpcionGiro: List<MaterialButton>   // mucho, un poco, justo, corto (en el orden de CalibracionNativa.GIRO_*)
    private lateinit var botonRestaurar: MaterialButton
    private lateinit var etiquetaLado: View
    private lateinit var filaLado: View
    private lateinit var bloqueRecta: View
    private lateinit var botonDirIzq: MaterialButton
    private lateinit var botonDirRecto: MaterialButton
    private lateinit var botonDirDer: MaterialButton
    private lateinit var botonMenos: MaterialButton
    private lateinit var botonMas: MaterialButton
    private lateinit var textoCm: TextView
    private lateinit var campoEjes: EditText
    private lateinit var panelGiroPrueba: View                  // «Giro de prueba»
    private lateinit var botonGp90: MaterialButton
    private lateinit var botonGp180: MaterialButton
    private lateinit var botonGpLibre: MaterialButton
    private lateinit var botonGpTres: MaterialButton
    private lateinit var filaGpLibre: View
    private lateinit var botonGpMenos: MaterialButton
    private lateinit var botonGpMas: MaterialButton
    private lateinit var textoGpMs: TextView
    private lateinit var textoGpDetalle: TextView
    private lateinit var textoGpAviso: TextView
    private lateinit var bloqueDesp: View                       // la medida del desplazamiento
    private lateinit var contadorAdelante: Contador
    private lateinit var contadorDerecha: Contador
    private lateinit var contadorAngulo: Contador

    private var paso = Paso.LISTA
    private var parte: Parte? = null                            // la parte que se está calibrando (null en la lista)
    private var secuenciaActual: List<Paso> = emptyList()
    private var indicePaso = 0
    private var todo: List<Parte>? = null                       // «Calibrar todo, en orden»: las partes que faltan por recorrer
    private var todoIndice = 0
    private var perfilAjuste = ControlLogica.Perfil.AUTO       // el perfil con el que se prueba y al que se le guarda el factor, el balance y el desplazamiento
    private var ladoAjuste = CalibracionNativa.LADO_DERECHA    // el lado del giro de 90° o del desplazamiento que se prueba
    private var resumenAjuste = ""                              // lo que pasó con el último ajuste (se muestra en la tarjeta de datos)
    private var direccionDesvio = 0                             // recta: hacia dónde se desvió: -1 izquierda, 0 recto, +1 derecha
    private var desvioCm = 1                                    // recta: cuántos cm
    private var intentosRecta = 0                               // recta: cuántas correcciones se han aplicado (máximo CalibracionNativa.RECTA_MAX_ITERACIONES)
    private var rectaLista = false                              // recta: ya terminó con 1 cm o menos de desvío
    private var giroPrueba = GiroPrueba.DEFECTO                 // el giro de prueba elegido (giros y desplazamiento)
    private var libreMs = 1000                                  // la duración libre elegida (500 a 1500 ms)

    // Datos anotados (de la parte en curso; se borran al entrar o salir de una parte).
    private val reacciones = ArrayList<Int>()
    private val toquesArranque = ArrayList<Int>()
    private val avance = Array(TIEMPOS_AVANCE.size) { ArrayList<Int>() }      // mm por prueba
    private val frenadoMm = ArrayList<Int>()                                   // mm TOTALES que avanzó en la prueba de frenado
    private val giroTres = arrayOf(Array(TIEMPOS_GIRO.size) { ArrayList<Int>() }, Array(TIEMPOS_GIRO.size) { ArrayList<Int>() })   // grados x10 por tiempo; [0] izquierda, [1] derecha
    private val giroFijo = arrayOf(ArrayList<Int>(), ArrayList<Int>())         // grados x10 de cada repetición con UN solo giro de prueba; [0] izquierda, [1] derecha
    private val giroFijoMs = IntArray(2)                                       // los ms con que se hicieron esas repeticiones
    private val retroceso = Array(TIEMPOS_AVANCE.size) { ArrayList<Int>() }   // mm hacia atrás por prueba
    private val fovReal = ArrayList<Int>()      // grados x10 que el usuario midió en el piso en cada giro de la prueba visual
    private val fovVisual = ArrayList<Int>()    // grados x10 que la cámara vio girar en ese mismo giro
    private var visualActualX10 = 0             // lo que la cámara va viendo en el giro de esta prueba
    private val despAdelanteUm = arrayOf(ArrayList<Int>(), ArrayList<Int>())  // µm por grado de cada repetición del desplazamiento; [0] izquierda, [1] derecha
    private val despDerechaUm = arrayOf(ArrayList<Int>(), ArrayList<Int>())
    private val despGrados = arrayOf(ArrayList<Int>(), ArrayList<Int>())      // los grados que se anotaron en cada repetición (para comparar en cm)
    private var despAdelanteMm = 0              // lo que el usuario mide en la repetición en curso
    private var despDerechaMm = 0
    private var despAngulo = 90
    private var despMsUltimo = 0                // los ms del último giro del desplazamiento (para el log)
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

    private var resultado: IntArray? = null  // calibración calculada en el RESUMEN (la guardada con el cambio de esta parte)
    private var resultadoOk = false          // el cálculo salió bien (si no, no se puede guardar)
    private var textoResumen = ""
    private var avisoGuardar = ""            // por qué no se guardó (por ejemplo, ms por grado fuera de 3 a 20)

    /** La sesión compartida avisa (conexión, perfil, latencia): se redibuja el chip y se habilitan o no los botones. */
    private val alCambiar: () -> Unit = { mostrarConexion() }

    private var camaraAbiertaMostrada = false   // lo último que se dibujó del estado de la cámara en el giro visual
    private var entradaPasoMs = 0L              // cuándo se mostró el paso actual (para ignorar el doble toque de «Listo»)

    /** Cada medio segundo: el aviso de «sin conexión» depende del tiempo que lleva sin conectar. También redibuja el giro visual cuando la cámara termina de abrir (abre sola, en otro hilo). */
    private val tareaReloj = object : Runnable {
        override fun run() {
            actualizarChipYAviso()
            if (usaCamara() && camara.abierta != camaraAbiertaMostrada && !ocupado()) {
                mostrarPaso()
            }
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
        avisoDesp = findViewById(R.id.dp_aviso)
        chipConexion = findViewById(R.id.calib_chip_conexion)
        textoPaso = findViewById(R.id.texto_paso)
        progreso = findViewById(R.id.calib_progreso)
        ilustracion = findViewById(R.id.calib_ilustracion)
        scroll = findViewById(R.id.calib_scroll)
        listaContenedor = findViewById(R.id.calib_lista)
        listaPartes = findViewById(R.id.calib_lista_partes)
        textoListaAviso = findViewById(R.id.texto_lista_aviso)
        contenidoPaso = findViewById(R.id.calib_contenido_paso)
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
        botonOmitir = findViewById(R.id.boton_omitir)
        filaNav = findViewById(R.id.calib_fila_nav)
        panelAjuste = findViewById(R.id.aj_panel)
        botonPerfilAuto = findViewById(R.id.aj_perfil_auto)
        botonPerfilManual = findViewById(R.id.aj_perfil_manual)
        botonLadoIzq = findViewById(R.id.aj_lado_izq)
        botonLadoDer = findViewById(R.id.aj_lado_der)
        bloqueGiro = findViewById(R.id.aj_bloque_giro)
        botonesOpcionGiro = listOf(R.id.aj_op_mucho, R.id.aj_op_poco, R.id.aj_op_justo, R.id.aj_op_corto).map { findViewById<MaterialButton>(it) }
        botonRestaurar = findViewById(R.id.aj_restaurar)
        etiquetaLado = findViewById(R.id.aj_etiqueta_lado)
        filaLado = findViewById(R.id.aj_fila_lado)
        bloqueRecta = findViewById(R.id.aj_bloque_recta)
        botonDirIzq = findViewById(R.id.aj_dir_izq)
        botonDirRecto = findViewById(R.id.aj_dir_recto)
        botonDirDer = findViewById(R.id.aj_dir_der)
        botonMenos = findViewById(R.id.aj_menos)
        botonMas = findViewById(R.id.aj_mas)
        textoCm = findViewById(R.id.aj_cm)
        campoEjes = findViewById(R.id.aj_ejes)
        panelGiroPrueba = findViewById(R.id.gp_panel)
        botonGp90 = findViewById(R.id.gp_90)
        botonGp180 = findViewById(R.id.gp_180)
        botonGpLibre = findViewById(R.id.gp_libre)
        botonGpTres = findViewById(R.id.gp_tres)
        filaGpLibre = findViewById(R.id.gp_fila_libre)
        botonGpMenos = findViewById(R.id.gp_menos)
        botonGpMas = findViewById(R.id.gp_mas)
        textoGpMs = findViewById(R.id.gp_ms)
        textoGpDetalle = findViewById(R.id.gp_detalle)
        textoGpAviso = findViewById(R.id.gp_aviso)
        bloqueDesp = findViewById(R.id.dp_bloque)
        contadorAdelante = Contador(findViewById(R.id.dp_adelante))
        contadorDerecha = Contador(findViewById(R.id.dp_derecha))
        contadorAngulo = Contador(findViewById(R.id.dp_angulo))
        contadorAdelante.etiqueta.setText(R.string.cal_dp_adelante)
        contadorDerecha.etiqueta.setText(R.string.cal_dp_derecha)
        contadorAngulo.etiqueta.setText(R.string.cal_dp_angulo)
        for (c in listOf(contadorAdelante, contadorDerecha, contadorAngulo)) {
            c.menos.contentDescription = getString(R.string.cal_dp_menos)
            c.mas.contentDescription = getString(R.string.cal_dp_mas)
        }
        botonGpMenos.contentDescription = getString(R.string.cal_gp_menos)
        botonGpMas.contentDescription = getString(R.string.cal_gp_mas)
        findViewById<TextView>(R.id.barra_titulo).setText(R.string.cal_titulo)
        findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { alVolver() }
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                alVolver()
            }
        })
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
        botonAtras.setOnClickListener { atras() }
        botonSiguiente.setOnClickListener { siguiente() }
        botonUsar.setOnClickListener { usarValores() }
        botonOmitir.setOnClickListener { omitirParte() }
        botonCopiar.setOnClickListener { copiarTexto() }
        botonBorrarCal.setOnClickListener { confirmarBorrarCalibracion() }
        botonPerfilAuto.setOnClickListener { elegirPerfilAjuste(ControlLogica.Perfil.AUTO) }
        botonPerfilManual.setOnClickListener { elegirPerfilAjuste(ControlLogica.Perfil.MANUAL) }
        botonLadoIzq.setOnClickListener { elegirLadoAjuste(CalibracionNativa.LADO_IZQUIERDA) }
        botonLadoDer.setOnClickListener { elegirLadoAjuste(CalibracionNativa.LADO_DERECHA) }
        for (opcion in botonesOpcionGiro.indices) {
            botonesOpcionGiro[opcion].setOnClickListener { aplicarGradosGiro(CalibracionNativa.gradosDeOpcionGiro(opcion)) }
        }
        botonRestaurar.setOnClickListener { restaurarAjuste() }
        botonDirIzq.setOnClickListener { elegirDireccionDesvio(CalibracionNativa.LADO_IZQUIERDA) }
        botonDirRecto.setOnClickListener { elegirDireccionDesvio(0) }
        botonDirDer.setOnClickListener { elegirDireccionDesvio(CalibracionNativa.LADO_DERECHA) }
        botonMenos.setOnClickListener { cambiarDesvioCm(-1) }
        botonMas.setOnClickListener { cambiarDesvioCm(1) }
        botonGp90.setOnClickListener { elegirGiroPrueba(GiroPrueba(GiroPrueba.Tipo.NOVENTA)) }
        botonGp180.setOnClickListener { elegirGiroPrueba(GiroPrueba(GiroPrueba.Tipo.CIENTO_OCHENTA)) }
        botonGpLibre.setOnClickListener { elegirGiroPrueba(GiroPrueba(GiroPrueba.Tipo.LIBRE, libreMs)) }
        botonGpTres.setOnClickListener { elegirGiroPrueba(GiroPrueba(GiroPrueba.Tipo.TRES_TIEMPOS)) }
        botonGpMenos.setOnClickListener { cambiarLibreMs(-GiroPrueba.LIBRE_PASO_MS) }
        botonGpMas.setOnClickListener { cambiarLibreMs(GiroPrueba.LIBRE_PASO_MS) }
        contadorAdelante.menos.setOnClickListener { cambiarMedidaDesp(-DESP_PASO_MM, 0, 0) }
        contadorAdelante.mas.setOnClickListener { cambiarMedidaDesp(DESP_PASO_MM, 0, 0) }
        contadorDerecha.menos.setOnClickListener { cambiarMedidaDesp(0, -DESP_PASO_MM, 0) }
        contadorDerecha.mas.setOnClickListener { cambiarMedidaDesp(0, DESP_PASO_MM, 0) }
        contadorAngulo.menos.setOnClickListener { cambiarMedidaDesp(0, 0, -ANGULO_PASO) }
        contadorAngulo.mas.setOnClickListener { cambiarMedidaDesp(0, 0, ANGULO_PASO) }
        campoEjes.setText(String.format(Locale.US, "%.1f", guardada.anchoEjesMm / 10.0).removeSuffix(".0"))
        campoEjes.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(texto: CharSequence?, inicio: Int, cuenta: Int, despues: Int) {}
            override fun onTextChanged(texto: CharSequence?, inicio: Int, antes: Int, cuenta: Int) {}
            override fun afterTextChanged(texto: Editable?) {
                val cm = texto.toString().trim().replace(',', '.').toDoubleOrNull()
                if (cm != null && cm >= 5.0 && cm <= 40.0) {
                    guardada.anchoEjesMm = Math.round(cm * 10).toInt()   // se guarda al escribir un valor razonable (5 a 40 cm)
                }
            }
        })
        campoResultado.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(texto: CharSequence?, inicio: Int, cuenta: Int, despues: Int) {}
            override fun onTextChanged(texto: CharSequence?, inicio: Int, antes: Int, cuenta: Int) {
                campoLayout.error = null   // al volver a escribir se quita el aviso de «número no válido»
            }
            override fun afterTextChanged(texto: Editable?) {}
        })
        // Solo para revisar las pantallas con adb (scripts/qa_captura.py): `--es paso GIRO_90` o `--es parte DESPLAZAMIENTO` abre la pantalla directamente en esa parte.
        val pedida = intent.getStringExtra("parte") ?: intent.getStringExtra("paso")
        if (pedida != null) {
            parteDeNombre(pedida)?.let { iniciarParte(it, false) }
        }
        if (paso == Paso.LISTA) {
            armarLista()
        }
        mostrarPaso()
        PantallasLog.abierta("CALIBRACION")
    }

    /** Los nombres de pasos de la versión anterior (y los de las partes) abren la parte que les corresponde. */
    private fun parteDeNombre(nombre: String): Parte? = when (nombre.uppercase()) {
        "REACCION", "ARRANQUE" -> Parte.ARRANQUE
        "AVANCE", "VELOCIDAD" -> Parte.VELOCIDAD
        "FRENADO" -> Parte.FRENADO
        "GIRO_IZQ" -> Parte.GIRO_IZQ
        "GIRO_DER" -> Parte.GIRO_DER
        "RETROCESO" -> Parte.RETROCESO
        "GIRO_VISUAL" -> Parte.GIRO_VISUAL
        "DESPLAZ", "DESPLAZAMIENTO" -> Parte.DESPLAZAMIENTO
        "GIRO_90" -> Parte.GIRO_90
        "RECTA" -> Parte.RECTA
        "VISION" -> Parte.VISION
        else -> null
    }

    override fun onResume() {
        super.onResume()
        SesionCarro.escuchar(alCambiar)
        SesionCarro.entrar(this, SesionNativa.USUARIO_CALIBRACION)
        logica.pedirPerfil(perfilEsperado())   // la calibración en el perfil AUTO (210), que es la velocidad a la que andan las rutas; el ajuste fino puede usar también MANUAL
        reloj.removeCallbacks(tareaReloj)
        reloj.post(tareaReloj)
        if (usaCamara()) {
            abrirCamaraSiHayPermiso()
        }
        mostrarConexion()
        if (paso == Paso.LISTA) {
            armarLista()   // por si algo cambió mientras la pantalla no estaba a la vista
        }
    }

    /** Los pasos que usan la cámara del teléfono: la prueba del giro visual y «Calibrar visión». */
    private fun usaCamara() = paso == Paso.GIRO_VISUAL || paso == Paso.VISION

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
        if (codigo == VisionActivity.PEDIDO_PERMISO_CAMARA && resultados.isNotEmpty() && resultados[0] == PackageManager.PERMISSION_GRANTED && usaCamara()) {
            abrirCamaraSiHayPermiso()
            mostrarPaso()
        }
    }

    override fun onPause() {
        guardada.aplicarMascaraAlDetector()   // por si se salió de «Calibrar visión» sin terminar: la máscara guardada vuelve a valer
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

    /** El perfil que debe tener el carro en el paso actual: AUTO siempre, salvo en el desplazamiento y el ajuste fino, donde es el que se eligió. */
    private fun perfilEsperado() = if (paso == Paso.GIRO_90 || paso == Paso.RECTA || paso == Paso.DESPLAZ) perfilAjuste else ControlLogica.Perfil.AUTO

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

    /** El aviso de «desplazamiento grande» (de 15 cm de desplazamiento total en el giro para arriba): el carro gira de pivote y el centro describe un arco; el radio sale de la cuerda
     *  (C: cal_desplazamiento_arco). null si el desplazamiento no es tan grande. No bloquea nada: la medida se acepta hasta ~45 cm en un giro de 90°. */
    private fun avisoDesplazamientoGrande(grados: Int, adelanteMm: Int, derechaMm: Int): Mensaje? {
        val arco = CalibracionNativa.desplazamientoArco(grados, adelanteMm, derechaMm)
        if (arco[0] != CalibracionNativa.OK || arco[1] < CalibracionNativa.DESP_AVISO_MM) {
            return null
        }
        return Mensaje("Desplazamiento grande: confirma que mediste del centro inicial al centro final",
            fmt("El carro avanza %.1f cm mientras gira (arco de unos %.1f cm de radio), no pivota sobre sí mismo.", arco[1] / 10.0, arco[2] / 10.0))
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

    private fun ocupado() = corriendo || arrancando || estadoReaccion != 0

    // ── La lista y la navegación entre partes y pasos ─────────────────────

    /** Borra todo lo anotado de la parte en curso (salir de una parte sin guardar no cambia nada). */
    private fun limpiarDatos() {
        reacciones.clear()
        toquesArranque.clear()
        for (l in avance) l.clear()
        for (l in retroceso) l.clear()
        frenadoMm.clear()
        for (lado in giroTres) for (l in lado) l.clear()
        for (l in giroFijo) l.clear()
        fovReal.clear()
        fovVisual.clear()
        for (l in despAdelanteUm) l.clear()
        for (l in despDerechaUm) l.clear()
        for (l in despGrados) l.clear()
        esperandoDato = false
        resultado = null
        resultadoOk = false
        textoResumen = ""
        avisoGuardar = ""
        resumenAjuste = ""
        intentosRecta = 0
        rectaLista = false
        campoResultado.setText("")
    }

    /** Empieza a calibrar UNA parte. `conTodo` = true cuando viene de «Calibrar todo, en orden» (entonces se conserva la lista de partes). */
    private fun iniciarParte(p: Parte, conTodo: Boolean) {
        if (ocupado()) {
            return
        }
        if (!conTodo) {
            todo = null
        }
        limpiarDatos()
        parte = p
        secuenciaActual = secuencia(p)
        indicePaso = 0
        giroPrueba = GiroPrueba.DEFECTO
        ladoAjuste = when (p) {
            Parte.GIRO_IZQ -> CalibracionNativa.LADO_IZQUIERDA
            else -> CalibracionNativa.LADO_DERECHA
        }
        despAdelanteMm = 0
        despDerechaMm = 0
        Log.i(CalibracionGuardada.ETIQUETA, "calibrar solo esto: ${p.name}" + (if (conTodo) " (calibrar todo, parte ${todoIndice + 1} de ${todo?.size})" else ""))
        entrarPaso()
    }

    /** «Calibrar todo, en orden»: recorre las partes una tras otra; cada una se guarda sola al terminar. */
    private fun iniciarTodo() {
        if (ocupado()) {
            return
        }
        todo = ORDEN_TODO
        todoIndice = 0
        iniciarParte(ORDEN_TODO[0], true)
    }

    /** Lo que hay que hacer cada vez que se muestra un paso nuevo (la cámara, el perfil, el cálculo del resultado). */
    private fun entrarPaso() {
        paso = secuenciaActual[indicePaso]
        entradaPasoMs = SystemClock.elapsedRealtime()
        esperandoDato = false
        resumenAjuste = ""
        campoResultado.setText("")
        if (paso == Paso.RESUMEN) {
            calcularResultado()
        }
        if (usaCamara()) {
            abrirCamaraSiHayPermiso()
        } else {
            camara.cerrar()
        }
        if (paso == Paso.VISION) {
            iniciarCalibracionVision()   // la máscara se apaga mientras se captura: el detector tiene que verlo todo
        }
        if (paso == Paso.RECTA) {
            intentosRecta = 0
            rectaLista = false
        }
        logica.pedirPerfil(perfilEsperado())   // al entrar o salir del desplazamiento y del ajuste fino el carro cambia de perfil si hace falta
        mostrarPaso()
        scroll.scrollTo(0, 0)   // cada paso empieza arriba
    }

    private fun volverALista() {
        guardada.aplicarMascaraAlDetector()   // si se estaba calibrando la visión, vuelve a valer la máscara guardada
        visionCapturando = false
        parte = null
        todo = null
        secuenciaActual = emptyList()
        indicePaso = 0
        paso = Paso.LISTA
        limpiarDatos()
        camara.cerrar()
        logica.pedirPerfil(ControlLogica.Perfil.AUTO)
        armarLista()
        mostrarPaso()
        scroll.scrollTo(0, 0)
    }

    /** Terminó una parte (se guardó, o era de las que se ajustan de a poco): sigue la siguiente de «Calibrar todo» o vuelve a la lista. */
    private fun terminarParte() {
        val lista = todo
        if (lista != null && todoIndice + 1 < lista.size) {
            todoIndice++
            iniciarParte(lista[todoIndice], true)
        } else {
            if (lista != null) {
                Toast.makeText(this, "Se acabaron las partes. Las que terminaste ya se guardaron.", Toast.LENGTH_LONG).show()
            }
            volverALista()
        }
    }

    /** «Omitir esta parte» (solo en «Calibrar todo»): no se guarda nada de ella y sigue la siguiente. */
    private fun omitirParte() {
        if (ocupado() || todo == null) {
            return
        }
        Log.i(CalibracionGuardada.ETIQUETA, "parte omitida: ${parte?.name}")
        terminarParte()
    }

    /** ¿Hay algo medido en esta parte que se perdería al salir? */
    private fun hayDatosSinGuardar(): Boolean =
        reacciones.isNotEmpty() || toquesArranque.isNotEmpty() || cuantas(avance) > 0 || cuantas(retroceso) > 0 || frenadoMm.isNotEmpty() ||
            giroTres.any { cuantas(it) > 0 } || giroFijo.any { it.isNotEmpty() } || fovReal.isNotEmpty() || despAdelanteUm.any { it.isNotEmpty() }

    /** La flecha de la barra de título o el botón «atrás» del teléfono: en una parte, cancela (no se guarda nada); en la lista, sale de la pantalla. */
    private fun alVolver() {
        if (parte == null) {
            finish()
        } else if (!ocupado()) {
            cancelarParte()
        }
    }

    private fun cancelarParte() {
        if (hayDatosSinGuardar()) {
            Dialogos.confirmar(this, "¿Cancelar esta calibración?", "Lo que ya mediste en esta parte no se guarda y los valores guardados no cambian.", "Cancelar la calibración") { volverALista() }
        } else {
            volverALista()
        }
    }

    private fun atras() {
        if (ocupado()) {
            return
        }
        if (indicePaso == 0) {
            cancelarParte()
            return
        }
        indicePaso--
        entrarPaso()
    }

    private fun siguiente() {
        if (ocupado() || esperandoDato) {
            return
        }
        if (SystemClock.elapsedRealtime() - entradaPasoMs < 700L) {
            return   // un doble toque en «Listo» no debe saltarse también la parte que viene (en «Calibrar todo»)
        }
        if (indicePaso >= secuenciaActual.size - 1) {
            // «Listo» (las partes que se ajustan de a poco ya guardaron). Una repetición del desplazamiento a medias se perdería: se pregunta.
            if (paso == Paso.DESPLAZ && hayDatosSinGuardar()) {
                Dialogos.confirmar(this, "¿Terminar sin completar?", "Falta la segunda repetición de ese lado: lo que ya mediste de él no se guarda. Los lados ya guardados no cambian.", "Terminar") { terminarParte() }
            } else {
                terminarParte()
            }
            return
        }
        indicePaso++
        entrarPaso()
    }

    /** Cuántas pruebas se han anotado en la lista de listas. */
    private fun cuantas(listas: Array<ArrayList<Int>>) = listas.sumOf { it.size }

    private fun totalPruebas() = TIEMPOS_AVANCE.size * REPETICIONES

    private fun indiceLado(lado: Int) = if (lado < 0) 0 else 1

    /** El lado del paso de giro actual. */
    private fun ladoDelPaso() = if (paso == Paso.GIRO_IZQ) CalibracionNativa.LADO_IZQUIERDA else CalibracionNativa.LADO_DERECHA

    /** Cuántas repeticiones de giro lleva el lado del paso (con tres tiempos o con un solo giro de prueba) y cuántas hacen falta. */
    private fun cuantasGiro(): Int = if (giroPrueba.tipo == GiroPrueba.Tipo.TRES_TIEMPOS) cuantas(giroTres[indiceLado(ladoDelPaso())]) else giroFijo[indiceLado(ladoDelPaso())].size

    private fun totalGiro(): Int = if (giroPrueba.tipo == GiroPrueba.Tipo.TRES_TIEMPOS) totalPruebas() else REPETICIONES

    private fun pasoCompleto(): Boolean = when (paso) {
        Paso.LISTA, Paso.RESUMEN -> true
        Paso.REACCION -> reacciones.size >= TOQUES_REACCION
        Paso.ARRANQUE -> toquesArranque.size >= TOQUES_ARRANQUE
        Paso.AVANCE -> cuantas(avance) >= totalPruebas()
        Paso.FRENADO -> frenadoMm.size >= REPETICIONES
        Paso.GIRO_IZQ, Paso.GIRO_DER -> cuantasGiro() >= totalGiro()
        Paso.RETROCESO -> cuantas(retroceso) >= totalPruebas()
        Paso.GIRO_VISUAL -> fovReal.isNotEmpty()
        Paso.VISION -> visionHechos >= 3
        Paso.DESPLAZ, Paso.GIRO_90, Paso.RECTA -> true
    }

    /** La lista de listas del paso actual (velocidad, retroceso o giro de tres tiempos) o null. */
    private fun datosDelPaso(): Array<ArrayList<Int>>? = when (paso) {
        Paso.AVANCE -> avance
        Paso.RETROCESO -> retroceso
        Paso.GIRO_IZQ, Paso.GIRO_DER -> if (giroPrueba.tipo == GiroPrueba.Tipo.TRES_TIEMPOS) giroTres[indiceLado(ladoDelPaso())] else null
        else -> null
    }

    /** true en los pasos donde se mide una distancia en cm (velocidad, retroceso y frenado); false en los de giro. */
    private fun esPasoLineal() = paso == Paso.AVANCE || paso == Paso.RETROCESO || paso == Paso.FRENADO

    /** Tiempo (ms) de la prueba que toca ahora en un paso de velocidad, retroceso, frenado o giro. */
    private fun tiempoActual(): Int {
        val datos = datosDelPaso()
        val indice = if (datos == null) 0 else (cuantas(datos) / REPETICIONES).coerceAtMost(TIEMPOS_AVANCE.size - 1)
        return when (paso) {
            Paso.AVANCE, Paso.RETROCESO -> TIEMPOS_AVANCE[indice]
            Paso.FRENADO -> TIEMPO_FRENADO_MS
            Paso.GIRO_IZQ, Paso.GIRO_DER ->
                if (giroPrueba.tipo == GiroPrueba.Tipo.TRES_TIEMPOS) TIEMPOS_GIRO[indice] else msDeGiroPrueba(ladoDelPaso(), giroPrueba, ControlLogica.Perfil.AUTO)
            else -> 0
        }
    }

    /** Cuántos ms de motor manda el giro de prueba hacia ese lado: el comando que usa el navegador para 90° o 180° (con la calibración y el ajuste fino guardados del perfil) o la duración libre. */
    private fun msDeGiroPrueba(lado: Int, prueba: GiroPrueba, perfil: ControlLogica.Perfil): Int {
        val ms = when (prueba.tipo) {
            GiroPrueba.Tipo.NOVENTA, GiroPrueba.Tipo.CIENTO_OCHENTA -> CalibracionNativa.gradosAMs(guardada.leerAjustada(perfil), lado * prueba.grados())
            GiroPrueba.Tipo.LIBRE -> prueba.ms
            GiroPrueba.Tipo.TRES_TIEMPOS -> 0
        }
        return ms.coerceAtMost(TIEMPO_MAXIMO_PRUEBA_MS)
    }

    private fun comandoDelPaso() = when (paso) {
        Paso.AVANCE, Paso.FRENADO -> "FORWARD"
        Paso.RETROCESO -> "REVERSE"
        Paso.GIRO_IZQ -> "LEFT"
        else -> "RIGHT"
    }

    private fun nombreParte(p: Parte) = getString(when (p) {
        Parte.ARRANQUE -> R.string.cal_parte_arranque
        Parte.VELOCIDAD -> R.string.cal_parte_velocidad
        Parte.FRENADO -> R.string.cal_parte_frenado
        Parte.GIRO_IZQ -> R.string.cal_parte_giro_izq
        Parte.GIRO_DER -> R.string.cal_parte_giro_der
        Parte.RETROCESO -> R.string.cal_parte_retroceso
        Parte.GIRO_VISUAL -> R.string.cal_parte_giro_visual
        Parte.DESPLAZAMIENTO -> R.string.cal_parte_desplazamiento
        Parte.GIRO_90 -> R.string.cal_parte_giro_90
        Parte.RECTA -> R.string.cal_parte_recta
        Parte.VISION -> R.string.cal_parte_vision
    })

    /** La escena de la ilustración que corresponde al paso actual. */
    private fun escenaDelPaso(): IlustracionCalibracion.Escena = when (paso) {
        Paso.LISTA -> IlustracionCalibracion.Escena.INTRO
        Paso.REACCION -> IlustracionCalibracion.Escena.REACCION
        Paso.ARRANQUE -> IlustracionCalibracion.Escena.ARRANQUE
        Paso.AVANCE -> IlustracionCalibracion.Escena.AVANCE
        Paso.FRENADO -> IlustracionCalibracion.Escena.FRENADO
        Paso.GIRO_IZQ -> IlustracionCalibracion.Escena.GIRO_IZQ
        Paso.GIRO_DER -> IlustracionCalibracion.Escena.GIRO_DER
        Paso.RETROCESO -> IlustracionCalibracion.Escena.RETROCESO
        Paso.GIRO_VISUAL, Paso.VISION -> IlustracionCalibracion.Escena.GIRO_VISUAL
        Paso.DESPLAZ -> IlustracionCalibracion.Escena.DESPLAZAMIENTO
        Paso.RESUMEN -> IlustracionCalibracion.Escena.RESUMEN
        Paso.GIRO_90 -> IlustracionCalibracion.Escena.GIRO_90
        Paso.RECTA -> IlustracionCalibracion.Escena.RECTA
    }

    /** Pone la pista del campo de resultado solo si cambió (cambiarla en cada cuadro de la prueba visual haría parpadear el campo). */
    private fun ponerPista(texto: String) {
        if (campoLayout.hint?.toString() != texto) {
            campoLayout.hint = texto
        }
    }

    // ── La lista: qué se ve de cada parte ─────────────────────────────────

    private fun fmt(formato: String, vararg valores: Any) = String.format(Locale.US, formato, *valores)

    private fun lineaFecha(parte: Parte, medido: Boolean, perfil: ControlLogica.Perfil? = null): String {
        val ms = guardada.fecha(parte, perfil)
        return if (ms > 0L) guardada.textoFecha(ms) else if (medido) "medido, sin fecha" else "nunca"
    }

    /** El valor actual, si está medido y la fecha de la última calibración de una parte. */
    private fun estadoDeParte(p: Parte): EstadoParte {
        val cal = guardada.leer()
        val medidos = cal[CalibracionNativa.CAL_MEDIDOS]
        val ajustada = guardada.leerAjustada(ControlLogica.Perfil.AUTO)
        return when (p) {
            Parte.ARRANQUE -> {
                val medido = medidos and CalibracionNativa.MEDIDO_ARRANQUE != 0
                EstadoParte(medido, if (medido) "${cal[CalibracionNativa.CAL_ARRANQUE_MS]} ms" else "Sin medir (se usa 0 ms)", lineaFecha(p, medido))
            }
            Parte.VELOCIDAD -> {
                val medido = medidos and CalibracionNativa.MEDIDO_AVANCE != 0
                val cmPorSegundo = 10000.0 / cal[CalibracionNativa.CAL_MS_POR_CM_X10]
                EstadoParte(medido, if (medido) fmt("%.1f ms por cm (%.0f cm/s)", cal[CalibracionNativa.CAL_MS_POR_CM_X10] / 10.0, cmPorSegundo) else "Sin medir (se usa el valor de ejemplo)", lineaFecha(p, medido))
            }
            Parte.FRENADO -> {
                val medido = guardada.fecha(p) > 0L || medidos and CalibracionNativa.MEDIDO_AVANCE != 0
                EstadoParte(medido, if (medido) fmt("%.1f cm después del STOP", cal[CalibracionNativa.CAL_FRENADO_MM] / 10.0) else "Sin medir (se usa 0)", lineaFecha(p, medido))
            }
            Parte.GIRO_IZQ, Parte.GIRO_DER -> {
                val izq = p == Parte.GIRO_IZQ
                val lado = if (izq) CalibracionNativa.LADO_IZQUIERDA else CalibracionNativa.LADO_DERECHA
                val medido = medidos and (if (izq) CalibracionNativa.MEDIDO_GIRO_IZQ else CalibracionNativa.MEDIDO_GIRO_DER) != 0
                val msGrado = cal[if (izq) CalibracionNativa.CAL_MS_GRADO_IZQ_X10 else CalibracionNativa.CAL_MS_GRADO_DER_X10] / 10.0
                val prueba = guardada.giroPruebaDeGiro(lado)
                val valor = if (medido) fmt("%.1f ms por grado · un giro de 90° dura %d ms", msGrado, CalibracionNativa.gradosAMs(ajustada, lado * 90)) +
                    (if (prueba != null) "\nMedido con: ${prueba.texto()}" else "") else "Sin medir (se usa el valor de ejemplo)"
                EstadoParte(medido, valor, lineaFecha(p, medido))
            }
            Parte.RETROCESO -> {
                val medido = medidos and CalibracionNativa.MEDIDO_ATRAS != 0
                EstadoParte(medido, if (medido) fmt("%.1f ms por cm hacia atrás", cal[CalibracionNativa.CAL_MS_POR_CM_ATRAS_X10] / 10.0) else "Sin medir (usa la velocidad de avance)", lineaFecha(p, medido))
            }
            Parte.VISION -> {
                val medido = guardada.hayVision()
                val celdas = guardada.celdasEnmascaradas()
                EstadoParte(medido, if (!medido) "Sin calibrar (no se ignora ninguna celda de la imagen)"
                    else (if (celdas == 0) "Sin artefactos fijos: no se ignora ninguna celda" else "$celdas de 100 celdas de la imagen se ignoran (artefactos fijos)") +
                        (if (guardada.visionSospechosa()) ". VISIÓN SOSPECHOSA: revisa el informe y repite la calibración" else ""), lineaFecha(p, medido), medido && guardada.visionSospechosa())
            }
            Parte.GIRO_VISUAL -> {
                val medido = medidos and CalibracionNativa.MEDIDO_FOV != 0
                EstadoParte(medido, if (medido) fmt("Campo de visión de la cámara: %.1f°", cal[CalibracionNativa.CAL_FOV_GRADOS_X10] / 10.0) else "Sin medir (se usa el que calcula la cámara)", lineaFecha(p, medido))
            }
            Parte.DESPLAZAMIENTO -> {
                val lineas = ArrayList<String>()
                val fechas = ArrayList<String>()
                for (perfil in ControlLogica.Perfil.values()) {
                    if (guardada.hayDesplazamiento(perfil)) {
                        // Lo que el modelo de pivote da para un giro de 90° (marco del carro al empezar el giro), con la calibración de ese perfil.
                        val cal = guardada.leerAjustada(perfil)
                        val izq = CalibracionNativa.desplazamientoDelGiro(cal, CalibracionNativa.LADO_IZQUIERDA, 90)
                        val der = CalibracionNativa.desplazamientoDelGiro(cal, CalibracionNativa.LADO_DERECHA, 90)
                        lineas.add("${perfil.name}: en un giro de 90° el centro se mueve " + fmt("izq %.1f cm adelante y %.1f de lado, der %.1f adelante y %.1f de lado", izq[1] / 10.0, izq[2] / 10.0, der[1] / 10.0, der[2] / 10.0) +
                            (if (der[4] == 1 && der[3] == 0) fmt(". Punto fijo del giro a la derecha: %.1f cm del centro a la derecha", der[6] / 10.0) else "") +
                            (if (izq[3] == 1) ".\n" + getString(R.string.cal_izq_espejo) else if (der[3] == 1) ".\n" + getString(R.string.cal_der_espejo) else ""))
                        fechas.add("${perfil.name} ${lineaFecha(p, true, perfil)}")
                    }
                }
                val poca = ArrayList<String>()
                for (perfil in ControlLogica.Perfil.values()) {
                    for (lado in intArrayOf(CalibracionNativa.LADO_IZQUIERDA, CalibracionNativa.LADO_DERECHA)) {
                        if (guardada.desplazamientoPocaPrecision(perfil, lado)) poca.add("${perfil.name} ${nombreLado(lado)}")
                    }
                }
                if (lineas.isEmpty()) EstadoParte(false, "Sin medir (el carro gira en su sitio)", "nunca")
                else EstadoParte(true, lineas.joinToString("\n") + (if (poca.isEmpty()) "" else "\nCalibrado con poca precisión (${poca.joinToString(", ")}): las repeticiones no coincidían. Repite la medición cuando puedas."),
                    fechas.joinToString(" · "), poca.isNotEmpty())
            }
            Parte.GIRO_90 -> {
                val lineas = ArrayList<String>()
                val fechas = ArrayList<String>()
                for (perfil in ControlLogica.Perfil.values()) {
                    val fi = guardada.factorGiro(perfil, CalibracionNativa.LADO_IZQUIERDA)
                    val fd = guardada.factorGiro(perfil, CalibracionNativa.LADO_DERECHA)
                    if (fi != CalibracionNativa.FACTOR_GIRO_DEFECTO || fd != CalibracionNativa.FACTOR_GIRO_DEFECTO || guardada.fecha(p, perfil) > 0L) {
                        lineas.add("${perfil.name}: factor izquierda $fi, derecha $fd (1000 = sin ajuste)")
                        fechas.add("${perfil.name} ${lineaFecha(p, true, perfil)}")
                    }
                }
                if (lineas.isEmpty()) EstadoParte(false, "Sin ajuste (factor 1000)", "nunca") else EstadoParte(true, lineas.joinToString("\n"), fechas.joinToString(" · "))
            }
            Parte.RECTA -> {
                val lineas = ArrayList<String>()
                val fechas = ArrayList<String>()
                for (perfil in ControlLogica.Perfil.values()) {
                    if (guardada.hayBalanceGuardado(perfil) || guardada.fecha(p, perfil) > 0L) {
                        lineas.add("${perfil.name}: balance ${guardada.balance(perfil)} de 1000")
                        fechas.add("${perfil.name} ${lineaFecha(p, true, perfil)}")
                    }
                }
                if (lineas.isEmpty()) EstadoParte(false, "Sin ajuste (balance por defecto: AUTO ${CalibracionNativa.BALANCE_DEFECTO_AUTO}, MANUAL ${CalibracionNativa.BALANCE_DEFECTO_MANUAL})", "nunca")
                else EstadoParte(true, lineas.joinToString("\n"), fechas.joinToString(" · "))
            }
        }
    }

    /** Las partes sin las que el navegador usa valores de ejemplo: si faltan, el chip va en ámbar. */
    private fun esObligatoria(p: Parte) = p == Parte.ARRANQUE || p == Parte.VELOCIDAD || p == Parte.GIRO_IZQ || p == Parte.GIRO_DER

    /** Arma las tarjetas de la lista (una por parte). */
    private fun armarLista() {
        // Solo se avisa de lo que falta de lo indispensable; el retroceso y lo demás se ven en su tarjeta.
        val falta = guardada.avisoSiFalta()
        textoListaAviso.text = falta ?: ""
        textoListaAviso.visibility = if (falta == null) View.GONE else View.VISIBLE
        listaPartes.removeAllViews()
        for (p in Parte.values()) {
            val fila = layoutInflater.inflate(R.layout.item_calibracion_parte, listaPartes, false)
            val estado = estadoDeParte(p)
            fila.findViewById<TextView>(R.id.parte_titulo).text = nombreParte(p)
            fila.findViewById<ChipEstado>(R.id.parte_chip).apply {
                if (estado.medido && estado.pocaPrecision) {
                    poner(Nivel.AVISO, getString(R.string.cal_chip_poca_precision))
                } else if (estado.medido) {
                    poner(Nivel.OK, getString(R.string.cal_chip_calibrado))
                } else {
                    poner(if (esObligatoria(p)) Nivel.AVISO else Nivel.NEUTRO, getString(R.string.cal_chip_sin))
                }
            }
            fila.findViewById<TextView>(R.id.parte_valor).text = estado.valor
            fila.findViewById<TextView>(R.id.parte_fecha).text = "Última calibración: ${estado.fecha}"
            fila.findViewById<View>(R.id.parte_pilas).visibility = if (p.dependeDePilas) View.VISIBLE else View.GONE
            fila.findViewById<MaterialButton>(R.id.parte_boton).apply {
                contentDescription = "Calibrar solo ${nombreParte(p)}"
                setOnClickListener { iniciarParte(p, false) }
            }
            listaPartes.addView(fila)
        }
        // Tarjeta de los SENSORES (giroscopio y láser): no es una parte que se mida, abre su propia pantalla.
        val filaSensores = layoutInflater.inflate(R.layout.item_calibracion_parte, listaPartes, false)
        filaSensores.findViewById<TextView>(R.id.parte_titulo).setText(R.string.sens_titulo)
        filaSensores.findViewById<ChipEstado>(R.id.parte_chip).apply {
            when {
                logica.capsGiroscopio == null -> poner(Nivel.NEUTRO, getString(R.string.sens_chip_ninguno))
                logica.capsGiroscopio == true && logica.capsLaser == true -> poner(Nivel.OK, getString(R.string.sens_chip_ok))
                else -> poner(Nivel.AVISO, getString(R.string.sens_chip_parcial))
            }
        }
        filaSensores.findViewById<TextView>(R.id.parte_valor).setText(R.string.sens_en_lista_detalle)
        filaSensores.findViewById<View>(R.id.parte_fecha).visibility = View.GONE
        filaSensores.findViewById<View>(R.id.parte_pilas).visibility = View.GONE
        filaSensores.findViewById<MaterialButton>(R.id.parte_boton).apply {
            setText(R.string.sens_abrir)
            setOnClickListener { startActivity(android.content.Intent(this@CalibracionActivity, SensoresActivity::class.java)) }
        }
        listaPartes.addView(filaSensores)
    }

    // ── Mostrar el paso ───────────────────────────────────────────────────

    private fun mostrarPaso() {
        val enLista = paso == Paso.LISTA
        listaContenedor.visibility = if (enLista) View.VISIBLE else View.GONE
        contenidoPaso.visibility = if (enLista) View.GONE else View.VISIBLE
        progreso.visibility = if (enLista) View.GONE else View.VISIBLE
        botonProbar.visibility = View.GONE
        botonYa.visibility = View.GONE
        campoLayout.visibility = View.GONE
        botonGuardar.visibility = View.GONE
        botonRepetir.visibility = View.GONE
        botonUsar.visibility = View.GONE
        textoNota.visibility = View.GONE
        panelAjuste.visibility = View.GONE
        panelGiroPrueba.visibility = View.GONE
        bloqueGiro.visibility = View.GONE
        bloqueRecta.visibility = View.GONE
        bloqueDesp.visibility = View.GONE
        botonRestaurar.visibility = View.GONE
        etiquetaLado.visibility = View.VISIBLE
        filaLado.visibility = View.VISIBLE
        botonOmitir.visibility = if (todo != null && !enLista) View.VISIBLE else View.GONE

        if (enLista) {
            textoPaso.setText(R.string.cal_paso_lista)
            botonProbar.visibility = View.VISIBLE
            botonProbar.setText(R.string.cal_b_todo)
            actualizarBotones()
            return
        }

        val p = parte
        val total = secuenciaActual.size
        val lista = todo
        textoPaso.text = when {
            lista != null -> getString(R.string.cal_paso_todo, todoIndice + 1, lista.size, indicePaso + 1, total)
            total > 1 -> getString(R.string.cal_paso, indicePaso + 1, total)   // el nombre de la parte ya es el título de la pantalla
            else -> getString(R.string.cal_paso_unico)
        }
        if (progreso.max != total) {
            progreso.max = total
        }
        if (progreso.progress != indicePaso + 1) {
            progreso.setProgressCompat(indicePaso + 1, true)
        }
        ilustracion.escena = escenaDelPaso()
        ilustracion.enMarcha = ocupado()

        when (paso) {
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
            Paso.AVANCE, Paso.RETROCESO -> {
                val datos = datosDelPaso()!!
                val hechas = cuantas(datos)
                textoTitulo.setText(if (paso == Paso.AVANCE) R.string.cal_t_avance else R.string.cal_t_retroceso)
                textoInstruccion.setText(if (paso == Paso.AVANCE) R.string.cal_i_avance else R.string.cal_i_retroceso)
                textoNota.setText(if (paso == Paso.AVANCE) R.string.cal_n_avance else R.string.cal_n_retroceso)
                textoNota.visibility = View.VISIBLE
                botonProbar.visibility = View.VISIBLE
                botonProbar.text = if (hechas >= totalPruebas()) getString(R.string.cal_b_todas) else getString(R.string.cal_b_probar_n, tiempoActual(), hechas + 1, totalPruebas())
                if (esperandoDato) {
                    campoLayout.visibility = View.VISIBLE
                    ponerPista(getString(if (paso == Paso.AVANCE) R.string.cal_h_cm else R.string.cal_h_cm_atras))
                    botonGuardar.visibility = View.VISIBLE
                }
                botonRepetir.visibility = View.VISIBLE
                textoDatos.text = armarTabla(datos)
            }
            Paso.FRENADO -> {
                textoTitulo.setText(R.string.cal_t_frenado)
                textoInstruccion.setText(R.string.cal_i_frenado)
                textoNota.setText(R.string.cal_n_frenado)
                textoNota.visibility = View.VISIBLE
                botonProbar.visibility = View.VISIBLE
                botonProbar.text = if (frenadoMm.size >= REPETICIONES) getString(R.string.cal_b_todas) else getString(R.string.cal_b_probar_frenado, TIEMPO_FRENADO_MS, frenadoMm.size + 1, REPETICIONES)
                if (esperandoDato) {
                    campoLayout.visibility = View.VISIBLE
                    ponerPista(getString(R.string.cal_h_cm_total))
                    botonGuardar.visibility = View.VISIBLE
                }
                botonRepetir.visibility = View.VISIBLE
                val cal = guardada.leer()
                textoDatos.text = "Con la velocidad guardada (" + fmt("%.1f", cal[CalibracionNativa.CAL_MS_POR_CM_X10] / 10.0) + " ms por cm) y el arranque de ${cal[CalibracionNativa.CAL_ARRANQUE_MS]} ms, " +
                    "$TIEMPO_FRENADO_MS ms deberían avanzar " + fmt("%.1f", CalibracionNativa.msAMm(cal, TIEMPO_FRENADO_MS) / 10.0) + " cm sin contar el frenado.\n" +
                    "Medidas (cm): " + (if (frenadoMm.isEmpty()) "-" else frenadoMm.joinToString("   ") { fmt("%.1f", it / 10.0) })
            }
            Paso.GIRO_IZQ, Paso.GIRO_DER -> mostrarGiroCalibracion()
            Paso.GIRO_VISUAL -> {
                camaraAbiertaMostrada = camara.abierta
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
            Paso.VISION -> {
                camaraAbiertaMostrada = camara.abierta
                textoTitulo.setText(R.string.cal_t_vision)
                textoInstruccion.setText(R.string.cal_i_vision)
                textoNota.setText(R.string.cal_n_vision)
                textoNota.visibility = View.VISIBLE
                botonProbar.visibility = View.VISIBLE
                botonProbar.text = if (visionHechos >= 3) getString(R.string.cal_b_vision_listo) else getString(R.string.cal_b_vision_n, visionHechos + 1)
                botonRepetir.visibility = View.VISIBLE
                botonRestaurar.visibility = View.VISIBLE
                val sb = StringBuilder()
                sb.append("Cámara: " + (if (camara.abierta) "abierta" else if (camara.error.isNotEmpty()) "no se pudo abrir (revisa el permiso de cámara y cierra otras apps que la usen)" else "abriendo...") + "\n")
                for (i in 0 until 3) {
                    sb.append("Lugar ${i + 1}: " + (if (i < visionHechos) "${visionLugares[i][0]} cuadros capturados" else if (i == visionHechos && visionCapturando) "capturando: ${visionLugares[i][0]} de ${MASC_CUADROS}" else "falta") + "\n")
                }
                if (visionInforme.isNotEmpty()) {
                    sb.append("\n").append(visionInforme)
                } else if (guardada.hayVision() && visionHechos == 0) {
                    sb.append("\nGuardado antes: ").append(guardada.celdasEnmascaradas()).append(" celdas enmascaradas.\n").append(guardada.informeVision())
                }
                textoDatos.text = sb.toString()
            }
            Paso.DESPLAZ -> mostrarDesplazamiento()
            Paso.RESUMEN -> {
                textoTitulo.setText(R.string.cal_t_resumen)
                textoInstruccion.setText(R.string.cal_i_resumen)
                textoDatos.text = textoResumen + (if (avisoGuardar.isNotEmpty()) "\n\n$avisoGuardar" else "")
                botonUsar.visibility = View.VISIBLE
            }
            Paso.GIRO_90 -> mostrarGiro90()
            Paso.RECTA -> mostrarRecta()
            Paso.LISTA -> {}
        }
        // Cuando aparece el campo del resultado, la pantalla baja hasta el botón «Guardar este resultado» (solo esa primera vez).
        if (esperandoDato && !datoMostrado) {
            scroll.post { scroll.fullScroll(View.FOCUS_DOWN) }   // hasta el final: ahí están el campo, «Guardar» y «Borrar el último dato» (botonGuardar.bottom es relativo a su contenedor, no al scroll)
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
        val ocupado = ocupado()
        val enLista = paso == Paso.LISTA
        filaNav.visibility = if (enLista) View.GONE else View.VISIBLE
        botonAtras.visibility = View.VISIBLE
        botonSiguiente.visibility = if (paso == Paso.RESUMEN) View.GONE else View.VISIBLE   // en el resultado queda «Atrás» (para corregir un dato) y «Guardar esta calibración»
        val probarActivo = when (paso) {
            Paso.LISTA -> !ocupado
            Paso.REACCION -> estadoReaccion == 0 && reacciones.size < TOQUES_REACCION
            Paso.ARRANQUE -> listoParaProbar() && !arrancando && toquesArranque.size < TOQUES_ARRANQUE
            Paso.AVANCE, Paso.RETROCESO -> listoParaProbar() && !corriendo && !esperandoDato && cuantas(datosDelPaso()!!) < totalPruebas()
            Paso.FRENADO -> listoParaProbar() && !corriendo && !esperandoDato && frenadoMm.size < REPETICIONES
            Paso.GIRO_IZQ, Paso.GIRO_DER -> listoParaProbar() && !corriendo && !esperandoDato && cuantasGiro() < totalGiro()
            Paso.GIRO_VISUAL -> listoParaProbar() && camara.abierta && !corriendo && !esperandoDato && fovReal.size < 3
            Paso.VISION -> camara.abierta && !visionCapturando && visionHechos < 3
            Paso.DESPLAZ -> listoParaProbar() && !corriendo && !esperandoDato
            Paso.GIRO_90 -> listoParaProbar() && !corriendo && !esperandoDato
            Paso.RECTA -> listoParaProbar() && !corriendo && !esperandoDato && !rectaLista && intentosRecta < CalibracionNativa.RECTA_MAX_ITERACIONES
            Paso.RESUMEN -> false
        }
        habilitar(botonProbar, probarActivo)
        habilitar(botonYa, (paso == Paso.REACCION && estadoReaccion != 0) || (paso == Paso.ARRANQUE && arrancando))
        habilitar(botonRepetir, !ocupado && when (paso) {
            Paso.ARRANQUE -> toquesArranque.isNotEmpty()
            Paso.AVANCE, Paso.RETROCESO -> cuantas(datosDelPaso()!!) > 0 || esperandoDato
            Paso.FRENADO -> frenadoMm.isNotEmpty() || esperandoDato
            Paso.GIRO_IZQ, Paso.GIRO_DER -> cuantasGiro() > 0 || esperandoDato
            Paso.GIRO_VISUAL -> fovReal.isNotEmpty() || esperandoDato
            Paso.VISION -> visionHechos > 0 || visionCapturando
            Paso.DESPLAZ -> esperandoDato   // descarta el giro en curso (no se guarda esa repetición) para volver a girar
            else -> false
        })
        habilitar(botonAtras, !ocupado)
        botonAtras.setText(if (indicePaso == 0) R.string.cal_b_cancelar else R.string.cal_atras)
        val ultimo = indicePaso >= secuenciaActual.size - 1
        habilitar(botonSiguiente, !ocupado && pasoCompleto() && !esperandoDato)
        botonSiguiente.setText(when {
            ultimo -> R.string.cal_b_listo
            secuenciaActual.getOrNull(indicePaso + 1) == Paso.RESUMEN -> R.string.cal_b_ver_resumen
            else -> R.string.cal_siguiente
        })
        habilitar(botonOmitir, !ocupado)
        habilitar(botonUsar, !ocupado && resultadoOk)

        // Solo UN botón se ve como primario: el que toca tocar ahora. Con una prueba en marcha, ninguno (lo importante es el PARO y el botón grande).
        val destacado: MaterialButton? = when {
            ocupado -> null
            paso == Paso.LISTA -> botonProbar
            paso == Paso.RESUMEN -> botonUsar
            esperandoDato -> if (paso == Paso.GIRO_90) null else botonGuardar   // en el giro de 90° las opciones de «¿cómo giró?» son las que se tocan
            probarActivo -> botonProbar
            else -> botonSiguiente
        }
        for (boton in arrayOf(botonProbar, botonGuardar, botonSiguiente, botonUsar)) {
            resaltar(boton, boton === destacado)
        }
    }

    // ── Pruebas ───────────────────────────────────────────────────────────

    private fun alProbar() {
        when (paso) {
            Paso.LISTA -> iniciarTodo()
            Paso.REACCION -> empezarReaccion()
            Paso.ARRANQUE -> empezarArranque()
            Paso.AVANCE, Paso.RETROCESO, Paso.FRENADO, Paso.GIRO_IZQ, Paso.GIRO_DER -> empezarPruebaDeTiempo()
            Paso.GIRO_VISUAL -> empezarGiroVisual()
            Paso.VISION -> empezarCapturaVision()
            Paso.DESPLAZ -> empezarGiroDesplazamiento()
            Paso.GIRO_90 -> empezarGiro90()
            Paso.RECTA -> empezarRecta()
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
        if (ms <= 0) {
            return
        }
        val comando = comandoDelPaso()
        corriendo = true
        if (paso == Paso.GIRO_IZQ || paso == Paso.GIRO_DER) {
            giroFijoMs[indiceLado(ladoDelPaso())] = ms   // para calcular los ms por grado con el tiempo con que se hizo
        }
        Log.i(CalibracionGuardada.ETIQUETA, "prueba $paso $comando durante $ms ms" + (if (paso == Paso.GIRO_IZQ || paso == Paso.GIRO_DER) " (${giroPrueba.texto()})" else ""))
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
        val ms = CalibracionNativa.gradosAMs(guardada.leerAjustada(ControlLogica.Perfil.AUTO), 90)
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

    // ── Giro de prueba (pruebas de giro y desplazamiento) ─────────────────

    /** Un botón de selección (perfil, lado o giro de prueba): el elegido lleva un borde del acento y fondo de tarjeta elevada; los demás se ven como secundarios. No es «el primario». */
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

    /** Muestra el panel «Giro de prueba»: los botones, la duración libre si toca, qué se va a mandar y el aviso si se mezclarían dos giros de prueba distintos. */
    private fun mostrarPanelGiroPrueba(conTres: Boolean) {
        panelGiroPrueba.visibility = View.VISIBLE
        marcarSeleccion(botonGp90, giroPrueba.tipo == GiroPrueba.Tipo.NOVENTA)
        marcarSeleccion(botonGp180, giroPrueba.tipo == GiroPrueba.Tipo.CIENTO_OCHENTA)
        marcarSeleccion(botonGpLibre, giroPrueba.tipo == GiroPrueba.Tipo.LIBRE)
        botonGpTres.visibility = if (conTres) View.VISIBLE else View.GONE
        marcarSeleccion(botonGpTres, giroPrueba.tipo == GiroPrueba.Tipo.TRES_TIEMPOS)
        filaGpLibre.visibility = if (giroPrueba.tipo == GiroPrueba.Tipo.LIBRE) View.VISIBLE else View.GONE
        textoGpMs.text = "$libreMs ms"
        val bloqueado = ocupado() || esperandoDato
        for (b in listOf(botonGp90, botonGp180, botonGpLibre, botonGpTres, botonGpMenos, botonGpMas)) {
            habilitar(b, !bloqueado)
        }
        val desplazamiento = paso == Paso.DESPLAZ
        val lado = if (desplazamiento) ladoAjuste else ladoDelPaso()
        val perfil = if (desplazamiento) perfilAjuste else ControlLogica.Perfil.AUTO
        textoGpDetalle.text = when (giroPrueba.tipo) {
            GiroPrueba.Tipo.TRES_TIEMPOS -> "Gira 500, 1000 y 1500 ms, 3 veces cada uno (9 pruebas). Mide también cuánto sigue girando al frenar."
            GiroPrueba.Tipo.LIBRE -> "Gira $libreMs ms" + (if (desplazamiento) "." else ", 3 veces.") + " Dices cuántos grados giró de verdad."
            else -> "Gira ${msDeGiroPrueba(lado, giroPrueba, perfil)} ms: lo que el navegador pide hoy para ${giroPrueba.grados()}° a la ${if (lado < 0) "izquierda" else "derecha"}." +
                (if (desplazamiento) "" else " Dices cuántos grados giró de verdad; si no fueron ${giroPrueba.grados()}, el giro se corrige.")
        }
        textoGpAviso.text = (if (desplazamiento) guardada.avisoMezclaDesplazamiento(perfilAjuste, ladoAjuste, giroPrueba) else guardada.avisoMezclaGiro(ladoDelPaso(), giroPrueba)) ?: ""
        textoGpAviso.visibility = if (textoGpAviso.text.isEmpty()) View.GONE else View.VISIBLE
    }

    private fun elegirGiroPrueba(prueba: GiroPrueba) {
        if (ocupado() || esperandoDato || prueba.mismaQue(giroPrueba) && prueba.tipo != GiroPrueba.Tipo.LIBRE) {
            return
        }
        val cambia = !prueba.mismaQue(giroPrueba)
        if (cambia && hayMedidasDelGiro()) {
            // Mezclar dos giros de prueba en una misma medida no sirve: se borran las repeticiones de este lado.
            borrarMedidasDelGiro()
            resumenAjuste = "Se borraron las medidas de este lado porque cambiaste el giro de prueba."
        }
        giroPrueba = prueba
        mostrarPaso()
    }

    private fun cambiarLibreMs(delta: Int) {
        if (ocupado() || esperandoDato) {
            return
        }
        val nuevo = (libreMs + delta).coerceIn(GiroPrueba.LIBRE_MIN_MS, GiroPrueba.LIBRE_MAX_MS)
        if (nuevo == libreMs) {
            return
        }
        if (hayMedidasDelGiro() && giroPrueba.tipo == GiroPrueba.Tipo.LIBRE) {
            borrarMedidasDelGiro()
            resumenAjuste = "Se borraron las medidas de este lado porque cambiaste la duración."
        }
        libreMs = nuevo
        if (giroPrueba.tipo == GiroPrueba.Tipo.LIBRE) {
            giroPrueba = GiroPrueba(GiroPrueba.Tipo.LIBRE, libreMs)
        }
        mostrarPaso()
    }

    private fun hayMedidasDelGiro(): Boolean = when (paso) {
        Paso.GIRO_IZQ, Paso.GIRO_DER -> cuantasGiro() > 0
        Paso.DESPLAZ -> despAdelanteUm.any { it.isNotEmpty() }   // las repeticiones de los dos lados deben ser del mismo giro de prueba
        else -> false
    }

    private fun borrarMedidasDelGiro() {
        when (paso) {
            Paso.GIRO_IZQ, Paso.GIRO_DER -> {
                for (l in giroTres[indiceLado(ladoDelPaso())]) l.clear()
                giroFijo[indiceLado(ladoDelPaso())].clear()
            }
            Paso.DESPLAZ -> {
                for (l in despAdelanteUm) l.clear()
                for (l in despDerechaUm) l.clear()
                for (l in despGrados) l.clear()
            }
            else -> {}
        }
    }

    /** La pantalla de la calibración del giro de un lado: el panel del giro de prueba, el botón «Probar», el campo de los grados y lo ya anotado. */
    private fun mostrarGiroCalibracion() {
        val lado = ladoDelPaso()
        textoTitulo.setText(if (lado < 0) R.string.cal_t_giro_izq else R.string.cal_t_giro_der)
        textoInstruccion.setText(R.string.cal_i_giro)
        textoNota.text = getString(R.string.cal_n_giro) + "\n" + getString(R.string.cal_aj_pilas)
        textoNota.visibility = View.VISIBLE
        mostrarPanelGiroPrueba(true)
        val hechas = cuantasGiro()
        val total = totalGiro()
        botonProbar.visibility = View.VISIBLE
        botonProbar.text = when {
            hechas >= total -> getString(R.string.cal_b_todas)
            giroPrueba.tipo == GiroPrueba.Tipo.TRES_TIEMPOS -> getString(R.string.cal_b_probar_n, tiempoActual(), hechas + 1, total)
            else -> getString(R.string.cal_b_probar_giro_n, textoGiroCorto(), tiempoActual(), hechas + 1, total)
        }
        if (esperandoDato) {
            campoLayout.visibility = View.VISIBLE
            ponerPista(getString(R.string.cal_h_grados))
            botonGuardar.visibility = View.VISIBLE
        }
        botonRepetir.visibility = View.VISIBLE
        val sb = StringBuilder()
        if (giroPrueba.tipo == GiroPrueba.Tipo.TRES_TIEMPOS) {
            sb.append(armarTabla(giroTres[indiceLado(lado)]))
        } else {
            val grados = giroFijo[indiceLado(lado)]
            sb.append("Giros anotados (${giroPrueba.texto()}): ").append(if (grados.isEmpty()) "-" else grados.joinToString("   ") { String.format(Locale.US, "%.0f°", it / 10.0) }).append("\n")
        }
        if (resumenAjuste.isNotEmpty()) {
            sb.append("\n").append(resumenAjuste)
        }
        textoDatos.text = sb.toString()
    }

    private fun textoGiroCorto() = when (giroPrueba.tipo) {
        GiroPrueba.Tipo.NOVENTA -> "giro de 90°"
        GiroPrueba.Tipo.CIENTO_OCHENTA -> "giro de 180°"
        else -> "giro libre"
    }

    // ── Desplazamiento del centro al girar ────────────────────────────────

    /** Los grados que se esperan que gire el giro de prueba (los ángulos de 45 en 45 más cercanos): lo que se propone en el contador del ángulo. */
    private fun anguloEsperado(): Int {
        val pedido = giroPrueba.grados()
        if (pedido > 0) {
            return pedido
        }
        val cal = guardada.leerAjustada(perfilAjuste)
        val izq = ladoAjuste < 0
        val msGrado = cal[if (izq) CalibracionNativa.CAL_MS_GRADO_IZQ_X10 else CalibracionNativa.CAL_MS_GRADO_DER_X10]
        val grados = (giroPrueba.ms - cal[CalibracionNativa.CAL_ARRANQUE_MS]).coerceAtLeast(0) * 10 / msGrado.coerceAtLeast(1)
        return (Math.round(grados / ANGULO_PASO.toDouble()).toInt() * ANGULO_PASO).coerceIn(ANGULO_PASO, 360)
    }

    private fun textoMm(mm: Int) = String.format(Locale.US, "%+.1f cm", mm / 10.0)

    private fun mostrarDesplazamiento() {
        textoTitulo.setText(R.string.cal_t_desplazamiento)
        textoInstruccion.setText(R.string.cal_i_desplazamiento)
        textoNota.text = getString(R.string.cal_n_desplazamiento) + "\n" + getString(R.string.cal_aj_pilas)
        textoNota.visibility = View.VISIBLE
        panelAjuste.visibility = View.VISIBLE
        marcarSeleccion(botonPerfilAuto, perfilAjuste == ControlLogica.Perfil.AUTO)
        marcarSeleccion(botonPerfilManual, perfilAjuste == ControlLogica.Perfil.MANUAL)
        marcarSeleccion(botonLadoIzq, ladoAjuste == CalibracionNativa.LADO_IZQUIERDA)
        marcarSeleccion(botonLadoDer, ladoAjuste == CalibracionNativa.LADO_DERECHA)
        mostrarPanelGiroPrueba(false)
        botonRestaurar.visibility = View.VISIBLE
        val indice = indiceLado(ladoAjuste)
        val hechas = despAdelanteUm[indice].size
        botonProbar.visibility = View.VISIBLE
        botonProbar.text = getString(R.string.cal_b_girar_desp, (hechas + 1).coerceAtMost(REPETICIONES_DESP), REPETICIONES_DESP)
        if (esperandoDato) {
            bloqueDesp.visibility = View.VISIBLE
            contadorAdelante.valor.text = textoMm(despAdelanteMm)
            contadorDerecha.valor.text = textoMm(despDerechaMm)
            contadorAngulo.valor.text = "$despAngulo°"
            // Aviso no bloqueante: de 15 cm de desplazamiento total para arriba (la medida se acepta hasta ~45 cm en un giro de 90°)
            val grande = avisoDesplazamientoGrande(despAngulo, despAdelanteMm, despDerechaMm)
            if (grande != null) avisoDesp.mostrar(Nivel.AVISO, grande.quePaso, grande.queHacer) else avisoDesp.ocultar()
            botonGuardar.visibility = View.VISIBLE
            botonGuardar.setText(R.string.cal_b_guardar_rep)
            botonRepetir.visibility = View.VISIBLE
        } else {
            botonGuardar.setText(R.string.cal_guardar)
        }
        val sb = StringBuilder()
        val (ga, gd) = guardada.desplazamiento(perfilAjuste, ladoAjuste)
        sb.append("Guardado (${perfilAjuste.name}, ${nombreLado(ladoAjuste)}): " + fmt("%.3f mm por grado adelante, %.3f a la derecha", ga / 1000.0, gd / 1000.0) +
            "  ·  última vez: ${lineaFecha(Parte.DESPLAZAMIENTO, ga != 0 || gd != 0, perfilAjuste)}" +
            (if (guardada.desplazamientoPocaPrecision(perfilAjuste, ladoAjuste)) "\nEste lado se guardó con poca precisión (las repeticiones no coincidían)." else "") + "\n")
        for (i in despAdelanteUm[indice].indices) {
            sb.append("Repetición ${i + 1}: " + fmt("%.3f mm por grado adelante, %.3f a la derecha", despAdelanteUm[indice][i] / 1000.0, despDerechaUm[indice][i] / 1000.0) + "\n")
        }
        if (resumenAjuste.isNotEmpty()) {
            sb.append("\n").append(resumenAjuste)
        }
        textoDatos.text = sb.toString()
    }

    /** El carro gira el giro de prueba hacia el lado elegido y se detiene; después se mide con la cinta. */
    private fun empezarGiroDesplazamiento() {
        if (!conectado() || corriendo) {
            return
        }
        val ms = msDeGiroPrueba(ladoAjuste, giroPrueba, perfilAjuste)
        if (ms <= 0) {
            return
        }
        val comando = if (ladoAjuste < 0) "LEFT" else "RIGHT"
        corriendo = true
        resumenAjuste = ""
        despMsUltimo = ms
        Log.i(CalibracionGuardada.ETIQUETA, "desplazamiento: $comando durante $ms ms (perfil ${perfilAjuste.name}, ${giroPrueba.texto()})")
        logica.presionar(comando)
        mostrarPaso()
        principal.postDelayed({
            logica.parar()
            corriendo = false
            esperandoDato = true
            despAdelanteMm = 0
            despDerechaMm = 0
            despAngulo = anguloEsperado()
            mostrarPaso()
        }, ms.toLong())
    }

    private fun cambiarMedidaDesp(dAdelante: Int, dDerecha: Int, dAngulo: Int) {
        if (!esperandoDato || paso != Paso.DESPLAZ) {
            return
        }
        despAdelanteMm = (despAdelanteMm + dAdelante).coerceIn(-DESP_MAX_MM, DESP_MAX_MM)
        despDerechaMm = (despDerechaMm + dDerecha).coerceIn(-DESP_MAX_MM, DESP_MAX_MM)
        despAngulo = (despAngulo + dAngulo).coerceIn(ANGULO_PASO, 360)
        mostrarPaso()
    }

    /** Una línea TURN_DIAG de la calibración del desplazamiento (el mismo registro de los giros de la ruta; origen=calibracion). */
    private fun diagDesp(texto: String) {
        Log.i("TURN_DIAG", "TURN_DIAG lado=${if (ladoAjuste < 0) "IZQ" else "DER"} origen=calibracion perfil=${perfilAjuste.name} $texto")
    }

    /** Pasa la medida de esta repetición a µm por grado (en C), la anota y, con 2 repeticiones del lado, las compara (se aceptan si difieren 30 % o menos, o 1 cm o menos). Si no coinciden NO se
     *  borra nada en silencio: se dice el motivo exacto y se pregunta (repetir, guardar el promedio de todos modos, o cancelar). */
    private fun guardarRepeticionDesp() {
        if (!esperandoDato) {
            return
        }
        val indice = indiceLado(ladoAjuste)
        val (codigo, adelante, derecha) = CalibracionNativa.desplazamientoDeMedida(ladoAjuste, despAdelanteMm, despDerechaMm, despAngulo).let { Triple(it[0], it[1], it[2]) }
        if (codigo != CalibracionNativa.OK) {
            // La medida no sirve: no se pierde nada. Se queda con los contadores para corregirla y volver a guardar.
            resumenAjuste = "Esa medida no se pudo usar (" + fmt("%.1f cm adelante, %.1f cm de lado, %d°", despAdelanteMm / 10.0, despDerechaMm / 10.0, despAngulo) + "): " +
                (if (codigo == CalibracionNativa.ERR_RANGO) "el desplazamiento total pasa de ~45 cm en un giro de 90° (el máximo que acepta la calibración); revisa que mediste del centro inicial al centro final." else CalibracionNativa.textoError(codigo)) +
                " Corrígela con los contadores y toca «Guardar esta repetición» otra vez."
            diagDesp("repeticion=${despAdelanteUm[indice].size + 1} resultado=medida_rechazada motivo=medida_no_valida codigo=$codigo medida_mm=($despAdelanteMm,$despDerechaMm) grados=$despAngulo")
            Log.w(CalibracionGuardada.ETIQUETA, "desplazamiento: medida no valida (codigo $codigo)")
            mostrarPaso()
            return
        }
        Log.i(CalibracionGuardada.ETIQUETA, "desplazamiento medida: perfil=${perfilAjuste.name} lado=${nombreLado(ladoAjuste)} adelante_mm=$despAdelanteMm derecha_mm=$despDerechaMm grados=$despAngulo -> " +
            "$adelante um/grado adelante, $derecha um/grado derecha (giro ${despMsUltimo} ms, ${giroPrueba.texto()})")
        despAdelanteUm[indice].add(adelante)
        despDerechaUm[indice].add(derecha)
        despGrados[indice].add(despAngulo)
        val arco = CalibracionNativa.desplazamientoArco(despAngulo, despAdelanteMm, despDerechaMm)
        val grande = avisoDesplazamientoGrande(despAngulo, despAdelanteMm, despDerechaMm)
        diagDesp("repeticion=${despAdelanteUm[indice].size} resultado=guardada medida_mm=($despAdelanteMm,$despDerechaMm) grados=$despAngulo um_grado=($adelante,$derecha) giro_ms=$despMsUltimo prueba=\"${giroPrueba.texto()}\"" +
            " total_mm=${arco[1]} radio_arco_mm=${arco[2]}" + (if (grande != null) " aviso=desplazamiento_grande" else ""))
        esperandoDato = false
        if (despAdelanteUm[indice].size < REPETICIONES_DESP) {
            resumenAjuste = "Repetición ${despAdelanteUm[indice].size} guardada. Falta ${REPETICIONES_DESP - despAdelanteUm[indice].size}: vuelve a poner el carro en la marca y toca «Girar»." +
                (if (grande != null) "\nAviso: ${grande.quePaso}. ${grande.queHacer}" else "")
            mostrarPaso()
            return
        }
        val r = CalibracionNativa.desplazamientoPromediar(despAdelanteUm[indice].toIntArray(), despDerechaUm[indice].toIntArray(), despGrados[indice].toIntArray())
        when (r[0]) {
            CalibracionNativa.OK -> guardarPromedioDesp(indice, r[1], r[2], false)
            CalibracionNativa.ERR_DIFIEREN -> preguntarDiferenciaDesp(indice, r)
            else -> {
                val motivo = "No se pudo promediar: ${CalibracionNativa.textoError(r[0])}"
                diagDesp("resultado=rechazada motivo=no_se_pudo_promediar codigo=${r[0]}")
                despAdelanteUm[indice].clear(); despDerechaUm[indice].clear(); despGrados[indice].clear()
                resumenAjuste = "$motivo No se guardó nada: se descartaron las repeticiones."
                mostrarPaso()
            }
        }
    }

    /** El texto exacto de por qué dos repeticiones no coinciden, con los números de C (décimas de mm → cm). */
    private fun motivoDiferencia(r: IntArray): String =
        fmt("repetición %d: %.1f cm, repetición %d: %.1f cm, difieren %.1f cm (se acepta hasta %.1f cm: el 30 %% o 1 cm, lo que sea mayor)",
            r[3], r[5] / 100.0, r[4], r[6] / 100.0, r[7] / 100.0, r[8] / 100.0)

    /** Las dos repeticiones no coinciden: se dice el motivo y se deja elegir. Nada se guarda hasta que el usuario decida. */
    private fun preguntarDiferenciaDesp(indice: Int, r: IntArray) {
        val motivo = motivoDiferencia(r)
        resumenAjuste = "Las repeticiones no coinciden: $motivo. Elige qué hacer."
        diagDesp("resultado=rechazada motivo=difieren_mas_de_30pct_y_1cm rep_a=${r[3]} rep_b=${r[4]} mov_a_cm=${fmt("%.1f", r[5] / 100.0)} mov_b_cm=${fmt("%.1f", r[6] / 100.0)} " +
            "dif_cm=${fmt("%.1f", r[7] / 100.0)} tolerancia_cm=${fmt("%.1f", r[8] / 100.0)}")
        Log.w(CalibracionGuardada.ETIQUETA, "desplazamiento: las repeticiones no coinciden ($motivo)")
        mostrarPaso()
        Dialogos.tresOpciones(this, "Las repeticiones no coinciden",
            motivo.replaceFirstChar { it.uppercase() } + ".\n\nLo mejor es repetir la medición. Si guardas el promedio de todos modos, la calibración será menos confiable.",
            "Repetir la medición", {
                despAdelanteUm[indice].clear(); despDerechaUm[indice].clear(); despGrados[indice].clear()
                resumenAjuste = "Se descartaron las dos repeticiones. ${motivo.replaceFirstChar { it.uppercase() }}. Empieza otra vez con la repetición 1: pon el carro en la marca y toca «Girar»."
                diagDesp("resultado=repetir")
                mostrarPaso()
            },
            "Guardar el promedio de todos modos", {
                val p = CalibracionNativa.desplazamientoPromedio(despAdelanteUm[indice].toIntArray(), despDerechaUm[indice].toIntArray())
                if (p[0] == CalibracionNativa.OK) {
                    guardarPromedioDesp(indice, p[1], p[2], true)
                } else {
                    resumenAjuste = "No se pudo calcular el promedio: ${CalibracionNativa.textoError(p[0])} No se guardó nada."
                    diagDesp("resultado=rechazada motivo=promedio_no_valido codigo=${p[0]}")
                    mostrarPaso()
                }
            },
            "Cancelar", {
                despAdelanteUm[indice].clear(); despDerechaUm[indice].clear(); despGrados[indice].clear()
                resumenAjuste = "Cancelado: no se guardó nada. ${motivo.replaceFirstChar { it.uppercase() }}. Los valores guardados antes siguen igual."
                diagDesp("resultado=cancelado")
                mostrarPaso()
            })
    }

    /** Guarda el promedio de las dos repeticiones del lado. `pocaPrecision` = las repeticiones no coincidían y el usuario eligió guardar de todos modos: la lista lo marca. */
    private fun guardarPromedioDesp(indice: Int, promedioAdelante: Int, promedioDerecha: Int, pocaPrecision: Boolean) {
        val prueba = giroPrueba
        val gradosMedidos = if (despGrados[indice].isEmpty()) 90 else despGrados[indice].sum() / despGrados[indice].size   // el ángulo medio de las repeticiones (el modelo de pivote lo usa)
        guardada.guardarDesplazamiento(perfilAjuste, ladoAjuste, promedioAdelante, promedioDerecha, pocaPrecision, gradosMedidos.coerceIn(10, 360))
        guardada.guardarGiroPruebaDeDesplazamiento(perfilAjuste, ladoAjuste, prueba)
        guardada.marcarCalibrado(Parte.DESPLAZAMIENTO, perfilAjuste)
        guardada.aplicarAlNavegador()
        despAdelanteUm[indice].clear()
        despDerechaUm[indice].clear()
        despGrados[indice].clear()
        diagDesp("resultado=promedio_guardado poca_precision=${if (pocaPrecision) "si" else "no"} promedio_um=($promedioAdelante,$promedioDerecha)")
        resumenAjuste = "Guardado para el giro a la ${nombreLado(ladoAjuste)} (${perfilAjuste.name}): " + fmt("%.3f mm por grado adelante y %.3f a la derecha", promedioAdelante / 1000.0, promedioDerecha / 1000.0) +
            ". En un giro de 90° el centro se mueve " + fmt("%.1f cm adelante y %.1f cm de lado", promedioAdelante * 0.009, promedioDerecha * 0.009) + "." +
            (if (pocaPrecision) " OJO: calibrado con poca precisión (las dos repeticiones no coincidían); la calibración es menos confiable, repite la medición cuando puedas." else "") +
            " Puedes hacer el otro lado o tocar «Listo»."
        mostrarPaso()
    }

    // ── Ajuste fino: giro de 90° ──────────────────────────────────────────

    private fun elegirPerfilAjuste(perfil: ControlLogica.Perfil) {
        if (corriendo || esperandoDato || perfil == perfilAjuste) {
            return
        }
        perfilAjuste = perfil
        logica.pedirPerfil(perfilEsperado())
        resumenAjuste = ""
        intentosRecta = 0      // la recta se calibra por perfil: el conteo de intentos empieza otra vez
        rectaLista = false
        if (paso == Paso.DESPLAZ) {
            despAdelanteUm[0].clear(); despAdelanteUm[1].clear(); despDerechaUm[0].clear(); despDerechaUm[1].clear(); despGrados[0].clear(); despGrados[1].clear()
        }
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
        botonRestaurar.visibility = View.VISIBLE
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
        val cal = guardada.leerAjustada(perfilAjuste)
        val sb = StringBuilder()
        if (!guardada.estaCompleta()) {
            sb.append(getString(R.string.cal_aj_sin_cal)).append("\n\n")
        }
        sb.append(String.format(Locale.US, "Factor del giro (%s): izquierda %d, derecha %d (1000 = sin corrección)\n", perfilAjuste.texto,
            guardada.factorGiro(perfilAjuste, CalibracionNativa.LADO_IZQUIERDA), guardada.factorGiro(perfilAjuste, CalibracionNativa.LADO_DERECHA)))
        sb.append(String.format(Locale.US, "Un giro de 90° dura: izquierda %d ms, derecha %d ms\n",
            CalibracionNativa.gradosAMs(cal, -90), CalibracionNativa.gradosAMs(cal, 90)))
        sb.append("Última vez (${perfilAjuste.name}): ${lineaFecha(Parte.GIRO_90, guardada.fecha(Parte.GIRO_90, perfilAjuste) > 0L, perfilAjuste)}\n")
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

    /** Corrige el factor del lado probado con los grados que giró de verdad (los de la opción elegida o los que escribió el usuario). Si el factor dejaría los ms por grado fuera de 3 a 20, no se guarda. */
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
        // Antes de guardar: ¿cómo quedaría la calibración? Los ms por grado deben quedar entre 3 y 20.
        val factorIzq = if (ladoAjuste < 0) factor else guardada.factorGiro(perfilAjuste, CalibracionNativa.LADO_IZQUIERDA)
        val factorDer = if (ladoAjuste > 0) factor else guardada.factorGiro(perfilAjuste, CalibracionNativa.LADO_DERECHA)
        val (codigoPrueba, prueba) = CalibracionNativa.desempaquetar(CalibracionNativa.conFactorGiro(guardada.leer(), factorIzq, factorDer))
        val validacion = if (codigoPrueba == CalibracionNativa.OK) CalibracionNativa.validarParaGuardar(prueba)[0] else codigoPrueba
        if (validacion != CalibracionNativa.OK) {
            resumenAjuste = "No se guardó: con ese factor ($factor) los ms por grado de la ${nombreLado(ladoAjuste)} quedarían fuera de 3 a 20. Casi seguro se anotó mal el ángulo. Vuelve a probar."
            Log.w(CalibracionGuardada.ETIQUETA, "ajuste giro 90° rechazado: factor $antes -> $factor deja los ms por grado fuera de 3 a 20")
            esperandoDato = false
            campoResultado.setText("")
            mostrarPaso()
            return
        }
        guardada.guardarFactorGiro(perfilAjuste, ladoAjuste, factor)
        guardada.marcarCalibrado(Parte.GIRO_90, perfilAjuste)
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

    /** «Restaurar»: pregunta antes, diciendo EXACTAMENTE qué se va a borrar (y qué no se toca). Después de borrar queda una copia y un aviso «Deshacer» de 10 segundos. */
    private fun restaurarAjuste() {
        if (corriendo) {
            return
        }
        val que: String
        val detalle: String
        when (paso) {
            Paso.DESPLAZ -> {
                val (a, d) = guardada.desplazamiento(perfilAjuste, ladoAjuste)
                que = "el desplazamiento al girar a la ${nombreLado(ladoAjuste)} del perfil ${perfilAjuste.texto}"
                detalle = "Se borra SOLO el de la ${nombreLado(ladoAjuste)} (hoy " + fmt("%.1f cm adelante y %.1f de lado en un giro de 90°", a * 0.009, d * 0.009) + "). El otro lado NO se toca."
            }
            Paso.GIRO_90 -> {
                que = "el ajuste fino del giro de 90° del perfil ${perfilAjuste.texto}"
                detalle = "Se borran los factores de giro de LOS DOS lados (izquierda y derecha) de ese perfil. El desplazamiento y el balance NO se tocan."
            }
            Paso.RECTA -> {
                que = "el balance de la recta del perfil ${perfilAjuste.texto}"
                detalle = "El balance vuelve al del firmware. El giro y el desplazamiento NO se tocan."
            }
            Paso.VISION -> {
                que = "la máscara de la visión"
                detalle = "Se borran las ${guardada.celdasEnmascaradas()} celdas enmascaradas y la fecha de esa calibración. Nada más se toca."
            }
            else -> return
        }
        Dialogos.confirmar(this, "¿Restaurar $que?", detalle + "\n\nAntes se guarda una copia: tendrás 10 segundos para «Deshacer» (y se conservan las últimas ${CalibracionGuardada.MAX_COPIAS}).", "Restaurar") {
            guardada.guardarCopia(que)
            restaurarAjusteConfirmado()
            mostrarDeshacer(que)
        }
    }

    /** El aviso de 10 segundos con «Deshacer» después de borrar algo: vuelve todo a como estaba antes. */
    private fun mostrarDeshacer(que: String) {
        Snackbar.make(findViewById(android.R.id.content), "Se borró $que.", 10000).setAction("Deshacer") {
            if (guardada.deshacer()) {
                guardada.aplicarAlNavegador()
                Toast.makeText(this, "Deshecho: vuelve $que", Toast.LENGTH_LONG).show()
                if (paso == Paso.LISTA) armarLista() else mostrarPaso()
            }
        }.show()
    }

    private fun restaurarAjusteConfirmado() {
        when (paso) {
            Paso.VISION -> {
                guardada.restaurarVision()
                guardada.aplicarAlNavegador()
                visionInforme = ""
                iniciarCalibracionVision()
                return
            }
            else -> {}
        }
        when (paso) {
            Paso.RECTA -> {
                restaurarRecta()
                return
            }
            Paso.DESPLAZ -> {
                guardada.restaurarDesplazamientoLado(perfilAjuste, ladoAjuste)
                guardada.aplicarAlNavegador()
                despAdelanteUm[0].clear(); despAdelanteUm[1].clear(); despDerechaUm[0].clear(); despDerechaUm[1].clear(); despGrados[0].clear(); despGrados[1].clear()
                resumenAjuste = "Se restauró el desplazamiento de la ${nombreLado(ladoAjuste)} en el perfil ${perfilAjuste.texto} (el otro lado no se tocó)."
            }
            else -> {
                guardada.restaurarFactoresGiro(perfilAjuste)
                guardada.aplicarAlNavegador()
                resumenAjuste = "Se restauró el valor por defecto del giro en el perfil ${perfilAjuste.texto}."
            }
        }
        esperandoDato = false
        campoResultado.setText("")
        mostrarPaso()
    }

    // ── Ajuste fino: recta (balance de los lados) ─────────────────────────

    private fun elegirDireccionDesvio(direccion: Int) {
        if (!esperandoDato) {
            return
        }
        direccionDesvio = direccion
        mostrarPaso()
    }

    private fun cambiarDesvioCm(delta: Int) {
        if (!esperandoDato) {
            return
        }
        desvioCm = (desvioCm + delta).coerceIn(0, 40)
        mostrarPaso()
    }

    private fun nombreDireccion(d: Int) = when {
        d < 0 -> "a la izquierda"
        d > 0 -> "a la derecha"
        else -> "recto"
    }

    private fun mostrarRecta() {
        textoTitulo.setText(R.string.cal_t_recta)
        textoInstruccion.setText(R.string.cal_i_recta)
        textoNota.text = getString(R.string.cal_n_recta) + "\n" + getString(R.string.cal_aj_pilas)
        textoNota.visibility = View.VISIBLE
        panelAjuste.visibility = View.VISIBLE
        botonRestaurar.visibility = View.VISIBLE
        etiquetaLado.visibility = View.GONE
        filaLado.visibility = View.GONE
        marcarSeleccion(botonPerfilAuto, perfilAjuste == ControlLogica.Perfil.AUTO)
        marcarSeleccion(botonPerfilManual, perfilAjuste == ControlLogica.Perfil.MANUAL)
        botonProbar.visibility = View.VISIBLE
        botonProbar.text = when {
            rectaLista -> getString(R.string.cal_b_recta_lista)
            intentosRecta >= CalibracionNativa.RECTA_MAX_ITERACIONES -> getString(R.string.cal_b_recta_maximo)
            else -> getString(R.string.cal_b_probar_recta, intentosRecta + 1, CalibracionNativa.RECTA_MAX_ITERACIONES)
        }
        if (esperandoDato) {
            bloqueRecta.visibility = View.VISIBLE
            marcarSeleccion(botonDirIzq, direccionDesvio < 0)
            marcarSeleccion(botonDirRecto, direccionDesvio == 0)
            marcarSeleccion(botonDirDer, direccionDesvio > 0)
            textoCm.text = String.format(Locale.US, "%d cm", desvioCm)
            botonGuardar.visibility = View.VISIBLE
            botonGuardar.setText(R.string.cal_b_aplicar_desvio)
        } else {
            botonGuardar.setText(R.string.cal_guardar)
        }
        textoDatos.text = textoAjusteRecta()
    }

    private fun textoAjusteRecta(): String {
        val sb = StringBuilder()
        sb.append(String.format(Locale.US, "Balance del lado izquierdo (%s): %d de 1000 (por defecto %d)\n", perfilAjuste.texto, guardada.balance(perfilAjuste),
            if (perfilAjuste == ControlLogica.Perfil.AUTO) CalibracionNativa.BALANCE_DEFECTO_AUTO else CalibracionNativa.BALANCE_DEFECTO_MANUAL))
        sb.append(String.format(Locale.US, "Intentos: %d de %d. El carro lo guarda en su memoria.\n", intentosRecta, CalibracionNativa.RECTA_MAX_ITERACIONES))
        sb.append("Última vez (${perfilAjuste.name}): ${lineaFecha(Parte.RECTA, guardada.fecha(Parte.RECTA, perfilAjuste) > 0L || guardada.hayBalanceGuardado(perfilAjuste), perfilAjuste)}\n")
        if (resumenAjuste.isNotEmpty()) {
            sb.append("\n").append(resumenAjuste)
        }
        return sb.toString()
    }

    /** Lo que recorre el carro en los 4 s de la prueba, en mm: con la calibración en AUTO (más el frenado); en MANUAL, con la velocidad medida de ese perfil (26.3 ms por cm). */
    private fun recorridoDeLaPruebaMm(): Int {
        if (perfilAjuste == ControlLogica.Perfil.AUTO) {
            val cal = guardada.leer()
            val sinArranque = (TIEMPO_RECTA_MS - cal[CalibracionNativa.CAL_ARRANQUE_MS]).coerceAtLeast(0)
            return sinArranque * 100 / cal[CalibracionNativa.CAL_MS_POR_CM_X10] + cal[CalibracionNativa.CAL_FRENADO_MM]
        }
        return TIEMPO_RECTA_MS * 100 / MS_POR_CM_MANUAL_X10
    }

    /** Antes de avanzar se le manda al carro el balance que la app tiene para ese perfil (con ACK): así lo que se prueba es lo que se ve en pantalla. */
    private fun empezarRecta() {
        if (!conectado() || corriendo) {
            return
        }
        corriendo = true
        resumenAjuste = ""
        mostrarPaso()
        val balance = guardada.balance(perfilAjuste)
        logica.enviarBalance(perfilAjuste, balance, 1000L) { confirmado ->
            if (!corriendo) {
                return@enviarBalance   // se tocó PARO (o se salió de la pantalla) mientras se esperaba el ACK: no se mueve el carro
            }
            if (!confirmado) {
                corriendo = false
                resumenAjuste = "El carro no confirmó el balance. ¿Está cargado el firmware nuevo en el carro? Revisa la conexión y vuelve a tocar «Avanzar»."
                Log.w(CalibracionGuardada.ETIQUETA, "recta: el carro no confirmó BAL_${perfilAjuste.name}:$balance")
                mostrarPaso()
                return@enviarBalance
            }
            Log.i(CalibracionGuardada.ETIQUETA, "recta: FORWARD durante $TIEMPO_RECTA_MS ms (perfil ${perfilAjuste.name}, balance $balance)")
            logica.presionar("FORWARD")
            principal.postDelayed({
                logica.parar()
                corriendo = false
                esperandoDato = true
                direccionDesvio = 0
                desvioCm = 1
                mostrarPaso()
            }, TIEMPO_RECTA_MS.toLong())
        }
    }

    /** Con lo que el usuario vio (hacia dónde y cuántos cm) calcula el balance nuevo, lo guarda y se lo manda al carro. Con 1 cm o menos (o «recto») la recta ya quedó bien. */
    private fun aplicarDesvioRecta() {
        if (!esperandoDato) {
            return
        }
        val antes = guardada.balance(perfilAjuste)
        val desvioMm = direccionDesvio * desvioCm * 10
        esperandoDato = false
        if (direccionDesvio == 0 || desvioCm * 10 <= CalibracionNativa.RECTA_OK_MM) {
            rectaLista = true
            guardada.guardarBalance(perfilAjuste, antes)
            guardada.guardarBalanceCalibrado(perfilAjuste, antes)
            guardada.marcarCalibrado(Parte.RECTA, perfilAjuste)
            resumenAjuste = "Listo: se desvió ${if (direccionDesvio == 0) "nada" else "1 cm o menos"}. El balance $antes queda guardado en el carro y en el teléfono."
            Log.i(CalibracionGuardada.ETIQUETA, "recta lista: perfil=${perfilAjuste.name} balance=$antes en ${intentosRecta + 1} prueba(s)")
            mostrarPaso()
            return
        }
        val (codigo, nuevo) = CalibracionNativa.balanceNuevo(antes, desvioMm, recorridoDeLaPruebaMm(), guardada.anchoEjesMm).let { Pair(it[0], it[1]) }
        if (codigo != CalibracionNativa.OK) {
            resumenAjuste = "Esos números no tienen sentido (¿el desvío es mayor que la mitad de lo que avanzó?). Repite la prueba."
            mostrarPaso()
            return
        }
        intentosRecta++
        guardada.guardarBalance(perfilAjuste, nuevo)
        guardada.marcarCalibrado(Parte.RECTA, perfilAjuste)
        resumenAjuste = "Se desvió $desvioCm cm ${nombreDireccion(direccionDesvio)}: el balance pasó de $antes a $nuevo." +
            (if (intentosRecta >= CalibracionNativa.RECTA_MAX_ITERACIONES) " Se hicieron los ${CalibracionNativa.RECTA_MAX_ITERACIONES} intentos: queda este balance. Si sigue desviándose, revisa las ruedas o restaura el valor por defecto."
            else " Vuelve a probar con «Avanzar 4 s».")
        Log.i(CalibracionGuardada.ETIQUETA, "recta: perfil=${perfilAjuste.name} desvio=${desvioMm} mm balance $antes -> $nuevo (intento $intentosRecta)")
        logica.enviarBalance(perfilAjuste, nuevo, 1000L) { confirmado ->
            if (!confirmado) {
                resumenAjuste += " OJO: el carro no confirmó el nuevo balance; se le enviará al volver a conectar."
                mostrarPaso()
            }
        }
        mostrarPaso()
    }

    /** «Restaurar valor por defecto» de la recta: el balance del perfil vuelve al del firmware (y se le manda al carro). */
    private fun restaurarRecta() {
        guardada.restaurarBalance(perfilAjuste)
        intentosRecta = 0
        rectaLista = false
        esperandoDato = false
        val defecto = guardada.balance(perfilAjuste)
        resumenAjuste = "Se restauró el balance por defecto ($defecto) en el perfil ${perfilAjuste.texto}."
        logica.enviarBalance(perfilAjuste, defecto, 1000L) { confirmado ->
            if (!confirmado) {
                resumenAjuste += " OJO: el carro no confirmó el cambio."
                mostrarPaso()
            }
        }
        mostrarPaso()
    }

    // ── Datos ─────────────────────────────────────────────────────────────

    private fun guardarDato() {
        when (paso) {
            Paso.GIRO_VISUAL -> {
                guardarGiroVisual()
                return
            }
            Paso.RECTA -> {
                aplicarDesvioRecta()
                return
            }
            Paso.DESPLAZ -> {
                guardarRepeticionDesp()
                return
            }
            Paso.GIRO_90 -> {
                val grados = campoResultado.text.toString().trim().replace(',', '.').toDoubleOrNull()
                if (grados == null || grados < 10.0 || grados > 360.0) {
                    campoLayout.error = getString(R.string.cal_error_numero)
                    return
                }
                aplicarGradosGiro(Math.round(grados).toInt())
                return
            }
            else -> {}
        }
        val numero = campoResultado.text.toString().trim().replace(',', '.').toDoubleOrNull()
        if (numero == null || numero <= 0.0 || numero > 1000.0) {
            campoLayout.error = getString(R.string.cal_error_numero)
            return
        }
        val entero = Math.round(numero * 10).toInt()   // mm para avance, retroceso y frenado; grados x10 para giro
        when (paso) {
            Paso.FRENADO -> frenadoMm.add(entero)
            Paso.GIRO_IZQ, Paso.GIRO_DER -> {
                val datos = datosDelPaso()
                if (datos != null) {   // tres tiempos
                    val indice = (cuantas(datos) / REPETICIONES).coerceAtMost(TIEMPOS_GIRO.size - 1)
                    datos[indice].add(entero)
                } else {
                    giroFijo[indiceLado(ladoDelPaso())].add(entero)
                }
            }
            else -> {
                val datos = datosDelPaso() ?: return
                val indice = (cuantas(datos) / REPETICIONES).coerceAtMost(TIEMPOS_AVANCE.size - 1)
                datos[indice].add(entero)
            }
        }
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
            Paso.VISION -> {
                visionCapturando = false
                principal.removeCallbacksAndMessages(null)
                iniciarCalibracionVision()   // «Repetir»: se descartan los lugares capturados y se empieza otra vez
            }
            Paso.GIRO_VISUAL -> {
                if (esperandoDato) {
                    esperandoDato = false
                } else if (fovReal.isNotEmpty()) {
                    fovReal.removeAt(fovReal.size - 1)
                    fovVisual.removeAt(fovVisual.size - 1)
                }
            }
            Paso.ARRANQUE -> if (toquesArranque.isNotEmpty()) toquesArranque.removeAt(toquesArranque.size - 1)
            Paso.FRENADO -> {
                if (esperandoDato) {
                    esperandoDato = false   // se cancela la prueba que esperaba su dato: se repite
                } else if (frenadoMm.isNotEmpty()) {
                    frenadoMm.removeAt(frenadoMm.size - 1)
                }
            }
            Paso.GIRO_IZQ, Paso.GIRO_DER -> {
                val datos = datosDelPaso()
                if (esperandoDato) {
                    esperandoDato = false
                } else if (datos != null) {
                    val ultimo = datos.indexOfLast { it.isNotEmpty() }
                    if (ultimo >= 0) {
                        datos[ultimo].removeAt(datos[ultimo].size - 1)
                    }
                } else {
                    val grados = giroFijo[indiceLado(ladoDelPaso())]
                    if (grados.isNotEmpty()) {
                        grados.removeAt(grados.size - 1)
                    }
                }
            }
            Paso.DESPLAZ -> esperandoDato = false   // la repetición en curso no se guarda: se repite el giro
            Paso.AVANCE, Paso.RETROCESO -> {
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

    // ── Cálculo y resultado ───────────────────────────────────────────────

    /** Calcula lo que cambiaría en la calibración guardada con lo anotado de ESTA parte (en C) y arma el texto. Lo demás de la calibración no se toca. */
    private fun calcularResultado() {
        val sb = StringBuilder()
        val antes = guardada.leer()
        var cal = antes.copyOf()
        var ok = false
        when (parte) {
            Parte.ARRANQUE -> {
                val medio = CalibracionNativa.promedioSinAtipicos(toquesArranque.toIntArray())
                val arranque = (medio - reaccionMs()).coerceAtLeast(0)
                val (codigo, nueva) = CalibracionNativa.desempaquetar(CalibracionNativa.ponerArranque(cal, arranque))
                if (codigo == CalibracionNativa.OK) {
                    cal = nueva
                    ok = true
                    sb.append("Arranque: ${antes[CalibracionNativa.CAL_ARRANQUE_MS]} → $arranque ms (toque ${medio} ms menos reacción ${reaccionMs()} ms)\n")
                    if (Math.abs(arranque - antes[CalibracionNativa.CAL_ARRANQUE_MS]) >= ARRANQUE_CAMBIO_AVISO_MS && antes[CalibracionNativa.CAL_MEDIDOS] and CalibracionNativa.MEDIDO_AVANCE != 0) {
                        sb.append("\nOJO: el arranque cambió más de $ARRANQUE_CAMBIO_AVISO_MS ms. La velocidad y los giros que ya estaban guardados se midieron con el arranque anterior; si te importa, repítelos.\n")
                    }
                } else {
                    sb.append("Arranque: ${CalibracionNativa.textoError(codigo)}\n")
                }
            }
            Parte.VELOCIDAD -> {
                val nueva = ajustarGrupo(sb, cal, "Velocidad de avance", avance, TIEMPOS_AVANCE, 0)
                if (nueva != null) {
                    cal = nueva
                    ok = true
                    sb.append(fmt("Velocidad: %.1f → %.1f ms por cm (el frenado de %.1f cm y el arranque de %d ms se conservan)\n",
                        antes[CalibracionNativa.CAL_MS_POR_CM_X10] / 10.0, cal[CalibracionNativa.CAL_MS_POR_CM_X10] / 10.0, cal[CalibracionNativa.CAL_FRENADO_MM] / 10.0, cal[CalibracionNativa.CAL_ARRANQUE_MS]))
                    sb.append("Ejemplo: para avanzar 40 cm el carro recibirá ${CalibracionNativa.cmAMs(cal, 40)} ms.\n")
                }
            }
            Parte.FRENADO -> {
                val mm = CalibracionNativa.promedioSinAtipicos(frenadoMm.toIntArray())
                val (codigo, nueva) = CalibracionNativa.desempaquetar(CalibracionNativa.ajustarFrenado(cal, TIEMPO_FRENADO_MS, mm))
                if (codigo == CalibracionNativa.OK) {
                    cal = nueva
                    ok = true
                    sb.append(fmt("Frenado de avance: %.1f → %.1f cm (el carro avanzó %.1f cm en total con %d ms; la velocidad guardada decía %.1f cm)\n",
                        antes[CalibracionNativa.CAL_FRENADO_MM] / 10.0, cal[CalibracionNativa.CAL_FRENADO_MM] / 10.0, mm / 10.0, TIEMPO_FRENADO_MS, CalibracionNativa.msAMm(cal, TIEMPO_FRENADO_MS) / 10.0))
                } else {
                    sb.append("Frenado: ${CalibracionNativa.textoError(codigo)}")
                    if (codigo == CalibracionNativa.ERR_DATOS) {
                        sb.append(" Si cambiaste de pilas o de piso, calibra primero la velocidad de avance.")
                    }
                    sb.append("\n")
                }
            }
            Parte.GIRO_IZQ, Parte.GIRO_DER -> {
                val lado = if (parte == Parte.GIRO_IZQ) CalibracionNativa.LADO_IZQUIERDA else CalibracionNativa.LADO_DERECHA
                val nombre = "Giro a la ${nombreLado(lado)}"
                val msGradoAntes = antes[if (lado < 0) CalibracionNativa.CAL_MS_GRADO_IZQ_X10 else CalibracionNativa.CAL_MS_GRADO_DER_X10]
                val nueva: IntArray? = if (giroPrueba.tipo == GiroPrueba.Tipo.TRES_TIEMPOS) {
                    ajustarGrupo(sb, cal, nombre, giroTres[indiceLado(lado)], TIEMPOS_GIRO, lado)
                } else {
                    val grados = CalibracionNativa.promedioSinAtipicos(giroFijo[indiceLado(lado)].toIntArray())
                    val (codigo, calculada) = CalibracionNativa.desempaquetar(CalibracionNativa.ajustarGiroFijo(cal, lado, giroFijoMs[indiceLado(lado)], grados))
                    if (codigo == CalibracionNativa.OK) {
                        sb.append("$nombre: bien (${giroFijo[indiceLado(lado)].size} giros de ${giroFijoMs[indiceLado(lado)]} ms, promedio " + fmt("%.0f", grados / 10.0) + "°)\n")
                        calculada
                    } else {
                        sb.append("$nombre: ${CalibracionNativa.textoError(codigo)}\n")
                        null
                    }
                }
                if (nueva != null) {
                    cal = nueva
                    ok = true
                    val indiceMs = if (lado < 0) CalibracionNativa.CAL_MS_GRADO_IZQ_X10 else CalibracionNativa.CAL_MS_GRADO_DER_X10
                    val indiceFrenado = if (lado < 0) CalibracionNativa.CAL_FRENADO_GRADOS_IZQ_X10 else CalibracionNativa.CAL_FRENADO_GRADOS_DER_X10
                    sb.append(fmt("Giro: %.1f → %.1f ms por grado, frenado del giro %.1f°\n", msGradoAntes / 10.0, cal[indiceMs] / 10.0, cal[indiceFrenado] / 10.0))
                    sb.append("Un giro de 90° a la ${nombreLado(lado)} durará ${CalibracionNativa.gradosAMs(cal, lado * 90)} ms (antes ${CalibracionNativa.gradosAMs(antes, lado * 90)} ms).\n")
                    if (guardada.factorGiro(ControlLogica.Perfil.AUTO, lado) != CalibracionNativa.FACTOR_GIRO_DEFECTO || guardada.factorGiro(ControlLogica.Perfil.MANUAL, lado) != CalibracionNativa.FACTOR_GIRO_DEFECTO) {
                        sb.append("El ajuste fino de este lado vuelve a 1000: la medida nueva ya lo incluye.\n")
                    }
                    guardada.avisoMezclaGiro(lado, giroPrueba)?.let { sb.append("\nOJO: ").append(it).append("\n") }
                }
            }
            Parte.RETROCESO -> {
                val nueva = ajustarGrupo(sb, cal, "Retroceso", retroceso, TIEMPOS_AVANCE, LADO_ATRAS)
                if (nueva != null) {
                    cal = nueva
                    ok = true
                    sb.append(fmt("Retroceso: %.1f → %.1f ms por cm hacia atrás\n", antes[CalibracionNativa.CAL_MS_POR_CM_ATRAS_X10] / 10.0, cal[CalibracionNativa.CAL_MS_POR_CM_ATRAS_X10] / 10.0))
                }
            }
            Parte.GIRO_VISUAL -> {
                // El campo de visión con el que se midió la prueba (el de la cámara si no estaba calibrado) es el que se ajusta.
                cal[CalibracionNativa.CAL_FOV_GRADOS_X10] = camara.fovEfectivoX10
                val (codigo, nueva) = CalibracionNativa.desempaquetar(CalibracionNativa.ajustarFov(cal, fovReal.sum(), fovVisual.sum()))
                if (codigo == CalibracionNativa.OK) {
                    cal = nueva
                    ok = true
                    sb.append(String.format(Locale.US, "Giro visual: campo de visión %.1f° (real %.1f° contra %.1f° que vio la cámara en %d pruebas)\n",
                        cal[CalibracionNativa.CAL_FOV_GRADOS_X10] / 10.0, fovReal.sum() / 10.0, fovVisual.sum() / 10.0, fovReal.size))
                } else {
                    sb.append("Giro visual: ${CalibracionNativa.textoError(codigo)}\n")
                }
            }
            else -> {}
        }
        resultado = cal
        resultadoOk = ok
        avisoGuardar = ""
        textoResumen = sb.toString()
        Log.i(CalibracionGuardada.ETIQUETA, "resultado de ${parte?.name} (${if (ok) "bien" else "no se puede guardar"}): " + CalibracionNativa.aTexto(cal).replace('\n', ' '))
    }

    /** Promedia cada tiempo (sin atípicos) y ajusta la velocidad (lado 0), el retroceso (LADO_ATRAS) o el giro de un lado con tres tiempos. Devuelve la calibración nueva o null si no se pudo. */
    private fun ajustarGrupo(sb: StringBuilder, cal: IntArray, nombre: String, datos: Array<ArrayList<Int>>, tiempos: IntArray, lado: Int): IntArray? {
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
            return null
        }
        val respuesta = when (lado) {
            0 -> CalibracionNativa.ajustarVelocidad(cal, ms.toIntArray(), valores.toIntArray())
            LADO_ATRAS -> CalibracionNativa.ajustarRetroceso(cal, ms.toIntArray(), valores.toIntArray())
            else -> CalibracionNativa.ajustarGiro(cal, lado, ms.toIntArray(), valores.toIntArray())
        }
        val (codigo, nueva) = CalibracionNativa.desempaquetar(respuesta)
        if (codigo != CalibracionNativa.OK) {
            sb.append("$nombre: ${CalibracionNativa.textoError(codigo)}\n")
            return null
        }
        sb.append("$nombre: bien (${ms.size} tiempos)\n")
        return nueva
    }

    // ── Botones del resultado y de la lista ───────────────────────────────

    /** «Guardar esta calibración»: el código en C revisa antes que los ms por grado estén entre 3 y 20; si no, no guarda nada y dice por qué. */
    private fun usarValores() {
        val cal = resultado ?: return
        val p = parte ?: return
        if (!resultadoOk) {
            return
        }
        val codigo = guardada.guardar(cal)
        if (codigo != CalibracionNativa.OK) {
            avisoGuardar = if (codigo == CalibracionNativa.ERR_GIRO_FUERA) {
                val izq = cal[CalibracionNativa.CAL_MS_GRADO_IZQ_X10] / 10.0
                val der = cal[CalibracionNativa.CAL_MS_GRADO_DER_X10] / 10.0
                val guardadaAntes = guardada.leer()
                // ¿El valor que está fuera de 3 a 20 lo cambió ESTA parte o ya estaba guardado así de antes?
                val cambioIzq = cal[CalibracionNativa.CAL_MS_GRADO_IZQ_X10] != guardadaAntes[CalibracionNativa.CAL_MS_GRADO_IZQ_X10]
                val cambioDer = cal[CalibracionNativa.CAL_MS_GRADO_DER_X10] != guardadaAntes[CalibracionNativa.CAL_MS_GRADO_DER_X10]
                val deEstaParte = cambioIzq || cambioDer
                fmt("NO SE GUARDÓ NADA: los ms por grado quedarían en %.1f (izquierda) y %.1f (derecha), y deben estar entre 3 y 20. ", izq, der) +
                    (if (deEstaParte) "Casi seguro se anotó mal algún dato: toca «Atrás», borra el dato malo con «Borrar el último dato» o repite la prueba."
                    else "Esa medida de giro ya estaba guardada así de antes y no es de esta prueba: calibra primero el giro que está fuera de rango (en la lista, «Giro a la izquierda» o «Giro a la derecha»).")
            } else {
                "NO SE GUARDÓ NADA: ${CalibracionNativa.textoError(codigo)}"
            }
            Toast.makeText(this, "No se guardó: los valores no son válidos. Revisa las mediciones.", Toast.LENGTH_LONG).show()
            mostrarPaso()
            return
        }
        when (p) {
            Parte.GIRO_IZQ, Parte.GIRO_DER -> {
                val lado = if (p == Parte.GIRO_IZQ) CalibracionNativa.LADO_IZQUIERDA else CalibracionNativa.LADO_DERECHA
                guardada.restaurarFactorGiroDeLado(lado)   // el giro de este lado se acaba de medir: el factor de antes ya no aplica
                guardada.guardarGiroPruebaDeGiro(lado, giroPrueba)
            }
            else -> {}
        }
        guardada.marcarCalibrado(p)
        guardada.aplicarAlNavegador()
        Toast.makeText(this, "${nombreParte(p)}: guardado. Lo demás no cambió.", Toast.LENGTH_LONG).show()
        terminarParte()
    }

    private fun copiarTexto() {
        val texto = guardada.textoParaCopiar()
        if (texto.isEmpty()) {
            Toast.makeText(this, "Los valores no son válidos", Toast.LENGTH_SHORT).show()
            return
        }
        val portapapeles = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        portapapeles.setPrimaryClip(ClipData.newPlainText("calibracion", texto))
        Toast.makeText(this, "Texto copiado", Toast.LENGTH_SHORT).show()
    }

    private fun confirmarBorrarCalibracion() {
        Dialogos.confirmar(this, "¿Borrar la calibración?", "Se vuelve a los valores por defecto en todas las partes medidas (arranque, velocidad, frenado, giros, retroceso y campo de visión). El ajuste fino, el desplazamiento, el balance y la máscara de la visión no se tocan.\n\nAntes se guarda una copia: tendrás 10 segundos para «Deshacer».", "Borrar") {
            guardada.guardarCopia("la calibración medida (arranque, velocidad, frenado, giros, retroceso y campo de visión)")
            guardada.borrar()
            guardada.aplicarAlNavegador()
            armarLista()
            mostrarDeshacer("la calibración medida")
        }
    }

    // ── Calibrar visión (J9): la máscara de artefactos fijos de la imagen ─────────────────────────────

    private val visionLugares = Array(3) { IntArray(ObstaculosNativa.ENTEROS_LUGAR) }   // por lugar: [cuadros, marcas de las 100 celdas gruesas]
    private var visionHechos = 0
    private var visionCapturando = false
    private var visionInforme = ""
    private val MASC_CUADROS = 20      // cuadros que se toman en cada lugar
    private val MASC_ESPERA_MS = 12000L   // si en tanto tiempo no llegan los cuadros, se cancela la captura

    /** Empieza la calibración de la visión: sin lugares capturados y SIN máscara en el detector (si no, lo ya enmascarado no se vería). */
    private fun iniciarCalibracionVision() {
        for (l in visionLugares) l.fill(0)
        visionHechos = 0
        visionCapturando = false
        visionInforme = ""
        ObstaculosNativa.activarMascara(false)
    }

    /** «Capturar el lugar N de 3»: toma MASC_CUADROS cuadros del detector (el piso debe estar despejado) y los cuenta. */
    private fun empezarCapturaVision() {
        if (visionCapturando || visionHechos >= 3 || !camara.abierta) {
            return
        }
        val lugar = visionLugares[visionHechos]
        lugar.fill(0)
        visionCapturando = true
        val inicio = SystemClock.elapsedRealtime()
        val tarea = object : Runnable {
            override fun run() {
                val nuevo = camara.tomarCuadroNuevo()
                val detalle = camara.cuadroTomado?.resultado
                if (nuevo != null && detalle != null && detalle.isNotEmpty() && detalle[0] == 0) {
                    ObstaculosNativa.mascaraLugarCuadro(lugar, detalle)
                }
                if (lugar[0] >= MASC_CUADROS) {
                    visionCapturando = false
                    visionHechos++
                    Log.i(CalibracionGuardada.ETIQUETA, "visión: lugar $visionHechos capturado con ${lugar[0]} cuadros")
                    if (visionHechos >= 3) {
                        terminarCalibracionVision()
                    }
                    mostrarPaso()
                } else if (SystemClock.elapsedRealtime() - inicio > MASC_ESPERA_MS) {
                    visionCapturando = false
                    lugar.fill(0)
                    visionInforme = "No llegaron cuadros de la cámara a tiempo (${lugar[0]} de $MASC_CUADROS). Revisa la cámara y toca «Capturar» otra vez."
                    mostrarPaso()
                } else {
                    mostrarPaso()
                    principal.postDelayed(this, 80)
                }
            }
        }
        mostrarPaso()
        principal.postDelayed(tarea, 80)
    }

    /** Con los tres lugares capturados: calcula la máscara, la guarda con fecha y muestra el informe (números, sin imagen). Si sale sospechosa NO se activa. */
    private fun terminarCalibracionVision() {
        val texto = ObstaculosNativa.mascaraCalcular(visionLugares[0], visionLugares[1], visionLugares[2])
        val primera = texto.substringBefore('\n')
        val informe = texto.substringAfter('\n', "")
        val partes = primera.split(' ')
        if (partes.size >= 4 && partes[0] == "OK") {
            val celdas = partes[2].toIntOrNull() ?: 0
            val sospechosa = partes[3].toIntOrNull() ?: 0
            if (sospechosa != 0) {
                guardada.guardarVision("", informe, 0, sospechosa)
                guardada.aplicarAlNavegador()   // sospechosa: NO se activa la máscara (se guarda solo el informe)
                ObstaculosNativa.reiniciarModelo()
                visionInforme = "VISIÓN SOSPECHOSA: no se activó la máscara. Revisa el informe y toca «Repetir».\n" + informe
                Toast.makeText(this, "Visión sospechosa: no se activó la máscara, repite la calibración", Toast.LENGTH_LONG).show()
            } else {
                guardada.guardarVision(partes[1], informe, celdas, 0)
                guardada.aplicarAlNavegador()   // la máscara nueva ya vale (y el modelo del piso se reaprende con ella)
                ObstaculosNativa.reiniciarModelo()
                visionInforme = "Guardado: $celdas celdas enmascaradas.\n" + informe
                Toast.makeText(this, "Visión calibrada: $celdas celdas ignoradas", Toast.LENGTH_LONG).show()
            }
        } else {
            visionInforme = "No se pudo calcular la máscara (" + primera + "): hacen falta al menos 10 cuadros en cada lugar. Toca «Repetir»."
        }
    }
}
