/*
 * CalibracionGuardada.kt — Guarda y lee la calibración del carro en el teléfono.
 *
 * GUÍA DE LECTURA
 *  1. Se guarda como TEXTO ("clave=valor", una por línea; el mismo que produce el código en C) en
 *     SharedPreferences ("calibracion"). Es el mismo texto que se exporta: se puede copiar,
 *     pegar en otro teléfono o leer a mano.
 *  2. guardar() además escribe el archivo calibracion.txt en la carpeta de archivos de la app, para
 *     sacarlo con adb (ver docs/calibracion.md). Nunca va al repositorio. ANTES de guardar revisa con el código en C que los ms por grado de cada lado estén entre 3 y 20
 *     (si no, no guarda nada y devuelve CalibracionNativa.ERR_GIRO_FUERA: casi seguro se anotó mal un dato).
 *  3. leer() devuelve la calibración guardada o, si no hay (o el texto no es válido), la de por
 *     defecto. estaCompleta() dice si todo se midió (si no, la pantalla de control avisa).
 *  4. aplicarAlNavegador() se la pasa al navegador en C (se usa al cargar la siguiente ruta).
 *  5. AJUSTE FINO DEL GIRO: además de la calibración hay un FACTOR de corrección del giro por lado (izquierda y derecha) y por perfil (AUTO y MANUAL), en milésimas
 *     (1000 = sin corrección), que sale de la prueba «Giro de 90°». leer() devuelve la calibración tal como se midió; leerAjustada() le aplica el factor y es la que usan el
 *     navegador, la simulación y la prueba del propio giro. Al guardar una calibración con los giros recién medidos los factores vuelven a 1000.
 *  6. AJUSTE FINO DE LA RECTA: el BALANCE es el factor (milésimas, 800 a 1000) con que el firmware reduce la potencia del lado izquierdo, por perfil. El teléfono guarda el que salió
 *     de la prueba «Recta» (balance()) y se lo manda al carro al conectar (balancesParaEnviar); el carro lo guarda también en su memoria. balanceCalibrado() es el valor con el que la
 *     prueba terminó con 1 cm o menos de desvío (0 = nunca se calibró): el simulador lo usa para saber cuánto empuja de más el lado izquierdo.
 *  7. DESPLAZAMIENTO DEL CENTRO AL GIRAR: por lado y por perfil, en µm por grado (adelante y derecha), de la prueba «Desplazamiento al girar». Se guarda aparte del texto de la calibración
 *     (igual que el factor de giro) y leerAjustada() lo pone en la calibración que usan las rutas, el simulador y el navegador. Sin medir vale 0 (el carro gira en su sitio, como antes).
 *  8. CALIBRACIÓN POR PARTES: cada parte (enum Parte) se mide sola. Aquí queda de cada una la FECHA y hora de la última vez que se calibró (fecha / marcarCalibrado) y, de los giros y del
 *     desplazamiento, con QUÉ giro de prueba se midió (GiroPrueba): los ms por grado dependen de la duración con que se midieron, así que se avisa si los dos lados (o el giro y el
 *     desplazamiento) se midieron con pruebas distintas (avisoMezcla).
 */
package com.fsi.carro

import android.content.Context
import android.util.Log
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Las partes de la calibración que se pueden medir cada una por su lado. */
enum class Parte(val porPerfil: Boolean, val dependeDePilas: Boolean) {
    ARRANQUE(false, true),
    VELOCIDAD(false, true),
    FRENADO(false, true),
    GIRO_IZQ(false, true),
    GIRO_DER(false, true),
    RETROCESO(false, true),
    GIRO_VISUAL(false, false),
    DESPLAZAMIENTO(true, true),
    GIRO_90(true, true),
    RECTA(true, true)
}

/**
 * El giro con el que se hace una prueba de giro: «Giro de 90° real» (el comando que usa el navegador, por defecto), 180°, una duración libre (500 a 1500 ms) o, solo para
 * calibrar el giro, la prueba completa de tres tiempos (500, 1000 y 1500 ms; mide también cuánto sigue girando al frenar).
 */
data class GiroPrueba(val tipo: Tipo, val ms: Int = 1000) {

    enum class Tipo { NOVENTA, CIENTO_OCHENTA, LIBRE, TRES_TIEMPOS }

    companion object {
        val DEFECTO = GiroPrueba(Tipo.NOVENTA)
        const val LIBRE_MIN_MS = 500
        const val LIBRE_MAX_MS = 1500
        const val LIBRE_PASO_MS = 100

        /** Lee el código guardado ("90", "180", "libre:1000", "tres"); null si no hay o no se entiende. */
        fun deCodigo(codigo: String?): GiroPrueba? = when {
            codigo == null -> null
            codigo == "90" -> GiroPrueba(Tipo.NOVENTA)
            codigo == "180" -> GiroPrueba(Tipo.CIENTO_OCHENTA)
            codigo == "tres" -> GiroPrueba(Tipo.TRES_TIEMPOS)
            codigo.startsWith("libre:") -> codigo.substring(6).toIntOrNull()?.let { GiroPrueba(Tipo.LIBRE, it.coerceIn(LIBRE_MIN_MS, LIBRE_MAX_MS)) }
            else -> null
        }
    }

    fun codigo(): String = when (tipo) {
        Tipo.NOVENTA -> "90"
        Tipo.CIENTO_OCHENTA -> "180"
        Tipo.LIBRE -> "libre:$ms"
        Tipo.TRES_TIEMPOS -> "tres"
    }

    /** El nombre que ve el usuario. */
    fun texto(): String = when (tipo) {
        Tipo.NOVENTA -> "Giro de 90° real"
        Tipo.CIENTO_OCHENTA -> "Giro de 180°"
        Tipo.LIBRE -> "Duración libre de $ms ms"
        Tipo.TRES_TIEMPOS -> "Tres tiempos (500, 1000 y 1500 ms)"
    }

    /** Los grados que se piden con este giro (0 si es por duración). */
    fun grados(): Int = when (tipo) {
        Tipo.NOVENTA -> 90
        Tipo.CIENTO_OCHENTA -> 180
        else -> 0
    }

    /** ¿Es la misma prueba? Dos duraciones libres cuentan como la misma si difieren menos de 150 ms. */
    fun mismaQue(otra: GiroPrueba): Boolean =
        tipo == otra.tipo && (tipo != Tipo.LIBRE || Math.abs(ms - otra.ms) < 150)
}

class CalibracionGuardada(contexto: Context) {

    companion object {
        const val ETIQUETA = "CALIBRACION"
        const val NOMBRE_ARCHIVO = "calibracion.txt"
    }

    private val aplicacion = contexto.applicationContext
    private val preferencias = aplicacion.getSharedPreferences("calibracion", Context.MODE_PRIVATE)

    /** La calibración guardada (14 enteros, el desplazamiento en 0) o la de por defecto. */
    fun leer(): IntArray {
        val texto = preferencias.getString("texto", null)
        if (texto != null) {
            val (codigo, cal) = CalibracionNativa.desempaquetar(CalibracionNativa.deTexto(texto))
            if (codigo == CalibracionNativa.OK) {
                return cal
            }
            Log.w(ETIQUETA, "el texto guardado no es válido (código $codigo): se usa la calibración por defecto")
        }
        return CalibracionNativa.desempaquetar(CalibracionNativa.defecto()).second
    }

    // ── Ajuste fino del giro de 90° (factor por lado y por perfil) ──

    private fun claveGiro(perfil: ControlLogica.Perfil, lado: Int) = "giro_factor_${perfil.name.lowercase()}_${if (lado < 0) "izq" else "der"}"

    /** El factor de corrección del giro de ese lado (CalibracionNativa.LADO_*) y perfil, en milésimas. 1000 = sin corrección. */
    fun factorGiro(perfil: ControlLogica.Perfil, lado: Int): Int =
        preferencias.getInt(claveGiro(perfil, lado), CalibracionNativa.FACTOR_GIRO_DEFECTO)

    fun guardarFactorGiro(perfil: ControlLogica.Perfil, lado: Int, milesimas: Int) {
        preferencias.edit().putInt(claveGiro(perfil, lado), milesimas).apply()
        Log.i(ETIQUETA, "factor de giro ${perfil.name} ${if (lado < 0) "izquierda" else "derecha"} = $milesimas")
    }

    /** Vuelve los dos lados del perfil a 1000 (sin corrección). */
    fun restaurarFactoresGiro(perfil: ControlLogica.Perfil) {
        preferencias.edit().remove(claveGiro(perfil, CalibracionNativa.LADO_IZQUIERDA)).remove(claveGiro(perfil, CalibracionNativa.LADO_DERECHA)).remove(claveFecha(Parte.GIRO_90, perfil)).apply()
        Log.i(ETIQUETA, "factores de giro ${perfil.name} restaurados a 1000")
    }

    /** Vuelve a 1000 el factor de UN lado en los dos perfiles (se acaba de medir el giro de ese lado: el factor de antes ya no aplica). */
    fun restaurarFactorGiroDeLado(lado: Int) {
        val edicion = preferencias.edit()
        for (perfil in ControlLogica.Perfil.values()) {
            edicion.remove(claveGiro(perfil, lado))
            val otroLado = -lado
            if (factorGiro(perfil, otroLado) == CalibracionNativa.FACTOR_GIRO_DEFECTO) {
                edicion.remove(claveFecha(Parte.GIRO_90, perfil))   // ya no queda ningún ajuste fino en ese perfil
            }
        }
        edicion.apply()
        Log.i(ETIQUETA, "factor de giro de la ${if (lado < 0) "izquierda" else "derecha"} restaurado a 1000 en los dos perfiles")
    }

    /** true si algún lado de algún perfil tiene un factor distinto de 1000. */
    fun hayFactoresGiro(): Boolean = ControlLogica.Perfil.values().any { p ->
        factorGiro(p, CalibracionNativa.LADO_IZQUIERDA) != CalibracionNativa.FACTOR_GIRO_DEFECTO || factorGiro(p, CalibracionNativa.LADO_DERECHA) != CalibracionNativa.FACTOR_GIRO_DEFECTO
    }

    // ── Desplazamiento del centro al girar (por lado y por perfil) ──

    private fun claveDespAdelante(perfil: ControlLogica.Perfil, lado: Int) = "desp_adelante_${perfil.name.lowercase()}_${if (lado < 0) "izq" else "der"}"
    private fun claveDespDerecha(perfil: ControlLogica.Perfil, lado: Int) = "desp_derecha_${perfil.name.lowercase()}_${if (lado < 0) "izq" else "der"}"

    /** El desplazamiento del centro al girar hacia ese lado y perfil: (adelante, derecha) en µm por grado. (0, 0) = sin medir: el carro gira en su sitio. */
    fun desplazamiento(perfil: ControlLogica.Perfil, lado: Int): Pair<Int, Int> =
        Pair(preferencias.getInt(claveDespAdelante(perfil, lado), 0), preferencias.getInt(claveDespDerecha(perfil, lado), 0))

    fun guardarDesplazamiento(perfil: ControlLogica.Perfil, lado: Int, adelanteUm: Int, derechaUm: Int) {
        preferencias.edit().putInt(claveDespAdelante(perfil, lado), adelanteUm).putInt(claveDespDerecha(perfil, lado), derechaUm).apply()
        Log.i(ETIQUETA, "desplazamiento al girar ${perfil.name} ${if (lado < 0) "izquierda" else "derecha"}: adelante=$adelanteUm um/grado derecha=$derechaUm um/grado")
    }

    /** «Restaurar valor por defecto»: el desplazamiento de los dos lados del perfil vuelve a 0. */
    fun restaurarDesplazamiento(perfil: ControlLogica.Perfil) {
        val edicion = preferencias.edit()
        for (lado in intArrayOf(CalibracionNativa.LADO_IZQUIERDA, CalibracionNativa.LADO_DERECHA)) {
            edicion.remove(claveDespAdelante(perfil, lado)).remove(claveDespDerecha(perfil, lado)).remove(claveGiroPruebaDesp(perfil, lado))
        }
        edicion.remove(claveFecha(Parte.DESPLAZAMIENTO, perfil)).apply()
        Log.i(ETIQUETA, "desplazamiento al girar ${perfil.name} restaurado a 0")
    }

    /** true si ese perfil tiene algún desplazamiento distinto de cero. */
    fun hayDesplazamiento(perfil: ControlLogica.Perfil): Boolean = listOf(CalibracionNativa.LADO_IZQUIERDA, CalibracionNativa.LADO_DERECHA).any {
        val (a, d) = desplazamiento(perfil, it)
        a != 0 || d != 0
    }

    /** La calibración con el factor de giro y el desplazamiento del perfil aplicados (AUTO por defecto: es la que usan las rutas y el simulador). Si el factor deja la calibración fuera de rango, la medida. */
    fun leerAjustada(perfil: ControlLogica.Perfil = ControlLogica.Perfil.AUTO): IntArray {
        val medida = leer()
        val (codigo, ajustada) = CalibracionNativa.desempaquetar(CalibracionNativa.conFactorGiro(
            medida, factorGiro(perfil, CalibracionNativa.LADO_IZQUIERDA), factorGiro(perfil, CalibracionNativa.LADO_DERECHA)))
        var resultado = if (codigo == CalibracionNativa.OK) ajustada else medida
        for (lado in intArrayOf(CalibracionNativa.LADO_IZQUIERDA, CalibracionNativa.LADO_DERECHA)) {
            val (adelante, derecha) = desplazamiento(perfil, lado)
            val (c, conDesp) = CalibracionNativa.desempaquetar(CalibracionNativa.ponerDesplazamiento(resultado, lado, adelante, derecha))
            if (c == CalibracionNativa.OK) {
                resultado = conDesp
            }
        }
        return resultado
    }

    // ── Ajuste fino de la recta (balance de los lados) ──

    private fun claveBalance(perfil: ControlLogica.Perfil) = "balance_${perfil.name.lowercase()}"
    private fun claveBalanceCalibrado(perfil: ControlLogica.Perfil) = "balance_calibrado_${perfil.name.lowercase()}"

    private fun balanceDefecto(perfil: ControlLogica.Perfil) =
        if (perfil == ControlLogica.Perfil.AUTO) CalibracionNativa.BALANCE_DEFECTO_AUTO else CalibracionNativa.BALANCE_DEFECTO_MANUAL

    /** El balance del lado izquierdo de ese perfil, en milésimas (800 a 1000). Si nunca se cambió, el por defecto del firmware. */
    fun balance(perfil: ControlLogica.Perfil): Int =
        preferencias.getInt(claveBalance(perfil), 0).takeIf { it in CalibracionNativa.BALANCE_MIN..CalibracionNativa.BALANCE_MAX } ?: balanceDefecto(perfil)

    /** true si el balance de ese perfil se cambió alguna vez (si no, no hace falta mandárselo al carro). */
    fun hayBalanceGuardado(perfil: ControlLogica.Perfil): Boolean = preferencias.getInt(claveBalance(perfil), 0) > 0

    fun guardarBalance(perfil: ControlLogica.Perfil, milesimas: Int) {
        preferencias.edit().putInt(claveBalance(perfil), milesimas).apply()
        Log.i(ETIQUETA, "balance ${perfil.name} = $milesimas")
    }

    /** El balance con el que la prueba de recta terminó bien (0 = nunca se calibró). */
    fun balanceCalibrado(perfil: ControlLogica.Perfil): Int = preferencias.getInt(claveBalanceCalibrado(perfil), 0)

    fun guardarBalanceCalibrado(perfil: ControlLogica.Perfil, milesimas: Int) {
        preferencias.edit().putInt(claveBalanceCalibrado(perfil), milesimas).apply()
    }

    /** Vuelve al balance por defecto del firmware y olvida que se había calibrado. */
    fun restaurarBalance(perfil: ControlLogica.Perfil) {
        preferencias.edit().remove(claveBalance(perfil)).remove(claveBalanceCalibrado(perfil)).remove(claveFecha(Parte.RECTA, perfil)).apply()
        Log.i(ETIQUETA, "balance ${perfil.name} restaurado al valor por defecto (${balanceDefecto(perfil)})")
    }

    /** Los balances que hay que mandarle al carro al conectar: [AUTO, MANUAL]; 0 = ese no se ha cambiado, no se manda. */
    fun balancesParaEnviar(): IntArray = intArrayOf(
        if (hayBalanceGuardado(ControlLogica.Perfil.AUTO)) balance(ControlLogica.Perfil.AUTO) else 0,
        if (hayBalanceGuardado(ControlLogica.Perfil.MANUAL)) balance(ControlLogica.Perfil.MANUAL) else 0)

    /** El ancho entre las ruedas de un lado y las del otro, en mm (12 cm por defecto): lo usa la geometría de la prueba de recta. */
    var anchoEjesMm: Int
        get() = preferencias.getInt("ancho_ejes_mm", CalibracionNativa.EJES_MM_DEFECTO)
        set(valor) = preferencias.edit().putInt("ancho_ejes_mm", valor).apply()

    /** Para el simulador: cuánto más rápido empuja el lado izquierdo, en milésimas (1000 = igual). Si se calibró la recta es lo que hace falta para que el balance calibrado deje el carro recto; si no, el valor típico medido. */
    fun desbalanceParaSimular(): Int {
        val calibrado = balanceCalibrado(ControlLogica.Perfil.AUTO)
        return if (calibrado > 0) (1000000 / calibrado) else CalibracionNativa.DESBALANCE_TIPICO_MIL
    }

    /** El campo de visión horizontal de la cámara guardado, en grados por 10 (600 si no se calibró). */
    fun fovX10(): Int = leer()[CalibracionNativa.CAL_FOV_GRADOS_X10]

    /** El campo de visión CALIBRADO, o 0 si todavía no se calibró (entonces la cámara usa el que calcula con su focal y su sensor). */
    fun fovCalibradoX10(): Int {
        val cal = leer()
        return if (cal[CalibracionNativa.CAL_MEDIDOS] and CalibracionNativa.MEDIDO_FOV != 0) cal[CalibracionNativa.CAL_FOV_GRADOS_X10] else 0
    }

    /** true si los ms por cm al RETROCEDER están medidos (si no, los reintentos de esquive usan los del avance). */
    fun retrocesoCalibrado(): Boolean = leer()[CalibracionNativa.CAL_MEDIDOS] and CalibracionNativa.MEDIDO_ATRAS != 0

    /** Aviso si el retroceso no se calibró (lo usan la pantalla de control y la de calibración), o null si ya está. */
    fun avisoRetroceso(): String? =
        if (retrocesoCalibrado()) null
        else "Retroceso sin calibrar: al retroceder para reintentar un esquive se usa la velocidad del avance. Mídelo en «Calibración del carro» (retroceso, opcional)."

    /** true si ya se midió todo (arranque, avance y los dos giros). */
    fun estaCompleta(): Boolean = CalibracionNativa.faltantes(leer()) == 0

    /** Guarda la calibración y escribe el archivo de texto. Primero la revisa (CalibracionNativa.validarParaGuardar: valores válidos y los ms por grado de cada lado entre 3 y 20).
     *  Devuelve CalibracionNativa.OK, o el código de error (ERR_GIRO_FUERA, ERR_RANGO...) y en ese caso NO cambia nada de lo guardado. */
    fun guardar(calibracion: IntArray): Int {
        val (codigo, ladoMalo) = CalibracionNativa.validarParaGuardar(calibracion).let { Pair(it[0], it[1]) }
        if (codigo != CalibracionNativa.OK) {
            Log.w(ETIQUETA, "no se guardó: código $codigo" + (if (ladoMalo != 0) " (giro ${if (ladoMalo < 0) "izquierda" else "derecha"} fuera de 3 a 20 ms por grado)" else ""))
            return codigo
        }
        val texto = CalibracionNativa.aTexto(calibracion)
        if (texto.isEmpty()) {
            return CalibracionNativa.ERR_RANGO
        }
        preferencias.edit().putString("texto", texto).apply()
        try {
            File(aplicacion.getExternalFilesDir(null), NOMBRE_ARCHIVO).writeText(texto)
        } catch (e: Exception) {
            Log.w(ETIQUETA, "no se pudo escribir $NOMBRE_ARCHIVO: ${e.message}")
        }
        Log.i(ETIQUETA, "guardada: " + texto.replace('\n', ' '))
        return CalibracionNativa.OK
    }

    /** Vuelve a los valores por defecto (borra lo guardado y el archivo). */
    fun borrar() {
        preferencias.edit().remove("texto").apply()
        try {
            File(aplicacion.getExternalFilesDir(null), NOMBRE_ARCHIVO).delete()
        } catch (e: Exception) {
            Log.w(ETIQUETA, "no se pudo borrar $NOMBRE_ARCHIVO: ${e.message}")
        }
        Log.i(ETIQUETA, "calibración borrada: se usan los valores por defecto")
    }

    /** Le entrega la calibración guardada (con el factor de giro y el desplazamiento del perfil AUTO) al navegador, para la próxima ruta. */
    fun aplicarAlNavegador() {
        val codigo = NavegadorNativa.ponerCalibracion(leerAjustada(ControlLogica.Perfil.AUTO))
        if (codigo != 0) {
            Log.w(ETIQUETA, "el navegador rechazó la calibración (código $codigo)")
        }
    }

    /** Texto del aviso si falta calibrar algo, o null si todo está calibrado. */
    fun avisoSiFalta(): String? {
        val falta = CalibracionNativa.faltantes(leer())
        if (falta == 0) {
            return null
        }
        val partes = ArrayList<String>()
        if (falta and CalibracionNativa.MEDIDO_ARRANQUE != 0) partes.add("arranque")
        if (falta and CalibracionNativa.MEDIDO_AVANCE != 0) partes.add("avance")
        if (falta and CalibracionNativa.MEDIDO_GIRO_IZQ != 0) partes.add("giro izquierda")
        if (falta and CalibracionNativa.MEDIDO_GIRO_DER != 0) partes.add("giro derecha")
        return "Carro sin calibrar (${partes.joinToString(", ")}): se usan valores por defecto y la ruta puede salir chueca. Mide lo que falta en «Calibración del carro»."
    }

    // ── Calibración por partes: fecha de la última vez y giro de prueba ──

    private fun claveFecha(parte: Parte, perfil: ControlLogica.Perfil?) =
        "fecha_${parte.name.lowercase()}" + (if (parte.porPerfil && perfil != null) "_${perfil.name.lowercase()}" else "")

    /** Cuándo se calibró por última vez esa parte (milisegundos desde 1970), o 0 si nunca (o se midió antes de que la app anotara la fecha). Las partes por perfil piden el perfil. */
    fun fecha(parte: Parte, perfil: ControlLogica.Perfil? = null): Long = preferencias.getLong(claveFecha(parte, perfil), 0L)

    /** Anota que esa parte se acaba de calibrar. */
    fun marcarCalibrado(parte: Parte, perfil: ControlLogica.Perfil? = null) {
        preferencias.edit().putLong(claveFecha(parte, perfil), System.currentTimeMillis()).apply()
        Log.i(ETIQUETA, "parte calibrada: ${parte.name}" + (if (parte.porPerfil && perfil != null) " (${perfil.name})" else ""))
    }

    /** La fecha y hora para mostrar ("15 oct 21:30"), o «sin fecha» si no se anotó. */
    fun textoFecha(milisegundos: Long): String =
        if (milisegundos <= 0L) "sin fecha" else SimpleDateFormat("d MMM HH:mm", Locale.forLanguageTag("es-MX")).format(Date(milisegundos)).replace(".", "")

    private fun claveGiroPrueba(lado: Int) = "giro_prueba_${if (lado < 0) "izq" else "der"}"
    private fun claveGiroPruebaDesp(perfil: ControlLogica.Perfil, lado: Int) = "desp_prueba_${perfil.name.lowercase()}_${if (lado < 0) "izq" else "der"}"

    /** El giro de prueba con el que se midió el giro de ese lado, o null si no se anotó. */
    fun giroPruebaDeGiro(lado: Int): GiroPrueba? = GiroPrueba.deCodigo(preferencias.getString(claveGiroPrueba(lado), null))

    fun guardarGiroPruebaDeGiro(lado: Int, prueba: GiroPrueba) {
        preferencias.edit().putString(claveGiroPrueba(lado), prueba.codigo()).apply()
    }

    /** El giro de prueba con el que se midió el desplazamiento de ese lado y perfil, o null si no se anotó. */
    fun giroPruebaDeDesplazamiento(perfil: ControlLogica.Perfil, lado: Int): GiroPrueba? = GiroPrueba.deCodigo(preferencias.getString(claveGiroPruebaDesp(perfil, lado), null))

    fun guardarGiroPruebaDeDesplazamiento(perfil: ControlLogica.Perfil, lado: Int, prueba: GiroPrueba) {
        preferencias.edit().putString(claveGiroPruebaDesp(perfil, lado), prueba.codigo()).apply()
    }

    /** Aviso si el OTRO lado se midió con un giro de prueba distinto del que se va a usar ahora (los ms por grado dependen de la duración: no se deben mezclar sin saberlo), o null. */
    fun avisoMezclaGiro(lado: Int, prueba: GiroPrueba): String? {
        val otro = giroPruebaDeGiro(-lado) ?: return null
        if (otro.mismaQue(prueba)) {
            return null
        }
        return "La ${if (-lado < 0) "izquierda" else "derecha"} se midió con «${otro.texto()}» y esta con «${prueba.texto()}». Los ms por grado cambian con la duración: para que los dos lados sean comparables, mide los dos con el mismo giro de prueba."
    }

    /** Igual para el desplazamiento: el otro lado (mismo perfil) y el giro con el que se midió la calibración del giro de este lado. */
    fun avisoMezclaDesplazamiento(perfil: ControlLogica.Perfil, lado: Int, prueba: GiroPrueba): String? {
        val otro = giroPruebaDeDesplazamiento(perfil, -lado)
        if (otro != null && !otro.mismaQue(prueba)) {
            return "El desplazamiento de la ${if (-lado < 0) "izquierda" else "derecha"} se midió con «${otro.texto()}» y este con «${prueba.texto()}». El desplazamiento por grado cambia con el ángulo: mide los dos lados con el mismo giro de prueba."
        }
        return null
    }
}
