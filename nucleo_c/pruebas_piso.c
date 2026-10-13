/*
 * pruebas_piso.c — Pruebas del piso con VARIOS colores, del aprendizaje continuo seguro, del cambio de
 * piso y de la señal de bordes (obstaculos.c), con imágenes sintéticas.
 *
 * GUÍA DE LECTURA
 *  1. Las imágenes se pintan aquí mismo (sin archivos): baldosas de dos tonos (damero), vetas (rayas
 *     finas más oscuras), sombra suave (una rampa de luz), una libreta casi del color del piso con su
 *     borde, una mano, una escalera lejana.
 *  2. Cada escena se analiza con obstaculos_analizar_con (sin memoria) o con obstaculos_analizar_modelo
 *     (con memoria, varios cuadros seguidos) y se compara con lo esperado.
 *  3. Al final de cada grupo se imprimen NÚMEROS de los experimentos (cuántas celdas, qué grupos) para
 *     poder decidir los valores por defecto con datos (ver docs/obstaculos.md).
 *  Convención: la fila 0 es la de arriba; todas las coordenadas se dan en píxeles de la imagen.
 */

#include <stdio.h>
#include <string.h>
#include "obstaculos.h"

static unsigned char imagen[OBS_MAX_COLUMNAS * OBS_LADO_CELDA * OBS_MAX_FILAS * OBS_LADO_CELDA * 3];
static int g_ancho = 320;
static int g_alto = 240;

static int casos = 0;
static int fallos = 0;

static const unsigned char PISO[3]   = { 110, 100, 90 };    /* piso marrón claro */
static const unsigned char PISO_B[3] = { 150, 138, 124 };   /* segundo tono de baldosa, no muy distinto */
static const unsigned char CLARA[3]  = { 200, 190, 180 };   /* baldosa clara */
static const unsigned char OSCURA[3] = { 90, 80, 70 };      /* baldosa oscura del mismo tono */
static const unsigned char CAJA[3]   = { 200, 50, 40 };     /* caja roja */
static const unsigned char MANO[3]   = { 205, 150, 125 };   /* piel */
static const unsigned char NUEVO[3]  = { 30, 60, 200 };     /* un piso muy distinto (alfombra azul) */

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* ── Generación de imágenes ────────────────────────────────────────────── */

static unsigned int semilla = 4242u;

static unsigned int aleatorio(void)
{
    semilla = semilla * 1103515245u + 12345u;
    return (semilla >> 16) & 0x7FFF;
}

static unsigned char limitar(int valor)
{
    if (valor < 0) {
        return 0;
    }
    return (unsigned char)((valor > 255) ? 255 : valor);
}

static void poner(int x, int y, const unsigned char color[3])
{
    if (x >= 0 && x < g_ancho && y >= 0 && y < g_alto) {
        imagen[(y * g_ancho + x) * 3] = color[0];
        imagen[(y * g_ancho + x) * 3 + 1] = color[1];
        imagen[(y * g_ancho + x) * 3 + 2] = color[2];
    }
}

/* Pinta un rectángulo en píxeles [x0, x1) x [y0, y1). */
static void rect_px(int x0, int y0, int x1, int y1, const unsigned char color[3])
{
    int x, y;

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            poner(x, y, color);
        }
    }
}

static void rellenar(const unsigned char color[3])
{
    rect_px(0, 0, g_ancho, g_alto, color);
}

/* Suma ruido de -amplitud a +amplitud a cada canal de cada píxel. */
static void ruido(int amplitud)
{
    int i;

    for (i = 0; i < g_ancho * g_alto * 3; i++) {
        imagen[i] = limitar(imagen[i] + (int)(aleatorio() % (unsigned)(2 * amplitud + 1)) - amplitud);
    }
}

/* Baldosas cuadradas de `lado` píxeles de dos colores, como un tablero de ajedrez. `desfase` mueve el dibujo. */
static void baldosas(int lado, int desfase, const unsigned char a[3], const unsigned char b[3])
{
    int x, y;

    for (y = 0; y < g_alto; y++) {
        for (x = 0; x < g_ancho; x++) {
            int par = (((x + desfase) / lado) + ((y + desfase) / lado)) % 2;

            poner(x, y, par ? b : a);
        }
    }
}

/* Vetas: rayas diagonales finas (`grosor` píxeles) que restan `resta` a cada canal, una cada `separacion` píxeles. */
static void vetas(int grosor, int separacion, int resta)
{
    int x, y, c;

    for (y = 0; y < g_alto; y++) {
        for (x = 0; x < g_ancho; x++) {
            if (((x + y) % separacion) < grosor) {
                for (c = 0; c < 3; c++) {
                    imagen[(y * g_ancho + x) * 3 + c] = limitar(imagen[(y * g_ancho + x) * 3 + c] - resta);
                }
            }
        }
    }
}

/* Sombra suave: dentro de [x0, x1) x [y0, y1) la luz baja linealmente de 0 a `resta` de izquierda a derecha. */
static void sombra_suave(int x0, int y0, int x1, int y1, int resta)
{
    int x, y, c;

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            int baja = resta * (x - x0) / (x1 - x0);

            for (c = 0; c < 3; c++) {
                imagen[(y * g_ancho + x) * 3 + c] = limitar(imagen[(y * g_ancho + x) * 3 + c] - baja);
            }
        }
    }
}

/* Suma el mismo valor a todos los canales de toda la imagen (cambio de luz). */
static void sumar_luz(int cantidad)
{
    int i;

    for (i = 0; i < g_ancho * g_alto * 3; i++) {
        imagen[i] = limitar(imagen[i] + cantidad);
    }
}

/* Una libreta: papel casi del color del piso (+dr, +dg, +db) con un borde oscuro de `grosor` píxeles. */
static void libreta(int x0, int y0, int x1, int y1, int dr, int dg, int db, int grosor, int borde_resta)
{
    unsigned char papel[3], borde[3];
    int c;

    papel[0] = limitar(PISO[0] + dr);
    papel[1] = limitar(PISO[1] + dg);
    papel[2] = limitar(PISO[2] + db);
    for (c = 0; c < 3; c++) {
        borde[c] = limitar(papel[c] - borde_resta);
    }
    rect_px(x0, y0, x1, y1, borde);
    rect_px(x0 + grosor, y0 + grosor, x1 - grosor, y1 - grosor, papel);
}

/* ── Análisis y resumen ────────────────────────────────────────────────── */

static ConfigObstaculos cfg;
static ResultadoObstaculos r;

/* Analiza la imagen sin memoria con `cfg`. */
static int analizar(void)
{
    return obstaculos_analizar_con(&cfg, imagen, g_ancho, g_alto, &r);
}

/* ¿Las tres zonas coinciden con las cercanías esperadas (0 = libre)? */
static int zonas(int izq, int centro, int der)
{
    return r.zona[ZONA_IZQUIERDA].cercania == izq && r.zona[ZONA_CENTRO].cercania == centro &&
           r.zona[ZONA_DERECHA].cercania == der;
}

static void informar(const char *nombre, int codigo)
{
    printf("      [%s] codigo=%d grupos=%d celdas izq=%d centro=%d der=%d cercanias=%d,%d,%d bordes=%d\n", nombre, codigo, r.grupos,
           r.zona[0].celdas, r.zona[1].celdas, r.zona[2].celdas,
           r.zona[0].cercania, r.zona[1].cercania, r.zona[2].cercania, r.celdas_borde);
}

static int libre(void)
{
    return zonas(0, 0, 0);
}

/* ── Baldosas, vetas y sombras ─────────────────────────────────────────── */

static void pruebas_baldosas(void)
{
    int codigo;

    obstaculos_config_defecto(&cfg);

    baldosas(40, 0, PISO, PISO_B);
    ruido(6);
    codigo = analizar();
    informar("baldosas 40 px, tonos parecidos", codigo);
    caso("baldosas de 2 tonos parecidos (damero de 40 px): sin obstaculos", codigo == OBS_OK && libre());

    baldosas(40, 0, CLARA, OSCURA);
    ruido(6);
    codigo = analizar();
    informar("baldosas 40 px, claro/oscuro mismo tono", codigo);
    caso("baldosas muy distintas pero del mismo tono (clara y oscura): sin obstaculos ni error", codigo == OBS_OK && libre());
    caso("baldosas claro/oscuro: se aprenden 2 grupos o mas", r.grupos >= 2);

    cfg.grupos_piso = 1;
    codigo = analizar();
    informar("baldosas claro/oscuro con K=1", codigo);
    caso("con K=1 las baldosas claro/oscuro NO se pueden representar (error de franja o falsas alarmas)",
         codigo == OBS_ERR_REFERENCIA || !libre());
    obstaculos_config_defecto(&cfg);

    baldosas(25, 7, PISO, PISO_B);
    ruido(6);
    codigo = analizar();
    informar("baldosas 25 px desfasadas", codigo);
    caso("baldosas de 25 px (no alineadas con las celdas, bordes mezclados): sin obstaculos", codigo == OBS_OK && libre());

    baldosas(40, 0, PISO, PISO_B);
    ruido(6);
    rect_px(120, 100, 200, 210, CAJA);   /* una caja roja de 80x110 px al centro (llega hasta la fila 20) */
    codigo = analizar();
    informar("baldosas + caja", codigo);
    caso("baldosas con una caja roja al centro: solo CENTRO, muy cerca", codigo == OBS_OK && zonas(0, 3, 0));

    baldosas(40, 0, PISO, PISO_B);
    ruido(6);
    rect_px(120, 100, 200, 210, MANO);
    codigo = analizar();
    informar("baldosas + mano", codigo);
    caso("baldosas con una mano (color piel) al centro: se detecta", codigo == OBS_OK && zonas(0, 3, 0));
}

static void pruebas_vetas_y_sombras(void)
{
    int codigo;

    obstaculos_config_defecto(&cfg);

    rellenar(PISO);
    ruido(6);
    vetas(3, 37, 22);
    codigo = analizar();
    informar("vetas finas", codigo);
    caso("piso con vetas finas (rayas oscuras de 3 px): sin obstaculos", codigo == OBS_OK && libre());

    rellenar(PISO);
    ruido(6);
    vetas(6, 41, 30);
    codigo = analizar();
    informar("vetas gruesas", codigo);
    caso("piso con vetas gruesas (6 px, mas oscuras): sin obstaculos", codigo == OBS_OK && libre());

    rellenar(PISO);
    ruido(6);
    sombra_suave(40, 130, 280, 240, 40);
    codigo = analizar();
    informar("sombra suave", codigo);
    caso("sombra suave (rampa de luz de 0 a -40): NO se detecta", codigo == OBS_OK && libre());

    rellenar(PISO);
    ruido(6);
    sombra_suave(0, 120, 320, 240, 35);
    codigo = analizar();
    informar("sombra suave ancha", codigo);
    caso("sombra suave que cruza toda la imagen (0 a -35): NO se detecta", codigo == OBS_OK && libre());
}

/* ── Escalera lejana ───────────────────────────────────────────────────── */

static void pruebas_escalera(void)
{
    static const unsigned char ESCALON_A[3] = { 150, 150, 160 };
    static const unsigned char ESCALON_B[3] = { 70, 70, 80 };
    int codigo, i;

    obstaculos_config_defecto(&cfg);

    /* Escalera al fondo: rayas horizontales en la parte de arriba de la imagen (arriba de la fila minima). */
    rellenar(PISO);
    ruido(6);
    for (i = 0; i < 6; i++) {
        rect_px(0, 20 + i * 16, 320, 20 + i * 16 + 8, (i % 2) ? ESCALON_A : ESCALON_B);
    }
    codigo = analizar();
    informar("escalera al fondo", codigo);
    caso("escalera al fondo (arriba de la fila minima): sin obstaculos", codigo == OBS_OK && libre());

    /* Escalera "lejana" que ya entra en la zona de avance pero sigue lejos: su borde de abajo queda en las
     * primeras filas de la zona (cercania 1): se informa, pero no es cercania 3 (el navegador solo frena desde 2). */
    rellenar(PISO);
    ruido(6);
    for (i = 0; i < 4; i++) {
        rect_px(40, 125 + i * 10, 280, 125 + i * 10 + 5, (i % 2) ? ESCALON_A : ESCALON_B);
    }
    rect_px(40, 125, 280, 165, ESCALON_B);
    codigo = analizar();
    informar("escalera lejana en la zona de avance", codigo);
    caso("escalera lejana dentro de la zona de avance: se ve, pero con cercania 1 (no dispara frenar)",
         codigo == OBS_OK && r.zona[ZONA_CENTRO].cercania == 1 && r.zona[ZONA_IZQUIERDA].cercania <= 1 && r.zona[ZONA_DERECHA].cercania <= 1);
}

/* ── Libreta y bordes ──────────────────────────────────────────────────── */

static void pruebas_libreta(void)
{
    int codigo, sin_bordes_celdas, con_bordes_celdas;

    obstaculos_config_defecto(&cfg);

    /* Libreta de 70x60 px casi del color del piso (+22,+14,-6) con un borde oscuro de 3 px. */
    rellenar(PISO);
    ruido(4);
    libreta(125, 140, 195, 200, 22, 14, -6, 3, 55);
    cfg.usar_bordes = 0;
    codigo = analizar();
    informar("libreta, bordes apagados", codigo);
    sin_bordes_celdas = r.zona[ZONA_CENTRO].celdas;

    cfg.usar_bordes = 1;
    codigo = analizar();
    informar("libreta, bordes encendidos", codigo);
    con_bordes_celdas = r.zona[ZONA_CENTRO].celdas;
    printf("      libreta: celdas del centro sin bordes=%d, con bordes=%d\n", sin_bordes_celdas, con_bordes_celdas);
    caso("libreta del color del piso: con la senal de bordes se ve MAS que sin ella (mejora)", con_bordes_celdas > sin_bordes_celdas);
    obstaculos_config_defecto(&cfg);
}

/* Cuenta falsos positivos de una escena con bordes apagados y encendidos. */
static void falsos_positivos(const char *nombre, int *sin, int *con)
{
    cfg.usar_bordes = 0;
    analizar();
    *sin = r.zona[0].celdas + r.zona[1].celdas + r.zona[2].celdas;
    cfg.usar_bordes = 1;
    analizar();
    *con = r.zona[0].celdas + r.zona[1].celdas + r.zona[2].celdas;
    printf("      falsos positivos [%s]: celdas sin bordes=%d, con bordes=%d (zonas con obstaculo: %d,%d,%d)\n", nombre, *sin, *con,
           r.zona[0].hay_obstaculo, r.zona[1].hay_obstaculo, r.zona[2].hay_obstaculo);
    obstaculos_config_defecto(&cfg);
}

static void pruebas_bordes_falsos_positivos(void)
{
    int sin, con, total_sin = 0, total_con = 0, escenas_peores = 0;

    obstaculos_config_defecto(&cfg);
    caso("la senal de bordes viene APAGADA por defecto", cfg.usar_bordes == 0);

    baldosas(40, 0, PISO, PISO_B);
    ruido(6);
    falsos_positivos("baldosas", &sin, &con);
    total_sin += sin; total_con += con; escenas_peores += (con > sin);

    rellenar(PISO);
    ruido(6);
    vetas(6, 41, 30);
    falsos_positivos("vetas gruesas", &sin, &con);
    total_sin += sin; total_con += con; escenas_peores += (con > sin);

    rellenar(PISO);
    ruido(6);
    sombra_suave(40, 130, 280, 240, 40);
    falsos_positivos("sombra suave", &sin, &con);
    total_sin += sin; total_con += con; escenas_peores += (con > sin);
    caso("bordes encendidos: la sombra suave (rampa de 40) produce falsos positivos", con > sin);

    rellenar(PISO);
    ruido(6);
    falsos_positivos("piso liso", &sin, &con);
    total_sin += sin; total_con += con; escenas_peores += (con > sin);

    baldosas(25, 7, PISO, PISO_B);
    ruido(6);
    falsos_positivos("baldosas de 25 px", &sin, &con);
    total_sin += sin; total_con += con; escenas_peores += (con > sin);
    caso("bordes encendidos: las baldosas de 25 px (bordes mezclados) producen muchos falsos positivos", con > sin + 20);

    printf("      bordes: en 5 escenas SIN objetos, celdas falsas: %d sin bordes, %d con bordes; %d escenas empeoran\n",
           total_sin, total_con, escenas_peores);
    caso("DECISION: la senal de bordes mejora la libreta pero empeora escenas sin objetos, asi que se deja APAGADA por defecto",
         escenas_peores > 0);
}

/* ── Memoria del piso entre cuadros ────────────────────────────────────── */

/* Pinta un cuadro nuevo: piso con ruido y una luz extra. */
static void cuadro_de_piso(int luz)
{
    rellenar(PISO);
    ruido(4);
    sumar_luz(luz);
}

/* Distancia (suma de canales) entre dos colores. */
static int distancia(const int a[3], const unsigned char b[3])
{
    int c, total = 0;

    for (c = 0; c < 3; c++) {
        int d = a[c] - b[c];

        total += (d < 0) ? -d : d;
    }
    return total;
}

static void pruebas_aprendizaje(void)
{
    ModeloPiso m;
    unsigned char piso_final[3];
    int i, codigo = OBS_OK;
    int grupos_con, grupos_sin;
    int color_con[3] = { 0, 0, 0 };

    /* El piso se aclara poco a poco (+40 por canal en 300 cuadros: unos 30 s a 10 cuadros por segundo). */
    obstaculos_config_defecto(&cfg);
    obstaculos_modelo_iniciar(&m);
    for (i = 0; i < 300; i++) {
        cuadro_de_piso(40 * i / 299);
        codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 1, (unsigned int)i * 100u, &r);
        if (codigo != OBS_OK || !libre()) {
            break;
        }
    }
    piso_final[0] = limitar(PISO[0] + 40);
    piso_final[1] = limitar(PISO[1] + 40);
    piso_final[2] = limitar(PISO[2] + 40);
    informar("aprendizaje con luz que sube, avanzando", codigo);
    caso("el piso se aclara 40 por canal mientras avanza: nunca da obstaculos", i == 300 && codigo == OBS_OK && libre());
    grupos_con = r.grupos;
    color_con[0] = r.ref_r; color_con[1] = r.ref_g; color_con[2] = r.ref_b;
    printf("      con aprendizaje el color del modelo quedo a %d del piso real (sin seguirlo estaria a %d)\n", distancia(color_con, piso_final), distancia(color_con, PISO));
    caso("con aprendizaje el modelo SIGUE al piso con un pequeno retraso (queda a menos de 30 del color real)", distancia(color_con, piso_final) < 30);
    caso("con aprendizaje el aprendio se marca en los cuadros con piso libre", r.aprendio == 1);

    /* Lo mismo pero sin aprender: el modelo se queda atras y se llena de grupos. */
    cfg.aprendizaje_mil = 0;
    obstaculos_modelo_iniciar(&m);
    for (i = 0; i < 300; i++) {
        cuadro_de_piso(40 * i / 299);
        codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 1, (unsigned int)i * 100u, &r);
    }
    grupos_sin = r.grupos;
    printf("      sin aprendizaje: %d grupos; con aprendizaje: %d grupo(s)\n", grupos_sin, grupos_con);
    caso("sin aprendizaje (velocidad 0) el modelo no aprende nunca (aprendio = 0)", r.aprendio == 0);
    caso("sin aprendizaje el modelo necesita mas grupos para seguir el cambio de luz que con aprendizaje", grupos_sin > grupos_con);
    obstaculos_config_defecto(&cfg);

    /* Con el carro parado no se aprende. */
    obstaculos_modelo_iniciar(&m);
    cuadro_de_piso(0);
    obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, 0, &r);
    cuadro_de_piso(0);
    obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, 100, &r);
    caso("con el carro parado NO aprende", r.aprendio == 0);
    cuadro_de_piso(0);
    obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 1, 200, &r);
    caso("con el carro avanzando y sin obstaculos SI aprende", r.aprendio == 1);

    /* Con un obstaculo en la zona no se aprende NADA (ni del piso). */
    cuadro_de_piso(0);
    rect_px(110, 130, 210, 205, CAJA);
    codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 1, 300, &r);
    caso("con un obstaculo en la zona de avance NO aprende (aunque avance)", codigo == OBS_OK && r.zona[ZONA_CENTRO].hay_obstaculo && r.aprendio == 0);
}

/* El modelo nunca aprende celdas marcadas como obstaculo ni dudosas, aunque aprenda muy rapido. */
static void pruebas_aprendizaje_seguro(void)
{
    ModeloPiso m;
    int i, codigo = OBS_OK, peor = 0;
    obstaculos_config_defecto(&cfg);

    /* Un objeto rojo de 8x6 celdas (48 celdas, 12 % de la zona de avance) que NO se reporta como obstaculo porque se pide
     * un minimo de 100 celdas: 100 cuadros con el carro avanzando y aprendizaje muy rapido. Si el modelo aprendiera las
     * celdas del objeto, se movería unos 24 hacia el rojo. */
    cfg.min_celdas = 100;
    cfg.aprendizaje_mil = 500;
    obstaculos_modelo_iniciar(&m);
    for (i = 0; i < 100; i++) {
        int g, d;

        cuadro_de_piso(0);
        rect_px(120, 130, 200, 190, CAJA);
        codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 1, (unsigned int)i * 100u, &r);
        for (g = 0; g < r.grupos; g++) {
            d = distancia(r.grupo_color[g], PISO);
            if (d > peor) {
                peor = d;
            }
        }
    }
    informar("objeto rojo pequeno, aprendizaje 500", codigo);
    printf("      el grupo del modelo mas lejano del piso real quedo a %d\n", peor);
    caso("un objeto rojo (48 celdas, no se reporta) NO se aprende como piso: ningun grupo se aleja del piso (menos de 10)",
         codigo == OBS_OK && libre() && peor < 10 && r.grupos == 1);

    /* Un objeto "dudoso": color a 40 del piso (dentro del umbral, pero mas lejos de la mitad del umbral). */
    obstaculos_modelo_iniciar(&m);
    peor = 0;
    for (i = 0; i < 100; i++) {
        unsigned char dudoso[3];
        int g, d;

        dudoso[0] = limitar(PISO[0] + 22);
        dudoso[1] = limitar(PISO[1] + 12);
        dudoso[2] = limitar(PISO[2] - 6);   /* distancia 40 del piso */
        cuadro_de_piso(0);
        rect_px(20, 125, 300, 210, dudoso);
        codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 1, (unsigned int)i * 100u, &r);
        for (g = 0; g < r.grupos; g++) {
            d = distancia(r.grupo_color[g], PISO);
            if (d > peor) {
                peor = d;
            }
        }
    }
    informar("objeto dudoso (distancia 40), aprendizaje 500", codigo);
    printf("      el grupo del modelo mas lejano del piso real quedo a %d\n", peor);
    caso("una zona grande (65 % de la zona de avance) casi del color del piso (distancia 40, mas que la mitad del umbral) NO se aprende: el modelo casi no se mueve (menos de 10)",
         codigo == OBS_OK && peor < 10);
    obstaculos_config_defecto(&cfg);
}

static void pruebas_cambio_de_piso(void)
{
    ModeloPiso m;
    int i, t, relearn_en = -1, antes_de_tiempo = 0, tras_relearn_libre = 1, codigo;

    obstaculos_config_defecto(&cfg);
    obstaculos_modelo_iniciar(&m);
    for (i = 0; i < 5; i++) {
        cuadro_de_piso(0);
        obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, (unsigned int)i * 100u, &r);
    }
    caso("antes del cambio: el piso se aprendio y no hay obstaculos", r.grupos == 1 && libre() && r.piso_reaprendido == 0);

    /* La alfombra azul aparece en el cuadro de 500 ms (el carro esta parado o avanzando, da igual). */
    for (i = 5; i < 40; i++) {
        t = i * 100;
        rellenar(NUEVO);
        ruido(4);
        codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, (unsigned int)t, &r);
        if (i == 5) {
            informar("primer cuadro del piso nuevo", codigo);
            caso("el primer cuadro con el piso nuevo se ve como obstaculo en las tres zonas (el navegador se detiene)",
                 codigo == OBS_OK && zonas(3, 3, 3) && r.piso_reaprendido == 0);
        }
        if (r.piso_reaprendido && relearn_en < 0) {
            relearn_en = t;
        }
        if (relearn_en >= 0 && t > relearn_en && !libre()) {
            tras_relearn_libre = 0;
        }
        if (relearn_en < 0 && t < 500 + OBS_MS_PISO_NUEVO && r.piso_reaprendido) {
            antes_de_tiempo = 1;
        }
    }
    printf("      el piso nuevo empezo a los 500 ms y se reaprendio a los %d ms\n", relearn_en);
    caso("el piso se reaprende cuando todo estuvo lejos al menos OBS_MS_PISO_NUEVO (1000 ms): a los 1500 ms", relearn_en == 1500 && !antes_de_tiempo);
    caso("despues de reaprender ya no hay obstaculos (piso nuevo = piso)", tras_relearn_libre && libre());
    caso("el modelo cuenta 1 reaprendizaje", m.reaprendidos == 1);
    caso("el color de referencia ahora es el de la alfombra azul", distancia((int[3]){ r.ref_r, r.ref_g, r.ref_b }, NUEVO) < 12);

    /* Solo una parte del piso cambia (mitad izquierda): nunca se reaprende (hay celdas de piso). */
    obstaculos_modelo_iniciar(&m);
    cuadro_de_piso(0);
    obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, 0, &r);
    relearn_en = -1;
    for (i = 1; i < 60; i++) {
        cuadro_de_piso(0);
        rect_px(0, 0, 150, 240, NUEVO);
        obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, (unsigned int)i * 100u, &r);
        if (r.piso_reaprendido) {
            relearn_en = i * 100;
        }
    }
    caso("si solo cambia parte del piso (mitad izquierda) NO se reaprende nunca (6 s)", relearn_en < 0 && m.reaprendidos == 0);
    caso("... y la parte que cambio se sigue viendo como obstaculo (izquierda)", r.zona[ZONA_IZQUIERDA].hay_obstaculo == 1);

    /* Un objeto interrumpe: si todo vuelve a ser piso antes de 1 s, el contador se reinicia. */
    obstaculos_modelo_iniciar(&m);
    cuadro_de_piso(0);
    obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, 0, &r);
    relearn_en = -1;
    for (i = 1; i < 30; i++) {
        if (i % 8 == 7) {
            cuadro_de_piso(0);    /* cada 800 ms un cuadro de piso: el tiempo "todo lejos" nunca llega a 1 s */
        } else {
            rellenar(NUEVO);
            ruido(4);
        }
        obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, (unsigned int)i * 100u, &r);
        if (r.piso_reaprendido) {
            relearn_en = i * 100;
        }
    }
    caso("si el piso vuelve a verse antes de 1 s el contador se reinicia y no se reaprende", relearn_en < 0);

    /* RIESGO CONOCIDO: una pared que llena TODA la imagen (franja incluida) tambien se reaprende como piso. */
    obstaculos_modelo_iniciar(&m);
    cuadro_de_piso(0);
    obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 1, 0, &r);
    relearn_en = -1;
    for (i = 1; i < 30; i++) {
        rellenar(CAJA);     /* una pared roja que lo tapa todo */
        ruido(4);
        obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, (unsigned int)i * 100u, &r);
        if (r.piso_reaprendido && relearn_en < 0) {
            relearn_en = i * 100;
        }
    }
    printf("      pared que lo llena todo: se 'reaprendio' a los %d ms (riesgo conocido: durante ese segundo el navegador ya se detuvo)\n", relearn_en);
    caso("una pared que llena toda la imagen se reaprende como piso a los 1000 ms (riesgo conocido y documentado)", relearn_en == 1100);
}

static void pruebas_modelo_y_errores(void)
{
    ModeloPiso m, copia;
    int codigo;

    obstaculos_config_defecto(&cfg);

    /* Franja tapada con un modelo ya aprendido: se sigue con lo aprendido y el obstaculo se detecta. */
    obstaculos_modelo_iniciar(&m);
    cuadro_de_piso(0);
    codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, 0, &r);
    caso("primer cuadro: el modelo aprende el piso", codigo == OBS_OK && m.valido == 1 && r.franja_contaminada == 0);
    cuadro_de_piso(0);
    rect_px(130, 190, 190, 240, CAJA);    /* algo pegado al carro tapa TODA la franja de referencia (queda uniforme, de otro color) */
    codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, 100, &r);
    informar("franja tapada con modelo", codigo);
    caso("con un modelo ya aprendido, la franja tapada NO da error: se usa lo aprendido y se avisa", codigo == OBS_OK && r.franja_contaminada == 1);
    cuadro_de_piso(0);
    rect_px(130, 190, 160, 240, CAJA);    /* tapa solo la mitad de la franja: no es uniforme */
    codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, 150, &r);
    caso("con un modelo ya aprendido, la franja tapada a medias tampoco da error (se usa lo aprendido)", codigo == OBS_OK && r.franja_contaminada == 1 && r.zona[ZONA_CENTRO].hay_obstaculo);
    cuadro_de_piso(0);
    rect_px(130, 190, 190, 240, CAJA);
    caso("... y el objeto pegado al carro se ve como obstaculo muy cerca", r.zona[ZONA_CENTRO].hay_obstaculo == 1 && r.zona[ZONA_CENTRO].cercania == 3);
    cuadro_de_piso(0);
    rect_px(130, 190, 160, 240, CAJA);    /* media franja tapada */
    codigo = obstaculos_analizar_con(&cfg, imagen, g_ancho, g_alto, &r);
    caso("sin modelo (analizar_con) la franja tapada a medias da OBS_ERR_REFERENCIA, como siempre", codigo == OBS_ERR_REFERENCIA);

    obstaculos_modelo_iniciar(&m);
    codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, 0, &r);
    caso("con modelo vacio y la franja tapada a medias: OBS_ERR_REFERENCIA", codigo == OBS_ERR_REFERENCIA && m.valido == 0);

    /* Con un error el modelo no cambia. */
    cuadro_de_piso(0);
    obstaculos_modelo_iniciar(&m);
    obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 0, 0, &r);
    copia = m;
    cfg.grupos_piso = 9;
    codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, g_ancho, g_alto, 1, 100, &r);
    caso("con configuracion no valida: OBS_ERR_CONFIG y el modelo queda igual", codigo == OBS_ERR_CONFIG && memcmp(&copia, &m, sizeof m) == 0);
    obstaculos_config_defecto(&cfg);
    codigo = obstaculos_analizar_modelo(&cfg, &m, imagen, 20, 20, 1, 100, &r);
    caso("con un tamano no valido: OBS_ERR_TAMANO y el modelo queda igual", codigo == OBS_ERR_TAMANO && memcmp(&copia, &m, sizeof m) == 0);
    caso("modelo NULL: OBS_ERR_IMAGEN_NULA", obstaculos_analizar_modelo(&cfg, NULL, imagen, g_ancho, g_alto, 0, 0, &r) == OBS_ERR_IMAGEN_NULA);

    /* Validacion de los campos nuevos de la configuracion. */
    obstaculos_modelo_iniciar(&m);
    cuadro_de_piso(0);
    cfg.grupos_piso = 0;
    caso("grupos_piso 0: OBS_ERR_CONFIG", analizar() == OBS_ERR_CONFIG);
    obstaculos_config_defecto(&cfg);
    cfg.grupos_piso = OBS_MAX_GRUPOS + 1;
    caso("grupos_piso 5: OBS_ERR_CONFIG", analizar() == OBS_ERR_CONFIG);
    obstaculos_config_defecto(&cfg);
    cfg.aprendizaje_mil = -1;
    caso("aprendizaje -1: OBS_ERR_CONFIG", analizar() == OBS_ERR_CONFIG);
    obstaculos_config_defecto(&cfg);
    cfg.aprendizaje_mil = 1001;
    caso("aprendizaje 1001: OBS_ERR_CONFIG", analizar() == OBS_ERR_CONFIG);
    obstaculos_config_defecto(&cfg);
    cfg.usar_bordes = 2;
    caso("usar_bordes 2: OBS_ERR_CONFIG", analizar() == OBS_ERR_CONFIG);
    obstaculos_config_defecto(&cfg);
    caso("config por defecto: K = 4, aprendizaje 20 (2 % por cuadro), bordes apagados",
         cfg.grupos_piso == OBS_MAX_GRUPOS && cfg.aprendizaje_mil == 20 && cfg.usar_bordes == 0);

    /* K limita los grupos. */
    baldosas(40, 0, CLARA, OSCURA);
    ruido(6);
    cfg.grupos_piso = 2;
    codigo = analizar();
    caso("K = 2 con baldosas claro/oscuro: funciona con 2 grupos", codigo == OBS_OK && r.grupos == 2 && libre());
    obstaculos_config_defecto(&cfg);

    /* Franja contaminada: una celda roja en la franja da error sin modelo con cualquier K. */
    cuadro_de_piso(0);
    rect_px(160, 230, 170, 240, CAJA);
    caso("una celda roja dentro de la franja: OBS_ERR_REFERENCIA (la caja no es un tono del piso)", analizar() == OBS_ERR_REFERENCIA);
    obstaculos_config_defecto(&cfg);
}

/* El mismo piso de baldosas a 240x176 (el modo de la camara real). */
static void pruebas_baldosas_hqvga(void)
{
    int codigo;

    g_ancho = 240;
    g_alto = 176;
    obstaculos_config_defecto(&cfg);
    baldosas(40, 0, CLARA, OSCURA);
    ruido(6);
    codigo = analizar();
    informar("baldosas 240x176", codigo);
    caso("[240x176] baldosas claro/oscuro: sin obstaculos", codigo == OBS_OK && libre());
    baldosas(40, 0, PISO, PISO_B);
    ruido(6);
    rect_px(90, 70, 150, 150, CAJA);
    codigo = analizar();
    informar("baldosas + caja 240x176", codigo);
    caso("[240x176] baldosas con una caja al centro: solo CENTRO con obstaculo", codigo == OBS_OK && r.zona[ZONA_CENTRO].hay_obstaculo &&
         !r.zona[ZONA_IZQUIERDA].hay_obstaculo && !r.zona[ZONA_DERECHA].hay_obstaculo);
    g_ancho = 320;
    g_alto = 240;
}

int main(void)
{
    pruebas_baldosas();
    pruebas_vetas_y_sombras();
    pruebas_escalera();
    pruebas_libreta();
    pruebas_bordes_falsos_positivos();
    pruebas_aprendizaje();
    pruebas_aprendizaje_seguro();
    pruebas_cambio_de_piso();
    pruebas_modelo_y_errores();
    pruebas_baldosas_hqvga();
    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
