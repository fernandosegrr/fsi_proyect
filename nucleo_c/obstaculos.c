/*
 * obstaculos.c — Detector de obstáculos por color de piso (ver obstaculos.h).
 *
 * GUÍA DE LECTURA
 *  1. Rejilla: el tamaño de la imagen se pasa en cada llamada; de ahí salen las columnas y
 *     filas de celdas de 10x10 píxeles (calcular_rejilla). Nada supone 320x240.
 *  2. promediar_celdas(): convierte la imagen en un color promedio por celda.
 *  3. calcular_franja() y calcular_referencia(): ubican la franja del piso (abajo al centro,
 *     calculada como proporción de la rejilla), la promedian y comprueban que sea uniforme.
 *  4. es_piso(): decide si el color de una celda es piso (ver el comentario de la función).
 *  5. marcar_candidatas() y confirmar_celdas(): marca las celdas "no piso" y descarta las
 *     aisladas (ruido).
 *  6. resumir_zonas(): cuenta las celdas por zona (izquierda, centro, derecha) desde la fila
 *     mínima (proporción del alto, por defecto la mitad inferior) hacia abajo y calcula la
 *     cercanía.
 *  7. obstaculos_analizar_con() junta todo en ese orden con una ConfigObstaculos (los parámetros
 *     que se ajustan); obstaculos_analizar() es lo mismo con la configuración por defecto.
 *  Solo enteros y arreglos de tamaño fijo: sin malloc, sin recursión.
 */

#include <stddef.h>
#include "obstaculos.h"

/* Color promedio de cada celda: [fila][columna][R, G, B]. Solo se usa la parte [filas][columnas]. */
typedef unsigned char MapaColores[OBS_MAX_FILAS][OBS_MAX_COLUMNAS][3];
typedef unsigned char MapaCeldas[OBS_MAX_FILAS][OBS_MAX_COLUMNAS];

/* Tamaño de la cuadrícula del análisis actual, y dónde cae la franja de referencia. */
typedef struct {
    int ancho, alto;            /* tamaño de la imagen en píxeles */
    int columnas, filas;        /* celdas de la cuadrícula */
    int ref_columna, ref_fila;  /* primera celda de la franja de referencia */
    int ref_ncolumnas, ref_nfilas;
} Rejilla;

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

/* Convierte una proporción en milésimas a celdas: total * milesimas / 1000 (hacia abajo). */
static int proporcion(int total, int milesimas)
{
    return total * milesimas / 1000;
}

static int limitar_entre(int valor, int minimo, int maximo)
{
    if (valor < minimo) {
        return minimo;
    }
    return (valor > maximo) ? maximo : valor;
}

int obstaculos_columnas_de(int ancho)
{
    return (ancho > 0) ? ancho / OBS_LADO_CELDA : 0;
}

int obstaculos_filas_de(int alto)
{
    return (alto > 0) ? alto / OBS_LADO_CELDA : 0;
}

/* ── Paso 1: la rejilla según el tamaño de la imagen ───────────────────── */

/* Revisa el tamaño y calcula la cuadrícula. Devuelve OBS_OK u OBS_ERR_TAMANO (ancho o alto no
 * positivos, más grandes que el máximo o con menos de OBS_MIN_COLUMNAS x OBS_MIN_FILAS celdas). */
static int calcular_rejilla(int ancho, int alto, Rejilla *rejilla)
{
    if (ancho <= 0 || alto <= 0 || ancho > OBS_MAX_COLUMNAS * OBS_LADO_CELDA || alto > OBS_MAX_FILAS * OBS_LADO_CELDA) {
        return OBS_ERR_TAMANO;
    }
    rejilla->ancho = ancho;
    rejilla->alto = alto;
    rejilla->columnas = obstaculos_columnas_de(ancho);
    rejilla->filas = obstaculos_filas_de(alto);
    if (rejilla->columnas < OBS_MIN_COLUMNAS || rejilla->filas < OBS_MIN_FILAS ||
        rejilla->columnas > OBS_MAX_COLUMNAS || rejilla->filas > OBS_MAX_FILAS) {
        return OBS_ERR_TAMANO;
    }
    return OBS_OK;
}

/* Ubica la franja de referencia dentro de la rejilla a partir de las proporciones de la config:
 * su tamaño es una fracción de las columnas y filas (al menos 1 celda) y se coloca centrada en
 * `ref_centro_x_mil` y con su borde de abajo en `ref_borde_inf_mil`; si se saldría de la imagen,
 * se corre hacia adentro. */
static void calcular_franja(const ConfigObstaculos *config, Rejilla *rejilla)
{
    int ncolumnas = limitar_entre(proporcion(rejilla->columnas, config->ref_ancho_mil), 1, rejilla->columnas);
    int nfilas = limitar_entre(proporcion(rejilla->filas, config->ref_alto_mil), 1, rejilla->filas);
    int centro = proporcion(rejilla->columnas, config->ref_centro_x_mil);
    int borde = proporcion(rejilla->filas, config->ref_borde_inf_mil);

    rejilla->ref_ncolumnas = ncolumnas;
    rejilla->ref_nfilas = nfilas;
    rejilla->ref_columna = limitar_entre(centro - ncolumnas / 2, 0, rejilla->columnas - ncolumnas);
    rejilla->ref_fila = limitar_entre(borde - nfilas, 0, rejilla->filas - nfilas);
}

/* ── Paso 2: promedio de cada celda ────────────────────────────────────── */

static void promediar_celdas(const unsigned char *rgb, const Rejilla *rejilla, MapaColores celdas)
{
    int fila, columna, y, x, c;

    for (fila = 0; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int suma[3] = { 0, 0, 0 };

            for (y = 0; y < OBS_LADO_CELDA; y++) {
                for (x = 0; x < OBS_LADO_CELDA; x++) {
                    int pixel_y = fila * OBS_LADO_CELDA + y;
                    int pixel_x = columna * OBS_LADO_CELDA + x;
                    const unsigned char *p = rgb + (pixel_y * rejilla->ancho + pixel_x) * 3;

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

/* ── Paso 3: color de referencia del piso ──────────────────────────────── */

/* Copia el color de una celda a un arreglo de enteros. */
static void leer_celda(MapaColores celdas, int fila, int columna, int color[3])
{
    color[0] = celdas[fila][columna][0];
    color[1] = celdas[fila][columna][1];
    color[2] = celdas[fila][columna][2];
}

/* Promedia las celdas de la franja de referencia. Devuelve OBS_OK, u OBS_ERR_REFERENCIA si
 * alguna celda de la franja se aleja demasiado del promedio (franja contaminada). */
static int calcular_referencia(MapaColores celdas, const Rejilla *rejilla, int referencia[3])
{
    int suma[3] = { 0, 0, 0 };
    int cuantas = rejilla->ref_ncolumnas * rejilla->ref_nfilas;
    int fila, columna, c;

    for (fila = rejilla->ref_fila; fila < rejilla->ref_fila + rejilla->ref_nfilas; fila++) {
        for (columna = rejilla->ref_columna; columna < rejilla->ref_columna + rejilla->ref_ncolumnas; columna++) {
            for (c = 0; c < 3; c++) {
                suma[c] += celdas[fila][columna][c];
            }
        }
    }
    for (c = 0; c < 3; c++) {
        referencia[c] = suma[c] / cuantas;
    }

    for (fila = rejilla->ref_fila; fila < rejilla->ref_fila + rejilla->ref_nfilas; fila++) {
        for (columna = rejilla->ref_columna; columna < rejilla->ref_columna + rejilla->ref_ncolumnas; columna++) {
            int color[3];

            leer_celda(celdas, fila, columna, color);
            if (distancia_color(color, referencia) > MAX_VARIACION_REF) {
                return OBS_ERR_REFERENCIA;
            }
        }
    }
    return OBS_OK;
}

/* ── Paso 4: ¿esta celda es piso? ──────────────────────────────────────── */

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

/* ── Paso 5: celdas "no piso" y filtro de ruido ────────────────────────── */

/* Marca con 1 las celdas que no parecen piso. */
static void marcar_candidatas(MapaColores celdas, const Rejilla *rejilla, const int referencia[3],
                              int umbral_color, MapaCeldas candidatas)
{
    int fila, columna;

    for (fila = 0; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int color[3];

            leer_celda(celdas, fila, columna, color);
            candidatas[fila][columna] = (unsigned char)(es_piso(color, referencia, umbral_color) ? 0 : 1);
        }
    }
}

/* Cuenta las vecinas (8 alrededor) marcadas. */
static int contar_vecinas(MapaCeldas candidatas, const Rejilla *rejilla, int fila, int columna)
{
    int cuenta = 0;
    int df, dc;

    for (df = -1; df <= 1; df++) {
        for (dc = -1; dc <= 1; dc++) {
            int f = fila + df;
            int c = columna + dc;

            if ((df != 0 || dc != 0) && f >= 0 && f < rejilla->filas && c >= 0 && c < rejilla->columnas) {
                cuenta += candidatas[f][c];
            }
        }
    }
    return cuenta;
}

/* Una celda "no piso" solo cuenta si tiene al menos una vecina "no piso" (descarta el ruido). */
static void confirmar_celdas(MapaCeldas candidatas, const Rejilla *rejilla, MapaCeldas confirmadas)
{
    int fila, columna;

    for (fila = 0; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int sola = (contar_vecinas(candidatas, rejilla, fila, columna) == 0);

            confirmadas[fila][columna] = (unsigned char)((candidatas[fila][columna] && !sola) ? 1 : 0);
        }
    }
}

/* ── Paso 6: zonas y cercanía ──────────────────────────────────────────── */

/* Cercanía (1 a 3) según la fila más baja alcanzada: los límites son proporciones del alto. */
static int cercania_por_fila(const Rejilla *rejilla, int fila_mas_baja)
{
    if (fila_mas_baja >= proporcion(rejilla->filas, OBS_CERCA_MIL)) {
        return 3;
    }
    if (fila_mas_baja >= proporcion(rejilla->filas, OBS_MEDIA_MIL)) {
        return 2;
    }
    return 1;
}

/* Cuenta las celdas confirmadas de cada zona (tercios de ancho) desde la fila `fila_minima`
 * hacia abajo; las filas de arriba se ignoran. Una zona tiene obstáculo si tiene al menos
 * `min_celdas` celdas. La cercanía sale de la fila más baja que alcanza el obstáculo. */
static void resumir_zonas(MapaCeldas confirmadas, const Rejilla *rejilla, int min_celdas,
                          int fila_minima, ResultadoObstaculos *resultado)
{
    int fila_mas_baja[OBS_ZONAS] = { -1, -1, -1 };
    int fila, columna, zona;

    for (fila = fila_minima; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            if (confirmadas[fila][columna]) {
                zona = columna * OBS_ZONAS / rejilla->columnas;   /* 0, 1 o 2 */
                resultado->zona[zona].celdas++;
                fila_mas_baja[zona] = fila;   /* las filas se recorren de arriba abajo */
            }
        }
    }
    for (zona = 0; zona < OBS_ZONAS; zona++) {
        if (resultado->zona[zona].celdas >= min_celdas) {
            resultado->zona[zona].hay_obstaculo = 1;
            resultado->zona[zona].cercania = cercania_por_fila(rejilla, fila_mas_baja[zona]);
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
    resultado->columnas = 0;
    resultado->filas = 0;
    resultado->fila_minima_usada = 0;
    resultado->rejilla_chica = 0;
    resultado->ref_columna = 0;
    resultado->ref_fila = 0;
    resultado->ref_ncolumnas = 0;
    resultado->ref_nfilas = 0;
    for (fila = 0; fila < OBS_MAX_FILAS; fila++) {
        for (columna = 0; columna < OBS_MAX_COLUMNAS; columna++) {
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
    config->ref_ancho_mil = OBS_REF_ANCHO_MIL;
    config->ref_alto_mil = OBS_REF_ALTO_MIL;
    config->ref_centro_x_mil = OBS_REF_CENTRO_X_MIL;
    config->ref_borde_inf_mil = OBS_REF_BORDE_INF_MIL;
    config->fila_minima_mil = OBS_FILA_MINIMA_MIL;
}

/* La configuración es válida si los umbrales y las proporciones están en rango. */
static int config_es_valida(const ConfigObstaculos *config)
{
    return config->umbral_color >= 0 && config->umbral_color <= 765 &&
           config->min_celdas >= 1 &&
           config->ref_ancho_mil >= 1 && config->ref_ancho_mil <= 1000 &&
           config->ref_alto_mil >= 1 && config->ref_alto_mil <= 1000 &&
           config->ref_centro_x_mil >= 0 && config->ref_centro_x_mil <= 1000 &&
           config->ref_borde_inf_mil >= 1 && config->ref_borde_inf_mil <= 1000 &&
           config->fila_minima_mil >= 0 && config->fila_minima_mil <= 1000;
}

int obstaculos_analizar_con(const ConfigObstaculos *config, const unsigned char *rgb,
                            int ancho, int alto, ResultadoObstaculos *resultado)
{
    MapaColores celdas;
    MapaCeldas candidatas;
    Rejilla rejilla;
    int referencia[3];
    int fila_minima;
    int codigo;

    if (resultado == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    limpiar_resultado(resultado);
    if (rgb == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    codigo = calcular_rejilla(ancho, alto, &rejilla);
    if (codigo != OBS_OK) {
        return codigo;
    }
    if (config == NULL || !config_es_valida(config)) {
        return OBS_ERR_CONFIG;
    }
    calcular_franja(config, &rejilla);

    promediar_celdas(rgb, &rejilla, celdas);
    codigo = calcular_referencia(celdas, &rejilla, referencia);
    if (codigo != OBS_OK) {
        return codigo;
    }
    resultado->ref_r = referencia[0];
    resultado->ref_g = referencia[1];
    resultado->ref_b = referencia[2];
    resultado->columnas = rejilla.columnas;
    resultado->ref_columna = rejilla.ref_columna;
    resultado->ref_fila = rejilla.ref_fila;
    resultado->ref_ncolumnas = rejilla.ref_ncolumnas;
    resultado->ref_nfilas = rejilla.ref_nfilas;
    resultado->filas = rejilla.filas;
    resultado->rejilla_chica = (rejilla.columnas < OBS_AVISO_COLUMNAS || rejilla.filas < OBS_AVISO_FILAS) ? 1 : 0;

    /* La fila mínima nunca pasa de la última fila (con 1000 se mira solo la última). */
    fila_minima = limitar_entre(proporcion(rejilla.filas, config->fila_minima_mil), 0, rejilla.filas - 1);
    resultado->fila_minima_usada = fila_minima;

    marcar_candidatas(celdas, &rejilla, referencia, config->umbral_color, candidatas);
    confirmar_celdas(candidatas, &rejilla, resultado->no_piso);
    resumir_zonas(resultado->no_piso, &rejilla, config->min_celdas, fila_minima, resultado);
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
    case OBS_ERR_TAMANO:        return "Tamano de imagen no valido (de 30x30 a 640x480)";
    case OBS_ERR_REFERENCIA:    return "El piso de referencia no es uniforme (hay un objeto debajo del carro?)";
    case OBS_ERR_CONFIG:        return "La configuracion no es valida";
    default:                    return "Error desconocido";
    }
}
