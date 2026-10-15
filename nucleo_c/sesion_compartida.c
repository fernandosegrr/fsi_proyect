/*
 * sesion_compartida.c — Ver sesion_compartida.h para la idea general.
 *
 * GUÍA DE LECTURA
 *  - Una sesión es una estructura pequeña: el estado, un número con un bit por usuario y dos horas (cierre y reintento).
 *  - Nada se abre ni se cierra aquí: cada función devuelve la ACCIÓN que la app debe hacer.
 */
#include "sesion_compartida.h"

/* true si la hora `a` ya llegó a la hora `b` (aguanta que el reloj de 32 bits dé la vuelta). */
static int ya_paso(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) >= 0;
}

static int usuario_valido(int usuario)
{
    return usuario >= 0 && usuario < SES_MAX_USUARIOS;
}

void ses_iniciar(Sesion *s)
{
    s->estado = SES_CERRADA;
    s->usuarios = 0;
    s->cierre_pendiente = 0;
    s->t_cierre_ms = 0;
    s->t_reintento_ms = 0;
    s->intentos = 0;
    s->fallos = 0;
}

AccionSesion ses_entrar(Sesion *s, int usuario, uint32_t ahora_ms)
{
    (void)ahora_ms;
    if (!usuario_valido(usuario)) {
        return SES_NADA;
    }
    s->usuarios |= (uint32_t)1 << usuario;
    s->cierre_pendiente = 0;          /* alguien volvió a tiempo: el cierre programado se cancela */
    if (s->estado == SES_CERRADA) {
        s->estado = SES_ABRIENDO;
        s->intentos++;
        s->fallos = 0;
        return SES_ABRIR;
    }
    return SES_NADA;                  /* ya está abierto o abriéndose: se comparte */
}

AccionSesion ses_salir(Sesion *s, int usuario, uint32_t ahora_ms)
{
    if (!usuario_valido(usuario) || !(s->usuarios & ((uint32_t)1 << usuario))) {
        return SES_NADA;
    }
    s->usuarios &= ~((uint32_t)1 << usuario);
    if (s->usuarios == 0 && s->estado != SES_CERRADA) {
        s->cierre_pendiente = 1;
        s->t_cierre_ms = ahora_ms + SES_GRACIA_CIERRE_MS;
    }
    return SES_NADA;
}

AccionSesion ses_tick(Sesion *s, uint32_t ahora_ms)
{
    if (s->estado == SES_CERRADA) {
        return SES_NADA;
    }
    if (s->usuarios == 0) {
        /* nadie lo usa: si venció la gracia se cierra */
        if (s->cierre_pendiente && ya_paso(ahora_ms, s->t_cierre_ms)) {
            s->cierre_pendiente = 0;
            s->estado = SES_CERRADA;
            return SES_CERRAR;
        }
        return SES_NADA;
    }
    if (s->estado == SES_FALLO && ya_paso(ahora_ms, s->t_reintento_ms)) {
        s->estado = SES_ABRIENDO;
        s->intentos++;
        return SES_REABRIR;
    }
    return SES_NADA;
}

void ses_funciona(Sesion *s)
{
    if (s->estado == SES_CERRADA) {
        return;
    }
    s->estado = SES_ABIERTA;
    s->fallos = 0;
}

void ses_fallo(Sesion *s, uint32_t ahora_ms)
{
    if (s->estado == SES_CERRADA) {
        return;
    }
    if (s->estado != SES_FALLO) {
        s->fallos++;
        s->t_reintento_ms = ahora_ms + SES_REINTENTO_MS;
    }
    s->estado = SES_FALLO;
}

AccionSesion ses_reintentar(Sesion *s, uint32_t ahora_ms)
{
    (void)ahora_ms;
    if (s->usuarios == 0 || s->estado == SES_CERRADA) {
        return SES_NADA;
    }
    s->estado = SES_ABRIENDO;
    s->intentos++;
    return SES_REABRIR;
}

int ses_cuenta_usuarios(const Sesion *s)
{
    int n = 0;
    uint32_t bits = s->usuarios;

    while (bits) {
        n += (int)(bits & 1u);
        bits >>= 1;
    }
    return n;
}

int ses_tiene_usuario(const Sesion *s, int usuario)
{
    return usuario_valido(usuario) && (s->usuarios & ((uint32_t)1 << usuario)) != 0;
}

const char *ses_texto_estado(EstadoSesion estado)
{
    switch (estado) {
    case SES_CERRADA:  return "CERRADA";
    case SES_ABRIENDO: return "ABRIENDO";
    case SES_ABIERTA:  return "ABIERTA";
    case SES_FALLO:    return "FALLO";
    }
    return "?";
}
