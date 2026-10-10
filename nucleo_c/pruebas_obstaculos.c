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

static unsigned char imagen[OBS_ALTO * OBS_ANCHO * 3];

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

    for (i = 0; i < OBS_ALTO * OBS_ANCHO; i++) {
        imagen[i * 3]     = color[0];
        imagen[i * 3 + 1] = color[1];
        imagen[i * 3 + 2] = color[2];
    }
}

/* Suma ruido de -amplitud a +amplitud a cada canal de cada píxel. */
static void ruido(int amplitud)
{
    int i;

    for (i = 0; i < OBS_ALTO * OBS_ANCHO * 3; i++) {
        imagen[i] = limitar(imagen[i] + (int)(aleatorio() % (unsigned)(2 * amplitud + 1)) - amplitud);
    }
}

/* Suma el mismo valor a todos los canales (cambio de luz global). */
static void sumar_luz(int cantidad)
{
    int i;

    for (i = 0; i < OBS_ALTO * OBS_ANCHO * 3; i++) {
        imagen[i] = limitar(imagen[i] + cantidad);
    }
}

/* Degradado de luz: arriba suma "maximo" y abajo suma 0 (el piso lejano más claro). */
static void degradado_luz(int maximo)
{
    int x, y, c;

    for (y = 0; y < OBS_ALTO; y++) {
        int extra = (OBS_ALTO - 1 - y) * maximo / (OBS_ALTO - 1);

        for (x = 0; x < OBS_ANCHO; x++) {
            for (c = 0; c < 3; c++) {
                imagen[(y * OBS_ANCHO + x) * 3 + c] = limitar(imagen[(y * OBS_ANCHO + x) * 3 + c] + extra);
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
            imagen[(y * OBS_ANCHO + x) * 3]     = color[0];
            imagen[(y * OBS_ANCHO + x) * 3 + 1] = color[1];
            imagen[(y * OBS_ANCHO + x) * 3 + 2] = color[2];
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

    codigo = obstaculos_analizar(imagen, OBS_ANCHO, OBS_ALTO, &r);
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

    codigo = obstaculos_analizar_con(config, imagen, OBS_ANCHO, OBS_ALTO, &r);
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

    return obstaculos_analizar(imagen, OBS_ANCHO, OBS_ALTO, &r);
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
    rectangulo_celdas(8, 0, 23, 10, CAJA);
    caso("objeto solo en la mitad superior de la imagen: se ignora", verificar(0, 0, 0));

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, CAJA);
    {
        ResultadoObstaculos r;
        int codigo = obstaculos_analizar(imagen, OBS_ANCHO, OBS_ALTO, &r);
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

    caso("imagen nula: OBS_ERR_IMAGEN_NULA", obstaculos_analizar(NULL, OBS_ANCHO, OBS_ALTO, &r) == OBS_ERR_IMAGEN_NULA);
    caso("resultado nulo: OBS_ERR_IMAGEN_NULA", obstaculos_analizar(imagen, OBS_ANCHO, OBS_ALTO, NULL) == OBS_ERR_IMAGEN_NULA);
    caso("alto invalido (200): OBS_ERR_TAMANO", obstaculos_analizar(imagen, OBS_ANCHO, 200, &r) == OBS_ERR_TAMANO);
    caso("ancho invalido (640): OBS_ERR_TAMANO", obstaculos_analizar(imagen, 640, OBS_ALTO, &r) == OBS_ERR_TAMANO);
    caso("ancho cero: OBS_ERR_TAMANO", obstaculos_analizar(imagen, 0, 0, &r) == OBS_ERR_TAMANO);

    /* Con un error el resultado queda en ceros. */
    memset(&r, 0x55, sizeof r);
    obstaculos_analizar(imagen, 100, 100, &r);
    caso("con un error el resultado queda en ceros", r.zona[1].celdas == 0 && r.zona[1].hay_obstaculo == 0 && r.ref_r == 0);

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(13, 22, 15, 23, CAJA);   /* un objeto cubre la mitad de la franja de referencia */
    caso("referencia contaminada (objeto sobre la franja): OBS_ERR_REFERENCIA", codigo_de_analisis() == OBS_ERR_REFERENCIA);

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(16, 23, 16, 23, CAJA);   /* una sola celda de la franja */
    caso("referencia con una sola celda distinta: OBS_ERR_REFERENCIA", codigo_de_analisis() == OBS_ERR_REFERENCIA);

    caso("textos de error en espanol", strlen(obstaculos_texto_error(OBS_ERR_REFERENCIA)) > 10 &&
         strlen(obstaculos_texto_error(OBS_ERR_TAMANO)) > 5);
}

static void pruebas_repetibilidad(void)
{
    ResultadoObstaculos a, b;

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, CAJA);
    obstaculos_analizar(imagen, OBS_ANCHO, OBS_ALTO, &a);
    obstaculos_analizar(imagen, OBS_ANCHO, OBS_ALTO, &b);
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
         config.ref_columnas == OBS_REF_COLUMNAS && config.ref_filas == OBS_REF_FILAS &&
         config.ref_columna_inicial == OBS_REF_COLUMNA_INICIAL && config.ref_fila_inicial == OBS_REF_FILA_INICIAL);

    rellenar(PISO);
    ruido(8);
    rectangulo_celdas(12, 14, 19, 20, CAJA);
    obstaculos_analizar(imagen, OBS_ANCHO, OBS_ALTO, &por_defecto);
    obstaculos_analizar_con(&config, imagen, OBS_ANCHO, OBS_ALTO, &con_config);
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
    caso("la caja tapa parte de la franja por defecto: OBS_ERR_REFERENCIA", codigo_de_analisis() == OBS_ERR_REFERENCIA);
    config.ref_columna_inicial = 0;     /* otra franja: abajo a la izquierda, que esta libre */
    config.ref_columnas = 6;
    {
        ResultadoObstaculos r;
        int codigo = obstaculos_analizar_con(&config, imagen, OBS_ANCHO, OBS_ALTO, &r);
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
        caso("config NULL: OBS_ERR_CONFIG", obstaculos_analizar_con(NULL, imagen, OBS_ANCHO, OBS_ALTO, &r) == OBS_ERR_CONFIG);
        mala = config; mala.min_celdas = 0;
        caso("min_celdas 0: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, OBS_ANCHO, OBS_ALTO, &r) == OBS_ERR_CONFIG);
        mala = config; mala.umbral_color = -1;
        caso("umbral_color negativo: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, OBS_ANCHO, OBS_ALTO, &r) == OBS_ERR_CONFIG);
        mala = config; mala.ref_columna_inicial = 30;
        caso("franja que se sale por la derecha: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, OBS_ANCHO, OBS_ALTO, &r) == OBS_ERR_CONFIG);
        mala = config; mala.ref_fila_inicial = 23; mala.ref_filas = 2;
        caso("franja que se sale por abajo: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, OBS_ANCHO, OBS_ALTO, &r) == OBS_ERR_CONFIG);
        mala = config; mala.ref_filas = 0;
        caso("franja de alto cero: OBS_ERR_CONFIG", obstaculos_analizar_con(&mala, imagen, OBS_ANCHO, OBS_ALTO, &r) == OBS_ERR_CONFIG);
        caso("config valida pero imagen de otro tamano: OBS_ERR_TAMANO primero",
             obstaculos_analizar_con(&config, imagen, 100, 100, &r) == OBS_ERR_TAMANO);
        caso("con OBS_ERR_CONFIG el resultado queda en ceros",
             (mala.ref_filas = 0, obstaculos_analizar_con(&mala, imagen, OBS_ANCHO, OBS_ALTO, &r) == OBS_ERR_CONFIG) &&
             r.zona[1].celdas == 0 && r.ref_r == 0);
    }
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

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
