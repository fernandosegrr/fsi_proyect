/*
 * CorredorNativa.kt — Declara la función en C del corredor de trayectoria (nucleo_c/corredor.c, ver docs/obstaculos.md, «Corredor»).
 *
 * GUÍA DE LECTURA
 *  1. El corredor es la huella del carro (su ancho más un margen) proyectada en la imagen con perspectiva. Un obstáculo frena o se esquiva
 *     solo si está DENTRO; el que pasa por un lado solo avisa. Viene APAGADO por defecto (ConfigVision.corredorActivo).
 *  2. aplicar() recibe la configuración (7 enteros) y el detalle del detector (ObstaculosNativa.analizarConfig) y devuelve 10 enteros
 *     (posiciones F_*, C_* y P_* más abajo). CamaraObstaculos lo llama en cada cuadro; ModoSemiautonomo le pasa el resultado al navegador.
 */
package com.fsi.carro

object CorredorNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    // Posiciones dentro de la configuración (7 enteros).
    const val CFG_ACTIVO = 0
    const val CFG_ANCHO_CARRO_CM = 1
    const val CFG_ALTURA_CAMARA_CM = 2
    const val CFG_INCLINACION_X10 = 3      // grados por 10 (200 = 20.0)
    const val CFG_FOV_X10 = 4              // campo de visión horizontal, grados por 10 (600 = 60.0)
    const val CFG_MARGEN_CM = 5
    const val CFG_MIN_PESO = 6

    // Posiciones dentro del resultado (10 enteros).
    const val CODIGO = 0            // 0 = bien
    const val FUERA = 1             // 1 a 3: zona izquierda, centro y derecha con obstáculo FUERA del corredor (solo avisa)
    const val CERCANIA = 4          // 4 a 6: cercanía de lo que está dentro del corredor (0 = usar la de siempre)
    const val PESO = 7              // 7 a 9: celdas dentro del corredor

    /** Aplica el corredor al detalle del detector de un cuadro de ancho x alto píxeles. */
    external fun aplicar(config: IntArray, detalle: IntArray, ancho: Int, alto: Int): IntArray
}
