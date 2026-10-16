/*
 * RutasFragment.kt — La pestaña RUTAS: dibujar la ruta, probarla SIN el carro (simulador) y ejecutarla.
 *
 * GUÍA DE LECTURA
 *  1. EDITAR: EditorRutaView traduce los toques y RutaEditorNativa (C, ruta_editor.c) hace las cuentas: agregar, mover, borrar punto, limpiar, deshacer y rehacer,
 *     plantillas y resumen (largo, tiempo con la calibración guardada, giros y advertencias). Cada cambio guarda el BORRADOR solo (AlmacenRutas) y se recupera al volver.
 *  2. GUARDAR / ABRIR: rutas con nombre en el teléfono (AlmacenRutas). El nombre se valida en C (re_nombre_valido).
 *  3. SIMULAR: SimRutaNativa corre el navegador REAL (navegador.c) con un carro virtual y la calibración guardada (o valores de ejemplo, con aviso); la cuadrícula anima el
 *     recorrido. Con «Poner obstáculos virtuales» se tocan celdas para ver cómo esquiva. «Repetir con ruido de ±20°» repite la ruta 8 veces con el giro más o menos
 *     errado para mostrar la dispersión de los finales.
 *  4. EJECUTAR: abre EjecutarRutaFragment con la ruta (ya con los pasos intermedios). El HISTORIAL de ejecuciones se ve al final.
 */
package com.fsi.carro

import android.os.Bundle
import android.text.format.DateFormat
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.materialswitch.MaterialSwitch
import com.google.android.material.textfield.TextInputEditText
import com.google.android.material.textfield.TextInputLayout
import java.util.Date
import java.util.Locale

class RutasFragment : Fragment(R.layout.fragment_rutas) {

    private lateinit var almacen: AlmacenRutas
    private lateinit var cuadricula: EditorRutaView
    private lateinit var nombre: TextView
    private lateinit var chipPuntos: ChipEstado
    private lateinit var ayuda: TextView
    private lateinit var largo: TextView
    private lateinit var tiempo: TextView
    private lateinit var giros: TextView
    private lateinit var advertencia: TextView
    private lateinit var botonDeshacer: MaterialButton
    private lateinit var botonRehacer: MaterialButton
    private lateinit var tarjetaSim: MaterialCardView
    private lateinit var simChip: ChipEstado
    private lateinit var simTexto: TextView
    private lateinit var simComandos: TextView
    private lateinit var historial: LinearLayout
    private lateinit var historialVacio: View
    private lateinit var botonSimular: MaterialButton

    private var nombreActual: String? = null

    override fun onViewCreated(vista: View, guardado: Bundle?) {
        val ctx = requireContext()
        almacen = AlmacenRutas(ctx)
        cuadricula = vista.findViewById(R.id.rutas_cuadricula)
        nombre = vista.findViewById(R.id.rutas_nombre)
        chipPuntos = vista.findViewById(R.id.rutas_chip_puntos)
        ayuda = vista.findViewById(R.id.rutas_ayuda)
        largo = vista.findViewById(R.id.rutas_largo)
        tiempo = vista.findViewById(R.id.rutas_tiempo)
        giros = vista.findViewById(R.id.rutas_giros)
        advertencia = vista.findViewById(R.id.rutas_advertencia)
        botonDeshacer = vista.findViewById(R.id.rutas_deshacer)
        botonRehacer = vista.findViewById(R.id.rutas_rehacer)
        tarjetaSim = vista.findViewById(R.id.rutas_tarjeta_sim)
        simChip = vista.findViewById(R.id.rutas_sim_chip)
        simTexto = vista.findViewById(R.id.rutas_sim_texto)
        simComandos = vista.findViewById(R.id.rutas_sim_comandos)
        historial = vista.findViewById(R.id.rutas_historial)
        historialVacio = vista.findViewById(R.id.rutas_historial_vacio)
        botonSimular = vista.findViewById(R.id.rutas_simular)
        vista.findViewById<TextView>(R.id.vacio_texto).setText(R.string.rutas_historial_vacio)

        // Al abrir por primera vez en esta ejecución de la app, se recupera el borrador.
        if (guardado == null && RutaEditorNativa.vertices().isEmpty()) {
            almacen.recuperarBorrador()
        }

        cuadricula.alCambiar = {
            quitarSimulacion()
            almacen.guardarBorrador()
            nombreActual = null
            actualizar()
        }
        cuadricula.alError = { codigo ->
            Haptica.obstaculo(ctx)
            ayuda.text = RutaEditorNativa.textoError(codigo)
            ayuda.setTextColor(Diseno.colorDe(ctx, Nivel.AVISO))
        }
        cuadricula.alAnimar = { muestra -> alAnimarSimulacion(muestra) }

        botonDeshacer.setOnClickListener { hacer { RutaEditorNativa.deshacer() } }
        botonRehacer.setOnClickListener { hacer { RutaEditorNativa.rehacer() } }
        vista.findViewById<View>(R.id.rutas_borrar_punto).setOnClickListener {
            if (cuadricula.seleccionado < 0) {
                avisarAyuda(getString(R.string.rutas_sin_punto))
            } else {
                hacer { cuadricula.borrarSeleccionado() }
            }
        }
        vista.findViewById<View>(R.id.rutas_limpiar).setOnClickListener {
            if (RutaEditorNativa.vertices().isEmpty()) {
                return@setOnClickListener
            }
            Dialogos.confirmar(ctx, getString(R.string.rutas_limpiar_titulo), getString(R.string.rutas_limpiar_texto), getString(R.string.rutas_limpiar_si)) {
                hacer { RutaEditorNativa.limpiar(); 0 }
            }
        }
        for ((id, plantilla) in listOf(R.id.rutas_plantilla_recta to RutaEditorNativa.PLANTILLA_RECTA, R.id.rutas_plantilla_cuadrado to RutaEditorNativa.PLANTILLA_CUADRADO,
            R.id.rutas_plantilla_ocho to RutaEditorNativa.PLANTILLA_OCHO, R.id.rutas_plantilla_zigzag to RutaEditorNativa.PLANTILLA_ZIGZAG)) {
            vista.findViewById<View>(id).setOnClickListener { hacer { RutaEditorNativa.plantilla(plantilla) } }
        }
        vista.findViewById<MaterialSwitch>(R.id.rutas_switch_obstaculo).setOnCheckedChangeListener { _, encendido ->
            cuadricula.modo = if (encendido) EditorRutaView.Modo.OBSTACULOS else EditorRutaView.Modo.RUTA
            cuadricula.deseleccionar()
            ayuda.setText(if (encendido) R.string.rutas_ayuda_obstaculos else R.string.rutas_ayuda_toque)
            ayuda.setTextColor(Diseno.colorDe(ctx, Nivel.NEUTRO))
        }
        botonSimular.setOnClickListener { simular(0, 0) }
        vista.findViewById<View>(R.id.rutas_sim_ruido).setOnClickListener { dispersar() }
        vista.findViewById<View>(R.id.rutas_sim_cerrar).setOnClickListener { quitarSimulacion() }
        vista.findViewById<View>(R.id.rutas_guardar).setOnClickListener { pedirNombreYGuardar() }
        vista.findViewById<View>(R.id.rutas_abrir).setOnClickListener { elegirRutaParaAbrir() }
        vista.findViewById<View>(R.id.rutas_ejecutar).setOnClickListener { ejecutar() }
        for (id in listOf(R.id.rutas_simular, R.id.rutas_guardar, R.id.rutas_abrir, R.id.rutas_ejecutar)) {
            Movimiento.respuestaAlTocar(vista.findViewById(id))
        }
        actualizar()
        mostrarHistorial()
    }

    override fun onStart() {
        super.onStart()
        SesionCarro.entrar(requireContext(), SesionNativa.USUARIO_RUTAS)
        SesionCarro.logica.pedirPerfil(ControlLogica.Perfil.AUTO)
        PantallasLog.abierta("RUTAS")
    }

    override fun onStop() {
        cuadricula.detenerAnimacion()
        SesionCarro.salir(SesionNativa.USUARIO_RUTAS)
        super.onStop()
    }

    // ── Editar ────────────────────────────────────────────────────────────

    /** Hace una acción del editor (devuelve 0 o un código de error), y actualiza la pantalla. */
    private fun hacer(accion: () -> Int) {
        val codigo = accion()
        if (codigo != 0) {
            Haptica.obstaculo(requireContext())
            avisarAyuda(RutaEditorNativa.textoError(codigo))
        } else {
            ayuda.setText(if (cuadricula.modo == EditorRutaView.Modo.OBSTACULOS) R.string.rutas_ayuda_obstaculos else R.string.rutas_ayuda_toque)
            ayuda.setTextColor(Diseno.colorDe(requireContext(), Nivel.NEUTRO))
        }
        cuadricula.deseleccionar()
        quitarSimulacion()
        almacen.guardarBorrador()
        nombreActual = null
        actualizar()
    }

    private fun avisarAyuda(texto: String) {
        ayuda.text = texto
        ayuda.setTextColor(Diseno.colorDe(requireContext(), Nivel.AVISO))
    }

    /** Refresca el resumen, el nombre y los botones con el estado del editor. */
    private fun actualizar() {
        val cal = calibracionParaSimular()
        val r = RutaEditorNativa.resumen(cal)
        val celdas = r[RutaEditorNativa.R_CELDAS]
        val vertices = r[RutaEditorNativa.R_VERTICES]
        nombre.text = nombreActual ?: getString(R.string.rutas_borrador)
        chipPuntos.poner(if (vertices >= 2) Nivel.OK else Nivel.NEUTRO, if (vertices == 0) getString(R.string.rutas_chip_vacia) else getString(R.string.rutas_chip_puntos, vertices))
        botonDeshacer.isEnabled = RutaEditorNativa.puedeDeshacer() != 0
        botonRehacer.isEnabled = RutaEditorNativa.puedeRehacer() != 0
        if (celdas >= 2) {
            largo.text = "${r[RutaEditorNativa.R_LARGO_CM]} cm"
            tiempo.text = textoDuracion(r[RutaEditorNativa.R_TIEMPO_MS].toLong())
            giros.text = "${r[RutaEditorNativa.R_GIROS]}"
        } else {
            largo.setText(R.string.valor_sin_dato)
            tiempo.setText(R.string.valor_sin_dato)
            giros.setText(R.string.valor_sin_dato)
        }
        val avisos = ArrayList<String>()
        if (r[RutaEditorNativa.R_ADVERTENCIAS] and RutaEditorNativa.ADV_GIRO_CERRADO != 0) {
            avisos.add("Hay un giro muy cerrado (casi media vuelta): el carro puede perder precisión.")
        }
        if (celdas >= 2 && cal == null) {
            avisos.add("El tiempo usa valores de ejemplo: calibra el carro en Más → Calibración.")
        }
        advertencia.visibility = if (avisos.isEmpty()) View.GONE else View.VISIBLE
        advertencia.text = avisos.joinToString("\n")
    }

    /** La calibración guardada si está completa; null = valores de ejemplo (el resumen y la simulación avisan). */
    private fun calibracionParaSimular(): IntArray? {
        val guardada = CalibracionGuardada(requireContext())
        return if (guardada.estaCompleta()) guardada.leerAjustada() else null   // la medida con el factor de giro (la misma que usan las rutas de verdad)
    }

    private fun textoDuracion(ms: Long): String {
        val s = (ms + 500) / 1000
        return if (s < 60) "$s s" else String.format(Locale.US, "%d min %02d s", s / 60, s % 60)
    }

    // ── Simular ───────────────────────────────────────────────────────────

    private fun quitarSimulacion() {
        cuadricula.limpiarSimulacion()
        tarjetaSim.visibility = View.GONE
        botonSimular.isEnabled = true
    }

    private fun simular(ruido: Int, semilla: Int) {
        if (RutaEditorNativa.celdas().size < 4) {
            avisarAyuda(getString(R.string.rutas_ejecutar_sin_ruta))
            return
        }
        val guardada = CalibracionGuardada(requireContext())
        SimRutaNativa.ponerBalance(guardada.desbalanceParaSimular(), guardada.balance(ControlLogica.Perfil.AUTO))   // el mismo balance de los lados que usa el carro (AUTO)
        val r = SimRutaNativa.simular(calibracionParaSimular(), cuadricula.obstaculosComoArreglo(), ruido, semilla)
        tarjetaSim.visibility = View.VISIBLE
        if (r[SimRutaNativa.S_CODIGO] != 0) {
            simChip.poner(Nivel.ERROR, getString(R.string.sim_no_llego))
            simTexto.setText(R.string.sim_error_ruta)
            simComandos.text = ""
            return
        }
        ultimaSimulacion = r
        simChip.poner(Nivel.NEUTRO, getString(R.string.ayuda_simulando))
        simTexto.text = ""
        simComandos.text = ""
        botonSimular.isEnabled = false
        cuadricula.animar(SimRutaNativa.muestras())
    }

    private var ultimaSimulacion: IntArray? = null

    /** Un paso de la animación: muestra el tiempo y lo que hace el carro. Al terminar (muestra null) muestra el resultado. */
    private fun alAnimarSimulacion(muestra: IntArray?) {
        val r = ultimaSimulacion ?: return
        if (muestra != null) {
            simChip.poner(Nivel.NEUTRO, textoDuracion(muestra[0].toLong()))
            simTexto.text = palabraComando(muestra[4], muestra[5])
            return
        }
        botonSimular.isEnabled = true
        val llego = r[SimRutaNativa.S_LLEGO] == 1
        val ctx = requireContext()
        simChip.poner(if (llego) Nivel.OK else Nivel.ERROR, getString(if (llego) R.string.sim_llego else R.string.sim_no_llego))
        val duracion = textoDuracion(r[SimRutaNativa.S_DURACION_MS].toLong())
        val errorCm = r[SimRutaNativa.S_ERROR_FINAL_MM] / 10
        var texto = if (llego) getString(R.string.sim_texto_llego, duracion, r[SimRutaNativa.S_COMANDOS], r[SimRutaNativa.S_ESQUIVADAS], errorCm)
        else getString(R.string.sim_texto_no_llego, duracion, errorCm)
        val motivo = when (r[SimRutaNativa.S_MOTIVO_ERROR]) {
            1 -> R.string.sim_motivo_1
            2 -> R.string.sim_motivo_2
            3 -> R.string.sim_motivo_3
            4 -> R.string.sim_motivo_4
            else -> 0
        }
        val causa = SimRutaNativa.textoCausa()
        if (!llego && causa.isNotEmpty()) {
            texto += " $causa"     // por qué no llegó, con los números ("El obstáculo bloqueó el camino y se agotaron los 2 reintentos.")
        } else if (!llego && motivo != 0) {
            texto += " " + getString(motivo)
        }
        simTexto.text = texto
        val avisos = ArrayList<String>()
        for (bit in listOf(SimRutaNativa.ADV_SALE_CUADRICULA, SimRutaNativa.ADV_CALIBRACION)) {
            if (r[SimRutaNativa.S_ADVERTENCIAS] and bit != 0) {
                avisos.add("• " + SimRutaNativa.textoAdvertencia(bit))
            }
        }
        if (CalibracionGuardada(ctx).balanceCalibrado(ControlLogica.Perfil.AUTO) == 0) {
            avisos.add("• " + getString(R.string.sim_aviso_recta))   // sin calibrar la recta el carro virtual se tuerce como el real medido
        }
        simComandos.text = avisos.joinToString("\n")
        Haptica.corta(ctx)
    }

    private fun palabraComando(comando: Int, estado: Int): String = when {
        estado == NavegadorNativa.TERMINADO -> getString(R.string.ejecutar_estado_terminado)
        estado == NavegadorNativa.ESQUIVANDO -> getString(R.string.ejecutar_estado_esquivando)
        estado == NavegadorNativa.REINCORPORANDO -> getString(R.string.ejecutar_estado_volviendo)
        estado == NavegadorNativa.DETENIDO_OBSTACULO -> getString(R.string.ejecutar_estado_obstaculo)
        comando == NavegadorNativa.CMD_FORWARD -> getString(R.string.ejecutar_estado_avanzando)
        comando == NavegadorNativa.CMD_REVERSE -> "Retrocediendo"
        comando == NavegadorNativa.CMD_LEFT || comando == NavegadorNativa.CMD_RIGHT -> getString(R.string.ejecutar_estado_girando)
        else -> getString(R.string.ejecutar_estado_detenido)
    }

    /** Repite la ruta 8 veces con un giro hasta 20° errado: muestra dónde termina cada una. */
    private fun dispersar() {
        val guardada = CalibracionGuardada(requireContext())
        SimRutaNativa.ponerBalance(guardada.desbalanceParaSimular(), guardada.balance(ControlLogica.Perfil.AUTO))
        val d = SimRutaNativa.dispersion(calibracionParaSimular(), cuadricula.obstaculosComoArreglo(), 20, 8)
        if (d[SimRutaNativa.D_CODIGO] != 0) {
            simTexto.setText(R.string.sim_error_ruta)
            return
        }
        val n = d[SimRutaNativa.D_CORRIDAS]
        val puntos = ArrayList<IntArray>()
        for (i in 0 until n) {
            val b = SimRutaNativa.D_ENCABEZADO + i * SimRutaNativa.D_PASO
            puntos.add(intArrayOf(d[b + 1], d[b + 2], d[b]))
        }
        cuadricula.mostrarFinales(puntos)
        val llegaron = d[SimRutaNativa.D_LLEGARON]
        simChip.poner(if (llegaron == n) Nivel.OK else Nivel.AVISO, "$llegaron de $n")
        simTexto.text = getString(R.string.sim_dispersion, llegaron, n, d[SimRutaNativa.D_DISPERSION_MM] / 10, d[SimRutaNativa.D_ERROR_MEDIO_MM] / 10, d[SimRutaNativa.D_ERROR_MAX_MM] / 10)
        simComandos.text = ""
    }

    // ── Guardar y abrir ───────────────────────────────────────────────────

    private fun pedirNombreYGuardar() {
        val ctx = requireContext()
        if (RutaEditorNativa.vertices().isEmpty()) {
            avisarAyuda(getString(R.string.rutas_ejecutar_sin_ruta))
            return
        }
        val capa = TextInputLayout(ctx)
        capa.hint = getString(R.string.rutas_guardar_hint)
        val campo = TextInputEditText(capa.context)
        campo.setText(nombreActual ?: "")
        capa.addView(campo)
        val margen = Diseno.dp(ctx, 24)
        val envoltura = LinearLayout(ctx)
        envoltura.setPadding(margen, Diseno.dp(ctx, 8), margen, 0)
        envoltura.addView(capa, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT))
        MaterialAlertDialogBuilder(ctx)
            .setTitle(R.string.rutas_guardar_titulo)
            .setView(envoltura)
            .setPositiveButton(R.string.rutas_guardar) { _, _ ->
                val texto = campo.text?.toString()?.trim().orEmpty()
                if (RutaEditorNativa.nombreValido(texto) == 0) {
                    avisarAyuda(getString(R.string.rutas_guardar_nombre_malo))
                } else if (almacen.existe(texto)) {
                    Dialogos.confirmar(ctx, getString(R.string.rutas_guardar_titulo), getString(R.string.rutas_guardar_existe), getString(R.string.rutas_guardar)) { guardarComo(texto) }
                } else {
                    guardarComo(texto)
                }
            }
            .setNegativeButton(R.string.cancelar, null)
            .show()
    }

    private fun guardarComo(texto: String) {
        if (almacen.guardar(texto)) {
            nombreActual = texto
            actualizar()
            Haptica.corta(requireContext())
            avisarAyuda(getString(R.string.rutas_guardar_listo, texto))
            ayuda.setTextColor(Diseno.colorDe(requireContext(), Nivel.OK))
        } else {
            avisarAyuda(getString(R.string.rutas_guardar_fallo))
        }
    }

    private fun elegirRutaParaAbrir() {
        val ctx = requireContext()
        val nombres = almacen.nombres()
        if (nombres.isEmpty()) {
            Dialogos.avisar(ctx, getString(R.string.rutas_abrir_titulo), getString(R.string.rutas_abrir_vacio))
            return
        }
        MaterialAlertDialogBuilder(ctx)
            .setTitle(R.string.rutas_abrir_titulo)
            .setItems(nombres.toTypedArray()) { _, i -> abrirRuta(nombres[i]) }
            .setNeutralButton(R.string.rutas_borrar_titulo) { _, _ -> elegirRutaParaBorrar() }
            .setNegativeButton(R.string.cancelar, null)
            .show()
    }

    private fun abrirRuta(nombreRuta: String) {
        val codigo = almacen.abrir(nombreRuta)
        if (codigo != 0) {
            avisarAyuda(getString(R.string.rutas_abrir_error))
            return
        }
        nombreActual = nombreRuta
        cuadricula.deseleccionar()
        quitarSimulacion()
        almacen.guardarBorrador()
        actualizar()
        cuadricula.invalidate()
    }

    private fun elegirRutaParaBorrar() {
        val ctx = requireContext()
        val nombres = almacen.nombres()
        MaterialAlertDialogBuilder(ctx)
            .setTitle(R.string.rutas_borrar_titulo)
            .setItems(nombres.toTypedArray()) { _, i ->
                Dialogos.confirmar(ctx, getString(R.string.rutas_borrar_titulo), getString(R.string.rutas_borrar_confirmar, nombres[i]), getString(R.string.rutas_borrar_si)) {
                    almacen.borrar(nombres[i])
                }
            }
            .setNegativeButton(R.string.cancelar, null)
            .show()
    }

    // ── Ejecutar e historial ──────────────────────────────────────────────

    private fun ejecutar() {
        val celdas = RutaEditorNativa.celdas()
        if (celdas.size < 4) {
            avisarAyuda(getString(R.string.rutas_ejecutar_sin_ruta))
            return
        }
        Haptica.corta(requireContext())
        (activity as? MainActivity)?.abrirSubpantalla(EjecutarRutaFragment.nueva(nombreActual ?: getString(R.string.rutas_borrador), celdas))
    }

    private fun mostrarHistorial() {
        historial.removeAllViews()
        val lista = almacen.ejecuciones().take(8)
        historialVacio.visibility = if (lista.isEmpty()) View.VISIBLE else View.GONE
        val ctx = requireContext()
        for (e in lista) {
            val fila = LinearLayout(ctx)
            fila.orientation = LinearLayout.HORIZONTAL
            fila.gravity = android.view.Gravity.CENTER_VERTICAL
            fila.minimumHeight = Diseno.dp(ctx, 48)
            val columna = LinearLayout(ctx)
            columna.orientation = LinearLayout.VERTICAL
            val titulo = TextView(ctx)
            titulo.setTextAppearance(R.style.Texto_Cuerpo)
            titulo.text = e.ruta
            titulo.maxLines = 1
            titulo.ellipsize = android.text.TextUtils.TruncateAt.END
            val detalle = TextView(ctx)
            detalle.setTextAppearance(R.style.Texto_Suave)
            val fecha = DateFormat.format("d MMM, HH:mm", Date(e.epochS * 1000)).toString()
            detalle.text = "$fecha · ${e.duracionS} s" + (if (e.esquivadas > 0) " · ${e.esquivadas} esquivadas" else "")
            columna.addView(titulo)
            columna.addView(detalle)
            fila.addView(columna, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            val chip = ChipEstado(ctx)
            when (e.resultado) {
                "TERMINADO" -> chip.poner(Nivel.OK, getString(R.string.hist_terminado))
                "ERROR" -> chip.poner(Nivel.ERROR, getString(R.string.hist_error))
                else -> chip.poner(Nivel.AVISO, getString(R.string.hist_cancelado))
            }
            fila.addView(chip)
            historial.addView(fila, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).also { it.bottomMargin = Diseno.dp(ctx, 8) })
        }
    }
}
