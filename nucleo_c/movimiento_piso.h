/*
 * movimiento_piso.h — El FLUJO DEL PISO: cuánto se mueve cada bloque del piso entre dos cuadros seguidos, comparado con lo que se
 * DEBERÍA mover si fuera piso plano.
 *
 * GUÍA DE LECTURA
 *  1. Para qué: el detector de color se equivoca con pisos de dos colores, sombras y reflejos (ve "no piso" donde solo hay otra baldosa).
 *     Pero cuando el carro avanza, el piso de verdad se mueve en la imagen de una forma que se puede CALCULAR con la geometría de la
 *     cámara, y una cosa que está parada sobre el piso (una caja, una mochila) se mueve MÁS (porque está más cerca de la cámara que el piso
 *     que tapa). El flujo mira cuánto se movió cada bloque y lo compara con lo esperado.
 *  2. Lo esperado: la cámara está a `altura` del piso, inclinada hacia abajo, con campo de visión FOV (los mismos números del corredor,
 *     corredor.h). Un punto del piso que está a d cm hacia adelante pasa a estar a d - s cm cuando el carro avanza s cm (s = velocidad del
 *     perfil AUTO por los ms entre los dos cuadros). Su fila en la imagen sube/baja a la fila que corresponde a d - s, y su columna se abre
 *     hacia afuera en la misma proporción en que crece A(v) (ver corredor.h). Eso da el desplazamiento esperado (dx, dy) de cada fila.
 *  3. Lo medido: el cuadro anterior se parte en bloques de 16x16 píxeles; para cada bloque se busca, en una ventana alrededor del
 *     desplazamiento esperado, dónde quedó en el cuadro nuevo (la suma de diferencias absolutas más chica, SAD, mirando un píxel de cada dos).
 *  4. Cada bloque queda en uno de estos estados:
 *       FLU_SIN_DATO  no tiene textura (piso liso o negro), o la mejor coincidencia no es mucho mejor que las demás (borde recto, dibujo que
 *                     se repite, ruido): NUNCA se toma como obstáculo ni como piso;
 *       FLU_PISO      tiene textura y se movió como el piso (la diferencia con lo esperado cabe en la tolerancia);
 *       FLU_CERCA     tiene textura y se movió claramente MÁS que el piso: está más cerca que el piso: candidato a obstáculo;
 *       FLU_DUDOSO    tiene textura pero no coincide con nada razonable (se movió de menos, se tapó, cambió la luz).
 *  5. Solo se evalúa si el carro AVANZA y los cuadros llegaron a tiempo (entre dt_min_ms y dt_max_ms): si no, `evaluado` = 0 y el motivo
 *     dice por qué. Las filas de arriba (el piso lejano casi no se mueve) y las de abajo (el bloque se saldría de la imagen) no se evalúan.
 *  6. Por zona (izquierda, centro, derecha como el detector) se cuentan bloques; una zona es "PISO CONSISTENTE" si tiene al menos
 *     `min_bloques` bloques con dato y TODOS los que tienen dato son piso (ninguno cerca). La FUSIÓN (fusion.h) usa solo eso: si el color
 *     marca obstáculo lejano en una zona de piso consistente, baja la severidad. El flujo NUNCA frena por sí solo (a lo sumo avisa).
 *  7. APAGADO por defecto (ConfigFlujo.activo = 0). Solo enteros, sin malloc ni recursión; los errores son códigos de retorno.
 */
#ifndef MOVIMIENTO_PISO_H
#define MOVIMIENTO_PISO_H

#include "corredor.h"

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

#define FLU_BLOQUE          16   /* lado del bloque, en píxeles */
#define FLU_MAX_COLUMNAS    40   /* 640 / 16 */
#define FLU_MAX_FILAS       30   /* 480 / 16 */

/* Estados de un bloque. */
#define FLU_SIN_DATO   0
#define FLU_PISO       1
#define FLU_CERCA      2
#define FLU_DUDOSO     3

/* Por qué no se evaluó el cuadro (ResultadoFlujo.motivo). */
#define FLU_MOT_EVALUADO      0
#define FLU_MOT_APAGADO       1   /* config.activo = 0 */
#define FLU_MOT_QUIETO        2   /* el carro no avanza (velocidad 0 o no va hacia adelante) */
#define FLU_MOT_TIEMPO        3   /* el tiempo entre cuadros es muy corto o muy largo */

typedef struct {
    int activo;               /* 0 = apagado (por defecto) */
    int velocidad_mm_s;       /* velocidad hacia adelante del perfil AUTO, en mm por segundo (193 = 19.3 cm/s medidos con PWM 210) */
    int ventana_px;           /* cuántos píxeles MÁS que lo esperado se busca (hacia abajo); 8 */
    int ruido_gradiente;      /* diferencias entre vecinos menores que esto se toman por ruido y no cuentan como textura (4 niveles) */
    int textura_min;          /* lo que pasa del ruido, en promedio por píxel, que debe tener un bloque para tener "textura" (2) */
    int ajuste_max;           /* diferencia media por píxel de la mejor coincidencia; más que esto = DUDOSO (14) */
    int margen_unico;         /* cuánto mejor (por píxel) debe ser la mejor coincidencia que la mejor de las lejanas; menos = SIN_DATO (3) */
    int tolerancia_px4;       /* tolerancia fija entre lo medido y lo esperado, en CUARTOS de píxel (6 = 1.5 px) */
    int tolerancia_pct;       /* tolerancia extra como porcentaje de lo esperado (20) */
    int dt_min_ms;            /* tiempo mínimo entre cuadros (40) */
    int dt_max_ms;            /* tiempo máximo entre cuadros (300) */
    int distancia_max_cm;     /* las filas del piso más lejos que esto no se evalúan (60) */
    int min_bloques;          /* bloques con dato mínimos en una zona para decir "piso consistente" (3) */
} ConfigFlujo;

/* Lo que se supo de una zona. */
typedef struct {
    int piso;                 /* bloques FLU_PISO */
    int cerca;                /* bloques FLU_CERCA */
    int dudoso;               /* bloques FLU_DUDOSO */
    int sin_dato;             /* bloques FLU_SIN_DATO (solo de las filas evaluadas) */
    int piso_consistente;     /* 1 si piso + cerca >= min_bloques y cerca = 0 y dudoso no pasa de la mitad de lo que tiene dato */
    int hay_cerca;            /* 1 si hay al menos un bloque FLU_CERCA */
    int cercania;             /* 1 a 3 según la fila más baja de los bloques FLU_CERCA (como el detector); 0 si no hay */
} ZonaFlujo;

typedef struct {
    int evaluado;             /* 1 si se calculó el flujo; 0 si no (ver motivo) */
    int motivo;               /* FLU_MOT_* */
    int columnas, filas;      /* tamaño de la cuadrícula de bloques */
    int fila_inicial, fila_final;   /* filas de bloques evaluadas: de fila_inicial a fila_final - 1 */
    int avance_mm;            /* cuánto avanzó el carro entre los dos cuadros según velocidad y tiempo */
    ZonaFlujo zona[OBS_ZONAS];
    int total_piso, total_cerca, total_dudoso, total_sin_dato;
    unsigned char estado[FLU_MAX_FILAS][FLU_MAX_COLUMNAS];
    int medido_dy4[FLU_MAX_FILAS][FLU_MAX_COLUMNAS];     /* desplazamiento vertical medido, en cuartos de píxel (solo diagnóstico) */
    int esperado_dy4[FLU_MAX_FILAS][FLU_MAX_COLUMNAS];   /* el esperado para ese bloque, en cuartos de píxel (solo diagnóstico) */
} ResultadoFlujo;

/* Códigos de retorno. */
#define FLU_OK                 0
#define FLU_ERR_ARGUMENTO     -1   /* puntero nulo o tamaño de cuadro no válido (de 48x48 a 640x480) */
#define FLU_ERR_CONFIG        -2   /* configuración no válida */

/* Los valores por defecto de arriba, con el flujo APAGADO. */
void flujo_config_defecto(ConfigFlujo *config);

/* 1 si la configuración sirve. */
int flujo_config_es_valida(const ConfigFlujo *config);

/* Convierte un cuadro RGB (ancho x alto x 3) en grises (ancho x alto), como lo espera flujo_analizar. */
void flujo_gris_de_rgb(const unsigned char *rgb, int ancho, int alto, unsigned char *gris);

/* El desplazamiento ESPERADO de un punto del piso que está en (u, v) cuando el carro avanza `avance_mm` mm. Deja en *dy4 el desplazamiento vertical y en
 * *dx4 el horizontal, en CUARTOS de píxel (positivo = hacia abajo / hacia la derecha). Devuelve 1 si el punto es del piso y sigue dentro de la imagen
 * después de avanzar, 0 si no (sobre el horizonte, o ya pasó por debajo del carro). */
int flujo_esperado(const Corredor *corredor, int u, int v, int avance_mm, int *dx4, int *dy4);

/* Compara el cuadro anterior con el actual (los dos en grises de ancho x alto) y llena el resultado. `avanzando` = 1 si el carro iba hacia adelante entre los
 * dos cuadros; `dt_ms` = tiempo entre los dos cuadros. Devuelve FLU_OK o un error; si el flujo no se evaluó por el motivo que sea el retorno es FLU_OK y
 * resultado->evaluado = 0. */
int flujo_analizar(const ConfigFlujo *config, const Corredor *corredor, const unsigned char *gris_anterior, const unsigned char *gris_actual,
                   int ancho, int alto, int avanzando, int dt_ms, ResultadoFlujo *resultado);

/* Convierte los bloques FLU_CERCA en cajas de píxeles (peso 1, fuente COR_FUENTE_FLUJO) para el corredor. Devuelve cuántas puso (hasta `maximo`). */
int flujo_cajas(const ResultadoFlujo *resultado, CajaImagen *cajas, int maximo);

#ifdef __cplusplus
}
#endif

#endif /* MOVIMIENTO_PISO_H */
