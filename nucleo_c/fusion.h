/*
 * fusion.h — Une el detector de piso por color (rápido, ~7 ms) con el modelo de objetos
 * EfficientDet-Lite0 (más lento, ~45 ms) para decidir qué hay delante del carro.
 *
 * GUÍA DE LECTURA
 *  1. Por qué dos: el detector de color ve cualquier cosa que no se parezca al piso (una caja, una pared)
 *     pero no sabe qué es; el modelo reconoce objetos (persona, silla, botella...) aunque tengan el color
 *     del piso, pero tarda más y a veces se equivoca.
 *  2. Entrada: el resultado del detector del cuadro ACTUAL + las ÚLTIMAS detecciones del modelo, con su
 *     marca de tiempo (el modelo corre en otro hilo, a otro ritmo) + la hora de ahora.
 *  3. Salida: las tres zonas ya fusionadas (hay obstáculo y cercanía, el mismo formato que el detector, así
 *     que el navegador las usa igual), la severidad, y QUIÉN decidió (detector, modelo o ambos).
 *  4. Reglas (en este orden de importancia):
 *       a) El DETECTOR MANDA en seguridad: si el detector ve un obstáculo cerca, la salida es esa, sin esperar
 *          al modelo y aunque el modelo diga otra cosa (contradicción) o esté caducado o ausente. La fusión
 *          nunca baja lo que ve el detector.
 *       b) Si el modelo ve un objeto EN EL CAMINO (cruza la zona central) y el detector DUDA (ve algo pero
 *          menos cerca, o ve unas pocas celdas que no llegan al mínimo), sube la severidad al mayor de los dos.
 *       c) Si el modelo ve un objeto lejano que el detector no ve, SOLO AVISA: no frena.
 *       d) Si el modelo ve un objeto cercano (cercanía 2 o 3) que el detector no ve en absoluto, frena solo
 *          si el modelo está muy seguro (CONFIANZA_FRENAR); si no, solo avisa.
 *       e) Un resultado del modelo más viejo que `vigencia_ms` (300 ms) se ignora por completo.
 *  5. Caja → zona y cercanía: ver fusion_caja_a_zona y fusion_cercania_de_caja. La cercanía sale de la posición
 *     vertical del borde de abajo de la caja y de su tamaño; los límites son los mismos que usa el detector
 *     de color (OBS_MEDIA_MIL y OBS_CERCA_MIL), así los dos hablan el mismo idioma.
 *  6. Clases de interés (persona, mascota, silla, mesa, botella...): se configuran con fusion_marcar_clase.
 *     Una clase de interés fuera del camino también genera un AVISO. Cualquier otra clase cuenta igual cuando
 *     está en el camino: el modelo no necesita "conocer" el objeto para que frene.
 *  Solo enteros, sin malloc, sin recursión; los errores son códigos de retorno.
 */
#ifndef FUSION_H
#define FUSION_H

#include "obstaculos.h"

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

/* ── Constantes ────────────────────────────────────────────────────────── */

#define FUS_MAX_CAJAS        10    /* cuántas cajas del modelo se guardan por cuadro */
#define FUS_CLASES_MAX       96    /* clases numeradas de 0 a 95 (COCO tiene 80 en 90 números) */
#define FUS_PALABRAS_CLASES   3    /* 3 palabras de 32 bits = 96 clases */

/* Quién decidió. */
#define FUS_FUENTE_NINGUNA    0    /* no hay nada que decidir */
#define FUS_FUENTE_DETECTOR   1    /* solo el detector de color */
#define FUS_FUENTE_MODELO     2    /* solo el modelo de objetos (subió la severidad por su cuenta) */
#define FUS_FUENTE_AMBOS      3    /* los dos están de acuerdo, o el modelo subió lo que el detector dudaba */

/* Severidad. */
#define FUS_SEV_NINGUNA       0
#define FUS_SEV_AVISO         1    /* solo informar: algo lejos, o un objeto de interés fuera del camino */
#define FUS_SEV_FRENAR        2    /* obstáculo a media distancia */
#define FUS_SEV_PARAR         3    /* obstáculo muy cerca: parar ya */

/* ── Tipos ─────────────────────────────────────────────────────────────── */

/* Un objeto que vio el modelo. La caja va en MILÉSIMAS del cuadro (0 a 1000), con el origen arriba a la
 * izquierda (igual que la imagen); la confianza también en milésimas. */
typedef struct {
    int clase;
    int confianza_mil;
    int xmin_mil, ymin_mil, xmax_mil, ymax_mil;
} CajaModelo;

/* Lo último que dijo el modelo, con la hora a la que terminó de calcularlo. */
typedef struct {
    unsigned int marca_ms;
    int n;
    CajaModelo cajas[FUS_MAX_CAJAS];
} DeteccionesModelo;

/* Parámetros ajustables (ConfigFusion). */
typedef struct {
    int vigencia_ms;          /* un resultado del modelo más viejo que esto se ignora (300) */
    int confianza_min_mil;    /* por debajo de esta confianza una caja no cuenta (600 = 60 %) */
    int confianza_frenar_mil; /* para frenar SOLO por el modelo (detector sin evidencia) hace falta esta confianza (600) */
    int solape_camino_mil;    /* una caja está "en el camino" si cruza la zona central al menos esto, en milésimas del ancho (60) */
    int cerca_mil;            /* borde de abajo de la caja a partir de aquí = cercanía 3 (834, como el detector) */
    int media_mil;            /* ... a partir de aquí = cercanía 2 (667) */
    int area_grande_mil;      /* una caja que ocupa esto o más del cuadro (en milésimas del área) sube una cercanía (200) */
    unsigned int clases_interes[FUS_PALABRAS_CLASES];   /* bits de las clases de interés (ver fusion_marcar_clase) */
} ConfigFusion;

/* Lo que sale de decidir. */
typedef struct {
    ZonaObstaculo zona[OBS_ZONAS];   /* zonas ya fusionadas: el mismo formato que usa el navegador */
    int severidad;                   /* FUS_SEV_* (la mayor de las tres zonas, o aviso si solo hay un aviso) */
    int fuente;                      /* FUS_FUENTE_* */
    int detector_solo_decide;        /* 1 si el detector solo ya pide frenar (cercanía >= 2): no se espera al modelo */
    int modelo_vigente;              /* 1 si el resultado del modelo se usó (no estaba caducado ni vacío) */
    int edad_modelo_ms;              /* edad del resultado del modelo (0 si no hay) */
    int aviso;                       /* 1 si hay algo que solo se avisa (no frena) */
    int aviso_clase;                 /* clase del objeto del aviso (-1 si no hay) */
    int aviso_zona;                  /* zona del aviso */
    int aviso_cercania;              /* cercanía del aviso (1 a 3) */
    int clase_en_camino;             /* clase del objeto del modelo que cuenta en el camino (-1 si no hay) */
    int desmentidos;                 /* modo «cualquier objeto»: cuántas zonas con un falso positivo de color de cercanía baja se desmintieron */
} ResultadoFusion;

/* Códigos de retorno. */
#define FUS_OK               0
#define FUS_ERR_ARGUMENTO   -1   /* un puntero nulo */
#define FUS_ERR_CONFIG      -2   /* la configuración no es válida */

/* ── Funciones ─────────────────────────────────────────────────────────── */

/* Llena la configuración con los valores por defecto y las clases de interés por defecto (persona, gato, perro, botella, silla,
 * mesa). Los números son los de la lista de etiquetas de EfficientDet-Lite0 (90 posiciones; el número es el id de COCO menos 1):
 * persona 0, gato 16, perro 17, botella 43, silla 61, mesa 66. La app los busca por NOMBRE en las etiquetas del modelo y los
 * pone con fusion_marcar_clase; estos números son solo el valor de partida. */
void fusion_config_defecto(ConfigFusion *config);

/* Marca (valor 1) o desmarca (valor 0) una clase de interés. Devuelve FUS_OK o FUS_ERR_ARGUMENTO si la clase no existe. */
int fusion_marcar_clase(ConfigFusion *config, int clase, int de_interes);

/* ¿La clase es de interés? */
int fusion_clase_de_interes(const ConfigFusion *config, int clase);

/* 1 si la configuración es válida. */
int fusion_config_es_valida(const ConfigFusion *config);

/* Convierte la caja a zona y dice si está EN EL CAMINO (cruza la zona central). Si está en el camino la zona es el centro;
 * si no, la zona (izquierda o derecha) donde cae el centro de la caja. Devuelve la zona y deja en *en_camino 1 o 0. */
int fusion_caja_a_zona(const ConfigFusion *config, const CajaModelo *caja, int *en_camino);

/* Cercanía aproximada (1 a 3) de la caja: por el borde de abajo (>= cerca_mil → 3, >= media_mil → 2, si no 1) y, si la caja es
 * grande (area_grande_mil), una más (hasta 3). */
int fusion_cercania_de_caja(const ConfigFusion *config, const CajaModelo *caja);

/* Decide con el resultado del detector del cuadro actual, lo último del modelo (puede ser NULL = ninguno) y la hora de ahora (en
 * ms, el mismo reloj que `marca_ms`). Devuelve FUS_OK o un código de error; con un error `salida` queda en ceros. */
int fusion_decidir(const ConfigFusion *config, const ResultadoObstaculos *detector, const DeteccionesModelo *modelo,
                   unsigned int ahora_ms, ResultadoFusion *salida);

/* MODO «CUALQUIER OBJETO» (cualquiera.h): el modelo no dice QUÉ es, solo que hay una caja sobre el piso (ya filtrada y con persistencia: `cualquiera` trae las zonas que
 * cuentan). La regla, que se prueba en pruebas_cualquiera.c, es: EL COLOR MANDA EN SEGURIDAD.
 *   a) El color ve obstáculo en una zona y «cualquier objeto» también (coinciden): la zona queda con la MAYOR de las dos cercanías (sube la severidad); fuente AMBOS.
 *   b) El color DUDA (algunas celdas, menos del mínimo) y «cualquier objeto» ve algo en esa zona: se sube al valor de «cualquier objeto»; fuente AMBOS.
 *   c) «Cualquier objeto» ve algo y el color no ve nada ni duda: solo AVISO (aviso = 1), NUNCA frena.
 *   d) El color ve obstáculo con cercanía BAJA (1: lejos), «cualquier objeto» tiene un resultado vigente y NO ve nada en esa zona: se DESMIENTE el falso positivo de color
 *      (la zona queda libre y se cuenta en `desmentidos`). Con cercanía 2 o 3 el color nunca se desmiente.
 *   e) Sin resultado vigente de «cualquier objeto» (`cualquiera_vigente` = 0) no cambia nada: sale lo que vio el color.
 * Devuelve FUS_OK o un código de error; con un error `salida` queda en ceros. */
int fusion_decidir_cualquiera(const ConfigFusion *config, const ResultadoObstaculos *detector, const ZonaObstaculo cualquiera[OBS_ZONAS],
                              int cualquiera_vigente, ResultadoFusion *salida);

/* Texto corto en español de una fuente (para mostrar en pantalla). */
const char *fusion_texto_fuente(int fuente);

#ifdef __cplusplus
}
#endif

#endif /* FUSION_H */
