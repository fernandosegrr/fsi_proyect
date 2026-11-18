/*
 * vision_tele.h — TELEMETRÍA NUMÉRICA DE LA VISIÓN (autónomo v2).
 *
 * GUÍA DE LECTURA
 *  1. Para qué: cuando el detector de color marca «obstáculo» sobre un piso vacío hace falta saber POR QUÉ (una mancha de sol, la cámara
 *     que cambió su exposición, un piso aprendido con otra luz, las juntas...) sin mirar ni guardar ninguna imagen del carro. Este módulo
 *     resume cada cuadro en unos cuantos NÚMEROS que la app escribe en la caja negra de la corrida.
 *  2. Qué mide, sobre las celdas de la zona de avance (las mismas que usa obstaculos.c):
 *       - a qué distancia de color queda cada celda del grupo de piso más cercano (suma |dR| + |dG| + |dB|): histograma, media y
 *         percentiles. Con el piso bien aprendido casi todo cae en las dos primeras cubetas; una mancha de sol sube unas celdas a las
 *         cubetas de en medio; un cambio de exposición corre TODO el histograma a la vez;
 *       - el brillo (media y percentiles 10 y 90), cuántas celdas están quemadas o negras, y cuánto color tienen (croma);
 *       - cuántas celdas «no piso» hay por zona (izquierda, centro, derecha) y por banda de distancia (lejos, media, cerca), y por fila;
 *       - los grupos de color del piso que tiene el modelo (color y peso), la ganancia con que se reajustó el brillo y los motivos.
 *  3. Cómo se usa: vision_tele_calcular() con las celdas promediadas del cuadro (obstaculos_promediar), el modelo, la configuración y el
 *     resultado del análisis; vision_tele_texto() arma UNA línea de texto «VTEL ...» (solo números) para el registro.
 *  4. No cambia nada del detector: solo lee. Solo enteros, sin malloc ni recursión; los errores son códigos de retorno.
 */
#ifndef VISION_TELE_H
#define VISION_TELE_H

#include "obstaculos.h"

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

#define VT_OK             0
#define VT_ERR_ARGUMENTO  1
#define VT_ERR_TAMANO     2

#define VT_CUBETAS        8     /* distancia al piso: 0-9, 10-19, 20-29, 30-39, 40-59, 60-89, 90-149, 150 o más */
#define VT_BANDAS         3     /* lejos, media, cerca (las mismas bandas que la cercanía 1, 2 y 3) */
#define VT_TEXTO_MAX    640     /* capacidad recomendada para vision_tele_texto */

typedef struct {
    int columnas, filas, fila_minima;      /* la cuadrícula y la primera fila de la zona de avance */
    int celdas;                            /* celdas de la zona de avance que se midieron */
    int con_modelo;                        /* 1 si había un piso aprendido contra el cual medir la distancia */
    int hist[VT_CUBETAS];                  /* celdas por cubeta de distancia al grupo de piso más cercano */
    int dist_media, dist_p50, dist_p90, dist_max;
    int brillo_media, brillo_p10, brillo_p90;   /* brillo = (R + G + B) / 3 de la celda */
    int quemadas, negras;                  /* celdas con algún canal en 250 o más / con los tres en 8 o menos */
    int croma_media;                       /* media de (mayor canal - menor canal) */
    int no_piso[OBS_ZONAS][VT_BANDAS];     /* celdas «no piso» por zona y banda */
    int no_piso_fila[OBS_MAX_FILAS];       /* celdas «no piso» en cada fila de la zona de avance (toda la anchura) */
    int no_piso_total;
    int grupos;                            /* grupos de color del piso */
    int grupo_color[OBS_MAX_GRUPOS][3];
    int grupo_peso[OBS_MAX_GRUPOS];
    int umbral_color, min_celdas;          /* lo que estaba configurado */
    int ganancia_mil, brillo_zona, motivo; /* del resultado del análisis */
    int ref_r, ref_g, ref_b;               /* color de la franja de referencia en este cuadro */
} VisionTele;

/* Calcula la telemetría de un cuadro. `celdas` son columnas x filas x 3 bytes (R, G, B promediados por celda, por filas), como las entrega
 * obstaculos_promediar o el paquete de celdas de la cámara. `modelo` puede ser NULL o no válido (entonces no hay distancias: con_modelo = 0).
 * `resultado` es el del análisis de ESE cuadro (da la rejilla «no piso», la fila mínima, la ganancia y los motivos). */
int vision_tele_calcular(const unsigned char *celdas, int columnas, int filas, const ModeloPiso *modelo, const ConfigObstaculos *config,
                         const ResultadoObstaculos *resultado, VisionTele *tele);

/* Una línea «VTEL ...» con todos los números. Devuelve VT_OK, o VT_ERR_TAMANO si no cabe (el texto queda cortado pero terminado en 0). */
int vision_tele_texto(const VisionTele *tele, char *texto, int capacidad);

#ifdef __cplusplus
}
#endif

#endif
