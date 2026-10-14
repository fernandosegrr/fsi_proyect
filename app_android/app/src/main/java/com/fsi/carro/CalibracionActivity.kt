/*
 * CalibracionActivity.kt — Asistente paso a paso para MEDIR el carro y que el navegador convierta
 * distancias y giros en tiempos reales (el carro no tiene encoders ni IMU).
 *
 * GUÍA DE LECTURA
 *  1. Pasos (enum Paso): INTRO → REACCION → ARRANQUE → AVANCE → GIRO_IZQ → GIRO_DER → GIRO_VISUAL → RESUMEN.
 *       - REACCION: mide tu tiempo de reacción (toca cuando el botón se pone verde).
 *       - ARRANQUE: manda FORWARD y tú tocas "YA SE MOVIÓ" cuando ves moverse las ruedas; el
 *         arranque es ese tiempo menos tu reacción. 5 repeticiones, se descartan los datos raros.
 *       - AVANCE: manda FORWARD durante 1000, 2000 y 3000 ms (3 veces cada uno) y tú escribes cuántos
 *         cm avanzó (el carro frena solo después del STOP: mídelo hasta que se detenga del todo).
 *       - GIRO_IZQ / GIRO_DER: igual con LEFT y RIGHT (500, 1000 y 1500 ms), anotando los grados
 *         aproximados (con marcas de 90 grados en el piso). Cada lado por separado.
 *       - GIRO_VISUAL (opcional): gira 90 grados a la derecha con la calibración ya medida mientras la cámara (la que alimenta al
 *         detector) mide cuánto vio girar; tú escribes cuántos grados giró de verdad. Con eso se ajusta el campo de visión (FOV)
 *         de la cámara, que convierte píxeles en grados (docs/giro_visual.md).
 *       - RESUMEN: calcula con el código en C (CalibracionNativa) y muestra los valores; "Usar estos
 *         valores" los guarda (CalibracionGuardada) y el navegador los usa en la próxima ruta.
 *  2. Toda la cuenta (promedios, atípicos, rectas, frenado) está en C y probada en PC
 *     (nucleo_c/calibracion.c). Aquí solo hay pantalla y temporizadores.
 *  3. Seguridad: el botón STOP siempre está arriba; al salir de la pantalla se manda STOP y se
 *     cierra la conexión. Hay que tener ~2 m libres delante del carro.
 *  4. Los mensajes importantes también van a logcat con la etiqueta CALIBRACION.
 */
package com.fsi.carro

import android.Manifest
import android.app.Activity
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import java.util.Locale
import kotlin.random.Random

class CalibracionActivity : Activity() {

    companion object {
        val TIEMPOS_AVANCE = intArrayOf(1000, 2000, 3000)   // ms de FORWARD en cada prueba de avance
        val TIEMPOS_GIRO = intArrayOf(500, 1000, 1500)      // ms de LEFT / RIGHT en cada prueba de giro
        const val REPETICIONES = 3                          // por cada tiempo
        const val TOQUES_REACCION = 3
        const val TOQUES_ARRANQUE = 5
        const val REACCION_POR_DEFECTO_MS = 180             // si no se midió la reacción
        const val ESPERA_MAXIMA_ARRANQUE_MS = 3000L         // si no tocas en este tiempo, se detiene el carro
    }

    private enum class Paso { INTRO, REACCION, ARRANQUE, AVANCE, GIRO_IZQ, GIRO_DER, GIRO_VISUAL, RESUMEN }

    private val principal = Handler(Looper.getMainLooper())
    private lateinit var logica: ControlLogica
    private lateinit var guardada: CalibracionGuardada

    private lateinit var textoConexion: TextView
    private lateinit var campoDireccion: EditText
    private lateinit var textoTitulo: TextView
    private lateinit var textoInstruccion: TextView
    private lateinit var textoDatos: TextView
    private lateinit var botonProbar: Button
    private lateinit var botonYa: Button
    private lateinit var campoResultado: EditText
    private lateinit var botonGuardar: Button
    private lateinit var botonRepetir: Button
    private lateinit var botonAtras: Button
    private lateinit var botonSiguiente: Button
    private lateinit var botonUsar: Button
    private lateinit var botonCopiar: Button
    private lateinit var botonBorrarCal: Button

    private var paso = Paso.INTRO

    // Datos anotados.
    private val reacciones = ArrayList<Int>()
    private val toquesArranque = ArrayList<Int>()
    private val avance = Array(TIEMPOS_AVANCE.size) { ArrayList<Int>() }      // mm por prueba
    private val giroIzq = Array(TIEMPOS_GIRO.size) { ArrayList<Int>() }       // grados x10
    private val giroDer = Array(TIEMPOS_GIRO.size) { ArrayList<Int>() }
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

    private var resultado: IntArray? = null  // calibración calculada en el RESUMEN
    private var textoResumen = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_calibracion)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        guardada = CalibracionGuardada(this)

        textoConexion = findViewById(R.id.texto_conexion)
        campoDireccion = findViewById(R.id.campo_direccion)
        textoTitulo = findViewById(R.id.texto_titulo_paso)
        textoInstruccion = findViewById(R.id.texto_instruccion)
        textoDatos = findViewById(R.id.texto_datos)
        botonProbar = findViewById(R.id.boton_probar)
        botonYa = findViewById(R.id.boton_ya)
        campoResultado = findViewById(R.id.campo_resultado)
        botonGuardar = findViewById(R.id.boton_guardar)
        botonRepetir = findViewById(R.id.boton_repetir)
        botonAtras = findViewById(R.id.boton_atras)
        botonSiguiente = findViewById(R.id.boton_siguiente)
        botonUsar = findViewById(R.id.boton_usar)
        botonCopiar = findViewById(R.id.boton_copiar)
        botonBorrarCal = findViewById(R.id.boton_borrar_cal)
        intent.getStringExtra("direccion")?.let { campoDireccion.setText(it) }

        logica = ControlLogica(EnlaceOkHttp()) { mostrarConexion() }
        val configVision = ConfigVision(this)
        camara = CamaraObstaculos(this, { configVision.comoArreglo() }, { intArrayOf(configVision.anchoCuadro, configVision.altoCuadro) })

        findViewById<Button>(R.id.boton_conectar).setOnClickListener { logica.iniciar(campoDireccion.text.toString().trim()) }
        findViewById<Button>(R.id.boton_stop).setOnClickListener { pararTodo("STOP") }
        botonProbar.setOnClickListener { alProbar() }
        botonYa.setOnClickListener { alTocar() }
        botonGuardar.setOnClickListener { guardarDato() }
        botonRepetir.setOnClickListener { borrarUltimoDato() }
        botonAtras.setOnClickListener { cambiarPaso(-1) }
        botonSiguiente.setOnClickListener { cambiarPaso(1) }
        botonUsar.setOnClickListener { usarValores() }
        botonCopiar.setOnClickListener { copiarTexto() }
        botonBorrarCal.setOnClickListener { borrarCalibracion() }
        mostrarPaso()
    }

    override fun onResume() {
        super.onResume()
        logica.iniciar(campoDireccion.text.toString().trim())
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
        pararTodo("pantalla en segundo plano")
        logica.detener()
        super.onPause()
    }

    // ── Conexión y seguridad ──────────────────────────────────────────────

    private fun conectado() = logica.conexion == ControlLogica.Conexion.CONECTADO

    private fun mostrarConexion() {
        textoConexion.text = when (logica.conexion) {
            ControlLogica.Conexion.CONECTADO -> "Conectado al carro" + (logica.latenciaMs?.let { " · latencia $it ms" } ?: "")
            ControlLogica.Conexion.RECONECTANDO -> "Reconectando con el carro..."
            else -> "Sin conexión con el carro (las pruebas del carro no funcionan)"
        }
        actualizarBotones()
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
        campoResultado.setText("")
        mostrarPaso()
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
        Paso.GIRO_VISUAL -> true   // es opcional
        Paso.RESUMEN -> true
    }

    /** La lista de listas del paso actual (avance o giro) o null. */
    private fun datosDelPaso(): Array<ArrayList<Int>>? = when (paso) {
        Paso.AVANCE -> avance
        Paso.GIRO_IZQ -> giroIzq
        Paso.GIRO_DER -> giroDer
        else -> null
    }

    /** Tiempo (ms) y comando de la prueba que toca ahora en un paso de avance o giro. */
    private fun tiempoActual(): Int {
        val datos = datosDelPaso() ?: return 0
        val indice = (cuantas(datos) / REPETICIONES).coerceAtMost(TIEMPOS_AVANCE.size - 1)
        return if (paso == Paso.AVANCE) TIEMPOS_AVANCE[indice] else TIEMPOS_GIRO[indice]
    }

    private fun comandoDelPaso() = when (paso) {
        Paso.AVANCE -> "FORWARD"
        Paso.GIRO_IZQ -> "LEFT"
        else -> "RIGHT"
    }

    private fun mostrarPaso() {
        val datos = datosDelPaso()
        botonProbar.visibility = View.GONE
        botonYa.visibility = View.GONE
        campoResultado.visibility = View.GONE
        botonGuardar.visibility = View.GONE
        botonRepetir.visibility = View.GONE
        botonUsar.visibility = View.GONE
        botonCopiar.visibility = View.GONE
        botonBorrarCal.visibility = View.GONE

        when (paso) {
            Paso.INTRO -> {
                textoTitulo.text = "Calibración del carro"
                textoInstruccion.text = "Vamos a medir cuánto tarda el carro en arrancar, cuánto avanza y cuánto gira. " +
                    "Hace falta:\n• el carro en el piso donde va a andar, con la batería cargada;\n• unos 2 metros libres adelante y a los lados;" +
                    "\n• una cinta o regla para medir lo que avanza;\n• marcas de 90 grados en el piso (con cinta) para los giros.\n\n" +
                    "El botón STOP de arriba siempre detiene el carro. Toca «Siguiente» para empezar."
                textoDatos.text = guardada.avisoSiFalta() ?: "Ya hay una calibración completa guardada."
            }
            Paso.REACCION -> {
                textoTitulo.text = "1. Tu tiempo de reacción"
                textoInstruccion.text = "Toca «Empezar». Cuando el botón grande se ponga VERDE, tócalo lo más rápido que puedas. " +
                    "Se repite $TOQUES_REACCION veces. Sirve para descontarlo después de la prueba de arranque."
                botonProbar.visibility = View.VISIBLE
                botonProbar.text = "Empezar"
                botonYa.visibility = View.VISIBLE
                botonYa.text = if (estadoReaccion == 2) "¡TOCA YA!" else if (estadoReaccion == 1) "Espera el verde..." else "(toca «Empezar»)"
                botonYa.setBackgroundColor(if (estadoReaccion == 2) 0xFF2E9E3E.toInt() else 0xFFBBBBBB.toInt())
                textoDatos.text = "Reacciones (ms): " + reacciones.joinToString(", ") +
                    "\nPromedio: " + (if (reacciones.isEmpty()) "-" else "${CalibracionNativa.promedioSinAtipicos(reacciones.toIntArray())} ms")
            }
            Paso.ARRANQUE -> {
                textoTitulo.text = "2. Retardo de arranque"
                textoInstruccion.text = "Toca «Probar»: el carro recibe FORWARD. Toca «YA SE MOVIÓ» en el instante en que veas que " +
                    "las ruedas empiezan a girar. El carro se detiene solo al tocar (o a los 3 s). " +
                    "Se repite $TOQUES_ARRANQUE veces."
                botonProbar.visibility = View.VISIBLE
                botonProbar.text = "Probar (el carro avanza)"
                botonYa.visibility = View.VISIBLE
                botonYa.text = "YA SE MOVIÓ"
                botonYa.setBackgroundColor(if (arrancando) 0xFF2E9E3E.toInt() else 0xFFBBBBBB.toInt())
                botonRepetir.visibility = View.VISIBLE
                botonRepetir.text = getString(R.string.cal_repetir)
                textoDatos.text = "Tiempos (ms, con tu reacción dentro): " + toquesArranque.joinToString(", ") +
                    "\nFaltan: ${(TOQUES_ARRANQUE - toquesArranque.size).coerceAtLeast(0)}"
            }
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER -> {
                val hechas = cuantas(datos!!)
                val unidad = if (paso == Paso.AVANCE) "cm" else "grados"
                textoTitulo.text = when (paso) {
                    Paso.AVANCE -> "3. Avance en línea recta"
                    Paso.GIRO_IZQ -> "4. Giro a la izquierda"
                    else -> "5. Giro a la derecha"
                }
                textoInstruccion.text = if (paso == Paso.AVANCE)
                    "Pon el carro en la marca de salida, mirando hacia adelante. Toca «Probar»: avanza un tiempo fijo y se detiene. " +
                        "Espera a que pare del todo y escribe CUÁNTOS CM avanzó (de la marca de salida a las ruedas). " +
                        "Cada vez, vuelve a poner el carro en la marca."
                else
                    "Pon el carro sobre la marca y alineado con una marca de 90 grados del piso. Toca «Probar»: gira un tiempo fijo " +
                        "y se detiene. Espera a que pare y escribe los GRADOS que giró, aproximados " +
                        "(por ejemplo 45, 90, 135...). Cada vez, vuelve a alinear el carro."
                botonProbar.visibility = View.VISIBLE
                botonProbar.text = if (hechas >= totalPruebas()) "Ya están todas las pruebas" else
                    "Probar (${tiempoActual()} ms, prueba ${hechas + 1} de ${totalPruebas()})"
                if (esperandoDato) {
                    campoResultado.visibility = View.VISIBLE
                    campoResultado.hint = "¿Cuántos $unidad?"
                    botonGuardar.visibility = View.VISIBLE
                }
                botonRepetir.visibility = View.VISIBLE
                botonRepetir.text = getString(R.string.cal_repetir)
                textoDatos.text = armarTabla(datos)
            }
            Paso.GIRO_VISUAL -> {
                textoTitulo.text = "6. Giro con la cámara (opcional)"
                textoInstruccion.text = "La cámara también puede medir cuánto gira el carro. Para que convierta bien píxeles en grados hay que ajustar su campo de visión: " +
                    "con el carro alineado a una marca de 90 grados, mirando una pared o muebles con detalles (no una pared lisa), toca «Probar»: gira a la " +
                    "DERECHA lo que la calibración dice que son 90 grados. Después escribe cuántos grados giró de verdad. Se repite hasta 3 veces. " +
                    "Si no quieres este paso, toca «Ver resumen»."
                botonProbar.visibility = View.VISIBLE
                botonProbar.text = if (fovReal.size >= 3) "Ya están las 3 pruebas" else "Probar: girar 90° a la derecha (prueba ${fovReal.size + 1} de 3)"
                if (esperandoDato) {
                    campoResultado.visibility = View.VISIBLE
                    campoResultado.hint = "¿Cuántos grados giró de verdad?"
                    botonGuardar.visibility = View.VISIBLE
                }
                botonRepetir.visibility = View.VISIBLE
                botonRepetir.text = getString(R.string.cal_repetir)
                val sb = StringBuilder()
                sb.append("Cámara: " + (if (camara.abierta) "ABIERTA" else if (camara.error.isNotEmpty()) camara.error else "abriendo...") + "\n")
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
                textoTitulo.text = "Resumen"
                textoInstruccion.text = "Estos son los valores calculados con tus mediciones."
                textoDatos.text = textoResumen
                botonUsar.visibility = View.VISIBLE
                botonCopiar.visibility = View.VISIBLE
                botonBorrarCal.visibility = View.VISIBLE
            }
        }
        actualizarBotones()
    }

    /** Tabla de lo anotado: una línea por tiempo con sus repeticiones. */
    private fun armarTabla(datos: Array<ArrayList<Int>>): String {
        val tiempos = if (paso == Paso.AVANCE) TIEMPOS_AVANCE else TIEMPOS_GIRO
        val sb = StringBuilder()
        for (i in tiempos.indices) {
            val valores = datos[i].map {
                if (paso == Paso.AVANCE) String.format(Locale.US, "%.1f cm", it / 10.0) else String.format(Locale.US, "%.0f°", it / 10.0)
            }
            sb.append(String.format(Locale.US, "%4d ms: %s\n", tiempos[i], if (valores.isEmpty()) "-" else valores.joinToString("  ")))
        }
        return sb.toString()
    }

    private fun actualizarBotones() {
        val ocupado = corriendo || arrancando || estadoReaccion != 0
        botonProbar.isEnabled = when (paso) {
            Paso.REACCION -> estadoReaccion == 0 && reacciones.size < TOQUES_REACCION
            Paso.ARRANQUE -> conectado() && !arrancando && toquesArranque.size < TOQUES_ARRANQUE
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER ->
                conectado() && !corriendo && !esperandoDato && cuantas(datosDelPaso()!!) < totalPruebas()
            Paso.GIRO_VISUAL -> conectado() && camara.abierta && !corriendo && !esperandoDato && fovReal.size < 3
            else -> false
        }
        botonYa.isEnabled = (paso == Paso.REACCION && estadoReaccion != 0) || (paso == Paso.ARRANQUE && arrancando)
        botonRepetir.isEnabled = !ocupado && when (paso) {
            Paso.ARRANQUE -> toquesArranque.isNotEmpty()
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER -> cuantas(datosDelPaso()!!) > 0
            Paso.GIRO_VISUAL -> fovReal.isNotEmpty()
            else -> false
        }
        botonAtras.isEnabled = !ocupado && paso != Paso.INTRO
        botonSiguiente.isEnabled = !ocupado && paso != Paso.RESUMEN && pasoCompleto() && !esperandoDato
        botonSiguiente.text = if (paso == Paso.GIRO_VISUAL) "Ver resumen" else getString(R.string.cal_siguiente)
    }

    // ── Pruebas ───────────────────────────────────────────────────────────

    private fun alProbar() {
        when (paso) {
            Paso.REACCION -> empezarReaccion()
            Paso.ARRANQUE -> empezarArranque()
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER -> empezarPruebaDeTiempo()
            Paso.GIRO_VISUAL -> empezarGiroVisual()
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

    // ── Datos ─────────────────────────────────────────────────────────────

    private fun guardarDato() {
        if (paso == Paso.GIRO_VISUAL) {
            guardarGiroVisual()
            return
        }
        val datos = datosDelPaso() ?: return
        val numero = campoResultado.text.toString().trim().replace(',', '.').toDoubleOrNull()
        if (numero == null || numero <= 0.0 || numero > 1000.0) {
            Toast.makeText(this, "Escribe un número mayor que cero", Toast.LENGTH_SHORT).show()
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
            Toast.makeText(this, "Escribe un número mayor que cero", Toast.LENGTH_SHORT).show()
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
            Paso.AVANCE, Paso.GIRO_IZQ, Paso.GIRO_DER -> {
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
        var cal = CalibracionNativa.desempaquetar(CalibracionNativa.defecto()).second

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
        val respuesta = if (lado == 0) CalibracionNativa.ajustarAvance(cal, ms.toIntArray(), valores.toIntArray())
                        else CalibracionNativa.ajustarGiro(cal, lado, ms.toIntArray(), valores.toIntArray())
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
