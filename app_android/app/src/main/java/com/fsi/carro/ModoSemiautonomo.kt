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
        // Prueba de obstáculos: una recta de 5 celdas (1 m). Al frenar por un obstáculo la pasada se mide y la ruta se cancela (el carro queda detenido).
        val RUTA_PRUEBA_OBSTACULO = intArrayOf(2, 8, 2, 7, 2, 6, 2, 5, 2, 4, 2, 3)
        // Ruta para esquivar un objeto grande: recta de 7 celdas (1.4 m). A diferencia de la de arriba NO se cancela al frenar: el carro esquiva
        // (gira, mira, retrocede y reintenta si hace falta) y sigue hasta el final. También anota OBST_PRUEBA en el primer frenado.
        val RUTA_ESQUIVAR_OBJETO = intArrayOf(2, 8, 2, 7, 2, 6, 2, 5, 2, 4, 2, 3, 2, 2, 2, 1)
        const val ETIQUETA_PRUEBA = "OBST_PRUEBA"
        const val MOTIVO_SIN_VIDEO = 5   // MotivoErrorNav en navegador.h: NAV_MOTIVO_SIN_VIDEO
        val NOMBRE_ZONA = arrayOf("IZQUIERDA", "CENTRO", "DERECHA")
    }

    private val manejador = Handler(Looper.getMainLooper())
    private var ultimoComando = NavegadorNativa.CMD_STOP

    /** Obstáculos simulados (se usan si `camara` es null): izquierda, centro, derecha. */
    val obstaculoSimulado = BooleanArray(3)

    /** Si no es null, los obstáculos salen de esta cámara en vez de los interruptores. */
    var camara: CamaraObstaculos? = null

    private var ultimoCuadroMs = 0L
    private var ultimoEstadoCentro = -1
    private var ultimaFaseLog = -1      // para escribir en el log solo cuando cambia la maniobra de esquivar
    private var ultimosReintentosLog = -1

    // ── Prueba "Obstáculos: cámara" ─────────────────────────────────────
    /** La calibración guardada (la pone la pantalla de control): se usan ms por cm y frenado para convertir el retraso en cm. */
    var leerCalibracion: () -> IntArray = { IntArray(0) }

    /** Lo que muestra la pantalla de la prueba: última pasada y resumen. */
    var textoPrueba = ""
        private set

    private var modoPruebaObstaculo = false     // true con la ruta de prueba
    private var resumenEscrito = true            // el resumen de esta ruta ya se escribió en el log
    // El PRIMER cuadro de la racha actual con un obstáculo cerca (cercanía 2 o más): de ahí se cuentan los tiempos.
    private var bloqueoMarcaMs = 0L
    private var bloqueoMsDetectar = 0.0
    private var bloqueoCercania = 0
    private var bloqueoZona = 0
    private var bloqueoFuente = ""

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
        modoPruebaObstaculo = (ruta === RUTA_PRUEBA_OBSTACULO)   // la ruta de esquivar tiene otros puntos, pero igual se mide el primer frenado
        bloqueoMarcaMs = 0L
        resumenEscrito = false
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
        // Con el video del carro (retraso + 38 cm/s) el STOP llega ~17 cm tarde: se frena también con un obstáculo lejano al centro.
        NavegadorNativa.frenarLejosCentro(if (fuente?.esVideoCarro == true) 1 else 0)
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
        // El retroceso de los reintentos de esquive es CIEGO (no hay cámara atrás): se avisa siempre, y si no se calibró, que usa el del avance.
        val calAtras = leerCalibracion()
        if (calAtras.size > CalibracionNativa.CAL_MEDIDOS && calAtras[CalibracionNativa.CAL_MEDIDOS] and CalibracionNativa.MEDIDO_ATRAS == 0) {
            Log.w(ETIQUETA, "retroceso SIN calibrar: los reintentos de esquive retroceden con los ms por cm del avance")
            mensaje = "Aviso: retroceso sin calibrar (usa el valor del avance). Si hay que esquivar un objeto grande el carro puede retroceder hasta 15 cm: deja libre el espacio de atrás."
        }
        ultimaFaseLog = -1
        ultimosReintentosLog = -1
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
        escribirResumenDePrueba()
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
                seguirBloqueo(fuente.cuadroTomado, cuadro)
            } else if (ahora - ultimoCuadroMs > TIMEOUT_CAMARA_MS) {
                cancelar("Cámara sin imágenes")
                return
            }
        } else {
            cuadro = armarCuadro()
        }
        // Giro con la cámara: lo que se vio girar entre los dos últimos cuadros corrige el giro por tiempo del navegador.
        fuente?.tomarGiroNuevo()?.let { NavegadorNativa.aporteVisual(it.giroX10, it.dtMs, it.confiable) }
        if (cuadro != null) {
            // Celdas de obstáculo de cada zona (de la cámara): el navegador elige el lado con más espacio libre. Con los botones no se sabe (-1).
            val detalle = fuente?.cuadroTomado?.resultado
            if (fuente != null && detalle != null && detalle.size > ObstaculosNativa.CELDAS_ZONA + 2) {
                NavegadorNativa.ponerCeldas(detalle[ObstaculosNativa.CELDAS_ZONA], detalle[ObstaculosNativa.CELDAS_ZONA + 1], detalle[ObstaculosNativa.CELDAS_ZONA + 2])
            } else {
                NavegadorNativa.ponerCeldas(-1, -1, -1)
            }
        }
        val comando = NavegadorNativa.tick(ahora, cuadro != null, cuadro ?: IntArray(10))
        val estado = NavegadorNativa.estado()[NavegadorNativa.E_ESTADO]
        registrarEsquive()

        if (comando != ultimoComando) {
            ultimoComando = comando
            avanzando = (comando == NavegadorNativa.CMD_FORWARD)
            val texto = NavegadorNativa.textoComando(comando)
            Log.i(ETIQUETA, "comando=$texto estado=${NavegadorNativa.textoEstado(estado)}")
            if (comando == NavegadorNativa.CMD_STOP && estado == NavegadorNativa.DETENIDO_OBSTACULO && fuente != null && bloqueoMarcaMs != 0L) {
                frenarPorObstaculo(fuente)
            } else {
                logica.ordenAutomatica(texto)
            }
        }
        if (estado == NavegadorNativa.TERMINADO || estado == NavegadorNativa.ERROR || estado == NavegadorNativa.ESPERANDO) {
            activo = false
            avanzando = false
            mensaje = NavegadorNativa.textoEstado(estado)
            if (estado == NavegadorNativa.ERROR && NavegadorNativa.estado()[NavegadorNativa.E_MOTIVO_ERROR] == MOTIVO_SIN_VIDEO) {
                mensaje = "DETENIDO: se perdió el video de la cámara (más de 500 ms sin cuadros). No reanuda solo: inicia la ruta otra vez."
            }
            Log.i(ETIQUETA, "fin: $mensaje")
            escribirResumenDePrueba()
        }
        alCambiar()
    }

    /** Escribe en el log los cambios de la maniobra de esquivar (giro, mirar, retroceso, reintentos) para revisarla después de la prueba. */
    private fun registrarEsquive() {
        val e = NavegadorNativa.estado()
        val fase = if (e[NavegadorNativa.E_ESTADO] == NavegadorNativa.ESQUIVANDO) e[NavegadorNativa.E_FASE] else -1
        val reintentos = e[NavegadorNativa.E_REINTENTOS]
        if (fase != ultimaFaseLog || reintentos != ultimosReintentosLog) {
            ultimaFaseLog = fase
            ultimosReintentosLog = reintentos
            if (fase >= 0) {
                val nombre = arrayOf("GIRAR", "AVANZAR_DE_LADO", "VOLVER", "MIRAR", "RETROCEDER", "ASENTAR").getOrElse(fase) { "?" }
                Log.i(ETIQUETA, "esquivar fase=$nombre reintentos=$reintentos rumbo=${e[NavegadorNativa.E_RUMBO]} x_mm=${e[NavegadorNativa.E_X_MM]} y_mm=${e[NavegadorNativa.E_Y_MM]}")
            }
        }
    }

    /** Lleva la racha de cuadros con un obstáculo cerca (cercanía 2 o más en cualquier zona) y guarda cómo era el PRIMERO. */
    private fun seguirBloqueo(info: CuadroAnalizado?, cuadro: IntArray) {
        var zonaMasCerca = -1
        var cercania = 0
        for (zona in 0..2) {
            if (cuadro[1 + 2 * zona] == 1 && cuadro[2 + 2 * zona] >= 2 && cuadro[2 + 2 * zona] > cercania) {
                cercania = cuadro[2 + 2 * zona]
                zonaMasCerca = zona
            }
        }
        if (zonaMasCerca < 0 || info == null) {
            bloqueoMarcaMs = 0L   // el frente está libre: la racha termina
            return
        }
        if (bloqueoMarcaMs == 0L) {
            bloqueoMarcaMs = info.marcaMs
            bloqueoMsDetectar = info.msHastaResultado
            bloqueoCercania = cercania
            bloqueoZona = zonaMasCerca
            val fusion = info.fusion
            bloqueoFuente = if (fusion != null && fusion[FusionNativa.R_CODIGO] == 0) {
                when (fusion[FusionNativa.R_FUENTE]) {
                    FusionNativa.FUENTE_MODELO -> "modelo"
                    FusionNativa.FUENTE_AMBOS -> "detector+modelo"
                    else -> "detector"
                }
            } else {
                "detector"
            }
        }
    }

    /** El navegador frenó por un obstáculo con la cámara: manda el STOP (con PING para medir la respuesta) y deja anotados los tiempos. */
    private fun frenarPorObstaculo(fuente: CamaraObstaculos) {
        val marca = bloqueoMarcaMs
        val msDetectar = bloqueoMsDetectar
        val cercania = bloqueoCercania
        val zona = bloqueoZona
        val origen = bloqueoFuente
        val latenciaVideo = fuente.latenciaVisualMs
        logica.pararConRespuesta { msRespuesta ->
            manejador.post { terminarPasada(marca, msDetectar, msStop, msRespuesta.toInt(), cercania, zona, origen, latenciaVideo) }
        }
        msStop = (SystemClock.elapsedRealtime() - marca).toInt()   // desde que llegó el cuadro hasta que el STOP salió
    }

    private var msStop = 0

    /** Cierra una pasada: calcula el retraso y los cm (en C), lo escribe en el log y en pantalla, y cancela la ruta de prueba. */
    private fun terminarPasada(marca: Long, msDetectar: Double, msStopEnviado: Int, msRespuesta: Int, cercania: Int, zona: Int, origen: String, latenciaVideo: Int) {
        val cal = leerCalibracion()
        val msPorCmX10 = if (cal.size > CalibracionNativa.CAL_MS_POR_CM_X10) cal[CalibracionNativa.CAL_MS_POR_CM_X10] else 263
        val frenadoMm = if (cal.size > CalibracionNativa.CAL_FRENADO_MM) cal[CalibracionNativa.CAL_FRENADO_MM] else 9
        val retraso = PruebaObstaculoNativa.retrasoMs(msStopEnviado, msRespuesta, latenciaVideo)
        val cmX10 = PruebaObstaculoNativa.cmX10(retraso, msPorCmX10, frenadoMm)
        PruebaObstaculoNativa.agregar(retraso, cmX10)
        val linea = String.format(java.util.Locale.US,
            "marca_cuadro_ms=%d ms_detectar=%.1f ms_stop=%d ms_respuesta=%d cercania=%d zona=%s fuente=%s latencia_video_ms=%d retraso_total_ms=%d cm_recorridos=%.1f (a %.1f ms/cm + frenado %.1f cm)",
            marca, msDetectar, msStopEnviado, msRespuesta, cercania, NOMBRE_ZONA[zona], origen, latenciaVideo, retraso, cmX10 / 10.0, msPorCmX10 / 10.0, frenadoMm / 10.0)
        Log.i(ETIQUETA_PRUEBA, linea)
        val r = PruebaObstaculoNativa.resumen()
        textoPrueba = String.format(java.util.Locale.US,
            "Última pasada: detectó en %.0f ms, STOP enviado a los %d ms, respuesta del carro %d ms, video %d ms → retraso %d ms ≈ %.1f cm (cercanía %d, zona %s, fuente %s)\n"
                + "Pasadas: %d | retraso promedio %d ms, máximo %d ms | cm promedio %.1f, máximo %.1f",
            msDetectar, msStopEnviado, msRespuesta, latenciaVideo, retraso, cmX10 / 10.0, cercania, NOMBRE_ZONA[zona], origen,
            r[PruebaObstaculoNativa.R_PASADAS], r[PruebaObstaculoNativa.R_RETRASO_PROM_MS], r[PruebaObstaculoNativa.R_RETRASO_MAX_MS],
            r[PruebaObstaculoNativa.R_CM_PROM_X10] / 10.0, r[PruebaObstaculoNativa.R_CM_MAX_X10] / 10.0)
        if (modoPruebaObstaculo && activo) {
            cancelar("Prueba de obstáculo: pasada medida. El carro queda detenido; llévalo de vuelta y toca Iniciar para otra pasada.")
        } else {
            alCambiar()
        }
    }

    /** Al terminar la ruta escribe el resumen de la prueba (una vez por ruta, si hubo pasadas). */
    private fun escribirResumenDePrueba() {
        if (resumenEscrito) {
            return
        }
        resumenEscrito = true
        val r = PruebaObstaculoNativa.resumen()
        if (r[PruebaObstaculoNativa.R_PASADAS] > 0) {
            Log.i(ETIQUETA_PRUEBA, String.format(java.util.Locale.US,
                "RESUMEN pasadas=%d retraso_promedio_ms=%d retraso_max_ms=%d cm_promedio=%.1f cm_max=%.1f",
                r[PruebaObstaculoNativa.R_PASADAS], r[PruebaObstaculoNativa.R_RETRASO_PROM_MS], r[PruebaObstaculoNativa.R_RETRASO_MAX_MS],
                r[PruebaObstaculoNativa.R_CM_PROM_X10] / 10.0, r[PruebaObstaculoNativa.R_CM_MAX_X10] / 10.0))
        }
    }

    /** Borra las cuentas de la prueba (promedios y máximos) para empezar otra serie de pasadas. */
    fun reiniciarPrueba() {
        PruebaObstaculoNativa.iniciarResumen()
        textoPrueba = ""
        Log.i(ETIQUETA_PRUEBA, "cuentas de la prueba reiniciadas")
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
