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
           en_rango(cal->ms_por_cm_atras_x10, MS_CM_X10_MIN, MS_CM_X10_MAX);
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
    if (!en_rango(pendiente, MS_GRADO_X10_MIN, MS_GRADO_X10_MAX) || !en_rango(frenado, 0, FRENADO_GRADOS_X10_MAX)) {
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
