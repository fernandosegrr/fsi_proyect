/*
 * pruebas_vision_tele.c — Pruebas de la telemetría numérica de la visión (vision_tele.c) con cuadros SINTÉTICOS (ninguna imagen real).
 */
#include <stdio.h>
#include <string.h>

#include "vision_tele.h"

static int casos = 0, fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK    " : "FALLO ", nombre);
}

#define COLS 24
#define FILS 17

static unsigned char celdas[COLS * FILS * 3];

static void pintar(int f0, int f1, int c0, int c1, int r, int v, int a)
{
    int f, c;

    for (f = f0; f <= f1; f++) {
        for (c = c0; c <= c1; c++) {
            unsigned char *p = celdas + (f * COLS + c) * 3;

            p[0] = (unsigned char)r;
            p[1] = (unsigned char)v;
            p[2] = (unsigned char)a;
        }
    }
}

int main(void)
{
    ModeloPiso modelo;
    ConfigObstaculos config;
    ResultadoObstaculos res;
    VisionTele t;
    char texto[VT_TEXTO_MAX];
    int codigo, i, suma;

    obstaculos_config_defecto(&config);
    obstaculos_modelo_iniciar(&modelo);
    modelo.valido = 1;
    modelo.grupos = 2;
    modelo.color256[0][0] = 120 * 256; modelo.color256[0][1] = 118 * 256; modelo.color256[0][2] = 112 * 256;
    modelo.color256[1][0] = 90 * 256;  modelo.color256[1][1] = 88 * 256;  modelo.color256[1][2] = 84 * 256;
    modelo.peso[0] = 150;
    modelo.peso[1] = 40;
    memset(&res, 0, sizeof res);
    res.columnas = COLS;
    res.filas = FILS;
    res.fila_minima_usada = 7;
    res.ganancia_mil = 1000;

    /* 1. Piso idéntico al primer grupo: todo en la primera cubeta. */
    pintar(0, FILS - 1, 0, COLS - 1, 120, 118, 112);
    codigo = vision_tele_calcular(celdas, COLS, FILS, &modelo, &config, &res, &t);
    caso("piso igual al modelo: VT_OK y 240 celdas de la zona de avance (filas 7 a 16)", codigo == VT_OK && t.celdas == 240 && t.fila_minima == 7 && t.con_modelo == 1);
    caso("... todas en la primera cubeta, distancia 0", t.hist[0] == 240 && t.dist_media == 0 && t.dist_p90 == 0 && t.dist_max == 0);
    caso("... brillo 116, sin celdas quemadas ni negras, croma 8", t.brillo_media == 116 && t.brillo_p10 == 116 && t.brillo_p90 == 116 && t.quemadas == 0 && t.negras == 0 && t.croma_media == 8);
    caso("... sin celdas no piso", t.no_piso_total == 0);

    /* 2. Una mancha clara (sol) en la izquierda, cerca: sube unas celdas de cubeta y las cuenta por zona y banda cuando el detector las marcó. */
    pintar(14, 16, 2, 5, 200, 196, 180);
    for (i = 14; i <= 16; i++) {
        int c;

        for (c = 2; c <= 5; c++) {
            res.no_piso[i][c] = 1;
        }
    }
    codigo = vision_tele_calcular(celdas, COLS, FILS, &modelo, &config, &res, &t);
    caso("mancha de 12 celdas a 226 de distancia: van a la ultima cubeta y el resto sigue en la primera", codigo == VT_OK && t.hist[VT_CUBETAS - 1] == 12 && t.hist[0] == 228 && t.dist_max == 226);
    caso("... 12 celdas no piso, todas en la zona izquierda y en la banda cercana", t.no_piso_total == 12 && t.no_piso[ZONA_IZQUIERDA][2] == 12 && t.no_piso[ZONA_CENTRO][2] == 0 && t.no_piso[ZONA_DERECHA][0] == 0);
    caso("... por fila: 4 en cada una de las filas 14, 15 y 16", t.no_piso_fila[14] == 4 && t.no_piso_fila[15] == 4 && t.no_piso_fila[16] == 4 && t.no_piso_fila[13] == 0);
    caso("... el percentil 90 del brillo no cambia (la mancha es el 5 %) y la media sube", t.brillo_p90 == 116 && t.brillo_media > 116);
    suma = 0;
    for (i = 0; i < VT_CUBETAS; i++) {
        suma += t.hist[i];
    }
    caso("... el histograma suma las celdas medidas", suma == t.celdas);

    /* 3. La cámara cambió su exposición: TODO el piso sale 18 por canal más claro. El histograma entero se corre a la cubeta de 40 a 59. */
    pintar(0, FILS - 1, 0, COLS - 1, 138, 136, 130);
    memset(res.no_piso, 0, sizeof res.no_piso);
    codigo = vision_tele_calcular(celdas, COLS, FILS, &modelo, &config, &res, &t);
    caso("exposicion corrida 18 por canal: todas las celdas a 54 de distancia (cubeta 40-59), ninguna en la primera", codigo == VT_OK && t.hist[4] == 240 && t.hist[0] == 0 && t.dist_p50 == 54);

    /* 4. Segundo grupo del piso (baldosa oscura): la distancia es al grupo MÁS CERCANO. */
    pintar(0, FILS - 1, 0, COLS - 1, 92, 90, 85);
    codigo = vision_tele_calcular(celdas, COLS, FILS, &modelo, &config, &res, &t);
    caso("celdas junto al segundo grupo: distancia 5 (al mas cercano), primera cubeta", codigo == VT_OK && t.hist[0] == 240 && t.dist_max == 5);

    /* 5. Quemadas y negras. */
    pintar(7, 7, 0, COLS - 1, 255, 255, 250);
    pintar(8, 8, 0, 11, 3, 2, 2);
    codigo = vision_tele_calcular(celdas, COLS, FILS, &modelo, &config, &res, &t);
    caso("24 celdas quemadas y 12 negras", codigo == VT_OK && t.quemadas == 24 && t.negras == 12);

    /* 6. Sin piso aprendido no hay distancias, pero sí brillo y conteos. */
    codigo = vision_tele_calcular(celdas, COLS, FILS, NULL, &config, &res, &t);
    caso("sin modelo: con_modelo 0, histograma en ceros y el brillo se mide igual", codigo == VT_OK && t.con_modelo == 0 && t.hist[0] == 0 && t.grupos == 0 && t.celdas == 240 && t.brillo_media > 0);

    /* 7. El texto: una sola línea, solo números, dentro de la capacidad recomendada. */
    pintar(0, FILS - 1, 0, COLS - 1, 120, 118, 112);
    vision_tele_calcular(celdas, COLS, FILS, &modelo, &config, &res, &t);
    codigo = vision_tele_texto(&t, texto, (int)sizeof texto);
    caso("texto: VT_OK, empieza con VTEL, trae la cuadricula y cabe en VT_TEXTO_MAX", codigo == VT_OK && strncmp(texto, "VTEL c=24x17 fm=7 n=240", 23) == 0 && strlen(texto) < VT_TEXTO_MAX);
    caso("... trae el histograma, las zonas, las filas y los dos grupos con su peso", strstr(texto, " h=240,0,0,0,0,0,0,0 ") != NULL && strstr(texto, " z=0,0,0|0,0,0|0,0,0 ") != NULL &&
         strstr(texto, " f=0,0,0,0,0,0,0,0,0,0 ") != NULL && strstr(texto, " g=2:120,118,112,150:90,88,84,40") != NULL);
    caso("... no trae saltos de linea", strchr(texto, '\n') == NULL);
    codigo = vision_tele_texto(&t, texto, 40);
    caso("texto con poca capacidad: VT_ERR_TAMANO y queda terminado en cero", codigo == VT_ERR_TAMANO && strlen(texto) < 40);

    /* 8. Argumentos malos. */
    caso("punteros nulos: VT_ERR_ARGUMENTO", vision_tele_calcular(NULL, COLS, FILS, &modelo, &config, &res, &t) == VT_ERR_ARGUMENTO &&
         vision_tele_calcular(celdas, COLS, FILS, &modelo, &config, NULL, &t) == VT_ERR_ARGUMENTO && vision_tele_texto(NULL, texto, 100) == VT_ERR_ARGUMENTO);
    caso("cuadricula fuera de limites: VT_ERR_TAMANO", vision_tele_calcular(celdas, 2, FILS, &modelo, &config, &res, &t) == VT_ERR_TAMANO &&
         vision_tele_calcular(celdas, COLS, OBS_MAX_FILAS + 1, &modelo, &config, &res, &t) == VT_ERR_TAMANO);
    res.fila_minima_usada = 99;
    caso("fila minima absurda en el resultado: se mide toda la imagen sin leer fuera", vision_tele_calcular(celdas, COLS, FILS, &modelo, &config, &res, &t) == VT_OK && t.celdas == COLS * FILS);

    printf("%d casos, %d fallos\n", casos, fallos);
    return fallos ? 1 : 0;
}
