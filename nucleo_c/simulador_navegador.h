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
#include "calibracion.h"

#define SIM_MAX_EVENTOS   8
#define SIM_MAX_CAMBIOS 256

/* Un obstáculo programado: visible en una zona entre t_ini_ms (incluido) y t_fin_ms (excluido). */
typedef struct {
    uint32_t t_ini_ms;
    uint32_t t_fin_ms;
    int zona;        /* ZONA_IZQUIERDA, ZONA_CENTRO o ZONA_DERECHA */
    int cercania;    /* 1 a 3 */
    int celdas;      /* cuántas celdas de obstáculo ve el detector en esa zona (por defecto 10) */
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
    /* Calibración del carro REAL simulado (por defecto la de ejemplo: sin arranque ni frenado). Si es
     * distinta de la que usa el navegador, el navegador se equivoca como con un carro mal calibrado.
     * Un comando de movimiento no mueve el carro durante `arranque_ms` y, al soltarlo, el carro sigue
     * avanzando (o girando) el frenado. */
    CalibracionCarro real;
    /* Cámara que mide el giro (giro visual): con `visual_activo` = 1, cada cuadro le avisa al navegador cuánto giró el carro REAL desde
     * el cuadro anterior (navegador_aporte_visual), con un error de `visual_error_pct` por ciento (+10 = mide 10 % de más) y sin
     * confianza entre `visual_perdida_desde_ms` y `visual_perdida_hasta_ms` (por ejemplo, un cuadro tapado). */
    int visual_activo;
    int visual_error_pct;
    uint32_t visual_perdida_desde_ms;
    uint32_t visual_perdida_hasta_ms;
    int visual_latencia_ms;           /* retraso de la cámara: cada medida llega al navegador este tiempo después de la hora del cuadro */
    long giro_desde_cuadro_cgrados;   /* giro real (derecha +) desde el último cuadro, en centésimas de grado */
    long pend_medido_x10[64];         /* medidas que todavía "viajan" por el retraso: giro, tiempo entre cuadros, si es confiable y hora de llegada */
    int pend_dt_ms[64];
    int pend_confiable[64];
    uint32_t pend_llegada_ms[64];
    int n_pendientes;
    uint32_t t_cuadro_visual_ms;      /* hora del último cuadro visual */
    /* Equilibrio de los lados (recta). `desbalance_mil` = cuánto más rápido empuja el lado IZQUIERDO que el derecho con la misma potencia (1000 = igual, 1120 = 12 % más);
     * `balance_mil` = el factor con que el firmware reduce la potencia del lado izquierdo (1000 = sin reducir, 893 = 10.7 % menos). El carro avanza recto cuando
     * desbalance x balance = 1000 x 1000; si no, al avanzar o retroceder se tuerce (hacia la derecha si el izquierdo gana). Por defecto los dos valen 1000 (sin desvío). */
    int desbalance_mil;
    int balance_mil;
    ComandoNav cmd_previo;   /* comando del tick anterior */
    int ms_en_comando;       /* cuánto lleva activo el comando actual */
    /* Desplazamiento del centro al girar (sim->real.desp_*): lo que lleva el giro actual. */
    long giro_h0_cgrados;    /* rumbo con el que empezó el comando de giro actual */
    long giro_acum_cgrados;  /* cuánto lleva girado en este comando (valor absoluto, con lo que sigue girando al frenar) */
    long desp_x_cmm, desp_y_cmm;   /* cuánto se ha movido ya el centro por ese giro */

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

/* Igual que sim_agregar_obstaculo pero con el número de celdas de obstáculo de la zona (sirve para probar la elección del lado). */
int sim_agregar_obstaculo_celdas(Simulador *sim, uint32_t t_ini_ms, uint32_t t_fin_ms, int zona, int cercania, int celdas);

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
