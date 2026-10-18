/*
 * CalibrarGiroActivity.kt — Pantalla CALIBRAR EL GIRO CON EL GIROSCOPIO (J12). Se abre desde la lista de Calibración; el carro hace 3 giros a cada lado y calcula solo los ms por grado y el frenado.
 *
 * GUÍA DE LECTURA
 *  1. El USUARIO toca «Empezar» (nunca arranca sola) con el carro quieto, en el piso y con ~1 m libre alrededor. Hace falta el giroscopio (CAPS) y el perfil AUTO confirmado.
 *  2. Cada 50 ms se le da a CalGiroNativa (nucleo_c/calgiro.c) el ángulo del último SENS; ella decide si el carro espera, gira a la izquierda/derecha (LEFT/RIGHT por tiempo, como siempre) o frena. Aquí solo se
 *     traduce su respuesta a presionar()/parar() y se muestra el avance «Giro k de 6».
 *  3. Al terminar (LISTO): se guarda una copia (para «Deshacer»), se guarda la calibración nueva (solo cambian los ms por grado y el frenado de cada giro; el desplazamiento del pivote sigue manual), se
 *     anota la fecha de GIRO_IZQ y GIRO_DER y se vuelve a 1000 el ajuste fino del giro de 90° de esos lados (ya no aplica). Si un valor cambió más de 10 % contra lo guardado, se avisa en rojo.
 *  4. Seguridad: PARO, salir de la pantalla o cualquier error mandan STOP y nunca dejan un comando puesto. Nada se guarda si la calibración no terminó bien. El resultado queda en el log (CALGIRO ...).
 */
package com.fsi.carro

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.view.View
import android.widget.TextView
import com.google.android.material.button.MaterialButton
import com.google.android.material.progressindicator.LinearProgressIndicator

class CalibrarGiroActivity : androidx.appcompat.app.AppCompatActivity() {

    companion object {
        const val ETIQUETA = "CALGIRO"
        private const val PERIODO_MS = 50L
        private const val AVISO_PCT = 10            // igual que CG_CAMBIO_AVISO_PCT de calgiro.h
        private const val ANGULO_FRESCO_MS = 500L   // un SENS más viejo que esto no cuenta como ángulo
    }

    private lateinit var logica: ControlLogica
    private lateinit var guardada: CalibracionGuardada
    private val reloj = Handler(Looper.getMainLooper())

    private lateinit var chip: ChipEstado
    private lateinit var textoEstado: TextView
    private lateinit var progreso: LinearProgressIndicator
    private lateinit var botonEmpezar: MaterialButton
    private lateinit var tarjetaResultado: View
    private lateinit var textoResultado: TextView
    private lateinit var textoAviso: TextView
    private lateinit var botonDeshacer: MaterialButton

    private var corriendo = false
    private var comandoPuesto = CalGiroNativa.CMD_STOP
    private var yaGuardado = false
    private var iniciada = false           // el estado de CalGiroNativa es global: solo vale si ESTA pantalla empezó

    private val tareaTick = object : Runnable {
        override fun run() {
            if (corriendo) {
                avanzar()
                reloj.postDelayed(this, PERIODO_MS)
            }
        }
    }

    private val alCambiar: () -> Unit = { mostrar() }

    private val tareaPantalla = object : Runnable {
        override fun run() {
            mostrar()
            reloj.postDelayed(this, 250L)
        }
    }

    override fun onCreate(guardado: Bundle?) {
        super.onCreate(guardado)
        setContentView(R.layout.activity_calibrar_giro)
        intent.getStringExtra("direccion")?.let { Ajustes(this).direccionCarro = it }   // para abrirla con adb contra el servidor falso, igual que Sensores
        logica = SesionCarro.lograr(this)
        guardada = CalibracionGuardada(this)

        findViewById<TextView>(R.id.barra_titulo).setText(R.string.cg_titulo_corto)
        findViewById<MaterialButton>(R.id.barra_volver).setOnClickListener { finish() }
        chip = findViewById(R.id.cg_chip)
        textoEstado = findViewById(R.id.cg_estado)
        progreso = findViewById(R.id.cg_progreso)
        botonEmpezar = findViewById(R.id.cg_empezar)
        tarjetaResultado = findViewById(R.id.cg_tarjeta_resultado)
        textoResultado = findViewById(R.id.cg_texto_resultado)
        textoAviso = findViewById(R.id.cg_texto_aviso)
        botonDeshacer = findViewById(R.id.cg_deshacer)

        progreso.max = CalGiroNativa.TOTAL_GIROS
        botonEmpezar.setOnClickListener { empezar() }
        botonDeshacer.setOnClickListener { deshacer() }
        findViewById<MaterialButton>(R.id.cg_paro).setOnClickListener {
            Haptica.corta(this)
            detener("PARO")
        }
        PantallasLog.abierta("CALIBRAR_GIRO")
    }

    override fun onResume() {
        super.onResume()
        SesionCarro.escuchar(alCambiar)
        SesionCarro.entrar(this, SesionNativa.USUARIO_CALIBRACION)
        logica.pedirPerfil(ControlLogica.Perfil.AUTO)   // SENS solo llega con el perfil AUTO
        reloj.removeCallbacks(tareaPantalla)
        reloj.post(tareaPantalla)
        mostrar()
    }

    override fun onPause() {
        detener("pantalla en segundo plano")
        reloj.removeCallbacks(tareaPantalla)
        SesionCarro.dejarDeEscuchar(alCambiar)
        SesionCarro.salir(SesionNativa.USUARIO_CALIBRACION)
        super.onPause()
    }

    // ── Control ───────────────────────────────────────────────────────────

    private fun hayAngulo(): Boolean =
        logica.capsGiroscopio == true && logica.ultimoSensMs != 0L && SystemClock.elapsedRealtime() - logica.ultimoSensMs < ANGULO_FRESCO_MS

    private fun puedeEmpezar(): Boolean =
        !corriendo && logica.conexion == ControlLogica.Conexion.CONECTADO && logica.capsGiroscopio == true && logica.perfilConfirmado == ControlLogica.Perfil.AUTO

    private fun empezar() {
        if (!puedeEmpezar()) {
            return
        }
        yaGuardado = false
        tarjetaResultado.visibility = View.GONE
        if (CalGiroNativa.iniciar(guardada.leer(), SystemClock.elapsedRealtime()) != 0) {
            textoEstado.setText(R.string.cg_error_inicio)
            return
        }
        Log.i(ETIQUETA, "inicio (lo tocó el usuario)")
        iniciada = true
        corriendo = true
        comandoPuesto = CalGiroNativa.CMD_STOP
        logica.parar()
        reloj.removeCallbacks(tareaTick)
        reloj.post(tareaTick)
        mostrar()
    }

    /** Una vuelta: le pasa el ángulo a calgiro y pone el comando que ella pide. */
    private fun avanzar() {
        val comando = CalGiroNativa.tick(SystemClock.elapsedRealtime(), hayAngulo(), logica.anguloX10)
        if (logica.conexion != ControlLogica.Conexion.CONECTADO) {
            CalGiroNativa.cancelar()
            logica.parar()
            comandoPuesto = CalGiroNativa.CMD_STOP
            terminar("sin conexión")
            return
        }
        if (comando != comandoPuesto) {
            comandoPuesto = comando
            when (comando) {
                CalGiroNativa.CMD_IZQ -> logica.presionar("LEFT")
                CalGiroNativa.CMD_DER -> logica.presionar("RIGHT")
                else -> logica.parar()
            }
        }
        val estado = CalGiroNativa.estado()[CalGiroNativa.E_ESTADO]
        if (estado == CalGiroNativa.LISTO || estado == CalGiroNativa.ERROR || estado == CalGiroNativa.CANCELADO) {
            logica.parar()
            comandoPuesto = CalGiroNativa.CMD_STOP
            terminar("estado $estado")
        }
        mostrar()
    }

    /** PARO, salir de la pantalla o un error: cancela, manda STOP y no guarda nada. */
    private fun detener(motivo: String) {
        if (corriendo) {
            Log.i(ETIQUETA, "cancelada: $motivo")
            CalGiroNativa.cancelar()
            corriendo = false
            reloj.removeCallbacks(tareaTick)
        }
        comandoPuesto = CalGiroNativa.CMD_STOP
        logica.parar()
        mostrar()
    }

    private fun terminar(motivo: String) {
        corriendo = false
        reloj.removeCallbacks(tareaTick)
        val estado = CalGiroNativa.estado()
        Log.i(ETIQUETA, "fin ($motivo): estado=${estado[CalGiroNativa.E_ESTADO]} error=${estado[CalGiroNativa.E_ERROR]} giro=${estado[CalGiroNativa.E_GIRO_ACTUAL]}")
        if (estado[CalGiroNativa.E_ESTADO] == CalGiroNativa.LISTO) {
            aplicarResultado()
        }
        mostrar()
    }

    // ── Resultado ─────────────────────────────────────────────────────────

    private fun aplicarResultado() {
        val r = CalGiroNativa.resultado()
        if (r.size < 47 || r[CalGiroNativa.R_HAY] != 1 || yaGuardado) {
            return
        }
        val nueva = r.copyOfRange(CalGiroNativa.R_CALIBRACION, CalGiroNativa.R_CALIBRACION + CalibracionNativa.ENTEROS)
        guardada.guardarCopia("Calibración del giro con el giroscopio")
        val codigo = guardada.guardar(nueva)
        if (codigo != CalibracionNativa.OK) {
            Log.w(ETIQUETA, "no se guardó: código $codigo")
            textoResultado.text = getString(R.string.cg_no_guardo, codigo)
            textoAviso.text = ""
            tarjetaResultado.visibility = View.VISIBLE
            botonDeshacer.visibility = View.GONE
            return
        }
        yaGuardado = true
        guardada.restaurarFactorGiroDeLado(CalibracionNativa.LADO_IZQUIERDA)
        guardada.restaurarFactorGiroDeLado(CalibracionNativa.LADO_DERECHA)
        guardada.marcarCalibrado(Parte.GIRO_IZQ)
        guardada.marcarCalibrado(Parte.GIRO_DER)
        val lineas = StringBuilder()
        val avisos = StringBuilder()
        for ((nombre, base) in listOf("izquierda" to CalGiroNativa.R_LADO_IZQ, "derecha" to CalGiroNativa.R_LADO_DER)) {
            val msAntes = r[base + CalGiroNativa.L_MS_ANTES] / 10.0
            val msDespues = r[base + CalGiroNativa.L_MS_DESPUES] / 10.0
            val frAntes = r[base + CalGiroNativa.L_FRENADO_ANTES] / 10.0
            val frDespues = r[base + CalGiroNativa.L_FRENADO_DESPUES] / 10.0
            val cambio = r[base + CalGiroNativa.L_CAMBIO_MS_PCT]
            val grados = (0 until 3).joinToString(", ") { "%.0f°".format(r[base + CalGiroNativa.L_GRADOS + it] / 10.0) }
            lineas.append(getString(R.string.cg_linea_lado, nombre, msAntes, msDespues, cambio, frAntes, frDespues, grados)).append('\n')
            if (r[base + CalGiroNativa.L_CAMBIO_GRANDE] != 0) {
                avisos.append(getString(R.string.cg_aviso_cambio, nombre, Math.abs(cambio), AVISO_PCT)).append('\n')
            }
            Log.i(ETIQUETA, "lado=$nombre ms_grado_antes=$msAntes ms_grado_despues=$msDespues cambio_pct=$cambio frenado_antes=$frAntes frenado_despues=$frDespues " +
                "error_max_x10=${r[base + CalGiroNativa.L_ERROR_MAX]} giros=$grados grande=${r[base + CalGiroNativa.L_CAMBIO_GRANDE]}")
        }
        textoResultado.text = lineas.toString().trimEnd()
        textoAviso.text = if (avisos.isEmpty()) getString(R.string.cg_sin_cambio_grande) else avisos.toString().trimEnd() + "\n" + getString(R.string.cg_aviso_revisar)
        botonDeshacer.visibility = View.VISIBLE
        tarjetaResultado.visibility = View.VISIBLE
    }

    private fun deshacer() {
        if (guardada.deshacer()) {
            Log.i(ETIQUETA, "deshecho: vuelve la calibración de antes")
            textoResultado.setText(R.string.cg_deshecho)
            textoAviso.text = ""
            botonDeshacer.visibility = View.GONE
        }
    }

    // ── Pantalla ──────────────────────────────────────────────────────────

    private fun mostrar() {
        val estado = CalGiroNativa.estado()
        val st = if (iniciada) estado[CalGiroNativa.E_ESTADO] else CalGiroNativa.PARADO
        val conectado = logica.conexion == ControlLogica.Conexion.CONECTADO
        when {
            corriendo -> chip.poner(Nivel.AVISO, getString(R.string.cg_chip_midiendo))
            st == CalGiroNativa.LISTO -> chip.poner(Nivel.OK, getString(R.string.cg_chip_listo))
            st == CalGiroNativa.ERROR -> chip.poner(Nivel.ERROR, getString(R.string.cg_chip_error))
            !conectado -> chip.poner(Nivel.AVISO, getString(R.string.sens_chip_sin_conexion))
            logica.capsGiroscopio != true -> chip.poner(Nivel.ERROR, getString(R.string.cg_chip_sin_giroscopio))
            else -> chip.poner(Nivel.NEUTRO, getString(R.string.cg_chip_listo_para))
        }
        val giro = estado[CalGiroNativa.E_GIRO_ACTUAL]
        textoEstado.text = when {
            corriendo -> getString(R.string.cg_avance, Math.min(giro + 1, CalGiroNativa.TOTAL_GIROS), CalGiroNativa.TOTAL_GIROS, fase(st))
            st == CalGiroNativa.ERROR -> getString(R.string.cg_error_texto, CalGiroNativa.textoError(estado[CalGiroNativa.E_ERROR]))
            st == CalGiroNativa.CANCELADO -> getString(R.string.cg_cancelada)
            st == CalGiroNativa.LISTO -> getString(R.string.cg_terminada)
            !conectado -> getString(R.string.sens_sin_conexion_prueba)
            logica.capsGiroscopio != true -> getString(R.string.sens_sin_giroscopio_prueba)
            else -> getString(R.string.cg_antes_de_empezar)
        }
        progreso.progress = if (corriendo || st == CalGiroNativa.LISTO) giro else 0
        botonEmpezar.isEnabled = puedeEmpezar()
    }

    private fun fase(estado: Int): String = when (estado) {
        CalGiroNativa.PREPARANDO -> getString(R.string.cg_fase_quieto)
        CalGiroNativa.GIRANDO -> getString(R.string.cg_fase_girando)
        CalGiroNativa.ASENTANDO -> getString(R.string.cg_fase_midiendo)
        else -> ""
    }
}
