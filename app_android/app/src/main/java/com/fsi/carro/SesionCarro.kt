/*
 * SesionCarro.kt — La conexión con el carro (WebSocket a la DevKit) COMPARTIDA por toda la app.
 *
 * GUÍA DE LECTURA
 *  1. Hay UNA sola ControlLogica y UN solo ModoSemiautonomo para toda la app. Las pantallas (Inicio, Manual, Rutas, Calibración...) ENTRAN con su
 *     número de usuario (SesionNativa.USUARIO_*) y SALEN al irse: la conexión se abre con el primero y se cierra, con 1.5 s de gracia, al irse el
 *     último (el código que decide está en nucleo_c/sesion_compartida.c, con sus pruebas). Cambiar de pantalla o girar el teléfono no corta la conexión.
 *  2. Las pantallas se enteran de los cambios con escuchar(): se les avisa en el hilo principal cuando cambia la conexión, el perfil, la latencia
 *     o la ruta.
 *  3. conectarAhora() es el botón «Conectar»: vuelve a abrir la conexión de inmediato (con la dirección de Ajustes).
 *  4. pararTodo() = STOP inmediato + cancelar la ruta. Lo usa el botón de PARO de la cabecera y de cada pantalla.
 *  5. El manejo del carro NO depende del video: aquí no hay nada del video (eso es SesionVideo).
 */
package com.fsi.carro

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import java.util.concurrent.CopyOnWriteArrayList

object SesionCarro {

    const val ETIQUETA = "SESION_CARRO"
    private const val PERIODO_TICK_MS = 250L

    private val principal = Handler(Looper.getMainLooper())
    private val escuchas = CopyOnWriteArrayList<() -> Unit>()
    private var contexto: Context? = null
    private var iniciada = false
    private var tickProgramado = false

    /** La lógica de control (WebSocket, PING, perfiles, comandos). */
    lateinit var logica: ControlLogica
        private set

    /** El ciclo del modo semiautónomo (el navegador en C). */
    lateinit var semi: ModoSemiautonomo
        private set

    private fun asegurar(ctx: Context) {
        if (iniciada) {
            return
        }
        contexto = ctx.applicationContext
        logica = ControlLogica(EnlaceOkHttp()) { avisar() }
        val guardada = CalibracionGuardada(ctx.applicationContext)
        val ajustes = Ajustes(ctx.applicationContext)
        logica.usarGiroscopio = ajustes.usarGiroscopio             // los interruptores de la pantalla Sensores (J11)
        logica.usarSensorDistancia = ajustes.usarSensorDistancia
        logica.proveedorBalances = { guardada.balancesParaEnviar() }   // el balance de los lados guardado en el teléfono se le manda al carro al conectar
        semi = ModoSemiautonomo(logica) { avisar() }
        iniciada = true
    }

    /** Devuelve la lógica, creándola si hace falta (para pantallas que la necesitan antes de entrar). */
    fun lograr(ctx: Context): ControlLogica {
        asegurar(ctx)
        return logica
    }

    /** Una pantalla entra a usar la conexión con el carro. */
    fun entrar(ctx: Context, usuario: Int) {
        asegurar(ctx)
        val accion = SesionNativa.entrar(SesionNativa.RECURSO_CONTROL, usuario, SystemClock.elapsedRealtime())
        Log.i(ETIQUETA, "entra usuario=$usuario accion=$accion")
        if (accion == SesionNativa.ACCION_ABRIR) {
            abrir()
        }
        programarTick()
        avisar()
    }

    /** Una pantalla deja de usar la conexión. Si era la última, se cierra tras la gracia (con STOP primero). */
    fun salir(usuario: Int) {
        if (!iniciada) {
            return
        }
        SesionNativa.salir(SesionNativa.RECURSO_CONTROL, usuario, SystemClock.elapsedRealtime())
        Log.i(ETIQUETA, "sale usuario=$usuario")
        programarTick()
        avisar()
    }

    /** El botón «Conectar»: abre la conexión de nuevo ya (aunque ya esté intentando). */
    fun conectarAhora() {
        if (!iniciada) {
            return
        }
        Log.i(ETIQUETA, "conectar ahora a ${direccion()}")
        logica.iniciar(direccion())
        avisar()
    }

    /** PARO: STOP inmediato y la ruta en marcha se cancela. */
    fun pararTodo(motivo: String = "PARO") {
        if (!iniciada) {
            return
        }
        semi.cancelar(motivo)
        logica.parar()
    }

    /** true si hay una ruta en marcha o un botón de movimiento sostenido (la cabecera muestra el PARO). */
    val enMovimiento: Boolean
        get() = iniciada && (logica.moviendo || semi.activo || semi.esperandoPerfil)

    fun direccion(): String = Ajustes(contexto ?: throw IllegalStateException("sin contexto")).direccionCarro

    fun escuchar(alCambiar: () -> Unit) {
        escuchas.addIfAbsent(alCambiar)
    }

    fun dejarDeEscuchar(alCambiar: () -> Unit) {
        escuchas.remove(alCambiar)
    }

    private fun abrir() {
        Log.i(ETIQUETA, "abriendo la conexión con el carro (${direccion()})")
        logica.iniciar(direccion())
    }

    private val tick = object : Runnable {
        override fun run() {
            tickProgramado = false
            when (SesionNativa.tick(SesionNativa.RECURSO_CONTROL, SystemClock.elapsedRealtime())) {
                SesionNativa.ACCION_CERRAR -> {
                    Log.i(ETIQUETA, "cerrando la conexión (nadie la usa)")
                    semi.cancelar("Sin pantallas")
                    logica.detener()
                }
                SesionNativa.ACCION_REABRIR -> abrir()
            }
            if (SesionNativa.estado(SesionNativa.RECURSO_CONTROL)[SesionNativa.E_ESTADO] != SesionNativa.CERRADA) {
                programarTick()
            }
        }
    }

    private fun programarTick() {
        if (!tickProgramado) {
            tickProgramado = true
            principal.postDelayed(tick, PERIODO_TICK_MS)
        }
    }

    private fun avisar() {
        for (e in escuchas) {
            e()
        }
    }
}
