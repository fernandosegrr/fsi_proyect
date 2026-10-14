/*
 * navegador.h — El "cerebro" del modo semiautónomo: sigue una ruta y esquiva obstáculos.
 *
 * GUÍA DE LECTURA
 *  1. Une dos módulos: trayectoria.c (de una ruta de celdas saca pasos AVANZAR/GIRAR con
 *     tiempos) y obstaculos.c (dice si hay algo adelante). El resultado es UN comando.
 *  2. Se usa así: navegador_iniciar() → navegador_cargar_ruta() → navegador_comenzar() y
 *     después se llama navegador_tick() cada ~50 ms; devuelve el comando que el carro debe
 *     MANTENER (los mismos 5 del protocolo real, sin parámetros).
 *  3. Estados (ver el diagrama en docs/navegador.md):
 *       ESPERANDO → SIGUIENDO → DETENIDO_OBSTACULO → ESQUIVANDO → REINCORPORANDO → SIGUIENDO
 *       ... → TERMINADO (llegó) o ERROR (no pudo seguir).
 *  4. El navegador NO tiene sensores de posición: lleva la cuenta por TIEMPO (cuánto tiempo
 *     estuvo avanzando o girando), con la calibración del carro (calibracion.h): por defecto la
 *     de ejemplo, o la medida con navegador_poner_calibracion(). Es una estimación.
 *  5. Solo enteros, sin malloc, sin recursión; los errores son códigos de retorno.
 *  6. Regla de seguridad: en los estados ESPERANDO, TERMINADO y ERROR el comando es SIEMPRE STOP.
 */
#ifndef NAVEGADOR_H
#define NAVEGADOR_H

#include <stdint.h>
#include "trayectoria.h"
#include "obstaculos.h"
#include "calibracion.h"

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

/* ── Parámetros a calibrar ─────────────────────────────────────────────── */

#define NAV_TICK_MS                50    /* cada cuánto se llama navegador_tick (nominal) */
#define NAV_DT_MAX_MS            1000    /* tope del tiempo entre ticks (el firmware frena a los 1000 ms) */

/* Antirrebote EN MILISEGUNDOS (no en cuadros): así el retraso es el mismo con 8, 12 o 30 cuadros
 * por segundo. Un obstáculo en CUALQUIER zona frena el carro si su cercanía (0 a 3) es al menos
 * NAV_CERCANIA_MIN_DETENER (2): un obstáculo realmente lejano (cercanía 1, por ejemplo una escalera al fondo)
 * solo se informa, no frena. Después hay dos casos:
 *   - INMEDIATO (con UN solo cuadro): cercanía 3 en cualquier zona, o cercanía 2 en la zona central
 *     (el obstáculo ya está cerca y de frente: no hay tiempo que perder).
 *   - A CONFIRMAR (cercanía 2 en una zona de los lados): hay que verlo MS_CONFIRMAR_LEJOS milisegundos
 *     seguidos (y en al menos 2 cuadros) antes de detenerse; así un cuadro de ruido no frena el carro.
 * Para volver a avanzar, el frente debe verse libre MS_CONFIRMAR_LIBRE milisegundos seguidos. */
#define MS_CONFIRMAR_LEJOS         150
#define MS_CONFIRMAR_LIBRE         250
#define NAV_CERCANIA_MIN_DETENER     2   /* desde esta cercanía (en cualquier zona) se frena; 1 = lejos, solo se informa */
#define NAV_CERCANIA_INMEDIATA       2   /* en el centro: desde esta cercanía se confirma con un cuadro */

/* Giro medido con la cámara (ver navegador_aporte_visual): el giro por tiempo se corrige con lo que la cámara ve girar.
 *   - NAV_COBERTURA_VISUAL_PCT: la cámara debe haber medido al menos este % del tiempo de giro (y NAV_MIN_MS_VISUAL ms)
 *     para fiarse de ella; si no, el giro sigue por tiempo.
 *   - NAV_GIRO_MIN_PCT: aunque la cámara diga que ya se llegó al ángulo, no se corta antes de este % del tiempo planeado
 *     (por si la cámara se equivoca).
 *   - NAV_GIRO_MAX_PCT: tiempo de RESPALDO: aunque la cámara diga que falta, el giro se corta a este % del tiempo planeado. */
#define NAV_COBERTURA_VISUAL_PCT   50
#define NAV_MIN_MS_VISUAL         200
#define NAV_GIRO_MIN_PCT           50
#define NAV_GIRO_MAX_PCT          150

#define NAV_PAUSA_MS              500    /* pausa con el carro detenido antes de decidir qué hacer */
#define NAV_PAUSA_ASENTAR_MS      300    /* STOP entre avanzar y girar (y al revés): el carro termina de moverse y el giro/avance siguiente
                                          * arranca desde cero, como en la calibración. Sin esto, el giro dependía de la inercia del avance. */
#define NAV_TIMEOUT_ESQUIVAR_MS  4000    /* si en este tiempo no se puede esquivar: ERROR */
#define NAV_ANGULO_ESQUIVAR        90    /* grados que gira para apartarse (múltiplo de 45) */
#define NAV_TRAMO_ESQUIVAR_CM      40    /* cuánto avanza de lado, en cm */
#define NAV_MAX_ESQUIVADAS          3    /* máximo de esquivadas en una misma ruta */

/* ── Tipos ─────────────────────────────────────────────────────────────── */

typedef enum {
    NAV_ESPERANDO,
    NAV_SIGUIENDO,
    NAV_DETENIDO_OBSTACULO,
    NAV_ESQUIVANDO,
    NAV_REINCORPORANDO,
    NAV_TERMINADO,
    NAV_ERROR
} EstadoNav;

/* Los 5 comandos del protocolo real del carro (texto sin parámetros). */
typedef enum {
    NAV_CMD_STOP,
    NAV_CMD_FORWARD,
    NAV_CMD_REVERSE,
    NAV_CMD_LEFT,
    NAV_CMD_RIGHT
} ComandoNav;

/* Por qué terminó en ERROR. */
typedef enum {
    NAV_MOTIVO_NINGUNO,
    NAV_MOTIVO_LADOS_BLOQUEADOS,   /* obstáculo al frente y ningún lado libre tras el tiempo máximo */
    NAV_MOTIVO_MAX_ESQUIVADAS,     /* ya esquivó NAV_MAX_ESQUIVADAS veces y vuelve a haber obstáculo */
    NAV_MOTIVO_ESQUIVANDO,         /* apareció un obstáculo muy cerca mientras esquivaba */
    NAV_MOTIVO_PLAN                /* no se pudo calcular el camino de regreso a la ruta */
} MotivoErrorNav;

/* Códigos de retorno de las funciones de configuración. */
#define NAV_OK                 0
#define NAV_ERR_ARGUMENTO     -1   /* puntero nulo o ruta vacía */
#define NAV_ERR_RUTA          -2   /* la ruta no es válida (ver codigo_trayectoria) */
#define NAV_ERR_ESTADO        -3   /* esa función no se puede usar en el estado actual */

typedef struct {
    /* Calibración del carro (arranque, ms por cm, frenado, ms por grado de cada lado). */
    CalibracionCarro cal;

    /* Ruta original (celdas) y camino actual (pasos por tiempo). */
    Punto ruta[MAX_PUNTOS];
    int   n_puntos;
    int   indice_objetivo;       /* punto de la ruta al que se dirige (1 = el segundo) */
    Paso  pasos[MAX_PASOS];
    int   n_pasos;
    int   indice_paso;
    int   ms_paso;               /* tiempo ya ejecutado del paso actual */

    /* Estado general. */
    EstadoNav      estado;
    MotivoErrorNav motivo_error;
    int            codigo_trayectoria;   /* último código de trayectoria.c (si hubo error de ruta) */
    ComandoNav     cmd_actual;           /* el comando que se devolvió en el último tick */
    uint32_t       t_ultimo_ms;          /* hora del último tick */
    uint32_t       t_detenido_ms;        /* hora en que se detuvo por obstáculo */
    int            asentando;            /* 1 mientras espera detenido entre dos movimientos distintos */
    uint32_t       asentar_hasta_ms;     /* hora en que termina esa espera */

    /* Posición estimada: milímetros (x hacia la derecha, y hacia abajo) y rumbo en grados
     * (0 = arriba, positivo = hacia la derecha, 0 a 359). */
    int x_mm, y_mm, rumbo_grados;
    int base_x_mm, base_y_mm, base_rumbo_grados;   /* pose al empezar el comando actual */
    int seg_ms;                                    /* tiempo bajo el comando actual */
    int giro_visual_x10;         /* grados por 10 que la cámara vio girar, en la dirección del comando de giro actual */
    int visual_cubierto_ms;      /* cuánto tiempo del giro actual cubren los cuadros confiables de la cámara */
    int latencia_visual_ms;      /* retraso de la cámara: un cuadro que llega ahora muestra lo que pasó hace esto (0 = sin retraso) */

    /* Lo último que se vio del obstáculo. */
    int zona_hay[OBS_ZONAS];
    int zona_cercania[OBS_ZONAS];
    int frames_bloqueado;        /* cuadros seguidos con obstáculo al frente */
    int frames_libre;            /* cuadros seguidos con el frente libre */
    int frames_recibidos;        /* cuadros desde la última detención */
    uint32_t t_bloqueo_ms;       /* hora del primer cuadro de la racha con obstáculo al frente */
    uint32_t t_libre_ms;         /* hora del primer cuadro de la racha con el frente libre */
    int bloqueo_inmediato;       /* 1 si en la racha hubo un cuadro de confirmación inmediata */

    /* Esquivar. */
    int esquivadas;
    int lado;                    /* -1 = izquierda, +1 = derecha */
    int fase;                    /* 0 = girar para apartarse, 1 = avanzar de lado, 2 = girar de vuelta */
    int ms_fase;
} Navegador;

/* ── Funciones ─────────────────────────────────────────────────────────── */

/* Deja el navegador en ESPERANDO, sin ruta. */
void navegador_iniciar(Navegador *nav);

/* Cambia la calibración del carro (por defecto se usa la de ejemplo de calibracion.h). Solo con el
 * navegador parado (ESPERANDO, TERMINADO o ERROR). Devuelve NAV_OK, NAV_ERR_ARGUMENTO (nula o no
 * válida) o NAV_ERR_ESTADO. Los pasos de una ruta ya cargada se calculan al comenzar, así que se
 * puede cambiar justo antes de navegador_comenzar. */
int navegador_poner_calibracion(Navegador *nav, const CalibracionCarro *cal);

/* Carga la ruta (lista de celdas contiguas, ver trayectoria.h). El carro se supone en el
 * primer punto mirando hacia ORIENTACION_INICIAL. Devuelve NAV_OK o un código de error. */
int navegador_cargar_ruta(Navegador *nav, const Punto *puntos, int n_puntos);

/* Empieza a seguir la ruta (o la retoma desde la posición estimada si se canceló antes).
 * Solo desde ESPERANDO. Una ruta de un solo punto termina de inmediato. */
int navegador_comenzar(Navegador *nav, uint32_t ahora_ms);

/* Cancela: STOP inmediato y vuelve a ESPERANDO (el siguiente tick devuelve STOP). */
void navegador_cancelar(Navegador *nav);

/* Una vuelta del cerebro. `ahora_ms` es un reloj en milisegundos (puede dar la vuelta a 32 bits).
 * `obs` es el resultado del último cuadro de obstaculos_analizar, o NULL si no hay cuadro
 * nuevo (entonces no cambia nada de lo que se sabe del obstáculo). Devuelve el comando
 * que el carro debe mantener hasta el siguiente tick. */
ComandoNav navegador_tick(Navegador *nav, uint32_t ahora_ms, const ResultadoObstaculos *obs);

/* Dice cuánto se atrasa la cámara (de 0 a 1000 ms): un cuadro que llega ahora muestra lo que pasó hace tanto. Con la cámara del
 * teléfono es ~0; con la cámara del carro por WiFi, ~300 ms. Así los cuadros que todavía muestran el carro sin girar no cuentan como
 * "cubiertos" y el pedazo de giro que la cámara aún no ve se completa por tiempo (el retraso queda compensado). Devuelve NAV_OK o
 * NAV_ERR_ARGUMENTO. */
int navegador_poner_latencia_visual(Navegador *nav, int ms);

/* Le avisa al navegador cuánto vio girar la cámara entre dos cuadros consecutivos (movimiento_visual.c): `giro_grados_x10` es el
 * giro del carro por 10 (POSITIVO a la derecha), `dt_ms` el tiempo entre esos dos cuadros y `confiable` 1 si la cámara estaba segura.
 * Solo cuenta mientras el carro está girando (LEFT o RIGHT) y después del retardo de arranque (los cuadros durante el arranque se
 * ignoran: el carro todavía no se mueve). El giro se corta cuando la cámara ve el ángulo pedido (con el tiempo como respaldo). */
void navegador_aporte_visual(Navegador *nav, int giro_grados_x10, int dt_ms, int confiable);

/* Texto del comando tal como lo entiende el firmware ("FORWARD", "STOP"...). */
const char *navegador_texto_comando(ComandoNav comando);

/* Texto del estado (para mostrar en pantalla o en los registros). */
const char *navegador_texto_estado(EstadoNav estado);

#ifdef __cplusplus
}
#endif

#endif /* NAVEGADOR_H */
