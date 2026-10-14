/*
 * pruebas_obstaculos.c — Pruebas de obstaculos.c en PC con imágenes sintéticas.
 *
 * GUÍA DE LECTURA
 *  1. Las imágenes se generan aquí mismo (no hay archivos): piso de un color, con ruido,
 *     con una caja de otro color en ciertas celdas, etc.
 *  2. Funciones de ayuda: rellenar() pinta todo, ruido() agrega ruido, rectangulo_celdas()
 *     pinta una caja usando coordenadas de celdas (columna, fila).
 *  3. Cada caso llama a obstaculos_analizar() y compara el resultado con lo esperado:
 *     verificar() revisa las tres zonas (hay obstáculo y cercanía) de una vez.
 *  Convención: la fila 0 es la de arriba; la franja del piso de referencia está en las
 *  filas 22 y 23, columnas 13 a 18.
 */

#include <stdio.h>
#include <string.h>
#include "obstaculos.h"

static int g_ancho = OBS_ANCHO;   /* tamaño de la imagen que se está generando (cambia en las pruebas por tamaño) */
static int g_alto = OBS_ALTO;
static unsigned char imagen[OBS_MAX_COLUMNAS * OBS_LADO_CELDA * OBS_MAX_FILAS * OBS_LADO_CELDA * 3];

static int casos  = 0;
static int fallos = 0;

/* Colores de prueba. */
static const unsigned char PISO[3]  = { 110, 100, 90 };    /* piso marrón claro */
static const unsigned char CAJA[3]  = { 200, 50, 40 };     /* caja roja */
static const unsigned char PARED[3] = { 180, 190, 200 };   /* pared azulada */

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* Convierte "fila N de un total de T" en milésimas SIN perder la fila al volver a convertir
 * (redondea hacia arriba): así un valor de las pruebas viejas (en filas) se expresa como proporción. */
static int mil_de(int celdas, int total)
{
    return (celdas * 1000 + total - 1) / total;
}

/* ── Generación de imágenes ────────────────────────────────────────────── */

static unsigned int semilla = 12345u;

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

/* Pinta toda la imagen de un color. */
static void rellenar(const unsigned char color[3])
{
    int i;

    for (i = 0; i < g_alto * g_ancho; i++) {
        imagen[i * 3]     = color[0];
        imagen[i * 3 + 1] = color[1];
        imagen[i * 3 + 2] = color[2];
    }
}

/* Suma ruido de -amplitud a +amplitud a cada canal de cada píxel. */
static void ruido(int amplitud)
{
    int i;

    for (i = 0; i < g_alto * g_ancho * 3; i++) {
        imagen[i] = limitar(imagen[i] + (int)(aleatorio() % (unsigned)(2 * amplitud + 1)) - amplitud);
    }
}

/* Suma el mismo valor a todos los canales (cambio de luz global). */
static void sumar_luz(int cantidad)
{
    int i;

    for (i = 0; i < g_alto * g_ancho * 3; i++) {
        imagen[i] = limitar(imagen[i] + cantidad);
    }
}

/* Degradado de luz: arriba suma "maximo" y abajo suma 0 (el piso lejano más claro). */
static void degradado_luz(int maximo)
{
    int x, y, c;

    for (y = 0; y < g_alto; y++) {
        int extra = (g_alto - 1 - y) * maximo / (g_alto - 1);

        for (x = 0; x < g_ancho; x++) {
            for (c = 0; c < 3; c++) {
                imagen[(y * g_ancho + x) * 3 + c] = limitar(imagen[(y * g_ancho + x) * 3 + c] + extra);
            }
        }
    }
}

/* Pinta un rectángulo de celdas [columna0..columna1] x [fila0..fila1] (ambos inclusive). */
static void rectangulo_celdas(int columna0, int fila0, int columna1, int fila1, const unsigned char color[3])
{
    int x, y;

    for (y = fila0 * OBS_LADO_CELDA; y < (fila1 + 1) * OBS_LADO_CELDA; y++) {
        for (x = columna0 * OBS_LADO_CELDA; x < (columna1 + 1) * OBS_LADO_CELDA; x++) {
            imagen[(y * g_ancho + x) * 3]     = color[0];
            imagen[(y * g_ancho + x) * 3 + 1] = color[1];
            imagen[(y * g_ancho + x) * 3 + 2] = color[2];
        }
    }
}

/* ── Verificación ──────────────────────────────────────────────────────── */

/* Analiza la imagen actual y compara las tres zonas: cercanía esperada de cada una
 * (0 = sin obstáculo, 1 a 3 = con obstáculo). Devuelve 1 si todo coincide. */
static int verificar(int cercania_izq, int cercania_centro, int cercania_der)
{
    ResultadoObstaculos r;
    int esperado[3];
    int codigo, z, ok;

    esperado[ZONA_IZQUIERDA] = cercania_izq;
    esperado[ZONA_CENTRO]    = cercania_centro;
    esperado[ZONA_DERECHA]   = cercania_der;

    codigo = obstaculos_analizar(imagen, g_ancho, g_alto, &r);
    ok = (codigo == OBS_OK);
    for (z = 0; z < OBS_ZONAS; z++) {
        ok = ok && r.zona[z].cercania == esperado[z] && r.zona[z].hay_obstaculo == (esperado[z] > 0);
    }
    if (!ok) {
        printf("      codigo=%d  izq(hay=%d,cerc=%d,celdas=%d)  centro(%d,%d,%d)  der(%d,%d,%d)  esperado cercanias=%d,%d,%d\n",
               codigo,
               r.zona[0].hay_obstaculo, r.zona[0].cercania, r.zona[0].celdas,
               r.zona[1].hay_obstaculo, r.zona[1].cercania, r.zona[1].celdas,
               r.zona[2].hay_obstaculo, r.zona[2].cercania, r.zona[2].celdas,
               cercania_izq, cercania_centro, cercania_der);
    }
    return ok;
}

/* Igual que verificar(), pero con una configuración distinta de la de por defecto. */
static int verificar_con(const ConfigObstaculos *config, int cercania_izq, int cercania_centro, int cercania_der)
{
    ResultadoObstaculos r;
    int esperado[3];
    int codigo, z, ok;

    esperado[ZONA_IZQUIERDA] = cercania_izq;
    esperado[ZONA_CENTRO]    = cercania_centro;
    esperado[ZONA_DERECHA]   = cercania_der;

    codigo = obstaculos_analizar_con(config, imagen, g_ancho, g_alto, &r);
    ok = (codigo == OBS_OK);
    for (z = 0; z < OBS_ZONAS; z++) {
        ok = ok && r.zona[z].cercania == esperado[z] && r.zona[z].hay_obstaculo == (esperado[z] > 0);
    }
    if (!ok) {
        printf("      codigo=%d  celdas izq=%d centro=%d der=%d  esperado cercanias=%d,%d,%d\n", codigo,
               r.zona[0].celdas, r.zona[1].celdas, r.zona[2].celdas, cercania_izq, cercania_centro, cercania_der);
    }
    return ok;
}

/* Código de retorno de analizar la imagen actual. */
static int codigo_de_analisis(void)
{
    ResultadoObstaculos r;

    return obstaculos_analizar(imagen, g_ancho, g_alto, &r);
}

/* Igual, con el detector de ANTES de las mejoras de piso (mejoras_piso = 0): una celda rara en la franja la contamina aunque sea poca. */
static int codigo_de_analisis_antes(void)
{
    ResultadoObstaculos r;
    ConfigObstaculos c;

    obstaculos_config_defecto(&c);
    c.mejoras_piso = 0;
    c.ref_ancho_mil = 188;   /* y su franja angosta de siempre */
    return obstaculos_analizar_con(&c, imagen, g_ancho, g_alto, &r);
}

/* ── Casos ─────────────────────────────────────────────────────────────── */

static void pruebas_piso_libre(void)
{
    rellenar(PISO);
    caso("piso liso: sin obstaculos", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    caso("piso con ruido leve (+-8): sin obstaculos", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(25);
    caso("piso con ruido fuerte (+-25): sin obstaculos", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    sumar_luz(30);
    caso("cambio de luz global (+30 de brillo): sin obstaculos", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    degradado_luz(70);
    caso("degradado de luz (piso lejano mas claro): sin obstaculos", verificar(0, 0, 0));
}

static void pruebas_cajas(void)
{
    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, CAJA);
    caso("caja al centro (hasta la fila 20): solo CENTRO, muy cerca", verificar(0, 3, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(1, 14, 8, 20, CAJA);
    caso("caja a la izquierda: solo IZQUIERDA, muy cerca", verificar(3, 0, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(23, 14, 30, 20, CAJA);
    caso("caja a la derecha: solo DERECHA, muy cerca", verificar(0, 0, 3));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 12, 19, 14, CAJA);
    caso("caja lejos (llega hasta la fila 14): cercania 1", verificar(0, 1, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 18, CAJA);
    caso("caja a media distancia (hasta la fila 18): cercania 2", verificar(0, 2, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(6, 14, 15, 19, CAJA);
    caso("caja entre izquierda y centro: ambas zonas", verificar(2, 2, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(0, 15, 6, 23, CAJA);
    caso("caja pegada abajo a la izquierda (fuera de la franja de referencia)", verificar(3, 0, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, PARED);
    caso("objeto gris azulado en el centro: tambien se detecta", verificar(0, 3, 0));

    rellenar(PISO);
    ruido(8);
    {
        static const unsigned char OSCURA[3] = { 35, 30, 25 };   /* misma tonalidad, mucho mas oscura */
        rectangulo_celdas(12, 14, 19, 20, OSCURA);
    }
    caso("caja oscura del mismo tono que el piso: se detecta por brillo", verificar(0, 3, 0));
}

static void pruebas_pared(void)
{
    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(0, 0, 31, 21, PARED);
    caso("pared que ocupa todo (el piso solo se ve en la franja de abajo): las tres zonas, muy cerca",
         verificar(3, 3, 3));
}

static void pruebas_ruido_y_limites(void)
{
    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(15, 16, 15, 16, CAJA);
    caso("una celda aislada no cuenta como obstaculo", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    {
        /* 8 celdas sueltas en la zona izquierda, ninguna junto a otra: sin el filtro de
         * celdas aisladas sumarian mas que MIN_CELDAS y darian un falso obstaculo. */
        int columnas[3] = { 1, 4, 7 };
        int filas[3] = { 13, 16, 19 };
        int i, j;

        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                if (!(i == 2 && j == 2)) {
                    rectangulo_celdas(columnas[i], filas[j], columnas[i], filas[j], CAJA);
                }
            }
        }
    }
    caso("8 celdas sueltas (ruido disperso): el filtro de aisladas las descarta", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(14, 16, 15, 17, CAJA);
    caso("objeto de 4 celdas (menos que MIN_CELDAS): no cuenta", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(13, 16, 15, 18, CAJA);
    caso("objeto de 9 celdas (3x3): si cuenta", verificar(0, 2, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(8, 0, 23, 9, CAJA);
    caso("objeto solo en la mitad superior de la imagen (filas 0-9): se ignora", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, CAJA);
    {
        ResultadoObstaculos r;
        int codigo = obstaculos_analizar(imagen, g_ancho, g_alto, &r);
        caso("la referencia del piso se calcula bien (cerca de 110,100,90)",
             codigo == OBS_OK && r.ref_r >= 107 && r.ref_r <= 113 && r.ref_g >= 97 && r.ref_g <= 103 &&
             r.ref_b >= 87 && r.ref_b <= 93);
        caso("la mascara marca las celdas de la caja y no las del piso",
             r.no_piso[17][15] == 1 && r.no_piso[17][2] == 0 && r.no_piso[23][15] == 0);
    }
}

static void pruebas_errores(void)
{
    ResultadoObstaculos r;

    caso("imagen nula: OBS_ERR_IMAGEN_NULA", obstaculos_analizar(NULL, g_ancho, g_alto, &r) == OBS_ERR_IMAGEN_NULA);
    caso("resultado nulo: OBS_ERR_IMAGEN_NULA", obstaculos_analizar(imagen, g_ancho, g_alto, NULL) == OBS_ERR_IMAGEN_NULA);
    caso("alto invalido (20 px, menos de 3 filas de celdas): OBS_ERR_TAMANO", obstaculos_analizar(imagen, g_ancho, 20, &r) == OBS_ERR_TAMANO);
    caso("ancho invalido (660 px, mas que el maximo): OBS_ERR_TAMANO", obstaculos_analizar(imagen, 660, g_alto, &r) == OBS_ERR_TAMANO);
    caso("ancho cero: OBS_ERR_TAMANO", obstaculos_analizar(imagen, 0, 0, &r) == OBS_ERR_TAMANO);

    /* Con un error el resultado queda en ceros. */
    memset(&r, 0x55, sizeof r);
    obstaculos_analizar(imagen, 20, 20, &r);
    caso("con un error el resultado queda en ceros", r.zona[1].celdas == 0 && r.zona[1].hay_obstaculo == 0 && r.ref_r == 0);

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(13, 22, 15, 23, CAJA);   /* un objeto cubre la mitad de la franja de referencia */
    caso("referencia contaminada (objeto sobre la mitad de la franja angosta de antes): OBS_ERR_REFERENCIA", codigo_de_analisis_antes() == OBS_ERR_REFERENCIA);
    /* Con la tira ancha por defecto (80 % del ancho) el mismo objeto de 2 celdas es un intruso chico (menos del 15 % de la tira): se descarta. */
    caso("el mismo objeto con la tira ancha por defecto (menos del 15 % de apoyo): se descarta y la imagen se analiza", codigo_de_analisis() == OBS_OK);

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(16, 23, 16, 23, CAJA);   /* una sola celda de la franja */
    caso("referencia con una sola celda distinta (sin las mejoras de piso): OBS_ERR_REFERENCIA", codigo_de_analisis_antes() == OBS_ERR_REFERENCIA);
    /* Con las mejoras, una celda rara que respalda menos del 15 % de la franja se descarta (no se aprende como piso y no ensucia la franja). */
    caso("referencia con una sola celda distinta (con las mejoras, la franja es de 12 celdas: 8 % de apoyo): se descarta y la imagen se analiza", codigo_de_analisis() == OBS_OK);

    caso("textos de error en espanol", strlen(obstaculos_texto_error(OBS_ERR_REFERENCIA)) > 10 &&
         strlen(obstaculos_texto_error(OBS_ERR_TAMANO)) > 5);
}

static void pruebas_repetibilidad(void)
{
    ResultadoObstaculos a, b;

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, CAJA);
    obstaculos_analizar(imagen, g_ancho, g_alto, &a);
    obstaculos_analizar(imagen, g_ancho, g_alto, &b);
    caso("analizar dos veces la misma imagen da lo mismo", memcmp(&a, &b, sizeof a) == 0);
}

/* La configuración (ConfigObstaculos) cambia el resultado. */
static void pruebas_configuracion(void)
{
    ConfigObstaculos config;
    ResultadoObstaculos por_defecto, con_config;
    static const unsigned char SUTIL[3] = { 130, 100, 70 };   /* casi del color del piso (110,100,90) */

    obstaculos_config_defecto(&config);
    caso("config por defecto: umbral_color = UMBRAL_COLOR y min_celdas = MIN_CELDAS",
         config.umbral_color == UMBRAL_COLOR && config.min_celdas == MIN_CELDAS &&
         config.ref_ancho_mil == OBS_REF_ANCHO_MIL && config.ref_alto_mil == OBS_REF_ALTO_MIL &&
         config.ref_centro_x_mil == OBS_REF_CENTRO_X_MIL && config.ref_borde_inf_mil == OBS_REF_BORDE_INF_MIL &&
         config.fila_minima_mil == OBS_FILA_MINIMA_MIL);

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, CAJA);
    obstaculos_analizar(imagen, g_ancho, g_alto, &por_defecto);
    obstaculos_analizar_con(&config, imagen, g_ancho, g_alto, &con_config);
    caso("obstaculos_analizar equivale a analizar_con la config por defecto", memcmp(&por_defecto, &con_config, sizeof por_defecto) == 0);

    /* MIN_CELDAS: un objeto de 4 celdas (2x2) se detecta si se baja el minimo y no si se sube. */
    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(14, 16, 15, 17, CAJA);
    caso("min_celdas 6 (defecto): objeto de 4 celdas no cuenta", verificar_con(&config, 0, 0, 0));
    config.min_celdas = 3;
    caso("min_celdas 3: el mismo objeto de 4 celdas SI cuenta", verificar_con(&config, 0, 2, 0));
    obstaculos_config_defecto(&config);

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(13, 16, 15, 18, CAJA);   /* 9 celdas */
    caso("min_celdas 6 (defecto): objeto de 9 celdas cuenta", verificar_con(&config, 0, 2, 0));
    config.min_celdas = 12;
    caso("min_celdas 12: el mismo objeto de 9 celdas YA NO cuenta", verificar_con(&config, 0, 0, 0));
    obstaculos_config_defecto(&config);

    /* UMBRAL_COLOR: un objeto de color casi igual al piso solo se ve con un umbral bajo. */
    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, SUTIL);
    caso("umbral_color 60 (defecto): objeto de color casi igual al piso pasa como piso", verificar_con(&config, 0, 0, 0));
    config.umbral_color = 30;
    caso("umbral_color 30: el mismo objeto SI se detecta", verificar_con(&config, 0, 3, 0));
    obstaculos_config_defecto(&config);

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, CAJA);
    config.umbral_color = 765;
    caso("umbral_color 765 (maximo): hasta la caja roja se toma por piso (no detecta nada)", verificar_con(&config, 0, 0, 0));
    obstaculos_config_defecto(&config);

    /* Franja de referencia: si un objeto tapa la franja por defecto, se puede usar otra. */
    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 20, 15, 23, CAJA);   /* la caja tapa la mitad de la franja central de abajo */
    config.ref_ancho_mil = 188;   /* la franja angosta de antes: la caja tapa la mitad */
    config.mejoras_piso = 0;
    caso("la caja tapa parte de la franja angosta: OBS_ERR_REFERENCIA", obstaculos_analizar_con(&config, imagen, g_ancho, g_alto, &(ResultadoObstaculos){0}) == OBS_ERR_REFERENCIA);
    config.ref_centro_x_mil = mil_de(3, OBS_COLUMNAS);   /* otra franja: abajo a la izquierda (columnas 0 a 5), que esta libre */
    {
        ResultadoObstaculos r;
        int codigo = obstaculos_analizar_con(&config, imagen, g_ancho, g_alto, &r);
        caso("con la franja de referencia en otra parte (libre) se detecta la caja del centro",
             codigo == OBS_OK && r.zona[ZONA_CENTRO].hay_obstaculo == 1 && r.zona[ZONA_CENTRO].cercania == 3 &&
             r.ref_r >= 107 && r.ref_r <= 113);
    }
    obstaculos_config_defecto(&config);

    /* Configuraciones no validas. */
    {
        ResultadoObstaculos r;
        ConfigObstaculos mala;

        rellenar(PISO);
        caso("config NULL: OBS_ERR_CONFIG", obstaculos_analizar_con(NULL, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG);
        mala = config; mala.min_celdas = 0;
        caso("min_celdas 0: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG);
        mala = config; mala.umbral_color = -1;
        caso("umbral_color negativo: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG);
        mala = config; mala.ref_centro_x_mil = 1001;
        caso("centro de la franja fuera de la imagen (1001 milesimas): OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG);
        mala = config; mala.ref_borde_inf_mil = 1500;
        caso("borde de la franja por debajo de la imagen: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG);
        mala = config; mala.ref_alto_mil = 0;
        caso("franja de alto cero: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG);
        mala = config; mala.ref_ancho_mil = 0;
        caso("franja de ancho cero: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG);
        mala = config; mala.ref_centro_x_mil = 1000; mala.ref_ancho_mil = 1000;
        caso("franja del ancho de toda la imagen es valida (se acomoda dentro)", obstaculos_analizar_con(&mala, imagen, g_ancho, g_alto, &r) != OBS_ERR_CONFIG);
        mala = config; mala.ref_centro_x_mil = 1000;
        caso("franja pegada al borde derecho: se corre hacia adentro y no se sale de la imagen",
             obstaculos_analizar_con(&mala, imagen, g_ancho, g_alto, &r) == OBS_OK);
        caso("config valida pero imagen de otro tamano: OBS_ERR_TAMANO primero",
             obstaculos_analizar_con(&config, imagen, 20, 20, &r) == OBS_ERR_TAMANO);
        caso("con OBS_ERR_CONFIG el resultado queda en ceros",
             (mala.ref_alto_mil = 0, obstaculos_analizar_con(&mala, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG) &&
             r.zona[1].celdas == 0 && r.ref_r == 0);
    }
}

/* Los valores que eran el defecto ANTES de validarlos en el carro real (fila minima 500, 6 celdas, umbral 60): las pruebas de mecanica
 * (fila limite, objetos de 6 celdas) se escribieron con ellos y siguen valiendo con esta configuracion explicita. */
static void config_clasica(ConfigObstaculos *config)
{
    obstaculos_config_defecto(config);
    config->fila_minima_mil = 500;
    config->min_celdas = 6;
    config->umbral_color = 60;
}

/* FILA_MINIMA: las filas de arriba de esa fila se ignoran; la cercanía sigue saliendo de la fila más baja. */
static void pruebas_fila_minima(void)
{
    ConfigObstaculos config;
    ResultadoObstaculos r;

    obstaculos_config_defecto(&config);
    caso("config por defecto (validada en el carro real): fila_minima 440, min_celdas 7, umbral_color 40",
         config.fila_minima_mil == OBS_FILA_MINIMA_MIL && config.fila_minima_mil == 440 && config.min_celdas == 7 && config.umbral_color == 40);
    config_clasica(&config);   /* el resto de este grupo prueba la mecanica de la fila minima con los valores de siempre */

    /* Un objeto lejano, todo por encima de la fila minima (filas 4 a 8). */
    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(13, 4, 18, 8, CAJA);
    caso("fila_minima 12 (defecto): objeto en filas 4-8 se ignora", verificar_con(&config, 0, 0, 0));
    config.fila_minima_mil = mil_de(4, OBS_FILAS);
    caso("fila_minima 4: el mismo objeto SI se detecta (cercania 1: fila baja 8)", verificar_con(&config, 0, 1, 0));
    config.fila_minima_mil = mil_de(9, OBS_FILAS);
    caso("fila_minima 9: objeto en filas 4-8 se ignora (una fila por encima del limite)", verificar_con(&config, 0, 0, 0));
    config.fila_minima_mil = mil_de(8, OBS_FILAS);
    caso("fila_minima 8: la fila 8 es la limite y solo ella cuenta (6 celdas): SI se detecta",
         verificar_con(&config, 0, 1, 0));
    config.fila_minima_mil = mil_de(4, OBS_FILAS);
    obstaculos_analizar_con(&config, imagen, g_ancho, g_alto, &r);
    caso("fila_minima 4: se cuentan las 30 celdas del objeto", r.zona[ZONA_CENTRO].celdas == 30);
    config_clasica(&config);

    /* Objeto justo en la fila limite (filas 12 y 13). */
    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(13, 12, 18, 13, CAJA);
    caso("objeto en las filas 12-13 con fila_minima 12: cuenta (justo en la fila limite)",
         verificar_con(&config, 0, 1, 0));
    config.fila_minima_mil = mil_de(13, OBS_FILAS);
    caso("fila_minima 13: queda solo la fila 13 (6 celdas): sigue contando", verificar_con(&config, 0, 1, 0));
    config.fila_minima_mil = mil_de(14, OBS_FILAS);
    caso("fila_minima 14: las filas 12-13 quedan arriba del limite: se ignora", verificar_con(&config, 0, 0, 0));
    config_clasica(&config);

    /* La cercania sigue saliendo de la fila MAS BAJA del objeto, sea cual sea fila_minima. */
    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(13, 6, 18, 21, CAJA);   /* objeto alto: de la fila 6 a la 21 */
    caso("objeto alto (filas 6-21), fila_minima 12: cercania 3 (fila baja 21)", verificar_con(&config, 0, 3, 0));
    config.fila_minima_mil = mil_de(0, OBS_FILAS);
    caso("fila_minima 0: cercania 3 igual", verificar_con(&config, 0, 3, 0));
    config.fila_minima_mil = mil_de(18, OBS_FILAS);
    caso("fila_minima 18: cercania 3 igual (la fila mas baja no cambia)", verificar_con(&config, 0, 3, 0));
    config.fila_minima_mil = mil_de(22, OBS_FILAS);
    caso("fila_minima 22: el objeto acaba en la fila 21, queda arriba del limite: se ignora", verificar_con(&config, 0, 0, 0));
    config_clasica(&config);

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(13, 6, 18, 14, CAJA);   /* objeto que acaba en la fila 14 (media distancia: 1) */
    caso("objeto filas 6-14: cercania 1 (fila baja 14 < 16)", verificar_con(&config, 0, 1, 0));
    config.fila_minima_mil = mil_de(14, OBS_FILAS);
    caso("fila_minima 14: solo cuenta la fila 14 (6 celdas), cercania 1", verificar_con(&config, 0, 1, 0));
    config_clasica(&config);

    /* Valores validos en los extremos y no validos. */
    rellenar(PISO);
    config.fila_minima_mil = mil_de(0, OBS_FILAS);
    caso("fila_minima 0 es valida", obstaculos_analizar_con(&config, imagen, g_ancho, g_alto, &r) == OBS_OK);
    config.fila_minima_mil = mil_de(OBS_FILAS - 1, OBS_FILAS);
    caso("fila_minima 23 (ultima fila) es valida", obstaculos_analizar_con(&config, imagen, g_ancho, g_alto, &r) == OBS_OK);
    config.fila_minima_mil = mil_de(-1, OBS_FILAS);
    caso("fila_minima -1: OBS_ERR_CONFIG", obstaculos_analizar_con(&config, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG);
    config.fila_minima_mil = 1000;
    caso("fila_minima 1.0 (1000 milesimas): valida, se mira solo la ultima fila", obstaculos_analizar_con(&config, imagen, g_ancho, g_alto, &r) == OBS_OK && r.fila_minima_usada == OBS_FILAS - 1);
    config.fila_minima_mil = 1001;
    caso("fila_minima 1001 milesimas: OBS_ERR_CONFIG y resultado en ceros",
         obstaculos_analizar_con(&config, imagen, g_ancho, g_alto, &r) == OBS_ERR_CONFIG && r.zona[1].celdas == 0);
}

/* ── Pruebas por tamaño de imagen ──────────────────────────────────────────
 * El mismo juego de escenas se repite en 320x240, 240x176 (HQVGA, el modo de la cámara real) y
 * 160x120. Las cajas se colocan con PROPORCIONES de la imagen (no con celdas fijas), así que la
 * escena es la misma en cualquier tamaño. */

static char etiqueta[32];

static void caso_t(const char *nombre, int ok)
{
    char completo[160];

    snprintf(completo, sizeof completo, "[%s] %s", etiqueta, nombre);
    caso(completo, ok);
}

/* Celdas = total * milesimas / 1000: la misma conversión que usa el detector. */
static int celdas_de(int total, int milesimas)
{
    return total * milesimas / 1000;
}

/* Pinta una caja de celdas dada por proporciones del ancho (columnas) y por filas absolutas. */
static void caja_por_proporcion(int col0_mil, int col1_mil, int fila0, int fila1, const unsigned char color[3])
{
    int columnas = obstaculos_columnas_de(g_ancho);
    int c0 = celdas_de(columnas, col0_mil);
    int c1 = celdas_de(columnas, col1_mil);

    rectangulo_celdas(c0, fila0, c1, fila1, color);
}

/* Cercanía esperada de una caja cuya fila más baja es `fila_baja`, igual que la regla del detector. */
static int cercania_esperada(int filas, int fila_baja)
{
    if (fila_baja >= celdas_de(filas, OBS_CERCA_MIL)) {
        return 3;
    }
    return (fila_baja >= celdas_de(filas, OBS_MEDIA_MIL)) ? 2 : 1;
}

static void pruebas_tamano(int ancho, int alto, int rejilla_chica_esperada)
{
    int columnas = obstaculos_columnas_de(ancho);
    int filas = obstaculos_filas_de(alto);
    int fila_min = celdas_de(filas, OBS_FILA_MINIMA_MIL);
    int fila_franja = filas - ((celdas_de(filas, OBS_REF_ALTO_MIL) < 1) ? 1 : celdas_de(filas, OBS_REF_ALTO_MIL));
    int baja_cerca = celdas_de(filas, OBS_CERCA_MIL);
    int baja_media = celdas_de(filas, OBS_MEDIA_MIL);
    int baja_lejos = baja_media - 1;
    ResultadoObstaculos r;
    int codigo;

    g_ancho = ancho;
    g_alto = alto;
    snprintf(etiqueta, sizeof etiqueta, "%dx%d", ancho, alto);

    rellenar(PISO);
    ruido(8);
    caso_t("piso liso con ruido leve: sin obstaculos", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(25);
    caso_t("piso con ruido fuerte: sin obstaculos", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    sumar_luz(30);
    caso_t("cambio de luz global: sin obstaculos", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    caja_por_proporcion(400, 600, fila_min, baja_cerca, CAJA);
    caso_t("caja al centro llegando hasta abajo: solo CENTRO, muy cerca", verificar(0, 3, 0));

    rellenar(PISO);
    ruido(8);
    caja_por_proporcion(30, 250, fila_min, baja_cerca, CAJA);
    caso_t("caja a la izquierda: solo IZQUIERDA, muy cerca", verificar(3, 0, 0));

    rellenar(PISO);
    ruido(8);
    caja_por_proporcion(760, 950, fila_min, baja_cerca, CAJA);
    caso_t("caja a la derecha: solo DERECHA, muy cerca", verificar(0, 0, 3));

    rellenar(PISO);
    ruido(8);
    caja_por_proporcion(400, 600, fila_min, baja_media, CAJA);
    caso_t("caja a media distancia: cercania 2", verificar(0, cercania_esperada(filas, baja_media), 0));

    rellenar(PISO);
    ruido(8);
    caja_por_proporcion(400, 600, fila_min, baja_lejos, CAJA);
    caso_t("caja lejos (justo antes del limite de media distancia): cercania 1", verificar(0, 1, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(0, 0, columnas - 1, fila_franja - 1, PARED);
    caso_t("pared que ocupa todo menos la franja del piso: las tres zonas, muy cerca", verificar(3, 3, 3));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(columnas / 2, fila_min + 1, columnas / 2, fila_min + 1, CAJA);
    caso_t("una celda aislada no cuenta", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    caja_por_proporcion(300, 700, 0, fila_min - 1, CAJA);
    caso_t("objeto solo en la mitad de arriba: se ignora", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(columnas / 2, filas - 1, columnas / 2, filas - 1, CAJA);
    caso_t("una celda de la franja de referencia distinta (sin las mejoras de piso): OBS_ERR_REFERENCIA", codigo_de_analisis_antes() == OBS_ERR_REFERENCIA);

    rellenar(PISO);
    ruido(8);
    caja_por_proporcion(400, 600, fila_min, baja_cerca, CAJA);
    codigo = obstaculos_analizar(imagen, ancho, alto, &r);
    caso_t("el resultado trae el tamano de la rejilla y la fila minima usada",
           codigo == OBS_OK && r.columnas == columnas && r.filas == filas && r.fila_minima_usada == fila_min);
    caso_t("la referencia del piso es la del piso (cerca de 110,100,90)",
           r.ref_r >= 107 && r.ref_r <= 113 && r.ref_g >= 97 && r.ref_g <= 103 && r.ref_b >= 87 && r.ref_b <= 93);
    caso_t("la mascara marca la caja y no las celdas de piso",
           r.no_piso[baja_cerca][columnas / 2] == 1 && r.no_piso[filas - 1][columnas / 2] == 0 &&
           r.no_piso[fila_min][0] == 0);
    caso_t("el aviso de rejilla chica es el esperado", r.rejilla_chica == rejilla_chica_esperada);
    caso_t("la franja de referencia queda abajo y al centro, dentro de la rejilla",
           r.ref_nfilas >= 1 && r.ref_ncolumnas >= 1 && r.ref_fila + r.ref_nfilas == filas &&
           r.ref_columna >= 0 && r.ref_columna + r.ref_ncolumnas <= columnas &&
           (r.ref_columna + r.ref_ncolumnas / 2 - columnas / 2 >= -1) &&
           (r.ref_columna + r.ref_ncolumnas / 2 - columnas / 2 <= 1));

    /* Una caja de tamaño de objeto real (no una pared) se ve con la config por defecto. */
    rellenar(PISO);
    ruido(8);
    caja_por_proporcion(430, 570, baja_media - 1, baja_media, CAJA);
    caso_t("objeto pequeno (2 filas) en el centro: se detecta o se descarta sin fallar",
           obstaculos_analizar(imagen, ancho, alto, &r) == OBS_OK);

    g_ancho = OBS_ANCHO;
    g_alto = OBS_ALTO;
    etiqueta[0] = '\0';
}

/* Tamaños raros: nunca se cuelga ni lee fuera de la imagen; da un error claro o funciona. */
static void pruebas_tamanos_raros(void)
{
    ResultadoObstaculos r;
    int i;
    static const int anchos[] = { 1, 20, 29, 30, 245, 300, 640, 641, 700 };
    static const int altos[]  = { 1, 20, 29, 30, 179, 300, 480, 481, 500 };

    g_ancho = 30;
    g_alto = 30;
    rellenar(PISO);
    caso("30x30 (3x3 celdas, el minimo): funciona y avisa rejilla chica",
         obstaculos_analizar(imagen, 30, 30, &r) == OBS_OK && r.columnas == 3 && r.filas == 3 && r.rejilla_chica == 1);

    g_ancho = 640;
    g_alto = 480;
    rellenar(PISO);
    caso("640x480 (el maximo): funciona y no avisa", obstaculos_analizar(imagen, 640, 480, &r) == OBS_OK &&
         r.columnas == 64 && r.filas == 48 && r.rejilla_chica == 0);

    g_ancho = 245;
    g_alto = 179;
    rellenar(PISO);
    caso("245x179 (no es multiplo de 10): usa 24x17 celdas e ignora los pixeles que sobran",
         obstaculos_analizar(imagen, 245, 179, &r) == OBS_OK && r.columnas == 24 && r.filas == 17);

    for (i = 0; i < (int)(sizeof anchos / sizeof anchos[0]); i++) {
        int esperado_error = (anchos[i] < 30 || anchos[i] > 640 || altos[i] < 30 || altos[i] > 480);

        g_ancho = (anchos[i] > 640) ? 640 : anchos[i];
        g_alto = (altos[i] > 480) ? 480 : altos[i];
        if (g_ancho > 0 && g_alto > 0) {
            rellenar(PISO);
        }
        caso("tamano raro (ancho y alto en la lista): error claro de tamano o analisis correcto, sin colgarse",
             obstaculos_analizar(imagen, anchos[i], altos[i], &r) == (esperado_error ? OBS_ERR_TAMANO : OBS_OK));
    }

    caso("ancho negativo: OBS_ERR_TAMANO", obstaculos_analizar(imagen, -320, 240, &r) == OBS_ERR_TAMANO);
    caso("alto negativo: OBS_ERR_TAMANO", obstaculos_analizar(imagen, 320, -1, &r) == OBS_ERR_TAMANO);
    caso("2 columnas x 100 filas: OBS_ERR_TAMANO (menos de 3 columnas)", obstaculos_analizar(imagen, 25, 1000, &r) == OBS_ERR_TAMANO);
    caso("el texto del error de tamano es claro", strlen(obstaculos_texto_error(OBS_ERR_TAMANO)) > 10);

    g_ancho = OBS_ANCHO;
    g_alto = OBS_ALTO;
}

int main(void)
{
    pruebas_piso_libre();
    pruebas_cajas();
    pruebas_pared();
    pruebas_ruido_y_limites();
    pruebas_errores();
    pruebas_repetibilidad();
    pruebas_configuracion();
    pruebas_fila_minima();
    pruebas_tamano(320, 240, 0);
    pruebas_tamano(240, 176, 0);
    pruebas_tamano(160, 120, 1);
    pruebas_tamanos_raros();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
