/*
 * corredor.h — El CORREDOR de trayectoria: la huella del carro proyectada en la imagen.
 *
 * GUÍA DE LECTURA
 *  1. Para qué: hoy el navegador decide con tres zonas (izquierda, centro, derecha) que son tercios de la imagen. Un objeto que pasa
 *     por un lado, fuera de por donde va a pasar el carro, también frena. El corredor es la franja de piso por la que el carro SÍ va a
 *     pasar (su ancho más un margen), dibujada en la imagen con la perspectiva: ancha cerca y angosta lejos. Un obstáculo frena o hace
 *     esquivar solo si está DENTRO del corredor; el que pasa por un lado solo AVISA.
 *  2. Geometría (cámara de agujero, mirando hacia adelante e inclinada hacia abajo), con enteros:
 *       - la cámara está a ALTURA_CAMARA_CM del piso, inclinada INCLINACION_GRADOS hacia abajo, con un campo de visión horizontal FOV;
 *       - la distancia focal en píxeles es f = (ancho / 2) / tan(FOV / 2);
 *       - para la fila v de la imagen (cy = alto / 2 es el centro): A(v) = f * sen(incl) + (v - cy) * cos(incl);
 *         el piso que se ve en esa fila está a  d = altura * (f * cos(incl) - (v - cy) * sen(incl)) / A(v)  cm hacia adelante, y un
 *         ancho de x cm a esa distancia mide  x * A(v) / altura  píxeles. Como A(v) crece hacia abajo, el corredor se ESTRECHA con la
 *         distancia (hacia arriba en la imagen) y llega a cero en el horizonte (A = 0).
 *  3. Una CAJA (en píxeles) toca el piso por su borde de abajo: esa fila da la distancia y el ancho del corredor ahí. La caja está
 *     DENTRO si cruza la huella del carro (ancho / 2 a cada lado del centro), en el BORDE si solo cruza el margen lateral (y por la regla
 *     "en la duda, frena" también cuenta como en el corredor), y FUERA si no toca ni el margen.
 *  4. Una sola interfaz para todas las fuentes: CajaImagen (rectángulo en píxeles + peso + quién la dio). El detector de color la usa con
 *     una caja por celda marcada (peso 1), el modelo de objetos con una caja por objeto (peso grande: una sola basta) y el flujo del piso
 *     con una caja por bloque. corredor_aplicar_zonas() reparte las cajas en las tres zonas del navegador y marca cuáles quedan fuera.
 *  5. APAGADO por defecto (ConfigCorredor.activo = 0): con el corredor apagado nada cambia. Los valores por defecto son de partida: hay que
 *     MEDIR con una cinta el ancho del carro, la altura de la cámara y su inclinación (docs/obstaculos.md, «Corredor»).
 *  Solo enteros, sin malloc ni recursión; los errores son códigos de retorno.
 */
#ifndef CORREDOR_H
#define CORREDOR_H

#include "obstaculos.h"

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

/* ── Valores por defecto (de partida: HAY QUE MEDIRLOS en el carro) ─────── */

#define ANCHO_CARRO_CM            18   /* ancho del carro (las ruedas más anchas), en cm */
#define ALTURA_CAMARA_CM          10   /* altura de la lente sobre el piso, en cm */
#define INCLINACION_GRADOS        20   /* cuánto mira la cámara hacia abajo respecto a la horizontal, en grados */
#define FOV_GRADOS                60   /* campo de visión HORIZONTAL de la cámara, en grados */
#define MARGEN_LATERAL_CM          3   /* margen a cada lado de la huella del carro, en cm */
#define CORREDOR_MIN_PESO          2   /* peso mínimo dentro del corredor para que una zona cuente (con celdas de peso 1: 2 celdas) */

/* ── Tipos ─────────────────────────────────────────────────────────────── */

/* Quién produjo la caja (solo informa; el corredor trata igual a todas). */
#define COR_FUENTE_DETECTOR   0
#define COR_FUENTE_MODELO     1
#define COR_FUENTE_FLUJO      2

/* Resultado de clasificar una caja. */
#define COR_FUERA     0   /* no toca ni la huella del carro ni el margen: solo avisa */
#define COR_BORDE     1   /* toca solo el margen lateral: en la duda, frena */
#define COR_DENTRO    2   /* cruza la huella del carro: frena */

/* Un rectángulo del cuadro en PÍXELES: x0 y y0 son el primer píxel; x1 y y1 el borde (primer píxel que ya NO es de la caja).
 * `peso` es cuánto cuenta dentro de su zona (celda del detector: 1; objeto del modelo o bloque de flujo: lo que decida quien llama). */
typedef struct {
    int x0, y0, x1, y1;
    int peso;
    int fuente;   /* COR_FUENTE_* */
} CajaImagen;

/* Configuración. Con `activo` = 0 el corredor está APAGADO (no filtra nada). */
typedef struct {
    int activo;
    int ancho_carro_cm;
    int altura_camara_cm;
    int inclinacion_grados_x10;   /* grados por 10 (200 = 20.0) */
    int fov_grados_x10;           /* campo de visión horizontal, grados por 10 (600 = 60.0) */
    int margen_lateral_cm;
    int min_peso;                 /* peso mínimo dentro del corredor para que una zona cuente (CORREDOR_MIN_PESO) */
} ConfigCorredor;

/* El corredor ya preparado para un tamaño de cuadro (guarda f, sen y cos para no recalcular). Se llena con corredor_preparar. */
typedef struct {
    ConfigCorredor config;
    int ancho, alto;        /* tamaño del cuadro en píxeles */
    int cx, cy;             /* centro del cuadro */
    int f;                  /* distancia focal en píxeles */
    int seno_mil, coseno_mil;   /* del ángulo de inclinación, en milésimas */
} Corredor;

/* Qué se supo de una caja. */
typedef struct {
    int estado;             /* COR_FUERA, COR_BORDE o COR_DENTRO */
    int distancia_cm;       /* distancia hacia adelante del piso bajo el borde de abajo de la caja; -1 si está sobre el horizonte */
    int sobre_horizonte;    /* 1 si el borde de abajo está en o sobre el horizonte (no se puede ubicar en el piso): estado = COR_FUERA */
    int semiancho_huella_px;   /* mitad del ancho de la huella del carro en esa fila, en píxeles */
    int semiancho_corredor_px; /* mitad del ancho del corredor (huella + margen) en esa fila */
} ResultadoCaja;

/* Códigos de retorno. */
#define COR_OK                 0
#define COR_ERR_ARGUMENTO     -1   /* puntero nulo o tamaño de cuadro no válido */
#define COR_ERR_CONFIG        -2   /* configuración no válida (ver corredor_config_es_valida) */

/* ── Funciones ─────────────────────────────────────────────────────────── */

/* Los valores por defecto de arriba, con el corredor APAGADO. */
void corredor_config_defecto(ConfigCorredor *config);

/* 1 si la configuración sirve: ancho de 1 a 100 cm, altura de 1 a 100 cm, inclinación de 0 a 80 grados, campo de visión de 20 a 150 grados,
 * margen de 0 a 50 cm y peso mínimo de 1 a 1000. */
int corredor_config_es_valida(const ConfigCorredor *config);

/* Prepara el corredor para cuadros de `ancho` x `alto` píxeles (de 30x30 a 640x480). Devuelve COR_OK, COR_ERR_ARGUMENTO o COR_ERR_CONFIG.
 * Se puede preparar aunque `activo` sea 0 (las funciones de clasificar siguen funcionando; solo corredor_aplicar_zonas lo respeta). */
int corredor_preparar(Corredor *corredor, const ConfigCorredor *config, int ancho, int alto);

/* Clasifica una caja: dentro, en el borde o fuera del corredor, y a qué distancia está. Devuelve COR_OK o COR_ERR_ARGUMENTO. */
int corredor_clasificar(const Corredor *corredor, const CajaImagen *caja, ResultadoCaja *resultado);

/* Distancia hacia adelante (cm) del piso que se ve en la fila `v` (píxeles, 0 arriba); -1 si esa fila está en o sobre el horizonte. */
int corredor_distancia_de_fila(const Corredor *corredor, int v);

/* Mitad del ancho (en píxeles) que ocupa en la fila `v` un ancho de `ancho_cm` cm centrado en el carro; 0 en el horizonte o sobre él. */
int corredor_semiancho_px(const Corredor *corredor, int v, int ancho_cm);

/* Reparte las cajas en las tres zonas del navegador y marca cuáles quedan FUERA del corredor.
 *   - La zona de una caja es el tercio del ancho donde cae su centro (como el detector de color).
 *   - Entran al corredor las cajas DENTRO o en el BORDE. Si el peso total de una zona dentro del corredor llega a `min_peso`, la zona cuenta;
 *     si la zona tiene obstáculo (hay_obstaculo = 1) pero no llega, queda fuera_corredor = 1 (solo avisa).
 *   - cercania_corredor: 1, 2 o 3 según la fila más baja (borde de abajo) de las cajas dentro del corredor, con los mismos límites del detector
 *     (OBS_MEDIA_MIL y OBS_CERCA_MIL); celdas_corredor: el peso dentro del corredor.
 * `zonas` ya trae hay_obstaculo y cercania de quien las produjo (detector, modelo o flujo); solo se rellenan los tres campos del corredor. Con el
 * corredor apagado (activo = 0) o NULL no toca nada: fuera_corredor = 0 y cercania_corredor = 0 (el navegador usa la cercanía de siempre).
 * Devuelve COR_OK o COR_ERR_ARGUMENTO. */
int corredor_aplicar_zonas(const Corredor *corredor, const CajaImagen *cajas, int n_cajas, ZonaObstaculo zonas[OBS_ZONAS]);

/* El detector de color: usa la máscara de celdas "no piso" (una caja de peso 1 por celda, de la fila mínima hacia abajo) y llama a
 * corredor_aplicar_zonas con ellas. `mascara` tiene `filas` filas de `columnas` celdas y cada fila empieza `paso_fila` posiciones después de la anterior (paso_fila >= columnas). Devuelve COR_OK o un error. */
int corredor_aplicar_mascara(const Corredor *corredor, const unsigned char *mascara, int paso_fila, int columnas, int filas, int fila_minima,
                             ZonaObstaculo zonas[OBS_ZONAS]);

/* Lo mismo directamente con un resultado del detector (usa su máscara no_piso y su fila mínima) y deja el resultado en sus zonas. */
int corredor_aplicar_resultado(const Corredor *corredor, ResultadoObstaculos *resultado);

/* Convierte una caja en milésimas del cuadro (como las del modelo de objetos) a píxeles. */
void corredor_caja_de_milesimas(int xmin_mil, int ymin_mil, int xmax_mil, int ymax_mil, int ancho, int alto, int peso, int fuente, CajaImagen *caja);

/* Texto corto en español: "fuera", "borde" o "dentro". */
const char *corredor_texto_estado(int estado);

#ifdef __cplusplus
}
#endif

#endif /* CORREDOR_H */
