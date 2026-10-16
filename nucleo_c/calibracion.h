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
#define CAL_MEDIDO_ATRAS     32   /* opcional: los ms por cm al RETROCEDER; no cuenta para "todo" (si falta se usa el del avance) */

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
    int ms_por_cm_atras_x10;        /* ms por cada cm RETROCEDIENDO, por 10. Solo vale si el bit CAL_MEDIDO_ATRAS está puesto; si no, se usa ms_por_cm_x10 */
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

/* Prueba de RETROCESO: igual que cal_ajustar_avance pero mandando REVERSE; guarda SOLO los ms por cm hacia atrás (ms_por_cm_atras_x10) y
 * marca el bit CAL_MEDIDO_ATRAS. El arranque y el frenado se toman de los del avance (no se miden aparte). Mismos códigos de error; si hay
 * error no cambia nada. Se mide igual que el avance: varios tiempos y los cm que recorrió hacia atrás. */
int cal_ajustar_retroceso(CalibracionCarro *cal, int n, const int *ms, const int *mm);

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

/* Ms por cm (por 10) que se usan al RETROCEDER: los medidos si el bit CAL_MEDIDO_ATRAS está puesto; si no, los del avance (el retroceso
 * no calibrado se trata como si fuera igual que avanzar; la app avisa). */
int cal_ms_por_cm_atras_x10(const CalibracionCarro *cal);

/* 1 si los ms por cm hacia atrás están medidos de verdad (bit CAL_MEDIDO_ATRAS). */
int cal_retroceso_medido(const CalibracionCarro *cal);

/* Como cal_cm_a_ms pero RETROCEDIENDO (con el arranque y el frenado del avance y los ms por cm hacia atrás). 0 si el frenado ya cubre la distancia. */
int cal_cm_a_ms_atras(const CalibracionCarro *cal, int cm);

/* Cuántos mm retrocedió el carro tras `ms` de REVERSE (sin contar el frenado). */
int cal_ms_a_mm_atras(const CalibracionCarro *cal, int ms);

/* Lo mismo para girar `grados` (positivo = derecha, negativo = izquierda): se usa el lado. */
int cal_grados_a_ms(const CalibracionCarro *cal, int grados);

/* Al revés: cuántos mm avanzó el carro tras `ms` de FORWARD (sin contar el frenado, que se suma al soltar). */
int cal_ms_a_mm(const CalibracionCarro *cal, int ms);

/* Cuántos grados giró tras `ms` girando hacia `lado` (sin contar el frenado). Siempre >= 0. */
int cal_ms_a_grados(const CalibracionCarro *cal, int lado, int ms);

/* Lo que sigue girando después del STOP, en grados enteros, para ese lado. */
int cal_frenado_grados(const CalibracionCarro *cal, int lado);

/* ── Ajuste fino: el giro de 90° y la recta ────────────────────────────────
 * Dos pruebas rápidas que se hacen con el carro ya calibrado (la recalibración completa tarda; estas tardan un minuto), con las pilas que se usarán en la demo:
 *
 *  GIRO DE 90°. El carro gira lo que la calibración dice que son 90°. El usuario dice qué pasó (se pasó mucho / un poco / justo / se quedó corto) o escribe los grados que
 *  giró de verdad. Si giró `reales` grados cuando se pidieron `pedidos`, los ms por grado eran `reales / pedidos` veces demasiado grandes: se corrigen con un FACTOR
 *  (en milésimas, 1000 = sin cambio) que se guarda por lado (izquierda y derecha) y por perfil (AUTO y MANUAL) y que multiplica los ms por grado medidos. Se puede repetir:
 *  el factor nuevo parte del anterior. Con 110° por 90° el factor sale ~818 (los giros duran 18 % menos).
 *
 *  RECTA. El carro avanza 2 s sobre una línea; el usuario dice hacia dónde se desvió y cuántos cm. Con la geometría de un arco (ángulo ~ 2 x desvío / recorrido; la diferencia
 *  entre las dos ruedas es ángulo x ancho de ejes / recorrido) sale cuánto más rápido va un lado, y el BALANCE (en milésimas, de 800 a 1000) es el factor con que el firmware
 *  reduce la potencia del lado izquierdo. Ver docs/perfiles_velocidad.md. */
#define CAL_FACTOR_GIRO_DEFECTO   1000   /* sin corrección */
#define CAL_FACTOR_GIRO_MIN        500
#define CAL_FACTOR_GIRO_MAX       1500

/* Qué dice el usuario del giro de 90° (cal_grados_de_opcion_giro). */
#define CAL_GIRO_SE_PASO_MUCHO     0   /* se pasó mucho: se supone que giró 120° */
#define CAL_GIRO_SE_PASO_UN_POCO   1   /* se pasó un poco: 105° */
#define CAL_GIRO_JUSTO             2   /* justo: 90° (no cambia nada) */
#define CAL_GIRO_QUEDO_CORTO       3   /* se quedó corto: 78° */

/* Los grados que se suponen girados en cada opción, con 90 pedidos. 0 si la opción no existe. */
int cal_grados_de_opcion_giro(int opcion);

/* Factor nuevo: `factor_mil` x pedidos / reales (redondeado y dentro de CAL_FACTOR_GIRO_MIN a MAX). Con `pedidos` o `reales` fuera de 10 a 360: CAL_ERR_DATOS y no cambia
 * nada. Devuelve CAL_OK, CAL_ERR_ARGUMENTO (puntero nulo o factor fuera de rango) o CAL_ERR_DATOS. */
int cal_factor_giro_nuevo(int factor_mil, int pedidos, int reales, int *nuevo_mil);

/* Copia `cal` en `salida` con los ms por grado multiplicados por el factor de cada lado (en milésimas). Es lo que usan el navegador y el simulador. Devuelve CAL_OK,
 * CAL_ERR_ARGUMENTO (puntero nulo o factor fuera de rango) o CAL_ERR_RANGO (el resultado no es una calibración válida; `salida` queda igual a `cal`). */
int cal_con_factor_giro(const CalibracionCarro *cal, int factor_izq_mil, int factor_der_mil, CalibracionCarro *salida);

#define CAL_BALANCE_MIN            800   /* el factor del lado izquierdo (en milésimas) va de 800 a 1000 */
#define CAL_BALANCE_MAX           1000
#define CAL_EJES_MM_DEFECTO        120   /* ancho entre las ruedas de un lado y las del otro (12 cm), se puede cambiar */
#define CAL_EJES_MM_MIN             50
#define CAL_EJES_MM_MAX            400
#define CAL_RECTA_OK_MM             10   /* con 1 cm de desvío o menos la recta ya está bien */
#define CAL_RECTA_MAX_ITERACIONES    4

/* Balance nuevo después de una prueba de recta. `desvio_mm` es lo que se desvió al final (positivo = a la derecha, negativo = a la izquierda) en `recorrido_mm` de avance, con
 * `ejes_mm` entre las ruedas. Un desvío a la derecha quiere decir que el lado izquierdo empuja más: se baja su factor. Un desvío a la izquierda lo sube (sin pasar de 1000). Devuelve
 * CAL_OK (y el nuevo en *nuevo_mil), CAL_ERR_ARGUMENTO (punteros nulos, balance fuera de 800 a 1000 o ejes fuera de rango) o CAL_ERR_DATOS (recorrido no positivo o desvío mayor que el
 * recorrido). Con `desvio_mm` en cero o hasta CAL_RECTA_OK_MM en valor absoluto no cambia nada. */
int cal_balance_nuevo(int balance_mil, int desvio_mm, int recorrido_mm, int ejes_mm, int *nuevo_mil);

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
