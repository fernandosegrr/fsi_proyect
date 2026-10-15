/* pruebas_sesion_compartida.c — pruebas de sesion_compartida.c: abrir, compartir, cerrar, fallo y reintento (sin hardware). */
#include <stdio.h>
#include <stdint.h>
#include "sesion_compartida.h"

#define U_INICIO  0
#define U_MANUAL  1
#define U_VISION  2
#define U_RUTA    3

static int casos = 0, fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

int main(void)
{
    Sesion s;
    uint32_t t = 1000;

    /* ── Abrir ─────────────────────────────────────────────────────────── */
    ses_iniciar(&s);
    caso("recién creada: cerrada y sin usuarios", s.estado == SES_CERRADA && ses_cuenta_usuarios(&s) == 0);
    caso("tick sin nadie: no hace nada", ses_tick(&s, t) == SES_NADA);
    caso("el primero que entra manda ABRIR", ses_entrar(&s, U_MANUAL, t) == SES_ABRIR);
    caso("tras abrir está ABRIENDO con 1 usuario e 1 intento", s.estado == SES_ABRIENDO && ses_cuenta_usuarios(&s) == 1 && s.intentos == 1);
    ses_funciona(&s);
    caso("llega el primer cuadro: ABIERTA", s.estado == SES_ABIERTA);

    /* ── Compartir ─────────────────────────────────────────────────────── */
    caso("el segundo usuario NO vuelve a abrir (se comparte)", ses_entrar(&s, U_VISION, t) == SES_NADA);
    caso("con dos usuarios sigue ABIERTA y con 1 solo intento", s.estado == SES_ABIERTA && ses_cuenta_usuarios(&s) == 2 && s.intentos == 1);
    caso("el tercer usuario tampoco abre", ses_entrar(&s, U_RUTA, t) == SES_NADA && ses_cuenta_usuarios(&s) == 3);
    caso("entrar dos veces con el mismo número cuenta UNA vez", ses_entrar(&s, U_RUTA, t) == SES_NADA && ses_cuenta_usuarios(&s) == 3);
    caso("ses_tiene_usuario: dentro y fuera", ses_tiene_usuario(&s, U_VISION) && !ses_tiene_usuario(&s, U_INICIO));

    /* ── Cerrar solo cuando sale el último ─────────────────────────────── */
    ses_salir(&s, U_MANUAL, t);
    ses_salir(&s, U_RUTA, t);
    caso("salen dos de tres: sigue ABIERTA y no hay cierre programado", s.estado == SES_ABIERTA && !s.cierre_pendiente && ses_tick(&s, t + 10000) == SES_NADA);
    t += 20000;
    caso("sale el último: programa el cierre pero NO cierra al instante", (ses_salir(&s, U_VISION, t), s.cierre_pendiente == 1) && s.estado == SES_ABIERTA);
    caso("antes de la gracia no se cierra", ses_tick(&s, t + SES_GRACIA_CIERRE_MS - 1) == SES_NADA && s.estado == SES_ABIERTA);
    caso("al vencer la gracia manda CERRAR", ses_tick(&s, t + SES_GRACIA_CIERRE_MS) == SES_CERRAR && s.estado == SES_CERRADA);
    caso("cerrada: otro tick no repite el cierre", ses_tick(&s, t + 99999) == SES_NADA);

    /* ── Gracia: cambiar de pantalla no corta el video ─────────────────── */
    ses_iniciar(&s);
    t = 5000;
    ses_entrar(&s, U_MANUAL, t);
    ses_funciona(&s);
    ses_salir(&s, U_MANUAL, t + 100);                         /* sale Manual ... */
    caso("al cambiar de pantalla, la que entra dentro de la gracia NO reabre", ses_entrar(&s, U_RUTA, t + 300) == SES_NADA);
    caso("el cierre programado se cancela", !s.cierre_pendiente && ses_tick(&s, t + 5000) == SES_NADA && s.estado == SES_ABIERTA);
    caso("y fue UN solo intento de apertura", s.intentos == 1);
    ses_salir(&s, U_RUTA, t + 6000);
    caso("si vuelve a entrar justo antes del cierre, se salva", ses_entrar(&s, U_MANUAL, t + 6000 + SES_GRACIA_CIERRE_MS - 1) == SES_NADA && s.estado == SES_ABIERTA);
    ses_salir(&s, U_MANUAL, t + 9000);
    ses_tick(&s, t + 9000 + SES_GRACIA_CIERRE_MS);
    caso("pero si entra después de cerrarse, se vuelve a ABRIR", ses_entrar(&s, U_INICIO, t + 12000) == SES_ABRIR && s.intentos == 2);

    /* ── Salidas raras ─────────────────────────────────────────────────── */
    ses_iniciar(&s);
    caso("salir sin haber entrado no hace nada", ses_salir(&s, U_MANUAL, 100) == SES_NADA && !s.cierre_pendiente && s.estado == SES_CERRADA);
    caso("usuario negativo: se ignora", ses_entrar(&s, -1, 100) == SES_NADA && s.estado == SES_CERRADA);
    caso("usuario 16 (fuera de rango): se ignora", ses_entrar(&s, SES_MAX_USUARIOS, 100) == SES_NADA && s.estado == SES_CERRADA);
    caso("usuario 15 (el último válido) abre", ses_entrar(&s, SES_MAX_USUARIOS - 1, 100) == SES_ABRIR);
    ses_salir(&s, 7, 200);
    caso("salir con otro número no saca al que está", ses_cuenta_usuarios(&s) == 1);

    /* ── Fallo y reintento ─────────────────────────────────────────────── */
    ses_iniciar(&s);
    t = 10000;
    ses_entrar(&s, U_MANUAL, t);
    ses_fallo(&s, t + 3000);
    caso("falla al abrir: estado FALLO y 1 fallo", s.estado == SES_FALLO && s.fallos == 1);
    caso("en FALLO el usuario sigue dentro (la pantalla muestra el aviso)", ses_tiene_usuario(&s, U_MANUAL));
    caso("antes del plazo no reintenta", ses_tick(&s, t + 3000 + SES_REINTENTO_MS - 1) == SES_NADA && s.estado == SES_FALLO);
    caso("al vencer el plazo reintenta solo (REABRIR)", ses_tick(&s, t + 3000 + SES_REINTENTO_MS) == SES_REABRIR && s.estado == SES_ABRIENDO && s.intentos == 2);
    ses_fallo(&s, t + 9000);
    caso("otro fallo seguido: ya van 2", s.fallos == 2 && s.estado == SES_FALLO);
    ses_fallo(&s, t + 9500);
    caso("avisar el mismo fallo dos veces no cuenta doble ni mueve el plazo", s.fallos == 2 && s.t_reintento_ms == t + 9000 + SES_REINTENTO_MS);
    caso("el usuario toca «Reintentar»: REABRIR al instante", ses_reintentar(&s, t + 9600) == SES_REABRIR && s.estado == SES_ABRIENDO && s.intentos == 3);
    ses_funciona(&s);
    caso("si funciona, se olvidan los fallos", s.estado == SES_ABIERTA && s.fallos == 0);

    /* ── Fallo con la sesión ya abierta (se cayó el video) ─────────────── */
    ses_fallo(&s, t + 20000);
    caso("el video abierto se cae: FALLO", s.estado == SES_FALLO && s.fallos == 1);
    ses_funciona(&s);
    caso("el video vuelve solo: ABIERTA otra vez", s.estado == SES_ABIERTA);

    /* ── Fallo y nadie lo usa ──────────────────────────────────────────── */
    ses_fallo(&s, t + 30000);
    ses_salir(&s, U_MANUAL, t + 30100);
    caso("en FALLO sin usuarios no se reintenta: se cierra tras la gracia", ses_tick(&s, t + 30100 + SES_GRACIA_CIERRE_MS) == SES_CERRAR && s.estado == SES_CERRADA);
    caso("Reintentar sin usuarios no hace nada", ses_reintentar(&s, t + 40000) == SES_NADA);

    /* ── Avisos atrasados con la sesión cerrada ────────────────────────── */
    ses_iniciar(&s);
    ses_funciona(&s);
    caso("«funciona» con la sesión cerrada se ignora", s.estado == SES_CERRADA);
    ses_fallo(&s, 100);
    caso("«fallo» con la sesión cerrada se ignora", s.estado == SES_CERRADA && s.fallos == 0);

    /* ── El reloj de 32 bits da la vuelta ──────────────────────────────── */
    ses_iniciar(&s);
    t = 0xFFFFFF00u;
    ses_entrar(&s, U_MANUAL, t);
    ses_funciona(&s);
    ses_salir(&s, U_MANUAL, t);                                /* cierre programado cruzando el desbordamiento */
    caso("desbordamiento: antes de la gracia no cierra", ses_tick(&s, t + 1000) == SES_NADA);
    caso("desbordamiento: después de la gracia cierra", ses_tick(&s, t + SES_GRACIA_CIERRE_MS + 10) == SES_CERRAR);
    ses_iniciar(&s);
    ses_entrar(&s, U_MANUAL, t);
    ses_fallo(&s, t);
    caso("desbordamiento: el reintento también cruza el cero", ses_tick(&s, t + SES_REINTENTO_MS - 1) == SES_NADA && ses_tick(&s, t + SES_REINTENTO_MS) == SES_REABRIR);

    /* ── Textos ────────────────────────────────────────────────────────── */
    caso("textos de estado", ses_texto_estado(SES_CERRADA)[0] == 'C' && ses_texto_estado(SES_FALLO)[0] == 'F' && ses_texto_estado(SES_ABIERTA)[0] == 'A');

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
