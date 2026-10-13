/*
 * trayectoria.h — Convierte una ruta dibujada en una cuadrícula en una lista
 * de pasos (AVANZAR / GIRAR) con su tiempo en milisegundos.
 *
 * C estándar, solo enteros, sin malloc ni math.h: sirve en el PC y, más
 * adelante, desde Android con el NDK.
 */
#ifndef TRAYECTORIA_H
#define TRAYECTORIA_H

#include "calibracion.h"

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

/* ── Constantes ────────────────────────────────────────────────────────── */

#define CELDA_CM            20    /* lado de una celda de la cuadrícula, en cm */
#define DIAGONAL_CM_MIL   1414    /* raíz de 2 por 1000: diagonal = celda*1414/1000 */
#define MAX_PUNTOS          64    /* puntos máximos de una ruta */
/* Pasos máximos de salida. Cada tramo da a lo más un GIRAR y un AVANZAR y hay
 * a lo más MAX_PUNTOS-1 tramos, así que una ruta válida nunca desborda. */
#define MAX_PASOS  (2 * MAX_PUNTOS)

/* Dirección inicial del carro: índice de dirección (0 = "arriba"). */
#define ORIENTACION_INICIAL  0

/* ── Tipos ─────────────────────────────────────────────────────────────── */

/* Un punto de la cuadrícula. La fila crece hacia ABAJO (como en pantalla). */
typedef struct {
    int columna;
    int fila;
} Punto;

typedef enum {
    PASO_AVANZAR,   /* valor = centímetros */
    PASO_GIRAR      /* valor = grados; positivo = derecha, negativo = izquierda */
} TipoPaso;

typedef struct {
    TipoPaso tipo;
    int      valor;
    int      tiempo_ms;
} Paso;

/* Códigos de retorno de trayectoria_calcular. */
#define TRAY_OK                      0
#define TRAY_ERR_ARGUMENTO          -1   /* puntero nulo, sin puntos o sin capacidad */
#define TRAY_ERR_SALTO              -2   /* un punto no es vecino (8 direcciones) del anterior */
#define TRAY_ERR_REPETIDO           -3   /* un punto igual al anterior */
#define TRAY_ERR_DEMASIADOS_PUNTOS  -4   /* más de MAX_PUNTOS */
#define TRAY_ERR_DEMASIADOS_PASOS   -5   /* la salida no cabe en la capacidad dada */

/* ── Funciones ─────────────────────────────────────────────────────────── */

/* Calcula los pasos de la ruta. Devuelve TRAY_OK o un código de error; nunca
 * aborta. Deja en *n_pasos cuántos pasos escribió (0 si hubo error).
 * Si indice_error no es NULL y hay error de salto o repetido, recibe el
 * índice del punto problemático (0 en los demás casos). */
int trayectoria_calcular(const Punto *puntos, int n_puntos,
                         Paso *pasos, int capacidad, int *n_pasos,
                         int *indice_error);

/* Igual que trayectoria_calcular, pero el carro empieza mirando hacia `orientacion_inicial`
 * (índice de dirección 0 a 7: 0 = arriba, 1 = arriba-derecha, 2 = derecha...). Sirve para
 * replanificar desde la posición y el rumbo en que el carro ya está (ver navegador.c).
 * trayectoria_calcular equivale a usar ORIENTACION_INICIAL. */
int trayectoria_calcular_desde(int orientacion_inicial, const Punto *puntos, int n_puntos,
                               Paso *pasos, int capacidad, int *n_pasos,
                               int *indice_error);

/* Igual que trayectoria_calcular_desde, pero los tiempos de cada paso salen de una calibración
 * medida del carro (arranque, ms por cm, frenado, ms por grado de cada lado; ver calibracion.h).
 * Con `cal` = NULL usa la calibración por defecto (MS_POR_CM y MS_POR_GRADO), igual que las demás. */
int trayectoria_calcular_desde_cal(const CalibracionCarro *cal, int orientacion_inicial,
                                   const Punto *puntos, int n_puntos,
                                   Paso *pasos, int capacidad, int *n_pasos,
                                   int *indice_error);

/* Texto corto en español para un código de retorno (para mostrar al usuario). */
const char *trayectoria_texto_error(int codigo);

#ifdef __cplusplus
}
#endif

#endif /* TRAYECTORIA_H */
