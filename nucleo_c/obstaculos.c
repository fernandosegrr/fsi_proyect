/*
 * obstaculos.c — Detector de obstáculos por color de piso (ver obstaculos.h).
 *
 * GUÍA DE LECTURA
 *  1. promediar_celdas(): convierte la imagen de 320x240 en 32x24 colores promedio.
 *  2. calcular_referencia(): promedia la franja de abajo al centro (el piso libre) y
 *     comprueba que sea uniforme.
 *  3. es_piso(): decide si el color de una celda es piso (ver el comentario de la función).
 *  4. marcar_candidatas() y confirmar_celdas(): marca las celdas "no piso" y descarta las
 *     aisladas (ruido).
 *  5. resumir_zonas(): cuenta las celdas por zona (izquierda, centro, derecha) en la
 *     mitad inferior y calcula la cercanía.
 *  6. obstaculos_analizar_con() junta todo en ese orden con una ConfigObstaculos (los parámetros
 *     que se ajustan); obstaculos_analizar() es lo mismo con la configuración por defecto.
 *  Solo enteros y arreglos de tamaño fijo: sin malloc, sin recursión.
 */

#include <stddef.h>
#include "obstaculos.h"

/* Color promedio de cada celda: [fila][columna][R, G, B]. */
typedef unsigned char MapaColores[OBS_FILAS][OBS_COLUMNAS][3];

/* ── Utilidades ────────────────────────────────────────────────────────── */

static int valor_absoluto(int x)
{
    return (x < 0) ? -x : x;
}

/* Suma de los tres canales: sirve como "brillo" (0 a 765). */
static int brillo(const int color[3])
{
    return color[0] + color[1] + color[2];
}

/* Suma de las diferencias absolutas por canal entre dos colores (0 a 765). */
static int distancia_color(const int a[3], const int b[3])
{
    return valor_absoluto(a[0] - b[0]) + valor_absoluto(a[1] - b[1]) + valor_absoluto(a[2] - b[2]);
}

/* Diferencia entre los colores NORMALIZADOS por brillo (0 a 512): cada canal se divide
 * entre la suma R+G+B (escalado a 256), así dos grises de distinta luz dan casi lo mismo.
 * Hay que haber comprobado antes que ambas sumas sean mayores que cero. */
static int distancia_croma(const int a[3], const int b[3])
{
    int suma_a = brillo(a);
    int suma_b = brillo(b);
    int total = 0;
    int c;

    for (c = 0; c < 3; c++) {
        total += valor_absoluto(a[c] * 256 / suma_a - b[c] * 256 / suma_b);
    }
    return total;
}

/* ── Paso 1: promedio de cada celda ────────────────────────────────────── */

static void promediar_celdas(const unsigned char *rgb, MapaColores celdas)
{
    int fila, columna, y, x, c;

    for (fila = 0; fila < OBS_FILAS; fila++) {
        for (columna = 0; columna < OBS_COLUMNAS; columna++) {
            int suma[3] = { 0, 0, 0 };

            for (y = 0; y < OBS_LADO_CELDA; y++) {
                for (x = 0; x < OBS_LADO_CELDA; x++) {
                    int pixel_y = fila * OBS_LADO_CELDA + y;
                    int pixel_x = columna * OBS_LADO_CELDA + x;
                    const unsigned char *p = rgb + (pixel_y * OBS_ANCHO + pixel_x) * 3;

                    suma[0] += p[0];
                    suma[1] += p[1];
                    suma[2] += p[2];
                }
            }
            for (c = 0; c < 3; c++) {
                celdas[fila][columna][c] = (unsigned char)(suma[c] / (OBS_LADO_CELDA * OBS_LADO_CELDA));
            }
        }
    }
}

/* ── Paso 2: color de referencia del piso ──────────────────────────────── */

/* Copia el color de una celda a un arreglo de enteros. */
static void leer_celda(MapaColores celdas, int fila, int columna, int color[3])
{
    color[0] = celdas[fila][columna][0];
    color[1] = celdas[fila][columna][1];
    color[2] = celdas[fila][columna][2];
}

/* Promedia las celdas de la franja de referencia (la que indica la configuración). Devuelve
 * OBS_OK, u OBS_ERR_REFERENCIA si alguna celda de la franja se aleja demasiado del promedio
 * (franja contaminada). */
static int calcular_referencia(MapaColores celdas, const ConfigObstaculos *config, int referencia[3])
{
    int suma[3] = { 0, 0, 0 };
    int cuantas = config->ref_columnas * config->ref_filas;
    int fila, columna, c;

    for (fila = config->ref_fila_inicial; fila < config->ref_fila_inicial + config->ref_filas; fila++) {
        for (columna = config->ref_columna_inicial; columna < config->ref_columna_inicial + config->ref_columnas; columna++) {
            for (c = 0; c < 3; c++) {
                suma[c] += celdas[fila][columna][c];
            }
        }
    }
    for (c = 0; c < 3; c++) {
        referencia[c] = suma[c] / cuantas;
    }

    for (fila = config->ref_fila_inicial; fila < config->ref_fila_inicial + config->ref_filas; fila++) {
        for (columna = config->ref_columna_inicial; columna < config->ref_columna_inicial + config->ref_columnas; columna++) {
            int color[3];

            leer_celda(celdas, fila, columna, color);
            if (distancia_color(color, referencia) > MAX_VARIACION_REF) {
                return OBS_ERR_REFERENCIA;
            }
        }
    }
    return OBS_OK;
}

/* ── Paso 3: ¿esta celda es piso? ──────────────────────────────────────── */

/* Devuelve 1 si el color de la celda es piso:
 *   A) casi igual al de referencia (suma de diferencias <= umbral_color), o
 *   B) mismo tono pero distinta luz: color normalizado parecido (<= UMBRAL_CROMA) y
 *      brillo que no difiere más de UMBRAL_BRILLO. B solo se usa si ambos colores
 *      tienen brillo suficiente (SUMA_MIN_CROMA); en la oscuridad el tono no es confiable. */
static int es_piso(const int color[3], const int referencia[3], int umbral_color)
{
    if (distancia_color(color, referencia) <= umbral_color) {
        return 1;
    }
    if (brillo(color) >= SUMA_MIN_CROMA && brillo(referencia) >= SUMA_MIN_CROMA &&
        distancia_croma(color, referencia) <= UMBRAL_CROMA &&
        valor_absoluto(brillo(color) - brillo(referencia)) <= UMBRAL_BRILLO) {
        return 1;
    }
    return 0;
}

/* ── Paso 4: celdas "no piso" y filtro de ruido ────────────────────────── */

/* Marca con 1 las celdas que no parecen piso. */
static void marcar_candidatas(MapaColores celdas, const int referencia[3], int umbral_color,
                              unsigned char candidatas[OBS_FILAS][OBS_COLUMNAS])
{
    int fila, columna;

    for (fila = 0; fila < OBS_FILAS; fila++) {
        for (columna = 0; columna < OBS_COLUMNAS; columna++) {
            int color[3];

            leer_celda(celdas, fila, columna, color);
            candidatas[fila][columna] = (unsigned char)(es_piso(color, referencia, umbral_color) ? 0 : 1);
        }
    }
}

/* Cuenta las vecinas (8 alrededor) marcadas. */
static int contar_vecinas(unsigned char candidatas[OBS_FILAS][OBS_COLUMNAS], int fila, int columna)
{
    int cuenta = 0;
    int df, dc;

    for (df = -1; df <= 1; df++) {
        for (dc = -1; dc <= 1; dc++) {
            int f = fila + df;
            int c = columna + dc;

            if ((df != 0 || dc != 0) && f >= 0 && f < OBS_FILAS && c >= 0 && c < OBS_COLUMNAS) {
                cuenta += candidatas[f][c];
            }
        }
    }
    return cuenta;
}

/* Una celda "no piso" solo cuenta si tiene al menos una vecina "no piso" (descarta el ruido). */
static void confirmar_celdas(unsigned char candidatas[OBS_FILAS][OBS_COLUMNAS],
                             unsigned char confirmadas[OBS_FILAS][OBS_COLUMNAS])
{
    int fila, columna;

    for (fila = 0; fila < OBS_FILAS; fila++) {
        for (columna = 0; columna < OBS_COLUMNAS; columna++) {
            int sola = (contar_vecinas(candidatas, fila, columna) == 0);

            confirmadas[fila][columna] = (unsigned char)((candidatas[fila][columna] && !sola) ? 1 : 0);
        }
    }
}

/* ── Paso 5: zonas y cercanía ──────────────────────────────────────────── */

/* Cercanía (1 a 3) según la fila más baja alcanzada. */
static int cercania_por_fila(int fila_mas_baja)
{
    if (fila_mas_baja >= OBS_FILA_CERCA) {
        return 3;
    }
    if (fila_mas_baja >= OBS_FILA_MEDIA) {
        return 2;
    }
    return 1;
}

/* Cuenta las celdas confirmadas de cada zona (tercios de ancho, mitad inferior). Una zona
 * tiene obstáculo si tiene al menos `min_celdas` celdas. */
static void resumir_zonas(unsigned char confirmadas[OBS_FILAS][OBS_COLUMNAS], int min_celdas,
                          ResultadoObstaculos *resultado)
{
    int fila_mas_baja[OBS_ZONAS] = { -1, -1, -1 };
    int fila, columna, zona;

    for (fila = OBS_FILA_INICIAL_ZONAS; fila < OBS_FILAS; fila++) {
        for (columna = 0; columna < OBS_COLUMNAS; columna++) {
            if (confirmadas[fila][columna]) {
                zona = columna * OBS_ZONAS / OBS_COLUMNAS;   /* 0, 1 o 2 */
                resultado->zona[zona].celdas++;
                fila_mas_baja[zona] = fila;   /* las filas se recorren de arriba abajo */
            }
        }
    }
    for (zona = 0; zona < OBS_ZONAS; zona++) {
        if (resultado->zona[zona].celdas >= min_celdas) {
            resultado->zona[zona].hay_obstaculo = 1;
            resultado->zona[zona].cercania = cercania_por_fila(fila_mas_baja[zona]);
        }
    }
}

/* ── Función principal ─────────────────────────────────────────────────── */

/* Deja el resultado en ceros. */
static void limpiar_resultado(ResultadoObstaculos *resultado)
{
    int zona, fila, columna;

    for (zona = 0; zona < OBS_ZONAS; zona++) {
        resultado->zona[zona].hay_obstaculo = 0;
        resultado->zona[zona].cercania = 0;
        resultado->zona[zona].celdas = 0;
    }
    resultado->ref_r = 0;
    resultado->ref_g = 0;
    resultado->ref_b = 0;
    for (fila = 0; fila < OBS_FILAS; fila++) {
        for (columna = 0; columna < OBS_COLUMNAS; columna++) {
            resultado->no_piso[fila][columna] = 0;
        }
    }
}

void obstaculos_config_defecto(ConfigObstaculos *config)
{
    if (config == NULL) {
        return;
    }
    config->umbral_color = UMBRAL_COLOR;
    config->min_celdas = MIN_CELDAS;
    config->ref_columnas = OBS_REF_COLUMNAS;
    config->ref_filas = OBS_REF_FILAS;
    config->ref_columna_inicial = OBS_REF_COLUMNA_INICIAL;
    config->ref_fila_inicial = OBS_REF_FILA_INICIAL;
}

/* La configuración es válida si los umbrales están en rango y la franja cabe en la imagen. */
static int config_es_valida(const ConfigObstaculos *config)
{
    return config->umbral_color >= 0 && config->umbral_color <= 765 &&
           config->min_celdas >= 1 &&
           config->ref_columnas >= 1 && config->ref_filas >= 1 &&
           config->ref_columna_inicial >= 0 && config->ref_fila_inicial >= 0 &&
           config->ref_columna_inicial + config->ref_columnas <= OBS_COLUMNAS &&
           config->ref_fila_inicial + config->ref_filas <= OBS_FILAS;
}

int obstaculos_analizar_con(const ConfigObstaculos *config, const unsigned char *rgb,
                            int ancho, int alto, ResultadoObstaculos *resultado)
{
    MapaColores celdas;
    unsigned char candidatas[OBS_FILAS][OBS_COLUMNAS];
    int referencia[3];
    int codigo;

    if (resultado == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    limpiar_resultado(resultado);
    if (rgb == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    if (ancho != OBS_ANCHO || alto != OBS_ALTO) {
        return OBS_ERR_TAMANO;
    }
    if (config == NULL || !config_es_valida(config)) {
        return OBS_ERR_CONFIG;
    }

    promediar_celdas(rgb, celdas);
    codigo = calcular_referencia(celdas, config, referencia);
    if (codigo != OBS_OK) {
        return codigo;
    }
    resultado->ref_r = referencia[0];
    resultado->ref_g = referencia[1];
    resultado->ref_b = referencia[2];

    marcar_candidatas(celdas, referencia, config->umbral_color, candidatas);
    confirmar_celdas(candidatas, resultado->no_piso);
    resumir_zonas(resultado->no_piso, config->min_celdas, resultado);
    return OBS_OK;
}

int obstaculos_analizar(const unsigned char *rgb, int ancho, int alto, ResultadoObstaculos *resultado)
{
    ConfigObstaculos config;

    obstaculos_config_defecto(&config);
    return obstaculos_analizar_con(&config, rgb, ancho, alto, resultado);
}

const char *obstaculos_texto_error(int codigo)
{
    switch (codigo) {
    case OBS_OK:                return "Imagen analizada";
    case OBS_ERR_IMAGEN_NULA:   return "No hay imagen";
    case OBS_ERR_TAMANO:        return "La imagen debe ser de 320x240";
    case OBS_ERR_REFERENCIA:    return "El piso de referencia no es uniforme (hay un objeto debajo del carro?)";
    case OBS_ERR_CONFIG:        return "La configuracion no es valida";
    default:                    return "Error desconocido";
    }
}
