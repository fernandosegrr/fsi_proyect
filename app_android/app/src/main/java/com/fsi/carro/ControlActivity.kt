/*
 * ControlActivity.kt — Pantalla de control del carro (botones + estado de la conexión).
 *
 * GUÍA DE LECTURA
 *  1. Un campo de texto con la dirección del carro (por defecto ws://192.168.4.1/ws) y el
 *     botón "Conectar" para aplicar una dirección nueva.
 *  2. Cinco botones grandes: adelante, atrás, izquierda, derecha y STOP. Un botón de
 *     movimiento manda su comando mientras se mantiene presionado; al soltar manda STOP.
 *  3. Toda la lógica (qué enviar, cada cuánto, reconexión, PING) está en ControlLogica;
 *     aquí solo se conectan los botones y se muestra el estado.
 *  4. onResume abre la conexión; onPause envía STOP y la cierra.
 *  5. El botón de arriba cambia entre Manual (los botones) y Semiautónomo (una ruta de prueba
 *     que sigue el navegador en C, con obstáculos simulados; ver ModoSemiautonomo).
 *  6. Para pruebas se puede abrir por adb con otra dirección:
 *       adb shell am start -n com.fsi.carro/.ControlActivity --es direccion ws://localhost:8080/ws
 */
package com.fsi.carro

import android.app.Activity
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.util.Log
import android.widget.Button
import android.widget.EditText
import android.widget.TextView

class ControlActivity : Activity() {

    private lateinit var campoDireccion: EditText
    private lateinit var textoEstado: TextView
    private lateinit var logica: ControlLogica
    private lateinit var semi: ModoSemiautonomo
    private lateinit var panelManual: View
    private lateinit var panelSemi: View
    private lateinit var textoNav: TextView
    private lateinit var botonModo: Button
    private var modoSemi = false
    private var rutaElegida = ModoSemiautonomo.RUTA_CUADRADO
    private var nombreRuta = "Cuadrado"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_control)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        campoDireccion = findViewById(R.id.campo_direccion)
        textoEstado = findViewById(R.id.texto_estado)
        intent.getStringExtra("direccion")?.let { campoDireccion.setText(it) }

        logica = ControlLogica(EnlaceOkHttp()) { mostrarEstado() }
        semi = ModoSemiautonomo(logica) { mostrarEstado() }
        panelManual = findViewById(R.id.panel_manual)
        panelSemi = findViewById(R.id.panel_semi)
        textoNav = findViewById(R.id.texto_nav)
        botonModo = findViewById(R.id.boton_modo)
        botonModo.setOnClickListener { cambiarModo(!modoSemi) }
        findViewById<Button>(R.id.boton_ruta_cuadrado).setOnClickListener {
            rutaElegida = ModoSemiautonomo.RUTA_CUADRADO
            nombreRuta = "Cuadrado"
            mostrarEstado()
        }
        findViewById<Button>(R.id.boton_ruta_zigzag).setOnClickListener {
            rutaElegida = ModoSemiautonomo.RUTA_ZIGZAG
            nombreRuta = "Zigzag"
            mostrarEstado()
        }
        findViewById<Button>(R.id.boton_iniciar_ruta).setOnClickListener { semi.iniciar(rutaElegida) }
        findViewById<Button>(R.id.boton_cancelar_ruta).setOnClickListener { semi.cancelar() }
        findViewById<Button>(R.id.boton_obstaculo_izquierda).setOnClickListener { semi.alternarObstaculo(0) }
        findViewById<Button>(R.id.boton_obstaculo_centro).setOnClickListener { semi.alternarObstaculo(1) }
        findViewById<Button>(R.id.boton_obstaculo_derecha).setOnClickListener { semi.alternarObstaculo(2) }

        findViewById<Button>(R.id.boton_conectar).setOnClickListener {
            logica.iniciar(campoDireccion.text.toString().trim())
        }
        enlazarMovimiento(R.id.boton_adelante, "FORWARD")
        enlazarMovimiento(R.id.boton_atras, "REVERSE")
        enlazarMovimiento(R.id.boton_izquierda, "LEFT")
        enlazarMovimiento(R.id.boton_derecha, "RIGHT")
        findViewById<View>(R.id.boton_stop).setOnTouchListener { _, evento ->
            if (evento.actionMasked == MotionEvent.ACTION_DOWN) {
                logica.parar()
            }
            true
        }
        mostrarEstado()
    }

    override fun onResume() {
        super.onResume()
        // PENDIENTE HARDWARE: aquí (antes de abrir el WebSocket) irá
        // ConnectivityManager.bindProcessToNetwork(redDelCarro) para forzar que el tráfico
        // salga por el WiFi del carro (192.168.4.1) aunque esa red no tenga Internet y el
        // teléfono prefiera los datos móviles. Se programa y prueba con el carro real.
        logica.iniciar(campoDireccion.text.toString().trim())
    }

    override fun onPause() {
        semi.cancelar("Pantalla en segundo plano")   // STOP inmediato y fin de la ruta
        logica.detener()                              // STOP y cierre de la conexión
        super.onPause()
    }

    /** Cambia entre Manual y Semiautónomo (al cambiar se cancela cualquier ruta en marcha). */
    private fun cambiarModo(semiautonomo: Boolean) {
        semi.cancelar("Cambio de modo")
        modoSemi = semiautonomo
        panelManual.visibility = if (modoSemi) View.GONE else View.VISIBLE
        panelSemi.visibility = if (modoSemi) View.VISIBLE else View.GONE
        Log.i("NAV", "modo=${if (modoSemi) "SEMIAUTONOMO" else "MANUAL"}")
        mostrarEstado()
    }

    /** Un botón de movimiento: presionado = comando repetido; soltado (o cancelado) = STOP. */
    private fun enlazarMovimiento(id: Int, comando: String) {
        findViewById<View>(id).setOnTouchListener { vista, evento ->
            when (evento.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    vista.isPressed = true
                    logica.presionar(comando)
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    vista.isPressed = false
                    logica.soltar(comando)
                }
            }
            true
        }
    }

    /** Muestra Conectado / Desconectado / Reconectando y la latencia PING-PONG. */
    private fun mostrarEstado() {
        val estado = when (logica.conexion) {
            ControlLogica.Conexion.CONECTADO -> getString(R.string.estado_conectado)
            ControlLogica.Conexion.RECONECTANDO -> getString(R.string.estado_reconectando)
            ControlLogica.Conexion.DESCONECTADO -> getString(R.string.estado_desconectado)
        }
        val latencia = logica.latenciaMs?.let { "$it ms" } ?: "—"
        textoEstado.text = getString(R.string.estado_con_latencia, estado, latencia)
        mostrarSemiautonomo()
    }

    /** Textos del panel semiautónomo: estado del navegador y los interruptores de obstáculos. */
    private fun mostrarSemiautonomo() {
        botonModo.text = getString(if (modoSemi) R.string.modo_semi else R.string.modo_manual)
        val si = getString(R.string.si)
        val no = getString(R.string.no)
        findViewById<Button>(R.id.boton_obstaculo_izquierda).text = getString(R.string.obstaculo_izquierda, if (semi.obstaculoSimulado[0]) si else no)
        findViewById<Button>(R.id.boton_obstaculo_centro).text = getString(R.string.obstaculo_centro, if (semi.obstaculoSimulado[1]) si else no)
        findViewById<Button>(R.id.boton_obstaculo_derecha).text = getString(R.string.obstaculo_derecha, if (semi.obstaculoSimulado[2]) si else no)
        findViewById<Button>(R.id.boton_iniciar_ruta).isEnabled = !semi.activo
        val e = NavegadorNativa.estado()
        textoNav.text = "Ruta: $nombreRuta\n" +
            "Estado: ${NavegadorNativa.textoEstado(e[NavegadorNativa.E_ESTADO])}\n" +
            "Comando: ${NavegadorNativa.textoComando(e[NavegadorNativa.E_COMANDO])}\n" +
            "Punto: ${e[NavegadorNativa.E_OBJETIVO]} de ${e[NavegadorNativa.E_PUNTOS] - 1}   Esquivadas: ${e[NavegadorNativa.E_ESQUIVADAS]}\n" +
            "Posición estimada: x=${e[NavegadorNativa.E_X_MM] / 10} cm  y=${e[NavegadorNativa.E_Y_MM] / 10} cm  rumbo=${e[NavegadorNativa.E_RUMBO]}°\n" +
            semi.mensaje
    }
}
