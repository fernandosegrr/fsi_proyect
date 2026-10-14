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
static void caja_lejos_en_camino(int clase, int confianza_mil)
{
    agregar_caja(clase, confianza_mil, 450, 400, 550, 500);
}

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

static void pruebas_detector_manda(void)
{
    int codigo;

    fusion_config_defecto(&cfg);

    detector_libre();
    modelo_vacio(1000);
    codigo = decidir(1100);
    caso("sin nada del detector y sin cajas: severidad NINGUNA, fuente NINGUNA", codigo == FUS_OK && sal.severidad == FUS_SEV_NINGUNA &&
         sal.fuente == FUS_FUENTE_NINGUNA && sal.aviso == 0);

    detector_libre();
    detector_ve(ZONA_CENTRO, 3);
    codigo = fusion_decidir(&cfg, &det, NULL, 1000, &sal);
    caso("el detector ve un obstaculo muy cerca y no hay modelo (NULL): PARAR, fuente detector, no espera al modelo",
         codigo == FUS_OK && sal.severidad == FUS_SEV_PARAR && sal.fuente == FUS_FUENTE_DETECTOR && sal.detector_solo_decide == 1 &&
         sal.zona[ZONA_CENTRO].hay_obstaculo == 1 && sal.zona[ZONA_CENTRO].cercania == 3 && sal.modelo_vigente == 0);

    modelo_vacio(1000);
    codigo = decidir(1100);
    caso("lo mismo con un modelo vacio (0 cajas)", sal.severidad == FUS_SEV_PARAR && sal.fuente == FUS_FUENTE_DETECTOR && sal.detector_solo_decide == 1);

    /* Contradiccion: el modelo no ve nada en el camino (solo una persona lejos a la izquierda). */
    modelo_vacio(1000);
    agregar_caja(0, 900, 50, 300, 200, 500);
    codigo = decidir(1100);
    caso("CONTRADICCION: el modelo solo ve una persona lejos a la izquierda; el detector ve algo muy cerca al centro: se frena igual",
         sal.severidad == FUS_SEV_PARAR && sal.zona[ZONA_CENTRO].cercania == 3 && sal.fuente == FUS_FUENTE_DETECTOR && sal.detector_solo_decide == 1);
    caso("... y el modelo avisa de la persona (de interes) a la izquierda", sal.aviso == 1 && sal.aviso_clase == 0 && sal.aviso_zona == ZONA_IZQUIERDA);

    /* Contradiccion: el modelo ve el objeto en el camino pero "lejos" (c=1) y el detector lo ve muy cerca. */
    modelo_vacio(1000);
    caja_lejos_en_camino(60, 900);
    codigo = decidir(1100);
    caso("CONTRADICCION: el modelo lo ve lejos (cercania 1) y el detector muy cerca (3): la fusion NUNCA baja lo del detector",
         sal.zona[ZONA_CENTRO].cercania == 3 && sal.severidad == FUS_SEV_PARAR && sal.fuente == FUS_FUENTE_AMBOS);

    /* Detector a media distancia a un lado. */
    detector_libre();
    detector_ve(ZONA_IZQUIERDA, 2);
    modelo_vacio(1000);
    decidir(1100);
    caso("el detector ve algo a media distancia a la izquierda: FRENAR y el detector solo ya decide", sal.severidad == FUS_SEV_FRENAR && sal.detector_solo_decide == 1 &&
         sal.zona[ZONA_IZQUIERDA].cercania == 2);
    detector_libre();
    detector_ve(ZONA_DERECHA, 1);
    decidir(1100);
    caso("el detector ve algo lejos (cercania 1): solo AVISO y detector_solo_decide = 0 (no hace falta frenar)", sal.severidad == FUS_SEV_AVISO &&
         sal.detector_solo_decide == 0 && sal.fuente == FUS_FUENTE_DETECTOR);
}

/* ── Modelo caducado ───────────────────────────────────────────────────── */

static void pruebas_caducidad(void)
{
    fusion_config_defecto(&cfg);
    detector_libre();
    detector_duda(ZONA_CENTRO, 3);   /* el detector duda: el modelo podria subirlo si fuera vigente */

    modelo_vacio(1000);
    caja_cerca_en_camino(60, 900);
    decidir(1300);
    caso("modelo con 300 ms de edad (el limite): se usa (vigente) y sube lo que el detector dudaba", sal.modelo_vigente == 1 && sal.edad_modelo_ms == 300 &&
         sal.zona[ZONA_CENTRO].cercania == 3 && sal.fuente == FUS_FUENTE_AMBOS);
    decidir(1301);
    caso("modelo con 301 ms de edad: CADUCADO, se ignora por completo (sin cambios al detector)", sal.modelo_vigente == 0 && sal.edad_modelo_ms == 301 &&
         sal.zona[ZONA_CENTRO].hay_obstaculo == 0 && sal.severidad == FUS_SEV_NINGUNA && sal.fuente == FUS_FUENTE_NINGUNA && sal.aviso == 0);

    /* Caducado y el detector SI ve: manda el detector. */
    detector_libre();
    detector_ve(ZONA_CENTRO, 2);
    modelo_vacio(1000);
    caja_cerca_en_camino(60, 900);
    decidir(2000);
    caso("modelo caducado (1000 ms) y el detector ve algo a media distancia: sale lo del detector (cercania 2), sin subirlo",
         sal.modelo_vigente == 0 && sal.zona[ZONA_CENTRO].cercania == 2 && sal.fuente == FUS_FUENTE_DETECTOR && sal.severidad == FUS_SEV_FRENAR);

    /* Vigencia configurable. */
    detector_libre();
    detector_duda(ZONA_CENTRO, 3);
    cfg.vigencia_ms = 100;
    modelo_vacio(1000);
    caja_cerca_en_camino(60, 900);
    decidir(1150);
    caso("con vigencia de 100 ms (ConfigFusion), un resultado de 150 ms ya esta caducado", sal.modelo_vigente == 0 && sal.zona[ZONA_CENTRO].hay_obstaculo == 0);
    cfg.vigencia_ms = 500;
    decidir(1450);
    caso("con vigencia de 500 ms, uno de 450 ms todavia vale", sal.modelo_vigente == 1 && sal.zona[ZONA_CENTRO].cercania == 3);
    fusion_config_defecto(&cfg);

    /* Reloj. */
    modelo_vacio(5000);
    caja_cerca_en_camino(60, 900);
    decidir(4000);
    caso("un modelo con hora FUTURA (reloj atrasado) se trata como edad 0 (no caduca)", sal.modelo_vigente == 1 && sal.edad_modelo_ms == 0);
    modelo_vacio(4294967200u);
    caja_cerca_en_camino(60, 900);
    decidir(50);
    caso("el reloj de 32 bits da la vuelta (4294967200 -> 50): edad 146 ms, vigente", sal.edad_modelo_ms == 146 && sal.modelo_vigente == 1);
    modelo_vacio(4294967000u);
    caja_cerca_en_camino(60, 900);
    decidir(100);
    caso("con la vuelta del reloj, 396 ms de edad: caducado", sal.edad_modelo_ms == 396 && sal.modelo_vigente == 0);
}

/* ── El modelo sube lo que el detector duda / ve algo que el detector no ve ─ */

static void pruebas_modelo_aporta(void)
{
    fusion_config_defecto(&cfg);

    /* Detector duda (3 celdas, menos del minimo). */
    detector_libre();
    detector_duda(ZONA_CENTRO, 3);
    modelo_vacio(1000);
    caja_cerca_en_camino(60, 700);
    decidir(1100);
    caso("el detector duda (3 celdas) y el modelo ve un objeto muy cerca en el camino: SUBE a cercania 3, fuente AMBOS, PARAR",
         sal.zona[ZONA_CENTRO].hay_obstaculo == 1 && sal.zona[ZONA_CENTRO].cercania == 3 && sal.fuente == FUS_FUENTE_AMBOS &&
         sal.severidad == FUS_SEV_PARAR && sal.detector_solo_decide == 0 && sal.clase_en_camino == 60);

    /* Detector ve algo mas lejos que el modelo. */
    detector_libre();
    detector_ve(ZONA_CENTRO, 1);
    modelo_vacio(1000);
    caja_cerca_en_camino(1, 800);
    decidir(1100);
    caso("el detector ve algo lejos (1) y el modelo lo ve muy cerca (3): se sube a 3, fuente AMBOS", sal.zona[ZONA_CENTRO].cercania == 3 &&
         sal.fuente == FUS_FUENTE_AMBOS);

    /* Detector ve mas cerca que el modelo: no se baja. */
    detector_libre();
    detector_ve(ZONA_CENTRO, 3);
    modelo_vacio(1000);
    agregar_caja(1, 800, 400, 300, 600, 720);   /* cercania 2 */
    decidir(1100);
    caso("el detector lo ve muy cerca (3) y el modelo a media distancia (2): se queda en 3 (no se baja), AMBOS", sal.zona[ZONA_CENTRO].cercania == 3 &&
         sal.fuente == FUS_FUENTE_AMBOS);

    /* Detector no ve NADA: el modelo ve algo cercano y muy seguro. */
    detector_libre();
    modelo_vacio(1000);
    caja_cerca_en_camino(62, 900);
    decidir(1100);
    caso("el detector no ve NADA y el modelo ve una silla muy cerca con confianza 0.9: FRENA solo por el modelo (PARAR, fuente MODELO)",
         sal.zona[ZONA_CENTRO].hay_obstaculo == 1 && sal.zona[ZONA_CENTRO].cercania == 3 && sal.fuente == FUS_FUENTE_MODELO &&
         sal.severidad == FUS_SEV_PARAR && sal.detector_solo_decide == 0 && sal.clase_en_camino == 62);

    /* El caso "solo avisa" existe solo si la confianza mínima es MENOR que la de frenar: por defecto las dos son 0.6, así que aquí se baja
     * la mínima a 0.4 a propósito (como era antes). */
    cfg.confianza_min_mil = 400;
    modelo_vacio(1000);
    caja_cerca_en_camino(62, 500);   /* menos seguro que 0.6 */
    decidir(1100);
    caso("lo mismo pero con confianza 0.5 (menos que la 0.6 para frenar solo por el modelo): NO frena, solo AVISA",
         sal.zona[ZONA_CENTRO].hay_obstaculo == 0 && sal.aviso == 1 && sal.severidad == FUS_SEV_AVISO && sal.aviso_cercania == 3 &&
         sal.fuente == FUS_FUENTE_MODELO);

    /* Objeto lejano que el detector no ve: solo avisa. */
    modelo_vacio(1000);
    caja_lejos_en_camino(0, 950);
    decidir(1100);
    caso("el modelo ve una persona LEJOS en el camino (cercania 1) que el detector no ve: SOLO AVISA, no frena (zonas libres)",
         sal.zona[ZONA_CENTRO].hay_obstaculo == 0 && sal.aviso == 1 && sal.aviso_cercania == 1 && sal.severidad == FUS_SEV_AVISO &&
         sal.detector_solo_decide == 0);

    /* Confianza minima. */
    modelo_vacio(1000);
    caja_cerca_en_camino(62, 300);
    decidir(1100);
    caso("una caja con confianza 0.3 (menos que la minima 0.4) no cuenta ni como aviso", sal.zona[ZONA_CENTRO].hay_obstaculo == 0 && sal.aviso == 0 &&
         sal.severidad == FUS_SEV_NINGUNA);

    cfg.confianza_min_mil = 600;   /* de vuelta al valor por defecto */

    /* Varias cajas: gana la mas cercana. */
    detector_libre();
    detector_duda(ZONA_CENTRO, 2);
    modelo_vacio(1000);
    caja_lejos_en_camino(60, 900);
    caja_cerca_en_camino(61, 900);
    decidir(1100);
    caso("con dos cajas en el camino (lejos y muy cerca) gana la mas cercana: cercania 3 y clase 61", sal.zona[ZONA_CENTRO].cercania == 3 && sal.clase_en_camino == 61);

    /* Mas cajas de las que caben. */
    modelo_vacio(1000);
    mod.n = FUS_MAX_CAJAS + 5;   /* un n absurdo: no se lee fuera del arreglo */
    memset(mod.cajas, 0, sizeof mod.cajas);
    decidir(1100);
    caso("un modelo con n mayor que FUS_MAX_CAJAS no se sale del arreglo (se lee solo hasta FUS_MAX_CAJAS)", sal.modelo_vigente == 1);
}

/* ── Clases de interés y cualquier otra clase ──────────────────────────── */

static void pruebas_clases(void)
{
    fusion_config_defecto(&cfg);
    detector_libre();

    modelo_vacio(1000);
    agregar_caja(0, 900, 50, 300, 200, 700);   /* persona a la izquierda, fuera del camino */
    decidir(1100);
    caso("una persona (clase de interes) fuera del camino genera un AVISO en su zona y no frena", sal.aviso == 1 && sal.aviso_clase == 0 &&
         sal.aviso_zona == ZONA_IZQUIERDA && sal.zona[ZONA_IZQUIERDA].hay_obstaculo == 0 && sal.severidad == FUS_SEV_AVISO);

    modelo_vacio(1000);
    agregar_caja(77, 900, 50, 300, 200, 700);   /* una clase que no es de interes, fuera del camino */
    decidir(1100);
    caso("un objeto de una clase que NO es de interes fuera del camino se ignora (ni aviso)", sal.aviso == 0 && sal.severidad == FUS_SEV_NINGUNA);

    modelo_vacio(1000);
    caja_cerca_en_camino(77, 900);
    decidir(1100);
    caso("CUALQUIER clase cuenta igual en el camino: una clase 77 (no es de interes) muy cerca frena", sal.zona[ZONA_CENTRO].cercania == 3 &&
         sal.severidad == FUS_SEV_PARAR && sal.clase_en_camino == 77);

    fusion_marcar_clase(&cfg, 0, 0);
    modelo_vacio(1000);
    agregar_caja(0, 900, 50, 300, 200, 700);
    decidir(1100);
    caso("si se quita 'persona' de las clases de interes, la persona a un lado ya no avisa", sal.aviso == 0);
    fusion_marcar_clase(&cfg, 77, 1);
    modelo_vacio(1000);
    agregar_caja(77, 900, 800, 300, 950, 700);
    decidir(1100);
    caso("si se agrega la clase 77, un objeto 77 a la derecha avisa", sal.aviso == 1 && sal.aviso_clase == 77 && sal.aviso_zona == ZONA_DERECHA);
    fusion_config_defecto(&cfg);

    /* Un aviso no cambia lo que ve el detector en esa zona. */
    detector_libre();
    detector_ve(ZONA_IZQUIERDA, 2);
    modelo_vacio(1000);
    agregar_caja(0, 900, 50, 300, 200, 700);
    decidir(1100);
    caso("un aviso a un lado no cambia lo que el detector ve en esa zona (cercania 2 sigue)", sal.zona[ZONA_IZQUIERDA].cercania == 2 && sal.aviso == 1);
}

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

static void pruebas_linea_de_tiempo(void)
{
    unsigned int t;
    unsigned int primer_parar = 0, primer_subida_modelo = 0, ultimo_modelo_vigente = 0;
    DeteccionesModelo ultimo;
    int hay_modelo = 0;
    unsigned int proxima_inferencia = 0;

    fusion_config_defecto(&cfg);
    memset(&ultimo, 0, sizeof ultimo);

    /* El objeto aparece a los 1000 ms. El detector lo ve cercano (3) desde el cuadro de 1040 ms. El modelo lo ve
     * (muy cerca, sin que el detector lo vea) en su resultado que termina a los 1080 ms (los resultados del modelo
     * llegan cada 120 ms). El objeto desaparece a los 2000 ms. */
    for (t = 0; t <= 3000; t += 80) {
        detector_libre();
        if (t >= 1040 && t < 2000) {
            detector_ve(ZONA_CENTRO, 3);
        }
        if (t >= proxima_inferencia) {   /* el modelo termina una inferencia con lo que ve en este momento */
            modelo_vacio(t);
            if (t >= 1000 && t < 2000) {
                caja_cerca_en_camino(62, 900);
            }
            ultimo = mod;
            hay_modelo = 1;
            proxima_inferencia = t + 120;
        }
        fusion_decidir(&cfg, &det, hay_modelo ? &ultimo : NULL, t, &sal);
        if (sal.severidad == FUS_SEV_PARAR && primer_parar == 0) {
            primer_parar = t;
        }
        if (sal.fuente == FUS_FUENTE_MODELO && primer_subida_modelo == 0) {
            primer_subida_modelo = t;
        }
        if (sal.modelo_vigente) {
            ultimo_modelo_vigente = t;
        }
    }
    printf("      linea de tiempo: primer PARAR a los %u ms; primera vez que decide solo el modelo a los %u ms; ultimo cuadro con modelo vigente a los %u ms\n",
           primer_parar, primer_subida_modelo, ultimo_modelo_vigente);
    caso("linea de tiempo: se manda PARAR en el primer cuadro con el obstaculo visto por el detector o el modelo (1040 ms o antes)", primer_parar > 0 && primer_parar <= 1040);

    /* Con el modelo dejando de contestar (se cuelga): su ultimo resultado caduca y el detector sigue mandando solo. */
    detector_libre();
    detector_ve(ZONA_CENTRO, 3);
    modelo_vacio(5000);
    caja_cerca_en_camino(62, 900);
    fusion_decidir(&cfg, &det, &mod, 5100, &sal);
    caso("con el modelo vigente y el detector de acuerdo: fuente AMBOS", sal.fuente == FUS_FUENTE_AMBOS && sal.severidad == FUS_SEV_PARAR);
    fusion_decidir(&cfg, &det, &mod, 6000, &sal);
    caso("el modelo se cuelga (su resultado ya tiene 1000 ms): el detector sigue decidiendo solo y frena igual",
         sal.modelo_vigente == 0 && sal.fuente == FUS_FUENTE_DETECTOR && sal.severidad == FUS_SEV_PARAR && sal.detector_solo_decide == 1);
}

int main(void)
{
    pruebas_configuracion();
    pruebas_cajas();
    pruebas_confianza_minima();
    pruebas_detector_manda();
    pruebas_caducidad();
    pruebas_modelo_aporta();
    pruebas_clases();
    pruebas_errores();
    pruebas_linea_de_tiempo();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
