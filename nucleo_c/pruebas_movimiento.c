/*
 * pruebas_movimiento.c — Pruebas de movimiento_visual.c en PC con imágenes sintéticas.
 *
 * GUÍA DE LECTURA
 *  1. Se fabrica un PANORAMA (un cuarto ancho, solo con números: cada columna tiene una textura de bordes) y
 *     un cuadro es una ventana de ese panorama. Si el carro gira a la derecha la ventana se mueve a la
 *     derecha del panorama y el contenido del cuadro se corre a la izquierda.
 *  2. La mitad de arriba del cuadro es el cuarto (la que mira el estimador); la de abajo es "piso" con ruido
 *     distinto en cada cuadro, para comprobar que no se usa.
 *  3. Casos: corrimientos de 1, 2, 5, 10, 20, 40 y 80 píxeles a los dos lados, con y sin ruido, cambio de luz, escena
 *     casi uniforme y textura repetida (sin confianza), escenas sin relación, cuadros iguales, errores y un giro
 *     de 90 grados acumulado cuadro a cuadro.
 */

#include <stdio.h>
#include <string.h>
#include "movimiento_visual.h"

#define ANCHO_PANORAMA 1400
#define ALTO_MAX       240

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

static unsigned int semilla = 99u;

static unsigned int aleatorio(void)
{
    semilla = semilla * 1103515245u + 12345u;
    return (semilla >> 16) & 0x7FFF;
}

static int valor_absoluto(int x)
{
    return (x < 0) ? -x : x;
}

/* El panorama: brillo (de 0 a 255) de cada punto del cuarto. Cada fila de la mitad de arriba tiene una textura de
 * "nudos" aleatorios cada 7 píxeles unidos con rectas (así tiene bordes y los corrimientos de 1 píxel se notan). */
static unsigned char panorama[ALTO_MAX][ANCHO_PANORAMA];

static void fabricar_panorama(void)
{
    static int nudo[ALTO_MAX][ANCHO_PANORAMA / 7 + 2];
    int y, x;

    for (y = 0; y < ALTO_MAX; y++) {
        for (x = 0; x < ANCHO_PANORAMA / 7 + 2; x++) {
            nudo[y][x] = 40 + (int)(aleatorio() % 180u);
        }
    }
    for (y = 0; y < ALTO_MAX; y++) {
        for (x = 0; x < ANCHO_PANORAMA; x++) {
            int k = x / 7, f = x % 7;

            panorama[y][x] = (unsigned char)((nudo[y][k] * (7 - f) + nudo[y][k + 1] * f) / 7);
        }
    }
}

static unsigned char limitar(int v)
{
    if (v < 0) {
        return 0;
    }
    return (unsigned char)((v > 255) ? 255 : v);
}

/* Un cuadro de ancho x alto: ventana del panorama desde la columna `origen`, con ruido de +-ruido y luz extra.
 * La mitad de abajo (el piso) es otra cosa en cada cuadro. */
static unsigned char cuadro[ALTO_MAX * MV_MAX_ANCHO * 3];

static void tomar_cuadro(int ancho, int alto, int origen, int ruido, int luz)
{
    int x, y, c;

    for (y = 0; y < alto; y++) {
        for (x = 0; x < ancho; x++) {
            int brillo = (y < alto / 2) ? panorama[y][origen + x] : 60 + (int)(aleatorio() % 120u);   /* abajo: piso al azar */

            for (c = 0; c < 3; c++) {
                int r = (ruido > 0) ? (int)(aleatorio() % (unsigned)(2 * ruido + 1)) - ruido : 0;

                cuadro[(y * ancho + x) * 3 + c] = limitar(brillo + luz + r);
            }
        }
    }
}

/* Compara dos cuadros con ventanas en `origen` y `origen + d` (el carro gira a la derecha d "pixeles de panorama"). */
static int medir(int ancho, int alto, int origen, int d, int ruido, int luz_b, MovimientoVisual *r)
{
    int huella_a[MV_MAX_ANCHO], huella_b[MV_MAX_ANCHO];

    tomar_cuadro(ancho, alto, origen, ruido, 0);
    mv_huella(cuadro, ancho, alto, huella_a);
    tomar_cuadro(ancho, alto, origen + d, ruido, luz_b);
    mv_huella(cuadro, ancho, alto, huella_b);
    return mv_corrimiento(huella_a, huella_b, ancho, ancho * 2 / 5, r);
}

/* ── Corrimientos ──────────────────────────────────────────────────────── */

static void pruebas_corrimientos(int ancho, int alto)
{
    static const int DESPLAZAMIENTOS[] = { 1, 2, 5, 10, 20, 40, 80 };
    MovimientoVisual r;
    int i, lado;
    char nombre[160];

    for (i = 0; i < (int)(sizeof DESPLAZAMIENTOS / sizeof DESPLAZAMIENTOS[0]); i++) {
        for (lado = -1; lado <= 1; lado += 2) {
            int d = DESPLAZAMIENTOS[i] * lado;   /* + = la ventana va a la derecha: el contenido se corre a la izquierda */
            int esperado_x16 = -d * 16;

            medir(ancho, alto, 300, d, 0, 0, &r);
            snprintf(nombre, sizeof nombre, "[%dx%d] sin ruido: ventana %+d px -> corrimiento %+.2f px (error menos de 0.25 px)", ancho, alto, d, esperado_x16 / 16.0);
            caso(nombre, r.valido && valor_absoluto(r.corrimiento_x16 - esperado_x16) <= 4);
            if (!(r.valido && valor_absoluto(r.corrimiento_x16 - esperado_x16) <= 4)) {
                printf("      valido=%d motivo=%d corrimiento_x16=%d esperado=%d confianza=%d energia=%d\n", r.valido, r.motivo, r.corrimiento_x16, esperado_x16, r.confianza_mil, r.energia);
            }
        }
    }
}

static void pruebas_con_ruido(int ancho, int alto)
{
    static const int DESPLAZAMIENTOS[] = { 1, 10, 40, 80 };
    MovimientoVisual r;
    int i, lado, buenas = 0, total = 0, peor = 0;
    int repeticion;
    char nombre[160];

    for (repeticion = 0; repeticion < 20; repeticion++) {
        for (i = 0; i < 4; i++) {
            for (lado = -1; lado <= 1; lado += 2) {
                int d = DESPLAZAMIENTOS[i] * lado;
                int esperado_x16 = -d * 16;
                int error;

                semilla = 5000u + (unsigned int)repeticion * 97u + (unsigned int)i * 13u + (unsigned int)(lado + 1) * 7u;
                medir(ancho, alto, 200 + repeticion * 13, d, 10, 0, &r);
                total++;
                error = r.valido ? valor_absoluto(r.corrimiento_x16 - esperado_x16) : 9999;
                if (error <= 8) {
                    buenas++;
                } else if (error > peor) {
                    peor = error;
                }
            }
        }
    }
    printf("      [%dx%d] con ruido de +-10: %d de %d corrimientos (1, 10, 40, 80 px a los dos lados) con error de medio pixel o menos\n", ancho, alto, buenas, total);
    snprintf(nombre, sizeof nombre, "[%dx%d] con ruido de +-10 por canal: al menos 95 %% de los corrimientos de 1, 10, 40 y 80 px salen con error de 0.5 px o menos", ancho, alto);
    caso(nombre, buenas * 100 / total >= 95);

    medir(ancho, alto, 300, 80, 10, 0, &r);
    snprintf(nombre, sizeof nombre, "[%dx%d] con ruido: el corrimiento de 80 px (un tercio del ancho en 240) sigue siendo valido", ancho, alto);
    caso(nombre, r.valido);
}

static void pruebas_luz_y_piso(void)
{
    MovimientoVisual r;

    medir(240, 176, 300, 10, 4, 40, &r);
    caso("cambio de luz entre cuadros (+40 en todo el cuadro): el corrimiento de 10 px se encuentra igual", r.valido && valor_absoluto(r.corrimiento_x16 + 160) <= 8);
    medir(240, 176, 300, -25, 4, -30, &r);
    caso("cambio de luz (-30) y corrimiento de -25 px: se encuentra igual", r.valido && valor_absoluto(r.corrimiento_x16 - 400) <= 8);
    medir(240, 176, 300, 0, 0, 0, &r);
    caso("dos cuadros iguales (el carro quieto): corrimiento 0 y giro 0", r.valido && r.corrimiento_x16 == 0 && mv_corrimiento_a_grados_x10(r.corrimiento_x16, 240, 600) == 0);
}

/* ── Sin confianza ─────────────────────────────────────────────────────── */

static void pruebas_sin_confianza(void)
{
    MovimientoVisual r;
    int huella_a[MV_MAX_ANCHO], huella_b[MV_MAX_ANCHO];
    int x, y, i, invalidos = 0;

    /* Escena casi uniforme (una pared lisa) con un poco de ruido. */
    for (y = 0; y < 176; y++) {
        for (x = 0; x < 240; x++) {
            int c;

            for (c = 0; c < 3; c++) {
                cuadro[(y * 240 + x) * 3 + c] = limitar(120 + (int)(aleatorio() % 5u) - 2);
            }
        }
    }
    mv_huella(cuadro, 240, 176, huella_a);
    for (y = 0; y < 176; y++) {
        for (x = 0; x < 240; x++) {
            int c;

            for (c = 0; c < 3; c++) {
                cuadro[(y * 240 + x) * 3 + c] = limitar(120 + (int)(aleatorio() % 5u) - 2);
            }
        }
    }
    mv_huella(cuadro, 240, 176, huella_b);
    mv_corrimiento(huella_a, huella_b, 240, 96, &r);
    caso("escena casi uniforme (pared lisa con ruido de +-2): SIN CONFIANZA (escena uniforme)", !r.valido && r.motivo == MV_ESCENA_UNIFORME);
    printf("      escena uniforme: energia=%d (minimo %d)\n", r.energia, MV_ENERGIA_MIN);

    /* Escena totalmente uniforme sin ruido. */
    memset(cuadro, 100, sizeof cuadro);
    mv_huella(cuadro, 240, 176, huella_a);
    mv_corrimiento(huella_a, huella_a, 240, 96, &r);
    caso("escena perfectamente uniforme: SIN CONFIANZA", !r.valido && r.motivo == MV_ESCENA_UNIFORME);

    /* Escena uniforme con una sola mancha pequeña: casi sin bordes. */
    for (y = 0; y < 176; y++) {
        for (x = 0; x < 240; x++) {
            int c;

            for (c = 0; c < 3; c++) {
                cuadro[(y * 240 + x) * 3 + c] = limitar(120 + (int)(aleatorio() % 5u) - 2 + ((x >= 100 && x < 106) ? 20 : 0));
            }
        }
    }
    mv_huella(cuadro, 240, 176, huella_a);
    mv_corrimiento(huella_a, huella_a, 240, 96, &r);
    caso("pared lisa con una sola marca de 6 px: no alcanza (SIN CONFIANZA)", !r.valido);

    /* Textura que se repite (rayas cada 16 px): varios corrimientos igual de buenos. */
    for (y = 0; y < 176; y++) {
        for (x = 0; x < 240; x++) {
            int c;
            int brillo = ((x / 8) % 2) ? 200 : 60;

            for (c = 0; c < 3; c++) {
                cuadro[(y * 240 + x) * 3 + c] = limitar(brillo + (int)(aleatorio() % 5u) - 2);
            }
        }
    }
    mv_huella(cuadro, 240, 176, huella_a);
    for (y = 0; y < 176; y++) {
        for (x = 0; x < 240; x++) {
            int c;
            int brillo = (((x + 21) / 8) % 2) ? 200 : 60;

            for (c = 0; c < 3; c++) {
                cuadro[(y * 240 + x) * 3 + c] = limitar(brillo + (int)(aleatorio() % 5u) - 2);
            }
        }
    }
    mv_huella(cuadro, 240, 176, huella_b);
    mv_corrimiento(huella_a, huella_b, 240, 96, &r);
    caso("rayas que se repiten cada 16 px: SIN CONFIANZA (ambiguo)", !r.valido && (r.motivo == MV_AMBIGUO || r.motivo == MV_COINCIDENCIA_DEBIL));
    printf("      rayas repetidas: motivo=%d confianza=%d\n", r.motivo, r.confianza_mil);

    /* Dos escenas sin relación entre sí. */
    for (i = 0; i < 40; i++) {
        semilla = 7000u + (unsigned int)i * 31u;
        fabricar_panorama();
        tomar_cuadro(240, 176, 100, 4, 0);
        mv_huella(cuadro, 240, 176, huella_a);
        semilla = 9000u + (unsigned int)i * 17u;
        fabricar_panorama();
        tomar_cuadro(240, 176, 100, 4, 0);
        mv_huella(cuadro, 240, 176, huella_b);
        mv_corrimiento(huella_a, huella_b, 240, 96, &r);
        if (!r.valido) {
            invalidos++;
        }
    }
    printf("      escenas sin relacion: %d de 40 pares dan sin confianza\n", invalidos);
    caso("dos escenas SIN RELACION entre si: al menos 90 % de los pares dan sin confianza", invalidos * 100 / 40 >= 90);
    semilla = 99u;
    fabricar_panorama();
}

/* ── Grados ────────────────────────────────────────────────────────────── */

static void pruebas_grados(void)
{
    caso("conversion: contenido 10 px a la IZQUIERDA (-160 x16) en 240 px con FOV 60 = giro de +2.5 grados a la derecha (+25)",
         mv_corrimiento_a_grados_x10(-160, 240, 600) == 25);
    caso("conversion: contenido 10 px a la DERECHA = giro de -2.5 grados (a la izquierda)", mv_corrimiento_a_grados_x10(160, 240, 600) == -25);
    caso("conversion con 320 px de ancho: 40 px a la izquierda = 7.5 grados (75)", mv_corrimiento_a_grados_x10(-640, 320, 600) == 75);
    caso("conversion con FOV de 70 grados (700): 80 px en 240 = 23.3 grados (233)", mv_corrimiento_a_grados_x10(-1280, 240, 700) == 233);
    caso("conversion: el FOV escala el resultado (el doble de FOV, el doble de grados)", mv_corrimiento_a_grados_x10(-160, 240, 1200) == 2 * mv_corrimiento_a_grados_x10(-160, 240, 600));
    caso("conversion: corrimiento 0 = 0 grados; ancho 0 = 0 (no divide entre cero)", mv_corrimiento_a_grados_x10(0, 240, 600) == 0 && mv_corrimiento_a_grados_x10(-160, 0, 600) == 0);
}

/* ── Procesar cuadro a cuadro y errores ────────────────────────────────── */

static void pruebas_procesar(void)
{
    EstimadorMovimiento est;
    MovimientoVisual r;
    int codigo;

    mv_iniciar(&est);
    tomar_cuadro(240, 176, 300, 4, 0);
    codigo = mv_procesar(&est, cuadro, 240, 176, 600, &r);
    caso("primer cuadro: MV_OK pero SIN comparar (MV_SIN_PREVIO)", codigo == MV_OK && !r.valido && r.motivo == MV_SIN_PREVIO);
    tomar_cuadro(240, 176, 310, 4, 0);
    codigo = mv_procesar(&est, cuadro, 240, 176, 600, &r);
    caso("segundo cuadro (la ventana 10 px a la derecha): corrimiento -10 px y giro de +2.5 grados", codigo == MV_OK && r.valido &&
         valor_absoluto(r.corrimiento_x16 + 160) <= 4 && valor_absoluto(r.giro_grados_x10 - 25) <= 1);
    tomar_cuadro(320, 240, 310, 4, 0);
    codigo = mv_procesar(&est, cuadro, 320, 240, 600, &r);
    caso("un cuadro de otro ancho vuelve a empezar (SIN_PREVIO), no compara columnas distintas", codigo == MV_OK && !r.valido && r.motivo == MV_SIN_PREVIO);

    caso("rgb NULL: MV_ERR_ARGUMENTO", mv_procesar(&est, NULL, 240, 176, 600, &r) == MV_ERR_ARGUMENTO);
    caso("estimador NULL: MV_ERR_ARGUMENTO", mv_procesar(NULL, cuadro, 240, 176, 600, &r) == MV_ERR_ARGUMENTO);
    caso("salida NULL: MV_ERR_ARGUMENTO", mv_procesar(&est, cuadro, 240, 176, 600, NULL) == MV_ERR_ARGUMENTO);
    caso("ancho 20 (menos de 30): MV_ERR_TAMANO", mv_procesar(&est, cuadro, 20, 176, 600, &r) == MV_ERR_TAMANO);
    caso("ancho 700 (mas de 640): MV_ERR_TAMANO", mv_procesar(&est, cuadro, 700, 176, 600, &r) == MV_ERR_TAMANO);
    caso("alto 1: MV_ERR_TAMANO", mv_procesar(&est, cuadro, 240, 1, 600, &r) == MV_ERR_TAMANO);
    caso("mv_corrimiento con huellas NULL: MV_ERR_ARGUMENTO", mv_corrimiento(NULL, NULL, 240, 96, &r) == MV_ERR_ARGUMENTO);
    {
        int h[MV_MAX_ANCHO];
        int i;

        for (i = 0; i < MV_MAX_ANCHO; i++) {
            h[i] = i * 7 % 100;
        }
        caso("mv_corrimiento con n = 10: MV_ERR_TAMANO", mv_corrimiento(h, h, 10, 4, &r) == MV_ERR_TAMANO);
        caso("mv_huella con salida NULL: MV_ERR_ARGUMENTO", mv_huella(cuadro, 240, 176, NULL) == MV_ERR_ARGUMENTO);
    }
}

/* ── Un giro de 90 grados cuadro a cuadro ──────────────────────────────── */

/* Panorama CIRCULAR de 360 grados donde 1 columna = 0.25 grados (el mismo supuesto de FOV 60 en 240 px). Con el carro
 * girando `grados_por_cuadro` por cuadro, se suman los giros estimados y se compara con lo real. */
static void pruebas_giro_acumulado(void)
{
    /* ventana del panorama grande: 360 grados / 0.25 = 1440 columnas; usamos el panorama de 1400 + margen: giro de hasta ~80 grados */
    EstimadorMovimiento est;
    MovimientoVisual r;
    int cuadros, total_x10, validos, i;
    int grados_por_cuadro_x10;
    int origen;

    /* 90 grados a 7.5 grados por cuadro = 12 cuadros = 30 px de panorama por cuadro: se necesitan 360 px + 240 de ventana. */
    mv_iniciar(&est);
    grados_por_cuadro_x10 = 75;
    total_x10 = 0;
    validos = 0;
    cuadros = 12;
    for (i = 0; i <= cuadros; i++) {
        origen = 200 + i * 30;
        tomar_cuadro(240, 176, origen, 8, 0);
        mv_procesar(&est, cuadro, 240, 176, 600, &r);
        if (i > 0 && r.valido) {
            total_x10 += r.giro_grados_x10;
            validos++;
        }
    }
    printf("      giro de 90 grados en 12 cuadros de 7.5 grados: suma estimada %d.%d grados con %d cuadros validos\n", total_x10 / 10, valor_absoluto(total_x10 % 10), validos);
    caso("giro de 90 grados (12 cuadros de 7.5 grados, 30 px cada uno): la suma estimada queda entre 88 y 92 grados", validos == 12 && total_x10 >= 880 && total_x10 <= 920);
    (void)grados_por_cuadro_x10;

    /* Con ruido fuerte y un cuadro intermedio sin confianza (se pierde): la suma queda menor (hay que extrapolar por tiempo). */
    mv_iniciar(&est);
    total_x10 = 0;
    validos = 0;
    for (i = 0; i <= 12; i++) {
        origen = 200 + i * 30;
        tomar_cuadro(240, 176, origen, 8, 0);
        if (i == 6) {
            memset(cuadro, 100, 240 * 88 * 3);   /* un cuadro tapado / borroso: pared lisa arriba */
        }
        mv_procesar(&est, cuadro, 240, 176, 600, &r);
        if (i > 0 && r.valido) {
            total_x10 += r.giro_grados_x10;
            validos++;
        }
    }
    printf("      con un cuadro tapado: %d.%d grados con %d cuadros validos de 12\n", total_x10 / 10, valor_absoluto(total_x10 % 10), validos);
    caso("con un cuadro tapado se pierden los 2 giros que lo tocan (no se suman: sin confianza)", validos == 10 && total_x10 < 800);
}

int main(void)
{
    fabricar_panorama();
    pruebas_corrimientos(240, 176);
    pruebas_corrimientos(320, 240);
    pruebas_con_ruido(240, 176);
    pruebas_con_ruido(320, 240);
    pruebas_luz_y_piso();
    pruebas_sin_confianza();
    pruebas_grados();
    pruebas_procesar();
    pruebas_giro_acumulado();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
