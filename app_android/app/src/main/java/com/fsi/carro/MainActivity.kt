/*
 * MainActivity.kt — Única pantalla de la app.
 *
 * GUÍA DE LECTURA
 *  1. onCreate() busca las vistas del layout (res/layout/activity_main.xml).
 *  2. Cuando el dedo cambia la ruta, CuadriculaView avisa y se actualiza el contador.
 *  3. "Calcular" le pasa los puntos al código en C (TrayectoriaNativa) y muestra
 *     el texto que devuelve; Kotlin no interpreta nada.
 *  4. "Borrar" limpia la ruta y el resultado.
 *  5. "Visión" abre VisionActivity (cámara + detector de obstáculos con umbrales ajustables).
 *  5b. "Calibración del carro" abre CalibracionActivity (mide arranque, avance y giro con el carro).
 *  6. "Control del carro" abre ControlActivity (botones + WebSocket hacia el carro).
 *  7. "Benchmark del detector" abre BenchmarkActivity (mide el detector de objetos).
 */
package com.fsi.carro

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.TextView

class MainActivity : Activity() {

    private lateinit var cuadricula: CuadriculaView
    private lateinit var contador: TextView
    private lateinit var resultado: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        cuadricula = findViewById(R.id.cuadricula)
        contador = findViewById(R.id.contador)
        resultado = findViewById(R.id.resultado)

        cuadricula.alCambiarRuta = { mostrarContador() }
        findViewById<Button>(R.id.boton_calcular).setOnClickListener { calcular() }
        findViewById<Button>(R.id.boton_borrar).setOnClickListener { borrar() }
        findViewById<Button>(R.id.boton_vision).setOnClickListener {
            startActivity(Intent(this, VisionActivity::class.java))
        }
        findViewById<Button>(R.id.boton_calibracion).setOnClickListener {
            startActivity(Intent(this, CalibracionActivity::class.java))
        }
        findViewById<Button>(R.id.boton_control).setOnClickListener {
            startActivity(Intent(this, ControlActivity::class.java))
        }
        findViewById<Button>(R.id.boton_benchmark).setOnClickListener {
            startActivity(Intent(this, BenchmarkActivity::class.java))
        }

        mostrarContador()
    }

    /** Muestra cuántos puntos tiene la ruta y cuántos se permiten. */
    private fun mostrarContador() {
        contador.text = "Puntos: ${cuadricula.cantidadPuntos()} / ${TrayectoriaNativa.maxPuntos()}"
    }

    /** Llama al módulo en C y muestra su texto (pasos o mensaje de error). */
    private fun calcular() {
        resultado.text = TrayectoriaNativa.calcular(cuadricula.puntosComoArreglo())
    }

    /** Borra la ruta dibujada y el resultado anterior. */
    private fun borrar() {
        cuadricula.borrar()
        resultado.text = getString(R.string.resultado_inicial)
    }
}
