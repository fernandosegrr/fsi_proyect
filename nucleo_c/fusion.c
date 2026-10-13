/*
 * fusion.c — Fusión del detector de piso por color con el modelo de objetos (ver fusion.h).
 *
 * GUÍA DE LECTURA
 *  1. Configuración: fusion_config_defecto, fusion_marcar_clase y fusion_clase_de_interes (las clases de
 *     interés son un conjunto de bits: una palabra de 32 bits cada 32 clases).
 *  2. Caja → zona y cercanía: fusion_caja_a_zona y fusion_cercania_de_caja (la parte "geométrica").
 *  3. fusion_decidir(): el corazón. Primero copia lo que ve el detector (es lo que manda en seguridad), luego
 *     revisa una por una las cajas del modelo (si no están caducadas) y aplica las reglas del encabezado:
 *     subir lo que el detector duda, frenar solo por el modelo si está muy seguro, o solo avisar.
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
    config->confianza_min_mil = 400;
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

/* Sube la zona al menos a esa cercanía y la marca con obstáculo. */
static void subir_zona(ZonaObstaculo *zona, int cercania)
{
    zona->hay_obstaculo = 1;
    if (cercania > zona->cercania) {
        zona->cercania = cercania;
    }
    if (zona->celdas < 1) {
        zona->celdas = 1;
    }
}

int fusion_decidir(const ConfigFusion *config, const ResultadoObstaculos *detector, const DeteccionesModelo *modelo,
                   unsigned int ahora_ms, ResultadoFusion *salida)
{
    int z, i;
    int vigente = 0;
    int detector_ve_algo = 0;
    int uso_detector = 0, uso_modelo = 0;
    int mejor_en_camino = 0;

    if (salida == NULL) {
        return FUS_ERR_ARGUMENTO;
    }
    limpiar_salida(salida);
    if (config == NULL || detector == NULL) {
        return FUS_ERR_ARGUMENTO;
    }
    if (!fusion_config_es_valida(config)) {
        return FUS_ERR_CONFIG;
    }

    /* 1. Lo que ve el detector manda: se copia tal cual y la fusión nunca lo baja. */
    for (z = 0; z < OBS_ZONAS; z++) {
        salida->zona[z] = detector->zona[z];
        if (detector->zona[z].hay_obstaculo) {
            detector_ve_algo = 1;
        }
    }
    salida->detector_solo_decide = (severidad_de_zonas(salida->zona) >= FUS_SEV_FRENAR) ? 1 : 0;
    uso_detector = detector_ve_algo;

    /* 2. El modelo, si hay algo y no está caducado. */
    if (modelo != NULL && modelo->n > 0) {
        int edad = (int)(ahora_ms - modelo->marca_ms);

        if (edad < 0) {
            edad = 0;   /* un reloj que va hacia atrás no hace caducar nada */
        }
        salida->edad_modelo_ms = edad;
        vigente = (edad <= config->vigencia_ms);
        salida->modelo_vigente = vigente;
    }
    if (vigente) {
        int n = (modelo->n > FUS_MAX_CAJAS) ? FUS_MAX_CAJAS : modelo->n;

        for (i = 0; i < n; i++) {
            const CajaModelo *caja = &modelo->cajas[i];
            int en_camino, zona, cercania;
            const ZonaObstaculo *det;
            int detector_ve, detector_duda;

            if (caja->confianza_mil < config->confianza_min_mil) {
                continue;
            }
            zona = fusion_caja_a_zona(config, caja, &en_camino);
            cercania = fusion_cercania_de_caja(config, caja);
            if (!en_camino) {
                if (fusion_clase_de_interes(config, caja->clase)) {
                    anotar_aviso(salida, caja->clase, zona, cercania);   /* una persona a un lado: se avisa, no se frena */
                }
                continue;
            }
            /* EN EL CAMINO: cualquier clase cuenta igual. */
            det = &detector->zona[ZONA_CENTRO];
            detector_ve = det->hay_obstaculo && det->cercania > 0;
            detector_duda = !det->hay_obstaculo && det->celdas > 0;   /* algo hay, pero menos del mínimo */
            if (cercania > mejor_en_camino) {
                mejor_en_camino = cercania;
                salida->clase_en_camino = caja->clase;
            }
            if (detector_ve) {
                if (cercania > det->cercania) {
                    subir_zona(&salida->zona[ZONA_CENTRO], cercania);   /* el modelo lo ve más cerca que el detector */
                }
                uso_modelo = 1;
            } else if (detector_duda) {
                subir_zona(&salida->zona[ZONA_CENTRO], cercania);       /* el detector dudaba: se sube al valor del modelo */
                uso_modelo = 1;
                uso_detector = 1;
            } else if (cercania >= 2 && caja->confianza_mil >= config->confianza_frenar_mil) {
                subir_zona(&salida->zona[ZONA_CENTRO], cercania);       /* el detector no ve nada, pero el modelo está muy seguro */
                uso_modelo = 1;
            } else {
                anotar_aviso(salida, caja->clase, ZONA_CENTRO, cercania);   /* lejos, o poco seguro: solo se avisa */
            }
        }
    }

    /* 3. Severidad y quién decidió. */
    salida->severidad = severidad_de_zonas(salida->zona);
    if (salida->severidad == FUS_SEV_NINGUNA && salida->aviso) {
        salida->severidad = FUS_SEV_AVISO;
    }
    if (salida->zona[0].hay_obstaculo || salida->zona[1].hay_obstaculo || salida->zona[2].hay_obstaculo) {
        if (uso_detector && uso_modelo) {
            salida->fuente = FUS_FUENTE_AMBOS;
        } else if (uso_modelo) {
            salida->fuente = FUS_FUENTE_MODELO;
        } else {
            salida->fuente = FUS_FUENTE_DETECTOR;
        }
    } else if (salida->aviso) {
        salida->fuente = FUS_FUENTE_MODELO;   /* solo el modelo tiene algo que decir (un aviso) */
    }
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
