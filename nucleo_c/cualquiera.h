/*
 * cualquiera.h — Modo «cualquier objeto» del modelo de objetos: usar EfficientDet SOLO para saber que hay una caja sobre el piso, ignorando la clase.
 *
 * GUÍA DE LECTURA
 *  1. Por qué: con la cámara del carro el modelo casi nunca acierta la clase (no reconoció el termo; inventó «person» y «refrigerator»), pero SÍ suele
 *     dibujar una caja sobre lo que está en el piso. Este modo descarta la clase y se queda con la caja.
 *  2. Filtros geométricos (todos configurables, ver ConfigCualquiera): la confianza debe SUPERAR el umbral (UMBRAL_CUALQUIERA, 0.15 por defecto; se puede bajar
 *     hasta 0.01); se descartan las cajas que cubren más del 60 % del cuadro («refrigerator» sobre una pared lisa), las que tienen el borde de abajo arriba de la
 *     mitad de la imagen (lo que está en el fondo o flota), las muy pequeñas y las muy angostas. Solo pasan las que TOCAN LA ZONA INFERIOR.
 *  3. Persistencia: una caja cuenta si aparece en el mismo sitio (superposición mayor al 50 %) en PERSISTENCIA_CUADROS cuadros SEGUIDOS del modelo (2 por
 *     defecto). Con cercanía alta (3) basta un cuadro. Un cuadro del modelo es uno con marca de tiempo distinta: repetir la misma marca no avanza la historia.
 *  4. Salida: las mismas zonas (izquierda, centro, derecha) y cercanía (por la posición vertical del borde de abajo) que da el detector de color, y las cajas
 *     en píxeles (CajaImagen) para el corredor de trayectoria (corredor.h).
 *  5. Apagado por defecto. La fusión con el color está en fusion_decidir_cualquiera (fusion.h): el color manda.
 *  Solo enteros, sin malloc ni recursión.
 */
#ifndef CUALQUIERA_H
#define CUALQUIERA_H

#include "fusion.h"
#include "corredor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Valores por defecto ───────────────────────────────────────────────── */

#define UMBRAL_CUALQUIERA_MIL        150   /* confianza mínima (se debe SUPERAR): 0.15 */
#define UMBRAL_CUALQUIERA_MIN_MIL     10   /* lo más bajo que acepta la pantalla de visión: 0.01 */
#define UMBRAL_CUALQUIERA_MAX_MIL    500   /* lo más alto: 0.50 */
#define CUALQ_AREA_MAX_MIL           600   /* se descartan las cajas que cubren más del 60 % del cuadro */
#define CUALQ_BORDE_INFERIOR_MIN_MIL 500   /* se descartan las cajas con el borde de abajo arriba de la mitad (menos de 500 milésimas) */
#define CUALQ_AREA_MIN_MIL            15   /* se descartan las muy pequeñas: menos del 1.5 % del cuadro */
#define CUALQ_ANCHO_MIN_MIL           40   /* y las muy angostas: menos del 4 % del ancho */
#define PERSISTENCIA_CUADROS           2   /* cuadros seguidos del modelo en el mismo sitio */
#define CUALQ_MAX_PERSISTENCIA         4
#define CUALQ_SOLAPE_MIL             500   /* superposición (intersección / unión) mayor a esto = «el mismo sitio» */
#define CUALQ_CERCANIA_ALTA            3   /* con esta cercanía basta un cuadro */

/* Por qué se descarta una caja (0 = pasa). */
#define CUALQ_PASA            0
#define CUALQ_POCA_CONFIANZA  1
#define CUALQ_MUY_GRANDE      2
#define CUALQ_BORDE_ALTO      3
#define CUALQ_MUY_CHICA       4
#define CUALQ_MUY_ANGOSTA     5
#define CUALQ_CAJA_INVALIDA   6

/* ── Tipos ─────────────────────────────────────────────────────────────── */

typedef struct {
    int activo;                    /* 0 = apagado (por defecto) */
    int umbral_mil;                /* la confianza de la caja debe SUPERAR esto (10 a 500) */
    int area_max_mil;              /* descarta las que cubren más de esto del cuadro */
    int borde_inferior_min_mil;    /* descarta las que tienen el borde de abajo arriba de esto */
    int area_min_mil;              /* descarta las más chicas que esto */
    int ancho_min_mil;             /* descarta las más angostas que esto */
    int persistencia;              /* cuadros seguidos del modelo en el mismo sitio (1 a CUALQ_MAX_PERSISTENCIA) */
    int solape_mil;                /* superposición mínima para ser «el mismo sitio» */
    int vigencia_ms;               /* un resultado del modelo más viejo que esto se ignora */
} ConfigCualquiera;

typedef struct {
    int xmin_mil, ymin_mil, xmax_mil, ymax_mil;
} CajaMil;

/* Lo que se recuerda de los cuadros anteriores del modelo (las cajas que pasaron los filtros de cada uno). */
typedef struct {
    int cuadros;                                             /* cuántos cuadros del modelo hay en la historia (hasta persistencia - 1) */
    int n_cajas[CUALQ_MAX_PERSISTENCIA];
    CajaMil cajas[CUALQ_MAX_PERSISTENCIA][FUS_MAX_CAJAS];    /* [0] es el cuadro anterior, [1] el anterior a ese... */
    unsigned int ultima_marca;
    int hay_marca;
} EstadoCualquiera;

/* Lo que sale de un cuadro. */
typedef struct {
    ZonaObstaculo zona[OBS_ZONAS];   /* hay_obstaculo y cercania por zona de las cajas que cuentan (celdas = 1) */
    int vigente;                     /* 1 si el resultado del modelo se usó (no estaba caducado) */
    int edad_modelo_ms;
    int cajas_modelo;                /* cuántas cajas trajo el modelo */
    int pasan_filtros;               /* cuántas pasaron los filtros geométricos */
    int persistentes;                /* cuántas pasaron además la persistencia: las que cuentan */
    int n_cuentan;
    CajaMil cuentan[FUS_MAX_CAJAS];  /* las cajas que cuentan */
    int zona_cuentan[FUS_MAX_CAJAS]; /* su zona */
    int cercania_cuentan[FUS_MAX_CAJAS];   /* y su cercanía */
    int mejor_zona;                  /* zona y cercanía de la que más cerca está (-1 / 0 si ninguna) */
    int mejor_cercania;
    int nuevo_cuadro;                /* 1 si este resultado del modelo era un cuadro nuevo (marca distinta) */
} ResultadoCualquiera;

#define CUALQ_OK               0
#define CUALQ_ERR_ARGUMENTO   -1
#define CUALQ_ERR_CONFIG      -2

/* ── Funciones ─────────────────────────────────────────────────────────── */

/* Los valores por defecto, con el modo APAGADO. */
void cualq_config_defecto(ConfigCualquiera *config);

/* 1 si la configuración sirve (umbral de 10 a 500, áreas de 0 a 1000, persistencia de 1 a CUALQ_MAX_PERSISTENCIA...). */
int cualq_config_es_valida(const ConfigCualquiera *config);

/* Deja el estado sin historia. */
void cualq_iniciar(EstadoCualquiera *estado);

/* ¿Esta caja pasa los filtros geométricos y de confianza? Devuelve CUALQ_PASA o el motivo por el que se descarta (CUALQ_*). No mira la persistencia. */
int cualq_motivo_descarte(const ConfigCualquiera *config, const CajaModelo *caja);

/* Cercanía (1 a 3) de una caja por la posición vertical de su borde de abajo (los mismos límites del detector de color: OBS_MEDIA_MIL y OBS_CERCA_MIL). */
int cualq_cercania_de_caja(const CajaModelo *caja);

/* Zona de una caja (el tercio del ancho donde cae su centro). */
int cualq_zona_de_caja(const CajaModelo *caja);

/* Procesa lo último que dijo el modelo (puede ser NULL = nada) a la hora `ahora_ms`: filtra, aplica la persistencia y llena las zonas. Si `modelo` trae una
 * marca distinta de la anterior es un cuadro nuevo y entra a la historia; con la misma marca no. Devuelve CUALQ_OK o un error (con un error no cambia el estado). */
int cualq_actualizar(const ConfigCualquiera *config, EstadoCualquiera *estado, const DeteccionesModelo *modelo, unsigned int ahora_ms, ResultadoCualquiera *resultado);

/* Convierte las cajas que cuentan en CajaImagen (píxeles de un cuadro de ancho x alto) con ese peso, para el corredor de trayectoria. Devuelve cuántas escribió. */
int cualq_a_cajas_imagen(const ResultadoCualquiera *resultado, int ancho, int alto, int peso, CajaImagen *cajas, int capacidad);

/* Texto corto en español del motivo de descarte. */
const char *cualq_texto_motivo(int motivo);

#ifdef __cplusplus
}
#endif

#endif /* CUALQUIERA_H */
