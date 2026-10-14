/*
 * pruebas_corredor.c — Pruebas en PC de corredor.c (la huella del carro proyectada en la imagen).
 *
 * GUÍA DE LECTURA
 *  1. La geometría se comprueba contra una referencia INDEPENDIENTE escrita aquí con números decimales (double) y senos por serie de Taylor:
 *     se proyecta un punto del piso (x cm a un lado, d cm adelante) a la imagen con la cámara de agujero y se compara con lo que dice corredor.c
 *     (que usa enteros y una tabla de senos).
 *  2. Obstáculos DENTRO, en el BORDE y FUERA del corredor a varias distancias (20 a 120 cm) y con varios desplazamientos laterales.
 *  3. Las zonas: el detector de color (con la máscara y con una imagen sintética de verdad), cajas del modelo de objetos y bloques del flujo.
 *  4. Apagado por defecto: sin activarlo nada se filtra.
 *  5. Datos al azar: la clasificación cumple sus invariantes y no se sale de rango.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "corredor.h"
#include "obstaculos.h"

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

/* ── Referencia con double ─────────────────────────────────────────────── */

static double grados_a_radianes(double g)
{
    return g * 3.14159265358979323846 / 180.0;
}

static double seno_d(double x)
{
    double termino = x, suma = x;
    int k;

    for (k = 1; k < 14; k++) {
        termino *= -x * x / ((2.0 * k) * (2.0 * k + 1.0));
        suma += termino;
    }
    return suma;
}

static double coseno_d(double x)
{
    return seno_d(x + 1.57079632679489661923);
}

static double absoluto(double x)
{
    return (x < 0) ? -x : x;
}

/* La cámara de la referencia: altura h cm, inclinación (grados), campo de visión horizontal (grados), cuadro ancho x alto. */
typedef struct {
    double h, incl, fov;
    int ancho, alto;
    double f;
} Camara;

static Camara camara_de(const ConfigCorredor *c, int ancho, int alto)
{
    Camara cam;

    cam.h = c->altura_camara_cm;
    cam.incl = grados_a_radianes(c->inclinacion_grados_x10 / 10.0);
    cam.fov = grados_a_radianes(c->fov_grados_x10 / 10.0);
    cam.ancho = ancho;
    cam.alto = alto;
    cam.f = (ancho / 2.0) * coseno_d(cam.fov / 2.0) / seno_d(cam.fov / 2.0);
    return cam;
}

/* Proyecta el punto del piso (x cm a la derecha, d cm adelante): devuelve la columna u y la fila v (decimales). */
static void proyectar(const Camara *cam, double x, double d, double *u, double *v)
{
    double s = seno_d(cam->incl), c = coseno_d(cam->incl);
    double zc = d * c + cam->h * s;
    double yc = cam->h * c - d * s;

    *u = cam->ancho / 2.0 + cam->f * x / zc;
    *v = cam->alto / 2.0 + cam->f * yc / zc;
}

/* Distancia (cm) del piso que se ve en la fila v, por bisección sobre la proyección (independiente de la fórmula cerrada). */
static double distancia_de_fila_ref(const Camara *cam, double v)
{
    double bajo = 0.0, alto = 100000.0, u, vv;
    int i;

    for (i = 0; i < 100; i++) {
        double medio = (bajo + alto) / 2.0;

        proyectar(cam, 0.0, medio, &u, &vv);
        if (vv > v) {
            bajo = medio;    /* medio se proyecta más abajo que v: el piso de la fila v está más lejos */
        } else {
            alto = medio;
        }
    }
    return (bajo + alto) / 2.0;
}

/* ── Pruebas ───────────────────────────────────────────────────────────── */

static void pruebas_configuracion(void)
{
    ConfigCorredor c;
    Corredor cor;

    corredor_config_defecto(&c);
    caso("por defecto el corredor esta APAGADO", c.activo == 0);
    caso("por defecto: 18 cm de ancho, 10 cm de altura, 20 grados, 60 de campo de vision, 3 cm de margen, peso minimo 2",
         c.ancho_carro_cm == 18 && c.altura_camara_cm == 10 && c.inclinacion_grados_x10 == 200 && c.fov_grados_x10 == 600 &&
         c.margen_lateral_cm == 3 && c.min_peso == 2);
    caso("la configuracion por defecto es valida", corredor_config_es_valida(&c));
    caso("config NULL no es valida", !corredor_config_es_valida(NULL));

    c.ancho_carro_cm = 0;
    caso("ancho 0 no es valido", !corredor_config_es_valida(&c));
    corredor_config_defecto(&c); c.altura_camara_cm = 0;
    caso("altura 0 no es valida", !corredor_config_es_valida(&c));
    corredor_config_defecto(&c); c.inclinacion_grados_x10 = 900;
    caso("inclinacion de 90 grados no es valida", !corredor_config_es_valida(&c));
    corredor_config_defecto(&c); c.inclinacion_grados_x10 = -1;
    caso("inclinacion negativa no es valida", !corredor_config_es_valida(&c));
    corredor_config_defecto(&c); c.fov_grados_x10 = 100;
    caso("campo de vision de 10 grados no es valido", !corredor_config_es_valida(&c));
    corredor_config_defecto(&c); c.fov_grados_x10 = 1600;
    caso("campo de vision de 160 grados no es valido", !corredor_config_es_valida(&c));
    corredor_config_defecto(&c); c.margen_lateral_cm = -1;
    caso("margen negativo no es valido", !corredor_config_es_valida(&c));
    corredor_config_defecto(&c); c.min_peso = 0;
    caso("peso minimo 0 no es valido", !corredor_config_es_valida(&c));
    corredor_config_defecto(&c); c.activo = 2;
    caso("activo = 2 no es valido", !corredor_config_es_valida(&c));

    corredor_config_defecto(&c);
    caso("preparar con NULL: COR_ERR_ARGUMENTO", corredor_preparar(NULL, &c, 240, 176) == COR_ERR_ARGUMENTO && corredor_preparar(&cor, NULL, 240, 176) == COR_ERR_ARGUMENTO);
    caso("preparar con un cuadro de 20x20 o 700x100: COR_ERR_ARGUMENTO", corredor_preparar(&cor, &c, 20, 20) == COR_ERR_ARGUMENTO &&
         corredor_preparar(&cor, &c, 700, 100) == COR_ERR_ARGUMENTO);
    c.ancho_carro_cm = 0;
    caso("preparar con una configuracion mala: COR_ERR_CONFIG", corredor_preparar(&cor, &c, 240, 176) == COR_ERR_CONFIG);
    corredor_config_defecto(&c);
    caso("preparar con lo bueno: COR_OK, centro (120, 88)", corredor_preparar(&cor, &c, 240, 176) == COR_OK && cor.cx == 120 && cor.cy == 88);
    caso("distancia focal de 240 px con 60 grados: unos 208 px (+-1)", cor.f >= 207 && cor.f <= 209);
}

static void pruebas_perspectiva(void)
{
    ConfigCorredor c;
    Corredor cor;
    Camara cam;
    int v, ok_distancias = 1, ok_ancho = 1, ok_decrece = 1, anterior_ancho = 100000, anterior_distancia = -1;
    int worst_d = 0;

    corredor_config_defecto(&c);
    c.activo = 1;
    corredor_preparar(&cor, &c, 240, 176);
    cam = camara_de(&c, 240, 176);

    /* Distancia de cada fila contra la bisección sobre la proyección. */
    for (v = 100; v <= 176; v++) {
        double ref = distancia_de_fila_ref(&cam, (double)v);
        int d = corredor_distancia_de_fila(&cor, v);

        if (absoluto(d - ref) > 1.5 + 0.02 * ref) {
            ok_distancias = 0;
            printf("      fila %d: corredor.c %d cm, referencia %.1f cm\n", v, d, ref);
        }
        if (absoluto(d - ref) > worst_d) {
            worst_d = (int)absoluto(d - ref);
        }
    }
    caso("la distancia de cada fila (100 a 176) coincide con la referencia (+-1.5 cm +-2 %)", ok_distancias);

    /* La mitad del ancho de la huella (9 cm) en pixeles contra la referencia. */
    for (v = 40; v <= 176; v += 4) {
        double ref_d = distancia_de_fila_ref(&cam, (double)v);
        double u, vv;
        int mio;

        proyectar(&cam, 9.0, ref_d, &u, &vv);
        mio = corredor_semiancho_px(&cor, v, 18);
        if (absoluto(mio - (u - 120.0)) > 1.6) {
            ok_ancho = 0;
            printf("      fila %d: semiancho %d px, referencia %.1f px\n", v, mio, u - 120.0);
        }
    }
    caso("la mitad del ancho de la huella (9 cm) en cada fila coincide con la referencia (+-1.6 px)", ok_ancho);

    /* El corredor se estrecha con la distancia: de abajo hacia arriba el ancho baja y la distancia sube. */
    for (v = 176; v >= 20; v--) {
        int ancho = corredor_semiancho_px(&cor, v, 18);
        int d = corredor_distancia_de_fila(&cor, v);

        if (ancho > anterior_ancho || (d >= 0 && d < anterior_distancia)) {
            ok_decrece = 0;
        }
        anterior_ancho = ancho;
        anterior_distancia = d;
    }
    caso("subiendo por la imagen el corredor se estrecha y la distancia crece (sin excepciones, filas 176 a 20)", ok_decrece);

    printf("      corredor de 18 cm (semiancho en px): fila 176 -> %d, 150 -> %d, 120 -> %d, 100 -> %d, 80 -> %d, 60 -> %d, 40 -> %d\n",
           corredor_semiancho_px(&cor, 176, 18), corredor_semiancho_px(&cor, 150, 18), corredor_semiancho_px(&cor, 120, 18),
           corredor_semiancho_px(&cor, 100, 18), corredor_semiancho_px(&cor, 80, 18), corredor_semiancho_px(&cor, 60, 18),
           corredor_semiancho_px(&cor, 40, 18));
    printf("      distancia por fila (cm): 176 -> %d, 150 -> %d, 120 -> %d, 100 -> %d, 80 -> %d, 60 -> %d, 40 -> %d\n",
           corredor_distancia_de_fila(&cor, 176), corredor_distancia_de_fila(&cor, 150), corredor_distancia_de_fila(&cor, 120),
           corredor_distancia_de_fila(&cor, 100), corredor_distancia_de_fila(&cor, 80), corredor_distancia_de_fila(&cor, 60),
           corredor_distancia_de_fila(&cor, 40));

    /* El horizonte: tan(20) * 208 = 75.7 px arriba del centro (fila 12.3). */
    caso("en el horizonte (fila 12) y arriba no hay piso: distancia -1 y ancho 0", corredor_distancia_de_fila(&cor, 12) == -1 &&
         corredor_distancia_de_fila(&cor, 0) == -1 && corredor_semiancho_px(&cor, 12, 18) == 0);
    caso("justo debajo del horizonte (fila 14) el piso esta muy lejos (mas de 3 m)", corredor_distancia_de_fila(&cor, 14) > 300);
    caso("la distancia de una fila con cor NULL es -1", corredor_distancia_de_fila(NULL, 100) == -1 && corredor_semiancho_px(NULL, 100, 18) == 0);
}

/* Genera una caja de un objeto de `ancho` cm de ancho cuyo centro esta `lateral` cm a la derecha del eje y a `distancia` cm adelante. */
static CajaImagen caja_de_objeto(const Camara *cam, double lateral, double distancia, double ancho)
{
    CajaImagen caja;
    double u0, u1, v, descartada;

    proyectar(cam, lateral - ancho / 2.0, distancia, &u0, &v);
    proyectar(cam, lateral + ancho / 2.0, distancia, &u1, &descartada);
    caja.x0 = (int)(u0 + 0.5);
    caja.x1 = (int)(u1 + 0.5);
    caja.y1 = (int)(v + 0.5);
    caja.y0 = caja.y1 - 30;
    if (caja.y0 < 0) {
        caja.y0 = 0;
    }
    caja.peso = 1;
    caja.fuente = COR_FUENTE_MODELO;
    return caja;
}

static void pruebas_clasificacion(void)
{
    ConfigCorredor c;
    Corredor cor;
    Camara cam;
    static const double distancias[] = { 20.0, 30.0, 50.0, 80.0, 120.0 };
    static const double laterales[]  = { 0.0, 8.0, -8.0, 13.5, -13.5, 18.0, -18.0, 30.0, -30.0 };
    int i, j;
    int malos = 0, total = 0;
    int dentro_ok = 1, borde_ok = 1, fuera_ok = 1;

    corredor_config_defecto(&c);
    c.activo = 1;
    corredor_preparar(&cor, &c, 240, 176);
    cam = camara_de(&c, 240, 176);

    /* Objetos de 6 cm de ancho. El carro mide 18 cm (huella +-9 cm) y el margen es de 3 cm (corredor +-12 cm).
     * Centro 0   -> [-3, 3]    cruza la huella            : DENTRO
     * Centro +-8 -> [5, 11]    cruza la huella (5 a 9)    : DENTRO
     * Centro +-13.5 -> [10.5, 16.5] solo el margen (9 a 12): BORDE
     * Centro +-18 -> [15, 21]  fuera del margen           : FUERA
     * Centro +-30 -> [27, 33]  fuera                      : FUERA */
    for (i = 0; i < (int)(sizeof distancias / sizeof distancias[0]); i++) {
        for (j = 0; j < (int)(sizeof laterales / sizeof laterales[0]); j++) {
            CajaImagen caja = caja_de_objeto(&cam, laterales[j], distancias[i], 6.0);
            ResultadoCaja r;
            double centro = absoluto(laterales[j]);
            int esperado = (centro <= 8.0) ? COR_DENTRO : (centro <= 13.5) ? COR_BORDE : COR_FUERA;

            total++;
            if (corredor_clasificar(&cor, &caja, &r) != COR_OK || r.estado != esperado) {
                malos++;
                if (esperado == COR_DENTRO) dentro_ok = 0;
                if (esperado == COR_BORDE) borde_ok = 0;
                if (esperado == COR_FUERA) fuera_ok = 0;
                printf("      a %.0f cm, lateral %.1f cm: esperado %s, corredor.c dice %s (caja x %d..%d, y1 %d, semiancho huella %d, corredor %d)\n",
                       distancias[i], laterales[j], corredor_texto_estado(esperado), corredor_texto_estado(r.estado), caja.x0, caja.x1, caja.y1,
                       r.semiancho_huella_px, r.semiancho_corredor_px);
            }
        }
    }
    caso("objetos de 6 cm DENTRO de la huella (centro 0 y +-8 cm) a 20, 30, 50, 80 y 120 cm: todos DENTRO", dentro_ok);
    caso("objetos en el BORDE (solo el margen, centro +-13.5 cm) a 20, 30, 50, 80 y 120 cm: todos BORDE (en la duda, frena)", borde_ok);
    caso("objetos FUERA (centro +-18 y +-30 cm) a 20, 30, 50, 80 y 120 cm: todos FUERA (solo avisan)", fuera_ok);
    printf("      %d de %d clasificaciones coinciden con lo esperado\n", total - malos, total);

    /* La distancia que reporta es la del borde de abajo de la caja. */
    {
        CajaImagen caja = caja_de_objeto(&cam, 0.0, 50.0, 6.0);
        ResultadoCaja r;

        corredor_clasificar(&cor, &caja, &r);
        caso("un objeto a 50 cm reporta una distancia de 50 cm (+-4)", r.distancia_cm >= 46 && r.distancia_cm <= 54);
        printf("      objeto a 50 cm: distancia %d cm, semiancho huella %d px, corredor %d px\n", r.distancia_cm, r.semiancho_huella_px, r.semiancho_corredor_px);
    }

    /* El mismo desplazamiento lateral (11 cm): cerca esta DENTRO del margen y... lejos tambien; pero a 4 m del eje (60 cm) esta FUERA aun cerca. */
    {
        CajaImagen cerca_lado = caja_de_objeto(&cam, 40.0, 20.0, 6.0);
        ResultadoCaja r;

        corredor_clasificar(&cor, &cerca_lado, &r);
        caso("un objeto a 40 cm a un lado y a 20 cm adelante esta FUERA (pasa por un lado)", r.estado == COR_FUERA);
    }

    /* Caja sobre el horizonte. */
    {
        CajaImagen alta = { 100, 0, 140, 10, 1, COR_FUENTE_MODELO };
        ResultadoCaja r;

        corredor_clasificar(&cor, &alta, &r);
        caso("una caja con el borde de abajo sobre el horizonte: FUERA, sobre_horizonte = 1 y distancia -1", r.estado == COR_FUERA && r.sobre_horizonte == 1 && r.distancia_cm == -1);
    }

    /* Caja que se sale por abajo del cuadro. */
    {
        CajaImagen grande = { 60, 100, 180, 230, 1, COR_FUENTE_MODELO };
        ResultadoCaja r;

        corredor_clasificar(&cor, &grande, &r);
        caso("una caja que sale por abajo del cuadro (y1 = 230) se trata con la ultima fila: DENTRO", r.estado == COR_DENTRO && r.distancia_cm > 0 && r.distancia_cm < 15);
    }
    caso("clasificar con NULL: COR_ERR_ARGUMENTO", corredor_clasificar(NULL, NULL, NULL) == COR_ERR_ARGUMENTO);

    /* Con otra camara (mas alta, mas inclinada, mas ancha) la geometria sigue coincidiendo con la referencia. */
    {
        ConfigCorredor c2;
        Corredor cor2;
        Camara cam2;
        int malos2 = 0, total2 = 0;

        corredor_config_defecto(&c2);
        c2.activo = 1;
        c2.altura_camara_cm = 15;
        c2.inclinacion_grados_x10 = 350;
        c2.fov_grados_x10 = 780;
        c2.ancho_carro_cm = 22;
        c2.margen_lateral_cm = 4;
        corredor_preparar(&cor2, &c2, 320, 240);
        cam2 = camara_de(&c2, 320, 240);
        for (i = 0; i < (int)(sizeof distancias / sizeof distancias[0]); i++) {
            static const double lat2[] = { 0.0, 10.0, 16.5, 22.0, 40.0 };   /* huella +-11, corredor +-15 */
            static const int esp2[] = { COR_DENTRO, COR_DENTRO, COR_BORDE, COR_FUERA, COR_FUERA };

            for (j = 0; j < 5; j++) {
                CajaImagen caja = caja_de_objeto(&cam2, lat2[j], distancias[i] + 10.0, 6.0);
                ResultadoCaja r;

                total2++;
                if (corredor_clasificar(&cor2, &caja, &r) != COR_OK || r.estado != esp2[j]) {
                    malos2++;
                    printf("      (otra camara) a %.0f cm, lateral %.1f: esperado %s, dice %s\n", distancias[i] + 10.0, lat2[j],
                           corredor_texto_estado(esp2[j]), corredor_texto_estado(r.estado));
                }
            }
        }
        caso("con otra camara (15 cm de altura, 35 grados, 78 de campo, 320x240, carro de 22 cm, margen 4) las 25 clasificaciones coinciden", malos2 == 0 && total2 == 25);
    }
}

/* ── Zonas ─────────────────────────────────────────────────────────────── */

static void zonas_con_obstaculo(ZonaObstaculo zonas[OBS_ZONAS], int izq, int cen, int der)
{
    int z;

    memset(zonas, 0, sizeof(ZonaObstaculo) * OBS_ZONAS);
    zonas[ZONA_IZQUIERDA].hay_obstaculo = izq;
    zonas[ZONA_CENTRO].hay_obstaculo = cen;
    zonas[ZONA_DERECHA].hay_obstaculo = der;
    for (z = 0; z < OBS_ZONAS; z++) {
        zonas[z].cercania = zonas[z].hay_obstaculo ? 2 : 0;
    }
}

static void pruebas_zonas(void)
{
    ConfigCorredor c;
    Corredor cor;
    Camara cam;
    ZonaObstaculo zonas[OBS_ZONAS];
    CajaImagen cajas[4];

    corredor_config_defecto(&c);
    c.activo = 1;
    corredor_preparar(&cor, &c, 240, 176);
    cam = camara_de(&c, 240, 176);

    /* Un objeto del modelo (una caja de peso suficiente) delante, centrado a 50 cm: cuenta en la zona del centro. */
    cajas[0] = caja_de_objeto(&cam, 0.0, 50.0, 10.0);
    cajas[0].peso = c.min_peso;
    zonas_con_obstaculo(zonas, 0, 1, 0);
    caso("modelo: un objeto de 10 cm centrado a 50 cm cuenta en el centro (no queda fuera)", corredor_aplicar_zonas(&cor, cajas, 1, zonas) == COR_OK &&
         zonas[ZONA_CENTRO].fuera_corredor == 0 && zonas[ZONA_CENTRO].celdas_corredor == c.min_peso && zonas[ZONA_CENTRO].cercania_corredor >= 1);

    /* Lo mismo a un lado, a 40 cm del eje: la zona tiene obstaculo pero queda FUERA (solo avisa). */
    cajas[0] = caja_de_objeto(&cam, -40.0, 50.0, 10.0);
    cajas[0].peso = c.min_peso;
    zonas_con_obstaculo(zonas, 1, 0, 0);
    corredor_aplicar_zonas(&cor, cajas, 1, zonas);
    caso("modelo: un objeto a 40 cm a la izquierda a 50 cm adelante: zona izquierda con obstaculo pero fuera_corredor = 1",
         zonas[ZONA_IZQUIERDA].hay_obstaculo == 1 && zonas[ZONA_IZQUIERDA].fuera_corredor == 1 && zonas[ZONA_IZQUIERDA].cercania_corredor == 0 &&
         zonas[ZONA_IZQUIERDA].celdas_corredor == 0);

    /* Dos objetos a la vez: uno en el corredor y otro fuera. */
    cajas[0] = caja_de_objeto(&cam, 2.0, 60.0, 10.0);
    cajas[0].peso = c.min_peso;
    cajas[1] = caja_de_objeto(&cam, 45.0, 40.0, 10.0);
    cajas[1].peso = c.min_peso;
    zonas_con_obstaculo(zonas, 0, 1, 1);
    corredor_aplicar_zonas(&cor, cajas, 2, zonas);
    caso("dos objetos: el del centro cuenta y el de la derecha (45 cm del eje) queda fuera",
         zonas[ZONA_CENTRO].fuera_corredor == 0 && zonas[ZONA_CENTRO].celdas_corredor > 0 && zonas[ZONA_DERECHA].fuera_corredor == 1);

    /* Ninguna caja. */
    zonas_con_obstaculo(zonas, 0, 0, 0);
    caso("sin cajas: ninguna zona queda marcada fuera ni con cercania de corredor", corredor_aplicar_zonas(&cor, NULL, 0, zonas) == COR_OK &&
         zonas[0].fuera_corredor == 0 && zonas[1].fuera_corredor == 0 && zonas[2].fuera_corredor == 0 && zonas[1].cercania_corredor == 0);

    /* El peso minimo: una caja de peso 1 sola no alcanza (min_peso = 2); dos si. */
    cajas[0] = caja_de_objeto(&cam, 0.0, 50.0, 6.0);
    cajas[0].peso = 1;
    zonas_con_obstaculo(zonas, 0, 1, 0);
    corredor_aplicar_zonas(&cor, cajas, 1, zonas);
    caso("una sola caja de peso 1 (menos que el minimo 2) no cuenta: la zona queda fuera_corredor", zonas[ZONA_CENTRO].fuera_corredor == 1 && zonas[ZONA_CENTRO].celdas_corredor == 1);
    cajas[1] = cajas[0];
    zonas_con_obstaculo(zonas, 0, 1, 0);
    corredor_aplicar_zonas(&cor, cajas, 2, zonas);
    caso("dos cajas de peso 1 si cuentan", zonas[ZONA_CENTRO].fuera_corredor == 0 && zonas[ZONA_CENTRO].celdas_corredor == 2);

    /* La cercania sale del borde de abajo mas bajo, con los limites del detector (17 filas: cerca desde la 14, media desde la 11). */
    cajas[0].x0 = 100; cajas[0].x1 = 140; cajas[0].y0 = 120; cajas[0].y1 = 160; cajas[0].peso = 2; cajas[0].fuente = COR_FUENTE_FLUJO;   /* fila 15: cerca */
    zonas_con_obstaculo(zonas, 0, 1, 0);
    corredor_aplicar_zonas(&cor, cajas, 1, zonas);
    caso("caja con el borde de abajo en la fila 15 de 17: cercania de corredor 3", zonas[ZONA_CENTRO].cercania_corredor == 3);
    cajas[0].y0 = 90; cajas[0].y1 = 120;   /* fila 11: media */
    zonas_con_obstaculo(zonas, 0, 1, 0);
    corredor_aplicar_zonas(&cor, cajas, 1, zonas);
    caso("borde de abajo en la fila 11: cercania de corredor 2", zonas[ZONA_CENTRO].cercania_corredor == 2);
    cajas[0].y0 = 60; cajas[0].y1 = 90;    /* fila 8: lejos */
    zonas_con_obstaculo(zonas, 0, 1, 0);
    corredor_aplicar_zonas(&cor, cajas, 1, zonas);
    caso("borde de abajo en la fila 8: cercania de corredor 1", zonas[ZONA_CENTRO].cercania_corredor == 1);

    /* APAGADO: nada se filtra. */
    {
        ConfigCorredor apagado = c;
        Corredor cor_apagado;

        apagado.activo = 0;
        corredor_preparar(&cor_apagado, &apagado, 240, 176);
        cajas[0] = caja_de_objeto(&cam, -60.0, 50.0, 10.0);
        cajas[0].peso = 5;
        zonas_con_obstaculo(zonas, 1, 0, 0);
        zonas[0].fuera_corredor = 1; zonas[0].cercania_corredor = 3; zonas[0].celdas_corredor = 9;   /* basura previa */
        corredor_aplicar_zonas(&cor_apagado, cajas, 1, zonas);
        caso("con el corredor APAGADO se limpian los campos del corredor y nada queda fuera (el navegador usa lo de siempre)",
             zonas[0].fuera_corredor == 0 && zonas[0].cercania_corredor == 0 && zonas[0].celdas_corredor == 0 && zonas[0].hay_obstaculo == 1);
    }
    caso("aplicar_zonas con NULL: COR_ERR_ARGUMENTO", corredor_aplicar_zonas(NULL, cajas, 1, zonas) == COR_ERR_ARGUMENTO &&
         corredor_aplicar_zonas(&cor, cajas, 1, NULL) == COR_ERR_ARGUMENTO && corredor_aplicar_zonas(&cor, NULL, 2, zonas) == COR_ERR_ARGUMENTO);

    /* Cajas de milesimas a pixeles. */
    {
        CajaImagen caja;

        corredor_caja_de_milesimas(250, 500, 750, 900, 240, 176, 3, COR_FUENTE_MODELO, &caja);
        caso("caja del modelo (250,500)-(750,900) milesimas en 240x176: x 60..180, y 88..159, peso 3",
             caja.x0 == 60 && caja.x1 == 180 && caja.y0 == 88 && caja.y1 == 159 && caja.peso == 3 && caja.fuente == COR_FUENTE_MODELO);
    }
}

/* ── El detector de color ──────────────────────────────────────────────── */

#define ANCHO_IMG 240
#define ALTO_IMG 176
static unsigned char imagen[ANCHO_IMG * ALTO_IMG * 3];

static void rellenar(int r, int g, int b)
{
    int i;

    for (i = 0; i < ANCHO_IMG * ALTO_IMG; i++) {
        imagen[i * 3] = (unsigned char)r;
        imagen[i * 3 + 1] = (unsigned char)g;
        imagen[i * 3 + 2] = (unsigned char)b;
    }
}

static void rectangulo(int x0, int y0, int x1, int y1, int r, int g, int b)
{
    int x, y;

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            imagen[(y * ANCHO_IMG + x) * 3] = (unsigned char)r;
            imagen[(y * ANCHO_IMG + x) * 3 + 1] = (unsigned char)g;
            imagen[(y * ANCHO_IMG + x) * 3 + 2] = (unsigned char)b;
        }
    }
}

static void pruebas_detector(void)
{
    ConfigCorredor cc;
    ConfigObstaculos co;
    Corredor cor;
    ResultadoObstaculos res;

    corredor_config_defecto(&cc);
    cc.activo = 1;
    corredor_preparar(&cor, &cc, ANCHO_IMG, ALTO_IMG);
    obstaculos_config_defecto(&co);
    co.min_celdas = 6;

    /* Una caja azul de 6 x 3 celdas (60 x 30 px) CENTRADA, con el borde de abajo en la fila 120 px. */
    rellenar(120, 100, 80);
    rectangulo(90, 90, 150, 120, 30, 60, 200);
    obstaculos_analizar_con(&co, imagen, ANCHO_IMG, ALTO_IMG, &res);
    caso("detector: la caja centrada se detecta en el centro", res.zona[ZONA_CENTRO].hay_obstaculo == 1 && res.zona[ZONA_CENTRO].celdas == 18);
    corredor_aplicar_resultado(&cor, &res);
    caso("detector + corredor: la caja centrada queda DENTRO (no fuera, cercania de corredor >= 1)",
         res.zona[ZONA_CENTRO].fuera_corredor == 0 && res.zona[ZONA_CENTRO].cercania_corredor >= 1 && res.zona[ZONA_CENTRO].celdas_corredor == 18);

    /* Una caja pegada a la izquierda (x 0..40) y lejos (borde de abajo 80 px). En esa fila el corredor (huella 9 cm + margen 3 cm) mide +-77 px
     * alrededor de x = 120, o sea desde x = 43: la caja queda fuera. Con el detector en 3 celdas minimas (la fila minima 440 deja solo la fila 7). */
    co.min_celdas = 3;
    rellenar(120, 100, 80);
    rectangulo(0, 50, 40, 80, 30, 60, 200);
    obstaculos_analizar_con(&co, imagen, ANCHO_IMG, ALTO_IMG, &res);
    caso("detector: la caja de la izquierda se detecta en la zona izquierda", res.zona[ZONA_IZQUIERDA].hay_obstaculo == 1);
    corredor_aplicar_resultado(&cor, &res);
    caso("detector + corredor: la caja de la izquierda (pasa por un lado) sigue en hay_obstaculo pero queda FUERA del corredor",
         res.zona[ZONA_IZQUIERDA].hay_obstaculo == 1 && res.zona[ZONA_IZQUIERDA].fuera_corredor == 1 && res.zona[ZONA_IZQUIERDA].cercania_corredor == 0);
    printf("      zona izquierda: celdas %d, celdas dentro del corredor %d, cercania detector %d\n", res.zona[ZONA_IZQUIERDA].celdas,
           res.zona[ZONA_IZQUIERDA].celdas_corredor, res.zona[ZONA_IZQUIERDA].cercania);

    /* La misma caja, pero CERCA (borde de abajo en 150 px): a esa altura el corredor cubre casi todo el cuadro y queda DENTRO. */
    rellenar(120, 100, 80);
    rectangulo(0, 120, 40, 150, 30, 60, 200);
    obstaculos_analizar_con(&co, imagen, ANCHO_IMG, ALTO_IMG, &res);
    corredor_aplicar_resultado(&cor, &res);
    caso("detector + corredor: la misma caja de la izquierda pero CERCA (fila 150) ya cae dentro del corredor (cerca el corredor es ancho)",
         res.zona[ZONA_IZQUIERDA].hay_obstaculo == 1 && res.zona[ZONA_IZQUIERDA].fuera_corredor == 0 && res.zona[ZONA_IZQUIERDA].cercania_corredor == 3);

    /* Piso vacio: nada. */
    rellenar(120, 100, 80);
    obstaculos_analizar_con(&co, imagen, ANCHO_IMG, ALTO_IMG, &res);
    corredor_aplicar_resultado(&cor, &res);
    caso("piso vacio: ninguna zona con obstaculo ni fuera del corredor", res.zona[0].hay_obstaculo == 0 && res.zona[1].hay_obstaculo == 0 && res.zona[2].hay_obstaculo == 0 &&
         res.zona[0].fuera_corredor == 0 && res.zona[1].fuera_corredor == 0 && res.zona[2].fuera_corredor == 0);

    /* Apagado: la caja de la izquierda no se marca fuera. */
    {
        ConfigCorredor apagado = cc;
        Corredor cor_apagado;

        apagado.activo = 0;
        corredor_preparar(&cor_apagado, &apagado, ANCHO_IMG, ALTO_IMG);
        rellenar(120, 100, 80);
        rectangulo(0, 50, 40, 80, 30, 60, 200);
        obstaculos_analizar_con(&co, imagen, ANCHO_IMG, ALTO_IMG, &res);
        corredor_aplicar_resultado(&cor_apagado, &res);
        caso("corredor APAGADO: la caja de la izquierda no se marca fuera (todo cuenta como siempre)", res.zona[ZONA_IZQUIERDA].hay_obstaculo == 1 && res.zona[ZONA_IZQUIERDA].fuera_corredor == 0);
    }
    caso("aplicar_resultado con NULL: COR_ERR_ARGUMENTO", corredor_aplicar_resultado(NULL, &res) == COR_ERR_ARGUMENTO && corredor_aplicar_resultado(&cor, NULL) == COR_ERR_ARGUMENTO);
    {
        ZonaObstaculo zonas[OBS_ZONAS];
        unsigned char mascara[4] = { 0, 0, 0, 0 };

        memset(zonas, 0, sizeof zonas);
        caso("aplicar_mascara con medidas malas (0 columnas, paso menor que columnas): COR_ERR_ARGUMENTO",
             corredor_aplicar_mascara(&cor, mascara, 2, 0, 2, 0, zonas) == COR_ERR_ARGUMENTO && corredor_aplicar_mascara(&cor, mascara, 1, 2, 2, 0, zonas) == COR_ERR_ARGUMENTO);
    }
}

/* ── Flujo: bloques de 16x16 ───────────────────────────────────────────── */

static void pruebas_flujo(void)
{
    ConfigCorredor c;
    Corredor cor;
    Camara cam;
    ZonaObstaculo zonas[OBS_ZONAS];
    CajaImagen bloques[3];
    double u, v;

    corredor_config_defecto(&c);
    c.activo = 1;
    corredor_preparar(&cor, &c, 240, 176);
    cam = camara_de(&c, 240, 176);

    /* Tres bloques de 16x16 pegados, al centro, sobre el piso a 40 cm. */
    proyectar(&cam, 0.0, 40.0, &u, &v);
    {
        int i;

        for (i = 0; i < 3; i++) {
            bloques[i].x0 = 104 + 16 * (i - 1) + 16;
            bloques[i].x1 = bloques[i].x0 + 16;
            bloques[i].y1 = (int)(v + 0.5);
            bloques[i].y0 = bloques[i].y1 - 16;
            bloques[i].peso = 1;
            bloques[i].fuente = COR_FUENTE_FLUJO;
        }
    }
    zonas_con_obstaculo(zonas, 0, 1, 0);
    corredor_aplicar_zonas(&cor, bloques, 3, zonas);
    caso("flujo: tres bloques de 16x16 sobre el piso a 40 cm, al centro: cuentan (peso 3) y no quedan fuera",
         zonas[ZONA_CENTRO].fuera_corredor == 0 && zonas[ZONA_CENTRO].celdas_corredor == 3);

    /* Los mismos bloques a 70 cm del eje, a la izquierda. */
    {
        int i, desplazamiento;
        double u2, v2;

        proyectar(&cam, -70.0, 40.0, &u2, &v2);
        desplazamiento = (int)(u2 - u);
        for (i = 0; i < 3; i++) {
            bloques[i].x0 += desplazamiento;
            bloques[i].x1 += desplazamiento;
        }
    }
    zonas_con_obstaculo(zonas, 1, 0, 0);
    corredor_aplicar_zonas(&cor, bloques, 3, zonas);
    caso("flujo: los mismos bloques 70 cm a la izquierda quedan fuera del corredor (solo avisan)", zonas[ZONA_IZQUIERDA].hay_obstaculo == 1 && zonas[ZONA_IZQUIERDA].fuera_corredor == 1);
}

/* ── Al azar ───────────────────────────────────────────────────────────── */

static unsigned long semilla = 20261014UL;

static int azar(int maximo)
{
    semilla = semilla * 1103515245UL + 12345UL;
    return (int)((semilla >> 16) % (unsigned long)(maximo + 1));
}

static void pruebas_azar(void)
{
    ConfigCorredor c;
    Corredor cor;
    int i, rango_ok = 1, coherente = 1, monotono = 1;

    corredor_config_defecto(&c);
    c.activo = 1;
    corredor_preparar(&cor, &c, 240, 176);

    for (i = 0; i < 100000; i++) {
        CajaImagen caja;
        ResultadoCaja r;
        int centro, ancho_caja;

        caja.x0 = azar(230);
        ancho_caja = 1 + azar(100);
        caja.x1 = caja.x0 + ancho_caja;
        caja.y1 = azar(176);
        caja.y0 = caja.y1 - azar(60);
        caja.peso = 1;
        caja.fuente = COR_FUENTE_DETECTOR;
        if (corredor_clasificar(&cor, &caja, &r) != COR_OK || r.estado < COR_FUERA || r.estado > COR_DENTRO) {
            rango_ok = 0;
            continue;
        }
        centro = (caja.x0 + caja.x1) / 2;
        (void)centro;
        /* DENTRO exige cruzar la huella; BORDE, cruzar el corredor pero no la huella; FUERA no cruza el corredor; la huella cabe en el corredor. */
        if (!r.sobre_horizonte) {
            int cruza_huella = caja.x1 > cor.cx - r.semiancho_huella_px && caja.x0 < cor.cx + r.semiancho_huella_px;
            int cruza_corredor = caja.x1 > cor.cx - r.semiancho_corredor_px && caja.x0 < cor.cx + r.semiancho_corredor_px;

            if ((r.estado == COR_DENTRO) != cruza_huella || (r.estado == COR_BORDE) != (cruza_corredor && !cruza_huella) ||
                r.semiancho_huella_px > r.semiancho_corredor_px) {
                coherente = 0;
            }
        } else if (r.estado != COR_FUERA) {
            coherente = 0;
        }
    }
    caso("100 000 cajas al azar: el estado siempre esta en rango", rango_ok);
    caso("100 000 cajas al azar: DENTRO, BORDE y FUERA siempre coinciden con las anchuras que reporta (y la huella cabe en el corredor)", coherente);

    /* Un margen mas grande nunca vuelve FUERA lo que antes estaba dentro o en el borde. */
    {
        ConfigCorredor ancha = c;
        Corredor cor_ancho;

        ancha.margen_lateral_cm = 10;
        corredor_preparar(&cor_ancho, &ancha, 240, 176);
        for (i = 0; i < 50000; i++) {
            CajaImagen caja;
            ResultadoCaja a, b;

            caja.x0 = azar(230);
            caja.x1 = caja.x0 + 1 + azar(60);
            caja.y1 = azar(176);
            caja.y0 = 0;
            caja.peso = 1;
            caja.fuente = COR_FUENTE_MODELO;
            corredor_clasificar(&cor, &caja, &a);
            corredor_clasificar(&cor_ancho, &caja, &b);
            if (a.estado != COR_FUERA && b.estado == COR_FUERA) {
                monotono = 0;
            }
        }
    }
    caso("un margen mayor (10 cm contra 3) nunca deja fuera lo que ya estaba dentro o en el borde", monotono);
}

int main(void)
{
    pruebas_configuracion();
    pruebas_perspectiva();
    pruebas_clasificacion();
    pruebas_zonas();
    pruebas_detector();
    pruebas_flujo();
    pruebas_azar();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
