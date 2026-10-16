/*
 * ControlLogica.kt — La "cabeza" del control: qué comando enviar y cuándo. Sin pantalla.
 *
 * GUÍA DE LECTURA
 *  1. Los tiempos son los mismos que usa la página web del carro (pagina.h):
 *       - mientras un botón está presionado se repite su comando cada 250 ms (heartbeat),
 *         porque el carro frena solo si pasan 1000 ms sin comandos;
 *       - al soltar se envía STOP;
 *       - PING cada 800 ms para mantener viva la conexión.
 *  2. presionar() / soltar() / parar() son lo que llama la pantalla.
 *  3. iniciar() abre la conexión; detener() (la pantalla pasa a segundo plano) envía STOP y cierra.
 *  4. Si la conexión se cae, se reintenta cada 2 s. Al caerse se deja de repetir el comando:
 *     para volver a mover el carro hay que soltar y volver a presionar (más seguro).
 *  4b. PERFILES DE VELOCIDAD: el carro tiene el perfil MANUAL (255) y el AUTO (210). Al abrir o reabrir la conexión el carro siempre queda en
 *     AUTO, así que esta clase manda el perfil que la pantalla quiere (perfilDeseado) en cuanto se abre el WebSocket y lo repite cada ~3 s
 *     (repetirlo en el mismo perfil no frena nada; si el carro volvió a AUTO por el watchdog, así se repone). El carro contesta ACK_MANUAL o
 *     ACK_AUTO: perfilConfirmado. confirmarPerfil() lo pide y espera el ACK (las rutas no arrancan sin él). Un carro con el firmware viejo
 *     no contesta: perfilConfirmado queda en null y las rutas se niegan a arrancar.
 *  4c. BALANCE DE LOS LADOS (recta): el carro guarda en su memoria un factor del lado izquierdo por perfil. Al abrir la conexión esta clase manda los que la app tiene
 *     guardados (proveedorBalances) y enviarBalance() manda uno y espera su ACK_BAL_AUTO:NNN / ACK_BAL_MANUAL:NNN (o NAK_BAL). Un carro con el firmware viejo no
 *     contesta: el aviso sale en false. Ver docs/perfiles_velocidad.md.
 *  5. La latencia es el tiempo entre enviar PING y recibir PONG. El carro real todavía no
 *     contesta PONG (solo el servidor falso), así que con el carro real se verá "—".
 *  Todo ocurre en el hilo principal (los avisos del Enlace ya llegan ahí).
 */
package com.fsi.carro

import android.os.Handler
import android.os.Looper
import android.util.Log

class ControlLogica(
    private val enlace: Enlace,
    private val alCambiar: () -> Unit   // la pantalla se redibuja cuando cambia el estado
) : EscuchaEnlace {

    companion object {
        const val HEARTBEAT_MS = 250L
        const val PING_MS = 800L
        const val REINTENTO_MS = 2000L
    }

    enum class Conexion { DESCONECTADO, CONECTADO, RECONECTANDO }

    /** Perfil de velocidad del carro: MANUAL (255, para los botones) y AUTO (210, para rutas y calibración). */
    enum class Perfil(val comando: String, val ack: String, val texto: String) {
        MANUAL("MODO_MANUAL", "ACK_MANUAL", "MANUAL (255)"),
        AUTO("MODO_AUTO", "ACK_AUTO", "AUTO (210)")
    }

    var conexion = Conexion.DESCONECTADO
        private set
    var latenciaMs: Long? = null   // null = todavía no hay medición
        private set

    /** El perfil que la pantalla quiere. Por defecto AUTO (el seguro); las pantallas piden su perfil (Manual = MANUAL). Se manda al abrir la conexión. */
    var perfilDeseado = Perfil.AUTO
    /** El último perfil que el carro CONFIRMÓ con ACK_MANUAL / ACK_AUTO (null = todavía no confirmó nada en esta conexión). */
    var perfilConfirmado: Perfil? = null
        private set

    /** true mientras hay un botón de movimiento sostenido o una orden automática distinta de STOP (la cabecera muestra el PARO y la pantalla no se apaga). */
    val moviendo: Boolean get() = comandoActivo != null

    /** El comando que se está repitiendo ahora (null = ninguno). */
    val comandoEnMarcha: String? get() = comandoActivo

    /** Quién dice qué balances (milésimas) hay que mandarle al carro al abrir la conexión: [AUTO, MANUAL]; un 0 = no mandar ese. Lo pone SesionCarro con lo guardado en el teléfono. */
    var proveedorBalances: (() -> IntArray)? = null

    /** El último balance que el carro CONFIRMÓ por perfil: [AUTO, MANUAL] (0 = ninguno en esta conexión). */
    val balanceConfirmado = IntArray(2)

    /** Si no es null, se llama con cada medida de PONG (ms): la usa el registro MANUAL_VIDEO y la prueba de latencia con y sin video. */
    var alPong: ((Long) -> Unit)? = null

    /** Cuánto lleva la conexión sin abrirse desde el último intento, en ms (0 si está conectada o sin intentar). Sirve para decidir cuándo avisar al usuario. */
    val msSinConexion: Long get() = if (conexion == Conexion.CONECTADO || sinConexionDesdeMs == 0L) 0L else android.os.SystemClock.elapsedRealtime() - sinConexionDesdeMs
    private var sinConexionDesdeMs = 0L

    private val manejador = Handler(Looper.getMainLooper())
    private var url = ""
    private var activo = false              // true entre iniciar() y detener()
    private var comandoActivo: String? = null   // el botón que se mantiene presionado
    private var pingEnviadoNs = 0L
    private var esperandoPong = false
    // Medida STOP → respuesta del carro (prueba de obstáculos): se cuentan los PING enviados y los PONG recibidos; el PONG del PING que
    // sale justo después del STOP es el que cierra la medida (los anteriores todavía en camino no cuentan).
    private var pingsEnviados = 0
    private var pongsRecibidos = 0
    private var pongObjetivo = 0
    private var stopEnviadoNs = 0L
    private var alRespuestaStop: ((Long) -> Unit)? = null
    private var pingsDesdePerfil = 0
    private var confirmacionPerfil: Perfil? = null            // perfil cuyo ACK se está esperando (confirmarPerfil)
    private var alConfirmarPerfil: ((Boolean) -> Unit)? = null
    private val tareaTiempoPerfil = Runnable { terminarConfirmacion(false) }
    private var balanceEsperado: Pair<Perfil, Int>? = null    // el balance cuyo ACK se está esperando (enviarBalance)
    private var alConfirmarBalance: ((Boolean) -> Unit)? = null
    private val tareaTiempoBalance = Runnable { terminarBalance(false) }

    // ── Temporizadores ────────────────────────────────────────────────────

    /** Repite el comando del botón presionado cada 250 ms. */
    private val tareaHeartbeat = object : Runnable {
        override fun run() {
            val comando = comandoActivo ?: return
            enviar(comando)
            manejador.postDelayed(this, HEARTBEAT_MS)
        }
    }

    /** Envía PING cada 800 ms mientras haya conexión. */
    private val tareaPing = object : Runnable {
        override fun run() {
            if (conexion == Conexion.CONECTADO) {
                pingEnviadoNs = System.nanoTime()
                esperandoPong = true
                pingsEnviados++
                enviar("PING")
                if (++pingsDesdePerfil % 4 == 0) {
                    enviar(perfilDeseado.comando)   // cada 4 PING (3.2 s) se repone el perfil por si el carro volvió a AUTO por el watchdog
                }
            }
            manejador.postDelayed(this, PING_MS)
        }
    }

    /** Vuelve a conectar (se programa 2 s después de una caída). */
    private val tareaReintento = Runnable {
        if (activo) {
            Log.i(EnlaceOkHttp.ETIQUETA, "reintentando conexión")
            enlace.conectar(url)
        }
    }

    // ── Lo que llama la pantalla ──────────────────────────────────────────

    /** Abre (o reabre) la conexión con la dirección dada. */
    fun iniciar(direccion: String) {
        detenerTemporizadores()
        enlace.escucha = this
        url = direccion
        activo = true
        sinConexionDesdeMs = android.os.SystemClock.elapsedRealtime()   // desde ahora se cuenta cuánto tarda en conectar
        cambiarEstado(Conexion.DESCONECTADO)
        enlace.conectar(url)
    }

    /** La pantalla pasa a segundo plano: STOP y cierre de la conexión. */
    fun detener() {
        if (activo) {
            Log.i(EnlaceOkHttp.ETIQUETA, "pausa: STOP y cierre de la conexión")
            enviar("STOP")
        }
        activo = false
        detenerTemporizadores()
        comandoActivo = null
        enlace.cerrar()
        latenciaMs = null
        sinConexionDesdeMs = 0L   // ya no se está intentando conectar
        cambiarEstado(Conexion.DESCONECTADO)
    }

    /** Un botón de movimiento se presionó: enviar su comando ya y repetirlo cada 250 ms. */
    fun presionar(comando: String) {
        comandoActivo = comando
        enviar(comando)
        manejador.removeCallbacks(tareaHeartbeat)
        manejador.postDelayed(tareaHeartbeat, HEARTBEAT_MS)
    }

    /** Se soltó un botón: si era el que estaba activo, parar. (Soltar otro botón no hace nada.) */
    fun soltar(comando: String) {
        if (comandoActivo == comando) {
            parar()
        }
    }

    /**
     * Orden del modo semiautónomo (ModoSemiautonomo): se llama solo cuando el comando CAMBIA.
     * STOP se envía al instante y deja de repetirse; cualquier otro se envía al instante y se
     * repite cada 250 ms (heartbeat), igual que si se mantuviera presionado un botón.
     */
    fun ordenAutomatica(comando: String) {
        if (comando == "STOP") {
            parar()
        } else {
            presionar(comando)
        }
    }

    /**
     * Como parar(), pero justo después del STOP manda un PING y, cuando vuelve su PONG (el carro ya procesó el STOP porque los mensajes
     * llegan en orden), avisa con los ms que pasaron desde que salió el STOP. Lo usa la prueba de obstáculos.
     */
    fun pararConRespuesta(alRespuesta: (Long) -> Unit) {
        stopEnviadoNs = System.nanoTime()
        parar()
        pingEnviadoNs = System.nanoTime()
        esperandoPong = true
        pingsEnviados++
        pongObjetivo = pingsEnviados
        alRespuestaStop = alRespuesta
        enviar("PING")
    }

    /** Cambia el perfil que se quiere y lo manda ya si hay conexión (sin esperar la respuesta). */
    fun pedirPerfil(perfil: Perfil) {
        perfilDeseado = perfil
        if (conexion == Conexion.CONECTADO) {
            enviar(perfil.comando)
        }
    }

    /**
     * Pide el perfil y espera su ACK hasta `esperaMs` ms; avisa true si el carro lo confirmó y false si no (sin conexión, la conexión se cayó
     * o no contestó a tiempo). Las rutas lo usan para no arrancar sin saber en qué velocidad está el carro.
     */
    fun confirmarPerfil(perfil: Perfil, esperaMs: Long, alTerminar: (Boolean) -> Unit) {
        terminarConfirmacion(false)   // una petición anterior que siguiera esperando termina en "no"
        perfilDeseado = perfil
        if (conexion != Conexion.CONECTADO) {
            alTerminar(false)
            return
        }
        confirmacionPerfil = perfil
        alConfirmarPerfil = alTerminar
        manejador.postDelayed(tareaTiempoPerfil, esperaMs)
        enviar(perfil.comando)
    }

    /** Deja de esperar el ACK de confirmarPerfil sin avisar (la ruta se canceló mientras esperaba). */
    fun cancelarConfirmacion() {
        manejador.removeCallbacks(tareaTiempoPerfil)
        confirmacionPerfil = null
        alConfirmarPerfil = null
    }

    private fun terminarConfirmacion(exito: Boolean) {
        manejador.removeCallbacks(tareaTiempoPerfil)
        val aviso = alConfirmarPerfil
        confirmacionPerfil = null
        alConfirmarPerfil = null
        aviso?.invoke(exito)
    }

    /**
     * Le manda al carro el balance (factor del lado izquierdo, 800 a 1000) de un perfil y espera su ACK hasta `esperaMs` ms: avisa true si el carro lo confirmó con ese
     * mismo valor y false si no (sin conexión, NAK_BAL, firmware viejo que no contesta o se acabó el tiempo).
     */
    fun enviarBalance(perfil: Perfil, milesimas: Int, esperaMs: Long, alTerminar: (Boolean) -> Unit) {
        terminarBalance(false)
        if (conexion != Conexion.CONECTADO) {
            alTerminar(false)
            return
        }
        balanceEsperado = Pair(perfil, milesimas)
        alConfirmarBalance = alTerminar
        manejador.postDelayed(tareaTiempoBalance, esperaMs)
        enviar("BAL_${perfil.name}:$milesimas")
    }

    private fun terminarBalance(exito: Boolean) {
        manejador.removeCallbacks(tareaTiempoBalance)
        val aviso = alConfirmarBalance
        balanceEsperado = null
        alConfirmarBalance = null
        aviso?.invoke(exito)
    }

    /** Dejar de repetir el comando y enviar STOP. */
    fun parar() {
        comandoActivo = null
        manejador.removeCallbacks(tareaHeartbeat)
        enviar("STOP")
    }

    // ── Lo que avisa el Enlace ────────────────────────────────────────────

    override fun alAbrir() {
        Log.i(EnlaceOkHttp.ETIQUETA, "conexión abierta")
        manejador.removeCallbacks(tareaReintento)
        esperandoPong = false
        pingsEnviados = 0
        pongsRecibidos = 0
        pongObjetivo = 0
        alRespuestaStop = null
        perfilConfirmado = null          // el carro vuelve a AUTO al conectarse: hasta su ACK no se sabe en qué perfil está
        pingsDesdePerfil = 0
        cambiarEstado(Conexion.CONECTADO)
        enviar(perfilDeseado.comando)    // el perfil que se quiere, antes que cualquier otro comando
        balanceConfirmado[0] = 0
        balanceConfirmado[1] = 0
        proveedorBalances?.invoke()?.let { b ->   // el balance de los lados que la app tiene guardado (el carro también lo guarda; esto lo deja igual en los dos)
            if (b.size >= 2 && b[0] > 0) {
                enviar("BAL_AUTO:${b[0]}")
            }
            if (b.size >= 2 && b[1] > 0) {
                enviar("BAL_MANUAL:${b[1]}")
            }
        }
        manejador.removeCallbacks(tareaPing)
        manejador.post(tareaPing)   // primer PING ya, luego cada 800 ms
    }

    override fun alMensaje(texto: String) {
        if (texto == Perfil.MANUAL.ack || texto == Perfil.AUTO.ack) {
            val perfil = if (texto == Perfil.MANUAL.ack) Perfil.MANUAL else Perfil.AUTO
            perfilConfirmado = perfil
            Log.i(EnlaceOkHttp.ETIQUETA, "perfil confirmado por el carro: ${perfil.texto}")
            if (confirmacionPerfil == perfil) {
                terminarConfirmacion(true)
            }
            alCambiar()
        }
        if (texto.startsWith("ACK_BAL_AUTO:") || texto.startsWith("ACK_BAL_MANUAL:")) {
            val perfil = if (texto.startsWith("ACK_BAL_AUTO:")) Perfil.AUTO else Perfil.MANUAL
            val valor = texto.substringAfter(':').toIntOrNull() ?: 0
            balanceConfirmado[if (perfil == Perfil.AUTO) 0 else 1] = valor
            Log.i(EnlaceOkHttp.ETIQUETA, "balance confirmado por el carro: ${perfil.name} $valor")
            if (balanceEsperado == Pair(perfil, valor)) {
                terminarBalance(true)
            }
            alCambiar()
        }
        if (texto == "NAK_BAL") {
            Log.w(EnlaceOkHttp.ETIQUETA, "el carro rechazó el balance (NAK_BAL)")
            terminarBalance(false)
        }
        if (texto == "PONG") {
            pongsRecibidos++
            val aviso = alRespuestaStop
            if (aviso != null && pongsRecibidos >= pongObjetivo) {
                alRespuestaStop = null
                aviso(Math.round((System.nanoTime() - stopEnviadoNs) / 1.0e6))
            }
        }
        if (texto == "PONG" && esperandoPong) {
            esperandoPong = false
            latenciaMs = Math.round((System.nanoTime() - pingEnviadoNs) / 1.0e6)
            Log.i(EnlaceOkHttp.ETIQUETA, "latencia_ms=$latenciaMs")
            alPong?.invoke(latenciaMs!!)
            alCambiar()
        }
    }

    override fun alCerrar() {
        manejador.removeCallbacks(tareaPing)
        manejador.removeCallbacks(tareaHeartbeat)
        comandoActivo = null
        esperandoPong = false
        latenciaMs = null
        perfilConfirmado = null
        terminarConfirmacion(false)
        terminarBalance(false)
        if (activo) {
            Log.i(EnlaceOkHttp.ETIQUETA, "conexión perdida: se reintenta en ${REINTENTO_MS / 1000} s")
            cambiarEstado(Conexion.RECONECTANDO)
            manejador.removeCallbacks(tareaReintento)
            manejador.postDelayed(tareaReintento, REINTENTO_MS)
        } else {
            cambiarEstado(Conexion.DESCONECTADO)
        }
    }

    // ── Ayudas ────────────────────────────────────────────────────────────

    private fun enviar(texto: String) {
        val enviado = enlace.enviar(texto)
        Log.i(EnlaceOkHttp.ETIQUETA, "enviado=$texto ok=$enviado")
    }

    private fun detenerTemporizadores() {
        manejador.removeCallbacks(tareaHeartbeat)
        manejador.removeCallbacks(tareaPing)
        manejador.removeCallbacks(tareaReintento)
    }

    private fun cambiarEstado(nuevo: Conexion) {
        if (conexion != nuevo) {
            Log.i(EnlaceOkHttp.ETIQUETA, "estado=$nuevo")
            sinConexionDesdeMs = if (nuevo == Conexion.CONECTADO) 0L else if (sinConexionDesdeMs == 0L) android.os.SystemClock.elapsedRealtime() else sinConexionDesdeMs
        }
        conexion = nuevo
        alCambiar()
    }
}
