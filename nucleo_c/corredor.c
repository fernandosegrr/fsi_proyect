/*
 * corredor.c — El corredor de trayectoria: la huella del carro proyectada en la imagen (ver corredor.h).
 *
 * GUÍA DE LECTURA
 *  1. Trigonometría sin math.h: una tabla de senos de 0 a 90 grados (en milésimas) con interpolación cada décima de grado.
 *  2. corredor_preparar: valida la configuración y calcula lo que no cambia entre cuadros (distancia focal, seno y coseno de la inclinación).
 *  3. Las dos fórmulas de la perspectiva están en corredor_distancia_de_fila y corredor_semiancho_px (explicadas en corredor.h, punto 2).
 *  4. corredor_clasificar: una caja contra el corredor (dentro, borde o fuera).
 *  5. corredor_aplicar_zonas: junta las cajas por zona, decide qué zonas quedan fuera del corredor y la cercanía de las que quedan dentro.
 *     corredor_aplicar_mascara es lo mismo con las celdas del detector de color como cajas de 10x10 píxeles.
 *  Solo enteros (long donde hace falta), sin malloc ni recursión.
 */

#include <stddef.h>
#include "corredor.h"

/* Seno de 0 a 90 grados, en milésimas (1000 = 1.0). */
static const int SENO_MIL[91] = {
       0,   17,   35,   52,   70,   87,  105,  122,  139,  156,  174,  191,  208,  225,  242,
     259,  276,  292,  309,  326,  342,  358,  375,  391,  407,  423,  438,  454,  469,  485,
     500,  515,  530,  545,  559,  574,  588,  602,  616,  629,  643,  656,  669,  682,  695,
     707,  719,  731,  743,  755,  766,  777,  788,  799,  809,  819,  829,  839,  848,  857,
     866,  875,  883,  891,  899,  906,  914,  921,  927,  934,  940,  946,  951,  956,  961,
     966,  970,  974,  978,  982,  985,  988,  990,  993,  995,  996,  998,  999,  999, 1000,
    1000
};

/* Seno (milésimas) de un ángulo de 0 a 90 grados dado en décimas de grado, con interpolación lineal. */
static int seno_mil(int grados_x10)
{
    int entero, resto;

    if (grados_x10 <= 0) {
        return 0;
    }
    if (grados_x10 >= 900) {
        return 1000;
    }
    entero = grados_x10 / 10;
    resto = grados_x10 % 10;
    return SENO_MIL[entero] + (SENO_MIL[entero + 1] - SENO_MIL[entero]) * resto / 10;
}

static int coseno_mil(int grados_x10)
{
    return seno_mil(900 - grados_x10);
}

static int en_rango(int valor, int minimo, int maximo)
{
    return valor >= minimo && valor <= maximo;
}

/* ── Configuración ─────────────────────────────────────────────────────── */

void corredor_config_defecto(ConfigCorredor *config)
{
    if (config == NULL) {
        return;
    }
    config->activo = 0;   /* APAGADO por defecto */
    config->ancho_carro_cm = ANCHO_CARRO_CM;
    config->altura_camara_cm = ALTURA_CAMARA_CM;
    config->inclinacion_grados_x10 = INCLINACION_GRADOS * 10;
    config->fov_grados_x10 = FOV_GRADOS * 10;
    config->margen_lateral_cm = MARGEN_LATERAL_CM;
    config->min_peso = CORREDOR_MIN_PESO;
}

int corredor_config_es_valida(const ConfigCorredor *config)
{
    if (config == NULL) {
        return 0;
    }
    return (config->activo == 0 || config->activo == 1) &&
           en_rango(config->ancho_carro_cm, 1, 100) &&
           en_rango(config->altura_camara_cm, 1, 100) &&
           en_rango(config->inclinacion_grados_x10, 0, 800) &&
           en_rango(config->fov_grados_x10, 200, 1500) &&
           en_rango(config->margen_lateral_cm, 0, 50) &&
           en_rango(config->min_peso, 1, 1000);
}

int corredor_preparar(Corredor *corredor, const ConfigCorredor *config, int ancho, int alto)
{
    int medio_fov_seno, medio_fov_coseno;

    if (corredor == NULL || config == NULL || !en_rango(ancho, 30, 640) || !en_rango(alto, 30, 480)) {
        return COR_ERR_ARGUMENTO;
    }
    if (!corredor_config_es_valida(config)) {
        return COR_ERR_CONFIG;
    }
    corredor->config = *config;
    corredor->ancho = ancho;
    corredor->alto = alto;
    corredor->cx = ancho / 2;
    corredor->cy = alto / 2;
    medio_fov_seno = seno_mil(config->fov_grados_x10 / 2);
    medio_fov_coseno = coseno_mil(config->fov_grados_x10 / 2);
    /* f = (ancho / 2) / tan(FOV / 2) = ancho * cos / (2 * sen) */
    corredor->f = (int)(((long)ancho * medio_fov_coseno + medio_fov_seno) / (2L * medio_fov_seno));
    corredor->seno_mil = seno_mil(config->inclinacion_grados_x10);
    corredor->coseno_mil = coseno_mil(config->inclinacion_grados_x10);
    return COR_OK;
}

/* ── Perspectiva ───────────────────────────────────────────────────────── */

/* A(v) = f * sen(incl) + (v - cy) * cos(incl), en píxeles por mil. Es <= 0 en el horizonte y arriba de él. */
static long factor_a(const Corredor *corredor, int v)
{
    return (long)corredor->f * corredor->seno_mil + (long)(v - corredor->cy) * corredor->coseno_mil;
}

int corredor_distancia_de_fila(const Corredor *corredor, int v)
{
    long a, numerador;

    if (corredor == NULL) {
        return -1;
    }
    a = factor_a(corredor, v);
    if (a <= 0) {
        return -1;   /* en el horizonte o arriba de él: no hay piso ahí */
    }
    numerador = (long)corredor->f * corredor->coseno_mil - (long)(v - corredor->cy) * corredor->seno_mil;
    if (numerador < 0) {
        return 0;    /* debajo de la cámara: la fila ve el piso pegado al carro */
    }
    return (int)(((long)corredor->config.altura_camara_cm * numerador + a / 2) / a);
}

/* Mitad del ancho en píxeles de `ancho_mm` milímetros (a cada lado del centro) en la fila v, redondeada HACIA ARRIBA (más ancho = más
 * prudente: en la duda se frena). */
static int semiancho_mm_a_px(const Corredor *corredor, int v, long medio_mm)
{
    long a = factor_a(corredor, v);
    long divisor;

    if (a <= 0) {
        return 0;
    }
    divisor = (long)corredor->config.altura_camara_cm * 10L * 1000L;   /* altura en mm por mil */
    return (int)((medio_mm * a + divisor - 1) / divisor);
}

int corredor_semiancho_px(const Corredor *corredor, int v, int ancho_cm)
{
    if (corredor == NULL || ancho_cm < 0) {
        return 0;
    }
    return semiancho_mm_a_px(corredor, v, (long)ancho_cm * 5L);   /* medio ancho en mm: cm * 10 / 2 */
}

/* ── Clasificar una caja ───────────────────────────────────────────────── */

int corredor_clasificar(const Corredor *corredor, const CajaImagen *caja, ResultadoCaja *resultado)
{
    int v, huella, total;

    if (corredor == NULL || caja == NULL || resultado == NULL) {
        return COR_ERR_ARGUMENTO;
    }
    resultado->estado = COR_FUERA;
    resultado->distancia_cm = -1;
    resultado->sobre_horizonte = 0;
    resultado->semiancho_huella_px = 0;
    resultado->semiancho_corredor_px = 0;

    /* La caja toca el piso por su borde de abajo. */
    v = caja->y1;
    if (v > corredor->alto) {
        v = corredor->alto;
    }
    if (factor_a(corredor, v) <= 0) {
        resultado->sobre_horizonte = 1;   /* no se puede ubicar en el piso: no se sabe cuánto mide el corredor ahí */
        return COR_OK;
    }
    resultado->distancia_cm = corredor_distancia_de_fila(corredor, v);
    huella = semiancho_mm_a_px(corredor, v, (long)corredor->config.ancho_carro_cm * 5L);
    total = semiancho_mm_a_px(corredor, v, (long)corredor->config.ancho_carro_cm * 5L + (long)corredor->config.margen_lateral_cm * 10L);
    resultado->semiancho_huella_px = huella;
    resultado->semiancho_corredor_px = total;

    if (caja->x1 > corredor->cx - huella && caja->x0 < corredor->cx + huella) {
        resultado->estado = COR_DENTRO;
    } else if (caja->x1 > corredor->cx - total && caja->x0 < corredor->cx + total) {
        resultado->estado = COR_BORDE;
    }
    return COR_OK;
}

/* ── Zonas ─────────────────────────────────────────────────────────────── */

/* Cercanía (1 a 3) de un borde de abajo en píxeles: los mismos límites del detector de color (proporciones del alto). */
static int cercania_de_borde(const Corredor *corredor, int y1)
{
    int filas = corredor->alto / OBS_LADO_CELDA;
    int fila = (y1 - 1) / OBS_LADO_CELDA;

    if (fila >= filas * OBS_CERCA_MIL / 1000) {
        return 3;
    }
    if (fila >= filas * OBS_MEDIA_MIL / 1000) {
        return 2;
    }
    return 1;
}

/* Lo que se va juntando por zona mientras se recorren las cajas. */
typedef struct {
    long peso[OBS_ZONAS];        /* peso de lo que cae dentro del corredor */
    int borde_bajo[OBS_ZONAS];   /* el borde de abajo más bajo (más cerca del carro) de lo que cae dentro */
} Acumulado;

/* Clasifica una caja y, si está dentro o en el borde del corredor, la suma a su zona. */
static void acumular_caja(const Corredor *corredor, const CajaImagen *caja, Acumulado *acumulado)
{
    ResultadoCaja r;
    int z, y1;

    if (corredor_clasificar(corredor, caja, &r) != COR_OK || r.estado == COR_FUERA) {
        return;
    }
    z = ((caja->x0 + caja->x1) / 2) * OBS_ZONAS / corredor->ancho;   /* tercio del ancho donde cae el centro de la caja */
    if (z < 0) {
        z = 0;
    }
    if (z >= OBS_ZONAS) {
        z = OBS_ZONAS - 1;
    }
    y1 = (caja->y1 > corredor->alto) ? corredor->alto : caja->y1;
    acumulado->peso[z] += caja->peso;
    if (y1 > acumulado->borde_bajo[z]) {
        acumulado->borde_bajo[z] = y1;
    }
}

/* Con lo acumulado decide, zona por zona, si cuenta o queda fuera del corredor. */
static void cerrar_zonas(const Corredor *corredor, const Acumulado *acumulado, ZonaObstaculo zonas[OBS_ZONAS])
{
    int z;

    for (z = 0; z < OBS_ZONAS; z++) {
        long peso = acumulado->peso[z];

        zonas[z].celdas_corredor = (int)((peso > 100000L) ? 100000L : peso);
        if (peso >= corredor->config.min_peso) {
            zonas[z].fuera_corredor = 0;
            zonas[z].cercania_corredor = cercania_de_borde(corredor, acumulado->borde_bajo[z]);
        } else {
            zonas[z].fuera_corredor = zonas[z].hay_obstaculo ? 1 : 0;   /* hay algo en la zona pero no por donde pasa el carro */
            zonas[z].cercania_corredor = 0;
        }
    }
}

/* Con el corredor apagado nada se filtra: todo cuenta y el navegador usa la cercanía de siempre. */
static void limpiar_zonas(ZonaObstaculo zonas[OBS_ZONAS])
{
    int z;

    for (z = 0; z < OBS_ZONAS; z++) {
        zonas[z].celdas_corredor = 0;
        zonas[z].fuera_corredor = 0;
        zonas[z].cercania_corredor = 0;
    }
}

int corredor_aplicar_zonas(const Corredor *corredor, const CajaImagen *cajas, int n_cajas, ZonaObstaculo zonas[OBS_ZONAS])
{
    Acumulado acumulado = { { 0, 0, 0 }, { 0, 0, 0 } };
    int i;

    if (corredor == NULL || zonas == NULL || (cajas == NULL && n_cajas > 0) || n_cajas < 0) {
        return COR_ERR_ARGUMENTO;
    }
    if (!corredor->config.activo) {
        limpiar_zonas(zonas);
        return COR_OK;
    }
    for (i = 0; i < n_cajas; i++) {
        acumular_caja(corredor, &cajas[i], &acumulado);
    }
    cerrar_zonas(corredor, &acumulado, zonas);
    return COR_OK;
}
int corredor_aplicar_mascara(const Corredor *corredor, const unsigned char *mascara, int paso_fila, int columnas, int filas, int fila_minima,
                             ZonaObstaculo zonas[OBS_ZONAS])
{
    Acumulado acumulado = { { 0, 0, 0 }, { 0, 0, 0 } };
    int fila, columna;

    if (corredor == NULL || mascara == NULL || zonas == NULL || columnas < 1 || filas < 1 || paso_fila < columnas ||
        columnas > OBS_MAX_COLUMNAS || filas > OBS_MAX_FILAS) {
        return COR_ERR_ARGUMENTO;
    }
    if (!corredor->config.activo) {
        limpiar_zonas(zonas);
        return COR_OK;
    }
    if (fila_minima < 0) {
        fila_minima = 0;
    }
    for (fila = fila_minima; fila < filas; fila++) {
        for (columna = 0; columna < columnas; columna++) {
            if (mascara[fila * paso_fila + columna]) {   /* cada celda marcada es una caja de 10x10 píxeles de peso 1 */
                CajaImagen celda;

                celda.x0 = columna * OBS_LADO_CELDA;
                celda.x1 = (columna + 1) * OBS_LADO_CELDA;
                celda.y0 = fila * OBS_LADO_CELDA;
                celda.y1 = (fila + 1) * OBS_LADO_CELDA;
                celda.peso = 1;
                celda.fuente = COR_FUENTE_DETECTOR;
                acumular_caja(corredor, &celda, &acumulado);
            }
        }
    }
    cerrar_zonas(corredor, &acumulado, zonas);
    return COR_OK;
}

int corredor_aplicar_resultado(const Corredor *corredor, ResultadoObstaculos *resultado)
{
    if (corredor == NULL || resultado == NULL) {
        return COR_ERR_ARGUMENTO;
    }
    return corredor_aplicar_mascara(corredor, &resultado->no_piso[0][0], OBS_MAX_COLUMNAS, resultado->columnas, resultado->filas,
                                    resultado->fila_minima_usada, resultado->zona);
}

void corredor_caja_de_milesimas(int xmin_mil, int ymin_mil, int xmax_mil, int ymax_mil, int ancho, int alto, int peso, int fuente, CajaImagen *caja)
{
    if (caja == NULL) {
        return;
    }
    caja->x0 = ancho * xmin_mil / 1000;
    caja->y0 = alto * ymin_mil / 1000;
    caja->x1 = (ancho * xmax_mil + 999) / 1000;
    caja->y1 = (alto * ymax_mil + 999) / 1000;
    caja->peso = peso;
    caja->fuente = fuente;
}

const char *corredor_texto_estado(int estado)
{
    switch (estado) {
    case COR_DENTRO: return "dentro";
    case COR_BORDE:  return "borde";
    default:         return "fuera";
    }
}
