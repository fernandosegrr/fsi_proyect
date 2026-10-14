/*
 * calibracion.h — Medidas físicas del carro que convierten distancia y giro
 * en tiempo (el carro navega por tiempo, sin encoders ni GPS), y las funciones
 * para calcularlas a partir de pruebas hechas con el carro real.
 *
 * GUÍA DE LECTURA
 *  1. Los dos #define de abajo (MS_POR_CM y MS_POR_GRADO) son los valores POR DEFECTO, de
 *     partida, que se usan mientras nadie calibre el carro.
 *  2. CalibracionCarro guarda lo MEDIDO con el carro: el retardo de arranque, los ms por cm, los
 *     cm que el carro sigue avanzando después de un STOP (frenado) y los ms por grado de cada
 *     lado (izquierda y derecha son distintos: el giro se hace con un solo motor).
 *  3. Cómo se mide (lo hace la app, ver docs/calibracion.md):
 *       - ARRANQUE: cuántos ms pasan desde que se ordena avanzar hasta que el carro se mueve.
 *       - AVANCE: se manda FORWARD durante varios tiempos distintos (por ejemplo 1000, 2000 y
 *         3000 ms) y se mide cuántos cm avanzó. La PENDIENTE de la recta (mínimos cuadrados)
 *         da los ms por cm sin que el arranque ni el frenado la ensucien; lo que sobra es el frenado.
 *       - GIRO: igual, con LEFT y con RIGHT por separado, midiendo grados.
 *  4. Funciones puras (sin estado ni malloc): promedio, mediana, promedio SIN valores atípicos,
 *     ajuste de las dos pruebas, conversión cm → ms y grados → ms (y al revés), y texto.
 *  Solo enteros. Los valores con decimales van multiplicados por 10 (sufijo _x10).
 *
 * MEDIR con el carro real, en el piso donde va a andar y con la batería cargada: las medidas
 * cambian con el peso, la superficie y el voltaje.
 */
#ifndef CALIBRACION_H
#define CALIBRACION_H

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

/* Valores POR DEFECTO (de ejemplo): milisegundos de motores a velocidad máxima para avanzar 1 cm
 * en línea recta, y milisegundos de pivote para girar 1 grado. */
#define MS_POR_CM     50
#define MS_POR_GRADO  10

/* ── Tipos ─────────────────────────────────────────────────────────────── */

/* Qué partes de la calibración están medidas de verdad (suma de bits en `medidos`). */
#define CAL_MEDIDO_ARRANQUE   1
#define CAL_MEDIDO_AVANCE     2
#define CAL_MEDIDO_GIRO_IZQ   4
#define CAL_MEDIDO_GIRO_DER   8
#define CAL_MEDIDO_TODO      15   /* los cuatro bits obligatorios: arranque, avance y los dos giros */
#define CAL_MEDIDO_FOV       16   /* opcional: el campo de visión de la cámara (giro visual); no cuenta para "todo" */

/* Lado de un giro. */
#define CAL_LADO_IZQUIERDA  -1
#define CAL_LADO_DERECHA     1

typedef struct {
    int medidos;                    /* bits CAL_MEDIDO_*: qué se midió (0 = todo es por defecto) */
    int arranque_ms;                /* ms entre ordenar el movimiento y que el carro se mueva */
    int ms_por_cm_x10;              /* ms por cada cm de avance, por 10 (500 = 50.0 ms/cm) */
    int frenado_mm;                 /* mm que el carro sigue avanzando después de un STOP */
    int ms_por_grado_izq_x10;       /* ms por cada grado girando a la izquierda, por 10 */
    int ms_por_grado_der_x10;       /* ms por cada grado girando a la derecha, por 10 */
    int frenado_grados_izq_x10;     /* grados (por 10) que sigue girando después del STOP, a la izquierda */
    int frenado_grados_der_x10;     /* idem a la derecha */
    int fov_grados_x10;             /* campo de visión HORIZONTAL de la cámara, en grados por 10 (600 = 60.0), para medir el giro con la cámara */
} CalibracionCarro;

/* Códigos de retorno. */
#define CAL_OK                0
#define CAL_ERR_ARGUMENTO    -1   /* puntero nulo o lista vacía */
#define CAL_ERR_DATOS        -2   /* las pruebas no tienen sentido (el carro "avanzó menos con más tiempo", etc.) */
#define CAL_ERR_RANGO        -3   /* el resultado sale de lo razonable (ver cal_es_valida) */
#define CAL_ERR_TEXTO        -4   /* el texto exportado no se puede leer */

/* Máximo de muestras que aceptan las funciones de estadística, y de puntos de un ajuste. */
#define CAL_MAX_MUESTRAS     16
#define CAL_MAX_PUNTOS        8

/* Un dato se considera ATÍPICO si se aleja de la mediana más de este por ciento de la mediana. */
#define CAL_ATIPICO_PCT      30

/* Tamaño sobrado del texto exportado (cal_a_texto). */
#define CAL_LARGO_TEXTO     400

/* ── Estadística ───────────────────────────────────────────────────────── */

/* Promedio redondeado de n valores (0 si n < 1 o hay demasiados). */
int cal_promedio(const int *valores, int n);

/* Mediana de n valores (en el caso par, el promedio de los dos del centro). */
int cal_mediana(const int *valores, int n);

/* Promedio de los valores que NO son atípicos (ver CAL_ATIPICO_PCT). Con *usados (si no es NULL)
 * dice cuántos se usaron. Con 1 o 2 valores no se rechaza ninguno. */
int cal_promedio_sin_atipicos(const int *valores, int n, int *usados);

/* ── Valores por defecto y validación ──────────────────────────────────── */

/* Calibración "sin medir": arranque 0, frenado 0, MS_POR_CM y MS_POR_GRADO. */
void cal_defecto(CalibracionCarro *cal);

/* 1 si todos los valores están en rangos razonables (arranque 0 a 2000 ms, 2 a 500 ms por cm,
 * 1 a 200 ms por grado, frenado hasta 500 mm y 180 grados, campo de visión de 20 a 150 grados). */
int cal_es_valida(const CalibracionCarro *cal);

/* Bits CAL_MEDIDO_* que FALTAN (0 = todo calibrado). */
int cal_faltantes(const CalibracionCarro *cal);

/* ── Ajuste con los datos de las pruebas ───────────────────────────────── */

/* Prueba de AVANCE: se mandó FORWARD durante los tiempos ms[0..n-1] (al menos 2 distintos, todos
 * mayores que el arranque) y el carro avanzó mm[0..n-1] (en milímetros, ya promediados). Ajusta una
 * recta por mínimos cuadrados: calcula ms_por_cm_x10 (la pendiente) y frenado_mm (usa
 * cal->arranque_ms, el retardo ya medido; con 0 el frenado absorbe el arranque). Marca el bit
 * AVANCE. Devuelve CAL_OK, CAL_ERR_ARGUMENTO, CAL_ERR_DATOS (los datos no tienen sentido: el carro
 * "avanzó menos con más tiempo", o todos los tiempos son iguales) o CAL_ERR_RANGO. Si hay error no
 * cambia nada. n va de 2 a CAL_MAX_PUNTOS. */
int cal_ajustar_avance(CalibracionCarro *cal, int n, const int *ms, const int *mm);

/* Prueba de GIRO de un lado (CAL_LADO_IZQUIERDA o CAL_LADO_DERECHA): igual, con los grados
 * girados por 10 (grados_x10). */
int cal_ajustar_giro(CalibracionCarro *cal, int lado, int n, const int *ms, const int *grados_x10);

/* Calibra el campo de visión de la cámara con UN giro medido: el carro giró `real_x10` grados de verdad (por 10, medidos en el piso)
 * y la cámara, con el campo de visión actual, midió `visual_x10`. El giro visual es proporcional al campo de visión, así que el nuevo
 * valor es el actual por real / visual. Marca el bit FOV. CAL_ERR_DATOS si algún valor no es positivo; CAL_ERR_RANGO si el resultado
 * no está entre 20 y 150 grados. Si hay error no cambia nada. */
int cal_ajustar_fov(CalibracionCarro *cal, int real_x10, int visual_x10);

/* Guarda el retardo de arranque medido (marca el bit ARRANQUE). CAL_ERR_RANGO si no es razonable. */
int cal_poner_arranque(CalibracionCarro *cal, int arranque_ms);

/* ── Conversiones que usa el navegador ─────────────────────────────────── */

/* Cuántos ms de motor ordenar para que el carro avance `cm` cm: arranque + el avance necesario
 * descontando lo que avanza solo al frenar. 0 si el frenado ya cubre la distancia. */
int cal_cm_a_ms(const CalibracionCarro *cal, int cm);

/* Igual que cal_cm_a_ms, pero se puede quitar el arranque (`arranca` = 0: el carro YA va avanzando, no hay
 * arranque que esperar) y el frenado (`frena` = 0: el carro NO se va a detener al final del tramo, sigue
 * avanzando en el tramo siguiente). Lo usa el navegador para encadenar tramos rectos sin parar. */
int cal_cm_a_ms_tramo(const CalibracionCarro *cal, int cm, int arranca, int frena);

/* Lo mismo para girar `grados` (positivo = derecha, negativo = izquierda): se usa el lado. */
int cal_grados_a_ms(const CalibracionCarro *cal, int grados);

/* Al revés: cuántos mm avanzó el carro tras `ms` de FORWARD (sin contar el frenado, que se suma al soltar). */
int cal_ms_a_mm(const CalibracionCarro *cal, int ms);

/* Cuántos grados giró tras `ms` girando hacia `lado` (sin contar el frenado). Siempre >= 0. */
int cal_ms_a_grados(const CalibracionCarro *cal, int lado, int ms);

/* Lo que sigue girando después del STOP, en grados enteros, para ese lado. */
int cal_frenado_grados(const CalibracionCarro *cal, int lado);

/* ── Texto (para exportar a un archivo o copiar y pegar) ───────────────── */

/* Escribe la calibración como texto "clave=valor", una por línea. Devuelve el largo escrito o
 * CAL_ERR_ARGUMENTO si no cabe en `capacidad`. */
int cal_a_texto(const CalibracionCarro *cal, char *texto, int capacidad);

/* Lee el texto de cal_a_texto. Ignora líneas vacías y comentarios (#). Si falta una clave obligatoria o algo
 * no es válido devuelve CAL_ERR_TEXTO y deja `cal` sin cambios. La clave del campo de visión es OPCIONAL (los
 * textos guardados antes de que existiera se siguen leyendo, con el campo de visión por defecto). */
int cal_de_texto(const char *texto, CalibracionCarro *cal);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRACION_H */
