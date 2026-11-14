/* Banco del modo autónomo v2: el navegador REAL, la fusión REAL y el clasificador de color REAL contra un mundo sintético (mundo_v2.c).
 *
 * TODO LO DE AQUÍ ES SIMULADO. Que el banco mejore no prueba que el carro mejore: falta la validación física
 * (docs/protocolo_prueba_fisica_autonomo_v2.md). Nunca usa imágenes reales.
 *
 * Qué hace cada corrida (igual que la app, ver ModoSemiautonomo.kt y CamaraObstaculos.kt):
 *   1. arma una escena (ruta, objetos, piso y luz) a partir de la semilla;
 *   2. «Aprender piso» con 12 cuadros sintéticos desde la salida y deja el piso fijo durante la ruta;
 *   3. cada 50 ms: cámara (cuadros a ~12 por segundo que llegan 110 a 170 ms tarde, 2 % perdidos) → obstaculos_analizar_modelo_celdas →
 *      fusion_decidir con lo último del detector neuronal (inferencia de 60 a 120 ms en su hilo) → navegador_tick; láser de cono cada SENS;
 *      el carro lo mueve simulador_navegador.c con el modelo físico J16 (AUTO 19.3 cm/s, pivotes medidos, RUMBO, latencias, ruido);
 *   4. en cada tick revisa el contacto del chasis con los objetos y al final mide dónde quedó.
 *
 * Uso:  ./pruebas_autonomo_v2                 prueba rápida (40 corridas por escenario) con las compuertas; la corre make test
 *       ./pruebas_autonomo_v2 1000            el informe de 1000 corridas por escenario (semillas 170000 + índice * 10007 + corrida)
 *       ./pruebas_autonomo_v2 1000 7          solo el escenario 7
 *       ./pruebas_autonomo_v2 depurar 9 12    una corrida (escenario 9, corrida 12) con su registro
 * Para comparar con versiones anteriores del núcleo: scripts/compuertas_autonomo.py (compila este mismo banco contra navegador.c, fusion.c y
 * obstaculos.c de v1.1 y de 6bea71e con -DAV2_BASE_V11 o -DAV2_BASE_6BEA).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "simulador_navegador.h"
#include "fusion.h"
#include "corredor.h"
#include "mundo_v2.h"

#if !defined(AV2_BASE_V11) && !defined(AV2_BASE_6BEA)
#define AV2_ACTUAL 1
#endif
#ifndef AV2_BASE_V11
#define AV2_FUENTES 1      /* el resultado de visión lleva la procedencia por zona (color / red) */
#endif

enum { T_FALSO, T_OBSTACULO, T_IMPOSIBLE, T_LIMITE };
enum {
    E_LIBRE, E_FP_JUNTAS, E_FP_SOL, E_FP_SOMBRA, E_FP_REFLEJO, E_FP_EXPOSICION, E_FP_DETECTOR,
    E_CAJA20, E_CAJA20_LADO, E_BANQUITO, E_LARGO90, E_SILLA, E_PARED_FINAL, E_DOS, E_PASILLO, E_APARECE, E_BAJO,
    E_CAJA20_SOL, E_BATERIA, E_PARED_FRENTE, E_BAJO_SOLO_COLOR,
    E_CAJA_PARED, E_ESQUINA, N_ESC   /* añadidos tras la revisión independiente: van al final para no mover las semillas de los anteriores */
};
static const struct { const char *nombre; int tipo; } ESC[N_ESC] = {
    { "libre_limpio", T_FALSO }, { "falso_juntas", T_FALSO }, { "falso_sol", T_FALSO }, { "falso_sombra", T_FALSO },
    { "falso_reflejo", T_FALSO }, { "falso_exposicion", T_FALSO }, { "falso_detector", T_FALSO },
    { "caja20", T_OBSTACULO }, { "caja20_lateral", T_OBSTACULO }, { "banquito35x45", T_OBSTACULO }, { "objeto_largo90", T_OBSTACULO },
    { "silla_patas", T_OBSTACULO }, { "pared_tras_el_final", T_OBSTACULO }, { "dos_obstaculos", T_OBSTACULO }, { "pasillo", T_OBSTACULO },
    { "aparece_de_repente", T_OBSTACULO }, { "objeto_bajo", T_OBSTACULO }, { "caja20_con_sol", T_OBSTACULO }, { "bateria_baja", T_OBSTACULO },
    { "pared_al_frente", T_IMPOSIBLE }, { "bajo_solo_color", T_LIMITE },
    { "caja_y_pared_al_lado", T_OBSTACULO }, { "caja_antes_de_esquina", T_OBSTACULO }
};

static Navegador nav;
static Simulador sim;
static Mundo mundo;
static int casos, fallos, depurar;
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

static double unif(double a, double b) { return a + (b - a) * (entre(0, 1000000) / 1000000.0); }

static int normal_x10(int sigma_x10, int tope)
{
    int i, suma = 0;

    for (i = 0; i < 12; i++) suma += entre(-500, 500);
    suma = suma * sigma_x10 / 1000;
    return suma < -tope ? -tope : (suma > tope ? tope : suma);
}

/* ── Escenas ───────────────────────────────────────────────────────────── */

typedef struct {
    Punto ruta[40];
    int n;
    int tipo;
    double x0, y0;            /* salida (mm) */
    double fin_x, fin_y;      /* final de la ruta */
    double ux, uy;            /* hacia dónde va el último tramo */
    int sag_pct;              /* cuánto cae la velocidad a lo largo de la corrida (batería) */
    int aparece_obj;          /* objeto que aparece de repente (-1 = ninguno) y a qué distancia del frente del carro */
    double aparece_mm;
    int fondo_medible;        /* 1 = hay una pared lejana que el láser alcanza al final */
} Escena;

static void ruta_recta(Escena *s, int celdas)
{
    int i;

    s->n = celdas + 1;
    for (i = 0; i <= celdas; i++) { s->ruta[i].columna = 5; s->ruta[i].fila = celdas - i; }
    s->x0 = 1000.0; s->y0 = celdas * 200.0;
    s->fin_x = 1000.0; s->fin_y = 0.0;
    s->ux = 0.0; s->uy = -1.0;
}

/* Ruta en L: `celdas` hacia -y y luego `despues` hacia +x (signo > 0) o -x. */
static void ruta_ele(Escena *s, int celdas, int despues, int signo)
{
    int i;

    s->n = celdas + 1 + despues;
    for (i = 0; i <= celdas; i++) { s->ruta[i].columna = 5; s->ruta[i].fila = celdas - i; }
    for (i = 1; i <= despues; i++) { s->ruta[celdas + i].columna = 5 + signo * i; s->ruta[celdas + i].fila = 0; }
    s->x0 = 1000.0; s->y0 = celdas * 200.0;
    s->fin_x = 1000.0 + signo * despues * 200.0; s->fin_y = 0.0;
    s->ux = (double)signo; s->uy = 0.0;
}

/* Luz y piso "de todos los días": juntas normales y, a veces, una sombra suave o algo de brillo. */
static void luz_normal(void)
{
    mundo.foto.loseta_mm = unif(300.0, 450.0);
    mundo.foto.junta_mm = unif(3.0, 5.0);
    mundo.foto.junta_factor = unif(0.78, 0.92);
    mundo.foto.rot_rad = unif(-0.08, 0.08);
    mundo.foto.off_x = unif(0.0, 400.0);
    mundo.foto.off_y = unif(0.0, 400.0);
    mundo.foto.albedo[0] = unif(0.62, 0.78);
    mundo.foto.albedo[1] = mundo.foto.albedo[0] * unif(0.95, 1.0);
    mundo.foto.albedo[2] = mundo.foto.albedo[0] * unif(0.88, 0.97);
    if (entre(0, 99) < 30) mv_parche(&mundo, 1000.0 + unif(-500.0, 500.0), unif(300.0, 1500.0), unif(150.0, 400.0), unif(150.0, 400.0), unif(0.0, 90.0), unif(0.80, 0.92), 80.0);
}

static void armar_escena(int e, Escena *s)
{
    double d, lado;

    memset(s, 0, sizeof *s);
    s->tipo = ESC[e].tipo;
    s->aparece_obj = -1;
    ruta_recta(s, 10);
    luz_normal();
    switch (e) {
    case E_LIBRE:
        break;
    case E_FP_JUNTAS:        /* juntas anchas y oscuras */
        mundo.foto.loseta_mm = unif(280.0, 340.0);
        mundo.foto.junta_mm = unif(5.0, 8.0);
        mundo.foto.junta_factor = unif(0.40, 0.60);
        break;
    case E_FP_SOL:           /* una franja de sol que cruza la ruta (y a veces otra) */
        mv_parche(&mundo, 1000.0 + unif(-150.0, 150.0), unif(700.0, 1300.0), unif(500.0, 900.0), unif(150.0, 300.0), unif(-25.0, 25.0), unif(2.0, 3.5), unif(8.0, 25.0));
        if (entre(0, 1)) mv_parche(&mundo, 1000.0 + unif(-300.0, 300.0), unif(200.0, 500.0), unif(200.0, 400.0), unif(100.0, 200.0), unif(-40.0, 40.0), unif(1.8, 3.0), 15.0);
        break;
    case E_FP_SOMBRA:        /* sombras de muebles y la del propio carro */
        mv_parche(&mundo, 1000.0 + unif(-150.0, 150.0), unif(700.0, 1300.0), unif(400.0, 800.0), unif(150.0, 350.0), unif(-30.0, 30.0), unif(0.45, 0.65), unif(20.0, 60.0));
        mundo.foto.sombra_propia = entre(0, 1) ? unif(0.55, 0.75) : 1.0;
        break;
    case E_FP_REFLEJO:       /* piso brillante: se refleja una ventana que está al frente */
        mundo.foto.reflejo = 1;
        mundo.foto.brillo_piso = unif(0.5, 1.0);
        mundo.foto.vent_x = 1000.0 + unif(-400.0, 400.0);
        mundo.foto.vent_y = -unif(1500.0, 3000.0);
        mundo.foto.vent_nx = 0.0; mundo.foto.vent_ny = 1.0;
        mundo.foto.vent_semiancho = unif(500.0, 1000.0);
        mundo.foto.vent_z0 = unif(700.0, 1000.0);
        mundo.foto.vent_z1 = mundo.foto.vent_z0 + unif(900.0, 1300.0);
        mundo.foto.vent_brillo = unif(1.5, 3.0);
        break;
    case E_FP_EXPOSICION:    /* la cámara cambia de exposición de golpe a media ruta */
        mundo.foto.escalon_ganancia = entre(0, 1) ? unif(0.55, 0.78) : unif(1.28, 1.7);
        mundo.foto.escalon_ms = (uint32_t)entre(3000, 8000);
        break;
    case E_FP_DETECTOR:      /* el detector neuronal inventa cajas a cada rato */
        mundo.det_falsos_por_inferencia = unif(0.10, 0.25);
        break;
    case E_CAJA20:
    case E_CAJA20_SOL:
    case E_BATERIA:
        d = unif(900.0, 1200.0);
        mv_caja(&mundo, "caja20", 1000.0 + unif(-30.0, 30.0), s->y0 - d - 100.0, 200.0, 200.0, 200.0, 150, 110, 70, 0.5, 0.03, 72, 0);
        if (e == E_CAJA20_SOL) {
            mv_parche(&mundo, 1000.0 + unif(-150.0, 150.0), s->y0 - unif(300.0, 700.0), unif(500.0, 900.0), unif(120.0, 250.0), unif(-25.0, 25.0), unif(2.0, 3.5), 15.0);
        }
        if (e == E_BATERIA) s->sag_pct = entre(8, 18);
        break;
    case E_CAJA20_LADO:      /* la misma caja corrida de lado: todavía estorba al chasis (medio carro = 100 mm) */
        d = unif(900.0, 1200.0);
        lado = unif(90.0, 190.0) * (entre(0, 1) ? 1.0 : -1.0);
        mv_caja(&mundo, "caja20", 1000.0 + lado, s->y0 - d - 100.0, 200.0, 200.0, 200.0, 150, 110, 70, 0.5, 0.03, 72, 0);
        break;
    case E_BANQUITO:         /* 35 x 45 cm con patas; el asiento queda arriba del haz del láser */
        d = unif(800.0, 1100.0);
        mv_mueble(&mundo, "banquito", 1000.0 + unif(-30.0, 30.0), s->y0 - d - 225.0, 350.0, 450.0, 35.0, unif(280.0, 400.0), 0.0, 140, 95, 55, unif(0.45, 0.70), 13);
        break;
    case E_LARGO90:          /* objeto macizo de 90 cm de fondo */
        ruta_recta(s, 14);
        d = unif(800.0, 1100.0);
        mv_caja(&mundo, "largo90", 1000.0 + unif(-30.0, 30.0), s->y0 - d - 450.0, 300.0, 900.0, 250.0, 90, 100, 130, 0.5, 0.05, 72, 0);
        break;
    case E_SILLA:            /* silla de patas delgadas con respaldo */
        d = unif(800.0, 1100.0);
        mv_mueble(&mundo, "silla", 1000.0 + unif(-40.0, 40.0), s->y0 - d - 210.0, 400.0, 420.0, 25.0, unif(420.0, 460.0), 400.0, 60, 60, 70, unif(0.70, 0.90), 61);
        break;
    case E_PARED_FINAL:      /* la ruta termina a 45-65 cm de una pared: debe terminar sin tocarla ni rodearla */
        ruta_recta(s, 7);
        d = unif(450.0, 650.0);
        {
            int o = mv_objeto(&mundo, "pared", 0.0, 0, 0, 0);

            mv_prisma(&mundo, o, -1500.0, -d - 120.0 - 100.0, 3500.0, -d - 120.0, 0.0, 1500.0, 215, 210, 200, 0.6);
        }
        break;
    case E_DOS:              /* dos cajas seguidas */
        ruta_recta(s, 15);
        d = unif(700.0, 900.0);
        mv_caja(&mundo, "caja_a", 1000.0 + unif(-30.0, 30.0), s->y0 - d - 100.0, 200.0, 200.0, 200.0, 150, 110, 70, 0.5, 0.03, 72, 0);
        d = unif(1750.0, 2000.0);
        mv_caja(&mundo, "caja_b", 1000.0 + unif(-60.0, 60.0), s->y0 - d - 100.0, 200.0, 200.0, 200.0, 70, 120, 80, 0.5, 0.03, 72, 0);
        break;
    case E_PASILLO:          /* paredes a los lados con margen suficiente para rodear */
        d = unif(900.0, 1100.0);
        mv_caja(&mundo, "caja20", 1000.0 + unif(-30.0, 30.0), s->y0 - d - 100.0, 200.0, 200.0, 200.0, 150, 110, 70, 0.5, 0.03, 72, 0);
        {
            int o = mv_objeto(&mundo, "paredes", 0.0, 0, 0, 0);
            double w = unif(780.0, 900.0);

            mv_prisma(&mundo, o, 1000.0 - w - 100.0, -800.0, 1000.0 - w, s->y0 + 600.0, 0.0, 1500.0, 215, 210, 200, 0.6);
            mv_prisma(&mundo, o, 1000.0 + w, -800.0, 1000.0 + w + 100.0, s->y0 + 600.0, 0.0, 1500.0, 215, 210, 200, 0.6);
        }
        break;
    case E_APARECE:          /* alguien deja una caja delante del carro en marcha */
        d = unif(900.0, 1300.0);
        s->aparece_obj = mv_caja(&mundo, "caja_sorpresa", 1000.0 + unif(-30.0, 30.0), s->y0 - d - 100.0, 200.0, 200.0, 200.0, 150, 110, 70, 0.5, 0.03, 72, 0xFFFFFFFFu);
        s->aparece_mm = unif(350.0, 600.0);
        break;
    case E_BAJO:             /* objeto plano (1.5 cm) bajo el haz del láser: lo ven el color y (a veces) la red, el láser no */
        d = unif(900.0, 1200.0);
        mv_caja(&mundo, "plano", 1000.0 + unif(-30.0, 30.0), s->y0 - d - 125.0, 300.0, 250.0, 15.0, 40, 45, 95, 0.3, unif(0.45, 0.70), 73, 0);
        break;
    case E_PARED_FRENTE:     /* pared que cierra el paso: no hay por dónde. Debe parar sin tocar y pedir ayuda */
        d = unif(900.0, 1200.0);
        {
            int o = mv_objeto(&mundo, "pared", 0.0, 0, 0, 1);

            mv_prisma(&mundo, o, -1500.0, s->y0 - d - 100.0, 3500.0, s->y0 - d, 0.0, 1500.0, 215, 210, 200, 0.6);
        }
        break;
    case E_BAJO_SOLO_COLOR:  /* el mismo objeto plano pero de una clase que la red NO conoce: límite conocido de la percepción */
        d = unif(900.0, 1200.0);
        mv_caja(&mundo, "plano_desconocido", 1000.0 + unif(-30.0, 30.0), s->y0 - d - 125.0, 300.0, 250.0, 15.0, 40, 45, 95, 0.3, 0.0, 0, 0);
        break;
    case E_CAJA_PARED:       /* (revisión) la caja de siempre con una pared larga pegada a UN lado (25 a 44 cm de la línea): por ese lado no cabe el rodeo ni la vuelta */
        d = unif(900.0, 1200.0);
        mv_caja(&mundo, "caja20", 1000.0 + unif(-30.0, 30.0), s->y0 - d - 100.0, 200.0, 200.0, 200.0, 150, 110, 70, 0.5, 0.03, 72, 0);
        {
            int o = mv_objeto(&mundo, "pared_lateral", 0.0, 0, 0, 0);
            double w = unif(250.0, 440.0);

            if (entre(0, 1)) mv_prisma(&mundo, o, 1000.0 + w, -800.0, 1000.0 + w + 100.0, s->y0 + 600.0, 0.0, 1500.0, 215, 210, 200, 0.6);
            else             mv_prisma(&mundo, o, 1000.0 - w - 100.0, -800.0, 1000.0 - w, s->y0 + 600.0, 0.0, 1500.0, 215, 210, 200, 0.6);
        }
        break;
    default:                 /* E_ESQUINA (revisión): ruta en L con la caja 40 a 60 cm antes de la esquina: el paso en paralelo no puede seguir de frente más allá */
        {
            int o;

            ruta_ele(s, 8, 6, entre(0, 1) ? 1 : -1);
            d = unif(400.0, 600.0);              /* de la cara de la caja a la esquina (que está en y = 0) */
            o = mv_caja(&mundo, "caja20", 1000.0 + unif(-30.0, 30.0), d + 100.0, 200.0, 200.0, 200.0, 150, 110, 70, 0.5, 0.03, 72, 0);
            mundo.objetos[o].en_ruta = 0;        /* «haberla dejado atrás» aquí es llegar al final, que está después de la esquina */
        }
        break;
    }
    s->fondo_medible = 0;
}

/* ── Una corrida ───────────────────────────────────────────────────────── */

typedef struct {
    int exito, llego, contacto, ayuda, error, causa, terminado_falso, esquivas, paradas, sin_terminar;
    int tiempo_ms, lateral, longitudinal, desvio_max, cola_mm, piso_aprendido, cuadros_color, cuadros_coinc;
    int bajo_mueble;   /* entró en la planta de un mueble (pasó por debajo, entre las patas) sin tocar nada físico */
    int vistazos, vistazos_con_eco;
} Resultado;

#define COLA_CUADROS 16
typedef struct { uint32_t llega_ms; int usado; unsigned char celdas[MV_FILAS * MV_COLUMNAS * 3]; MvPose pose; uint32_t captura_ms; } CuadroEnViaje;

static MvPose pose_real(void)
{
    MvPose p;

    p.x = sim.x_cmm / 100.0;
    p.y = sim.y_cmm / 100.0;
    p.rumbo_rad = sim_rumbo_cgrados(&sim) * 3.14159265358979323846 / 18000.0;
    return p;
}

static void celdas_a_imagen(const unsigned char *celdas, unsigned char *rgb)
{
    int f, c, y, x;

    for (f = 0; f < MV_FILAS; f++) for (y = 0; y < 10; y++) for (c = 0; c < MV_COLUMNAS; c++) for (x = 0; x < 10; x++) {
        memcpy(rgb + ((f * 10 + y) * 240 + c * 10 + x) * 3, celdas + (f * MV_COLUMNAS + c) * 3, 3);
    }
}

static void volcar_diag(void)
{
    char linea[NAV_DIAG_LARGO];

    while (navegador_diag_siguiente(&nav, linea, sizeof linea)) {
        if (depurar > 1 || (depurar && strncmp(linea, "RUTA_DIAG envia", 15) != 0 && strncmp(linea, "POSE_DIAG", 9) != 0)) {
            printf("   [%6u ms real=(%d,%d,%d)] %s\n", (unsigned)sim.t_ms, sim_x_mm(&sim), sim_y_mm(&sim), sim_rumbo_grados(&sim), linea);
        }
    }
}

/* Opciones del banco para estudiar variantes (por variables de entorno, ver main). */
static int op_umbral = 0, op_celdas = 0, op_aec_fijo = 0;

static Resultado correr(int e, uint32_t semilla)
{
    static unsigned char imagen[240 * 170 * 3];
    static CuadroEnViaje cola[COLA_CUADROS];
    Escena s;
    CalibracionCarro cal;
    Resultado r;
    ConfigObstaculos cfg_color;
    ConfigFusion cfg_fusion;
    ModeloPiso piso;
    DeteccionesModelo det_listo, det_en_curso;
    ResultadoObstaculos cuadro;
    uint32_t prox_captura = 0, det_fin_ms = 0;
    int estado_fin = 0, causa_fin = 0;
    int det_ocupado = 0, hay_det = 0;
    int i, t, a, d, zm, delta_ppm, tope_ms, base_avance, cmd_antes = NAV_CMD_STOP;
    double ux, uy;

    memset(&r, 0, sizeof r);
    memset(cola, 0, sizeof cola);
    memset(&det_listo, 0, sizeof det_listo);
    memset(&det_en_curso, 0, sizeof det_en_curso);
    memset(&cuadro, 0, sizeof cuadro);
    azar = semilla;
    mv_iniciar(&mundo, semilla);
    armar_escena(e, &s);
    if (op_aec_fijo) { mundo.foto.aec_auto = 0; mundo.foto.awb_auto = 0; }

    /* Lo que el navegador cree: la calibración del carro (la de J15/J16). */
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
    navegador_poner_calibracion(&nav, &cal);
    navegador_poner_giroscopio(&nav, 1);
    navegador_poner_sensor_distancia(&nav, 1);
    navegador_poner_autoridad_laser(&nav, 1);
    navegador_poner_requiere_camara(&nav, 1);
    navegador_poner_pedir_ayuda(&nav, 1);
    navegador_poner_latencia_visual(&nav, 320);
    navegador_poner_frenar_lejos_centro(&nav, 1);
    navegador_poner_rumbo_fw(&nav, 1);
    navegador_cargar_ruta(&nav, s.ruta, s.n);

    /* El carro real: el modelo físico J16 con sus dispersiones (pruebas_precision_j16.c). */
    sim_iniciar(&sim, s.ruta[0].columna, s.ruta[0].fila, 0);
    sim_sembrar_azar_fw(semilla);   /* cada corrida con su propio ruido de giro: «depurar» repite la corrida de la tanda */
#ifdef NAV_PRECAUCION_NUNCA
    if (getenv("AV2_PRECAUCION")) navegador_poner_precaucion(&nav, atoi(getenv("AV2_PRECAUCION")));   /* 0 nunca (por defecto), 1 color cerca, 2 siempre */
    if (getenv("AV2_VISTAZO")) navegador_poner_vistazo(&nav, atoi(getenv("AV2_VISTAZO")));           /* 0 no, 1 lejos, 2 lejos y medio, 3 todos */
#endif
    if (depurar) {   /* dónde quedó cada cosa en esta corrida: sin esto una traza no se puede leer */
        for (i = 0; i < mundo.n_objetos; i++) {
            printf("MUNDO %s planta=(x %.0f..%.0f, y %.0f..%.0f) en_ruta=%d aparece_ms=%lu\n", mundo.objetos[i].nombre, mundo.objetos[i].bx0, mundo.objetos[i].bx1,
                   mundo.objetos[i].by0, mundo.objetos[i].by1, mundo.objetos[i].en_ruta, (unsigned long)mundo.objetos[i].aparece_ms);
        }
    }
    sim_j16_activar(&sim);
    sim.real = cal;
    base_avance = entre(96, 104);
    sim.pct_avance = base_avance;
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
    delta_ppm = entre(3000, 21000);
    {
        long long b = (long long)(210 - zm) * (2000000LL + delta_ppm), c = (long long)(207 - zm) * (2000000LL - delta_ppm);

        sim.desbalance_mil = (int)((b * 1000 + c / 2) / c);
    }
    sim.j16_sesgo_mdps = entre(-50, 50);
    sim.j16_latencia_sens_ms = entre(50, 150);
    sim.j16_latencia_cmd_ticks = entre(0, 2);
    sim.rumbo_cgrados += entre(-100, 100);
    sim.rumbo0_cgrados = sim.rumbo_cgrados;
    sim.x_cmm += (long)entre(-10, 10) * 100;
    sim.fw_giroscopio = 1;
    sim.tof_activo = 1;
    sim.laser_externo = 1;
    sim.periodo_cuadro_ms = 1000000;
    sim.j16_rumbo = 1;
    sim.fw_escala_pct = entre(-1, 1);
#ifdef SIM_REALISMO_V2
    sim.v2_azar = semilla ^ 0x5EEDu;
    sim.v2_frenado_giro_pct = 100;
    sim.v2_jitter_cmd_pct = 30;        /* 30 % de los comandos llegan un tick antes o después */
    sim.v2_giro_tiempo_real = 1;       /* un giro por tiempo arranca antes de lo calibrado y sigue girando más al frenar */
    sim.v2_sens_coherente = 1;         /* el ángulo y la distancia de SENS son del mismo instante (la distancia, ~50 ms más vieja) */
#endif

    /* «Aprender piso»: la cámara se asienta y se suman 12 cuadros con el carro quieto. */
    obstaculos_config_defecto(&cfg_color);
    cfg_color.grupos_piso = 3;         /* lo que fija la app (ConfigVision.kt): K = 3 y franja de referencia de 600 milésimas */
    cfg_color.ref_ancho_mil = 600;
    if (op_umbral > 0) cfg_color.umbral_color = op_umbral;
    if (op_celdas > 0) cfg_color.min_celdas = op_celdas;
    fusion_config_defecto(&cfg_fusion);
    obstaculos_modelo_iniciar(&piso);
    {
        static AcumuladorPiso acum;
        InformePiso informe;
        unsigned char celdas[MV_FILAS * MV_COLUMNAS * 3];

        for (i = 0; i < 25; i++) mv_render_celdas(&mundo, pose_real(), 0, celdas);
        obstaculos_aprender_iniciar(&acum);
        for (i = 0; i < 12; i++) {
            mv_render_celdas(&mundo, pose_real(), 0, celdas);
            celdas_a_imagen(celdas, imagen);
            obstaculos_aprender_cuadro(&acum, &cfg_color, imagen, 240, 170);
        }
        /* La app rechaza el aprendizaje si sale «muy variado» o descarta más del 5 % (EjecutarRutaFragment.evaluarPiso): aquí se anota y se sigue. */
        r.piso_aprendido = (obstaculos_aprender_terminar(&acum, &cfg_color, &piso, &informe) == OBS_OK) && !informe.muy_variado && informe.descartado_pct <= 5;
    }
    cfg_color.piso_fijo = 1;

    navegador_comenzar(&nav, 0);
    tope_ms = 150000;
    ux = s.ux; uy = s.uy;  /* hacia dónde va el último tramo (casi todas las rutas del banco van hacia -y) */
    for (t = 0; t < tope_ms; t += NAV_TICK_MS) {
        MvPose p = pose_real();
        const CuadroEnViaje *llega = NULL;
        int mm, valido;

        /* Objeto que aparece de repente cuando el frente del carro está a la distancia sorteada. */
        if (s.aparece_obj >= 0 && mundo.objetos[s.aparece_obj].aparece_ms == 0xFFFFFFFFu) {
            double libre = (p.y - 120.0) - mundo.objetos[s.aparece_obj].by1;

            if (libre <= s.aparece_mm) mundo.objetos[s.aparece_obj].aparece_ms = (uint32_t)t;
        }
        /* Cámara: captura a ~12 cuadros por segundo; cada cuadro llega al teléfono 110 a 170 ms después (2 % se pierden). */
        if ((uint32_t)t >= prox_captura) {
            int libre = -1;

            for (i = 0; i < COLA_CUADROS; i++) if (!cola[i].usado) { libre = i; break; }
            if (libre >= 0) {
                mv_render_celdas(&mundo, p, (uint32_t)t, cola[libre].celdas);
                cola[libre].pose = p;
                cola[libre].captura_ms = (uint32_t)t;
                cola[libre].llega_ms = (uint32_t)t + (uint32_t)(110 + (int)(60.0 * mv_uniforme(&mundo)));
                cola[libre].usado = (mv_uniforme(&mundo) >= 0.02);
            }
            prox_captura = (uint32_t)t + 70u + (uint32_t)(25.0 * mv_uniforme(&mundo));
        }
        for (i = 0; i < COLA_CUADROS; i++) {   /* el más nuevo de los que ya llegaron; los demás se descartan (buzón de último cuadro) */
            if (cola[i].usado && cola[i].llega_ms <= (uint32_t)t) {
                if (llega == NULL || cola[i].captura_ms > llega->captura_ms) {
                    if (llega != NULL) ((CuadroEnViaje *)llega)->usado = 0;
                    llega = &cola[i];
                } else {
                    cola[i].usado = 0;
                }
            }
        }
        /* Hilo del detector neuronal: toma el cuadro que llega si está libre; su resultado sale 60 a 120 ms después. */
        if (det_ocupado && (uint32_t)t >= det_fin_ms) {
            det_listo = det_en_curso;
            hay_det = 1;
            det_ocupado = 0;
        }
        sim.proximo_cuadro_ms = sim.t_ms + 1000000u;
        if (llega != NULL) {
            ResultadoObstaculos color;
            ResultadoFusion fusion;
            int z;

            if (!det_ocupado) {
                det_fin_ms = (uint32_t)t + (uint32_t)(60 + (int)(60.0 * mv_uniforme(&mundo)));
                mv_detector(&mundo, llega->pose, llega->captura_ms, det_fin_ms, &det_en_curso);
                det_ocupado = 1;
            }
            if (obstaculos_analizar_modelo_celdas(&cfg_color, &piso, llega->celdas, MV_COLUMNAS, MV_FILAS, nav.cmd_actual == NAV_CMD_FORWARD, (unsigned int)t, &color) == OBS_OK) {
                fusion_decidir(&cfg_fusion, &color, hay_det ? &det_listo : NULL, (unsigned int)t, &fusion);
                cuadro = color;
                for (z = 0; z < OBS_ZONAS; z++) {
                    int celdas_color = color.zona[z].celdas;

                    cuadro.zona[z] = fusion.zona[z];
                    cuadro.zona[z].celdas = celdas_color;
#ifdef AV2_FUENTES
                    cuadro.fuentes_vision[z] = fusion.fuentes_vision[z];
                    if (fusion.fuentes_vision[z] == 3) r.cuadros_coinc++;
#endif
                    if (color.zona[z].hay_obstaculo) r.cuadros_color++;
                }
                sim.vision_externa = &cuadro;
                sim.proximo_cuadro_ms = sim.t_ms;   /* este tick el navegador recibe un cuadro nuevo */
                if (depurar > 1) {
                    printf("   [%6d ms] cuadro color=(%d/%d/%d %d/%d/%d %d/%d/%d) fusion=(%d%d%d) det_n=%d brillo=%d motivo=%d\n", t,
                           color.zona[0].hay_obstaculo, color.zona[0].cercania, color.zona[0].celdas, color.zona[1].hay_obstaculo, color.zona[1].cercania,
                           color.zona[1].celdas, color.zona[2].hay_obstaculo, color.zona[2].cercania, color.zona[2].celdas,
                           fusion.zona[0].hay_obstaculo, fusion.zona[1].hay_obstaculo, fusion.zona[2].hay_obstaculo, hay_det ? det_listo.n : -1, color.brillo_medio, color.motivo);
                }
            }
            ((CuadroEnViaje *)llega)->usado = 0;
        }
        /* Láser. */
        valido = mv_laser(&mundo, p, (uint32_t)t, &mm);
        sim.laser_externo_mm = mm;
        sim.tof_sin_eco = !valido;
        sim.fw_error_izq_x10 = normal_x10(11, 30);
        sim.fw_error_der_x10 = normal_x10(17, 30);
        if (s.sag_pct > 0) sim.pct_avance = base_avance - s.sag_pct * (t > 40000 ? 40000 : t) / 40000;

        sim_correr(&sim, &nav, NAV_TICK_MS);
        volcar_diag();
        if (depurar > 1 && (t % 500) == 0) {
            printf("   [%6d ms] real=(%.0f,%.0f,%.1f) nav=(%d,%d,%d) estado=%s cmd=%s laser=%d%s\n", t, p.x, p.y, p.rumbo_rad * 57.29578, nav.x_mm, nav.y_mm, nav.rumbo_grados,
                   navegador_texto_estado(nav.estado), navegador_texto_comando(nav.cmd_actual), mm, valido ? "" : " (sin eco)");
        }
        /* Paradas intermedias: FORWARD → STOP siguiendo la ruta, sin estar cerrándola. */
        if (cmd_antes == NAV_CMD_FORWARD && nav.cmd_actual == NAV_CMD_STOP && nav.estado != NAV_TERMINADO
#ifdef AV2_FUENTES
            && !nav.fin_pendiente
#endif
            && (nav.estado == NAV_SIGUIENDO || nav.estado == NAV_DETENIDO_OBSTACULO)) {
            r.paradas++;
        }
        cmd_antes = nav.cmd_actual;
        p = pose_real();
        {
            int objeto = -1;

            int tipo = r.contacto ? 0 : mv_contacto_tipo(&mundo, p, (uint32_t)t, &objeto);

            if (tipo == 1) {
                r.contacto = 1;
                if (depurar) printf("   [%6d ms] CONTACTO con %s real=(%.0f,%.0f,%.1f) estado=%s fase=%d reinc=%d cmd=%s\n", t, mundo.objetos[objeto].nombre, p.x, p.y,
                                    p.rumbo_rad * 57.29578, navegador_texto_estado(nav.estado), nav.fase, nav.reinc_fase, navegador_texto_comando(nav.cmd_actual));
            } else if (tipo == 2 && !r.bajo_mueble) {
                r.bajo_mueble = 1;
                if (depurar) printf("   [%6d ms] BAJO EL MUEBLE %s (entra en su planta sin tocar nada) real=(%.0f,%.0f,%.1f) estado=%s cmd=%s\n", t,
                                    mundo.objetos[objeto].nombre, p.x, p.y, p.rumbo_rad * 57.29578, navegador_texto_estado(nav.estado), navegador_texto_comando(nav.cmd_actual));
            }
        }
        {
            int desvio = (int)fabs(p.x - s.x0);

            if (desvio > r.desvio_max) r.desvio_max = desvio;
        }
        if (nav.estado == NAV_TERMINADO || nav.estado == NAV_ERROR || nav.estado == NAV_AYUDA) break;
    }
    r.tiempo_ms = t;
    r.sin_terminar = (t >= tope_ms);
    /* Cómo terminó, ANTES de dejar que el carro acabe de frenar: en esos 600 ms el banco ya no manda cuadros y el navegador, con razón, acaba dando «sin video»
     * (ERROR, causa 5). Eso es del banco, no de la corrida: se anotaban como ERROR unas pocas corridas que habían terminado pidiendo ayuda. */
    estado_fin = nav.estado;
    causa_fin = nav.error_causa;
    sim_correr(&sim, &nav, 600);   /* que termine de frenar */
    volcar_diag();
    {
        MvPose p = pose_real();
        double fx = sin(p.rumbo_rad), fy = -cos(p.rumbo_rad);
        double cola_s = (p.x - fx * 120.0 - s.fin_x) * ux + (p.y - fy * 120.0 - s.fin_y) * uy;
        int o, pasados = 1;

        if (!r.contacto) {
            int tipo = mv_contacto_tipo(&mundo, p, (uint32_t)t, NULL);

            if (tipo == 1) r.contacto = 1;
            else if (tipo == 2) r.bajo_mueble = 1;
        }
#ifdef NAV_V2
        r.vistazos = nav.vistazos;
        r.vistazos_con_eco = nav.vistazos_con_eco;
#endif
        r.cola_mm = 99999;
        for (o = 0; o < mundo.n_objetos; o++) {
            if (mundo.objetos[o].en_ruta && mundo.objetos[o].aparece_ms != 0xFFFFFFFFu) {
                /* la cara lejana del objeto a lo largo de la ruta (hacia -y es la de menor y) */
                double lejos_s = (0.0) * ux + (mundo.objetos[o].by0 - s.fin_y) * uy;
                int margen = (int)(cola_s - lejos_s);

                if (margen < r.cola_mm) r.cola_mm = margen;
                if (margen <= 0) pasados = 0;
            }
        }
        r.llego = (estado_fin == NAV_TERMINADO);
        r.ayuda = (estado_fin == NAV_AYUDA || nav.ayudas > 0);
        r.error = (estado_fin == NAV_ERROR);
        r.causa = causa_fin;
        r.esquivas = nav.esquivadas;
        r.lateral = (int)fabs((p.x - s.fin_x) * (-uy) + (p.y - s.fin_y) * ux);
        r.longitudinal = (int)fabs((p.x - s.fin_x) * ux + (p.y - s.fin_y) * uy);
        r.terminado_falso = r.llego && !pasados;
        switch (s.tipo) {
        /* Pasar por debajo de un mueble sin tocarlo NO cuenta como éxito (el carro debía rodearlo), pero se lleva aparte: no es un contacto. */
        case T_FALSO:     r.exito = r.llego && !r.contacto && nav.esquivadas == 0 && nav.ayudas == 0; break;
        case T_IMPOSIBLE: r.exito = !r.contacto && estado_fin == NAV_AYUDA; break;
        default:          r.exito = r.llego && !r.contacto && !r.bajo_mueble && pasados; break;
        }
        if (depurar) {
            printf("FIN %s semilla=%u estado=%s causa=%d exito=%d contacto=%d esquivas=%d ayudas=%d paradas=%d t=%d ms real=(%.0f,%.0f,%.1f) nav=(%d,%d,%d) lateral=%d long=%d cola=%d piso_aprendido=%d\n",
                   ESC[e].nombre, (unsigned)semilla, navegador_texto_estado((EstadoNav)estado_fin), causa_fin, r.exito, r.contacto, nav.esquivadas, nav.ayudas, r.paradas, t,
                   p.x, p.y, p.rumbo_rad * 57.29578, nav.x_mm, nav.y_mm, nav.rumbo_grados, r.lateral, r.longitudinal, r.cola_mm, r.piso_aprendido);
        }
    }
    return r;
}

/* ── Estadística ───────────────────────────────────────────────────────── */

typedef struct {
    int n, exito, llegan, contactos, ayudas, errores, falsos_terminado, sin_terminar, esquivas, paradas, corridas_con_parada, sin_piso;
    int bajo_mueble, vistazos, vistazos_con_eco;
    int causas[20];
    int t_media, t_p95, lat_media, lat_p95, lon_media, lon_p95, desvio_p95, color_cuadros, coinc_cuadros;
} Resumen;

static int comparar(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static void percentiles(int *v, int n, int *media, int *p95)
{
    long suma = 0;
    int i;

    if (n <= 0) { *media = *p95 = 0; return; }
    qsort(v, (size_t)n, sizeof(int), comparar);
    for (i = 0; i < n; i++) suma += v[i];
    *media = (int)(suma / n);
    *p95 = v[(n * 95 + 99) / 100 - 1];
}

static Resumen monte_carlo(int e, int n)
{
    Resumen s;
    int *vt = malloc(sizeof(int) * (size_t)n), *vlat = malloc(sizeof(int) * (size_t)n), *vlon = malloc(sizeof(int) * (size_t)n), *vdes = malloc(sizeof(int) * (size_t)n);
    int i, k = 0;

    memset(&s, 0, sizeof s);
    s.n = n;
    for (i = 0; i < n; i++) {
        Resultado r = correr(e, 170000u + (uint32_t)e * 10007u + (uint32_t)i);

        s.exito += r.exito; s.llegan += r.llego; s.contactos += r.contacto; s.ayudas += r.ayuda; s.errores += r.error;
        s.falsos_terminado += r.terminado_falso; s.sin_terminar += r.sin_terminar; s.esquivas += r.esquivas; s.paradas += r.paradas;
        s.corridas_con_parada += (r.paradas > 0); s.sin_piso += !r.piso_aprendido;
        s.color_cuadros += r.cuadros_color; s.coinc_cuadros += r.cuadros_coinc;
        s.bajo_mueble += r.bajo_mueble; s.vistazos += r.vistazos; s.vistazos_con_eco += r.vistazos_con_eco;
        if ((r.ayuda || r.error) && r.causa >= 0 && r.causa < 20) s.causas[r.causa]++;
        vdes[i] = r.desvio_max;
        if (r.llego) { vt[k] = r.tiempo_ms; vlat[k] = r.lateral; vlon[k] = r.longitudinal; k++; }
        if (getenv("AV2_LISTAR") && !r.exito) printf("   corrida %d: llego=%d contacto=%d bajo_mueble=%d ayuda=%d causa=%d error=%d falso_fin=%d esquivas=%d\n", i, r.llego, r.contacto, r.bajo_mueble, r.ayuda, r.causa, r.error, r.terminado_falso, r.esquivas);
    }
    percentiles(vt, k, &s.t_media, &s.t_p95);
    percentiles(vlat, k, &s.lat_media, &s.lat_p95);
    percentiles(vlon, k, &s.lon_media, &s.lon_p95);
    {
        int m;

        percentiles(vdes, n, &m, &s.desvio_p95);
    }
    free(vt); free(vlat); free(vlon); free(vdes);
    return s;
}

static void imprimir(int e, const Resumen *s)
{
    static const char *TIPO[4] = { "falso", "obstaculo", "imposible", "limite" };
    int c;

    printf("AV2 escenario=%s tipo=%s n=%d exito=%d contactos=%d ayuda=%d error=%d terminado_falso=%d sin_terminar=%d llegan=%d | tiempo_ms media=%d p95=%d | lateral media=%d p95=%d | longitudinal media=%d p95=%d | desvio_max p95=%d | esquivas=%d paradas=%d corridas_con_parada=%d | cuadros color=%d coincidencia=%d | sin_piso=%d | bajo_mueble=%d vistazos=%d vistazos_con_eco=%d | causas",
           ESC[e].nombre, TIPO[ESC[e].tipo], s->n, s->exito, s->contactos, s->ayudas, s->errores, s->falsos_terminado, s->sin_terminar, s->llegan, s->t_media, s->t_p95,
           s->lat_media, s->lat_p95, s->lon_media, s->lon_p95, s->desvio_p95, s->esquivas, s->paradas, s->corridas_con_parada, s->color_cuadros, s->coinc_cuadros, s->sin_piso,
           s->bajo_mueble, s->vistazos, s->vistazos_con_eco);
    for (c = 0; c < 20; c++) if (s->causas[c]) printf(" %d:%d", c, s->causas[c]);
    printf("\n");
}

int main(int argc, char **argv)
{
    int n = 40, e, solo = -1;
    static Resumen res[N_ESC];

    if (getenv("AV2_UMBRAL")) op_umbral = atoi(getenv("AV2_UMBRAL"));
    if (getenv("AV2_CELDAS")) op_celdas = atoi(getenv("AV2_CELDAS"));
    if (getenv("AV2_AEC_FIJO")) op_aec_fijo = atoi(getenv("AV2_AEC_FIJO"));
    if (argc > 3 && strcmp(argv[1], "depurar") == 0) {
        depurar = getenv("AV2_TODO") ? 2 : 1;
        e = atoi(argv[2]);
        if (e < 0 || e >= N_ESC) return 2;
        correr(e, 170000u + (uint32_t)e * 10007u + (uint32_t)atoi(argv[3]));
        return 0;
    }
    if (argc > 1) {
        n = atoi(argv[1]);
        if (n < 10) n = 10;
        if (n > 5000) n = 5000;
    }
    if (argc > 2) solo = atoi(argv[2]);
    printf("-- Banco autonomo v2 (SIMULADO / NO MEDIDO en el carro), %d corridas por escenario, semillas 170000 + indice * 10007 + corrida --\n", n);
    for (e = 0; e < N_ESC; e++) {
        if (solo >= 0 && e != solo) continue;
        res[e] = monte_carlo(e, n);
        imprimir(e, &res[e]);
        fflush(stdout);
    }
#ifdef AV2_ACTUAL
    if (solo < 0 && argc <= 1) {   /* compuertas de la prueba rápida (make test); el informe de 1000 corridas lo revisa scripts/compuertas_autonomo.py */
        int contactos = 0, falsos = 0;

        for (e = 0; e < N_ESC; e++) {
            if (ESC[e].tipo == T_LIMITE) continue;
            contactos += res[e].contactos;
            falsos += res[e].falsos_terminado;
        }
        caso("compuerta: cero contactos en todos los escenarios (salvo el limite conocido)", contactos == 0);
        caso("compuerta: ningun Terminado sin haber dejado atras el objeto", falsos == 0);
        printf("%d casos, %d fallos\n", casos, fallos);
    }
#else
    (void)caso;
#endif
    return fallos ? EXIT_FAILURE : EXIT_SUCCESS;
}
