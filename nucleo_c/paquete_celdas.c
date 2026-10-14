/*
 * paquete_celdas.c — Arma y lee el paquete UDP del modo CELDAS (ver paquete_celdas.h).
 *
 * GUÍA DE LECTURA
 *  1. poner16 / poner32 / leer16 / leer32: escritura y lectura de enteros en little-endian byte por byte (no depende del orden ni de la alineación del procesador).
 *  2. paquete_armar: encabezado + celdas. paquete_leer: revisa la marca, los tamaños y que el largo cuadre EXACTO antes de copiar nada.
 */

#include <stddef.h>
#include <string.h>
#include "paquete_celdas.h"

static void poner16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void poner32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static unsigned int leer16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int leer32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int paquete_armar(unsigned char *buf, int capacidad, const DatosPaquete *datos, const unsigned char *celdas)
{
    int total_celdas, largo;

    if (buf == NULL || datos == NULL || celdas == NULL) {
        return PAQ_ERR_ARGUMENTO;
    }
    if (datos->columnas < 1 || datos->columnas > PAQ_MAX_COLUMNAS || datos->filas < 1 || datos->filas > PAQ_MAX_FILAS || datos->ancho < 0 || datos->ancho > 65535 ||
        datos->alto < 0 || datos->alto > 65535) {
        return PAQ_ERR_TAMANO;
    }
    total_celdas = datos->columnas * datos->filas * 3;
    largo = PAQ_ENCABEZADO + total_celdas;
    if (capacidad < largo) {
        return PAQ_ERR_ARGUMENTO;
    }
    buf[0] = 'C';
    buf[1] = 'E';
    buf[2] = 'L';
    buf[3] = '1';
    poner32(buf + 4, datos->numero);
    poner32(buf + 8, datos->ms_captura);
    poner16(buf + 12, (unsigned int)datos->ancho);
    poner16(buf + 14, (unsigned int)datos->alto);
    buf[16] = (unsigned char)datos->columnas;
    buf[17] = (unsigned char)datos->filas;
    buf[18] = 0;
    buf[19] = 0;
    memcpy(buf + PAQ_ENCABEZADO, celdas, (size_t)total_celdas);
    return largo;
}

int paquete_leer(const unsigned char *buf, int largo, DatosPaquete *datos, unsigned char *celdas, int capacidad_celdas)
{
    int columnas, filas, total_celdas;

    if (buf == NULL || datos == NULL || celdas == NULL || largo < 0) {
        return PAQ_ERR_ARGUMENTO;
    }
    if (largo < PAQ_ENCABEZADO || buf[0] != 'C' || buf[1] != 'E' || buf[2] != 'L' || buf[3] != '1') {
        return (largo < PAQ_ENCABEZADO) ? PAQ_ERR_LARGO : PAQ_ERR_MARCA;
    }
    columnas = buf[16];
    filas = buf[17];
    if (columnas < 1 || columnas > PAQ_MAX_COLUMNAS || filas < 1 || filas > PAQ_MAX_FILAS) {
        return PAQ_ERR_TAMANO;
    }
    total_celdas = columnas * filas * 3;
    if (largo != PAQ_ENCABEZADO + total_celdas) {
        return PAQ_ERR_LARGO;
    }
    if (capacidad_celdas < total_celdas) {
        return PAQ_ERR_ARGUMENTO;
    }
    datos->numero = leer32(buf + 4);
    datos->ms_captura = leer32(buf + 8);
    datos->ancho = (int)leer16(buf + 12);
    datos->alto = (int)leer16(buf + 14);
    datos->columnas = columnas;
    datos->filas = filas;
    memcpy(celdas, buf + PAQ_ENCABEZADO, (size_t)total_celdas);
    return PAQ_OK;
}
