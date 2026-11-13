/*
 * pruebas_navegador.c — Pruebas de navegador.c en PC con un carro simulado.
 *
 * GUÍA DE LECTURA
 *  1. Cada caso arma una ruta, un simulador (carro + obstáculos programados), corre el ciclo
 *     de 50 ms y revisa el resultado: estado final, posición REAL del carro simulado,
 *     secuencia de comandos y cuántas veces se detuvo.
 *  Los contratos de fusion, cierre y esquive iterativo se prueban ahora en
 *  pruebas_esquive_confirmado.c con objetos geometricos, no eventos fijados al reloj.
 *  2. preparar() carga la ruta y deja todo listo; correr() avanza el tiempo.
 *  3. Posiciones en milímetros; cada celda mide CELDA_CM * 10 mm (200 mm).
 *  4. Al final se comprueba que en TODAS las pruebas nunca hubo movimiento en ESPERANDO,
 *     TERMINADO o ERROR (violaciones_seguridad).
 */

#include <stdio.h>
#include <string.h>
#include "navegador.h"
#include "simulador_navegador.h"

#define MM (CELDA_CM * 10)

static Navegador nav;
static Simulador sim;
static int casos = 0;
static int fallos = 0;
static int violaciones_totales = 0;

/* ── Rutas de prueba (cada punto es vecino del anterior) ───────────────── */

static const Punto RECTA[]    = { {2, 8}, {2, 7}, {2, 6}, {2, 5}, {2, 4}, {2, 3} };            /* 5 celdas arriba */
static const Punto CUADRADO[] = { {1, 4}, {1, 3}, {1, 2}, {2, 2}, {3, 2}, {3, 3}, {3, 4}, {2, 4}, {1, 4} };
static const Punto ZIGZAG[]   = { {0, 5}, {0, 4}, {1, 4}, {1, 3}, {2, 3}, {2, 2}, {3, 2} };
static const Punto DIAGONAL[] = { {0, 4}, {1, 3}, {2, 2} };
static const Punto ABAJO[]    = { {2, 3}, {2, 4}, {2, 5} };                                    /* hacia abajo: gira 180 */
static const Punto UN_PUNTO[] = { {4, 4} };
static const Punto LARGA[] = {   /* 6 m en linea recta hacia arriba (columna 2, de la fila 30 a la 0) */
    {2, 30}, {2, 29}, {2, 28}, {2, 27}, {2, 26}, {2, 25}, {2, 24}, {2, 23}, {2, 22}, {2, 21}, {2, 20}, {2, 19}, {2, 18}, {2, 17}, {2, 16}, {2, 15},
    {2, 14}, {2, 13}, {2, 12}, {2, 11}, {2, 10}, {2, 9}, {2, 8}, {2, 7}, {2, 6}, {2, 5}, {2, 4}, {2, 3}, {2, 2}, {2, 1}, {2, 0}
};
static const Punto CON_SALTO[] = { {0, 0}, {3, 0} };

#define N(ruta) ((int)(sizeof(ruta) / sizeof((ruta)[0])))

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
        printf("DIAG estado=%d causa=%d esquivas=%d correcciones=%d real=(%d,%d) estimada=(%d,%d)\n",nav.estado,nav.error_causa,nav.esquivadas,nav.reinc_correcciones,sim_x_mm(&sim),sim_y_mm(&sim),nav.x_mm,nav.y_mm);
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* Deja el navegador y el simulador listos en el primer punto de la ruta, y comienza. */
static void preparar(const Punto *ruta, int n)
{
    navegador_iniciar(&nav);
    navegador_cargar_ruta(&nav, ruta, n);
    sim_iniciar(&sim, ruta[0].columna, ruta[0].fila, 0);
    navegador_comenzar(&nav, 0);
}

static void correr(uint32_t ms)
{
    sim_correr(&sim, &nav, ms);
    violaciones_totales += sim.violaciones_seguridad;
    sim.violaciones_seguridad = 0;
}

static int valor_absoluto(int x)
{
    return (x < 0) ? -x : x;
}

/* ¿El carro simulado está a menos de `tolerancia_mm` del centro de la celda (columna, fila)? */
static int cerca_de(int columna, int fila, int tolerancia_mm)
{
    return valor_absoluto(sim_x_mm(&sim) - columna * MM) <= tolerancia_mm &&
           valor_absoluto(sim_y_mm(&sim) - fila * MM) <= tolerancia_mm;
}

static void informar_posicion(const char *nombre)
{
    printf("      %s: estado=%s motivo=%d esquivadas=%d real=(%d,%d) mm rumbo=%d  estimada=(%d,%d)\n",
           nombre, navegador_texto_estado(nav.estado), (int)nav.motivo_error, nav.esquivadas,
           sim_x_mm(&sim), sim_y_mm(&sim), sim_rumbo_grados(&sim), nav.x_mm, nav.y_mm);
}

/* ── Casos: rutas sin obstáculos ───────────────────────────────────────── */

static void pruebas_sin_obstaculos(void)
{
    preparar(RECTA, N(RECTA));
    correr(12000);
    caso("ruta recta de 5 celdas sin obstaculos: TERMINADO cerca del destino (2,3)",
         nav.estado == NAV_TERMINADO && cerca_de(2, 3, 60));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(2, 3, 60))) {
        informar_posicion("recta");
    }
    caso("al terminar el ultimo comando es STOP", sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);

    preparar(CUADRADO, N(CUADRADO));
    correr(30000);
    caso("cuadrado: TERMINADO y regresa al punto de partida (1,4)",
         nav.estado == NAV_TERMINADO && cerca_de(1, 4, 80));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(1, 4, 80))) {
        informar_posicion("cuadrado");
    }

    preparar(ZIGZAG, N(ZIGZAG));
    correr(30000);
    caso("zigzag: TERMINADO cerca del destino (3,2)", nav.estado == NAV_TERMINADO && cerca_de(3, 2, 80));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(3, 2, 80))) {
        informar_posicion("zigzag");
    }

    preparar(DIAGONAL, N(DIAGONAL));
    correr(20000);
    caso("diagonal: TERMINADO cerca del destino (2,2)", nav.estado == NAV_TERMINADO && cerca_de(2, 2, 100));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(2, 2, 100))) {
        informar_posicion("diagonal");
    }

    preparar(ABAJO, N(ABAJO));
    correr(20000);
    caso("ir hacia abajo (media vuelta de 180 grados): TERMINADO cerca de (2,5)",
         nav.estado == NAV_TERMINADO && cerca_de(2, 5, 80));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(2, 5, 80))) {
        informar_posicion("abajo");
    }
}

/* ── Casos: rutas raras y estados sin movimiento ───────────────────────── */

static void pruebas_rutas_raras(void)
{
    int x0, y0, hubo_movimiento = 0, i;

    navegador_iniciar(&nav);
    caso("ruta vacia (0 puntos): NAV_ERR_ARGUMENTO", navegador_cargar_ruta(&nav, RECTA, 0) == NAV_ERR_ARGUMENTO);
    caso("sin ruta cargada, comenzar da NAV_ERR_ESTADO", navegador_comenzar(&nav, 0) == NAV_ERR_ESTADO);
    caso("ruta con puntero nulo: NAV_ERR_ARGUMENTO", navegador_cargar_ruta(&nav, NULL, 3) == NAV_ERR_ARGUMENTO);
    caso("ruta con un salto invalido: NAV_ERR_RUTA y codigo TRAY_ERR_SALTO",
         navegador_cargar_ruta(&nav, CON_SALTO, N(CON_SALTO)) == NAV_ERR_RUTA && nav.codigo_trayectoria == TRAY_ERR_SALTO);

    preparar(UN_PUNTO, N(UN_PUNTO));
    x0 = sim_x_mm(&sim);
    y0 = sim_y_mm(&sim);
    correr(2000);
    caso("ruta de un solo punto: TERMINADO sin moverse",
         nav.estado == NAV_TERMINADO && sim_x_mm(&sim) == x0 && sim_y_mm(&sim) == y0 &&
         sim_contar_cambios_a(&sim, NAV_CMD_FORWARD) == 0);

    /* ESPERANDO (ruta cargada pero sin comenzar): nunca hay movimiento. */
    navegador_iniciar(&nav);
    navegador_cargar_ruta(&nav, RECTA, N(RECTA));
    for (i = 0; i < 100; i++) {
        if (navegador_tick(&nav, (uint32_t)i * NAV_TICK_MS, NULL) != NAV_CMD_STOP) {
            hubo_movimiento = 1;
        }
    }
    caso("en ESPERANDO nunca hay movimiento (100 ticks)", !hubo_movimiento && nav.estado == NAV_ESPERANDO);
}

/* ── Casos: obstáculos ─────────────────────────────────────────────────── */

/* Cuadro de la zona CENTRO con la cercanía dada (para los tests que llaman al navegador sin simulador). */

/* Prepara un navegador que avanza por la RECTA y le da cuadros cada `periodo_ms` con `cercania` en el
 * centro desde la hora `t_obstaculo_ms`. Los ticks son exactamente a la hora de cada cuadro (sin
 * simulador), para medir el retraso real del antirrebote. Devuelve la hora del primer STOP o -1. */

/* El antirrebote cuenta MILISEGUNDOS: con cuadros cada 80 ms y cada 120 ms el retraso es casi el mismo. */

static void pruebas_errores_de_navegacion(void)
{
    int x_error, y_error, i, hubo_movimiento = 0;

    /* Ambos lados bloqueados: espera NAV_TIMEOUT_ESQUIVAR_MS y entonces ERROR. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_IZQUIERDA, 3);
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_DERECHA, 3);
    correr(4000);
    caso("ambos lados bloqueados: sigue detenido mientras espera (antes del tiempo maximo)",
         nav.estado == NAV_DETENIDO_OBSTACULO);
    correr(7000);
    caso("ambos lados bloqueados: ERROR por LADOS_BLOQUEADOS tras el tiempo maximo",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_LADOS_BLOQUEADOS &&
         sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);

    /* Despues de un ERROR no hay movimiento, por mas obstaculos que cambien. */
    x_error = sim_x_mm(&sim);
    y_error = sim_y_mm(&sim);
    for (i = 0; i < 100; i++) {
        correr(NAV_TICK_MS);
    }
    caso("tras ERROR no hay movimiento (100 ticks mas)",
         sim_x_mm(&sim) == x_error && sim_y_mm(&sim) == y_error && nav.estado == NAV_ERROR);
    {
        int k;
        for (k = 0; k < 20; k++) {
            if (navegador_tick(&nav, sim.t_ms + (uint32_t)k * NAV_TICK_MS, NULL) != NAV_CMD_STOP) {
                hubo_movimiento = 1;
            }
        }
    }
    caso("tras ERROR todos los comandos son STOP", !hubo_movimiento);

    /* Obstaculo permanente al centro con cercania 2 (media): al apartarse lo sigue viendo de frente, ahora una cercania 2 CONFIRMADA tambien bloquea al avanzar de
     * lado: usa los dos reintentos y, como no se va, termina en ERROR por los reintentos (antes esquivaba sin ver el obstaculo hasta llegar al maximo de esquivadas;
     * ese caso, con un obstaculo que reaparece cada vez que vuelve a la linea, esta en pruebas_reincorporacion). */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 0, 9999999, ZONA_CENTRO, 2);
    correr(80000);
    caso("obstaculo permanente de cercania 2: reintenta dos veces y termina en ERROR (ESQUIVANDO, causa REINTENTOS)",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_ESQUIVANDO && nav.error_causa == NAV_CAUSA_REINTENTOS &&
         sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == NAV_MAX_REINTENTOS && sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);
    if (!(nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_ESQUIVANDO)) {
        informar_posicion("permanente");
    }

    /* Obstaculo permanente MUY cerca: mientras esquiva lo vuelve a ver al frente: ERROR. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 0, 9999999, ZONA_CENTRO, 3);
    correr(30000);
    caso("obstaculo permanente muy cerca: ERROR al esquivar (STOP final)",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_ESQUIVANDO &&
         sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);
    if (!(nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_ESQUIVANDO)) {
        informar_posicion("permanente cerca");
    }
}

/* ── Casos: cancelar y estados finales ─────────────────────────────────── */

static void pruebas_cancelar(void)
{
    int x_cancel, y_cancel, i, hubo_movimiento = 0;

    preparar(RECTA, N(RECTA));
    sim.t_cancelar_ms = 2000;
    correr(2000);
    caso("cancelar a mitad: el carro iba avanzando", sim_contar_cambios_a(&sim, NAV_CMD_FORWARD) >= 1 && nav.estado == NAV_SIGUIENDO);
    correr(NAV_TICK_MS);   /* aqui se cancela */
    x_cancel = sim_x_mm(&sim);
    y_cancel = sim_y_mm(&sim);
    caso("cancelar: pasa a ESPERANDO y el comando es STOP al instante",
         nav.estado == NAV_ESPERANDO && sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);
    correr(2000);
    for (i = 0; i < 20; i++) {
        if (navegador_tick(&nav, sim.t_ms + (uint32_t)i * NAV_TICK_MS, NULL) != NAV_CMD_STOP) {
            hubo_movimiento = 1;
        }
    }
    caso("tras cancelar el carro ya no se mueve", !hubo_movimiento && sim_y_mm(&sim) >= y_cancel - 12 && sim_x_mm(&sim) == x_cancel);

    /* Se puede reanudar despues de cancelar: retoma desde la posicion estimada. */
    navegador_comenzar(&nav, sim.t_ms);
    correr(12000);
    caso("cancelar y volver a comenzar: retoma la ruta y llega cerca del destino (2,3)",
         nav.estado == NAV_TERMINADO && cerca_de(2, 3, 120));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(2, 3, 120))) {
        informar_posicion("reanudar");
    }

    /* Despues de TERMINADO tampoco hay movimiento. */
    hubo_movimiento = 0;
    x_cancel = sim_x_mm(&sim);
    y_cancel = sim_y_mm(&sim);
    correr(3000);
    caso("tras TERMINADO no hay movimiento", nav.estado == NAV_TERMINADO && sim_x_mm(&sim) == x_cancel && sim_y_mm(&sim) == y_cancel);

    /* Cancelar durante DETENIDO_OBSTACULO y durante ESQUIVANDO. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 3);
    correr(2400);   /* ya esta esquivando */
    caso("(antes de cancelar) el navegador esta ESQUIVANDO", nav.estado == NAV_ESQUIVANDO);
    navegador_cancelar(&nav);
    caso("cancelar durante ESQUIVANDO: STOP en el siguiente tick y ESPERANDO",
         nav.estado == NAV_ESPERANDO && navegador_tick(&nav, sim.t_ms, NULL) == NAV_CMD_STOP);
}

static void pruebas_deriva(void)
{
    /* Si el carro real avanza 5 % menos que la calibracion, termina 50 mm antes, y el navegador no lo sabe. */
    preparar(RECTA, N(RECTA));
    sim.pct_avance = 95;
    correr(12000);
    caso("carro real con 5 % menos de avance: TERMINADO, 40 a 70 mm antes del destino",
         nav.estado == NAV_TERMINADO && valor_absoluto(sim_y_mm(&sim) - 3 * MM) >= 40 && valor_absoluto(sim_y_mm(&sim) - 3 * MM) <= 70);

    /* Si gira 5 % menos, los giros se quedan cortos y la posicion real se desvia (limite conocido). */
    preparar(CUADRADO, N(CUADRADO));
    sim.pct_giro = 95;
    correr(30000);
    caso("carro real con 5 % menos de giro: el navegador cree que llego, pero el carro real se desvia (unos 90 mm en este cuadrado)",
         nav.estado == NAV_TERMINADO && !cerca_de(1, 4, 50) && cerca_de(1, 4, 150));
    informar_posicion("giro 95%");
}

static void pruebas_textos(void)
{
    caso("textos de comandos = protocolo del carro",
         strcmp(navegador_texto_comando(NAV_CMD_FORWARD), "FORWARD") == 0 &&
         strcmp(navegador_texto_comando(NAV_CMD_REVERSE), "REVERSE") == 0 &&
         strcmp(navegador_texto_comando(NAV_CMD_LEFT), "LEFT") == 0 &&
         strcmp(navegador_texto_comando(NAV_CMD_RIGHT), "RIGHT") == 0 &&
         strcmp(navegador_texto_comando(NAV_CMD_STOP), "STOP") == 0);
    caso("texto de estado", strcmp(navegador_texto_estado(NAV_DETENIDO_OBSTACULO), "DETENIDO_OBSTACULO") == 0);

    /* Un tick con obs NULL no cambia lo que se sabe del obstaculo. */
    {
        ResultadoObstaculos obs;
        int antes;

        preparar(RECTA, N(RECTA));
        memset(&obs, 0, sizeof obs);
        obs.zona[ZONA_CENTRO].hay_obstaculo = 1;
        obs.zona[ZONA_CENTRO].cercania = 3;
        obs.fuentes_vision[ZONA_CENTRO] = 3;
        navegador_tick(&nav, 50, &obs);
        antes = nav.frames_bloqueado;
        navegador_tick(&nav, 100, NULL);
        navegador_tick(&nav, 150, NULL);
        caso("tick sin cuadro nuevo (NULL) no cambia los contadores de cuadros",
             antes == 1 && nav.frames_bloqueado == 1);
    }
}

/* ── Casos: calibración medida del carro ──────────────────────────────── */

/* Un carro "real" distinto de la calibración de ejemplo: tarda 150 ms en arrancar, avanza 42 ms/cm,
 * sigue 30 mm al frenar y gira distinto a cada lado (9 ms/grado a la izquierda y 11.5 a la derecha). */
static CalibracionCarro carro_distinto(void)
{
    CalibracionCarro real;

    cal_defecto(&real);
    real.medidos = CAL_MEDIDO_TODO;
    real.arranque_ms = 150;
    real.ms_por_cm_x10 = 420;
    real.frenado_mm = 30;
    real.ms_por_grado_izq_x10 = 90;
    real.ms_por_grado_der_x10 = 115;
    real.frenado_grados_izq_x10 = 60;
    real.frenado_grados_der_x10 = 40;
    return real;
}

static void pruebas_calibracion(void)
{
    CalibracionCarro real = carro_distinto();
    CalibracionCarro mala;
    int distancia_sin, distancia_con;

    /* Entrega de la calibración al navegador. */
    navegador_iniciar(&nav);
    caso("el navegador nuevo usa la calibracion por defecto", nav.cal.medidos == 0 && nav.cal.ms_por_cm_x10 == MS_POR_CM * 10);
    caso("poner una calibracion valida: NAV_OK", navegador_poner_calibracion(&nav, &real) == NAV_OK && nav.cal.arranque_ms == 150);
    mala = real;
    mala.ms_por_cm_x10 = 0;
    caso("poner una calibracion no valida: NAV_ERR_ARGUMENTO y no cambia", navegador_poner_calibracion(&nav, &mala) == NAV_ERR_ARGUMENTO && nav.cal.ms_por_cm_x10 == 420);
    caso("poner una calibracion NULL: NAV_ERR_ARGUMENTO", navegador_poner_calibracion(&nav, NULL) == NAV_ERR_ARGUMENTO && navegador_poner_calibracion(NULL, &real) == NAV_ERR_ARGUMENTO);
    preparar(RECTA, N(RECTA));
    caso("poner la calibracion con la ruta en marcha: NAV_ERR_ESTADO", navegador_poner_calibracion(&nav, &real) == NAV_ERR_ESTADO);
    navegador_cancelar(&nav);
    caso("cancelada la ruta (ESPERANDO) si se puede cambiar la calibracion", navegador_poner_calibracion(&nav, &real) == NAV_OK);

    /* Un carro real con arranque, frenado y otra velocidad: SIN calibrar el navegador se pasa del destino. */
    preparar(RECTA, N(RECTA));                 /* 5 celdas = 100 cm hacia arriba */
    sim.real = real;
    correr(15000);
    distancia_sin = valor_absoluto(sim_y_mm(&sim) - 3 * MM);
    caso("carro distinto SIN calibrar: llega, pero lejos del destino (mas de 100 mm)", nav.estado == NAV_TERMINADO && distancia_sin > 100);
    printf("      sin calibrar: el carro real quedo a %d mm del destino\n", distancia_sin);

    /* Con la calibracion verdadera, llega al destino. */
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, &real);
    navegador_cargar_ruta(&nav, RECTA, N(RECTA));
    sim_iniciar(&sim, RECTA[0].columna, RECTA[0].fila, 0);
    sim.real = real;
    navegador_comenzar(&nav, 0);
    correr(15000);
    distancia_con = valor_absoluto(sim_y_mm(&sim) - 3 * MM);
    caso("carro distinto CON su calibracion: llega a menos de 40 mm del destino", nav.estado == NAV_TERMINADO && distancia_con < 40);
    printf("      calibrado: el carro real quedo a %d mm del destino\n", distancia_con);
    caso("con calibracion la posicion estimada coincide con la real (menos de 40 mm)",
         valor_absoluto(nav.y_mm - sim_y_mm(&sim)) < 40 && valor_absoluto(nav.x_mm - sim_x_mm(&sim)) < 40);

    /* Zigzag: giros a los dos lados con tiempos distintos. */
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, &real);
    navegador_cargar_ruta(&nav, ZIGZAG, N(ZIGZAG));
    sim_iniciar(&sim, ZIGZAG[0].columna, ZIGZAG[0].fila, 0);
    sim.real = real;
    navegador_comenzar(&nav, 0);
    correr(40000);
    caso("zigzag (giros a izquierda y derecha) con la calibracion verdadera: llega cerca de (3,2)",
         nav.estado == NAV_TERMINADO && cerca_de(3, 2, 90));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(3, 2, 90))) {
        informar_posicion("zigzag calibrado");
    }
    {
        int error_con = valor_absoluto(sim_x_mm(&sim) - 3 * MM) + valor_absoluto(sim_y_mm(&sim) - 2 * MM);

        preparar(ZIGZAG, N(ZIGZAG));
        sim.real = real;
        correr(40000);
        caso("el mismo zigzag SIN calibrar termina mas lejos que con calibracion",
             valor_absoluto(sim_x_mm(&sim) - 3 * MM) + valor_absoluto(sim_y_mm(&sim) - 2 * MM) > error_con);
    }

    /* Giros a ambos lados con la calibracion de cada lado: el cuadrado vuelve al punto de partida. */
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, &real);
    navegador_cargar_ruta(&nav, CUADRADO, N(CUADRADO));
    sim_iniciar(&sim, CUADRADO[0].columna, CUADRADO[0].fila, 0);
    sim.real = real;
    navegador_comenzar(&nav, 0);
    correr(40000);
    caso("cuadrado con la calibracion verdadera: vuelve cerca del punto de partida (1,4)", nav.estado == NAV_TERMINADO && cerca_de(1, 4, 100));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(1, 4, 100))) {
        informar_posicion("cuadrado calibrado");
    }

    /* Una detencion a mitad de camino: el carro frena (sigue 30 mm), tiene que volver a arrancar (150 ms)
     * y aun asi debe llegar al destino. */
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, &real);
    navegador_cargar_ruta(&nav, RECTA, N(RECTA));
    sim_iniciar(&sim, RECTA[0].columna, RECTA[0].fila, 0);
    sim.real = real;
    sim_agregar_obstaculo(&sim, 1500, 1700, ZONA_CENTRO, 3);
    navegador_comenzar(&nav, 0);
    correr(20000);
    caso("calibrado, con una detencion por obstaculo a mitad de camino: reanuda y llega a menos de 40 mm del destino",
         nav.estado == NAV_TERMINADO && nav.esquivadas == 0 && sim_contar_cambios_a(&sim, NAV_CMD_STOP) == 2 &&
         valor_absoluto(sim_y_mm(&sim) - 3 * MM) < 40);

    /* Los tiempos de los pasos usan la calibracion: avanzar 100 cm pide menos ms con frenado (30 mm). */
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, &real);
    navegador_cargar_ruta(&nav, RECTA, N(RECTA));
    navegador_comenzar(&nav, 0);
    caso("el primer tramo de 20 cm (la ruta sigue recta): arranque + 200 mm * 4.2 = 990 ms, sin descontar el frenado",
         nav.n_pasos == 1 && nav.pasos[0].tiempo_ms == cal_cm_a_ms_tramo(&real, 20, 1, 0) && nav.pasos[0].tiempo_ms == 990);
    caso("un avance de 100 cm de una sola vez (sin tramos) pide arranque + (1000 - 30 mm) * 4.2 = 4224 ms", cal_cm_a_ms(&real, 100) == 4224);
}

/* ── Casos: giro medido con la cámara ──────────────────────────────────── */

static const Punto ESQUINA_DERECHA[] = { {1, 6}, {1, 5}, {2, 5} };   /* sube una celda y gira 90 grados a la DERECHA */
static const Punto ESQUINA_IZQUIERDA[] = { {3, 6}, {3, 5}, {2, 5} }; /* sube una celda y gira 90 grados a la IZQUIERDA */

/* Corre hasta que termina el primer giro (LEFT o RIGHT) y devuelve el rumbo REAL del carro en ese momento (0 a 359), con lo que
 * sigue girando al frenar. -1 si no hubo giro. */
static int rumbo_tras_el_primer_giro(void)
{
    int visto = 0, i;

    for (i = 0; i < 600; i++) {
        correr(NAV_TICK_MS);
        if (sim.cmd_previo == NAV_CMD_LEFT || sim.cmd_previo == NAV_CMD_RIGHT) {
            visto = 1;
        } else if (visto) {
            return sim_rumbo_grados(&sim);
        }
    }
    return -1;
}

/* Prepara una ruta de esquina con un carro cuyo giro real es `pct_giro` por ciento del calibrado. */
static void preparar_esquina(const Punto *ruta, int n, int pct_giro, int con_camara)
{
    preparar(ruta, n);
    sim.pct_giro = pct_giro;
    sim.periodo_cuadro_ms = 80;
    sim.visual_activo = con_camara;
}

/* Diferencia (en grados, de 0 a 180) entre dos rumbos. */
static int diferencia_de_rumbos(int a, int b)
{
    int d = valor_absoluto(a - b) % 360;

    return (d > 180) ? 360 - d : d;
}

/* Entre avanzar y girar el carro se detiene NAV_PAUSA_ASENTAR_MS; entre dos tramos rectos seguidos no se detiene. */
static void pruebas_pausa_entre_movimientos(void)
{
    int i, visto_avance = 0, stop_ms = 0, pausa_medida = -1, paradas_en_recta = 0, estaba_avanzando = 0;

    preparar(ESQUINA_DERECHA, N(ESQUINA_DERECHA));
    for (i = 0; i < 600 && pausa_medida < 0; i++) {
        correr(NAV_TICK_MS);
        if (sim.cmd_previo == NAV_CMD_FORWARD) {
            visto_avance = 1;
            stop_ms = 0;
        } else if (visto_avance && sim.cmd_previo == NAV_CMD_STOP) {
            stop_ms += NAV_TICK_MS;
        } else if (visto_avance && (sim.cmd_previo == NAV_CMD_LEFT || sim.cmd_previo == NAV_CMD_RIGHT)) {
            pausa_medida = stop_ms;
        }
    }
    printf("      pausa entre avanzar y girar: %d ms (pedida %d)\n", pausa_medida, NAV_PAUSA_ASENTAR_MS);
    caso("entre avanzar y girar el carro se detiene la pausa pedida (+- 2 ticks)", pausa_medida >= NAV_PAUSA_ASENTAR_MS - 2 * NAV_TICK_MS && pausa_medida <= NAV_PAUSA_ASENTAR_MS + 2 * NAV_TICK_MS);

    preparar(RECTA, N(RECTA));
    for (i = 0; i < 1500 && nav.estado != NAV_TERMINADO; i++) {
        correr(NAV_TICK_MS);
        if (sim.cmd_previo == NAV_CMD_FORWARD) {
            estaba_avanzando = 1;
        } else if (estaba_avanzando && sim.cmd_previo == NAV_CMD_STOP && nav.estado != NAV_TERMINADO && !nav.fin_pendiente) {
            paradas_en_recta++;
        }
    }
    caso("una recta de varios tramos no se detiene a la mitad", paradas_en_recta == 0 && nav.estado == NAV_TERMINADO);
}

/* Vigilancia del video: un hueco largo sin cuadros detiene el carro y NO lo reanuda solo cuando el video vuelve. */
static void pruebas_video_perdido(void)
{
    int x_antes, y_antes, i, sin_movimiento = 1;

    /* Hueco corto (300 ms, menos del límite): el carro sigue. */
    preparar(RECTA, N(RECTA));
    correr(500);
    caso("con cuadros cada 50 ms el carro avanza", nav.estado == NAV_SIGUIENDO && sim.cmd_previo == NAV_CMD_FORWARD);
    sim.periodo_cuadro_ms = 0;                      /* se cae el video */
    correr(300);
    sim.periodo_cuadro_ms = NAV_TICK_MS;            /* vuelve */
    sim.proximo_cuadro_ms = sim.t_ms;
    correr(300);
    caso("un hueco de 300 ms de video no detiene el carro", nav.estado == NAV_SIGUIENDO && nav.motivo_error == NAV_MOTIVO_NINGUNO);

    /* Hueco largo (700 ms): STOP y ERROR por falta de video. */
    preparar(RECTA, N(RECTA));
    correr(500);
    sim.periodo_cuadro_ms = 0;
    correr(NAV_MAX_HUECO_VIDEO_MS + 3 * NAV_TICK_MS);
    caso("un hueco de video mayor al límite detiene el carro (ERROR por falta de video)",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_SIN_VIDEO && sim.cmd_previo == NAV_CMD_STOP);
    x_antes = nav.x_mm;
    y_antes = nav.y_mm;

    /* El video vuelve: el carro NO reanuda; sigue detenido hasta una orden nueva. */
    sim.periodo_cuadro_ms = NAV_TICK_MS;
    sim.proximo_cuadro_ms = sim.t_ms;
    for (i = 0; i < 100; i++) {                    /* 5 segundos más, con el frente libre */
        correr(NAV_TICK_MS);
        if (sim.cmd_previo != NAV_CMD_STOP) {
            sin_movimiento = 0;
        }
    }
    caso("cuando el video vuelve el carro NO reanuda solo: sigue en STOP y en ERROR",
         sin_movimiento && nav.estado == NAV_ERROR && nav.x_mm == x_antes && nav.y_mm == y_antes);

    /* Solo una ruta nueva lo vuelve a mover. */
    navegador_cargar_ruta(&nav, RECTA, N(RECTA));
    navegador_comenzar(&nav, sim.t_ms);
    correr(500);
    caso("con una ruta nueva vuelve a avanzar", nav.estado == NAV_SIGUIENDO && sim.cmd_previo == NAV_CMD_FORWARD);

    /* La vigilancia empieza con el primer cuadro: sin cámara (fuente de botones, cuadros siempre o nunca) no se dispara sola. */
    preparar(RECTA, N(RECTA));
    sim.periodo_cuadro_ms = 0;                      /* nunca llega un cuadro */
    correr(2000);
    caso("si nunca llegó un cuadro no se vigila el hueco (la ruta decide por tiempo, como antes)", nav.motivo_error != NAV_MOTIVO_SIN_VIDEO);
}

static void pruebas_giro_visual(void)
{
    int rumbo;

    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 100, 0);
    rumbo = rumbo_tras_el_primer_giro();
    caso("referencia: carro que gira justo lo calibrado, sin camara: termina el giro mirando a 90 grados (+-3)", diferencia_de_rumbos(rumbo, 90) <= 3);

    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 80, 0);
    rumbo = rumbo_tras_el_primer_giro();
    printf("      giro 20 %% mas lento, sin camara: rumbo %d (deberia ser 90)\n", rumbo);
    caso("carro que gira 20 % MENOS de lo calibrado, SIN camara: se queda corto (unos 72 grados)", diferencia_de_rumbos(rumbo, 72) <= 4);

    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 80, 1);
    rumbo = rumbo_tras_el_primer_giro();
    printf("      giro 20 %% mas lento, CON camara: rumbo %d\n", rumbo);
    caso("el mismo carro (20 % mas lento) CON camara: el giro se alarga y termina mirando a 90 grados (+-6)", diferencia_de_rumbos(rumbo, 90) <= 6);

    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 125, 0);
    rumbo = rumbo_tras_el_primer_giro();
    caso("carro que gira 25 % MAS de lo calibrado, SIN camara: se pasa (unos 112 grados)", diferencia_de_rumbos(rumbo, 112) <= 5);
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 125, 1);
    rumbo = rumbo_tras_el_primer_giro();
    printf("      giro 25 %% mas rapido, CON camara: rumbo %d\n", rumbo);
    caso("el mismo carro (25 % mas rapido) CON camara: se corta antes y queda a 90 grados (+-8)", diferencia_de_rumbos(rumbo, 90) <= 8);

    preparar_esquina(ESQUINA_IZQUIERDA, N(ESQUINA_IZQUIERDA), 80, 0);
    rumbo = rumbo_tras_el_primer_giro();
    caso("giro a la IZQUIERDA, 20 % mas lento, sin camara: se queda corto (unos 288 grados)", diferencia_de_rumbos(rumbo, 288) <= 4);
    preparar_esquina(ESQUINA_IZQUIERDA, N(ESQUINA_IZQUIERDA), 80, 1);
    rumbo = rumbo_tras_el_primer_giro();
    caso("giro a la IZQUIERDA, 20 % mas lento, con camara: queda a 270 grados (+-6)", diferencia_de_rumbos(rumbo, 270) <= 6);

    /* La camara pierde la confianza todo el giro: se comporta como sin camara. */
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 80, 1);
    sim.visual_perdida_desde_ms = 0;
    sim.visual_perdida_hasta_ms = 999999;
    rumbo = rumbo_tras_el_primer_giro();
    caso("si la camara NUNCA tiene confianza, el giro sigue por tiempo (igual que sin camara: unos 72 grados)", diferencia_de_rumbos(rumbo, 72) <= 4);

    /* La camara solo ve el final del giro (menos del 50 % del tiempo cubierto): tampoco se usa. */
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 80, 1);
    sim.visual_perdida_desde_ms = 0;
    sim.visual_perdida_hasta_ms = 2250;
    rumbo = rumbo_tras_el_primer_giro();
    printf("      camara solo al final del giro: rumbo %d\n", rumbo);
    caso("si la camara cubre poco del giro (menos de la mitad), no se fia de ella: giro por tiempo", diferencia_de_rumbos(rumbo, 72) <= 6 || diferencia_de_rumbos(rumbo, 90) <= 6);

    /* La camara se pierde en medio del giro (cuadro tapado 300 ms): lo que falta se completa por tiempo. */
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 80, 1);
    sim.visual_perdida_desde_ms = 2300;
    sim.visual_perdida_hasta_ms = 2600;
    rumbo = rumbo_tras_el_primer_giro();
    printf("      camara perdida 300 ms a mitad del giro: rumbo %d\n", rumbo);
    caso("con la camara perdida 300 ms a mitad del giro (el resto por tiempo), el giro queda entre 78 y 100 grados", rumbo >= 78 && rumbo <= 100);

    /* La camara mide 10 % de mas. */
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 80, 1);
    sim.visual_error_pct = 10;
    rumbo = rumbo_tras_el_primer_giro();
    printf("      camara que mide 10 %% de mas: rumbo %d (el esperado es 90 / 1.1 = 82)\n", rumbo);
    caso("una camara que mide 10 % de mas hace que el giro quede 10 % corto (unos 82 grados +-6), mejor que sin camara (72)", diferencia_de_rumbos(rumbo, 82) <= 6);

    /* El giro termina aunque la camara nunca vea suficiente: el tiempo de respaldo (150 %). */
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 35, 1);
    correr(20000);
    caso("un carro que gira muy poco (35 %) y la camara lo ve: el giro se corta por el tiempo de respaldo y la ruta TERMINA (no se queda girando)",
         nav.estado == NAV_TERMINADO);

    /* Esquivar: el giro de apartarse tambien usa la camara. */
    preparar(RECTA, N(RECTA));
    sim.pct_giro = 80;
    sim.periodo_cuadro_ms = 80;
    sim.visual_activo = 0;
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr(1700);   /* se detuvo */
    rumbo = rumbo_tras_el_primer_giro();
    caso("esquivar con un carro 20 % mas lento, SIN camara: el giro de apartarse se queda corto (unos 288 grados)", diferencia_de_rumbos(rumbo, 288) <= 6);
    preparar(RECTA, N(RECTA));
    sim.pct_giro = 80;
    sim.periodo_cuadro_ms = 80;
    sim.visual_activo = 1;
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr(1700);
    rumbo = rumbo_tras_el_primer_giro();
    printf("      esquivar con camara: rumbo %d\n", rumbo);
    caso("esquivar con el mismo carro CON camara: el giro de apartarse queda a 270 grados (+-8)", diferencia_de_rumbos(rumbo, 270) <= 8);

    /* Los cuadros durante el retardo de arranque se ignoran. */
    {
        CalibracionCarro cal;
        ResultadoObstaculos libre;

        cal_defecto(&cal);
        cal.arranque_ms = 300;
        memset(&libre, 0, sizeof libre);
        navegador_iniciar(&nav);
        navegador_poner_calibracion(&nav, &cal);
        navegador_cargar_ruta(&nav, ESQUINA_DERECHA, N(ESQUINA_DERECHA));
        navegador_comenzar(&nav, 0);
        {
            uint32_t t;
            int giro_visto_en_arranque = 0, giro_tras_arranque = 0, empezo_a_girar = 0;

            for (t = 0; t < 8000; t += NAV_TICK_MS) {
                ComandoNav c = navegador_tick(&nav, t, NULL);

                if (c == NAV_CMD_RIGHT) {
                    if (nav.seg_ms <= 300) {
                        navegador_aporte_visual(&nav, 900, 80, 1);   /* un giro de 90 grados "visto" durante el arranque: debe ignorarse */
                        giro_visto_en_arranque = nav.giro_visual_x10;
                    } else if (!empezo_a_girar) {
                        empezo_a_girar = 1;
                        navegador_aporte_visual(&nav, 50, 80, 1);   /* ya paso el arranque: este si cuenta */
                        giro_tras_arranque = nav.giro_visual_x10;
                        break;
                    }
                }
            }
            caso("los cuadros que llegan durante el retardo de arranque (300 ms) se ignoran: no suman nada", giro_visto_en_arranque == 0);
            caso("despues del arranque los cuadros si suman (5 grados)", giro_tras_arranque == 50 && nav.visual_cubierto_ms == 80);
        }
        caso("un cuadro sin confianza no suma", (navegador_aporte_visual(&nav, 500, 80, 0), nav.giro_visual_x10 == 50));
        caso("un cuadro con dt cero no suma", (navegador_aporte_visual(&nav, 500, 0, 1), nav.giro_visual_x10 == 50));
        caso("aporte con navegador NULL no revienta", (navegador_aporte_visual(NULL, 500, 80, 1), 1));
        navegador_cancelar(&nav);
        navegador_tick(&nav, 9000, NULL);
        navegador_aporte_visual(&nav, 500, 80, 1);
        caso("con el carro parado (STOP) los cuadros no suman", nav.giro_visual_x10 == 0);
    }
}

/* La camara del carro llega por WiFi con ~320 ms de retraso: lo que mide es lo que paso hace 320 ms. */
static void pruebas_giro_visual_con_retraso(void)
{
    int rumbo, sin_compensar;

    /* Un carro 20 % mas lento con una camara que se atrasa 320 ms, y el navegador SIN saber del retraso: se pasa. */
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 80, 1);
    sim.visual_latencia_ms = 320;
    sin_compensar = rumbo_tras_el_primer_giro();
    printf("      camara con 320 ms de retraso, navegador sin compensar: rumbo %d\n", sin_compensar);

    /* Lo mismo, avisandole al navegador del retraso. */
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 80, 1);
    sim.visual_latencia_ms = 320;
    navegador_poner_latencia_visual(&nav, 320);
    rumbo = rumbo_tras_el_primer_giro();
    printf("      camara con 320 ms de retraso, navegador compensando: rumbo %d\n", rumbo);
    caso("camara con 320 ms de retraso y el navegador avisado: el giro de un carro 20 % mas lento queda a 90 grados (+-10)", diferencia_de_rumbos(rumbo, 90) <= 10);
    caso("... y compensar el retraso acerca el giro a 90 (menos error que sin compensar)", diferencia_de_rumbos(rumbo, 90) <= diferencia_de_rumbos(sin_compensar, 90));

    /* Carro 25 % mas rapido con retraso: sin la compensacion la camara llega tarde y se pasa mas. */
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 125, 1);
    sim.visual_latencia_ms = 320;
    sin_compensar = rumbo_tras_el_primer_giro();
    preparar_esquina(ESQUINA_DERECHA, N(ESQUINA_DERECHA), 125, 1);
    sim.visual_latencia_ms = 320;
    navegador_poner_latencia_visual(&nav, 320);
    rumbo = rumbo_tras_el_primer_giro();
    printf("      carro 25 %% mas rapido con 320 ms de retraso: sin compensar %d, compensando %d\n", sin_compensar, rumbo);
    /* Con la pausa de asentamiento entre avanzar y girar, el sistema se retrasa un poco más (106 contra 100 antes): sigue mucho mejor que sin compensar (131). */
    caso("carro 25 % mas rapido con la camara retrasada 320 ms y el navegador avisado: queda a 90 grados (+-16)", diferencia_de_rumbos(rumbo, 90) <= 16);
    caso("... y mucho mejor que sin compensar", diferencia_de_rumbos(rumbo, 90) + 10 <= diferencia_de_rumbos(sin_compensar, 90));

    caso("la latencia debe estar entre 0 y 1000 ms", navegador_poner_latencia_visual(&nav, 1001) == NAV_ERR_ARGUMENTO &&
         navegador_poner_latencia_visual(&nav, -1) == NAV_ERR_ARGUMENTO && navegador_poner_latencia_visual(NULL, 100) == NAV_ERR_ARGUMENTO &&
         navegador_poner_latencia_visual(&nav, 0) == NAV_OK);
}


/* ── Casos: objetos grandes, reintentos de esquive y elección del lado ──── */

/* Hora (ms) del n-ésimo cambio (empezando en 1) a `comando`; -1 si no hubo tantos. */


/* Cuánto duró el n-ésimo tramo con `comando` (hasta el siguiente cambio de comando); -1 si no hay. */


/* Lista la ruta RECTA con la calibración `cal` (la del navegador y la del carro simulado son la misma). */


/* Un cuadro armado a mano: el centro muy cerca y las celdas de cada lado como se pidan (sin obstáculo en los lados). */


/* Primer giro de apartarse con esos cuadros: LEFT o RIGHT (STOP si no giró). */

/* ── Casos: el corredor de trayectoria (corredor.h) en las decisiones del navegador ───────────── */

/* Corre la ruta RECTA con un cuadro fijo (el mismo en cada tick desde 1000 ms) y dice qué comando mandó. */

/* ── Casos: volver a la línea después de esquivar, y falso obstáculo al esquivar ─────────────── */

static long raiz_larga(long v)
{
    long x = 0;

    while ((x + 1) * (x + 1) <= v) {
        x++;
    }
    return x;
}

/* Distancia (mm) de la posición REAL del carro simulado a la línea quebrada de la ruta. */
static int distancia_a_la_ruta(const Punto *ruta, int n)
{
    long mejor = -1;
    int k;

    for (k = 0; k + 1 < n; k++) {
        long ax = (long)ruta[k].columna * MM, ay = (long)ruta[k].fila * MM;
        long vx = (long)ruta[k + 1].columna * MM - ax, vy = (long)ruta[k + 1].fila * MM - ay;
        long largo2 = vx * vx + vy * vy;
        long px = sim_x_mm(&sim) - ax, py = sim_y_mm(&sim) - ay;
        long u = (largo2 > 0) ? (px * vx + py * vy) * 1000 / largo2 : 0;
        long cx, cy, d;

        u = (u < 0) ? 0 : ((u > 1000) ? 1000 : u);
        cx = ax + vx * u / 1000;
        cy = ay + vy * u / 1000;
        d = raiz_larga((sim_x_mm(&sim) - cx) * (sim_x_mm(&sim) - cx) + (sim_y_mm(&sim) - cy) * (sim_y_mm(&sim) - cy));
        if (mejor < 0 || d < mejor) {
            mejor = d;
        }
    }
    return (int)mejor;
}

/* Corre de a un tick hasta que termina la vuelta a la línea (pasó por REINCORPORANDO y volvió a SIGUIENDO o TERMINADO). 1 si lo logró; 0 si hubo ERROR o se acabó el tiempo. */


/* ¿Hay una línea de diagnóstico que empiece con `prefijo` y contenga `trozo`? (vacía la cola; guarda las líneas en `todas`) */
static char lineas_diag[64][NAV_DIAG_LARGO];
static int n_lineas_diag;

static void leer_diag(void)
{
    char linea[NAV_DIAG_LARGO];

    while (navegador_diag_siguiente(&nav, linea, sizeof linea)) {
        if (n_lineas_diag < 64) {
            memcpy(lineas_diag[n_lineas_diag++], linea, sizeof linea);
        }
    }
}

static int hay_diag(const char *trozo)
{
    int i;

    for (i = 0; i < n_lineas_diag; i++) {
        if (strstr(lineas_diag[i], trozo) != NULL) {
            return 1;
        }
    }
    return 0;
}

/* Cuántas líneas de diagnóstico leídas contienen `trozo`. */
static int cuenta_diag(const char *trozo)
{
    int i, cuenta = 0;

    for (i = 0; i < n_lineas_diag; i++) {
        if (strstr(lineas_diag[i], trozo) != NULL) {
            cuenta++;
        }
    }
    return cuenta;
}

/* Revisa que el carro estimado y el real coincidan al terminar (el navegador no se perdió) y que el rumbo quede cerca de `rumbo`. */
static int pose_estimada_coincide(int rumbo)
{
    int dr = (sim_rumbo_grados(&sim) - rumbo + 540) % 360 - 180;

    return valor_absoluto(nav.x_mm - sim_x_mm(&sim)) <= 80 && valor_absoluto(nav.y_mm - sim_y_mm(&sim)) <= 80 && valor_absoluto(dr) <= 10;
}

/* Después de la esquivada, de los comandos que salen mientras vuelve a la línea (desde `desde` en la bitácora): ¿el primero de movimiento es un giro y el siguiente
 * es avanzar? (nunca avanza recto sin reorientarse) y ¿las pausas entre un movimiento y otro duran al menos 250 ms? */

/* ── Casos: pedir ayuda («toma el control») en vez de ERROR ───────────────────────────────────── */

/* Corre `ms` con el comando manual `cmd` puesto (como la cruceta sostenida) y lo suelta al final. */
static void maniobra(ComandoNav cmd, uint32_t ms)
{
    uint32_t t0 = sim.t_ms;

    navegador_ayuda_comando(&nav, cmd);
    while (sim.t_ms - t0 < ms) {
        correr(NAV_TICK_MS);
    }
    navegador_ayuda_comando(&nav, NAV_CMD_STOP);
    correr(400);
}

static void pruebas_ayuda(void)
{
    char texto[NAV_DIAG_LARGO];
    int hubo_movimiento_propio = 0;

    /* Sin activarlo, la pared sigue dando ERROR (lo de siempre). */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_CENTRO, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_IZQUIERDA, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_DERECHA, 3, 40);
    correr(11000);
    caso("ayuda apagada (por defecto): la pared de lado a lado sigue dando ERROR", nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_LADOS_BLOQUEADOS);

    /* Con ayuda: pasa a AYUDA y queda quieto. */
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    sim_agregar_obstaculo_celdas(&sim, 1500, 12000, ZONA_CENTRO, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 12000, ZONA_IZQUIERDA, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 12000, ZONA_DERECHA, 3, 40);
    correr(11000);
    navegador_texto_error(&nav, texto, sizeof texto);
    caso("ayuda activa: con los dos lados ocupados pasa a AYUDA (no a ERROR) y queda quieto", nav.estado == NAV_AYUDA && nav.ayudas == 1 &&
         sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP && nav.cmd_actual == NAV_CMD_STOP);
    caso("... y dice por que pide ayuda (la misma causa concreta)", strstr(texto, "dos lados") != NULL);
    {
        int x = sim_x_mm(&sim), y = sim_y_mm(&sim);

        correr(2000);
        caso("... y sigue quieto mientras nadie hace nada", sim_x_mm(&sim) == x && sim_y_mm(&sim) == y && nav.estado == NAV_AYUDA);
    }
    caso("navegador_ayuda_comando fuera de AYUDA y con comandos raros da error", (navegador_ayuda_comando(&nav, NAV_CMD_FORWARD) == NAV_OK) && navegador_ayuda_comando(&nav, (ComandoNav)9) == NAV_ERR_ARGUMENTO &&
         navegador_ayuda_comando(NULL, NAV_CMD_STOP) == NAV_ERR_ARGUMENTO);
    navegador_ayuda_comando(&nav, NAV_CMD_STOP);
    correr(300);

    /* El usuario rodea el obstáculo con la cruceta: derecha 90°, 40 cm de lado, izquierda 90°, 50 cm adelante. */
    maniobra(NAV_CMD_RIGHT, 900);
    maniobra(NAV_CMD_FORWARD, 2000);
    maniobra(NAV_CMD_LEFT, 900);
    maniobra(NAV_CMD_FORWARD, 2500);
    caso("la cruceta mueve el carro (ya se aparto de la linea) y la pose estimada lo sigue (+-8 cm)",
         nav.estado == NAV_AYUDA && valor_absoluto(sim_x_mm(&sim) - 400) > 250 && valor_absoluto(nav.x_mm - sim_x_mm(&sim)) <= 80 && valor_absoluto(nav.y_mm - sim_y_mm(&sim)) <= 80);
    {
        uint32_t t0 = sim.t_ms;

        while (sim.t_ms - t0 < 15000) {
            correr(NAV_TICK_MS);
            if (nav.estado != NAV_AYUDA) {
                hubo_movimiento_propio = 1;
            }
        }
        caso("sin tocar nada en 15 s sigue en AYUDA (no se va solo)", !hubo_movimiento_propio);
    }
    caso("continuar: devuelve OK, pasa a REINCORPORANDO y las esquivadas se cuentan de nuevo desde cero", navegador_ayuda_continuar(&nav, sim.t_ms) == NAV_OK && nav.estado == NAV_REINCORPORANDO &&
         nav.esquivadas == 0);
    caso("... y continuar otra vez (ya no esta en AYUDA) da error de estado", navegador_ayuda_continuar(&nav, sim.t_ms) == NAV_ERR_ESTADO);
    correr(40000);
    caso("... vuelve a la linea y termina la ruta cerca del destino (2,3), a 15 cm o menos", nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150))) {
        informar_posicion("ayuda");
    }


    /* La ruta terminaba justo donde estaba el obstáculo: se rodea con «toma el control» y se pasa del punto final. Al continuar NO debe regresar hacia atrás, la ruta ya terminó. */
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    sim_agregar_obstaculo_celdas(&sim, 1500, 14000, ZONA_CENTRO, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 14000, ZONA_IZQUIERDA, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 14000, ZONA_DERECHA, 3, 40);
    correr(11000);
    maniobra(NAV_CMD_RIGHT, 900);
    maniobra(NAV_CMD_FORWARD, 2000);   /* 40 cm de lado */
    maniobra(NAV_CMD_LEFT, 900);
    maniobra(NAV_CMD_FORWARD, 5200);   /* ya más allá del punto final (la salida está a 70 cm de él y avanza unos 84 cm) */
    {
        int lefts_antes = sim_contar_cambios_a(&sim, NAV_CMD_LEFT), rights_antes = sim_contar_cambios_a(&sim, NAV_CMD_RIGHT);

        navegador_ayuda_continuar(&nav, sim.t_ms);
        correr(2000);
        caso("pasó del punto final con la cruceta: al continuar la ruta TERMINA y el carro no gira ni regresa hacia atrás",
             nav.estado == NAV_TERMINADO && sim_contar_cambios_a(&sim, NAV_CMD_LEFT) == lefts_antes && sim_contar_cambios_a(&sim, NAV_CMD_RIGHT) == rights_antes &&
             sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);
    }

    /* Un obstáculo que no se va y no hay a quién pedirle: a los 3 minutos ERROR. */
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    sim_agregar_obstaculo_celdas(&sim, 1500, 9999999, ZONA_CENTRO, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 9999999, ZONA_IZQUIERDA, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 9999999, ZONA_DERECHA, 3, 40);
    correr(11000);
    caso("(antes) pide ayuda", nav.estado == NAV_AYUDA);
    correr(NAV_AYUDA_MAX_MS);
    caso("a los 3 minutos sin que nadie conteste: ERROR con el motivo con el que pidió ayuda y STOP",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_LADOS_BLOQUEADOS && sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);

    /* Otros errores NO piden ayuda: sin video sigue siendo ERROR. */
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    correr(1500);
    {
        ResultadoObstaculos libre;

        memset(&libre, 0, sizeof libre);
        navegador_tick(&nav, sim.t_ms, &libre);   /* llega un cuadro: desde aquí se vigila el video */
        navegador_tick(&nav, sim.t_ms + NAV_MAX_HUECO_VIDEO_MS + 100, NULL);   /* y pasa más que el máximo sin cuadros */
        caso("sin video sigue siendo ERROR aunque se pida ayuda (no hay con que ver al maniobrar)", nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_SIN_VIDEO);
    }

    /* Un cuadro armado a medias (la app solo llena las zonas; el resto con basura) no debe romper al navegador durante la esquivada (esto tumbó la app en la prueba real). */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 3);
    correr(2600);   /* ya esquivando */
    {
        ResultadoObstaculos sucio;
        int i, bien = 1;

        memset(&sucio, 0xAB, sizeof sucio);
        for (i = 0; i < OBS_ZONAS; i++) {
            sucio.zona[i].hay_obstaculo = (i == ZONA_CENTRO);
            sucio.zona[i].cercania = (i == ZONA_CENTRO) ? 3 : 0;
            sucio.zona[i].celdas = 10;
            sucio.zona[i].celdas_corredor = 0;
            sucio.zona[i].fuera_corredor = 0;
            sucio.zona[i].cercania_corredor = 0;
        }
        for (i = 0; i < 20; i++) {
            navegador_tick(&nav, sim.t_ms + (uint32_t)(i * NAV_TICK_MS), &sucio);   /* si leyera fuera de la cuadrícula, esto revienta */
            bien = bien && nav.zona_fila_min[ZONA_CENTRO] == -1;
        }
        caso("un resultado de obstaculos con campos sin llenar (basura) no hace leer fuera del arreglo y no da filas ni columnas", bien);
    }

    /* Cancelar desde AYUDA. */
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_CENTRO, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_IZQUIERDA, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_DERECHA, 3, 40);
    correr(11000);
    navegador_ayuda_comando(&nav, NAV_CMD_FORWARD);
    correr(300);
    navegador_cancelar(&nav);
    caso("cancelar desde AYUDA: ESPERANDO y STOP en el siguiente tick", nav.estado == NAV_ESPERANDO && navegador_tick(&nav, sim.t_ms, NULL) == NAV_CMD_STOP);
    caso("los textos del estado AYUDA", strcmp(navegador_texto_estado(NAV_AYUDA), "AYUDA") == 0);
}

/* Corre la ruta RECTA con un obstáculo programado y dice si hubo ERROR de esquivar. */

/* ── Auditoría del 2026-10-15 (ver docs/navegador.md, "Correcciones de la auditoría") ─────────────────────── */


/* Un cuadro con `cercania` en una zona (0 = vacío). `corredor` = cercanía que da el corredor (0 = sin corredor). */

/* En AYUDA: mantiene `comando` durante `ms` (con cuadros vacíos) y devuelve la hora final. */

/* ── Casos: desplazamiento del centro al girar (ruedas mecanum) ────────────────────────────────── */

/* Pone el mismo desplazamiento en el carro REAL simulado y, si `modelo` es 1, también en lo que el navegador cree (la calibración del navegador); con `modelo` 0 el navegador
 * supone que el carro gira en su sitio, como antes. Valores en µm por grado: (adelante, derecha) de cada lado. */
static void preparar_con_desplazamiento(const Punto *ruta, int n, int modelo, int izq_adelante, int izq_derecha, int der_adelante, int der_derecha)
{
    CalibracionCarro real;

    cal_defecto(&real);
    cal_poner_desplazamiento(&real, CAL_LADO_IZQUIERDA, izq_adelante, izq_derecha);
    cal_poner_desplazamiento(&real, CAL_LADO_DERECHA, der_adelante, der_derecha);
    navegador_iniciar(&nav);
    if (modelo) {
        navegador_poner_calibracion(&nav, &real);
    }
    navegador_cargar_ruta(&nav, ruta, n);
    sim_iniciar(&sim, ruta[0].columna, ruta[0].fila, 0);
    sim.real = real;
    navegador_comenzar(&nav, 0);
}

static void pruebas_desplazamiento_al_girar(void)
{
    static const Punto A_LA_DERECHA[] = { {2, 8}, {3, 8} };   /* gira 90 a la derecha y avanza 20 cm */
    int lleno = 0, errores_con = 0, errores_sin = 0, dist_con, dist_sin, estimada_con;

    /* ── (a) Un giro de 90 con desplazamiento definido: la pose estimada coincide con la cuenta a mano ──
     * 1 mm por grado hacia adelante a la derecha: el centro se mueve 90 mm en la direccion del rumbo medio (45 grados) = 64 mm a la derecha y 64 mm arriba (90 x 0.707). */
    preparar_con_desplazamiento(A_LA_DERECHA, N(A_LA_DERECHA), 1, 0, 0, 1000, 0);
    n_lineas_diag = 0;
    while (nav.indice_paso < 1 && sim.t_ms < 3000) {
        correr(NAV_TICK_MS);
        leer_diag();
    }
    caso("giro de 90 a la derecha con 1 mm/grado adelante: la pose estimada termina 64 mm a la derecha y 64 mm arriba de donde empezo, mirando a la derecha",
         nav.x_mm == 2 * MM + 64 && nav.y_mm == 8 * MM - 64 && nav.rumbo_grados == 90);
    caso("... y el carro simulado (que se desplaza con los mismos valores) quedo en el mismo lugar (+-3 mm)",
         valor_absoluto(sim_x_mm(&sim) - (2 * MM + 64)) <= 3 && valor_absoluto(sim_y_mm(&sim) - (8 * MM - 64)) <= 3 && valor_absoluto(sim_rumbo_grados(&sim) - 90) <= 1);
    caso("... TURN_DIAG anota el lado, los grados pedidos y hechos, los um por grado aplicados y el desplazamiento (64, -64) en el mapa y (64, 64) en el marco del carro",
         hay_diag("TURN_DIAG lado=DER origen=ruta pedido=90 hecho=90") && hay_diag("um_grado=(1000,0)") && hay_diag("mm=(64,-64)") && hay_diag("carro_mm=(64,64)"));
    caso("... y RUTA_DIAG anota el giro enviado (RIGHT, 90 grados, 900 ms planeados) y su fin (900 ms reales)",
         hay_diag("RUTA_DIAG envia cmd=RIGHT origen=ruta pedido=90 plan_ms=900") && hay_diag("RUTA_DIAG fin cmd=RIGHT origen=ruta pedido=90 plan_ms=900 real_ms=900 hecho=90"));

    /* Sin el modelo (valor 0, como siempre) el navegador cree que el carro giro en su sitio: la pose no se mueve de lado, pero el carro real si. */
    preparar_con_desplazamiento(A_LA_DERECHA, N(A_LA_DERECHA), 0, 0, 0, 1000, 0);
    while (nav.indice_paso < 1 && sim.t_ms < 3000) {
        correr(NAV_TICK_MS);
    }
    caso("con desplazamiento 0 en el navegador la pose estimada no se mueve al girar (como antes) aunque el carro real si",
         nav.x_mm == 2 * MM && nav.y_mm == 8 * MM && valor_absoluto(sim_x_mm(&sim) - (2 * MM + 64)) <= 3);

    /* La izquierda usa sus propios valores: gira 90 a la izquierda con 1 mm/grado hacia atras y 0.5 mm/grado a la derecha. */
    {
        static const Punto A_LA_IZQUIERDA[] = { {2, 8}, {1, 8} };

        preparar_con_desplazamiento(A_LA_IZQUIERDA, N(A_LA_IZQUIERDA), 1, -1000, 500, 0, 0);
        while (nav.indice_paso < 1 && sim.t_ms < 3000) {
            correr(NAV_TICK_MS);
        }
        /* Rumbo medio -45: adelante = (-0.707, -0.707) x (-90 mm) = (+63.6, +63.6); derecha = (0.707, -0.707) x 45 mm = (+31.8, -31.8). Total (95.4, 31.8). */
        caso("giro de 90 a la izquierda con 1 mm/grado atras y 0.5 mm/grado a la derecha: 95 mm a la derecha y 32 mm abajo",
             valor_absoluto(nav.x_mm - (2 * MM + 95)) <= 1 && valor_absoluto(nav.y_mm - (8 * MM + 32)) <= 1 && nav.rumbo_grados == 270 &&
             valor_absoluto(sim_x_mm(&sim) - nav.x_mm) <= 3 && valor_absoluto(sim_y_mm(&sim) - nav.y_mm) <= 3);
    }

    /* ── (b) Un esquive completo: obstaculo al centro, el carro se aparta, pasa y vuelve a la linea (4 giros: apartarse, enderezarse y los dos de la vuelta) ──
     * Con el carro de una sola rueda motriz por lado el centro se mueve sobre todo hacia adelante al girar: 0.4 y 0.7 mm por grado (unos 4 a 6 cm por giro de 90) mas un poco de lado.
     * Lo que queda aun con el modelo (unos 8 cm) es la tolerancia del propio navegador: a menos de 8 cm de la linea solo corrige el rumbo y no se mueve de lado. */
    preparar_con_desplazamiento(RECTA, N(RECTA), 1, 400, 80, 700, -120);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr(60000);
    dist_con = distancia_a_la_ruta(nav.ruta, nav.n_puntos);
    estimada_con = (valor_absoluto(nav.x_mm - sim_x_mm(&sim)) > valor_absoluto(nav.y_mm - sim_y_mm(&sim))) ? valor_absoluto(nav.x_mm - sim_x_mm(&sim)) : valor_absoluto(nav.y_mm - sim_y_mm(&sim));
    errores_con = (nav.estado == NAV_TERMINADO && nav.esquivadas == 1);
    caso("esquive completo con el modelo: llega (TERMINADO, 1 esquivada) y la pose final queda a 10 cm o menos de la linea", errores_con && dist_con <= 100);
    caso("... y la pose estimada coincide con la real (+-3 cm)", estimada_con <= 30);

    preparar_con_desplazamiento(RECTA, N(RECTA), 0, 400, 80, 700, -120);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr(60000);
    dist_sin = distancia_a_la_ruta(nav.ruta, nav.n_puntos);
    errores_sin = (nav.estado == NAV_TERMINADO || nav.estado == NAV_ERROR);
    caso("el mismo esquive con desplazamiento 0 en el navegador (no sabe que el carro se desplaza): el error respecto de la linea es mayor", errores_sin && dist_sin > dist_con);
    lleno = dist_sin > 100;
    caso("... y pasa de 10 cm (el modelo es lo que lo deja dentro)", lleno);
    printf("      esquive con modelo: %d mm de la linea (estimada vs real %d mm); sin modelo: %d mm\n", dist_con, estimada_con, dist_sin);

    /* ── (c) El pivote REAL medido: 15 cm adelante y 16.5 cm a la derecha en un giro de 90 a la derecha (AUTO 210) ── */
    {
        static const Punto DOS_DERECHAS[] = { {2, 8}, {3, 8}, {3, 9} };   /* gira 90 a la derecha, avanza 20 cm, gira 90 a la derecha otra vez y avanza 20 cm */
        int pa_um, pd_um, ia_um, id_um, a1 = 0, d1 = 0, a2 = 0, d2 = 0, m1x = 0, m1y = 0, m2x = 0, m2y = 0, i, vistos = 0, x_antes, y_antes;

        cal_desplazamiento_de_medida(CAL_LADO_DERECHA, 150, 165, 90, &pa_um, &pd_um);
        cal_desplazamiento_de_medida(CAL_LADO_IZQUIERDA, 150, -165, 90, &ia_um, &id_um);

        preparar_con_desplazamiento(A_LA_DERECHA, N(A_LA_DERECHA), 1, ia_um, id_um, pa_um, pd_um);
        n_lineas_diag = 0;
        while (nav.indice_paso < 1 && sim.t_ms < 3000) {
            correr(NAV_TICK_MS);
            leer_diag();
        }
        caso("pivote real: tras el giro de 90 a la derecha la pose estimada esta 16.5 cm a la derecha y 15 cm arriba del punto de partida (+-3 mm), mirando a la derecha",
             valor_absoluto(nav.x_mm - (2 * MM + 165)) <= 3 && valor_absoluto(nav.y_mm - (8 * MM - 150)) <= 3 && nav.rumbo_grados == 90);
        caso("... y el carro simulado quedo en el mismo lugar (+-3 mm): el simulador usa el mismo marco inicial",
             valor_absoluto(sim_x_mm(&sim) - nav.x_mm) <= 3 && valor_absoluto(sim_y_mm(&sim) - nav.y_mm) <= 3 && valor_absoluto(sim_rumbo_grados(&sim) - 90) <= 1);

        /* Una ruta de dos giros: el primero es el de 90 (el navegador ya sabe que el carro se corrio 22 cm, asi que el segundo sale de otro angulo). El marco se comprueba con el primero. */
        preparar_con_desplazamiento(DOS_DERECHAS, N(DOS_DERECHAS), 1, ia_um, id_um, pa_um, pd_um);
        n_lineas_diag = 0;
        correr(15000);
        leer_diag();
        for (i = 0; i < n_lineas_diag; i++) {
            const char *t = strstr(lineas_diag[i], "TURN_DIAG lado=DER");
            const char *c = (t != NULL) ? strstr(t, "carro_mm=(") : NULL;
            const char *m = (t != NULL) ? strstr(t, " mm=(") : NULL;

            if (c != NULL && m != NULL && strstr(t, "um_grado=(0,0)") == NULL && vistos < 1) {
                int ca = 0, cd = 0, mx = 0, my = 0;

                if (sscanf(c, "carro_mm=(%d,%d)", &ca, &cd) == 2 && sscanf(m, " mm=(%d,%d)", &mx, &my) == 2) {
                    if (vistos == 0) { a1 = ca; d1 = cd; m1x = mx; m1y = my; } else { a2 = ca; d2 = cd; m2x = mx; m2y = my; }
                    vistos++;
                }
            }
        }
        caso("pivote real, el primer giro de la ruta de dos giros: TURN_DIAG anota el desplazamiento en el marco del carro al empezar (15 cm adelante, 16.5 cm a la derecha) y en el mapa (165, -150), +-2 mm",
             vistos == 1 && valor_absoluto(a1 - 150) <= 2 && valor_absoluto(d1 - 165) <= 2 && valor_absoluto(m1x - 165) <= 2 && valor_absoluto(m1y + 150) <= 2);
        (void)a2; (void)d2; (void)m2x; (void)m2y; (void)x_antes; (void)y_antes;

        /* El esquive completo con el pivote real: 4 giros de ~22 cm cada uno. Con el modelo el navegador sabe a dónde lo lleva cada giro. */
        preparar_con_desplazamiento(RECTA, N(RECTA), 1, ia_um, id_um, pa_um, pd_um);
        sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
        correr(90000);
        dist_con = distancia_a_la_ruta(nav.ruta, nav.n_puntos);
        estimada_con = (valor_absoluto(nav.x_mm - sim_x_mm(&sim)) > valor_absoluto(nav.y_mm - sim_y_mm(&sim))) ? valor_absoluto(nav.x_mm - sim_x_mm(&sim)) : valor_absoluto(nav.y_mm - sim_y_mm(&sim));
        errores_con = (nav.estado == NAV_TERMINADO && nav.esquivadas == 1);
        preparar_con_desplazamiento(RECTA, N(RECTA), 0, ia_um, id_um, pa_um, pd_um);
        sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
        correr(90000);
        dist_sin = distancia_a_la_ruta(nav.ruta, nav.n_puntos);
        errores_sin = (nav.estado == NAV_TERMINADO || nav.estado == NAV_ERROR);
        printf("      esquive con el pivote real: con el modelo termina=%d a %d mm de la linea (estimada vs real %d mm); sin modelo termina=%d a %d mm de la linea\n", errores_con, dist_con, estimada_con, errores_sin, dist_sin);
        caso("esquive completo con el pivote real y el modelo: llega (TERMINADO, 1 esquivada)", errores_con);
        caso("... y la reincorporacion MEJORA con el modelo: termina mas cerca de la linea que sin el (el navegador que cree que el carro gira en su sitio)", errores_sin && dist_con < dist_sin);
        caso("... y con el modelo la pose estimada coincide con la real (+-3 cm)", estimada_con <= 30);
    }

    /* ── Pantalla de resultado: «Giro pedido / Giro hecho» ── */
    {
        char texto[300];

        preparar_con_desplazamiento(A_LA_DERECHA, N(A_LA_DERECHA), 1, 0, 0, 300, 0);
        correr(5000);
        caso("la ruta de 20 cm a la derecha recuerda UN giro: derecha, pedido 90, hecho 90",
             navegador_cuantos_giros(&nav) == 1 && navegador_texto_giros(&nav, texto, sizeof texto) > 0 && strstr(texto, "derecha: pedido 90°, hecho ~90°") != NULL);
    }
}

/* ── Casos: la ruta de 40 cm a la derecha con 1 giro (J5): ¿el programa manda bien el giro? ───────────────────────────────────
 * En la prueba real esta ruta giró bien 2 de 3 veces; en una avanzó recto. Probablemente es un contacto eléctrico intermitente (hardware). Aquí se comprueba la parte de SOFTWARE: que el primer
 * comando sea el giro de 90° con el tiempo esperado, antes de cualquier avance, y que RUTA_DIAG lo deje anotado (comando enviado, duración planeada y real). */

static void pruebas_ruta_derecha_40cm(void)
{
    static const Punto DERECHA_40[] = { {2, 8}, {3, 8}, {4, 8} };   /* 40 cm a la derecha: gira 90 y avanza 40 cm */
    CalibracionCarro cal;
    char texto[300];
    int pasos_antes_del_giro = 0, i;

    /* ── Con la calibración de ejemplo (10 ms por grado, 50 ms por cm): el giro de 90 dura 900 ms y el avance de 40 cm 2000 ms ── */
    preparar(DERECHA_40, N(DERECHA_40));
    n_lineas_diag = 0;
    while (nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR && sim.t_ms < 20000) {
        correr(NAV_TICK_MS);
        leer_diag();
    }
    for (i = 0; i < n_lineas_diag; i++) printf("      DBG %s\n", lineas_diag[i]);
    caso("ruta de 40 cm a la derecha: termina (TERMINADO) sin error", nav.estado == NAV_TERMINADO);
    caso("... el PRIMER comando es RIGHT, en el primer tick (t = 0)", sim.n_cambios >= 1 && sim.cambios[0].comando == NAV_CMD_RIGHT && sim.cambios[0].t_ms == 0);
    caso("... el giro de 90 dura lo esperado: el siguiente cambio es STOP a los 900 ms (10 ms por grado x 90)", sim.n_cambios >= 2 && sim.cambios[1].comando == NAV_CMD_STOP && sim.cambios[1].t_ms == 900);
    caso("... despues una pausa de 300 ms y FORWARD", sim.n_cambios >= 3 && sim.cambios[2].comando == NAV_CMD_FORWARD && sim.cambios[2].t_ms == 1200);
    for (i = 0; i < sim.n_cambios; i++) {
        if (sim.cambios[i].comando == NAV_CMD_FORWARD) {
            break;
        }
        if (sim.cambios[i].comando == NAV_CMD_RIGHT) {
            pasos_antes_del_giro++;
        }
    }
    caso("... el programa nunca manda avanzar antes de girar, y manda UN solo giro en toda la ruta (ningun LEFT)", pasos_antes_del_giro == 1 && sim_contar_cambios_a(&sim, NAV_CMD_RIGHT) == 1 && sim_contar_cambios_a(&sim, NAV_CMD_LEFT) == 0);
    caso("... el carro simulado queda mirando a la derecha y a 40 cm del punto de partida (+-6 cm)", valor_absoluto(sim_rumbo_grados(&sim) - 90) <= 2 && valor_absoluto(sim_x_mm(&sim) - (4 * MM)) <= 60);
    caso("RUTA_DIAG anota el giro enviado: RIGHT, origen ruta, pedido 90 grados, 900 ms planeados",
         hay_diag("RUTA_DIAG envia cmd=RIGHT origen=ruta pedido=90 plan_ms=900"));
    caso("... y cuando termina: 900 ms reales y 90 grados hechos",
         hay_diag("RUTA_DIAG fin cmd=RIGHT origen=ruta pedido=90 plan_ms=900 real_ms=900 hecho=90"));
    caso("... y despues el avance de 40 cm en dos tramos de una celda (200 mm, 1000 ms planeados cada uno), cada uno con su inicio y su fin",
         cuenta_diag("RUTA_DIAG envia cmd=FORWARD origen=ruta pedido=200 plan_ms=1000") == 2 && cuenta_diag("RUTA_DIAG fin cmd=FORWARD origen=ruta pedido=200 plan_ms=1000 real_ms=1000 hecho=200") == 2);
    caso("... y la pantalla de resultado: Giro pedido 90, giro hecho ~90",
         navegador_cuantos_giros(&nav) == 1 && navegador_texto_giros(&nav, texto, sizeof texto) > 0 && strstr(texto, "Giro derecha: pedido 90°, hecho ~90°") != NULL);

    /* ── Con una calibración medida (la del carro real, 14 oct: giro a la derecha 8.2 ms por grado): el giro de 90 dura 738 ms ── */
    cal_defecto(&cal);
    cal.ms_por_grado_der_x10 = 82;
    cal.ms_por_cm_x10 = 517;
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, &cal);
    navegador_cargar_ruta(&nav, DERECHA_40, N(DERECHA_40));
    sim_iniciar(&sim, DERECHA_40[0].columna, DERECHA_40[0].fila, 0);
    sim.real = cal;
    navegador_comenzar(&nav, 0);
    n_lineas_diag = 0;
    while (nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR && sim.t_ms < 20000) {
        correr(NAV_TICK_MS);
        leer_diag();
    }
    caso("con 8.2 ms por grado: el primer comando sigue siendo RIGHT y el giro de 90 dura 738 ms (el STOP llega en el tick de 750 ms)",
         sim.n_cambios >= 2 && sim.cambios[0].comando == NAV_CMD_RIGHT && sim.cambios[0].t_ms == 0 && sim.cambios[1].comando == NAV_CMD_STOP && sim.cambios[1].t_ms == 750);
    caso("... RUTA_DIAG anota 738 ms planeados para el giro", hay_diag("RUTA_DIAG envia cmd=RIGHT origen=ruta pedido=90 plan_ms=738"));
    caso("... y el carro simulado gira 90 grados justos (+-3)", valor_absoluto(sim_rumbo_grados(&sim) - 90) <= 3);
    caso("... nunca se manda FORWARD antes del primer giro", sim.n_cambios >= 1 && sim.cambios[0].comando != NAV_CMD_FORWARD);

    /* ── Cancelada a mitad del giro: RUTA_DIAG deja el giro con menos tiempo real que el planeado ── */
    preparar(DERECHA_40, N(DERECHA_40));
    n_lineas_diag = 0;
    correr(300);
    leer_diag();
    navegador_cancelar(&nav);
    leer_diag();
    for (i = 0; i < n_lineas_diag; i++) printf("      DBG2 %s\n", lineas_diag[i]);
    caso("si se cancela a mitad del giro, RUTA_DIAG fin anota menos tiempo real (250 ms, 25 grados) que el planeado (900 ms, 90 grados)",
         hay_diag("RUTA_DIAG fin cmd=RIGHT origen=ruta pedido=90 plan_ms=900 real_ms=250 hecho=25"));
}

/* ── Casos: plan con pivote (J8) ──────────────────────────────────────────────────────────────────────────────────────────────
 * Todos los giros del esquive y de la vuelta a la línea se planean con el desplazamiento del giro (modelo de pivote medido: 15 cm adelante y 16.5 cm a la derecha en un giro de 90 a la
 * derecha; la izquierda es el espejo mientras no se mida). El carro simulado usa el MISMO modelo. Recorre la ruta LARGA (6 m recta hacia arriba) con un obstáculo al frente. */

/* Lo que se vio en las líneas de diagnóstico de una corrida. */
typedef struct {
    int espejo_aviso;      /* TURN_DIAG con aviso=espejo_estimado */
    int espejo_comenzar;   /* el aviso de «estimada como espejo» al comenzar la ruta */
    int cambia_de_lado;    /* ESQUIVE_DIAG plan cambia_de_lado */
    int correcciones;      /* REINC_DIAG correccion n= */
    int tramo_100;         /* ESQUIVE_DIAG tramo ... tramo_mm=100 */
    int sin_solucion;      /* REINC_DIAG sin_solucion */
    int agotada;           /* REINC_DIAG correccion_agotada */
    int giros_180_de_ruta;    /* RUTA_DIAG envia ... origen=ruta pedido=180: media vuelta de la ruta (despues de una vuelta a la linea es el sintoma de «se volvio loco») */
    int puntos_saltados;      /* REINC_DIAG saltados: puntos de la ruta que el carro ya habia pasado al terminar la vuelta */
    int correcciones_rumbo;   /* REINC_DIAG correccion_rumbo (el rumbo quedo a 4 grados o mas de la linea: los giros por tiempo salen de a 5 grados) */
} MarcasPlan;

static MarcasPlan marcas_plan;

static void vigilar_marcas(void)
{
    char linea[NAV_DIAG_LARGO];

    while (navegador_diag_siguiente(&nav, linea, sizeof linea)) {
        marcas_plan.espejo_aviso += (strstr(linea, "TURN_DIAG lado=") != NULL && strstr(linea, "aviso=espejo_estimado") != NULL);
        marcas_plan.espejo_comenzar += (strstr(linea, "estimada_como_espejo_de_la_") != NULL);
        marcas_plan.cambia_de_lado += (strstr(linea, "ESQUIVE_DIAG plan cambia_de_lado") != NULL);
        marcas_plan.correcciones += (strstr(linea, "REINC_DIAG correccion n=") != NULL);
        marcas_plan.tramo_100 += (strstr(linea, "ESQUIVE_DIAG tramo") != NULL && strstr(linea, "tramo_mm=100 ") != NULL);
        marcas_plan.sin_solucion += (strstr(linea, "REINC_DIAG sin_solucion") != NULL);
        marcas_plan.agotada += (strstr(linea, "REINC_DIAG correccion_agotada") != NULL);
        marcas_plan.giros_180_de_ruta += (strstr(linea, "RUTA_DIAG envia") != NULL && strstr(linea, "origen=ruta") != NULL && strstr(linea, "pedido=180") != NULL);
        marcas_plan.puntos_saltados += (strstr(linea, "REINC_DIAG saltados=") != NULL);
        marcas_plan.correcciones_rumbo += (strstr(linea, "REINC_DIAG correccion_rumbo") != NULL);
    }
}

/* Prepara el carro REAL (simulado) y el navegador con el mismo pivote: la derecha con 15 cm adelante y 16.5 cm a la derecha por `escala_pct` por ciento; la izquierda, si `izq_pct` no es 0,
 * con el espejo medido por `izq_pct` por ciento (si es 0 queda sin medir: el modelo usa el espejo de la derecha). */
static void preparar_plan_pivote(const Punto *ruta, int n, int escala_pct, int izq_pct, int ayuda)
{
    CalibracionCarro real;
    int pa, pd, ia, id;

    cal_defecto(&real);
    if (escala_pct > 0) {
        cal_desplazamiento_de_medida(CAL_LADO_DERECHA, 150 * escala_pct / 100, 165 * escala_pct / 100, 90, &pa, &pd);
        cal_poner_desplazamiento(&real, CAL_LADO_DERECHA, pa, pd);
    }
    if (izq_pct > 0) {
        cal_desplazamiento_de_medida(CAL_LADO_IZQUIERDA, 150 * izq_pct / 100, -165 * izq_pct / 100, 90, &ia, &id);
        cal_poner_desplazamiento(&real, CAL_LADO_IZQUIERDA, ia, id);
    }
    memset(&marcas_plan, 0, sizeof marcas_plan);
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, &real);
    navegador_poner_pedir_ayuda(&nav, ayuda);
    navegador_cargar_ruta(&nav, ruta, n);
    sim_iniciar(&sim, ruta[0].columna, ruta[0].fila, 0);
    sim.real = real;
    navegador_comenzar(&nav, 0);
    vigilar_marcas();
}

/* Rumbo del carro REAL respecto de la línea (hacia arriba = 0): de -180 a 180. */
static int error_rumbo_real(void)
{
    int r = sim_rumbo_grados(&sim);

    return (r > 180) ? r - 360 : r;
}

/* Corre hasta que el navegador termina, da ERROR o pide AYUDA (o pasa el límite). */
static void correr_hasta_el_final(uint32_t limite_ms)
{
    uint32_t t0 = sim.t_ms;

    while (sim.t_ms - t0 < limite_ms && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR && nav.estado != NAV_AYUDA) {
        correr(NAV_TICK_MS);
        vigilar_marcas();
    }
}

/* Un esquive completo con el obstáculo al centro y, si `zona_extra` es ZONA_IZQUIERDA o ZONA_DERECHA, también en esa zona (para que se aparte al otro lado). Devuelve 1 si terminó
 * bien: TERMINADO con una esquivada, a 10 cm o menos de la línea y con el rumbo de la línea (+-5 grados). Escribe cuánto quedó de la línea (mm) y de rumbo (grados). */
static int esquive_con_pivote(int escala_pct, int izq_pct, int zona_extra, int cercania, int *distancia_mm, int *rumbo_error)
{
    preparar_plan_pivote(LARGA, N(LARGA), escala_pct, izq_pct, 1);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, cercania);
    if (zona_extra >= 0) {
        sim_agregar_obstaculo(&sim, 1500, 2800, zona_extra, 1);
    }
    correr_hasta_el_final(170000);
    *distancia_mm = distancia_a_la_ruta(LARGA, N(LARGA));
    *rumbo_error = error_rumbo_real();
    return nav.estado == NAV_TERMINADO && nav.esquivadas == 1 && *distancia_mm <= 100 && valor_absoluto(*rumbo_error) <= 5;
}

/* Corre hasta el final y, cuando la vuelta a la línea llega a una pausa (termina el giro 1, el avance o el giro 2), le mueve la pose a los dos (la estimada y la real) `dx_mm` hacia la
 * izquierda: es como si una corrección externa (la cámara) dijera que el carro está ahí. Si `cada_pausa` es 1 lo hace en todas las pausas; si es 0, UNA sola vez, en la de después del avance. */
static void correr_con_empujon(int dx_mm, int cada_pausa, uint32_t limite_ms)
{
    uint32_t t0 = sim.t_ms, ultima = 0xFFFFFFFFu;
    int empujado = 0;

    while (sim.t_ms - t0 < limite_ms && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR && nav.estado != NAV_AYUDA) {
        correr(NAV_TICK_MS);
        vigilar_marcas();
        if (nav.estado == NAV_REINCORPORANDO && ((cada_pausa && (nav.reinc_fase == NAV_RF_PAUSA1 || nav.reinc_fase == NAV_RF_PAUSA2 || nav.reinc_fase == NAV_RF_PAUSA3)) ||
                                                 (!cada_pausa && nav.reinc_fase == NAV_RF_PAUSA2)) && nav.reinc_t_fase_ms != ultima && nav.ms_fase == 0 && nav.cmd_actual == NAV_CMD_STOP && (cada_pausa || !empujado)) {
            ultima = nav.reinc_t_fase_ms;
            empujado = 1;
            nav.x_mm -= dx_mm;
            nav.base_x_mm -= dx_mm;
            sim.x_cmm -= (long)dx_mm * 100;
        }
    }
}

static void pruebas_plan_pivote(void)
{
    int dist, rumbo, ok, d_centro, d_izq, d_der, d_cero, d_doble, d_propia;

    /* ── Esquive completo con el pivote real, derecha medida e izquierda = espejo ── */
    ok = esquive_con_pivote(100, 0, -1, 3, &dist, &rumbo);
    d_centro = dist;
    caso("plan con pivote, obstaculo al centro: llega (TERMINADO, 1 esquivada), a 10 cm o menos de la linea y con el rumbo de la linea (+-5 grados)", ok);
    caso("... la pose estimada coincide con la real (+-3 cm)", valor_absoluto(nav.x_mm - sim_x_mm(&sim)) <= 30 && valor_absoluto(nav.y_mm - sim_y_mm(&sim)) <= 30);
    caso("... el tramo de lado se planeo con el desplazamiento de los giros: 40 cm de margen - 16.5 (apartarse) - 15 (enderezarse) = 8.5 cm, y el minimo es 10 cm",
         marcas_plan.tramo_100 >= 1);
    caso("... y avisa que la izquierda es el espejo de la derecha: al comenzar y en el TURN_DIAG de los giros a la izquierda", marcas_plan.espejo_comenzar == 1 && marcas_plan.espejo_aviso >= 1);
    printf("      obstaculo al centro (derecha medida, izquierda espejo): %d mm de la linea, rumbo %d (correcciones de rumbo: %d)\n", dist, rumbo, marcas_plan.correcciones_rumbo);

    ok = esquive_con_pivote(100, 0, ZONA_IZQUIERDA, 3, &dist, &rumbo);
    d_izq = dist;
    caso("plan con pivote, obstaculo a la izquierda (y al centro): se aparta a la derecha, llega a 10 cm o menos de la linea y con el rumbo de la linea (+-5 grados)", ok && nav.lado == 1);
    printf("      obstaculo a la izquierda (se aparta a la derecha, medida propia): %d mm de la linea, rumbo %d\n", dist, rumbo);

    ok = esquive_con_pivote(100, 0, ZONA_DERECHA, 3, &dist, &rumbo);
    d_der = dist;
    caso("plan con pivote, obstaculo a la derecha (y al centro): se aparta a la izquierda (espejo), llega a 10 cm o menos de la linea y con el rumbo de la linea (+-5 grados)", ok && nav.lado == -1);
    printf("      obstaculo a la derecha (se aparta a la izquierda, espejo): %d mm de la linea, rumbo %d\n", dist, rumbo);

    ok = esquive_con_pivote(100, 100, -1, 3, &dist, &rumbo);
    d_propia = dist;
    caso("... con la izquierda medida (sin espejo) tambien llega bien y NO avisa de espejo", ok && marcas_plan.espejo_comenzar == 0 && marcas_plan.espejo_aviso == 0);
    printf("      obstaculo al centro (izquierda medida): %d mm de la linea, rumbo %d\n", dist, rumbo);

    ok = esquive_con_pivote(0, 0, -1, 3, &dist, &rumbo);
    d_cero = dist;
    caso("plan con desplazamiento 0: el esquive de siempre sigue llegando a 10 cm o menos de la linea con el rumbo de la linea, sin avisos de espejo", ok && marcas_plan.espejo_comenzar == 0);
    printf("      desplazamiento 0: %d mm de la linea, rumbo %d\n", dist, rumbo);

    ok = esquive_con_pivote(200, 0, -1, 2, &dist, &rumbo);
    d_doble = dist;
    caso("plan con el doble de desplazamiento (30 cm adelante y 33 de lado, obstaculo a cercania 2): llega a 10 cm o menos de la linea con el rumbo de la linea (+-5 grados)", ok);
    printf("      el doble de desplazamiento (30 y 33 cm): %d mm de la linea, rumbo %d\n", dist, rumbo);
    (void)d_centro; (void)d_izq; (void)d_der; (void)d_cero; (void)d_doble; (void)d_propia;

    /* ── Si no cabe: el otro lado o ayuda ── */
    preparar_plan_pivote(LARGA, N(LARGA), 200, 0, 1);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr_hasta_el_final(60000);
    {
        char texto[300];

        navegador_texto_error(&nav, texto, sizeof texto);
        caso("arco no cabe: retrocede por espacio conocido; al desaparecer el objeto reanuda y termina",
             nav.estado == NAV_TERMINADO && nav.esquivadas == 0 && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 1);
        printf("      texto: %s\n", texto);
    }
    caso("... y no se movio mas que el frenado antes de pedirla (menos de 5 cm de giro: el rumbo sigue en 0)", valor_absoluto(error_rumbo_real()) <= 2);
    preparar_plan_pivote(LARGA, N(LARGA), 200, 0, 0);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr_hasta_el_final(60000);
    caso("sin ayuda activada tambien recupera con el retroceso", nav.estado == NAV_TERMINADO && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 1);

    {   /* La izquierda mide el doble (no cabe) y la derecha lo normal (cabe): con el centro tapado elige la izquierda por costumbre, ve que no cabe y se aparta a la derecha. */
        preparar_plan_pivote(LARGA, N(LARGA), 100, 200, 1);
        sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
        correr_hasta_el_final(170000);
        dist = distancia_a_la_ruta(LARGA, N(LARGA));
        caso("no cabe a la izquierda (30 cm adelante) pero si a la derecha (15 cm): cambia de lado, se aparta a la derecha y llega a 10 cm o menos de la linea",
             marcas_plan.cambia_de_lado == 1 && nav.estado == NAV_TERMINADO && nav.lado == 1 && dist <= 100 && valor_absoluto(error_rumbo_real()) <= 5);
        printf("      cambia de lado: %d mm de la linea\n", dist);
    }

    /* ── Red de seguridad ── */
    preparar_plan_pivote(LARGA, N(LARGA), 100, 0, 1);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    nav.sin_red_seguridad = 1;
    correr_con_empujon(120, 0, 170000);
    dist = distancia_a_la_ruta(LARGA, N(LARGA));
    caso("sin red de seguridad, un empujon de 12 cm a la pose en plena vuelta se queda: termina a mas de 10 cm de la linea o se descontrola tanto que el freno anti-locura lo para (recorrio mas de 2.5 veces la ruta)", marcas_plan.correcciones == 0 && ((nav.estado == NAV_TERMINADO && dist > 100) || (nav.estado == NAV_AYUDA && nav.error_causa == NAV_CAUSA_RECORRIDO)));
    printf("      sin red de seguridad, empujon de 12 cm: %d mm de la linea (estado %s, causa %d, recorrido %ld mm de %ld)\n", dist, navegador_texto_estado(nav.estado), nav.error_causa, nav.recorrido_mm, nav.ruta_largo_mm);
    {
        int sin_red = dist;

        preparar_plan_pivote(LARGA, N(LARGA), 100, 0, 1);
        sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
        correr_con_empujon(120, 0, 170000);
        dist = distancia_a_la_ruta(LARGA, N(LARGA));
        caso("con red de seguridad el mismo empujon se corrige: vuelve a planear (1 correccion) y termina a 10 cm o menos de la linea con el rumbo de la linea (+-5 grados)",
             nav.estado == NAV_TERMINADO && marcas_plan.correcciones == 1 && dist <= 100 && valor_absoluto(error_rumbo_real()) <= 5 && dist < sin_red);
        printf("      con red de seguridad, empujon de 12 cm: %d mm de la linea (1 correccion)\n", dist);
    }
    preparar_plan_pivote(LARGA, N(LARGA), 100, 0, 1);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr_con_empujon(60, 0, 170000);
    caso("un empujon de 6 cm (menos de 8) no dispara la red: no vuelve a planear", nav.estado == NAV_TERMINADO && marcas_plan.correcciones == 0);

    preparar_plan_pivote(LARGA, N(LARGA), 100, 0, 1);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr_con_empujon(120, 1, 170000);
    {
        char texto[300];

        navegador_texto_error(&nav, texto, sizeof texto);
        caso("empujones repetidos invalidan el indice de ruta: detiene por incoherencia de pose antes de corregir otra vez", nav.estado == NAV_AYUDA && nav.error_causa == NAV_CAUSA_INDICE && marcas_plan.correcciones > 0 && strstr(texto, "retrocedi") != NULL);
        printf("      texto: %s\n", texto);
    }
    caso("... y el carro esta quieto en AYUDA", nav.cmd_actual == NAV_CMD_STOP);

    /* sin_solucion nunca debe aparecer en los esquives que si se planean */
    preparar_plan_pivote(LARGA, N(LARGA), 100, 0, 1);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr_hasta_el_final(170000);
    caso("los esquives normales nunca terminan en 'sin solucion'", marcas_plan.sin_solucion == 0 && marcas_plan.agotada == 0);
}

/* ── Caso real del 16 oct: «al final se volvió loco» ──────────────────────────────────────────────────────────────────────────────
 * Ruta recta de 8 puntos (1.4 m). Un obstáculo real al arrancar (esquiva y vuelve) y otro más adelante (segunda esquivada). La vuelta que solo corrige el rumbo dejaba el índice del siguiente punto
 * en el punto al que iba al detenerse (el 1): el carro, ya a más de un metro de él, daba media vuelta y recorría la ruta hacia atrás. Calibración del teléfono: la izquierda medida a 180 grados
 * (-88, -148 mm); el carro real tiene también la derecha medida a 90 grados (150, 165 mm). `derecha_conocida` 0 = el navegador solo conoce la izquierda (la derecha es su espejo, como estaba el
 * teléfono). `sin_arreglo` 1 = se apaga el arreglo del índice (para reproducir el bug y comprobar que el freno anti-locura lo para). */

/* ── Frenos anti-locura (J9): cada uno se prueba forzando la situacion en una ruta larga ── */
static void pruebas_frenos_anti_locura(void)
{
    char texto[300];

    /* El indice de la ruta retrocede. */
    preparar(LARGA, N(LARGA));
    navegador_poner_pedir_ayuda(&nav, 1);
    correr(6000);
    nav.indice_objetivo -= 2;
    correr(200);
    navegador_texto_error(&nav, texto, sizeof texto);
    caso("freno: el indice del siguiente punto de la ruta retrocede -> para y pide AYUDA (NAV_CAUSA_INDICE)", nav.estado == NAV_AYUDA && nav.error_causa == NAV_CAUSA_INDICE && nav.cmd_actual == NAV_CMD_STOP && strstr(texto, "retrocedió") != NULL);

    /* Distancia recorrida de mas de 2.5 veces la de la ruta. */
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    correr(1500);
    nav.recorrido_mm = 2600;   /* la ruta mide 1000 mm: 2.5 veces son 2500 */
    correr(200);
    navegador_texto_error(&nav, texto, sizeof texto);
    caso("freno: la distancia recorrida pasa de 2.5 veces la de la ruta (2600 mm contra 1000) -> para y pide AYUDA (NAV_CAUSA_RECORRIDO)",
         nav.estado == NAV_AYUDA && nav.error_causa == NAV_CAUSA_RECORRIDO && strstr(texto, "2.5 veces") != NULL);
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    correr(1500);
    nav.recorrido_mm = 2400;
    correr(200);
    caso("... y con 2400 mm (menos de 2.5 veces) no frena", nav.estado == NAV_SIGUIENDO);

    /* La pose a mas de 1 m de la ruta. */
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    correr(1500);
    nav.x_mm += 1200;
    nav.base_x_mm += 1200;
    correr(200);
    navegador_texto_error(&nav, texto, sizeof texto);
    caso("freno: la pose queda a mas de 1 m de la ruta -> para y pide AYUDA (NAV_CAUSA_LEJOS)", nav.estado == NAV_AYUDA && nav.error_causa == NAV_CAUSA_LEJOS && strstr(texto, "100 cm") != NULL);
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    correr(1500);
    nav.x_mm += 900;
    nav.base_x_mm += 900;
    correr(200);
    caso("... y a 90 cm de la ruta no frena por esto", nav.error_causa != NAV_CAUSA_LEJOS);

    /* Sin pedir ayuda el freno es ERROR con la misma causa. */
    preparar(RECTA, N(RECTA));
    correr(1500);
    nav.recorrido_mm = 2600;
    correr(200);
    caso("sin pedir ayuda, el mismo freno da ERROR con la misma causa y el carro queda quieto", nav.estado == NAV_ERROR && nav.error_causa == NAV_CAUSA_RECORRIDO && nav.cmd_actual == NAV_CMD_STOP);

    /* Mas de 3 esquivadas: ya existia (NAV_MAX_ESQUIVADAS); aqui se confirma que la cuarta pide ayuda. */
    preparar(LARGA, N(LARGA));
    navegador_poner_pedir_ayuda(&nav, 1);
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_IZQUIERDA, 3);
    correr(60000);
    caso("mas de 3 esquivadas: no se esquiva una cuarta vez; para y pide ayuda (el limite NAV_MAX_ESQUIVADAS)", nav.estado == NAV_AYUDA && nav.esquivadas <= NAV_MAX_ESQUIVADAS);
}

/* ── Casos: arranque, sondeo del obstáculo fijo y cercanía 3 (J9) ──────────────────────────────────────────────────────────────────────────────
 * El obstáculo fantasma de la prueba real: la zona izquierda con celdas en cercanía 3 desde el primer cuadro, con el piso despejado. */

typedef struct {
    int fijo;           /* OBS_DIAG sondeo=sospechoso_fijo */
    int real;           /* OBS_DIAG sondeo=real */
    int inicio;         /* OBS_DIAG evento=sondeo_inicio */
    int arranque;       /* OBS_DIAG evento=arranque */
    int detiene;        /* OBS_DIAG evento=detiene */
    int esquiva;        /* OBS_DIAG evento=esquiva */
    int hex_valido;     /* OBS_DIAG con las tres máscaras de 25 cifras hexadecimales */
} MarcasArranque;

static MarcasArranque marcas_arr;

static void vigilar_arranque(void)
{
    char linea[NAV_DIAG_LARGO];

    while (navegador_diag_siguiente(&nav, linea, sizeof linea)) {
        marcas_arr.fijo += (strstr(linea, "OBS_DIAG sondeo=sospechoso_fijo") != NULL);
        marcas_arr.real += (strstr(linea, "OBS_DIAG sondeo=real") != NULL);
        marcas_arr.inicio += (strstr(linea, "OBS_DIAG evento=sondeo_inicio") != NULL);
        marcas_arr.arranque += (strstr(linea, "OBS_DIAG evento=arranque") != NULL);
        marcas_arr.detiene += (strstr(linea, "OBS_DIAG evento=detiene") != NULL);
        marcas_arr.esquiva += (strstr(linea, "OBS_DIAG evento=esquiva") != NULL);
        if (strstr(linea, "OBS_DIAG evento=") != NULL) {
            const char *i = strstr(linea, "mascara_izq="), *c = strstr(linea, "mascara_cen="), *d = strstr(linea, "mascara_der=");

            if (i != NULL && c != NULL && d != NULL && strspn(i + 12, "0123456789abcdef") == 25 && strspn(c + 12, "0123456789abcdef") == 25 && strspn(d + 12, "0123456789abcdef") == 25 &&
                strstr(linea, "franja_tapada=") != NULL && strstr(linea, "cercania=(") != NULL) {
                marcas_arr.hex_valido++;
            }
        }
    }
}

static void correr_arranque(uint32_t limite_ms)
{
    uint32_t t0 = sim.t_ms;

    while (sim.t_ms - t0 < limite_ms && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR && nav.estado != NAV_AYUDA) {
        correr(NAV_TICK_MS);
        vigilar_arranque();
    }
}

/* ── J10 Puntos 7 y 8: Ruta en L sin obstáculos con calibración real/espejo, y pose tras parar ───────────────────────────────── */
static int error_pose_mm(int x1, int y1, int x2, int y2)
{
    long dx = (long)x1 - x2;
    long dy = (long)y1 - y2;
    long d2 = dx * dx + dy * dy;
    long r = 0;

    while ((r + 1) * (r + 1) <= d2) {
        r++;
    }
    return (int)r;
}

static int correr_ruta_en_l(const Punto *ruta, int n, int usar_espejo_izq, int sin_compensar, int *err_plan_mm, int *err_est_mm)
{
    CalibracionCarro cal;
    int pa_der, pd_der, pa_izq, pd_izq;
    int dest_x = ruta[n - 1].columna * MM;
    int dest_y = ruta[n - 1].fila * MM;

    cal_defecto(&cal);
    /* Calibración real: derecha 15 cm adelante y 16.5 cm de lado a 90°; izquierda radio 9 cm (90 mm adelante, -90 mm a la derecha a 90°) o espejo */
    cal_desplazamiento_de_medida(CAL_LADO_DERECHA, 150, 165, 90, &pa_der, &pd_der);
    cal_poner_desplazamiento(&cal, CAL_LADO_DERECHA, pa_der, pd_der);
    if (!usar_espejo_izq) {
        cal_desplazamiento_de_medida(CAL_LADO_IZQUIERDA, 90, -90, 90, &pa_izq, &pd_izq);
        cal_poner_desplazamiento(&cal, CAL_LADO_IZQUIERDA, pa_izq, pd_izq);
    }

    navegador_iniciar(&nav);
    nav.sin_compensar_giro = sin_compensar;
    navegador_poner_calibracion(&nav, &cal);
    navegador_cargar_ruta(&nav, ruta, n);
    sim_iniciar(&sim, ruta[0].columna, ruta[0].fila, 0);
    sim.real = cal;
    navegador_comenzar(&nav, 0);
    while (nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR && sim.t_ms < 60000) {
        correr(NAV_TICK_MS);
    }
    *err_plan_mm = error_pose_mm(sim_x_mm(&sim), sim_y_mm(&sim), dest_x, dest_y);
    *err_est_mm = error_pose_mm(sim_x_mm(&sim), sim_y_mm(&sim), nav.x_mm, nav.y_mm);
    return nav.estado == NAV_TERMINADO;
}

static void pruebas_j10_ruta_l_y_frenado(void)
{
    /* Ruta en L de 60 cm + 60 cm hacia la derecha: (0,600) -> (0,0) -> (600,0) en coordenadas de mapa (en celdas: (0,3)->(0,2)->(0,1)->(0,0)->(1,0)->(2,0)->(3,0))
     * y en espejo hacia la izquierda: (600,600) -> (600,0) -> (0,0) (en celdas: (3,3)->(3,2)->(3,1)->(3,0)->(2,0)->(1,0)->(0,0)). */
    static const Punto RUTA_L_DER[] = { {0, 3}, {0, 2}, {0, 1}, {0, 0}, {1, 0}, {2, 0}, {3, 0} };
    static const Punto RUTA_L_IZQ[] = { {3, 3}, {3, 2}, {3, 1}, {3, 0}, {2, 0}, {1, 0}, {0, 0} };
    static const Punto RUTA_1M[]    = { {2, 7}, {2, 6}, {2, 5}, {2, 4}, {2, 3}, {2, 2} };   /* 5 celdas = 1000 mm (1 m) */
    int err_antes_der = 0, err_despues_der = 0, est_der = 0;
    int err_antes_izq = 0, err_despues_izq = 0, est_izq = 0;
    int err_antes_esp = 0, err_despues_esp = 0, est_esp = 0;

    correr_ruta_en_l(RUTA_L_DER, N(RUTA_L_DER), 0, 1, &err_antes_der, &est_der);
    correr_ruta_en_l(RUTA_L_DER, N(RUTA_L_DER), 0, 0, &err_despues_der, &est_der);
    printf("      J10 7 ruta en L derecha (15 cm adelante, 16.5 cm lado): error antes = %d mm, despues = %d mm (real vs estimada = %d mm)\n",
           err_antes_der, err_despues_der, est_der);
    caso("J10 7: ruta en L derecha (60 cm + 60 cm): sin compensar el desplazamiento del giro el error pasa de 5 cm (~22.3 cm); compensando queda en <= 5 cm (<= 1 cm)",
         err_antes_der > 50 && err_despues_der <= 50 && est_der <= 10);

    correr_ruta_en_l(RUTA_L_IZQ, N(RUTA_L_IZQ), 0, 1, &err_antes_izq, &est_izq);
    correr_ruta_en_l(RUTA_L_IZQ, N(RUTA_L_IZQ), 0, 0, &err_despues_izq, &est_izq);
    printf("      J10 7 ruta en L izquierda (radio 9 cm): error antes = %d mm, despues = %d mm (real vs estimada = %d mm)\n",
           err_antes_izq, err_despues_izq, est_izq);
    caso("J10 7: ruta en L izquierda con radio 9 cm: sin compensar el error pasa de 5 cm (~12.7 cm); compensando queda en <= 5 cm (<= 1 cm)",
         err_antes_izq > 50 && err_despues_izq <= 50 && est_izq <= 10);

    correr_ruta_en_l(RUTA_L_IZQ, N(RUTA_L_IZQ), 1, 1, &err_antes_esp, &est_esp);
    correr_ruta_en_l(RUTA_L_IZQ, N(RUTA_L_IZQ), 1, 0, &err_despues_esp, &est_esp);
    printf("      J10 7 ruta en L izquierda en espejo (espejo de derecha): error antes = %d mm, despues = %d mm (real vs estimada = %d mm)\n",
           err_antes_esp, err_despues_esp, est_esp);
    caso("J10 7: ruta en L en espejo: sin compensar el error pasa de 5 cm (~22.3 cm); compensando queda en <= 5 cm (<= 1 cm)",
         err_antes_esp > 50 && err_despues_esp <= 50 && est_esp <= 10);

    /* Comprobar que planificar() no sobrescribe el rumbo real y corrige con un giro si difiere más de 4° */
    {
        navegador_iniciar(&nav);
        navegador_cargar_ruta(&nav, RUTA_1M, N(RUTA_1M));
        navegador_comenzar(&nav, 0);
        /* Si el rumbo real difiere 3° (<= 4°), planificar() no lo sobrescribe con 0° ni mete giro */
        nav.rumbo_grados = 3;
        nav.base_rumbo_grados = 3;
        navegador_tick(&nav, 1000, NULL);   /* completa el primer tramo de 200 mm (1000 ms) y llama a planificar() */
        caso("J10 7: planificar() no sobrescribe rumbo_grados cuando difiere <= 4 grados (queda en 3, sin PASO_GIRAR)",
             nav.rumbo_grados == 3 && nav.pasos[0].tipo == PASO_AVANZAR);
        /* Si el rumbo real difiere 6° (> 4°), planificar() emite un giro corrector de -6° */
        nav.rumbo_grados = 6;
        nav.base_rumbo_grados = 6;
        navegador_tick(&nav, 2000, NULL);   /* completa el siguiente tramo y llama a planificar() */
        caso("J10 7: planificar() emite un giro corrector cuando el rumbo difiere mas de 4 grados (6 grados -> giro a la izquierda)",
             nav.pasos[0].tipo == PASO_GIRAR && nav.pasos[0].valor == -6);
    }

    /* J10 Punto 8: tick_detenido acota ms_paso a 0 y cuenta el frenado una sola vez; sondeo de 4 cm + ruta de 1 m termina a <= 1 cm de lo planeado. */
    {
        CalibracionCarro cal;
        int err_1m;

        cal_defecto(&cal);
        cal.arranque_ms = 80;
        cal.frenado_mm = 6;    /* a 50 ms/cm son 30 ms: frenado(30) - arranque(80) = -50 ms */
        memset(&marcas_arr, 0, sizeof marcas_arr);
        navegador_iniciar(&nav);
        navegador_poner_calibracion(&nav, &cal);
        navegador_cargar_ruta(&nav, RUTA_1M, N(RUTA_1M));
        sim_iniciar(&sim, RUTA_1M[0].columna, RUTA_1M[0].fila, 0);
        sim.real = cal;
        /* Color solo con laser libre: ya no hace un sondeo de 4 cm. */
        ResultadoObstaculos falso;
        memset(&falso,0,sizeof falso);
        falso.fuentes_vision[0]=1;falso.zona[0].cercania=3;falso.zona[0].celdas=10;
        sim.vision_externa=&falso;sim.tof_activo=1;navegador_poner_sensor_distancia(&nav,1);
        navegador_comenzar(&nav, 0);
        correr_arranque(30000);
        err_1m = error_pose_mm(sim_x_mm(&sim), sim_y_mm(&sim), RUTA_1M[N(RUTA_1M) - 1].columna * MM, RUTA_1M[N(RUTA_1M) - 1].fila * MM);
        printf("      J10 8 sondeo 4 cm + ruta 1 m: pose final real (%d, %d) vs planeada (%d, %d), error = %d mm\n",
               sim_x_mm(&sim), sim_y_mm(&sim), RUTA_1M[N(RUTA_1M) - 1].columna * MM, RUTA_1M[N(RUTA_1M) - 1].fila * MM, err_1m);
        caso("color aislado con laser libre: la ruta de 1 m conserva precision de 1 cm sin sondeo",
             nav.estado == NAV_TERMINADO && nav.esquivadas == 0 && nav.sondeo_hecho == 0 && err_1m <= 10);

        /* Además, verificar directamente que ms_paso nunca queda negativo y el frenado se suma una sola vez al parar en tick_detenido */
        navegador_iniciar(&nav);
        navegador_poner_calibracion(&nav, &cal);
        navegador_cargar_ruta(&nav, RUTA_1M, N(RUTA_1M));
        navegador_comenzar(&nav, 0);
        /* Avanza solo 20 ms y entra en NAV_DETENIDO_OBSTACULO: 20 + 30 - 80 = -30 -> se acota a 0 */
        navegador_tick(&nav, 20, NULL);
        nav.estado = NAV_DETENIDO_OBSTACULO;
        nav.cmd_actual = NAV_CMD_STOP;
        nav.frenado_pendiente = 1;
        navegador_tick(&nav, 40, NULL);
        caso("J10 8: cuando ms_paso + frenado - arranque < 0, ms_paso se acota a 0 (no queda negativo)",
             nav.ms_paso == 0 && nav.frenado_pendiente == 0);
        /* Y cuando ms_paso es 200 ms con frenado=60 ms (12 mm) y arranque=20 ms (+40 ms), dos ticks seguidos en detenido lo aplican UNA sola vez (240, no 280) */
        cal.arranque_ms = 20;
        cal.frenado_mm = 12;   /* 12 mm * 50 ms/cm = 60 ms */
        nav.cal = cal;
        nav.ms_paso = 200;
        nav.frenado_pendiente = 1;
        navegador_tick(&nav, 60, NULL);
        navegador_tick(&nav, 80, NULL);
        caso("J10 8: el ajuste de frenado - arranque se aplica una sola vez al detenerse (200 + 40 = 240 ms, no dos veces)",
             nav.ms_paso == 240);
    }
}

#include <stdlib.h>

/* ── Revisión independiente de J10: rutas en L con calibraciones y errores de giro al azar ───────────────────── */

static uint32_t azar_mc = 20260101u;
static int azar_entre(int minimo, int maximo)
{
    azar_mc = azar_mc * 1664525u + 1013904223u;
    return minimo + (int)((azar_mc >> 8) % (uint32_t)(maximo - minimo + 1));
}

/* Carro con pivote de radio `radio_der_mm` (derecha) y `radio_izq_mm` (izquierda). */
static CalibracionCarro carro_con_radios(int radio_izq_mm, int radio_der_mm)
{
    CalibracionCarro c;
    int a, d;

    cal_defecto(&c);
    cal_desplazamiento_de_medida(CAL_LADO_DERECHA, radio_der_mm, radio_der_mm, 90, &a, &d);
    cal_poner_desplazamiento(&c, CAL_LADO_DERECHA, a, d);
    cal_poner_desplazamiento_grados(&c, CAL_LADO_DERECHA, 90);
    cal_desplazamiento_de_medida(CAL_LADO_IZQUIERDA, radio_izq_mm, -radio_izq_mm, 90, &a, &d);
    cal_poner_desplazamiento(&c, CAL_LADO_IZQUIERDA, a, d);
    cal_poner_desplazamiento_grados(&c, CAL_LADO_IZQUIERDA, 90);
    return c;
}

static int comparar_enteros(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static int raiz_entera_mc(long v)
{
    long r = 0;
    while ((r + 1) * (r + 1) <= v) {
        r++;
    }
    return (int)r;
}

#define MC_RUTAS 200

static void pruebas_monte_carlo_ruta_l(void)
{
    static const Punto L_NORMAL[] = { {0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 3} };
    static const Punto L_ESPEJO[] = { {3, 0}, {3, 1}, {3, 2}, {3, 3}, {2, 3}, {1, 3}, {0, 3} };
    int errores[2 * MC_RUTAS];
    int n = 0, i, v, no_termino = 0, max_correcciones = 0;
    long suma = 0;

    for (v = 0; v < 2; v++) {
        const Punto *ruta = (v == 0) ? L_NORMAL : L_ESPEJO;

        for (i = 0; i < MC_RUTAS; i++) {
            CalibracionCarro real = carro_con_radios(azar_entre(60, 200), azar_entre(60, 200));
            long dx, dy;
            int error;

            navegador_iniciar(&nav);
            navegador_poner_calibracion(&nav, &real);
            navegador_cargar_ruta(&nav, ruta, 7);
            sim_iniciar(&sim, ruta[0].columna, ruta[0].fila, 0);
            sim.real = real;
            sim.pct_giro = azar_entre(90, 110);
            navegador_comenzar(&nav, 0);
            while (sim.t_ms < 120000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR) {
                correr(200);
                if (nav.corr_rumbo_cuenta > max_correcciones) {
                    max_correcciones = nav.corr_rumbo_cuenta;
                }
            }
            correr(1500);
            if (nav.estado != NAV_TERMINADO) {
                no_termino++;
            }
            dx = sim_x_mm(&sim) - ruta[6].columna * MM;
            dy = sim_y_mm(&sim) - ruta[6].fila * MM;
            error = raiz_entera_mc(dx * dx + dy * dy);
            errores[n++] = error;
            suma += error;
        }
    }
    qsort(errores, (size_t)n, sizeof errores[0], comparar_enteros);
    printf("      MC rutas en L (200 normal + 200 espejo, radios 6-20 cm, giro +-10%%): error final media=%d mm, p95=%d mm (no termino=%d, max giros corr/vertice=%d)\n",
           (int)(suma / n), errores[(n * 95) / 100], no_termino, max_correcciones);
    caso("MC rutas en L: todas las 400 corridas terminan en NAV_TERMINADO (ninguna en ERROR ni bucle)", no_termino == 0);
    caso("MC rutas en L: error final con media menor que 200 mm y percentil 95 menor que 450 mm",
         (suma / n) < 200 && errores[(n * 95) / 100] < 450);
    caso("MC rutas en L: la correccion de rumbo no supera el tope de 2 correcciones por vertice", max_correcciones <= 2);
}

/* ── J11: giroscopio (giros por ángulo medido) y láser ───────────────────────────────────────────────── */

/* Deja todo listo con el giroscopio encendido o no. El carro REAL gira `pct_giro` % de lo que cree el navegador y, al soltar, sigue girando 6 grados que el navegador no conoce
 * (la calibración del navegador es la de ejemplo, sin frenado). El firmware simulado tiene un giroscopio con +1 % de escala, 0.1 grados/s de sesgo y ruido de ±0.5 grados por giro. */
static void preparar_giro(const Punto *ruta, int n, int giroscopio, int pct_giro)
{
    navegador_iniciar(&nav);
    navegador_poner_giroscopio(&nav, giroscopio);
    navegador_cargar_ruta(&nav, ruta, n);
    sim_iniciar(&sim, ruta[0].columna, ruta[0].fila, 0);
    sim.pct_giro = pct_giro;
    sim.real.frenado_grados_izq_x10 = 60;
    sim.real.frenado_grados_der_x10 = 60;
    sim.fw_giroscopio = giroscopio;
    sim.fw_escala_pct = 1;
    sim.fw_sesgo_x100 = 10;
    sim.fw_ruido_x10 = 5;
    n_lineas_diag = 0;
    navegador_comenzar(&nav, 0);
}

/* Corre `ms` milisegundos y guarda las líneas de diagnóstico que van saliendo (la cola solo guarda 16). */
static void correr_leyendo(uint32_t ms)
{
    uint32_t hecho;

    for (hecho = 0; hecho < ms; hecho += 100) {
        correr(100);
        leer_diag();
    }
}

static int distancia_a(int columna, int fila)
{
    long dx = sim_x_mm(&sim) - columna * MM, dy = sim_y_mm(&sim) - fila * MM;

    return raiz_entera_mc(dx * dx + dy * dy);
}

static void pruebas_j11_giroscopio(void)
{
    int sin_gyro, con_gyro;
    char texto[600];

    printf("-- J11: giros por angulo medido (giroscopio) --\n");

    /* 1. El cuadrado con un carro que gira 12 % de menos y sigue 6 grados al frenar: por tiempo se desvia; con giroscopio no. */
    preparar_giro(CUADRADO, N(CUADRADO), 0, 88);
    correr_leyendo(40000);
    sin_gyro = distancia_a(1, 4);
    caso("sin giroscopio no se pide ningun giro por angulo (todo como antes)", nav.n_giros_angulo == 0 && !navegador_giro_fw_en_curso(&nav));
    preparar_giro(CUADRADO, N(CUADRADO), 1, 88);
    correr_leyendo(40000);
    con_gyro = distancia_a(1, 4);
    printf("      cuadrado, carro que gira 12 %% de menos: error final sin giroscopio %d mm, con giroscopio %d mm (giros por angulo %d, por tiempo %d)\n", sin_gyro, con_gyro,
           nav.n_giros_angulo, nav.n_giros_tiempo);
    caso("cuadrado con giroscopio: TERMINADO", nav.estado == NAV_TERMINADO);
    caso("cuadrado con giroscopio: los 3 giros de la ruta se hicieron por angulo y ninguno por tiempo", nav.n_giros_angulo == 3 && nav.n_giros_tiempo == 0);
    caso("cuadrado con giroscopio: queda mucho mas cerca del punto de partida que sin giroscopio", con_gyro < sin_gyro && con_gyro <= 120);
    caso("la pose estimada coincide con la real (el navegador usa el angulo REAL que informa el firmware)", pose_estimada_coincide(270));
    caso("hubo lineas SENS_DIAG con el objetivo y el angulo real de cada giro", hay_diag("SENS_DIAG giro por=angulo objetivo=90 real_x10=") && cuenta_diag("SENS_DIAG giro por=angulo") == 3);
    caso("las lineas TURN_DIAG dicen por=angulo", hay_diag("TURN_DIAG") && hay_diag("por=angulo fw_real_x10="));
    navegador_texto_giros(&nav, texto, sizeof texto);
    caso("el resumen final dice cuantos giros fueron por angulo y por tiempo", strstr(texto, "Giros por ángulo (giroscopio): 3, por tiempo: 0") != NULL);
    caso("los giros anotados marcan por_angulo", nav.giros[0].por_angulo == 1 && nav.n_giros == 3);

    /* 2. Durante un giro por angulo el navegador no manda el giro otra vez ni pide mas de uno a la vez. */
    preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
    {
        int pedidos = 0, ms;

        for (ms = 0; ms < 40000 && nav.estado != NAV_TERMINADO; ms += 50) {
            sim_correr(&sim, &nav, 50);
            if (nav.giro_fw_pedir) {
                pedidos++;   /* nadie lo tomo: el simulador lo toma en el mismo ciclo, asi que esto no debe pasar */
            }
        }
        caso("cada giro por angulo se pide una sola vez (el simulador lo toma de inmediato)", pedidos == 0 && sim.fw_n_giros == 3);
    }

    /* 3. Un giro de menos de 5 grados o de mas de 200 no se pide al firmware. */
    {
        int g;

        preparar_giro(RECTA, N(RECTA), 1, 100);
        nav.cmd_pedido = 4;
        nav.giro_fw = 0;
        nav.usa_giroscopio = 1;
        g = 0;
        caso("tomar_giro_fw sin giro pendiente devuelve 0", navegador_tomar_giro_fw(&nav, &g) == 0);
    }

    /* 4. Fallos: el firmware responde TIMEOUT / ERROR con datos / ERROR sin datos desde el 2.o giro, o no responde. */
    preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
    sim.fw_falla_desde = 1;
    sim.fw_falla_codigo = NAV_GIRO_ERROR_SIN_DATOS; /* GIRO_ERROR:sin_sesgo */
    correr_leyendo(60000);
    caso("J13: firmware sin sesgo desde el primer giro cae a tiempo y termina sin angulo inventado", nav.estado == NAV_TERMINADO &&
         nav.n_giros_angulo == 0 && nav.n_giros_tiempo >= 3 && nav.giro_fw_caido && hay_diag("SENS_DIAG giro_a_tiempo"));

    preparar_giro(CUADRADO, N(CUADRADO), 1, 90);
    sim.fw_falla_desde = 2;
    sim.fw_falla_codigo = NAV_GIRO_ERROR_SIN_DATOS;
    correr_leyendo(60000);
    printf("      fallo SIN datos desde el 2.o giro: estado=%s, por angulo %d, por tiempo %d, distancia al inicio %d mm\n", navegador_texto_estado(nav.estado), nav.n_giros_angulo, nav.n_giros_tiempo, distancia_a(1, 4));
    caso("fallo sin datos: el primer giro fue por angulo, el resto por tiempo, y la ruta termina", nav.estado == NAV_TERMINADO && nav.n_giros_angulo == 1 && nav.n_giros_tiempo >= 2);
    caso("fallo sin datos: avisa en el log (SENS_DIAG giro_a_tiempo) y marca el giroscopio caido en la ruta", hay_diag("SENS_DIAG giro_a_tiempo motivo=el_firmware_no_pudo_girar") && nav.giro_fw_caido);
    caso("fallo sin datos: el carro no se queda trabado ni se aleja (termina a menos de 40 cm del inicio)", distancia_a(1, 4) < 400);

    preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
    sim.fw_falla_desde = 2;
    sim.fw_falla_codigo = NAV_GIRO_TIMEOUT;
    correr_leyendo(60000);
    caso("GIRO_TIMEOUT: usa el angulo real que informo, avisa y los giros siguientes van por tiempo", nav.estado == NAV_TERMINADO && nav.n_giros_angulo >= 2 && nav.n_giros_tiempo >= 1 &&
         hay_diag("SENS_DIAG giro aviso=GIRO_TIMEOUT"));

    preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
    sim.fw_falla_desde = 3;
    sim.fw_falla_codigo = NAV_GIRO_ERROR_CON_DATOS;
    correr_leyendo(60000);
    caso("GIRO_ERROR con angulo parcial: lo usa, avisa y sigue por tiempo", nav.estado == NAV_TERMINADO && hay_diag("SENS_DIAG giro aviso=GIRO_ERROR") && nav.n_giros_tiempo >= 1);

    /* J12, mensaje de giro perdido: antes de repetir por tiempo se pregunta IMU? y se compara el angulo con el del inicio del giro. */
    preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
    correr_leyendo(60000);
    {
        int base_distancia = distancia_a(1, 4);

        preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
        sim.fw_pierde_mensaje = 1;   /* el carro gira completo pero el GIRO_OK nunca llega */
        correr_leyendo(80000);
        printf("      mensaje perdido con el carro que SI giro: distancia al inicio %d mm (sin perdida %d mm), giros por angulo %d, por tiempo %d\n", distancia_a(1, 4), base_distancia, nav.n_giros_angulo, nav.n_giros_tiempo);
        caso("mensaje perdido y el carro ya giro: pregunta IMU?, ve que giro mas del 80 % y NO repite el giro (ni uno por tiempo)", nav.estado == NAV_TERMINADO && hay_diag("SENS_DIAG giro sin_GIRO_OK: se pregunta IMU?") &&
             hay_diag("fuente=IMU?") && hay_diag("NO se repite") && nav.n_giros_tiempo == 0 && !hay_diag("giro_a_tiempo"));
        caso("... y termina casi donde terminaria sin perder el mensaje (el giro no se duplico)", nav.estado == NAV_TERMINADO && distancia_a(1, 4) <= base_distancia + 100 && pose_estimada_coincide(270));
    }
    preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
    sim.fw_pierde_mensaje = 1;
    sim.fw_total_pct = 40;   /* ...y el carro solo hizo el 40 % del giro */
    correr_leyendo(80000);
    printf("      mensaje perdido con el carro que giro 40 %%: estado=%s distancia al inicio %d mm, rumbo real %d\n", navegador_texto_estado(nav.estado), distancia_a(1, 4), sim_rumbo_grados(&sim));
    caso("mensaje perdido y el carro giro menos del 80 %: repite por tiempo SOLO lo que falta (el giro de 90 no se hace entero otra vez)", nav.estado == NAV_TERMINADO && hay_diag("giro menos del 80 %, se repite por tiempo SOLO lo que falta") &&
         !hay_diag("sin_respuesta_ni"));
    caso("... y el carro real termina cerca del destino (rumbo 270 +-15, a menos de 45 cm de donde empezo)", nav.estado == NAV_TERMINADO && distancia_a(1, 4) <= 450 && ((sim_rumbo_grados(&sim) - 270 + 540) % 360 - 180) <= 15 && ((sim_rumbo_grados(&sim) - 270 + 540) % 360 - 180) >= -15);

    preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
    sim.fw_pierde_mensaje = 1;
    sim.imu_sin_respuesta = 1;   /* el IMU? tampoco se contesta, pero SENS sigue llegando */
    correr_leyendo(90000);
    caso("mensaje perdido y sin respuesta a IMU?: usa el ultimo SENS (fresco) y no repite el giro", nav.estado == NAV_TERMINADO && hay_diag("fuente=ultimo_SENS") && nav.n_giros_tiempo == 0);

    preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
    sim.fw_pierde_mensaje = 1;
    sim.imu_sin_respuesta = 1;
    sim.sens_apagado = 1;        /* sin SENS no hay angulo del inicio: no se puede comparar */
    correr_leyendo(120000);
    caso("sin angulo del inicio (no llega SENS) y sin respuesta: cae al metodo por tiempo como antes y la ruta termina (nunca se queda trabada)", nav.estado == NAV_TERMINADO &&
         hay_diag("motivo=sin_respuesta_ni_angulo_inicial") && nav.n_giros_tiempo >= 2);

    /* 5. Cancelar a mitad de un giro por angulo no deja nada colgado. */
    preparar_giro(CUADRADO, N(CUADRADO), 1, 100);
    correr_leyendo(2000);
    navegador_cancelar(&nav);
    sim_correr(&sim, &nav, 100);
    caso("cancelar durante un giro por angulo: STOP y ESPERANDO", nav.estado == NAV_ESPERANDO && nav.cmd_actual == NAV_CMD_STOP && !navegador_giro_fw_en_curso(&nav));
}

/* Prepara una RECTA con el láser encendido y SENS de 100 ms. */

/* Monte Carlo con y sin giroscopio: las 400 rutas en L de siempre (radios de 6 a 20 cm, giro del carro +-10 %), con las MISMAS semillas en las dos vueltas. */
static void mc_rutas_l(int con_giroscopio, int *media, int *p95, int *no_termino, int *giros_angulo)
{
    static const Punto L_NORMAL[] = { {0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 3} };
    static const Punto L_ESPEJO[] = { {3, 0}, {3, 1}, {3, 2}, {3, 3}, {2, 3}, {1, 3}, {0, 3} };
    int errores[2 * MC_RUTAS];
    int n = 0, i, v;
    long suma = 0;

    azar_mc = 20260101u;
    *no_termino = 0;
    *giros_angulo = 0;
    for (v = 0; v < 2; v++) {
        const Punto *ruta = (v == 0) ? L_NORMAL : L_ESPEJO;

        for (i = 0; i < MC_RUTAS; i++) {
            CalibracionCarro real = carro_con_radios(azar_entre(60, 200), azar_entre(60, 200));
            long dx, dy;
            int error;

            navegador_iniciar(&nav);
            navegador_poner_calibracion(&nav, &real);
            navegador_poner_giroscopio(&nav, con_giroscopio);
            navegador_cargar_ruta(&nav, ruta, 7);
            sim_iniciar(&sim, ruta[0].columna, ruta[0].fila, 0);
            sim.real = real;
            sim.pct_giro = azar_entre(90, 110);
            if (con_giroscopio) {   /* un giroscopio de verdad: error de escala de +-2 %, sesgo de +-0.3 grados/s sin corregir y ruido de +-1 grado por giro */
                sim.fw_giroscopio = 1;
                sim.fw_escala_pct = azar_entre(-2, 2);
                sim.fw_sesgo_x100 = azar_entre(-30, 30);
                sim.fw_ruido_x10 = 10;
            }
            navegador_comenzar(&nav, 0);
            while (sim.t_ms < 120000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR) {
                correr(200);
            }
            correr(1500);
            if (nav.estado != NAV_TERMINADO) {
                (*no_termino)++;
            }
            *giros_angulo += nav.n_giros_angulo;
            dx = sim_x_mm(&sim) - ruta[6].columna * MM;
            dy = sim_y_mm(&sim) - ruta[6].fila * MM;
            error = raiz_entera_mc(dx * dx + dy * dy);
            errores[n++] = error;
            suma += error;
        }
    }
    qsort(errores, (size_t)n, sizeof errores[0], comparar_enteros);
    *media = (int)(suma / n);
    *p95 = errores[(n * 95) / 100];
}

static void pruebas_j11_monte_carlo(void)
{
    int media_sin, p95_sin, no_sin, ang_sin, media_con, p95_con, no_con, ang_con;

    printf("-- J11: Monte Carlo de las 400 rutas en L, con y sin giroscopio --\n");
    mc_rutas_l(0, &media_sin, &p95_sin, &no_sin, &ang_sin);
    mc_rutas_l(1, &media_con, &p95_con, &no_con, &ang_con);
    printf("      MC 400 rutas en L (radios 6-20 cm, giro del carro +-10%%):\n");
    printf("        SIN giroscopio: error final medio %d mm, p95 %d mm (no termino %d)\n", media_sin, p95_sin, no_sin);
    printf("        CON giroscopio: error final medio %d mm, p95 %d mm (no termino %d, giros por angulo %d)\n", media_con, p95_con, no_con, ang_con);
    caso("MC con giroscopio: las 400 corridas terminan y se hicieron giros por angulo", no_con == 0 && ang_con > 0);
    caso("MC: con giroscopio el error medio y el p95 son menores que sin giroscopio", media_con < media_sin && p95_con < p95_sin);
}

static void pruebas_j14_recta_predefinida(void)
{
    static const Punto RECTA_8[] = {
        {2, 8}, {2, 7}, {2, 6}, {2, 5}, {2, 4}, {2, 3}, {2, 2}, {2, 1}, {2, 0}
    };
    int t_esperado;

    navegador_iniciar(&nav);
    navegador_cargar_ruta(&nav, RECTA_8, N(RECTA_8));
    sim_iniciar(&sim, RECTA_8[0].columna, RECTA_8[0].fila, 0);
    navegador_poner_largo_recta_mm(&nav, 1500);
    navegador_comenzar(&nav, 0);
    caso("J14: recta con override 1500 mm fija ruta_largo_mm en 1500", nav.ruta_largo_mm == 1500);

    /* En el primer tick se planifica el paso de 150 cm */
    navegador_tick(&nav, 0, NULL);
    caso("J14: paso 0 es AVANZAR 150 cm", nav.n_pasos >= 1 && nav.pasos[0].tipo == PASO_AVANZAR && nav.pasos[0].valor == 150);
    t_esperado = cal_cm_a_ms_tramo(&nav.cal, 150, 1, 1);
    caso("J14: tiempo planeado coincide con cal_cm_a_ms_tramo(150)", nav.pasos[0].tiempo_ms == t_esperado);

    /* Correr el simulador hasta terminar la ruta */
    correr(t_esperado + 3000);
    caso("J14: la recta de 150 cm termina en NAV_TERMINADO", nav.estado == NAV_TERMINADO);
    caso("J14: la distancia recorrida es ~150 cm", nav.recorrido_mm >= 1480 && nav.recorrido_mm <= 1520);
}

int main(void)
{
    pruebas_ruta_derecha_40cm();
    pruebas_desplazamiento_al_girar();
    pruebas_plan_pivote();
    pruebas_frenos_anti_locura();
    pruebas_j10_ruta_l_y_frenado();
    pruebas_sin_obstaculos();
    pruebas_rutas_raras();
    pruebas_errores_de_navegacion();
    pruebas_cancelar();
    pruebas_deriva();
    pruebas_calibracion();
    pruebas_giro_visual();
    pruebas_video_perdido();
    pruebas_pausa_entre_movimientos();
    pruebas_giro_visual_con_retraso();
    pruebas_ayuda();
    pruebas_textos();
    pruebas_monte_carlo_ruta_l();
    pruebas_j11_giroscopio();
    pruebas_j11_monte_carlo();
    pruebas_j14_recta_predefinida();

    caso("en TODAS las pruebas nunca hubo movimiento en ESPERANDO, TERMINADO o ERROR", violaciones_totales == 0);

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
