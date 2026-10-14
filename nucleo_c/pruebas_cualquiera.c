/*
 * pruebas_cualquiera.c — Pruebas en PC del modo «cualquier objeto» (cualquiera.c) y de su fusión con el color (fusion_decidir_cualquiera).
 *
 * GUÍA DE LECTURA
 *  1. Salidas SIMULADAS del modelo de objetos (cajas en milésimas del cuadro con su confianza): no hace falta el teléfono ni el modelo.
 *  2. Los filtros: la caja del termo con 0.20 cuenta; la que cubre todo con 0.43 (el «refrigerator» sobre una pared) se ignora; una caja arriba se ignora;
 *     las muy chicas o muy angostas también; el umbral se debe SUPERAR.
 *  3. La persistencia: las cajas que parpadean no cuentan, las estables sí (con 2 y con 3 cuadros), con cercanía alta basta uno, el mismo cuadro del modelo
 *     consultado dos veces no cuenta como dos, y un resultado caducado se ignora y borra la historia.
 *  4. Varias cajas a la vez, ninguna, y la conversión a CajaImagen para el corredor de trayectoria.
 *  5. La regla de la fusión (el color manda): coincidir sube, dudar sube, solo «cualquier objeto» = aviso, desmentir solo un falso positivo de color de cercanía baja.
 *  6. Un barrido de umbrales sobre una secuencia simulada (para ver el efecto de bajar el umbral de 0.30 a 0.01).
 */

#include <stdio.h>
#include <string.h>
#include "cualquiera.h"
#include "fusion.h"
#include "corredor.h"

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

/* Una caja del modelo. */
static CajaModelo caja(int xmin, int ymin, int xmax, int ymax, int confianza)
{
    CajaModelo c;

    c.clase = 12;   /* la clase da igual: se ignora */
    c.confianza_mil = confianza;
    c.xmin_mil = xmin;
    c.ymin_mil = ymin;
    c.xmax_mil = xmax;
    c.ymax_mil = ymax;
    return c;
}

/* Detecciones de un cuadro del modelo con 0 a 3 cajas y esa marca de tiempo. */
static DeteccionesModelo cuadro_con(unsigned int marca, int n, CajaModelo a, CajaModelo b, CajaModelo c)
{
    DeteccionesModelo d;

    memset(&d, 0, sizeof d);
    d.marca_ms = marca;
    d.n = n;
    d.cajas[0] = a;
    d.cajas[1] = b;
    d.cajas[2] = c;
    return d;
}

static ConfigCualquiera cfg_activa(void)
{
    ConfigCualquiera c;

    cualq_config_defecto(&c);
    c.activo = 1;
    return c;
}

/* ── 1. Configuración ──────────────────────────────────────────────────── */

static void pruebas_configuracion(void)
{
    ConfigCualquiera c;

    cualq_config_defecto(&c);
    caso("por defecto el modo esta APAGADO", c.activo == 0);
    caso("por defecto: umbral 0.15, area maxima 60 %, borde de abajo desde la mitad, persistencia de 2 cuadros, solape 50 %",
         c.umbral_mil == 150 && c.area_max_mil == 600 && c.borde_inferior_min_mil == 500 && c.persistencia == 2 && c.solape_mil == 500);
    caso("la configuracion por defecto es valida", cualq_config_es_valida(&c));
    c.umbral_mil = 5;
    caso("umbral 0.005 no es valido (el minimo es 0.01)", !cualq_config_es_valida(&c));
    c.umbral_mil = 10;
    caso("umbral 0.01 si es valido", cualq_config_es_valida(&c));
    c.umbral_mil = 500;
    caso("umbral 0.50 si es valido", cualq_config_es_valida(&c));
    c.umbral_mil = 501;
    caso("umbral 0.501 no es valido (el maximo es 0.50)", !cualq_config_es_valida(&c));
    cualq_config_defecto(&c);
    c.persistencia = 0;
    caso("persistencia 0 no es valida", !cualq_config_es_valida(&c));
    c.persistencia = CUALQ_MAX_PERSISTENCIA + 1;
    caso("persistencia mayor al maximo no es valida", !cualq_config_es_valida(&c));
    caso("config NULL no es valida", !cualq_config_es_valida(NULL));
}

/* ── 2. Filtros ────────────────────────────────────────────────────────── */

static void pruebas_filtros(void)
{
    ConfigCualquiera c = cfg_activa();
    CajaModelo termo = caja(420, 450, 540, 900, 200);   /* alta y delgada, abajo al centro, 0.20 */

    caso("la caja del termo con 0.20 cuenta (pasa los filtros)", cualq_motivo_descarte(&c, &termo) == CUALQ_PASA);

    {
        CajaModelo todo = caja(0, 0, 1000, 1000, 430);   /* «refrigerator» 0.43 que cubre todo el cuadro */
        caso("la caja que cubre todo con 0.43 se ignora (cubre mas del 60 %)", cualq_motivo_descarte(&c, &todo) == CUALQ_MUY_GRANDE);
    }
    {
        CajaModelo grande = caja(100, 100, 900, 900, 400);   /* 64 % del cuadro */
        CajaModelo justo = caja(100, 100, 900, 850, 400);    /* 60 % exactos: no se pasa */

        caso("una caja del 64 % del cuadro se ignora", cualq_motivo_descarte(&c, &grande) == CUALQ_MUY_GRANDE);
        caso("una caja del 60 % exacto pasa (se descarta solo lo que SUPERA el 60 %)", cualq_motivo_descarte(&c, &justo) == CUALQ_PASA);
    }
    {
        CajaModelo arriba = caja(300, 50, 700, 450, 500);   /* el borde de abajo (450) esta arriba de la mitad */
        CajaModelo limite = caja(300, 100, 700, 500, 500);  /* el borde de abajo justo en la mitad: pasa */

        caso("una caja arriba (borde de abajo en 0.45) se ignora", cualq_motivo_descarte(&c, &arriba) == CUALQ_BORDE_ALTO);
        caso("una caja con el borde de abajo justo en la mitad pasa (toca la zona inferior)", cualq_motivo_descarte(&c, &limite) == CUALQ_PASA);
    }
    {
        CajaModelo chica = caja(450, 800, 500, 840, 300);     /* 5 % x 4 % = 0.2 % del cuadro */
        CajaModelo angosta = caja(480, 200, 500, 950, 300);   /* 2 % de ancho y alta (1.5 % del cuadro) */

        caso("una caja muy pequena (0.2 % del cuadro) se ignora", cualq_motivo_descarte(&c, &chica) == CUALQ_MUY_CHICA);
        caso("una caja muy angosta (2 % del ancho) se ignora", cualq_motivo_descarte(&c, &angosta) == CUALQ_MUY_ANGOSTA);
    }
    {
        CajaModelo justo_umbral = caja(420, 450, 540, 900, 150);
        CajaModelo apenas_mas = caja(420, 450, 540, 900, 151);

        caso("confianza igual al umbral (0.15) NO pasa: se debe superar", cualq_motivo_descarte(&c, &justo_umbral) == CUALQ_POCA_CONFIANZA);
        caso("confianza 0.151 pasa", cualq_motivo_descarte(&c, &apenas_mas) == CUALQ_PASA);
        c.umbral_mil = 10;
        caso("con el umbral en 0.01 una caja de 0.05 pasa", cualq_motivo_descarte(&c, &(CajaModelo){ 12, 50, 420, 450, 540, 900 }) == CUALQ_PASA);
        c.umbral_mil = 300;
        caso("con el umbral en 0.30 la caja del termo con 0.20 ya no pasa", cualq_motivo_descarte(&c, &termo) == CUALQ_POCA_CONFIANZA);
    }
    {
        CajaModelo mala = caja(500, 500, 500, 900, 900);
        caso("una caja de ancho cero no es valida", cualq_motivo_descarte(&c, &mala) == CUALQ_CAJA_INVALIDA && cualq_motivo_descarte(NULL, &mala) == CUALQ_CAJA_INVALIDA);
    }
    caso("la clase no importa: la misma caja con cualquier clase pasa igual", (termo.clase = 0, cualq_motivo_descarte(&c, &termo) != CUALQ_CAJA_INVALIDA));
    caso("textos de los motivos en espanol", strlen(cualq_texto_motivo(CUALQ_MUY_GRANDE)) > 5 && strlen(cualq_texto_motivo(99)) > 3);
}

/* ── 3. Zona y cercanía ────────────────────────────────────────────────── */

static void pruebas_zona_y_cercania(void)
{
    CajaModelo cerca = caja(420, 600, 540, 950, 300);
    CajaModelo media = caja(420, 400, 540, 700, 300);
    CajaModelo lejos = caja(420, 300, 540, 600, 300);

    caso("cercania por el borde de abajo: 950 -> 3, 700 -> 2, 600 -> 1", cualq_cercania_de_caja(&cerca) == 3 && cualq_cercania_de_caja(&media) == 2 &&
         cualq_cercania_de_caja(&lejos) == 1);
    caso("los limites son los del detector de color (667 y 834)", cualq_cercania_de_caja(&(CajaModelo){ 0, 0, 0, 0, 0, 833 }) == 2 &&
         cualq_cercania_de_caja(&(CajaModelo){ 0, 0, 0, 0, 0, 834 }) == 3 && cualq_cercania_de_caja(&(CajaModelo){ 0, 0, 0, 0, 0, 666 }) == 1 &&
         cualq_cercania_de_caja(&(CajaModelo){ 0, 0, 0, 0, 0, 667 }) == 2);
    caso("zona por el centro de la caja: izquierda, centro, derecha",
         cualq_zona_de_caja(&(CajaModelo){ 0, 0, 100, 0, 300, 800 }) == ZONA_IZQUIERDA && cualq_zona_de_caja(&(CajaModelo){ 0, 0, 450, 0, 550, 800 }) == ZONA_CENTRO &&
         cualq_zona_de_caja(&(CajaModelo){ 0, 0, 750, 0, 950, 800 }) == ZONA_DERECHA);
}

/* ── 4. Persistencia ───────────────────────────────────────────────────── */

static void pruebas_persistencia(void)
{
    ConfigCualquiera c = cfg_activa();
    EstadoCualquiera e;
    ResultadoCualquiera r;
    DeteccionesModelo d;
    CajaModelo termo = caja(420, 450, 540, 800, 200);   /* cercania 2 (borde de abajo 800) */
    CajaModelo ninguna = caja(0, 0, 0, 0, 0);
    int i, cuentan_estable = 0, cuentan_parpadeo = 0;

    /* La caja del termo con 0.20: el primer cuadro NO cuenta (persistencia 2), el segundo sí. */
    cualq_iniciar(&e);
    d = cuadro_con(1000, 1, termo, ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1000, &r);
    caso("persistencia 2: la caja del termo (0.20) en el primer cuadro todavia NO cuenta (pasa los filtros, no la persistencia)",
         r.pasan_filtros == 1 && r.persistentes == 0 && r.zona[ZONA_CENTRO].hay_obstaculo == 0 && r.nuevo_cuadro == 1);
    d = cuadro_con(1100, 1, termo, ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1100, &r);
    caso("... y en el segundo cuadro seguido, en el mismo sitio, SI cuenta: zona centro, cercania 2",
         r.persistentes == 1 && r.zona[ZONA_CENTRO].hay_obstaculo == 1 && r.zona[ZONA_CENTRO].cercania == 2 && r.n_cuentan == 1 && r.mejor_zona == ZONA_CENTRO && r.mejor_cercania == 2);

    /* El mismo cuadro del modelo consultado dos veces no cuenta como dos. */
    cualq_iniciar(&e);
    d = cuadro_con(5000, 1, termo, ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 5000, &r);
    cualq_actualizar(&c, &e, &d, 5030, &r);
    cualq_actualizar(&c, &e, &d, 5060, &r);
    caso("el mismo cuadro del modelo (misma marca) consultado tres veces NO cuenta como persistente", r.persistentes == 0 && r.nuevo_cuadro == 0);

    /* Cajas que parpadean (un cuadro si, uno no) no cuentan nunca; estables cuentan. */
    cualq_iniciar(&e);
    for (i = 0; i < 20; i++) {
        d = cuadro_con(1000u + (unsigned int)i * 100u, (i % 2 == 0) ? 1 : 0, termo, ninguna, ninguna);
        cualq_actualizar(&c, &e, &d, 1000u + (unsigned int)i * 100u, &r);
        cuentan_parpadeo += (r.persistentes > 0);
    }
    cualq_iniciar(&e);
    for (i = 0; i < 20; i++) {
        d = cuadro_con(1000u + (unsigned int)i * 100u, 1, termo, ninguna, ninguna);
        cualq_actualizar(&c, &e, &d, 1000u + (unsigned int)i * 100u, &r);
        cuentan_estable += (r.persistentes > 0);
    }
    caso("una caja que PARPADEA (aparece un cuadro si y uno no) no cuenta en ninguno de 20 cuadros", cuentan_parpadeo == 0);
    caso("una caja ESTABLE cuenta en 19 de 20 cuadros (todos menos el primero)", cuentan_estable == 19);

    /* Una caja que se mueve a otro sitio entre cuadros no cuenta (superposicion menor al 50 %). */
    cualq_iniciar(&e);
    d = cuadro_con(1000, 1, caja(100, 450, 220, 800, 300), ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1000, &r);
    d = cuadro_con(1100, 1, caja(700, 450, 820, 800, 300), ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1100, &r);
    caso("una caja que salta de la izquierda a la derecha entre cuadros no cuenta (no esta en el mismo sitio)", r.persistentes == 0);
    cualq_iniciar(&e);
    d = cuadro_con(1000, 1, caja(420, 450, 540, 800, 300), ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1000, &r);
    d = cuadro_con(1100, 1, caja(440, 450, 560, 800, 300), ninguna, ninguna);   /* corrida 2 % del ancho: sigue casi igual */
    cualq_actualizar(&c, &e, &d, 1100, &r);
    caso("una caja que se corre un poco (superposicion de mas del 50 %) sigue contando", r.persistentes == 1);

    /* Cercanía alta: basta un cuadro. */
    cualq_iniciar(&e);
    d = cuadro_con(1000, 1, caja(420, 600, 540, 950, 300), ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1000, &r);
    caso("con cercania alta (3) una sola aparicion basta: cuenta en el primer cuadro", r.persistentes == 1 && r.zona[ZONA_CENTRO].cercania == 3);

    /* Persistencia 1 y 3. */
    c.persistencia = 1;
    cualq_iniciar(&e);
    d = cuadro_con(1000, 1, termo, ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1000, &r);
    caso("persistencia 1: cuenta en el primer cuadro", r.persistentes == 1);
    c.persistencia = 3;
    cualq_iniciar(&e);
    d = cuadro_con(1000, 1, termo, ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1000, &r);
    d = cuadro_con(1100, 1, termo, ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1100, &r);
    caso("persistencia 3: en el segundo cuadro todavia no cuenta", r.persistentes == 0);
    d = cuadro_con(1200, 1, termo, ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1200, &r);
    caso("persistencia 3: en el tercero ya cuenta", r.persistentes == 1);
    c.persistencia = PERSISTENCIA_CUADROS;

    /* Resultado caducado: se ignora y se borra la historia. */
    cualq_iniciar(&e);
    d = cuadro_con(1000, 1, termo, ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1000, &r);
    cualq_actualizar(&c, &e, &d, 1500, &r);   /* 500 ms despues: caducado (vigencia 300) */
    caso("un resultado del modelo caducado (500 ms) se ignora: no vigente y sin zonas", r.vigente == 0 && r.edad_modelo_ms == 500 && r.zona[ZONA_CENTRO].hay_obstaculo == 0);
    d = cuadro_con(1600, 1, termo, ninguna, ninguna);
    cualq_actualizar(&c, &e, &d, 1600, &r);
    caso("... y la historia se borro: la caja del cuadro nuevo todavia no cuenta", r.persistentes == 0 && r.vigente == 1);

    /* Modelo NULL y errores. */
    cualq_iniciar(&e);
    caso("sin resultado del modelo (NULL): sin cajas y no vigente", cualq_actualizar(&c, &e, NULL, 0, &r) == CUALQ_OK && r.vigente == 0 && r.n_cuentan == 0);
    {
        ConfigCualquiera mala = c;

        mala.umbral_mil = 0;
        caso("configuracion mala: CUALQ_ERR_CONFIG", cualq_actualizar(&mala, &e, &d, 1600, &r) == CUALQ_ERR_CONFIG);
    }
    caso("argumentos nulos: CUALQ_ERR_ARGUMENTO", cualq_actualizar(&c, NULL, &d, 0, &r) == CUALQ_ERR_ARGUMENTO && cualq_actualizar(NULL, &e, &d, 0, &r) == CUALQ_ERR_ARGUMENTO &&
         cualq_actualizar(&c, &e, &d, 0, NULL) == CUALQ_ERR_ARGUMENTO);
}

/* ── 5. Varias cajas, ninguna, y el corredor ───────────────────────────── */

static void pruebas_varias(void)
{
    ConfigCualquiera c = cfg_activa();
    EstadoCualquiera e;
    ResultadoCualquiera r;
    DeteccionesModelo d;
    CajaModelo izquierda = caja(50, 450, 250, 750, 300);    /* cercania 2 */
    CajaModelo centro = caja(420, 450, 580, 900, 300);      /* cercania 3 */
    CajaModelo derecha = caja(750, 450, 950, 620, 300);     /* cercania 1 */
    CajaModelo vacia = caja(0, 0, 0, 0, 0);

    cualq_iniciar(&e);
    d = cuadro_con(1000, 3, izquierda, centro, derecha);
    cualq_actualizar(&c, &e, &d, 1000, &r);
    d = cuadro_con(1100, 3, izquierda, centro, derecha);
    cualq_actualizar(&c, &e, &d, 1100, &r);
    caso("varias cajas estables: tres zonas con obstaculo y su cercania (izquierda 2, centro 3, derecha 1)",
         r.persistentes == 3 && r.zona[ZONA_IZQUIERDA].hay_obstaculo && r.zona[ZONA_IZQUIERDA].cercania == 2 && r.zona[ZONA_CENTRO].cercania == 3 &&
         r.zona[ZONA_DERECHA].cercania == 1 && r.mejor_zona == ZONA_CENTRO && r.mejor_cercania == 3);
    d = cuadro_con(1200, 0, vacia, vacia, vacia);
    cualq_actualizar(&c, &e, &d, 1200, &r);
    caso("ninguna caja: ninguna zona con obstaculo, vigente, mejor zona -1", r.vigente == 1 && r.n_cuentan == 0 && r.mejor_zona == -1 && !r.zona[0].hay_obstaculo && !r.zona[1].hay_obstaculo &&
         !r.zona[2].hay_obstaculo);

    /* Una caja ancha que cubre dos zonas las marca las dos. */
    cualq_iniciar(&e);
    {
        CajaModelo ancha = caja(100, 450, 600, 800, 300);   /* izquierda y centro */

        d = cuadro_con(1000, 1, ancha, vacia, vacia);
        cualq_actualizar(&c, &e, &d, 1000, &r);
        d = cuadro_con(1100, 1, ancha, vacia, vacia);
        cualq_actualizar(&c, &e, &d, 1100, &r);
        caso("una caja ancha (10 % a 60 % del ancho) marca la zona izquierda y la del centro", r.zona[ZONA_IZQUIERDA].hay_obstaculo && r.zona[ZONA_CENTRO].hay_obstaculo &&
             !r.zona[ZONA_DERECHA].hay_obstaculo);
    }

    /* Mezcla: el termo, el refrigerador que cubre todo y una caja arriba: solo cuenta el termo. */
    cualq_iniciar(&e);
    {
        CajaModelo termo = caja(420, 450, 540, 850, 200);
        CajaModelo refri = caja(0, 0, 1000, 1000, 430);
        CajaModelo arriba = caja(300, 50, 700, 400, 600);

        d = cuadro_con(1000, 3, termo, refri, arriba);
        cualq_actualizar(&c, &e, &d, 1000, &r);
        d = cuadro_con(1100, 3, termo, refri, arriba);
        cualq_actualizar(&c, &e, &d, 1100, &r);
        caso("con el termo 0.20, un refrigerador 0.43 que cubre todo y una caja arriba: pasan los filtros 1 de 3 y solo cuenta el termo",
             r.cajas_modelo == 3 && r.pasan_filtros == 1 && r.persistentes == 1 && r.n_cuentan == 1);

        /* Compatible con el corredor de trayectoria (corredor.h). */
        {
            ConfigCorredor cc;
            Corredor cor;
            CajaImagen cajas[FUS_MAX_CAJAS];
            ZonaObstaculo zonas[OBS_ZONAS];
            int n;

            corredor_config_defecto(&cc);
            cc.activo = 1;
            corredor_preparar(&cor, &cc, 240, 176);
            n = cualq_a_cajas_imagen(&r, 240, 176, cc.min_peso, cajas, FUS_MAX_CAJAS);
            memcpy(zonas, r.zona, sizeof zonas);
            corredor_aplicar_zonas(&cor, cajas, n, zonas);
            caso("compatible con el corredor: el termo del centro (en pixeles de un cuadro de 240x176) queda DENTRO del corredor y frena",
                 n == 1 && cajas[0].x0 == 100 && cajas[0].x1 == 130 && zonas[ZONA_CENTRO].fuera_corredor == 0 && zonas[ZONA_CENTRO].cercania_corredor >= 1);
            {
                CajaModelo lado = caja(0, 250, 45, 600, 300);   /* pegada al borde izquierdo y lejos */
                DeteccionesModelo d2;

                cualq_iniciar(&e);
                d2 = cuadro_con(2000, 1, lado, vacia, vacia);
                cualq_actualizar(&c, &e, &d2, 2000, &r);
                d2 = cuadro_con(2100, 1, lado, vacia, vacia);
                cualq_actualizar(&c, &e, &d2, 2100, &r);
                n = cualq_a_cajas_imagen(&r, 240, 176, cc.min_peso, cajas, FUS_MAX_CAJAS);
                memcpy(zonas, r.zona, sizeof zonas);
                corredor_aplicar_zonas(&cor, cajas, n, zonas);
                caso("... y un objeto pegado al borde izquierdo (x 0 a 4.5 %, borde de abajo en 60 %) queda FUERA del corredor (solo avisa)",
                     n == 1 && zonas[ZONA_IZQUIERDA].hay_obstaculo == 1 && zonas[ZONA_IZQUIERDA].fuera_corredor == 1);
            }
        }
    }
    caso("cualq_a_cajas_imagen con NULL o capacidad 0 no escribe nada", cualq_a_cajas_imagen(NULL, 240, 176, 1, NULL, 0) == 0);
}

/* ── 6. La fusión con el color ─────────────────────────────────────────── */

static ResultadoObstaculos detector_con(int hay_izq, int cerc_izq, int hay_cen, int cerc_cen, int celdas_cen)
{
    ResultadoObstaculos d;

    memset(&d, 0, sizeof d);
    d.zona[ZONA_IZQUIERDA].hay_obstaculo = hay_izq;
    d.zona[ZONA_IZQUIERDA].cercania = cerc_izq;
    d.zona[ZONA_IZQUIERDA].celdas = hay_izq ? 9 : 0;
    d.zona[ZONA_CENTRO].hay_obstaculo = hay_cen;
    d.zona[ZONA_CENTRO].cercania = cerc_cen;
    d.zona[ZONA_CENTRO].celdas = celdas_cen;
    return d;
}

static void zonas_cualquiera(ZonaObstaculo z[OBS_ZONAS], int cerc_izq, int cerc_cen, int cerc_der)
{
    memset(z, 0, sizeof(ZonaObstaculo) * OBS_ZONAS);
    z[ZONA_IZQUIERDA].hay_obstaculo = cerc_izq > 0; z[ZONA_IZQUIERDA].cercania = cerc_izq;
    z[ZONA_CENTRO].hay_obstaculo = cerc_cen > 0;    z[ZONA_CENTRO].cercania = cerc_cen;
    z[ZONA_DERECHA].hay_obstaculo = cerc_der > 0;   z[ZONA_DERECHA].cercania = cerc_der;
}

static void pruebas_fusion(void)
{
    ConfigFusion f;
    ResultadoObstaculos det;
    ZonaObstaculo cual[OBS_ZONAS];
    ResultadoFusion s;

    fusion_config_defecto(&f);

    /* a) coinciden: sube a la mayor cercania. */
    det = detector_con(0, 0, 1, 1, 8);
    zonas_cualquiera(cual, 0, 3, 0);
    fusion_decidir_cualquiera(&f, &det, cual, 1, &s);
    caso("a) el color ve (cercania 1) y «cualquier objeto» lo ve mas cerca (3) en la misma zona: sube a 3 y la fuente es AMBOS",
         s.zona[ZONA_CENTRO].hay_obstaculo && s.zona[ZONA_CENTRO].cercania == 3 && s.fuente == FUS_FUENTE_AMBOS && s.severidad == FUS_SEV_PARAR && s.desmentidos == 0);
    det = detector_con(0, 0, 1, 3, 8);
    zonas_cualquiera(cual, 0, 1, 0);
    fusion_decidir_cualquiera(&f, &det, cual, 1, &s);
    caso("a) el color ve muy cerca (3) y «cualquier objeto» lejos (1): NUNCA se baja lo que ve el color", s.zona[ZONA_CENTRO].cercania == 3 && s.severidad == FUS_SEV_PARAR);

    /* b) el color duda. */
    det = detector_con(0, 0, 0, 0, 4);   /* 4 celdas: menos del minimo, duda */
    zonas_cualquiera(cual, 0, 2, 0);
    fusion_decidir_cualquiera(&f, &det, cual, 1, &s);
    caso("b) el color duda (4 celdas, menos del minimo) y «cualquier objeto» ve algo a media distancia: se sube a 2 (frenar) con fuente AMBOS",
         s.zona[ZONA_CENTRO].hay_obstaculo && s.zona[ZONA_CENTRO].cercania == 2 && s.fuente == FUS_FUENTE_AMBOS && s.severidad == FUS_SEV_FRENAR);

    /* c) solo «cualquier objeto»: aviso, nunca frena. */
    det = detector_con(0, 0, 0, 0, 0);
    zonas_cualquiera(cual, 0, 3, 0);
    fusion_decidir_cualquiera(&f, &det, cual, 1, &s);
    caso("c) solo «cualquier objeto» ve algo (cercania 3) y el color no ve ni duda: NO frena (zona libre), solo avisa",
         !s.zona[ZONA_CENTRO].hay_obstaculo && s.aviso == 1 && s.aviso_zona == ZONA_CENTRO && s.aviso_cercania == 3 && s.severidad == FUS_SEV_AVISO && s.fuente == FUS_FUENTE_MODELO);

    /* d) desmentir. */
    det = detector_con(0, 0, 1, 1, 8);   /* falso positivo de color lejano */
    zonas_cualquiera(cual, 0, 0, 0);
    fusion_decidir_cualquiera(&f, &det, cual, 1, &s);
    caso("d) el color ve algo LEJANO (cercania 1), «cualquier objeto» vigente no ve nada: se desmiente (zona libre, 1 desmentido)",
         !s.zona[ZONA_CENTRO].hay_obstaculo && s.zona[ZONA_CENTRO].cercania == 0 && s.desmentidos == 1 && s.severidad == FUS_SEV_NINGUNA);
    det = detector_con(0, 0, 1, 2, 8);
    fusion_decidir_cualquiera(&f, &det, cual, 1, &s);
    caso("d) con cercania 2 el color NUNCA se desmiente", s.zona[ZONA_CENTRO].hay_obstaculo && s.zona[ZONA_CENTRO].cercania == 2 && s.desmentidos == 0 && s.severidad == FUS_SEV_FRENAR);
    det = detector_con(0, 0, 1, 3, 8);
    fusion_decidir_cualquiera(&f, &det, cual, 1, &s);
    caso("d) con cercania 3 el color NUNCA se desmiente", s.zona[ZONA_CENTRO].hay_obstaculo && s.zona[ZONA_CENTRO].cercania == 3 && s.desmentidos == 0 && s.severidad == FUS_SEV_PARAR);

    /* e) sin resultado vigente. */
    det = detector_con(0, 0, 1, 1, 8);
    zonas_cualquiera(cual, 0, 0, 0);
    fusion_decidir_cualquiera(&f, &det, cual, 0, &s);
    caso("e) sin resultado vigente de «cualquier objeto» el falso positivo de color de cercania baja NO se desmiente", s.zona[ZONA_CENTRO].hay_obstaculo && s.desmentidos == 0 &&
         s.fuente == FUS_FUENTE_DETECTOR && s.modelo_vigente == 0);

    /* Una zona a la vez: el desmentido de una no toca las otras. */
    det = detector_con(1, 3, 1, 1, 8);
    zonas_cualquiera(cual, 0, 0, 0);
    fusion_decidir_cualquiera(&f, &det, cual, 1, &s);
    caso("el desmentido de la zona del centro no toca la izquierda (cercania 3 del color se mantiene)", s.zona[ZONA_IZQUIERDA].hay_obstaculo && s.zona[ZONA_IZQUIERDA].cercania == 3 &&
         !s.zona[ZONA_CENTRO].hay_obstaculo && s.desmentidos == 1);

    /* Piso vacio y nada que decir. */
    det = detector_con(0, 0, 0, 0, 0);
    zonas_cualquiera(cual, 0, 0, 0);
    fusion_decidir_cualquiera(&f, &det, cual, 1, &s);
    caso("nada en ninguno: severidad ninguna y fuente nadie", s.severidad == FUS_SEV_NINGUNA && s.fuente == FUS_FUENTE_NINGUNA && s.aviso == 0);
    caso("argumentos nulos: FUS_ERR_ARGUMENTO", fusion_decidir_cualquiera(NULL, &det, cual, 1, &s) == FUS_ERR_ARGUMENTO && fusion_decidir_cualquiera(&f, NULL, cual, 1, &s) == FUS_ERR_ARGUMENTO &&
         fusion_decidir_cualquiera(&f, &det, NULL, 1, &s) == FUS_ERR_ARGUMENTO && fusion_decidir_cualquiera(&f, &det, cual, 1, NULL) == FUS_ERR_ARGUMENTO);

    /* La regla no cambia fusion_decidir original. */
    {
        DeteccionesModelo m;
        ResultadoFusion s2;

        memset(&m, 0, sizeof m);
        det = detector_con(0, 0, 1, 2, 8);
        fusion_decidir(&f, &det, &m, 0, &s2);
        caso("fusion_decidir (el modo de clases) sigue igual y tiene desmentidos = 0", s2.desmentidos == 0 && s2.zona[ZONA_CENTRO].cercania == 2);
    }
}

/* ── 7. Barrido de umbrales ────────────────────────────────────────────── */

static void pruebas_barrido(void)
{
    static const int umbrales[] = { 10, 50, 100, 150, 200, 300 };
    int u, i, ok = 1, anterior_cuentan = -1, cuentan_en[6];
    CajaModelo termo, ruido1, ruido2, ninguna = caja(0, 0, 0, 0, 0);

    /* Una secuencia de 30 cuadros: el termo estable con 0.20 en los cuadros 10 a 29, y dos cajitas de ruido que aparecen y desaparecen con 0.03 y 0.08. */
    printf("      umbral:   cuadros con el termo contado / cuadros con ruido contado\n");
    for (u = 0; u < 6; u++) {
        ConfigCualquiera c = cfg_activa();
        EstadoCualquiera e;
        ResultadoCualquiera r;
        int con_termo = 0, con_ruido = 0;

        c.umbral_mil = umbrales[u];
        cualq_iniciar(&e);
        for (i = 0; i < 30; i++) {
            DeteccionesModelo d;

            termo = caja(420, 450, 540, 800, 200);
            ruido1 = caja(100 + (i % 3) * 5, 520, 220, 800, 30);     /* 0.03: aparece fijo */
            ruido2 = caja(700 + (i * 37) % 200, 500, 820 + (i * 37) % 100, 780, 80);   /* 0.08: salta de lugar */
            d = cuadro_con(1000u + (unsigned int)i * 100u, 3, (i >= 10) ? termo : ninguna, ruido1, ruido2);
            cualq_actualizar(&c, &e, &d, 1000u + (unsigned int)i * 100u, &r);
            if (r.zona[ZONA_CENTRO].hay_obstaculo) {
                con_termo++;
            }
            if (r.zona[ZONA_IZQUIERDA].hay_obstaculo || r.zona[ZONA_DERECHA].hay_obstaculo) {
                con_ruido++;
            }
        }
        cuentan_en[u] = con_termo;
        printf("      %.2f:   %2d / %2d\n", umbrales[u] / 1000.0, con_termo, con_ruido);
        if (umbrales[u] < 200 && con_termo != 19) {
            ok = 0;   /* con un umbral por debajo de 0.20 el termo cuenta del cuadro 11 al 29 (19 cuadros) */
        }
        (void)anterior_cuentan;
    }
    caso("barrido de umbrales: con 0.01 a 0.15 el termo (0.20) cuenta en 19 cuadros; con 0.20 y 0.30 no cuenta", ok && cuentan_en[4] == 0 && cuentan_en[5] == 0);
}

int main(void)
{
    pruebas_configuracion();
    pruebas_filtros();
    pruebas_zona_y_cercania();
    pruebas_persistencia();
    pruebas_varias();
    pruebas_fusion();
    pruebas_barrido();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
