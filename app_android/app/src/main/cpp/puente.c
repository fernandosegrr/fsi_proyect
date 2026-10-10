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
 *     piso (obstaculos.c): recibe la imagen RGB de 320x240 y devuelve 10 enteros.
 *  Todo con arreglos de tamaño fijo: no hay malloc.
 */

#include <jni.h>
#include <stdarg.h>
#include <stdio.h>
#include "trayectoria.h"
#include "obstaculos.h"

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

/* Analiza una imagen RGB de 320x240 (320*240*3 bytes). Devuelve 10 enteros:
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
    int codigo;
    int i;

    (void)objeto;
    for (i = 0; i < ENTEROS_RESULTADO_OBSTACULOS; i++) {
        salida[i] = 0;
    }

    if (rgb == NULL) {
        codigo = OBS_ERR_IMAGEN_NULA;
    } else if ((*env)->GetArrayLength(env, rgb) < OBS_ANCHO * OBS_ALTO * 3) {
        codigo = OBS_ERR_TAMANO;   /* el arreglo es más chico que una imagen de 320x240 */
    } else {
        /* Acceso directo al arreglo (sin copiarlo) mientras se analiza. */
        void *datos = (*env)->GetPrimitiveArrayCritical(env, rgb, NULL);

        if (datos == NULL) {
            codigo = OBS_ERR_IMAGEN_NULA;
        } else {
            codigo = obstaculos_analizar((const unsigned char *)datos, ancho, alto, &resultado);
            (*env)->ReleasePrimitiveArrayCritical(env, rgb, datos, JNI_ABORT);   /* no se modificó */
        }
    }

    salida[0] = codigo;
    if (codigo == OBS_OK) {
        for (i = 0; i < OBS_ZONAS; i++) {
            salida[1 + 2 * i] = resultado.zona[i].hay_obstaculo;
            salida[2 + 2 * i] = resultado.zona[i].cercania;
        }
        salida[7] = resultado.ref_r;
        salida[8] = resultado.ref_g;
        salida[9] = resultado.ref_b;
    }

    arreglo = (*env)->NewIntArray(env, ENTEROS_RESULTADO_OBSTACULOS);
    if (arreglo != NULL) {
        (*env)->SetIntArrayRegion(env, arreglo, 0, ENTEROS_RESULTADO_OBSTACULOS, salida);
    }
    return arreglo;
}
