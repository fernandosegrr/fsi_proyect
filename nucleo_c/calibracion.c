/*
 * calibracion.c — Funciones puras de calibración del carro (ver calibracion.h).
 *
 * GUÍA DE LECTURA
 *  1. Estadística: cal_promedio, cal_mediana y cal_promedio_sin_atipicos (descarta los datos
 *     que se alejan de la mediana, por ejemplo una medición mal anotada).
 *  2. cal_defecto / cal_es_valida / cal_faltantes: el estado de la calibración.
 *  3. cal_ajustar_avance y cal_ajustar_giro: con DOS pruebas (dos tiempos distintos y lo que
 *     avanzó o giró el carro) calculan los ms por cm (o por grado) como la pendiente entre los
 *     dos puntos y, con lo que sobra, el frenado. Así el arranque y el frenado no ensucian la
 *     pendiente. El modelo es:  distancia = (tiempo - arranque) * velocidad + frenado.
 *  4. Conversiones: cal_cm_a_ms y cal_grados_a_ms (para ordenar), cal_ms_a_mm y cal_ms_a_grados
 *     (para estimar dónde está el carro).
 *  5. Texto: cal_a_texto y cal_de_texto (exportar e importar).
 *  Solo enteros, arreglos de tamaño fijo, sin malloc ni recursión.
 */

#include <stddef.h>
#include "calibracion.h"

/* Rangos razonables (ver cal_es_valida). */
#define ARRANQUE_MAX_MS        2000
#define MS_CM_X10_MIN            20
#define MS_CM_X10_MAX          5000
#define MS_GRADO_X10_MIN         10
#define MS_GRADO_X10_MAX       2000
#define FRENADO_MM_MAX          500
#define FRENADO_GRADOS_X10_MAX 1800
#define FRENADO_GRADOS_AJUSTE_MAX_X10 200   /* al MEDIR un giro: más de 20 grados de giro tras el STOP no es inercia, es un ajuste malo (se midió 38.9 a la izquierda y 0 a la derecha
                                             * con el mismo carro: un giro de 45 se ordenaba por 82 ms y la pose estimada sumaba 39 grados que no ocurrieron). Se rechaza: hay que repetir la medición */
#define FOV_X10_MIN             200
#define FOV_X10_MAX            1500
#define FOV_X10_DEFECTO         600

/* ── Utilidades ────────────────────────────────────────────────────────── */

static int valor_absoluto(int x)
{
    return (x < 0) ? -x : x;
}

static int en_rango(int valor, int minimo, int maximo)
{
    return valor >= minimo && valor <= maximo;
}

/* División entera redondeada al más cercano (para numerador y denominador positivos). */
static long dividir_redondeado(long numerador, long denominador)
{
    return (numerador + denominador / 2) / denominador;
}

/* ── Estadística ───────────────────────────────────────────────────────── */

int cal_promedio(const int *valores, int n)
{
    long suma = 0;
    int i;

    if (valores == NULL || n < 1 || n > CAL_MAX_MUESTRAS) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        suma += valores[i];
    }
    /* Redondeo al más cercano, también con suma negativa. */
    if (suma >= 0) {
        return (int)((suma + n / 2) / n);
    }
    return (int)-((-suma + n / 2) / n);
}

/* Copia los valores a `orden` y los ordena de menor a mayor (inserción: son pocos). */
static void ordenar_copia(const int *valores, int n, int *orden)
{
    int i, j;

    for (i = 0; i < n; i++) {
        int valor = valores[i];

        for (j = i; j > 0 && orden[j - 1] > valor; j--) {
            orden[j] = orden[j - 1];
        }
        orden[j] = valor;
    }
}

int cal_mediana(const int *valores, int n)
{
    int orden[CAL_MAX_MUESTRAS];

    if (valores == NULL || n < 1 || n > CAL_MAX_MUESTRAS) {
        return 0;
    }
    ordenar_copia(valores, n, orden);
    if (n % 2 == 1) {
        return orden[n / 2];
    }
    return (orden[n / 2 - 1] + orden[n / 2] + 1) / 2;
}

int cal_promedio_sin_atipicos(const int *valores, int n, int *usados)
{
    int buenos[CAL_MAX_MUESTRAS];
    int cuantos = 0;
    int mediana, tolerancia, i;

    if (usados != NULL) {
        *usados = 0;
    }
    if (valores == NULL || n < 1 || n > CAL_MAX_MUESTRAS) {
        return 0;
    }
    if (n <= 2) {                       /* con 1 o 2 datos no hay cómo decir cuál es el raro */
        if (usados != NULL) {
            *usados = n;
        }
        return cal_promedio(valores, n);
    }
    mediana = cal_mediana(valores, n);
    tolerancia = valor_absoluto(mediana) * CAL_ATIPICO_PCT / 100;
    if (tolerancia < 1) {
        tolerancia = 1;
    }
    for (i = 0; i < n; i++) {
        if (valor_absoluto(valores[i] - mediana) <= tolerancia) {
            buenos[cuantos++] = valores[i];
        }
    }
    if (usados != NULL) {
        *usados = cuantos;
    }
    return cal_promedio(buenos, cuantos);   /* la mediana siempre cumple: cuantos >= 1 */
}

/* ── Valores por defecto y validación ──────────────────────────────────── */

void cal_defecto(CalibracionCarro *cal)
{
    if (cal == NULL) {
        return;
    }
    cal->medidos = 0;
    cal->arranque_ms = 0;
    cal->ms_por_cm_x10 = MS_POR_CM * 10;
    cal->frenado_mm = 0;
    cal->ms_por_grado_izq_x10 = MS_POR_GRADO * 10;
    cal->ms_por_grado_der_x10 = MS_POR_GRADO * 10;
    cal->frenado_grados_izq_x10 = 0;
    cal->frenado_grados_der_x10 = 0;
    cal->fov_grados_x10 = FOV_X10_DEFECTO;
    cal->ms_por_cm_atras_x10 = MS_POR_CM * 10;
    cal->desp_adelante_izq_um = 0;   /* sin desplazamiento: el carro gira en su sitio, como antes */
    cal->desp_derecha_izq_um = 0;
    cal->desp_adelante_der_um = 0;
    cal->desp_derecha_der_um = 0;
}

int cal_es_valida(const CalibracionCarro *cal)
{
    if (cal == NULL) {
        return 0;
    }
    return en_rango(cal->medidos, 0, CAL_MEDIDO_TODO | CAL_MEDIDO_FOV | CAL_MEDIDO_ATRAS) &&
           en_rango(cal->arranque_ms, 0, ARRANQUE_MAX_MS) &&
           en_rango(cal->ms_por_cm_x10, MS_CM_X10_MIN, MS_CM_X10_MAX) &&
           en_rango(cal->frenado_mm, 0, FRENADO_MM_MAX) &&
           en_rango(cal->ms_por_grado_izq_x10, MS_GRADO_X10_MIN, MS_GRADO_X10_MAX) &&
           en_rango(cal->ms_por_grado_der_x10, MS_GRADO_X10_MIN, MS_GRADO_X10_MAX) &&
           en_rango(cal->frenado_grados_izq_x10, 0, FRENADO_GRADOS_X10_MAX) &&
           en_rango(cal->frenado_grados_der_x10, 0, FRENADO_GRADOS_X10_MAX) &&
           en_rango(cal->fov_grados_x10, FOV_X10_MIN, FOV_X10_MAX) &&
           en_rango(cal->ms_por_cm_atras_x10, MS_CM_X10_MIN, MS_CM_X10_MAX) &&
           en_rango(cal->desp_adelante_izq_um, -CAL_DESP_UM_MAX, CAL_DESP_UM_MAX) &&
           en_rango(cal->desp_derecha_izq_um, -CAL_DESP_UM_MAX, CAL_DESP_UM_MAX) &&
           en_rango(cal->desp_adelante_der_um, -CAL_DESP_UM_MAX, CAL_DESP_UM_MAX) &&
           en_rango(cal->desp_derecha_der_um, -CAL_DESP_UM_MAX, CAL_DESP_UM_MAX);
}

int cal_faltantes(const CalibracionCarro *cal)
{
    if (cal == NULL) {
        return CAL_MEDIDO_TODO;
    }
    return CAL_MEDIDO_TODO & ~cal->medidos;
}

/* ── Ajuste con los datos de las pruebas ───────────────────────────────── */

int cal_ajustar_fov(CalibracionCarro *cal, int real_x10, int visual_x10)
{
    long nuevo;

    if (cal == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    if (real_x10 <= 0 || visual_x10 <= 0) {
        return CAL_ERR_DATOS;
    }
    nuevo = dividir_redondeado((long)cal->fov_grados_x10 * real_x10, visual_x10);
    if (nuevo < FOV_X10_MIN || nuevo > FOV_X10_MAX) {
        return CAL_ERR_RANGO;
    }
    cal->fov_grados_x10 = (int)nuevo;
    cal->medidos |= CAL_MEDIDO_FOV;
    return CAL_OK;
}

int cal_poner_arranque(CalibracionCarro *cal, int arranque_ms)
{
    if (cal == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    if (!en_rango(arranque_ms, 0, ARRANQUE_MAX_MS)) {
        return CAL_ERR_RANGO;
    }
    cal->arranque_ms = arranque_ms;
    cal->medidos |= CAL_MEDIDO_ARRANQUE;
    return CAL_OK;
}

/* Ajusta la recta  valor = (tiempo - arranque) * velocidad + frenado  a n puntos (ms, valor) por
 * mínimos cuadrados, con enteros de 64 bits. Calcula:
 *   - la pendiente como "ms por unidad por 10" = ms / valor * 100 (sirve igual para ms por cm
 *     por 10 con el valor en mm, y para ms por grado por 10 con el valor en grados por 10);
 *   - el frenado: lo que la recta vale en el instante t = arranque (lo que el carro ya avanzó
 *     "sin haber empezado" a contar).
 * Devuelve CAL_OK o CAL_ERR_DATOS. */
static int ajustar_recta(int arranque_ms, int n, const int *ms, const int *valor,
                         int *pendiente_x10, int *frenado)
{
    long long suma_x = 0, suma_y = 0, suma_xx = 0, suma_xy = 0;
    long long sxx, sxy, numerador;
    int i;

    if (ms == NULL || valor == NULL || n < 2 || n > CAL_MAX_PUNTOS) {
        return CAL_ERR_ARGUMENTO;
    }
    for (i = 0; i < n; i++) {
        if (ms[i] <= arranque_ms || valor[i] < 0) {
            return CAL_ERR_DATOS;   /* cada prueba debe durar más que el arranque */
        }
        suma_x += ms[i];
        suma_y += valor[i];
        suma_xx += (long long)ms[i] * ms[i];
        suma_xy += (long long)ms[i] * valor[i];
    }
    sxx = n * suma_xx - suma_x * suma_x;     /* n por la suma de (x - promedio)^2 */
    sxy = n * suma_xy - suma_x * suma_y;     /* n por la suma de (x - prom)(y - prom) */
    if (sxx <= 0 || sxy <= 0) {
        return CAL_ERR_DATOS;   /* todos los tiempos iguales, o avanza menos con más tiempo */
    }
    *pendiente_x10 = (int)((100 * sxx + sxy / 2) / sxy);
    numerador = suma_y * sxx - sxy * suma_x + (long long)n * arranque_ms * sxy;
    numerador /= (long long)n * sxx;          /* valor de la recta en t = arranque */
    if (numerador < 0) {
        numerador = 0;   /* un frenado negativo no existe: el arranque medido era demasiado grande */
    }
    *frenado = (numerador > 1000000) ? 1000000 : (int)numerador;
    return CAL_OK;
}

int cal_ajustar_avance(CalibracionCarro *cal, int n, const int *ms, const int *mm)
{
    int pendiente, frenado, codigo;

    if (cal == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    codigo = ajustar_recta(cal->arranque_ms, n, ms, mm, &pendiente, &frenado);
    if (codigo != CAL_OK) {
        return codigo;
    }
    if (!en_rango(pendiente, MS_CM_X10_MIN, MS_CM_X10_MAX) || !en_rango(frenado, 0, FRENADO_MM_MAX)) {
        return CAL_ERR_RANGO;
    }
    cal->ms_por_cm_x10 = pendiente;
    cal->frenado_mm = frenado;
    cal->medidos |= CAL_MEDIDO_AVANCE;
    return CAL_OK;
}

int cal_ajustar_giro(CalibracionCarro *cal, int lado, int n, const int *ms, const int *grados_x10)
{
    int pendiente, frenado, codigo;

    if (cal == NULL || (lado != CAL_LADO_IZQUIERDA && lado != CAL_LADO_DERECHA)) {
        return CAL_ERR_ARGUMENTO;
    }
    codigo = ajustar_recta(cal->arranque_ms, n, ms, grados_x10, &pendiente, &frenado);
    if (codigo != CAL_OK) {
        return codigo;
    }
    if (!en_rango(pendiente, MS_GRADO_X10_MIN, MS_GRADO_X10_MAX) || !en_rango(frenado, 0, FRENADO_GRADOS_AJUSTE_MAX_X10)) {
        return CAL_ERR_RANGO;
    }
    if (lado == CAL_LADO_IZQUIERDA) {
        cal->ms_por_grado_izq_x10 = pendiente;
        cal->frenado_grados_izq_x10 = frenado;
        cal->medidos |= CAL_MEDIDO_GIRO_IZQ;
    } else {
        cal->ms_por_grado_der_x10 = pendiente;
        cal->frenado_grados_der_x10 = frenado;
        cal->medidos |= CAL_MEDIDO_GIRO_DER;
    }
    return CAL_OK;
}

int cal_ajustar_retroceso(CalibracionCarro *cal, int n, const int *ms, const int *mm)
{
    int pendiente, frenado, codigo;

    if (cal == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    codigo = ajustar_recta(cal->arranque_ms, n, ms, mm, &pendiente, &frenado);   /* el frenado que sale aquí se descarta: se usa el del avance */
    if (codigo != CAL_OK) {
        return codigo;
    }
    if (!en_rango(pendiente, MS_CM_X10_MIN, MS_CM_X10_MAX)) {
        return CAL_ERR_RANGO;
    }
    cal->ms_por_cm_atras_x10 = pendiente;
    cal->medidos |= CAL_MEDIDO_ATRAS;
    return CAL_OK;
}

/* ── Conversiones ──────────────────────────────────────────────────────── */

int cal_cm_a_ms_tramo(const CalibracionCarro *cal, int cm, int arranca, int frena)
{
    long mm;

    if (cal == NULL || cm <= 0) {
        return 0;
    }
    mm = (long)cm * 10 - (frena ? cal->frenado_mm : 0);   /* lo que el carro avanza solo al frenar no hay que ordenarlo */
    if (mm <= 0) {
        return 0;
    }
    return (arranca ? cal->arranque_ms : 0) + (int)dividir_redondeado(mm * cal->ms_por_cm_x10, 100);
}

int cal_cm_a_ms(const CalibracionCarro *cal, int cm)
{
    return cal_cm_a_ms_tramo(cal, cm, 1, 1);
}

int cal_retroceso_medido(const CalibracionCarro *cal)
{
    return cal != NULL && (cal->medidos & CAL_MEDIDO_ATRAS) != 0;
}

int cal_ms_por_cm_atras_x10(const CalibracionCarro *cal)
{
    if (cal == NULL) {
        return MS_POR_CM * 10;
    }
    return cal_retroceso_medido(cal) ? cal->ms_por_cm_atras_x10 : cal->ms_por_cm_x10;   /* sin medir: igual que avanzar */
}

int cal_cm_a_ms_atras(const CalibracionCarro *cal, int cm)
{
    long mm;

    if (cal == NULL || cm <= 0) {
        return 0;
    }
    mm = (long)cm * 10 - cal->frenado_mm;   /* lo que el carro sigue retrocediendo al frenar no hay que ordenarlo */
    if (mm <= 0) {
        return 0;
    }
    return cal->arranque_ms + (int)dividir_redondeado(mm * cal_ms_por_cm_atras_x10(cal), 100);
}

/* Datos de un lado: ms por grado (x10) y frenado (grados x10). */
static int ms_por_grado_x10(const CalibracionCarro *cal, int lado)
{
    return (lado < 0) ? cal->ms_por_grado_izq_x10 : cal->ms_por_grado_der_x10;
}

static int frenado_grados_x10(const CalibracionCarro *cal, int lado)
{
    return (lado < 0) ? cal->frenado_grados_izq_x10 : cal->frenado_grados_der_x10;
}

int cal_grados_a_ms(const CalibracionCarro *cal, int grados)
{
    int lado = (grados < 0) ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA;
    long efectivo_x10;

    if (cal == NULL || grados == 0) {
        return 0;
    }
    efectivo_x10 = (long)valor_absoluto(grados) * 10 - frenado_grados_x10(cal, lado);
    if (efectivo_x10 <= 0) {
        return 0;
    }
    return cal->arranque_ms + (int)dividir_redondeado(efectivo_x10 * ms_por_grado_x10(cal, lado), 100);
}

int cal_ms_a_mm(const CalibracionCarro *cal, int ms)
{
    if (cal == NULL || ms <= cal->arranque_ms) {
        return 0;
    }
    return (int)(((long)(ms - cal->arranque_ms) * 100) / cal->ms_por_cm_x10);
}

int cal_ms_a_mm_atras(const CalibracionCarro *cal, int ms)
{
    if (cal == NULL || ms <= cal->arranque_ms) {
        return 0;
    }
    return (int)(((long)(ms - cal->arranque_ms) * 100) / cal_ms_por_cm_atras_x10(cal));
}

int cal_ms_a_grados(const CalibracionCarro *cal, int lado, int ms)
{
    if (cal == NULL || ms <= cal->arranque_ms) {
        return 0;
    }
    return (int)(((long)(ms - cal->arranque_ms) * 10) / ms_por_grado_x10(cal, lado));
}

int cal_frenado_grados(const CalibracionCarro *cal, int lado)
{
    if (cal == NULL) {
        return 0;
    }
    return frenado_grados_x10(cal, lado) / 10;
}

/* ── Texto ─────────────────────────────────────────────────────────────── */

/* Nombres de las claves, en el orden en que se escriben. */
#define CLAVES 10                /* las 8 primeras son obligatorias; la 9 (campo de visión) y la 10 (ms por cm hacia atrás) son opcionales */
#define CLAVES_OBLIGATORIAS 8
static const char *const NOMBRES[CLAVES] = {
    "medidos", "arranque_ms", "ms_por_cm_x10", "frenado_mm",
    "ms_por_grado_izq_x10", "ms_por_grado_der_x10",
    "frenado_grados_izq_x10", "frenado_grados_der_x10",
    "fov_grados_x10", "ms_por_cm_atras_x10"
};

/* Apunta a cada campo de la estructura en el mismo orden que NOMBRES. */
static int *campo(CalibracionCarro *cal, int indice)
{
    switch (indice) {
    case 0: return &cal->medidos;
    case 1: return &cal->arranque_ms;
    case 2: return &cal->ms_por_cm_x10;
    case 3: return &cal->frenado_mm;
    case 4: return &cal->ms_por_grado_izq_x10;
    case 5: return &cal->ms_por_grado_der_x10;
    case 6: return &cal->frenado_grados_izq_x10;
    case 7: return &cal->frenado_grados_der_x10;
    case 8: return &cal->fov_grados_x10;
    default: return &cal->ms_por_cm_atras_x10;
    }
}

/* Agrega un texto al final del buffer. Devuelve 0 si no cupo. */
static int agregar_texto(char *texto, int capacidad, int *largo, const char *nuevo)
{
    while (*nuevo != '\0') {
        if (*largo >= capacidad - 1) {
            return 0;
        }
        texto[(*largo)++] = *nuevo++;
    }
    return 1;
}

/* Agrega un entero (con signo) en decimal. */
static int agregar_entero(char *texto, int capacidad, int *largo, int valor)
{
    char digitos[12];
    int n = 0;
    long v = valor;

    if (v < 0) {
        if (!agregar_texto(texto, capacidad, largo, "-")) {
            return 0;
        }
        v = -v;
    }
    do {
        digitos[n++] = (char)('0' + (int)(v % 10));
        v /= 10;
    } while (v > 0);
    while (n > 0) {
        char una[2];

        una[0] = digitos[--n];
        una[1] = '\0';
        if (!agregar_texto(texto, capacidad, largo, una)) {
            return 0;
        }
    }
    return 1;
}

int cal_a_texto(const CalibracionCarro *cal, char *texto, int capacidad)
{
    CalibracionCarro copia;
    int largo = 0;
    int i;

    if (cal == NULL || texto == NULL || capacidad < 2) {
        return CAL_ERR_ARGUMENTO;
    }
    copia = *cal;
    for (i = 0; i < CLAVES; i++) {
        if (!agregar_texto(texto, capacidad, &largo, NOMBRES[i]) ||
            !agregar_texto(texto, capacidad, &largo, "=") ||
            !agregar_entero(texto, capacidad, &largo, *campo(&copia, i)) ||
            !agregar_texto(texto, capacidad, &largo, "\n")) {
            texto[0] = '\0';
            return CAL_ERR_ARGUMENTO;
        }
    }
    texto[largo] = '\0';
    return largo;
}

/* ¿El texto `s` empieza con `clave` seguida de '='? Devuelve el puntero al valor o NULL. */
static const char *despues_de_clave(const char *s, const char *clave)
{
    while (*clave != '\0') {
        if (*s != *clave) {
            return NULL;
        }
        s++;
        clave++;
    }
    return (*s == '=') ? s + 1 : NULL;
}

/* Lee un entero (con signo, hasta 9 dígitos) y avanza el puntero. Devuelve 0 si no hay número. */
static int leer_entero(const char **s, int *valor)
{
    const char *p = *s;
    int negativo = 0;
    int digitos = 0;
    long resultado = 0;

    if (*p == '-') {
        negativo = 1;
        p++;
    }
    while (*p >= '0' && *p <= '9' && digitos < 9) {
        resultado = resultado * 10 + (*p - '0');
        digitos++;
        p++;
    }
    if (digitos == 0) {
        return 0;
    }
    *valor = (int)(negativo ? -resultado : resultado);
    *s = p;
    return 1;
}

int cal_de_texto(const char *texto, CalibracionCarro *cal)
{
    CalibracionCarro nueva;
    int vistas[CLAVES] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    const char *p = texto;
    int i;

    if (texto == NULL || cal == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    cal_defecto(&nueva);
    while (*p != '\0') {
        int valor = 0;
        int encontrada = -1;

        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (*p == '#') {                               /* comentario: se salta toda la línea */
            while (*p != '\0' && *p != '\n') {
                p++;
            }
            continue;
        }
        for (i = 0; i < CLAVES && encontrada < 0; i++) {
            const char *despues = despues_de_clave(p, NOMBRES[i]);

            if (despues != NULL) {
                encontrada = i;
                p = despues;
            }
        }
        if (encontrada < 0 || !leer_entero(&p, &valor) || vistas[encontrada]) {
            return CAL_ERR_TEXTO;   /* clave desconocida, sin número o repetida */
        }
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p != '\n' && *p != '\0') {
            return CAL_ERR_TEXTO;   /* basura después del número */
        }
        *campo(&nueva, encontrada) = valor;
        vistas[encontrada] = 1;
    }
    for (i = 0; i < CLAVES_OBLIGATORIAS; i++) {
        if (!vistas[i]) {
            return CAL_ERR_TEXTO;   /* falta una clave obligatoria */
        }
    }
    if (!cal_es_valida(&nueva)) {
        return CAL_ERR_TEXTO;
    }
    *cal = nueva;
    return CAL_OK;
}

/* ── Ajuste fino: el giro de 90° y la recta ────────────────────────────── */

int cal_grados_de_opcion_giro(int opcion)
{
    switch (opcion) {
    case CAL_GIRO_SE_PASO_MUCHO:   return 120;
    case CAL_GIRO_SE_PASO_UN_POCO: return 105;
    case CAL_GIRO_JUSTO:           return 90;
    case CAL_GIRO_QUEDO_CORTO:     return 78;
    default:                       return 0;
    }
}

int cal_factor_giro_nuevo(int factor_mil, int pedidos, int reales, int *nuevo_mil)
{
    long nuevo;

    if (nuevo_mil == NULL || !en_rango(factor_mil, CAL_FACTOR_GIRO_MIN, CAL_FACTOR_GIRO_MAX)) {
        return CAL_ERR_ARGUMENTO;
    }
    if (!en_rango(pedidos, 10, 360) || !en_rango(reales, 10, 360)) {
        return CAL_ERR_DATOS;
    }
    nuevo = dividir_redondeado((long)factor_mil * pedidos, reales);
    if (nuevo < CAL_FACTOR_GIRO_MIN) {
        nuevo = CAL_FACTOR_GIRO_MIN;
    }
    if (nuevo > CAL_FACTOR_GIRO_MAX) {
        nuevo = CAL_FACTOR_GIRO_MAX;
    }
    *nuevo_mil = (int)nuevo;
    return CAL_OK;
}

int cal_con_factor_giro(const CalibracionCarro *cal, int factor_izq_mil, int factor_der_mil, CalibracionCarro *salida)
{
    CalibracionCarro copia;

    if (cal == NULL || salida == NULL || !en_rango(factor_izq_mil, CAL_FACTOR_GIRO_MIN, CAL_FACTOR_GIRO_MAX) ||
        !en_rango(factor_der_mil, CAL_FACTOR_GIRO_MIN, CAL_FACTOR_GIRO_MAX)) {
        return CAL_ERR_ARGUMENTO;
    }
    copia = *cal;
    copia.ms_por_grado_izq_x10 = (int)dividir_redondeado((long)cal->ms_por_grado_izq_x10 * factor_izq_mil, 1000);
    copia.ms_por_grado_der_x10 = (int)dividir_redondeado((long)cal->ms_por_grado_der_x10 * factor_der_mil, 1000);
    if (!cal_es_valida(&copia)) {
        *salida = *cal;
        return CAL_ERR_RANGO;
    }
    *salida = copia;
    return CAL_OK;
}

int cal_balance_nuevo(int balance_mil, int desvio_mm, int recorrido_mm, int ejes_mm, int *nuevo_mil)
{
    long delta_mil, nuevo;
    int magnitud = valor_absoluto(desvio_mm);

    if (nuevo_mil == NULL || !en_rango(balance_mil, CAL_BALANCE_MIN, CAL_BALANCE_MAX) || !en_rango(ejes_mm, CAL_EJES_MM_MIN, CAL_EJES_MM_MAX)) {
        return CAL_ERR_ARGUMENTO;
    }
    if (recorrido_mm <= 0 || recorrido_mm > 5000 || magnitud * 2 > recorrido_mm) {
        return CAL_ERR_DATOS;
    }
    *nuevo_mil = balance_mil;
    if (magnitud <= CAL_RECTA_OK_MM) {
        return CAL_OK;
    }
    /* Ángulo ~ 2 x desvío / recorrido; diferencia relativa de velocidades = ángulo x ejes / recorrido  →  delta (en milésimas) = 2000 x desvío x ejes / recorrido^2. */
    delta_mil = dividir_redondeado(2000L * magnitud * ejes_mm, (long)recorrido_mm * recorrido_mm);
    if (delta_mil > 1000) {
        delta_mil = 1000;   /* nunca se corrige más de la mitad de la potencia en un solo paso */
    }
    if (desvio_mm > 0) {   /* se fue a la derecha: el lado izquierdo es más rápido: se baja su factor */
        nuevo = dividir_redondeado((long)balance_mil * (2000 - delta_mil), 2000 + delta_mil);
    } else {               /* se fue a la izquierda: el lado izquierdo es más lento: se sube su factor */
        nuevo = dividir_redondeado((long)balance_mil * (2000 + delta_mil), 2000 - delta_mil);
    }
    if (nuevo < CAL_BALANCE_MIN) {
        nuevo = CAL_BALANCE_MIN;
    }
    if (nuevo > CAL_BALANCE_MAX) {
        nuevo = CAL_BALANCE_MAX;
    }
    *nuevo_mil = (int)nuevo;
    return CAL_OK;
}

/* ── Calibración por partes ────────────────────────────────────────────── */

int cal_ajustar_velocidad(CalibracionCarro *cal, int n, const int *ms, const int *mm)
{
    int pendiente, frenado, codigo;

    if (cal == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    codigo = ajustar_recta(cal->arranque_ms, n, ms, mm, &pendiente, &frenado);   /* el frenado que sale aquí se descarta: se conserva el que ya había */
    if (codigo != CAL_OK) {
        return codigo;
    }
    if (!en_rango(pendiente, MS_CM_X10_MIN, MS_CM_X10_MAX)) {
        return CAL_ERR_RANGO;
    }
    cal->ms_por_cm_x10 = pendiente;
    cal->medidos |= CAL_MEDIDO_AVANCE;
    return CAL_OK;
}

int cal_ajustar_frenado(CalibracionCarro *cal, int ms, int mm)
{
    long sobrante;

    if (cal == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    if (ms <= cal->arranque_ms || mm < 0 || mm > 5000) {
        return CAL_ERR_DATOS;
    }
    sobrante = (long)mm - cal_ms_a_mm(cal, ms);   /* lo que avanzó de más sobre lo que la velocidad guardada decía */
    if (sobrante < -30) {
        return CAL_ERR_DATOS;                     /* avanzó mucho MENOS de lo esperado: la velocidad guardada ya no vale */
    }
    if (sobrante < 0) {
        sobrante = 0;
    }
    if (sobrante > FRENADO_MM_MAX) {
        return CAL_ERR_RANGO;
    }
    cal->frenado_mm = (int)sobrante;
    return CAL_OK;
}

int cal_ajustar_giro_fijo(CalibracionCarro *cal, int lado, int ms, int grados_x10)
{
    long girados_x10, pendiente, frenado_x10;

    if (cal == NULL || (lado != CAL_LADO_IZQUIERDA && lado != CAL_LADO_DERECHA)) {
        return CAL_ERR_ARGUMENTO;
    }
    if (ms <= cal->arranque_ms) {
        return CAL_ERR_DATOS;
    }
    frenado_x10 = frenado_grados_x10(cal, lado);
    if (frenado_x10 > FRENADO_GRADOS_AJUSTE_MAX_X10) {
        frenado_x10 = 0;   /* un frenado de giro de más de 20 grados guardado antes (se midió 38.9 con un carro que giraba bien) no es inercia sino un ajuste malo: al volver a medir ese giro se descarta */
    }
    girados_x10 = (long)grados_x10 - frenado_x10;   /* lo que giró mientras los motores empujaban (sin el frenado) */
    if (girados_x10 <= 0) {
        return CAL_ERR_DATOS;
    }
    pendiente = dividir_redondeado(100L * (ms - cal->arranque_ms), girados_x10);   /* ms por grado por 10 */
    if (pendiente < MS_GRADO_X10_MIN || pendiente > MS_GRADO_X10_MAX) {
        return CAL_ERR_RANGO;
    }
    if (lado == CAL_LADO_IZQUIERDA) {
        cal->ms_por_grado_izq_x10 = (int)pendiente;
        cal->frenado_grados_izq_x10 = (int)frenado_x10;
        cal->medidos |= CAL_MEDIDO_GIRO_IZQ;
    } else {
        cal->ms_por_grado_der_x10 = (int)pendiente;
        cal->frenado_grados_der_x10 = (int)frenado_x10;
        cal->medidos |= CAL_MEDIDO_GIRO_DER;
    }
    return CAL_OK;
}

int cal_validar_para_guardar(const CalibracionCarro *cal, int *lado_malo)
{
    if (lado_malo != NULL) {
        *lado_malo = 0;
    }
    if (cal == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    if (!cal_es_valida(cal)) {
        return CAL_ERR_RANGO;
    }
    if (!en_rango(cal->ms_por_grado_izq_x10, CAL_MS_GRADO_GUARDAR_MIN_X10, CAL_MS_GRADO_GUARDAR_MAX_X10)) {
        if (lado_malo != NULL) {
            *lado_malo = CAL_LADO_IZQUIERDA;
        }
        return CAL_ERR_GIRO_FUERA;
    }
    if (!en_rango(cal->ms_por_grado_der_x10, CAL_MS_GRADO_GUARDAR_MIN_X10, CAL_MS_GRADO_GUARDAR_MAX_X10)) {
        if (lado_malo != NULL) {
            *lado_malo = CAL_LADO_DERECHA;
        }
        return CAL_ERR_GIRO_FUERA;
    }
    return CAL_OK;
}

/* ── Desplazamiento del centro al girar ────────────────────────────────── */

/* Seno de 0 a 90 grados, de grado en grado, en milésimas (1000 = 1.0): con él se rota el vector sin usar math.h. */
static const int SENO_MIL[91] = {
       0,   17,   35,   52,   70,   87,  105,  122,  139,  156,  174,  191,  208,  225,  242,
     259,  276,  292,  309,  326,  342,  358,  375,  391,  407,  423,  438,  454,  469,  485,
     500,  515,  530,  545,  559,  574,  588,  602,  616,  629,  643,  656,  669,  682,  695,
     707,  719,  731,  743,  755,  766,  777,  788,  799,  809,  819,  829,  839,  848,  857,
     866,  875,  883,  891,  899,  906,  914,  921,  927,  934,  940,  946,  951,  956,  961,
     966,  970,  974,  978,  982,  985,  988,  990,  993,  995,  996,  998,  999,  999, 1000,
    1000
};

static int seno_mil(int grados)
{
    int g = ((grados % 360) + 360) % 360;

    if (g <= 90) {
        return SENO_MIL[g];
    }
    if (g <= 180) {
        return SENO_MIL[180 - g];
    }
    if (g <= 270) {
        return -SENO_MIL[g - 180];
    }
    return -SENO_MIL[360 - g];
}

static int coseno_mil(int grados)
{
    return seno_mil(grados + 90);
}

/* División entera redondeada al más cercano, también con numerador negativo (b > 0). */
static long dividir_con_signo(long a, long b)
{
    return (a >= 0) ? (a + b / 2) / b : -((-a + b / 2) / b);
}

/* Raíz cuadrada entera (hacia abajo). */
static long raiz_entera(long valor)
{
    long x = 0, paso = 1L << 30;

    if (valor <= 0) {
        return 0;
    }
    while (paso > valor) {
        paso >>= 2;
    }
    while (paso != 0) {
        if (valor >= x + paso) {
            valor -= x + paso;
            x = (x >> 1) + paso;
        } else {
            x >>= 1;
        }
        paso >>= 2;
    }
    return x;
}

static int lado_valido(int lado)
{
    return lado == CAL_LADO_IZQUIERDA || lado == CAL_LADO_DERECHA;
}

int cal_poner_desplazamiento(CalibracionCarro *cal, int lado, int adelante_um, int derecha_um)
{
    if (cal == NULL || !lado_valido(lado)) {
        return CAL_ERR_ARGUMENTO;
    }
    if (!en_rango(adelante_um, -CAL_DESP_UM_MAX, CAL_DESP_UM_MAX) || !en_rango(derecha_um, -CAL_DESP_UM_MAX, CAL_DESP_UM_MAX)) {
        return CAL_ERR_RANGO;
    }
    if (lado == CAL_LADO_IZQUIERDA) {
        cal->desp_adelante_izq_um = adelante_um;
        cal->desp_derecha_izq_um = derecha_um;
    } else {
        cal->desp_adelante_der_um = adelante_um;
        cal->desp_derecha_der_um = derecha_um;
    }
    return CAL_OK;
}

void cal_leer_desplazamiento(const CalibracionCarro *cal, int lado, int *adelante_um, int *derecha_um)
{
    int a = 0, d = 0;

    if (cal != NULL && lado == CAL_LADO_IZQUIERDA) {
        a = cal->desp_adelante_izq_um;
        d = cal->desp_derecha_izq_um;
    } else if (cal != NULL && lado == CAL_LADO_DERECHA) {
        a = cal->desp_adelante_der_um;
        d = cal->desp_derecha_der_um;
    }
    if (adelante_um != NULL) {
        *adelante_um = a;
    }
    if (derecha_um != NULL) {
        *derecha_um = d;
    }
}

int cal_desplazamiento_de_medida(int lado, int adelante_mm, int derecha_mm, int grados, int *adelante_um, int *derecha_um)
{
    int medio;
    long f_um, d_um, por_adelante, por_derecha;

    if (!lado_valido(lado) || adelante_um == NULL || derecha_um == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    if (!en_rango(grados, 10, 360) || !en_rango(adelante_mm, -1000, 1000) || !en_rango(derecha_mm, -1000, 1000)) {
        return CAL_ERR_DATOS;
    }
    /* El vector medido está en el marco de la posición inicial (adelante = f0, derecha = r0). En el marco del carro a mitad del giro (rumbo `medio`):
     *   adelante = r0 x sen(medio) + f0 x cos(medio)      derecha = r0 x cos(medio) - f0 x sen(medio)
     * (mm x milésimas / 1000 x 1000 = µm: por eso no hay que dividir). Después se reparte entre los grados girados. */
    medio = lado * (grados / 2);
    f_um = (long)derecha_mm * seno_mil(medio) + (long)adelante_mm * coseno_mil(medio);
    d_um = (long)derecha_mm * coseno_mil(medio) - (long)adelante_mm * seno_mil(medio);
    por_adelante = dividir_con_signo(f_um, grados);
    por_derecha = dividir_con_signo(d_um, grados);
    if (por_adelante < -CAL_DESP_UM_MAX || por_adelante > CAL_DESP_UM_MAX || por_derecha < -CAL_DESP_UM_MAX || por_derecha > CAL_DESP_UM_MAX ||
        raiz_entera(por_adelante * por_adelante + por_derecha * por_derecha) > CAL_DESP_TOTAL_UM_MAX) {
        return CAL_ERR_RANGO;
    }
    *adelante_um = (int)por_adelante;
    *derecha_um = (int)por_derecha;
    return CAL_OK;
}

/* µm por grado x grados = µm; entre 100 = décimas de mm. */
static int um_a_mm10(long um)
{
    return (int)dividir_con_signo(um, 100);
}

int cal_desplazamiento_arco(int grados, int adelante_mm, int derecha_mm, int *total_mm, int *radio_mm)
{
    long total, seno_medio_mil;

    if (total_mm == NULL || radio_mm == NULL) {
        return CAL_ERR_ARGUMENTO;
    }
    if (!en_rango(grados, 10, 360) || !en_rango(adelante_mm, -1000, 1000) || !en_rango(derecha_mm, -1000, 1000)) {
        return CAL_ERR_DATOS;
    }
    total = raiz_entera((long)adelante_mm * adelante_mm + (long)derecha_mm * derecha_mm);
    /* sen(grados/2) con los medios grados: el promedio de los senos de los dos grados enteros vecinos (45 grados da 22.5 y no 22). */
    seno_medio_mil = ((long)seno_mil(grados / 2) + seno_mil((grados + 1) / 2)) / 2;
    *total_mm = (int)total;
    *radio_mm = (seno_medio_mil > 0) ? (int)dividir_con_signo(total * 1000, 2 * seno_medio_mil) : 0;
    return CAL_OK;
}

int cal_desplazamiento_promediar(const int *adelante_um, const int *derecha_um, const int *grados, int n, int *prom_adelante_um, int *prom_derecha_um, CalDifDesp *detalle)
{
    int i, j;
    int hay_malo = 0;
    long peor_exceso = 0;
    CalDifDesp peor;

    if (adelante_um == NULL || derecha_um == NULL || grados == NULL || prom_adelante_um == NULL || prom_derecha_um == NULL || n < 2 || n > CAL_MAX_MUESTRAS) {
        return CAL_ERR_ARGUMENTO;
    }
    peor.i = peor.j = peor.mov_i_mm10 = peor.mov_j_mm10 = peor.dif_mm10 = peor.limite_mm10 = 0;
    for (i = 0; i < n; i++) {
        if (grados[i] <= 0) {
            return CAL_ERR_ARGUMENTO;
        }
    }
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n; j++) {
            long g = ((long)grados[i] + grados[j]) / 2;   /* el ángulo con el que se comparan: el medio de las dos */
            long df = ((long)adelante_um[i] - adelante_um[j]) * g;
            long dr = ((long)derecha_um[i] - derecha_um[j]) * g;
            long diferencia = raiz_entera(df * df + dr * dr);   /* µm */
            long mov_i = raiz_entera((long)adelante_um[i] * adelante_um[i] + (long)derecha_um[i] * derecha_um[i]) * g;
            long mov_j = raiz_entera((long)adelante_um[j] * adelante_um[j] + (long)derecha_um[j] * derecha_um[j]) * g;
            long mayor = (mov_i > mov_j) ? mov_i : mov_j;
            long limite = mayor * CAL_DESP_TOLERANCIA_PCT / 100;

            if (limite < (long)CAL_DESP_TOLERANCIA_MM * 1000) {
                limite = (long)CAL_DESP_TOLERANCIA_MM * 1000;
            }
            if (diferencia > limite && (!hay_malo || diferencia - limite > peor_exceso)) {
                hay_malo = 1;
                peor_exceso = diferencia - limite;
                peor.i = i + 1;
                peor.j = j + 1;
                peor.mov_i_mm10 = um_a_mm10(mov_i);
                peor.mov_j_mm10 = um_a_mm10(mov_j);
                peor.dif_mm10 = um_a_mm10(diferencia);
                peor.limite_mm10 = um_a_mm10(limite);
            }
        }
    }
    if (hay_malo) {
        if (detalle != NULL) {
            *detalle = peor;
        }
        return CAL_ERR_DIFIEREN;
    }
    *prom_adelante_um = cal_promedio(adelante_um, n);
    *prom_derecha_um = cal_promedio(derecha_um, n);
    return CAL_OK;
}

int cal_desplazamiento_promedio(const int *adelante_um, const int *derecha_um, int n, int *prom_adelante_um, int *prom_derecha_um)
{
    if (adelante_um == NULL || derecha_um == NULL || prom_adelante_um == NULL || prom_derecha_um == NULL || n < 1 || n > CAL_MAX_MUESTRAS) {
        return CAL_ERR_ARGUMENTO;
    }
    *prom_adelante_um = cal_promedio(adelante_um, n);
    *prom_derecha_um = cal_promedio(derecha_um, n);
    return CAL_OK;
}

void cal_desplazamiento_giro(const CalibracionCarro *cal, int lado, int grados, int rumbo_inicial,
                             int *dx_mm, int *dy_mm, int *adelante_mm, int *derecha_mm)
{
    int adelante_um = 0, derecha_um = 0, medio;
    long f_um, d_um, dx_um, dy_um, a_um, r_um;

    cal_leer_desplazamiento(cal, lado, &adelante_um, &derecha_um);
    if (grados < 0) {
        grados = -grados;
    }
    medio = rumbo_inicial + (lado_valido(lado) ? lado * (grados / 2) : 0);
    f_um = (long)adelante_um * grados;      /* µm que se movió hacia adelante y hacia la derecha, en el marco del carro a mitad del giro */
    d_um = (long)derecha_um * grados;
    /* Al mapa (x a la derecha, y hacia abajo; rumbo 0 = arriba): adelante = (sen, -cos), derecha = (cos, sen). */
    dx_um = (f_um * seno_mil(medio) + d_um * coseno_mil(medio)) / 1000;
    dy_um = (-f_um * coseno_mil(medio) + d_um * seno_mil(medio)) / 1000;
    /* Y en el marco del carro AL EMPEZAR el giro (rumbo_inicial), que es el que se mide con la cinta. */
    a_um = (dx_um * seno_mil(rumbo_inicial) - dy_um * coseno_mil(rumbo_inicial)) / 1000;
    r_um = (dx_um * coseno_mil(rumbo_inicial) + dy_um * seno_mil(rumbo_inicial)) / 1000;
    if (dx_mm != NULL) {
        *dx_mm = (int)dividir_con_signo(dx_um, 1000);
    }
    if (dy_mm != NULL) {
        *dy_mm = (int)dividir_con_signo(dy_um, 1000);
    }
    if (adelante_mm != NULL) {
        *adelante_mm = (int)dividir_con_signo(a_um, 1000);
    }
    if (derecha_mm != NULL) {
        *derecha_mm = (int)dividir_con_signo(r_um, 1000);
    }
}
