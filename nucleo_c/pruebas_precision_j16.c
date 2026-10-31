/* J16: precisión de navegación en simulación. Solo números; nunca imágenes reales.
 *
 * TODO LO DE AQUÍ ES SIMULADO. Que la simulación mejore no prueba que el carro mejore: falta la validación física (docs/precision/plan_pruebas.md).
 *
 * Parte A: la réplica de RUMBO del simulador da lo mismo que firmware_carro/rumbo.c (20000 pasos al azar).
 * Parte B: propiedades de la matemática de pose (pose medida con el giroscopio, giro de pivote ida y vuelta, objetivo de RUMBO).
 * Parte C: Monte Carlo con semilla fija. Un "mundo" independiente del estimador: simulador_navegador.c con el modelo físico J16 (sim_j16_activar),
 *          cámara por zonas (sr_vista) desde la pose atrasada, láser por rayo contra las cajas, choque por ejes separadores.
 *          Supuestos del mundo (ajustados a lo medido en J14; ver docs/precision/simulador.md): AUTO 19.3 cm/s; arranque 100 ms; diferencia de empuje
 *          después del balance de 0.3 a 2.1 % (rectas de 5.5 a 20 cm); 250 ms de arranque a 255/255 sin balance; zona muerta del motor de 60 a 120 de PWM;
 *          giro por ángulo con error de cierre normal (1.1° izq, 1.7° der, tope ±3°); pivote medido (izq 90/-90 mm, der 150/165 mm a 90°) ±15 % por corrida;
 *          velocidad real ±4 %; frenado 25 a 35 mm; carro colocado con ±1° y ±10 mm; escala del giroscopio de -1 a +1 % y sesgo ±0.05 °/s; SENS con 50 a 150 ms
 *          de atraso; comandos con 0 a 2 ticks de retraso; video a 10 cuadros/s con 200 a 300 ms de retraso.
 * Uso: ./pruebas_precision_j16          (rápido: Partes A y B y 150 corridas por escenario, con comprobaciones; lo corre make test)
 *      ./pruebas_precision_j16 1000     (el informe: 1000 corridas por escenario y configuración)
 *      ./pruebas_precision_j16 calibrar (solo la configuración J14 contra lo medido)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simulador_navegador.h"
#include "sim_ruta.h"
#include "rumbo.h"

static Navegador nav;
static Simulador sim;
static int casos, fallos;
static uint32_t azar;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) fallos++;
    printf("%s %s\n", ok ? "OK    " : "FALLO ", nombre);
}

static int entre(int a, int b)
{
    azar = azar * 1664525u + 1013904223u;
    return a + (int)((azar >> 8) % (uint32_t)(b - a + 1));
}

static int abs_i(int a) { return a < 0 ? -a : a; }
static long abs_l(long a) { return a < 0 ? -a : a; }

static long raiz(long v)
{
    long x = 0, b = 1L << 30;

    if (v <= 0) return 0;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= x + b) { v -= x + b; x = (x >> 1) + b; } else { x >>= 1; }
        b >>= 2;
    }
    return x;
}

/* Seno en milésimas de un ángulo en décimas de grado (tabla cada 5° interpolada; solo para medir en el mundo). */
static const int SENOS5[19] = {0, 87, 174, 259, 342, 423, 500, 574, 643, 707, 766, 819, 866, 906, 940, 966, 985, 996, 1000};
static int seno_x10(int g10)
{
    int s = 1, i, r;

    g10 = (g10 % 3600 + 3600) % 3600;
    if (g10 > 1800) { g10 -= 1800; s = -1; }
    if (g10 > 900) g10 = 1800 - g10;
    i = g10 / 50; r = g10 % 50;
    return s * (SENOS5[i] + (r ? (SENOS5[i + 1] - SENOS5[i]) * r / 50 : 0));
}

/* Error normal aproximado (suma de 12 uniformes), sigma en décimas, con tope ±tope décimas. */
static int normal_x10(int sigma_x10, int tope)
{
    int i, suma = 0;

    for (i = 0; i < 12; i++) suma += entre(-500, 500);
    suma = suma * sigma_x10 / 1000;
    return suma < -tope ? -tope : (suma > tope ? tope : suma);
}

/* ── Parte A: réplica de RUMBO contra firmware_carro/rumbo.c ───────────── */

static void parte_a(void)
{
    Rumbo r;
    int i, distintos = 0, reparto_distinto = 0, t = 0, ang = 0;

    printf("-- Parte A: replica de RUMBO del simulador contra firmware_carro/rumbo.c --\n");
    azar = 16001u;
    rumbo_iniciar(&r);
    memset(&sim, 0, sizeof sim);
    for (i = 0; i < 20000; i++) {
        int accion = entre(0, 99);

        if (accion < 3) {                       /* empezar un avance */
            r.encendido = 1;
            rumbo_empezar(&r, ang, (uint32_t)t);
            sim_j16_rumbo_empezar(&sim, ang, t);
        } else if (accion < 5) {                /* terminarlo */
            rumbo_terminar(&r);
            sim_j16_rumbo_terminar(&sim);
        } else if (accion < 10) {               /* objetivo nuevo */
            int o = ang + entre(-400, 400);

            rumbo_poner_objetivo(&r, o);
            sim_j16_rumbo_objetivo(&sim, o);
        } else {
            int c1, c2, i1, d1, i2, d2, bi = entre(150, 255), bd = entre(150, 255);

            t += entre(0, 70);
            ang += entre(-40, 40);
            c1 = rumbo_corregir(&r, ang, (uint32_t)t);
            c2 = sim_j16_rumbo_corregir(&sim, ang, t);
            if (c1 != c2 || r.integral != (int32_t)sim.j16_rumbo_integral) distintos++;
            rumbo_repartir(c1, bi, bd, 255, &i1, &d1);
            sim_j16_rumbo_repartir(c2, bi, bd, 255, &i2, &d2);
            if (i1 != i2 || d1 != d2) reparto_distinto++;
        }
    }
    caso("la replica del simulador corrige igual que rumbo.c en 20000 pasos al azar", distintos == 0);
    caso("y reparte las potencias igual", reparto_distinto == 0);
    caso("y aprende lo mismo", r.aprendido_mil == sim.j16_rumbo_aprendido_mil);
}

/* ── Parte B: propiedades de la matemática de pose ─────────────────────── */

static void preparar_recta(int gyro, int rumbo_fw, int largo_mm)
{
    Punto ruta[11];
    CalibracionCarro cal;
    int i;

    for (i = 0; i < 11; i++) { ruta[i].columna = 20; ruta[i].fila = 30 - i; }
    cal_defecto(&cal);
    cal.ms_por_cm_x10 = 500;
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, &cal);
    navegador_poner_giroscopio(&nav, gyro);
    navegador_poner_rumbo_fw(&nav, rumbo_fw);
    navegador_cargar_ruta(&nav, ruta, 11);
    if (largo_mm > 0) navegador_poner_largo_recta_mm(&nav, largo_mm);   /* un solo avance largo: sin planes en cada celda que lo corten */
}

static void parte_b(void)
{
    int i, malos_dir = 0, malos_largo = 0, malos_paseo = 0, malos_pivote = 0, malos_rumbo0 = 0, malos_lado = 0;

    printf("-- Parte B: propiedades de la pose --\n");
    azar = 16002u;
    /* B1: con el giroscopio fijo en un ángulo cualquiera desde el arranque, el avance estimado va en esa dirección y mide lo que dice el tiempo. */
    for (i = 0; i < 300; i++) {
        int ref = entre(-90000, 90000), theta = entre(-240, 240), t;   /* un salto de más de 25° en un tick se descarta a propósito (J15): se prueban rumbos de hasta 24° con décimas */
        long dx, dy, largo, esperado;
        int dir;

        preparar_recta(1, 0, 0);
        navegador_comenzar(&nav, 0);
        navegador_aporte_sens_imu(&nav, 1200, 1, 0, ref, 1, 0);
        navegador_tick(&nav, 0, NULL);            /* empieza FORWARD con la referencia del giroscopio */
        for (t = 50; t <= 900; t += 50) {   /* dentro de la primera celda (200 mm a 20 cm/s): sin planes nuevos que corrijan el rumbo */
            navegador_aporte_sens_imu(&nav, 1200, 1, 0, ref - theta, 1, (uint32_t)t);   /* + izquierda en el firmware: theta a la derecha */
            navegador_tick(&nav, (uint32_t)t, NULL);
        }
        dx = nav.x_mm - nav.base_x_mm;
        dy = nav.y_mm - nav.base_y_mm;
        largo = raiz(dx * dx + dy * dy);
        esperado = cal_ms_a_mm(&nav.cal, nav.seg_ms);
        /* el primer incremento (50 ms) se hace con el rumbo medio entre 0 y theta; el resto en theta */
        dir = (int)(((dx * seno_x10(theta) - dy * seno_x10(theta + 900)) * 1000) / (largo > 0 ? largo * 1000 : 1));
        if (largo > 0 && dir < 998) malos_dir++;   /* coseno del ángulo entre el avance y theta mayor que 0.998 (~3.6° por el primer pedazo) */
        if (abs_l(largo - esperado) > 3) { malos_largo++; if (getenv("J16_B1")) printf("B1 theta=%d largo=%ld esperado=%ld seg=%d pose=(%d,%d) base=(%d,%d) n_pasos=%d\n", theta, largo, esperado, nav.seg_ms, nav.x_mm, nav.y_mm, nav.base_x_mm, nav.base_y_mm, nav.n_pasos); }
    }
    caso("B1: con el rumbo medido fijo, el avance va en esa direccion (300 rumbos de -24 a 24 grados, con decimas)", malos_dir == 0);
    caso("B1: y su largo es el del tiempo, +-3 mm", malos_largo == 0);
    /* B2: con un paseo al azar del giroscopio, el avance nunca es más largo que lo que dice el tiempo. */
    for (i = 0; i < 300; i++) {
        int ref = entre(-90000, 90000), a = ref, t;
        long dx, dy, largo, esperado;

        preparar_recta(1, 0, 0);
        navegador_comenzar(&nav, 0);
        navegador_aporte_sens_imu(&nav, 1200, 1, 0, ref, 1, 0);
        navegador_tick(&nav, 0, NULL);
        for (t = 50; t <= 900; t += 50) {
            a += entre(-60, 60);
            navegador_aporte_sens_imu(&nav, 1200, 1, 0, a, 1, (uint32_t)t);
            navegador_tick(&nav, (uint32_t)t, NULL);
        }
        dx = nav.x_mm - nav.base_x_mm;
        dy = nav.y_mm - nav.base_y_mm;
        largo = raiz(dx * dx + dy * dy);
        esperado = cal_ms_a_mm(&nav.cal, nav.seg_ms);
        if (largo > esperado + 3) malos_paseo++;
    }
    caso("B2: con el giroscopio paseando al azar el avance estimado nunca pasa del largo por tiempo (300 paseos)", malos_paseo == 0);
    /* B3: girar y deshacer el giro con el modelo de pivote devuelve la misma pose (±2 mm y el mismo rumbo). */
    {
        CalibracionCarro cal;
        int a, d;

        cal_defecto(&cal);
        cal_desplazamiento_de_medida(-1, 90, -90, 90, &a, &d);
        cal_poner_desplazamiento(&cal, -1, a, d);
        cal_desplazamiento_de_medida(1, 150, 165, 90, &a, &d);
        cal_poner_desplazamiento(&cal, 1, a, d);
        for (i = 0; i < 2000; i++) {
            PoseCarro p, q, r;
            int lado = entre(0, 1) ? 1 : -1, g = entre(1, 200);

            p.x_mm = entre(-5000, 5000);
            p.y_mm = entre(-5000, 5000);
            p.rumbo_grados = entre(0, 359);
            cal_pose_tras_giro(&cal, &p, lado, g, &q);
            cal_pose_antes_del_giro(&cal, &q, lado, g, &r);
            if (abs_i(r.x_mm - p.x_mm) > 2 || abs_i(r.y_mm - p.y_mm) > 2 || r.rumbo_grados != p.rumbo_grados) malos_pivote++;
        }
    }
    caso("B3: girar con pivote y deshacerlo devuelve la pose (2000 casos, ambos lados, 1 a 200 grados)", malos_pivote == 0);
    /* B4: con RUMBO, sobre la línea y mirando a lo largo de ella, el primer objetivo es el ángulo del giroscopio al arrancar (sostener): para cualquier ángulo. */
    for (i = 0; i < 300; i++) {
        int ref = entre(-90000, 90000), objetivo = 0, hay;

        preparar_recta(1, 1, 0);
        navegador_comenzar(&nav, 0);
        navegador_aporte_sens_imu(&nav, 1200, 1, 0, ref, 1, 0);
        navegador_tick(&nav, 0, NULL);
        hay = navegador_tomar_rumbo_fw(&nav, &objetivo);
        if (!hay || abs_i(objetivo - ref) > 1) malos_rumbo0++;
    }
    caso("B4: con RUMBO y el carro sobre la linea, el objetivo es sostener el angulo del arranque (300 angulos)", malos_rumbo0 == 0);
    /* B5: con el carro de lado, el objetivo lo regresa a la línea (+ izquierda en el firmware si quedó a la derecha) y nunca se aparta más de NAV_RUMBO_CORR_MAX. */
    for (i = 0; i < 300; i++) {
        int ref = entre(-90000, 90000), lado = entre(-300, 300), objetivo = 0, esperado_x10;

        preparar_recta(1, 1, 0);
        nav.x_mm += lado;                         /* + = a la derecha de la línea (que va hacia arriba) */
        nav.base_x_mm = nav.x_mm;
        navegador_comenzar(&nav, 0);
        navegador_aporte_sens_imu(&nav, 1200, 1, 0, ref, 1, 0);
        navegador_tick(&nav, 0, NULL);
        if (!navegador_tomar_rumbo_fw(&nav, &objetivo)) {
            if (lado != 0) malos_lado++;
            continue;
        }
        /* a la derecha (lado > 0) hay que girar a la izquierda: objetivo del firmware mayor que ref, hasta NAV_RUMBO_CORR_MAX grados */
        esperado_x10 = (lado > 0) ? 1 : ((lado < 0) ? -1 : 0);
        if ((esperado_x10 > 0 && objetivo <= ref && abs_i(lado) > 5) || (esperado_x10 < 0 && objetivo >= ref && abs_i(lado) > 5) ||
            abs_i(objetivo - ref) > NAV_RUMBO_CORR_MAX * 10 + 1) malos_lado++;
    }
    caso("B5: con el carro de lado, el objetivo lo regresa a la linea sin pasar de NAV_RUMBO_CORR_MAX (300 casos)", malos_lado == 0);
}

/* ── Parte C: Monte Carlo ──────────────────────────────────────────────── */

#define MAX_CAJAS 4
#define HISTORIA 32
enum { E_RECTA100, E_RECTA150, E_RECTA200, E_GIRO45, E_GIRO90, E_GIRO180, E_L, E_CUADRADO, E_ESQUIVE20, E_ESQUIVE_ANCHA, N_ESCENARIOS };
static const char *NOMBRE_ESC[N_ESCENARIOS] = { "recta_100", "recta_150", "recta_200", "giro_45", "giro_90", "giro_180", "ruta_L", "cuadrado", "esquive_20x20", "esquive_ancha" };
enum { C_J14, C_J15, C_J16_SIN_FW, C_J16_RUMBO, N_CONFIG };
static const char *NOMBRE_CFG[N_CONFIG] = { "J14 (lo que corrio en B4-B6)", "ANTES: J15 actual", "DESPUES sin firmware nuevo", "DESPUES con RUMBO (firmware nuevo)" };

typedef struct {
    Punto ruta[24];
    int n;
    int override_mm;              /* largo fijo (recta 150) */
    Punto cajas[MAX_CAJAS];
    int n_cajas;
    int fin_x, fin_y;             /* punto final buscado (mm) */
    int linea_x0, linea_y0, linea_rumbo;   /* recta del último tramo para el error lateral */
} Escenario;

static void armar_escenario(int e, Escenario *s, int config)
{
    int i;

    memset(s, 0, sizeof *s);
    switch (e) {
    case E_RECTA100:
        for (i = 0; i <= 5; i++) { s->ruta[i].columna = 2; s->ruta[i].fila = 10 - i; }
        s->n = 6;
        break;
    case E_RECTA150:   /* la de la app: 8 celdas y largo fijo de 1500 mm (J14 y J15 no lo aplicaban: hacían las 8 celdas, 160 cm) */
        for (i = 0; i <= 8; i++) { s->ruta[i].columna = 2; s->ruta[i].fila = 8 - i; }
        s->n = 9;
        s->override_mm = (config >= C_J16_SIN_FW) ? 1500 : 0;
        break;
    case E_RECTA200:
    case E_ESQUIVE20:
    case E_ESQUIVE_ANCHA:
        for (i = 0; i <= 10; i++) { s->ruta[i].columna = 2; s->ruta[i].fila = 10 - i; }
        s->n = 11;
        if (e == E_ESQUIVE20) { s->cajas[0].columna = 2; s->cajas[0].fila = 5; s->n_cajas = 1; }
        if (e == E_ESQUIVE_ANCHA) {   /* 40 x 20 cm, de x = 300 a 700 mm: tapa la línea y se sale 30 cm a la derecha (una caja de 60 cm centrada tapa los tres lados de la cámara y el carro pide ayuda, como está diseñado) */
            for (i = 0; i < 2; i++) { s->cajas[i].columna = 2 + i; s->cajas[i].fila = 5; }
            s->n_cajas = 2;
        }
        break;
    case E_GIRO45:     /* 40 cm, 45° a la derecha, 4 celdas en diagonal */
        s->ruta[0].columna = 2; s->ruta[0].fila = 8;
        s->ruta[1].columna = 2; s->ruta[1].fila = 7;
        s->ruta[2].columna = 2; s->ruta[2].fila = 6;
        for (i = 1; i <= 4; i++) { s->ruta[2 + i].columna = 2 + i; s->ruta[2 + i].fila = 6 - i; }
        s->n = 7;
        break;
    case E_GIRO90:     /* 40 cm, 90° a la derecha, 100 cm */
        s->ruta[0].columna = 2; s->ruta[0].fila = 8;
        s->ruta[1].columna = 2; s->ruta[1].fila = 7;
        s->ruta[2].columna = 2; s->ruta[2].fila = 6;
        for (i = 1; i <= 5; i++) { s->ruta[2 + i].columna = 2 + i; s->ruta[2 + i].fila = 6; }
        s->n = 8;
        break;
    case E_GIRO180:    /* 40 cm, media vuelta, 100 cm de regreso */
        s->ruta[0].columna = 2; s->ruta[0].fila = 8;
        s->ruta[1].columna = 2; s->ruta[1].fila = 7;
        s->ruta[2].columna = 2; s->ruta[2].fila = 6;
        for (i = 1; i <= 5; i++) { s->ruta[2 + i].columna = 2; s->ruta[2 + i].fila = 6 + i; }
        s->n = 8;
        break;
    case E_L:          /* la de la app: (2,6) a (2,1) y a (7,1) */
        for (i = 0; i <= 5; i++) { s->ruta[i].columna = 2; s->ruta[i].fila = 6 - i; }
        for (i = 1; i <= 5; i++) { s->ruta[5 + i].columna = 2 + i; s->ruta[5 + i].fila = 1; }
        s->n = 11;
        break;
    default:           /* cuadrado cerrado de 60 cm: arriba, derecha, abajo, izquierda, vuelve al inicio */
        {
            int k = 0;

            for (i = 0; i <= 3; i++) { s->ruta[k].columna = 2; s->ruta[k].fila = 6 - i; k++; }
            for (i = 1; i <= 3; i++) { s->ruta[k].columna = 2 + i; s->ruta[k].fila = 3; k++; }
            for (i = 1; i <= 3; i++) { s->ruta[k].columna = 5; s->ruta[k].fila = 3 + i; k++; }
            for (i = 1; i <= 3; i++) { s->ruta[k].columna = 5 - i; s->ruta[k].fila = 6; k++; }
            s->n = k;
        }
        break;
    }
    s->fin_x = s->ruta[s->n - 1].columna * 200;
    s->fin_y = s->ruta[s->n - 1].fila * 200;
    if (e == E_RECTA150) {
        s->fin_y = s->ruta[0].fila * 200 - 1500;   /* lo que pide la ruta: 150 cm (J14 y J15 hacían 160) */
    }
    s->linea_x0 = s->ruta[s->n - 2].columna * 200;
    s->linea_y0 = s->ruta[s->n - 2].fila * 200;
    {
        int dx = s->ruta[s->n - 1].columna - s->ruta[s->n - 2].columna, dy = s->ruta[s->n - 1].fila - s->ruta[s->n - 2].fila;

        s->linea_rumbo = (dx == 0) ? ((dy < 0) ? 0 : 180) : ((dy == 0) ? ((dx > 0) ? 90 : 270) : ((dx > 0) ? ((dy < 0) ? 45 : 135) : ((dy < 0) ? 315 : 225)));
    }
}

/* Choque del carro (200 x 240 mm, centrado en su pose) con una caja de 200 x 200 mm: ejes separadores (los de la caja y los del carro). */
static int toca(const Escenario *s)
{
    int k;
    long x = sim_x_mm(&sim), y = sim_y_mm(&sim);
    int h10 = (int)(sim_rumbo_cgrados(&sim) / 10);
    long sn = seno_x10(h10), cs = seno_x10(h10 + 900);   /* adelante = (sn, -cs), derecha = (cs, sn) */

    for (k = 0; k < s->n_cajas; k++) {
        long dx = x - s->cajas[k].columna * 200, dy = y - s->cajas[k].fila * 200;
        long ext_x = (120 * abs_l(sn) + 100 * abs_l(cs)) / 1000, ext_y = (120 * abs_l(cs) + 100 * abs_l(sn)) / 1000;
        long a_lo_largo = (dx * sn - dy * cs) / 1000, de_lado = (dx * cs + dy * sn) / 1000;
        long caja_largo = (100 * abs_l(sn) + 100 * abs_l(cs)) / 1000;   /* lo que mide la caja proyectada en cada eje del carro */

        if (abs_l(dx) > 100 + ext_x || abs_l(dy) > 100 + ext_y) continue;
        if (abs_l(a_lo_largo) > 120 + caja_largo || abs_l(de_lado) > 100 + caja_largo) continue;
        return 1;
    }
    return 0;
}

/* Rayo del láser desde el frente del carro (centro + 120 mm) contra las cajas, de 5 en 5 mm hasta 1200 mm. */
static int laser(const Escenario *s)
{
    int d, k;
    long x = sim_x_mm(&sim), y = sim_y_mm(&sim);
    int h10 = (int)(sim_rumbo_cgrados(&sim) / 10);
    long sn = seno_x10(h10), cs = seno_x10(h10 + 900);

    if (s->n_cajas == 0) return 1200;
    for (d = 5; d < 1200; d += 5) {
        long px = x + sn * (120 + d) / 1000, py = y - cs * (120 + d) / 1000;

        for (k = 0; k < s->n_cajas; k++) {
            if (abs_l(px - s->cajas[k].columna * 200) <= 100 && abs_l(py - s->cajas[k].fila * 200) <= 100) return d;
        }
    }
    return 1200;
}

typedef struct {
    int error;          /* métrica principal (mm): lateral en rectas y esquives, posición final en rutas con giros */
    int lateral;        /* a la recta del último tramo */
    int longitudinal;   /* a lo largo del último tramo, respecto del punto final buscado */
    int rumbo_x10;      /* error de rumbo final respecto del último tramo (décimas, valor absoluto) */
    int estimado;       /* lo que el navegador cree que es el error principal */
    int llego, toco, esquivas, ayuda;
} Resultado;

static int depurar = 0;

static void volcar_diag(void)
{
    char linea[NAV_DIAG_LARGO];

    while (navegador_diag_siguiente(&nav, linea, sizeof linea)) {
        if (depurar && (depurar > 1 || strncmp(linea, "RUTA_DIAG fin", 13) == 0 || strncmp(linea, "TURN_DIAG", 9) == 0 || strncmp(linea, "REINC_DIAG", 10) == 0 ||
                        strncmp(linea, "ESQUIVE_DIAG", 12) == 0 || strncmp(linea, "AYUDA_DIAG", 10) == 0 || strncmp(linea, "SENS_DIAG giro", 14) == 0)) {
            printf("   [%6u ms real=(%d,%d,%d.%02d)] %s\n", (unsigned)sim.t_ms, sim_x_mm(&sim), sim_y_mm(&sim), (int)(sim_rumbo_cgrados(&sim) / 100), (int)abs_l(sim_rumbo_cgrados(&sim) % 100), linea);
        }
    }
}

static Resultado correr(int e, int config, uint32_t semilla)
{
    Escenario s;
    CalibracionCarro cal;
    Resultado r;
    int hx[HISTORIA], hy[HISTORIA], hh[HISTORIA];
    int i, t, retraso, a, d, zm, delta_ppm, tope_ms;
    int ex, ey;

    memset(&r, 0, sizeof r);
    azar = semilla;
    armar_escenario(e, &s, config);

    /* Lo que el navegador cree (la calibración de J15). */
    cal_defecto(&cal);
    cal.arranque_ms = 100;
    cal.ms_por_cm_x10 = 518;
    cal.frenado_mm = 30;
    cal.ms_por_grado_izq_x10 = 74;
    cal.ms_por_grado_der_x10 = 78;
    cal_desplazamiento_de_medida(-1, 90, -90, 90, &a, &d);
    cal_poner_desplazamiento(&cal, -1, a, d);
    cal_desplazamiento_de_medida(1, 150, 165, 90, &a, &d);
    cal_poner_desplazamiento(&cal, 1, a, d);

    navegador_iniciar(&nav);
    nav.sin_rumbo_medido = (config == C_J14);
    nav.sin_trig_fino = (config <= C_J15);
    nav.sin_arreglo_dt_giro = (config <= C_J15);
    nav.sin_margen_paso_j16 = (config <= C_J15);
    if (getenv("J16_SIN_TRIG") && config >= C_J16_SIN_FW) nav.sin_trig_fino = 1;
    if (getenv("J16_SIN_DT") && config >= C_J16_SIN_FW) nav.sin_arreglo_dt_giro = 1;
    if (getenv("J16_SIN_LINEA") && config >= C_J16_SIN_FW) nav.sin_volver_a_linea = 1;
    if (getenv("J16_RUMBO_VUELTA") && config >= C_J16_SIN_FW) nav.rumbo_en_vuelta = 1;
    navegador_poner_calibracion(&nav, &cal);
    navegador_poner_giroscopio(&nav, 1);
    navegador_poner_sensor_distancia(&nav, 1);
    navegador_poner_autoridad_laser(&nav, 1);
    navegador_poner_requiere_camara(&nav, 1);
    navegador_poner_pedir_ayuda(&nav, 1);
    navegador_poner_latencia_visual(&nav, 250);
    navegador_poner_rumbo_fw(&nav, config == C_J16_RUMBO);
    navegador_cargar_ruta(&nav, s.ruta, s.n);
    if (s.override_mm > 0) navegador_poner_largo_recta_mm(&nav, s.override_mm);

    /* El mundo: el carro real. */
    sim_iniciar(&sim, s.ruta[0].columna, s.ruta[0].fila, 0);
    sim_j16_activar(&sim);
    sim.real = cal;
    sim.pct_avance = entre(96, 104);
    sim.real.frenado_mm = entre(25, 35);
    i = entre(85, 115);
    sim.real.desp_adelante_der_um = cal.desp_adelante_der_um * i / 100;
    sim.real.desp_derecha_der_um = cal.desp_derecha_der_um * i / 100;
    i = entre(85, 115);
    sim.real.desp_adelante_izq_um = cal.desp_adelante_izq_um * i / 100;
    sim.real.desp_derecha_izq_um = cal.desp_derecha_izq_um * i / 100;
    zm = entre(60, 120);
    sim.j16_zona_muerta_pwm = zm;
    sim.j16_tau_motor_ms = entre(40, 100);
    /* Diferencia de empuje que queda después del balance (lo que tuerce la recta): 0.3 a 2.1 %, siempre el izquierdo (B4-1 se desvió a la derecha).
     * Calibrado con "./pruebas_precision_j16 calibrar": los desvíos de B4 (55, 150, 180, 200 mm en las 8 celdas = 160 cm) caen en los percentiles 9, 57, 73 y 82. */
    delta_ppm = entre(getenv("J16_DMIN") ? atoi(getenv("J16_DMIN")) : 3000, getenv("J16_DMAX") ? atoi(getenv("J16_DMAX")) : 21000);
    {
        long long b = (long long)(210 - zm) * (2000000LL + delta_ppm), c = (long long)(207 - zm) * (2000000LL - delta_ppm);

        sim.desbalance_mil = (int)((b * 1000 + c / 2) / c);
    }
    sim.j16_sesgo_mdps = entre(-50, 50);
    sim.j16_latencia_sens_ms = entre(50, 150);
    sim.j16_latencia_cmd_ticks = entre(0, 2);
    sim.rumbo_cgrados += entre(-100, 100);   /* colocado a mano: ±1° (el giroscopio no lo ve: mide desde ahí) */
    sim.rumbo0_cgrados = sim.rumbo_cgrados;
    sim.x_cmm += (long)entre(-10, 10) * 100;
    sim.fw_giroscopio = 1;
    sim.tof_activo = 1;
    sim.laser_externo = 1;
    sim.periodo_cuadro_ms = 100;
    sim.j16_rumbo = (config == C_J16_RUMBO);
    sim.fw_escala_pct = entre(-1, 1);   /* error de escala del giroscopio: -1, 0 o +1 % (el simulador lo maneja en % enteros) */
    retraso = entre(4, 6);
    for (i = 0; i < HISTORIA; i++) { hx[i] = sim_x_mm(&sim); hy[i] = sim_y_mm(&sim); hh[i] = 0; }
    navegador_comenzar(&nav, 0);
    tope_ms = (e >= E_ESQUIVE20) ? 120000 : 60000;
    for (t = 0; t < tope_ms; t += NAV_TICK_MS) {
        int hay[3] = {0, 0, 0}, cerc[3] = {0, 0, 0}, celdas[3] = {0, 0, 0}, z;
        int k = (t / NAV_TICK_MS) % HISTORIA, viejo = (k + HISTORIA - retraso) % HISTORIA;

        hx[k] = sim_x_mm(&sim); hy[k] = sim_y_mm(&sim); hh[k] = sim_rumbo_grados(&sim);
        if (s.n_cajas > 0) sr_vista(hx[viejo], hy[viejo], hh[viejo], s.cajas, s.n_cajas, hay, cerc, celdas);
        sim.n_eventos = 0;
        for (z = 0; z < 3; z++) if (hay[z]) sim_agregar_obstaculo_celdas(&sim, (uint32_t)t, (uint32_t)t + 50, z, cerc[z], celdas[z]);
        sim.laser_externo_mm = laser(&s);
        sim.fw_error_izq_x10 = normal_x10(11, 30);
        sim.fw_error_der_x10 = normal_x10(17, 30);
        sim_correr(&sim, &nav, NAV_TICK_MS);
        if (depurar && getenv("J16_TRAZA") && t >= atoi(getenv("J16_TRAZA")) && t <= atoi(getenv("J16_TRAZA")) + 600) {
            printf("   TRAZA t=%d real=(%d,%d,%ld) nav_cmd=%s fisico=%s ms_avance=%d delta_ppm=%ld rumbo_marcha=%d corr=%d giro_fw=%d fw_activo=%d\n", t, sim_x_mm(&sim), sim_y_mm(&sim),
                   sim_rumbo_cgrados(&sim), navegador_texto_comando(nav.cmd_actual), navegador_texto_comando(sim.cmd_previo), sim.j16_ms_avance, sim.j16_delta_ppm,
                   sim.j16_rumbo_en_marcha, sim.j16_rumbo_corr_mil, nav.giro_fw, sim.fw_activo);
        }
        volcar_diag();
        if (s.n_cajas > 0 && toca(&s)) {
            if ((depurar || getenv("J16_TOQUES")) && !r.toco) printf("   [%6u ms] TOQUE real=(%d,%d,%ld) estado=%s fase=%d reinc_fase=%d cmd=%s\n", (unsigned)sim.t_ms, sim_x_mm(&sim), sim_y_mm(&sim),
                                           sim_rumbo_cgrados(&sim) / 100, navegador_texto_estado(nav.estado), nav.fase, nav.reinc_fase, navegador_texto_comando(nav.cmd_actual));
            r.toco = 1;
        }
        if (nav.estado == NAV_TERMINADO || nav.estado == NAV_ERROR || nav.estado == NAV_AYUDA) break;
    }
    sim_correr(&sim, &nav, 600);   /* que termine de frenar */
    r.llego = nav.estado == NAV_TERMINADO;
    r.ayuda = nav.estado == NAV_AYUDA || nav.ayudas > 0;
    r.esquivas = nav.esquivadas;
    {
        long ux = seno_x10(s.linea_rumbo * 10), uy = -seno_x10(s.linea_rumbo * 10 + 900);
        long rx = sim_x_mm(&sim) - s.linea_x0, ry = sim_y_mm(&sim) - s.linea_y0;
        long qx = nav.x_mm - s.linea_x0, qy = nav.y_mm - s.linea_y0;
        long fx = s.fin_x - s.linea_x0, fy = s.fin_y - s.linea_y0;
        int lat = (int)abs_l((rx * (-uy) + ry * ux) / 1000);
        int lat_est = (int)abs_l((qx * (-uy) + qy * ux) / 1000);
        int lon = (int)abs_l(((rx - fx) * ux + (ry - fy) * uy) / 1000);
        long dr = (sim_rumbo_cgrados(&sim) / 10 - s.linea_rumbo * 10) % 3600;

        if (dr < -1800) dr += 3600;
        if (dr > 1800) dr -= 3600;
        r.lateral = lat;
        r.longitudinal = lon;
        r.rumbo_x10 = (int)abs_l(dr);
        ex = sim_x_mm(&sim) - s.fin_x;
        ey = sim_y_mm(&sim) - s.fin_y;
        if (e <= E_RECTA200 || e >= E_ESQUIVE20) {
            r.error = lat;
            r.estimado = lat_est;
        } else {
            r.error = (int)raiz((long)ex * ex + (long)ey * ey);
            r.estimado = (int)raiz((long)(nav.x_mm - s.fin_x) * (nav.x_mm - s.fin_x) + (long)(nav.y_mm - s.fin_y) * (nav.y_mm - s.fin_y));
        }
    }
    return r;
}

typedef struct { int media, p50, p95, max; } Estad;

static int comparar(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static Estad estadistica(int *v, int n)
{
    Estad s;
    long suma = 0;
    int i;

    qsort(v, (size_t)n, sizeof(int), comparar);
    for (i = 0; i < n; i++) suma += v[i];
    s.media = (int)(suma / n);
    s.p50 = v[n / 2];
    s.p95 = v[(n * 95 + 99) / 100 - 1];
    s.max = v[n - 1];
    return s;
}

typedef struct { Estad error, lateral, longitudinal, rumbo, estimado; int llegan, toques, ayudas, esquivas; } Resumen;

static int *v_error, *v_lat, *v_lon, *v_rumbo, *v_est;

/* Calibración: qué fracción (por mil) de las corridas simuladas quedó por debajo de cada valor medido. `v` ya está ordenado. */
static void percentiles_de(const char *que, const int *v, int n, const int *medidos, int n_medidos)
{
    int i, j;

    printf("CALIBRACION %s:", que);
    for (j = 0; j < n_medidos; j++) {
        int debajo = 0;

        for (i = 0; i < n; i++) if (v[i] <= medidos[j]) debajo++;
        printf(" medido %d mm -> percentil %d", medidos[j], debajo * 100 / n);
    }
    printf("\n");
}

static Resumen monte_carlo(int e, int config, int n)
{
    Resumen s;
    int i;

    memset(&s, 0, sizeof s);
    for (i = 0; i < n; i++) {
        Resultado r = correr(e, config, 160000u + (uint32_t)e * 10007u + (uint32_t)i);   /* misma semilla en todas las configuraciones: corridas pareadas */

        if (getenv("J16_LISTAR")) printf("   corrida %d error=%d rumbo_x10=%d toco=%d\n", i, r.error, r.rumbo_x10, r.toco);
        v_error[i] = r.error; v_lat[i] = r.lateral; v_lon[i] = r.longitudinal; v_rumbo[i] = r.rumbo_x10; v_est[i] = abs_i(r.error - r.estimado);
        s.llegan += r.llego; s.toques += r.toco; s.ayudas += r.ayuda; s.esquivas += r.esquivas;
    }
    s.error = estadistica(v_error, n);
    s.lateral = estadistica(v_lat, n);
    s.longitudinal = estadistica(v_lon, n);
    s.rumbo = estadistica(v_rumbo, n);
    s.estimado = estadistica(v_est, n);
    return s;
}

static int tolerancia_p95(int antes)
{
    int a = antes + 10, b = antes * 120 / 100;

    return (a > b) ? a : b;
}

static void imprimir(int e, int config, int n, const Resumen *s)
{
    printf("J16 escenario=%s config=\"%s\" n=%d error_mm media=%d p50=%d p95=%d max=%d | lateral p95=%d | longitudinal media=%d p95=%d | rumbo_x10 media=%d p95=%d | real_vs_estimado p95=%d | llegan=%d toques=%d ayuda=%d esquivas=%d\n",
           NOMBRE_ESC[e], NOMBRE_CFG[config], n, s->error.media, s->error.p50, s->error.p95, s->error.max, s->lateral.p95, s->longitudinal.media, s->longitudinal.p95,
           s->rumbo.media, s->rumbo.p95, s->estimado.p95, s->llegan, s->toques, s->ayudas, s->esquivas);
}

int main(int argc, char **argv)
{
    int n = 150, e, c, calibrar = 0;
    static Resumen res[N_ESCENARIOS][N_CONFIG];

    if (argc > 4 && strcmp(argv[1], "depurar") == 0) {   /* depurar <escenario> <config> <corrida>: una sola corrida con su registro */
        Resultado r;

        depurar = getenv("J16_TODO") ? 2 : 1;
        e = atoi(argv[2]);
        c = atoi(argv[3]);
        r = correr(e, c, 160000u + (uint32_t)e * 10007u + (uint32_t)atoi(argv[4]));
        printf("%s %s: error=%d lateral=%d longitudinal=%d rumbo_x10=%d estimado=%d llego=%d toco=%d esquivas=%d ayuda=%d estado=%s pose_nav=(%d,%d,%d) real=(%d,%d) desbalance=%d escala=%d sesgo=%d lat_cmd=%d lat_sens=%d\n",
               NOMBRE_ESC[e], NOMBRE_CFG[c], r.error, r.lateral, r.longitudinal, r.rumbo_x10, r.estimado, r.llego, r.toco, r.esquivas, r.ayuda,
               navegador_texto_estado(nav.estado), nav.x_mm, nav.y_mm, nav.rumbo_grados, sim_x_mm(&sim), sim_y_mm(&sim), sim.desbalance_mil,
               sim.fw_escala_pct, sim.j16_sesgo_mdps, sim.j16_latencia_cmd_ticks, sim.j16_latencia_sens_ms);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "calibrar") == 0) {
        calibrar = 1;
        n = 1000;
    } else if (argc > 1) {
        n = atoi(argv[1]);
        if (n < 20) n = 20;
        if (n > 5000) n = 5000;
    }
    v_error = malloc(sizeof(int) * (size_t)n); v_lat = malloc(sizeof(int) * (size_t)n); v_lon = malloc(sizeof(int) * (size_t)n);
    v_rumbo = malloc(sizeof(int) * (size_t)n); v_est = malloc(sizeof(int) * (size_t)n);
    if (!v_error || !v_lat || !v_lon || !v_rumbo || !v_est) return EXIT_FAILURE;
    if (!calibrar) {
        parte_a();
        parte_b();
    }
    printf("-- Parte C: Monte Carlo (SIMULADO), %d corridas por escenario y configuracion --\n", n);
    for (e = 0; e < N_ESCENARIOS; e++) {
        for (c = 0; c < N_CONFIG; c++) {
            if (calibrar && c != C_J14) continue;
            res[e][c] = monte_carlo(e, c, n);
            imprimir(e, c, n, &res[e][c]);
            if (calibrar && e == E_RECTA150) {   /* B4 (J14): la "Recta 150 cm" hacía 160 cm; desvíos medidos 5.5, 15, 18 y 20 cm */
                static const int b4[4] = {55, 150, 180, 200};
                percentiles_de("B4 recta (160 cm en J14)", v_error, n, b4, 4);
            }
            if (calibrar && e == E_ESQUIVE20) {   /* B6-4: ~40 cm a la derecha tras una esquiva (una sola corrida, a ojo) */
                static const int b6[1] = {400};
                percentiles_de("B6-4 esquive", v_error, n, b6, 1);
            }
        }
        fflush(stdout);
    }
    if (!calibrar) {
        for (e = 0; e < N_ESCENARIOS; e++) {
            char t[160];
            const Resumen *antes = &res[e][C_J15], *sin_fw = &res[e][C_J16_SIN_FW], *rumbo = &res[e][C_J16_RUMBO];

            /* Tolerancias: la media no sube más de 5 mm; el p95 (más ruidoso con pocas corridas) no sube más de 10 mm o del 20 %, lo que sea mayor. */
            snprintf(t, sizeof t, "%s: sin firmware nuevo no empeora la media (+5 mm), el p95 (+10 mm o +20 %%), los toques ni las llegadas", NOMBRE_ESC[e]);
            caso(t, sin_fw->error.media <= antes->error.media + 5 && sin_fw->error.p95 <= tolerancia_p95(antes->error.p95) && sin_fw->toques <= antes->toques && sin_fw->llegan >= antes->llegan);
            snprintf(t, sizeof t, "%s: con RUMBO no empeora la media (+5 mm), el p95 (+10 mm o +20 %%), los toques ni las llegadas", NOMBRE_ESC[e]);
            caso(t, rumbo->error.media <= antes->error.media + 5 && rumbo->error.p95 <= tolerancia_p95(antes->error.p95) && rumbo->toques <= antes->toques && rumbo->llegan >= antes->llegan);
        }
        caso("rectas: con RUMBO la media del error lateral baja al menos a la mitad en 100, 150 y 200 cm",
             res[E_RECTA100][C_J16_RUMBO].error.media * 2 <= res[E_RECTA100][C_J15].error.media &&
             res[E_RECTA150][C_J16_RUMBO].error.media * 2 <= res[E_RECTA150][C_J15].error.media &&
             res[E_RECTA200][C_J16_RUMBO].error.media * 2 <= res[E_RECTA200][C_J15].error.media);
        caso("recta 150: el largo fijo se respeta (error longitudinal medio menor que el de las 8 celdas de antes)",
             res[E_RECTA150][C_J16_SIN_FW].longitudinal.media < res[E_RECTA150][C_J15].longitudinal.media);
        caso("esquives: con la holgura de 150 mm ninguna configuracion J16 toca la caja (y J15 no toca mas que antes)",
             res[E_ESQUIVE20][C_J16_SIN_FW].toques == 0 && res[E_ESQUIVE20][C_J16_RUMBO].toques == 0 &&
             res[E_ESQUIVE_ANCHA][C_J16_SIN_FW].toques == 0 && res[E_ESQUIVE_ANCHA][C_J16_RUMBO].toques == 0);
    }
    printf("%d casos, %d fallos\n", casos, fallos);
    free(v_error); free(v_lat); free(v_lon); free(v_rumbo); free(v_est);
    return fallos ? EXIT_FAILURE : EXIT_SUCCESS;
}
