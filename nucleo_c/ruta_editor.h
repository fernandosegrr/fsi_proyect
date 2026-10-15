/*
 * ruta_editor.h — La lógica del EDITOR de rutas de la pantalla Rutas (sin pantalla: solo cuentas).
 *
 * GUÍA DE LECTURA
 *  1. Una ruta se arma con VÉRTICES: los puntos que el usuario toca en la cuadrícula (RE_COLUMNAS x RE_FILAS celdas de CELDA_CM = 20 cm). El navegador necesita
 *     una ruta de celdas VECINAS (cada punto pegado al anterior, en 8 direcciones); re_expandir() rellena los huecos entre dos vértices paso a paso
 *     (primero en diagonal y luego recto, igual que al arrastrar el dedo en el editor anterior).
 *  2. Acciones del editor: agregar un punto, mover uno, borrar uno, limpiar, DESHACER y REHACER (hasta RE_MAX_HISTORIAL pasos), y 4 PLANTILLAS
 *     (línea recta de 1 m, cuadrado de 40 cm, ocho y zigzag). Cada acción que no cabe devuelve un código negativo y NO cambia nada.
 *  3. RESUMEN: largo en cm, tiempo estimado con la calibración del carro (trayectoria.c), número de giros y ADVERTENCIAS (por ejemplo un giro muy cerrado).
 *  4. GUARDAR / ABRIR: re_a_texto() y re_de_texto() pasan la ruta a un texto de una línea ("RUTA1;12x8;1,6;1,4;...") que la app guarda con un nombre (re_nombre_valido).
 *  5. Solo enteros, sin malloc ni recursión; los errores son códigos de retorno.
 */
#ifndef RUTA_EDITOR_H
#define RUTA_EDITOR_H

#include "trayectoria.h"
#include "calibracion.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RE_COLUMNAS         12   /* tamaño de la cuadrícula (240 x 160 cm) */
#define RE_FILAS             8
#define RE_MAX_VERTICES     32   /* puntos que el usuario puede colocar */
#define RE_MAX_HISTORIAL    32   /* pasos que se pueden deshacer */
#define RE_MAX_NOMBRE       24   /* letras de un nombre de ruta */

/* Códigos de retorno (0 = bien). */
#define RE_OK                 0
#define RE_ERR_FUERA         -1   /* el punto cae fuera de la cuadrícula */
#define RE_ERR_LLENO         -2   /* ya hay RE_MAX_VERTICES puntos */
#define RE_ERR_REPETIDO      -3   /* el punto es igual al anterior (o a su vecino) */
#define RE_ERR_LARGA         -4   /* la ruta completa (con los pasos intermedios) pasaría de MAX_PUNTOS celdas */
#define RE_ERR_INDICE        -5   /* no existe ese número de punto */
#define RE_ERR_NADA          -6   /* no hay nada que deshacer / rehacer */
#define RE_ERR_TEXTO         -7   /* el texto guardado no se puede leer */
#define RE_ERR_ARGUMENTO     -8   /* puntero nulo, plantilla desconocida o texto sin capacidad */

/* Plantillas. */
#define RE_PLANTILLA_RECTA     0   /* línea recta de 5 celdas (1 m) */
#define RE_PLANTILLA_CUADRADO  1   /* cuadrado de 2 celdas por lado (40 cm) */
#define RE_PLANTILLA_OCHO      2   /* ocho hecho con dos rombos que se tocan en el centro */
#define RE_PLANTILLA_ZIGZAG    3   /* escalones de 1 celda */
#define RE_N_PLANTILLAS        4

/* Advertencias (bits). */
#define RE_ADV_POCOS_PUNTOS    1   /* menos de 2 puntos: no hay ruta */
#define RE_ADV_GIRO_CERRADO    2   /* algún giro de 135 grados o más (el carro casi da la vuelta) */

typedef struct {
    Punto v[RE_MAX_VERTICES];
    int   n;
} RutaVertices;

typedef struct {
    RutaVertices actual;
    RutaVertices deshacer[RE_MAX_HISTORIAL];
    int          n_deshacer;
    RutaVertices rehacer[RE_MAX_HISTORIAL];
    int          n_rehacer;
    int          arrastre;   /* número del punto que se está arrastrando (-1 = ninguno): un arrastre entero es UN solo paso de «deshacer» */
} EditorRuta;

typedef struct {
    int largo_cm;       /* largo de la ruta */
    int tiempo_ms;      /* tiempo estimado con la calibración (ms) */
    int giros;          /* cuántos giros hace */
    int n_vertices;
    int n_celdas;       /* celdas de la ruta ya con los pasos intermedios */
    int advertencias;   /* bits RE_ADV_* */
    int codigo;         /* 0 si se pudo calcular; si no, el código de trayectoria.h */
} ResumenRuta;

void re_iniciar(EditorRuta *e);

int  re_agregar(EditorRuta *e, int columna, int fila);
int  re_mover(EditorRuta *e, int indice, int columna, int fila);
/* Avisa que terminó el arrastre del punto (el siguiente mover vuelve a guardar un paso de «deshacer»). */
void re_fin_arrastre(EditorRuta *e);
int  re_borrar_punto(EditorRuta *e, int indice);
void re_limpiar(EditorRuta *e);

int  re_deshacer(EditorRuta *e);
int  re_rehacer(EditorRuta *e);
int  re_puede_deshacer(const EditorRuta *e);
int  re_puede_rehacer(const EditorRuta *e);

/* Reemplaza la ruta por una plantilla, puesta abajo y al centro de la cuadrícula (se puede deshacer). */
int  re_plantilla(EditorRuta *e, int plantilla);

/* Rellena los huecos entre vértices: la ruta de celdas vecinas que entiende el navegador. Devuelve cuántas celdas escribió (0 si no hay ruta) o RE_ERR_LARGA. */
int  re_expandir(const RutaVertices *r, Punto *salida, int capacidad);

/* Largo, tiempo, giros y advertencias. `cal` = NULL usa la calibración por defecto. */
ResumenRuta re_resumen(const EditorRuta *e, const CalibracionCarro *cal);

/* Guardar y abrir como texto. re_a_texto devuelve el largo escrito o un código negativo; re_de_texto reemplaza la ruta (y borra el historial) o deja todo igual si falla. */
int  re_a_texto(const EditorRuta *e, char *texto, int capacidad);
int  re_de_texto(const char *texto, EditorRuta *e);

/* 1 si el nombre sirve para guardar una ruta: de 1 a RE_MAX_NOMBRE letras, números, espacios, guion o guion bajo; sin espacios al principio ni al final. */
int  re_nombre_valido(const char *nombre);

/* Texto corto de un código de error, en español (para la pantalla). */
const char *re_texto_error(int codigo);

#ifdef __cplusplus
}
#endif

#endif /* RUTA_EDITOR_H */
