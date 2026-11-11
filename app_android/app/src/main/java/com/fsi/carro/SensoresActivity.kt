/*
 * SensoresActivity.kt — Pantalla SENSORES: el giroscopio (MPU6050) y el sensor láser (VL53L0X) del carro. Se abre desde la lista de Calibración.
 *
 * GUÍA DE LECTURA
 *  1. INTERRUPTORES: «Usar giroscopio» (los giros de las rutas se piden al carro por ángulo medido) y «Usar sensor de distancia» (el freno de emergencia del láser y la confirmación de la
 *     cámara). Se guardan en Ajustes y se copian a ControlLogica (usarGiroscopio / usarSensorDistancia). Apagados, el sistema se comporta como antes: sirve para comparar. Solo valen si el
 *     carro avisó (CAPS) que el sensor funciona.
 *  2. ESTADO en vivo (cada 250 ms y cuando llega un SENS): giroscopio ok/mal, ángulo y sesgo; láser ok/mal y distancia en mm. «Recalibrar giroscopio» manda IMU_CAL (el carro debe estar
 *     quieto) y «Ángulo en cero» manda IMU_CERO.
 *  3. PRUEBA DE GIRO de 90° a la izquierda o a la derecha con dos métodos: «por ángulo» (GIRAR:±90, el carro frena cuando el giroscopio mide 90°) o «por tiempo» (LEFT o RIGHT durante los ms
 *     de la calibración). Se compara lo que midió el giroscopio y cuánto tardó: queda en pantalla y en el log (SENS_DIAG prueba_giro ...). Si el giroscopio no está disponible el método por
 *     tiempo solo informa los ms.
 *  4. La conexión es la compartida de toda la app (SesionCarro); esta pantalla entra con el número de usuario de Calibración. Al salir se manda STOP. El botón PARO está siempre abajo.
 *  5. J16, ESCALA DEL GIROSCOPIO: con el carro alineado con una línea del piso se guarda el ángulo (quieto: dos SENS a 400 ms que casi no cambian); el usuario lo gira vueltas
 *     completas (a mano o con los botones de 90°) y lo vuelve a alinear; con lo que midió el giroscopio, CalGiroNativa.escala (nucleo_c/calgiro.c) da la corrección nueva y
 *     «Guardar en el carro» la manda (IMU_ESCALA:<ppm>, el firmware la guarda en su memoria). Solo con un firmware que anuncia IMUS en CAPS.
 */
package com.fsi.carro

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.widget.RadioGroup
import android.widget.TextView
import com.google.android.material.button.MaterialButton
import com.google.android.material.materialswitch.MaterialSwitch

class SensoresActivity : androidx.appcompat.app.AppCompatActivity() {

    companion object {
        const val ETIQUETA = "SENS_DIAG"
        private const val ESPERA_ASENTAR_MS = 900L   // después de un giro por tiempo, lo que se espera a que el carro termine de moverse antes de leer el ángulo
        private const val ESPERA_GIRO_MS = 6000L     // si el firmware no contesta a un giro por ángulo en este tiempo, se avisa
        private const val ESCALA_QUIETO_MS = 400L    // J16: dos lecturas del ángulo separadas este tiempo...
        private const val ESCALA_QUIETO_X10 = 3      // ...que no difieren más de 0.3° = el carro está quieto
        private const val ESCALA_MAX_MS = 120_000L   // J16: una medición más larga deja pesar la deriva del giroscopio: se repite
    }

    private lateinit var logica: ControlLogica
    private lateinit var ajustes: Ajustes
    private lateinit var guardada: CalibracionGuardada
    private val reloj = Handler(Looper.getMainLooper())

    private lateinit var interruptorGiro: MaterialSwitch
    private lateinit var interruptorLaser: MaterialSwitch
    private lateinit var interruptorAutoridad: MaterialSwitch
    private lateinit var interruptorRumbo: MaterialSwitch
    private lateinit var chip: ChipEstado
    private lateinit var textoGiro: TextView
    private lateinit var textoLaser: TextView
    private lateinit var textoAviso: TextView
    private lateinit var textoResultado: TextView
    private lateinit var grupoMetodo: RadioGroup
    private lateinit var botonIzq: MaterialButton
    private lateinit var botonDer: MaterialButton

    private var probando = false          // una prueba de giro en curso
    private var angulo0X10 = 0            // ángulo del giroscopio al empezar una prueba por tiempo
    private var inicioMs = 0L

    // J16: medición de la escala del giroscopio. Paso 0 = sin medir, 1 = ángulo inicial guardado (el usuario gira el carro), 2 = corrección calculada, falta guardarla.
    private lateinit var textoEscala: TextView
    private lateinit var botonEscala: MaterialButton
    private lateinit var botonEscalaCancelar: MaterialButton
    private lateinit var botonEscalaQuitar: MaterialButton
    private lateinit var botonRecalibrar: MaterialButton
    private lateinit var botonCero: MaterialButton
    private var escalaPaso = 0
    private var escalaInicioX10 = 0
    private var escalaInicioMs = 0L
    private var escalaNuevaPpm = 0
    private var escalaEsperando = false   // esperando (ESCALA_QUIETO_MS) la segunda lectura del ángulo
    private var escalaEnviando = false    // esperando que el carro confirme IMU_ESCALA
    private var escalaMensaje = ""

    private val alCambiar: () -> Unit = { mostrar() }

    /** El aviso «el carro no contestó»: se cancela en cuanto llega la respuesta, al empezar otra prueba y al parar (si no, el aviso de una prueba vieja marcaba como fallida la siguiente). */
    private val tareaSinRespuesta = Runnable {
        if (probando) {
            probando = false
            textoResultado.text = "El carro no contestó al giro en ${ESPERA_GIRO_MS / 1000} s (¿sensor caído?)."
            Log.w(ETIQUETA, "prueba_giro sin_respuesta")
            mostrar()
        }
    }

    private val tareaReloj = object : Runnable {
        override fun run() {
            mostrar()
            reloj.postDelayed(this, 250L)
        }
    }

    override fun onCreate(guardado: Bundle?) {
        super.onCreate(guardado)
        setContentView(R.layout.activity_sensores)
        intent.getStringExtra("direccion")?.let { Ajustes(this).direccionCarro = it }   // para pruebas con adb (el servidor falso), igual que en Calibración
        logica = SesionCarro.lograr(this)
        ajustes = Ajustes(this)
        guardada = CalibracionGuardada(this)

        findViewById<TextView>(R.id.barra_titulo).setText(R.string.sens_titulo_corto)
        findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { finish() }
        interruptorGiro = findViewById(R.id.sens_switch_giro)
        interruptorLaser = findViewById(R.id.sens_switch_laser)
        chip = findViewById(R.id.sens_chip)
        textoGiro = findViewById(R.id.sens_texto_giro)
        textoLaser = findViewById(R.id.sens_texto_laser)
        textoAviso = findViewById(R.id.sens_texto_aviso)
        textoResultado = findViewById(R.id.sens_resultado)
        grupoMetodo = findViewById(R.id.sens_grupo_metodo)
        botonIzq = findViewById(R.id.sens_girar_izq)
        botonDer = findViewById(R.id.sens_girar_der)

        interruptorGiro.isChecked = ajustes.usarGiroscopio
        interruptorLaser.isChecked = ajustes.usarSensorDistancia
        interruptorAutoridad = findViewById(R.id.sens_switch_autoridad)
        interruptorAutoridad.isChecked = ajustes.autoridadLaser
        interruptorAutoridad.setOnCheckedChangeListener { _, encendido ->
            ajustes.autoridadLaser = encendido
            logica.autoridadLaser = encendido
            Log.i(ETIQUETA, "interruptor autoridad_laser=$encendido")
        }
        findViewById<View>(R.id.sens_fila_autoridad).setOnClickListener { interruptorAutoridad.toggle() }
        interruptorRumbo = findViewById(R.id.sens_switch_rumbo)
        interruptorRumbo.isChecked = ajustes.usarRumbo
        interruptorRumbo.setOnCheckedChangeListener { _, encendido ->
            ajustes.usarRumbo = encendido
            logica.usarRumbo = encendido
            Log.i(ETIQUETA, "interruptor usar_rumbo_fw=$encendido")
        }
        findViewById<View>(R.id.sens_fila_rumbo).setOnClickListener { interruptorRumbo.toggle() }
        grupoMetodo.check(R.id.sens_metodo_angulo)
        interruptorGiro.setOnCheckedChangeListener { _, encendido ->
            ajustes.usarGiroscopio = encendido
            logica.usarGiroscopio = encendido
            Log.i(ETIQUETA, "interruptor usar_giroscopio=$encendido")
            mostrar()
        }
        interruptorLaser.setOnCheckedChangeListener { _, encendido ->
            ajustes.usarSensorDistancia = encendido
            logica.usarSensorDistancia = encendido
            Log.i(ETIQUETA, "interruptor usar_sensor_distancia=$encendido")
            logica.pedirCapacidades()   // le avisa al carro (TOF_USO) para que su freno de emergencia se apague o se encienda
            mostrar()
        }
        findViewById<View>(R.id.sens_fila_giro).setOnClickListener { interruptorGiro.toggle() }
        findViewById<View>(R.id.sens_fila_laser).setOnClickListener { interruptorLaser.toggle() }
        botonRecalibrar = findViewById(R.id.sens_recalibrar)
        botonCero = findViewById(R.id.sens_cero)
        botonRecalibrar.setOnClickListener { logica.recalibrarGiroscopio() }
        botonCero.setOnClickListener { logica.ponerAnguloCero() }
        textoEscala = findViewById(R.id.sens_escala_texto)
        botonEscala = findViewById(R.id.sens_escala_paso)
        botonEscalaCancelar = findViewById(R.id.sens_escala_cancelar)
        botonEscalaQuitar = findViewById(R.id.sens_escala_quitar)
        botonEscala.setOnClickListener { pasoEscala() }
        botonEscalaCancelar.setOnClickListener {
            escalaPaso = 0
            escalaMensaje = "Medición cancelada."
            Log.i(ETIQUETA, "escala_giroscopio cancelada")
            mostrar()
        }
        botonEscalaQuitar.setOnClickListener { guardarEscala(0) }
        botonIzq.setOnClickListener { probarGiro(90) }
        botonDer.setOnClickListener { probarGiro(-90) }
        findViewById<MaterialButton>(R.id.sens_paro).setOnClickListener {
            Haptica.corta(this)
            parar("PARO")
        }
        PantallasLog.abierta("SENSORES")
    }

    override fun onResume() {
        super.onResume()
        logica.usarGiroscopio = ajustes.usarGiroscopio
        logica.usarSensorDistancia = ajustes.usarSensorDistancia
        logica.autoridadLaser = ajustes.autoridadLaser
        logica.usarRumbo = ajustes.usarRumbo
        SesionCarro.escuchar(alCambiar)
        SesionCarro.entrar(this, SesionNativa.USUARIO_CALIBRACION)
        logica.pedirPerfil(ControlLogica.Perfil.AUTO)   // SENS solo llega con el perfil AUTO
        logica.alGiro = { codigo, objetivo, real, ms, ant -> alRespuestaGiro(codigo, objetivo, real, ms, ant) }
        reloj.removeCallbacks(tareaReloj)
        reloj.post(tareaReloj)
        mostrar()
    }

    override fun onPause() {
        escalaPaso = 0           // J16: al volver no se sabe qué pasó con el carro: la medición de la escala empieza de nuevo
        escalaEsperando = false
        parar("pantalla en segundo plano")
        logica.alGiro = null
        reloj.removeCallbacks(tareaReloj)
        SesionCarro.dejarDeEscuchar(alCambiar)
        SesionCarro.salir(SesionNativa.USUARIO_CALIBRACION)
        super.onPause()
    }

    private fun parar(motivo: String) {
        reloj.removeCallbacksAndMessages(null)
        reloj.removeCallbacks(tareaSinRespuesta)
        reloj.post(tareaReloj)
        if (probando) {
            Log.i(ETIQUETA, "prueba_giro cancelada: $motivo")
        }
        probando = false
        escalaEsperando = false   // J16: la segunda lectura del ángulo que estuviera pendiente se canceló arriba (hay que volver a tocar el botón)
        logica.parar()
        mostrar()
    }

    // ── J16: escala del giroscopio ────────────────────────────────────────

    /** El último SENS es reciente (menos de 500 ms), con el giroscopio listo y sin calibrar. */
    private fun anguloFresco(): Boolean {
        val b = logica.banderasSens
        return logica.ultimoSensMs != 0L && android.os.SystemClock.elapsedRealtime() - logica.ultimoSensMs < 500L && b and 1 != 0 && b and 8 == 0
    }

    /** Lee el ángulo con el carro QUIETO: dos lecturas separadas ESCALA_QUIETO_MS que no difieren más de ESCALA_QUIETO_X10. Llama a `listo` con el ángulo (décimas) o con null. */
    private fun anguloQuieto(listo: (Int?) -> Unit) {
        if (!anguloFresco()) {
            listo(null)
            return
        }
        val primero = logica.anguloX10
        escalaEsperando = true
        reloj.postDelayed({
            escalaEsperando = false
            val segundo = logica.anguloX10
            listo(if (anguloFresco() && kotlin.math.abs(segundo - primero) <= ESCALA_QUIETO_X10) segundo else null)
        }, ESCALA_QUIETO_MS)
        mostrar()
    }

    /** El botón principal de la tarjeta: Empezar (guarda el ángulo inicial), Terminé (calcula la corrección) o Guardar en el carro. */
    private fun pasoEscala() {
        when (escalaPaso) {
            0 -> anguloQuieto { angulo ->
                if (angulo == null) {
                    escalaMensaje = "No hay un ángulo estable del giroscopio: deja el carro quieto (y el giroscopio listo) y vuelve a tocar Empezar."
                } else {
                    escalaInicioX10 = angulo
                    escalaInicioMs = android.os.SystemClock.elapsedRealtime()
                    escalaPaso = 1
                    escalaMensaje = "Ángulo inicial guardado (${"%.1f".format(angulo / 10.0)}°). Gira el carro dos vueltas completas hacia el mismo lado, vuelve a alinearlo con la línea y toca Terminé."
                    Log.i(ETIQUETA, "escala_giroscopio inicio_x10=$angulo escala_actual_ppm=${logica.escalaImuPpm}")
                }
                mostrar()
            }
            1 -> anguloQuieto { angulo ->
                val duracion = android.os.SystemClock.elapsedRealtime() - escalaInicioMs
                val actual = logica.escalaImuPpm
                if (angulo == null) {
                    escalaMensaje = "El carro todavía se mueve (o se perdió el giroscopio): déjalo quieto, alineado, y vuelve a tocar Terminé."
                } else if (actual == null) {
                    escalaPaso = 0
                    escalaMensaje = "El carro ya no anuncia la escala (¿se reconectó con otro firmware?). Empieza de nuevo."
                } else if (duracion > ESCALA_MAX_MS) {
                    escalaPaso = 0
                    escalaMensaje = "Tardaste más de ${ESCALA_MAX_MS / 60_000} minutos: la deriva del giroscopio pesaría en la medida. Empieza de nuevo."
                } else {
                    val medido = angulo - escalaInicioX10
                    val r = CalGiroNativa.escala(medido, actual)
                    Log.i(ETIQUETA, "escala_giroscopio medido_x10=$medido ms=$duracion codigo=${r[CalGiroNativa.ESC_CODIGO]} vueltas=${r[CalGiroNativa.ESC_VUELTAS]} " +
                        "error_ppm=${r[CalGiroNativa.ESC_ERROR_PPM]} nueva_ppm=${r[CalGiroNativa.ESC_NUEVA_PPM]} anterior_ppm=$actual")
                    val grados = "%.1f".format(medido / 10.0)
                    when (r[CalGiroNativa.ESC_CODIGO]) {
                        CalGiroNativa.ESC_OK -> {
                            escalaNuevaPpm = r[CalGiroNativa.ESC_NUEVA_PPM]
                            escalaPaso = 2
                            escalaMensaje = "Midió $grados° en ${r[CalGiroNativa.ESC_VUELTAS]} vuelta(s): con la escala de ahora el giroscopio lee " +
                                "${"%+.2f".format(r[CalGiroNativa.ESC_ERROR_PPM] / 10000.0)} %. Corrección nueva: ${"%+d".format(escalaNuevaPpm)} ppm. Toca Guardar en el carro."
                        }
                        CalGiroNativa.ESC_POCO -> {
                            escalaPaso = 0
                            escalaMensaje = "Midió $grados°: menos de una vuelta completa. Empieza de nuevo."
                        }
                        CalGiroNativa.ESC_NO_CUADRA -> {
                            escalaPaso = 0
                            escalaMensaje = "Midió $grados°: no cuadra con vueltas completas (¿quedó bien alineado? ¿contaste bien las vueltas?). Empieza de nuevo."
                        }
                        else -> {
                            escalaPaso = 0
                            escalaMensaje = "La corrección saldría de ${"%+d".format(r[CalGiroNativa.ESC_NUEVA_PPM])} ppm (más de ±5 %): revisa la alineación y empieza de nuevo."
                        }
                    }
                }
                mostrar()
            }
            else -> guardarEscala(escalaNuevaPpm)
        }
    }

    /** Manda la corrección al carro y espera su confirmación (el firmware la guarda en su memoria y la usa en los giros, en SENS y en RUMBO). */
    private fun guardarEscala(ppm: Int) {
        escalaEnviando = true
        mostrar()
        logica.enviarEscalaImu(ppm, 1500L) { ok ->
            escalaEnviando = false
            escalaPaso = 0
            escalaMensaje = if (ok) "Guardada en el carro: ${"%+d".format(ppm)} ppm. Ya la usan los giros, SENS y el rumbo."
                            else "El carro no la confirmó (¿se estaba moviendo? ¿firmware anterior?): no se guardó."
            Log.i(ETIQUETA, "escala_giroscopio guardar ppm=$ppm confirmado=$ok")
            mostrar()
        }
    }

    // ── Estado en pantalla ────────────────────────────────────────────────

    private fun mostrar() {
        val conectado = logica.conexion == ControlLogica.Conexion.CONECTADO
        val giroOk = logica.capsGiroscopio == true
        val laserOk = logica.capsLaser == true
        val contesto = logica.capsGiroscopio != null
        findViewById<TextView>(R.id.sens_versiones).text = "${logica.versionFirmware}\nApp ${BuildConfig.VERSION_NAME} (${BuildConfig.GIT_HASH})"
        when {
            !conectado -> chip.poner(Nivel.AVISO, getString(R.string.sens_chip_sin_conexion))
            !contesto -> chip.poner(Nivel.AVISO, getString(R.string.sens_chip_ninguno))
            giroOk && laserOk -> chip.poner(Nivel.OK, getString(R.string.sens_chip_ok))
            giroOk || laserOk -> chip.poner(Nivel.AVISO, getString(R.string.sens_chip_parcial))
            else -> chip.poner(Nivel.ERROR, getString(R.string.sens_chip_ninguno))
        }
        val frescoMs = android.os.SystemClock.elapsedRealtime() - logica.ultimoSensMs
        val hayAngulo = giroOk && logica.ultimoSensMs != 0L && frescoMs < 1500L
        textoGiro.text = if (!contesto) "Giroscopio: sin respuesta del carro"
            else if (!giroOk) "Giroscopio: ${if (logica.imuEstado == 1) "calibrando sesgo" else "MAL (motivo ${logica.imuMotivo})"}; las rutas usan giros por tiempo"
            else "Giroscopio: ok · " + (if (hayAngulo) "ángulo ${"%.1f".format(logica.anguloX10 / 10.0)}°" else "ángulo —") +
                (logica.sesgoX100?.let { " · sesgo ${"%.2f".format(it / 100.0)} °/s" } ?: "") + (if (logica.usarGiroscopio) " · en uso" else " · apagado")
        textoGiro.append("\nReintentos del sesgo: ${logica.imuReintentos}. Últimos arranques con reintentos del giroscopio: ${logica.imuArranquesConReintentos} de ${logica.imuArranquesAnotados}.")
        // J16: si este firmware sabe sostener el rumbo al avanzar y si las rutas lo van a usar (lo decide ControlLogica.rumboActivo).
        textoGiro.append("\nRumbo al avanzar: " + when (logica.capsRumbo) {
            null -> "sin respuesta del carro"
            false -> "este firmware no lo tiene (anterior a J16)"
            true -> if (logica.rumboActivo) "disponible · en uso en las rutas" else if (!logica.usarRumbo) "disponible · apagado" else "disponible · no actúa sin el giroscopio"
        } + (if (logica.ultimoRumbo.isNotEmpty()) "\nÚltimo estado del rumbo: ${logica.ultimoRumbo}" else ""))
        val hayDistancia = laserOk && logica.ultimoSensMs != 0L && frescoMs < 1500L
        textoLaser.text = if (!contesto) "Láser: sin respuesta del carro"
            else if (!laserOk) "Láser: MAL (no funciona; sin freno de emergencia)"
            else "Láser: ok · " + (if (!hayDistancia) "distancia —" else if (logica.banderasSens and 4 != 0) "${logica.distanciaMm} mm" else "libre (más de 1.2 m)") +
                (if (logica.usarSensorDistancia) " · en uso" else " · apagado") +
                (if (logica.tofReintentos > 0) "\n" + getString(R.string.sens_laser_reintentos, logica.tofReintentos) else "") +
                (if (logica.arranquesAnotados > 0) "\n" + getString(R.string.sens_arranques, logica.arranquesConReintentos, logica.arranquesAnotados) else "")
        textoAviso.text = if (!conectado) getString(R.string.sens_sin_conexion_prueba) else if (!contesto) getString(R.string.sens_sin_firmware) else
            (if (logica.laserArrancaMal) getString(R.string.sens_aviso_arranques) + "\n" else "") + logica.ultimoAvisoSensores
        textoAviso.visibility = if (textoAviso.text.isEmpty()) View.GONE else View.VISIBLE
        val puedeProbar = conectado && !probando && logica.perfilConfirmado == ControlLogica.Perfil.AUTO
        botonIzq.isEnabled = puedeProbar
        botonDer.isEnabled = puedeProbar

        // J16: escala del giroscopio.
        val escala = logica.escalaImuPpm
        textoEscala.text = (if (!conectado) "Sin conexión con el carro."
            else if (escala == null) (if (contesto) "Este firmware no la tiene (anterior a J16): no se puede corregir." else "Sin respuesta del carro.")
            else "Corrección en uso: ${"%+d".format(escala)} ppm (${"%+.2f".format(escala / 10000.0)} %)") +
            (if (escalaMensaje.isNotEmpty()) "\n" + escalaMensaje else "")
        val ocupadoEscala = escalaEsperando || escalaEnviando
        botonEscala.setText(when (escalaPaso) { 1 -> R.string.sens_escala_termine; 2 -> R.string.sens_escala_guardar; else -> R.string.sens_escala_empezar })
        botonEscala.isEnabled = conectado && escala != null && giroOk && !probando && !ocupadoEscala
        botonEscalaCancelar.isEnabled = escalaPaso != 0 && !ocupadoEscala
        botonEscalaQuitar.isEnabled = conectado && escala != null && escala != 0 && escalaPaso == 0 && !probando && !ocupadoEscala
        botonRecalibrar.isEnabled = escalaPaso != 1   // recalibrar o poner el ángulo en cero a mitad de la medición la echaría a perder
        botonCero.isEnabled = escalaPaso != 1
    }

    // ── Prueba de giro ────────────────────────────────────────────────────

    /** Gira 90° (positivo = izquierda) con el método elegido. */
    private fun probarGiro(grados: Int) {
        val porAngulo = grupoMetodo.checkedRadioButtonId == R.id.sens_metodo_angulo
        if (logica.conexion != ControlLogica.Conexion.CONECTADO) {
            textoResultado.setText(R.string.sens_sin_conexion_prueba)
            return
        }
        if (porAngulo && logica.capsGiroscopio != true) {
            textoResultado.setText(R.string.sens_sin_giroscopio_prueba)
            return
        }
        reloj.removeCallbacks(tareaSinRespuesta)
        probando = true
        inicioMs = android.os.SystemClock.elapsedRealtime()
        angulo0X10 = logica.anguloX10
        val lado = if (grados > 0) "izquierda" else "derecha"
        if (porAngulo) {
            textoResultado.text = "Girando 90° a la $lado por ángulo…"
            Log.i(ETIQUETA, "prueba_giro inicio por=angulo lado=$lado objetivo=$grados")
            logica.girar(grados)
            reloj.postDelayed(tareaSinRespuesta, ESPERA_GIRO_MS)
        } else {
            val lado0 = if (grados > 0) CalibracionNativa.LADO_IZQUIERDA else CalibracionNativa.LADO_DERECHA
            val ms = CalibracionNativa.gradosAMs(guardada.leerAjustada(ControlLogica.Perfil.AUTO), lado0 * 90)
            textoResultado.text = "Girando 90° a la $lado por tiempo ($ms ms)…"
            Log.i(ETIQUETA, "prueba_giro inicio por=tiempo lado=$lado ms_planeados=$ms")
            logica.presionar(if (grados > 0) "LEFT" else "RIGHT")
            reloj.postDelayed({
                logica.parar()
                reloj.postDelayed({ terminarPruebaPorTiempo(grados, ms) }, ESPERA_ASENTAR_MS)
            }, ms.toLong())
        }
        mostrar()
    }

    private fun terminarPruebaPorTiempo(grados: Int, ms: Int) {
        probando = false
        val lado = if (grados > 0) "izquierda" else "derecha"
        val medidoX10 = logica.anguloX10 - angulo0X10
        val hayAngulo = logica.capsGiroscopio == true && android.os.SystemClock.elapsedRealtime() - logica.ultimoSensMs < 1500L
        textoResultado.text = if (hayAngulo) "Por tiempo: $ms ms → el giroscopio midió ${"%.1f".format(Math.abs(medidoX10) / 10.0)}° (pedido 90°)"
            else "Por tiempo: $ms ms (sin giroscopio no se sabe cuánto giró)"
        Log.i(ETIQUETA, "prueba_giro fin por=tiempo lado=$lado objetivo=${Math.abs(grados)} ms=$ms medido_x10=${if (hayAngulo) Math.abs(medidoX10) else -1}")
        mostrar()
    }

    /** Llegó GIRO_OK / GIRO_TIMEOUT / GIRO_ERROR del carro (sirve a la prueba por ángulo; ModoSemiautonomo pone la suya durante una ruta). */
    private fun alRespuestaGiro(codigo: Int, objetivo: Int, realX10: Int, ms: Int, ant: Int) {
        if (!probando) {
            return
        }
        reloj.removeCallbacks(tareaSinRespuesta)
        probando = false
        val estado = when (codigo) {
            NavegadorNativa.GIRO_OK -> "OK"
            NavegadorNativa.GIRO_TIMEOUT -> "TIMEOUT"
            NavegadorNativa.GIRO_ERROR_CON_DATOS -> "ERROR con ángulo parcial"
            else -> "ERROR (no giró)"
        }
        textoResultado.text = "Por ángulo ($estado): pedido ${Math.abs(objetivo)}°, el giroscopio midió ${"%.1f".format(Math.abs(realX10) / 10.0)}° en $ms ms (ant $ant ms)"
        Log.i(ETIQUETA, "prueba_giro fin por=angulo estado=$estado objetivo=${Math.abs(objetivo)} medido_x10=${Math.abs(realX10)} ms=$ms ant=$ant")
        mostrar()
    }
}
