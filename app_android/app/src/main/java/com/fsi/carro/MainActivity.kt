/*
 * MainActivity.kt — El armazón de la app: cabecera fija, barra inferior con Inicio / Manual / Rutas / Más y el contenido (cada pantalla es un Fragment).
 *
 * GUÍA DE LECTURA
 *  1. CABECERA FIJA: estado de la conexión (el punto «respira» cuando está conectado), perfil de velocidad (Rápido / Lento), batería del teléfono y, SOLO
 *     mientras una ruta mueve el carro, el botón de PARO rojo (también hay PARO en Manual y en Ejecutar ruta).
 *  2. AVISO GLOBAL: si no hay conexión con el carro en las pantallas que la necesitan, una banda dice qué pasó y qué hacer, con el botón Conectar.
 *  3. irA(pestaña) cambia de pantalla. Si hay una ruta en marcha pide confirmación antes de cambiar (y la detiene si se acepta).
 *  4. La conexión con el carro y el video son sesiones compartidas (SesionCarro, SesionVideo): las pantallas entran y salen; cambiar de pantalla o girar el teléfono
 *     no las corta.
 *  5. onPause manda STOP y cancela la ruta (seguridad: si la app pasa a segundo plano el carro se detiene).
 *  6. La pantalla no se apaga mientras el carro se mueve, en Manual o con una ruta en marcha.
 *  7. Primera vez: se muestra el asistente de 3 pantallas (se puede saltar y se vuelve a ver desde Ayuda).
 *  8. Para pruebas se puede abrir una pantalla directo con adb:
 *       adb shell am start -n com.fsi.carro/.MainActivity --es pantalla manual     (inicio, manual, rutas, mas, ajustes, ayuda, acerca, prueba, asistente)
 */
package com.fsi.carro

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Bundle
import android.view.View
import android.view.WindowManager
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentTransaction
import com.google.android.material.button.MaterialButton
import com.google.android.material.navigation.NavigationBarView

class MainActivity : AppCompatActivity() {

    private lateinit var barra: NavigationBarView
    private lateinit var puntoConexion: PuntoConexion
    private lateinit var textoConexion: TextView
    private lateinit var chipPerfil: ChipEstado
    private lateinit var iconoBateria: ImageView
    private lateinit var textoBateria: TextView
    private lateinit var botonParo: MaterialButton
    private lateinit var avisoGlobal: AvisoBanda
    private lateinit var cabecera: View
    private var pestanaActual = R.id.pestana_inicio
    private var sincronizando = false
    private var conectadoDesdeMs = 0L

    private val alCambiar: () -> Unit = { actualizarCabecera() }

    private val receptorBateria = object : BroadcastReceiver() {
        override fun onReceive(contexto: Context, intento: Intent) {
            mostrarBateria(intento)
        }
    }

    override fun onCreate(guardado: Bundle?) {
        super.onCreate(guardado)
        setContentView(R.layout.activity_main)
        Diseno.barras(this)
        SesionCarro.lograr(this)

        barra = findViewById(R.id.barra)
        cabecera = findViewById<View>(R.id.punto_conexion).parent.parent as View
        puntoConexion = findViewById(R.id.punto_conexion)
        textoConexion = findViewById(R.id.texto_conexion)
        chipPerfil = findViewById(R.id.chip_perfil)
        iconoBateria = findViewById(R.id.icono_bateria)
        textoBateria = findViewById(R.id.texto_bateria)
        botonParo = findViewById(R.id.boton_paro_cabecera)
        avisoGlobal = findViewById(R.id.aviso_global)

        barra.setOnItemSelectedListener { item ->
            if (!sincronizando) {
                irA(item.itemId)
            }
            true
        }
        botonParo.setOnClickListener {
            Haptica.corta(this)
            SesionCarro.pararTodo("PARO (cabecera)")
        }
        Movimiento.respuestaAlTocar(botonParo)

        supportFragmentManager.addOnBackStackChangedListener {
            if (supportFragmentManager.findFragmentByTag("asistente") == null) {
                modoCompleto(false)   // el asistente ya se cerró: vuelven la cabecera y la barra
            }
        }
        aplicarExtrasDePrueba()
        if (guardado == null) {
            val ajustes = Ajustes(this)
            val destino = intent.getStringExtra("pantalla")
            if (destino != null) {
                abrirPorNombre(destino)
            } else if (!ajustes.asistenteVisto) {
                mostrarInicio()
                abrirAsistente()
            } else {
                mostrarInicio()
            }
        } else {
            pestanaActual = guardado.getInt("pestana", R.id.pestana_inicio)
            sincronizarBarra()
        }
    }

    /**
     * Ayudas para PROBAR con adb (no cambian nada si no se mandan): `--es video_url http://127.0.0.1:8085/stream` (la cámara falsa), `--es ws_url ws://127.0.0.1:8080/ws`
     * (el servidor falso del carro), `--es tema oscuro|claro|sistema`, `--es orientacion horizontal|vertical|auto` y `--ez asistente_visto true`, `--ez video_en_manual true|false`, `--ei ruta_plantilla 0..3` (pone una plantilla en el editor de rutas).
     */
    private fun aplicarExtrasDePrueba() {
        intent.getStringExtra("video_url")?.let { ConfigVision(this).urlVideoCarro = it; ConfigVision(this).fuenteCarro = true }
        intent.getStringExtra("ws_url")?.let { Ajustes(this).direccionCarro = it }
        if (intent.hasExtra("asistente_visto")) {
            Ajustes(this).asistenteVisto = intent.getBooleanExtra("asistente_visto", true)
        }
        if (intent.hasExtra("ruta_plantilla")) {
            RutaEditorNativa.plantilla(intent.getIntExtra("ruta_plantilla", 0))
        }
        if (intent.hasExtra("video_en_manual")) {
            Ajustes(this).videoEnManual = intent.getBooleanExtra("video_en_manual", true)
        }
        intent.getStringExtra("tema")?.let {
            Ajustes(this).tema = it
            Diseno.aplicarTema(this)
        }
        intent.getStringExtra("orientacion")?.let {
            requestedOrientation = when (it) {
                "horizontal" -> android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
                "vertical" -> android.content.pm.ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                else -> android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
            }
        }
    }

    override fun onSaveInstanceState(salida: Bundle) {
        super.onSaveInstanceState(salida)
        salida.putInt("pestana", pestanaActual)
    }

    override fun onNewIntent(intento: Intent) {
        super.onNewIntent(intento)
        setIntent(intento)
        aplicarExtrasDePrueba()
        intento.getStringExtra("pantalla")?.let { abrirPorNombre(it) }
    }

    override fun onStart() {
        super.onStart()
        SesionCarro.entrar(this, SesionNativa.USUARIO_MAS)   // mientras la app se ve, la conexión con el carro se mantiene (las pantallas internas no la cortan)
        SesionCarro.escuchar(alCambiar)
        mostrarBateria(registerReceiver(receptorBateria, IntentFilter(Intent.ACTION_BATTERY_CHANGED)))
        actualizarCabecera()
    }

    override fun onStop() {
        SesionCarro.salir(SesionNativa.USUARIO_MAS)
        SesionCarro.dejarDeEscuchar(alCambiar)
        try { unregisterReceiver(receptorBateria) } catch (e: IllegalArgumentException) { /* ya estaba suelto */ }
        super.onStop()
    }

    override fun onPause() {
        // Seguridad: al pasar a segundo plano (o girar el teléfono) el carro se detiene y la ruta se cancela.
        SesionCarro.pararTodo("Pantalla en segundo plano")
        super.onPause()
    }

    // ── Navegación ────────────────────────────────────────────────────────

    /** Cambia a una pestaña. Con una ruta en marcha pregunta antes. */
    fun irA(pestana: Int) {
        if (SesionCarro.semi.activo && pestana != R.id.pestana_rutas) {
            Dialogos.confirmar(this, getString(R.string.confirmar_ruta_titulo), getString(R.string.confirmar_ruta_texto), getString(R.string.confirmar_ruta_si)) {
                SesionCarro.pararTodo("Cambio de pantalla")
                cambiarA(pestana)
            }
            sincronizarBarra()
            return
        }
        cambiarA(pestana)
    }

    private fun cambiarA(pestana: Int) {
        supportFragmentManager.popBackStack(null, androidx.fragment.app.FragmentManager.POP_BACK_STACK_INCLUSIVE)
        pestanaActual = pestana
        val fragmento: Fragment = when (pestana) {
            R.id.pestana_manual -> ManualFragment()
            R.id.pestana_rutas -> RutasFragment()
            R.id.pestana_mas -> MasFragment()
            else -> InicioFragment()
        }
        supportFragmentManager.beginTransaction()
            .setTransition(FragmentTransaction.TRANSIT_FRAGMENT_FADE)
            .replace(R.id.contenedor, fragmento, "pestana")
            .commit()
        sincronizarBarra()
        actualizarCabecera()
    }

    private fun mostrarInicio() = cambiarA(R.id.pestana_inicio)

    private fun sincronizarBarra() {
        sincronizando = true
        barra.selectedItemId = pestanaActual
        sincronizando = false
    }

    /** Abre una pantalla de «Más» (o cualquier fragmento) encima de la actual, con la flecha de volver. */
    fun abrirSubpantalla(fragmento: Fragment) {
        supportFragmentManager.beginTransaction()
            .setTransition(FragmentTransaction.TRANSIT_FRAGMENT_FADE)
            .replace(R.id.contenedor, fragmento, "sub")
            .addToBackStack(null)
            .commit()
    }

    /** Vuelve a la pantalla anterior. */
    fun volver() {
        if (supportFragmentManager.backStackEntryCount > 0) {
            supportFragmentManager.popBackStack()
        }
    }

    /** El asistente de primera vez ocupa toda la pantalla (sin cabecera ni barra). */
    fun abrirAsistente() {
        supportFragmentManager.beginTransaction()
            .setTransition(FragmentTransaction.TRANSIT_FRAGMENT_FADE)
            .replace(R.id.contenedor, AsistenteFragment(), "asistente")
            .addToBackStack(null)
            .commit()
        modoCompleto(true)
    }

    /** Oculta (o muestra) la cabecera y la barra. */
    fun modoCompleto(completo: Boolean) {
        cabecera.visibility = if (completo) View.GONE else View.VISIBLE
        barra.visibility = if (completo) View.GONE else View.VISIBLE
        if (completo) {
            avisoGlobal.visibility = View.GONE
        }
    }

    private fun abrirPorNombre(nombre: String) {
        when (nombre) {
            "inicio" -> cambiarA(R.id.pestana_inicio)
            "manual" -> cambiarA(R.id.pestana_manual)
            "rutas" -> cambiarA(R.id.pestana_rutas)
            "mas" -> cambiarA(R.id.pestana_mas)
            "asistente" -> {
                cambiarA(R.id.pestana_inicio)
                abrirAsistente()
            }
            "ejecutar" -> {
                cambiarA(R.id.pestana_rutas)
                if (RutaEditorNativa.vertices().isEmpty()) {
                    RutaEditorNativa.plantilla(RutaEditorNativa.PLANTILLA_CUADRADO)
                }
                supportFragmentManager.executePendingTransactions()
                abrirSubpantalla(EjecutarRutaFragment.nueva("Ruta de prueba", RutaEditorNativa.celdas()))
            }
            else -> {
                cambiarA(R.id.pestana_mas)
                supportFragmentManager.executePendingTransactions()
                (supportFragmentManager.findFragmentByTag("pestana") as? MasFragment)?.abrirPorNombre(nombre)
            }
        }
    }

    // ── Cabecera ──────────────────────────────────────────────────────────

    private fun actualizarCabecera() {
        val logica = SesionCarro.logica
        val semi = SesionCarro.semi
        when (logica.conexion) {
            ControlLogica.Conexion.CONECTADO -> {
                puntoConexion.nivel = Nivel.OK
                textoConexion.setText(R.string.conexion_conectado)
                if (conectadoDesdeMs == 0L) {
                    conectadoDesdeMs = android.os.SystemClock.elapsedRealtime()
                }
            }
            ControlLogica.Conexion.RECONECTANDO -> {
                puntoConexion.nivel = Nivel.AVISO
                textoConexion.setText(R.string.conexion_reconectando)
                conectadoDesdeMs = 0L
            }
            ControlLogica.Conexion.DESCONECTADO -> {
                conectadoDesdeMs = 0L
                if (logica.msSinConexion > 0) {
                    puntoConexion.nivel = Nivel.AVISO
                    textoConexion.setText(R.string.conexion_conectando)
                } else {
                    puntoConexion.nivel = Nivel.NEUTRO
                    textoConexion.setText(R.string.conexion_sin)
                }
            }
        }
        val deseado = logica.perfilDeseado
        val textoPerfil = getString(if (deseado == ControlLogica.Perfil.MANUAL) R.string.perfil_rapido else R.string.perfil_lento)
        when {
            logica.conexion != ControlLogica.Conexion.CONECTADO -> chipPerfil.poner(Nivel.NEUTRO, textoPerfil)
            logica.perfilConfirmado == deseado -> chipPerfil.poner(Nivel.OK, textoPerfil)
            else -> chipPerfil.poner(Nivel.AVISO, "$textoPerfil…")
        }
        val enMarcha = semi.activo || semi.esperandoPerfil
        botonParo.visibility = if (enMarcha) View.VISIBLE else View.GONE
        if (enMarcha || logica.moviendo || pestanaActual == R.id.pestana_manual) {
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
        actualizarAvisoGlobal()
    }

    /** El aviso de arriba: si falta la conexión con el carro (o su velocidad no se confirmó) en una pantalla que la necesita. */
    private fun actualizarAvisoGlobal() {
        val logica = SesionCarro.logica
        val necesitaCarro = pestanaActual == R.id.pestana_manual || pestanaActual == R.id.pestana_rutas
        if (!necesitaCarro || supportFragmentManager.findFragmentByTag("asistente") != null) {
            avisoGlobal.ocultar()
            return
        }
        if (logica.conexion != ControlLogica.Conexion.CONECTADO) {
            if (logica.msSinConexion > InicioFragment.ESPERA_ANTES_DE_AVISAR_MS) {
                val m = if (RedCarro.enRedDelCarro(this)) Mensajes.carroNoResponde() else Mensajes.sinRedDelCarro()
                avisoGlobal.mostrar(Nivel.ERROR, m.quePaso, m.queHacer, getString(R.string.conectar)) { SesionCarro.conectarAhora() }
            } else {
                avisoGlobal.ocultar()
            }
            return
        }
        val desde = conectadoDesdeMs
        if (logica.perfilConfirmado == null && desde != 0L && android.os.SystemClock.elapsedRealtime() - desde > 5000L) {
            val m = Mensajes.perfilSinConfirmar()
            avisoGlobal.mostrar(Nivel.AVISO, m.quePaso, m.queHacer)
        } else {
            avisoGlobal.ocultar()
        }
    }

    private fun mostrarBateria(intento: Intent?) {
        intento ?: return
        val nivel = intento.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
        val escala = intento.getIntExtra(BatteryManager.EXTRA_SCALE, 100)
        val estado = intento.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
        val cargando = estado == BatteryManager.BATTERY_STATUS_CHARGING || estado == BatteryManager.BATTERY_STATUS_FULL
        if (nivel < 0 || escala <= 0) {
            return
        }
        val porciento = nivel * 100 / escala
        textoBateria.text = "$porciento %"
        val baja = porciento <= 20 && !cargando
        iconoBateria.setImageResource(if (baja) R.drawable.ic_battery_alert else R.drawable.ic_battery_full)
        iconoBateria.imageTintList = android.content.res.ColorStateList.valueOf(Diseno.colorDe(this, if (baja) Nivel.ERROR else Nivel.NEUTRO))
        textoBateria.setTextColor(Diseno.colorDe(this, if (baja) Nivel.ERROR else Nivel.NEUTRO))
    }
}
