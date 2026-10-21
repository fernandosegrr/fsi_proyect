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
    }

    private lateinit var logica: ControlLogica
    private lateinit var ajustes: Ajustes
    private lateinit var guardada: CalibracionGuardada
    private val reloj = Handler(Looper.getMainLooper())

    private lateinit var interruptorGiro: MaterialSwitch
    private lateinit var interruptorLaser: MaterialSwitch
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
        findViewById<MaterialButton>(R.id.sens_recalibrar).setOnClickListener { logica.recalibrarGiroscopio() }
        findViewById<MaterialButton>(R.id.sens_cero).setOnClickListener { logica.ponerAnguloCero() }
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
        SesionCarro.escuchar(alCambiar)
        SesionCarro.entrar(this, SesionNativa.USUARIO_CALIBRACION)
        logica.pedirPerfil(ControlLogica.Perfil.AUTO)   // SENS solo llega con el perfil AUTO
        logica.alGiro = { codigo, objetivo, real, ms -> alRespuestaGiro(codigo, objetivo, real, ms) }
        reloj.removeCallbacks(tareaReloj)
        reloj.post(tareaReloj)
        mostrar()
    }

    override fun onPause() {
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
        logica.parar()
        mostrar()
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
    private fun alRespuestaGiro(codigo: Int, objetivo: Int, realX10: Int, ms: Int) {
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
        textoResultado.text = "Por ángulo ($estado): pedido ${Math.abs(objetivo)}°, el giroscopio midió ${"%.1f".format(Math.abs(realX10) / 10.0)}° en $ms ms"
        Log.i(ETIQUETA, "prueba_giro fin por=angulo estado=$estado objetivo=${Math.abs(objetivo)} medido_x10=${Math.abs(realX10)} ms=$ms")
        mostrar()
    }
}
