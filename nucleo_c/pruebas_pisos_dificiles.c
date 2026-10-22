/*
 * pruebas_pisos_dificiles.c — Pisos negros y de 2 colores: reproduce el problema en PC con cuadros sintéticos y comprueba los arreglos.
 *
 * GUÍA DE LECTURA
 *  1. Los cuadros salen de pisos_sinteticos.c (piso negro con ruido, viñeteo, degradado y reflejo; baldosas, franjas y damero de dos colores con color
 *     mezclado en el borde; objetos mano, libreta, caja y mochila; sombra suave; cambios de exposición). Solo números: nada se guarda ni se mira.
 *  2. Cada escena se corre con TRES configuraciones del detector:
 *       ANTES  : mejoras_piso = 0 y la franja de referencia de siempre (188 milésimas de ancho): el detector de antes.
 *       NUEVO  : los valores por defecto de hoy (mejoras_piso = 1 y la tira ancha por defecto).
 *     Las escenas de reproducción comprueban que el problema SÍ pasaba antes (si no, el generador no sirve); las demás, que ahora no pasa.
 *  3. Se imprime la tabla de números (cuadros con falso obstáculo, objetos detectados) para pegarla en docs/obstaculos.md.
 */

#include <stdio.h>
#include <string.h>
#include "obstaculos.h"
#include "pisos_sinteticos.h"

static unsigned char imagen[PS_ANCHO * PS_ALTO * 3];
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

typedef struct { int cuadros, con_obstaculo, en_centro, errores, racha_sin; } Cuenta;   /* racha_sin: la racha más larga de cuadros SEGUIDOS sin obstáculo */

/* Corre `n` cuadros de la misma escena (cambia solo el ruido) con un modelo que se conserva. Si `salto_en` > 0, desde ese cuadro la exposición pasa a
 * `exposicion_despues` por ciento. Cuenta los cuadros con obstáculo (en cualquier zona y en el centro) desde el cuadro `desde`. */
static Cuenta correr(const ConfigObstaculos *cfg, EscenaSintetica esc, int n, int avanzando, int salto_en, int exposicion_despues, int desde)
{
    ModeloPiso modelo;
    ResultadoObstaculos r;
    Cuenta c = { 0, 0, 0, 0, 0 };
    int i, racha = 0;

    obstaculos_modelo_iniciar(&modelo);
    for (i = 0; i < n; i++) {
        int codigo;

        if (salto_en > 0 && i >= salto_en) {
            esc.exposicion_pct = exposicion_despues;
        }
        esc.semilla = (unsigned long)(i + 1) * 7919UL;
        ps_dibujar(&esc, imagen);
        codigo = obstaculos_analizar_modelo(cfg, &modelo, imagen, PS_ANCHO, PS_ALTO, avanzando, (unsigned int)(i * 80), &r);
        if (i < desde) {
            continue;
        }
        c.cuadros++;
        if (codigo != OBS_OK) {
            c.errores++;
        } else {
            if (r.zona[0].hay_obstaculo || r.zona[1].hay_obstaculo || r.zona[2].hay_obstaculo) {
                c.con_obstaculo++;
                racha = 0;
            } else {
                racha++;
                if (racha > c.racha_sin) {
                    c.racha_sin = racha;
                }
            }
            if (r.zona[ZONA_CENTRO].hay_obstaculo) {
                c.en_centro++;
            }
        }
    }
    return c;
}

static EscenaSintetica base(int piso)
{
    EscenaSintetica e;

    memset(&e, 0, sizeof e);
    e.piso = piso;
    e.exposicion_pct = 100;
    e.objeto = PS_NADA;
    return e;
}

static void config_antes(ConfigObstaculos *cfg)
{
    obstaculos_config_defecto(cfg);
    cfg->mejoras_piso = 0;
    cfg->ref_ancho_mil = 188;
}

/* Como en una ruta: el piso no cambia (no se agregan colores ni se reaprende). `con_mejoras` = 0 es el detector de antes (sin reajuste de brillo). */
static void config_ruta(ConfigObstaculos *cfg, int con_mejoras)
{
    obstaculos_config_defecto(cfg);
    cfg->piso_fijo = 1;
    if (!con_mejoras) {
        cfg->mejoras_piso = 0;
        cfg->ref_ancho_mil = 188;
    }
}

static void config_nuevo(ConfigObstaculos *cfg)
{
    obstaculos_config_defecto(cfg);   /* los valores por defecto de hoy */
}

static void pruebas_piso_negro(void)
{
    ConfigObstaculos antes, nuevo;
    int reflejo, croma;
    int antes_falla_con_reflejo = 0, nuevo_ok = 1, nuevo_total = 0;

    config_antes(&antes);
    config_nuevo(&nuevo);
    printf("\n--- PISO NEGRO vacio (60 cuadros): falsos obstaculos con ANTES / NUEVO ---\n");
    printf("     reflejo de luz abajo al centro (niveles) por columna; error de color por bloque de 16x16 (+-niveles) por fila\n");
    for (croma = 0; croma <= 6; croma += 3) {
        printf("     croma +-%d:", croma);
        for (reflejo = 0; reflejo <= 60; reflejo += 20) {
            EscenaSintetica e = base(PS_NEGRO);
            Cuenta a, n;

            e.sigma = 6; e.vineteo_pct = 20; e.reflejo = reflejo; e.croma_bloques = croma;
            a = correr(&antes, e, 60, 0, 0, 0, 0);
            n = correr(&nuevo, e, 60, 0, 0, 0, 0);
            printf("  +%2d: %2d / %2d", reflejo, a.con_obstaculo, n.con_obstaculo);
            if (reflejo >= 40 && a.con_obstaculo >= 30) {
                antes_falla_con_reflejo++;
            }
            nuevo_total++;
            if (n.con_obstaculo != 0 || n.errores != 0) {
                nuevo_ok = 0;
            }
        }
        printf("\n");
    }
    caso("REPRODUCCION: antes, un piso negro con un reflejo de +40 o +60 niveles abajo al centro daba falso obstaculo en casi todos los cuadros (6 casos)", antes_falla_con_reflejo == 6);
    caso("negro: con el detector nuevo, ningun cuadro vacio da falso obstaculo ni error (reflejos de 0 a 60 niveles, error de color 0 a 6)", nuevo_ok && nuevo_total == 12);

    /* Ruido de 3 a 8 con viñeteo y degradado. */
    {
        int sigma, vin, deg, malos = 0, total = 0;

        for (sigma = 3; sigma <= 8; sigma += 1) {
            for (vin = 0; vin <= 40; vin += 20) {
                for (deg = -30; deg <= 30; deg += 30) {
                    EscenaSintetica e = base(PS_NEGRO);
                    Cuenta n;

                    e.sigma = sigma; e.vineteo_pct = vin; e.degradado_pct = deg; e.reflejo = 30;
                    n = correr(&nuevo, e, 25, 0, 0, 0, 0);
                    total++;
                    if (n.con_obstaculo != 0) {
                        malos++;
                    }
                }
            }
        }
        caso("negro: ruido de 3 a 8, viñeteo de 0 a 40 %, degradado de -30 a +30 % y un reflejo de +30: 54 escenas sin un solo falso obstaculo", malos == 0 && total == 54);
    }
}

static void pruebas_piso_dos_colores(void)
{
    ConfigObstaculos antes, nuevo;
    int l, d, antes_malos = 0, nuevo_malos = 0, total = 0;
    static const int lados[] = { 40, 60, 80, 120 };
    static const int desfases[] = { 0, 20, 40, 60 };

    config_antes(&antes);
    config_nuevo(&nuevo);
    printf("\n--- PISO DE 2 COLORES: la referencia (abajo al centro) cae sobre UNA sola baldosa. Cuadros con falso obstaculo de 30, ANTES / NUEVO ---\n");
    for (l = 0; l < 4; l++) {
        printf("     baldosas de %3d px:", lados[l]);
        for (d = 0; d < 4; d++) {
            EscenaSintetica e = base(PS_BALDOSAS);
            Cuenta a, n;

            e.sigma = 4; e.baldosa_px = lados[l]; e.desfase_x = desfases[d];
            a = correr(&antes, e, 30, 0, 0, 0, 0);
            n = correr(&nuevo, e, 30, 0, 0, 0, 0);
            printf("  desfase %2d: %2d / %2d", desfases[d], a.con_obstaculo, n.con_obstaculo);
            total++;
            if (a.con_obstaculo >= 15) {
                antes_malos++;
            }
            if (n.con_obstaculo != 0 || n.errores != 0) {
                nuevo_malos++;
            }
        }
        printf("\n");
    }
    caso("REPRODUCCION: antes, con baldosas de 2 colores, en la mayoria de las posiciones (12 de 16) un color quedaba como obstaculo", antes_malos >= 12 && total == 16);
    caso("2 colores: con el detector nuevo ninguna de las 16 posiciones da falso obstaculo", nuevo_malos == 0);

    /* Los otros pisos de 2 colores. */
    {
        static const int pisos[] = { PS_BALDOSAS, PS_FRANJAS, PS_DAMERO };
        int p, malos_antes = 0, malos_nuevo = 0;

        printf("     franjas, damero y baldosas de 40 px con ruido 4 / 8 y viñeteo 30:");
        for (p = 0; p < 3; p++) {
            int sigma;

            for (sigma = 4; sigma <= 8; sigma += 4) {
                EscenaSintetica e = base(pisos[p]);
                Cuenta a, n;

                e.sigma = sigma; e.vineteo_pct = 30;
                a = correr(&antes, e, 30, 0, 0, 0, 0);
                n = correr(&nuevo, e, 30, 0, 0, 0, 0);
                printf("  %s s%d: %d / %d", ps_nombre_piso(pisos[p]), sigma, a.con_obstaculo, n.con_obstaculo);
                malos_antes += (a.con_obstaculo > 0);
                malos_nuevo += (n.con_obstaculo > 0 || n.errores > 0);
            }
        }
        printf("\n");
        caso("franjas, damero y baldosas de 40 px (la franja vieja ve las dos baldosas) con ruido 4 y 8: sin falsos obstaculos con el detector nuevo", malos_nuevo == 0);
        (void)malos_antes;
    }
}

static void pruebas_exposicion(void)
{
    ConfigObstaculos antes, nuevo;
    int piso, s, antes_malos = 0, nuevo_malos = 0, total = 0;
    static const int saltos[] = { 135, 50 };

    config_ruta(&antes, 0);
    config_ruta(&nuevo, 1);
    printf("\n--- EXPOSICION AUTOMATICA EN RUTA (piso_fijo: no se agregan colores): el brillo de todo el cuadro cambia de golpe en el cuadro 20 (cuadros 21 a 59 con falso obstaculo de 39, ANTES / NUEVO) ---\n");
    for (piso = PS_LISO; piso <= PS_DAMERO; piso++) {
        printf("     %-8s:", ps_nombre_piso(piso));
        for (s = 0; s < 2; s++) {
            EscenaSintetica e = base(piso);
            Cuenta a, n;

            e.sigma = 4; e.vineteo_pct = 20;
            a = correr(&antes, e, 60, 0, 20, saltos[s], 21);
            n = correr(&nuevo, e, 60, 0, 20, saltos[s], 21);
            printf("  100 -> %3d: %2d / %2d", saltos[s], a.con_obstaculo, n.con_obstaculo);
            total++;
            antes_malos += (a.con_obstaculo >= 20);
            nuevo_malos += (n.con_obstaculo > 3 || n.errores > 0);   /* se admiten hasta 3 cuadros de ajuste justo después del salto */
        }
        printf("\n");
    }
    caso("REPRODUCCION: antes, en una ruta (piso fijo), un cambio brusco de exposicion (x1.35 o x0.5) marcaba todo el piso como obstaculo en en 4 de las 8 escenas", antes_malos >= 4 && total == 8);
    caso("exposicion: con el reajuste de brillo, a lo mas 3 cuadros con falso obstaculo tras el salto en las 8 escenas", nuevo_malos == 0);

    /* El reajuste NO debe absorber un objeto grande que aparece: la mochila sobre el piso sigue detectandose despues del cambio. */
    {
        EscenaSintetica e = base(PS_LISO);
        Cuenta n;

        e.sigma = 4; e.vineteo_pct = 20; e.objeto = PS_MOCHILA; e.obj_x = 70; e.obj_y = 75;
        n = correr(&nuevo, e, 40, 0, 10, 190, 12);
        caso("exposicion: una mochila en el cuadro sigue detectandose despues del salto de exposicion (el reajuste no la absorbe)", n.en_centro >= n.cuadros - 2);
    }
}

static void pruebas_objetos(void)
{
    ConfigObstaculos nuevo, antes;
    static const int pisos[] = { PS_NEGRO, PS_LISO, PS_BALDOSAS, PS_FRANJAS, PS_DAMERO };
    static const int objetos[] = { PS_MANO, PS_LIBRETA, PS_CAJA, PS_MOCHILA };
    static const int ox[] = { 90, 90, 85, 70 };
    static const int oy[] = { 100, 105, 95, 75 };
    int p, o, no_detectados = 0, falsos_sombra = 0, falsos_vacio = 0, peor_racha = 0, peor_detectado = 30;

    config_nuevo(&nuevo);
    config_antes(&antes);
    printf("\n--- OBJETOS sobre cada piso (30 cuadros cada uno): cuadros con el objeto detectado / sombra suave y piso vacio (falsos), NUEVO ---\n");
    for (p = 0; p < 5; p++) {
        printf("     %-8s", ps_nombre_piso(pisos[p]));
        for (o = 0; o < 4; o++) {
            EscenaSintetica e = base(pisos[p]);
            Cuenta c;

            e.sigma = 5; e.vineteo_pct = 20; e.objeto = objetos[o]; e.obj_x = ox[o]; e.obj_y = oy[o];
            c = correr(&nuevo, e, 30, 0, 0, 0, 0);
            printf("  %s %2d/30", ps_nombre_objeto(objetos[o]), c.con_obstaculo);
            if (c.con_obstaculo < 30) {
                no_detectados++;
            }
            if (c.racha_sin > peor_racha) {
                peor_racha = c.racha_sin;
            }
            if (c.con_obstaculo < peor_detectado) {
                peor_detectado = c.con_obstaculo;
            }
        }
        {
            EscenaSintetica e = base(pisos[p]);
            Cuenta c;

            e.sigma = 5; e.vineteo_pct = 20; e.sombra = 1;
            c = correr(&nuevo, e, 30, 0, 0, 0, 0);
            printf("  sombra %2d", c.con_obstaculo);
            falsos_sombra += c.con_obstaculo;
            e.sombra = 0;
            c = correr(&nuevo, e, 30, 0, 0, 0, 0);
            printf("  vacio %2d\n", c.con_obstaculo);
            falsos_vacio += c.con_obstaculo;
        }
    }
    printf("     peor caso: %d de 30 cuadros con el objeto detectado; la racha mas larga de cuadros seguidos sin detectarlo es %d (%d combinaciones no llegan a 30/30)\n", peor_detectado, peor_racha, no_detectados);
    caso("mano, libreta, caja y mochila sobre los 5 pisos: en las 20 combinaciones el objeto se detecta en al menos 20 de 30 cuadros y nunca pasan mas de 3 cuadros seguidos sin verlo", peor_detectado >= 20 && peor_racha <= 3);
    caso("una sombra suave sobre cada piso: ningun falso obstaculo", falsos_sombra == 0);
    caso("el piso vacio de cada tipo: ningun falso obstaculo", falsos_vacio == 0);

    /* Lo mismo con el detector de antes (para que se vea qué se conserva). */
    {
        int detectados_antes = 0, total = 0;

        for (p = 0; p < 5; p++) {
            for (o = 0; o < 4; o++) {
                EscenaSintetica e = base(pisos[p]);
                Cuenta c;

                e.sigma = 5; e.vineteo_pct = 20; e.objeto = objetos[o]; e.obj_x = ox[o]; e.obj_y = oy[o];
                c = correr(&antes, e, 30, 0, 0, 0, 0);
                detectados_antes += (c.con_obstaculo == 30);
                total++;
            }
        }
        printf("     (el detector de antes detecta el objeto en los 30 cuadros en %d de %d combinaciones)\n", detectados_antes, total);
    }
}

/* Aprender piso: con el carro quieto, aprende los colores de toda la zona. */
static void pruebas_aprender(void)
{
    static const int pisos[] = { PS_NEGRO, PS_LISO, PS_BALDOSAS, PS_FRANJAS, PS_DAMERO };
    ConfigObstaculos cfg;
    int p, bien = 1;

    config_nuevo(&cfg);
    printf("\n--- APRENDER PISO (8 cuadros con el carro quieto): colores aprendidos y brillo medio ---\n");
    for (p = 0; p < 5; p++) {
        static AcumuladorPiso acumulador;
        InformePiso informe;
        ModeloPiso modelo;
        EscenaSintetica e = base(pisos[p]);
        int i, codigo;

        obstaculos_aprender_iniciar(&acumulador);
        for (i = 0; i < 8; i++) {
            e.sigma = 5; e.vineteo_pct = 20; e.semilla = (unsigned long)(i + 1) * 31UL;
            ps_dibujar(&e, imagen);
            if (obstaculos_aprender_cuadro(&acumulador, &cfg, imagen, PS_ANCHO, PS_ALTO) != OBS_OK) {
                bien = 0;
            }
        }
        obstaculos_modelo_iniciar(&modelo);
        codigo = obstaculos_aprender_terminar(&acumulador, &cfg, &modelo, &informe);
        printf("     %-8s: codigo %d, colores %d, brillo medio %3d, muy_oscuro %d, muy_variado %d, descartado %d %%\n", ps_nombre_piso(pisos[p]), codigo,
               informe.grupos, informe.brillo_medio, informe.muy_oscuro, informe.muy_variado, informe.descartado_pct);
        if (codigo != OBS_OK || !modelo.valido || informe.muy_variado != 0) {
            bien = 0;
        }
    }
    caso("aprender piso: los 5 pisos se aprenden sin error, sin muy_variado y dejan un modelo valido", bien);

    /* J10 Punto 1: caja de otro color a 40 cm (filas gruesas 5 a 7, cubre >= 15 % de la zona) y a 60 cm (filas gruesas 4 a 6, cubre >= 10 % de la zona):
     * como no aparece en la franja inmediata (filas 8 y 9), su grupo se descarta, descartado_pct > 5, muy_variado = 1 ("Hay algo frente al carro, despéjalo")
     * y el color de la caja NO entra al modelo del piso. */
    {
        static AcumuladorPiso ac40, ac60;
        InformePiso inf40, inf60;
        ModeloPiso mod40, mod60;
        ResultadoObstaculos r40, r60;
        int i, x, y;

        obstaculos_aprender_iniciar(&ac40);
        obstaculos_aprender_iniciar(&ac60);
        for (i = 0; i < 6; i++) {
            EscenaSintetica e = base(PS_LISO);

            e.sigma = 4; e.semilla = (unsigned long)(i + 1) * 41UL;
            ps_dibujar(&e, imagen);
            /* Caja roja a ~40 cm: filas 90..136 de 176 (filas gruesas 5..7, arriba de la 8), columnas 60..180 (~26 % de la zona de avance). */
            for (y = 90; y < 136; y++) {
                for (x = 60; x < 180; x++) {
                    imagen[(y * PS_ANCHO + x) * 3] = 210;
                    imagen[(y * PS_ANCHO + x) * 3 + 1] = 40;
                    imagen[(y * PS_ANCHO + x) * 3 + 2] = 35;
                }
            }
            obstaculos_aprender_cuadro(&ac40, &cfg, imagen, PS_ANCHO, PS_ALTO);

            ps_dibujar(&e, imagen);
            /* Caja azul a ~60 cm: filas 72..105 de 176 (filas gruesas 4..5, arriba de la 8), columnas 70..170 (~14 % de la zona de avance, >= 10 %). */
            for (y = 72; y < 105; y++) {
                for (x = 70; x < 170; x++) {
                    imagen[(y * PS_ANCHO + x) * 3] = 30;
                    imagen[(y * PS_ANCHO + x) * 3 + 1] = 65;
                    imagen[(y * PS_ANCHO + x) * 3 + 2] = 215;
                }
            }
            obstaculos_aprender_cuadro(&ac60, &cfg, imagen, PS_ANCHO, PS_ALTO);
        }
        obstaculos_modelo_iniciar(&mod40);
        obstaculos_aprender_terminar(&ac40, &cfg, &mod40, &inf40);
        obstaculos_modelo_iniciar(&mod60);
        obstaculos_aprender_terminar(&ac60, &cfg, &mod60, &inf60);
        printf("     caja a 40 cm: grupos=%d descartado=%d %% muy_variado=%d; caja a 60 cm: grupos=%d descartado=%d %% muy_variado=%d\n",
               inf40.grupos, inf40.descartado_pct, inf40.muy_variado, inf60.grupos, inf60.descartado_pct, inf60.muy_variado);
        caso("aprender piso con caja de otro color a 40 cm (>= 10 % de la zona, solo arriba de filas 8-9): se descarta (> 5 %), muy_variado = 1 y no se guarda como piso",
             inf40.grupos == 1 && inf40.descartado_pct > 5 && inf40.muy_variado == 1 && inf40.color[0][0] < 195);
        caso("aprender piso con caja de otro color a 60 cm (>= 10 % de la zona, solo arriba de filas 8-9): se descarta (> 5 %), muy_variado = 1 y no se guarda como piso",
             inf60.grupos == 1 && inf60.descartado_pct > 5 && inf60.muy_variado == 1 && inf60.color[0][2] < 180);
        /* Y el modelo aprendido sigue viendo ambas cajas como obstáculo en el centro. */
        {
            EscenaSintetica e = base(PS_LISO);

            e.sigma = 4; e.semilla = 99UL;
            ps_dibujar(&e, imagen);
            for (y = 90; y < 136; y++) {
                for (x = 60; x < 180; x++) {
                    imagen[(y * PS_ANCHO + x) * 3] = 210;
                    imagen[(y * PS_ANCHO + x) * 3 + 1] = 40;
                    imagen[(y * PS_ANCHO + x) * 3 + 2] = 35;
                }
            }
            obstaculos_analizar_modelo(&cfg, &mod40, imagen, PS_ANCHO, PS_ALTO, 0, 100, &r40);
            ps_dibujar(&e, imagen);
            for (y = 72; y < 105; y++) {
                for (x = 70; x < 170; x++) {
                    imagen[(y * PS_ANCHO + x) * 3] = 30;
                    imagen[(y * PS_ANCHO + x) * 3 + 1] = 65;
                    imagen[(y * PS_ANCHO + x) * 3 + 2] = 215;
                }
            }
            obstaculos_analizar_modelo(&cfg, &mod60, imagen, PS_ANCHO, PS_ALTO, 0, 100, &r60);
            caso("... y el detector sigue marcando como obstaculo central la caja a 40 cm y la caja a 60 cm",
                 r40.zona[ZONA_CENTRO].hay_obstaculo == 1 && r60.zona[ZONA_CENTRO].hay_obstaculo == 1);
        }
    }

    /* Piso de 2 colores donde el segundo color solo se ve lejos (filas gruesas 4..7, arriba de filas 8 y 9: tapete o loseta de otro tono).
     * Si no se acepta como franja continua, se descarta (> 5 %), muy_variado = 1 y luego da falsos positivos en todos los cuadros.
     * Al aceptarse como franja continua: descartado_pct <= 5 %, muy_variado = 0, y 0 falsos positivos al analizar el piso. */
    {
        static AcumuladorPiso ac_tapete;
        InformePiso inf_tapete;
        ModeloPiso mod_tapete;
        ResultadoObstaculos r_tapete;
        int i, x, y, falsos_tapete = 0;

        obstaculos_aprender_iniciar(&ac_tapete);
        for (i = 0; i < 6; i++) {
            EscenaSintetica e = base(PS_LISO);

            e.sigma = 4; e.semilla = (unsigned long)(i + 1) * 73UL;
            ps_dibujar(&e, imagen);
            /* Tapete / segunda loseta que abarca todo el ancho (x = 0..PS_ANCHO) solo en filas 70..115 (arriba de filas 8 y 9). */
            for (y = 70; y < 115; y++) {
                for (x = 0; x < PS_ANCHO; x++) {
                    imagen[(y * PS_ANCHO + x) * 3] = 60;
                    imagen[(y * PS_ANCHO + x) * 3 + 1] = 95;
                    imagen[(y * PS_ANCHO + x) * 3 + 2] = 140;
                }
            }
            obstaculos_aprender_cuadro(&ac_tapete, &cfg, imagen, PS_ANCHO, PS_ALTO);
        }
        obstaculos_modelo_iniciar(&mod_tapete);
        obstaculos_aprender_terminar(&ac_tapete, &cfg, &mod_tapete, &inf_tapete);
        printf("     tapete lejos (todo el ancho): grupos=%d descartado=%d %% muy_variado=%d\n",
               inf_tapete.grupos, inf_tapete.descartado_pct, inf_tapete.muy_variado);
        caso("piso de 2 colores con segundo color lejos en franja continua: se acepta en el modelo (grupos >= 2), descartado_pct <= 5 % y muy_variado = 0",
             inf_tapete.grupos >= 2 && inf_tapete.descartado_pct <= 5 && inf_tapete.muy_variado == 0);

        /* Comprobar falsos positivos al analizar el piso con el tapete lejos: con el modelo aprendido no debe marcar obstáculo. */
        for (i = 0; i < 10; i++) {
            EscenaSintetica e = base(PS_LISO);

            e.sigma = 4; e.semilla = (unsigned long)(i + 50) * 37UL;
            ps_dibujar(&e, imagen);
            for (y = 70; y < 115; y++) {
                for (x = 0; x < PS_ANCHO; x++) {
                    imagen[(y * PS_ANCHO + x) * 3] = 60;
                    imagen[(y * PS_ANCHO + x) * 3 + 1] = 95;
                    imagen[(y * PS_ANCHO + x) * 3 + 2] = 140;
                }
            }
            obstaculos_analizar_modelo(&cfg, &mod_tapete, imagen, PS_ANCHO, PS_ALTO, 0, (unsigned int)(i * 80), &r_tapete);
            if (r_tapete.zona[ZONA_IZQUIERDA].hay_obstaculo || r_tapete.zona[ZONA_CENTRO].hay_obstaculo || r_tapete.zona[ZONA_DERECHA].hay_obstaculo) {
                falsos_tapete++;
            }
        }
        caso("... y al recorrer ese piso de 2 colores: 0 falsos positivos (el tapete no se marca como obstaculo)", falsos_tapete == 0);
    }

    /* J10c: caja muy ancha de otro color en las filas cercanas al carro (casi todo el ancho de la imagen).
     * No debe aceptarse como segundo color de piso (debe dar descartado_pct > 5 % y muy_variado = 1),
     * y debe contar como OBSTÁCULO en la zona centro. */
    {
        static AcumuladorPiso ac_ancha;
        InformePiso inf_ancha;
        ModeloPiso mod_ancha;
        ResultadoObstaculos r_ancha;
        int i, x, y;

        obstaculos_aprender_iniciar(&ac_ancha);
        for (i = 0; i < 6; i++) {
            EscenaSintetica e = base(PS_LISO);

            e.sigma = 4; e.semilla = (unsigned long)(i + 1) * 89UL;
            ps_dibujar(&e, imagen);
            /* Caja muy ancha (x = 10..230 de 240, casi todo el ancho) en filas cercanas al carro (y = 95..136 de 176, filas 5 a 7). */
            for (y = 95; y < 136; y++) {
                for (x = 10; x < 230; x++) {
                    imagen[(y * PS_ANCHO + x) * 3] = 40;
                    imagen[(y * PS_ANCHO + x) * 3 + 1] = 190;
                    imagen[(y * PS_ANCHO + x) * 3 + 2] = 50;
                }
            }
            obstaculos_aprender_cuadro(&ac_ancha, &cfg, imagen, PS_ANCHO, PS_ALTO);
        }
        obstaculos_modelo_iniciar(&mod_ancha);
        obstaculos_aprender_terminar(&ac_ancha, &cfg, &mod_ancha, &inf_ancha);
        printf("     caja muy ancha en filas cercanas: grupos=%d descartado=%d %% muy_variado=%d\n",
               inf_ancha.grupos, inf_ancha.descartado_pct, inf_ancha.muy_variado);
        caso("J10c: caja muy ancha en filas cercanas NO se acepta como segundo color de piso (descartado > 5 %, muy_variado = 1)",
             inf_ancha.grupos == 1 && inf_ancha.descartado_pct > 5 && inf_ancha.muy_variado == 1);

        /* Comprobar que en la detección cuenta como obstáculo en zona centro */
        {
            EscenaSintetica e = base(PS_LISO);
            e.sigma = 4; e.semilla = 123UL;
            ps_dibujar(&e, imagen);
            for (y = 95; y < 136; y++) {
                for (x = 10; x < 230; x++) {
                    imagen[(y * PS_ANCHO + x) * 3] = 40;
                    imagen[(y * PS_ANCHO + x) * 3 + 1] = 190;
                    imagen[(y * PS_ANCHO + x) * 3 + 2] = 50;
                }
            }
            obstaculos_analizar_modelo(&cfg, &mod_ancha, imagen, PS_ANCHO, PS_ALTO, 0, 100, &r_ancha);
            caso("... y la caja muy ancha cuenta como obstaculo en la zona centro",
                 r_ancha.zona[ZONA_CENTRO].hay_obstaculo == 1);
        }
    }

    /* J10 Punto 2: tras terminar el aprendizaje (en CONFIRMAR y CUENTA), con piso_fijo = 1 una pared o caja uniforme durante > 1 s NO sobrescribe el piso. */
    {
        static AcumuladorPiso ac;
        InformePiso inf;
        ModeloPiso mod_sin_fijo, mod_con_fijo;
        ConfigObstaculos cfg_sin = cfg, cfg_con = cfg;
        ResultadoObstaculos r_sin, r_con;
        int i;

        obstaculos_aprender_iniciar(&ac);
        for (i = 0; i < 6; i++) {
            EscenaSintetica e = base(PS_LISO);

            e.sigma = 4; e.semilla = (unsigned long)(i + 1) * 19UL;
            ps_dibujar(&e, imagen);
            obstaculos_aprender_cuadro(&ac, &cfg, imagen, PS_ANCHO, PS_ALTO);
        }
        obstaculos_modelo_iniciar(&mod_sin_fijo);
        obstaculos_aprender_terminar(&ac, &cfg, &mod_sin_fijo, &inf);
        mod_con_fijo = mod_sin_fijo;
        cfg_sin.piso_fijo = 0;
        cfg_con.piso_fijo = 1;
        /* Durante 1.5 s (CONFIRMAR / CUENTA) una caja o pared uniforme tapa toda la vista (franja incluida). */
        for (i = 1; i <= 15; i++) {
            int k;

            for (k = 0; k < PS_ANCHO * PS_ALTO; k++) {
                imagen[k * 3] = 200;
                imagen[k * 3 + 1] = 45;
                imagen[k * 3 + 2] = 40;
            }
            obstaculos_analizar_modelo(&cfg_sin, &mod_sin_fijo, imagen, PS_ANCHO, PS_ALTO, 0, (unsigned int)(i * 100), &r_sin);
            obstaculos_analizar_modelo(&cfg_con, &mod_con_fijo, imagen, PS_ANCHO, PS_ALTO, 0, (unsigned int)(i * 100), &r_con);
        }
        caso("tras aprender el piso (CONFIRMAR y CUENTA): sin piso_fijo una pared uniforme sobrescribe el piso en 1 s; con piso_fijo = 1 NO lo sobrescribe y sigue viendo obstaculo",
             mod_sin_fijo.reaprendidos >= 1 && mod_con_fijo.reaprendidos == 0 && r_con.zona[ZONA_CENTRO].hay_obstaculo == 1);
    }
}


/* Otras reglas del detector nuevo. */
static void pruebas_reglas(void)
{
    ConfigObstaculos cfg;
    ResultadoObstaculos r;
    ModeloPiso modelo;
    char texto[80];
    int i, codigo, falsos_con_fijo = 0, falsos_sin_fijo = 0;

    /* Configuración: los dos campos nuevos solo aceptan 0 o 1. */
    obstaculos_config_defecto(&cfg);
    caso("por defecto: mejoras_piso = 1 y piso_fijo = 0; tira de referencia de 800 milesimas", cfg.mejoras_piso == 1 && cfg.piso_fijo == 0 && cfg.ref_ancho_mil == 800);
    cfg.mejoras_piso = 2;
    ps_dibujar(&(EscenaSintetica){ PS_LISO, 4, 0, 0, 100, PS_NADA, 0, 0, 0, 0, 0, 0, 0, 1 }, imagen);
    obstaculos_modelo_iniciar(&modelo);
    caso("mejoras_piso = 2 no es valido: OBS_ERR_CONFIG", obstaculos_analizar_modelo(&cfg, &modelo, imagen, PS_ANCHO, PS_ALTO, 0, 0, &r) == OBS_ERR_CONFIG);
    obstaculos_config_defecto(&cfg);
    cfg.piso_fijo = -1;
    caso("piso_fijo = -1 no es valido: OBS_ERR_CONFIG", obstaculos_analizar_modelo(&cfg, &modelo, imagen, PS_ANCHO, PS_ALTO, 0, 0, &r) == OBS_ERR_CONFIG);

    /* Un piso que CAMBIA a media ruta: con piso_fijo no se reaprende (la ruta no cambia de piso); sin piso_fijo se reaprende en ~1 s. */
    for (i = 0; i < 2; i++) {
        EscenaSintetica e = base(PS_LISO);
        int cuadro, falsos = 0;

        obstaculos_config_defecto(&cfg);
        cfg.piso_fijo = i;
        obstaculos_modelo_iniciar(&modelo);
        for (cuadro = 0; cuadro < 40; cuadro++) {
            e.sigma = 4;
            e.piso = (cuadro < 5) ? PS_LISO : PS_NEGRO;   /* de golpe, otro piso (negro: no se parece al anterior) */
            e.semilla = (unsigned long)(cuadro + 1) * 131UL;
            ps_dibujar(&e, imagen);
            codigo = obstaculos_analizar_modelo(&cfg, &modelo, imagen, PS_ANCHO, PS_ALTO, 0, (unsigned int)(cuadro * 80), &r);
            if (cuadro >= 25 && codigo == OBS_OK && (r.zona[0].hay_obstaculo || r.zona[1].hay_obstaculo || r.zona[2].hay_obstaculo)) {
                falsos++;
            }
        }
        if (i) {
            falsos_con_fijo = falsos;
        } else {
            falsos_sin_fijo = falsos;
        }
    }
    printf("     el piso cambia de golpe en el cuadro 5: cuadros 25 a 39 con obstaculo: sin piso_fijo %d, con piso_fijo %d\n", falsos_sin_fijo, falsos_con_fijo);
    caso("piso que cambia a media ruta: sin piso_fijo se reaprende (a los 25 cuadros ya no hay falsos obstaculos)", falsos_sin_fijo == 0);
    caso("piso_fijo = 1: NO se reaprende el piso a media ruta (sigue marcando obstaculo: el carro no cambia de piso por su cuenta)", falsos_con_fijo >= 10);

    /* Objetos con contraste fuerte en pisos extremos. */
    {
        EscenaSintetica e = base(PS_NEGRO);
        Cuenta c;
        ConfigObstaculos nuevo;

        config_nuevo(&nuevo);
        e.sigma = 5; e.objeto = PS_LIBRETA; e.obj_x = 90; e.obj_y = 100;
        c = correr(&nuevo, e, 20, 0, 0, 0, 0);
        caso("una libreta clara sobre el piso negro se detecta en los 20 cuadros", c.con_obstaculo == 20);
    }

    /* Motivos para el registro PISO_DIAG. */
    obstaculos_texto_motivo(0, texto, sizeof texto);
    caso("texto del motivo 0 = ninguno", strcmp(texto, "ninguno") == 0);
    obstaculos_texto_motivo(OBS_MOTIVO_OBSTACULO | OBS_MOTIVO_REAJUSTE | OBS_MOTIVO_PISO_OSCURO, texto, sizeof texto);
    caso("texto con varios motivos separados por +", strcmp(texto, "obstaculo+reajuste_brillo+piso_oscuro") == 0);
    obstaculos_texto_motivo(63, texto, 10);
    caso("con un buffer chico el texto se corta sin pasarse", strlen(texto) <= 9);

    /* Diagnóstico del cuadro. */
    {
        EscenaSintetica e = base(PS_NEGRO);

        obstaculos_config_defecto(&cfg);
        obstaculos_modelo_iniciar(&modelo);
        e.sigma = 4;
        ps_dibujar(&e, imagen);
        obstaculos_analizar_modelo(&cfg, &modelo, imagen, PS_ANCHO, PS_ALTO, 0, 0, &r);
        caso("PISO_DIAG: el piso negro da brillo medio bajo y el motivo piso_oscuro", r.brillo_medio < 30 && (r.motivo & OBS_MOTIVO_PISO_OSCURO) && r.ganancia_mil == 1000);
        e.objeto = PS_MOCHILA; e.obj_x = 70; e.obj_y = 75;
        ps_dibujar(&e, imagen);
        obstaculos_analizar_modelo(&cfg, &modelo, imagen, PS_ANCHO, PS_ALTO, 0, 80, &r);
        caso("PISO_DIAG: con la mochila hay celdas marcadas y el motivo obstaculo", r.celdas_marcadas > 20 && (r.motivo & OBS_MOTIVO_OBSTACULO));
    }
}

int main(void)
{
    pruebas_piso_negro();
    pruebas_piso_dos_colores();
    pruebas_exposicion();
    pruebas_objetos();
    pruebas_aprender();
    pruebas_reglas();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
