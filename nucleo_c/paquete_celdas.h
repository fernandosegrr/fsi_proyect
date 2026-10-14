/*
 * paquete_celdas.h — El paquete UDP del modo de video CELDAS: un cuadro resumido en el promedio RGB de cada celda de 10x10 píxeles.
 *
 * GUÍA DE LECTURA
 *  1. Quién lo usa: la CÁMARA del carro (camara_prueba.ino) lo arma con paquete_armar(); el TELÉFONO lo lee con paquete_leer() (por JNI); las pruebas de PC
 *     arman y leen el mismo formato. Es el mismo código en los tres sitios.
 *  2. Formato (todo en little-endian, el orden de la ESP32 y del teléfono):
 *         bytes  0 a  3   marca "CEL1" (el 1 es la versión del formato)
 *         bytes  4 a  7   número de cuadro (sube de uno en uno; si salta, se perdieron paquetes)
 *         bytes  8 a 11   ms desde que la cámara capturó el cuadro hasta que armó el paquete
 *         bytes 12 a 13   ancho de la imagen original, en píxeles
 *         bytes 14 a 15   alto de la imagen original, en píxeles
 *         byte  16        columnas de celdas
 *         byte  17        filas de celdas
 *         bytes 18 a 19   reservado (0)
 *         bytes 20 ...    columnas * filas * 3 bytes: R, G, B de cada celda, fila por fila
 *     Con HQVGA (240x176): 24 columnas x 17 filas = 1224 bytes de celdas + 20 de encabezado = 1244 bytes: cabe en un solo paquete UDP (el límite sin partir es 1472).
 *  3. Un paquete mal formado (marca distinta, largo que no cuadra, celdas en cero o de más) se rechaza con un código de error, nunca se lee más allá de su largo.
 *  Solo enteros, sin malloc.
 */
#ifndef PAQUETE_CELDAS_H
#define PAQUETE_CELDAS_H

#ifdef __cplusplus
extern "C" {
#endif

#define PAQ_ENCABEZADO        20
#define PAQ_MAX_COLUMNAS      64
#define PAQ_MAX_FILAS         48
#define PAQ_MAX_TOTAL         (PAQ_ENCABEZADO + PAQ_MAX_COLUMNAS * PAQ_MAX_FILAS * 3)
#define PAQ_LIMITE_UDP        1472   /* lo más que cabe en un paquete UDP sin partirse en una red con MTU 1500 */

#define PAQ_OK                 0
#define PAQ_ERR_ARGUMENTO     -1   /* puntero nulo o capacidad insuficiente */
#define PAQ_ERR_TAMANO        -2   /* columnas o filas fuera de rango */
#define PAQ_ERR_MARCA         -3   /* no empieza con "CEL1" */
#define PAQ_ERR_LARGO         -4   /* el largo del paquete no cuadra con sus columnas y filas */

/* Lo que trae un paquete (sin las celdas). */
typedef struct {
    unsigned int numero;
    unsigned int ms_captura;
    int ancho, alto;
    int columnas, filas;
} DatosPaquete;

/* Arma un paquete en `buf` (capacidad `capacidad` bytes) con las celdas `celdas` (columnas * filas * 3 bytes). Devuelve el largo del paquete (> 0) o un código de error (< 0). */
int paquete_armar(unsigned char *buf, int capacidad, const DatosPaquete *datos, const unsigned char *celdas);

/* Lee un paquete de `largo` bytes. Deja los datos en `datos` y copia las celdas a `celdas` (capacidad `capacidad_celdas` bytes). Devuelve PAQ_OK o un código de error. */
int paquete_leer(const unsigned char *buf, int largo, DatosPaquete *datos, unsigned char *celdas, int capacidad_celdas);

#ifdef __cplusplus
}
#endif

#endif /* PAQUETE_CELDAS_H */
