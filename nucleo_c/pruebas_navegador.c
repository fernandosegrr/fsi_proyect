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
    obs->zona[zona].celdas = cercania > 0 ? NAV_CERC3_AREA_INMEDIATA : 0;
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

    /* Frenar desde lejos (opción para el video del carro): cercanía 1 en el CENTRO también frena, tras 2 cuadros y 60 ms. */
    preparar(RECTA, N(RECTA));
    navegador_poner_frenar_lejos_centro(&nav, 1);
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 1);
    correr(2500);
    caso("frenar desde lejos activado: un obstaculo LEJANO (cercania 1) al centro SI detiene, a los 60 ms (2 cuadros de 50 ms)",
         sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000) == 1550 || sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000) == 1600);
    preparar(RECTA, N(RECTA));
    navegador_poner_frenar_lejos_centro(&nav, 1);
    sim_agregar_obstaculo(&sim, 1500, 1550, ZONA_CENTRO, 1);   /* un solo cuadro lejano: ruido */
    correr(3000);
    caso("frenar desde lejos activado: un solo cuadro lejano (ruido) NO detiene", sim_contar_cambios_a(&sim, NAV_CMD_STOP) <= 1);
    preparar(RECTA, N(RECTA));
    navegador_poner_frenar_lejos_centro(&nav, 1);
    sim_agregar_obstaculo(&sim, 0, 99999, ZONA_IZQUIERDA, 1);
    sim_agregar_obstaculo(&sim, 0, 99999, ZONA_DERECHA, 1);
    correr(12000);
    caso("frenar desde lejos activado: cercania 1 a los LADOS sigue sin detener (el unico STOP de mas es la espera de 4 cuadros iguales al arrancar con algo a la vista)", nav.estado == NAV_TERMINADO && sim_contar_cambios_a(&sim, NAV_CMD_STOP) <= 2 && nav.esquivadas == 0);
    caso("navegador_poner_frenar_lejos_centro con argumentos malos: error", navegador_poner_frenar_lejos_centro(NULL, 1) == NAV_ERR_ARGUMENTO &&
         navegador_poner_frenar_lejos_centro(&nav, 2) == NAV_ERR_ARGUMENTO);

    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 1550, ZONA_DERECHA, 3);   /* un cuadro muy cerca a un lado: inmediato */
    correr(3000);
    caso("un solo cuadro con obstaculo muy cerca (cercania 3) a un lado SI detiene al instante",
         sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000) == 1500);

    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 0, 99999, ZONA_IZQUIERDA, 1);
    sim_agregar_obstaculo(&sim, 0, 99999, ZONA_DERECHA, 1);
    correr(12000);
    caso("obstaculos lejanos a los lados (centro libre) no detienen (el STOP de mas es la espera de 4 cuadros iguales al arrancar con algo a la vista)", nav.estado == NAV_TERMINADO &&
         sim_contar_cambios_a(&sim, NAV_CMD_STOP) <= 2 && nav.esquivadas == 0);
}

/* El antirrebote cuenta MILISEGUNDOS: con cuadros cada 80 ms y cada 120 ms el retraso es casi el mismo. */
static void pruebas_antirrebote_en_ms(void)
{
    /* Obstaculo muy cerca (inmediato): un solo cuadro. El STOP sale en el mismo tick del primer cuadro. */
    caso("cuadros cada 80 ms, cercania 3: STOP con el primer cuadro con obstaculo (1 cuadro)",
         hora_del_stop(80, 1600, ZONA_CENTRO, 3, 5000) == 1600);
    caso("cuadros cada 120 ms, cercania 3: STOP con el primer cuadro con obstaculo (1 cuadro)",
         hora_del_stop(120, 1560, ZONA_CENTRO, 3, 5000) == 1560);

    /* Cercania 2 en el centro: exige 2 cuadros seguidos (segundo cuadro a 1680 ms con 80 ms y a 1680 ms con 120 ms). */
    caso("cuadros cada 80 ms, cercania 2 en el centro: exige 2 cuadros (STOP en el segundo cuadro, 1680 ms)",
         hora_del_stop(80, 1600, ZONA_CENTRO, 2, 5000) == 1680);
    caso("cuadros cada 120 ms, cercania 2 en el centro: exige 2 cuadros (STOP en el segundo cuadro, 1680 ms)",
         hora_del_stop(120, 1560, ZONA_CENTRO, 2, 5000) == 1680);

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
        } else if (estaba_avanzando && sim.cmd_previo == NAV_CMD_STOP && nav.estado != NAV_TERMINADO) {
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
static long enesimo_cambio_a(ComandoNav comando, int n)
{
    int i;

    for (i = 0; i < sim.n_cambios; i++) {
        if (sim.cambios[i].comando == comando && --n == 0) {
            return (long)sim.cambios[i].t_ms;
        }
    }
    return -1;
}

/* Cuánto duró el n-ésimo tramo con `comando` (hasta el siguiente cambio de comando); -1 si no hay. */
static long duracion_del_tramo(ComandoNav comando, int n)
{
    int i;

    for (i = 0; i + 1 < sim.n_cambios; i++) {
        if (sim.cambios[i].comando == comando && --n == 0) {
            return (long)sim.cambios[i + 1].t_ms - (long)sim.cambios[i].t_ms;
        }
    }
    return -1;
}

/* Lista la ruta RECTA con la calibración `cal` (la del navegador y la del carro simulado son la misma). */
static void preparar_con_calibracion(const CalibracionCarro *cal)
{
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, cal);
    navegador_cargar_ruta(&nav, RECTA, N(RECTA));
    sim_iniciar(&sim, RECTA[0].columna, RECTA[0].fila, 0);
    sim.real = *cal;
    navegador_comenzar(&nav, 0);
}

/* Un cuadro armado a mano: el centro muy cerca y las celdas de cada lado como se pidan (sin obstáculo en los lados). */
static ResultadoObstaculos cuadro_con_celdas(int celdas_izq, int celdas_der)
{
    ResultadoObstaculos cuadro;

    memset(&cuadro, 0, sizeof cuadro);
    cuadro.zona[ZONA_CENTRO].hay_obstaculo = 1;
    cuadro.zona[ZONA_CENTRO].cercania = 3;
    cuadro.zona[ZONA_CENTRO].celdas = 12;
    cuadro.zona[ZONA_IZQUIERDA].celdas = celdas_izq;
    cuadro.zona[ZONA_DERECHA].celdas = celdas_der;
    return cuadro;
}

/* Primer giro de apartarse con esos cuadros: LEFT o RIGHT (STOP si no giró). */
static ComandoNav primer_giro_con_celdas(int celdas_izq, int celdas_der)
{
    ResultadoObstaculos cuadro = cuadro_con_celdas(celdas_izq, celdas_der);
    ResultadoObstaculos libre;
    uint32_t t;

    memset(&libre, 0, sizeof libre);
    navegador_iniciar(&nav);
    navegador_cargar_ruta(&nav, RECTA, N(RECTA));
    navegador_comenzar(&nav, 0);
    for (t = 0; t < 3000; t += NAV_TICK_MS) {
        ComandoNav comando = navegador_tick(&nav, t, (t < 1000) ? &libre : &cuadro);

        if (nav.estado == NAV_ESQUIVANDO && (comando == NAV_CMD_LEFT || comando == NAV_CMD_RIGHT)) {
            return comando;
        }
    }
    return NAV_CMD_STOP;
}

static void pruebas_esquive_objetos_grandes(void)
{
    CalibracionCarro cal;
    long t_stop, t_atras, t_giro;
    int i;

    /* ── Elección del lado: el que tiene menos celdas de obstáculo ── */
    caso("lado: los dos libres y la derecha con menos celdas de obstaculo (1 contra 4): gira a la DERECHA", primer_giro_con_celdas(4, 1) == NAV_CMD_RIGHT);
    caso("lado: los dos libres y la izquierda con menos celdas (1 contra 4): gira a la IZQUIERDA", primer_giro_con_celdas(1, 4) == NAV_CMD_LEFT);
    caso("lado: los dos libres con las mismas celdas (empate): conserva el comportamiento de siempre, IZQUIERDA", primer_giro_con_celdas(3, 3) == NAV_CMD_LEFT);
    caso("lado: los dos libres sin celdas (0 y 0): IZQUIERDA como siempre", primer_giro_con_celdas(0, 0) == NAV_CMD_LEFT);

    /* ── Objeto grande que ocupa todo el frente (centro e izquierda): se aparta por la derecha, que está libre ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo_celdas(&sim, 1500, 3000, ZONA_CENTRO, 3, 30);
    sim_agregar_obstaculo_celdas(&sim, 1500, 3000, ZONA_IZQUIERDA, 3, 25);
    correr(30000);
    t_stop = sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000);
    caso("objeto grande (centro e izquierda ocupados): se aparta por la DERECHA (RIGHT primero) y llega al destino",
         t_stop >= 0 && sim_primer_cambio_a(&sim, NAV_CMD_RIGHT, (uint32_t)t_stop) > t_stop && sim_primer_cambio_a(&sim, NAV_CMD_LEFT, (uint32_t)t_stop) > 0 &&
         nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150));
    caso("... y no necesito reintentos ni retroceder (el obstaculo ya no esta al mirar)", nav.reintentos == 0 && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 0);
    if (!(nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150))) {
        informar_posicion("objeto grande");
    }

    /* ── Pared: ocupa los tres lados, no hay hacia dónde apartarse: espera y ERROR como antes ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_CENTRO, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_IZQUIERDA, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_DERECHA, 3, 40);
    correr(9000);
    caso("pared de lado a lado: no gira ni retrocede (sin lado libre), ERROR por LADOS_BLOQUEADOS y STOP",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_LADOS_BLOQUEADOS && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 0 &&
         sim_contar_cambios_a(&sim, NAV_CMD_LEFT) == 0 && sim_contar_cambios_a(&sim, NAV_CMD_RIGHT) == 0 && sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);

    /* ── El obstáculo desaparece tras el giro: al mirar está libre, sin retroceder ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 3000, ZONA_CENTRO, 3);
    correr(30000);
    caso("obstaculo que desaparece tras el giro: no retrocede, avanza de lado y llega", sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 0 &&
         nav.estado == NAV_TERMINADO && nav.esquivadas == 1 && cerca_de(2, 3, 150));

    /* ── Sigue muy cerca tras el giro, y al 1.er reintento (retroceder 15 cm + 45 grados) ya está libre ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 4300, ZONA_CENTRO, 3);   /* aún está al terminar el giro (2900) y el retroceso; se va antes del 1.er reintento */
    correr(40000);
    caso("sigue muy cerca tras el giro: UN reintento (un retroceso) y despues avanza y llega al destino",
         sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 1 && nav.estado == NAV_TERMINADO && nav.esquivadas == 1 && cerca_de(2, 3, 150));
    if (!(sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 1 && nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150))) {
        informar_posicion("un reintento");
        printf("      reversas=%d\n", sim_contar_cambios_a(&sim, NAV_CMD_REVERSE));
    }

    /* ── Esquina: pared de frente y a la izquierda; tras el giro a la derecha la otra pared tapa el frente hasta el 2.º reintento ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo_celdas(&sim, 1500, 6100, ZONA_CENTRO, 3, 30);
    sim_agregar_obstaculo_celdas(&sim, 1500, 3000, ZONA_IZQUIERDA, 3, 30);
    correr(60000);
    caso("esquina: gira a la derecha, reintenta DOS veces (45 y 90 grados mas) y al final sale y llega",
         sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 2 && nav.estado == NAV_TERMINADO && nav.esquivadas == 1);
    caso("esquina: la secuencia es RIGHT, REVERSE, RIGHT, REVERSE, RIGHT (siempre del mismo lado)",
         enesimo_cambio_a(NAV_CMD_RIGHT, 3) > enesimo_cambio_a(NAV_CMD_REVERSE, 2) && enesimo_cambio_a(NAV_CMD_REVERSE, 2) > enesimo_cambio_a(NAV_CMD_RIGHT, 2) &&
         enesimo_cambio_a(NAV_CMD_RIGHT, 2) > enesimo_cambio_a(NAV_CMD_REVERSE, 1) && enesimo_cambio_a(NAV_CMD_REVERSE, 1) > enesimo_cambio_a(NAV_CMD_RIGHT, 1));
    if (!(nav.estado == NAV_TERMINADO && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 2)) {
        informar_posicion("esquina");
    }
    t_giro = enesimo_cambio_a(NAV_CMD_RIGHT, 1);
    caso("esquina: los giros duran 90, 45 y 90 grados (900, 450 y 900 ms con 10 ms por grado; +-100 ms)",
         t_giro > 0 && valor_absoluto((int)(duracion_del_tramo(NAV_CMD_RIGHT, 1) - 900)) <= 100 && valor_absoluto((int)(duracion_del_tramo(NAV_CMD_RIGHT, 2) - 450)) <= 100 &&
         valor_absoluto((int)(duracion_del_tramo(NAV_CMD_RIGHT, 3) - 900)) <= 100);

    /* ── 2 reintentos agotados y el frente sigue muy cerca: ERROR y STOP como antes ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 9999999, ZONA_CENTRO, 3);
    correr(40000);
    caso("obstaculo que nunca se va: 2 retrocesos y ERROR (ESQUIVANDO) con STOP, sin un tercer reintento",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_ESQUIVANDO && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 2 &&
         nav.reintentos == NAV_MAX_REINTENTOS && sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);
    caso("... y despues del ERROR el carro no se mueve mas (STOP en los comandos que siguen)",
         navegador_tick(&nav, sim.t_ms + 50, NULL) == NAV_CMD_STOP && navegador_tick(&nav, sim.t_ms + 100, NULL) == NAV_CMD_STOP);
    if (!(nav.estado == NAV_ERROR && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 2)) {
        informar_posicion("reintentos agotados");
    }

    /* ── Cuánto retrocede: 15 cm (750 ms con 50 ms/cm) ── */
    t_atras = duracion_del_tramo(NAV_CMD_REVERSE, 1);
    printf("      retroceso: %ld ms\n", t_atras);
    caso("cada retroceso dura 15 cm de motor: 750 ms con la calibracion de ejemplo (50 ms/cm), +-60", valor_absoluto((int)(t_atras - 750)) <= 60 &&
         valor_absoluto((int)(duracion_del_tramo(NAV_CMD_REVERSE, 2) - 750)) <= 60);

    /* ── Retroceso calibrado: si el carro va más lento hacia atrás (100 ms/cm), el tiempo se duplica y la distancia sigue siendo 15 cm ── */
    cal_defecto(&cal);
    cal.medidos |= CAL_MEDIDO_ATRAS;
    cal.ms_por_cm_atras_x10 = 1000;
    preparar_con_calibracion(&cal);
    sim_agregar_obstaculo(&sim, 1500, 9999999, ZONA_CENTRO, 3);
    correr(60000);
    caso("retroceso calibrado a 100 ms/cm: el retroceso dura 1500 ms (los mismos 15 cm), +-60", valor_absoluto((int)(duracion_del_tramo(NAV_CMD_REVERSE, 1) - 1500)) <= 60);
    caso("el navegador sabe que el retroceso esta calibrado", navegador_retroceso_calibrado(&nav) && !navegador_retroceso_calibrado(NULL));
    cal_defecto(&cal);
    preparar_con_calibracion(&cal);
    caso("sin calibrar el retroceso, el navegador lo dice (para que la app avise)", !navegador_retroceso_calibrado(&nav));

    /* ── Tope absoluto: aunque la calibración pida mucho (500 ms/cm hacia atrás = 7500 ms), NO pasa de NAV_RETROCESO_MAX_MS ── */
    cal_defecto(&cal);
    cal.medidos |= CAL_MEDIDO_ATRAS;
    cal.ms_por_cm_atras_x10 = 5000;
    preparar_con_calibracion(&cal);
    sim_agregar_obstaculo(&sim, 1500, 9999999, ZONA_CENTRO, 3);
    correr(80000);
    caso("calibracion absurda (500 ms/cm hacia atras): el retroceso se corta en NAV_RETROCESO_MAX_MS (2000 ms), +-60",
         duracion_del_tramo(NAV_CMD_REVERSE, 1) > 0 && duracion_del_tramo(NAV_CMD_REVERSE, 1) <= NAV_RETROCESO_MAX_MS + 60);

    /* ── Con retraso de video: el primer cuadro después del giro todavía muestra el carro girando ── */
    /* El obstaculo (visto con 320 ms de retraso) sigue en los cuadros hasta 3220, aunque el carro ya esta quieto desde 2900. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 3220, ZONA_CENTRO, 3);
    correr(40000);
    caso("sin avisar el retraso, el cuadro viejo parece un obstaculo: retrocede de mas (falsa alarma)", sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) >= 1);
    preparar(RECTA, N(RECTA));
    navegador_poner_latencia_visual(&nav, 320);
    sim_agregar_obstaculo(&sim, 1500, 3220, ZONA_CENTRO, 3);
    correr(40000);
    caso("avisando el retraso de 320 ms, espera un cuadro posterior: no retrocede y llega", sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 0 &&
         nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150));

    /* ── Cancelar mientras retrocede: STOP al instante ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 9999999, ZONA_CENTRO, 3);
    correr(3000);
    for (i = 0; i < 40 && nav.cmd_actual != NAV_CMD_REVERSE; i++) {
        correr(NAV_TICK_MS);
    }
    caso("(antes de cancelar) el carro esta retrocediendo", nav.cmd_actual == NAV_CMD_REVERSE && nav.estado == NAV_ESQUIVANDO);
    navegador_cancelar(&nav);
    caso("cancelar mientras retrocede: ESPERANDO y STOP en el siguiente tick", nav.estado == NAV_ESPERANDO && navegador_tick(&nav, sim.t_ms, NULL) == NAV_CMD_STOP);

    /* ── Sin cámara no hay a quién mirar: no se retrocede jamas (el obstaculo nunca se vio) ── */
    preparar(RECTA, N(RECTA));
    sim.periodo_cuadro_ms = 0;
    sim_agregar_obstaculo(&sim, 0, 9999999, ZONA_CENTRO, 3);
    correr(15000);
    caso("sin cuadros de la camara el carro ni se detiene ni retrocede (llega por tiempo)", sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 0 && nav.estado == NAV_TERMINADO);

    /* ── El retroceso entra en la posición estimada (con la calibración del retroceso) ── */
    cal_defecto(&cal);
    cal.medidos |= CAL_MEDIDO_ATRAS;
    cal.ms_por_cm_atras_x10 = 1000;
    preparar_con_calibracion(&cal);
    sim_agregar_obstaculo(&sim, 1500, 4800, ZONA_CENTRO, 3);
    correr(60000);
    caso("con retroceso calibrado distinto del avance, la posicion estimada sigue cerca de la real al terminar (+-120 mm)",
         nav.estado == NAV_TERMINADO && valor_absoluto(nav.x_mm - sim_x_mm(&sim)) <= 120 && valor_absoluto(nav.y_mm - sim_y_mm(&sim)) <= 120 && cerca_de(2, 3, 150));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150))) {
        informar_posicion("retroceso calibrado");
    }
}


/* ── Casos: el corredor de trayectoria (corredor.h) en las decisiones del navegador ───────────── */

/* Corre la ruta RECTA con un cuadro fijo (el mismo en cada tick desde 1000 ms) y dice qué comando mandó. */
static ComandoNav comando_con_cuadro(const ResultadoObstaculos *cuadro, uint32_t hasta_ms, int frenar_lejos)
{
    ResultadoObstaculos libre;
    uint32_t tiempo;
    ComandoNav comando = NAV_CMD_STOP;

    memset(&libre, 0, sizeof libre);
    navegador_iniciar(&nav);
    navegador_cargar_ruta(&nav, RECTA, N(RECTA));
    navegador_poner_frenar_lejos_centro(&nav, frenar_lejos);
    navegador_comenzar(&nav, 0);
    for (tiempo = 0; tiempo < hasta_ms; tiempo += NAV_TICK_MS) {
        comando = navegador_tick(&nav, tiempo, (tiempo < 1000) ? &libre : cuadro);
    }
    return comando;
}

static void pruebas_corredor_en_navegador(void)
{
    ResultadoObstaculos cuadro;
    ComandoNav comando;

    /* Sin corredor (los campos en 0): un obstaculo muy cerca al centro frena, como siempre. */
    memset(&cuadro, 0, sizeof cuadro);
    cuadro.zona[ZONA_CENTRO].hay_obstaculo = 1;
    cuadro.zona[ZONA_CENTRO].cercania = 3;
    cuadro.zona[ZONA_CENTRO].celdas = 12;
    comando = comando_con_cuadro(&cuadro, 1300, 0);
    caso("corredor apagado (campos en 0): obstaculo al centro con cercania 3 frena", comando == NAV_CMD_STOP && nav.estado == NAV_DETENIDO_OBSTACULO);

    /* El mismo obstaculo marcado FUERA del corredor: solo avisa, no frena y la ruta termina. */
    cuadro.zona[ZONA_CENTRO].fuera_corredor = 1;
    comando = comando_con_cuadro(&cuadro, 1600, 0);
    caso("obstaculo FUERA del corredor (cercania 3): no frena, sigue avanzando", comando == NAV_CMD_FORWARD && nav.estado == NAV_SIGUIENDO &&
         nav.zona_hay[ZONA_CENTRO] == 1 && nav.zona_frena[ZONA_CENTRO] == 0);
    comando = comando_con_cuadro(&cuadro, 14000, 0);
    caso("... y la ruta termina sin detenerse ni esquivar", nav.estado == NAV_TERMINADO && nav.esquivadas == 0);

    /* Lo mismo en un lado: un obstaculo de cercania 3 a la izquierda fuera del corredor no frena. */
    memset(&cuadro, 0, sizeof cuadro);
    cuadro.zona[ZONA_IZQUIERDA].hay_obstaculo = 1;
    cuadro.zona[ZONA_IZQUIERDA].cercania = 3;
    cuadro.zona[ZONA_IZQUIERDA].fuera_corredor = 1;
    comando = comando_con_cuadro(&cuadro, 1600, 0);
    caso("obstaculo cercano a un lado FUERA del corredor: no frena", comando == NAV_CMD_FORWARD);
    cuadro.zona[ZONA_IZQUIERDA].fuera_corredor = 0;
    comando = comando_con_cuadro(&cuadro, 1300, 0);
    caso("el mismo obstaculo a un lado DENTRO del corredor: si frena (cercania 3 inmediata)", comando == NAV_CMD_STOP);

    /* La cercania del corredor manda: la zona dice 3 pero lo que cae dentro del corredor esta lejos (cercania 1): no frena. */
    memset(&cuadro, 0, sizeof cuadro);
    cuadro.zona[ZONA_CENTRO].hay_obstaculo = 1;
    cuadro.zona[ZONA_CENTRO].cercania = 3;
    cuadro.zona[ZONA_CENTRO].cercania_corredor = 1;
    comando = comando_con_cuadro(&cuadro, 1600, 0);
    caso("la parte del obstaculo dentro del corredor esta lejos (cercania_corredor 1): no frena aunque la zona diga 3", comando == NAV_CMD_FORWARD);
    comando = comando_con_cuadro(&cuadro, 1300, 1);
    caso("... salvo con frenar desde lejos en el centro (cercania 1 al centro frena a los 2 cuadros)", comando == NAV_CMD_STOP);
    cuadro.zona[ZONA_CENTRO].cercania_corredor = 3;
    cuadro.zona[ZONA_CENTRO].cercania = 1;
    comando = comando_con_cuadro(&cuadro, 1300, 0);
    caso("al reves: la zona dice 1 pero lo que esta dentro del corredor esta muy cerca (3): frena", comando == NAV_CMD_STOP);

    /* Elegir lado: un obstaculo a la izquierda FUERA del corredor sigue contando como ocupado (no se aparta hacia el). */
    {
        ResultadoObstaculos libre;
        uint32_t tiempo;
        ComandoNav giro = NAV_CMD_STOP;

        memset(&libre, 0, sizeof libre);
        memset(&cuadro, 0, sizeof cuadro);
        cuadro.zona[ZONA_CENTRO].hay_obstaculo = 1;
        cuadro.zona[ZONA_CENTRO].cercania = 3;
        cuadro.zona[ZONA_CENTRO].celdas = 12;
        cuadro.zona[ZONA_IZQUIERDA].hay_obstaculo = 1;
        cuadro.zona[ZONA_IZQUIERDA].cercania = 2;
        cuadro.zona[ZONA_IZQUIERDA].fuera_corredor = 1;   /* a la izquierda hay algo, pero no frena */
        navegador_iniciar(&nav);
        navegador_cargar_ruta(&nav, RECTA, N(RECTA));
        navegador_comenzar(&nav, 0);
        for (tiempo = 0; tiempo < 3500 && giro == NAV_CMD_STOP; tiempo += NAV_TICK_MS) {
            ComandoNav c = navegador_tick(&nav, tiempo, (tiempo < 1000) ? &libre : &cuadro);

            if (nav.estado == NAV_ESQUIVANDO && (c == NAV_CMD_LEFT || c == NAV_CMD_RIGHT)) {
                giro = c;
            }
        }
        caso("elegir lado: la izquierda tiene un obstaculo fuera del corredor: el navegador se aparta por la DERECHA (sigue viendolo ocupado)", giro == NAV_CMD_RIGHT);
    }
}

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
static int correr_hasta_terminar_vuelta(uint32_t limite_ms)
{
    int paso = 0;
    uint32_t t0 = sim.t_ms;

    while (sim.t_ms - t0 < limite_ms) {
        correr(NAV_TICK_MS);
        if (nav.estado == NAV_REINCORPORANDO) {
            paso = 1;
        }
        if (nav.estado == NAV_ERROR) {
            return 0;
        }
        if (paso && nav.estado != NAV_REINCORPORANDO && nav.estado != NAV_DETENIDO_OBSTACULO && nav.estado != NAV_ESQUIVANDO) {
            return 1;
        }
    }
    return 0;
}

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
static int vuelta_reorienta_y_pausa(int desde)
{
    int i, giros_antes = 0, ok = 1;
    long fin_anterior = -1;
    ComandoNav anterior = NAV_CMD_STOP;

    for (i = desde; i < sim.n_cambios; i++) {
        ComandoNav c = sim.cambios[i].comando;

        if (c == NAV_CMD_STOP) {
            fin_anterior = (long)sim.cambios[i].t_ms;
        } else {
            if (anterior == NAV_CMD_STOP && fin_anterior >= 0 && (long)sim.cambios[i].t_ms - fin_anterior < 250) {
                ok = 0;                                  /* pausa demasiado corta entre dos movimientos */
            }
            if (c == NAV_CMD_FORWARD && giros_antes == 0) {
                ok = 0;                                  /* avanzó recto sin haber girado antes */
            }
            if (c == NAV_CMD_LEFT || c == NAV_CMD_RIGHT) {
                giros_antes++;
            }
        }
        anterior = c;
    }
    return ok;
}

static void pruebas_reincorporacion(void)
{
    char texto[NAV_DIAG_LARGO];
    int i, cambios_antes;

    /* ── Obstáculo al centro (los dos lados libres: se aparta por la izquierda) y vuelve a la línea x = 400 ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    cambios_antes = 0;
    {
        int llego = 0;
        uint32_t t0 = sim.t_ms;

        n_lineas_diag = 0;
        while (sim.t_ms - t0 < 40000 && !llego) {
            correr(NAV_TICK_MS);
            leer_diag();
            if (nav.estado == NAV_REINCORPORANDO && cambios_antes == 0) {
                cambios_antes = sim.n_cambios;
            }
            llego = (cambios_antes > 0 && nav.estado != NAV_REINCORPORANDO);
        }
        caso("volver a la linea (obstaculo al centro): termina la vuelta sin error", llego && nav.estado == NAV_SIGUIENDO);
    }
    caso("... la pose final del carro REAL queda a 10 cm o menos de la linea", distancia_a_la_ruta(RECTA, N(RECTA)) <= 100);
    caso("... y la pose estimada coincide con la real y el rumbo es el de la linea", pose_estimada_coincide(0));
    caso("... gira primero, avanza despues, hay pausa de 300 ms entre movimientos y nunca avanza sin reorientarse", vuelta_reorienta_y_pausa(cambios_antes));
    caso("... el diagnostico REINC_DIAG anota la pose antes y despues, el punto objetivo, la distancia y los angulos",
         hay_diag("REINC_DIAG plan pose_antes=(") && hay_diag("objetivo=(") && hay_diag("dist_mm=") && hay_diag("giro1=") && hay_diag("giro2=") &&
         hay_diag("REINC_DIAG fin pose_despues=("));
    caso("... y anota cuanto avanza en paralelo para pasar el obstaculo", hay_diag("REINC_DIAG pasar paralelo_mm="));
    correr(30000);
    caso("... y despues llega al destino (2,3)", nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150) && nav.esquivadas == 1);

    /* ── Obstáculo a la izquierda (centro e izquierda ocupados): se aparta por la derecha ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 3500, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 1500, 3500, ZONA_IZQUIERDA, 3);
    caso("volver a la linea (obstaculo a la izquierda, esquiva por la derecha): termina la vuelta", correr_hasta_terminar_vuelta(40000));
    caso("... queda a 10 cm o menos de la linea con la pose estimada igual a la real", distancia_a_la_ruta(RECTA, N(RECTA)) <= 100 && pose_estimada_coincide(0));
    correr(30000);
    caso("... y llega al destino", nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150));

    /* ── Obstáculo a la derecha (centro y derecha ocupados): se aparta por la izquierda ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 3500, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 1500, 3500, ZONA_DERECHA, 3);
    caso("volver a la linea (obstaculo a la derecha, esquiva por la izquierda): termina la vuelta", correr_hasta_terminar_vuelta(40000));
    caso("... queda a 10 cm o menos de la linea con la pose estimada igual a la real", distancia_a_la_ruta(RECTA, N(RECTA)) <= 100 && pose_estimada_coincide(0));
    correr(30000);
    caso("... y llega al destino", nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150));

    /* ── Con reintento (el obstáculo sigue al mirar: retrocede, gira 45 grados más y se va) ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 4300, ZONA_CENTRO, 3);
    caso("volver a la linea (con un reintento): termina la vuelta", correr_hasta_terminar_vuelta(60000) && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 1);
    caso("... queda a 10 cm o menos de la linea con la pose estimada igual a la real", distancia_a_la_ruta(RECTA, N(RECTA)) <= 100 && pose_estimada_coincide(0));
    correr(30000);
    caso("... y llega al destino", nav.estado == NAV_TERMINADO && cerca_de(2, 3, 150));

    /* ── En curva: el obstáculo aparece cerca de la esquina del cuadrado; vuelve a la línea (que da la vuelta) ── */
    preparar(CUADRADO, N(CUADRADO));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    caso("volver a la linea en curva (obstaculo cerca de la esquina): termina la vuelta", correr_hasta_terminar_vuelta(60000));
    caso("... queda a 10 cm o menos de la linea del cuadrado", distancia_a_la_ruta(CUADRADO, N(CUADRADO)) <= 100);
    correr(60000);
    caso("... y termina el cuadrado cerca del punto de partida", nav.estado == NAV_TERMINADO && cerca_de(1, 4, 150));
    if (!(nav.estado == NAV_TERMINADO && cerca_de(1, 4, 150))) {
        informar_posicion("curva");
    }

    /* ── Desviación de menos de 8 cm: solo corrige el rumbo, sin moverse de lado ── */
    preparar(RECTA, N(RECTA));
    correr(1000);
    nav.estado = NAV_REINCORPORANDO;       /* se arma a mano la situación: el carro quedó a 3 cm de la linea y 12 grados torcido */
    nav.reinc_fase = NAV_RF_PLAN;
    nav.reinc_t_fase_ms = sim.t_ms;
    nav.cmd_actual = NAV_CMD_STOP;
    nav.x_mm = nav.base_x_mm = 430;
    nav.y_mm = nav.base_y_mm = 1300;
    nav.rumbo_grados = nav.base_rumbo_grados = 12;
    nav.seg_ms = 0;
    nav.indice_objetivo = 2;
    nav.salida_x_mm = 400;
    nav.salida_y_mm = 1300;
    nav.salida_cercania = 3;
    sim.x_cmm = 43000L;
    sim.y_cmm = 130000L;
    sim.rumbo_cgrados = 1200L;
    sim.cmd_previo = NAV_CMD_STOP;
    sim.n_cambios = 0;
    n_lineas_diag = 0;
    for (i = 0; i < 100 && nav.estado == NAV_REINCORPORANDO; i++) {
        correr(NAV_TICK_MS);
        leer_diag();
    }
    caso("desviacion menor a 8 cm: solo corrige el rumbo (gira a la izquierda, no avanza de lado) y sigue",
         nav.estado == NAV_SIGUIENDO && sim_contar_cambios_a(&sim, NAV_CMD_LEFT) == 1 && sim_contar_cambios_a(&sim, NAV_CMD_FORWARD) == 0 &&
         hay_diag("solo_rumbo=1") && valor_absoluto(sim_x_mm(&sim) - 430) <= 10);
    caso("... y el rumbo queda dentro de +-6 grados de la linea", valor_absoluto(sim_rumbo_grados(&sim) > 180 ? sim_rumbo_grados(&sim) - 360 : sim_rumbo_grados(&sim)) <= 6);

    /* ── El tramo de vuelta está bloqueado: se detiene y esquiva otra vez ── */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    {
        int agregado = 0;
        uint32_t t0 = sim.t_ms;

        while (sim.t_ms - t0 < 70000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR) {
            correr(NAV_TICK_MS);
            if (!agregado && nav.estado == NAV_REINCORPORANDO && nav.reinc_fase == NAV_RF_AVANZAR) {
                sim_agregar_obstaculo(&sim, sim.t_ms + 100, sim.t_ms + 1800, ZONA_CENTRO, 3);   /* algo aparece en el camino de vuelta */
                agregado = 1;
            }
        }
        caso("el tramo de vuelta bloqueado: se detiene, repite el esquive (2 en total) y llega", agregado && nav.estado == NAV_TERMINADO && nav.esquivadas == 2 && cerca_de(2, 3, 150));
    }
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    {
        int agregado = 0;
        uint32_t t0 = sim.t_ms;

        while (sim.t_ms - t0 < 70000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR) {
            correr(NAV_TICK_MS);
            if (!agregado && nav.estado == NAV_REINCORPORANDO && nav.reinc_fase == NAV_RF_AVANZAR) {
                sim_agregar_obstaculo(&sim, sim.t_ms + 100, sim.t_ms + 450, ZONA_CENTRO, 3);   /* algo pasa por delante un momento */
                agregado = 1;
            }
        }
        caso("el tramo de vuelta bloqueado un momento: se detiene, ya no esta y retoma la vuelta SIN esquivar otra vez", agregado && nav.estado == NAV_TERMINADO && nav.esquivadas == 1 &&
             cerca_de(2, 3, 150));
    }

    /* ── Obstáculo que reaparece cada vez que vuelve a la línea: esquiva el máximo de veces y termina en ERROR (MAX_ESQUIVADAS) ── */
    preparar(LARGA, N(LARGA));
    {
        uint32_t t0 = sim.t_ms, ultimo_agregado = 0;

        while (sim.t_ms - t0 < 200000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR) {
            correr(NAV_TICK_MS);
            if (nav.estado == NAV_SIGUIENDO && nav.cmd_actual == NAV_CMD_FORWARD && sim.t_ms > ultimo_agregado + 4000 && sim.n_eventos < SIM_MAX_EVENTOS) {
                sim_agregar_obstaculo(&sim, sim.t_ms + 100, sim.t_ms + 900, ZONA_CENTRO, 3);   /* aparece de frente al volver a avanzar */
                ultimo_agregado = sim.t_ms;
            }
        }
    }
    caso("obstaculo que reaparece al volver a la linea: esquiva el maximo de veces y termina en ERROR (MAX_ESQUIVADAS)",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_MAX_ESQUIVADAS && nav.esquivadas == NAV_MAX_ESQUIVADAS && sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);
    navegador_texto_error(&nav, texto, sizeof texto);
    caso("... y el mensaje dice la causa concreta con los numeros", strstr(texto, "esquivó 3 veces") != NULL);

    /* ── Geometría de apoyo ── */
    {
        static const Punto UNA_CELDA[] = { {0, 0}, {0, 1} };

        navegador_iniciar(&nav);
        navegador_cargar_ruta(&nav, UNA_CELDA, 2);
        caso("la cola de diagnostico empieza vacia", navegador_diag_siguiente(&nav, texto, sizeof texto) == 0);
        caso("diag_siguiente con punteros nulos o capacidad chica no revienta", navegador_diag_siguiente(NULL, texto, sizeof texto) == 0 && navegador_diag_siguiente(&nav, NULL, 10) == 0 &&
             navegador_diag_siguiente(&nav, texto, 1) == 0);
    }
}


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
    correr(9000);
    caso("ayuda apagada (por defecto): la pared de lado a lado sigue dando ERROR", nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_LADOS_BLOQUEADOS);

    /* Con ayuda: pasa a AYUDA y queda quieto. */
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
    sim_agregar_obstaculo_celdas(&sim, 1500, 12000, ZONA_CENTRO, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 12000, ZONA_IZQUIERDA, 3, 40);
    sim_agregar_obstaculo_celdas(&sim, 1500, 12000, ZONA_DERECHA, 3, 40);
    correr(7000);
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
    correr(7000);
    maniobra(NAV_CMD_RIGHT, 900);
    maniobra(NAV_CMD_FORWARD, 2000);   /* 40 cm de lado */
    maniobra(NAV_CMD_LEFT, 900);
    maniobra(NAV_CMD_FORWARD, 4200);   /* ya más allá del punto final (la salida está a 70 cm de él y avanza unos 84 cm) */
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
    correr(8000);
    caso("(antes) pide ayuda", nav.estado == NAV_AYUDA);
    correr(NAV_AYUDA_MAX_MS - 2000);
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
    correr(7000);
    navegador_ayuda_comando(&nav, NAV_CMD_FORWARD);
    correr(300);
    navegador_cancelar(&nav);
    caso("cancelar desde AYUDA: ESPERANDO y STOP en el siguiente tick", nav.estado == NAV_ESPERANDO && navegador_tick(&nav, sim.t_ms, NULL) == NAV_CMD_STOP);
    caso("los textos del estado AYUDA", strcmp(navegador_texto_estado(NAV_AYUDA), "AYUDA") == 0);
}

/* Corre la ruta RECTA con un obstáculo programado y dice si hubo ERROR de esquivar. */
static void pruebas_falso_obstaculo(void)
{
    char texto[NAV_DIAG_LARGO];

    /* Hipótesis 1: el mismo obstáculo sigue visible un instante en el borde cuando ya avanza de lado y cuenta como nuevo. Un cuadro suelto de cercanía 3 al
     * centro (menos de MS_CONFIRMAR_LEJOS ms) NO debe abortar la esquivada. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 3600, 3700, ZONA_CENTRO, 3);   /* avanzando de lado: dos cuadros seguidos (100 ms) con el obstáculo en el centro */
    correr(50000);
    caso("falso obstaculo 1: el obstaculo visible 100 ms mientras avanza de lado NO aborta (sin ERROR, sin retroceder) y llega",
         nav.estado == NAV_TERMINADO && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 0 && nav.esquivadas == 1 && cerca_de(2, 3, 150));

    /* Hipótesis 2: al girar la cámara barre la escena y algo lejano u oscuro pasa por las filas cercanas. Durante TODO el giro (y los cuadros siguientes hasta
     * estabilizarse) se ignoran las detecciones: no se aborta ni se reintenta. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 5300, 6200, ZONA_CENTRO, 3);   /* algo en el centro mientras gira de vuelta (VOLVER, de 5200 a 6100 ms) y 100 ms mas */
    sim_agregar_obstaculo(&sim, 2000, 2850, ZONA_CENTRO, 2);   /* y otro durante el primer giro (GIRAR, de 2000 a 2900 ms) */
    n_lineas_diag = 0;
    {
        uint32_t t0 = sim.t_ms;

        while (sim.t_ms - t0 < 50000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR) {
            correr(NAV_TICK_MS);
            leer_diag();
        }
    }
    caso("falso obstaculo 2: detecciones de cercania 2 y 3 durante los giros se ignoran: llega sin ERROR y sin reintentos", nav.estado == NAV_TERMINADO && nav.reintentos == 0 &&
         sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 0 && cerca_de(2, 3, 150));
    caso("... y ESQUIVE_DIAG anota que se ignoraron por estar girando", hay_diag("ESQUIVE_DIAG fase=GIRAR") && hay_diag("accion=ignorada_girando"));

    /* Un obstáculo en un LADO (cercanía 3) o LEJANO en el centro (cercanía 1) mientras avanza de lado no aborta. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 3300, 6000, ZONA_DERECHA, 3);    /* lateral cerca (el lado hacia donde NO se aparta, la izquierda) */
    sim_agregar_obstaculo(&sim, 3300, 6000, ZONA_IZQUIERDA, 3);  /* y el otro lado */
    sim_agregar_obstaculo(&sim, 3300, 6000, ZONA_CENTRO, 1);     /* y algo lejano al centro */
    n_lineas_diag = 0;
    {
        uint32_t t0 = sim.t_ms;

        while (sim.t_ms - t0 < 50000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR) {
            correr(NAV_TICK_MS);
            leer_diag();
        }
    }
    caso("un obstaculo lateral cercano o uno lejano al centro mientras avanza de lado NO aborta ni reintenta", nav.estado == NAV_TERMINADO && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 0 &&
         cerca_de(2, 3, 150));
    caso("... y ESQUIVE_DIAG anota que no bloquea (lateral o lejos)", hay_diag("accion=lateral_no_bloquea") || hay_diag("accion=lejos_no_bloquea"));

    /* Hipótesis 3: el umbral dentro del esquive era distinto: ahora un obstáculo central de cercanía 2 CONFIRMADO también bloquea, y se usa el reintento. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 3300, 4800, ZONA_CENTRO, 2);   /* avanzando de lado: un obstáculo de cercanía 2 durante 1.5 s */
    n_lineas_diag = 0;
    {
        uint32_t t0 = sim.t_ms;

        while (sim.t_ms - t0 < 60000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR) {
            correr(NAV_TICK_MS);
            leer_diag();
        }
    }
    caso("obstaculo central de cercania 2 confirmado al avanzar de lado: bloquea y usa un reintento (retrocede), no aborta, y llega",
         nav.estado == NAV_TERMINADO && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == 1 && cerca_de(2, 3, 150));
    caso("... y ESQUIVE_DIAG lo anota como BLOQUEA", hay_diag("accion=BLOQUEA_reintento"));

    /* Si el obstáculo sigue, falla solo al agotar los reintentos y dice la causa concreta. */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, 3300, 9999999, ZONA_CENTRO, 2);
    correr(60000);
    navegador_texto_error(&nav, texto, sizeof texto);
    caso("obstaculo central que no se va: agota los 2 reintentos y recien entonces ERROR (2 retrocesos)",
         nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_ESQUIVANDO && sim_contar_cambios_a(&sim, NAV_CMD_REVERSE) == NAV_MAX_REINTENTOS &&
         sim.cambios[sim.n_cambios - 1].comando == NAV_CMD_STOP);
    caso("... el mensaje dice: se agotaron los 2 reintentos, el obstaculo seguia en el centro", strcmp(texto, "se agotaron los 2 reintentos, el obstáculo seguía en el centro") == 0);

    /* Fuera del esquive, un obstáculo cercano de frente sigue frenando de inmediato (con el primer cuadro). */
    preparar(RECTA, N(RECTA));
    sim_agregar_obstaculo(&sim, 1500, 99999, ZONA_CENTRO, 3);
    correr(1700);
    caso("fuera del esquive: un obstaculo cercano de frente sigue frenando en el primer cuadro (1500 ms)", sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000) == 1500);
}


/* ── Auditoría del 2026-10-15 (ver docs/navegador.md, "Correcciones de la auditoría") ─────────────────────── */

static const Punto IDA_VUELTA[] = { {2, 10}, {2, 9}, {2, 8}, {2, 7}, {2, 8}, {2, 9}, {2, 10} };   /* 60 cm de ida y 60 de vuelta */

/* Un cuadro con `cercania` en una zona (0 = vacío). `corredor` = cercanía que da el corredor (0 = sin corredor). */
static void cuadro_aud(ResultadoObstaculos *o, int zona, int cercania, int corredor)
{
    memset(o, 0, sizeof *o);
    if (cercania > 0) {
        o->zona[zona].hay_obstaculo = 1;
        o->zona[zona].cercania = cercania;
        o->zona[zona].celdas = 10;
        o->zona[zona].cercania_corredor = corredor;
    }
}

static void preparar_aud(const Punto *ruta, int n)
{
    navegador_iniciar(&nav);
    navegador_cargar_ruta(&nav, ruta, n);
    navegador_comenzar(&nav, 0);
    n_lineas_diag = 0;
}

/* En AYUDA: mantiene `comando` durante `ms` (con cuadros vacíos) y devuelve la hora final. */
static uint32_t ayuda_mantener(uint32_t t, ComandoNav comando, int ms)
{
    ResultadoObstaculos o;
    uint32_t fin = t + (uint32_t)ms;

    navegador_ayuda_comando(&nav, comando);
    for (; t < fin; t += 50) {
        cuadro_aud(&o, ZONA_CENTRO, 0, 0);
        navegador_tick(&nav, t, &o);
    }
    return t;
}

static void pruebas_auditoria(void)
{
    ResultadoObstaculos o;
    uint32_t t;
    ComandoNav c, previo;
    int saltos, forward, ymin, vuelta_ok, vio_volver, en_detenido, con_corredor;

    /* 1. Con cámara: sin el primer cuadro no se mueve y termina en ERROR sin video (antes recorría la ruta entera a ciegas). */
    navegador_iniciar(&nav);
    navegador_cargar_ruta(&nav, LARGA, N(LARGA));
    navegador_poner_requiere_camara(&nav, 1);
    navegador_comenzar(&nav, 0);
    forward = 0;
    for (t = 50; t <= 3000 && nav.estado != NAV_ERROR; t += 50) {
        if (navegador_tick(&nav, t, NULL) != NAV_CMD_STOP) {
            forward++;
        }
    }
    caso("auditoria: con camara y sin ningun cuadro el carro no se mueve y da ERROR sin video en 1.5 s", forward == 0 && nav.estado == NAV_ERROR &&
         nav.motivo_error == NAV_MOTIVO_SIN_VIDEO && t <= NAV_PRIMER_CUADRO_MAX_MS + 100);
    navegador_iniciar(&nav);
    navegador_cargar_ruta(&nav, LARGA, N(LARGA));
    navegador_poner_requiere_camara(&nav, 1);
    navegador_comenzar(&nav, 0);
    cuadro_aud(&o, ZONA_CENTRO, 0, 0);
    c = navegador_tick(&nav, 50, &o);
    caso("auditoria: ... con el primer cuadro arranca normal", c == NAV_CMD_FORWARD && nav.estado == NAV_SIGUIENDO);
    caso("auditoria: requiere_camara rechaza valores raros", navegador_poner_requiere_camara(&nav, 2) == NAV_ERR_ARGUMENTO &&
         navegador_poner_requiere_camara(NULL, 1) == NAV_ERR_ARGUMENTO);

    /* 2. Entre AVANZAR y VOLVER, y entre AVANZAR y RETROCEDER, hay STOP (sin la pausa el giro variaba 30 a 60 grados). */
    preparar_aud(LARGA, N(LARGA));
    previo = NAV_CMD_STOP;
    saltos = 0;
    for (t = 50; t < 150000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR; t += 50) {
        int bloquear = (t >= 1000 && t < 1400) ||
                       (nav.estado == NAV_ESQUIVANDO && nav.fase == NAV_FASE_AVANZAR && nav.reintentos == 0 && nav.esquivadas == 1 && nav.ms_fase > 300);

        cuadro_aud(&o, ZONA_CENTRO, bloquear ? 3 : 0, 0);
        c = navegador_tick(&nav, t, &o);
        if (previo == NAV_CMD_FORWARD && (c == NAV_CMD_LEFT || c == NAV_CMD_RIGHT || c == NAV_CMD_REVERSE)) {
            saltos++;
        }
        previo = c;
    }
    caso("auditoria: nunca pasa de FORWARD a un giro o a REVERSE sin un STOP en medio (con un reintento desde AVANZAR)", saltos == 0 && nav.estado == NAV_TERMINADO);

    /* 3. La cercanía con la que frenó es la MAYOR desde que se detuvo, aunque el último cuadro antes de decidir venga vacío. */
    preparar_aud(LARGA, N(LARGA));
    for (t = 50; t < 3000 && nav.estado != NAV_ESQUIVANDO; t += 50) {
        cuadro_aud(&o, ZONA_CENTRO, (t >= 1000 && t < 1450) ? 3 : 0, 0);
        navegador_tick(&nav, t, &o);
    }
    caso("auditoria: un cuadro vacio justo al decidir no baja la cercania de salida a 1 (sigue en 3)", nav.estado == NAV_ESQUIVANDO && nav.salida_cercania == 3);

    /* 4. En «Ida y vuelta» el paralelo no sigue de frente más allá de la vuelta de la ruta. */
    preparar_aud(IDA_VUELTA, N(IDA_VUELTA));
    ymin = 100000;
    for (t = 50; t < 60000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR; t += 50) {
        cuadro_aud(&o, ZONA_CENTRO, (t >= 300 && t < 900) ? 2 : 0, 0);
        navegador_tick(&nav, t, &o);
        if (nav.y_mm < ymin) {
            ymin = nav.y_mm;
        }
    }
    caso("auditoria: ida y vuelta con obstaculo al inicio: el paralelo no pasa del punto mas lejano de la ruta (antes 55 cm fuera)",
         ymin >= 7 * MM - 60 && nav.estado == NAV_TERMINADO);

    /* 5. Con dos reintentos (225 grados) la vuelta es hacia el lado CONTRARIO (los errores del giro se cancelan en vez de sumarse). */
    preparar_aud(LARGA, N(LARGA));
    vuelta_ok = 1;
    vio_volver = 0;
    for (t = 50; t < 150000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR; t += 50) {
        int bloquear = (t >= 1000 && t < 1400) || (nav.estado == NAV_ESQUIVANDO && nav.fase == NAV_FASE_MIRAR && nav.reintentos < 2 && nav.esquivadas == 1);

        cuadro_aud(&o, ZONA_CENTRO, bloquear ? 3 : 0, 0);
        c = navegador_tick(&nav, t, &o);
        if (nav.estado == NAV_ESQUIVANDO && nav.fase == NAV_FASE_VOLVER && (c == NAV_CMD_LEFT || c == NAV_CMD_RIGHT)) {
            vio_volver = 1;
            if (nav.angulo_total != 225 || c != ((nav.lado < 0) ? NAV_CMD_RIGHT : NAV_CMD_LEFT)) {
                vuelta_ok = 0;
            }
        }
    }
    caso("auditoria: tras dos reintentos (225 grados) se endereza girando hacia el lado contrario", vio_volver && vuelta_ok);

    /* 6. Tras «toma el control», si el usuario dejó el carro al lado del obstáculo (sin pasarlo), primero avanza en paralelo hasta pasarlo y luego vuelve. */
    preparar_aud(LARGA, N(LARGA));
    navegador_poner_pedir_ayuda(&nav, 1);
    for (t = 50; t < 20000 && nav.estado != NAV_AYUDA; t += 50) {
        memset(&o, 0, sizeof o);
        if (t >= 1000) {
            cuadro_aud(&o, ZONA_CENTRO, 3, 0);
            o.zona[ZONA_IZQUIERDA] = o.zona[ZONA_CENTRO];
            o.zona[ZONA_DERECHA] = o.zona[ZONA_CENTRO];
        }
        navegador_tick(&nav, t, &o);
    }
    caso("auditoria: los dos lados ocupados con ayuda activa: pide ayuda", nav.estado == NAV_AYUDA);
    t = ayuda_mantener(t, NAV_CMD_RIGHT, cal_grados_a_ms(&nav.cal, 90));
    t = ayuda_mantener(t, NAV_CMD_STOP, 400);
    t = ayuda_mantener(t, NAV_CMD_FORWARD, cal_cm_a_ms(&nav.cal, 40));
    t = ayuda_mantener(t, NAV_CMD_STOP, 400);
    t = ayuda_mantener(t, NAV_CMD_LEFT, cal_grados_a_ms(&nav.cal, -90));
    t = ayuda_mantener(t, NAV_CMD_STOP, 400);
    n_lineas_diag = 0;
    navegador_ayuda_continuar(&nav, t);
    for (; t < 120000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR && nav.estado != NAV_AYUDA; t += 50) {
        cuadro_aud(&o, ZONA_CENTRO, 0, 0);
        navegador_tick(&nav, t, &o);
        leer_diag();
    }
    caso("auditoria: tras la ayuda, al lado del obstaculo sin pasarlo: primero pasa en paralelo (pasar_primero=1) y luego vuelve y termina",
         hay_diag("pasar_primero=1") && hay_diag("REINC_DIAG paso_el_obstaculo") && nav.estado == NAV_TERMINADO);

    /* 7. Volviendo en diagonal a la línea: un lado muy cerca que el corredor ubica en el camino detiene; sin corredor no (no distingue al lado de en el camino). */
    for (con_corredor = 0; con_corredor <= 1; con_corredor++) {
        preparar_aud(LARGA, N(LARGA));
        en_detenido = 0;
        for (t = 50; t < 150000 && nav.estado != NAV_TERMINADO && nav.estado != NAV_ERROR; t += 50) {
            if (t >= 1000 && t < 1400) {
                cuadro_aud(&o, ZONA_CENTRO, 3, 0);
            } else if (nav.estado == NAV_REINCORPORANDO && nav.reinc_fase == NAV_RF_AVANZAR && nav.esquivadas == 1) {
                cuadro_aud(&o, ZONA_DERECHA, 3, con_corredor ? 3 : 0);
            } else {
                cuadro_aud(&o, ZONA_CENTRO, 0, 0);
            }
            navegador_tick(&nav, t, &o);
            if (nav.esquivadas == 1 && nav.estado == NAV_DETENIDO_OBSTACULO && nav.volver_a_reinc) {
                en_detenido = 1;
                break;
            }
        }
        if (con_corredor) {
            caso("auditoria: volviendo a la linea, un lado con cercania 3 dentro del corredor detiene el carro", en_detenido);
        } else {
            caso("auditoria: ... sin corredor ese lado no detiene (es el obstaculo que acaba de pasar)", !en_detenido && nav.estado == NAV_TERMINADO);
        }
    }
}

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
    dist_con = distancia_a_la_ruta(RECTA, N(RECTA));
    estimada_con = (valor_absoluto(nav.x_mm - sim_x_mm(&sim)) > valor_absoluto(nav.y_mm - sim_y_mm(&sim))) ? valor_absoluto(nav.x_mm - sim_x_mm(&sim)) : valor_absoluto(nav.y_mm - sim_y_mm(&sim));
    errores_con = (nav.estado == NAV_TERMINADO && nav.esquivadas == 1);
    caso("esquive completo con el modelo: llega (TERMINADO, 1 esquivada) y la pose final queda a 10 cm o menos de la linea", errores_con && dist_con <= 100);
    caso("... y la pose estimada coincide con la real (+-3 cm)", estimada_con <= 30);

    preparar_con_desplazamiento(RECTA, N(RECTA), 0, 400, 80, 700, -120);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr(60000);
    dist_sin = distancia_a_la_ruta(RECTA, N(RECTA));
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
        dist_con = distancia_a_la_ruta(RECTA, N(RECTA));
        estimada_con = (valor_absoluto(nav.x_mm - sim_x_mm(&sim)) > valor_absoluto(nav.y_mm - sim_y_mm(&sim))) ? valor_absoluto(nav.x_mm - sim_x_mm(&sim)) : valor_absoluto(nav.y_mm - sim_y_mm(&sim));
        errores_con = (nav.estado == NAV_TERMINADO && nav.esquivadas == 1);
        preparar_con_desplazamiento(RECTA, N(RECTA), 0, ia_um, id_um, pa_um, pd_um);
        sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
        correr(90000);
        dist_sin = distancia_a_la_ruta(RECTA, N(RECTA));
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
    correr_hasta_el_final(30000);
    {
        char texto[300];

        navegador_texto_error(&nav, texto, sizeof texto);
        caso("no cabe: con el doble de desplazamiento y el obstaculo a cercania 3 (menos de 30 cm) el giro de salida mueve el centro 30 cm adelante y solo hay 20 libres: pide AYUDA (no gira)",
             nav.estado == NAV_AYUDA && nav.error_causa == NAV_CAUSA_ARCO && nav.esquivadas == 0 && strstr(texto, "30 cm hacia adelante") != NULL && strstr(texto, "20 cm libres") != NULL);
        printf("      texto: %s\n", texto);
    }
    caso("... y no se movio mas que el frenado antes de pedirla (menos de 5 cm de giro: el rumbo sigue en 0)", valor_absoluto(error_rumbo_real()) <= 2);
    preparar_plan_pivote(LARGA, N(LARGA), 200, 0, 0);
    sim_agregar_obstaculo(&sim, 1500, 2800, ZONA_CENTRO, 3);
    correr_hasta_el_final(30000);
    caso("... sin pedir ayuda activada da ERROR con la misma causa", nav.estado == NAV_ERROR && nav.error_causa == NAV_CAUSA_ARCO);

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
        caso("si en cada tramo hay un empujon de 12 cm, tras 4 correcciones se agota la red, para y pide AYUDA con la causa", nav.estado == NAV_AYUDA && nav.error_causa == NAV_CAUSA_CORRECCIONES &&
             marcas_plan.correcciones == NAV_PLAN_CORRECCIONES && marcas_plan.agotada == 1 && strstr(texto, "4 veces") != NULL);
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
static void preparar_final_de_ruta(int derecha_conocida, int segundo_inicio_ms, int sin_arreglo)
{
    static const Punto OCHO[] = { {4, 7}, {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0} };
    CalibracionCarro real, modelo;
    int pa, pd, ia, id;

    cal_defecto(&real);
    cal_desplazamiento_de_medida(CAL_LADO_DERECHA, 150, 165, 90, &pa, &pd);
    cal_poner_desplazamiento(&real, CAL_LADO_DERECHA, pa, pd);
    cal_desplazamiento_de_medida(CAL_LADO_IZQUIERDA, -88, -148, 180, &ia, &id);
    cal_poner_desplazamiento(&real, CAL_LADO_IZQUIERDA, ia, id);
    cal_poner_desplazamiento_grados(&real, CAL_LADO_IZQUIERDA, 180);
    modelo = real;
    if (!derecha_conocida) {
        cal_poner_desplazamiento(&modelo, CAL_LADO_DERECHA, 0, 0);   /* el navegador solo conoce la izquierda: la derecha es su espejo */
    }
    memset(&marcas_plan, 0, sizeof marcas_plan);
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav, &modelo);
    navegador_poner_pedir_ayuda(&nav, 1);
    navegador_cargar_ruta(&nav, OCHO, N(OCHO));
    sim_iniciar(&sim, OCHO[0].columna, OCHO[0].fila, 0);
    sim.real = real;
    sim_agregar_obstaculo(&sim, 0, 1200, ZONA_CENTRO, 3);
    sim_agregar_obstaculo(&sim, (uint32_t)segundo_inicio_ms, (uint32_t)(segundo_inicio_ms + 700), ZONA_CENTRO, 3);
    nav.sin_arreglo_indice = sin_arreglo;
    navegador_comenzar(&nav, 0);
    vigilar_marcas();
}

static void pruebas_final_de_ruta(void)
{
    /* Con el arreglo (los puntos ya pasados se saltan): termina bien. */
    preparar_final_de_ruta(1, 8000, 0);
    correr_hasta_el_final(60000);
    caso("final de ruta (obstaculo al arrancar y otro a los 8 s): con el arreglo termina (TERMINADO), con dos esquivadas y SIN media vuelta de 180 grados para volver a un punto que ya paso",
         nav.estado == NAV_TERMINADO && nav.esquivadas == 2 && marcas_plan.giros_180_de_ruta == 0);
    caso("... la vuelta salta los puntos de la ruta que el carro ya paso", marcas_plan.puntos_saltados >= 1);

    /* Sin el arreglo reproduce el bug del indice vencido: la ruta pide un giro de mas de 150 grados que la ruta no tiene y el freno anti-locura lo para (AYUDA) en vez de darlo. */
    preparar_final_de_ruta(1, 8000, 1);
    correr_hasta_el_final(60000);
    {
        char texto[300];

        navegador_texto_error(&nav, texto, sizeof texto);
        caso("SIN el arreglo (el bug del indice vencido): el freno anti-locura para y pide AYUDA con la causa «giro de mas de 150 grados que la ruta no tiene» en vez de dar la media vuelta",
             nav.estado == NAV_AYUDA && nav.error_causa == NAV_CAUSA_GIRO_LOCO && marcas_plan.giros_180_de_ruta == 0 && nav.cmd_actual == NAV_CMD_STOP && strstr(texto, "150 grados") != NULL);
        printf("      texto: %s\n", texto);
    }
    /* Con la derecha como espejo (lo que habia en el telefono) y sin el arreglo, tampoco da la media vuelta: a lo mas se para. */
    preparar_final_de_ruta(0, 6000, 1);
    correr_hasta_el_final(60000);
    caso("con la derecha como espejo y sin el arreglo, nunca da una media vuelta de la ruta (termina o se para en AYUDA)", marcas_plan.giros_180_de_ruta == 0 && (nav.estado == NAV_TERMINADO || nav.estado == NAV_AYUDA));
}

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

static void preparar_arranque(void)
{
    memset(&marcas_arr, 0, sizeof marcas_arr);
    preparar(RECTA, N(RECTA));
    navegador_poner_pedir_ayuda(&nav, 1);
}

static void pruebas_arranque_y_sondeo(void)
{
    /* 1. Artefacto fijo: zona izquierda, cercanía 3, 10 celdas (dentro del tope de 12), siempre igual (no crece al acercarse). El carro avanza 4 cm, ve que no crece y lo ignora: llega sin esquivar. */
    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 0, 99999, ZONA_IZQUIERDA, 3, 10);
    correr_arranque(30000);
    caso("obstaculo fantasma fijo a la izquierda (cercania 3, 10 celdas, desde el primer cuadro): el carro lo sondea, ve que no crece y llega SIN esquivar (TERMINADO, 0 esquivadas)",
         nav.estado == NAV_TERMINADO && nav.esquivadas == 0 && marcas_arr.inicio == 1 && marcas_arr.fijo == 1 && marcas_arr.real == 0);
    caso("... no se movio hasta tener 4 cuadros iguales (200 ms) y el sondeo avanzo solo ~4 cm antes de seguir: el primer FORWARD sale a los 200 ms",
         sim_primer_cambio_a(&sim, NAV_CMD_FORWARD, 0) >= 150 && sim_primer_cambio_a(&sim, NAV_CMD_FORWARD, 0) <= 300);
    printf("      primer FORWARD a los %ld ms; arranque=%d inicio=%d fijo=%d hex_valido=%d\n", sim_primer_cambio_a(&sim, NAV_CMD_FORWARD, 0), marcas_arr.arranque, marcas_arr.inicio, marcas_arr.fijo, marcas_arr.hex_valido);
    caso("... y se loguea con OBS_DIAG: arranque, inicio del sondeo y sospechoso fijo, con las tres mascaras de 25 cifras hexadecimales, la cercania y la franja",
         marcas_arr.arranque == 1 && marcas_arr.hex_valido >= 3);
    caso("... y esa zona queda ignorada en esta corrida (fijo_ignorar) mientras sus celdas no crezcan, con zona_celdas en 0",
         nav.fijo_ignorar[ZONA_IZQUIERDA] == 1 && nav.fijo_ignorar[ZONA_CENTRO] == 0 && nav.zona_celdas[ZONA_IZQUIERDA] == 0);

    /* 1b. J10 Punto 3(a): Al ignorar la zona izquierda por sospechoso_fijo, zona_celdas[ZONA_IZQUIERDA] queda en 0 y elegir_lado no sesga a la derecha cuando aparece un obstáculo en el centro. */
    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 0, 99999, ZONA_IZQUIERDA, 3, 10);
    sim_agregar_obstaculo_celdas(&sim, 1500, 3500, ZONA_CENTRO, 3, 25);
    correr_arranque(60000);
    caso("J10 3(a): con fantasma fijo ignorado a la izquierda y obstaculo real en el centro a los 1500 ms, zona_celdas[IZQ] es 0 y elegir_lado no sesga a la derecha (elige izquierda, lado == -1)",
         nav.esquivadas >= 1 && nav.lado == -1 && nav.zona_celdas[ZONA_IZQUIERDA] == 0);
    /* Y en comparación, tras reincorporar el fantasma fijo sigue ignorado durante toda la corrida mientras su firma hexadecimal siga igual (J10 3(c)). */
    caso("J10 3(c): tras esquivar y reincorporar, el fantasma fijo a la izquierda sigue ignorado toda la corrida (no vuelve a detener el carro: TERMINADO con 1 sola esquivada)",
         nav.estado == NAV_TERMINADO && nav.esquivadas == 1 && nav.fijo_ignorar[ZONA_IZQUIERDA] == 1);

    /* 1c. J10 Punto 3(b): tick_sondeo no declara obstáculo real si no llegaron al menos 2 cuadros; espera hasta 2 cuadros o 1500 ms y, si no hay, repite sin declarar nada. */
    {
        ResultadoObstaculos obs_fijo;
        int f;

        memset(&obs_fijo, 0, sizeof obs_fijo);
        obs_fijo.columnas = 10;
        obs_fijo.filas = 10;
        obs_fijo.fila_minima_usada = 4;
        for (f = 7; f <= 9; f++) {
            obs_fijo.no_piso[f][1] = 1;
            obs_fijo.no_piso[f][2] = 1;
        }
        obs_fijo.zona[ZONA_IZQUIERDA].hay_obstaculo = 1;
        obs_fijo.zona[ZONA_IZQUIERDA].cercania = 3;
        obs_fijo.zona[ZONA_IZQUIERDA].celdas = 6;

        navegador_iniciar(&nav);
        navegador_cargar_ruta(&nav, RECTA, N(RECTA));
        navegador_comenzar(&nav, 0);
        /* 4 cuadros iguales al arrancar -> inicia sondeo en t = 150 ms */
        navegador_tick(&nav, 0, &obs_fijo);
        navegador_tick(&nav, 50, &obs_fijo);
        navegador_tick(&nav, 100, &obs_fijo);
        navegador_tick(&nav, 150, &obs_fijo);
        caso("J10 3(b): tras 4 cuadros entra en sondeo_fase=1 avanzando", nav.estado == NAV_SIGUIENDO && nav.sondeo_fase == 1);
        /* Avanza los ~200 ms del sondeo con cuadros para no disparar hueco de video, y entra a sondeo_fase = 2 en t = 400 ms (sondeo_listo_ms = 700) */
        navegador_tick(&nav, 200, &obs_fijo);
        navegador_tick(&nav, 300, &obs_fijo);
        navegador_tick(&nav, 400, &obs_fijo);
        caso("J10 3(b): al terminar los 4 cm pasa a sondeo_fase=2 con sondeo_n=0", nav.estado == NAV_SIGUIENDO && nav.sondeo_fase == 2 && nav.sondeo_n == 0);
        /* Pasan 300 ms desde sondeo_listo_ms=700 (t=1000 > 700+250) pero SIN cuadros nuevos (sondeo_n == 0): NO declara obstáculo real, sigue esperando */
        nav.t_ultimo_cuadro_ms = 900;
        navegador_tick(&nav, 1000, NULL);
        caso("J10 3(b): tras >250 ms mirando con 0 cuadros (sondeo_n==0), NO declara obstaculo real ni sale de sondeo_fase=2",
             nav.estado == NAV_SIGUIENDO && nav.sondeo_fase == 2);
        /* Llega 1 solo cuadro en t = 1100 ms (sondeo_n == 1 < 2): todavía espera al segundo cuadro o a los 1500 ms */
        navegador_tick(&nav, 1100, &obs_fijo);
        caso("J10 3(b): con 1 solo cuadro en sondeo (sondeo_n==1 < 2), sigue esperando",
             nav.estado == NAV_SIGUIENDO && nav.sondeo_fase == 2 && nav.sondeo_n == 1);
        /* En t = 2250 ms (>= sondeo_listo_ms(700) + 1500 = 2200) sin llegar un segundo cuadro: repite el sondeo (sondeo_fase=1, FORWARD) sin declarar obstáculo real */
        nav.t_ultimo_cuadro_ms = 2100;
        navegador_tick(&nav, 2250, NULL);
        caso("J10 3(b): al cumplirse 1500 ms sin reunir 2 cuadros, repite el sondeo (sondeo_fase vuelve a 1, cmd=FORWARD) sin declarar obstaculo real",
             nav.estado == NAV_SIGUIENDO && nav.sondeo_fase == 1 && nav.cmd_actual == NAV_CMD_FORWARD);
    }

    /* 1d. J10 Punto 3(c): tope de 12 celdas por zona (con 15 celdas no se ignora) y cancelación si crece más de 30 % o cambia la firma hexadecimal. */
    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 0, 99999, ZONA_IZQUIERDA, 3, 15);
    correr_arranque(30000);
    caso("J10 3(c): con 15 celdas (mas del tope de 12 celdas por zona) NO se ignora como fantasma fijo: esquiva",
         nav.fijo_ignorar[ZONA_IZQUIERDA] == 0 && marcas_arr.fijo == 0 && marcas_arr.real == 1 && nav.esquivadas >= 1);

    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 0, 1500, ZONA_IZQUIERDA, 3, 8);    /* se ignora con 8 celdas */
    sim_agregar_obstaculo_celdas(&sim, 1500, 99999, ZONA_IZQUIERDA, 3, 11); /* crece de 8 a 11 (> 10 = +37.5 % > 30 %) */
    correr_arranque(30000);
    caso("J10 3(c): fantasma fijo de 8 celdas ignorado al arrancar; si luego crece mas de 30 % (a 11 celdas) se cancela fijo_ignorar y esquiva",
         marcas_arr.fijo == 1 && nav.fijo_ignorar[ZONA_IZQUIERDA] == 0 && nav.esquivadas >= 1);

    /* 2. Obstáculo real al lado: crece al acercarse. */
    preparar_arranque();
    sim_agregar_obstaculo_creciente(&sim, 0, 99999, ZONA_IZQUIERDA, 3, 40, 100);
    correr_arranque(30000);
    caso("el mismo obstaculo pero REAL (las celdas crecen al avanzar 4 cm): el sondeo lo da por real, se detiene y esquiva (al menos 1 esquivada)",
         marcas_arr.inicio == 1 && marcas_arr.real == 1 && marcas_arr.fijo == 0 && nav.esquivadas >= 1);

    /* 3. Obstáculo en el centro: no se sondea (el centro no está libre): se detiene y esquiva como siempre. */
    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 0, 3000, ZONA_CENTRO, 3, 40);
    correr_arranque(30000);
    caso("obstaculo al CENTRO desde el primer cuadro: no se sondea (el centro no esta libre), se detiene y esquiva", marcas_arr.inicio == 0 && marcas_arr.detiene >= 1 && nav.esquivadas >= 1);

    /* 4. Un cuadro suelto de ruido al arrancar no decide. */
    preparar_arranque();
    sim_agregar_obstaculo(&sim, 0, 40, ZONA_CENTRO, 3);   /* solo el primer cuadro (t = 0) */
    correr_arranque(30000);
    caso("un solo cuadro con obstaculo al arrancar (ruido) NO decide: no se detiene ni esquiva y llega (TERMINADO, 0 esquivadas)", nav.estado == NAV_TERMINADO && nav.esquivadas == 0 && marcas_arr.detiene == 0);
    preparar_arranque();
    sim_agregar_obstaculo(&sim, 0, 140, ZONA_CENTRO, 3);   /* los 3 primeros cuadros (0, 50 y 100 ms): todavia no son 4 */
    correr_arranque(30000);
    caso("3 cuadros seguidos con obstaculo al arrancar (menos de 4) tampoco deciden", nav.estado == NAV_TERMINADO && nav.esquivadas == 0 && marcas_arr.detiene == 0);
    preparar_arranque();
    sim_agregar_obstaculo(&sim, 0, 1500, ZONA_CENTRO, 3);   /* 30 cuadros iguales: es real */
    correr_arranque(30000);
    caso("4 cuadros seguidos iguales con obstaculo cerca al centro al arrancar SI deciden: se detiene (evento arranque + detiene en el registro)", marcas_arr.arranque == 1 && marcas_arr.detiene >= 1 && nav.esquivadas >= 1);

    /* 5. Cercanía 3: 2 cuadros seguidos, o 1 si el área es de 6 celdas o más (NAV_CERC3_AREA_INMEDIATA = 6). Ya en marcha (a los 1500 ms), con el centro. */
    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 1500, 1550, ZONA_CENTRO, 3, 5);   /* 1 cuadro, 5 celdas */
    correr_arranque(4000);
    caso("cercania 3 con 5 celdas en UN solo cuadro: NO frena (hacen falta 6 celdas o 2 cuadros seguidos)", marcas_arr.detiene == 0 && nav.esquivadas == 0);
    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 1500, 1600, ZONA_CENTRO, 3, 5);   /* 2 cuadros */
    correr_arranque(4000);
    caso("cercania 3 con 5 celdas en DOS cuadros seguidos: frena en el segundo (1550 ms)", marcas_arr.detiene >= 1 && sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000) == 1550);
    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 1500, 1550, ZONA_CENTRO, 3, 6);   /* 1 cuadro, 6 celdas */
    correr_arranque(4000);
    caso("cercania 3 con 6 celdas en UN solo cuadro: frena al instante (1500 ms)", marcas_arr.detiene >= 1 && sim_primer_cambio_a(&sim, NAV_CMD_STOP, 1000) == 1500);
    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 1500, 1550, ZONA_IZQUIERDA, 3, 5);
    sim_agregar_obstaculo_celdas(&sim, 1650, 1700, ZONA_IZQUIERDA, 3, 5);   /* dos cuadros sueltos, separados: no son seguidos */
    correr_arranque(8000);
    caso("dos cuadros de cercania 3 con pocas celdas pero NO seguidos no frenan", marcas_arr.detiene == 0);
    preparar_arranque();
    sim_agregar_obstaculo_celdas(&sim, 1500, 1550, ZONA_CENTRO, 2, 20);   /* cercanía 2 en el centro: 1 cuadro no frena */
    correr_arranque(4000);
    caso("cercania 2 en el centro en UN solo cuadro: NO frena (exige 2 cuadros)", marcas_arr.detiene == 0 && nav.esquivadas == 0);
    /* Columnas 0 y 9 en filas 7 a 9: se evalúan solo durante esquive y reincorporación (bloquean aunque el corredor las deje fuera). */
    {
        ResultadoObstaculos obs_borde;
        int f;

        memset(&obs_borde, 0, sizeof obs_borde);
        obs_borde.columnas = 10;
        obs_borde.filas = 10;
        obs_borde.fila_minima_usada = 4;
        for (f = 7; f <= 9; f++) {
            obs_borde.no_piso[f][0] = 1;   /* columna 0 en filas 7 a 9 */
        }
        obs_borde.zona[ZONA_IZQUIERDA].hay_obstaculo = 1;
        obs_borde.zona[ZONA_IZQUIERDA].cercania = 3;
        obs_borde.zona[ZONA_IZQUIERDA].celdas = 3;
        obs_borde.zona[ZONA_IZQUIERDA].fuera_corredor = 1;   /* el corredor normal (80 %) ignora las columnas 0 y 9 */
        navegador_iniciar(&nav);
        navegador_cargar_ruta(&nav, RECTA, N(RECTA));
        navegador_comenzar(&nav, 0);
        navegador_tick(&nav, 50, &obs_borde);
        caso("columna 0 en filas 7-9 fuera del corredor: en SIGUIENDO no activa zona_borde_cerca ni frena (sin falsos positivos)",
             nav.zona_borde_cerca[ZONA_IZQUIERDA] == 0 && nav.zona_frena[ZONA_IZQUIERDA] == 0);
        nav.estado = NAV_ESQUIVANDO;
        nav.fase = NAV_FASE_AVANZAR;
        nav.esq_en_giro = 0;
        nav.esq_t_giro_fin_ms = 0;
        nav.esq_cuadros_tras_giro = 10;
        navegador_tick(&nav, 500, &obs_borde);
        navegador_tick(&nav, 700, &obs_borde);
        caso("columna 0 en filas 7-9 durante ESQUIVANDO (y REINCORPORANDO): si se evalua y bloquea tras confirmarse",
             nav.zona_borde_cerca[ZONA_IZQUIERDA] == 1 && nav.reintentos >= 1);
    }

    /* 6. Registro con la mascara de lo que se ve: un cuadro con celdas marcadas en la zona izquierda y la franja tapada. */
    {
        ResultadoObstaculos obs;
        int f, c, t, visto = 0;
        char linea[NAV_DIAG_LARGO];

        navegador_iniciar(&nav);
        navegador_cargar_ruta(&nav, RECTA, N(RECTA));
        navegador_comenzar(&nav, 0);
        memset(&obs, 0, sizeof obs);
        obs.columnas = 32;
        obs.filas = 24;
        obs.fila_minima_usada = 10;
        for (f = 19; f <= 23; f++) {
            for (c = 13; c <= 18; c++) {
                obs.no_piso[f][c] = 1;
            }
        }
        obs.zona[ZONA_CENTRO].hay_obstaculo = 1;
        obs.zona[ZONA_CENTRO].cercania = 3;
        obs.zona[ZONA_CENTRO].celdas = 49;
        obs.franja_contaminada = 1;
        obs.franja_tapada_pct = 45;
        for (t = 0; t < 1500; t += 50) {
            navegador_tick(&nav, (uint32_t)t, &obs);
        }
        while (navegador_diag_siguiente(&nav, linea, sizeof linea)) {
            if (strstr(linea, "OBS_DIAG evento=detiene") != NULL) {
                visto = (strstr(linea, "mascara_cen=0000000000000000000000000") == NULL && strstr(linea, "mascara_izq=0000000000000000000000000") != NULL &&
                         strstr(linea, "mascara_der=0000000000000000000000000") != NULL && strstr(linea, "cercania=(0,3,0)") != NULL && strstr(linea, "celdas=(0,49,0)") != NULL &&
                         strstr(linea, "franja_tapada=1 franja_pct=45") != NULL);
            }
        }
        caso("OBS_DIAG al detenerse: la mascara hexadecimal de la zona central tiene celdas (y la de la izquierda y la derecha no), con cercania (0,3,0), celdas (0,49,0) y la franja tapada 45 %", visto);
    }

    /* 7. Hueco de video: 700 ms en SIGUIENDO, ESQUIVANDO y REINCORPORANDO (J10 Punto 5: no reproducido, ir_a_error pasa directo a NAV_ERROR con STOP sin alternar FORWARD). */
    preparar(RECTA, N(RECTA));
    {
        ResultadoObstaculos libre;
        ComandoNav c1, c2;

        memset(&libre, 0, sizeof libre);
        correr(1500);
        navegador_tick(&nav, sim.t_ms, &libre);
        navegador_tick(&nav, sim.t_ms + 650, NULL);
        caso("hueco de video de 650 ms: sigue (NAV_MAX_HUECO_VIDEO_MS = 700)", nav.estado != NAV_ERROR && NAV_MAX_HUECO_VIDEO_MS == 700);
        navegador_tick(&nav, sim.t_ms + 760, NULL);
        caso("hueco de video de 760 ms: ERROR sin video", nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_SIN_VIDEO);

        /* J10 Punto 5: comprobar en ESQUIVANDO y REINCORPORANDO con pedir_ayuda=1 que un hueco > 700 ms pone STOP y pasa a NAV_ERROR sin volver a ordenar FORWARD. */
        preparar(RECTA, N(RECTA));
        navegador_poner_pedir_ayuda(&nav, 1);
        sim_agregar_obstaculo(&sim, 500, 2000, ZONA_CENTRO, 3);
        while (nav.estado != NAV_ESQUIVANDO && sim.t_ms < 5000) {
            correr(NAV_TICK_MS);
        }
        navegador_tick(&nav, sim.t_ms, &libre);
        c1 = navegador_tick(&nav, sim.t_ms + 750, NULL);
        c2 = navegador_tick(&nav, sim.t_ms + 800, NULL);
        caso("J10 5 (no reproducido): hueco de video > 700 ms en ESQUIVANDO pasa a NAV_ERROR con STOP y nunca vuelve a ordenar FORWARD",
             nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_SIN_VIDEO && c1 == NAV_CMD_STOP && c2 == NAV_CMD_STOP);

        preparar(RECTA, N(RECTA));
        navegador_poner_pedir_ayuda(&nav, 1);
        sim_agregar_obstaculo(&sim, 500, 1500, ZONA_CENTRO, 3);
        while (nav.estado != NAV_REINCORPORANDO && sim.t_ms < 15000) {
            correr(NAV_TICK_MS);
        }
        navegador_tick(&nav, sim.t_ms, &libre);
        c1 = navegador_tick(&nav, sim.t_ms + 750, NULL);
        c2 = navegador_tick(&nav, sim.t_ms + 800, NULL);
        caso("J10 5 (no reproducido): hueco de video > 700 ms en REINCORPORANDO pasa a NAV_ERROR con STOP y nunca vuelve a ordenar FORWARD",
             nav.estado == NAV_ERROR && nav.motivo_error == NAV_MOTIVO_SIN_VIDEO && c1 == NAV_CMD_STOP && c2 == NAV_CMD_STOP);
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
        /* Fantasma fijo de 10 celdas al arrancar -> dispara sondeo de 4 cm al inicio de la ruta de 1 m */
        sim_agregar_obstaculo_celdas(&sim, 0, 99999, ZONA_IZQUIERDA, 3, 10);
        navegador_comenzar(&nav, 0);
        correr_arranque(30000);
        err_1m = error_pose_mm(sim_x_mm(&sim), sim_y_mm(&sim), RUTA_1M[N(RUTA_1M) - 1].columna * MM, RUTA_1M[N(RUTA_1M) - 1].fila * MM);
        printf("      J10 8 sondeo 4 cm + ruta 1 m: pose final real (%d, %d) vs planeada (%d, %d), error = %d mm\n",
               sim_x_mm(&sim), sim_y_mm(&sim), RUTA_1M[N(RUTA_1M) - 1].columna * MM, RUTA_1M[N(RUTA_1M) - 1].fila * MM, err_1m);
        caso("J10 8: sondeo de 4 cm mas ruta de 1 m termina a 1 cm o menos de lo planeado (error <= 10 mm)",
             nav.estado == NAV_TERMINADO && marcas_arr.fijo == 1 && err_1m <= 10);

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

int main(void)
{
    pruebas_ruta_derecha_40cm();
    pruebas_desplazamiento_al_girar();
    pruebas_plan_pivote();
    pruebas_final_de_ruta();
    pruebas_frenos_anti_locura();
    pruebas_arranque_y_sondeo();
    pruebas_j10_ruta_l_y_frenado();
    pruebas_sin_obstaculos();
    pruebas_rutas_raras();
    pruebas_antirrebote();
    pruebas_antirrebote_en_ms();
    pruebas_detencion_y_esquivar();
    pruebas_errores_de_navegacion();
    pruebas_cancelar();
    pruebas_deriva();
    pruebas_calibracion();
    pruebas_giro_visual();
    pruebas_video_perdido();
    pruebas_pausa_entre_movimientos();
    pruebas_giro_visual_con_retraso();
    pruebas_esquive_objetos_grandes();
    pruebas_reincorporacion();
    pruebas_falso_obstaculo();
    pruebas_ayuda();
    pruebas_auditoria();
    pruebas_corredor_en_navegador();
    pruebas_textos();

    caso("en TODAS las pruebas nunca hubo movimiento en ESPERANDO, TERMINADO o ERROR", violaciones_totales == 0);

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
