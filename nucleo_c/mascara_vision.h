/*
 * mascara_vision.h — «Calibrar visión»: la MÁSCARA de artefactos fijos de la imagen (J9).
 *
 * GUÍA DE LECTURA
 *  1. Para qué: en la prueba real del 16 oct la zona izquierda marcaba obstáculo desde el primer cuadro (cercanía 3, 25 a 57 celdas) con el piso despejado, y el carro
 *     frenaba y esquivaba sin motivo. Si es un artefacto de la imagen (viñeteo, una parte del carro) se marca igual en CUALQUIER lugar; si es algo del lugar, solo en ese lugar.
 *  2. Cómo: el usuario pone el carro en piso despejado en MASC_LUGARES lugares distintos y en cada uno pulsa «Capturar»: se toman unos 20 cuadros y se cuenta, por celda
 *     gruesa de 10 x 10 (la imagen entera dividida en 10 filas y 10 columnas), en cuántos cuadros estuvo marcada como «no piso» (una celda gruesa cuenta como marcada en un
 *     cuadro si MASC_CELDA_PCT % o más de las celdas del detector que la forman lo están, mirando solo las filas de la zona de avance).
 *  3. Artefacto = celda marcada en MASC_FRECUENCIA_PCT % o más de los cuadros EN LOS TRES lugares. Eso es lo que se enmascara (el detector la trata como piso).
 *  4. Límites (si se pasan, «visión sospechosa»): a lo más MASC_MAX_PCT % de las celdas (20 de 100) y NUNCA las MASC_CERCANAS_FILAS filas más cercanas de las
 *     MASC_CENTRALES columnas centrales (ahí está lo que el carro va a chocar): esas celdas no se enmascaran aunque sean artefacto.
 *  5. Una zona que marca solo en algún lugar (no en los tres) se reporta como «depende del lugar», con sus celdas y frecuencias, y NO se enmascara.
 *  6. La máscara se guarda como texto hexadecimal (25 cifras = 100 bits) y se muestra como cuadrícula de números: nunca como imagen.
 *  Solo enteros, sin malloc, sin recursión; los errores son códigos de retorno.
 */
#ifndef MASCARA_VISION_H
#define MASCARA_VISION_H

#include "obstaculos.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MASC_LUGARES              3    /* lugares distintos que hay que capturar */
#define MASC_CUADROS_POR_LUGAR   20    /* cuadros que se toman en cada lugar (la app) */
#define MASC_MIN_CUADROS         10    /* con menos cuadros en un lugar no se puede calcular */
#define MASC_FRECUENCIA_PCT      80    /* celda marcada en este % o más de los cuadros de un lugar */
#define MASC_CELDA_PCT           50    /* una celda gruesa está marcada en un cuadro si este % o más de sus celdas finas (de la zona de avance) lo están */
#define MASC_MAX_PCT             20    /* máximo de celdas enmascaradas, en % de las 100 */
#define MASC_CERCANAS_FILAS       2    /* las filas más cercanas (abajo)... */
#define MASC_CENTRALES            2    /* ...de las columnas centrales nunca se enmascaran */
#define MASC_MAX_DEPENDE         20    /* cuántas celdas «depende del lugar» se listan */
#define MASC_HEX_LARGO           25    /* cifras hexadecimales de la máscara (sin el '\0') */

/* La máscara: 1 = celda gruesa enmascarada. */
typedef struct {
    unsigned char celda[OBS_MASC_FILAS][OBS_MASC_COLUMNAS];
} MascaraVision;

/* Lo que se cuenta en UN lugar. */
typedef struct {
    int cuadros;                                      /* cuadros sumados */
    int marcas[OBS_MASC_FILAS][OBS_MASC_COLUMNAS];    /* en cuántos de ellos la celda gruesa estuvo marcada */
} LugarMascara;

/* Por qué la visión es sospechosa (bits de InformeMascara.sospechosa). */
#define MASC_SOSP_DEMASIADAS   1   /* más de MASC_MAX_PCT % de las celdas serían artefacto: no se enmascara nada */
#define MASC_SOSP_CENTRO       2   /* una celda de las filas más cercanas de las columnas centrales sería artefacto: esa no se enmascara */

typedef struct {
    int artefactos;       /* celdas que cumplen la regla (antes de los límites) */
    int enmascaradas;     /* celdas que quedaron en la máscara */
    int sospechosa;       /* bits MASC_SOSP_*; 0 = bien */
    int frecuencia_min[OBS_MASC_FILAS][OBS_MASC_COLUMNAS];   /* % de cuadros marcada, el MENOR de los tres lugares (para la cuadrícula de números) */
    /* Celdas que marcan (MASC_FRECUENCIA_PCT % o más) en algún lugar pero no en los tres: «depende del lugar». No se enmascaran. */
    int depende_n;
    int depende_fila[MASC_MAX_DEPENDE], depende_columna[MASC_MAX_DEPENDE];
    int depende_pct[MASC_MAX_DEPENDE][MASC_LUGARES];
    /* Por zona del navegador (izquierda, centro, derecha: tercios de columnas) y por lugar: cuántas celdas marcan (MASC_FRECUENCIA_PCT % o más). */
    int zona_celdas[OBS_ZONAS][MASC_LUGARES];
    int zona_depende[OBS_ZONAS];   /* 1 si la zona marca en algún lugar pero no en todos */
} InformeMascara;

/* Códigos de retorno. */
#define MASC_OK              0
#define MASC_ERR_ARGUMENTO  -1
#define MASC_ERR_CUADROS    -2   /* algún lugar tiene menos de MASC_MIN_CUADROS cuadros */
#define MASC_ERR_TEXTO      -3   /* el texto hexadecimal no es válido */

/* Deja el lugar vacío. */
void masc_lugar_iniciar(LugarMascara *lugar);

/* Suma un cuadro (el resultado del detector SIN máscara) al lugar. Cada celda gruesa se marca si MASC_CELDA_PCT % o más de las celdas finas de la zona de avance que la forman
 * son «no piso». Devuelve MASC_OK o MASC_ERR_ARGUMENTO (resultado nulo o sin cuadrícula). */
int masc_lugar_cuadro(LugarMascara *lugar, const ResultadoObstaculos *resultado);

/* Calcula la máscara con los lugares capturados (MASC_LUGARES). Deja la máscara y el informe. MASC_ERR_CUADROS si algún lugar tiene menos de MASC_MIN_CUADROS cuadros. */
int masc_calcular(const LugarMascara lugares[MASC_LUGARES], MascaraVision *mascara, InformeMascara *informe);

/* Pone la máscara en la configuración del detector (activa si hay al menos una celda). */
void masc_aplicar_a_config(const MascaraVision *mascara, ConfigObstaculos *config);

/* Texto hexadecimal (MASC_HEX_LARGO cifras + '\0'; `capacidad` al menos MASC_HEX_LARGO + 1) de la máscara, fila por fila: cada cifra son 4 celdas seguidas. */
int masc_a_hex(const MascaraVision *mascara, char *texto, int capacidad);

/* Lee el texto de masc_a_hex. MASC_ERR_TEXTO si no son exactamente MASC_HEX_LARGO cifras hexadecimales (la máscara no cambia). */
int masc_de_hex(const char *texto, MascaraVision *mascara);

/* La máscara de lo que el detector marcó en UN cuadro, de una sola zona (solo sus columnas), en el mismo formato hexadecimal; sirve para el registro de cada decisión de obstáculo.
 * `zona` de 0 a 2 (ZONA_*). Mira la zona de avance (desde fila_minima_usada). */
int masc_zona_a_hex(const ResultadoObstaculos *resultado, int zona, char *texto, int capacidad);

/* Cuántas celdas tiene la máscara. */
int masc_contar(const MascaraVision *mascara);

/* El informe como texto en español, sin imagen: la cuadrícula de números (% de cuadros marcada, el menor de los tres lugares, 10 filas de 10 números), la máscara en hexadecimal, las celdas
 * «depende del lugar» y el aviso de visión sospechosa. Devuelve el largo escrito (siempre termina en '\0'). */
int masc_texto_informe(const MascaraVision *mascara, const InformeMascara *informe, char *texto, int capacidad);

#ifdef __cplusplus
}
#endif

#endif /* MASCARA_VISION_H */
