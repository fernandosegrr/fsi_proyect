/*
 * ModoSemiautonomo.kt — El ciclo del modo semiautónomo: llama al navegador (en C) cada 50 ms
 * y manda el comando resultante al carro.
 *
 * GUÍA DE LECTURA
 *  1. iniciar() carga una ruta en el navegador (NavegadorNativa) y arranca el ciclo.
 *  2. Cada 50 ms, tick() hace tres cosas: arma el "cuadro" de obstáculos, llama a
 *     NavegadorNativa.tick() y, si el comando cambió, se lo manda a ControlLogica.
 *  3. Los obstáculos pueden venir de dos fuentes: tres interruptores simulados (izquierda,
 *     centro, derecha) que pone la pantalla, o la cámara del teléfono (CamaraObstaculos): si
 *     `camara` no es null, cada tick usa el último cuadro NUEVO analizado por el detector.
 *     Si la cámara deja de entregar cuadros más de TIMEOUT_CAMARA_MS, se cancela la ruta (sin
 *     ojos no se avanza). Más adelante aquí irá la ESP32-CAM.
 *  4. ControlLogica ya repite el comando cada 250 ms (heartbeat) y manda STOP al instante
 *     cuando el comando pasa a STOP; aquí solo se avisa cuando CAMBIA.
 *  5. El temporizador de los tramos (cuánto dura cada avance o giro) está en el código en C
 *     (nucleo_c/navegador.c): aquí solo hay un reloj de 50 ms. Si el firmware algún día acepta
 *     comandos con duración, solo habrá que cambiar esa parte del C.
 *  6. Si se pierde la conexión con el carro, se cancela la ruta (más seguro).
 */
package com.fsi.carro

import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log

class ModoSemiautonomo(
    private val logica: ControlLogica,
    private val alCambiar: () -> Unit   // la pantalla se redibuja cuando cambia algo
) {

    companion object {
        const val ETIQUETA = "NAV"
        const val TICK_MS = 50L
        const val TIMEOUT_CAMARA_MS = 1000L   // sin cuadros nuevos durante este tiempo: se cancela la ruta

        // Rutas de prueba: [columna0, fila0, columna1, fila1, ...], cada punto vecino del anterior.
        // Cuadrado de 2 celdas por lado (40 cm) y zigzag de escalones de 1 celda.
        val RUTA_CUADRADO = intArrayOf(1, 4, 1, 3, 1, 2, 2, 2, 3, 2, 3, 3, 3, 4, 2, 4, 1, 4)
        val RUTA_ZIGZAG = intArrayOf(0, 5, 0, 4, 1, 4, 1, 3, 2, 3, 2, 2, 3, 2)
    }

    private val manejador = Handler(Looper.getMainLooper())
    private var ultimoComando = NavegadorNativa.CMD_STOP

    /** Obstáculos simulados (se usan si `camara` es null): izquierda, centro, derecha. */
    val obstaculoSimulado = BooleanArray(3)

    /** Si no es null, los obstáculos salen de esta cámara en vez de los interruptores. */
    var camara: CamaraObstaculos? = null

    private var ultimoCuadroMs = 0L
    private var ultimoEstadoCentro = -1

    /** true si la ruta está en marcha Y el último comando mandado fue avanzar (lo lee el hilo de la cámara). */
    @Volatile
    var avanzando = false
        private set

    /** true mientras el ciclo de 50 ms está corriendo. */
    var activo = false
        private set

    /** Último mensaje para mostrar en pantalla (por ejemplo por qué se canceló). */
    var mensaje = ""
        private set

    private val tarea = object : Runnable {
        override fun run() {
            if (!activo) {
                return
            }
            tick()
            if (activo) {
                manejador.postDelayed(this, TICK_MS)
            }
        }
    }

    // ── Lo que llama la pantalla ──────────────────────────────────────────

    /** Carga la ruta y empieza a seguirla. Devuelve false (y deja un mensaje) si no se pudo. */
    fun iniciar(ruta: IntArray): Boolean {
        if (activo) {
            return false
        }
        if (logica.conexion != ControlLogica.Conexion.CONECTADO) {
            mensaje = "Sin conexión con el carro"
            alCambiar()
            return false
        }
        val fuente = camara
        if (fuente != null && (!fuente.abierta || fuente.ultimoCuadro == null)) {
            mensaje = "La cámara todavía no entrega imágenes"
            alCambiar()
            return false
        }
        NavegadorNativa.ponerLatenciaVisual(fuente?.latenciaVisualMs ?: 0)   // el video del carro llega ~320 ms tarde
        val codigoRuta = NavegadorNativa.cargarRuta(ruta)
        if (codigoRuta != 0) {
            mensaje = "Ruta no válida (código $codigoRuta)"
            alCambiar()
            return false
        }
        val codigo = NavegadorNativa.comenzar(SystemClock.elapsedRealtime())
        if (codigo != 0) {
            mensaje = "No se pudo comenzar (código $codigo)"
            alCambiar()
            return false
        }
        Log.i(ETIQUETA, "ruta iniciada (${ruta.size / 2} puntos)")
        mensaje = ""
        ultimoComando = NavegadorNativa.CMD_STOP
        ultimoCuadroMs = SystemClock.elapsedRealtime()
        ultimoEstadoCentro = -1
        activo = true
        manejador.post(tarea)
        alCambiar()
        return true
    }

    /** Cancela la ruta: STOP inmediato. Se puede llamar aunque no esté activo. */
    fun cancelar(motivo: String = "Cancelado") {
        val estabaActivo = activo
        activo = false
        manejador.removeCallbacks(tarea)
        NavegadorNativa.cancelar()
        if (estabaActivo || ultimoComando != NavegadorNativa.CMD_STOP) {
            Log.i(ETIQUETA, "cancelado: $motivo")
            logica.ordenAutomatica("STOP")
        }
        ultimoComando = NavegadorNativa.CMD_STOP
        avanzando = false
        mensaje = if (estabaActivo) motivo else mensaje
        alCambiar()
    }

    /** Un interruptor de obstáculo simulado: 0 izquierda, 1 centro, 2 derecha. */
    fun alternarObstaculo(zona: Int) {
        obstaculoSimulado[zona] = !obstaculoSimulado[zona]
        Log.i(ETIQUETA, "obstaculo_simulado zona=${arrayOf("IZQUIERDA", "CENTRO", "DERECHA")[zona]} activo=${obstaculoSimulado[zona]}")
        alCambiar()
    }

    // ── El ciclo ──────────────────────────────────────────────────────────

    /** Una vuelta: obstáculos simulados → navegador → comando al carro. */
    private fun tick() {
        if (logica.conexion != ControlLogica.Conexion.CONECTADO) {
            cancelar("Conexión perdida")
            return
        }
        val ahora = SystemClock.elapsedRealtime()
        val fuente = camara
        val cuadro: IntArray?
        if (fuente != null) {
            cuadro = fuente.tomarCuadroNuevo()
            if (cuadro != null) {
                ultimoCuadroMs = ahora
                registrarCambioDeCamara(cuadro)
            } else if (ahora - ultimoCuadroMs > TIMEOUT_CAMARA_MS) {
                cancelar("Cámara sin imágenes")
                return
            }
        } else {
            cuadro = armarCuadro()
        }
        // Giro con la cámara: lo que se vio girar entre los dos últimos cuadros corrige el giro por tiempo del navegador.
        fuente?.tomarGiroNuevo()?.let { NavegadorNativa.aporteVisual(it.giroX10, it.dtMs, it.confiable) }
        val comando = NavegadorNativa.tick(ahora, cuadro != null, cuadro ?: IntArray(10))
        val estado = NavegadorNativa.estado()[NavegadorNativa.E_ESTADO]

        if (comando != ultimoComando) {
            ultimoComando = comando
            avanzando = (comando == NavegadorNativa.CMD_FORWARD)
            val texto = NavegadorNativa.textoComando(comando)
            Log.i(ETIQUETA, "comando=$texto estado=${NavegadorNativa.textoEstado(estado)}")
            logica.ordenAutomatica(texto)
        }
        if (estado == NavegadorNativa.TERMINADO || estado == NavegadorNativa.ERROR || estado == NavegadorNativa.ESPERANDO) {
            activo = false
            avanzando = false
            mensaje = NavegadorNativa.textoEstado(estado)
            Log.i(ETIQUETA, "fin: $mensaje")
        }
        alCambiar()
    }

    /** Anota en logcat cuando cambia lo que la cámara ve al frente (solo los cambios, no cada cuadro). */
    private fun registrarCambioDeCamara(cuadro: IntArray) {
        val estadoCentro = cuadro[ObstaculosNativa.HAY_CENTRO] * 10 + cuadro[ObstaculosNativa.CERCANIA_CENTRO]
        if (estadoCentro != ultimoEstadoCentro) {
            ultimoEstadoCentro = estadoCentro
            Log.i(ETIQUETA, "camara centro_hay=${cuadro[ObstaculosNativa.HAY_CENTRO]} cercania=${cuadro[ObstaculosNativa.CERCANIA_CENTRO]} codigo=${cuadro[ObstaculosNativa.CODIGO]}")
        }
    }

    /** Arma los 10 enteros que espera el navegador con los obstáculos simulados (cercanía 3 = muy cerca). */
    private fun armarCuadro(): IntArray {
        val cuadro = IntArray(10)
        for (zona in 0..2) {
            if (obstaculoSimulado[zona]) {
                cuadro[1 + 2 * zona] = 1   // hay obstáculo
                cuadro[2 + 2 * zona] = 3   // muy cerca
            }
        }
        return cuadro
    }
}
