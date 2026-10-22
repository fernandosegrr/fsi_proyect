/*
 * RutasPredefinidas.kt — Rutas predefinidas de prueba (A7) para ejecución directa y automatización por ADB.
 */
package com.fsi.carro

object RutasPredefinidas {
    const val ID_RECTA_150 = "recta_150"
    const val ID_RECTA_200 = "recta_200"
    const val ID_RUTA_L = "ruta_l"
    const val ID_OBSTACULO = "obstaculo"

    const val NOMBRE_RECTA_150 = "Recta 150 cm"
    const val NOMBRE_RECTA_200 = "Recta 200 cm"
    const val NOMBRE_RUTA_L = "Ruta en L (100 cm, giro 90° a la derecha, 100 cm)"
    const val NOMBRE_OBSTACULO = "Prueba de obstáculo (1 m)"

    // Base de 8 tramos (160 cm) ajustado a 150 cm exactos via ponerLargoRectaMm(1500)
    val CELDAS_RECTA_150 = intArrayOf(2, 8, 2, 7, 2, 6, 2, 5, 2, 4, 2, 3, 2, 2, 2, 1, 2, 0)

    // 10 tramos de 20 cm = 200 cm exactos
    val CELDAS_RECTA_200 = intArrayOf(2, 10, 2, 9, 2, 8, 2, 7, 2, 6, 2, 5, 2, 4, 2, 3, 2, 2, 2, 1, 2, 0)

    // 5 tramos al norte (100 cm), giro 90° a la derecha en (2,1), 5 tramos al este (100 cm)
    val CELDAS_RUTA_L = intArrayOf(2, 6, 2, 5, 2, 4, 2, 3, 2, 2, 2, 1, 3, 1, 4, 1, 5, 1, 6, 1, 7, 1)

    // 5 tramos al norte (100 cm = 1 m)
    val CELDAS_OBSTACULO = ModoSemiautonomo.RUTA_PRUEBA_OBSTACULO

    data class RutaInfo(
        val id: String,
        val nombre: String,
        val celdas: IntArray,
        val largoRectaMm: Int = 0
    )

    val TODAS = listOf(
        RutaInfo(ID_RECTA_150, NOMBRE_RECTA_150, CELDAS_RECTA_150, largoRectaMm = 1500),
        RutaInfo(ID_RECTA_200, NOMBRE_RECTA_200, CELDAS_RECTA_200),
        RutaInfo(ID_RUTA_L, NOMBRE_RUTA_L, CELDAS_RUTA_L),
        RutaInfo(ID_OBSTACULO, NOMBRE_OBSTACULO, CELDAS_OBSTACULO)
    )

    fun buscarPorIdONombre(clave: String?): RutaInfo? {
        if (clave == null) return null
        val limpia = clave.trim().lowercase()
        return TODAS.firstOrNull {
            it.id.lowercase() == limpia ||
            it.nombre.lowercase() == limpia ||
            (limpia.contains("150") && it.id == ID_RECTA_150) ||
            (limpia.contains("200") && it.id == ID_RECTA_200) ||
            ((limpia == "l" || limpia == "ruta_l" || limpia.contains("en l")) && it.id == ID_RUTA_L) ||
            ((limpia.contains("obstaculo") || limpia.contains("obstáculo")) && it.id == ID_OBSTACULO)
        }
    }
}
