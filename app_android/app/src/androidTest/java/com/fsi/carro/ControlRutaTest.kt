/*
 * ControlRutaTest.kt — Pulsar botones de la app por resource-id durante las pruebas físicas J14.
 *
 * GUÍA DE LECTURA
 *  1. Es una prueba de instrumentación (src/androidTest): se instala aparte y NO forma parte de la app.
 *  2. UiAutomator busca la vista por su resource-id, no por coordenadas fijas, y la pulsa con click().
 *     Así no hace falta capturar la pantalla de la ruta (que muestra la cámara real).
 *  3. La pantalla de ruta se refresca cada 250 ms y nunca queda "idle"; por eso se pone el tiempo de
 *     espera de idle en 0. Sin eso, uiautomator dump falla con "could not get idle state".
 *  4. Se pasa la vista con: adb shell am instrument ... -e vista ej_iniciar
 *  5. La prueba solo comprueba que la vista aparezca y la pulsa; el estado real se lee en logcat
 *     (PANTALLA, PISO_DIAG, RUTA_DIAG, etc.).
 */
package com.fsi.carro

import android.content.Intent
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Configurator
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ControlRutaTest {

    /** El dispositivo con la espera de "idle" apagada: la pantalla de ruta se refresca todo el tiempo. */
    private fun dispositivo(): UiDevice {
        val d = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        Configurator.getInstance().setWaitForIdleTimeout(0)
        return d
    }

    /** Paquete de la vista que se busca (por defecto el de la app; "android" para los botones de un diálogo). */
    private fun paquete(): String =
        InstrumentationRegistry.getArguments().getString("paquete") ?: "com.fsi.carro"

    /** Nombre del recurso que se busca (por ejemplo ej_iniciar, ej_cancelar, ej_paro). */
    private fun vista(): String =
        InstrumentationRegistry.getArguments().getString("vista") ?: "ej_iniciar"

    /**
     * Si se pasa -e ruta recta_150, abre la pantalla de ejecutar ruta antes de buscar la vista.
     * En los toques siguientes (confirmar, parar) se omite para no reiniciar la preparación.
     */
    private fun lanzarRutaSiSePide() {
        val ruta = InstrumentationRegistry.getArguments().getString("ruta")
        if (ruta.isNullOrBlank()) {
            return
        }
        val contexto = InstrumentationRegistry.getInstrumentation().targetContext
        val intento = Intent(contexto, MainActivity::class.java).apply {
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
            putExtra("pantalla", "ejecutar")
            putExtra("ruta_predefinida", ruta)
        }
        contexto.startActivity(intento)
        Thread.sleep(4000)   // deja que se arme la pantalla y abra el video
    }

    /** Si se pasa -e actividad sensores|calibrar, abre esa pantalla antes de buscar la vista. */
    private fun lanzarActividadSiSePide() {
        val actividad = InstrumentationRegistry.getArguments().getString("actividad")
        if (actividad.isNullOrBlank()) {
            return
        }
        val contexto = InstrumentationRegistry.getInstrumentation().targetContext
        val clase = when (actividad) {
            "sensores" -> SensoresActivity::class.java
            "calibrar" -> CalibrarGiroActivity::class.java
            else -> return
        }
        val intento = Intent(contexto, clase).apply {
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        }
        contexto.startActivity(intento)
        Thread.sleep(3000)
    }

    /** Espera a que la vista exista (hasta 8 s) y la pulsa. Con -e fijar true|false solo la pulsa si hace falta. */
    @Test
    fun pulsar() {
        lanzarActividadSiSePide()
        lanzarRutaSiSePide()
        val d = dispositivo()
        val nombre = vista()
        val objeto = d.wait(Until.findObject(By.res(paquete(), nombre)), 8000)
        assertNotNull("No apareció la vista $nombre", objeto)
        val marcadoAntes = runCatching { if (objeto!!.isCheckable) objeto.isChecked else null }.getOrNull()
        val fijar = InstrumentationRegistry.getArguments().getString("fijar")
        val hayQuePulsar = fijar == null || marcadoAntes == null || marcadoAntes != (fijar == "true")
        Log.i("TEST_RUTA", "pulsar paquete=${paquete()} vista=$nombre bounds=${objeto!!.visibleBounds} habilitada=${objeto.isEnabled} clickable=${objeto.isClickable} marcado=$marcadoAntes fijar=$fijar pulsar=$hayQuePulsar")
        if (hayQuePulsar) {
            objeto.click()
            Thread.sleep(400)   // deja que la app procese el toque antes de que termine la prueba
        }
        val marcadoDespues = runCatching { if (objeto.isCheckable) objeto.isChecked else null }.getOrNull()
        Log.i("TEST_RUTA", "despues vista=$nombre marcado=$marcadoDespues")
    }

    /** Espera a que la vista exista y deja en logcat si está visible y sus límites. No la pulsa. */
    @Test
    fun esperar() {
        lanzarRutaSiSePide()
        val d = dispositivo()
        val nombre = vista()
        val objeto = d.wait(Until.findObject(By.res(paquete(), nombre)), 8000)
        assertNotNull("No apareció la vista $nombre", objeto)
        Log.i("TEST_RUTA", "visible paquete=${paquete()} vista=$nombre bounds=${objeto!!.visibleBounds} habilitada=${objeto.isEnabled} clickable=${objeto.isClickable}")
    }

    /**
     * Abre la ruta, espera a que la conexión se estabilice, pulsa ej_iniciar y deja en logcat el texto
     * de los controles principales. Sirve para saber si la preparación empezó o si hay un aviso.
     */
    @Test
    fun iniciarYLeer() {
        lanzarRutaSiSePide()
        Thread.sleep(4000)   // además de los 4 s de lanzarRutaSiSePide: deja conectar el carro y el video
        val d = dispositivo()
        val boton = d.wait(Until.findObject(By.res("com.fsi.carro", "ej_iniciar")), 8000)
        assertNotNull("No apareció ej_iniciar", boton)
        boton!!.click()
        Thread.sleep(3000)
        for (id in listOf("ej_estado", "ej_ruta_nombre", "ej_progreso", "ej_aviso", "ej_iniciar", "ej_cancelar", "ej_paro")) {
            val o = d.findObject(By.res("com.fsi.carro", id))
            Log.i("TEST_RUTA", "texto $id=${o?.text} bounds=${o?.visibleBounds} habilitada=${o?.isEnabled}")
        }
        val contexto = InstrumentationRegistry.getInstrumentation().targetContext
        val archivo = java.io.File(contexto.cacheDir, "arbol_ui.xml")
        d.dumpWindowHierarchy(archivo)
        Log.i("TEST_RUTA", "arbol=${archivo.absolutePath} bytes=${archivo.length()}")
    }

    /** Vuelca la jerarquía de la pantalla a un archivo de la app (solo texto; no es una captura). */
    @Test
    fun volcar() {
        lanzarRutaSiSePide()
        val d = dispositivo()
        val contexto = InstrumentationRegistry.getInstrumentation().targetContext
        val archivo = java.io.File(contexto.cacheDir, "arbol_ui.xml")
        d.dumpWindowHierarchy(archivo)
        Log.i("TEST_RUTA", "arbol=${archivo.absolutePath} bytes=${archivo.length()}")
    }

    /** Texto de una vista y de sus hijos (los avisos de la app son un contenedor con varios TextView). */
    private fun textoConHijos(d: UiDevice, id: String): String {
        val objeto = d.findObject(By.res("com.fsi.carro", id)) ?: return ""
        val texto = StringBuilder()
        fun recorrer(x: androidx.test.uiautomator.UiObject2) {
            val t = runCatching { x.text }.getOrNull()
            if (!t.isNullOrBlank()) {
                texto.append(t).append(" | ")
            }
            for (hijo in runCatching { x.children }.getOrDefault(emptyList())) {
                recorrer(hijo)
            }
        }
        recorrer(objeto)
        return texto.toString()
    }

    /** Lo que sabe el navegador en C: estado, posición estimada, objetivo, esquivas y comando. */
    private fun estadoNav(): String {
        return try {
            val e = NavegadorNativa.estado()
            "estado=${e[NavegadorNativa.E_ESTADO]} x_mm=${e[NavegadorNativa.E_X_MM]} y_mm=${e[NavegadorNativa.E_Y_MM]} rumbo=${e[NavegadorNativa.E_RUMBO]} objetivo=${e[NavegadorNativa.E_OBJETIVO]}/${e[NavegadorNativa.E_PUNTOS]} esquivadas=${e[NavegadorNativa.E_ESQUIVADAS]} comando=${e[NavegadorNativa.E_COMANDO]} motivo_error=${e[NavegadorNativa.E_MOTIVO_ERROR]}"
        } catch (t: Throwable) {
            "nav_error=${t.javaClass.simpleName}:${t.message}"
        }
    }

    /** Resume el estado visible de la pantalla de ruta para el registro. */
    private fun resumen(d: UiDevice): String {
        val partes = StringBuilder()
        for (id in listOf("ej_estado", "ej_ruta_nombre", "ej_progreso", "ej_iniciar", "ej_cancelar", "ej_paro",
                          "ej_resumen_chip", "ej_resumen_duracion", "ej_resumen_esquivadas", "ej_resumen_giros")) {
            val o = d.findObject(By.res("com.fsi.carro", id))
            if (o != null) {
                partes.append(id).append("=[").append(runCatching { o.text }.getOrNull() ?: "").append("] ")
            }
        }
        partes.append("aviso=[").append(textoConHijos(d, "ej_aviso")).append("] ")
        partes.append("nav=[").append(estadoNav()).append("]")
        return partes.toString()
    }

    /**
     * Ejecuta una ruta completa: abre la pantalla, pulsa «Iniciar ruta», espera «Confirmar y salir»,
     * confirma y espera a que termine. Todo en la misma prueba porque al terminar la instrumentación
     * se cierra el proceso de la app.
     *
     * Argumentos: -e ruta recta_150 -e confirmar true|false -e espera_preparacion_s 20 -e espera_ruta_s 60
     *             -e espera_video_s 15 (ampliable si la cámara tarda en soltar la conexión anterior)
     */
    @Test
    fun ejecutarRuta() {
        val args = InstrumentationRegistry.getArguments()
        val ruta = args.getString("ruta") ?: "recta_150"
        val confirmar = args.getString("confirmar") != "false"
        val esperaPreparacionMs = (args.getString("espera_preparacion_s")?.toLongOrNull() ?: 20L) * 1000L
        val esperaRutaMs = (args.getString("espera_ruta_s")?.toLongOrNull() ?: 60L) * 1000L
        // La cámara del carro puede tardar en soltar la conexión anterior; se puede ampliar por argumento.
        val esperaVideoMs = (args.getString("espera_video_s")?.toLongOrNull() ?: 15L) * 1000L

        lanzarRutaSiSePide()
        Thread.sleep(3000)   // deja conectar el carro
        val d = dispositivo()
        val boton = d.wait(Until.findObject(By.res("com.fsi.carro", "ej_iniciar")), 10000)
        assertNotNull("No apareció ej_iniciar", boton)
        val hastaVideo = System.currentTimeMillis() + esperaVideoMs
        while (System.currentTimeMillis() < hastaVideo && SesionVideo.fps <= 0.5) {
            Thread.sleep(500)   // espera el primer cuadro del video del carro
        }
        Log.i("TEST_RUTA", "video fps=${SesionVideo.fps} estado=${SesionVideo.estado} antes: ${resumen(d)}")
        boton!!.click()

        val hastaPreparacion = System.currentTimeMillis() + esperaPreparacionMs
        var confirmarObj: androidx.test.uiautomator.UiObject2? = null
        while (System.currentTimeMillis() < hastaPreparacion) {
            Thread.sleep(500)
            val estado = runCatching { d.findObject(By.res("com.fsi.carro", "ej_estado"))?.text }.getOrNull() ?: ""
            val iniciar = d.findObject(By.res("com.fsi.carro", "ej_iniciar"))
            val textoIniciar = runCatching { iniciar?.text }.getOrNull() ?: ""
            if (textoIniciar.contains("Confirmar", ignoreCase = true)) {
                confirmarObj = iniciar
                break
            }
            if (estado.contains("Listo para empezar") && textoIniciar.contains("Iniciar", ignoreCase = true)) {
                break   // la preparación falló y volvió al estado inicial
            }
            if (estado.contains("Error", ignoreCase = true) || estado.contains("Terminado", ignoreCase = true)) {
                break
            }
        }
        Log.i("TEST_RUTA", "preparacion: ${resumen(d)}")
        if (confirmarObj == null) {
            Log.i("TEST_RUTA", "sin confirmar: la preparación no llegó a «Confirmar y salir»")
            return
        }
        if (!confirmar) {
            Log.i("TEST_RUTA", "listo para salir; no se confirma (confirmar=false)")
            return
        }
        confirmarObj.click()

        // B9: a 1 s de que el carro empiece a avanzar, se corta el WiFi 3 s y SIEMPRE se reactiva.
        val corteWifi = args.getString("corte_wifi") == "true"
        val tConfirmar = android.os.SystemClock.elapsedRealtime()
        var corteHecho = false
        var reactivado = false
        var tCorte = 0L
        var tParo = 0L
        var paradoDesde = 0L
        var rutaArrancada = false
        try {
            val hastaRuta = System.currentTimeMillis() + esperaRutaMs
            var ultimoLog = 0L
            while (System.currentTimeMillis() < hastaRuta) {
                Thread.sleep(200)
                val estado = runCatching { d.findObject(By.res("com.fsi.carro", "ej_estado"))?.text }.getOrNull() ?: ""
                val nav = runCatching { NavegadorNativa.estado() }.getOrNull()
                val comando = nav?.get(NavegadorNativa.E_COMANDO) ?: -1
                val navEstado = nav?.get(NavegadorNativa.E_ESTADO) ?: -1
                val ahora = android.os.SystemClock.elapsedRealtime()

                if (corteWifi && !corteHecho && comando == NavegadorNativa.CMD_FORWARD) {
                    Thread.sleep(1000)   // 1 s desde que empezó a avanzar
                    tCorte = android.os.SystemClock.elapsedRealtime()
                    Log.i("TEST_RUTA", "corte_wifi=disable t_desde_confirmar=${tCorte - tConfirmar}ms nav=${estadoNav()}")
                    val salida = d.executeShellCommand("svc wifi disable")
                    Log.i("TEST_RUTA", "corte_wifi=disable salida=[$salida]")
                    corteHecho = true
                    Thread.sleep(3000)
                    val salida2 = d.executeShellCommand("svc wifi enable")
                    reactivado = true
                    Log.i("TEST_RUTA", "corte_wifi=enable t_desde_corte=${android.os.SystemClock.elapsedRealtime() - tCorte}ms salida=[$salida2]")
                }
                if (corteHecho && tParo == 0L &&
                    (comando == NavegadorNativa.CMD_STOP || estado.contains("Detenido", ignoreCase = true) ||
                     estado.contains("Error", ignoreCase = true) || estado.contains("Terminado", ignoreCase = true))) {
                    tParo = android.os.SystemClock.elapsedRealtime()
                    Log.i("TEST_RUTA", "paro_tras_corte_ms=${tParo - tCorte} nav=${estadoNav()}")
                }
                if (ahora - ultimoLog >= 1000) {
                    ultimoLog = ahora
                    Log.i("TEST_RUTA", "en_ruta estado=[$estado] ${estadoNav()}")
                }
                // «Detenido» puede ser un estado pasajero entre dos tramos (p. ej. al empezar un giro):
                // no se corta por el texto. Se corta al terminar/errar o cuando la ruta lleva 1.5 s parada.
                if (navEstado == NavegadorNativa.TERMINADO || navEstado == NavegadorNativa.ERROR ||
                    estado.contains("Terminado", ignoreCase = true) || estado.contains("Error", ignoreCase = true)) {
                    break
                }
                val semiActivo = runCatching { SesionCarro.semi.activo }.getOrDefault(true)
                if (semiActivo || navEstado == NavegadorNativa.SIGUIENDO ||
                    estado.contains("Avanzando", ignoreCase = true) || estado.contains("Girando", ignoreCase = true) ||
                    estado.contains("Obstáculo", ignoreCase = true)) {
                    rutaArrancada = true   // la cuenta 3-2-1 no cuenta como ruta parada
                }
                if (rutaArrancada && !semiActivo) {
                    if (paradoDesde == 0L) {
                        paradoDesde = ahora
                    } else if (ahora - paradoDesde >= 1500) {
                        break
                    }
                } else {
                    paradoDesde = 0L
                }
            }
        } finally {
            if (corteHecho && !reactivado) {
                val salida = d.executeShellCommand("svc wifi enable")
                Log.i("TEST_RUTA", "corte_wifi=enable_final salida=[$salida]")
            }
            Log.i("TEST_RUTA", "final: ${resumen(d)}")
        }
    }
}
