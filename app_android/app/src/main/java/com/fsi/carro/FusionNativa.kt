/*
 * FusionNativa.kt — Declara la función en C que une el detector de color con el modelo de objetos
 * (nucleo_c/fusion.c; ver docs/fusion.md).
 *
 * GUÍA DE LECTURA
 *  1. decidir() recibe lo que vio el detector (el arreglo de ObstaculosNativa.analizarConfig), las cajas del
 *     modelo de objetos y los relojes. Devuelve 20 enteros (índices R_* más abajo): las zonas ya fusionadas
 *     (índices 1 a 6, el mismo formato que el resumen del detector, así el navegador las usa igual), la severidad,
 *     quién decidió (detector, modelo o ambos), avisos y la clase del objeto que está en el camino.
 *  2. Las cajas van en un solo arreglo [n, (clase, confianza_mil, xmin, ymin, xmax, ymax) * n] con las
 *     coordenadas en MILÉSIMAS del cuadro (0 a 1000, origen arriba a la izquierda).
 *  3. configConClases() arma la configuración con las clases de interés por NOMBRE (persona, gato, perro...), buscándolas
 *     en las etiquetas del modelo.
 */
package com.fsi.carro

object FusionNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    // Posiciones de la configuración (10 enteros).
    const val CFG_VIGENCIA_MS = 0
    const val CFG_PALABRAS_CLASES = 7   // 7, 8 y 9: los bits de las clases de interés (96 clases)

    // Posiciones del resultado (20 enteros).
    const val R_CODIGO = 0
    const val R_SEVERIDAD = 10
    const val R_FUENTE = 11
    const val R_DETECTOR_SOLO = 12
    const val R_MODELO_VIGENTE = 13
    const val R_EDAD_MODELO_MS = 14
    const val R_AVISO = 15
    const val R_AVISO_CLASE = 16
    const val R_AVISO_ZONA = 17
    const val R_AVISO_CERCANIA = 18
    const val R_CLASE_EN_CAMINO = 19
    const val ENTEROS_RESULTADO = 20

    const val FUENTE_NINGUNA = 0
    const val FUENTE_DETECTOR = 1
    const val FUENTE_MODELO = 2
    const val FUENTE_AMBOS = 3

    const val SEV_NINGUNA = 0
    const val SEV_AVISO = 1
    const val SEV_FRENAR = 2
    const val SEV_PARAR = 3

    const val ENTEROS_POR_CAJA = 6

    /** Nombres (como salen en las etiquetas del modelo) de las clases de interés por defecto. */
    val CLASES_DE_INTERES = listOf("person", "cat", "dog", "bottle", "chair", "dining table")

    /** Configuración por defecto del código en C (10 enteros). */
    external fun configDefecto(): IntArray

    /**
     * Decide con el resultado del detector (el arreglo de analizarConfig), las cajas del modelo, la hora en que
     * el modelo vio el cuadro (`marcaMs`) y la hora de ahora (`ahoraMs`; el mismo reloj, SystemClock.elapsedRealtime()).
     */
    external fun decidir(config: IntArray, detector: IntArray, cajas: IntArray, marcaMs: Long, ahoraMs: Long): IntArray

    /** La configuración por defecto con las clases de interés puestas por NOMBRE (las que no estén en las etiquetas se saltan). */
    fun configConClases(etiquetas: List<String>, nombres: List<String> = CLASES_DE_INTERES): IntArray {
        val config = configDefecto()
        for (i in 0 until 3) {
            config[CFG_PALABRAS_CLASES + i] = 0
        }
        for (nombre in nombres) {
            val clase = etiquetas.indexOf(nombre)
            if (clase in 0 until 96) {
                config[CFG_PALABRAS_CLASES + clase / 32] = config[CFG_PALABRAS_CLASES + clase / 32] or (1 shl (clase % 32))
            }
        }
        return config
    }

    /** Texto en español de la fuente que decidió. */
    fun textoFuente(fuente: Int): String = when (fuente) {
        FUENTE_DETECTOR -> "detector"
        FUENTE_MODELO -> "modelo"
        FUENTE_AMBOS -> "ambos"
        else -> "nadie"
    }
}
