/*
 * trayectoria.c — De puntos en una cuadrícula a pasos AVANZAR / GIRAR.
 *
 * GUÍA DE LECTURA
 *  1. Las 8 direcciones son índices 0..7 (múltiplos de 45°): 0 = arriba,
 *     1 = arriba-derecha, 2 = derecha ... 7 = arriba-izquierda (sentido
 *     de las manecillas del reloj).
 *  2. direccion_entre() dice en qué dirección va de un punto al siguiente.
 *  3. validar_ruta() revisa TODA la ruta antes de calcular nada.
 *  4. giro_grados() da el giro entre dos direcciones, de -180 a 180.
 *  5. trayectoria_calcular_desde() recorre la ruta, junta tramos de la misma
 *     dirección y escribe primero el GIRAR y luego el AVANZAR de cada tramo.
 *     (trayectoria_calcular() es el caso en que el carro empieza mirando hacia arriba.)
 *  Todo es entero y de tamaño fijo: no hay malloc ni math.h.
 */

#include <stddef.h>
#include "trayectoria.h"
#include "calibracion.h"

/* ── Direcciones ───────────────────────────────────────────────────────── */

#define SIN_DIRECCION  (-1)

/* Tabla [df+1][dc+1] con el índice de dirección. Se ve como la cuadrícula
 * vista desde arriba (fila hacia abajo). El centro (0,0) no es dirección. */
static const int TABLA_DIRECCION[3][3] = {
    { 7, 0, 1 },                   /* df = -1: arriba */
    { 6, SIN_DIRECCION, 2 },       /* df =  0 */
    { 5, 4, 3 }                    /* df = +1: abajo */
};

/* Dirección (0..7) para ir de a a b si son vecinos; SIN_DIRECCION si son el
 * mismo punto o si el salto es mayor que una celda. */
static int direccion_entre(const Punto *a, const Punto *b)
{
    int dc = b->columna - a->columna;
    int df = b->fila - a->fila;

    if (dc < -1 || dc > 1 || df < -1 || df > 1) {
        return SIN_DIRECCION;
    }
    return TABLA_DIRECCION[df + 1][dc + 1];
}

/* Las direcciones impares (1, 3, 5, 7) son diagonales. */
static int es_diagonal(int direccion)
{
    return (direccion % 2) == 1;
}

/* ── Validación ────────────────────────────────────────────────────────── */

/* Revisa toda la ruta antes de generar pasos, así un error no deja salida a
 * medias. Devuelve TRAY_OK o el código de error, y el índice del punto malo. */
static int validar_ruta(const Punto *puntos, int n_puntos, int *indice_error)
{
    int i;

    for (i = 1; i < n_puntos; i++) {
        int dc = puntos[i].columna - puntos[i - 1].columna;
        int df = puntos[i].fila - puntos[i - 1].fila;

        if (dc == 0 && df == 0) {
            *indice_error = i;
            return TRAY_ERR_REPETIDO;
        }
        if (direccion_entre(&puntos[i - 1], &puntos[i]) == SIN_DIRECCION) {
            *indice_error = i;
            return TRAY_ERR_SALTO;
        }
    }
    return TRAY_OK;
}

/* ── Giros y distancias ────────────────────────────────────────────────── */

/* Giro en grados para pasar de la dirección "desde" a "hasta": positivo a la
 * derecha, negativo a la izquierda, normalizado a -180..180. El medio giro
 * (180) se da como +180. */
static int giro_grados(int desde, int hasta)
{
    int pasos45 = ((hasta - desde) % 8 + 8) % 8;   /* 0..7 */

    if (pasos45 > 4) {
        pasos45 -= 8;                              /* -3..-1 */
    }
    return pasos45 * 45;
}

/* Centímetros de un tramo recto de `celdas` celdas. La diagonal se calcula
 * sobre el total (no celda por celda) para no acumular el redondeo. */
static int distancia_cm(int direccion, int celdas)
{
    if (es_diagonal(direccion)) {
        return celdas * CELDA_CM * DIAGONAL_CM_MIL / 1000;
    }
    return celdas * CELDA_CM;
}

static int valor_absoluto(int x)
{
    return (x < 0) ? -x : x;
}

/* ── Salida ────────────────────────────────────────────────────────────── */

/* Agrega un paso si hay lugar. Devuelve 1 si lo agregó, 0 si no cabe. */
static int agregar_paso(Paso *pasos, int capacidad, int *n_pasos,
                        TipoPaso tipo, int valor, int tiempo_ms)
{
    if (*n_pasos >= capacidad) {
        return 0;
    }
    pasos[*n_pasos].tipo      = tipo;
    pasos[*n_pasos].valor     = valor;
    pasos[*n_pasos].tiempo_ms = tiempo_ms;
    (*n_pasos)++;
    return 1;
}

/* ── Función principal ─────────────────────────────────────────────────── */

int trayectoria_calcular(const Punto *puntos, int n_puntos,
                         Paso *pasos, int capacidad, int *n_pasos,
                         int *indice_error)
{
    return trayectoria_calcular_desde(ORIENTACION_INICIAL, puntos, n_puntos,
                                      pasos, capacidad, n_pasos, indice_error);
}

int trayectoria_calcular_desde(int orientacion_inicial, const Punto *puntos, int n_puntos,
                               Paso *pasos, int capacidad, int *n_pasos,
                               int *indice_error)
{
    int indice_local = 0;
    int orientacion  = orientacion_inicial;
    int codigo;
    int i;

    if (indice_error == NULL) {
        indice_error = &indice_local;
    }
    *indice_error = 0;
    if (n_pasos != NULL) {
        *n_pasos = 0;
    }
    if (puntos == NULL || pasos == NULL || n_pasos == NULL ||
        n_puntos < 1 || capacidad < 1 ||
        orientacion_inicial < 0 || orientacion_inicial > 7) {
        return TRAY_ERR_ARGUMENTO;
    }
    if (n_puntos > MAX_PUNTOS) {
        return TRAY_ERR_DEMASIADOS_PUNTOS;
    }
    codigo = validar_ruta(puntos, n_puntos, indice_error);
    if (codigo != TRAY_OK) {
        return codigo;
    }

    i = 1;
    while (i < n_puntos) {
        int direccion = direccion_entre(&puntos[i - 1], &puntos[i]);
        int celdas = 1;
        int giro, cm;

        /* Juntar los tramos siguientes que van en la misma dirección. */
        while (i + celdas < n_puntos &&
               direccion_entre(&puntos[i + celdas - 1], &puntos[i + celdas]) == direccion) {
            celdas++;
        }

        giro = giro_grados(orientacion, direccion);
        if (giro != 0 &&
            !agregar_paso(pasos, capacidad, n_pasos, PASO_GIRAR, giro,
                          valor_absoluto(giro) * MS_POR_GRADO)) {
            *n_pasos = 0;
            return TRAY_ERR_DEMASIADOS_PASOS;
        }

        cm = distancia_cm(direccion, celdas);
        if (!agregar_paso(pasos, capacidad, n_pasos, PASO_AVANZAR, cm,
                          cm * MS_POR_CM)) {
            *n_pasos = 0;
            return TRAY_ERR_DEMASIADOS_PASOS;
        }

        orientacion = direccion;
        i += celdas;
    }
    return TRAY_OK;
}

const char *trayectoria_texto_error(int codigo)
{
    switch (codigo) {
    case TRAY_OK:                     return "Ruta correcta";
    case TRAY_ERR_ARGUMENTO:          return "Datos de entrada no validos";
    case TRAY_ERR_SALTO:              return "Un punto no es vecino del anterior";
    case TRAY_ERR_REPETIDO:           return "Un punto esta repetido";
    case TRAY_ERR_DEMASIADOS_PUNTOS:  return "La ruta tiene demasiados puntos";
    case TRAY_ERR_DEMASIADOS_PASOS:   return "La ruta genera demasiados pasos";
    default:                          return "Error desconocido";
    }
}
