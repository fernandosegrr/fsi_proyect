/*
 * rumbo.c — Control del rumbo al avanzar (ver rumbo.h).
 *
 * CONTROL. error = objetivo - ángulo (décimas de grado; + = hay que girar a la izquierda).
 *   proporcional = error x RUMBO_KP_MIL_GRADO / 10                         (milésimas de potencia)
 *   integral     = suma(error x dt) x RUMBO_KP_MIL_GRADO / (10 x RUMBO_TI_MS)
 *   corrección   = proporcional + integral, recortada a ±RUMBO_MAX_MIL.
 * El integral solo crece mientras la corrección no está en el tope en ese mismo sentido (no se "carga" de más) y nunca pasa de ±RUMBO_MAX_MIL por sí solo.
 * Todo en enteros de 32 bits: |error| <= RUMBO_ERROR_MAX_X10 y dt <= RUMBO_DT_MAX_MS, así que cada paso suma a lo más 300 x 50.
 */

#include <stdint.h>
#include "rumbo.h"
#include "config.h"

/* El integral (décimas x ms) que da `mil` milésimas de corrección. */
static int32_t integral_de_mil(int32_t mil)
{
    return (int32_t)(((int64_t)mil * 10 * RUMBO_TI_MS) / RUMBO_KP_MIL_GRADO);
}

static int32_t mil_de_integral(int32_t integral)
{
    return (int32_t)(((int64_t)integral * RUMBO_KP_MIL_GRADO) / (10 * (int64_t)RUMBO_TI_MS));
}

static int32_t recortar(int32_t v, int32_t limite)
{
    return (v > limite) ? limite : ((v < -limite) ? -limite : v);
}

void rumbo_iniciar(Rumbo *r)
{
    if (r == 0) {
        return;
    }
    r->encendido = 0;
    r->en_marcha = 0;
    r->inicio_x10 = 0;
    r->objetivo_x10 = 0;
    r->integral = 0;
    r->aprendido_mil = 0;
    r->t_ms = 0;
    r->hay_t = 0;
    r->corr_mil = 0;
    r->fuera = 0;
}

void rumbo_empezar(Rumbo *r, int32_t angulo_x10, uint32_t ahora_ms)
{
    if (r == 0 || !r->encendido) {
        return;
    }
    r->en_marcha = 1;
    r->inicio_x10 = angulo_x10;
    r->objetivo_x10 = angulo_x10;
    r->integral = integral_de_mil(recortar(r->aprendido_mil, RUMBO_MAX_MIL));
    r->t_ms = ahora_ms;
    r->hay_t = 1;
    r->corr_mil = 0;
    r->fuera = 0;
}

void rumbo_terminar(Rumbo *r)
{
    if (r == 0 || !r->en_marcha) {
        return;
    }
    r->aprendido_mil = recortar(mil_de_integral(r->integral), RUMBO_MAX_MIL);
    r->en_marcha = 0;
    r->corr_mil = 0;
    r->hay_t = 0;
}

int rumbo_poner_objetivo(Rumbo *r, int32_t objetivo_x10)
{
    if (r == 0 || !r->en_marcha) {
        return 0;
    }
    if (objetivo_x10 > r->inicio_x10 + RUMBO_MAX_OBJETIVO_X10) {
        objetivo_x10 = r->inicio_x10 + RUMBO_MAX_OBJETIVO_X10;
    } else if (objetivo_x10 < r->inicio_x10 - RUMBO_MAX_OBJETIVO_X10) {
        objetivo_x10 = r->inicio_x10 - RUMBO_MAX_OBJETIVO_X10;
    }
    r->objetivo_x10 = objetivo_x10;
    return 1;
}

int rumbo_corregir(Rumbo *r, int32_t angulo_x10, uint32_t ahora_ms)
{
    int32_t error, dt, p, i, u, candidato;

    if (r == 0 || !r->en_marcha) {
        return 0;
    }
    error = r->objetivo_x10 - angulo_x10;
    dt = r->hay_t ? (int32_t)(uint32_t)(ahora_ms - r->t_ms) : 0;
    if (dt < 0 || dt > RUMBO_DT_MAX_MS) {
        dt = RUMBO_DT_MAX_MS;   /* loop() atrasado o reloj raro: no se integra un hueco enorme */
    }
    r->t_ms = ahora_ms;
    r->hay_t = 1;
    if (error > RUMBO_ERROR_MAX_X10 || error < -RUMBO_ERROR_MAX_X10) {
        r->fuera = 1;           /* algo raro (choque, lo levantaron): no se pelea con el carro */
        r->corr_mil = 0;
        return 0;
    }
    r->fuera = 0;
    p = error * RUMBO_KP_MIL_GRADO / 10;
    candidato = recortar(r->integral + error * dt, integral_de_mil(RUMBO_MAX_MIL));
    u = p + mil_de_integral(candidato);
    if ((u <= RUMBO_MAX_MIL && u >= -RUMBO_MAX_MIL) || (u > RUMBO_MAX_MIL && error < 0) || (u < -RUMBO_MAX_MIL && error > 0)) {
        r->integral = candidato;   /* sin saturar, o el error ya empuja hacia afuera del tope: se integra */
    }
    i = mil_de_integral(r->integral);
    r->corr_mil = (int)recortar(p + i, RUMBO_MAX_MIL);
    return r->corr_mil;
}

/* x * m / 1000 redondeado (x >= 0, m >= 0). */
static int escalar_mil(int x, int m)
{
    return (int)(((int64_t)x * m + 500) / 1000);
}

void rumbo_repartir(int corr_mil, int base_izq, int base_der, int tope, int *izq, int *der)
{
    int c = (int)recortar(corr_mil, RUMBO_MAX_MIL);
    int a, b;

    if (izq == 0 || der == 0) {
        return;
    }
    a = escalar_mil(base_izq < 0 ? 0 : base_izq, 1000 - c);
    b = escalar_mil(base_der < 0 ? 0 : base_der, 1000 + c);
    if (b > tope && b > 0) {
        a = (int)(((int64_t)a * tope + b / 2) / b);
        b = tope;
    }
    if (a > tope && a > 0) {
        b = (int)(((int64_t)b * tope + a / 2) / a);
        a = tope;
    }
    *izq = (a < 0) ? 0 : a;
    *der = (b < 0) ? 0 : b;
}

int rumbo_leer_numero(const char *texto, int32_t *valor)
{
    int32_t n = 0;
    int signo = 1, cifras = 0;

    if (texto == 0 || valor == 0) {
        return 0;
    }
    if (*texto == '-' || *texto == '+') {
        signo = (*texto == '-') ? -1 : 1;
        texto++;
    }
    while (*texto >= '0' && *texto <= '9') {
        if (++cifras > 6) {
            return 0;
        }
        n = n * 10 + (*texto - '0');
        texto++;
    }
    if (cifras == 0 || *texto != '\0') {
        return 0;
    }
    *valor = signo * n;
    return 1;
}
