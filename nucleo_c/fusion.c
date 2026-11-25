/*
 * fusion.c — Fusión del detector de piso por color con el modelo de objetos (ver fusion.h).
 *
 * GUÍA DE LECTURA
 *  1. Configuración: fusion_config_defecto, fusion_marcar_clase y fusion_clase_de_interes (las clases de
 *     interés son un conjunto de bits: una palabra de 32 bits cada 32 clases).
 *  2. Caja → zona y cercanía: fusion_caja_a_zona y fusion_cercania_de_caja (la parte "geométrica").
 *  3. fusion_decidir(): conserva la procedencia POR ZONA. Solo la coincidencia
 *     entre color y red vigente confirma por vision; las fuentes aisladas avisan.
 *  4. Al final calcula la severidad (de las zonas fusionadas) y quién decidió.
 *  Solo enteros, sin malloc, sin recursión.
 */

#include <stddef.h>
#include "fusion.h"

/* Límites de las tres zonas de ancho (tercios), en milésimas del ancho del cuadro. */
#define ZONA_CENTRO_INICIO_MIL  333
#define ZONA_CENTRO_FIN_MIL     667

/* ── Configuración ─────────────────────────────────────────────────────── */

int fusion_marcar_clase(ConfigFusion *config, int clase, int de_interes)
{
    if (config == NULL || clase < 0 || clase >= FUS_CLASES_MAX) {
        return FUS_ERR_ARGUMENTO;
    }
    if (de_interes) {
        config->clases_interes[clase / 32] |= (1u << (clase % 32));
    } else {
        config->clases_interes[clase / 32] &= ~(1u << (clase % 32));
    }
    return FUS_OK;
}

int fusion_clase_de_interes(const ConfigFusion *config, int clase)
{
    if (config == NULL || clase < 0 || clase >= FUS_CLASES_MAX) {
        return 0;
    }
    return (config->clases_interes[clase / 32] >> (clase % 32)) & 1u ? 1 : 0;
}

void fusion_config_defecto(ConfigFusion *config)
{
    int i;

    if (config == NULL) {
        return;
    }
    config->vigencia_ms = 300;
    config->confianza_min_mil = 600;   /* 60 %: un "refrigerator 43 %" sobre una pared lisa debe ignorarse */
    config->confianza_frenar_mil = 600;
    config->solape_camino_mil = 60;
    config->cerca_mil = OBS_CERCA_MIL;
    config->media_mil = OBS_MEDIA_MIL;
    config->area_grande_mil = 200;
    for (i = 0; i < FUS_PALABRAS_CLASES; i++) {
        config->clases_interes[i] = 0;
    }
    fusion_marcar_clase(config, 0, 1);    /* persona */
    fusion_marcar_clase(config, 16, 1);   /* gato */
    fusion_marcar_clase(config, 17, 1);   /* perro */
    fusion_marcar_clase(config, 43, 1);   /* botella */
    fusion_marcar_clase(config, 61, 1);   /* silla */
    fusion_marcar_clase(config, 66, 1);   /* mesa */
}

int fusion_config_es_valida(const ConfigFusion *config)
{
    return config != NULL && config->vigencia_ms >= 0 && config->vigencia_ms <= 10000 &&
           config->confianza_min_mil >= 0 && config->confianza_min_mil <= 1000 &&
           config->confianza_frenar_mil >= config->confianza_min_mil && config->confianza_frenar_mil <= 1000 &&
           config->solape_camino_mil >= 0 && config->solape_camino_mil <= 334 &&
           config->media_mil >= 0 && config->cerca_mil >= config->media_mil && config->cerca_mil <= 1000 &&
           config->area_grande_mil >= 0 && config->area_grande_mil <= 1000;
}

/* ── Caja → zona y cercanía ────────────────────────────────────────────── */

static int limitar_milesimas(int valor)
{
    if (valor < 0) {
        return 0;
    }
    return (valor > 1000) ? 1000 : valor;
}

int fusion_caja_a_zona(const ConfigFusion *config, const CajaModelo *caja, int *en_camino)
{
    int xmin = limitar_milesimas(caja->xmin_mil);
    int xmax = limitar_milesimas(caja->xmax_mil);
    int desde = (xmin > ZONA_CENTRO_INICIO_MIL) ? xmin : ZONA_CENTRO_INICIO_MIL;
    int hasta = (xmax < ZONA_CENTRO_FIN_MIL) ? xmax : ZONA_CENTRO_FIN_MIL;
    int solape = (hasta > desde) ? hasta - desde : 0;

    if (solape >= config->solape_camino_mil && solape > 0) {
        *en_camino = 1;
        return ZONA_CENTRO;
    }
    *en_camino = 0;
    return ((xmin + xmax) / 2 < 500) ? ZONA_IZQUIERDA : ZONA_DERECHA;
}

int fusion_cercania_de_caja(const ConfigFusion *config, const CajaModelo *caja)
{
    int ymax = limitar_milesimas(caja->ymax_mil);
    int ancho = limitar_milesimas(caja->xmax_mil) - limitar_milesimas(caja->xmin_mil);
    int alto = ymax - limitar_milesimas(caja->ymin_mil);
    int cercania, area_mil;

    if (ymax >= config->cerca_mil) {
        cercania = 3;
    } else if (ymax >= config->media_mil) {
        cercania = 2;
    } else {
        cercania = 1;
    }
    area_mil = (ancho > 0 && alto > 0) ? ancho * alto / 1000 : 0;   /* área en milésimas del cuadro */
    if (config->area_grande_mil > 0 && area_mil >= config->area_grande_mil && cercania < 3) {
        cercania++;   /* una caja muy grande está cerca aunque su borde de abajo no baje tanto */
    }
    return cercania;
}

/* ── Decidir ───────────────────────────────────────────────────────────── */

/* Severidad que corresponde a una cercanía de zona (0 = sin obstáculo). */
static int severidad_de_cercania(int hay, int cercania)
{
    if (!hay || cercania <= 0) {
        return FUS_SEV_NINGUNA;
    }
    if (cercania >= 3) {
        return FUS_SEV_PARAR;
    }
    return (cercania == 2) ? FUS_SEV_FRENAR : FUS_SEV_AVISO;
}

static int mayor(int a, int b)
{
    return (a > b) ? a : b;
}

/* Severidad máxima de las tres zonas. */
static int severidad_de_zonas(const ZonaObstaculo zonas[OBS_ZONAS])
{
    int z, sev = FUS_SEV_NINGUNA;

    for (z = 0; z < OBS_ZONAS; z++) {
        sev = mayor(sev, severidad_de_cercania(zonas[z].hay_obstaculo, zonas[z].cercania));
    }
    return sev;
}

static void limpiar_salida(ResultadoFusion *salida)
{
    int z;

    for (z = 0; z < OBS_ZONAS; z++) {
        salida->zona[z].hay_obstaculo = 0;
        salida->zona[z].cercania = 0;
        salida->zona[z].celdas = 0;
        salida->zona[z].celdas_corredor = salida->zona[z].fuera_corredor = salida->zona[z].cercania_corredor = 0;
        salida->fuentes_vision[z] = 0;
    }
    salida->severidad = FUS_SEV_NINGUNA;
    salida->fuente = FUS_FUENTE_NINGUNA;
    salida->detector_solo_decide = 0;
    salida->modelo_vigente = 0;
    salida->edad_modelo_ms = 0;
    salida->aviso = 0;
    salida->aviso_clase = -1;
    salida->aviso_zona = ZONA_CENTRO;
    salida->aviso_cercania = 0;
    salida->clase_en_camino = -1;
    salida->desmentidos = 0;
    salida->bajados_flujo = 0;
}

/* Anota un aviso (algo que solo se informa). Se queda con el de mayor cercanía. */
static void anotar_aviso(ResultadoFusion *salida, int clase, int zona, int cercania)
{
    if (!salida->aviso || cercania > salida->aviso_cercania) {
        salida->aviso = 1;
        salida->aviso_clase = clase;
        salida->aviso_zona = zona;
        salida->aviso_cercania = cercania;
    }
}

/* La fusión conserva evidencia por zona. Una fuente aislada es aviso;
 * el navegador puede confirmarla con láser. Solo color + neuronal frena por visión. */
static void combinar_zonas(const ResultadoObstaculos *detector, const ZonaObstaculo modelo[OBS_ZONAS], ResultadoFusion *salida)
{
    int z;
    for (z = 0; z < OBS_ZONAS; z++) {
        int color = detector->zona[z].hay_obstaculo && detector->zona[z].cercania > 0;
        int neuronal = modelo[z].hay_obstaculo && modelo[z].cercania > 0;
        salida->zona[z] = detector->zona[z];
        salida->fuentes_vision[z] = (color ? 1 : 0) | (neuronal ? 2 : 0);
        salida->fuente |= salida->fuentes_vision[z];
        salida->zona[z].hay_obstaculo = color && neuronal;
        salida->zona[z].cercania = mayor(color ? detector->zona[z].cercania : 0, neuronal ? modelo[z].cercania : 0);
        if (salida->zona[z].hay_obstaculo) {
            /* El modelo confirma también zonas laterales, fuera del haz láser. */
            salida->zona[z].fuera_corredor = 0;
        } else if (color || neuronal) {
            anotar_aviso(salida, -1, z, salida->zona[z].cercania);
            salida->desmentidos += color;
        }
    }
    salida->severidad = severidad_de_zonas(salida->zona);
    if (!salida->severidad && salida->aviso) salida->severidad = FUS_SEV_AVISO;
    salida->detector_solo_decide = 0;
}

int fusion_decidir(const ConfigFusion *config, const ResultadoObstaculos *detector, const DeteccionesModelo *modelo,
                   unsigned int ahora_ms, ResultadoFusion *salida)
{
    ZonaObstaculo zonas[OBS_ZONAS] = {{0}};
    int clases[OBS_ZONAS] = {-1, -1, -1};
    int i, z;
    if (!salida) return FUS_ERR_ARGUMENTO;
    limpiar_salida(salida);
    if (!config || !detector) return FUS_ERR_ARGUMENTO;
    if (!fusion_config_es_valida(config)) return FUS_ERR_CONFIG;
    if (modelo) {
        unsigned int edad = ahora_ms - modelo->marca_ms;
        salida->edad_modelo_ms = edad > 2147483647u ? 2147483647 : (int)edad;
        salida->modelo_vigente = edad <= (unsigned int)config->vigencia_ms;
        for (i = 0; salida->modelo_vigente && i < modelo->n && i < FUS_MAX_CAJAS; i++) {
            const CajaModelo *c = &modelo->cajas[i];
            int cercania;
            if (c->confianza_mil < config->confianza_min_mil || c->xmax_mil <= c->xmin_mil || c->ymax_mil <= c->ymin_mil) continue;
            cercania = fusion_cercania_de_caja(config, c);
            for (z = 0; z < OBS_ZONAS; z++) {
                int desde = mayor(limitar_milesimas(c->xmin_mil), z * 1000 / 3);
                int hasta = limitar_milesimas(c->xmax_mil);
                int borde = (z + 1) * 1000 / 3;
                if (hasta > borde) hasta = borde;
                if (hasta - desde < config->solape_camino_mil || hasta <= desde) continue;
                zonas[z].hay_obstaculo = 1;
                zonas[z].cercania = mayor(zonas[z].cercania, cercania);
                clases[z] = c->clase;
                if (z == ZONA_CENTRO) salida->clase_en_camino = c->clase;
            }
        }
    }
    combinar_zonas(detector, zonas, salida);
    if (salida->aviso) salida->aviso_clase = clases[salida->aviso_zona];
    return FUS_OK;
}

int fusion_decidir_cualquiera(const ConfigFusion *config, const ResultadoObstaculos *detector, const ZonaObstaculo cualquiera[OBS_ZONAS],
                              int cualquiera_vigente, ResultadoFusion *salida)
{
    ZonaObstaculo vacias[OBS_ZONAS] = {{0}};
    if (!salida) return FUS_ERR_ARGUMENTO;
    limpiar_salida(salida);
    if (!config || !detector || !cualquiera) return FUS_ERR_ARGUMENTO;
    if (!fusion_config_es_valida(config)) return FUS_ERR_CONFIG;
    salida->modelo_vigente = cualquiera_vigente ? 1 : 0;
    combinar_zonas(detector, cualquiera_vigente ? cualquiera : vacias, salida);
    return FUS_OK;
}

int fusion_aplicar_flujo(const ConfigFusion *config, const ZonaFlujo flujo[OBS_ZONAS], int flujo_vigente, ResultadoFusion *salida)
{
    int z;

    if (config == NULL || flujo == NULL || salida == NULL) {
        return FUS_ERR_ARGUMENTO;
    }
    if (!flujo_vigente) {
        return FUS_OK;
    }
    for (z = 0; z < OBS_ZONAS; z++) {
        ZonaObstaculo *zona = &salida->zona[z];

        if (!zona->hay_obstaculo && flujo[z].hay_cerca) {
            anotar_aviso(salida, -1, z, (flujo[z].cercania > 0) ? flujo[z].cercania : 1);   /* b) solo avisa */
        }
    }
    salida->severidad = severidad_de_zonas(salida->zona);
    if (salida->severidad == FUS_SEV_NINGUNA && salida->aviso) {
        salida->severidad = FUS_SEV_AVISO;
    }
    salida->detector_solo_decide = 0;
    return FUS_OK;
}

const char *fusion_texto_fuente(int fuente)
{
    switch (fuente) {
    case FUS_FUENTE_DETECTOR: return "detector";
    case FUS_FUENTE_MODELO:   return "modelo";
    case FUS_FUENTE_AMBOS:    return "ambos";
    default:                  return "nadie";
    }
}
