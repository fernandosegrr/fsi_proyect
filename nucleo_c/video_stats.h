/*
 * video_stats.h — Estadística de la llegada de cuadros de video (para ver los "tropiezos").
 *
 * GUÍA DE LECTURA
 *  1. Cada vez que llega un cuadro se llama vs_cuadro(ahora_ms). Se guarda cuánto tiempo pasó desde el cuadro anterior (el "hueco").
 *  2. La memoria es de VS_VENTANA_S segundos (10): un cubo por segundo con cuántos cuadros llegaron, el hueco más largo y cuántos
 *     huecos pasaron de VS_HUECO_LARGO_MS (500 ms). Los cubos viejos se descartan solos.
 *  3. vs_resumen(ahora_ms) suma los cubos y devuelve: cuadros por segundo reales, el hueco más largo, cuántos huecos fueron largos
 *     y el hueco que está pasando AHORA (el tiempo desde el último cuadro): así un tropiezo que todavía no termina también se ve.
 *  4. Solo números: nunca guarda imágenes.
 */
#ifndef VIDEO_STATS_H
#define VIDEO_STATS_H

#include <stdint.h>

#define VS_VENTANA_S        10     /* segundos que recuerda */
#define VS_HUECO_LARGO_MS  500     /* un hueco mayor a esto cuenta como "tropiezo" */

typedef struct {
    int      cuadros;          /* cuadros en el cubo */
    int      hueco_max_ms;     /* hueco más largo que terminó en este segundo */
    int      huecos_largos;    /* huecos mayores a VS_HUECO_LARGO_MS que terminaron en este segundo */
    uint32_t segundo;          /* a qué segundo (ahora_ms / 1000) pertenece el cubo; sirve para saber si es viejo */
    int      en_uso;
} CuboVideo;

typedef struct {
    CuboVideo cubos[VS_VENTANA_S];
    int       tiene_ultimo;
    uint32_t  t_ultimo_ms;     /* hora del último cuadro */
    uint32_t  t_primero_ms;    /* hora del primer cuadro (para los fps mientras no pasan 10 s) */
} VideoStats;

typedef struct {
    int fps_x10;               /* cuadros por segundo reales, por 10 (la ventana es la más corta entre 10 s y el tiempo que lleva) */
    int hueco_max_ms;          /* el hueco más largo de la ventana (incluye el que está pasando ahora) */
    int huecos_largos;         /* cuántos huecos pasaron de VS_HUECO_LARGO_MS (incluye el que está pasando ahora) */
    int cuadros;               /* cuadros en la ventana */
    int hueco_actual_ms;       /* tiempo desde el último cuadro (0 si todavía no llegó ninguno) */
} ResumenVideo;

void vs_iniciar(VideoStats *vs);

/* Anota la llegada de un cuadro a la hora `ahora_ms`. */
void vs_cuadro(VideoStats *vs, uint32_t ahora_ms);

/* Resume los últimos VS_VENTANA_S segundos vistos desde `ahora_ms`. */
ResumenVideo vs_resumen(const VideoStats *vs, uint32_t ahora_ms);

#endif /* VIDEO_STATS_H */
