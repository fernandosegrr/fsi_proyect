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
 *  5. Para pruebas se puede abrir por adb con otra dirección:
 *       adb shell am start -n com.fsi.carro/.ControlActivity --es direccion ws://localhost:8080/ws
 */
package com.fsi.carro

import android.app.Activity
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.TextView

class ControlActivity : Activity() {

    private lateinit var campoDireccion: EditText
    private lateinit var textoEstado: TextView
    private lateinit var logica: ControlLogica

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_control)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        campoDireccion = findViewById(R.id.campo_direccion)
        textoEstado = findViewById(R.id.texto_estado)
        intent.getStringExtra("direccion")?.let { campoDireccion.setText(it) }

        logica = ControlLogica(EnlaceOkHttp()) { mostrarEstado() }

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
        logica.detener()   // STOP y cierre de la conexión
        super.onPause()
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
    }
}
