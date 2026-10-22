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
        const val PERFIL_ESPERA_MS = 1000L    // tiempo máximo para que el carro confirme el perfil AUTO antes de arrancar una ruta
        const val CONFIRMAR_PISO_MS = 10000L  // tiempo para confirmar (tocando Iniciar otra vez) que se usa el último piso aprendido

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
    private var giroPorAnguloAntes = false   // el giro del tick anterior lo hacía el firmware (J11)

    /** Obstáculos simulados (se usan si `camara` es null): izquierda, centro, derecha. */
    val obstaculoSimulado = BooleanArray(3)

    /** Si no es null, los obstáculos salen de esta cámara en vez de los interruptores. */
    var camara: CamaraObstaculos? = null

    private var ultimoCuadroMs = 0L
    private var ultimoEstadoCentro = -1
    private var inicioUltimaRutaMs = 0L           // hora en que empezó la última ruta (el piso aprendido antes de esa hora ya se "gastó")
    private var confirmarUltimoPisoHasta = 0L      // hasta cuándo un segundo toque en Iniciar confirma usar el último piso
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

    /** true mientras se espera (máximo 1 s) la confirmación ACK_AUTO del carro antes de arrancar la ruta. */
    var esperandoPerfil = false
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

    /**
     * Pide la ruta: primero revisa la conexión y la cámara; después manda MODO_AUTO y la ruta SOLO arranca cuando el carro contesta ACK_AUTO
     * (máximo PERFIL_ESPERA_MS = 1 s). Si no contesta, no se arranca y queda un ERROR claro. Devuelve false si no se pudo ni empezar a pedir.
     */
    fun iniciar(ruta: IntArray, nombre: String = ""): Boolean {
        if (activo || esperandoPerfil) {
            return false
        }
        if (logica.conexion != ControlLogica.Conexion.CONECTADO) {
            mensaje = "Sin conexión con el carro"
            alCambiar()
            return false
        }
        // El piso: cada ruta exige haberlo APRENDIDO (botón «Aprender piso») desde la ruta anterior, o confirmar usar el último (tocar Iniciar otra vez en 10 s).
        // Con la fuente de botones no hay cámara y no se pide.
        val camaraPiso = camara
        if (camaraPiso != null) {
            val ahora = SystemClock.elapsedRealtime()
            val aprendidoParaEstaRuta = camaraPiso.pisoAprendidoMs != 0L && camaraPiso.pisoAprendidoMs > inicioUltimaRutaMs
            if (camaraPiso.aprendiendoPiso) {
                mensaje = "Todavía se está aprendiendo el piso (~1 s): espera y vuelve a tocar Iniciar."
                alCambiar()
                return false
            }
            if (!aprendidoParaEstaRuta && ahora > confirmarUltimoPisoHasta) {
                confirmarUltimoPisoHasta = ahora + CONFIRMAR_PISO_MS
                mensaje = "Antes de la ruta hay que APRENDER EL PISO: con el carro quieto sobre el piso de la ruta toca «Aprender piso». " +
                    "O toca Iniciar otra vez (en 10 s) para usar el último piso" +
                    (camaraPiso.informePiso?.let { " aprendido (${it.grupos} color(es), brillo medio ${it.brilloMedio})" } ?: " (todavía no se aprendió ninguno: se usará lo que ve la tira de abajo)") + "."
                Log.i(ETIQUETA, "ruta pendiente: falta aprender el piso o confirmar el último")
                alCambiar()
                return false
            }
        }
        esperandoPerfil = true
        mensaje = "Pidiendo al carro el perfil AUTO (velocidad limitada)..."
        alCambiar()
        logica.confirmarPerfil(ControlLogica.Perfil.AUTO, PERFIL_ESPERA_MS) { confirmado ->
            esperandoPerfil = false
            if (confirmado) {
                comenzarRuta(ruta, nombre)
            } else {
                mensaje = "ERROR: el carro no confirmó el perfil AUTO (ACK_AUTO) en 1 s: la ruta NO arrancó. ¿Está cargado el firmware nuevo en el DevKit? " +
                    "Revisa la conexión y vuelve a tocar Iniciar."
                Log.e(ETIQUETA, "ruta no iniciada: sin ACK_AUTO en ${PERFIL_ESPERA_MS} ms")
                alCambiar()
            }
        }
        return true
    }

    /** Carga la ruta y empieza a seguirla (ya con el perfil AUTO confirmado). Devuelve false (y deja un mensaje) si no se pudo. */
    private fun comenzarRuta(ruta: IntArray, nombre: String = ""): Boolean {
        if (activo) {
            return false
        }
        val predef = RutasPredefinidas.buscarPorIdONombre(nombre)
            ?: RutasPredefinidas.TODAS.firstOrNull { it.celdas.contentEquals(ruta) }
        modoPruebaObstaculo = (ruta === RUTA_PRUEBA_OBSTACULO || predef?.id == RutasPredefinidas.ID_OBSTACULO)
        NavegadorNativa.ponerLargoRectaMm(predef?.largoRectaMm ?: 0)
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
        NavegadorNativa.requiereCamara(if (fuente != null) 1 else 0)   // con cámara no se mueve hasta su primer cuadro (antes, sin cuadros, recorría la ruta a ciegas)
        NavegadorNativa.ponerGiroscopio(if (logica.giroscopioActivo) 1 else 0)   // J11: giros por ángulo medido solo si el interruptor está encendido y el firmware dice que el giroscopio funciona
        NavegadorNativa.ponerSensorDistancia(if (logica.laserActivo) 1 else 0)
        NavegadorNativa.ponerAutoridadLaser(if (logica.autoridadLaser) 1 else 0)
        Log.i("SENS_DIAG", "ruta: giroscopio=${logica.giroscopioActivo} sensor_distancia=${logica.laserActivo}")
        logica.alGiro = { codigo, _, real, ms, ant -> NavegadorNativa.aporteGiro(codigo, real, ms, ant, SystemClock.elapsedRealtime()) }
        logica.alSens = { angulo, mm, banderas -> NavegadorNativa.aporteSens(mm, banderas and 2 != 0, banderas and 4 != 0, angulo, SystemClock.elapsedRealtime()) }
        logica.alImu = { angulo -> NavegadorNativa.aporteImu(angulo, SystemClock.elapsedRealtime()) }
        logica.alTofParo = { mm -> NavegadorNativa.aporteTofParo(mm, SystemClock.elapsedRealtime()) }
        NavegadorNativa.pedirAyuda(if (modoPruebaObstaculo) 0 else 1)   // si no puede esquivar, pide que tomes el control en vez de rendirse (la prueba de obstáculo no)
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
        inicioUltimaRutaMs = SystemClock.elapsedRealtime()
        confirmarUltimoPisoHasta = 0L
        mensaje = ""
        // El retroceso de los reintentos de esquive es CIEGO (no hay cámara atrás): se avisa siempre, y si no se calibró, que usa el del avance.
        val calAtras = leerCalibracion()
        val sinDesp = calAtras.size >= CalibracionNativa.ENTEROS &&
            ((calAtras[CalibracionNativa.CAL_DESP_ADELANTE_IZQ_UM] == 0 && calAtras[CalibracionNativa.CAL_DESP_DERECHA_IZQ_UM] == 0) ||
             (calAtras[CalibracionNativa.CAL_DESP_ADELANTE_DER_UM] == 0 && calAtras[CalibracionNativa.CAL_DESP_DERECHA_DER_UM] == 0))
        if (sinDesp) {
            Log.w(ETIQUETA, "desplazamiento al girar con algun lado sin medir (estimado)")
            mensaje = "Aviso: Izquierda/derecha sin medir: el esquive puede fallar"
        }
        if (calAtras.size > CalibracionNativa.CAL_MEDIDOS && calAtras[CalibracionNativa.CAL_MEDIDOS] and CalibracionNativa.MEDIDO_ATRAS == 0) {
            Log.w(ETIQUETA, "retroceso SIN calibrar: los reintentos de esquive retroceden con los ms por cm del avance")
            mensaje = if (mensaje.isEmpty()) "Aviso: retroceso sin calibrar (usa el valor del avance). Si hay que esquivar un objeto grande el carro puede retroceder hasta 15 cm: deja libre el espacio de atrás."
                      else "$mensaje · Retroceso sin calibrar (usa el valor del avance)."
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
        if (esperandoPerfil) {
            logica.cancelarConfirmacion()   // se canceló mientras esperaba el ACK: no arranca
            esperandoPerfil = false
        }
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
        if (cuadro != null) {
            // Corredor: un obstáculo FUERA de por donde pasa el carro no frena (solo se informa). Solo se confía en el corredor para las zonas que decidió el
            // detector de color; si el modelo de objetos subió una zona (fusión), esa zona cuenta entera.
            val info = fuente?.cuadroTomado
            val c = info?.corredor
            if (c != null && info.resultado.size > ObstaculosNativa.CERCANIA_DERECHA) {
                val det = info.resultado
                fun igual(zona: Int) = cuadro[1 + 2 * zona] == det[1 + 2 * zona] && cuadro[2 + 2 * zona] == det[2 + 2 * zona]
                NavegadorNativa.ponerCorredor(
                    if (igual(0)) c[CorredorNativa.FUERA] else 0, if (igual(1)) c[CorredorNativa.FUERA + 1] else 0, if (igual(2)) c[CorredorNativa.FUERA + 2] else 0,
                    if (igual(0)) c[CorredorNativa.CERCANIA] else 0, if (igual(1)) c[CorredorNativa.CERCANIA + 1] else 0, if (igual(2)) c[CorredorNativa.CERCANIA + 2] else 0)
            } else {
                NavegadorNativa.ponerCorredor(0, 0, 0, 0, 0, 0)
            }
        }
        val comando = NavegadorNativa.tick(ahora, cuadro != null, cuadro ?: IntArray(10))
        val estado = NavegadorNativa.estado()[NavegadorNativa.E_ESTADO]
        registrarEsquive()
        registrarDiagnostico()

        // J11: un giro por ángulo se le pide al firmware una sola vez (GIRAR:<grados>) y no se manda LEFT ni RIGHT mientras lo hace; si el giro cae al método por tiempo hay que mandar el LEFT/RIGHT que faltaba.
        val girarGrados = NavegadorNativa.tomarGiroFw()
        if (girarGrados != 0) {
            logica.girar(girarGrados)
        }
        if (NavegadorNativa.tomarConsultaImu() != 0) {
            Log.i("SENS_DIAG", "se perdió la respuesta de un giro: se pregunta IMU? para saber cuánto giró")
            logica.pedirImu()
        }
        val porAngulo = NavegadorNativa.giroFwActivo() == 1
        val reenviar = giroPorAnguloAntes && !porAngulo && (comando == NavegadorNativa.CMD_LEFT || comando == NavegadorNativa.CMD_RIGHT)
        giroPorAnguloAntes = porAngulo
        if (comando != ultimoComando || reenviar) {
            ultimoComando = comando
            avanzando = (comando == NavegadorNativa.CMD_FORWARD)
            val texto = NavegadorNativa.textoComando(comando)
            Log.i(ETIQUETA, "comando=$texto estado=${NavegadorNativa.textoEstado(estado)}")
            if (comando == NavegadorNativa.CMD_STOP && estado == NavegadorNativa.DETENIDO_OBSTACULO && fuente != null && bloqueoMarcaMs != 0L) {
                frenarPorObstaculo(fuente)
            } else if (!porAngulo) {
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

    /** Copia al log las líneas de diagnóstico del navegador. La etiqueta del log es la primera palabra de la línea: REINC_DIAG (la vuelta a la línea), ESQUIVE_DIAG (qué se vio y qué se hizo al esquivar),
     *  AYUDA_DIAG (toma el control), TURN_DIAG (cada giro con el desplazamiento del centro que se aplicó) y RUTA_DIAG (cada comando de movimiento enviado, con su duración planeada y la real). Solo números y palabras. */
    private fun registrarDiagnostico() {
        while (true) {
            val linea = NavegadorNativa.diagnostico()
            if (linea.isEmpty()) {
                break
            }
            val etiqueta = linea.substringBefore(' ')
            Log.i(if (etiqueta == "CONTRADICCION_LASER_CAMARA" || etiqueta.endsWith("_DIAG")) etiqueta else "ESQUIVE_DIAG", linea)
        }
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
                val nombre = arrayOf("GIRAR", "AVANZAR_DE_LADO", "VOLVER", "MIRAR", "RETROCEDER", "ASENTAR", "PAUSA_PASAR", "PARALELO").getOrElse(fase) { "?" }
                Log.i(ETIQUETA, "esquivar fase=$nombre reintentos=$reintentos rumbo=${e[NavegadorNativa.E_RUMBO]} x_mm=${e[NavegadorNativa.E_X_MM]} y_mm=${e[NavegadorNativa.E_Y_MM]}")
            }
        }
    }

    /** Lleva la racha de cuadros con un obstáculo cerca (cercanía 2 o más en cualquier zona; 1 también al centro con el video del carro) y guarda cómo era el PRIMERO. */
    private fun seguirBloqueo(info: CuadroAnalizado?, cuadro: IntArray) {
        var zonaMasCerca = -1
        var cercania = 0
        // Con el video del carro el navegador también frena con cercanía 1 al CENTRO (frenar desde lejos): esa racha también se mide.
        val minimoCentro = if (camara?.esVideoCarro == true) 1 else 2
        for (zona in 0..2) {
            val minimo = if (zona == 1) minimoCentro else 2
            val fueraCorredor = info?.corredor?.let { it[CorredorNativa.FUERA + zona] == 1 } ?: false   // un obstáculo fuera del corredor no frena: no es una pasada
            if (!fueraCorredor && cuadro[1 + 2 * zona] == 1 && cuadro[2 + 2 * zona] >= minimo && cuadro[2 + 2 * zona] > cercania) {
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
        val modoVideo = fuente.modoVideo
        logica.pararConRespuesta { msRespuesta ->
            manejador.post { terminarPasada(marca, msDetectar, msStop, msRespuesta.toInt(), cercania, zona, origen, latenciaVideo, modoVideo) }
        }
        msStop = (SystemClock.elapsedRealtime() - marca).toInt()   // desde que llegó el cuadro hasta que el STOP salió
    }

    private var msStop = 0

    /** Cierra una pasada: calcula el retraso y los cm (en C), lo escribe en el log y en pantalla, y cancela la ruta de prueba. */
    private fun terminarPasada(marca: Long, msDetectar: Double, msStopEnviado: Int, msRespuesta: Int, cercania: Int, zona: Int, origen: String, latenciaVideo: Int, modoVideo: String) {
        val cal = leerCalibracion()
        val msPorCmX10 = if (cal.size > CalibracionNativa.CAL_MS_POR_CM_X10) cal[CalibracionNativa.CAL_MS_POR_CM_X10] else 517
        val frenadoMm = if (cal.size > CalibracionNativa.CAL_FRENADO_MM) cal[CalibracionNativa.CAL_FRENADO_MM] else 14
        val retraso = PruebaObstaculoNativa.retrasoMs(msStopEnviado, msRespuesta, latenciaVideo)
        val cmX10 = PruebaObstaculoNativa.cmX10(retraso, msPorCmX10, frenadoMm)
        PruebaObstaculoNativa.agregar(retraso, cmX10)
        val linea = String.format(java.util.Locale.US,
            "marca_cuadro_ms=%d ms_detectar=%.1f ms_stop=%d ms_respuesta=%d cercania=%d zona=%s fuente=%s modo_video=%s latencia_video_ms=%d retraso_total_ms=%d cm_recorridos=%.1f (a %.1f ms/cm + frenado %.1f cm)",
            marca, msDetectar, msStopEnviado, msRespuesta, cercania, NOMBRE_ZONA[zona], origen, modoVideo, latenciaVideo, retraso, cmX10 / 10.0, msPorCmX10 / 10.0, frenadoMm / 10.0)
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
