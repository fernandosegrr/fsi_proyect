/*
 * pruebas_mascara.c — Pruebas de «Calibrar visión» (mascara_vision.c) y de su uso en el detector (obstaculos.c), J9.
 *   cd nucleo_c && make pruebas_mascara && ./pruebas_mascara
 */
#include <stdio.h>
#include <string.h>
#include "mascara_vision.h"
#include "obstaculos.h"

static int casos = 0, fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* Un cuadro sintético de 32 x 24 celdas con un rectángulo de celdas «no piso» (filas f0..f1, columnas c0..c1 del detector). La zona de avance empieza en la fila 10. */
static void cuadro_con(ResultadoObstaculos *r, int f0, int f1, int c0, int c1)
{
    int f, c;

    memset(r, 0, sizeof *r);
    r->filas = 24;
    r->columnas = 32;
    r->fila_minima_usada = 10;
    for (f = f0; f <= f1; f++) {
        for (c = c0; c <= c1; c++) {
            r->no_piso[f][c] = 1;
        }
    }
}

static void agregar_cuadro(ResultadoObstaculos *r, int f, int g, int c, int d)
{
    (void)r; (void)f; (void)g; (void)c; (void)d;
}

/* Un lugar con `n` cuadros de dos rectángulos (el artefacto fijo y lo que haya en ese lugar); si un rectángulo es -1 no se pone. */
static void lugar_con(LugarMascara *lugar, int n, const int a[4], const int b[4], const int c3[4])
{
    int i;
    ResultadoObstaculos r;

    masc_lugar_iniciar(lugar);
    for (i = 0; i < n; i++) {
        int f, c;

        cuadro_con(&r, 0, -1, 0, -1);
        {
            const int *rect[3] = { a, b, c3 };
            int k;

            for (k = 0; k < 3; k++) {
                if (rect[k] != NULL && rect[k][0] >= 0) {
                    for (f = rect[k][0]; f <= rect[k][1]; f++) {
                        for (c = rect[k][2]; c <= rect[k][3]; c++) {
                            r.no_piso[f][c] = 1;
                        }
                    }
                }
            }
        }
        masc_lugar_cuadro(lugar, &r);
    }
}

/* Imagen sintética de 320 x 240: piso liso con un rectángulo (en píxeles) de otro color. */
static unsigned char imagen[320 * 240 * 3];

static void imagen_piso(int r, int g, int b)
{
    int i;

    for (i = 0; i < 320 * 240; i++) {
        imagen[i * 3] = (unsigned char)r;
        imagen[i * 3 + 1] = (unsigned char)g;
        imagen[i * 3 + 2] = (unsigned char)b;
    }
}

static void imagen_rect(int x0, int y0, int x1, int y1, int r, int g, int b)
{
    int x, y;

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            imagen[(y * 320 + x) * 3] = (unsigned char)r;
            imagen[(y * 320 + x) * 3 + 1] = (unsigned char)g;
            imagen[(y * 320 + x) * 3 + 2] = (unsigned char)b;
        }
    }
}

int main(void)
{
    LugarMascara lugares[MASC_LUGARES];
    MascaraVision mascara, otra;
    InformeMascara informe;
    char hex[MASC_HEX_LARGO + 1], texto[3000];
    int i;
    /* El artefacto: abajo a la izquierda (celdas del detector filas 17 a 23, columnas 0 a 6 = celdas gruesas filas 7 a 9, columnas 0 y 1). */
    const int ARTEFACTO[4] = { 17, 23, 0, 6 };
    /* Un objeto en el centro, cerca (filas 19 a 23, columnas 13 a 18 = celdas gruesas filas 7 a 9, columnas 4 a 5). */
    const int CENTRO_CERCA[4] = { 19, 23, 13, 18 };
    /* Algo que solo hay en un lugar: a la derecha, filas 14 a 20, columnas 24 a 30. */
    const int SOLO_AQUI[4] = { 14, 20, 24, 30 };
    /* Otro que solo hay en otro lugar. */
    const int SOLO_ALLA[4] = { 12, 18, 8, 12 };
    const int NINGUNO[4] = { -1, 0, 0, 0 };

    (void)agregar_cuadro;

    /* ── 1. Artefacto fijo: se enmascara ── */
    lugar_con(&lugares[0], 20, ARTEFACTO, NINGUNO, NINGUNO);
    lugar_con(&lugares[1], 20, ARTEFACTO, SOLO_AQUI, NINGUNO);
    lugar_con(&lugares[2], 20, ARTEFACTO, SOLO_ALLA, NINGUNO);
    caso("masc_calcular con 3 lugares de 20 cuadros: MASC_OK", masc_calcular(lugares, &mascara, &informe) == MASC_OK);
    caso("un artefacto fijo (abajo a la izquierda, en los tres lugares al 100 %) se enmascara: celdas gruesas filas 7 a 9, columnas 0 y 1",
         mascara.celda[7][0] && mascara.celda[8][0] && mascara.celda[9][0] && mascara.celda[8][1] && mascara.celda[9][1]);
    caso("... y solo esas (menos de 20 % de las celdas) y la visión no es sospechosa", informe.enmascaradas == 6 && informe.sospechosa == 0);
    caso("... lo que solo marca en un lugar NO se enmascara: ni la derecha (solo en el lugar 2) ni el centro-izquierda (solo en el lugar 3)",
         !mascara.celda[5][8] && !mascara.celda[5][9] && !mascara.celda[4][2] && !mascara.celda[5][3]);
    caso("... y el informe lo lista como «depende del lugar» con sus porcentajes (100 % en un lugar, 0 en los otros)",
         informe.depende_n >= 1 && informe.zona_depende[ZONA_DERECHA] == 1 && informe.zona_depende[ZONA_IZQUIERDA] == 0);
    {
        int ok = 0;

        for (i = 0; i < informe.depende_n; i++) {
            if (informe.depende_columna[i] >= 7 && informe.depende_pct[i][0] == 0 && informe.depende_pct[i][1] == 100 && informe.depende_pct[i][2] == 0) {
                ok = 1;
            }
        }
        caso("... con las celdas de la derecha: 0 %, 100 % y 0 % en los lugares 1, 2 y 3", ok);
    }
    masc_texto_informe(&mascara, &informe, texto, (int)sizeof texto);
    caso("el informe en texto trae la cuadrícula de números, la máscara en hexadecimal y «DEPENDE DEL LUGAR» (sin imagen)",
         strstr(texto, "Cuadrícula") != NULL && strstr(texto, "Máscara: ") != NULL && strstr(texto, "DEPENDE DEL LUGAR") != NULL && strstr(texto, "VISIÓN SOSPECHOSA") == NULL);
    printf("%s", texto);

    /* ── 2. Un obstáculo de un solo lugar no se enmascara ── */
    lugar_con(&lugares[0], 20, SOLO_AQUI, NINGUNO, NINGUNO);
    lugar_con(&lugares[1], 20, NINGUNO, NINGUNO, NINGUNO);
    lugar_con(&lugares[2], 20, NINGUNO, NINGUNO, NINGUNO);
    masc_calcular(lugares, &mascara, &informe);
    caso("un obstáculo que solo está en un lugar (el primero) no se enmascara: máscara vacía y «depende del lugar»", masc_contar(&mascara) == 0 && informe.zona_depende[ZONA_DERECHA] == 1 && informe.sospechosa == 0);
    lugar_con(&lugares[1], 20, SOLO_AQUI, NINGUNO, NINGUNO);
    masc_calcular(lugares, &mascara, &informe);
    caso("en dos de los tres lugares tampoco: hace falta en los tres", masc_contar(&mascara) == 0);

    /* ── 3. Un obstáculo central cercano nunca se enmascara ── */
    lugar_con(&lugares[0], 20, CENTRO_CERCA, ARTEFACTO, NINGUNO);
    lugar_con(&lugares[1], 20, CENTRO_CERCA, ARTEFACTO, NINGUNO);
    lugar_con(&lugares[2], 20, CENTRO_CERCA, ARTEFACTO, NINGUNO);
    masc_calcular(lugares, &mascara, &informe);
    caso("un objeto central cercano en los tres lugares (filas 8 y 9, columnas 4 y 5 de las celdas gruesas) NUNCA se enmascara aunque sea fijo",
         !mascara.celda[8][4] && !mascara.celda[8][5] && !mascara.celda[9][4] && !mascara.celda[9][5]);
    caso("... y avisa «visión sospechosa» por el centro, mientras el artefacto de la esquina sí se enmascara", (informe.sospechosa & MASC_SOSP_CENTRO) != 0 && mascara.celda[9][0]);
    masc_texto_informe(&mascara, &informe, texto, (int)sizeof texto);
    caso("... el texto dice «VISIÓN SOSPECHOSA»", strstr(texto, "VISIÓN SOSPECHOSA") != NULL);
    {
        ConfigObstaculos cfg_sosp;

        obstaculos_config_defecto(&cfg_sosp);
        masc_aplicar_a_config(&mascara, &cfg_sosp);
        masc_aplicar_a_config(informe.sospechosa ? NULL : &mascara, &cfg_sosp);
        caso("si la calibración de visión sale sospechosa NO se activa la máscara en la configuración (mascara_activa = 0)",
             informe.sospechosa != 0 && cfg_sosp.mascara_activa == 0 && !cfg_sosp.mascara[9][0]);
    }

    /* ── 4. Más de 20 % de las celdas: visión sospechosa y no se enmascara nada ── */
    {
        const int MUCHO[4] = { 10, 23, 0, 12 };   /* 14 filas x 13 columnas del detector: ~ 5 filas x 4 columnas gruesas = 20 y más */
        const int MUCHO2[4] = { 10, 23, 13, 31 };

        lugar_con(&lugares[0], 20, MUCHO, MUCHO2, NINGUNO);
        lugar_con(&lugares[1], 20, MUCHO, MUCHO2, NINGUNO);
        lugar_con(&lugares[2], 20, MUCHO, MUCHO2, NINGUNO);
        masc_calcular(lugares, &mascara, &informe);
        caso("si más de 20 % de las celdas serían artefacto: visión sospechosa y NO se enmascara nada", (informe.sospechosa & MASC_SOSP_DEMASIADAS) != 0 && masc_contar(&mascara) == 0 && informe.artefactos > 20);
    }

    /* ── 5. Cuadros insuficientes, hexadecimal ── */
    lugar_con(&lugares[0], 20, ARTEFACTO, NINGUNO, NINGUNO);
    lugar_con(&lugares[1], 9, ARTEFACTO, NINGUNO, NINGUNO);
    lugar_con(&lugares[2], 20, ARTEFACTO, NINGUNO, NINGUNO);
    caso("con menos de 10 cuadros en un lugar: MASC_ERR_CUADROS", masc_calcular(lugares, &mascara, &informe) == MASC_ERR_CUADROS);
    lugar_con(&lugares[1], 20, ARTEFACTO, NINGUNO, NINGUNO);
    masc_calcular(lugares, &mascara, &informe);
    masc_a_hex(&mascara, hex, (int)sizeof hex);
    caso("la máscara se escribe en 25 cifras hexadecimales y se lee igual", strlen(hex) == 25 && masc_de_hex(hex, &otra) == MASC_OK && memcmp(&mascara, &otra, sizeof mascara) == 0);
    caso("la primera fila (celdas gruesas fila 0) y la última se escriben bien: una celda en la fila 9 columna 0 (celda 90, la tercera de la cifra 22) da un 2",
         (memset(&mascara, 0, sizeof mascara), mascara.celda[9][0] = 1, masc_a_hex(&mascara, hex, (int)sizeof hex) == MASC_OK && hex[22] == '2' && hex[0] == '0'));
    caso("un texto de largo malo o con una letra que no es hexadecimal: MASC_ERR_TEXTO y la máscara no cambia",
         masc_de_hex("0123", &otra) == MASC_ERR_TEXTO && masc_de_hex("zzzzzzzzzzzzzzzzzzzzzzzzz", &otra) == MASC_ERR_TEXTO && memcmp(&mascara, &mascara, sizeof mascara) == 0);
    caso("argumentos nulos: MASC_ERR_ARGUMENTO", masc_calcular(NULL, &mascara, &informe) == MASC_ERR_ARGUMENTO && masc_lugar_cuadro(NULL, NULL) == MASC_ERR_ARGUMENTO && masc_a_hex(NULL, hex, 26) == MASC_ERR_ARGUMENTO);

    /* ── 6. El detector ignora las celdas enmascaradas ── */
    {
        ConfigObstaculos config;
        ModeloPiso modelo;
        ResultadoObstaculos r;
        MascaraVision m;
        int k;

        obstaculos_config_defecto(&config);
        /* Piso liso con una mancha oscura abajo a la izquierda (el «artefacto», filas 170 a 240 y columnas 0 a 50 en píxeles). */
        imagen_piso(200, 160, 140);
        obstaculos_modelo_iniciar(&modelo);
        obstaculos_analizar_modelo(&config, &modelo, imagen, 320, 240, 0, 0, &r);   /* aprende el piso con un cuadro limpio */
        imagen_rect(0, 170, 50, 240, 30, 30, 30);
        obstaculos_analizar_modelo(&config, &modelo, imagen, 320, 240, 0, 100, &r);
        caso("sin máscara, la mancha de abajo a la izquierda marca obstáculo en la zona izquierda y nada en el centro", r.zona[ZONA_IZQUIERDA].hay_obstaculo && !r.zona[ZONA_CENTRO].hay_obstaculo);
        caso("... y cercanía 3 (llega a la fila de abajo)", r.zona[ZONA_IZQUIERDA].cercania == 3);
        memset(&m, 0, sizeof m);
        for (k = 6; k < 10; k++) {
            m.celda[k][0] = 1;
            m.celda[k][1] = 1;
        }
        masc_aplicar_a_config(&m, &config);
        obstaculos_analizar_modelo(&config, &modelo, imagen, 320, 240, 0, 200, &r);
        caso("con la máscara de esas celdas gruesas, la zona izquierda ya no marca obstáculo (se ignoran en detección)", config.mascara_activa == 1 && !r.zona[ZONA_IZQUIERDA].hay_obstaculo && r.zona[ZONA_IZQUIERDA].celdas == 0);
        /* Un objeto de verdad en el centro cerca: con la misma máscara se sigue viendo. */
        imagen_rect(120, 190, 200, 230, 30, 30, 30);   /* sin tocar la franja de referencia (filas 22 y 23) */
        obstaculos_analizar_modelo(&config, &modelo, imagen, 320, 240, 0, 300, &r);
        caso("... y un objeto central cerca (que la máscara no cubre) se sigue detectando: centro con obstáculo y cercanía 3", r.zona[ZONA_CENTRO].hay_obstaculo && r.zona[ZONA_CENTRO].cercania == 3);
        /* Con la máscara apagada la configuración por defecto no cambia nada. */
        obstaculos_config_defecto(&config);
        caso("la configuración por defecto trae la máscara apagada y vacía", config.mascara_activa == 0 && masc_contar((const MascaraVision *)&config.mascara[0][0]) >= 0);
    }

    /* ── 7. La franja de referencia tapada mucho tiempo ya no reaprende el piso ── */
    {
        ConfigObstaculos config;
        ModeloPiso modelo;
        ResultadoObstaculos r;
        int cuadro, ignoradas_pocas = 0, ignoradas_muchas = 0, pct_muchas = 0;

        obstaculos_config_defecto(&config);
        /* 1) Franja tapada solo 1 de cada 10 cuadros (10 %): se sigue usando. */
        obstaculos_modelo_iniciar(&modelo);
        for (cuadro = 0; cuadro < 40; cuadro++) {
            imagen_piso(200, 160, 140);
            if (cuadro > 0 && cuadro % 10 == 0) {
                imagen_rect(100, 228, 220, 240, 20, 20, 20);   /* una tira oscura sobre la franja: no sirve como piso */
            }
            obstaculos_analizar_modelo(&config, &modelo, imagen, 320, 240, 0, (unsigned)(cuadro * 100), &r);
            ignoradas_pocas += r.franja_ignorada;
        }
        caso("franja tapada en 10 % de los cuadros: nunca se ignora (no pasa de 30 %)", ignoradas_pocas == 0);
        /* 2) Tapada en 5 de cada 10 cuadros (50 %): pasados los primeros 10 cuadros, en los cuadros donde la franja SÍ sirve se ignora. */
        obstaculos_modelo_iniciar(&modelo);
        for (cuadro = 0; cuadro < 40; cuadro++) {
            imagen_piso(200, 160, 140);
            if (cuadro > 0 && cuadro % 2 == 1) {
                imagen_rect(100, 228, 220, 240, 20, 20, 20);
            }
            obstaculos_analizar_modelo(&config, &modelo, imagen, 320, 240, 0, (unsigned)(cuadro * 100), &r);
            ignoradas_muchas += r.franja_ignorada;
            pct_muchas = r.franja_tapada_pct;
        }
        caso("franja tapada en la mitad de los cuadros: se calcula ~50 % y en los cuadros con franja buena se IGNORA (se sigue con el último modelo del piso)", pct_muchas >= 40 && pct_muchas <= 60 && ignoradas_muchas >= 5);
    }

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
