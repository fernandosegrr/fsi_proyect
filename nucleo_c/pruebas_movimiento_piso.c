/*
 * pruebas_movimiento_piso.c — Pruebas en PC del flujo del piso (movimiento_piso.c) y de su entrada opcional en la fusión (fusion_aplicar_flujo).
 *
 * GUÍA DE LECTURA
 *  1. Un "mundo" simulado con trazado de rayos: la cámara (altura, inclinación y campo de visión del corredor) mira un piso plano (baldosas de
 *     2 colores, damero, liso o negro con ruido) y, si se pide, una CAJA con textura parada sobre el piso a cierta distancia. Se dibujan dos cuadros:
 *     uno con la cámara en el origen y otro con la cámara adelantada lo que avanza el carro en un cuadro. Así se prueba la geometría completa (no solo
 *     que el código sea coherente consigo mismo).
 *  2. Pruebas de la geometría esperada (flujo_esperado).
 *  3. Sin obstáculo: baldosas de 2 colores y damero se mueven como el piso (nunca "cerca"); el piso liso y el negro con ruido son "sin dato".
 *  4. Con obstáculo: una caja con textura que se acerca da bloques "cerca" en la zona central; una caja lisa no da dato (límite conocido).
 *  5. Cuándo NO se evalúa: apagado, carro quieto, cuadros muy seguidos o muy separados.
 *  6. La entrada opcional de la fusión: el color marca un obstáculo lejano y el flujo dice "piso con textura consistente" → baja la severidad;
 *     nunca baja una cercanía media o alta ni frena por su cuenta.
 *  Se compila con -lm (el generador de pruebas usa senos y tangentes; el módulo no).
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "movimiento_piso.h"
#include "fusion.h"

#define ANCHO 240
#define ALTO  176

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

/* ── El mundo simulado ─────────────────────────────────────────────────── */

#define PISO_LISO      0
#define PISO_BALDOSAS  1   /* baldosas de 10 cm, dos grises */
#define PISO_DAMERO    2   /* cuadros de 5 cm */
#define PISO_NEGRO     3   /* casi negro con ruido */

typedef struct {
    int piso;
    int sigma;              /* ruido por píxel */
    int hay_caja;
    double caja_d_cm;       /* distancia de la cara frontal de la caja a la cámara en el primer cuadro */
    double caja_ancho_cm;
    double caja_alto_cm;
    int caja_lisa;          /* 1 = la caja es de un solo color (sin textura) */
    unsigned int semilla;
} Mundo;

static unsigned int mezcla(unsigned int x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static int gris_del_piso(const Mundo *m, double x, double y)
{
    int ix = (int)floor(x / (m->piso == PISO_DAMERO ? 5.0 : 10.0));
    int iy = (int)floor(y / (m->piso == PISO_DAMERO ? 5.0 : 10.0));

    switch (m->piso) {
    case PISO_BALDOSAS:
    case PISO_DAMERO:
        return (((ix + iy) & 1) == 0) ? 70 : 150;
    case PISO_NEGRO:
        return 14;
    default:
        return 110;
    }
}

/* Valor al azar (0 a 255) de un punto de la retícula de la textura de la caja. */
static double punto_de_reticula(int iy, int iz)
{
    return (double)(mezcla((unsigned int)(iy * 7919 + iz * 104729 + 12345)) % 200) + 30.0;
}

/* Textura de la caja: ruido suave (interpolado) con una mancha cada 1.6 cm, como la de un objeto real (nada de saltos de un píxel a otro). */
static int gris_de_la_caja(const Mundo *m, double y, double z)
{
    double gy = y / 1.6, gz = z / 1.6, fy, fz, a, b, c, d;
    int iy = (int)floor(gy), iz = (int)floor(gz);

    if (m->caja_lisa) {
        return 125;
    }
    fy = gy - iy;
    fz = gz - iz;
    fy = fy * fy * (3.0 - 2.0 * fy);   /* suavizado */
    fz = fz * fz * (3.0 - 2.0 * fz);
    a = punto_de_reticula(iy, iz);
    b = punto_de_reticula(iy + 1, iz);
    c = punto_de_reticula(iy, iz + 1);
    d = punto_de_reticula(iy + 1, iz + 1);
    return (int)((a * (1 - fy) + b * fy) * (1 - fz) + (c * (1 - fy) + d * fy) * fz);
}

/* Dibuja la escena con la cámara adelantada `cam_x` cm. */
static void dibujar(const Mundo *m, const Corredor *c, double cam_x, unsigned int ruido, unsigned char *gris)
{
    double altura = c->config.altura_camara_cm;
    double inc = c->config.inclinacion_grados_x10 / 10.0 * 3.14159265358979 / 180.0;
    double seno = sin(inc), coseno = cos(inc);
    int u, v;

    for (v = 0; v < ALTO; v++) {
        for (u = 0; u < ANCHO; u++) {
            double xc = (u - c->cx) / (double)c->f;
            double yc = (v - c->cy) / (double)c->f;
            double dir_x = coseno - yc * seno, dir_y = xc, dir_z = -seno - yc * coseno;
            int valor = 0, hay = 0;
            double t_suelo = 1e18;

            if (dir_z < 0) {
                t_suelo = altura / -dir_z;
            }
            if (m->hay_caja && dir_x > 1e-9) {
                double t = (m->caja_d_cm - cam_x) / dir_x;
                double y = t * dir_y, z = altura + t * dir_z;

                if (t > 0 && t < t_suelo && fabs(y) <= m->caja_ancho_cm / 2 && z >= 0 && z <= m->caja_alto_cm) {
                    valor = gris_de_la_caja(m, y, z);
                    hay = 1;
                }
            }
            if (!hay && dir_z < 0) {
                valor = gris_del_piso(m, cam_x + t_suelo * dir_x, t_suelo * dir_y);
                hay = 1;
            }
            if (!hay) {
                valor = 200;   /* cielo / pared lejana */
            }
            if (m->sigma > 0) {
                /* ruido aproximadamente gaussiano: suma de 4 valores uniformes */
                unsigned int h = mezcla((unsigned int)(v * ANCHO + u) * 2654435761U + ruido * 40503U + m->semilla);
                int r = (int)(h & 255) + (int)((h >> 8) & 255) + (int)((h >> 16) & 255) + (int)((h >> 24) & 255) - 510;
                valor += r * m->sigma / 148;   /* la suma de 4 uniformes (0 a 255) tiene sigma ~ 148 */
            }
            gris[v * ANCHO + u] = (unsigned char)(valor < 0 ? 0 : valor > 255 ? 255 : valor);
        }
    }
}

static Corredor preparar_corredor(void)
{
    ConfigCorredor cc;
    Corredor c;

    corredor_config_defecto(&cc);
    corredor_preparar(&c, &cc, ANCHO, ALTO);
    return c;
}

static ConfigFlujo config_activa(void)
{
    ConfigFlujo cf;

    flujo_config_defecto(&cf);
    cf.activo = 1;
    return cf;
}

/* Dos cuadros seguidos de un carro que avanza a 193 mm/s con 100 ms entre cuadros y el resultado del flujo. */
static ResultadoFlujo correr(const Mundo *m, unsigned int ruido, int dt_ms)
{
    static unsigned char a[ANCHO * ALTO], b[ANCHO * ALTO];
    Corredor c = preparar_corredor();
    ConfigFlujo cf = config_activa();
    ResultadoFlujo r;
    double avance_cm = cf.velocidad_mm_s * dt_ms / 10000.0;

    dibujar(m, &c, 0.0, ruido, a);
    dibujar(m, &c, avance_cm, ruido + 1000, b);
    flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 1, dt_ms, &r);
    return r;
}

/* ── 1. Geometría ──────────────────────────────────────────────────────── */

static void pruebas_geometria(void)
{
    Corredor c = preparar_corredor();
    int dx4, dy4, dy_arriba = 0, dy_abajo = 0, ok_monotono = 1, ultimo = 0, v;

    caso("un punto sobre el horizonte no es piso", !flujo_esperado(&c, 120, 5, 19, &dx4, &dy4));
    caso("un punto del piso a la mitad de la imagen es piso", flujo_esperado(&c, 120, 100, 19, &dx4, &dy4));
    caso("al avanzar el piso baja en la imagen (dy positivo)", dy4 > 0);
    flujo_esperado(&c, 120, 70, 19, &dx4, &dy_arriba);
    flujo_esperado(&c, 120, 110, 19, &dx4, &dy_abajo);
    caso("el piso de abajo se mueve MÁS que el de arriba", dy_abajo > dy_arriba);
    for (v = 60; v < 130; v += 4) {
        if (flujo_esperado(&c, 120, v, 19, &dx4, &dy4)) {
            if (dy4 < ultimo) {
                ok_monotono = 0;
            }
            ultimo = dy4;
        }
    }
    caso("el desplazamiento esperado crece (sin bajar) al bajar por la imagen", ok_monotono);
    flujo_esperado(&c, 40, 100, 19, &dx4, &dy4);
    caso("a la izquierda del centro el punto se corre hacia la izquierda (dx negativo)", dx4 < 0);
    flujo_esperado(&c, 200, 100, 19, &dx4, &dy4);
    caso("a la derecha del centro el punto se corre hacia la derecha (dx positivo)", dx4 > 0);
    flujo_esperado(&c, 120, 100, 19, &dx4, &dy4);
    caso("en el centro no hay corrimiento horizontal", dx4 == 0);
    caso("un avance enorme saca el punto de la imagen (no es evaluable)", !flujo_esperado(&c, 120, 100, 400, &dx4, &dy4));
    caso("sin avance el desplazamiento esperado es cero", flujo_esperado(&c, 120, 100, 0, &dx4, &dy4) && dy4 == 0 && dx4 == 0);
    caso("argumento nulo no rompe", !flujo_esperado(NULL, 120, 100, 19, &dx4, &dy4) && !flujo_esperado(&c, 120, 100, 19, NULL, &dy4));
}

/* ── 2. Sin obstáculo ──────────────────────────────────────────────────── */

static void pruebas_sin_obstaculo(void)
{
    Mundo m;
    ResultadoFlujo r;
    int seed, nunca_cerca = 1, siempre_piso = 1, z;

    memset(&m, 0, sizeof m);
    m.piso = PISO_BALDOSAS;
    r = correr(&m, 1, 100);
    caso("baldosas de 2 colores: se evalúa", r.evaluado && r.motivo == FLU_MOT_EVALUADO);
    caso("baldosas de 2 colores: hay bloques de piso con textura", r.total_piso >= 4);
    caso("baldosas de 2 colores: ningún bloque 'cerca'", r.total_cerca == 0);
    printf("      baldosas: piso %d  cerca %d  dudoso %d  sin dato %d  (filas %d a %d)\n", r.total_piso, r.total_cerca, r.total_dudoso, r.total_sin_dato,
           r.fila_inicial, r.fila_final - 1);

    for (seed = 0; seed < 12; seed++) {
        m.piso = (seed % 2) ? PISO_BALDOSAS : PISO_DAMERO;
        m.sigma = 2 + seed % 4;
        m.semilla = (unsigned int)seed * 77u;
        r = correr(&m, (unsigned int)seed, 80 + (seed % 5) * 10);
        if (r.total_cerca != 0) {
            nunca_cerca = 0;
        }
        if (r.total_piso == 0) {
            siempre_piso = 0;
        }
    }
    caso("12 variantes de baldosas/damero con ruido y distintos tiempos: nunca 'cerca'", nunca_cerca);
    caso("12 variantes de baldosas/damero con ruido: siempre algún bloque de piso", siempre_piso);

    memset(&m, 0, sizeof m);
    m.piso = PISO_DAMERO;
    r = correr(&m, 3, 100);
    caso("damero de 5 cm: no hay bloques 'cerca'", r.total_cerca == 0);
    printf("      damero: piso %d  cerca %d  dudoso %d  sin dato %d\n", r.total_piso, r.total_cerca, r.total_dudoso, r.total_sin_dato);
    for (z = 0; z < OBS_ZONAS; z++) {
        if (r.zona[z].hay_cerca) {
            nunca_cerca = 0;
        }
    }

    memset(&m, 0, sizeof m);
    m.piso = PISO_LISO;
    r = correr(&m, 4, 100);
    caso("piso liso: ningún bloque tiene dato (todo 'sin dato')", r.total_piso == 0 && r.total_cerca == 0 && r.total_dudoso == 0 && r.total_sin_dato > 0);

    nunca_cerca = 1;
    for (seed = 0; seed < 20; seed++) {
        memset(&m, 0, sizeof m);
        m.piso = PISO_NEGRO;
        m.sigma = 3 + seed % 3;
        m.semilla = (unsigned int)seed * 131u + 5u;
        r = correr(&m, (unsigned int)seed + 50u, 100);
        if (r.total_cerca != 0 || r.total_piso != 0) {
            nunca_cerca = 0;
        }
    }
    caso("piso negro con ruido (20 semillas, sigma 3 a 5): sin dato, nunca cerca ni piso inventado", nunca_cerca);
}

/* ── 3. Con obstáculo ──────────────────────────────────────────────────── */

static void pruebas_con_obstaculo(void)
{
    Mundo m;
    ResultadoFlujo r;
    CajaImagen cajas[16];
    int n, i, centro_ok = 1;
    double distancias[3] = { 20.0, 24.0, 30.0 };

    memset(&m, 0, sizeof m);
    m.piso = PISO_BALDOSAS;
    m.hay_caja = 1;
    m.caja_ancho_cm = 14.0;
    m.caja_alto_cm = 20.0;
    for (i = 0; i < 3; i++) {
        m.caja_d_cm = distancias[i];
        r = correr(&m, 7, 200);
        printf("      caja a %.0f cm: cerca %d (centro %d, izq %d, der %d)  piso %d  dudoso %d  sin dato %d  cercania centro %d\n", distancias[i], r.total_cerca,
               r.zona[ZONA_CENTRO].cerca, r.zona[ZONA_IZQUIERDA].cerca, r.zona[ZONA_DERECHA].cerca, r.total_piso, r.total_dudoso, r.total_sin_dato,
               r.zona[ZONA_CENTRO].cercania);
        if (r.total_cerca < 2 || !r.zona[ZONA_CENTRO].hay_cerca) {
            centro_ok = 0;
        }
    }
    caso("caja con textura a 20, 24 y 30 cm (cuadros separados 200 ms): al menos 2 bloques 'cerca' y alguno en la zona central", centro_ok);

    m.caja_d_cm = 24.0;
    r = correr(&m, 7, 200);
    caso("la caja con textura NO da 'piso consistente' en la zona central", !r.zona[ZONA_CENTRO].piso_consistente);
    n = flujo_cajas(&r, cajas, 16);
    caso("flujo_cajas da una caja por bloque 'cerca'", n == r.total_cerca && n >= 2);
    caso("las cajas son de 16x16, peso 1, fuente flujo", n > 0 && cajas[0].x1 - cajas[0].x0 == 16 && cajas[0].y1 - cajas[0].y0 == 16 && cajas[0].peso == 1 &&
                                                          cajas[0].fuente == COR_FUENTE_FLUJO);
    caso("flujo_cajas respeta el máximo", flujo_cajas(&r, cajas, 1) == 1);

    /* Caja con textura sobre piso liso y sobre piso negro con ruido. */
    m.piso = PISO_LISO;
    r = correr(&m, 8, 200);
    caso("caja con textura sobre piso liso: hay 'cerca' en el centro", r.zona[ZONA_CENTRO].cerca >= 1);
    m.piso = PISO_NEGRO;
    m.sigma = 4;
    r = correr(&m, 9, 200);
    printf("      caja sobre piso negro con ruido: cerca %d (centro %d)  piso %d  dudoso %d  sin dato %d\n", r.total_cerca, r.zona[ZONA_CENTRO].cerca, r.total_piso, r.total_dudoso, r.total_sin_dato);
    caso("caja con textura sobre piso negro con ruido (sigma 4): hay 'cerca' en el centro", r.zona[ZONA_CENTRO].cerca >= 1);

    /* Límite conocido: una caja de un solo color no tiene textura, el flujo no la ve (para eso está el color). */
    m.piso = PISO_BALDOSAS;
    m.sigma = 0;
    m.caja_lisa = 1;
    r = correr(&m, 10, 200);
    printf("      caja lisa: cerca %d, piso %d, sin dato %d (solo bordes dan dato)\n", r.total_cerca, r.total_piso, r.total_sin_dato);
    caso("caja lisa: el flujo NO la toma por piso consistente en el centro (sus bordes la delatan o queda sin dato)", !r.zona[ZONA_CENTRO].piso_consistente);
}

/* ── 4. Cuándo no se evalúa ────────────────────────────────────────────── */

static void pruebas_no_evaluar(void)
{
    static unsigned char a[ANCHO * ALTO], b[ANCHO * ALTO];
    Corredor c = preparar_corredor();
    ConfigFlujo cf = config_activa();
    ResultadoFlujo r;
    Mundo m;

    memset(&m, 0, sizeof m);
    m.piso = PISO_BALDOSAS;
    dibujar(&m, &c, 0, 1, a);
    dibujar(&m, &c, 2, 2, b);

    cf.activo = 0;
    caso("apagado: devuelve OK y no evalúa (motivo apagado)", flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 1, 100, &r) == FLU_OK && !r.evaluado && r.motivo == FLU_MOT_APAGADO);
    cf.activo = 1;
    caso("el carro no avanza: no evalúa (motivo quieto)", flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 0, 100, &r) == FLU_OK && !r.evaluado && r.motivo == FLU_MOT_QUIETO);
    cf.velocidad_mm_s = 0;
    caso("velocidad cero: no evalúa", flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 1, 100, &r) == FLU_OK && !r.evaluado && r.motivo == FLU_MOT_QUIETO);
    cf.velocidad_mm_s = 193;
    caso("cuadros muy seguidos (20 ms): no evalúa (motivo tiempo)", flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 1, 20, &r) == FLU_OK && !r.evaluado && r.motivo == FLU_MOT_TIEMPO);
    caso("cuadros muy separados (800 ms): no evalúa (motivo tiempo)", flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 1, 800, &r) == FLU_OK && !r.evaluado && r.motivo == FLU_MOT_TIEMPO);
    caso("en condiciones normales sí evalúa", flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 1, 100, &r) == FLU_OK && r.evaluado);
    caso("sin evaluar, ninguna zona dice 'piso consistente' ni 'cerca'", (cf.activo = 0, flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 1, 100, &r), r.zona[0].piso_consistente == 0 && r.zona[1].hay_cerca == 0));
    cf.activo = 1;
    caso("puntero nulo: error de argumento", flujo_analizar(&cf, &c, NULL, b, ANCHO, ALTO, 1, 100, &r) == FLU_ERR_ARGUMENTO);
    caso("resultado nulo: error de argumento", flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 1, 100, NULL) == FLU_ERR_ARGUMENTO);
    caso("tamaño distinto del corredor: error de argumento", flujo_analizar(&cf, &c, a, b, 200, 100, 1, 100, &r) == FLU_ERR_ARGUMENTO);
    cf.ventana_px = 0;
    caso("configuración no válida: error de configuración", flujo_analizar(&cf, &c, a, b, ANCHO, ALTO, 1, 100, &r) == FLU_ERR_CONFIG);
    flujo_config_defecto(&cf);
    caso("la configuración por defecto es válida y está APAGADA", flujo_config_es_valida(&cf) && cf.activo == 0);
    caso("gris de RGB: blanco 255, negro 0", (flujo_gris_de_rgb((const unsigned char[]){ 255, 255, 255, 0, 0, 0 }, 2, 1, a), a[0] >= 254 && a[1] == 0));
}

/* ── 5. Entrada opcional de la fusión ──────────────────────────────────── */

static ZonaFlujo zona_flujo(int piso, int cerca, int dudoso)
{
    ZonaFlujo z;

    memset(&z, 0, sizeof z);
    z.piso = piso;
    z.cerca = cerca;
    z.dudoso = dudoso;
    z.hay_cerca = cerca > 0;
    z.piso_consistente = (piso + cerca >= 3 && cerca == 0 && dudoso * 2 <= piso + cerca) ? 1 : 0;
    return z;
}

static ResultadoObstaculos detector_con(int zona, int cercania, int celdas)
{
    ResultadoObstaculos d;

    memset(&d, 0, sizeof d);
    d.zona[zona].hay_obstaculo = 1;
    d.zona[zona].cercania = cercania;
    d.zona[zona].celdas = celdas;
    return d;
}

static void pruebas_fusion(void)
{
    ConfigFusion cfg;
    ResultadoObstaculos det;
    ResultadoFusion sal;
    ZonaFlujo flujo[OBS_ZONAS];

    fusion_config_defecto(&cfg);

    /* El color marca un obstáculo lejano (cercanía 1) en el centro de un piso de 2 colores: el flujo dice piso consistente. */
    det = detector_con(ZONA_CENTRO, 1, 12);
    fusion_decidir(&cfg, &det, NULL, 1000, &sal);
    caso("(sin flujo) el color con cercanía 1 queda como aviso de severidad 1", sal.severidad == FUS_SEV_AVISO && sal.zona[ZONA_CENTRO].hay_obstaculo);
    flujo[0] = zona_flujo(0, 0, 0);
    flujo[1] = zona_flujo(6, 0, 1);
    flujo[2] = zona_flujo(0, 0, 0);
    caso("fusion_aplicar_flujo devuelve OK", fusion_aplicar_flujo(&cfg, flujo, 1, &sal) == FUS_OK);
    caso("color cercanía 1 + piso consistente: se baja (la zona queda libre)", !sal.zona[ZONA_CENTRO].hay_obstaculo && sal.zona[ZONA_CENTRO].cercania == 0);
    caso("... y se cuenta en 'bajados por el flujo'", sal.bajados_flujo == 1);
    caso("... queda solo un aviso (severidad aviso), no frena", sal.aviso && sal.severidad == FUS_SEV_AVISO && sal.zona[ZONA_CENTRO].hay_obstaculo == 0);

    /* Cercanía media o alta: el color manda, el flujo no baja nada. */
    det = detector_con(ZONA_CENTRO, 2, 12);
    fusion_decidir(&cfg, &det, NULL, 1000, &sal);
    fusion_aplicar_flujo(&cfg, flujo, 1, &sal);
    caso("color cercanía 2 + piso consistente: NO se baja (el color manda)", sal.zona[ZONA_CENTRO].hay_obstaculo && sal.zona[ZONA_CENTRO].cercania == 2 && sal.bajados_flujo == 0);
    det = detector_con(ZONA_CENTRO, 3, 20);
    fusion_decidir(&cfg, &det, NULL, 1000, &sal);
    fusion_aplicar_flujo(&cfg, flujo, 1, &sal);
    caso("color cercanía 3 + piso consistente: NO se baja", sal.zona[ZONA_CENTRO].hay_obstaculo && sal.severidad == FUS_SEV_PARAR);

    /* Si el flujo no es consistente (hay 'cerca' o no hay datos) tampoco se baja. */
    det = detector_con(ZONA_CENTRO, 1, 12);
    fusion_decidir(&cfg, &det, NULL, 1000, &sal);
    flujo[1] = zona_flujo(2, 1, 0);
    fusion_aplicar_flujo(&cfg, flujo, 1, &sal);
    caso("color cercanía 1 + flujo con un bloque 'cerca': NO se baja", sal.zona[ZONA_CENTRO].hay_obstaculo && sal.bajados_flujo == 0);
    fusion_decidir(&cfg, &det, NULL, 1000, &sal);
    flujo[1] = zona_flujo(1, 0, 0);
    fusion_aplicar_flujo(&cfg, flujo, 1, &sal);
    caso("color cercanía 1 + muy pocos bloques de piso (sin dato): NO se baja", sal.zona[ZONA_CENTRO].hay_obstaculo && sal.bajados_flujo == 0);
    fusion_decidir(&cfg, &det, NULL, 1000, &sal);
    flujo[1] = zona_flujo(6, 0, 1);
    caso("flujo no vigente (cuadro sin evaluar): no cambia nada", fusion_aplicar_flujo(&cfg, flujo, 0, &sal) == FUS_OK && sal.zona[ZONA_CENTRO].hay_obstaculo && sal.bajados_flujo == 0);

    /* Una zona distinta no se toca. */
    fusion_decidir(&cfg, &det, NULL, 1000, &sal);
    flujo[1] = zona_flujo(0, 0, 0);
    flujo[0] = zona_flujo(6, 0, 0);
    fusion_aplicar_flujo(&cfg, flujo, 1, &sal);
    caso("el flujo consistente de OTRA zona no baja el obstáculo del centro", sal.zona[ZONA_CENTRO].hay_obstaculo);

    /* El flujo nunca frena por su cuenta: 'cerca' sin color = solo aviso. */
    memset(&det, 0, sizeof det);
    fusion_decidir(&cfg, &det, NULL, 1000, &sal);
    flujo[1] = zona_flujo(1, 4, 0);
    flujo[1].cercania = 3;
    fusion_aplicar_flujo(&cfg, flujo, 1, &sal);
    caso("el flujo ve 'cerca' y el color no ve nada: solo AVISO, nunca frena", sal.aviso && !sal.zona[ZONA_CENTRO].hay_obstaculo && sal.severidad == FUS_SEV_AVISO);

    /* Si un modelo ya confirmó el obstáculo (fuente ambos), el flujo no lo baja. */
    {
        DeteccionesModelo dm;
        memset(&dm, 0, sizeof dm);
        dm.marca_ms = 1000;
        dm.n = 1;
        dm.cajas[0].clase = 3;
        dm.cajas[0].confianza_mil = 900;
        dm.cajas[0].xmin_mil = 350;
        dm.cajas[0].xmax_mil = 650;
        dm.cajas[0].ymin_mil = 400;
        dm.cajas[0].ymax_mil = 600;
        det = detector_con(ZONA_CENTRO, 1, 12);
        fusion_decidir(&cfg, &det, &dm, 1000, &sal);
        flujo[1] = zona_flujo(6, 0, 0);
        fusion_aplicar_flujo(&cfg, flujo, 1, &sal);
        caso("si el modelo también ve el objeto (fuente ambos o modelo): el flujo no baja nada", sal.zona[ZONA_CENTRO].hay_obstaculo && sal.bajados_flujo == 0);
    }
    caso("argumentos nulos: error", fusion_aplicar_flujo(NULL, flujo, 1, &sal) == FUS_ERR_ARGUMENTO && fusion_aplicar_flujo(&cfg, NULL, 1, &sal) == FUS_ERR_ARGUMENTO &&
                                    fusion_aplicar_flujo(&cfg, flujo, 1, NULL) == FUS_ERR_ARGUMENTO);
}

int main(void)
{
    pruebas_geometria();
    pruebas_sin_obstaculo();
    pruebas_con_obstaculo();
    pruebas_no_evaluar();
    pruebas_fusion();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
