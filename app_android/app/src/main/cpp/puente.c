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
 *  5a. MovimientoVisualNativa.* mide cuánto giró el carro entre dos cuadros de la cámara (movimiento_visual.c).
 *  5b. FusionNativa.* une el resultado del detector con las cajas del modelo de objetos (fusion.c).
 *  6. Antes del navegador está la calibración del carro (calibracion.c): CalibracionNativa.* y la
 *     calibración que usa el navegador. Al final, NavegadorNativa.* maneja el navegador del modo semiautónomo (navegador.c): hay
 *     UNA sola instancia (variable estática), que usa solo el hilo principal de la app.
 *  Todo con arreglos de tamaño fijo: no hay malloc.
 */

#include <jni.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "trayectoria.h"
#include "obstaculos.h"
#include "navegador.h"
#include "calibracion.h"
#include "fusion.h"
#include "movimiento_visual.h"
#include "video_stats.h"
#include "prueba_obstaculo.h"
#include "corredor.h"
#include "cualquiera.h"
#include "movimiento_piso.h"
#include "paquete_celdas.h"
#include "sesion_compartida.h"
#include "latencia_video.h"
#include "ruta_editor.h"
#include "sim_ruta.h"

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
#define ENTEROS_ENCABEZADO_DETALLE   42
#define ENTEROS_DETALLE_MAXIMO       (ENTEROS_ENCABEZADO_DETALLE + OBS_MAX_FILAS * OBS_MAX_COLUMNAS)

/* El modelo del piso que se recuerda entre cuadros (lo usa solo el hilo de la cámara). `reiniciar_modelo` lo pide la
 * pantalla (otro hilo): se atiende al analizar el siguiente cuadro. */
static ModeloPiso modelo_global;
static AcumuladorPiso acumulador_piso;          /* lo que se va acumulando mientras se aprende el piso (botón «Aprender piso») */
static volatile int aprendiendo_piso = 0;       /* 1 mientras cada cuadro analizado también se suma al acumulador */
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
    if (aprendiendo_piso) {   /* el cuadro también alimenta el aprendizaje del piso (el carro está quieto) */
        obstaculos_aprender_cuadro(&acumulador_piso, config, (const unsigned char *)datos, ancho, alto);
    }
    if (modelo != NULL) {
        codigo = obstaculos_analizar_modelo(config, modelo, (const unsigned char *)datos, ancho, alto, avanzando, ahora_ms, resultado);
    } else {
        codigo = obstaculos_analizar_con(config, (const unsigned char *)datos, ancho, alto, resultado);
    }
    (*env)->ReleasePrimitiveArrayCritical(env, rgb, datos, JNI_ABORT);   /* no se modificó */
    return codigo;
}

/* Lo mismo que analizar_arreglo pero con las celdas ya promediadas por la cámara del carro (columnas * filas * 3 bytes; modo de video CELDAS). Siempre con memoria del piso. */
static int analizar_arreglo_celdas(JNIEnv *env, jbyteArray celdas, jint columnas, jint filas, const ConfigObstaculos *config, ModeloPiso *modelo, int avanzando,
                                   unsigned int ahora_ms, ResultadoObstaculos *resultado)
{
    void *datos;
    int codigo;

    if (celdas == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    if (columnas <= 0 || filas <= 0 || (long)(*env)->GetArrayLength(env, celdas) < (long)columnas * (long)filas * 3L) {
        return OBS_ERR_TAMANO;
    }
    datos = (*env)->GetPrimitiveArrayCritical(env, celdas, NULL);
    if (datos == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    codigo = obstaculos_analizar_modelo_celdas(config, modelo, (const unsigned char *)datos, columnas, filas, avanzando, ahora_ms, resultado);
    (*env)->ReleasePrimitiveArrayCritical(env, celdas, datos, JNI_ABORT);
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
 *   [35..37] celdas confirmadas de cada zona (izquierda, centro, derecha)
 *   [38] brillo medio de la zona de avance (0 a 255)  [39] ganancia con que se reajustó el brillo (1000 = ninguna)
 *   [40] celdas marcadas en la zona de avance  [41] motivo (bits OBS_MOTIVO_*)
 *   [42...] la máscara de celdas "no piso" (1 = no piso), fila por fila (índice 42 + fila * columnas + columna).
 * Si hay error, el arreglo trae solo el encabezado (con el código en [0]). */
static jintArray analizar_config_comun(JNIEnv *env, jbyteArray rgb, jbyteArray celdas, jint columnas, jint filas,
                                       jint ancho, jint alto, jintArray config,
                                       jboolean avanzando, jlong ahora_ms)
{
    ResultadoObstaculos resultado;
    ConfigObstaculos cfg;
    jint datos_config[ENTEROS_CONFIG_OBSTACULOS];
    jint salida[ENTEROS_DETALLE_MAXIMO];
    jintArray arreglo;
    int codigo, i, fila, columna, largo, g;

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
        cfg.mejoras_piso = 1;   /* los dos últimos enteros de la configuración son opcionales: [10] mejoras de piso (por defecto 1) y [11] piso fijo (por defecto 0) */
        cfg.piso_fijo = 0;
        if ((*env)->GetArrayLength(env, config) >= 12) {
            jint extra[2];

            (*env)->GetIntArrayRegion(env, config, 10, 2, extra);
            cfg.mejoras_piso = (int)extra[0];
            cfg.piso_fijo = (int)extra[1];
        }
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
        cfg.mejoras_piso = 1;
        cfg.piso_fijo = 0;
    }
    if (!modelo_inicializado || reiniciar_modelo) {
        obstaculos_modelo_iniciar(&modelo_global);
        modelo_inicializado = 1;
        reiniciar_modelo = 0;
    }
    if (celdas != NULL) {
        codigo = analizar_arreglo_celdas(env, celdas, columnas, filas, &cfg, &modelo_global, avanzando ? 1 : 0, (unsigned int)ahora_ms, &resultado);
    } else {
        codigo = analizar_arreglo(env, rgb, ancho, alto, &cfg, &modelo_global, avanzando ? 1 : 0, (unsigned int)ahora_ms, &resultado);
    }
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
        salida[38] = resultado.brillo_medio;
        salida[39] = resultado.ganancia_mil;
        salida[40] = resultado.celdas_marcadas;
        salida[41] = resultado.motivo;
        salida[35] = resultado.zona[0].celdas;   /* celdas confirmadas de cada zona (la fusión las usa para saber si el detector duda) */
        salida[36] = resultado.zona[1].celdas;
        salida[37] = resultado.zona[2].celdas;
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

JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_ObstaculosNativa_analizarConfig(JNIEnv *env, jobject objeto, jbyteArray rgb,
                                                    jint ancho, jint alto, jintArray config,
                                                    jboolean avanzando, jlong ahora_ms)
{
    (void)objeto;
    return analizar_config_comun(env, rgb, NULL, 0, 0, ancho, alto, config, avanzando, ahora_ms);
}

/* Lo mismo que analizarConfig pero con las CELDAS ya promediadas por la cámara del carro (modo de video CELDAS): `celdas` trae columnas * filas * 3 bytes (R, G, B de cada celda,
 * fila por fila). Devuelve el mismo arreglo de enteros. El resultado es exactamente el que daría la imagen completa (se prueba en nucleo_c/pruebas_celdas.c). «Aprender piso» no
 * funciona con este modo (necesita la imagen). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_ObstaculosNativa_analizarCeldasConfig(JNIEnv *env, jobject objeto, jbyteArray celdas, jint columnas, jint filas, jintArray config,
                                                          jboolean avanzando, jlong ahora_ms)
{
    (void)objeto;
    return analizar_config_comun(env, NULL, celdas, columnas, filas, 0, 0, config, avanzando, ahora_ms);
}

/* El paquete UDP de celdas (paquete_celdas.c): lee `largo` bytes de `paquete` y copia las celdas a `celdas`. Devuelve 7 enteros:
 * [0] código (0 = bien)  [1] número de cuadro (entero con signo: puede salir negativo pasado 2 mil millones)  [2] ms de la cámara entre captura y paquete
 * [3] ancho de la imagen  [4] alto  [5] columnas  [6] filas. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_PaqueteCeldasNativa_leer(JNIEnv *env, jobject objeto, jbyteArray paquete, jint largo, jbyteArray celdas)
{
    jint salida[7] = { 0, 0, 0, 0, 0, 0, 0 };
    jintArray arreglo;
    DatosPaquete datos;
    static unsigned char buf[PAQ_MAX_TOTAL];
    static unsigned char salida_celdas[PAQ_MAX_COLUMNAS * PAQ_MAX_FILAS * 3];
    int codigo;

    (void)objeto;
    if (paquete == NULL || celdas == NULL || largo < 0 || largo > PAQ_MAX_TOTAL || (*env)->GetArrayLength(env, paquete) < largo) {
        salida[0] = PAQ_ERR_ARGUMENTO;
    } else {
        (*env)->GetByteArrayRegion(env, paquete, 0, largo, (jbyte *)buf);
        codigo = paquete_leer(buf, (int)largo, &datos, salida_celdas, (int)sizeof salida_celdas);
        salida[0] = codigo;
        if (codigo == PAQ_OK) {
            if ((*env)->GetArrayLength(env, celdas) < datos.columnas * datos.filas * 3) {
                salida[0] = PAQ_ERR_ARGUMENTO;
            } else {
                (*env)->SetByteArrayRegion(env, celdas, 0, datos.columnas * datos.filas * 3, (const jbyte *)salida_celdas);
                salida[1] = (jint)datos.numero;
                salida[2] = (jint)datos.ms_captura;
                salida[3] = datos.ancho;
                salida[4] = datos.alto;
                salida[5] = datos.columnas;
                salida[6] = datos.filas;
            }
        }
    }
    arreglo = (*env)->NewIntArray(env, 7);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 7, salida);
    }
    return arreglo;
}

/* Empieza a aprender el piso: desde el siguiente cuadro, cada cuadro analizado se suma al acumulador. Solo desde el hilo de la cámara. */
JNIEXPORT void JNICALL
Java_com_fsi_carro_ObstaculosNativa_aprenderPisoIniciar(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    obstaculos_aprender_iniciar(&acumulador_piso);
    aprendiendo_piso = 1;
}

/* Termina de aprender el piso con lo acumulado: agrupa los colores de toda la zona de avance, deja ese modelo del piso y avisa qué aprendió.
 * `config` son los mismos 10 o 12 enteros de analizarConfig. Solo desde el hilo de la cámara. Devuelve 23 enteros:
 *   [0] código (0 = bien; OBS_ERR_REFERENCIA = muy pocos cuadros)  [1] colores aprendidos  [2] brillo medio (0 a 255)
 *   [3] 1 si el piso es muy oscuro  [4] 1 si es muy variado  [5] % descartado por poco apoyo  [6] cuadros usados
 *   [7..18] colores R,G,B de hasta 4 grupos  [19..22] % de celdas que respalda cada color. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_ObstaculosNativa_aprenderPisoTerminar(JNIEnv *env, jobject objeto, jintArray config)
{
    jint salida[23];
    ConfigObstaculos cfg;
    InformePiso informe;
    jint datos_config[ENTEROS_CONFIG_OBSTACULOS];
    jintArray arreglo;
    int codigo, i, g;

    (void)objeto;
    for (i = 0; i < 23; i++) {
        salida[i] = 0;
    }
    aprendiendo_piso = 0;
    obstaculos_config_defecto(&cfg);
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
    }
    if (!modelo_inicializado) {
        obstaculos_modelo_iniciar(&modelo_global);
        modelo_inicializado = 1;
    }
    memset(&informe, 0, sizeof informe);
    codigo = obstaculos_aprender_terminar(&acumulador_piso, &cfg, &modelo_global, &informe);
    salida[0] = codigo;
    if (codigo == OBS_OK) {
        salida[1] = informe.grupos;
        salida[2] = informe.brillo_medio;
        salida[3] = informe.muy_oscuro;
        salida[4] = informe.muy_variado;
        salida[5] = informe.descartado_pct;
        salida[6] = informe.cuadros;
        for (g = 0; g < OBS_MAX_GRUPOS; g++) {
            salida[7 + 3 * g] = informe.color[g][0];
            salida[8 + 3 * g] = informe.color[g][1];
            salida[9 + 3 * g] = informe.color[g][2];
            salida[19 + g] = informe.peso_pct[g];
        }
    }
    arreglo = (*env)->NewIntArray(env, 23);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 23, salida);
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


/* ── Giro medido con la cámara ─────────────────────────────────────────── */

#define ENTEROS_RESULTADO_MOVIMIENTO 9

/* El estimador (recuerda la huella del cuadro anterior). Solo lo usa el hilo de la cámara. `reiniciar_movimiento` lo pide
 * la pantalla (otro hilo): se atiende en el siguiente cuadro. */
static EstimadorMovimiento estimador_movimiento;
static int estimador_inicializado = 0;
static volatile int reiniciar_movimiento = 0;

/* Procesa un cuadro RGB (ancho x alto x 3 bytes), lo compara con el anterior y devuelve 9 enteros:
 *   [0] código (0 = bien, negativo = error de argumentos o tamaño)   [1] 1 si el corrimiento es confiable
 *   [2] motivo (0 = bien; 1 sin cuadro previo; 2 escena uniforme; 3 coincidencia débil; 4 ambiguo; 5 fuera de rango)
 *   [3] corrimiento del contenido en píxeles por 16   [4] confianza de 0 a 1000
 *   [5] giro del carro en grados por 10 (+ = a la derecha; 0 si no es confiable)   [6] energía de bordes de la huella
 *   [7] cuántos cuadros abarca el corrimiento (1; más si se rechazaron cuadros y se comparó con el último bueno)
 *   [8] 1 si la coincidencia era ambigua y se aceptó por coincidir con el movimiento anterior */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_MovimientoVisualNativa_procesar(JNIEnv *env, jobject objeto, jbyteArray rgb, jint ancho, jint alto, jint fov_x10)
{
    MovimientoVisual resultado;
    jint salida[ENTEROS_RESULTADO_MOVIMIENTO];
    jintArray arreglo;
    void *datos;
    int codigo, i;

    (void)objeto;
    for (i = 0; i < ENTEROS_RESULTADO_MOVIMIENTO; i++) {
        salida[i] = 0;
    }
    if (!estimador_inicializado || reiniciar_movimiento) {
        mv_iniciar(&estimador_movimiento);
        estimador_inicializado = 1;
        reiniciar_movimiento = 0;
    }
    if (rgb == NULL || ancho <= 0 || alto <= 0 || (long)(*env)->GetArrayLength(env, rgb) < (long)ancho * (long)alto * 3L) {
        codigo = MV_ERR_TAMANO;
    } else {
        datos = (*env)->GetPrimitiveArrayCritical(env, rgb, NULL);
        if (datos == NULL) {
            codigo = MV_ERR_ARGUMENTO;
        } else {
            codigo = mv_procesar(&estimador_movimiento, (const unsigned char *)datos, ancho, alto, fov_x10, &resultado);
            (*env)->ReleasePrimitiveArrayCritical(env, rgb, datos, JNI_ABORT);   /* no se modificó */
        }
    }
    salida[0] = codigo;
    if (codigo == MV_OK) {
        salida[1] = resultado.valido;
        salida[2] = resultado.motivo;
        salida[3] = resultado.corrimiento_x16;
        salida[4] = resultado.confianza_mil;
        salida[5] = resultado.giro_grados_x10;
        salida[6] = resultado.energia;
        salida[7] = resultado.cuadros;
        salida[8] = resultado.por_prediccion;
    }
    arreglo = (*env)->NewIntArray(env, ENTEROS_RESULTADO_MOVIMIENTO);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_RESULTADO_MOVIMIENTO, salida);
    }
    return arreglo;
}

/* Olvida el cuadro anterior: el siguiente cuadro no se compara con nada (por ejemplo, al abrir la cámara o cambiar el tamaño). */
JNIEXPORT void JNICALL
Java_com_fsi_carro_MovimientoVisualNativa_reiniciar(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    reiniciar_movimiento = 1;
}

/* ── Fusión del detector de color con el modelo de objetos ─────────────── */

#define ENTEROS_CONFIG_FUSION     10
#define ENTEROS_RESULTADO_FUSION  22
#define ENTEROS_POR_CAJA           6

/* Configuración por defecto de la fusión (10 enteros):
 *   [0] vigencia_ms  [1] confianza_min_mil  [2] confianza_frenar_mil  [3] solape_camino_mil  [4] cerca_mil
 *   [5] media_mil  [6] area_grande_mil  [7..9] las 3 palabras de bits de las clases de interés. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_FusionNativa_configDefecto(JNIEnv *env, jobject objeto)
{
    ConfigFusion cfg;
    jint salida[ENTEROS_CONFIG_FUSION];
    jintArray arreglo;

    (void)objeto;
    fusion_config_defecto(&cfg);
    salida[0] = cfg.vigencia_ms;
    salida[1] = cfg.confianza_min_mil;
    salida[2] = cfg.confianza_frenar_mil;
    salida[3] = cfg.solape_camino_mil;
    salida[4] = cfg.cerca_mil;
    salida[5] = cfg.media_mil;
    salida[6] = cfg.area_grande_mil;
    salida[7] = (jint)cfg.clases_interes[0];
    salida[8] = (jint)cfg.clases_interes[1];
    salida[9] = (jint)cfg.clases_interes[2];
    arreglo = (*env)->NewIntArray(env, ENTEROS_CONFIG_FUSION);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_CONFIG_FUSION, salida);
    }
    return arreglo;
}

/* Decide con lo que ve el detector (el arreglo de analizarConfig: se leen las zonas, [1] a [6], y sus celdas, [35] a
 * [37]), las cajas del modelo ([n, (clase, confianza_mil, xmin, ymin, xmax, ymax) * n], en milésimas del cuadro) y los relojes
 * `marca_ms` (cuándo terminó el modelo) y `ahora_ms`. Devuelve 20 enteros:
 *   [0] código (0 = bien)   [1..6] zonas fusionadas (hay, cercanía; igual que ObstaculosNativa.analizar)   [7..9] ceros
 *   [10] severidad (0 a 3)  [11] fuente (0 nadie, 1 detector, 2 modelo, 3 ambos)  [12] el detector solo ya decide
 *   [13] modelo vigente  [14] edad del modelo en ms  [15] hay aviso  [16] clase del aviso  [17] zona del aviso
 *   [18] cercanía del aviso  [19] clase del objeto en el camino (-1 si no hay)  [20] zonas desmentidas (solo en el modo «cualquier objeto») */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_FusionNativa_decidir(JNIEnv *env, jobject objeto, jintArray config, jintArray detector,
                                         jintArray cajas, jlong marca_ms, jlong ahora_ms)
{
    ConfigFusion cfg;
    ResultadoObstaculos det;
    DeteccionesModelo modelo;
    ResultadoFusion resultado;
    jint datos_config[ENTEROS_CONFIG_FUSION];
    jint datos_detector[ENTEROS_ENCABEZADO_DETALLE];
    jint datos_cajas[1 + FUS_MAX_CAJAS * ENTEROS_POR_CAJA];
    jint salida[ENTEROS_RESULTADO_FUSION];
    jintArray arreglo;
    const DeteccionesModelo *puntero_modelo = NULL;
    int codigo, i, z;

    (void)objeto;
    for (i = 0; i < ENTEROS_RESULTADO_FUSION; i++) {
        salida[i] = 0;
    }
    salida[19] = -1;
    memset(&det, 0, sizeof det);

    if (config == NULL || (*env)->GetArrayLength(env, config) < ENTEROS_CONFIG_FUSION ||
        detector == NULL || (*env)->GetArrayLength(env, detector) < ENTEROS_ENCABEZADO_DETALLE) {
        salida[0] = FUS_ERR_ARGUMENTO;
    } else {
        (*env)->GetIntArrayRegion(env, config, 0, ENTEROS_CONFIG_FUSION, datos_config);
        cfg.vigencia_ms = (int)datos_config[0];
        cfg.confianza_min_mil = (int)datos_config[1];
        cfg.confianza_frenar_mil = (int)datos_config[2];
        cfg.solape_camino_mil = (int)datos_config[3];
        cfg.cerca_mil = (int)datos_config[4];
        cfg.media_mil = (int)datos_config[5];
        cfg.area_grande_mil = (int)datos_config[6];
        cfg.clases_interes[0] = (unsigned int)datos_config[7];
        cfg.clases_interes[1] = (unsigned int)datos_config[8];
        cfg.clases_interes[2] = (unsigned int)datos_config[9];

        (*env)->GetIntArrayRegion(env, detector, 0, ENTEROS_ENCABEZADO_DETALLE, datos_detector);
        for (z = 0; z < OBS_ZONAS; z++) {
            det.zona[z].hay_obstaculo = (int)datos_detector[1 + 2 * z];
            det.zona[z].cercania = (int)datos_detector[2 + 2 * z];
            det.zona[z].celdas = (int)datos_detector[35 + z];
        }

        memset(&modelo, 0, sizeof modelo);
        modelo.marca_ms = (unsigned int)marca_ms;
        if (cajas != NULL && (*env)->GetArrayLength(env, cajas) >= 1) {
            jsize largo = (*env)->GetArrayLength(env, cajas);
            int n;

            if (largo > 1 + FUS_MAX_CAJAS * ENTEROS_POR_CAJA) {
                largo = 1 + FUS_MAX_CAJAS * ENTEROS_POR_CAJA;
            }
            (*env)->GetIntArrayRegion(env, cajas, 0, largo, datos_cajas);
            n = (int)datos_cajas[0];
            if (n > (int)((largo - 1) / ENTEROS_POR_CAJA)) {
                n = (int)((largo - 1) / ENTEROS_POR_CAJA);   /* no se lee más de lo que llegó */
            }
            if (n > FUS_MAX_CAJAS) {
                n = FUS_MAX_CAJAS;
            }
            for (i = 0; i < n; i++) {
                modelo.cajas[i].clase = (int)datos_cajas[1 + i * ENTEROS_POR_CAJA];
                modelo.cajas[i].confianza_mil = (int)datos_cajas[2 + i * ENTEROS_POR_CAJA];
                modelo.cajas[i].xmin_mil = (int)datos_cajas[3 + i * ENTEROS_POR_CAJA];
                modelo.cajas[i].ymin_mil = (int)datos_cajas[4 + i * ENTEROS_POR_CAJA];
                modelo.cajas[i].xmax_mil = (int)datos_cajas[5 + i * ENTEROS_POR_CAJA];
                modelo.cajas[i].ymax_mil = (int)datos_cajas[6 + i * ENTEROS_POR_CAJA];
            }
            modelo.n = (n > 0) ? n : 0;
            puntero_modelo = &modelo;
        }

        codigo = fusion_decidir(&cfg, &det, puntero_modelo, (unsigned int)ahora_ms, &resultado);
        salida[0] = codigo;
        if (codigo == FUS_OK) {
            for (z = 0; z < OBS_ZONAS; z++) {
                salida[1 + 2 * z] = resultado.zona[z].hay_obstaculo;
                salida[2 + 2 * z] = resultado.zona[z].cercania;
            }
            salida[7] = datos_detector[7];    /* el color del piso del detector, para que el formato sea el mismo */
            salida[8] = datos_detector[8];
            salida[9] = datos_detector[9];
            salida[10] = resultado.severidad;
            salida[11] = resultado.fuente;
            salida[12] = resultado.detector_solo_decide;
            salida[13] = resultado.modelo_vigente;
            salida[14] = resultado.edad_modelo_ms;
            salida[15] = resultado.aviso;
            salida[16] = resultado.aviso_clase;
            salida[17] = resultado.aviso_zona;
            salida[18] = resultado.aviso_cercania;
            salida[19] = resultado.clase_en_camino;
            salida[20] = resultado.desmentidos;
        }
    }
    arreglo = (*env)->NewIntArray(env, ENTEROS_RESULTADO_FUSION);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_RESULTADO_FUSION, salida);
    }
    return arreglo;
}

/* ── Modo «cualquier objeto» del modelo de objetos (cualquiera.c) ──────── */

#define ENTEROS_CONFIG_CUALQUIERA   9
#define ENTEROS_RESULTADO_CUALQUIERA (16 + 6 * FUS_MAX_CAJAS)
#define UMBRALES_BARRIDO            6

static EstadoCualquiera estado_cualquiera;          /* la historia del modo: solo la usa el hilo de la cámara */
static int estado_cualquiera_iniciado = 0;
static EstadoCualquiera estados_barrido[UMBRALES_BARRIDO];   /* una historia por cada umbral del registro OBJ_ANY */
static int estados_barrido_iniciados = 0;
static const int UMBRALES_BARRIDO_MIL[UMBRALES_BARRIDO] = { 10, 50, 100, 150, 200, 300 };

/* Lee la configuración del modo (9 enteros): [0] activo [1] umbral_mil [2] area_max_mil [3] borde_inferior_min_mil [4] area_min_mil [5] ancho_min_mil
 * [6] persistencia [7] solape_mil [8] vigencia_ms. Con un arreglo malo deja una configuración NO válida (el C responde con error). */
static void leer_config_cualquiera(JNIEnv *env, jintArray config, ConfigCualquiera *cfg)
{
    jint datos[ENTEROS_CONFIG_CUALQUIERA];

    if (config == NULL || (*env)->GetArrayLength(env, config) < ENTEROS_CONFIG_CUALQUIERA) {
        cualq_config_defecto(cfg);
        cfg->umbral_mil = -1;
        return;
    }
    (*env)->GetIntArrayRegion(env, config, 0, ENTEROS_CONFIG_CUALQUIERA, datos);
    cfg->activo = (int)datos[0];
    cfg->umbral_mil = (int)datos[1];
    cfg->area_max_mil = (int)datos[2];
    cfg->borde_inferior_min_mil = (int)datos[3];
    cfg->area_min_mil = (int)datos[4];
    cfg->ancho_min_mil = (int)datos[5];
    cfg->persistencia = (int)datos[6];
    cfg->solape_mil = (int)datos[7];
    cfg->vigencia_ms = (int)datos[8];
}

/* Lee las cajas del modelo ([n, (clase, confianza_mil, xmin, ymin, xmax, ymax) * n]) y la marca de tiempo. Devuelve 1 si hay un resultado del modelo (aunque sea con 0 cajas). */
static int leer_cajas_modelo(JNIEnv *env, jintArray cajas, jlong marca_ms, DeteccionesModelo *modelo)
{
    jint datos[1 + FUS_MAX_CAJAS * ENTEROS_POR_CAJA];
    jsize largo;
    int n, i;

    memset(modelo, 0, sizeof *modelo);
    modelo->marca_ms = (unsigned int)marca_ms;
    if (cajas == NULL || (*env)->GetArrayLength(env, cajas) < 1) {
        return 0;
    }
    largo = (*env)->GetArrayLength(env, cajas);
    if (largo > 1 + FUS_MAX_CAJAS * ENTEROS_POR_CAJA) {
        largo = 1 + FUS_MAX_CAJAS * ENTEROS_POR_CAJA;
    }
    (*env)->GetIntArrayRegion(env, cajas, 0, largo, datos);
    n = (int)datos[0];
    if (n > (int)((largo - 1) / ENTEROS_POR_CAJA)) {
        n = (int)((largo - 1) / ENTEROS_POR_CAJA);
    }
    if (n > FUS_MAX_CAJAS) {
        n = FUS_MAX_CAJAS;
    }
    for (i = 0; i < n; i++) {
        modelo->cajas[i].clase = (int)datos[1 + i * ENTEROS_POR_CAJA];
        modelo->cajas[i].confianza_mil = (int)datos[2 + i * ENTEROS_POR_CAJA];
        modelo->cajas[i].xmin_mil = (int)datos[3 + i * ENTEROS_POR_CAJA];
        modelo->cajas[i].ymin_mil = (int)datos[4 + i * ENTEROS_POR_CAJA];
        modelo->cajas[i].xmax_mil = (int)datos[5 + i * ENTEROS_POR_CAJA];
        modelo->cajas[i].ymax_mil = (int)datos[6 + i * ENTEROS_POR_CAJA];
    }
    modelo->n = (n > 0) ? n : 0;
    return 1;
}

/* Procesa lo último que dijo el modelo en el modo «cualquier objeto» (ver cualquiera.h). Solo desde el hilo de la cámara. Devuelve:
 *   [0] código (0 = bien)  [1] vigente  [2] edad del modelo en ms  [3] cajas que trajo el modelo  [4] las que pasan los filtros
 *   [5] las que además persisten (cuentan)  [6] cuántas cuentan  [7..12] zonas izq/centro/der: hay, cercanía  [13] mejor zona (-1 = ninguna)
 *   [14] mejor cercanía  [15] 1 si era un cuadro nuevo del modelo
 *   [16...] por cada caja que cuenta (hasta 10): xmin, ymin, xmax, ymax (milésimas), zona, cercanía */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CualquieraNativa_actualizar(JNIEnv *env, jobject objeto, jintArray config, jintArray cajas, jlong marca_ms, jlong ahora_ms)
{
    jint salida[ENTEROS_RESULTADO_CUALQUIERA];
    ConfigCualquiera cfg;
    DeteccionesModelo modelo;
    ResultadoCualquiera r;
    jintArray arreglo;
    int hay_modelo, codigo, i, z;

    (void)objeto;
    for (i = 0; i < ENTEROS_RESULTADO_CUALQUIERA; i++) {
        salida[i] = 0;
    }
    salida[13] = -1;
    if (!estado_cualquiera_iniciado) {
        cualq_iniciar(&estado_cualquiera);
        estado_cualquiera_iniciado = 1;
    }
    leer_config_cualquiera(env, config, &cfg);
    hay_modelo = leer_cajas_modelo(env, cajas, marca_ms, &modelo);
    codigo = cualq_actualizar(&cfg, &estado_cualquiera, hay_modelo ? &modelo : NULL, (unsigned int)ahora_ms, &r);
    salida[0] = codigo;
    if (codigo == CUALQ_OK) {
        salida[1] = r.vigente;
        salida[2] = r.edad_modelo_ms;
        salida[3] = r.cajas_modelo;
        salida[4] = r.pasan_filtros;
        salida[5] = r.persistentes;
        salida[6] = r.n_cuentan;
        for (z = 0; z < OBS_ZONAS; z++) {
            salida[7 + 2 * z] = r.zona[z].hay_obstaculo;
            salida[8 + 2 * z] = r.zona[z].cercania;
        }
        salida[13] = r.mejor_zona;
        salida[14] = r.mejor_cercania;
        salida[15] = r.nuevo_cuadro;
        for (i = 0; i < r.n_cuentan && i < FUS_MAX_CAJAS; i++) {
            salida[16 + 6 * i] = r.cuentan[i].xmin_mil;
            salida[17 + 6 * i] = r.cuentan[i].ymin_mil;
            salida[18 + 6 * i] = r.cuentan[i].xmax_mil;
            salida[19 + 6 * i] = r.cuentan[i].ymax_mil;
            salida[20 + 6 * i] = r.zona_cuentan[i];
            salida[21 + 6 * i] = r.cercania_cuentan[i];
        }
    }
    arreglo = (*env)->NewIntArray(env, ENTEROS_RESULTADO_CUALQUIERA);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_RESULTADO_CUALQUIERA, salida);
    }
    return arreglo;
}

/* El barrido del registro OBJ_ANY: con las mismas cajas (las del umbral más bajo) corre el modo con cada uno de los umbrales 0.01, 0.05, 0.10, 0.15, 0.20 y 0.30 (cada
 * uno con su propia historia de persistencia) y dice cuántas cajas pasan filtros y persistencia. `config` es la del modo (su umbral se ignora). Devuelve 6 x 5 enteros:
 *   por umbral: [umbral_mil, cajas que pasan filtros, las que cuentan, mejor zona (-1 = ninguna), mejor cercanía]. Solo desde el hilo de la cámara. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CualquieraNativa_barrido(JNIEnv *env, jobject objeto, jintArray config, jintArray cajas, jlong marca_ms, jlong ahora_ms)
{
    jint salida[UMBRALES_BARRIDO * 5];
    ConfigCualquiera cfg;
    DeteccionesModelo modelo;
    jintArray arreglo;
    int hay_modelo, u, i;

    (void)objeto;
    for (i = 0; i < UMBRALES_BARRIDO * 5; i++) {
        salida[i] = 0;
    }
    if (!estados_barrido_iniciados) {
        for (u = 0; u < UMBRALES_BARRIDO; u++) {
            cualq_iniciar(&estados_barrido[u]);
        }
        estados_barrido_iniciados = 1;
    }
    leer_config_cualquiera(env, config, &cfg);
    hay_modelo = leer_cajas_modelo(env, cajas, marca_ms, &modelo);
    for (u = 0; u < UMBRALES_BARRIDO; u++) {
        ResultadoCualquiera r;
        ConfigCualquiera c = cfg;

        c.umbral_mil = UMBRALES_BARRIDO_MIL[u];
        salida[5 * u] = c.umbral_mil;
        salida[5 * u + 3] = -1;
        if (cualq_actualizar(&c, &estados_barrido[u], hay_modelo ? &modelo : NULL, (unsigned int)ahora_ms, &r) == CUALQ_OK) {
            salida[5 * u + 1] = r.pasan_filtros;
            salida[5 * u + 2] = r.persistentes;
            salida[5 * u + 3] = r.mejor_zona;
            salida[5 * u + 4] = r.mejor_cercania;
        }
    }
    arreglo = (*env)->NewIntArray(env, UMBRALES_BARRIDO * 5);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, UMBRALES_BARRIDO * 5, salida);
    }
    return arreglo;
}

/* Olvida la historia de los dos (el modo y el barrido): por ejemplo, al encender el modo o al cerrar la cámara. */
JNIEXPORT void JNICALL
Java_com_fsi_carro_CualquieraNativa_reiniciar(JNIEnv *env, jobject objeto)
{
    int u;

    (void)env;
    (void)objeto;
    cualq_iniciar(&estado_cualquiera);
    estado_cualquiera_iniciado = 1;
    for (u = 0; u < UMBRALES_BARRIDO; u++) {
        cualq_iniciar(&estados_barrido[u]);
    }
    estados_barrido_iniciados = 1;
}

/* La fusión del modo «cualquier objeto» (fusion_decidir_cualquiera: el color manda). `detector` es el arreglo de analizarConfig; `zonas` son 6 enteros
 * (hay, cercanía) de izquierda, centro y derecha de lo que cuenta en «cualquier objeto»; `vigente` es 1 si ese resultado no está caducado. Devuelve los mismos
 * 21 enteros que FusionNativa.decidir. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_FusionNativa_decidirCualquiera(JNIEnv *env, jobject objeto, jintArray config, jintArray detector, jintArray zonas, jboolean vigente)
{
    ConfigFusion cfg;
    ResultadoObstaculos det;
    ZonaObstaculo cual[OBS_ZONAS];
    ResultadoFusion resultado;
    jint datos_config[ENTEROS_CONFIG_FUSION];
    jint datos_detector[ENTEROS_ENCABEZADO_DETALLE];
    jint datos_zonas[2 * OBS_ZONAS];
    jint salida[ENTEROS_RESULTADO_FUSION];
    jintArray arreglo;
    int codigo, i, z;

    (void)objeto;
    for (i = 0; i < ENTEROS_RESULTADO_FUSION; i++) {
        salida[i] = 0;
    }
    salida[19] = -1;
    memset(&det, 0, sizeof det);
    memset(cual, 0, sizeof cual);
    if (config == NULL || (*env)->GetArrayLength(env, config) < ENTEROS_CONFIG_FUSION || detector == NULL ||
        (*env)->GetArrayLength(env, detector) < ENTEROS_ENCABEZADO_DETALLE || zonas == NULL || (*env)->GetArrayLength(env, zonas) < 2 * OBS_ZONAS) {
        salida[0] = FUS_ERR_ARGUMENTO;
    } else {
        (*env)->GetIntArrayRegion(env, config, 0, ENTEROS_CONFIG_FUSION, datos_config);
        cfg.vigencia_ms = (int)datos_config[0];
        cfg.confianza_min_mil = (int)datos_config[1];
        cfg.confianza_frenar_mil = (int)datos_config[2];
        cfg.solape_camino_mil = (int)datos_config[3];
        cfg.cerca_mil = (int)datos_config[4];
        cfg.media_mil = (int)datos_config[5];
        cfg.area_grande_mil = (int)datos_config[6];
        cfg.clases_interes[0] = (unsigned int)datos_config[7];
        cfg.clases_interes[1] = (unsigned int)datos_config[8];
        cfg.clases_interes[2] = (unsigned int)datos_config[9];
        (*env)->GetIntArrayRegion(env, detector, 0, ENTEROS_ENCABEZADO_DETALLE, datos_detector);
        (*env)->GetIntArrayRegion(env, zonas, 0, 2 * OBS_ZONAS, datos_zonas);
        for (z = 0; z < OBS_ZONAS; z++) {
            det.zona[z].hay_obstaculo = (int)datos_detector[1 + 2 * z];
            det.zona[z].cercania = (int)datos_detector[2 + 2 * z];
            det.zona[z].celdas = (int)datos_detector[35 + z];
            cual[z].hay_obstaculo = (int)datos_zonas[2 * z];
            cual[z].cercania = (int)datos_zonas[2 * z + 1];
            cual[z].celdas = cual[z].hay_obstaculo ? 1 : 0;
        }
        codigo = fusion_decidir_cualquiera(&cfg, &det, cual, vigente ? 1 : 0, &resultado);
        salida[0] = codigo;
        if (codigo == FUS_OK) {
            for (z = 0; z < OBS_ZONAS; z++) {
                salida[1 + 2 * z] = resultado.zona[z].hay_obstaculo;
                salida[2 + 2 * z] = resultado.zona[z].cercania;
            }
            salida[7] = datos_detector[7];
            salida[8] = datos_detector[8];
            salida[9] = datos_detector[9];
            salida[10] = resultado.severidad;
            salida[11] = resultado.fuente;
            salida[12] = resultado.detector_solo_decide;
            salida[13] = resultado.modelo_vigente;
            salida[14] = resultado.edad_modelo_ms;
            salida[15] = resultado.aviso;
            salida[16] = resultado.aviso_clase;
            salida[17] = resultado.aviso_zona;
            salida[18] = resultado.aviso_cercania;
            salida[19] = resultado.clase_en_camino;
            salida[20] = resultado.desmentidos;
        }
    }
    arreglo = (*env)->NewIntArray(env, ENTEROS_RESULTADO_FUSION);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_RESULTADO_FUSION, salida);
    }
    return arreglo;
}

/* La entrada opcional del flujo del piso en la fusión (fusion_aplicar_flujo: el color manda). `fusion` es el arreglo que devolvió decidir() o decidirCualquiera();
 * `flujo` son 9 enteros (piso consistente, hay cerca, cercanía) de izquierda, centro y derecha; `vigente` es 1 si el flujo se evaluó en este cuadro. Devuelve los 22
 * enteros de la fusión (el último, [21], es cuántas zonas bajó el flujo). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_FusionNativa_aplicarFlujo(JNIEnv *env, jobject objeto, jintArray fusion, jintArray flujo, jboolean vigente)
{
    ConfigFusion cfg;
    ResultadoFusion resultado;
    ZonaFlujo zonas[OBS_ZONAS];
    jint datos[ENTEROS_RESULTADO_FUSION];
    jint datos_flujo[3 * OBS_ZONAS];
    jint salida[ENTEROS_RESULTADO_FUSION];
    jintArray arreglo;
    int i, z, codigo;

    (void)objeto;
    for (i = 0; i < ENTEROS_RESULTADO_FUSION; i++) {
        salida[i] = 0;
        datos[i] = 0;
    }
    salida[19] = -1;
    if (fusion == NULL || (*env)->GetArrayLength(env, fusion) < 21 || flujo == NULL || (*env)->GetArrayLength(env, flujo) < 3 * OBS_ZONAS) {
        salida[0] = FUS_ERR_ARGUMENTO;
    } else {
        (*env)->GetIntArrayRegion(env, fusion, 0, 21, datos);
        (*env)->GetIntArrayRegion(env, flujo, 0, 3 * OBS_ZONAS, datos_flujo);
        fusion_config_defecto(&cfg);
        memset(&resultado, 0, sizeof resultado);
        memset(zonas, 0, sizeof zonas);
        for (z = 0; z < OBS_ZONAS; z++) {
            resultado.zona[z].hay_obstaculo = (int)datos[1 + 2 * z];
            resultado.zona[z].cercania = (int)datos[2 + 2 * z];
            resultado.zona[z].celdas = (int)datos[7 + z];
            zonas[z].piso_consistente = (int)datos_flujo[3 * z];
            zonas[z].hay_cerca = (int)datos_flujo[3 * z + 1];
            zonas[z].cercania = (int)datos_flujo[3 * z + 2];
        }
        resultado.severidad = (int)datos[10];
        resultado.fuente = (int)datos[11];
        resultado.detector_solo_decide = (int)datos[12];
        resultado.modelo_vigente = (int)datos[13];
        resultado.edad_modelo_ms = (int)datos[14];
        resultado.aviso = (int)datos[15];
        resultado.aviso_clase = (int)datos[16];
        resultado.aviso_zona = (int)datos[17];
        resultado.aviso_cercania = (int)datos[18];
        resultado.clase_en_camino = (int)datos[19];
        resultado.desmentidos = (int)datos[20];
        codigo = fusion_aplicar_flujo(&cfg, zonas, vigente ? 1 : 0, &resultado);
        salida[0] = codigo;
        if (codigo == FUS_OK) {
            for (z = 0; z < OBS_ZONAS; z++) {
                salida[1 + 2 * z] = resultado.zona[z].hay_obstaculo;
                salida[2 + 2 * z] = resultado.zona[z].cercania;
                salida[7 + z] = resultado.zona[z].celdas;
            }
            salida[10] = resultado.severidad;
            salida[11] = resultado.fuente;
            salida[12] = resultado.detector_solo_decide;
            salida[13] = resultado.modelo_vigente;
            salida[14] = resultado.edad_modelo_ms;
            salida[15] = resultado.aviso;
            salida[16] = resultado.aviso_clase;
            salida[17] = resultado.aviso_zona;
            salida[18] = resultado.aviso_cercania;
            salida[19] = resultado.clase_en_camino;
            salida[20] = resultado.desmentidos;
            salida[21] = resultado.bajados_flujo;
        }
    }
    arreglo = (*env)->NewIntArray(env, ENTEROS_RESULTADO_FUSION);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_RESULTADO_FUSION, salida);
    }
    return arreglo;
}

/* ── Flujo del piso (movimiento_piso.c) ────────────────────────────────── */

#define ENTEROS_CONFIG_FLUJO     13
#define ENTEROS_RESULTADO_FLUJO  (10 + 7 * OBS_ZONAS)

static unsigned char gris_flujo_anterior[640 * 480];
static unsigned char gris_flujo_actual[640 * 480];
static ResultadoFlujo resultado_flujo;

/* Compara dos cuadros RGB seguidos (ancho x alto x 3 bytes) con el flujo del piso. `config` son 13 enteros (ver FlujoNativa.CFG_*), `corredor_cfg` los 7 de CorredorNativa
 * (de ahí salen la altura, la inclinación y el campo de visión de la cámara). Devuelve 31 enteros:
 *   [0] código  [1] evaluado  [2] motivo (FLU_MOT_*)  [3] avance en mm  [4] bloques piso  [5] cerca  [6] dudoso  [7] sin dato  [8] fila inicial  [9] fila final
 *   y por zona (izquierda, centro, derecha) 7 enteros desde [10]: piso, cerca, dudoso, sin dato, piso consistente, hay cerca, cercanía. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_FlujoNativa_analizar(JNIEnv *env, jobject objeto, jintArray config, jintArray corredor_cfg, jbyteArray anterior, jbyteArray actual,
                                         jint ancho, jint alto, jboolean avanzando, jint dt_ms)
{
    jint salida[ENTEROS_RESULTADO_FLUJO];
    jint cfg[ENTEROS_CONFIG_FLUJO];
    jint ccor[7];
    ConfigFlujo configuracion;
    ConfigCorredor cc;
    Corredor corredor;
    jintArray arreglo;
    int i, z, codigo;

    (void)objeto;
    for (i = 0; i < ENTEROS_RESULTADO_FLUJO; i++) {
        salida[i] = 0;
    }
    if (config == NULL || corredor_cfg == NULL || anterior == NULL || actual == NULL || (*env)->GetArrayLength(env, config) < ENTEROS_CONFIG_FLUJO ||
        (*env)->GetArrayLength(env, corredor_cfg) < 7 || ancho < 48 || alto < 48 || ancho > 640 || alto > 480 ||
        (*env)->GetArrayLength(env, anterior) < ancho * alto * 3 || (*env)->GetArrayLength(env, actual) < ancho * alto * 3) {
        salida[0] = FLU_ERR_ARGUMENTO;
    } else {
        jbyte *a = (jbyte *)(*env)->GetPrimitiveArrayCritical(env, anterior, NULL);
        jbyte *b = NULL;

        if (a != NULL) {
            flujo_gris_de_rgb((const unsigned char *)a, (int)ancho, (int)alto, gris_flujo_anterior);
            (*env)->ReleasePrimitiveArrayCritical(env, anterior, a, JNI_ABORT);
            b = (jbyte *)(*env)->GetPrimitiveArrayCritical(env, actual, NULL);
            if (b != NULL) {
                flujo_gris_de_rgb((const unsigned char *)b, (int)ancho, (int)alto, gris_flujo_actual);
                (*env)->ReleasePrimitiveArrayCritical(env, actual, b, JNI_ABORT);
            }
        }
        if (a == NULL || b == NULL) {
            salida[0] = FLU_ERR_ARGUMENTO;
        } else {
            (*env)->GetIntArrayRegion(env, config, 0, ENTEROS_CONFIG_FLUJO, cfg);
            (*env)->GetIntArrayRegion(env, corredor_cfg, 0, 7, ccor);
            configuracion.activo = (int)cfg[0];
            configuracion.velocidad_mm_s = (int)cfg[1];
            configuracion.ventana_px = (int)cfg[2];
            configuracion.ruido_gradiente = (int)cfg[3];
            configuracion.textura_min = (int)cfg[4];
            configuracion.ajuste_max = (int)cfg[5];
            configuracion.margen_unico = (int)cfg[6];
            configuracion.tolerancia_px4 = (int)cfg[7];
            configuracion.tolerancia_pct = (int)cfg[8];
            configuracion.dt_min_ms = (int)cfg[9];
            configuracion.dt_max_ms = (int)cfg[10];
            configuracion.distancia_max_cm = (int)cfg[11];
            configuracion.min_bloques = (int)cfg[12];
            cc.activo = 1;
            cc.ancho_carro_cm = (int)ccor[1];
            cc.altura_camara_cm = (int)ccor[2];
            cc.inclinacion_grados_x10 = (int)ccor[3];
            cc.fov_grados_x10 = (int)ccor[4];
            cc.margen_lateral_cm = (int)ccor[5];
            cc.min_peso = (int)ccor[6];
            codigo = corredor_preparar(&corredor, &cc, (int)ancho, (int)alto);
            if (codigo == COR_OK) {
                codigo = flujo_analizar(&configuracion, &corredor, gris_flujo_anterior, gris_flujo_actual, (int)ancho, (int)alto, avanzando ? 1 : 0, (int)dt_ms,
                                        &resultado_flujo);
            }
            salida[0] = codigo;
            if (codigo == FLU_OK) {
                const ResultadoFlujo *r = &resultado_flujo;

                salida[1] = r->evaluado;
                salida[2] = r->motivo;
                salida[3] = r->avance_mm;
                salida[4] = r->total_piso;
                salida[5] = r->total_cerca;
                salida[6] = r->total_dudoso;
                salida[7] = r->total_sin_dato;
                salida[8] = r->fila_inicial;
                salida[9] = r->fila_final;
                for (z = 0; z < OBS_ZONAS; z++) {
                    salida[10 + 7 * z] = r->zona[z].piso;
                    salida[11 + 7 * z] = r->zona[z].cerca;
                    salida[12 + 7 * z] = r->zona[z].dudoso;
                    salida[13 + 7 * z] = r->zona[z].sin_dato;
                    salida[14 + 7 * z] = r->zona[z].piso_consistente;
                    salida[15 + 7 * z] = r->zona[z].hay_cerca;
                    salida[16 + 7 * z] = r->zona[z].cercania;
                }
            }
        }
    }
    arreglo = (*env)->NewIntArray(env, ENTEROS_RESULTADO_FLUJO);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_RESULTADO_FLUJO, salida);
    }
    return arreglo;
}

/* Los valores por defecto del flujo (13 enteros, el primero —activo— en 0). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_FlujoNativa_configDefecto(JNIEnv *env, jobject objeto)
{
    ConfigFlujo c;
    jint datos[ENTEROS_CONFIG_FLUJO];
    jintArray arreglo;

    (void)objeto;
    flujo_config_defecto(&c);
    datos[0] = c.activo;
    datos[1] = c.velocidad_mm_s;
    datos[2] = c.ventana_px;
    datos[3] = c.ruido_gradiente;
    datos[4] = c.textura_min;
    datos[5] = c.ajuste_max;
    datos[6] = c.margen_unico;
    datos[7] = c.tolerancia_px4;
    datos[8] = c.tolerancia_pct;
    datos[9] = c.dt_min_ms;
    datos[10] = c.dt_max_ms;
    datos[11] = c.distancia_max_cm;
    datos[12] = c.min_bloques;
    arreglo = (*env)->NewIntArray(env, ENTEROS_CONFIG_FLUJO);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_CONFIG_FLUJO, datos);
    }
    return arreglo;
}

/* ── Calibración del carro ─────────────────────────────────────────────── */

/* La calibración se pasa a Kotlin como 10 enteros, en este orden (el mismo de CalibracionCarro):
 *   [0] medidos  [1] arranque_ms  [2] ms_por_cm_x10  [3] frenado_mm
 *   [4] ms_por_grado_izq_x10  [5] ms_por_grado_der_x10
 *   [6] frenado_grados_izq_x10  [7] frenado_grados_der_x10  [8] fov_grados_x10 (campo de visión de la cámara)
 *   [9] ms_por_cm_atras_x10 (ms por cm RETROCEDIENDO; vale solo si el bit CAL_MEDIDO_ATRAS está en `medidos`)
 * Las funciones que cambian la calibración devuelven 11 enteros: [0] = código (0 = bien) y [1..10] = la
 * calibración resultante (sin cambios si hubo error). */
#define ENTEROS_CALIBRACION 10

/* La calibración que usa el navegador: se aplica cada vez que se carga una ruta. */
static CalibracionCarro calibracion_global;
static int calibracion_inicializada = 0;
static int celdas_zona_global[3] = { -1, -1, -1 };   /* celdas de obstáculo de cada zona del último cuadro (-1 = no se sabe: se estiman) */
static int corredor_fuera_global[3] = { 0, 0, 0 };   /* 1 = la zona tiene un obstáculo FUERA del corredor (solo avisa): lo pone Kotlin antes de cada tick */
static int corredor_cercania_global[3] = { 0, 0, 0 };   /* cercanía de lo que está DENTRO del corredor en cada zona (0 = usar la de siempre) */
static int frenar_lejos_centro_global = 0;   /* 1 = también frena con un obstáculo LEJANO en el centro: se aplica en cada ruta nueva */
static int latencia_visual_global = 0;   /* retraso de la cámara en ms: se aplica en cada ruta nueva */

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
    cal->fov_grados_x10 = (int)datos[8];
    cal->ms_por_cm_atras_x10 = (int)datos[9];
    return 1;
}

/* Arma el arreglo de 10 enteros [código, calibración...]. */
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
    salida[9] = cal->fov_grados_x10;
    salida[10] = cal->ms_por_cm_atras_x10;
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

/* Ajusta el retroceso con los tiempos (ms de REVERSE) y lo que retrocedió el carro (mm). Solo cambia los ms por cm hacia atrás. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CalibracionNativa_ajustarRetroceso(JNIEnv *env, jobject objeto, jintArray calibracion,
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
    codigo = (n == 0) ? CAL_ERR_ARGUMENTO : cal_ajustar_retroceso(&cal, n, lista_ms, lista_mm);
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

/* Calibra el campo de visión de la cámara con un giro medido: real y visual, en grados por 10. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CalibracionNativa_ajustarFov(JNIEnv *env, jobject objeto, jintArray calibracion, jint real_x10, jint visual_x10)
{
    CalibracionCarro cal;
    int codigo;

    (void)objeto;
    if (!leer_calibracion(env, calibracion, &cal)) {
        cal_defecto(&cal);
        return devolver_calibracion(env, CAL_ERR_ARGUMENTO, &cal);
    }
    codigo = cal_ajustar_fov(&cal, (int)real_x10, (int)visual_x10);
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

#define ENTEROS_ESTADO_NAVEGADOR 11

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
    navegador_poner_frenar_lejos_centro(&navegador_global, frenar_lejos_centro_global);
    navegador_poner_latencia_visual(&navegador_global, latencia_visual_global);   /* el retraso de la cámara */
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

/* Dice cuánto se atrasa la cámara en ms (0 con la cámara del teléfono, ~300 con la del carro por WiFi). Devuelve 0 o un código de error. */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_NavegadorNativa_ponerLatenciaVisual(JNIEnv *env, jobject objeto, jint ms)
{
    (void)env;
    (void)objeto;
    if (ms < 0 || ms > 1000) {
        return NAV_ERR_ARGUMENTO;
    }
    latencia_visual_global = (int)ms;
    return navegador_poner_latencia_visual(obtener_navegador(), (int)ms);
}

/* Activa (1) o desactiva (0) el frenado desde lejos en el centro (ver navegador.h). Se guarda para las rutas siguientes. */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_NavegadorNativa_frenarLejosCentro(JNIEnv *env, jobject objeto, jint activo)
{
    (void)env;
    (void)objeto;
    if (activo != 0 && activo != 1) {
        return NAV_ERR_ARGUMENTO;
    }
    frenar_lejos_centro_global = (int)activo;
    return navegador_poner_frenar_lejos_centro(obtener_navegador(), (int)activo);
}

/* Dice cuántas celdas de obstáculo tiene cada zona en el cuadro que se va a mandar al siguiente tick (con -1 no se sabe). Sirve para
 * que el navegador elija el lado con MÁS espacio libre al esquivar (ver elegir_lado en navegador.c). */
JNIEXPORT void JNICALL
Java_com_fsi_carro_NavegadorNativa_ponerCeldas(JNIEnv *env, jobject objeto, jint izquierda, jint centro, jint derecha)
{
    (void)env;
    (void)objeto;
    celdas_zona_global[ZONA_IZQUIERDA] = (int)izquierda;
    celdas_zona_global[ZONA_CENTRO] = (int)centro;
    celdas_zona_global[ZONA_DERECHA] = (int)derecha;
}

/* Dice qué zonas tienen un obstáculo FUERA del corredor (solo avisan, no frenan) y la cercanía de lo que está DENTRO, para el siguiente tick
 * (los mismos números que devuelve CorredorNativa.aplicar). Con 0 en todo (corredor apagado o fuente de botones) todo cuenta como siempre. */
JNIEXPORT void JNICALL
Java_com_fsi_carro_NavegadorNativa_ponerCorredor(JNIEnv *env, jobject objeto, jint fuera_izq, jint fuera_cen, jint fuera_der,
                                                  jint cerc_izq, jint cerc_cen, jint cerc_der)
{
    (void)env;
    (void)objeto;
    corredor_fuera_global[ZONA_IZQUIERDA] = fuera_izq ? 1 : 0;
    corredor_fuera_global[ZONA_CENTRO] = fuera_cen ? 1 : 0;
    corredor_fuera_global[ZONA_DERECHA] = fuera_der ? 1 : 0;
    corredor_cercania_global[ZONA_IZQUIERDA] = (int)cerc_izq;
    corredor_cercania_global[ZONA_CENTRO] = (int)cerc_cen;
    corredor_cercania_global[ZONA_DERECHA] = (int)cerc_der;
}

/* Avisa cuánto vio girar la cámara entre dos cuadros consecutivos: `giro_x10` en grados por 10 (positivo a la derecha),
 * `dt_ms` el tiempo entre esos cuadros y `confiable` si la cámara estaba segura (ver navegador_aporte_visual). */
JNIEXPORT void JNICALL
Java_com_fsi_carro_NavegadorNativa_aporteVisual(JNIEnv *env, jobject objeto, jint giro_x10, jint dt_ms, jboolean confiable)
{
    (void)env;
    (void)objeto;
    navegador_aporte_visual(obtener_navegador(), (int)giro_x10, (int)dt_ms, confiable ? 1 : 0);
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
            resultado.zona[z].celdas = (celdas_zona_global[z] >= 0) ? celdas_zona_global[z] : (resultado.zona[z].hay_obstaculo ? MIN_CELDAS : 0);
            resultado.zona[z].celdas_corredor = 0;
            resultado.zona[z].fuera_corredor = corredor_fuera_global[z];
            resultado.zona[z].cercania_corredor = corredor_cercania_global[z];
        }
        cuadro = &resultado;
    }
    return (jint)navegador_tick(obtener_navegador(), (uint32_t)ahora_ms, cuadro);
}

/* Estado para mostrar en pantalla (11 enteros):
 *   [0] estado (0 ESPERANDO ... 6 ERROR, ver navegador.h)  [1] x en mm  [2] y en mm
 *   [3] rumbo en grados  [4] punto objetivo  [5] total de puntos  [6] esquivadas
 *   [7] motivo del error  [8] último comando (0 a 4)  [9] reintentos de esquive  [10] fase de esquivar (NAV_FASE_*) */
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
    salida[9] = nav->reintentos;
    salida[10] = nav->fase;
    arreglo = (*env)->NewIntArray(env, ENTEROS_ESTADO_NAVEGADOR);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_ESTADO_NAVEGADOR, salida);
    }
    return arreglo;
}

/* La siguiente línea de diagnóstico del navegador (REINC_DIAG o ESQUIVE_DIAG), o "" si no hay ninguna nueva. La app la llama después de cada tick hasta que
 * devuelve "" y copia cada línea al log. */
JNIEXPORT jstring JNICALL
Java_com_fsi_carro_NavegadorNativa_diagnostico(JNIEnv *env, jobject objeto)
{
    char texto[NAV_DIAG_LARGO];

    (void)objeto;
    if (!navegador_diag_siguiente(obtener_navegador(), texto, sizeof texto)) {
        texto[0] = '\0';
    }
    return (*env)->NewStringUTF(env, texto);
}

/* La causa concreta del ERROR del navegador, con los números ("se agotaron los 2 reintentos, el obstáculo seguía en el centro"), o "" si no hay error. */
JNIEXPORT jstring JNICALL
Java_com_fsi_carro_NavegadorNativa_textoError(JNIEnv *env, jobject objeto)
{
    char texto[NAV_DIAG_LARGO];

    (void)objeto;
    if (navegador_texto_error(obtener_navegador(), texto, sizeof texto) == 0) {
        texto[0] = '\0';
    }
    return (*env)->NewStringUTF(env, texto);
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


/* ── Estadística del video del carro (video_stats.c) ───────────────────── */

/* Una sola estadística (la del video del carro). La protege un candado de Kotlin (VideoStatsNativa), así que aquí no hace falta otro. */
static VideoStats estadistica_video;
static int estadistica_video_iniciada = 0;

JNIEXPORT void JNICALL
Java_com_fsi_carro_VideoStatsNativa_nIniciar(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    vs_iniciar(&estadistica_video);
    estadistica_video_iniciada = 1;
}

/* Llegó un cuadro de video a la hora `ahora_ms` (reloj del teléfono, ms). */
JNIEXPORT void JNICALL
Java_com_fsi_carro_VideoStatsNativa_nCuadro(JNIEnv *env, jobject objeto, jlong ahora_ms)
{
    (void)env;
    (void)objeto;
    if (!estadistica_video_iniciada) {
        vs_iniciar(&estadistica_video);
        estadistica_video_iniciada = 1;
    }
    vs_cuadro(&estadistica_video, (uint32_t)ahora_ms);
}

/* Resumen de los últimos 10 s: [0] fps por 10, [1] hueco máximo ms, [2] huecos mayores a 500 ms, [3] cuadros, [4] hueco actual ms. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_VideoStatsNativa_nResumen(JNIEnv *env, jobject objeto, jlong ahora_ms)
{
    jint salida[5];
    ResumenVideo r;
    jintArray arreglo;

    (void)objeto;
    if (!estadistica_video_iniciada) {
        vs_iniciar(&estadistica_video);
        estadistica_video_iniciada = 1;
    }
    r = vs_resumen(&estadistica_video, (uint32_t)ahora_ms);
    salida[0] = r.fps_x10;
    salida[1] = r.hueco_max_ms;
    salida[2] = r.huecos_largos;
    salida[3] = r.cuadros;
    salida[4] = r.hueco_actual_ms;
    arreglo = (*env)->NewIntArray(env, 5);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 5, salida);
    }
    return arreglo;
}

/* ── Cuentas de la prueba de obstáculos (prueba_obstaculo.c) ───────────── */

static ResumenPrueba resumen_prueba_obstaculo;

/* Retraso total obstáculo → STOP en ms (video + ms hasta enviar el STOP + mitad del ida y vuelta). */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_PruebaObstaculoNativa_retrasoMs(JNIEnv *env, jobject objeto, jint ms_stop, jint ms_respuesta, jint latencia_video_ms)
{
    PasadaObstaculo p;

    (void)env;
    (void)objeto;
    p.ms_detectar = 0;
    p.ms_stop = (int)ms_stop;
    p.ms_respuesta = (int)ms_respuesta;
    p.latencia_video_ms = (int)latencia_video_ms;
    return po_retraso_ms(&p);
}

/* Décimas de cm que avanza el carro durante `retraso_ms` con la calibración dada (-1 si la calibración no es válida). */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_PruebaObstaculoNativa_cmX10(JNIEnv *env, jobject objeto, jint retraso_ms, jint ms_por_cm_x10, jint frenado_mm)
{
    (void)env;
    (void)objeto;
    return po_cm_x10((int)retraso_ms, (int)ms_por_cm_x10, (int)frenado_mm);
}

JNIEXPORT void JNICALL
Java_com_fsi_carro_PruebaObstaculoNativa_iniciarResumen(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    po_resumen_iniciar(&resumen_prueba_obstaculo);
}

JNIEXPORT void JNICALL
Java_com_fsi_carro_PruebaObstaculoNativa_agregar(JNIEnv *env, jobject objeto, jint retraso_ms, jint cm_x10)
{
    (void)env;
    (void)objeto;
    po_resumen_agregar(&resumen_prueba_obstaculo, (int)retraso_ms, (int)cm_x10);
}

/* [0] pasadas, [1] retraso promedio ms, [2] retraso máximo ms, [3] cm promedio por 10, [4] cm máximo por 10. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_PruebaObstaculoNativa_resumen(JNIEnv *env, jobject objeto)
{
    jint salida[5];
    jintArray arreglo;

    (void)objeto;
    salida[0] = resumen_prueba_obstaculo.n;
    salida[1] = po_resumen_retraso_prom_ms(&resumen_prueba_obstaculo);
    salida[2] = resumen_prueba_obstaculo.max_retraso_ms;
    salida[3] = po_resumen_cm_prom_x10(&resumen_prueba_obstaculo);
    salida[4] = resumen_prueba_obstaculo.max_cm_x10;
    arreglo = (*env)->NewIntArray(env, 5);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 5, salida);
    }
    return arreglo;
}


/* ── Corredor de trayectoria (corredor.c) ──────────────────────────────── */

/* Aplica el corredor a lo que vio el detector de color en un cuadro.
 *   `config`: 7 enteros [activo, ancho del carro cm, altura de la cámara cm, inclinación grados por 10, campo de visión grados por 10,
 *             margen lateral cm, peso mínimo].
 *   `detalle`: el arreglo de ObstaculosNativa.analizarConfig (resumen, encabezado y máscara de celdas).
 *   `ancho`, `alto`: tamaño del cuadro en píxeles.
 * Devuelve 10 enteros: [0] código (0 = bien; COR_ERR_* o -10 si el detalle no sirve), [1..3] zona izquierda/centro/derecha FUERA del
 * corredor (1 = hay obstáculo pero no por donde pasa el carro), [4..6] cercanía de lo que está DENTRO del corredor en cada zona (0 = usar la de
 * siempre), [7..9] peso (celdas) dentro del corredor en cada zona. Con el corredor apagado devuelve ceros (código 0). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_CorredorNativa_aplicar(JNIEnv *env, jobject objeto, jintArray config, jintArray detalle, jint ancho, jint alto)
{
    jint salida[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    jint cfg[7];
    jintArray arreglo;
    ConfigCorredor configuracion;
    Corredor corredor;
    ZonaObstaculo zonas[OBS_ZONAS];
    static unsigned char mascara[OBS_MAX_FILAS * OBS_MAX_COLUMNAS];
    jsize largo;
    int z, codigo = COR_OK, columnas, filas, fila_minima, fila, columna;

    (void)objeto;
    if (config == NULL || detalle == NULL || (*env)->GetArrayLength(env, config) < 7) {
        salida[0] = COR_ERR_ARGUMENTO;
    } else {
        (*env)->GetIntArrayRegion(env, config, 0, 7, cfg);
        configuracion.activo = (int)cfg[0];
        configuracion.ancho_carro_cm = (int)cfg[1];
        configuracion.altura_camara_cm = (int)cfg[2];
        configuracion.inclinacion_grados_x10 = (int)cfg[3];
        configuracion.fov_grados_x10 = (int)cfg[4];
        configuracion.margen_lateral_cm = (int)cfg[5];
        configuracion.min_peso = (int)cfg[6];
        largo = (*env)->GetArrayLength(env, detalle);
        if (!configuracion.activo) {
            codigo = COR_OK;   /* apagado: ceros y nada se filtra */
        } else if (largo < ENTEROS_ENCABEZADO_DETALLE) {
            codigo = -10;
        } else {
            jint *datos = (jint *)(*env)->GetPrimitiveArrayCritical(env, detalle, NULL);

            if (datos == NULL) {
                codigo = -10;
            } else {
                columnas = (int)datos[10];
                filas = (int)datos[11];
                fila_minima = (int)datos[12];
                if (datos[0] != 0 || columnas < 1 || filas < 1 || columnas > OBS_MAX_COLUMNAS || filas > OBS_MAX_FILAS ||
                    largo < ENTEROS_ENCABEZADO_DETALLE + columnas * filas) {
                    codigo = -10;   /* el análisis falló o el detalle está incompleto */
                } else {
                    memset(zonas, 0, sizeof zonas);
                    for (z = 0; z < OBS_ZONAS; z++) {
                        zonas[z].hay_obstaculo = (int)datos[1 + 2 * z];
                        zonas[z].cercania = (int)datos[2 + 2 * z];
                        zonas[z].celdas = (int)datos[35 + z];
                    }
                    for (fila = 0; fila < filas; fila++) {
                        for (columna = 0; columna < columnas; columna++) {
                            mascara[fila * OBS_MAX_COLUMNAS + columna] = (unsigned char)(datos[ENTEROS_ENCABEZADO_DETALLE + fila * columnas + columna] ? 1 : 0);
                        }
                    }
                }
                (*env)->ReleasePrimitiveArrayCritical(env, detalle, datos, JNI_ABORT);
                if (codigo == COR_OK) {
                    codigo = corredor_preparar(&corredor, &configuracion, (int)ancho, (int)alto);
                    if (codigo == COR_OK) {
                        codigo = corredor_aplicar_mascara(&corredor, mascara, OBS_MAX_COLUMNAS, columnas, filas, fila_minima, zonas);
                    }
                    if (codigo == COR_OK) {
                        for (z = 0; z < OBS_ZONAS; z++) {
                            salida[1 + z] = zonas[z].fuera_corredor;
                            salida[4 + z] = zonas[z].cercania_corredor;
                            salida[7 + z] = zonas[z].celdas_corredor;
                        }
                    }
                }
            }
        }
        salida[0] = codigo;
    }
    arreglo = (*env)->NewIntArray(env, 10);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 10, salida);
    }
    return arreglo;
}

/* ── Sesiones compartidas (sesion_compartida.c): el video de la cámara y la conexión con el carro ─────────────────────────────── */

#define RECURSOS_SESION 2   /* 0 = video de la cámara, 1 = conexión (WebSocket) con el carro */

/* Las sesiones solo las toca el hilo principal de la app (los avisos de VideoCarro pasan por el manejador principal). */
static Sesion sesiones[RECURSOS_SESION];
static int sesiones_iniciadas = 0;

static Sesion *sesion_de(jint recurso)
{
    int i;

    if (!sesiones_iniciadas) {
        for (i = 0; i < RECURSOS_SESION; i++) {
            ses_iniciar(&sesiones[i]);
        }
        sesiones_iniciadas = 1;
    }
    return (recurso >= 0 && recurso < RECURSOS_SESION) ? &sesiones[recurso] : NULL;
}

/* Entra un usuario al recurso. Devuelve la acción (AccionSesion): 0 nada, 1 abrir, 2 cerrar, 3 reabrir. */
JNIEXPORT jint JNICALL
Java_com_fsi_carro_SesionNativa_entrar(JNIEnv *env, jobject objeto, jint recurso, jint usuario, jlong ahora_ms)
{
    Sesion *s = sesion_de(recurso);

    (void)env;
    (void)objeto;
    return s ? (jint)ses_entrar(s, usuario, (uint32_t)ahora_ms) : 0;
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_SesionNativa_salir(JNIEnv *env, jobject objeto, jint recurso, jint usuario, jlong ahora_ms)
{
    Sesion *s = sesion_de(recurso);

    (void)env;
    (void)objeto;
    return s ? (jint)ses_salir(s, usuario, (uint32_t)ahora_ms) : 0;
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_SesionNativa_tick(JNIEnv *env, jobject objeto, jint recurso, jlong ahora_ms)
{
    Sesion *s = sesion_de(recurso);

    (void)env;
    (void)objeto;
    return s ? (jint)ses_tick(s, (uint32_t)ahora_ms) : 0;
}

JNIEXPORT void JNICALL
Java_com_fsi_carro_SesionNativa_funciona(JNIEnv *env, jobject objeto, jint recurso)
{
    Sesion *s = sesion_de(recurso);

    (void)env;
    (void)objeto;
    if (s) {
        ses_funciona(s);
    }
}

JNIEXPORT void JNICALL
Java_com_fsi_carro_SesionNativa_fallo(JNIEnv *env, jobject objeto, jint recurso, jlong ahora_ms)
{
    Sesion *s = sesion_de(recurso);

    (void)env;
    (void)objeto;
    if (s) {
        ses_fallo(s, (uint32_t)ahora_ms);
    }
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_SesionNativa_reintentar(JNIEnv *env, jobject objeto, jint recurso, jlong ahora_ms)
{
    Sesion *s = sesion_de(recurso);

    (void)env;
    (void)objeto;
    return s ? (jint)ses_reintentar(s, (uint32_t)ahora_ms) : 0;
}

/* Estado de la sesion: [0] EstadoSesion, [1] cuántos usuarios, [2] intentos de apertura, [3] fallos seguidos. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_SesionNativa_estado(JNIEnv *env, jobject objeto, jint recurso)
{
    Sesion *s = sesion_de(recurso);
    jint salida[4] = {0, 0, 0, 0};
    jintArray arreglo = (*env)->NewIntArray(env, 4);

    (void)objeto;
    if (s) {
        salida[0] = (jint)s->estado;
        salida[1] = ses_cuenta_usuarios(s);
        salida[2] = s->intentos;
        salida[3] = s->fallos;
    }
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 4, salida);
    }
    return arreglo;
}

/* Deja las sesiones como recien creadas (lo usa la prueba de la app). */
JNIEXPORT void JNICALL
Java_com_fsi_carro_SesionNativa_reiniciar(JNIEnv *env, jobject objeto)
{
    int i;

    (void)env;
    (void)objeto;
    sesiones_iniciadas = 0;
    for (i = 0; i < RECURSOS_SESION; i++) {
        ses_iniciar(&sesiones[i]);
    }
    sesiones_iniciadas = 1;
}

/* ── Latencia del control con y sin video (latencia_video.c) ───────────────────────────────────────────────────────────────── */

static MedidaVideo medida_video;

JNIEXPORT void JNICALL
Java_com_fsi_carro_LatenciaVideoNativa_iniciar(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    lv_iniciar(&medida_video);
}

JNIEXPORT void JNICALL
Java_com_fsi_carro_LatenciaVideoNativa_muestra(JNIEnv *env, jobject objeto, jint con_video, jint pong_ms)
{
    (void)env;
    (void)objeto;
    lv_muestra(&medida_video, con_video, pong_ms);
}

/* Resultado: [0] promedio sin video x10, [1] promedio con video x10, [2] maximo sin, [3] maximo con, [4] diferencia x10, [5] veredicto (-1, 0 o 1). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_LatenciaVideoNativa_resultado(JNIEnv *env, jobject objeto)
{
    ResultadoVideo r = lv_resultado(&medida_video);
    jint salida[6];
    jintArray arreglo = (*env)->NewIntArray(env, 6);

    (void)objeto;
    salida[0] = r.prom_sin_x10;
    salida[1] = r.prom_con_x10;
    salida[2] = r.max_sin;
    salida[3] = r.max_con;
    salida[4] = r.diferencia_x10;
    salida[5] = (jint)r.veredicto;
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 6, salida);
    }
    return arreglo;
}

/* ── Editor de rutas (ruta_editor.c) y simulador de rutas (sim_ruta.c) ─────────────────────────────────────────────────────── */

/* UN editor para toda la app (la ruta que se está dibujando) y UN resultado de simulación. Solo los toca el hilo principal. */
static EditorRuta editor_global;
static int editor_iniciado = 0;
static ResultadoSimRuta simulacion_global;
static DispersionSim dispersion_global;

static EditorRuta *obtener_editor(void)
{
    if (!editor_iniciado) {
        re_iniciar(&editor_global);
        editor_iniciado = 1;
    }
    return &editor_global;
}

JNIEXPORT void JNICALL
Java_com_fsi_carro_RutaEditorNativa_iniciar(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    re_iniciar(obtener_editor());
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_agregar(JNIEnv *env, jobject objeto, jint columna, jint fila)
{
    (void)env;
    (void)objeto;
    return re_agregar(obtener_editor(), columna, fila);
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_mover(JNIEnv *env, jobject objeto, jint indice, jint columna, jint fila)
{
    (void)env;
    (void)objeto;
    return re_mover(obtener_editor(), indice, columna, fila);
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_borrarPunto(JNIEnv *env, jobject objeto, jint indice)
{
    (void)env;
    (void)objeto;
    return re_borrar_punto(obtener_editor(), indice);
}

JNIEXPORT void JNICALL
Java_com_fsi_carro_RutaEditorNativa_finArrastre(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    re_fin_arrastre(obtener_editor());
}

JNIEXPORT void JNICALL
Java_com_fsi_carro_RutaEditorNativa_limpiar(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    re_limpiar(obtener_editor());
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_deshacer(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    return re_deshacer(obtener_editor());
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_rehacer(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    return re_rehacer(obtener_editor());
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_puedeDeshacer(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    return re_puede_deshacer(obtener_editor());
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_puedeRehacer(JNIEnv *env, jobject objeto)
{
    (void)env;
    (void)objeto;
    return re_puede_rehacer(obtener_editor());
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_plantilla(JNIEnv *env, jobject objeto, jint plantilla)
{
    (void)env;
    (void)objeto;
    return re_plantilla(obtener_editor(), plantilla);
}

/* Los vértices que puso el usuario: [columna0, fila0, columna1, fila1, ...]. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_RutaEditorNativa_vertices(JNIEnv *env, jobject objeto)
{
    const EditorRuta *e = obtener_editor();
    jint salida[2 * RE_MAX_VERTICES];
    jintArray arreglo = (*env)->NewIntArray(env, 2 * e->actual.n);
    int i;

    (void)objeto;
    for (i = 0; i < e->actual.n; i++) {
        salida[2 * i] = e->actual.v[i].columna;
        salida[2 * i + 1] = e->actual.v[i].fila;
    }
    if (arreglo != NULL && e->actual.n > 0) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 2 * e->actual.n, salida);
    }
    return arreglo;
}

/* La ruta ya con los pasos intermedios (la que entiende el navegador): [columna0, fila0, ...]. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_RutaEditorNativa_celdas(JNIEnv *env, jobject objeto)
{
    Punto celdas[MAX_PUNTOS];
    jint salida[2 * MAX_PUNTOS];
    int n = re_expandir(&obtener_editor()->actual, celdas, MAX_PUNTOS);
    jintArray arreglo;
    int i;

    (void)objeto;
    if (n < 0) {
        n = 0;
    }
    arreglo = (*env)->NewIntArray(env, 2 * n);
    for (i = 0; i < n; i++) {
        salida[2 * i] = celdas[i].columna;
        salida[2 * i + 1] = celdas[i].fila;
    }
    if (arreglo != NULL && n > 0) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 2 * n, salida);
    }
    return arreglo;
}

/* Resumen: [0] largo cm, [1] tiempo ms, [2] giros, [3] vértices, [4] celdas, [5] advertencias, [6] código. `calibracion` puede ser null (valores por defecto). */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_RutaEditorNativa_resumen(JNIEnv *env, jobject objeto, jintArray calibracion)
{
    CalibracionCarro cal;
    int hay_cal = calibracion != NULL && leer_calibracion(env, calibracion, &cal) && cal_es_valida(&cal);
    ResumenRuta r = re_resumen(obtener_editor(), hay_cal ? &cal : NULL);
    jint salida[7];
    jintArray arreglo = (*env)->NewIntArray(env, 7);

    (void)objeto;
    salida[0] = r.largo_cm;
    salida[1] = r.tiempo_ms;
    salida[2] = r.giros;
    salida[3] = r.n_vertices;
    salida[4] = r.n_celdas;
    salida[5] = r.advertencias;
    salida[6] = r.codigo;
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 7, salida);
    }
    return arreglo;
}

JNIEXPORT jstring JNICALL
Java_com_fsi_carro_RutaEditorNativa_aTexto(JNIEnv *env, jobject objeto)
{
    char texto[512];

    (void)objeto;
    if (re_a_texto(obtener_editor(), texto, sizeof texto) < 0) {
        texto[0] = '\0';
    }
    return (*env)->NewStringUTF(env, texto);
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_deTexto(JNIEnv *env, jobject objeto, jstring texto)
{
    const char *c;
    int codigo;

    (void)objeto;
    if (texto == NULL) {
        return RE_ERR_ARGUMENTO;
    }
    c = (*env)->GetStringUTFChars(env, texto, NULL);
    if (c == NULL) {
        return RE_ERR_ARGUMENTO;
    }
    codigo = re_de_texto(c, obtener_editor());
    (*env)->ReleaseStringUTFChars(env, texto, c);
    return codigo;
}

JNIEXPORT jint JNICALL
Java_com_fsi_carro_RutaEditorNativa_nombreValido(JNIEnv *env, jobject objeto, jstring nombre)
{
    const char *c;
    int valido;

    (void)objeto;
    if (nombre == NULL) {
        return 0;
    }
    c = (*env)->GetStringUTFChars(env, nombre, NULL);
    if (c == NULL) {
        return 0;
    }
    valido = re_nombre_valido(c);
    (*env)->ReleaseStringUTFChars(env, nombre, c);
    return valido;
}

JNIEXPORT jstring JNICALL
Java_com_fsi_carro_RutaEditorNativa_textoError(JNIEnv *env, jobject objeto, jint codigo)
{
    (void)objeto;
    return (*env)->NewStringUTF(env, re_texto_error(codigo));
}

/* Lee los obstáculos de Kotlin: [columna0, fila0, ...]. Devuelve cuántos. */
static int leer_obstaculos(JNIEnv *env, jintArray arreglo, Punto *obstaculos)
{
    jint datos[2 * SR_MAX_OBSTACULOS];
    int n = 0;
    int i;

    if (arreglo == NULL) {
        return 0;
    }
    n = (int)(*env)->GetArrayLength(env, arreglo) / 2;
    if (n > SR_MAX_OBSTACULOS) {
        n = SR_MAX_OBSTACULOS;
    }
    if (n > 0) {
        (*env)->GetIntArrayRegion(env, arreglo, 0, 2 * n, datos);
    }
    for (i = 0; i < n; i++) {
        obstaculos[i].columna = (int)datos[2 * i];
        obstaculos[i].fila = (int)datos[2 * i + 1];
    }
    return n;
}

#define ENTEROS_SIMULACION 14

/*
 * Simula la ruta del editor con el navegador real. Devuelve 14 enteros:
 * [0] código, [1] código de la ruta, [2] llegó, [3] estado final, [4] motivo del error, [5] duración ms, [6] comandos, [7] esquivadas, [8] advertencias,
 * [9] x final mm, [10] y final mm, [11] error final mm, [12] muestras, [13] cambios de comando.
 */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_SimRutaNativa_simular(JNIEnv *env, jobject objeto, jintArray calibracion, jintArray obstaculos, jint ruido, jint semilla)
{
    CalibracionCarro cal;
    Punto obst[SR_MAX_OBSTACULOS];
    Punto celdas[MAX_PUNTOS];
    int hay_cal = calibracion != NULL && leer_calibracion(env, calibracion, &cal) && cal_es_valida(&cal);
    int n_obst = leer_obstaculos(env, obstaculos, obst);
    int n = re_expandir(&obtener_editor()->actual, celdas, MAX_PUNTOS);
    int codigo;
    jint salida[ENTEROS_SIMULACION];
    jintArray arreglo = (*env)->NewIntArray(env, ENTEROS_SIMULACION);
    ResultadoSimRuta *r = &simulacion_global;

    (void)objeto;
    codigo = sr_simular(celdas, n < 0 ? 0 : n, hay_cal ? &cal : NULL, obst, n_obst, ruido, (uint32_t)semilla, r);
    memset(salida, 0, sizeof salida);
    salida[0] = codigo;
    if (codigo == SR_OK || codigo == SR_ERR_RUTA) {
        salida[1] = r->codigo_ruta;
        salida[2] = r->llego;
        salida[3] = r->estado_final;
        salida[4] = r->motivo_error;
        salida[5] = (jint)r->duracion_ms;
        salida[6] = r->n_comandos;
        salida[7] = r->esquivadas;
        salida[8] = r->advertencias;
        salida[9] = r->x_final_mm;
        salida[10] = r->y_final_mm;
        salida[11] = r->error_final_mm;
        salida[12] = r->n_muestras;
        salida[13] = r->n_cambios;
    }
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_SIMULACION, salida);
    }
    return arreglo;
}

/* Las muestras de la última simulación: [t_ms, x_mm, y_mm, rumbo, comando, estado] por cada una. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_SimRutaNativa_muestras(JNIEnv *env, jobject objeto)
{
    const ResultadoSimRuta *r = &simulacion_global;
    static jint salida[6 * SR_MAX_MUESTRAS];   /* memoria estática: sin malloc */
    jintArray arreglo = (*env)->NewIntArray(env, 6 * r->n_muestras);
    int i;

    (void)objeto;
    for (i = 0; i < r->n_muestras; i++) {
        salida[6 * i] = (jint)r->muestras[i].t_ms;
        salida[6 * i + 1] = r->muestras[i].x_mm;
        salida[6 * i + 2] = r->muestras[i].y_mm;
        salida[6 * i + 3] = r->muestras[i].rumbo_grados;
        salida[6 * i + 4] = r->muestras[i].comando;
        salida[6 * i + 5] = r->muestras[i].estado;
    }
    if (arreglo != NULL && r->n_muestras > 0) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 6 * r->n_muestras, salida);
    }
    return arreglo;
}

/* Los cambios de comando de la última simulación: [t_ms, comando] por cada uno. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_SimRutaNativa_cambios(JNIEnv *env, jobject objeto)
{
    const ResultadoSimRuta *r = &simulacion_global;
    jint salida[2 * SR_MAX_CAMBIOS];
    jintArray arreglo = (*env)->NewIntArray(env, 2 * r->n_cambios);
    int i;

    (void)objeto;
    for (i = 0; i < r->n_cambios; i++) {
        salida[2 * i] = (jint)r->cambios[i].t_ms;
        salida[2 * i + 1] = r->cambios[i].comando;
    }
    if (arreglo != NULL && r->n_cambios > 0) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 2 * r->n_cambios, salida);
    }
    return arreglo;
}

/* Repite la ruta con ruido de giro: [código, corridas, llegaron, error medio mm, error máximo mm, dispersión mm] y luego [llegó, x, y, error, duración ms] por corrida. */
JNIEXPORT jintArray JNICALL
Java_com_fsi_carro_SimRutaNativa_dispersion(JNIEnv *env, jobject objeto, jintArray calibracion, jintArray obstaculos, jint ruido, jint corridas)
{
    CalibracionCarro cal;
    Punto obst[SR_MAX_OBSTACULOS];
    Punto celdas[MAX_PUNTOS];
    int hay_cal = calibracion != NULL && leer_calibracion(env, calibracion, &cal) && cal_es_valida(&cal);
    int n_obst = leer_obstaculos(env, obstaculos, obst);
    int n = re_expandir(&obtener_editor()->actual, celdas, MAX_PUNTOS);
    int codigo = sr_dispersion(celdas, n < 0 ? 0 : n, hay_cal ? &cal : NULL, obst, n_obst, ruido, corridas, &dispersion_global);
    jint salida[6 + 5 * SR_MAX_CORRIDAS];
    jintArray arreglo;
    int i;

    (void)objeto;
    memset(salida, 0, sizeof salida);
    salida[0] = codigo;
    if (codigo == SR_OK) {
        salida[1] = dispersion_global.n;
        salida[2] = dispersion_global.llegaron;
        salida[3] = dispersion_global.error_medio_mm;
        salida[4] = dispersion_global.error_max_mm;
        salida[5] = dispersion_global.dispersion_mm;
        for (i = 0; i < dispersion_global.n; i++) {
            salida[6 + 5 * i] = dispersion_global.corridas[i].llego;
            salida[7 + 5 * i] = dispersion_global.corridas[i].x_final_mm;
            salida[8 + 5 * i] = dispersion_global.corridas[i].y_final_mm;
            salida[9 + 5 * i] = dispersion_global.corridas[i].error_final_mm;
            salida[10 + 5 * i] = (jint)dispersion_global.corridas[i].duracion_ms;
        }
    }
    arreglo = (*env)->NewIntArray(env, 6 + 5 * (codigo == SR_OK ? dispersion_global.n : 0));
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, 6 + 5 * (codigo == SR_OK ? dispersion_global.n : 0), salida);
    }
    return arreglo;
}

/* Por qué no llegó la última simulación, en una frase ("El obstáculo bloqueó el camino y se agotaron los 2 reintentos."), o "" si llegó o no hay causa. */
JNIEXPORT jstring JNICALL
Java_com_fsi_carro_SimRutaNativa_textoCausa(JNIEnv *env, jobject objeto)
{
    char texto[200];

    (void)objeto;
    if (sr_texto_causa(&simulacion_global, texto, sizeof texto) == 0) {
        texto[0] = '\0';
    }
    return (*env)->NewStringUTF(env, texto);
}

JNIEXPORT jstring JNICALL
Java_com_fsi_carro_SimRutaNativa_textoAdvertencia(JNIEnv *env, jobject objeto, jint bit)
{
    char texto[160];

    (void)objeto;
    if (sr_texto_advertencia(bit, texto, sizeof texto) == 0) {
        texto[0] = '\0';
    }
    return (*env)->NewStringUTF(env, texto);
}
