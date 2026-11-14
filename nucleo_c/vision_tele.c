/*
 * vision_tele.c — Telemetría numérica de la visión (ver vision_tele.h). Solo lee: no cambia nada del detector.
 */
#include "vision_tele.h"

#include <stdio.h>
#include <string.h>

/* Límite superior (incluido) de cada cubeta de distancia; la última no tiene tope. */
static const int TOPE_CUBETA[VT_CUBETAS - 1] = { 9, 19, 29, 39, 59, 89, 149 };

static int absoluto(int x)
{
    return (x < 0) ? -x : x;
}

/* El valor por debajo del cual queda `pct` por ciento de las `n` muestras de un histograma de `tam` casillas. */
static int percentil(const int *hist, int tam, int n, int pct)
{
    long meta = ((long)n * pct + 99) / 100;
    long suma = 0;
    int i;

    if (n <= 0) {
        return 0;
    }
    if (meta < 1) {
        meta = 1;
    }
    for (i = 0; i < tam; i++) {
        suma += hist[i];
        if (suma >= meta) {
            return i;
        }
    }
    return tam - 1;
}

int vision_tele_calcular(const unsigned char *celdas, int columnas, int filas, const ModeloPiso *modelo, const ConfigObstaculos *config,
                         const ResultadoObstaculos *resultado, VisionTele *tele)
{
    /* Histogramas finos para los percentiles: distancia de 0 a 765 y brillo de 0 a 255. Son estáticos para no cargar la pila del hilo de video (unos 4 kB);
     * la función se usa desde un solo hilo. */
    static int hist_dist[766];
    static int hist_brillo[256];
    int fila, columna, g, c, fila_minima;
    long suma_dist = 0, suma_brillo = 0, suma_croma = 0;

    if (celdas == NULL || tele == NULL || resultado == NULL) {
        return VT_ERR_ARGUMENTO;
    }
    if (columnas < 3 || filas < 1 || columnas > OBS_MAX_COLUMNAS || filas > OBS_MAX_FILAS) {
        return VT_ERR_TAMANO;
    }
    memset(tele, 0, sizeof *tele);
    memset(hist_dist, 0, sizeof hist_dist);
    memset(hist_brillo, 0, sizeof hist_brillo);
    fila_minima = resultado->fila_minima_usada;
    if (fila_minima < 0 || fila_minima >= filas) {
        fila_minima = 0;
    }
    tele->columnas = columnas;
    tele->filas = filas;
    tele->fila_minima = fila_minima;
    tele->con_modelo = (modelo != NULL && modelo->valido && modelo->grupos >= 1 && modelo->grupos <= OBS_MAX_GRUPOS);
    if (tele->con_modelo) {
        tele->grupos = modelo->grupos;
        for (g = 0; g < modelo->grupos; g++) {
            for (c = 0; c < 3; c++) {
                tele->grupo_color[g][c] = modelo->color256[g][c] / 256;
            }
            tele->grupo_peso[g] = modelo->peso[g];
        }
    }
    if (config != NULL) {
        tele->umbral_color = config->umbral_color;
        tele->min_celdas = config->min_celdas;
    }
    tele->ganancia_mil = resultado->ganancia_mil;
    tele->brillo_zona = resultado->brillo_medio;
    tele->motivo = resultado->motivo;
    tele->ref_r = resultado->ref_r;
    tele->ref_g = resultado->ref_g;
    tele->ref_b = resultado->ref_b;

    for (fila = fila_minima; fila < filas; fila++) {
        /* La banda de distancia de esta fila, con los mismos cortes que la cercanía (obstaculos.h): hasta OBS_MEDIA_MIL lejos, hasta OBS_CERCA_MIL media, después cerca. */
        int permil = (fila + 1) * 1000 / filas;
        int banda = (permil > OBS_CERCA_MIL) ? 2 : ((permil > OBS_MEDIA_MIL) ? 1 : 0);

        for (columna = 0; columna < columnas; columna++) {
            const unsigned char *p = celdas + ((long)fila * columnas + columna) * 3;
            int r = p[0], v = p[1], a = p[2];
            int mayor = (r > v) ? ((r > a) ? r : a) : ((v > a) ? v : a);
            int menor = (r < v) ? ((r < a) ? r : a) : ((v < a) ? v : a);
            int brillo = (r + v + a) / 3;

            tele->celdas++;
            suma_brillo += brillo;
            suma_croma += mayor - menor;
            hist_brillo[brillo]++;
            if (mayor >= 250) {
                tele->quemadas++;
            }
            if (mayor <= 8) {
                tele->negras++;
            }
            if (tele->con_modelo) {
                int mejor = -1, cubeta = VT_CUBETAS - 1;

                for (g = 0; g < tele->grupos; g++) {
                    int d = absoluto(r - tele->grupo_color[g][0]) + absoluto(v - tele->grupo_color[g][1]) + absoluto(a - tele->grupo_color[g][2]);

                    if (mejor < 0 || d < mejor) {
                        mejor = d;
                    }
                }
                if (mejor > 765) {
                    mejor = 765;
                }
                hist_dist[mejor]++;
                suma_dist += mejor;
                if (mejor > tele->dist_max) {
                    tele->dist_max = mejor;
                }
                for (c = 0; c < VT_CUBETAS - 1; c++) {
                    if (mejor <= TOPE_CUBETA[c]) {
                        cubeta = c;
                        break;
                    }
                }
                tele->hist[cubeta]++;
            }
            if (resultado->no_piso[fila][columna]) {
                int zona = columna * OBS_ZONAS / columnas;

                tele->no_piso[zona][banda]++;
                tele->no_piso_fila[fila]++;
                tele->no_piso_total++;
            }
        }
    }
    if (tele->celdas > 0) {
        tele->brillo_media = (int)(suma_brillo / tele->celdas);
        tele->croma_media = (int)(suma_croma / tele->celdas);
        tele->brillo_p10 = percentil(hist_brillo, 256, tele->celdas, 10);
        tele->brillo_p90 = percentil(hist_brillo, 256, tele->celdas, 90);
        if (tele->con_modelo) {
            tele->dist_media = (int)(suma_dist / tele->celdas);
            tele->dist_p50 = percentil(hist_dist, 766, tele->celdas, 50);
            tele->dist_p90 = percentil(hist_dist, 766, tele->celdas, 90);
        }
    }
    return VT_OK;
}

/* Agrega texto con formato al final de `texto`; devuelve 0 si ya no cupo. */
static int agregar(char *texto, int capacidad, int *largo, const char *formato, int a, int b, int c, int d)
{
    int n;

    if (*largo >= capacidad - 1) {
        return 0;
    }
    n = snprintf(texto + *largo, (size_t)(capacidad - *largo), formato, a, b, c, d);
    if (n < 0 || n >= capacidad - *largo) {
        *largo = capacidad - 1;
        texto[capacidad - 1] = '\0';
        return 0;
    }
    *largo += n;
    return 1;
}

int vision_tele_texto(const VisionTele *tele, char *texto, int capacidad)
{
    int largo = 0, i, z, ok = 1;

    if (tele == NULL || texto == NULL || capacidad < 8) {
        return VT_ERR_ARGUMENTO;
    }
    texto[0] = '\0';
    ok &= agregar(texto, capacidad, &largo, "VTEL c=%dx%d fm=%d n=%d", tele->columnas, tele->filas, tele->fila_minima, tele->celdas);
    ok &= agregar(texto, capacidad, &largo, " um=%d mc=%d gan=%d mot=%d", tele->umbral_color, tele->min_celdas, tele->ganancia_mil, tele->motivo);
    ok &= agregar(texto, capacidad, &largo, " b=%d,%d,%d cr=%d", tele->brillo_media, tele->brillo_p10, tele->brillo_p90, tele->croma_media);
    ok &= agregar(texto, capacidad, &largo, " sat=%d,%d ref=%d,%d", tele->quemadas, tele->negras, tele->ref_r, tele->ref_g);
    ok &= agregar(texto, capacidad, &largo, ",%d d=%d,%d,%d", tele->ref_b, tele->dist_media, tele->dist_p50, tele->dist_p90);
    ok &= agregar(texto, capacidad, &largo, ",%d h=%d", tele->dist_max, tele->hist[0], 0, 0);
    for (i = 1; i < VT_CUBETAS; i++) {
        ok &= agregar(texto, capacidad, &largo, ",%d", tele->hist[i], 0, 0, 0);
    }
    ok &= agregar(texto, capacidad, &largo, " np=%d z=", tele->no_piso_total, 0, 0, 0);
    for (z = 0; z < OBS_ZONAS; z++) {
        ok &= agregar(texto, capacidad, &largo, z ? "|%d,%d,%d" : "%d,%d,%d", tele->no_piso[z][0], tele->no_piso[z][1], tele->no_piso[z][2], 0);
    }
    ok &= agregar(texto, capacidad, &largo, " f=", 0, 0, 0, 0);
    for (i = tele->fila_minima; i < tele->filas && i < OBS_MAX_FILAS; i++) {
        ok &= agregar(texto, capacidad, &largo, (i > tele->fila_minima) ? ",%d" : "%d", tele->no_piso_fila[i], 0, 0, 0);
    }
    ok &= agregar(texto, capacidad, &largo, " g=%d", tele->con_modelo ? tele->grupos : 0, 0, 0, 0);
    for (i = 0; i < tele->grupos && i < OBS_MAX_GRUPOS; i++) {
        ok &= agregar(texto, capacidad, &largo, ":%d,%d,%d,%d", tele->grupo_color[i][0], tele->grupo_color[i][1], tele->grupo_color[i][2], tele->grupo_peso[i]);
    }
    return ok ? VT_OK : VT_ERR_TAMANO;
}
