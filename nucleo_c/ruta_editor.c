/*
 * ruta_editor.c — Ver ruta_editor.h para la idea general.
 *
 * GUÍA DE LECTURA
 *  - Todo cambio pasa por guardar_para_deshacer(): copia la ruta actual a la pila de «deshacer» y vaciar la de «rehacer».
 *  - Antes de aceptar un cambio se prueba con una COPIA de la ruta: si no cabe (fuera de la cuadrícula, repetido, demasiado larga) se devuelve el código y la ruta
 *    queda igual.
 */
#include <stddef.h>
#include "ruta_editor.h"

static int sgn(int x)
{
    return (x > 0) - (x < 0);
}

static int absoluto(int x)
{
    return x < 0 ? -x : x;
}

static int dentro(int columna, int fila)
{
    return columna >= 0 && columna < RE_COLUMNAS && fila >= 0 && fila < RE_FILAS;
}

/* ── Expandir: de vértices a celdas vecinas ───────────────────────────── */

int re_expandir(const RutaVertices *r, Punto *salida, int capacidad)
{
    int total = 0;
    int i;

    if (r == NULL || salida == NULL || r->n <= 0) {
        return 0;
    }
    if (capacidad < 1) {
        return RE_ERR_LARGA;
    }
    salida[total++] = r->v[0];
    for (i = 1; i < r->n; i++) {
        Punto p = r->v[i - 1];
        const Punto destino = r->v[i];

        while (p.columna != destino.columna || p.fila != destino.fila) {
            p.columna += sgn(destino.columna - p.columna);   /* los dos a la vez: primero diagonal y luego recto */
            p.fila += sgn(destino.fila - p.fila);
            if (total >= capacidad) {
                return RE_ERR_LARGA;
            }
            salida[total++] = p;
        }
    }
    return total;
}

/* true si la ruta completa cabe en MAX_PUNTOS celdas. */
static int cabe(const RutaVertices *r)
{
    Punto temporal[MAX_PUNTOS];

    return re_expandir(r, temporal, MAX_PUNTOS) >= 0;
}

/* ── Historial ─────────────────────────────────────────────────────────── */

void re_iniciar(EditorRuta *e)
{
    if (e == NULL) {
        return;
    }
    e->actual.n = 0;
    e->n_deshacer = 0;
    e->n_rehacer = 0;
}

static void guardar_para_deshacer(EditorRuta *e)
{
    int i;

    if (e->n_deshacer >= RE_MAX_HISTORIAL) {   /* se pierde el cambio más viejo */
        for (i = 1; i < RE_MAX_HISTORIAL; i++) {
            e->deshacer[i - 1] = e->deshacer[i];
        }
        e->n_deshacer = RE_MAX_HISTORIAL - 1;
    }
    e->deshacer[e->n_deshacer++] = e->actual;
    e->n_rehacer = 0;
}

int re_deshacer(EditorRuta *e)
{
    if (e == NULL || e->n_deshacer <= 0) {
        return RE_ERR_NADA;
    }
    if (e->n_rehacer < RE_MAX_HISTORIAL) {
        e->rehacer[e->n_rehacer++] = e->actual;
    }
    e->actual = e->deshacer[--e->n_deshacer];
    return RE_OK;
}

int re_rehacer(EditorRuta *e)
{
    if (e == NULL || e->n_rehacer <= 0) {
        return RE_ERR_NADA;
    }
    if (e->n_deshacer < RE_MAX_HISTORIAL) {
        e->deshacer[e->n_deshacer++] = e->actual;
    }
    e->actual = e->rehacer[--e->n_rehacer];
    return RE_OK;
}

int re_puede_deshacer(const EditorRuta *e)
{
    return e != NULL && e->n_deshacer > 0;
}

int re_puede_rehacer(const EditorRuta *e)
{
    return e != NULL && e->n_rehacer > 0;
}

/* ── Cambios ───────────────────────────────────────────────────────────── */

int re_agregar(EditorRuta *e, int columna, int fila)
{
    RutaVertices prueba;

    if (e == NULL) {
        return RE_ERR_ARGUMENTO;
    }
    if (!dentro(columna, fila)) {
        return RE_ERR_FUERA;
    }
    if (e->actual.n >= RE_MAX_VERTICES) {
        return RE_ERR_LLENO;
    }
    if (e->actual.n > 0 && e->actual.v[e->actual.n - 1].columna == columna && e->actual.v[e->actual.n - 1].fila == fila) {
        return RE_ERR_REPETIDO;
    }
    prueba = e->actual;
    prueba.v[prueba.n].columna = columna;
    prueba.v[prueba.n].fila = fila;
    prueba.n++;
    if (!cabe(&prueba)) {
        return RE_ERR_LARGA;
    }
    guardar_para_deshacer(e);
    e->actual = prueba;
    return RE_OK;
}

int re_mover(EditorRuta *e, int indice, int columna, int fila)
{
    RutaVertices prueba;

    if (e == NULL) {
        return RE_ERR_ARGUMENTO;
    }
    if (indice < 0 || indice >= e->actual.n) {
        return RE_ERR_INDICE;
    }
    if (!dentro(columna, fila)) {
        return RE_ERR_FUERA;
    }
    if (e->actual.v[indice].columna == columna && e->actual.v[indice].fila == fila) {
        return RE_OK;   /* no se movió de celda: no es un cambio */
    }
    if ((indice > 0 && e->actual.v[indice - 1].columna == columna && e->actual.v[indice - 1].fila == fila) ||
        (indice + 1 < e->actual.n && e->actual.v[indice + 1].columna == columna && e->actual.v[indice + 1].fila == fila)) {
        return RE_ERR_REPETIDO;
    }
    prueba = e->actual;
    prueba.v[indice].columna = columna;
    prueba.v[indice].fila = fila;
    if (!cabe(&prueba)) {
        return RE_ERR_LARGA;
    }
    guardar_para_deshacer(e);
    e->actual = prueba;
    return RE_OK;
}

int re_borrar_punto(EditorRuta *e, int indice)
{
    RutaVertices prueba;
    int i;
    int n = 0;

    if (e == NULL) {
        return RE_ERR_ARGUMENTO;
    }
    if (indice < 0 || indice >= e->actual.n) {
        return RE_ERR_INDICE;
    }
    for (i = 0; i < e->actual.n; i++) {   /* sin ese punto y sin que queden dos iguales seguidos */
        if (i == indice) {
            continue;
        }
        if (n > 0 && prueba.v[n - 1].columna == e->actual.v[i].columna && prueba.v[n - 1].fila == e->actual.v[i].fila) {
            continue;
        }
        prueba.v[n++] = e->actual.v[i];
    }
    prueba.n = n;
    guardar_para_deshacer(e);
    e->actual = prueba;
    return RE_OK;
}

void re_limpiar(EditorRuta *e)
{
    if (e == NULL || e->actual.n == 0) {
        return;
    }
    guardar_para_deshacer(e);
    e->actual.n = 0;
}

/* ── Plantillas ────────────────────────────────────────────────────────── */

/* Puntos de cada plantilla, relativos al primero (la fila crece hacia abajo). */
static const Punto P_RECTA[] = {{0, 0}, {0, -5}};
static const Punto P_CUADRADO[] = {{0, 0}, {0, -2}, {2, -2}, {2, 0}, {0, 0}};
static const Punto P_OCHO[] = {{2, 1}, {3, 0}, {4, 1}, {3, 2}, {2, 1}, {1, 0}, {0, 1}, {1, 2}, {2, 1}};
static const Punto P_ZIGZAG[] = {{0, 0}, {0, -1}, {1, -1}, {1, -2}, {2, -2}, {2, -3}, {3, -3}};

int re_plantilla(EditorRuta *e, int plantilla)
{
    const Punto *puntos;
    int cantidad;
    int minc, maxc, minf, maxf;
    int i, dc, df;
    RutaVertices nueva;

    if (e == NULL) {
        return RE_ERR_ARGUMENTO;
    }
    switch (plantilla) {
    case RE_PLANTILLA_RECTA:    puntos = P_RECTA;    cantidad = (int)(sizeof P_RECTA / sizeof P_RECTA[0]); break;
    case RE_PLANTILLA_CUADRADO: puntos = P_CUADRADO; cantidad = (int)(sizeof P_CUADRADO / sizeof P_CUADRADO[0]); break;
    case RE_PLANTILLA_OCHO:     puntos = P_OCHO;     cantidad = (int)(sizeof P_OCHO / sizeof P_OCHO[0]); break;
    case RE_PLANTILLA_ZIGZAG:   puntos = P_ZIGZAG;   cantidad = (int)(sizeof P_ZIGZAG / sizeof P_ZIGZAG[0]); break;
    default: return RE_ERR_ARGUMENTO;
    }
    minc = maxc = puntos[0].columna;
    minf = maxf = puntos[0].fila;
    for (i = 1; i < cantidad; i++) {
        if (puntos[i].columna < minc) minc = puntos[i].columna;
        if (puntos[i].columna > maxc) maxc = puntos[i].columna;
        if (puntos[i].fila < minf) minf = puntos[i].fila;
        if (puntos[i].fila > maxf) maxf = puntos[i].fila;
    }
    dc = (RE_COLUMNAS - (maxc - minc + 1)) / 2 - minc;   /* centrada a lo ancho */
    df = (RE_FILAS - 1) - maxf;                           /* pegada abajo */
    nueva.n = cantidad;
    for (i = 0; i < cantidad; i++) {
        nueva.v[i].columna = puntos[i].columna + dc;
        nueva.v[i].fila = puntos[i].fila + df;
    }
    guardar_para_deshacer(e);
    e->actual = nueva;
    return RE_OK;
}

/* ── Resumen ───────────────────────────────────────────────────────────── */

ResumenRuta re_resumen(const EditorRuta *e, const CalibracionCarro *cal)
{
    ResumenRuta r = {0, 0, 0, 0, 0, 0, 0};
    Punto celdas[MAX_PUNTOS];
    Paso pasos[MAX_PASOS];
    int n_pasos = 0;
    int i;

    if (e == NULL) {
        return r;
    }
    r.n_vertices = e->actual.n;
    r.n_celdas = re_expandir(&e->actual, celdas, MAX_PUNTOS);
    if (r.n_celdas < 0) {
        r.n_celdas = 0;
        r.codigo = TRAY_ERR_DEMASIADOS_PUNTOS;
        return r;
    }
    if (r.n_celdas < 2) {
        r.advertencias |= RE_ADV_POCOS_PUNTOS;
        return r;
    }
    r.codigo = trayectoria_calcular_desde_cal(cal, ORIENTACION_INICIAL, celdas, r.n_celdas, pasos, MAX_PASOS, &n_pasos, NULL);
    if (r.codigo != TRAY_OK) {
        return r;
    }
    for (i = 0; i < n_pasos; i++) {
        r.tiempo_ms += pasos[i].tiempo_ms;
        if (pasos[i].tipo == PASO_AVANZAR) {
            r.largo_cm += pasos[i].valor;
        } else {
            r.giros++;
            if (absoluto(pasos[i].valor) >= 135) {
                r.advertencias |= RE_ADV_GIRO_CERRADO;
            }
        }
    }
    return r;
}

/* ── Guardar y abrir ───────────────────────────────────────────────────── */

/* Escribe un entero (con signo) al final del texto. Devuelve el nuevo largo o -1 si no cabe. */
static int escribir_entero(char *texto, int largo, int capacidad, int valor)
{
    char digitos[12];
    int n = 0;
    int k;

    if (valor < 0) {
        if (largo + 1 >= capacidad) return -1;
        texto[largo++] = '-';
        valor = -valor;
    }
    do {
        digitos[n++] = (char)('0' + valor % 10);
        valor /= 10;
    } while (valor > 0);
    if (largo + n >= capacidad) {
        return -1;
    }
    for (k = n - 1; k >= 0; k--) {
        texto[largo++] = digitos[k];
    }
    return largo;
}

static int escribir_caracter(char *texto, int largo, int capacidad, char c)
{
    if (largo + 1 >= capacidad) {
        return -1;
    }
    texto[largo++] = c;
    return largo;
}

int re_a_texto(const EditorRuta *e, char *texto, int capacidad)
{
    int largo = 0;
    int i;
    const char *cabeza = "RUTA1;";

    if (e == NULL || texto == NULL || capacidad < 16) {
        return RE_ERR_ARGUMENTO;
    }
    for (i = 0; cabeza[i] != '\0'; i++) {
        texto[largo++] = cabeza[i];
    }
    if ((largo = escribir_entero(texto, largo, capacidad, RE_COLUMNAS)) < 0 || (largo = escribir_caracter(texto, largo, capacidad, 'x')) < 0 ||
        (largo = escribir_entero(texto, largo, capacidad, RE_FILAS)) < 0) {
        return RE_ERR_ARGUMENTO;
    }
    for (i = 0; i < e->actual.n; i++) {
        if ((largo = escribir_caracter(texto, largo, capacidad, ';')) < 0 || (largo = escribir_entero(texto, largo, capacidad, e->actual.v[i].columna)) < 0 ||
            (largo = escribir_caracter(texto, largo, capacidad, ',')) < 0 || (largo = escribir_entero(texto, largo, capacidad, e->actual.v[i].fila)) < 0) {
            return RE_ERR_ARGUMENTO;
        }
    }
    texto[largo] = '\0';
    return largo;
}

/* Lee un entero sin signo desde *p y avanza. Devuelve -1 si no hay digitos. */
static int leer_entero(const char **p)
{
    int valor = 0;
    int hay = 0;

    while (**p >= '0' && **p <= '9') {
        valor = valor * 10 + (**p - '0');
        if (valor > 10000) {
            return -1;
        }
        (*p)++;
        hay = 1;
    }
    return hay ? valor : -1;
}

int re_de_texto(const char *texto, EditorRuta *e)
{
    const char *p = texto;
    RutaVertices nueva;
    int c, f;
    const char *cabeza = "RUTA1;";
    int i;

    if (texto == NULL || e == NULL) {
        return RE_ERR_ARGUMENTO;
    }
    for (i = 0; cabeza[i] != '\0'; i++) {
        if (*p++ != cabeza[i]) {
            return RE_ERR_TEXTO;
        }
    }
    c = leer_entero(&p);
    if (c != RE_COLUMNAS || *p++ != 'x') {
        return RE_ERR_TEXTO;
    }
    f = leer_entero(&p);
    if (f != RE_FILAS) {
        return RE_ERR_TEXTO;
    }
    nueva.n = 0;
    while (*p == ';') {
        p++;
        c = leer_entero(&p);
        if (c < 0 || *p++ != ',') {
            return RE_ERR_TEXTO;
        }
        f = leer_entero(&p);
        if (f < 0 || !dentro(c, f)) {
            return RE_ERR_TEXTO;
        }
        if (nueva.n >= RE_MAX_VERTICES || (nueva.n > 0 && nueva.v[nueva.n - 1].columna == c && nueva.v[nueva.n - 1].fila == f)) {
            return RE_ERR_TEXTO;
        }
        nueva.v[nueva.n].columna = c;
        nueva.v[nueva.n].fila = f;
        nueva.n++;
    }
    if (*p != '\0' || !cabe(&nueva)) {
        return RE_ERR_TEXTO;
    }
    e->actual = nueva;
    e->n_deshacer = 0;
    e->n_rehacer = 0;
    return RE_OK;
}

int re_nombre_valido(const char *nombre)
{
    int letras = 0;
    int i;

    if (nombre == NULL || nombre[0] == '\0' || nombre[0] == ' ') {
        return 0;
    }
    for (i = 0; nombre[i] != '\0'; i++) {
        unsigned char c = (unsigned char)nombre[i];

        if (c >= 0x80) {
            if (c < 0xC0) {
                continue;   /* byte de continuación de una letra con acento: ya se contó */
            }
        } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_')) {
            return 0;
        }
        letras++;
        if (letras > RE_MAX_NOMBRE) {
            return 0;
        }
    }
    return nombre[i - 1] != ' ';
}

const char *re_texto_error(int codigo)
{
    switch (codigo) {
    case RE_OK:           return "Bien.";
    case RE_ERR_FUERA:    return "Ese punto queda fuera de la cuadrícula.";
    case RE_ERR_LLENO:    return "La ruta ya tiene el máximo de puntos.";
    case RE_ERR_REPETIDO: return "Ese punto es igual al vecino.";
    case RE_ERR_LARGA:    return "La ruta quedaría demasiado larga para el carro.";
    case RE_ERR_INDICE:   return "Ese punto no existe.";
    case RE_ERR_NADA:     return "No hay nada que deshacer.";
    case RE_ERR_TEXTO:    return "La ruta guardada no se puede leer.";
    default:              return "No se pudo hacer.";
    }
}
