/*
 * simulador_navegador.h — Un carro "de mentira" en el PC para probar navegador.c.
 *
 * GUÍA DE LECTURA
 *  1. El simulador hace de carro real: recibe el comando que devuelve navegador_tick y lo
 *     "ejecuta" durante 50 ms, moviéndose o girando con la misma calibración (calibracion.h).
 *     Se puede hacer que el carro real sea distinto (pct_avance, pct_giro) para ver la deriva.
 *  2. También hace de cámara: según una lista de EVENTOS programados (obstáculo en tal zona
 *     entre tal y tal hora) arma el ResultadoObstaculos que vería el detector.
 *  3. sim_correr() repite el ciclo (tick → mover → siguiente tick) y apunta en una bitácora
 *     cada cambio de comando, para que las pruebas revisen la secuencia.
 *  4. Cuenta las "violaciones de seguridad": veces que el navegador mandó un movimiento
 *     estando en ESPERANDO, TERMINADO o ERROR (siempre debe ser cero).
 */
#ifndef SIMULADOR_NAVEGADOR_H
#define SIMULADOR_NAVEGADOR_H

#include <stdint.h>
#include "navegador.h"

#define SIM_MAX_EVENTOS   8
#define SIM_MAX_CAMBIOS 256

/* Un obstáculo programado: visible en una zona entre t_ini_ms (incluido) y t_fin_ms (excluido). */
typedef struct {
    uint32_t t_ini_ms;
    uint32_t t_fin_ms;
    int zona;        /* ZONA_IZQUIERDA, ZONA_CENTRO o ZONA_DERECHA */
    int cercania;    /* 1 a 3 */
} EventoObstaculo;

/* Un cambio de comando en la bitácora. */
typedef struct {
    uint32_t t_ms;
    ComandoNav comando;
    EstadoNav estado;    /* estado del navegador cuando se devolvió ese comando */
} CambioComando;

typedef struct {
    /* El carro "real": posición en centésimas de milímetro y rumbo en centésimas de grado. */
    long x_cmm, y_cmm, rumbo_cgrados;
    int pct_avance;      /* 100 = avanza justo lo que dice la calibración; 95 = 5 % menos */
    int pct_giro;

    /* Cámara simulada. */
    EventoObstaculo eventos[SIM_MAX_EVENTOS];
    int n_eventos;
    int periodo_cuadro_ms;   /* cada cuántos ms llega un cuadro nuevo (puede no ser múltiplo de NAV_TICK_MS: 80, 120...;
                                el cuadro llega al primer tick que sigue a su hora) */
    uint32_t proximo_cuadro_ms;   /* hora en que toca el siguiente cuadro */

    /* Bitácora y revisiones. */
    CambioComando cambios[SIM_MAX_CAMBIOS];
    int n_cambios;
    int violaciones_seguridad;
    uint32_t t_ms;           /* reloj simulado */
    uint32_t t_cancelar_ms;  /* si es distinto de 0xFFFFFFFF, se cancela a esa hora */
} Simulador;

/* Coloca el carro en el centro de la celda (columna, fila) mirando hacia `rumbo_grados`. */
void sim_iniciar(Simulador *sim, int columna, int fila, int rumbo_grados);

/* Agrega un obstáculo programado. Devuelve 0 si cupo, -1 si no. */
int sim_agregar_obstaculo(Simulador *sim, uint32_t t_ini_ms, uint32_t t_fin_ms, int zona, int cercania);

/* Corre el ciclo durante `duracion_ms`. Devuelve el último comando. */
ComandoNav sim_correr(Simulador *sim, Navegador *nav, uint32_t duracion_ms);

/* Posición real del carro simulado en milímetros y rumbo en grados. */
int sim_x_mm(const Simulador *sim);
int sim_y_mm(const Simulador *sim);
int sim_rumbo_grados(const Simulador *sim);

/* Cuántas veces cambió el comando a `comando` (para revisar la secuencia). */
int sim_contar_cambios_a(const Simulador *sim, ComandoNav comando);

/* Hora del primer cambio a `comando` que ocurre en o después de `desde_ms`; -1 si no hay. */
long sim_primer_cambio_a(const Simulador *sim, ComandoNav comando, uint32_t desde_ms);

#endif /* SIMULADOR_NAVEGADOR_H */
