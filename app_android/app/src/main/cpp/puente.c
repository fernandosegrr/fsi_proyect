/*
 * puente.c — Puente JNI entre Kotlin y el módulo de trayectoria (nucleo_c).
 *
 * GUÍA DE LECTURA
 *  1. Kotlin llama a TrayectoriaNativa.calcular(IntArray) (ver
 *     TrayectoriaNativa.kt); el nombre Java_com_fsi_carro_..._calcular es la
 *     regla de JNI: paquete + clase + función.
 *  2. La entrada es un arreglo plano [columna0, fila0, columna1, fila1, ...].
 *  3. Se llama a trayectoria_calcular() (C puro, en nucleo_c).
 *  4. La salida es un TEXTO listo para mostrar: la lista de pasos o un mensaje
 *     de error claro. Kotlin no interpreta nada, solo lo muestra.
 *  5. Más abajo, ObstaculosNativa.analizar() llama al detector de obstáculos por color de
 *     piso (obstaculos.c): recibe una imagen RGB (cualquier tamaño de 30x30 a 640x480) y devuelve 10 enteros.
 *  6. Antes del navegador está la calibración del carro (calibracion.c): CalibracionNativa.* y la
 *     calibración que usa el navegador. Al final, NavegadorNativa.* maneja el navegador del modo semiautónomo (navegador.c): hay
 *     UNA sola instancia (variable estática), que usa solo el hilo principal de la app.
 *  Todo con arreglos de tamaño fijo: no hay malloc.
 */

#include <jni.h>
#include <stdarg.h>
#include <stdio.h>
#include "trayectoria.h"
#include "obstaculos.h"
#include "navegador.h"
#include "calibracion.h"

#define TAMANO_TEXTO 8192   /* de sobra para MAX_PASOS líneas de ~40 caracteres */

/* ── Texto de salida ───────────────────────────────────────────────────── */

/* Buffer de texto con su largo actual (tamaño fijo, sin malloc). */
typedef struct {
    char texto[TAMANO_TEXTO];
    int  largo;
} Salida;

/* Como printf, pero escribe al final del buffer y nunca se pasa del tamaño. */
static void agregar(Salida *salida, const char *formato, ...)
{
    va_list argumentos;
    int escrito;

    if (salida->largo >= TAMANO_TEXTO - 1) {
        return;
    }
    va_start(argumentos, formato);
    escrito = vsnprintf(salida->texto + salida->largo,
                        (size_t)(TAMANO_TEXTO - salida->largo), formato, argumentos);
    va_end(argumentos);

    if (escrito > 0) {
        salida->largo += escrito;
        if (salida->largo > TAMANO_TEXTO - 1) {
            salida->largo = TAMANO_TEXTO - 1;   /* se truncó: el texto sigue terminado en '\0' */
        }
    }
}

/* Mensaje de error claro en español para cada código de trayectoria.h.
 * `indice` empieza en 0; al usuario se le habla del punto "número 1, 2, 3...". */
static void escribir_error(Salida *salida, int codigo, int indice)
{
    switch (codigo) {
    case TRAY_ERR_ARGUMENTO:
        agregar(salida, "Primero dibuja una ruta con el dedo sobre la cuadrícula.");
        break;
    case TRAY_ERR_SALTO:
        agregar(salida, "Error: el punto %d no es vecino del anterior (salto de más de una celda).", indice + 1);
        break;
    case TRAY_ERR_REPETIDO:
        agregar(salida, "Error: el punto %d está repetido (igual al anterior).", indice + 1);
        break;
    case TRAY_ERR_DEMASIADOS_PUNTOS:
        agregar(salida, "Error: la ruta tiene demasiados puntos (máximo %d).", MAX_PUNTOS);
        break;
    case TRAY_ERR_DEMASIADOS_PASOS:
        agregar(salida, "Error: la ruta genera demasiados pasos (máximo %d).", MAX_PASOS);
        break;
    default:
        agregar(salida, "Error desconocido (código %d).", codigo);
        break;
    }
}

/* Escribe la lista de pasos: una línea por paso y el tiempo total al final. */
static void escribir_pasos(Salida *salida, const Paso *pasos, int n_pasos)
{
    int total_ms = 0;
    int i;

    agregar(salida, "Ruta correcta: %d pasos\n", n_pasos);
    for (i = 0; i < n_pasos; i++) {
        if (pasos[i].tipo == PASO_AVANZAR) {
            agregar(salida, "%2d. AVANZAR  %4d cm       %6d ms\n",
                    i + 1, pasos[i].valor, pasos[i].tiempo_ms);
        } else {
            agregar(salida, "%2d. GIRAR    %+4d grados   %6d ms\n",
                    i + 1, pasos[i].valor, pasos[i].tiempo_ms);
        }
        total_ms += pasos[i].tiempo_ms;
    }
    agregar(salida, "Tiempo total: %d ms\n", total_ms);
}

/* ── Funciones que llama Kotlin ────────────────────────────────────────── */

/* Calcula la trayectoria y devuelve el texto con los pasos o el error. */
JNIEXPORT jstring JNICALL
Java_com_fsi_carro_TrayectoriaNativa_calcular(JNIEnv *env, jobject objeto, jintArray arreglo)
{
    Salida salida;
    jint   entrada[2 * MAX_PUNTOS];
    Punto  puntos[MAX_PUNTOS];
    Paso   pasos[MAX_PASOS];
    jsize  largo_entrada;
    int    n_puntos;
    int    n_pasos = 0;
    int    indice  = 0;
    int    codigo;
    int    i;

    (void)objeto;
    salida.texto[0] = '\0';
    salida.largo    = 0;

    largo_entrada = (arreglo == NULL) ? 0 : (*env)->GetArrayLength(env, arreglo);
    n_puntos = (int)(largo_entrada / 2);

    if (arreglo == NULL || largo_entrada % 2 != 0 || n_puntos < 1) {
        escribir_error(&salida, TRAY_ERR_ARGUMENTO, 0);
    } else if (n_puntos > MAX_PUNTOS) {
        escribir_error(&salida, TRAY_ERR_DEMASIADOS_PUNTOS, 0);   /* no cabe en el arreglo fijo */
    } else {
        (*env)->GetIntArrayRegion(env, arreglo, 0, largo_entrada, entrada);
        for (i = 0; i < n_puntos; i++) {
            puntos[i].columna = (int)entrada[2 * i];
            puntos[i].fila    = (int)entrada[2 * i + 1];
        }
        codigo = trayectoria_calcular(puntos, n_puntos, pasos, MAX_PASOS, &n_pasos, &indice);
        if (codigo == TRAY_OK) {
            escribir_pasos(&salida, pasos, n_pasos);
        } else {
            escribir_error(&salida, codigo, indice);
        }
    }
    return (*env)->NewStringUTF(env, salida.texto);
}

/* Máximo de puntos que acepta el módulo C (así Kotlin no repite el número). */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_TrayectoriaNativa_maxPuntos(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    return (jint)MAX_PUNTOS;
}

/* ── Detector de obstáculos por color de piso ──────────────────────────── */

#define ENTEROS_RESULTADO_OBSTACULOS 10
#define ENTEROS_CONFIG_OBSTACULOS    10
/* Encabezado del detalle (ver analizarConfig): 10 del resumen + tamaño de la rejilla, franja de referencia, aviso
 * de rejilla chica, estado del modelo del piso y los colores de los grupos del piso. Después viene la máscara. */
#define ENTEROS_ENCABEZADO_DETALLE   35
#define ENTEROS_DETALLE_MAXIMO       (ENTEROS_ENCABEZADO_DETALLE + OBS_MAX_FILAS * OBS_MAX_COLUMNAS)

/* El modelo del piso que se recuerda entre cuadros (lo usa solo el hilo de la cámara). `reiniciar_modelo` lo pide la
 * pantalla (otro hilo): se atiende al analizar el siguiente cuadro. */
static ModeloPiso modelo_global;
static int modelo_inicializado = 0;
static volatile int reiniciar_modelo = 0;

/* Revisa el arreglo de bytes y llama al detector con la configuración dada (NULL = por defecto). Con
 * `modelo` distinto de NULL conserva el modelo del piso entre cuadros (`avanzando` y `ahora_ms` como en
 * obstaculos_analizar_modelo); con NULL analiza sin memoria. Devuelve el código del detector. */
static int analizar_arreglo(JNIEnv *env, jbyteArray rgb, jint ancho, jint alto,
                            const ConfigObstaculos *config, ModeloPiso *modelo, int avanzando,
                            unsigned int ahora_ms, ResultadoObstaculos *resultado)
{
    ConfigObstaculos por_defecto;
    void *datos;
    int codigo;

    if (rgb == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    if (ancho <= 0 || alto <= 0 || (long)(*env)->GetArrayLength(env, rgb) < (long)ancho * (long)alto * 3L) {
        return OBS_ERR_TAMANO;   /* tamaño no positivo o arreglo más chico que la imagen: no se analiza */
    }
    if (config == NULL) {
        obstaculos_config_defecto(&por_defecto);
        config = &por_defecto;
    }
    /* Acceso directo al arreglo (sin copiarlo) mientras se analiza. */
    datos = (*env)->GetPrimitiveArrayCritical(env, rgb, NULL);
    if (datos == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    if (modelo != NULL) {
        codigo = obstaculos_analizar_modelo(config, modelo, (const unsigned char *)datos, ancho, alto, avanzando, ahora_ms, resultado);
    } else {
        codigo = obstaculos_analizar_con(config, (const unsigned char *)datos, ancho, alto, resultado);
    }
    (*env)->ReleasePrimitiveArrayCritical(env, rgb, datos, JNI_ABORT);   /* no se modificó */
    return codigo;
}

/* Copia el resultado a los primeros 10 enteros de `salida`. */
static void llenar_resumen(jint *salida, int codigo, const ResultadoObstaculos *resultado)
{
    int i;

    salida[0] = codigo;
    if (codigo == OBS_OK) {
        for (i = 0; i < OBS_ZONAS; i++) {
            salida[1 + 2 * i] = resultado->zona[i].hay_obstaculo;
            salida[2 + 2 * i] = resultado->zona[i].cercania;
        }
        salida[7] = resultado->ref_r;
        salida[8] = resultado->ref_g;
        salida[9] = resultado->ref_b;
    }
}

/* Analiza una imagen RGB de 320x240 (320*240*3 bytes) con la configuración por defecto.
 * Devuelve 10 enteros:
 *   [0] código (0 = bien, negativo = error, ver obstaculos.h)
 *   [1] hay obstáculo a la izquierda   [2] cercanía izquierda (0 a 3)
 *   [3] hay obstáculo al centro        [4] cercanía centro
 *   [5] hay obstáculo a la derecha     [6] cercanía derecha
 *   [7], [8], [9] color de referencia del piso (R, G, B) */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_ObstaculosNativa_analizar(JNIEnv *env, jobject objeto, jbyteArray rgb, jint ancho, jint alto)
{
    ResultadoObstaculos resultado;
    jint salida[ENTEROS_RESULTADO_OBSTACULOS];
    jintArray arreglo;
    int codigo, i;

    (void)objeto;
    for (i = 0; i < ENTEROS_RESULTADO_OBSTACULOS; i++) {
        salida[i] = 0;
    }
    codigo = analizar_arreglo(env, rgb, ancho, alto, NULL, NULL, 0, 0, &resultado);
    llenar_resumen(salida, codigo, &resultado);

    arreglo = (*env)->NewIntArray(env, ENTEROS_RESULTADO_OBSTACULOS);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_RESULTADO_OBSTACULOS, salida);
    }
    return arreglo;
}

/* Igual, pero con una configuración que se puede cambiar en vivo, CON MEMORIA del piso entre cuadros y con
 * el detalle de las celdas. `config` son 10 enteros: [umbralColor, minCeldas, refAnchoMil, refAltoMil,
 * refCentroXMil, refBordeInfMil, filaMinimaMil, gruposPiso, aprendizajeMil, usarBordes] (las proporciones van
 * en milésimas, ver obstaculos.h). `avanzando` dice si el carro está avanzando (solo entonces el modelo
 * aprende) y `ahora_ms` es un reloj en milisegundos (para el cambio de piso). Solo se debe llamar desde el
 * hilo de la cámara (el modelo es único). Devuelve ENTEROS_ENCABEZADO_DETALLE + filas * columnas enteros:
 *   [0..9]  el resumen de arriba
 *   [10] columnas de la cuadrícula   [11] filas   [12] fila mínima usada   [13] 1 si la rejilla es chica
 *   [14] columna de la franja de referencia   [15] fila   [16] ancho en celdas   [17] alto en celdas
 *   [18] grupos de color del piso   [19] 1 si la franja estaba tapada (se usó lo aprendido)
 *   [20] 1 si se reaprendió el piso en este cuadro (cambio de piso)   [21] 1 si el modelo aprendió en este cuadro
 *   [22] celdas marcadas solo por bordes   [23..34] colores R,G,B de hasta 4 grupos del piso
 *   [35...] la máscara de celdas "no piso" (1 = no piso), fila por fila (índice 35 + fila * columnas + columna).
 * Si hay error, el arreglo trae solo el encabezado (con el código en [0]). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_ObstaculosNativa_analizarConfig(JNIEnv *env, jobject objeto, jbyteArray rgb,
                                                    jint ancho, jint alto, jintArray config,
                                                    jboolean avanzando, jlong ahora_ms)
{
    ResultadoObstaculos resultado;
    ConfigObstaculos cfg;
    jint datos_config[ENTEROS_CONFIG_OBSTACULOS];
    jint salida[ENTEROS_DETALLE_MAXIMO];
    jintArray arreglo;
    int codigo, i, fila, columna, largo, g;

    (void)objeto;
    for (i = 0; i < ENTEROS_DETALLE_MAXIMO; i++) {
        salida[i] = 0;
    }
    if (config != NULL && (*env)->GetArrayLength(env, config) >= ENTEROS_CONFIG_OBSTACULOS) {
        (*env)->GetIntArrayRegion(env, config, 0, ENTEROS_CONFIG_OBSTACULOS, datos_config);
        cfg.umbral_color = (int)datos_config[0];
        cfg.min_celdas = (int)datos_config[1];
        cfg.ref_ancho_mil = (int)datos_config[2];
        cfg.ref_alto_mil = (int)datos_config[3];
        cfg.ref_centro_x_mil = (int)datos_config[4];
        cfg.ref_borde_inf_mil = (int)datos_config[5];
        cfg.fila_minima_mil = (int)datos_config[6];
        cfg.grupos_piso = (int)datos_config[7];
        cfg.aprendizaje_mil = (int)datos_config[8];
        cfg.usar_bordes = (int)datos_config[9];
    } else {
        /* Sin configuración: se pide al detector una configuración no válida. */
        cfg.umbral_color = -1;
        cfg.min_celdas = 0;
        cfg.ref_ancho_mil = 0;
        cfg.ref_alto_mil = 0;
        cfg.ref_centro_x_mil = -1;
        cfg.ref_borde_inf_mil = 0;
        cfg.fila_minima_mil = -1;
        cfg.grupos_piso = 0;
        cfg.aprendizaje_mil = -1;
        cfg.usar_bordes = 2;
    }
    if (!modelo_inicializado || reiniciar_modelo) {
        obstaculos_modelo_iniciar(&modelo_global);
        modelo_inicializado = 1;
        reiniciar_modelo = 0;
    }
    codigo = analizar_arreglo(env, rgb, ancho, alto, &cfg, &modelo_global, avanzando ? 1 : 0, (unsigned int)ahora_ms, &resultado);
    llenar_resumen(salida, codigo, &resultado);
    largo = ENTEROS_ENCABEZADO_DETALLE;
    if (codigo == OBS_OK) {
        salida[10] = resultado.columnas;
        salida[11] = resultado.filas;
        salida[12] = resultado.fila_minima_usada;
        salida[13] = resultado.rejilla_chica;
        salida[14] = resultado.ref_columna;
        salida[15] = resultado.ref_fila;
        salida[16] = resultado.ref_ncolumnas;
        salida[17] = resultado.ref_nfilas;
        salida[18] = resultado.grupos;
        salida[19] = resultado.franja_contaminada;
        salida[20] = resultado.piso_reaprendido;
        salida[21] = resultado.aprendio;
        salida[22] = resultado.celdas_borde;
        for (g = 0; g < resultado.grupos && g < OBS_MAX_GRUPOS; g++) {
            salida[23 + 3 * g] = resultado.grupo_color[g][0];
            salida[24 + 3 * g] = resultado.grupo_color[g][1];
            salida[25 + 3 * g] = resultado.grupo_color[g][2];
        }
        for (fila = 0; fila < resultado.filas; fila++) {
            for (columna = 0; columna < resultado.columnas; columna++) {
                salida[ENTEROS_ENCABEZADO_DETALLE + fila * resultado.columnas + columna] = resultado.no_piso[fila][columna];
            }
        }
        largo = ENTEROS_ENCABEZADO_DETALLE + resultado.filas * resultado.columnas;
    }

    arreglo = (*env)->NewIntArray(env, largo);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, largo, salida);
    }
    return arreglo;
}

/* Pide olvidar el piso aprendido: el siguiente cuadro vuelve a aprender los colores del piso de la franja. */
JNIEXPORT void JNICALL
Java_com_fsi_carro_ObstaculosNativa_reiniciarModelo(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    reiniciar_modelo = 1;
}

/* Configuración por defecto del detector (10 enteros, mismo orden que en analizarConfig). Así los
 * valores por defecto viven solo en el código en C (obstaculos.h). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_ObstaculosNativa_configDefecto(JNIEnv *env, jobject objeto)
{
    ConfigObstaculos cfg;
    jint salida[ENTEROS_CONFIG_OBSTACULOS];
    jintArray arreglo;

    (void)objeto;
    obstaculos_config_defecto(&cfg);
    salida[0] = cfg.umbral_color;
    salida[1] = cfg.min_celdas;
    salida[2] = cfg.ref_ancho_mil;
    salida[3] = cfg.ref_alto_mil;
    salida[4] = cfg.ref_centro_x_mil;
    salida[5] = cfg.ref_borde_inf_mil;
    salida[6] = cfg.fila_minima_mil;
    salida[7] = cfg.grupos_piso;
    salida[8] = cfg.aprendizaje_mil;
    salida[9] = cfg.usar_bordes;
    arreglo = (*env)->NewIntArray(env, ENTEROS_CONFIG_OBSTACULOS);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_CONFIG_OBSTACULOS, salida);
    }
    return arreglo;
}


/* ── Calibración del carro ─────────────────────────────────────────────── */

/* La calibración se pasa a Kotlin como 8 enteros, en este orden (el mismo de CalibracionCarro):
 *   [0] medidos  [1] arranque_ms  [2] ms_por_cm_x10  [3] frenado_mm
 *   [4] ms_por_grado_izq_x10  [5] ms_por_grado_der_x10
 *   [6] frenado_grados_izq_x10  [7] frenado_grados_der_x10
 * Las funciones que cambian la calibración devuelven 9 enteros: [0] = código (0 = bien) y [1..8] = la
 * calibración resultante (sin cambios si hubo error). */
#define ENTEROS_CALIBRACION 8

/* La calibración que usa el navegador: se aplica cada vez que se carga una ruta. */
static CalibracionCarro calibracion_global;
static int calibracion_inicializada = 0;

static CalibracionCarro *obtener_calibracion(void)
{
    if (!calibracion_inicializada) {
        cal_defecto(&calibracion_global);
        calibracion_inicializada = 1;
    }
    return &calibracion_global;
}

/* Lee 8 enteros de Kotlin. Devuelve 0 si el arreglo no sirve. */
static int leer_calibracion(JNIEnv *env, jintArray arreglo, CalibracionCarro *cal)
{
    jint datos[ENTEROS_CALIBRACION];

    if (arreglo == NULL || (*env)->GetArrayLength(env, arreglo) < ENTEROS_CALIBRACION) {
        return 0;
    }
    (*env)->GetIntArrayRegion(env, arreglo, 0, ENTEROS_CALIBRACION, datos);
    cal->medidos = (int)datos[0];
    cal->arranque_ms = (int)datos[1];
    cal->ms_por_cm_x10 = (int)datos[2];
    cal->frenado_mm = (int)datos[3];
    cal->ms_por_grado_izq_x10 = (int)datos[4];
    cal->ms_por_grado_der_x10 = (int)datos[5];
    cal->frenado_grados_izq_x10 = (int)datos[6];
    cal->frenado_grados_der_x10 = (int)datos[7];
    return 1;
}

/* Arma el arreglo de 9 enteros [código, calibración...]. */
static jintArray devolver_calibracion(JNIEnv *env, int codigo, const CalibracionCarro *cal)
{
    jint salida[1 + ENTEROS_CALIBRACION];
    jintArray arreglo;

    salida[0] = codigo;
    salida[1] = cal->medidos;
    salida[2] = cal->arranque_ms;
    salida[3] = cal->ms_por_cm_x10;
    salida[4] = cal->frenado_mm;
    salida[5] = cal->ms_por_grado_izq_x10;
    salida[6] = cal->ms_por_grado_der_x10;
    salida[7] = cal->frenado_grados_izq_x10;
    salida[8] = cal->frenado_grados_der_x10;
    arreglo = (*env)->NewIntArray(env, 1 + ENTEROS_CALIBRACION);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 1 + ENTEROS_CALIBRACION, salida);
    }
    return arreglo;
}

/* Calibración por defecto (código 0). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CalibracionNativa_defecto(JNIEnv *env, jobject objeto)
{
    CalibracionCarro cal;

    (void)objeto;
    cal_defecto(&cal);
    return devolver_calibracion(env, CAL_OK, &cal);
}

/* Promedio de los valores sin los atípicos (ver calibracion.h). Devuelve 0 si no hay valores. */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_CalibracionNativa_promedioSinAtipicos(JNIEnv *env, jobject objeto, jintArray valores)
{
    jint datos[CAL_MAX_MUESTRAS];
    int enteros[CAL_MAX_MUESTRAS];
    jsize n;
    int i;

    (void)objeto;
    if (valores == NULL) {
        return 0;
    }
    n = (*env)->GetArrayLength(env, valores);
    if (n < 1 || n > CAL_MAX_MUESTRAS) {
        return 0;
    }
    (*env)->GetIntArrayRegion(env, valores, 0, n, datos);
    for (i = 0; i < n; i++) {
        enteros[i] = (int)datos[i];
    }
    return (jint)cal_promedio_sin_atipicos(enteros, (int)n, NULL);
}

/* Guarda el retardo de arranque medido. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CalibracionNativa_ponerArranque(JNIEnv *env, jobject objeto, jintArray calibracion, jint arranque_ms)
{
    CalibracionCarro cal;
    int codigo;

    (void)objeto;
    if (!leer_calibracion(env, calibracion, &cal)) {
        cal_defecto(&cal);
        return devolver_calibracion(env, CAL_ERR_ARGUMENTO, &cal);
    }
    codigo = cal_poner_arranque(&cal, (int)arranque_ms);
    return devolver_calibracion(env, codigo, &cal);
}

/* Lee dos arreglos de enteros del mismo largo (de 2 a CAL_MAX_PUNTOS). Devuelve el largo o 0. */
static int leer_dos_listas(JNIEnv *env, jintArray a, jintArray b, int *lista_a, int *lista_b)
{
    jint datos[CAL_MAX_PUNTOS];
    jsize n;
    int i;

    if (a == NULL || b == NULL) {
        return 0;
    }
    n = (*env)->GetArrayLength(env, a);
    if (n < 2 || n > CAL_MAX_PUNTOS || (*env)->GetArrayLength(env, b) != n) {
        return 0;
    }
    (*env)->GetIntArrayRegion(env, a, 0, n, datos);
    for (i = 0; i < n; i++) {
        lista_a[i] = (int)datos[i];
    }
    (*env)->GetIntArrayRegion(env, b, 0, n, datos);
    for (i = 0; i < n; i++) {
        lista_b[i] = (int)datos[i];
    }
    return (int)n;
}

/* Ajusta el avance con los tiempos (ms) y lo que avanzó el carro (mm). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CalibracionNativa_ajustarAvance(JNIEnv *env, jobject objeto, jintArray calibracion,
                                                    jintArray ms, jintArray mm)
{
    CalibracionCarro cal;
    int lista_ms[CAL_MAX_PUNTOS], lista_mm[CAL_MAX_PUNTOS];
    int n, codigo;

    (void)objeto;
    if (!leer_calibracion(env, calibracion, &cal)) {
        cal_defecto(&cal);
        return devolver_calibracion(env, CAL_ERR_ARGUMENTO, &cal);
    }
    n = leer_dos_listas(env, ms, mm, lista_ms, lista_mm);
    codigo = (n == 0) ? CAL_ERR_ARGUMENTO : cal_ajustar_avance(&cal, n, lista_ms, lista_mm);
    return devolver_calibracion(env, codigo, &cal);
}

/* Ajusta el giro de un lado (-1 izquierda, 1 derecha) con los tiempos (ms) y los grados por 10. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CalibracionNativa_ajustarGiro(JNIEnv *env, jobject objeto, jintArray calibracion, jint lado,
                                                  jintArray ms, jintArray grados_x10)
{
    CalibracionCarro cal;
    int lista_ms[CAL_MAX_PUNTOS], lista_g[CAL_MAX_PUNTOS];
    int n, codigo;

    (void)objeto;
    if (!leer_calibracion(env, calibracion, &cal)) {
        cal_defecto(&cal);
        return devolver_calibracion(env, CAL_ERR_ARGUMENTO, &cal);
    }
    n = leer_dos_listas(env, ms, grados_x10, lista_ms, lista_g);
    codigo = (n == 0) ? CAL_ERR_ARGUMENTO : cal_ajustar_giro(&cal, (int)lado, n, lista_ms, lista_g);
    return devolver_calibracion(env, codigo, &cal);
}

/* Qué falta medir (bits CAL_MEDIDO_*; 0 = todo). */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_CalibracionNativa_faltantes(JNIEnv *env, jobject objeto, jintArray calibracion)
{
    CalibracionCarro cal;

    (void)objeto;
    if (!leer_calibracion(env, calibracion, &cal)) {
        return CAL_MEDIDO_TODO;
    }
    return (jint)cal_faltantes(&cal);
}

/* Cuántos ms de motor pide la calibración para avanzar `cm` o girar `grados` (negativo = izquierda). */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_CalibracionNativa_cmAMs(JNIEnv *env, jobject objeto, jintArray calibracion, jint cm)
{
    CalibracionCarro cal;

    (void)objeto;
    if (!leer_calibracion(env, calibracion, &cal)) {
        return 0;
    }
    return (jint)cal_cm_a_ms(&cal, (int)cm);
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_CalibracionNativa_gradosAMs(JNIEnv *env, jobject objeto, jintArray calibracion, jint grados)
{
    CalibracionCarro cal;

    (void)objeto;
    if (!leer_calibracion(env, calibracion, &cal)) {
        return 0;
    }
    return (jint)cal_grados_a_ms(&cal, (int)grados);
}

/* La calibración como texto "clave=valor" (vacío si no es válida). */
JNIEXPORT jstring JNICALL
Java_com_fsi_carro_CalibracionNativa_aTexto(JNIEnv *env, jobject objeto, jintArray calibracion)
{
    CalibracionCarro cal;
    char texto[CAL_LARGO_TEXTO];

    (void)objeto;
    if (!leer_calibracion(env, calibracion, &cal) || !cal_es_valida(&cal) || cal_a_texto(&cal, texto, sizeof texto) < 0) {
        texto[0] = '\0';
    }
    return (*env)->NewStringUTF(env, texto);
}

/* Lee el texto de aTexto: devuelve [código, calibración...]. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CalibracionNativa_deTexto(JNIEnv *env, jobject objeto, jstring texto)
{
    CalibracionCarro cal;
    const char *caracteres;
    int codigo;

    (void)objeto;
    cal_defecto(&cal);
    if (texto == NULL) {
        return devolver_calibracion(env, CAL_ERR_ARGUMENTO, &cal);
    }
    caracteres = (*env)->GetStringUTFChars(env, texto, NULL);
    if (caracteres == NULL) {
        return devolver_calibracion(env, CAL_ERR_ARGUMENTO, &cal);
    }
    codigo = cal_de_texto(caracteres, &cal);
    (*env)->ReleaseStringUTFChars(env, texto, caracteres);
    return devolver_calibracion(env, codigo, &cal);
}

/* Le dice al navegador qué calibración usar (se aplica en la próxima ruta que se cargue). Devuelve 0
 * o un código de error (la calibración no es válida). */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_NavegadorNativa_ponerCalibracion(JNIEnv *env, jobject objeto, jintArray calibracion)
{
    CalibracionCarro cal;

    (void)objeto;
    if (!leer_calibracion(env, calibracion, &cal) || !cal_es_valida(&cal)) {
        return CAL_ERR_ARGUMENTO;
    }
    *obtener_calibracion() = cal;
    return CAL_OK;
}

/* ── Navegador del modo semiautónomo ───────────────────────────────────── */

/* La única instancia del navegador (memoria estática, sin malloc). Solo se usa desde el
 * hilo principal de la app, así que no necesita protección. */
static Navegador navegador_global;
static int navegador_inicializado = 0;

#define ENTEROS_ESTADO_NAVEGADOR 9

static Navegador *obtener_navegador(void)
{
    if (!navegador_inicializado) {
        navegador_iniciar(&navegador_global);
        navegador_inicializado = 1;
    }
    return &navegador_global;
}

/* Carga la ruta [columna0, fila0, columna1, fila1, ...]. Devuelve NAV_OK (0) o un código de error. */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_NavegadorNativa_cargarRuta(JNIEnv *env, jobject objeto, jintArray arreglo)
{
    jint entrada[2 * MAX_PUNTOS];
    Punto puntos[MAX_PUNTOS];
    jsize largo;
    int n_puntos, i;

    (void)objeto;
    obtener_navegador();
    largo = (arreglo == NULL) ? 0 : (*env)->GetArrayLength(env, arreglo);
    n_puntos = (int)(largo / 2);
    if (arreglo == NULL || largo % 2 != 0 || n_puntos < 1 || n_puntos > MAX_PUNTOS) {
        return NAV_ERR_ARGUMENTO;
    }
    (*env)->GetIntArrayRegion(env, arreglo, 0, largo, entrada);
    for (i = 0; i < n_puntos; i++) {
        puntos[i].columna = (int)entrada[2 * i];
        puntos[i].fila = (int)entrada[2 * i + 1];
    }
    navegador_iniciar(&navegador_global);
    navegador_poner_calibracion(&navegador_global, obtener_calibracion());   /* la calibración medida del carro */
    return navegador_cargar_ruta(&navegador_global, puntos, n_puntos);
}

/* Empieza (o retoma) la ruta. `ahora_ms` es un reloj en milisegundos. */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_NavegadorNativa_comenzar(JNIEnv *env, jobject objeto, jlong ahora_ms)
{
    (void)env;
    (void)objeto;
    return navegador_comenzar(obtener_navegador(), (uint32_t)ahora_ms);
}

/* Cancela: el siguiente tick devuelve STOP y el estado es ESPERANDO. */
JNIEXPORT void JNICALL
Java_com_fsi_carro_NavegadorNativa_cancelar(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    navegador_cancelar(obtener_navegador());
}

/* Un tick. `obs` es el arreglo de 10 enteros de ObstaculosNativa.analizar (se usan los
 * índices 1 a 6: hay/cercanía de izquierda, centro y derecha); solo se mira si `hay_cuadro`
 * es verdadero. Devuelve el comando: 0 STOP, 1 FORWARD, 2 REVERSE, 3 LEFT, 4 RIGHT. */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_NavegadorNativa_tick(JNIEnv *env, jobject objeto, jlong ahora_ms,
                                         jboolean hay_cuadro, jintArray obs)
{
    ResultadoObstaculos resultado;
    jint datos[ENTEROS_RESULTADO_OBSTACULOS];
    const ResultadoObstaculos *cuadro = NULL;
    int z;

    (void)objeto;
    if (hay_cuadro && obs != NULL && (*env)->GetArrayLength(env, obs) >= ENTEROS_RESULTADO_OBSTACULOS) {
        (*env)->GetIntArrayRegion(env, obs, 0, ENTEROS_RESULTADO_OBSTACULOS, datos);
        for (z = 0; z < OBS_ZONAS; z++) {
            resultado.zona[z].hay_obstaculo = (int)datos[1 + 2 * z];
            resultado.zona[z].cercania = (int)datos[2 + 2 * z];
            resultado.zona[z].celdas = resultado.zona[z].hay_obstaculo ? MIN_CELDAS : 0;
        }
        cuadro = &resultado;
    }
    return (jint)navegador_tick(obtener_navegador(), (uint32_t)ahora_ms, cuadro);
}

/* Estado para mostrar en pantalla (9 enteros):
 *   [0] estado (0 ESPERANDO ... 6 ERROR, ver navegador.h)  [1] x en mm  [2] y en mm
 *   [3] rumbo en grados  [4] punto objetivo  [5] total de puntos  [6] esquivadas
 *   [7] motivo del error  [8] último comando (0 a 4) */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_NavegadorNativa_estado(JNIEnv *env, jobject objeto)
{
    const Navegador *nav = obtener_navegador();
    jint salida[ENTEROS_ESTADO_NAVEGADOR];
    jintArray arreglo;

    (void)objeto;
    salida[0] = (jint)nav->estado;
    salida[1] = nav->x_mm;
    salida[2] = nav->y_mm;
    salida[3] = nav->rumbo_grados;
    salida[4] = nav->indice_objetivo;
    salida[5] = nav->n_puntos;
    salida[6] = nav->esquivadas;
    salida[7] = (jint)nav->motivo_error;
    salida[8] = (jint)nav->cmd_actual;
    arreglo = (*env)->NewIntArray(env, ENTEROS_ESTADO_NAVEGADOR);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_ESTADO_NAVEGADOR, salida);
    }
    return arreglo;
}

/* Texto del comando (0 a 4) tal como lo entiende el firmware: "FORWARD", "STOP"... */
JNIEXPORT jstring JNICALL
Java_com_fsi_carro_NavegadorNativa_textoComando(JNIEnv *env, jobject objeto, jint comando)
{
    (void)objeto;
    return (*env)->NewStringUTF(env, navegador_texto_comando((ComandoNav)comando));
}

/* Texto del estado (0 a 6). */
JNIEXPORT jstring JNICALL
Java_com_fsi_carro_NavegadorNativa_textoEstado(JNIEnv *env, jobject objeto, jint estado)
{
    (void)objeto;
    return (*env)->NewStringUTF(env, navegador_texto_estado((EstadoNav)estado));
}
