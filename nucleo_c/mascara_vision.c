/*
 * mascara_vision.c — La máscara de artefactos fijos de la imagen (ver mascara_vision.h).
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "mascara_vision.h"

/* Una celda gruesa (fila, columna) es una de las cercanas del centro: las MASC_CERCANAS_FILAS últimas filas de las MASC_CENTRALES columnas del medio. */
static int es_celda_protegida(int fila, int columna)
{
    int primera_central = (OBS_MASC_COLUMNAS - MASC_CENTRALES) / 2;

    return fila >= OBS_MASC_FILAS - MASC_CERCANAS_FILAS && columna >= primera_central && columna < primera_central + MASC_CENTRALES;
}

void masc_lugar_iniciar(LugarMascara *lugar)
{
    if (lugar == NULL) {
        return;
    }
    memset(lugar, 0, sizeof *lugar);
}

int masc_lugar_cuadro(LugarMascara *lugar, const ResultadoObstaculos *resultado)
{
    int total[OBS_MASC_FILAS][OBS_MASC_COLUMNAS];
    int marcadas[OBS_MASC_FILAS][OBS_MASC_COLUMNAS];
    int fila, columna;

    if (lugar == NULL || resultado == NULL || resultado->filas < OBS_MASC_FILAS || resultado->columnas < OBS_MASC_COLUMNAS ||
        resultado->filas > OBS_MAX_FILAS || resultado->columnas > OBS_MAX_COLUMNAS) {
        return MASC_ERR_ARGUMENTO;
    }
    memset(total, 0, sizeof total);
    memset(marcadas, 0, sizeof marcadas);
    for (fila = resultado->fila_minima_usada; fila < resultado->filas; fila++) {
        for (columna = 0; columna < resultado->columnas; columna++) {
            int gf = fila * OBS_MASC_FILAS / resultado->filas;
            int gc = columna * OBS_MASC_COLUMNAS / resultado->columnas;

            total[gf][gc]++;
            marcadas[gf][gc] += resultado->no_piso[fila][columna] ? 1 : 0;
        }
    }
    for (fila = 0; fila < OBS_MASC_FILAS; fila++) {
        for (columna = 0; columna < OBS_MASC_COLUMNAS; columna++) {
            if (total[fila][columna] > 0 && marcadas[fila][columna] * 100 >= total[fila][columna] * MASC_CELDA_PCT) {
                lugar->marcas[fila][columna]++;
            }
        }
    }
    lugar->cuadros++;
    return MASC_OK;
}

/* % de cuadros en que la celda estuvo marcada en este lugar. */
static int frecuencia_pct(const LugarMascara *lugar, int fila, int columna)
{
    return (lugar->cuadros > 0) ? lugar->marcas[fila][columna] * 100 / lugar->cuadros : 0;
}

int masc_calcular(const LugarMascara lugares[MASC_LUGARES], MascaraVision *mascara, InformeMascara *informe)
{
    int fila, columna, lugar, zona, candidatas = 0;

    if (lugares == NULL || mascara == NULL || informe == NULL) {
        return MASC_ERR_ARGUMENTO;
    }
    for (lugar = 0; lugar < MASC_LUGARES; lugar++) {
        if (lugares[lugar].cuadros < MASC_MIN_CUADROS) {
            return MASC_ERR_CUADROS;
        }
    }
    memset(mascara, 0, sizeof *mascara);
    memset(informe, 0, sizeof *informe);
    for (fila = 0; fila < OBS_MASC_FILAS; fila++) {
        for (columna = 0; columna < OBS_MASC_COLUMNAS; columna++) {
            int pct[MASC_LUGARES], en_cuantos = 0, menor = 100;

            for (lugar = 0; lugar < MASC_LUGARES; lugar++) {
                pct[lugar] = frecuencia_pct(&lugares[lugar], fila, columna);
                if (pct[lugar] >= MASC_FRECUENCIA_PCT) {
                    en_cuantos++;
                }
                if (pct[lugar] < menor) {
                    menor = pct[lugar];
                }
            }
            informe->frecuencia_min[fila][columna] = menor;
            zona = columna * OBS_ZONAS / OBS_MASC_COLUMNAS;
            for (lugar = 0; lugar < MASC_LUGARES; lugar++) {
                if (pct[lugar] >= MASC_FRECUENCIA_PCT) {
                    informe->zona_celdas[zona][lugar]++;
                }
            }
            if (en_cuantos == MASC_LUGARES) {
                informe->artefactos++;
                if (es_celda_protegida(fila, columna)) {
                    informe->sospechosa |= MASC_SOSP_CENTRO;   /* justo donde el carro va a chocar: nunca se enmascara */
                } else {
                    mascara->celda[fila][columna] = 1;
                    candidatas++;
                }
            } else if (en_cuantos > 0 && informe->depende_n < MASC_MAX_DEPENDE) {
                informe->depende_fila[informe->depende_n] = fila;
                informe->depende_columna[informe->depende_n] = columna;
                for (lugar = 0; lugar < MASC_LUGARES; lugar++) {
                    informe->depende_pct[informe->depende_n][lugar] = pct[lugar];
                }
                informe->depende_n++;
            }
        }
    }
    if (candidatas * 100 > OBS_MASC_FILAS * OBS_MASC_COLUMNAS * MASC_MAX_PCT) {
        informe->sospechosa |= MASC_SOSP_DEMASIADAS;
        memset(mascara, 0, sizeof *mascara);   /* una máscara tan grande esconde la escena entera: no se usa */
        candidatas = 0;
    }
    informe->enmascaradas = candidatas;
    for (zona = 0; zona < OBS_ZONAS; zona++) {
        int hay = 0, todos = 1;

        for (lugar = 0; lugar < MASC_LUGARES; lugar++) {
            if (informe->zona_celdas[zona][lugar] > 0) {
                hay = 1;
            } else {
                todos = 0;
            }
        }
        informe->zona_depende[zona] = (hay && !todos) ? 1 : 0;
    }
    return MASC_OK;
}

void masc_aplicar_a_config(const MascaraVision *mascara, ConfigObstaculos *config)
{
    if (config == NULL) {
        return;
    }
    if (mascara == NULL) {
        memset(config->mascara, 0, sizeof config->mascara);
        config->mascara_activa = 0;
        return;
    }
    memcpy(config->mascara, mascara->celda, sizeof config->mascara);
    config->mascara_activa = (masc_contar(mascara) > 0) ? 1 : 0;
}

int masc_contar(const MascaraVision *mascara)
{
    int fila, columna, n = 0;

    if (mascara == NULL) {
        return 0;
    }
    for (fila = 0; fila < OBS_MASC_FILAS; fila++) {
        for (columna = 0; columna < OBS_MASC_COLUMNAS; columna++) {
            n += mascara->celda[fila][columna] ? 1 : 0;
        }
    }
    return n;
}

/* Las 100 celdas en orden de fila: la cifra k lleva las celdas 4k a 4k+3 (la primera en el bit más alto). */
static void celdas_a_hex(const unsigned char *celdas, char *texto)
{
    static const char CIFRAS[] = "0123456789abcdef";
    int k, b;

    for (k = 0; k < MASC_HEX_LARGO; k++) {
        int valor = 0;

        for (b = 0; b < 4; b++) {
            valor = valor * 2 + (celdas[k * 4 + b] ? 1 : 0);
        }
        texto[k] = CIFRAS[valor];
    }
    texto[MASC_HEX_LARGO] = '\0';
}

int masc_a_hex(const MascaraVision *mascara, char *texto, int capacidad)
{
    if (mascara == NULL || texto == NULL || capacidad < MASC_HEX_LARGO + 1) {
        return MASC_ERR_ARGUMENTO;
    }
    celdas_a_hex((const unsigned char *)mascara->celda, texto);
    return MASC_OK;
}

int masc_de_hex(const char *texto, MascaraVision *mascara)
{
    MascaraVision nueva;
    int k, b;

    if (texto == NULL || mascara == NULL) {
        return MASC_ERR_ARGUMENTO;
    }
    if (strlen(texto) != (size_t)MASC_HEX_LARGO) {
        return MASC_ERR_TEXTO;
    }
    memset(&nueva, 0, sizeof nueva);
    for (k = 0; k < MASC_HEX_LARGO; k++) {
        char c = texto[k];
        int valor;

        if (c >= '0' && c <= '9') {
            valor = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            valor = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            valor = c - 'A' + 10;
        } else {
            return MASC_ERR_TEXTO;
        }
        for (b = 0; b < 4; b++) {
            nueva.celda[(k * 4 + b) / OBS_MASC_COLUMNAS][(k * 4 + b) % OBS_MASC_COLUMNAS] = (unsigned char)((valor >> (3 - b)) & 1);
        }
    }
    *mascara = nueva;
    return MASC_OK;
}

int masc_zona_a_hex(const ResultadoObstaculos *resultado, int zona, char *texto, int capacidad)
{
    unsigned char celdas[OBS_MASC_FILAS * OBS_MASC_COLUMNAS];
    int total[OBS_MASC_FILAS][OBS_MASC_COLUMNAS];
    int marcadas[OBS_MASC_FILAS][OBS_MASC_COLUMNAS];
    int fila, columna;

    if (resultado == NULL || texto == NULL || capacidad < MASC_HEX_LARGO + 1 || zona < 0 || zona >= OBS_ZONAS ||
        resultado->filas < OBS_MASC_FILAS || resultado->columnas < OBS_MASC_COLUMNAS || resultado->filas > OBS_MAX_FILAS || resultado->columnas > OBS_MAX_COLUMNAS) {
        return MASC_ERR_ARGUMENTO;
    }
    memset(total, 0, sizeof total);
    memset(marcadas, 0, sizeof marcadas);
    memset(celdas, 0, sizeof celdas);
    for (fila = resultado->fila_minima_usada; fila < resultado->filas; fila++) {
        for (columna = 0; columna < resultado->columnas; columna++) {
            if (columna * OBS_ZONAS / resultado->columnas == zona) {
                int gf = fila * OBS_MASC_FILAS / resultado->filas;
                int gc = columna * OBS_MASC_COLUMNAS / resultado->columnas;

                total[gf][gc]++;
                marcadas[gf][gc] += resultado->no_piso[fila][columna] ? 1 : 0;
            }
        }
    }
    for (fila = 0; fila < OBS_MASC_FILAS; fila++) {
        for (columna = 0; columna < OBS_MASC_COLUMNAS; columna++) {
            if (total[fila][columna] > 0 && marcadas[fila][columna] * 100 >= total[fila][columna] * MASC_CELDA_PCT) {
                celdas[fila * OBS_MASC_COLUMNAS + columna] = 1;
            }
        }
    }
    celdas_a_hex(celdas, texto);
    return MASC_OK;
}

/* Agrega texto sin pasarse de `capacidad` (siempre deja el '\0'). */
static void agregar(char *texto, int capacidad, int *largo, const char *formato, int a, int b, int c)
{
    int n;

    if (*largo >= capacidad - 1) {
        return;
    }
    n = snprintf(texto + *largo, (size_t)(capacidad - *largo), formato, a, b, c);
    if (n > 0) {
        *largo += (n < capacidad - *largo) ? n : capacidad - *largo - 1;
    }
}

int masc_texto_informe(const MascaraVision *mascara, const InformeMascara *informe, char *texto, int capacidad)
{
    static const char *const ZONAS[OBS_ZONAS] = { "izquierda", "centro", "derecha" };
    char hex[MASC_HEX_LARGO + 1];
    int largo = 0, fila, columna, zona, i;

    if (texto == NULL || capacidad < 2) {
        return 0;
    }
    texto[0] = '\0';
    if (mascara == NULL || informe == NULL) {
        return 0;
    }
    agregar(texto, capacidad, &largo, "Cuadrícula (%% de cuadros en que la celda marcó «no piso», el menor de los %d lugares; %d filas de arriba a abajo):\n", MASC_LUGARES, OBS_MASC_FILAS, 0);
    for (fila = 0; fila < OBS_MASC_FILAS; fila++) {
        for (columna = 0; columna < OBS_MASC_COLUMNAS; columna++) {
            agregar(texto, capacidad, &largo, columna == 0 ? "%3d" : " %3d", informe->frecuencia_min[fila][columna], 0, 0);
        }
        agregar(texto, capacidad, &largo, "\n", 0, 0, 0);
    }
    masc_a_hex(mascara, hex, (int)sizeof hex);
    agregar(texto, capacidad, &largo, "Enmascaradas: %d de 100 (artefacto = marcada en %d %% o más de los cuadros en los tres lugares)\n", informe->enmascaradas, MASC_FRECUENCIA_PCT, 0);
    {
        int n = snprintf(texto + largo, (size_t)(capacidad - largo), "Máscara: %s\n", hex);

        if (n > 0 && largo + n < capacidad) {
            largo += n;
        }
    }
    for (zona = 0; zona < OBS_ZONAS; zona++) {
        if (informe->zona_depende[zona]) {
            int n = snprintf(texto + largo, (size_t)(capacidad - largo), "Zona %s: DEPENDE DEL LUGAR (celdas que marcan: lugar 1 = %d, lugar 2 = %d, lugar 3 = %d); no se enmascara\n", ZONAS[zona],
                             informe->zona_celdas[zona][0], informe->zona_celdas[zona][1], informe->zona_celdas[zona][2]);

            if (n > 0 && largo + n < capacidad) {
                largo += n;
            }
        }
    }
    for (i = 0; i < informe->depende_n; i++) {
        int n = snprintf(texto + largo, (size_t)(capacidad - largo), "  celda fila %d columna %d: %d %%, %d %%, %d %% en los tres lugares\n", informe->depende_fila[i], informe->depende_columna[i],
                         informe->depende_pct[i][0], informe->depende_pct[i][1], informe->depende_pct[i][2]);

        if (n > 0 && largo + n < capacidad) {
            largo += n;
        }
    }
    if (informe->sospechosa & MASC_SOSP_DEMASIADAS) {
        agregar(texto, capacidad, &largo, "VISIÓN SOSPECHOSA: más de %d %% de las celdas serían artefacto; no se enmascara nada. Revisa la cámara y el piso.\n", MASC_MAX_PCT, 0, 0);
    }
    if (informe->sospechosa & MASC_SOSP_CENTRO) {
        agregar(texto, capacidad, &largo, "VISIÓN SOSPECHOSA: hay un artefacto en las %d filas más cercanas de las columnas centrales; esas celdas NO se enmascaran (es justo lo que el carro va a chocar).\n", MASC_CERCANAS_FILAS, 0, 0);
    }
    return largo;
}
