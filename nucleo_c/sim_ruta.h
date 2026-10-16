/*
 * sim_ruta.h — PROBAR LA RUTA SIN EL CARRO: corre el navegador REAL (navegador.c) con un carro virtual (simulador_navegador.c) y devuelve el recorrido.
 *
 * GUÍA DE LECTURA
 *  1. sr_simular() recibe la ruta (celdas vecinas), la calibración del carro (o NULL = valores por defecto), los obstáculos virtuales (celdas de la cuadrícula) y
 *     un ruido de giro. Corre el ciclo del navegador cada 50 ms con el carro virtual y anota el recorrido cada 100 ms para que la pantalla lo anime.
 *  2. OBSTÁCULO VIRTUAL: una celda de la cuadrícula (20 x 20 cm): una CAJA de ~20 cm que la cámara virtual ve ocupando varias celdas del detector (más cuanto más cerca). En cada vuelta se calcula qué «vería» la cámara según dónde está el carro y hacia dónde mira:
 *     solo ve lo que está adelante dentro de ~60 grados de campo de visión y a menos de 80 cm; la zona (izquierda, centro, derecha) sale de a qué lado cae y la
 *     cercanía (1 lejos, 2 media, 3 muy cerca) de la distancia a su cara. Es un modelo APROXIMADO (la cámara real puede ver distinto), pero sirve para ver la maniobra
 *     de esquivar y comprobar que la ruta tiene espacio.
 *  3. RUIDO DE GIRO: un giro de 90 grados sale 90 + d grados, con d entre -ruido y +ruido (sr_dispersion repite la ruta con ruidos distintos para mostrar la dispersión).
 *  4. Resultado: si llegó, cuánto tardó, cuántos comandos mandó, cuántas veces esquivó, dónde terminó y ADVERTENCIAS (sale de la cuadrícula, no llegó...).
 *  5. Solo enteros, sin malloc ni recursión. El generador de números es propio (determinista): la misma semilla da el mismo resultado.
 */
#ifndef SIM_RUTA_H
#define SIM_RUTA_H

#include <stdint.h>
#include "navegador.h"
#include "ruta_editor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SR_MAX_MUESTRAS      1500   /* una muestra cada 100 ms: hasta 150 s */
#define SR_MAX_OBSTACULOS      16
#define SR_MAX_CAMBIOS        256   /* cambios de comando que se anotan */
#define SR_DURACION_MAX_MS 150000   /* si no termina en 150 s se corta */
#define SR_MUESTRA_CADA_MS    100
#define SR_MAX_CORRIDAS        24   /* corridas de sr_dispersion */

/* Códigos de retorno. */
#define SR_OK                  0
#define SR_ERR_ARGUMENTO      -1
#define SR_ERR_RUTA           -2   /* el navegador no aceptó la ruta (ver `codigo_ruta`) */

/* Advertencias (bits). */
#define SR_ADV_SALE_CUADRICULA  1   /* el carro salió de la cuadrícula en algún momento */
#define SR_ADV_NO_LLEGO         2   /* terminó en ERROR o no acabó a tiempo */
#define SR_ADV_CORTADA          4   /* la simulación se cortó por tiempo (150 s) */
#define SR_ADV_CALIBRACION      8   /* se usaron valores por defecto (no hay calibración válida) */

typedef struct {
    uint32_t t_ms;
    int x_mm, y_mm;       /* posición del carro virtual (0,0 = centro de la celda columna 0, fila 0) */
    int rumbo_grados;     /* 0 = arriba, 90 = derecha */
    int comando;          /* ComandoNav */
    int estado;           /* EstadoNav */
} MuestraSim;

typedef struct {
    uint32_t t_ms;
    int comando;
} CambioSim;

typedef struct {
    int codigo;            /* SR_OK o un error */
    int codigo_ruta;       /* código de trayectoria.h si la ruta no es válida */
    int llego;             /* 1 si el navegador terminó la ruta (TERMINADO) */
    int estado_final;      /* EstadoNav */
    int motivo_error;      /* MotivoErrorNav si terminó en ERROR */
    int error_causa;       /* NAV_CAUSA_* (causa concreta del ERROR, ver navegador.h) */
    uint32_t duracion_ms;
    int n_comandos;        /* cuántas veces cambió el comando */
    int esquivadas;
    int advertencias;      /* bits SR_ADV_* */
    int x_final_mm, y_final_mm;
    int error_final_mm;    /* distancia entre donde terminó y el último punto de la ruta */
    int n_muestras;
    int n_cambios;
    MuestraSim muestras[SR_MAX_MUESTRAS];
    CambioSim  cambios[SR_MAX_CAMBIOS];
} ResultadoSimRuta;

/* Corre una simulación. `ruta` son celdas vecinas (re_expandir). `obstaculos` son celdas de la cuadrícula. `ruido_giro_grados` = cuánto puede errar un giro de 90 grados
 * (0 = giro perfecto; el signo de `semilla` elige el lado y el tamaño del error). */
int sr_simular(const Punto *ruta, int n_ruta, const CalibracionCarro *cal, const Punto *obstaculos, int n_obstaculos, int ruido_giro_grados, uint32_t semilla, ResultadoSimRuta *r);

typedef struct {
    int llego;
    int x_final_mm, y_final_mm;
    int error_final_mm;
    uint32_t duracion_ms;
} CorridaSim;

typedef struct {
    int n;                     /* corridas hechas */
    CorridaSim corridas[SR_MAX_CORRIDAS];
    int llegaron;
    int error_medio_mm;        /* promedio del error final */
    int error_max_mm;
    int dispersion_mm;         /* distancia más grande entre dos finales (qué tan abierta queda la nube de puntos) */
} DispersionSim;

/* Repite la ruta `corridas` veces (hasta SR_MAX_CORRIDAS) con ruido de giro distinto cada vez (semillas 1, 2, 3...). */
int sr_dispersion(const Punto *ruta, int n_ruta, const CalibracionCarro *cal, const Punto *obstaculos, int n_obstaculos, int ruido_giro_grados, int corridas, DispersionSim *d);

/* El equilibrio de los dos lados del carro VIRTUAL (ver simulador_navegador.h): `desbalance_mil` cuánto más rápido empuja el lado izquierdo (1000 = igual) y `balance_mil` el factor con que
 * se reduce su potencia (1000 = sin reducir). Con desbalance x balance = 1000 x 1000 el carro virtual va recto; si no, se tuerce y la ruta sale chueca. Valen para las simulaciones que vengan
 * después. Por defecto 1000 y 1000 (sin desvío). Fuera de 500 a 2000 el valor se ignora. */
void sr_poner_balance(int desbalance_mil, int balance_mil);

/* Qué vería la cámara: rellena zona[3] (hay, cercanía, celdas) para un carro en (x_mm, y_mm) con rumbo `rumbo_grados`. Se deja pública para probarla. */
void sr_vista(int x_mm, int y_mm, int rumbo_grados, const Punto *obstaculos, int n_obstaculos, int hay[3], int cercania[3], int celdas[3]);

/* Por qué NO llegó, en una frase para la pantalla (por ejemplo "El obstáculo bloqueó el camino y se agotaron los 2 reintentos."). Devuelve el largo escrito; 0 si llegó
 * o si no hay una causa que contar. */
int sr_texto_causa(const ResultadoSimRuta *r, char *texto, int capacidad);

/* Texto corto de las advertencias en español para la pantalla (una frase por bit; devuelve cuántas escribió). */
int sr_texto_advertencia(int bit, char *texto, int capacidad);

#ifdef __cplusplus
}
#endif

#endif /* SIM_RUTA_H */
