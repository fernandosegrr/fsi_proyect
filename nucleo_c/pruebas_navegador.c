/*
 * pruebas_navegador.c — Pruebas de navegador.c en PC con un carro simulado.
 *
 * GUÍA DE LECTURA
 *  1. Cada caso arma una ruta, un simulador (carro + obstáculos programados), corre el ciclo
 *     de 50 ms y revisa el resultado: estado final, posición REAL del carro simulado,
 *     secuencia de comandos y cuántas veces se detuvo.
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
static const Punto CON_SALTO[] = { {0, 0}, {3, 0} };

#define N(ruta) ((int)(sizeof(ruta) / sizeof((ruta)[0])))

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
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
static void cuadro_zona(ResultadoObstaculos *obs, int zona, int cercania)
{
    memset(obs, 0, sizeof *obs);
    obs->zona[zona].hay_obstaculo = (cercania > 0);
    obs->zona[zona].cercania = cercania;
    obs->zona[zona].celdas = cercania > 0 ? 10 : 0;
}

static void cuadro_centro(ResultadoObstaculos *obs, int cercania)
{
    cuadro_zona(obs, ZONA_CENTRO, cercania);
}

/* Prepara un navegador que avanza por la RECTA y le da cuadros cada `periodo_ms` con `cercania` en el
 * centro desde la hora `t_obstaculo_ms`. Los ticks son exactamente a la hora de cada cuadro (sin
 * simulador), para medir el retraso real del antirrebote. Devuelve la hora del primer STOP o -1. */
static long hora_del_stop(int periodo_ms, int t_obstaculo_ms, int zona, int cercania, int hasta_ms)
{
    ResultadoObstaculos libre, con_obstaculo;
    int t;

    navegador_iniciar(&nav);
    navegador_cargar_ruta(&nav, RECTA, N(RECTA));
    navegador_comenzar(&nav, 0);
    cuadro_centro(&libre, 0);
    cuadro_zona(&con_obstaculo, zona, cercania);
    for (t = 0; t <= hasta_ms; t += periodo_ms) {
        const ResultadoObstaculos *cuadro = (t >= t_obstaculo_ms) ? &con_obstaculo : &libre;

        if (navegador_tick(&nav, (uint32_t)t, cuadro) == NAV_CMD_STOP && t > 100) {
            return t;
        }
    }
    return -1;
}

static void pruebas_antirrebote(void)
{
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 1550, ZONA_IZQUIERDA, 2);   /* un solo cuadro de ruido a un lado */
    correr(12000);
    caso("antirrebote: un cuadro de ruido de media distancia a un lado NO detiene", nav.estado == NAV_TERMINADO &&
         sim_contar_cambios_a(&sim, NAV_CMD_STOP) == 1 && nav.esquivadas == 0);

    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 1600, ZONA_DERECHA, 2);   /* dos cuadros (100 ms) a un lado: menos que MS_CONFIRMAR_LEJOS */
    correr(12000);
    caso("antirrebote: obstaculo a un lado visto solo 100 ms (menos de 150 ms) tampoco detiene",
         nav.estado == NAV_TERMINADO && sim_contar_cambios_a(&sim, NAV_CMD_STOP) == 1);

    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 1550, ZONA_CENTRO, 3);   /* un solo cuadro MUY cerca: confirmacion inmediata */
    correr(3000);
    caso("un solo cuadro con obstaculo muy cerca (cercania 3) SI detiene al instante (no espera 150 ms)",
         sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000) == 1500);

    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_IZQUIERDA, 2);   /* a un lado y permanente: se confirma tras MS_CONFIRMAR_LEJOS */
    correr(2500);
    caso("obstaculo de media distancia a un lado, permanente: detiene a los 150 ms (MS_CONFIRMAR_LEJOS)",
         sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000) == 1650);

    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 1);   /* LEJOS de verdad (cercania 1): solo se informa */
    correr(12000);
    caso("obstaculo LEJANO (cercania 1) al centro, permanente (por ejemplo una escalera al fondo): NO frena",
         nav.estado == NAV_TERMINADO && sim_contar_cambios_a(&sim, NAV_CMD_STOP) == 1);

    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 1550, ZONA_DERECHA, 3);   /* un cuadro muy cerca a un lado: inmediato */
    correr(3000);
    caso("un solo cuadro con obstaculo muy cerca (cercania 3) a un lado SI detiene al instante",
         sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000) == 1500);

    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 0, 99999, ZONA_IZQUIERDA, 1);
    sim_agregar_obstaculo(&sim, 0, 99999, ZONA_DERECHA, 1);
    correr(12000);
    caso("obstaculos lejanos a los lados (centro libre) no detienen", nav.estado == NAV_TERMINADO &&
         sim_contar_cambios_a(&sim, NAV_CMD_STOP) == 1 && nav.esquivadas == 0);
}

/* El antirrebote cuenta MILISEGUNDOS: con cuadros cada 80 ms y cada 120 ms el retraso es casi el mismo. */
static void pruebas_antirrebote_en_ms(void)
{
    /* Obstaculo muy cerca (inmediato): un solo cuadro. El STOP sale en el mismo tick del primer cuadro. */
    caso("cuadros cada 80 ms, cercania 3: STOP con el primer cuadro con obstaculo (1 cuadro)",
         hora_del_stop(80, 1600, ZONA_CENTRO, 3, 5000) == 1600);
    caso("cuadros cada 120 ms, cercania 3: STOP con el primer cuadro con obstaculo (1 cuadro)",
         hora_del_stop(120, 1560, ZONA_CENTRO, 3, 5000) == 1560);

    /* Cercania 2 en el centro: tambien inmediato. */
    caso("cuadros cada 80 ms, cercania 2 en el centro: STOP inmediato con 1 cuadro",
         hora_del_stop(80, 1600, ZONA_CENTRO, 2, 5000) == 1600);
    caso("cuadros cada 120 ms, cercania 2 en el centro: STOP inmediato con 1 cuadro",
         hora_del_stop(120, 1560, ZONA_CENTRO, 2, 5000) == 1560);

    /* Lejos (cercania 1): hay que verlo 150 ms y en al menos 2 cuadros.
     * Cuadros cada 80 ms: el primero a 1600, el segundo a 1680 (80 ms < 150), el tercero a 1760 (160 ms >= 150). */
    caso("cuadros cada 80 ms, obstaculo de media distancia a un lado: se confirma al tercer cuadro (160 ms despues del primero)",
         hora_del_stop(80, 1600, ZONA_IZQUIERDA, 2, 5000) == 1760);
    /* Cuadros cada 120 ms: 1560, 1680 (120 ms < 150), 1800 (240 ms >= 150). */
    caso("cuadros cada 120 ms, obstaculo de media distancia a un lado: se confirma al tercer cuadro (240 ms despues del primero)",
         hora_del_stop(120, 1560, ZONA_DERECHA, 2, 5000) == 1800);
    /* Cuadros cada 50 ms (el ritmo del tick): 4 cuadros = 150 ms justos. */
    caso("cuadros cada 50 ms, obstaculo de media distancia a un lado: se confirma justo a los 150 ms",
         hora_del_stop(50, 1500, ZONA_IZQUIERDA, 2, 5000) == 1650);

    /* El retraso por antirrebote nunca pasa de MS_CONFIRMAR_LEJOS + un periodo de cuadro. */
    caso("el retraso del antirrebote en obstaculo a confirmar es menos de 150 ms + un periodo (80 y 120 ms)",
         hora_del_stop(80, 1600, ZONA_IZQUIERDA, 2, 5000) - 1600 < MS_CONFIRMAR_LEJOS + 80 &&
         hora_del_stop(120, 1560, ZONA_DERECHA, 2, 5000) - 1560 < MS_CONFIRMAR_LEJOS + 120);

    /* Un cuadro lejano suelto entre cuadros libres no frena. */
    {
        ResultadoObstaculos libre, ruido_lejano;
        int t, alguna_vez_stop = 0;

        cuadro_centro(&libre, 0);
        cuadro_zona(&ruido_lejano, ZONA_IZQUIERDA, 2);
        navegador_iniciar(&nav);
        navegador_cargar_ruta(&nav, RECTA, N(RECTA));
        navegador_comenzar(&nav, 0);
        for (t = 0; t <= 4000; t += 80) {
            const ResultadoObstaculos *cuadro = (t == 1600) ? &ruido_lejano : &libre;

            if (navegador_tick(&nav, (uint32_t)t, cuadro) == NAV_CMD_STOP && t > 100) {
                alguna_vez_stop = 1;
            }
        }
        caso("cuadros cada 80 ms: un cuadro lejano aislado (ruido) no detiene", !alguna_vez_stop);
    }

    /* Intermitente: lejano, libre, lejano, libre... nunca llega a 2 cuadros seguidos: no frena. */
    {
        ResultadoObstaculos libre, lejano;
        int t, n = 0, alguna_vez_stop = 0;

        cuadro_centro(&libre, 0);
        cuadro_zona(&lejano, ZONA_DERECHA, 2);
        navegador_iniciar(&nav);
        navegador_cargar_ruta(&nav, RECTA, N(RECTA));
        navegador_comenzar(&nav, 0);
        for (t = 0; t <= 4000; t += 120, n++) {
            const ResultadoObstaculos *cuadro = (n % 2 == 0) ? &lejano : &libre;

            if (navegador_tick(&nav, (uint32_t)t, cuadro) == NAV_CMD_STOP && t > 100) {
                alguna_vez_stop = 1;
            }
        }
        caso("cuadros cada 120 ms: obstaculo lejano intermitente (nunca 2 cuadros seguidos) no detiene", !alguna_vez_stop);
    }

    /* Para volver a avanzar: el frente libre MS_CONFIRMAR_LIBRE ms. Cuadros cada 80 ms. */
    {
        ResultadoObstaculos libre, cerca;
        int t, t_reanuda = -1;
        ComandoNav cmd;

        cuadro_centro(&libre, 0);
        cuadro_centro(&cerca, 3);
        navegador_iniciar(&nav);
        navegador_cargar_ruta(&nav, RECTA, N(RECTA));
        navegador_comenzar(&nav, 0);
        for (t = 0; t <= 3000; t += 80) {
            /* obstaculo entre 1600 y 1760 (2 cuadros), luego libre */
            const ResultadoObstaculos *cuadro = (t >= 1600 && t < 1760) ? &cerca : &libre;

            cmd = navegador_tick(&nav, (uint32_t)t, cuadro);
            if (t_reanuda < 0 && t >= 1760 && cmd == NAV_CMD_FORWARD) {
                t_reanuda = t;
            }
        }
        caso("cuadros cada 80 ms: tras detenerse, reanuda cuando el frente lleva 250 ms libre",
             t_reanuda > 0 && t_reanuda - 1760 >= MS_CONFIRMAR_LIBRE && t_reanuda - 1760 < MS_CONFIRMAR_LIBRE + 80 &&
             nav.esquivadas == 0);
    }
}

static void pruebas_detencion_y_esquivar(void)
{
    long t_stop;

    /* Obstaculo muy cerca al centro desde 1500 ms: confirmacion inmediata, el STOP sale con el primer cuadro. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 3);
    correr(1700);
    t_stop = sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000);
    caso("detiene con STOP en el primer cuadro con el obstaculo cerca (1500 ms, sin esperar mas cuadros)",
         t_stop == 1500 && nav.estado == NAV_DETENIDO_OBSTACULO);

    preparar(RECTA, N(RECTA));
    sim.periodo_cuadro_ms = 100;
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 3);
    correr(2000);
    t_stop = sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000);
    caso("con cuadros cada 100 ms el STOP sale a los 1500 ms (el cuadro llega justo)", t_stop == 1500);

    /* Cuadros cada 80 y 120 ms (no son multiplos del tick de 50 ms): el cuadro llega al primer tick siguiente. */
    preparar(RECTA, N(RECTA));
    sim.periodo_cuadro_ms = 80;
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 3);
    correr(2500);
    t_stop = sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000);
    caso("con cuadros cada 80 ms: STOP en el primer cuadro con obstaculo (llega en el tick de 1550 ms)", t_stop == 1550);

    preparar(RECTA, N(RECTA));
    sim.periodo_cuadro_ms = 120;
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 3);
    correr(2500);
    t_stop = sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000);
    caso("con cuadros cada 120 ms: STOP en el primer cuadro con obstaculo (llega en el tick de 1600 ms)", t_stop == 1600);

    /* El obstaculo desaparece mientras el carro esta detenido: reanuda sin esquivar. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 1700, ZONA_CENTRO, 3);   /* desaparece antes de la pausa de 500 ms */
    correr(14000);
    caso("obstaculo que desaparece: se detiene, reanuda sin esquivar y llega",
         nav.estado == NAV_TERMINADO && nav.esquivadas == 0 && sim_contar_cambios_a(&sim, NAV_CMD_STOP) == 2 &&
         cerca_de(2, 3, 80));
    if (!(nav.estado == NAV_TERMINADO && nav.esquivadas == 0 && cerca_de(2, 3, 80))) {
        informar_posicion("desaparece");
    }

    /* Obstaculo que obliga a esquivar por la izquierda y se reincorpora. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr(30000);
    t_stop = sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000);
    caso("esquiva por la izquierda: STOP, LEFT, avanza de lado, RIGHT y sigue",
         t_stop >= 0 && sim_primer_cambio_a(&sim, NAV_CMD_LEFT, (uint32_t)t_stop) > t_stop &&
         sim_primer_cambio_a(&sim, NAV_CMD_RIGHT, (uint32_t)t_stop) > sim_primer_cambio_a(&sim, NAV_CMD_LEFT, (uint32_t)t_stop));
    caso("tras esquivar se reincorpora y llega cerca del destino (2,3)",
         nav.estado == NAV_TERMINADO && nav.esquivadas == 1 && cerca_de(2, 3, 120));
    if (!(nav.estado == NAV_TERMINADO && nav.esquivadas == 1 && cerca_de(2, 3, 120))) {
        informar_posicion("esquivar");
    }

    /* Izquierda bloqueada: esquiva por la derecha (primer giro de esquivar = RIGHT). */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 1500, 3500, ZONA_IZQUIERDA, 3);   /* ya no se ve cuando el carro se aparta */
    correr(30000);
    t_stop = sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000);
    caso("izquierda bloqueada: esquiva por la derecha (RIGHT primero, despues LEFT para volver)",
         t_stop >= 0 && sim_primer_cambio_a(&sim, NAV_CMD_RIGHT, (uint32_t)t_stop) > t_stop &&
         sim_primer_cambio_a(&sim, NAV_CMD_LEFT, (uint32_t)t_stop) > sim_primer_cambio_a(&sim, NAV_CMD_RIGHT, (uint32_t)t_stop) &&
         nav.estado == NAV_TERMINADO && cerca_de(2, 3, 120));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(2, 3, 120))) {
        informar_posicion("esquivar derecha");
    }
}

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
    correr(4000);
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

    /* Obstaculo permanente al centro pero con los lados libres: esquiva NAV_MAX_ESQUIVADAS veces y luego ERROR. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 0, 9999999, ZONA_CENTRO, 2);
    correr(80000);
    caso("obstaculo permanente: esquiva el maximo de veces y termina en ERROR (MAX_ESQUIVADAS)",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_MAX_ESQUIVADAS &&
         nav.esquivadas == NAV_MAX_ESQUIVADAS && sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);
    if (!(nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_MAX_ESQUIVADAS)) {
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

int main(void)
{
    pruebas_sin_obstaculos();
    pruebas_rutas_raras();
    pruebas_antirrebote();
    pruebas_antirrebote_en_ms();
    pruebas_detencion_y_esquivar();
    pruebas_errores_de_navegacion();
    pruebas_cancelar();
    pruebas_deriva();
    pruebas_calibracion();
    pruebas_textos();

    caso("en TODAS las pruebas nunca hubo movimiento en ESPERANDO, TERMINADO o ERROR", violaciones_totales == 0);

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
