/*
 * ObstaculosNativa.kt — Declara las funciones en C del detector de obstáculos por color de piso.
 *
 * GUÍA DE LECTURA
 *  1. `external` significa "la implementación está en código nativo (C)": está en
 *     app/src/main/cpp/puente.c y llama a nucleo_c/obstaculos.c.
 *  2. La imagen entra como bytes RGB (R, G, B, R, G, B...), fila por fila, de CUALQUIER tamaño
 *     razonable (de 30x30 a 640x480): el ancho y el alto se pasan en cada llamada. El detector
 *     la divide en celdas de 10x10 píxeles; con 240x176 (el modo de la cámara real) son 24x17.
 *  3. analizar() usa la configuración por defecto y devuelve 10 enteros.
 *     analizarConfig() usa una configuración que se puede cambiar en vivo y devuelve además el
 *     tamaño de la rejilla, la franja de referencia, el estado del modelo del piso (varios
 *     colores, aprendizaje, cambio de piso) y la máscara de celdas "no piso" (para dibujarlas).
 *     Los índices están más abajo.
 *  4. Todo lo que depende del tamaño (franja de referencia, fila mínima) va como proporción en
 *     MILÉSIMAS (500 = 0.5), así la misma configuración vale para cualquier tamaño de imagen.
 */
package com.fsi.carro

object ObstaculosNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    /** Tamaño de REFERENCIA (320x240): lo usan el benchmark y las imágenes de prueba. */
    const val ANCHO = 320
    const val ALTO = 240

    /** Tamaño de los cuadros de la cámara real del carro (Freenove en HQVGA). Es el de partida de la pantalla de visión. */
    const val ANCHO_CAMARA = 240
    const val ALTO_CAMARA = 176

    const val LADO_CELDA = 10   // cada celda mide 10x10 píxeles

    /** Columnas y filas de celdas de una imagen de ese tamaño (igual que obstaculos_columnas_de / obstaculos_filas_de). */
    fun columnasDe(ancho: Int) = ancho / LADO_CELDA
    fun filasDe(alto: Int) = alto / LADO_CELDA

    // Posiciones dentro del arreglo que devuelve analizar().
    const val CODIGO = 0            // 0 = bien; negativo = error
    const val HAY_IZQUIERDA = 1
    const val CERCANIA_IZQUIERDA = 2   // 0 = nada, 1 = lejos, 2 = media, 3 = muy cerca
    const val HAY_CENTRO = 3
    const val CERCANIA_CENTRO = 4
    const val HAY_DERECHA = 5
    const val CERCANIA_DERECHA = 6
    const val REF_R = 7             // color de referencia del piso
    const val REF_G = 8
    const val REF_B = 9

    // analizarConfig() devuelve los 10 de arriba y después este encabezado y la máscara.
    const val COLUMNAS_USADAS = 10      // columnas de la rejilla de ESTE análisis
    const val FILAS_USADAS = 11
    const val FILA_MINIMA_USADA = 12    // fila donde empieza la zona de avance
    const val REJILLA_CHICA = 13        // 1 = rejilla chica: funciona pero es más tosca
    const val REF_COLUMNA = 14          // franja de referencia del piso (en celdas)
    const val REF_FILA = 15
    const val REF_NCOLUMNAS = 16
    const val REF_NFILAS = 17

    // Estado del modelo del piso (varios colores, aprendizaje continuo y cambio de piso).
    const val GRUPOS_PISO = 18          // cuántos grupos de color del piso se están usando (1 a 4)
    const val FRANJA_TAPADA = 19        // 1 = la franja de referencia estaba tapada: se usó lo aprendido
    const val PISO_REAPRENDIDO = 20     // 1 = en este cuadro se reaprendió el piso (cambió el piso): avisar
    const val APRENDIO = 21             // 1 = en este cuadro el modelo aprendió (carro avanzando y sin obstáculos)
    const val CELDAS_BORDE = 22         // celdas marcadas solo por la señal de bordes
    const val COLORES_GRUPOS = 23       // R, G, B de hasta 4 grupos del piso: 23..34

    const val CELDAS_ZONA = 35           // celdas confirmadas de cada zona (izquierda, centro, derecha): 35, 36 y 37

    // Diagnóstico del cuadro (registro PISO_DIAG).
    const val BRILLO_MEDIO = 38          // brillo medio de la zona de avance, 0 a 255
    const val GANANCIA_MIL = 39          // factor con que se reajustó el brillo del piso (1000 = no se reajustó)
    const val CELDAS_MARCADAS = 40       // celdas "no piso" confirmadas en la zona de avance
    const val MOTIVO = 41                // bits MOTIVO_* (obstaculos.h: OBS_MOTIVO_*)
    const val MOTIVO_OBSTACULO = 1
    const val MOTIVO_FRANJA_TAPADA = 2
    const val MOTIVO_REAJUSTE = 4
    const val MOTIVO_PISO_NUEVO = 8
    const val MOTIVO_PISO_OSCURO = 16
    const val MOTIVO_TODO_NO_PISO = 32

    /** Desde aquí viene la máscara: 1 = celda "no piso", fila por fila (índice INICIO_MASCARA + fila * columnas + columna). */
    const val INICIO_MASCARA = 42

    // Posiciones dentro de la configuración (10 enteros). Las proporciones van en milésimas.
    const val CFG_UMBRAL_COLOR = 0
    const val CFG_MIN_CELDAS = 1
    const val CFG_REF_ANCHO_MIL = 2        // ancho de la franja de referencia
    const val CFG_REF_ALTO_MIL = 3         // alto de la franja de referencia
    const val CFG_REF_CENTRO_X_MIL = 4     // centro de la franja a lo ancho (500 = al centro)
    const val CFG_REF_BORDE_INF_MIL = 5    // borde de abajo de la franja (1000 = pegada abajo)
    const val CFG_FILA_MINIMA_MIL = 6      // fila mínima de la zona de avance (500 = mitad inferior); las de arriba se ignoran
    const val CFG_GRUPOS_PISO = 7          // K: cuántos colores distintos puede tener el piso (1 a 4)
    const val CFG_APRENDIZAJE_MIL = 8      // velocidad de aprendizaje del modelo del piso (milésimas por cuadro; 0 = no aprende)
    const val CFG_USAR_BORDES = 9          // 1 = señal extra de bordes (apagada por defecto: ver docs/obstaculos.md)
    const val CFG_MEJORAS_PISO = 10        // 1 (por defecto) = brillo oscuro, grupos con apoyo mínimo y reajuste de brillo (pisos negros y de 2 colores)
    const val CFG_PISO_FIJO = 11           // 1 = durante una ruta: no se reaprende el piso ni se agregan colores
    const val ENTEROS_CONFIG = 12

    // Resultado de aprenderPisoTerminar (23 enteros).
    const val APR_CODIGO = 0
    const val APR_GRUPOS = 1
    const val APR_BRILLO_MEDIO = 2
    const val APR_MUY_OSCURO = 3
    const val APR_MUY_VARIADO = 4
    const val APR_DESCARTADO_PCT = 5
    const val APR_CUADROS = 6
    const val APR_COLORES = 7        // R, G, B de hasta 4 grupos: 7..18
    const val APR_PESOS_PCT = 19     // % de celdas de cada color: 19..22

    /** Texto corto de los motivos de un cuadro (para PISO_DIAG). */
    fun textoMotivo(motivo: Int): String {
        val partes = ArrayList<String>()
        if (motivo and MOTIVO_OBSTACULO != 0) partes.add("obstaculo")
        if (motivo and MOTIVO_FRANJA_TAPADA != 0) partes.add("franja_tapada")
        if (motivo and MOTIVO_REAJUSTE != 0) partes.add("reajuste_brillo")
        if (motivo and MOTIVO_PISO_NUEVO != 0) partes.add("piso_nuevo")
        if (motivo and MOTIVO_PISO_OSCURO != 0) partes.add("piso_oscuro")
        if (motivo and MOTIVO_TODO_NO_PISO != 0) partes.add("todo_no_piso")
        return if (partes.isEmpty()) "ninguno" else partes.joinToString("+")
    }

    // Códigos de error de obstaculos.h.
    const val ERR_IMAGEN_NULA = -1
    const val ERR_TAMANO = -2
    const val ERR_REFERENCIA = -3
    const val ERR_CONFIG = -4

    /** Analiza una imagen RGB (ancho x alto x 3 bytes) con la configuración por defecto: 10 enteros. */
    external fun analizar(rgb: ByteArray, ancho: Int, alto: Int): IntArray

    /**
     * Igual, pero con una configuración propia (10 enteros), CON MEMORIA del piso entre cuadros y con el detalle de celdas:
     * INICIO_MASCARA + filas*columnas enteros. `avanzando` dice si el carro está avanzando (solo entonces el modelo aprende)
     * y `ahoraMs` es un reloj en milisegundos. Solo se llama desde el hilo de la cámara.
     */
    external fun analizarConfig(rgb: ByteArray, ancho: Int, alto: Int, config: IntArray, avanzando: Boolean, ahoraMs: Long): IntArray

    /** Empieza a aprender el piso: desde el siguiente cuadro cada cuadro analizado se suma (el carro debe estar QUIETO). Solo desde el hilo de la cámara. */
    external fun aprenderPisoIniciar()

    /** Termina de aprender el piso con lo acumulado y deja ese modelo (23 enteros, posiciones APR_*). Solo desde el hilo de la cámara. */
    external fun aprenderPisoTerminar(config: IntArray): IntArray

    /** Olvida el piso aprendido: el siguiente cuadro vuelve a aprender los colores del piso desde la franja de referencia. */
    external fun reiniciarModelo()

    /** Configuración por defecto del código en C (10 enteros). */
    external fun configDefecto(): IntArray
}
