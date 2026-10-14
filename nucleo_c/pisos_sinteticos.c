/*
 * pisos_sinteticos.c — Cuadros sintéticos de pisos difíciles (ver pisos_sinteticos.h). Solo para pruebas de PC.
 *
 * GUÍA DE LECTURA
 *  1. color_del_piso(): el color de un píxel del piso sin luz ni ruido, según el tipo (con el color MEZCLADO en el borde de las baldosas).
 *  2. luz_pct(): cuánto cambia la luz en ese píxel (degradado, viñeteo y exposición).
 *  3. ps_dibujar(): arma el cuadro píxel por píxel, con el ruido, la sombra y el objeto.
 */

#include <stddef.h>
#include <stdint.h>
#include "pisos_sinteticos.h"

/* ── Números al azar repetibles ────────────────────────────────────────── */

static uint64_t estado_azar;   /* 64 bits: en Windows unsigned long solo tiene 32 */

static int azar_uniforme(int minimo, int maximo)   /* de minimo a maximo, incluidos */
{
    estado_azar = estado_azar * 6364136223846793005ULL + 1442695040888963407ULL;
    return minimo + (int)((estado_azar >> 33) % (uint64_t)(maximo - minimo + 1));
}

/* Ruido aproximadamente normal con desvío `sigma`: la suma de cuatro uniformes. */
static int ruido(int sigma)
{
    int a, i, suma = 0;

    if (sigma <= 0) {
        return 0;
    }
    a = sigma * 866 / 1000;
    if (a < 1) {
        a = 1;
    }
    for (i = 0; i < 4; i++) {
        suma += azar_uniforme(-a, a);
    }
    return suma;
}

/* ── Colores ───────────────────────────────────────────────────────────── */

typedef struct { int r, g, b; } Color;

static Color color(int r, int g, int b)
{
    Color c;

    c.r = r;
    c.g = g;
    c.b = b;
    return c;
}

static Color mezclar(Color a, Color b, int peso_b_pct)
{
    Color c;

    c.r = (a.r * (100 - peso_b_pct) + b.r * peso_b_pct) / 100;
    c.g = (a.g * (100 - peso_b_pct) + b.g * peso_b_pct) / 100;
    c.b = (a.b * (100 - peso_b_pct) + b.b * peso_b_pct) / 100;
    return c;
}

/* Cuánto se mezcla el color vecino en un píxel que está a `d` píxeles de un borde de baldosa: 50 % en el borde, 0 % a 2 px. */
static int peso_de_borde(int d)
{
    return (d >= 2) ? 0 : (2 - d) * 25;
}

/* Distancia (en píxeles) de la coordenada `p` al borde más cercano de una cuadrícula de lado `lado`. */
static int distancia_a_borde(int p, int lado)
{
    int resto = p % lado;
    int a_la_izquierda = resto;
    int a_la_derecha = lado - 1 - resto;

    return (a_la_izquierda < a_la_derecha) ? a_la_izquierda : a_la_derecha;
}

/* Color del piso en (x, y), sin luz ni ruido. */
static Color color_del_piso(int tipo, int x_real, int desfase, int y, int baldosa_px)
{
    int x = x_real + desfase + 4000;   /* +4000 para que nunca sea negativa */
    Color a, b;
    int lado, indice, d, peso;

    switch (tipo) {
    case PS_NEGRO:
        return color(14, 14, 18);
    case PS_LISO:
        return color(120, 100, 80);
    case PS_BALDOSAS:   /* baldosas claras y oscuras de 40 px en damero */
        a = color(180, 172, 160);
        b = color(95, 98, 108);
        lado = (baldosa_px > 0) ? baldosa_px : 40;
        indice = ((x / lado) + (y / lado)) & 1;
        d = distancia_a_borde(x, lado);
        peso = distancia_a_borde(y, lado);
        d = (d < peso) ? d : peso;
        break;
    case PS_FRANJAS:    /* franjas verticales de 30 px */
        a = color(150, 125, 95);
        b = color(100, 80, 60);
        lado = 30;
        indice = (x / lado) & 1;
        d = distancia_a_borde(x, lado);
        break;
    default:            /* PS_DAMERO: cuadros de 20 px */
        a = color(200, 200, 195);
        b = color(60, 60, 65);
        lado = 20;
        indice = ((x / lado) + (y / lado)) & 1;
        d = distancia_a_borde(x, lado);
        peso = distancia_a_borde(y, lado);
        d = (d < peso) ? d : peso;
        break;
    }
    peso = peso_de_borde(d);
    return indice ? mezclar(b, a, peso) : mezclar(a, b, peso);
}

/* Cuánta luz hay en (x, y) en por ciento: degradado de arriba a abajo, viñeteo en las esquinas y exposición de todo el cuadro. */
static int luz_pct(const EscenaSintetica *e, int x, int y)
{
    long degradado = 100L + (long)e->degradado_pct * (PS_ALTO - 1 - y) / (PS_ALTO - 1);
    long dx = x - PS_ANCHO / 2, dy = y - PS_ALTO / 2;
    long r2_mil = (dx * dx * 1000L / ((PS_ANCHO / 2) * (PS_ANCHO / 2)) + dy * dy * 1000L / ((PS_ALTO / 2) * (PS_ALTO / 2))) / 2;   /* 0 a 1000 */
    long vineteo = 100L - (long)e->vineteo_pct * r2_mil / 1000L;
    long total = degradado * vineteo / 100L * e->exposicion_pct / 100L;

    return (int)total;
}

/* Sombra suave: el piso queda 30 % más oscuro en una elipse centrada en (120, 110) con el borde difuso. */
static int sombra_pct(int x, int y)
{
    long dx = x - 120, dy = y - 110;
    long r2_mil = dx * dx * 1000L / (50L * 50L) + dy * dy * 1000L / (30L * 30L);   /* 1000 = en el borde de la elipse */

    if (r2_mil >= 1600) {
        return 100;
    }
    if (r2_mil <= 400) {
        return 70;
    }
    return 70 + (int)((r2_mil - 400) * 30 / 1200);   /* se aclara poco a poco hacia afuera */
}

static int limitar(int v)
{
    return (v < 0) ? 0 : ((v > 255) ? 255 : v);
}

/* Tamaño y color de cada objeto. */
static void forma_del_objeto(int objeto, int *ancho, int *alto, Color *c)
{
    switch (objeto) {
    case PS_MANO:    *ancho = 40;  *alto = 50;  *c = color(205, 160, 135); break;
    case PS_LIBRETA: *ancho = 60;  *alto = 40;  *c = color(235, 225, 140); break;   /* libreta amarilla */
    case PS_CAJA:    *ancho = 70;  *alto = 60;  *c = color(50, 120, 160);  break;   /* caja azul-verdosa */
    default:         *ancho = 100; *alto = 80;  *c = color(35, 50, 110);   break;   /* PS_MOCHILA */
    }
}

/* Error de color por bloque de 16x16 (como el que deja el JPEG): un desplazamiento fijo por bloque y canal, repetible. */
static int croma_de_bloque(const EscenaSintetica *e, int x, int y, int canal)
{
    unsigned long h;

    if (e->croma_bloques <= 0) {
        return 0;
    }
    h = (unsigned long)((x / 16) * 73856093UL ^ (y / 16) * 19349663UL ^ (unsigned long)canal * 83492791UL ^ (unsigned long)(e->semilla / 1000UL));
    h = (h ^ (h >> 13)) * 1274126177UL;
    return (int)((h >> 8) % (unsigned long)(2 * e->croma_bloques + 1)) - e->croma_bloques;
}

void ps_dibujar(const EscenaSintetica *escena, unsigned char *rgb)
{
    int x, y, ancho_obj = 0, alto_obj = 0;
    Color color_obj = color(0, 0, 0);

    estado_azar = (uint64_t)escena->semilla * 2654435761ULL + 12345ULL;
    if (escena->objeto != PS_NADA) {
        forma_del_objeto(escena->objeto, &ancho_obj, &alto_obj, &color_obj);
    }
    for (y = 0; y < PS_ALTO; y++) {
        for (x = 0; x < PS_ANCHO; x++) {
            Color base;
            int luz, comun, canal[3];
            int dentro_del_objeto = (escena->objeto != PS_NADA && x >= escena->obj_x && x < escena->obj_x + ancho_obj &&
                                     y >= escena->obj_y && y < escena->obj_y + alto_obj);

            if (dentro_del_objeto) {
                base = color_obj;
            } else {
                base = color_del_piso(escena->piso, x, escena->desfase_x, y, escena->baldosa_px);
                if (escena->sombra) {
                    int s = sombra_pct(x, y);

                    base.r = base.r * s / 100;
                    base.g = base.g * s / 100;
                    base.b = base.b * s / 100;
                }
            }
            luz = luz_pct(escena, x, y);
            comun = ruido(escena->sigma / 2);   /* una parte del ruido es igual en los tres canales (luminancia) */
            if (escena->reflejo != 0 && !dentro_del_objeto) {   /* mancha de luz abajo al centro: se apaga hacia afuera */
                long dxr = x - 120, dyr = y - 150;
                long r2 = dxr * dxr * 1000L / (45L * 45L) + dyr * dyr * 1000L / (22L * 22L);

                if (r2 < 1000) {
                    comun += (int)(escena->reflejo * (1000 - r2) / 1000);
                }
            }
            canal[0] = limitar(base.r * luz / 100 + comun + ruido(escena->sigma) + croma_de_bloque(escena, x, y, 0));
            canal[1] = limitar(base.g * luz / 100 + comun + ruido(escena->sigma) + croma_de_bloque(escena, x, y, 1));
            canal[2] = limitar(base.b * luz / 100 + comun + ruido(escena->sigma) + croma_de_bloque(escena, x, y, 2));
            rgb[(y * PS_ANCHO + x) * 3] = (unsigned char)canal[0];
            rgb[(y * PS_ANCHO + x) * 3 + 1] = (unsigned char)canal[1];
            rgb[(y * PS_ANCHO + x) * 3 + 2] = (unsigned char)canal[2];
        }
    }
}

const char *ps_nombre_piso(int piso)
{
    switch (piso) {
    case PS_NEGRO:    return "negro";
    case PS_LISO:     return "liso";
    case PS_BALDOSAS: return "baldosas";
    case PS_FRANJAS:  return "franjas";
    default:          return "damero";
    }
}

const char *ps_nombre_objeto(int objeto)
{
    switch (objeto) {
    case PS_NADA:    return "nada";
    case PS_MANO:    return "mano";
    case PS_LIBRETA: return "libreta";
    case PS_CAJA:    return "caja";
    default:         return "mochila";
    }
}
