/*
 * cualquiera.c — Modo «cualquier objeto» del modelo de objetos (ver cualquiera.h).
 *
 * GUÍA DE LECTURA
 *  1. cualq_motivo_descarte(): los filtros de una caja (confianza, tamaño, posición). Es lo que se prueba con cada caja suelta.
 *  2. Zona y cercanía de una caja: por el tercio del ancho que cubre y por la posición vertical de su borde de abajo.
 *  3. cualq_actualizar(): el corazón. Por cada cuadro NUEVO del modelo: filtra las cajas, las mete a la historia, y deja pasar solo las que aparecen en el mismo
 *     sitio (superposición mayor al 50 %) en los últimos PERSISTENCIA cuadros seguidos (o que están muy cerca: cercanía 3). La historia guarda el cuadro actual en la
 *     posición 0 y los anteriores detrás: así consultar dos veces el mismo cuadro del modelo no cuenta como persistencia.
 *  Solo enteros (long donde hace falta), sin malloc ni recursión.
 */

#include <stddef.h>
#include "cualquiera.h"

/* ── Configuración ─────────────────────────────────────────────────────── */

void cualq_config_defecto(ConfigCualquiera *config)
{
    if (config == NULL) {
        return;
    }
    config->activo = 0;   /* APAGADO por defecto */
    config->umbral_mil = UMBRAL_CUALQUIERA_MIL;
    config->area_max_mil = CUALQ_AREA_MAX_MIL;
    config->borde_inferior_min_mil = CUALQ_BORDE_INFERIOR_MIN_MIL;
    config->area_min_mil = CUALQ_AREA_MIN_MIL;
    config->ancho_min_mil = CUALQ_ANCHO_MIN_MIL;
    config->persistencia = PERSISTENCIA_CUADROS;
    config->solape_mil = CUALQ_SOLAPE_MIL;
    config->vigencia_ms = 300;
}

int cualq_config_es_valida(const ConfigCualquiera *config)
{
    return config != NULL && (config->activo == 0 || config->activo == 1) &&
           config->umbral_mil >= UMBRAL_CUALQUIERA_MIN_MIL && config->umbral_mil <= UMBRAL_CUALQUIERA_MAX_MIL &&
           config->area_max_mil >= 1 && config->area_max_mil <= 1000 &&
           config->borde_inferior_min_mil >= 0 && config->borde_inferior_min_mil <= 1000 &&
           config->area_min_mil >= 0 && config->area_min_mil <= 1000 &&
           config->ancho_min_mil >= 0 && config->ancho_min_mil <= 1000 &&
           config->persistencia >= 1 && config->persistencia <= CUALQ_MAX_PERSISTENCIA &&
           config->solape_mil >= 1 && config->solape_mil <= 1000 &&
           config->vigencia_ms >= 0 && config->vigencia_ms <= 10000;
}

void cualq_iniciar(EstadoCualquiera *estado)
{
    int k;

    if (estado == NULL) {
        return;
    }
    estado->cuadros = 0;
    estado->ultima_marca = 0;
    estado->hay_marca = 0;
    for (k = 0; k < CUALQ_MAX_PERSISTENCIA; k++) {
        estado->n_cajas[k] = 0;
    }
}

/* ── Filtros de una caja ───────────────────────────────────────────────── */

static int limitar_mil(int v)
{
    return (v < 0) ? 0 : ((v > 1000) ? 1000 : v);
}

int cualq_motivo_descarte(const ConfigCualquiera *config, const CajaModelo *caja)
{
    int xmin, xmax, ymin, ymax, ancho, alto, area_mil;

    if (config == NULL || caja == NULL) {
        return CUALQ_CAJA_INVALIDA;
    }
    xmin = limitar_mil(caja->xmin_mil);
    xmax = limitar_mil(caja->xmax_mil);
    ymin = limitar_mil(caja->ymin_mil);
    ymax = limitar_mil(caja->ymax_mil);
    ancho = xmax - xmin;
    alto = ymax - ymin;
    if (ancho <= 0 || alto <= 0) {
        return CUALQ_CAJA_INVALIDA;
    }
    if (caja->confianza_mil <= config->umbral_mil) {
        return CUALQ_POCA_CONFIANZA;   /* la confianza debe SUPERAR el umbral */
    }
    area_mil = ancho * alto / 1000;    /* milésimas del área del cuadro */
    if (area_mil > config->area_max_mil) {
        return CUALQ_MUY_GRANDE;       /* cubre casi todo el cuadro: una pared, no un objeto sobre el piso */
    }
    if (ymax < config->borde_inferior_min_mil) {
        return CUALQ_BORDE_ALTO;       /* no toca la zona inferior: está en el fondo o flota */
    }
    if (area_mil < config->area_min_mil) {
        return CUALQ_MUY_CHICA;
    }
    if (ancho < config->ancho_min_mil) {
        return CUALQ_MUY_ANGOSTA;
    }
    return CUALQ_PASA;
}

const char *cualq_texto_motivo(int motivo)
{
    switch (motivo) {
    case CUALQ_PASA:           return "pasa";
    case CUALQ_POCA_CONFIANZA: return "poca confianza";
    case CUALQ_MUY_GRANDE:     return "cubre demasiado del cuadro";
    case CUALQ_BORDE_ALTO:     return "el borde de abajo esta arriba de la mitad";
    case CUALQ_MUY_CHICA:      return "muy chica";
    case CUALQ_MUY_ANGOSTA:    return "muy angosta";
    default:                   return "caja no valida";
    }
}

/* ── Zona y cercanía ───────────────────────────────────────────────────── */

int cualq_cercania_de_caja(const CajaModelo *caja)
{
    int ymax = (caja == NULL) ? 0 : limitar_mil(caja->ymax_mil);

    if (ymax >= OBS_CERCA_MIL) {
        return 3;
    }
    return (ymax >= OBS_MEDIA_MIL) ? 2 : 1;
}

int cualq_zona_de_caja(const CajaModelo *caja)
{
    int centro = (caja == NULL) ? 500 : (limitar_mil(caja->xmin_mil) + limitar_mil(caja->xmax_mil)) / 2;

    if (centro < 333) {
        return ZONA_IZQUIERDA;
    }
    return (centro < 667) ? ZONA_CENTRO : ZONA_DERECHA;
}

/* Cuánto de la caja (en milésimas del ancho del cuadro) cae en la zona z. */
static int solape_con_zona(const CajaMil *caja, int z)
{
    int ini = z * 1000 / OBS_ZONAS, fin = (z + 1) * 1000 / OBS_ZONAS;
    int desde = (caja->xmin_mil > ini) ? caja->xmin_mil : ini;
    int hasta = (caja->xmax_mil < fin) ? caja->xmax_mil : fin;

    return (hasta > desde) ? hasta - desde : 0;
}

/* Superposición (intersección / unión) de dos cajas, en milésimas. */
static int superposicion_mil(const CajaMil *a, const CajaMil *b)
{
    long x0 = (a->xmin_mil > b->xmin_mil) ? a->xmin_mil : b->xmin_mil;
    long y0 = (a->ymin_mil > b->ymin_mil) ? a->ymin_mil : b->ymin_mil;
    long x1 = (a->xmax_mil < b->xmax_mil) ? a->xmax_mil : b->xmax_mil;
    long y1 = (a->ymax_mil < b->ymax_mil) ? a->ymax_mil : b->ymax_mil;
    long inter, area_a, area_b, union_;

    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }
    inter = (x1 - x0) * (y1 - y0);
    area_a = (long)(a->xmax_mil - a->xmin_mil) * (a->ymax_mil - a->ymin_mil);
    area_b = (long)(b->xmax_mil - b->xmin_mil) * (b->ymax_mil - b->ymin_mil);
    union_ = area_a + area_b - inter;
    return (union_ > 0) ? (int)(inter * 1000L / union_) : 0;
}

/* ── Actualizar con un resultado del modelo ────────────────────────────── */

static void limpiar_resultado(ResultadoCualquiera *r)
{
    int z, i;

    for (z = 0; z < OBS_ZONAS; z++) {
        r->zona[z].hay_obstaculo = 0;
        r->zona[z].cercania = 0;
        r->zona[z].celdas = 0;
        r->zona[z].celdas_corredor = 0;
        r->zona[z].fuera_corredor = 0;
        r->zona[z].cercania_corredor = 0;
    }
    r->vigente = 0;
    r->edad_modelo_ms = 0;
    r->cajas_modelo = 0;
    r->pasan_filtros = 0;
    r->persistentes = 0;
    r->n_cuentan = 0;
    for (i = 0; i < FUS_MAX_CAJAS; i++) {
        r->cuentan[i].xmin_mil = r->cuentan[i].ymin_mil = r->cuentan[i].xmax_mil = r->cuentan[i].ymax_mil = 0;
        r->zona_cuentan[i] = 0;
        r->cercania_cuentan[i] = 0;
    }
    r->mejor_zona = -1;
    r->mejor_cercania = 0;
    r->nuevo_cuadro = 0;
}

int cualq_actualizar(const ConfigCualquiera *config, EstadoCualquiera *estado, const DeteccionesModelo *modelo, unsigned int ahora_ms, ResultadoCualquiera *resultado)
{
    CajaMil aceptadas[FUS_MAX_CAJAS];
    int n_aceptadas = 0, n, i, k, z, edad, nuevo;

    if (resultado == NULL) {
        return CUALQ_ERR_ARGUMENTO;
    }
    limpiar_resultado(resultado);
    if (config == NULL || estado == NULL) {
        return CUALQ_ERR_ARGUMENTO;
    }
    if (!cualq_config_es_valida(config)) {
        return CUALQ_ERR_CONFIG;
    }
    if (modelo == NULL) {
        return CUALQ_OK;   /* el modelo no ha dicho nada: sin cajas */
    }
    edad = (int)(ahora_ms - modelo->marca_ms);
    if (edad < 0) {
        edad = 0;
    }
    resultado->edad_modelo_ms = edad;
    if (edad > config->vigencia_ms) {
        cualq_iniciar(estado);   /* resultado caducado: se ignora y la historia empieza de cero (cajas viejas no cuentan después) */
        return CUALQ_OK;
    }
    resultado->vigente = 1;
    n = (modelo->n > FUS_MAX_CAJAS) ? FUS_MAX_CAJAS : ((modelo->n < 0) ? 0 : modelo->n);
    resultado->cajas_modelo = n;

    /* 1. Filtros. */
    for (i = 0; i < n; i++) {
        const CajaModelo *c = &modelo->cajas[i];

        if (cualq_motivo_descarte(config, c) == CUALQ_PASA) {
            aceptadas[n_aceptadas].xmin_mil = limitar_mil(c->xmin_mil);
            aceptadas[n_aceptadas].ymin_mil = limitar_mil(c->ymin_mil);
            aceptadas[n_aceptadas].xmax_mil = limitar_mil(c->xmax_mil);
            aceptadas[n_aceptadas].ymax_mil = limitar_mil(c->ymax_mil);
            n_aceptadas++;
        }
    }
    resultado->pasan_filtros = n_aceptadas;

    /* 2. Historia: un cuadro NUEVO del modelo entra en la posición 0 y empuja a los anteriores. */
    nuevo = (!estado->hay_marca || modelo->marca_ms != estado->ultima_marca);
    resultado->nuevo_cuadro = nuevo;
    if (nuevo) {
        for (k = CUALQ_MAX_PERSISTENCIA - 1; k > 0; k--) {
            estado->n_cajas[k] = estado->n_cajas[k - 1];
            for (i = 0; i < estado->n_cajas[k]; i++) {
                estado->cajas[k][i] = estado->cajas[k - 1][i];
            }
        }
        estado->n_cajas[0] = n_aceptadas;
        for (i = 0; i < n_aceptadas; i++) {
            estado->cajas[0][i] = aceptadas[i];
        }
        if (estado->cuadros < CUALQ_MAX_PERSISTENCIA) {
            estado->cuadros++;
        }
        estado->ultima_marca = modelo->marca_ms;
        estado->hay_marca = 1;
    }

    /* 3. Persistencia: la caja del cuadro actual cuenta si en cada uno de los cuadros anteriores hasta cubrir PERSISTENCIA hay otra que casi coincide (o si
     *    está muy cerca: con cercanía alta basta un cuadro). */
    for (i = 0; i < n_aceptadas; i++) {
        CajaModelo caja_modelo;
        int cercania, persiste = 1;

        caja_modelo.xmin_mil = aceptadas[i].xmin_mil;
        caja_modelo.xmax_mil = aceptadas[i].xmax_mil;
        caja_modelo.ymin_mil = aceptadas[i].ymin_mil;
        caja_modelo.ymax_mil = aceptadas[i].ymax_mil;
        caja_modelo.clase = 0;
        caja_modelo.confianza_mil = 1000;
        cercania = cualq_cercania_de_caja(&caja_modelo);
        if (config->persistencia > 1 && cercania < CUALQ_CERCANIA_ALTA) {
            if (estado->cuadros < config->persistencia) {
                persiste = 0;   // todavía no hay tantos cuadros en la historia
            }
            for (k = 1; persiste && k < config->persistencia; k++) {
                int hay = 0, j;

                for (j = 0; j < estado->n_cajas[k]; j++) {
                    if (superposicion_mil(&aceptadas[i], &estado->cajas[k][j]) > config->solape_mil) {
                        hay = 1;
                        break;
                    }
                }
                persiste = hay;
            }
        }
        if (!persiste) {
            continue;
        }
        resultado->persistentes++;
        /* Zona: cada tercio del ancho que cubre al menos CUALQ_SOLAPE_ZONA_MIL; si ninguno (caja muy angosta), la zona del centro de la caja. */
        {
            int marcada = 0, mejor_z = cualq_zona_de_caja(&caja_modelo), mejor_solape = 0;

            for (z = 0; z < OBS_ZONAS; z++) {
                int s = solape_con_zona(&aceptadas[i], z);

                if (s > mejor_solape) {
                    mejor_solape = s;
                    mejor_z = z;
                }
                if (s >= 60) {   /* como solape_camino_mil de la fusión: 6 % del ancho */
                    resultado->zona[z].hay_obstaculo = 1;
                    resultado->zona[z].celdas = 1;
                    if (cercania > resultado->zona[z].cercania) {
                        resultado->zona[z].cercania = cercania;
                    }
                    marcada = 1;
                }
            }
            if (!marcada) {
                resultado->zona[mejor_z].hay_obstaculo = 1;
                resultado->zona[mejor_z].celdas = 1;
                if (cercania > resultado->zona[mejor_z].cercania) {
                    resultado->zona[mejor_z].cercania = cercania;
                }
            }
            if (resultado->n_cuentan < FUS_MAX_CAJAS) {
                resultado->cuentan[resultado->n_cuentan] = aceptadas[i];
                resultado->zona_cuentan[resultado->n_cuentan] = mejor_z;
                resultado->cercania_cuentan[resultado->n_cuentan] = cercania;
                resultado->n_cuentan++;
            }
            if (cercania > resultado->mejor_cercania) {
                resultado->mejor_cercania = cercania;
                resultado->mejor_zona = mejor_z;
            }
        }
    }
    return CUALQ_OK;
}

int cualq_a_cajas_imagen(const ResultadoCualquiera *resultado, int ancho, int alto, int peso, CajaImagen *cajas, int capacidad)
{
    int i, n;

    if (resultado == NULL || cajas == NULL || capacidad < 0) {
        return 0;
    }
    n = (resultado->n_cuentan < capacidad) ? resultado->n_cuentan : capacidad;
    for (i = 0; i < n; i++) {
        corredor_caja_de_milesimas(resultado->cuentan[i].xmin_mil, resultado->cuentan[i].ymin_mil, resultado->cuentan[i].xmax_mil,
                                   resultado->cuentan[i].ymax_mil, ancho, alto, peso, COR_FUENTE_MODELO, &cajas[i]);
    }
    return n;
}
