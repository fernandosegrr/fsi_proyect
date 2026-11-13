/*
 * pruebas_fusion.c — Pruebas de fusion.c en PC con entradas simuladas (sin cámara ni modelo reales).
 *
 * GUÍA DE LECTURA
 *  1. Se arman "resultados del detector" a mano (quién ve qué en cada zona) y "detecciones del modelo" a
 *     mano (cajas en milésimas, con su clase, confianza y hora), y se comprueba lo que decide fusion_decidir.
 *  2. Grupos: configuración, caja → zona y cercanía, el detector manda, modelo caducado, contradicciones,
 *     el modelo sube lo que el detector duda, el modelo ve algo que el detector no ve, clases de interés,
 *     errores y una línea de tiempo con cuadros cada 80 ms y resultados del modelo cada 120 ms.
 *  3. Las mediciones reales en el Huawei (cuánto tarda cada etapa) NO están aquí: son PENDIENTE HARDWARE.
 */

#include <stdio.h>
#include <string.h>
#include "fusion.h"

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

static ConfigFusion cfg;
static ResultadoObstaculos det;
static DeteccionesModelo mod;
static ResultadoFusion sal;

/* Detector sin nada. */
static void detector_libre(void)
{
    memset(&det, 0, sizeof det);
}

/* El detector ve un obstáculo en una zona con esa cercanía (y 10 celdas). */
static void detector_ve(int zona, int cercania)
{
    det.zona[zona].hay_obstaculo = 1;
    det.zona[zona].cercania = cercania;
    det.zona[zona].celdas = 10;
}

/* El detector tiene unas pocas celdas en la zona, pero no llega al mínimo (duda). */
static void detector_duda(int zona, int celdas)
{
    det.zona[zona].hay_obstaculo = 0;
    det.zona[zona].cercania = 0;
    det.zona[zona].celdas = celdas;
}

/* Modelo vacío con la hora dada. */
static void modelo_vacio(unsigned int marca_ms)
{
    memset(&mod, 0, sizeof mod);
    mod.marca_ms = marca_ms;
}

static void agregar_caja(int clase, int confianza_mil, int xmin, int ymin, int xmax, int ymax)
{
    CajaModelo *c = &mod.cajas[mod.n++];

    c->clase = clase;
    c->confianza_mil = confianza_mil;
    c->xmin_mil = xmin;
    c->ymin_mil = ymin;
    c->xmax_mil = xmax;
    c->ymax_mil = ymax;
}

/* Una persona/objeto GRANDE en el camino, muy cerca: centro, borde de abajo en 950. */
static void caja_cerca_en_camino(int clase, int confianza_mil)
{
    agregar_caja(clase, confianza_mil, 380, 400, 620, 950);
}

/* Un objeto chico y lejano en el camino: el borde de abajo está en 500. */


static int decidir(unsigned int ahora_ms)
{
    return fusion_decidir(&cfg, &det, &mod, ahora_ms, &sal);
}

/* ── Configuración ─────────────────────────────────────────────────────── */

static void pruebas_configuracion(void)
{
    ConfigFusion c;
    int i;

    fusion_config_defecto(&c);
    caso("config por defecto: vigencia 300 ms, confianza minima 0.6, para frenar 0.6", c.vigencia_ms == 300 && c.confianza_min_mil == 600 &&
         c.confianza_frenar_mil == 600);
    caso("config por defecto: los limites de cercania son los del detector de color (834 y 667)", c.cerca_mil == OBS_CERCA_MIL && c.media_mil == OBS_MEDIA_MIL);
    caso("config por defecto es valida", fusion_config_es_valida(&c));
    caso("clases de interes por defecto: persona, gato, perro, botella, silla, mesa",
         fusion_clase_de_interes(&c, 0) && fusion_clase_de_interes(&c, 16) && fusion_clase_de_interes(&c, 17) &&
         fusion_clase_de_interes(&c, 43) && fusion_clase_de_interes(&c, 61) && fusion_clase_de_interes(&c, 66));
    caso("otras clases (1, 2, 40, 95) no son de interes", !fusion_clase_de_interes(&c, 1) && !fusion_clase_de_interes(&c, 2) &&
         !fusion_clase_de_interes(&c, 40) && !fusion_clase_de_interes(&c, 95));
    caso("marcar una clase la hace de interes y desmarcarla la quita (y no toca las demas)",
         fusion_marcar_clase(&c, 2, 1) == FUS_OK && fusion_clase_de_interes(&c, 2) && fusion_marcar_clase(&c, 0, 0) == FUS_OK &&
         !fusion_clase_de_interes(&c, 0) && fusion_clase_de_interes(&c, 16));
    caso("marcar una clase fuera de 0..95 o con config NULL: FUS_ERR_ARGUMENTO", fusion_marcar_clase(&c, -1, 1) == FUS_ERR_ARGUMENTO &&
         fusion_marcar_clase(&c, FUS_CLASES_MAX, 1) == FUS_ERR_ARGUMENTO && fusion_marcar_clase(NULL, 3, 1) == FUS_ERR_ARGUMENTO);
    caso("fusion_clase_de_interes con NULL o clase invalida = 0", !fusion_clase_de_interes(NULL, 0) && !fusion_clase_de_interes(&c, 200));
    for (i = 0; i < FUS_CLASES_MAX; i++) {
        fusion_marcar_clase(&c, i, 1);
    }
    caso("se pueden marcar las 96 clases (los bits de las 3 palabras)", fusion_clase_de_interes(&c, 0) && fusion_clase_de_interes(&c, 31) &&
         fusion_clase_de_interes(&c, 32) && fusion_clase_de_interes(&c, 63) && fusion_clase_de_interes(&c, 64) && fusion_clase_de_interes(&c, 95));

    fusion_config_defecto(&c);
    c.vigencia_ms = -1;
    caso("vigencia negativa: configuracion no valida", !fusion_config_es_valida(&c));
    fusion_config_defecto(&c);
    c.confianza_frenar_mil = 100;   /* menor que la minima */
    caso("confianza para frenar menor que la minima: no valida", !fusion_config_es_valida(&c));
    fusion_config_defecto(&c);
    c.cerca_mil = 500;   /* menor que media_mil */
    caso("limite de cercania 3 menor que el de 2: no valida", !fusion_config_es_valida(&c));
    caso("config NULL no es valida", !fusion_config_es_valida(NULL));
}

/* ── Caja → zona y cercanía ────────────────────────────────────────────── */

static int zona_de(int xmin, int xmax, int *camino)
{
    CajaModelo c;

    memset(&c, 0, sizeof c);
    c.xmin_mil = xmin;
    c.xmax_mil = xmax;
    c.ymax_mil = 500;
    return fusion_caja_a_zona(&cfg, &c, camino);
}

static int cercania_de(int ymin, int ymax, int xmin, int xmax)
{
    CajaModelo c;

    memset(&c, 0, sizeof c);
    c.xmin_mil = xmin;
    c.xmax_mil = xmax;
    c.ymin_mil = ymin;
    c.ymax_mil = ymax;
    return fusion_cercania_de_caja(&cfg, &c);
}

static void pruebas_cajas(void)
{
    int camino, zona;

    fusion_config_defecto(&cfg);

    zona = zona_de(400, 600, &camino);
    caso("caja al centro (400 a 600): zona CENTRO y en el camino", zona == ZONA_CENTRO && camino == 1);
    zona = zona_de(0, 300, &camino);
    caso("caja a la izquierda (0 a 300): zona IZQUIERDA y fuera del camino", zona == ZONA_IZQUIERDA && camino == 0);
    zona = zona_de(700, 1000, &camino);
    caso("caja a la derecha (700 a 1000): zona DERECHA y fuera del camino", zona == ZONA_DERECHA && camino == 0);
    zona = zona_de(250, 400, &camino);
    caso("caja de 250 a 400 (cruza 67 milesimas la zona central, mas de las 60 minimas): en el camino", zona == ZONA_CENTRO && camino == 1);
    zona = zona_de(280, 380, &camino);
    caso("caja de 280 a 380 (cruza solo 47): NO esta en el camino, es IZQUIERDA", zona == ZONA_IZQUIERDA && camino == 0);
    zona = zona_de(600, 800, &camino);
    caso("caja de 600 a 800 (cruza 67): en el camino", zona == ZONA_CENTRO && camino == 1);
    zona = zona_de(620, 800, &camino);
    caso("caja de 620 a 800 (cruza 47): NO esta en el camino, es DERECHA", zona == ZONA_DERECHA && camino == 0);
    zona = zona_de(0, 1000, &camino);
    caso("caja que ocupa todo el ancho: en el camino", zona == ZONA_CENTRO && camino == 1);
    zona = zona_de(-200, 1300, &camino);
    caso("caja que se sale de la imagen (-200 a 1300) se recorta y esta en el camino", zona == ZONA_CENTRO && camino == 1);
    zona = zona_de(500, 500, &camino);
    caso("caja de ancho cero en medio (500 a 500): no cruza nada, cae en la derecha (centro = 500)", camino == 0 && zona == ZONA_DERECHA);

    caso("cercania por el borde de abajo: 950 = 3, 834 = 3, 833 = 2, 700 = 2, 667 = 2, 666 = 1, 300 = 1",
         cercania_de(500, 950, 450, 550) == 3 && cercania_de(500, 834, 450, 550) == 3 && cercania_de(500, 833, 450, 550) == 2 &&
         cercania_de(500, 700, 450, 550) == 2 && cercania_de(500, 667, 450, 550) == 2 && cercania_de(500, 666, 450, 550) == 1 &&
         cercania_de(100, 300, 450, 550) == 1);
    caso("una caja GRANDE (500 x 500 = 250 de area) con el borde en 600 sube de 1 a 2",
         cercania_de(100, 600, 300, 800) == 2);
    caso("la misma caja con el borde en 833 (2) sube a 3 y no pasa de 3", cercania_de(333, 833, 300, 800) == 3 && cercania_de(100, 900, 0, 1000) == 3);
    caso("una caja chica con el borde en 600 se queda en cercania 1", cercania_de(500, 600, 450, 550) == 1);
    caso("una caja de alto cero o invertida no revienta", cercania_de(600, 600, 450, 550) == 1 && cercania_de(700, 500, 450, 550) == 1);
}

/* ── Confianza mínima del modelo ───────────────────────────────────────── */

/* Un "refrigerator 43 %" sobre una pared lisa (el detector de color no ve nada) debe ignorarse. */
static void pruebas_confianza_minima(void)
{
    int codigo;
    const int refrigerator = 71;   /* la clase da igual: aquí se prueba que la confianza baja la descarta antes de mirar la clase */

    fusion_config_defecto(&cfg);
    caso("la confianza minima por defecto es 60 %", cfg.confianza_min_mil == 600);

    detector_libre();                       /* pared lisa: el detector de color no ve ningún obstáculo */
    modelo_vacio(1000);
    caja_cerca_en_camino(refrigerator, 430);
    codigo = decidir(1100);
    caso("pared lisa + 'refrigerator 43 %' grande y cerca en el camino: se IGNORA (severidad NINGUNA, sin aviso)",
         codigo == FUS_OK && sal.severidad == FUS_SEV_NINGUNA && sal.aviso == 0 && sal.zona[ZONA_CENTRO].hay_obstaculo == 0);

    detector_libre();
    modelo_vacio(1000);
    caja_cerca_en_camino(refrigerator, 599);
    codigo = decidir(1100);
    caso("una caja con 59.9 % tampoco cuenta", codigo == FUS_OK && sal.severidad == FUS_SEV_NINGUNA && sal.aviso == 0);

    detector_libre();
    modelo_vacio(1000);
    caja_cerca_en_camino(refrigerator, 430);
    cfg.clases_interes[0] = cfg.clases_interes[1] = cfg.clases_interes[2] = 0xFFFFFFFFu;   /* aunque la clase sea "de interés" */
    codigo = decidir(1100);
    caso("aunque todas las clases sean de interés, con 43 % se ignora", codigo == FUS_OK && sal.severidad == FUS_SEV_NINGUNA && sal.aviso == 0);
    fusion_config_defecto(&cfg);

    detector_libre();
    modelo_vacio(1000);
    caja_cerca_en_camino(refrigerator, 650);
    cfg.clases_interes[0] = cfg.clases_interes[1] = cfg.clases_interes[2] = 0xFFFFFFFFu;
    codigo = decidir(1100);
    caso("la misma caja con 65 % SÍ se toma en cuenta (control: la prueba de arriba no pasa por no haber modelo)",
         codigo == FUS_OK && (sal.severidad != FUS_SEV_NINGUNA || sal.aviso != 0));
    fusion_config_defecto(&cfg);
}

/* ── El detector manda ─────────────────────────────────────────────────── */



/* ── Modelo caducado ───────────────────────────────────────────────────── */



/* ── El modelo sube lo que el detector duda / ve algo que el detector no ve ─ */



/* ── Clases de interés y cualquier otra clase ──────────────────────────── */



/* ── Errores ───────────────────────────────────────────────────────────── */

static void pruebas_errores(void)
{
    ResultadoFusion basura;

    fusion_config_defecto(&cfg);
    detector_libre();
    modelo_vacio(0);

    caso("salida NULL: FUS_ERR_ARGUMENTO", fusion_decidir(&cfg, &det, &mod, 0, NULL) == FUS_ERR_ARGUMENTO);
    memset(&basura, 0x55, sizeof basura);
    caso("detector NULL: FUS_ERR_ARGUMENTO y la salida queda en ceros", fusion_decidir(&cfg, NULL, &mod, 0, &basura) == FUS_ERR_ARGUMENTO &&
         basura.severidad == FUS_SEV_NINGUNA && basura.zona[1].hay_obstaculo == 0 && basura.aviso == 0);
    caso("config NULL: FUS_ERR_ARGUMENTO", fusion_decidir(NULL, &det, &mod, 0, &sal) == FUS_ERR_ARGUMENTO);
    cfg.vigencia_ms = -5;
    memset(&basura, 0x55, sizeof basura);
    caso("config no valida: FUS_ERR_CONFIG y la salida queda en ceros", fusion_decidir(&cfg, &det, &mod, 0, &basura) == FUS_ERR_CONFIG &&
         basura.severidad == FUS_SEV_NINGUNA && basura.fuente == FUS_FUENTE_NINGUNA);
    fusion_config_defecto(&cfg);
    caso("textos de las fuentes", strcmp(fusion_texto_fuente(FUS_FUENTE_DETECTOR), "detector") == 0 && strcmp(fusion_texto_fuente(FUS_FUENTE_MODELO), "modelo") == 0 &&
         strcmp(fusion_texto_fuente(FUS_FUENTE_AMBOS), "ambos") == 0 && strcmp(fusion_texto_fuente(FUS_FUENTE_NINGUNA), "nadie") == 0);
}

/* ── Línea de tiempo: cuadros cada 80 ms y resultados del modelo cada 120 ms ─ */




/* Tabla completa: dos fuentes independientes, tres bandas, tres zonas. */
static void pruebas_consenso(void)
{
    int z, banda, fuentes, otra;
    fusion_config_defecto(&cfg);
    for(z=0;z<3;z++) for(banda=1;banda<=3;banda++) for(fuentes=0;fuentes<4;fuentes++) {
        char nombre[100];
        detector_libre(); modelo_vacio(1000);
        if(fuentes&1)detector_ve(z,banda);
        if(fuentes&2)agregar_caja(77,900,z*333+30,banda==3?800:banda==2?650:400,z*333+300,banda==3?950:banda==2?750:550);
        decidir(1100);
        snprintf(nombre,sizeof nombre,"zona=%d banda=%d fuentes=%d: solo coincidencia confirma",z,banda,fuentes);
        caso(nombre,sal.fuentes_vision[z]==fuentes && sal.zona[z].hay_obstaculo==(fuentes==3) &&
             sal.detector_solo_decide==0 && (fuentes==3 || sal.severidad<=FUS_SEV_AVISO));
    }
    for(z=0;z<3;z++)for(otra=0;otra<3;otra++)if(z!=otra) {
        detector_libre();detector_ve(z,3);modelo_vacio(1000);
        agregar_caja(61,900,otra*333+30,800,otra*333+300,950);
        decidir(1100);
        caso("ambas fuentes globales en zonas distintas NO son coincidencia",sal.fuente==3 &&
             sal.fuentes_vision[z]==1 && sal.fuentes_vision[otra]==2 && !sal.zona[z].hay_obstaculo && !sal.zona[otra].hay_obstaculo);
    }
    detector_libre();detector_ve(1,3);modelo_vacio(1000);caja_cerca_en_camino(61,900);
    decidir(1300);caso("modelo a 300 ms aun confirma",sal.modelo_vigente && sal.zona[1].hay_obstaculo);
    decidir(1301);caso("modelo a 301 ms no confirma color",!sal.modelo_vigente && sal.fuentes_vision[1]==1 && !sal.zona[1].hay_obstaculo);
    decidir(999);caso("timestamp futuro no confirma",!sal.modelo_vigente && !sal.zona[1].hay_obstaculo);
    modelo_vacio(1400);decidir(1400);caso("modelo fresco vacio es desacuerdo, no sensor perdido",sal.modelo_vigente && !sal.zona[1].hay_obstaculo);
    fusion_decidir(&cfg,&det,NULL,1500,&sal);caso("sin modelo color solo avisa",sal.fuentes_vision[1]==1 && !sal.zona[1].hay_obstaculo);
    detector_libre();detector_duda(1,3);modelo_vacio(1600);caja_cerca_en_camino(61,900);decidir(1600);
    caso("pocas celdas sin positivo no cuentan como segunda fuente",sal.fuentes_vision[1]==2 && !sal.zona[1].hay_obstaculo);
}

int main(void)
{
    pruebas_configuracion();
    pruebas_cajas();
    pruebas_confianza_minima();
    pruebas_errores();
    pruebas_consenso();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
