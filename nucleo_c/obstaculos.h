/*
 * obstaculos.h — Detector de obstáculos por color de piso.
 *
 * GUÍA DE LECTURA
 *  1. Idea: el carro ve el piso libre en la parte baja de la imagen. Todo lo que no se
 *     parezca al color del piso, y esté en la zona de avance, es un obstáculo. Sirve para
 *     objetos que un modelo tipo COCO no conoce (una caja, una pared).
 *  2. Entrada: una imagen RGB de 320x240 (3 bytes por píxel: R, G, B, fila por fila).
 *  3. La imagen se divide en celdas de 10x10 píxeles (32 x 24 celdas).
 *  4. El color del piso se toma de una franja de celdas abajo y al centro (se asume libre).
 *  5. Una celda es "no piso" si su color se aleja de ese color (ver UMBRAL_COLOR, etc.).
 *  6. Se mira la zona de avance (por defecto la mitad inferior) en tres zonas (izquierda, centro, derecha) y cada una
 *     dice si hay obstáculo y qué tan cerca está (0 a 3).
 *  Solo enteros, sin malloc, sin recursión; los errores son códigos de retorno.
 *
 *  Los valores de calibración (umbrales) son de partida: hay que ajustarlos con la
 *  cámara real del carro y el piso real (ver docs/obstaculos.md).
 */
#ifndef OBSTACULOS_H
#define OBSTACULOS_H

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

/* ── Geometría de la imagen y de la cuadrícula de celdas ───────────────── */

#define OBS_ANCHO        320
#define OBS_ALTO         240
#define OBS_LADO_CELDA    10
#define OBS_COLUMNAS     (OBS_ANCHO / OBS_LADO_CELDA)   /* 32 */
#define OBS_FILAS        (OBS_ALTO / OBS_LADO_CELDA)    /* 24 */

/* Franja de referencia del piso: 6 celdas de ancho x 2 de alto, abajo y centrada.
 * Estos son los valores POR DEFECTO; se pueden cambiar con ConfigObstaculos (más abajo). */
#define OBS_REF_COLUMNAS         6
#define OBS_REF_FILAS            2
#define OBS_REF_COLUMNA_INICIAL  ((OBS_COLUMNAS - OBS_REF_COLUMNAS) / 2)   /* 13 */
#define OBS_REF_FILA_INICIAL     (OBS_FILAS - OBS_REF_FILAS)               /* 22 */

/* Solo se buscan obstáculos en la zona de avance: de FILA_MINIMA hacia abajo. Las filas de
 * arriba (el fondo del cuarto, paredes lejanas) se ignoran. Valor POR DEFECTO: la mitad inferior
 * de la imagen (filas 12 a 23); se puede cambiar con ConfigObstaculos.fila_minima. */
#define OBS_FILA_INICIAL_ZONAS   (OBS_FILAS / 2)

/* ── Parámetros a calibrar ─────────────────────────────────────────────────
 * Los colores se comparan sobre el promedio de cada celda (cada canal de 0 a 255).
 *
 * Una celda es PISO si se cumple una de estas dos condiciones:
 *   A) su color es casi igual al de referencia: la suma de las diferencias absolutas
 *      de los tres canales (R, G, B) es menor o igual que UMBRAL_COLOR; o
 *   B) tiene el mismo "tono" pero distinta luz (una zona más clara por una lámpara o
 *      más oscura por la distancia): se compara el color NORMALIZADO por brillo
 *      (cada canal dividido entre la suma R+G+B, así la luz casi no cuenta) y es
 *      menor o igual que UMBRAL_CROMA, y además el brillo (R+G+B) no difiere más de
 *      UMBRAL_BRILLO. Esto tolera degradados de luz, pero no una caja muy oscura.
 * Si no cumple ninguna, la celda es NO PISO.
 */
#define UMBRAL_COLOR      60   /* suma |dR|+|dG|+|dB|, de 0 a 765 (valor POR DEFECTO, ver ConfigObstaculos) */
#define UMBRAL_CROMA      30   /* suma de diferencias del color normalizado, de 0 a 512 */
#define UMBRAL_BRILLO    150   /* |(R+G+B) - (R+G+B de referencia)|, de 0 a 765 */
#define SUMA_MIN_CROMA    90   /* debajo de este R+G+B el color normalizado no es confiable */

/* La franja de referencia debe ser uniforme: si alguna de sus celdas se aleja del
 * promedio de la franja más de MAX_VARIACION_REF (misma suma de canales), probablemente
 * hay un objeto encima y se devuelve OBS_ERR_REFERENCIA. */
#define MAX_VARIACION_REF  40

/* Una zona tiene obstáculo si hay al menos MIN_CELDAS celdas "no piso" confirmadas.
 * Una celda "no piso" aislada (sin ninguna vecina "no piso") se descarta como ruido. */
#define MIN_CELDAS          6   /* valor POR DEFECTO, ver ConfigObstaculos */

/* Cercanía según la fila más baja (la más cercana al carro) que alcanza el obstáculo:
 *   fila >= OBS_FILA_CERCA  → 3 (muy cerca)
 *   fila >= OBS_FILA_MEDIA  → 2 (media distancia)
 *   en otro caso            → 1 (lejos)
 * y 0 si la zona no tiene obstáculo. */
#define OBS_FILA_MEDIA     16
#define OBS_FILA_CERCA     20

/* ── Resultado ─────────────────────────────────────────────────────────── */

#define ZONA_IZQUIERDA  0
#define ZONA_CENTRO     1
#define ZONA_DERECHA    2
#define OBS_ZONAS       3

typedef struct {
    int hay_obstaculo;   /* 1 si la zona tiene obstáculo, 0 si no */
    int cercania;        /* 0 = sin obstáculo, 1 = lejos, 2 = media, 3 = muy cerca */
    int celdas;          /* cuántas celdas "no piso" confirmadas hay en la zona */
} ZonaObstaculo;

typedef struct {
    ZonaObstaculo zona[OBS_ZONAS];
    int ref_r, ref_g, ref_b;   /* color de referencia del piso (promedio de la franja) */
    /* 1 en cada celda "no piso" confirmada (toda la imagen); sirve para depurar y dibujar */
    unsigned char no_piso[OBS_FILAS][OBS_COLUMNAS];
} ResultadoObstaculos;

/* ── Configuración ajustable ───────────────────────────────────────────────
 * Los parámetros que más se ajustan al probar con la cámara y el piso reales. Los demás
 * (UMBRAL_CROMA, UMBRAL_BRILLO...) siguen siendo constantes. La app los cambia en vivo. */
typedef struct {
    int umbral_color;           /* ver UMBRAL_COLOR; de 0 a 765 */
    int min_celdas;             /* ver MIN_CELDAS; al menos 1 */
    int ref_columnas;           /* ancho de la franja de referencia, en celdas */
    int ref_filas;              /* alto de la franja de referencia, en celdas */
    int ref_columna_inicial;    /* primera columna de la franja (0 a 31) */
    int ref_fila_inicial;       /* primera fila de la franja (0 a 23) */
    int fila_minima;            /* primera fila (contando desde arriba, 0 a 23) que es zona de avance;
                                   las filas de arriba se ignoran. Por defecto 12 (mitad inferior) */
} ConfigObstaculos;

/* Códigos de retorno de obstaculos_analizar. */
#define OBS_OK                   0
#define OBS_ERR_IMAGEN_NULA     -1   /* la imagen o el resultado son NULL */
#define OBS_ERR_TAMANO          -2   /* la imagen no es de 320x240 */
#define OBS_ERR_REFERENCIA      -3   /* la franja de referencia no es uniforme (¿hay un objeto?) */
#define OBS_ERR_CONFIG          -4   /* la configuración no es válida (franja fuera de la imagen, etc.) */

/* ── Funciones ─────────────────────────────────────────────────────────── */

/* Llena la configuración con los valores por defecto (los de las constantes de arriba). */
void obstaculos_config_defecto(ConfigObstaculos *config);

/* Analiza la imagen RGB (ancho x alto x 3 bytes; debe ser 320x240) con la configuración dada
 * y llena el resultado. Devuelve OBS_OK o un código de error; con un error el resultado
 * queda en ceros. */
int obstaculos_analizar_con(const ConfigObstaculos *config, const unsigned char *rgb,
                            int ancho, int alto, ResultadoObstaculos *resultado);

/* Igual que obstaculos_analizar_con pero con la configuración por defecto. */
int obstaculos_analizar(const unsigned char *rgb, int ancho, int alto, ResultadoObstaculos *resultado);

/* Texto corto en español para un código de retorno. */
const char *obstaculos_texto_error(int codigo);

#ifdef __cplusplus
}
#endif

#endif /* OBSTACULOS_H */
