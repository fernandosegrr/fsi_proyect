/*
 * mundo_v2.h — Un "mundo" sintético para probar el modo autónomo completo en PC (autónomo v2).
 *
 * TODO LO DE AQUÍ ES SIMULADO. Sirve para comparar versiones del software con las mismas semillas;
 * no reemplaza las pruebas con el carro. Nunca usa imágenes reales: los cuadros se generan aquí.
 *
 * GUÍA DE LECTURA
 *  1. Geometría: milímetros; x crece a la derecha, y crece hacia "abajo" de la cuadrícula; rumbo 0 = hacia
 *     -y (arriba), 90 = hacia +x. Es la misma convención de navegador.c y simulador_navegador.c.
 *  2. Objetos: listas de PRISMAS rectos (cajas alineadas a los ejes del mundo) con color, altura y
 *     reflectancia para el láser. Un banquito o una silla son patas + asiento (+ respaldo).
 *  3. Cámara (mv_render_celdas): cámara de agujero a MV_CAM_ALTURA_MM del piso, inclinada MV_CAM_INCL_GRADOS,
 *     con MV_CAM_FOV_GRADOS de campo horizontal y 240x176 píxeles. Entrega la MISMA rejilla de 24x17 celdas
 *     RGB (promedio de 10x10 px) que manda la cámara real por UDP (paquete CEL1), que es la entrada de
 *     obstaculos_analizar_modelo_celdas. El piso es de losetas con juntas; la luz tiene manchas de sol,
 *     sombras y el reflejo especular de una ventana; la cámara tiene exposición y balance de blancos
 *     automáticos (o fijos) y ruido.
 *  4. Láser (mv_laser): cono de MV_LASER_CONO_GRADOS; un objeto se detecta si la señal (cobertura del cono x
 *     reflectancia / distancia^2) supera un mínimo. Sin retorno suficiente la lectura es "sin eco".
 *  5. Detector neuronal (mv_detector): modelo ESTADÍSTICO de EfficientDet-Lite0: cada objeto tiene una
 *     probabilidad de detección por inferencia (correlacionada en el tiempo), la caja sale de proyectar el
 *     objeto, y de vez en cuando inventa una caja. No corre ninguna red.
 *  6. Contacto (mv_contacto): rectángulo orientado del chasis contra los prismas bajos.
 *  Usa double y math.h: es código de banco de pruebas, no va en la app ni en el firmware.
 */
#ifndef MUNDO_V2_H
#define MUNDO_V2_H

#include <stdint.h>
#include "fusion.h"

/* ── Carro y sensores (SUPUESTOS, pendientes de medir en el carro; ver docs/autonomo_v2_diseno.md) ── */
#define MV_CARRO_ANCHO_MM       200.0
#define MV_CARRO_LARGO_MM       240.0
#define MV_CARRO_ALTO_MM        130.0    /* un prisma que empieza por encima de esto no toca el chasis */
#define MV_LASER_ADELANTE_MM    120.0    /* del centro al sensor frontal */
#define MV_LASER_ALTURA_MM       55.0
#define MV_LASER_CONO_GRADOS     25.0    /* cono completo del VL53L0X */
#define MV_LASER_ALCANCE_MM    1200.0    /* TOF_LIBRE_MM del firmware: más lejos es "libre / sin eco" */
#define MV_CAM_ADELANTE_MM      100.0    /* del centro a la lente */
#define MV_CAM_ALTURA_MM        115.0
#define MV_CAM_INCL_GRADOS       12.0
#define MV_CAM_FOV_GRADOS        56.0
#define MV_CAM_ANCHO_PX         240
#define MV_CAM_ALTO_PX          176
#define MV_COLUMNAS              24
#define MV_FILAS                 17

#define MV_MAX_PRISMAS           64
#define MV_MAX_OBJETOS            8
#define MV_MAX_PARCHES            6

typedef struct {
    double x0, y0, x1, y1;      /* planta (mm, ejes del mundo) */
    double z0, z1;              /* de qué altura a qué altura */
    unsigned char rgb[3];       /* color (albedo x 255) */
    double reflectancia;        /* para el láser: 0 = no devuelve nada (negro mate, vidrio), 0.5 = cartón */
    int objeto;                 /* índice del objeto al que pertenece */
    int fantasma;               /* 1 = solo cuenta para el contacto (la planta de un mueble): ni la cámara ni el láser lo ven */
} MvPrisma;

typedef struct {
    const char *nombre;
    double recall;              /* probabilidad de que el detector neuronal lo vea en una inferencia (0 = no es clase COCO) */
    int clase;                  /* clase COCO-1 que diría (61 silla, 56 banca...) */
    uint32_t aparece_ms;        /* 0 = está desde el principio */
    int en_ruta;                /* 1 = estorba la ruta: "pasarlo" exige dejarlo atrás */
    int det_estado;             /* memoria del detector para este objeto (correlación temporal) */
    double bx0, by0, bx1, by1;  /* caja envolvente en planta (se llena al cerrar el objeto) */
} MvObjeto;

/* Una mancha de luz fija en el piso: factor > 1 = sol, < 1 = sombra. Rectángulo girado con borde suave. */
typedef struct {
    double cx, cy, hx, hy, rot_rad;
    double factor;
    double borde_mm;
    double tinte[3];            /* el sol es más cálido: multiplica cada canal */
} MvParche;

typedef struct {
    /* Piso de losetas. */
    double loseta_mm, junta_mm, junta_factor, rot_rad, off_x, off_y, var_loseta;
    double albedo[3];           /* 0..1 */
    /* Luz. */
    int n_parches;
    MvParche parche[MV_MAX_PARCHES];
    int reflejo;                /* 1 = hay una ventana que se refleja en el piso brillante */
    double vent_x, vent_y, vent_nx, vent_ny, vent_semiancho, vent_z0, vent_z1, vent_brillo, brillo_piso;
    double sombra_propia;       /* < 1: sombra del propio carro delante de él (luz a la espalda); 1 = no hay */
    /* Cámara fotométrica. */
    int aec_auto, awb_auto;
    double ganancia, wb[3], objetivo_luma, ruido;
    double escalon_ganancia;    /* cambio brusco de exposición programado (1 = ninguno) */
    uint32_t escalon_ms;
    /* Fondo (arriba del horizonte). */
    double fondo[3];
} MvFoto;

typedef struct {
    MvPrisma prismas[MV_MAX_PRISMAS];
    int n_prismas;
    MvObjeto objetos[MV_MAX_OBJETOS];
    int n_objetos;
    MvFoto foto;
    /* Detector: falsas cajas. */
    double det_falsos_por_inferencia;
    int det_falso_vivo;         /* inferencias que le quedan a la caja inventada actual */
    CajaModelo det_falso;
    uint32_t azar;
} Mundo;

typedef struct { double x, y, rumbo_rad; } MvPose;

/* ── Construcción ──────────────────────────────────────────────────────── */
void mv_iniciar(Mundo *m, uint32_t semilla);
int  mv_objeto(Mundo *m, const char *nombre, double recall, int clase, uint32_t aparece_ms, int en_ruta);
int  mv_prisma(Mundo *m, int objeto, double x0, double y0, double x1, double y1, double z0, double z1,
               int r, int g, int b, double reflectancia);
/* Atajos: caja maciza; mueble de 4 patas con asiento (banquito) y, si respaldo_mm > 0, respaldo (silla). cx, cy = centro en planta. */
int  mv_caja(Mundo *m, const char *nombre, double cx, double cy, double ancho, double fondo, double alto,
             int r, int g, int b, double reflectancia, double recall, int clase, uint32_t aparece_ms);
int  mv_mueble(Mundo *m, const char *nombre, double cx, double cy, double ancho, double fondo, double pata_mm,
               double alto_asiento, double respaldo_mm, int r, int g, int b, double recall, int clase);
int  mv_parche(Mundo *m, double cx, double cy, double hx, double hy, double rot_grados, double factor, double borde_mm);

/* ── Sensores ──────────────────────────────────────────────────────────── */
/* Rejilla de 24x17 celdas RGB (MV_FILAS * MV_COLUMNAS * 3 bytes) vista desde la pose `p` a la hora `t_ms`. Actualiza la
 * exposición y el balance automáticos de la cámara: hay que llamarla en orden de tiempo, una vez por cuadro capturado. */
void mv_render_celdas(Mundo *m, MvPose p, uint32_t t_ms, unsigned char *celdas);
/* Lectura del láser en mm (8..1200). Devuelve 1 si es una medida real y 0 si no hubo eco suficiente (entonces *mm = 1200). */
int  mv_laser(Mundo *m, MvPose p, uint32_t t_ms, int *mm);
/* Lo que diría el detector neuronal con un cuadro capturado desde `p`. `marca_ms` es la hora en que termina la inferencia. */
void mv_detector(Mundo *m, MvPose p, uint32_t t_ms, uint32_t marca_ms, DeteccionesModelo *salida);
/* 1 si el chasis toca algún prisma bajo presente a la hora t_ms; deja en *objeto cuál. */
int  mv_contacto(const Mundo *m, MvPose p, uint32_t t_ms, int *objeto);
/* Distancia libre (mm) desde el frente del chasis hasta el primer prisma bajo en línea recta (ancho del chasis); 9999 si no hay. */
double mv_libre_adelante(const Mundo *m, MvPose p, uint32_t t_ms);

/* Azar determinista del mundo (el mismo generador para todo lo sensorial). */
double mv_uniforme(Mundo *m);                 /* [0, 1) */
double mv_normal(Mundo *m, double sigma);     /* aproximada, suma de uniformes */

#endif /* MUNDO_V2_H */
