/*
 * ConfigVision.kt — Los parámetros del detector de obstáculos que se ajustan en vivo.
 *
 * GUÍA DE LECTURA
 *  1. Guarda UMBRAL_COLOR, MIN_CELDAS y la fila mínima en SharedPreferences (el almacenamiento de
 *     preferencias de Android), para que lo que se ajustó en VisionActivity también lo use el modo
 *     semiautónomo (Ejecutar ruta) y se conserve al cerrar la app.
 *  2. Los valores por defecto vienen del código en C (ObstaculosNativa.configDefecto).
 *  3. La fila mínima es una PROPORCIÓN del alto (en milésimas: 500 = la mitad inferior), no un
 *     número de fila: así vale para cualquier tamaño de cuadro. En pantalla se muestra en por ciento.
 *  4. También guarda el tamaño de los cuadros que se piden a la cámara del teléfono (por defecto
 *     240x176, el de la cámara real del carro).
 *  5. También guarda cuántos colores puede tener el piso (K, 1 a 4) y la velocidad de aprendizaje del modelo.
 *  6. usarModelo dice si además del detector de color se usa el modelo de objetos (fusión); por defecto SÍ.
 *  7. comoArreglo() da los 10 enteros que espera ObstaculosNativa.analizarConfig.
 */
package com.fsi.carro

import android.content.Context
import android.content.SharedPreferences

class ConfigVision(contexto: Context) {

    companion object {
        // Solo memoria de esta sesión. El extra de adb nunca escribe preferencias.
        var urlPrueba: String? = null
        val camaraDePrueba: Boolean get() = urlPrueba != null
        const val UMBRAL_COLOR_MAXIMO = 300   // tope de la barra deslizable (el máximo del detector es 765)
        const val MIN_CELDAS_MAXIMO = 40
        const val PORCENTAJE_FILA_MAXIMO = 100   // la barra de fila mínima va de 0 a 100 %
        const val GRUPOS_MAXIMO = 4              // K máximo (OBS_MAX_GRUPOS en el código en C)
        const val APRENDIZAJE_MAXIMO_MIL = 100   // tope de la barra de aprendizaje (el máximo del detector es 1000)
        // J14: la franja de referencia de 80 % del ancho incluía el borde izquierdo del propio carro
        // (la cámara lo marca fijo) y el aprendizaje del piso descartaba 8 % con el piso uniforme.
        // Con 60 % la franja toma solo el piso del centro del camino.
        const val REF_ANCHO_MIL_APP = 600
        // J14: en el piso real, con la franja de referencia al 60 %, K=4 seguía descartando sombras
        // del propio carro; con K=3 el piso uniforme quedó con descartado_pct=0. Es un valor de la app;
        // el valor por defecto del núcleo en C sigue siendo 4.
        const val GRUPOS_PISO_APP = 3

        // Valores de partida del corredor (los mismos de corredor.h: ANCHO_CARRO_CM, ALTURA_CAMARA_CM, INCLINACION_GRADOS, FOV_GRADOS...). SIN MEDIR.
        const val CORREDOR_ANCHO_CM = 18
        const val CORREDOR_ALTURA_CM = 10
        const val CORREDOR_INCLINACION_X10 = 200
        const val CORREDOR_FOV_X10 = 600
        const val CORREDOR_MARGEN_CM = 3
        const val CORREDOR_MIN_PESO = 2

        /** Tamaños de cuadro que se pueden pedir a la cámara del teléfono (ancho, alto). */
        val TAMANOS = arrayOf(
            intArrayOf(240, 176),   // HQVGA: el modo de la cámara real del carro
            intArrayOf(320, 240),   // QVGA
            intArrayOf(160, 120)    // QQVGA
        )
    }

    private val preferencias: SharedPreferences = contexto.getSharedPreferences("vision", Context.MODE_PRIVATE)
    private val defecto = ObstaculosNativa.configDefecto()

    var umbralColor: Int
        get() = preferencias.getInt("umbral_color", defecto[ObstaculosNativa.CFG_UMBRAL_COLOR])
        set(valor) = preferencias.edit().putInt("umbral_color", valor).apply()

    var minCeldas: Int
        get() = preferencias.getInt("min_celdas", defecto[ObstaculosNativa.CFG_MIN_CELDAS])
        set(valor) = preferencias.edit().putInt("min_celdas", valor).apply()

    /** Fila mínima de la zona de avance como proporción del alto, en milésimas (500 = la mitad inferior). */
    var filaMinimaMil: Int
        get() = preferencias.getInt("fila_minima_mil", defecto[ObstaculosNativa.CFG_FILA_MINIMA_MIL]).coerceIn(0, 1000)
        set(valor) = preferencias.edit().putInt("fila_minima_mil", valor.coerceIn(0, 1000)).apply()

    /** K: cuántos colores distintos puede tener el piso (baldosas, vetas...). */
    var gruposPiso: Int
        get() = preferencias.getInt("grupos_piso", GRUPOS_PISO_APP).coerceIn(1, GRUPOS_MAXIMO)
        set(valor) = preferencias.edit().putInt("grupos_piso", valor.coerceIn(1, GRUPOS_MAXIMO)).apply()

    /** Velocidad de aprendizaje del modelo del piso, en milésimas por cuadro (0 = no aprende). */
    var aprendizajeMil: Int
        get() = preferencias.getInt("aprendizaje_mil", defecto[ObstaculosNativa.CFG_APRENDIZAJE_MIL]).coerceIn(0, 1000)
        set(valor) = preferencias.edit().putInt("aprendizaje_mil", valor.coerceIn(0, 1000)).apply()

    /** true = se usa también el modelo de objetos (en su propio hilo) y se fusiona con el detector de color. */
    var usarModelo: Boolean
        get() = preferencias.getBoolean("usar_modelo", true)
        set(valor) = preferencias.edit().putBoolean("usar_modelo", valor).apply()

    // ── Modo «cualquier objeto» del modelo de objetos (cualquiera.h): APAGADO por defecto ──
    /** true = el modelo de objetos se usa solo para saber que hay una caja sobre el piso, ignorando la clase (necesita el modelo encendido). */
    var cualquierObjeto: Boolean
        get() = preferencias.getBoolean("cualquier_objeto", false)
        set(valor) = preferencias.edit().putBoolean("cualquier_objeto", valor).apply()

    /** Confianza que debe SUPERAR una caja, en milésimas (150 = 0.15; de 10 a 500). */
    var umbralCualquieraMil: Int
        get() = preferencias.getInt("umbral_cualquiera_mil", CualquieraNativa.UMBRAL_DEFECTO_MIL).coerceIn(CualquieraNativa.UMBRAL_MIN_MIL, CualquieraNativa.UMBRAL_MAX_MIL)
        set(valor) = preferencias.edit().putInt("umbral_cualquiera_mil", valor.coerceIn(CualquieraNativa.UMBRAL_MIN_MIL, CualquieraNativa.UMBRAL_MAX_MIL)).apply()

    /** Los 9 enteros de configuración del modo «cualquier objeto» (CualquieraNativa.CFG_*); los filtros de tamaño y posición son los de cualquiera.h. */
    fun cualquieraComoArreglo(): IntArray = intArrayOf(
        if (cualquierObjeto && usarModelo) 1 else 0, umbralCualquieraMil, 600, 500, 15, 40, 2, 500, 300)

    // ── Corredor de trayectoria (corredor.h): APAGADO por defecto ──
    /** true = solo frena un obstáculo que cae dentro de la huella del carro proyectada en la imagen (el que pasa por un lado solo avisa). */
    var corredorActivo: Boolean
        get() = preferencias.getBoolean("corredor_activo", false)
        set(valor) = preferencias.edit().putBoolean("corredor_activo", valor).apply()

    /** Ancho del carro en cm (las ruedas más anchas). HAY QUE MEDIRLO. */
    var corredorAnchoCm: Int
        get() = preferencias.getInt("corredor_ancho_cm", CORREDOR_ANCHO_CM)
        set(valor) = preferencias.edit().putInt("corredor_ancho_cm", valor.coerceIn(1, 100)).apply()

    /** Altura de la lente de la cámara sobre el piso, en cm. HAY QUE MEDIRLA. */
    var corredorAlturaCm: Int
        get() = preferencias.getInt("corredor_altura_cm", CORREDOR_ALTURA_CM)
        set(valor) = preferencias.edit().putInt("corredor_altura_cm", valor.coerceIn(1, 100)).apply()

    /** Inclinación de la cámara hacia abajo, en grados por 10 (200 = 20.0°). HAY QUE MEDIRLA. */
    var corredorInclinacionX10: Int
        get() = preferencias.getInt("corredor_inclinacion_x10", CORREDOR_INCLINACION_X10)
        set(valor) = preferencias.edit().putInt("corredor_inclinacion_x10", valor.coerceIn(0, 800)).apply()

    /** Campo de visión horizontal de la cámara del carro, en grados por 10 (600 = 60°, sin medir). */
    var corredorFovX10: Int
        get() = preferencias.getInt("corredor_fov_x10", CORREDOR_FOV_X10)
        set(valor) = preferencias.edit().putInt("corredor_fov_x10", valor.coerceIn(200, 1500)).apply()

    /** Margen lateral a cada lado de la huella del carro, en cm. */
    var corredorMargenCm: Int
        get() = preferencias.getInt("corredor_margen_cm", CORREDOR_MARGEN_CM)
        set(valor) = preferencias.edit().putInt("corredor_margen_cm", valor.coerceIn(0, 50)).apply()

    /** Los 7 enteros de configuración del corredor (CorredorNativa.CFG_*). */
    fun corredorComoArreglo(): IntArray = intArrayOf(
        if (corredorActivo) 1 else 0, corredorAnchoCm, corredorAlturaCm, corredorInclinacionX10, corredorFovX10, corredorMargenCm, CORREDOR_MIN_PESO)

    // ── Flujo del piso (movimiento_piso.h): APAGADO por defecto ──
    /** true = se compara cada cuadro con uno de ~200 ms antes mientras el carro avanza (registro FLUJO_DIAG) y el flujo puede bajar un falso positivo de color lejano. */
    var flujoActivo: Boolean
        get() = preferencias.getBoolean("flujo_activo", false)
        set(valor) = preferencias.edit().putBoolean("flujo_activo", valor).apply()

    /** Velocidad hacia adelante del perfil AUTO en mm/s (193 = 19.3 cm/s medidos con PWM 210; ver docs/calibracion.md). */
    var flujoVelocidadMmS: Int
        get() = preferencias.getInt("flujo_velocidad_mm_s", 193)
        set(valor) = preferencias.edit().putInt("flujo_velocidad_mm_s", valor.coerceIn(10, 1000)).apply()

    /** Los 13 enteros de configuración del flujo (FlujoNativa.CFG_*): los del código en C con el activo y la velocidad de aquí. */
    fun flujoComoArreglo(): IntArray {
        val c = FlujoNativa.configDefecto()
        c[FlujoNativa.CFG_ACTIVO] = if (flujoActivo) 1 else 0
        c[FlujoNativa.CFG_VELOCIDAD_MM_S] = flujoVelocidadMmS
        return c
    }

    /** true = en la pantalla de control los obstáculos salen de la cámara y no de los botones simulados (se recuerda al volver a abrir). */
    var obstaculosCamara: Boolean
        get() = preferencias.getBoolean("obstaculos_camara", false)
        set(valor) = preferencias.edit().putBoolean("obstaculos_camara", valor).apply()

    /** Modo de video de la cámara del carro (ModoVideo): jpeg (el predeterminado), raw o celdas. Si un modo falla, la app vuelve a jpeg sola. */
    var modoVideo: String
        get() = ModoVideo.valido(preferencias.getString("modo_video", ModoVideo.JPEG))
        set(valor) = preferencias.edit().putString("modo_video", ModoVideo.valido(valor)).apply()

    /** true = los cuadros salen de la cámara DEL CARRO (video por WiFi, VideoCarro); false = de la cámara del teléfono. */
    var fuenteCarro: Boolean
        get() = camaraDePrueba || preferencias.getBoolean("fuente_carro", false)
        set(valor) = preferencias.edit().putBoolean("fuente_carro", valor).apply()

    /** Dirección del video de la cámara del carro. */
    var urlVideoCarro: String
        get() = urlPrueba ?: preferencias.getString("url_video_carro", VideoCarro.URL_DEFECTO) ?: VideoCarro.URL_DEFECTO
        set(valor) = preferencias.edit().putString("url_video_carro", valor).apply()

    /** Apertura normal: corrige también la URL falsa que guardaban versiones antiguas. */
    fun direccionRealAlAbrir() {
        preferencias.edit().remove("url_video_carro").apply()
    }

    /** Índice en TAMANOS del tamaño de cuadro que se pide a la cámara del teléfono. */
    var indiceTamano: Int
        get() = preferencias.getInt("indice_tamano", 0).coerceIn(0, TAMANOS.size - 1)
        set(valor) = preferencias.edit().putInt("indice_tamano", valor.coerceIn(0, TAMANOS.size - 1)).apply()

    val anchoCuadro: Int get() = TAMANOS[indiceTamano][0]
    val altoCuadro: Int get() = TAMANOS[indiceTamano][1]

    /** Vuelve a los valores por defecto del código en C. */
    fun restablecer() {
        umbralColor = defecto[ObstaculosNativa.CFG_UMBRAL_COLOR]
        minCeldas = defecto[ObstaculosNativa.CFG_MIN_CELDAS]
        filaMinimaMil = defecto[ObstaculosNativa.CFG_FILA_MINIMA_MIL]
        gruposPiso = defecto[ObstaculosNativa.CFG_GRUPOS_PISO]
        aprendizajeMil = defecto[ObstaculosNativa.CFG_APRENDIZAJE_MIL]
    }

    /** Los 12 enteros de configuración (la franja de referencia queda con el valor por defecto del código en C: la tira ancha). El 12.º (piso fijo) lo cambia
     *  quien sabe si hay una ruta en marcha (EjecutarRutaFragment). */
    fun comoArreglo(): IntArray {
        val config = defecto.copyOf(ObstaculosNativa.ENTEROS_CONFIG)
        config[ObstaculosNativa.CFG_MEJORAS_PISO] = 1
        config[ObstaculosNativa.CFG_PISO_FIJO] = 0
        config[ObstaculosNativa.CFG_UMBRAL_COLOR] = umbralColor
        config[ObstaculosNativa.CFG_MIN_CELDAS] = minCeldas
        config[ObstaculosNativa.CFG_FILA_MINIMA_MIL] = filaMinimaMil
        config[ObstaculosNativa.CFG_GRUPOS_PISO] = gruposPiso
        config[ObstaculosNativa.CFG_APRENDIZAJE_MIL] = aprendizajeMil
        config[ObstaculosNativa.CFG_REF_ANCHO_MIL] = REF_ANCHO_MIL_APP
        return config
    }
}
