/*
 * CalibracionGuardada.kt — Guarda y lee la calibración del carro en el teléfono.
 *
 * GUÍA DE LECTURA
 *  1. Se guarda como TEXTO ("clave=valor", una por línea; el mismo que produce el código en C) en
 *     SharedPreferences ("calibracion"). Es el mismo texto que se exporta: se puede copiar,
 *     pegar en otro teléfono o leer a mano.
 *  2. guardar() además escribe el archivo calibracion.txt en la carpeta de archivos de la app, para
 *     sacarlo con adb (ver docs/calibracion.md). Nunca va al repositorio.
 *  3. leer() devuelve la calibración guardada o, si no hay (o el texto no es válido), la de por
 *     defecto. estaCompleta() dice si todo se midió (si no, la pantalla de control avisa).
 *  4. aplicarAlNavegador() se la pasa al navegador en C (se usa al cargar la siguiente ruta).
 *  5. AJUSTE FINO DEL GIRO: además de la calibración hay un FACTOR de corrección del giro por lado (izquierda y derecha) y por perfil (AUTO y MANUAL), en milésimas
 *     (1000 = sin corrección), que sale de la prueba «Giro de 90°». leer() devuelve la calibración tal como se midió; leerAjustada() le aplica el factor y es la que usan el
 *     navegador, la simulación y la prueba del propio giro. Al guardar una calibración con los giros recién medidos los factores vuelven a 1000.
 *  6. AJUSTE FINO DE LA RECTA: el BALANCE es el factor (milésimas, 800 a 1000) con que el firmware reduce la potencia del lado izquierdo, por perfil. El teléfono guarda el que salió
 *     de la prueba «Recta» (balance()) y se lo manda al carro al conectar (balancesParaEnviar); el carro lo guarda también en su memoria. balanceCalibrado() es el valor con el que la
 *     prueba terminó con 1 cm o menos de desvío (0 = nunca se calibró): el simulador lo usa para saber cuánto empuja de más el lado izquierdo.
 */
package com.fsi.carro

import android.content.Context
import android.util.Log
import java.io.File

class CalibracionGuardada(contexto: Context) {

    companion object {
        const val ETIQUETA = "CALIBRACION"
        const val NOMBRE_ARCHIVO = "calibracion.txt"
    }

    private val aplicacion = contexto.applicationContext
    private val preferencias = aplicacion.getSharedPreferences("calibracion", Context.MODE_PRIVATE)

    /** La calibración guardada (10 enteros) o la de por defecto. */
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
        preferencias.edit().remove(claveGiro(perfil, CalibracionNativa.LADO_IZQUIERDA)).remove(claveGiro(perfil, CalibracionNativa.LADO_DERECHA)).apply()
        Log.i(ETIQUETA, "factores de giro ${perfil.name} restaurados a 1000")
    }

    /** true si algún lado de algún perfil tiene un factor distinto de 1000. */
    fun hayFactoresGiro(): Boolean = ControlLogica.Perfil.values().any { p ->
        factorGiro(p, CalibracionNativa.LADO_IZQUIERDA) != CalibracionNativa.FACTOR_GIRO_DEFECTO || factorGiro(p, CalibracionNativa.LADO_DERECHA) != CalibracionNativa.FACTOR_GIRO_DEFECTO
    }

    /** La calibración con el factor de giro del perfil aplicado (AUTO por defecto: es la que usan las rutas y el simulador). Si el factor deja la calibración fuera de rango, la medida. */
    fun leerAjustada(perfil: ControlLogica.Perfil = ControlLogica.Perfil.AUTO): IntArray {
        val medida = leer()
        val (codigo, ajustada) = CalibracionNativa.desempaquetar(CalibracionNativa.conFactorGiro(
            medida, factorGiro(perfil, CalibracionNativa.LADO_IZQUIERDA), factorGiro(perfil, CalibracionNativa.LADO_DERECHA)))
        return if (codigo == CalibracionNativa.OK) ajustada else medida
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
        preferencias.edit().remove(claveBalance(perfil)).remove(claveBalanceCalibrado(perfil)).apply()
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
        else "Retroceso sin calibrar: al retroceder para reintentar un esquive se usa la velocidad del avance. Mídelo en «Calibración del carro» (paso 6, opcional)."

    /** true si ya se midió todo (arranque, avance y los dos giros). */
    fun estaCompleta(): Boolean = CalibracionNativa.faltantes(leer()) == 0

    /** Guarda la calibración y escribe el archivo de texto. Devuelve false si no es válida. */
    fun guardar(calibracion: IntArray): Boolean {
        val texto = CalibracionNativa.aTexto(calibracion)
        if (texto.isEmpty()) {
            return false
        }
        preferencias.edit().putString("texto", texto).apply()
        try {
            File(aplicacion.getExternalFilesDir(null), NOMBRE_ARCHIVO).writeText(texto)
        } catch (e: Exception) {
            Log.w(ETIQUETA, "no se pudo escribir $NOMBRE_ARCHIVO: ${e.message}")
        }
        Log.i(ETIQUETA, "guardada: " + texto.replace('\n', ' '))
        return true
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

    /** Le entrega la calibración guardada (con el factor de giro del perfil AUTO) al navegador, para la próxima ruta. */
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
        return "Carro sin calibrar (${partes.joinToString(", ")}): se usan valores por defecto y la ruta puede salir chueca. Mide con «Calibración del carro»."
    }
}
