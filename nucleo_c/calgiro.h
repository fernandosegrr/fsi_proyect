/*
 * calgiro.h — Calibración del GIRO con el giroscopio: el carro hace 3 giros a cada lado y los ms por grado y el frenado salen del ángulo REAL que mide el giroscopio, sin que nadie mida nada.
 *
 * GUÍA DE LECTURA
 *  1. La app la maneja así: calgiro_iniciar() (el USUARIO toca un botón, con el carro quieto y el espacio despejado) y después, cada ~50 ms, calgiro_tick() con el ángulo del giroscopio (lo último que
 *     dijo SENS, en décimas de grado, POSITIVO a la izquierda). calgiro_tick devuelve el comando que el carro debe mantener: CG_CMD_STOP, CG_CMD_IZQ (LEFT) o CG_CMD_DER (RIGHT).
 *  2. Son 6 giros alternados (izquierda, derecha, izquierda...) de unos 40, 70 y 100 grados, por TIEMPO (como siempre): para cada uno espera a que el carro esté quieto (el ángulo no cambia), manda LEFT o
 *     RIGHT durante el tiempo planeado, frena, espera CG_ASENTAR_MS a que termine de girar y anota el ángulo REAL que giró el giroscopio.
 *  3. Con los 3 pares (ms, grados) de cada lado se ajusta una recta (cal_ajustar_giro, el mismo ajuste de la calibración manual con el arranque que ya estaba guardado): sale ms por grado y frenado de cada lado.
 *     El desplazamiento del pivote NO se toca (sigue siendo manual).
 *  4. Seguridad: si el giroscopio deja de dar el ángulo, si el carro no gira, si gira al revés o más de CG_MAX_GRADOS, para y da error. Nunca deja un comando de giro puesto al terminar.
 *  5. Al terminar calgiro_resultado() da la calibración nueva y un resumen con cuánto cambió cada valor contra el guardado (cambio_grande si pasó de CG_CAMBIO_AVISO_PCT %). La app decide si la guarda.
 *  Solo enteros, sin malloc.
 */
#ifndef CALGIRO_H
#define CALGIRO_H

#include <stdint.h>
#include "calibracion.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CG_GIROS_POR_LADO      3
#define CG_TOTAL_GIROS         (2 * CG_GIROS_POR_LADO)

/* Lo que devuelve calgiro_tick: el comando que el carro debe mantener. */
#define CG_CMD_STOP            0
#define CG_CMD_IZQ             1
#define CG_CMD_DER             2

/* Estados. */
#define CG_PARADO              0   /* todavía no se inició */
#define CG_PREPARANDO          1   /* STOP: esperando a que el carro esté quieto */
#define CG_GIRANDO             2   /* mandando LEFT o RIGHT el tiempo planeado */
#define CG_ASENTANDO           3   /* STOP: el carro termina de girar y se mide el ángulo real */
#define CG_LISTO               4   /* los 6 giros hechos y la calibración calculada */
#define CG_ERROR               5   /* ver calgiro_error */
#define CG_CANCELADO           6

/* Errores (calgiro_error). */
#define CG_ERR_NINGUNO         0
#define CG_ERR_SIN_GIROSCOPIO  1   /* dejó de llegar el ángulo */
#define CG_ERR_NO_GIRO         2   /* el carro no giró (¿sin pilas de motores?) */
#define CG_ERR_SENTIDO         3   /* giró al revés (el ángulo salió contrario al pedido) */
#define CG_ERR_MUCHO           4   /* un giro pasó de CG_MAX_GRADOS */
#define CG_ERR_TIEMPO          5   /* tardó demasiado en total */
#define CG_ERR_INCONSISTENTE   6   /* los giros de un lado no siguen una recta (resultados muy distintos entre sí) */
#define CG_ERR_AJUSTE          7   /* el ajuste dio un valor fuera de lo razonable (ver calgiro_codigo_ajuste) */

/* Parámetros. */
#define CG_MS_MIN              300     /* duración mínima y máxima de un giro de prueba */
#define CG_MS_MAX              1600
#define CG_QUIETO_MS           800     /* el ángulo no debe cambiar más de CG_QUIETO_TOL_X10 en este tiempo antes de cada giro */
#define CG_QUIETO_TOL_X10      8
#define CG_ASENTAR_MS          1500    /* tiempo mínimo después del STOP antes de leer el ángulo final (y debe estar quieto CG_QUIETO_FINAL_MS) */
#define CG_QUIETO_FINAL_MS     400
#define CG_SIN_ANGULO_MS       1500    /* sin ángulo nuevo en este tiempo: error */
#define CG_TIEMPO_MAX_MS       90000
#define CG_MIN_GRADOS_X10      80      /* un giro de menos de 8 grados = el carro no giró */
#define CG_MAX_GRADOS          190
#define CG_INCONSISTENTE_X10   60      /* un punto a más de 6 grados de la recta (o 6 % del giro) = inconsistente */
#define CG_CAMBIO_AVISO_PCT    10      /* si un valor cambia más que esto contra lo guardado, se avisa */

typedef struct {
    int lado;                    /* CAL_LADO_IZQUIERDA o CAL_LADO_DERECHA */
    int ms_x10_antes, ms_x10_despues;           /* ms por grado por 10: lo guardado y lo medido */
    int frenado_x10_antes, frenado_x10_despues; /* grados por 10 que sigue girando después del STOP */
    int cambio_ms_pct;           /* cuánto cambió el ms por grado (con signo, por ciento) */
    int cambio_frenado_pct;      /* idem el frenado (0 si antes era 0) */
    int cambio_grande;           /* 1 si el ms por grado cambió más de CG_CAMBIO_AVISO_PCT % */
    int error_max_x10;           /* lo que más se aleja un giro de la recta ajustada, en décimas de grado */
    int duraciones_ms[CG_GIROS_POR_LADO];       /* los tiempos mandados */
    int grados_x10[CG_GIROS_POR_LADO];          /* lo que giró cada uno según el giroscopio */
} CalGiroLado;

typedef struct {
    int estado;
    int error;
    int codigo_ajuste;           /* si error = CG_ERR_AJUSTE: el código CAL_ERR_* que dio cal_ajustar_giro */
    int giro_actual;             /* 0 a CG_TOTAL_GIROS - 1: el giro que se está haciendo (CG_TOTAL_GIROS al terminar) */
    int comando;                 /* el último comando devuelto */
    int tiene_angulo;            /* 1 desde que llegó la primera lectura del ángulo */
    CalibracionCarro base;       /* lo guardado (con su arranque) */
    CalibracionCarro nueva;      /* lo calculado (solo vale en CG_LISTO) */
    int plan_ms[CG_TOTAL_GIROS];
    int ms_real[2][CG_GIROS_POR_LADO];
    int grados_x10[2][CG_GIROS_POR_LADO];
    uint32_t t_inicio_ms, t_estado_ms, t_cmd_ms, t_ref_ms, t_angulo_ms;
    int ang_ref_x10, ang0_x10, ang_x10;
    CalGiroLado resumen[2];      /* [0] izquierda, [1] derecha */
} CalGiro;

/* Empieza la calibración. `guardada` es la calibración guardada (su arranque y sus ms por grado sirven para elegir cuánto dura cada giro). */
void calgiro_iniciar(CalGiro *cg, const CalibracionCarro *guardada, uint32_t ahora_ms);

/* Una vuelta (cada ~50 ms). `hay_angulo` = 1 si el ángulo es reciente (llegó un SENS en los últimos ~500 ms); `angulo_x10` = grados por 10 del giroscopio, POSITIVO a la izquierda. Devuelve CG_CMD_*. */
int  calgiro_tick(CalGiro *cg, uint32_t ahora_ms, int hay_angulo, int angulo_x10);

/* Cancela: el siguiente tick devuelve STOP. */
void calgiro_cancelar(CalGiro *cg);

/* Solo en CG_LISTO: copia la calibración nueva y el resumen de los dos lados. Devuelve 1 si hay resultado, 0 si no. */
int  calgiro_resultado(const CalGiro *cg, CalibracionCarro *nueva, CalGiroLado resumen[2]);

/* Texto corto de un error, para mostrar en pantalla. */
const char *calgiro_texto_error(int error);

#ifdef __cplusplus
}
#endif

#endif /* CALGIRO_H */
