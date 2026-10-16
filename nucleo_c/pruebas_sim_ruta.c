/* pruebas_sim_ruta.c — pruebas de sim_ruta.c: la ruta simulada con el navegador real, con y sin obstáculo y con ruido de giro (sin hardware). */
#include <stdio.h>
#include <string.h>
#include "sim_ruta.h"

static int casos = 0, fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* Celdas de una plantilla ya expandidas. */
static int ruta_de(int plantilla, Punto *celdas)
{
    EditorRuta e;

    re_iniciar(&e);
    re_plantilla(&e, plantilla);
    return re_expandir(&e.actual, celdas, MAX_PUNTOS);
}

static ResultadoSimRuta res, res2;

/* true si en ninguna muestra el carro estuvo a menos de 6 cm del centro de la celda (columna, fila). */
static int sin_pisar(const ResultadoSimRuta *r, int columna, int fila)
{
    int i;

    for (i = 0; i < r->n_muestras; i++) {
        long dx = r->muestras[i].x_mm - columna * 200L;
        long dy = r->muestras[i].y_mm - fila * 200L;

        if (dx * dx + dy * dy < 60L * 60L) {
            return 0;
        }
    }
    return 1;
}

/* true si dos corridas con la misma semilla dan lo mismo. */
static int es_repetible(const Punto *ruta, int n)
{
    static ResultadoSimRuta a, b;

    sr_simular(ruta, n, NULL, NULL, 0, 20, 3, &a);
    sr_simular(ruta, n, NULL, NULL, 0, 20, 3, &b);
    return a.x_final_mm == b.x_final_mm && a.y_final_mm == b.y_final_mm && a.duracion_ms == b.duracion_ms;
}
static DispersionSim disp, disp0;

int main(void)
{
    Punto ruta[MAX_PUNTOS];
    Punto larga[MAX_PUNTOS];
    Punto obst[2];
    int n, nl, i, codigo;
    int hay[3], cerc[3], celdas[3];

    /* ── Sin obstáculos ────────────────────────────────────────────────── */
    n = ruta_de(RE_PLANTILLA_RECTA, ruta);
    codigo = sr_simular(ruta, n, NULL, NULL, 0, 0, 0, &res);
    printf("      recta: llego=%d duracion=%u ms comandos=%d error=%d mm muestras=%d\n", res.llego, (unsigned)res.duracion_ms, res.n_comandos, res.error_final_mm, res.n_muestras);
    caso("recta de 1 m: el simulador corre sin error", codigo == SR_OK && res.codigo == SR_OK);
    caso("recta de 1 m: llega (el navegador termina la ruta)", res.llego && res.estado_final == NAV_TERMINADO);
    caso("recta de 1 m: termina a menos de 8 cm del punto final", res.error_final_mm < 80);
    caso("recta de 1 m: tarda entre 1 y 15 s y manda al menos un comando", res.duracion_ms > 1000 && res.duracion_ms < 15000 && res.n_cambios >= 1);
    caso("recta de 1 m: sin esquivadas y sin salir de la cuadrícula", res.esquivadas == 0 && (res.advertencias & SR_ADV_SALE_CUADRICULA) == 0 && (res.advertencias & SR_ADV_NO_LLEGO) == 0);
    caso("sin calibración guardada (NULL): advertencia de valores de ejemplo", (res.advertencias & SR_ADV_CALIBRACION) != 0);
    caso("hay muestras cada 100 ms y la última muestra es de un tiempo creciente", res.n_muestras > 10 && res.muestras[5].t_ms == 500 && res.muestras[res.n_muestras - 1].t_ms >= res.muestras[1].t_ms);
    caso("el carro sube por la cuadrícula: termina con menos y que al empezar", res.y_final_mm < res.muestras[0].y_mm - 800);

    {
        CalibracionCarro cal;

        cal_defecto(&cal);
        codigo = sr_simular(ruta, n, &cal, NULL, 0, 0, 0, &res2);
        caso("con una calibración válida no sale la advertencia de ejemplo", codigo == SR_OK && (res2.advertencias & SR_ADV_CALIBRACION) == 0);
        caso("y el resultado es el mismo que con NULL (misma calibración por defecto)", res2.duracion_ms == res.duracion_ms && res2.x_final_mm == res.x_final_mm);
    }

    n = ruta_de(RE_PLANTILLA_CUADRADO, ruta);
    codigo = sr_simular(ruta, n, NULL, NULL, 0, 0, 0, &res);
    printf("      cuadrado: llego=%d duracion=%u ms comandos=%d error=%d mm\n", res.llego, (unsigned)res.duracion_ms, res.n_comandos, res.error_final_mm);
    caso("cuadrado de 40 cm: llega y vuelve cerca del punto de salida (menos de 10 cm)", codigo == SR_OK && res.llego && res.error_final_mm < 100);
    caso("cuadrado: manda varios comandos (avanzar y girar)", res.n_comandos >= 6);

    n = ruta_de(RE_PLANTILLA_OCHO, ruta);
    codigo = sr_simular(ruta, n, NULL, NULL, 0, 0, 0, &res);
    caso("ocho: llega", codigo == SR_OK && res.llego);
    n = ruta_de(RE_PLANTILLA_ZIGZAG, ruta);
    codigo = sr_simular(ruta, n, NULL, NULL, 0, 0, 0, &res);
    caso("zigzag: llega", codigo == SR_OK && res.llego);

    /* ── Con un obstáculo ──────────────────────────────────────────────── */
    nl = 0;
    for (i = 7; i >= 0; i--) {   /* una recta larga por la columna 5 */
        larga[nl].columna = 5;
        larga[nl].fila = i;
        nl++;
    }
    codigo = sr_simular(larga, nl, NULL, NULL, 0, 0, 0, &res);
    caso("recta de 1.4 m sin obstáculo: llega y no esquiva", codigo == SR_OK && res.llego && res.esquivadas == 0);
    obst[0].columna = 5;
    obst[0].fila = 3;
    codigo = sr_simular(larga, nl, NULL, obst, 1, 0, 0, &res2);
    printf("      con obstaculo en (5,3): llego=%d estado=%d esquivadas=%d duracion=%u ms comandos=%d error=%d mm\n", res2.llego, res2.estado_final, res2.esquivadas, (unsigned)res2.duracion_ms, res2.n_comandos, res2.error_final_mm);
    caso("con un obstáculo en el camino: la simulación corre", codigo == SR_OK);
    caso("el navegador ve el obstáculo y esquiva (al menos una esquivada)", res2.esquivadas >= 1);
    caso("esquivar tarda más y manda más comandos que ir recto", res2.duracion_ms > res.duracion_ms && res2.n_comandos > res.n_comandos);
    caso("el carro no pasa por encima del obstáculo: nunca está a menos de 6 cm del centro de la celda", sin_pisar(&res2, 5, 3));
    caso("termina de forma clara: llegó o dice por qué no", res2.llego || (res2.advertencias & SR_ADV_NO_LLEGO));

    /* ── Qué ve el ojo del carro ───────────────────────────────────────── */
    obst[0].columna = 5;
    obst[0].fila = 5;
    sr_vista(1000, 1400, 0, obst, 1, hay, cerc, celdas);
    caso("obstáculo de frente a 30 cm de su cara: zona centro, cercanía 3", hay[1] == 1 && cerc[1] == 3 && hay[0] == 0 && hay[2] == 0);
    obst[0].fila = 4;
    sr_vista(1000, 1400, 0, obst, 1, hay, cerc, celdas);
    caso("a 50 cm de la cara: cercanía 2 (o 3 en el borde)", hay[1] == 1 && cerc[1] >= 2);
    obst[0].fila = 3;
    sr_vista(1000, 1400, 0, obst, 1, hay, cerc, celdas);
    caso("a 70 cm de la cara: cercanía 1", hay[1] == 1 && cerc[1] == 1);
    obst[0].fila = 2;
    sr_vista(1000, 1400, 0, obst, 1, hay, cerc, celdas);
    caso("a 90 cm: ya no se ve", !hay[0] && !hay[1] && !hay[2]);
    obst[0].columna = 5;
    obst[0].fila = 7;
    sr_vista(1000, 1000, 0, obst, 1, hay, cerc, celdas);
    caso("un obstáculo DETRÁS del carro no se ve", !hay[0] && !hay[1] && !hay[2]);
    obst[0].columna = 4;
    obst[0].fila = 5;
    sr_vista(1000, 1400, 0, obst, 1, hay, cerc, celdas);
    caso("un obstáculo a la izquierda: zona izquierda", hay[0] == 1 && hay[2] == 0);
    obst[0].columna = 6;
    sr_vista(1000, 1400, 0, obst, 1, hay, cerc, celdas);
    caso("un obstáculo a la derecha: zona derecha", hay[2] == 1 && hay[0] == 0);
    obst[0].columna = 1;
    sr_vista(1000, 1400, 0, obst, 1, hay, cerc, celdas);
    caso("un obstáculo muy a un lado queda fuera del campo de visión", !hay[0] && !hay[1] && !hay[2]);
    obst[0].columna = 5;
    obst[0].fila = 5;
    sr_vista(1000, 1400, 90, obst, 1, hay, cerc, celdas);
    caso("si el carro gira a la derecha (rumbo 90) el obstáculo de antes ya no está de frente", !hay[1]);
    sr_vista(1000, 1400, 0, obst, 0, hay, cerc, celdas);
    caso("sin obstáculos no ve nada", !hay[0] && !hay[1] && !hay[2] && celdas[1] == 0);


    /* ── Casos simples: una caja de 20 cm (o de 40 cm de ancho) en una recta con espacio a los lados LLEGA ── */
    {
        int fila, col, todos = 1, esquivan = 0, casos_probados = 0;
        Punto caja[4];

        for (i = 0; i < 8; i++) {   /* la recta larga por la columna 5 (de la fila 7 a la 0) */
            larga[i].columna = 5;
            larga[i].fila = 7 - i;
        }
        for (fila = 1; fila <= 5; fila++) {
            for (col = 4; col <= 6; col++) {   /* centrado y a 20 cm de la línea (la cámara lo ve en un lado) */
                caja[0].columna = col;
                caja[0].fila = fila;
                codigo = sr_simular(larga, 8, NULL, caja, 1, 0, 0, &res);
                casos_probados++;
                if (codigo != SR_OK || !res.llego) {
                    todos = 0;
                    printf("      NO llegó con una caja de 20 cm en (%d,%d): estado=%d causa=%d\n", col, fila, res.estado_final, res.error_causa);
                }
                esquivan += (res.esquivadas >= 1);
            }
        }
        caso("una caja de 20 cm (una celda) en cualquier fila de 1 a 5 y a 0 o 20 cm de la línea: la ruta LLEGA", todos && casos_probados == 15);
        caso("... y las que están justo sobre la línea (al menos 5 de las 15) obligan a esquivar", esquivan >= 5);
        todos = 1;
        for (fila = 1; fila <= 4; fila++) {   /* una caja de 40 cm de ancho (dos celdas lado a lado) sobre la línea */
            caja[0].columna = 5;
            caja[0].fila = fila;
            caja[1].columna = 6;
            caja[1].fila = fila;
            codigo = sr_simular(larga, 8, NULL, caja, 2, 0, 0, &res);
            if (codigo != SR_OK || !res.llego) {
                todos = 0;
                printf("      NO llegó con una caja de 40 cm de ancho en (5..6,%d): estado=%d causa=%d\n", fila, res.estado_final, res.error_causa);
            }
        }
        caso("una caja de 40 cm de ancho (dos celdas) sobre la línea: LLEGA en las filas 1 a 4", todos);
        caso("con una caja la ruta termina cerca del final (menos de 15 cm) y sin salir de la cuadrícula", res.error_final_mm < 150 && (res.advertencias & SR_ADV_SALE_CUADRICULA) == 0);
    }

    /* ── La cámara virtual ve la caja de 20 cm ocupando varias celdas del detector (más cuanto más cerca) ── */
    obst[0].columna = 5;
    obst[0].fila = 5;
    sr_vista(1000, 1400, 0, obst, 1, hay, cerc, celdas);
    caso("la caja de 20 cm a 30 cm de su cara ocupa varias celdas del detector (al menos MIN_CELDAS = 7)", celdas[1] >= 7);
    {
        int cerca = celdas[1];

        sr_vista(1000, 1600, 0, obst, 1, hay, cerc, celdas);
        caso("... y más lejos (70 cm de su cara) ocupa menos celdas pero sigue siendo un obstáculo", hay[1] == 1 && celdas[1] >= 7 && celdas[1] < cerca);
    }

    /* ── Cuando no llega, la pantalla dice por qué ── */
    obst[0].columna = 5;
    obst[0].fila = 6;
    codigo = sr_simular(larga, 8, NULL, obst, 1, 0, 0, &res);
    {
        char causa[160];

        caso("un obstáculo pegado al punto de partida (no hay espacio para pasar): no llega", codigo == SR_OK && !res.llego);
        caso("... y sr_texto_causa dice por qué (los dos lados ocupados)", sr_texto_causa(&res, causa, sizeof causa) > 10 && strstr(causa, "lados") != NULL);
        res.llego = 0;
        res.error_causa = NAV_CAUSA_REINTENTOS;
        sr_texto_causa(&res, causa, sizeof causa);
        caso("causa REINTENTOS: \"El obstáculo bloqueó el camino y se agotaron los 2 reintentos.\"", strcmp(causa, "El obstáculo bloqueó el camino y se agotaron los 2 reintentos.") == 0);
        res.error_causa = NAV_CAUSA_MAX_ESQUIVADAS;
        sr_texto_causa(&res, causa, sizeof causa);
        caso("causa MAX_ESQUIVADAS: dice que se agotaron las 3 esquivadas", strstr(causa, "se agotaron las 3 esquivadas") != NULL);
        res.error_causa = NAV_CAUSA_NINGUNA;
        res.advertencias = SR_ADV_CORTADA;
        caso("sin error pero cortada por tiempo: dice que se cortó por tardar demasiado", sr_texto_causa(&res, causa, sizeof causa) > 10 && strstr(causa, "cortó") != NULL);
        res.llego = 1;
        caso("si llegó no hay causa que contar", sr_texto_causa(&res, causa, sizeof causa) == 0 && causa[0] == '\0');
        caso("sr_texto_causa con punteros nulos no revienta", sr_texto_causa(NULL, causa, sizeof causa) == 0 && sr_texto_causa(&res, NULL, 10) == 0);
    }


    /* ── Equilibrio de los lados: con el factor equivocado la recta se tuerce, con el calibrado llega ── */
    n = ruta_de(RE_PLANTILLA_RECTA, ruta);
    sr_poner_balance(1120, 986);   /* el carro real medido: el izquierdo empuja ~12 % más y el factor por defecto (986) solo corrige 1.4 % */
    codigo = sr_simular(ruta, n, NULL, NULL, 0, 0, 0, &res2);
    printf("      recta de 1 m con el factor por defecto (986): llego=%d error final=%d mm\n", res2.llego, res2.error_final_mm);
    sr_poner_balance(1120, 893);   /* el factor que calibra la prueba de recta (1000 / 1.12) */
    codigo = sr_simular(ruta, n, NULL, NULL, 0, 0, 0, &res);
    printf("      recta de 1 m con el factor calibrado (893): llego=%d error final=%d mm\n", res.llego, res.error_final_mm);
    caso("recta de 1 m con el factor por defecto (986) en un carro que empuja 12 % mas del izquierdo: se desvia mas de 15 cm del punto final", codigo == SR_OK && res2.error_final_mm > 150);
    caso("... con el factor calibrado (893) llega a menos de 5 cm del punto final", res.llego && res.error_final_mm < 50);
    sr_poner_balance(1000, 1000);
    codigo = sr_simular(ruta, n, NULL, NULL, 0, 0, 0, &res);
    caso("sin desbalance (1000 y 1000) la recta sale como siempre (menos de 8 cm)", codigo == SR_OK && res.llego && res.error_final_mm < 80);
    sr_poner_balance(100, 5000);
    sr_poner_balance(1000, 1000);
    codigo = sr_simular(ruta, n, NULL, NULL, 0, 0, 0, &res);
    caso("valores de balance fuera de 500 a 2000 se ignoran", codigo == SR_OK && res.error_final_mm < 80);

    /* ── Ruido de giro y dispersión ────────────────────────────────────── */
    n = ruta_de(RE_PLANTILLA_CUADRADO, ruta);
    codigo = sr_dispersion(ruta, n, NULL, NULL, 0, 0, 8, &disp0);
    caso("sin ruido las 8 corridas dan el mismo final (dispersión 0)", codigo == SR_OK && disp0.n == 8 && disp0.dispersion_mm == 0);
    codigo = sr_dispersion(ruta, n, NULL, NULL, 0, 20, 8, &disp);
    printf("      cuadrado con ruido de +-20 grados: llegaron %d de %d, error medio %d mm, maximo %d mm, dispersion %d mm\n", disp.llegaron, disp.n, disp.error_medio_mm, disp.error_max_mm, disp.dispersion_mm);
    caso("con ruido de 20 grados hay dispersión (los finales se abren)", codigo == SR_OK && disp.dispersion_mm > 50);
    caso("con ruido el error medio es mayor que sin ruido", disp.error_medio_mm > disp0.error_medio_mm);
    caso("con ruido el navegador termina la ruta igual (llegan todas)", disp.llegaron == disp.n);
    caso("la misma semilla da el mismo resultado (determinista)", es_repetible(ruta, n));
    caso("más corridas que el máximo se recortan", sr_dispersion(ruta, n, NULL, NULL, 0, 10, 99, &disp) == SR_OK && disp.n == SR_MAX_CORRIDAS);

    /* ── Errores ───────────────────────────────────────────────────────── */
    {
        Punto salto[2] = {{1, 1}, {4, 4}};

        codigo = sr_simular(salto, 2, NULL, NULL, 0, 0, 0, &res);
        caso("una ruta con salto: el navegador la rechaza (error de ruta con su código)", codigo == SR_ERR_RUTA && res.codigo_ruta == TRAY_ERR_SALTO);
    }
    caso("una ruta de un solo punto: error de argumento", sr_simular(ruta, 1, NULL, NULL, 0, 0, 0, &res) == SR_ERR_ARGUMENTO);
    caso("punteros nulos: error de argumento", sr_simular(NULL, 3, NULL, NULL, 0, 0, 0, &res) == SR_ERR_ARGUMENTO && sr_simular(ruta, n, NULL, NULL, 0, 0, 0, NULL) == SR_ERR_ARGUMENTO && sr_dispersion(ruta, n, NULL, NULL, 0, 0, 0, &disp) == SR_ERR_ARGUMENTO);
    {
        char texto[100];

        caso("textos de advertencia", sr_texto_advertencia(SR_ADV_NO_LLEGO, texto, sizeof texto) > 10 && sr_texto_advertencia(1 << 20, texto, sizeof texto) == 0);
    }

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
