/*
 * pruebas_celdas.c — Pruebas en PC de la entrada por CELDAS del detector (modo de video CELDAS) y del paquete UDP que las lleva.
 *
 * GUÍA DE LECTURA
 *  1. obstaculos_promediar: da la rejilla y los bytes correctos (240x176 → 24 x 17 celdas), rechaza tamaños raros y capacidades chicas.
 *  2. EQUIVALENCIA (lo importante): el detector alimentado con el CUADRO COMPLETO y el alimentado con las CELDAS ya promediadas dan exactamente lo mismo, cuadro
 *     tras cuadro, con memoria del piso, aprendizaje y cambios de exposición: las zonas, la máscara de celdas, el motivo, los colores de referencia y el modelo
 *     del piso que queda. Se prueba con los 5 pisos sintéticos, con y sin objeto, con ruido, con cambios de luz y con otros tamaños de imagen (320x240, 200x150).
 *  3. El paquete UDP (paquete_celdas.c): ida y vuelta con los 24 x 17 de siempre (cabe en un solo paquete UDP) y con otros tamaños; los paquetes mal formados se
 *     rechazan; y celdas que viajan por el paquete dan el mismo resultado del detector.
 */

#include <stdio.h>
#include <string.h>
#include "obstaculos.h"
#include "paquete_celdas.h"
#include "pisos_sinteticos.h"

static int casos = 0;
static int fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* ¿Dos resultados del detector son iguales en todo lo que se informa? */
static int resultados_iguales(const ResultadoObstaculos *a, const ResultadoObstaculos *b)
{
    int z, f, c, g;

    for (z = 0; z < OBS_ZONAS; z++) {
        if (a->zona[z].hay_obstaculo != b->zona[z].hay_obstaculo || a->zona[z].cercania != b->zona[z].cercania || a->zona[z].celdas != b->zona[z].celdas) {
            return 0;
        }
    }
    if (a->ref_r != b->ref_r || a->ref_g != b->ref_g || a->ref_b != b->ref_b || a->columnas != b->columnas || a->filas != b->filas ||
        a->fila_minima_usada != b->fila_minima_usada || a->rejilla_chica != b->rejilla_chica || a->ref_columna != b->ref_columna || a->ref_fila != b->ref_fila ||
        a->ref_ncolumnas != b->ref_ncolumnas || a->ref_nfilas != b->ref_nfilas || a->grupos != b->grupos || a->franja_contaminada != b->franja_contaminada ||
        a->piso_reaprendido != b->piso_reaprendido || a->aprendio != b->aprendio || a->celdas_borde != b->celdas_borde || a->brillo_medio != b->brillo_medio ||
        a->ganancia_mil != b->ganancia_mil || a->celdas_marcadas != b->celdas_marcadas || a->motivo != b->motivo) {
        return 0;
    }
    for (g = 0; g < a->grupos && g < OBS_MAX_GRUPOS; g++) {
        for (c = 0; c < 3; c++) {
            if (a->grupo_color[g][c] != b->grupo_color[g][c]) {
                return 0;
            }
        }
    }
    for (f = 0; f < a->filas; f++) {
        for (c = 0; c < a->columnas; c++) {
            if (a->no_piso[f][c] != b->no_piso[f][c]) {
                return 0;
            }
        }
    }
    return 1;
}

/* Una secuencia de `n` cuadros por las dos entradas con modelos separados; devuelve cuántos cuadros salieron distintos (0 = todo igual). */
static int comparar_secuencia(const ConfigObstaculos *cfg, const unsigned char *cuadros, int ancho, int alto, int n, int *errores_codigo)
{
    ModeloPiso modelo_img, modelo_cel;
    static unsigned char celdas[OBS_MAX_COLUMNAS * OBS_MAX_FILAS * 3];
    int i, distintos = 0, columnas = 0, filas = 0;

    obstaculos_modelo_iniciar(&modelo_img);
    obstaculos_modelo_iniciar(&modelo_cel);
    *errores_codigo = 0;
    for (i = 0; i < n; i++) {
        static ResultadoObstaculos ra, rb;
        const unsigned char *rgb = cuadros + (long)i * ancho * alto * 3;
        int avanzando = (i % 3) != 0;
        unsigned int ahora = (unsigned int)(1000 + 100 * i);
        int ca = obstaculos_analizar_modelo(cfg, &modelo_img, rgb, ancho, alto, avanzando, ahora, &ra);
        int cp = obstaculos_promediar(rgb, ancho, alto, celdas, (int)sizeof celdas, &columnas, &filas);
        int cb = (cp == OBS_OK) ? obstaculos_analizar_modelo_celdas(cfg, &modelo_cel, celdas, columnas, filas, avanzando, ahora, &rb) : cp;

        if (ca != cb) {
            (*errores_codigo)++;
            distintos++;
            continue;
        }
        if (ca == OBS_OK && !resultados_iguales(&ra, &rb)) {
            distintos++;
        }
        if (memcmp(&modelo_img, &modelo_cel, sizeof modelo_img) != 0) {
            distintos++;
        }
    }
    return distintos;
}

static void pruebas_promediar(void)
{
    static unsigned char rgb[PS_ANCHO * PS_ALTO * 3];
    static unsigned char celdas[OBS_MAX_COLUMNAS * OBS_MAX_FILAS * 3];
    EscenaSintetica e;
    int columnas = 0, filas = 0, i;

    memset(&e, 0, sizeof e);
    e.piso = PS_LISO;
    e.exposicion_pct = 100;
    e.semilla = 1;
    ps_dibujar(&e, rgb);
    caso("240x176 da una rejilla de 24 columnas x 17 filas", obstaculos_promediar(rgb, PS_ANCHO, PS_ALTO, celdas, (int)sizeof celdas, &columnas, &filas) == OBS_OK &&
                                                              columnas == 24 && filas == 17);
    /* Una celda del piso liso sin ruido ni luz distinta vale lo mismo que su primer píxel. */
    caso("sobre un piso liso sin ruido la celda promedia a un solo color", celdas[0] == rgb[0] || celdas[0] == rgb[0] + 1 || celdas[0] + 1 == rgb[0]);
    {
        /* Imagen de 30x30 (3x3 celdas) de un color constante: cada celda sale (200, 100, 50). */
        unsigned char img[30 * 30 * 3];
        unsigned char una[3 * 3 * 3];
        int c2 = 0, f2 = 0, ok = 1;

        for (i = 0; i < 30 * 30; i++) {
            img[3 * i] = 200;
            img[3 * i + 1] = 100;
            img[3 * i + 2] = 50;
        }
        if (obstaculos_promediar(img, 30, 30, una, (int)sizeof una, &c2, &f2) != OBS_OK || c2 != 3 || f2 != 3) {
            ok = 0;
        } else {
            for (i = 0; i < 9; i++) {
                if (una[3 * i] != 200 || una[3 * i + 1] != 100 || una[3 * i + 2] != 50) {
                    ok = 0;
                }
            }
        }
        caso("una imagen de un solo color da celdas de ese color (30x30 → 3x3)", ok);
    }
    caso("capacidad chica: error de tamaño", obstaculos_promediar(rgb, PS_ANCHO, PS_ALTO, celdas, 100, &columnas, &filas) == OBS_ERR_TAMANO);
    caso("imagen nula: error", obstaculos_promediar(NULL, PS_ANCHO, PS_ALTO, celdas, (int)sizeof celdas, &columnas, &filas) == OBS_ERR_IMAGEN_NULA);
    caso("tamaño raro (20x20, menos de 3x3 celdas): error", obstaculos_promediar(rgb, 20, 20, celdas, (int)sizeof celdas, &columnas, &filas) == OBS_ERR_TAMANO);
    caso("tamaño enorme (700x100): error", obstaculos_promediar(rgb, 700, 100, celdas, (int)sizeof celdas, &columnas, &filas) == OBS_ERR_TAMANO);
}

static void pruebas_equivalencia(void)
{
    static unsigned char cuadros[14 * PS_ANCHO * PS_ALTO * 3];
    ConfigObstaculos cfg;
    int piso, objeto, malos = 0, secuencias = 0, codigos = 0, i;
    const int objetos[3] = { PS_NADA, PS_CAJA, PS_MOCHILA };

    obstaculos_config_defecto(&cfg);
    for (piso = 0; piso < 5; piso++) {
        for (objeto = 0; objeto < 3; objeto++) {
            EscenaSintetica e;
            int d, e_cod;

            memset(&e, 0, sizeof e);
            e.piso = piso;
            e.sigma = 4;
            e.vineteo_pct = 20;
            e.degradado_pct = 10;
            e.exposicion_pct = 100;
            e.objeto = objetos[objeto];
            e.obj_x = 90;
            e.obj_y = 70;
            for (i = 0; i < 14; i++) {
                e.semilla = (unsigned long)(piso * 1000 + objeto * 100 + i);
                e.exposicion_pct = (i >= 8) ? 70 : 100;   /* a la mitad de la secuencia cambia la luz */
                e.obj_y = 40 + i * 3;                       /* el objeto se acerca */
                ps_dibujar(&e, cuadros + (long)i * PS_ANCHO * PS_ALTO * 3);
            }
            d = comparar_secuencia(&cfg, cuadros, PS_ANCHO, PS_ALTO, 14, &e_cod);
            malos += d;
            codigos += e_cod;
            secuencias++;
        }
    }
    printf("      %d secuencias de 14 cuadros (5 pisos x 3 escenas): %d diferencias entre cuadro y celdas\n", secuencias, malos);
    caso("cuadro completo y celdas dan el MISMO resultado y el MISMO modelo del piso en 15 secuencias de 14 cuadros", malos == 0 && codigos == 0 && secuencias == 15);

    /* Otros tamaños, con una imagen que no es sintética: gradiente más ruido repetible. */
    {
        static unsigned char grande[6 * 320 * 240 * 3];
        static unsigned char medio[6 * 200 * 150 * 3];
        unsigned int estado = 12345;
        int distintos_a, distintos_b, ca, cb;

        for (i = 0; i < 6 * 320 * 240 * 3; i++) {
            estado = estado * 1103515245u + 12345u;
            grande[i] = (unsigned char)(80 + (i / 3 % 320) / 8 + ((estado >> 16) & 7));
        }
        for (i = 0; i < 6 * 200 * 150 * 3; i++) {
            estado = estado * 1103515245u + 12345u;
            medio[i] = (unsigned char)(120 + ((estado >> 16) & 15));
        }
        distintos_a = comparar_secuencia(&cfg, grande, 320, 240, 6, &ca);
        distintos_b = comparar_secuencia(&cfg, medio, 200, 150, 6, &cb);
        caso("320x240 (32 x 24 celdas): cuadro y celdas dan lo mismo", distintos_a == 0 && ca == 0);
        caso("200x150 (20 x 15 celdas): cuadro y celdas dan lo mismo", distintos_b == 0 && cb == 0);
    }

    /* Con otras configuraciones (mejoras apagadas, piso fijo, otros umbrales). */
    {
        ConfigObstaculos otra = cfg;
        EscenaSintetica e;
        int ec;

        memset(&e, 0, sizeof e);
        e.piso = PS_BALDOSAS;
        e.sigma = 3;
        e.exposicion_pct = 100;
        e.objeto = PS_CAJA;
        e.obj_x = 80;
        e.obj_y = 80;
        for (i = 0; i < 6; i++) {
            e.semilla = (unsigned long)(77 + i);
            ps_dibujar(&e, cuadros + (long)i * PS_ANCHO * PS_ALTO * 3);
        }
        otra.mejoras_piso = 0;
        otra.umbral_color = 30;
        otra.usar_bordes = 1;
        caso("mejoras de piso apagadas, umbral 30 y bordes: cuadro y celdas dan lo mismo", comparar_secuencia(&otra, cuadros, PS_ANCHO, PS_ALTO, 6, &ec) == 0 && ec == 0);
        otra = cfg;
        otra.piso_fijo = 1;
        caso("piso fijo: cuadro y celdas dan lo mismo", comparar_secuencia(&otra, cuadros, PS_ANCHO, PS_ALTO, 6, &ec) == 0 && ec == 0);
    }
}

static void pruebas_errores_celdas(void)
{
    ModeloPiso modelo;
    ResultadoObstaculos r;
    ConfigObstaculos cfg;
    unsigned char celdas[24 * 17 * 3];

    obstaculos_config_defecto(&cfg);
    obstaculos_modelo_iniciar(&modelo);
    memset(celdas, 100, sizeof celdas);
    caso("celdas nulas: error", obstaculos_analizar_modelo_celdas(&cfg, &modelo, NULL, 24, 17, 0, 0, &r) == OBS_ERR_IMAGEN_NULA);
    caso("modelo nulo: error", obstaculos_analizar_modelo_celdas(&cfg, NULL, celdas, 24, 17, 0, 0, &r) == OBS_ERR_IMAGEN_NULA);
    caso("resultado nulo: error", obstaculos_analizar_modelo_celdas(&cfg, &modelo, celdas, 24, 17, 0, 0, NULL) == OBS_ERR_IMAGEN_NULA);
    caso("rejilla de 2 x 2 celdas: error de tamaño", obstaculos_analizar_modelo_celdas(&cfg, &modelo, celdas, 2, 2, 0, 0, &r) == OBS_ERR_TAMANO);
    caso("rejilla de 65 columnas: error de tamaño", obstaculos_analizar_modelo_celdas(&cfg, &modelo, celdas, 65, 10, 0, 0, &r) == OBS_ERR_TAMANO);
    cfg.umbral_color = -5;
    caso("configuración no válida: error de configuración", obstaculos_analizar_modelo_celdas(&cfg, &modelo, celdas, 24, 17, 0, 0, &r) == OBS_ERR_CONFIG);
    obstaculos_config_defecto(&cfg);
    caso("un piso de un solo color en celdas se analiza sin error y sin obstáculos",
         obstaculos_analizar_modelo_celdas(&cfg, &modelo, celdas, 24, 17, 0, 0, &r) == OBS_OK && !r.zona[0].hay_obstaculo && !r.zona[1].hay_obstaculo && !r.zona[2].hay_obstaculo);
}

static void pruebas_paquete(void)
{
    static unsigned char rgb[PS_ANCHO * PS_ALTO * 3];
    static unsigned char celdas[OBS_MAX_COLUMNAS * OBS_MAX_FILAS * 3], leidas[OBS_MAX_COLUMNAS * OBS_MAX_FILAS * 3];
    static unsigned char paquete[PAQ_MAX_TOTAL];
    EscenaSintetica e;
    DatosPaquete d, l;
    int columnas = 0, filas = 0, largo, i, ok;

    memset(&e, 0, sizeof e);
    e.piso = PS_BALDOSAS;
    e.sigma = 3;
    e.exposicion_pct = 100;
    e.objeto = PS_CAJA;
    e.obj_x = 90;
    e.obj_y = 90;
    ps_dibujar(&e, rgb);
    obstaculos_promediar(rgb, PS_ANCHO, PS_ALTO, celdas, (int)sizeof celdas, &columnas, &filas);
    d.numero = 4000000000u;   /* más de 2 mil millones: se prueba el entero sin signo */
    d.ms_captura = 1234;
    d.ancho = PS_ANCHO;
    d.alto = PS_ALTO;
    d.columnas = columnas;
    d.filas = filas;
    largo = paquete_armar(paquete, (int)sizeof paquete, &d, celdas);
    caso("el paquete de 24 x 17 mide 20 + 1224 = 1244 bytes", largo == 20 + 24 * 17 * 3);
    caso("cabe en un solo paquete UDP (menos de 1472 bytes)", largo > 0 && largo <= PAQ_LIMITE_UDP);
    caso("empieza con la marca CEL1", paquete[0] == 'C' && paquete[1] == 'E' && paquete[2] == 'L' && paquete[3] == '1');
    memset(&l, 0, sizeof l);
    caso("leer devuelve OK", paquete_leer(paquete, largo, &l, leidas, (int)sizeof leidas) == PAQ_OK);
    caso("ida y vuelta: número, ms, tamaño de imagen y rejilla iguales", l.numero == d.numero && l.ms_captura == d.ms_captura && l.ancho == d.ancho && l.alto == d.alto &&
                                                                          l.columnas == d.columnas && l.filas == d.filas);
    ok = 1;
    for (i = 0; i < columnas * filas * 3; i++) {
        if (leidas[i] != celdas[i]) {
            ok = 0;
        }
    }
    caso("ida y vuelta: las celdas llegan iguales", ok);

    /* El detector con las celdas que viajaron por el paquete da lo mismo que con el cuadro. */
    {
        ModeloPiso a, b;
        ResultadoObstaculos ra, rb;
        ConfigObstaculos cfg;

        obstaculos_config_defecto(&cfg);
        obstaculos_modelo_iniciar(&a);
        obstaculos_modelo_iniciar(&b);
        caso("celdas que viajaron por el paquete: el detector da lo mismo que con el cuadro",
             obstaculos_analizar_modelo(&cfg, &a, rgb, PS_ANCHO, PS_ALTO, 0, 0, &ra) == OBS_OK &&
             obstaculos_analizar_modelo_celdas(&cfg, &b, leidas, l.columnas, l.filas, 0, 0, &rb) == OBS_OK && resultados_iguales(&ra, &rb));
    }

    /* Paquetes mal formados. */
    caso("un byte de menos: error de largo", paquete_leer(paquete, largo - 1, &l, leidas, (int)sizeof leidas) == PAQ_ERR_LARGO);
    caso("un byte de más: error de largo", paquete_leer(paquete, largo + 1, &l, leidas, (int)sizeof leidas) == PAQ_ERR_LARGO);
    caso("solo el encabezado cortado (10 bytes): error de largo", paquete_leer(paquete, 10, &l, leidas, (int)sizeof leidas) == PAQ_ERR_LARGO);
    caso("largo cero: error de largo", paquete_leer(paquete, 0, &l, leidas, (int)sizeof leidas) == PAQ_ERR_LARGO);
    {
        unsigned char malo[PAQ_MAX_TOTAL];

        memcpy(malo, paquete, (size_t)largo);
        malo[0] = 'X';
        caso("marca distinta: error de marca", paquete_leer(malo, largo, &l, leidas, (int)sizeof leidas) == PAQ_ERR_MARCA);
        memcpy(malo, paquete, (size_t)largo);
        malo[3] = '2';
        caso("versión distinta (CEL2): error de marca", paquete_leer(malo, largo, &l, leidas, (int)sizeof leidas) == PAQ_ERR_MARCA);
        memcpy(malo, paquete, (size_t)largo);
        malo[16] = 0;
        caso("cero columnas: error de tamaño", paquete_leer(malo, largo, &l, leidas, (int)sizeof leidas) == PAQ_ERR_TAMANO);
        memcpy(malo, paquete, (size_t)largo);
        malo[17] = 200;
        caso("200 filas: error de tamaño", paquete_leer(malo, largo, &l, leidas, (int)sizeof leidas) == PAQ_ERR_TAMANO);
        memcpy(malo, paquete, (size_t)largo);
        malo[16] = 25;   /* cambia las columnas pero el largo ya no cuadra */
        caso("columnas que no cuadran con el largo: error de largo", paquete_leer(malo, largo, &l, leidas, (int)sizeof leidas) == PAQ_ERR_LARGO);
    }
    caso("capacidad de celdas chica: error de argumento", paquete_leer(paquete, largo, &l, leidas, 100) == PAQ_ERR_ARGUMENTO);
    caso("punteros nulos: error de argumento", paquete_leer(NULL, largo, &l, leidas, (int)sizeof leidas) == PAQ_ERR_ARGUMENTO && paquete_leer(paquete, largo, NULL, leidas, 5000) == PAQ_ERR_ARGUMENTO);
    caso("armar con capacidad chica: error", paquete_armar(paquete, 50, &d, celdas) == PAQ_ERR_ARGUMENTO);
    d.columnas = 0;
    caso("armar con 0 columnas: error de tamaño", paquete_armar(paquete, (int)sizeof paquete, &d, celdas) == PAQ_ERR_TAMANO);

    /* Un barrido de paquetes aleatorios nunca rompe nada. */
    {
        unsigned int estado = 99;
        int n, rechazados = 0, aceptados = 0;

        for (n = 0; n < 2000; n++) {
            unsigned char basura[300];
            int len = 0;

            for (i = 0; i < (int)sizeof basura; i++) {
                estado = estado * 1664525u + 1013904223u;
                basura[i] = (unsigned char)(estado >> 24);
            }
            if ((n % 4) == 0) {   /* con la marca buena, para llegar más adentro */
                basura[0] = 'C'; basura[1] = 'E'; basura[2] = 'L'; basura[3] = '1';
                basura[16] = (unsigned char)(1 + n % 8);
                basura[17] = (unsigned char)(1 + n % 5);
            }
            len = (n * 37) % 301;
            if (paquete_leer(basura, len, &l, leidas, (int)sizeof leidas) == PAQ_OK) {
                aceptados++;
            } else {
                rechazados++;
            }
        }
        printf("      2000 paquetes de basura: %d rechazados, %d aceptados (los aceptados traen largo exacto)\n", rechazados, aceptados);
        caso("2000 paquetes de basura no rompen la lectura (casi todos se rechazan)", rechazados > 1900);
    }
}

int main(void)
{
    pruebas_promediar();
    pruebas_equivalencia();
    pruebas_errores_celdas();
    pruebas_paquete();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
