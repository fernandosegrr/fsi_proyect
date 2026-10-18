/*
 * tof.c — Sensor láser VL53L0X en C puro (ver tof.h).
 *
 * Cada lectura cruda se clasifica así:
 *   8190, 65535 o más de TOF_LIBRE_MM  → LIBRE (nada a la vista): rompe cualquier racha de "cerca".
 *   menos de TOF_MIN_VALIDO_MM          → basura del sensor: se ignora del todo.
 *   el resto                            → medida válida; si es menor de TOF_PARO_MM suma a la racha de "cerca".
 * El valor que se informa es la mediana de las últimas 3 (un pico suelto no cambia nada); "libre" cuenta como TOF_LIBRE_MM.
 * Si el bus falla TOF_ERRORES_MAX veces seguidas, o pasan TOF_SIN_DATOS_MS sin ninguna lectura, el sensor se da por caído y no se vuelve a usar.
 */

#include <stdint.h>
#include "tof.h"
#include "hw_sensores.h"
#include "config.h"

static int      vivo            = 0;
static int      eventos         = 0;
static int      errores         = 0;
static uint32_t proximo_ms      = 0;    /* hora de la siguiente pregunta "¿hay medida lista?" */
static uint32_t ultima_lectura_ms = 0;
static int      hay_lectura     = 0;
static int      arranque_marcado = 0;   /* 1 cuando ya se anotó la hora desde la que se cuenta el tiempo sin datos */
static int      valida          = 0;
static int      cerca_seguidas  = 0;
static int      ultimas[TOF_PARO_LECTURAS];   /* las últimas lecturas válidas cercanas (para informar la mayor) */
static int      ventana[3];                   /* para la mediana */
static int      ventana_n       = 0;
static int      filtrado        = TOF_LIBRE_MM;
static uint32_t total_lecturas  = 0;

static void caer(void)
{
    if (vivo) {
        eventos |= TOF_EVT_MAL;
    }
    vivo = 0;
    cerca_seguidas = 0;
    valida = 0;
}

int tof_iniciar(void)
{
    vivo = 0;
    errores = 0;
    hay_lectura = 0;
    arranque_marcado = 0;
    valida = 0;
    cerca_seguidas = 0;
    ventana_n = 0;
    filtrado = TOF_LIBRE_MM;
    total_lecturas = 0;
    proximo_ms = 0;
    if (hw_tof_iniciar()) {
        vivo = 1;
    }
    return vivo;
}

static int mediana3(int a, int b, int c)
{
    if (a > b) { int t = a; a = b; b = t; }
    if (b > c) { int t = b; b = c; c = t; }
    if (a > b) { b = a; }
    return b;
}

/* Una lectura cruda ya recibida. */
static void procesar(int crudo, uint32_t ahora_ms)
{
    int valor;

    ultima_lectura_ms = ahora_ms;
    hay_lectura = 1;
    errores = 0;
    total_lecturas++;

    if (crudo < TOF_MIN_VALIDO_MM && crudo >= 0) {
        return;                                          /* basura: ni libre ni cerca */
    }
    if (crudo < 0 || crudo == 8190 || crudo == 65535 || crudo > TOF_LIBRE_MM) {
        valida = 0;
        cerca_seguidas = 0;
        valor = TOF_LIBRE_MM;
    } else {
        valida = 1;
        valor = crudo;
        if (crudo < TOF_PARO_MM) {
            ultimas[cerca_seguidas % TOF_PARO_LECTURAS] = crudo;
            if (cerca_seguidas < 1000) {
                cerca_seguidas++;
            }
        } else {
            cerca_seguidas = 0;
        }
    }

    if (ventana_n < 3) {
        int i;

        for (i = ventana_n; i < 3; i++) {
            ventana[i] = valor;                          /* la primera lectura llena la ventana */
        }
        ventana_n = 3;
    } else {
        ventana[0] = ventana[1];
        ventana[1] = ventana[2];
        ventana[2] = valor;
    }
    filtrado = mediana3(ventana[0], ventana[1], ventana[2]);
}

void tof_probar_lectura(int mm_crudo, uint32_t ahora_ms)
{
    procesar(mm_crudo, ahora_ms);
}

void tof_actualizar(uint32_t ahora_ms)
{
    int listo;
    int crudo = 0;

    if (!vivo) {
        return;
    }
    if (!hay_lectura && !arranque_marcado) {
        arranque_marcado = 1;
        ultima_lectura_ms = ahora_ms;                    /* desde aquí se cuenta el tiempo sin datos */
    }
    if ((uint32_t)(ahora_ms - ultima_lectura_ms) > TOF_SIN_DATOS_MS) {
        caer();                                          /* el sensor dejó de entregar medidas */
        return;
    }
    if ((int32_t)(ahora_ms - proximo_ms) < 0) {
        return;
    }
    listo = hw_tof_listo();
    if (listo < 0) {
        errores++;
        proximo_ms = ahora_ms + TOF_SONDEO_MS;
        if (errores >= TOF_ERRORES_MAX) {
            caer();
        }
        return;
    }
    if (!listo) {
        proximo_ms = ahora_ms + TOF_SONDEO_MS;
        return;
    }
    if (hw_tof_leer(&crudo) != 0) {
        errores++;
        proximo_ms = ahora_ms + TOF_SONDEO_MS;
        if (errores >= TOF_ERRORES_MAX) {
            caer();
        }
        return;
    }
    procesar(crudo, ahora_ms);
    proximo_ms = ahora_ms + TOF_PERIODO_MS - 8u;       /* la siguiente medida tarda ~TOF_PERIODO_MS: se empieza a preguntar un poco antes */
}

int tof_vivo(void)         { return vivo; }
int tof_mm(void)           { return filtrado; }
int tof_valida(void)       { return valida; }

int tof_cerca_confirmada(void)
{
    int i, mayor = 0;

    if (!vivo || cerca_seguidas < TOF_PARO_LECTURAS) {
        return 0;
    }
    for (i = 0; i < TOF_PARO_LECTURAS; i++) {
        if (ultimas[i] > mayor) {
            mayor = ultimas[i];
        }
    }
    return mayor;
}

int tof_tomar_eventos(void)
{
    int e = eventos;

    eventos = 0;
    return e;
}

uint32_t tof_lecturas(void)
{
    return total_lecturas;
}
