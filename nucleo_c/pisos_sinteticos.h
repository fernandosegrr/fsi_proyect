/*
 * pisos_sinteticos.h — Cuadros sintéticos de pisos difíciles para probar el detector de obstáculos en PC (solo para pruebas).
 *
 * GUÍA DE LECTURA
 *  1. Genera cuadros RGB de 240x176 (el tamaño de la cámara del carro) con un piso de un tipo dado y le agrega, según se pida:
 *       - ruido por píxel (sigma en niveles de 0 a 255),
 *       - viñeteo (las esquinas más oscuras que el centro) y degradado de luz de arriba a abajo,
 *       - cambio de exposición automática (el brillo de todo el cuadro se multiplica por un factor).
 *  2. Pisos: NEGRO (casi negro, un poco azulado), LISO (gris cálido), BALDOSAS (damero de dos colores con baldosas de 40 px),
 *     FRANJAS (franjas verticales de 30 px de dos colores) y DAMERO (cuadros pequeños de 20 px). En los pisos de dos colores el borde entre
 *     baldosas lleva el color MEZCLADO (transición de 4 px), como en una foto real.
 *  3. Objetos: mano, libreta, caja y mochila, con la forma (tamaño en píxeles) y el color de cada uno, sobre el piso; y una sombra suave.
 *  4. Todo con un generador de números al azar propio (repetible) y solo enteros.
 *  Nada de esto se usa en la app: es solo para pruebas de PC.
 */
#ifndef PISOS_SINTETICOS_H
#define PISOS_SINTETICOS_H

#define PS_ANCHO 240
#define PS_ALTO  176

/* Tipos de piso. */
#define PS_NEGRO     0
#define PS_LISO      1
#define PS_BALDOSAS  2
#define PS_FRANJAS   3
#define PS_DAMERO    4

/* Objetos. */
#define PS_NADA      0
#define PS_MANO      1
#define PS_LIBRETA   2
#define PS_CAJA      3
#define PS_MOCHILA   4

typedef struct {
    int piso;             /* PS_NEGRO... */
    int sigma;            /* ruido por píxel, en niveles (0 a 20) */
    int vineteo_pct;      /* cuánto se oscurece la esquina respecto al centro, en por ciento (0 a 60) */
    int degradado_pct;    /* cuánto más brillante es la fila de arriba que la de abajo, en por ciento (negativo = más oscura) */
    int exposicion_pct;   /* factor de brillo de TODO el cuadro en por ciento (100 = igual) */
    int objeto;           /* PS_NADA... */
    int obj_x, obj_y;     /* esquina de arriba a la izquierda del objeto, en píxeles */
    int sombra;           /* 1 = una sombra suave en el centro (el piso 30 % más oscuro con bordes suaves) */
    int baldosa_px;       /* lado de la baldosa de PS_BALDOSAS en píxeles (0 = 40)  */
    int desfase_x;        /* corre el dibujo del piso hacia los lados (en píxeles): decide qué baldosa queda justo abajo al centro */
    int reflejo;          /* niveles que SUMA un reflejo de luz (mancha brillante) abajo al centro, justo donde se mira la referencia del piso */
    int croma_bloques;    /* error de color por bloque de 16x16 (como el JPEG): +-niveles distintos en cada canal por bloque */
    unsigned long semilla; /* para el ruido */
} EscenaSintetica;

/* Llena `rgb` (PS_ANCHO x PS_ALTO x 3 bytes) con la escena. */
void ps_dibujar(const EscenaSintetica *escena, unsigned char *rgb);

/* Texto corto del tipo de piso. */
const char *ps_nombre_piso(int piso);
const char *ps_nombre_objeto(int objeto);

#endif /* PISOS_SINTETICOS_H */
