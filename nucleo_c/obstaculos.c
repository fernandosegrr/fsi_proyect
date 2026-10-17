/*
 * obstaculos.c — Detector de obstáculos por color de piso (ver obstaculos.h).
 *
 * GUÍA DE LECTURA
 *  1. Rejilla: el tamaño de la imagen se pasa en cada llamada; de ahí salen las columnas y
 *     filas de celdas de 10x10 píxeles (calcular_rejilla). Nada supone 320x240.
 *  2. promediar_celdas(): convierte la imagen en un color promedio por celda.
 *  3. FRANJA Y MODELO DEL PISO. El piso puede tener varios colores (baldosas, vetas), así que de la
 *     franja de referencia (abajo al centro) se sacan hasta K GRUPOS de color con un k-medias corto:
 *       reunir_franja → agrupar_franja → todos_compatibles / grupos_de_la_franja.
 *     Si la franja mezcla colores que no pueden ser piso (un objeto encima) da OBS_ERR_REFERENCIA.
 *  4. es_piso_modelo(): una celda es piso si se parece a CUALQUIER grupo, o a una mezcla entre dos
 *     grupos (el borde entre dos baldosas). Ver es_piso() para la regla de color y de luz.
 *  5. marcar_candidatas() y confirmar_celdas(): marca las celdas "no piso" (y, si se pide, los bordes
 *     de objetos casi del color del piso) y descarta las aisladas (ruido).
 *  6. resumir_zonas(): cuenta las celdas por zona (izquierda, centro, derecha) desde la fila
 *     mínima hacia abajo y calcula la cercanía.
 *  7. APRENDIZAJE (aprender_del_cuadro): con el carro avanzando y sin obstáculos, el modelo se mueve
 *     despacio hacia lo que se ve; NUNCA aprende celdas marcadas como obstáculo ni vecinas.
 *     CAMBIO DE PISO: si todo se ve "no piso" durante 1 s, se reaprende desde la franja.
 *  8. obstaculos_analizar_modelo() junta todo conservando el ModeloPiso entre cuadros;
 *     obstaculos_analizar_con() es lo mismo sin memoria (un modelo nuevo en cada llamada).
 *  Solo enteros y arreglos de tamaño fijo: sin malloc, sin recursión.
 */

#include <stddef.h>
#include <string.h>
#include "obstaculos.h"

#define MAX_CELDAS (OBS_MAX_FILAS * OBS_MAX_COLUMNAS)

/* Color promedio de cada celda: [fila][columna][R, G, B]. Solo se usa la parte [filas][columnas]. */
typedef unsigned char MapaColores[OBS_MAX_FILAS][OBS_MAX_COLUMNAS][3];
typedef unsigned char MapaCeldas[OBS_MAX_FILAS][OBS_MAX_COLUMNAS];

/* Tamaño de la cuadrícula del análisis actual, y dónde cae la franja de referencia. */
typedef struct {
    int ancho, alto;            /* tamaño de la imagen en píxeles */
    int columnas, filas;        /* celdas de la cuadrícula */
    int ref_columna, ref_fila;  /* primera celda de la franja de referencia */
    int ref_ncolumnas, ref_nfilas;
} Rejilla;

/* Los grupos de color del piso "en limpio" (enteros de 0 a 255) de un análisis. */
typedef struct {
    int grupos;
    int color[OBS_MAX_GRUPOS][3];
    int peso[OBS_MAX_GRUPOS];
    int mejoras;                /* 1 = usar la regla de brillo oscuro al comparar (config.mejoras_piso) */
} GruposPiso;

/* ── Utilidades ────────────────────────────────────────────────────────── */

static int valor_absoluto(int x)
{
    return (x < 0) ? -x : x;
}

/* Suma de los tres canales: sirve como "brillo" (0 a 765). */
static int brillo(const int color[3])
{
    return color[0] + color[1] + color[2];
}

/* Suma de las diferencias absolutas por canal entre dos colores (0 a 765). */
static int distancia_color(const int a[3], const int b[3])
{
    return valor_absoluto(a[0] - b[0]) + valor_absoluto(a[1] - b[1]) + valor_absoluto(a[2] - b[2]);
}

/* Diferencia entre los colores NORMALIZADOS por brillo (0 a 512): cada canal se divide
 * entre la suma R+G+B (escalado a 256), así dos grises de distinta luz dan casi lo mismo.
 * Hay que haber comprobado antes que ambas sumas sean mayores que cero. */
static int distancia_croma(const int a[3], const int b[3])
{
    int suma_a = brillo(a);
    int suma_b = brillo(b);
    int total = 0;
    int c;

    for (c = 0; c < 3; c++) {
        total += valor_absoluto(a[c] * 256 / suma_a - b[c] * 256 / suma_b);
    }
    return total;
}

/* ¿Dos colores tienen el mismo tono (aunque distinta luz)? Ambos con brillo suficiente y color normalizado parecido. */
static int mismo_tono(const int a[3], const int b[3])
{
    return brillo(a) >= SUMA_MIN_CROMA && brillo(b) >= SUMA_MIN_CROMA && distancia_croma(a, b) <= UMBRAL_CROMA;
}

/* Convierte una proporción en milésimas a celdas: total * milesimas / 1000 (hacia abajo). */
static int proporcion(int total, int milesimas)
{
    return total * milesimas / 1000;
}

static int limitar_entre(int valor, int minimo, int maximo)
{
    if (valor < minimo) {
        return minimo;
    }
    return (valor > maximo) ? maximo : valor;
}

int obstaculos_columnas_de(int ancho)
{
    return (ancho > 0) ? ancho / OBS_LADO_CELDA : 0;
}

int obstaculos_filas_de(int alto)
{
    return (alto > 0) ? alto / OBS_LADO_CELDA : 0;
}

/* ── Paso 1: la rejilla según el tamaño de la imagen ───────────────────── */

/* Revisa el tamaño y calcula la cuadrícula. Devuelve OBS_OK u OBS_ERR_TAMANO (ancho o alto no
 * positivos, más grandes que el máximo o con menos de OBS_MIN_COLUMNAS x OBS_MIN_FILAS celdas). */
static int calcular_rejilla(int ancho, int alto, Rejilla *rejilla)
{
    if (ancho <= 0 || alto <= 0 || ancho > OBS_MAX_COLUMNAS * OBS_LADO_CELDA || alto > OBS_MAX_FILAS * OBS_LADO_CELDA) {
        return OBS_ERR_TAMANO;
    }
    rejilla->ancho = ancho;
    rejilla->alto = alto;
    rejilla->columnas = obstaculos_columnas_de(ancho);
    rejilla->filas = obstaculos_filas_de(alto);
    if (rejilla->columnas < OBS_MIN_COLUMNAS || rejilla->filas < OBS_MIN_FILAS ||
        rejilla->columnas > OBS_MAX_COLUMNAS || rejilla->filas > OBS_MAX_FILAS) {
        return OBS_ERR_TAMANO;
    }
    return OBS_OK;
}

/* Ubica la franja de referencia dentro de la rejilla a partir de las proporciones de la config:
 * su tamaño es una fracción de las columnas y filas (al menos 1 celda) y se coloca centrada en
 * `ref_centro_x_mil` y con su borde de abajo en `ref_borde_inf_mil`; si se saldría de la imagen,
 * se corre hacia adentro. */
static void calcular_franja(const ConfigObstaculos *config, Rejilla *rejilla)
{
    int ncolumnas = limitar_entre(proporcion(rejilla->columnas, config->ref_ancho_mil), 1, rejilla->columnas);
    int nfilas = limitar_entre(proporcion(rejilla->filas, config->ref_alto_mil), 1, rejilla->filas);
    int centro = proporcion(rejilla->columnas, config->ref_centro_x_mil);
    int borde = proporcion(rejilla->filas, config->ref_borde_inf_mil);

    rejilla->ref_ncolumnas = ncolumnas;
    rejilla->ref_nfilas = nfilas;
    rejilla->ref_columna = limitar_entre(centro - ncolumnas / 2, 0, rejilla->columnas - ncolumnas);
    rejilla->ref_fila = limitar_entre(borde - nfilas, 0, rejilla->filas - nfilas);
}

/* ── Paso 2: promedio de cada celda ────────────────────────────────────── */

static void promediar_celdas(const unsigned char *rgb, const Rejilla *rejilla, MapaColores celdas)
{
    int fila, columna, y, x, c;

    for (fila = 0; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int suma[3] = { 0, 0, 0 };

            for (y = 0; y < OBS_LADO_CELDA; y++) {
                for (x = 0; x < OBS_LADO_CELDA; x++) {
                    int pixel_y = fila * OBS_LADO_CELDA + y;
                    int pixel_x = columna * OBS_LADO_CELDA + x;
                    const unsigned char *p = rgb + (pixel_y * rejilla->ancho + pixel_x) * 3;

                    suma[0] += p[0];
                    suma[1] += p[1];
                    suma[2] += p[2];
                }
            }
            for (c = 0; c < 3; c++) {
                celdas[fila][columna][c] = (unsigned char)(suma[c] / (OBS_LADO_CELDA * OBS_LADO_CELDA));
            }
        }
    }
}

/* Copia el color de una celda a un arreglo de enteros. */
static void leer_celda(MapaColores celdas, int fila, int columna, int color[3])
{
    color[0] = celdas[fila][columna][0];
    color[1] = celdas[fila][columna][1];
    color[2] = celdas[fila][columna][2];
}

/* ── Paso 3: grupos de color de la franja de referencia (k-medias corto) ─ */

/* Junta los colores de las celdas de la franja en `muestras`. Devuelve cuántos son. */
static int reunir_franja(MapaColores celdas, const Rejilla *rejilla, unsigned char muestras[][3])
{
    int n = 0;
    int fila, columna;

    for (fila = rejilla->ref_fila; fila < rejilla->ref_fila + rejilla->ref_nfilas; fila++) {
        for (columna = rejilla->ref_columna; columna < rejilla->ref_columna + rejilla->ref_ncolumnas; columna++) {
            muestras[n][0] = celdas[fila][columna][0];
            muestras[n][1] = celdas[fila][columna][1];
            muestras[n][2] = celdas[fila][columna][2];
            n++;
        }
    }
    return n;
}

/* Distancia de una muestra a un centro. */
static int distancia_muestra(const unsigned char muestra[3], const int centro[3])
{
    int color[3];

    color[0] = muestra[0];
    color[1] = muestra[1];
    color[2] = muestra[2];
    return distancia_color(color, centro);
}

/* Índice del centro más cercano a la muestra (el de menor índice si empatan). */
static int centro_mas_cercano(const unsigned char muestra[3], int centros[][3], int k)
{
    int mejor = 0;
    int distancia_mejor = distancia_muestra(muestra, centros[0]);
    int g;

    for (g = 1; g < k; g++) {
        int d = distancia_muestra(muestra, centros[g]);

        if (d < distancia_mejor) {
            distancia_mejor = d;
            mejor = g;
        }
    }
    return mejor;
}

/* Reparte las n muestras en hasta k_max grupos con un k-medias CORTO (3 vueltas):
 *   1. Centros iniciales por "el más lejano": el primero es la muestra más clara; cada siguiente es la
 *      muestra más lejos de los centros ya elegidos (y solo si está al menos OBS_DIST_FUSION lejos).
 *   2. Tres vueltas de: cada muestra al centro más cercano y cada centro al promedio de las suyas.
 *   3. Los grupos que quedan casi en el mismo color (OBS_DIST_FUSION) se funden.
 *   4. Con `apoyo_min_pct` > 0, un grupo respaldado por menos de ese % de las muestras se DESCARTA (menos el más grande): una mancha o un
 *      objeto chico en la franja ya no se aprende como piso ni la ensucia.
 * Llena `grupos` (colores y pesos) y devuelve el desvío máximo: lo más lejos que quedó una muestra de
 * su grupo (para decidir si la franja es uniforme). Las muestras de los grupos descartados no cuentan para el desvío. */
static int agrupar_franja(unsigned char muestras[][3], int n, int k_max, int apoyo_min_pct, GruposPiso *grupos, int *n_intrusos)
{
    int centros[OBS_MAX_GRUPOS][3];
    int descartados[OBS_MAX_GRUPOS][3];   /* centros de los grupos quitados por poco apoyo */
    int n_descartados = 0;
    int intrusos = 0;
    int suma[OBS_MAX_GRUPOS][3];
    int cuenta[OBS_MAX_GRUPOS];
    int k = 1;
    int i, g, c, vuelta, desvio = 0, cambio;

    /* 1. Centros iniciales. */
    {
        int mas_clara = 0;

        for (i = 1; i < n; i++) {
            if (muestras[i][0] + muestras[i][1] + muestras[i][2] >
                muestras[mas_clara][0] + muestras[mas_clara][1] + muestras[mas_clara][2]) {
                mas_clara = i;
            }
        }
        for (c = 0; c < 3; c++) {
            centros[0][c] = muestras[mas_clara][c];
        }
    }
    while (k < k_max) {
        int mejor = -1;
        int distancia_mejor = OBS_DIST_FUSION - 1;

        for (i = 0; i < n; i++) {
            int d = distancia_muestra(muestras[i], centros[centro_mas_cercano(muestras[i], centros, k)]);

            if (d > distancia_mejor) {
                distancia_mejor = d;
                mejor = i;
            }
        }
        if (mejor < 0) {
            break;   /* todas las muestras ya están cerca de algún centro */
        }
        for (c = 0; c < 3; c++) {
            centros[k][c] = muestras[mejor][c];
        }
        k++;
    }

    /* 2. Tres vueltas de k-medias. */
    for (vuelta = 0; vuelta < 3; vuelta++) {
        for (g = 0; g < k; g++) {
            suma[g][0] = suma[g][1] = suma[g][2] = 0;
            cuenta[g] = 0;
        }
        for (i = 0; i < n; i++) {
            g = centro_mas_cercano(muestras[i], centros, k);
            suma[g][0] += muestras[i][0];
            suma[g][1] += muestras[i][1];
            suma[g][2] += muestras[i][2];
            cuenta[g]++;
        }
        for (g = 0; g < k; g++) {
            if (cuenta[g] > 0) {
                for (c = 0; c < 3; c++) {
                    centros[g][c] = (suma[g][c] * 2 + cuenta[g]) / (2 * cuenta[g]);   /* promedio redondeado */
                }
            }
        }
    }

    /* 3. Pesos finales y fusión de grupos casi iguales. */
    for (g = 0; g < k; g++) {
        cuenta[g] = 0;
    }
    for (i = 0; i < n; i++) {
        cuenta[centro_mas_cercano(muestras[i], centros, k)]++;
    }
    do {
        int a, b;

        cambio = 0;
        for (a = 0; a < k && !cambio; a++) {
            for (b = a + 1; b < k && !cambio; b++) {
                if (cuenta[a] > 0 && cuenta[b] > 0 && distancia_color(centros[a], centros[b]) <= OBS_DIST_FUSION) {
                    int total = cuenta[a] + cuenta[b];

                    for (c = 0; c < 3; c++) {
                        centros[a][c] = (centros[a][c] * cuenta[a] + centros[b][c] * cuenta[b] + total / 2) / total;
                    }
                    cuenta[a] = total;
                    cuenta[b] = 0;
                    cambio = 1;
                }
            }
        }
    } while (cambio);

    /* 4. Quitar los grupos con poco apoyo (el más grande siempre se queda). */
    if (apoyo_min_pct > 0) {
        int mayor = 0;

        for (g = 1; g < k; g++) {
            if (cuenta[g] > cuenta[mayor]) {
                mayor = g;
            }
        }
        for (g = 0; g < k; g++) {
            if (g != mayor && cuenta[g] > 0 && cuenta[g] * 100 < n * apoyo_min_pct) {
                for (c = 0; c < 3; c++) {
                    descartados[n_descartados][c] = centros[g][c];
                }
                n_descartados++;
                cuenta[g] = 0;
            }
        }
    }

    /* Grupos que quedaron vivos (con peso > 0), en el orden en que aparecieron. */
    grupos->grupos = 0;
    for (g = 0; g < k; g++) {
        if (cuenta[g] > 0) {
            for (c = 0; c < 3; c++) {
                grupos->color[grupos->grupos][c] = centros[g][c];
            }
            grupos->peso[grupos->grupos] = cuenta[g];
            grupos->grupos++;
        }
    }
    for (i = 0; i < n; i++) {
        int mejor = centro_mas_cercano(muestras[i], grupos->color, grupos->grupos);
        int d = distancia_muestra(muestras[i], grupos->color[mejor]);

        if (n_descartados > 0) {   /* una muestra más cerca de un grupo descartado que de uno que se queda es un "intruso": no cuenta */
            int mas_cerca_de_descartado = centro_mas_cercano(muestras[i], descartados, n_descartados);

            if (distancia_muestra(muestras[i], descartados[mas_cerca_de_descartado]) < d) {
                intrusos++;
                continue;
            }
        }
        if (d > desvio) {
            desvio = d;
        }
    }
    if (n_intrusos != NULL) {
        *n_intrusos = intrusos;
    }
    return desvio;
}

/* ¿Dos grupos de color pueden ser los dos "piso"? Sí si no están muy lejos, o si tienen el mismo
 * tono con distinta luz (baldosa clara y baldosa oscura). Un objeto de otro color (una caja roja
 * sobre un piso café) NO es compatible. */
static int compatibles(const int a[3], const int b[3])
{
    return distancia_color(a, b) <= OBS_MAX_DIST_GRUPOS || mismo_tono(a, b);
}

static int todos_compatibles(const GruposPiso *g)
{
    int a, b;

    for (a = 0; a < g->grupos; a++) {
        for (b = a + 1; b < g->grupos; b++) {
            if (!compatibles(g->color[a], g->color[b])) {
                return 0;
            }
        }
    }
    return 1;
}

/* Calcula los grupos de la franja de referencia. Devuelve OBS_OK, u OBS_ERR_REFERENCIA si alguna celda
 * se aleja demasiado de su grupo (franja contaminada) o si los grupos no pueden ser todos piso. */
static int grupos_de_la_franja(MapaColores celdas, const Rejilla *rejilla, int k_max, int apoyo_min_pct, GruposPiso *grupos)
{
    unsigned char muestras[MAX_CELDAS][3];
    int n = reunir_franja(celdas, rejilla, muestras);
    int desvio = agrupar_franja(muestras, n, k_max, apoyo_min_pct, grupos, NULL);

    if (desvio > MAX_VARIACION_REF || !todos_compatibles(grupos)) {
        return OBS_ERR_REFERENCIA;
    }
    return OBS_OK;
}

/* ── El modelo del piso (lo que se recuerda entre cuadros) ─────────────── */

void obstaculos_modelo_iniciar(ModeloPiso *modelo)
{
    int g, c;

    if (modelo == NULL) {
        return;
    }
    modelo->valido = 0;
    modelo->grupos = 0;
    for (g = 0; g < OBS_MAX_GRUPOS; g++) {
        for (c = 0; c < 3; c++) {
            modelo->color256[g][c] = 0;
        }
        modelo->peso[g] = 0;
    }
    modelo->sin_piso_activo = 0;
    modelo->sin_piso_desde_ms = 0;
    modelo->reaprendidos = 0;
    modelo->franja_hist = 0;
    modelo->franja_hist_n = 0;
}

/* Deja en el modelo exactamente los grupos dados (aprende desde cero). */
static void modelo_desde_grupos(ModeloPiso *modelo, const GruposPiso *grupos)
{
    int g, c;
    int reaprendidos = modelo->reaprendidos;
    unsigned int franja_hist = modelo->franja_hist;
    int franja_hist_n = modelo->franja_hist_n;

    obstaculos_modelo_iniciar(modelo);
    modelo->reaprendidos = reaprendidos;
    modelo->franja_hist = franja_hist;          /* la historia de la franja no se borra al reaprender el piso */
    modelo->franja_hist_n = franja_hist_n;
    modelo->valido = 1;
    modelo->grupos = grupos->grupos;
    for (g = 0; g < grupos->grupos; g++) {
        for (c = 0; c < 3; c++) {
            modelo->color256[g][c] = grupos->color[g][c] * 256;
        }
        modelo->peso[g] = grupos->peso[g];
    }
}

/* El modelo "en limpio" (enteros de 0 a 255) para clasificar. */
static void modelo_a_grupos(const ModeloPiso *modelo, GruposPiso *grupos)
{
    int g, c;

    grupos->grupos = modelo->grupos;
    grupos->mejoras = 0;
    for (g = 0; g < modelo->grupos; g++) {
        for (c = 0; c < 3; c++) {
            grupos->color[g][c] = (modelo->color256[g][c] + 128) / 256;
        }
        grupos->peso[g] = modelo->peso[g];
    }
}

/* Une al modelo los grupos nuevos de la franja: uno se agrega si no se parece a ninguno de los que ya hay,
 * es compatible con todos (puede ser piso) y todavía hay lugar (K). Así el piso va ganando tonos sin que
 * un objeto de otro color pegado al carro se aprenda como piso. */
static void modelo_agregar_grupos(ModeloPiso *modelo, const GruposPiso *franja, int k_max)
{
    int i, g, c;

    for (i = 0; i < franja->grupos; i++) {
        GruposPiso actual;
        int cerca = 0, compatible = 1;

        modelo_a_grupos(modelo, &actual);
        for (g = 0; g < actual.grupos; g++) {
            if (distancia_color(franja->color[i], actual.color[g]) <= OBS_DIST_FUSION * 2) {
                cerca = 1;
            }
            if (!compatibles(franja->color[i], actual.color[g])) {
                compatible = 0;
            }
        }
        if (!cerca && compatible && modelo->grupos < k_max && modelo->grupos < OBS_MAX_GRUPOS) {
            for (c = 0; c < 3; c++) {
                modelo->color256[modelo->grupos][c] = franja->color[i][c] * 256;
            }
            modelo->peso[modelo->grupos] = franja->peso[i];
            modelo->grupos++;
        }
    }
}

/* ¿Cada grupo de la franja ya está cubierto por el modelo (hay un grupo del modelo cerca)? Si no, lo que
 * se ve abajo no se parece a lo aprendido (algo tapa la franja, o el piso cambió). */
static int franja_cubierta(const ModeloPiso *modelo, const GruposPiso *franja)
{
    GruposPiso actual;
    int i, g;

    modelo_a_grupos(modelo, &actual);
    for (i = 0; i < franja->grupos; i++) {
        int cerca = 0;

        for (g = 0; g < actual.grupos; g++) {
            if (distancia_color(franja->color[i], actual.color[g]) <= OBS_DIST_FUSION * 2) {
                cerca = 1;
            }
        }
        if (!cerca) {
            return 0;
        }
    }
    return 1;
}

/* ── Paso 4: ¿esta celda es piso? ──────────────────────────────────────── */

/* Devuelve 1 si el color de la celda es piso respecto a UN color de referencia:
 *   A) casi igual al de referencia (suma de diferencias <= umbral_color), o
 *   B) mismo tono pero distinta luz: color normalizado parecido (<= UMBRAL_CROMA) y
 *      brillo que no difiere más de UMBRAL_BRILLO. B solo se usa si ambos colores
 *      tienen brillo suficiente (SUMA_MIN_CROMA); en la oscuridad el tono no es confiable. */
static int es_piso(const int color[3], const int referencia[3], int umbral_color, int mejoras)
{
    if (distancia_color(color, referencia) <= umbral_color) {
        return 1;
    }
    /* Celda y referencia OSCURAS (mejoras_piso): el color no es confiable (ruido, reflejos, JPEG): solo se compara la luminancia, con una
     * diferencia mínima tolerada. Sin croma. */
    if (mejoras && brillo(color) < BRILLO_OSCURO && brillo(referencia) < BRILLO_OSCURO) {
        int tolerancia = (umbral_color > DIF_MIN_OSCURO) ? umbral_color : DIF_MIN_OSCURO;

        return valor_absoluto(brillo(color) - brillo(referencia)) <= tolerancia;
    }
    if (brillo(color) >= SUMA_MIN_CROMA && brillo(referencia) >= SUMA_MIN_CROMA &&
        distancia_croma(color, referencia) <= UMBRAL_CROMA &&
        valor_absoluto(brillo(color) - brillo(referencia)) <= UMBRAL_BRILLO) {
        return 1;
    }
    return 0;
}

/* El punto del segmento entre los grupos a y b que queda más cerca de `color` (el borde entre dos
 * baldosas mezcla sus dos colores). Usa una proyección en enteros: t va de 0 a 256. */
static void punto_del_segmento(const int color[3], const int a[3], const int b[3], int punto[3])
{
    int direccion[3], c;
    long largo2 = 0, producto = 0, t;

    for (c = 0; c < 3; c++) {
        direccion[c] = b[c] - a[c];
        largo2 += (long)direccion[c] * direccion[c];
        producto += (long)(color[c] - a[c]) * direccion[c];
    }
    if (largo2 == 0) {
        t = 0;
    } else {
        t = producto * 256 / largo2;
        t = (t < 0) ? 0 : ((t > 256) ? 256 : t);
    }
    for (c = 0; c < 3; c++) {
        punto[c] = a[c] + (int)(direccion[c] * t / 256);
    }
}

/* ¿Es piso respecto al modelo? Sí si lo es respecto a cualquier grupo, o respecto a un punto entre dos
 * grupos (solo con tolerancia reducida a la mitad: no se quiere aceptar cualquier cosa "en medio"). */
static int es_piso_modelo(const GruposPiso *piso, const int color[3], int umbral_color)
{
    int a, b;

    for (a = 0; a < piso->grupos; a++) {
        if (es_piso(color, piso->color[a], umbral_color, piso->mejoras)) {
            return 1;
        }
    }
    for (a = 0; a < piso->grupos; a++) {
        for (b = a + 1; b < piso->grupos; b++) {
            int punto[3];

            punto_del_segmento(color, piso->color[a], piso->color[b], punto);
            if (distancia_color(color, punto) <= umbral_color / 2) {
                return 1;
            }
        }
    }
    return 0;
}

/* Distancia al grupo más cercano (para saber cuán "dudosa" es una celda). */
static int distancia_al_piso(const GruposPiso *piso, const int color[3])
{
    int mejor = 765 * 3;
    int g;

    for (g = 0; g < piso->grupos; g++) {
        int d = distancia_color(color, piso->color[g]);

        if (d < mejor) {
            mejor = d;
        }
    }
    return mejor;
}

/* ── Paso 5: celdas "no piso" y filtro de ruido ────────────────────────── */

/* Marca con 1 las celdas que no parecen piso. Con `usar_bordes`, además marca el CONTORNO de los objetos
 * casi del color del piso: celdas "dudosas" (algo distintas del piso) que tienen al lado un salto de
 * color brusco con piso seguro. Una sombra suave no cuenta: sus celdas vecinas cambian poco entre sí.
 * Devuelve cuántas celdas se marcaron SOLO por los bordes. */
static int marcar_candidatas(MapaColores celdas, const Rejilla *rejilla, const GruposPiso *piso,
                             int umbral_color, int usar_bordes, MapaCeldas candidatas)
{
    int fila, columna;
    int solo_bordes = 0;
    int limite_dudosa = umbral_color * 35 / 100;   /* de aquí hacia arriba una celda de piso ya es "dudosa" */
    MapaCeldas por_borde;

    for (fila = 0; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int color[3];

            leer_celda(celdas, fila, columna, color);
            candidatas[fila][columna] = (unsigned char)(es_piso_modelo(piso, color, umbral_color) ? 0 : 1);
            por_borde[fila][columna] = 0;
        }
    }
    if (!usar_bordes) {
        return 0;
    }
    for (fila = 0; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int color[3], vecino[3];
            int df, dc;
            int salto = 0, hay_piso_seguro = 0;

            if (candidatas[fila][columna]) {
                continue;
            }
            leer_celda(celdas, fila, columna, color);
            if (distancia_al_piso(piso, color) < limite_dudosa) {
                continue;   /* es piso seguro: no puede ser contorno de un objeto */
            }
            for (df = -1; df <= 1; df++) {
                for (dc = -1; dc <= 1; dc++) {
                    int f = fila + df;
                    int c = columna + dc;

                    if ((df == 0) != (dc == 0) && f >= 0 && f < rejilla->filas && c >= 0 && c < rejilla->columnas) {
                        int d;

                        leer_celda(celdas, f, c, vecino);
                        d = distancia_color(color, vecino);
                        if (d > salto) {
                            salto = d;
                        }
                        if (distancia_al_piso(piso, vecino) < limite_dudosa) {
                            hay_piso_seguro = 1;
                        }
                    }
                }
            }
            if (salto >= OBS_UMBRAL_BORDE && hay_piso_seguro) {
                por_borde[fila][columna] = 1;
            }
        }
    }
    for (fila = 0; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            if (por_borde[fila][columna]) {
                candidatas[fila][columna] = 1;
                solo_bordes++;
            }
        }
    }
    return solo_bordes;
}

/* Máscara de artefactos (J9): las celdas cuya celda gruesa de 10 x 10 está enmascarada se tratan como piso. */
static void aplicar_mascara(const ConfigObstaculos *config, const Rejilla *rejilla, MapaCeldas candidatas)
{
    int fila, columna;

    if (!config->mascara_activa) {
        return;
    }
    for (fila = 0; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            if (config->mascara[fila * OBS_MASC_FILAS / rejilla->filas][columna * OBS_MASC_COLUMNAS / rejilla->columnas]) {
                candidatas[fila][columna] = 0;
            }
        }
    }
}

/* Cuenta las vecinas (8 alrededor) marcadas. */
static int contar_vecinas(MapaCeldas candidatas, const Rejilla *rejilla, int fila, int columna)
{
    int cuenta = 0;
    int df, dc;

    for (df = -1; df <= 1; df++) {
        for (dc = -1; dc <= 1; dc++) {
            int f = fila + df;
            int c = columna + dc;

            if ((df != 0 || dc != 0) && f >= 0 && f < rejilla->filas && c >= 0 && c < rejilla->columnas) {
                cuenta += candidatas[f][c];
            }
        }
    }
    return cuenta;
}

/* Una celda "no piso" solo cuenta si tiene al menos una vecina "no piso" (descarta el ruido). */
static void confirmar_celdas(MapaCeldas candidatas, const Rejilla *rejilla, MapaCeldas confirmadas)
{
    int fila, columna;

    for (fila = 0; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int sola = (contar_vecinas(candidatas, rejilla, fila, columna) == 0);

            confirmadas[fila][columna] = (unsigned char)((candidatas[fila][columna] && !sola) ? 1 : 0);
        }
    }
}

/* ── Paso 6: zonas y cercanía ──────────────────────────────────────────── */

/* Cercanía (1 a 3) según la fila más baja alcanzada: los límites son proporciones del alto. */
static int cercania_por_fila(const Rejilla *rejilla, int fila_mas_baja)
{
    if (fila_mas_baja >= proporcion(rejilla->filas, OBS_CERCA_MIL)) {
        return 3;
    }
    if (fila_mas_baja >= proporcion(rejilla->filas, OBS_MEDIA_MIL)) {
        return 2;
    }
    return 1;
}

/* Cuenta las celdas confirmadas de cada zona (tercios de ancho) desde la fila `fila_minima`
 * hacia abajo; las filas de arriba se ignoran. Una zona tiene obstáculo si tiene al menos
 * `min_celdas` celdas. La cercanía sale de la fila más baja que alcanza el obstáculo. */
static void resumir_zonas(MapaCeldas confirmadas, const Rejilla *rejilla, int min_celdas,
                          int fila_minima, ResultadoObstaculos *resultado)
{
    int fila_mas_baja[OBS_ZONAS] = { -1, -1, -1 };
    int fila, columna, zona;

    for (fila = fila_minima; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            if (confirmadas[fila][columna]) {
                zona = columna * OBS_ZONAS / rejilla->columnas;   /* 0, 1 o 2 */
                resultado->zona[zona].celdas++;
                fila_mas_baja[zona] = fila;   /* las filas se recorren de arriba abajo */
            }
        }
    }
    for (zona = 0; zona < OBS_ZONAS; zona++) {
        if (resultado->zona[zona].celdas >= min_celdas) {
            resultado->zona[zona].hay_obstaculo = 1;
            resultado->zona[zona].cercania = cercania_por_fila(rejilla, fila_mas_baja[zona]);
        }
    }
}

/* ── Paso 7: aprendizaje y cambio de piso ──────────────────────────────── */

/* Mueve cada grupo del modelo un poquito hacia el promedio de las celdas de piso que se le parecen.
 * Solo se llama con el carro avanzando y SIN obstáculo. Celdas que NO se usan nunca:
 *  - las marcadas como "no piso" (candidatas), ni sus vecinas (el borde de un objeto);
 *  - las "dudosas" (más lejos de la mitad del umbral de su grupo): podrían ser un objeto casi igual;
 *  - las de arriba de la fila mínima (el fondo del cuarto).
 * Devuelve 1 si se actualizó algo. */
static int aprender_del_cuadro(ModeloPiso *modelo, MapaColores celdas, const Rejilla *rejilla,
                               MapaCeldas candidatas, int umbral_color, int fila_minima, int velocidad_mil)
{
    GruposPiso piso;
    long suma[OBS_MAX_GRUPOS][3];
    int cuenta[OBS_MAX_GRUPOS];
    int fila, columna, g, c, actualizado = 0;

    if (velocidad_mil <= 0) {
        return 0;
    }
    modelo_a_grupos(modelo, &piso);
    for (g = 0; g < piso.grupos; g++) {
        suma[g][0] = suma[g][1] = suma[g][2] = 0;
        cuenta[g] = 0;
    }
    for (fila = fila_minima; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int color[3];
            int mejor = 0;
            int distancia_mejor, d;

            if (candidatas[fila][columna] || contar_vecinas(candidatas, rejilla, fila, columna) > 0) {
                continue;
            }
            leer_celda(celdas, fila, columna, color);
            distancia_mejor = distancia_color(color, piso.color[0]);
            for (g = 1; g < piso.grupos; g++) {
                d = distancia_color(color, piso.color[g]);
                if (d < distancia_mejor) {
                    distancia_mejor = d;
                    mejor = g;
                }
            }
            if (distancia_mejor > umbral_color / 2) {
                continue;
            }
            for (c = 0; c < 3; c++) {
                suma[mejor][c] += color[c];
            }
            cuenta[mejor]++;
        }
    }
    for (g = 0; g < piso.grupos; g++) {
        if (cuenta[g] > 0) {
            for (c = 0; c < 3; c++) {
                long promedio256 = suma[g][c] * 256 / cuenta[g];

                modelo->color256[g][c] += (int)((promedio256 - modelo->color256[g][c]) * velocidad_mil / 1000);
            }
            modelo->peso[g] = cuenta[g];
            actualizado = 1;
        }
    }
    return actualizado;
}

/* Cuenta las celdas de piso y el total en la zona de avance. */
static void contar_piso_en_zona(MapaCeldas candidatas, const Rejilla *rejilla, int fila_minima,
                                int *piso, int *total)
{
    int fila, columna;

    *piso = 0;
    *total = 0;
    for (fila = fila_minima; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            (*total)++;
            if (!candidatas[fila][columna]) {
                (*piso)++;
            }
        }
    }
}

/* ── Función principal ─────────────────────────────────────────────────── */

/* Deja el resultado en ceros. */
static void limpiar_resultado(ResultadoObstaculos *resultado)
{
    int zona, fila, columna, g, c;

    for (zona = 0; zona < OBS_ZONAS; zona++) {
        resultado->zona[zona].hay_obstaculo = 0;
        resultado->zona[zona].cercania = 0;
        resultado->zona[zona].celdas = 0;
        resultado->zona[zona].celdas_corredor = 0;      /* los campos del corredor los llena corredor.c; aquí empiezan en 0 */
        resultado->zona[zona].fuera_corredor = 0;
        resultado->zona[zona].cercania_corredor = 0;
    }
    resultado->ref_r = 0;
    resultado->ref_g = 0;
    resultado->ref_b = 0;
    resultado->columnas = 0;
    resultado->filas = 0;
    resultado->fila_minima_usada = 0;
    resultado->rejilla_chica = 0;
    resultado->ref_columna = 0;
    resultado->ref_fila = 0;
    resultado->ref_ncolumnas = 0;
    resultado->ref_nfilas = 0;
    resultado->grupos = 0;
    for (g = 0; g < OBS_MAX_GRUPOS; g++) {
        for (c = 0; c < 3; c++) {
            resultado->grupo_color[g][c] = 0;
        }
    }
    resultado->franja_contaminada = 0;
    resultado->franja_tapada_pct = 0;
    resultado->franja_ignorada = 0;
    resultado->piso_reaprendido = 0;
    resultado->aprendio = 0;
    resultado->celdas_borde = 0;
    resultado->brillo_medio = 0;
    resultado->ganancia_mil = 1000;
    resultado->celdas_marcadas = 0;
    resultado->motivo = 0;
    for (fila = 0; fila < OBS_MAX_FILAS; fila++) {
        for (columna = 0; columna < OBS_MAX_COLUMNAS; columna++) {
            resultado->no_piso[fila][columna] = 0;
        }
    }
}

void obstaculos_config_defecto(ConfigObstaculos *config)
{
    if (config == NULL) {
        return;
    }
    config->umbral_color = UMBRAL_COLOR;
    config->min_celdas = MIN_CELDAS;
    config->ref_ancho_mil = OBS_REF_ANCHO_MIL;
    config->ref_alto_mil = OBS_REF_ALTO_MIL;
    config->ref_centro_x_mil = OBS_REF_CENTRO_X_MIL;
    config->ref_borde_inf_mil = OBS_REF_BORDE_INF_MIL;
    config->fila_minima_mil = OBS_FILA_MINIMA_MIL;
    config->grupos_piso = OBS_GRUPOS_DEFECTO;
    config->aprendizaje_mil = OBS_APRENDIZAJE_DEFECTO;
    config->usar_bordes = 0;
    config->mejoras_piso = 1;
    config->piso_fijo = 0;
    config->mascara_activa = 0;
    memset(config->mascara, 0, sizeof config->mascara);
}

/* La configuración es válida si los umbrales y las proporciones están en rango. */
static int config_es_valida(const ConfigObstaculos *config)
{
    return config->umbral_color >= 0 && config->umbral_color <= 765 &&
           config->min_celdas >= 1 &&
           config->ref_ancho_mil >= 1 && config->ref_ancho_mil <= 1000 &&
           config->ref_alto_mil >= 1 && config->ref_alto_mil <= 1000 &&
           config->ref_centro_x_mil >= 0 && config->ref_centro_x_mil <= 1000 &&
           config->ref_borde_inf_mil >= 1 && config->ref_borde_inf_mil <= 1000 &&
           config->fila_minima_mil >= 0 && config->fila_minima_mil <= 1000 &&
           config->grupos_piso >= 1 && config->grupos_piso <= OBS_MAX_GRUPOS &&
           config->aprendizaje_mil >= 0 && config->aprendizaje_mil <= 1000 &&
           (config->usar_bordes == 0 || config->usar_bordes == 1) &&
           (config->mejoras_piso == 0 || config->mejoras_piso == 1) &&
           (config->piso_fijo == 0 || config->piso_fijo == 1) &&
           (config->mascara_activa == 0 || config->mascara_activa == 1);
}

/* ── Reajuste de brillo (cambio de exposición de la cámara) ────────────────
 * Para cada celda de la zona de avance con brillo suficiente se busca el grupo del piso que tiene su mismo TONO (color normalizado) y se mide la
 * razón de brillos celda / grupo. Si la mediana de esas razones se aleja al menos OBS_GANANCIA_MIN_MIL de 1000 y casi todas las celdas
 * (OBS_GANANCIA_CONSISTENCIA_PCT %, en las tres zonas) coinciden con ella, el piso entero cambió de brillo: es la exposición automática y no un
 * objeto. Entonces se escalan los colores del modelo por esa razón. Un objeto que tapa parte del piso rompe la consistencia (o deja menos de la
 * mitad de las celdas con tono de piso) y no se reajusta nada. Devuelve 1 si reajustó y deja la razón en *ganancia_mil. */
#define GANANCIA_BINS        120
#define GANANCIA_RAZON_MIN   250    /* la razón de brillo se mide de 0.25 a 3.25 */
#define GANANCIA_PASO         25

/* La razón de brillo (en milésimas) de una celda respecto al grupo del piso que tiene su mismo TONO. Devuelve 0 si la celda es oscura o no tiene el
 * tono de ningún grupo claro del piso (entonces no sirve para medir el brillo). */
static int razon_de_brillo(const GruposPiso *piso, const int color[3])
{
    int g, mejor = -1, d_mejor = 0;

    if (brillo(color) < SUMA_MIN_CROMA) {   /* con menos brillo que esto el tono no es confiable */
        return 0;
    }
    for (g = 0; g < piso->grupos; g++) {
        int d;

        if (brillo(piso->color[g]) < SUMA_MIN_CROMA) {
            continue;
        }
        d = distancia_croma(color, piso->color[g]);
        if (mejor < 0 || d < d_mejor) {
            mejor = g;
            d_mejor = d;
        }
    }
    if (mejor < 0 || d_mejor > 2 * UMBRAL_CROMA) {
        return 0;
    }
    return brillo(color) * 1000 / brillo(piso->color[mejor]);
}

static int ajustar_brillo(ModeloPiso *trabajo, MapaColores celdas, const Rejilla *rejilla, int fila_minima, int *ganancia_mil)
{
    GruposPiso piso;
    int histograma[GANANCIA_BINS];
    int por_zona[OBS_ZONAS], consistentes_zona[OBS_ZONAS];
    int fila, columna, g, c, considerados = 0, total = 0, acumulado, mediana, bin;
    int franja_total = 0, franja_consistente = 0, dentro = 0;
    int zona_de[OBS_MAX_COLUMNAS];

    modelo_a_grupos(trabajo, &piso);
    for (bin = 0; bin < GANANCIA_BINS; bin++) {
        histograma[bin] = 0;
    }
    for (g = 0; g < OBS_ZONAS; g++) {
        por_zona[g] = 0;
        consistentes_zona[g] = 0;
    }
    for (columna = 0; columna < rejilla->columnas; columna++) {
        zona_de[columna] = columna * OBS_ZONAS / rejilla->columnas;
    }
    for (fila = fila_minima; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int color[3], razon;

            total++;
            leer_celda(celdas, fila, columna, color);
            razon = razon_de_brillo(&piso, color);
            if (razon == 0) {
                continue;
            }
            bin = limitar_entre((razon - GANANCIA_RAZON_MIN) / GANANCIA_PASO, 0, GANANCIA_BINS - 1);
            histograma[bin]++;
            considerados++;
            por_zona[zona_de[columna]]++;
        }
    }
    if (considerados < 12 || considerados * 2 < total) {
        return 0;   /* menos de la mitad de la zona tiene tono de piso: no se puede afirmar que todo el piso cambió */
    }
    acumulado = 0;
    mediana = 0;
    for (bin = 0; bin < GANANCIA_BINS; bin++) {
        acumulado += histograma[bin];
        if (acumulado * 2 >= considerados) {
            mediana = GANANCIA_RAZON_MIN + bin * GANANCIA_PASO + GANANCIA_PASO / 2;
            break;
        }
    }
    if (valor_absoluto(mediana - 1000) < OBS_GANANCIA_MIN_MIL) {
        return 0;
    }
    /* Consistencia con la mediana: en total, en cada zona y en la franja de referencia (la que está pegada abajo, bajo el carro: una pared del
     * mismo tono que el piso puede llenar la zona pero no la franja; si la franja no cambió igual, no es la exposición). */
    for (fila = fila_minima; fila < rejilla->filas; fila++) {
        for (columna = 0; columna < rejilla->columnas; columna++) {
            int color[3], razon, consistente;
            int en_franja = (fila >= rejilla->ref_fila && fila < rejilla->ref_fila + rejilla->ref_nfilas &&
                             columna >= rejilla->ref_columna && columna < rejilla->ref_columna + rejilla->ref_ncolumnas);

            leer_celda(celdas, fila, columna, color);
            razon = razon_de_brillo(&piso, color);
            consistente = (razon != 0 && valor_absoluto(razon - mediana) <= OBS_GANANCIA_TOLERANCIA_MIL);
            if (razon != 0 && consistente) {
                dentro++;
                consistentes_zona[zona_de[columna]]++;
            }
            if (en_franja) {
                franja_total++;
                franja_consistente += consistente ? 1 : 0;
            }
        }
    }
    if (dentro * 100 < considerados * OBS_GANANCIA_CONSISTENCIA_PCT) {
        return 0;
    }
    for (g = 0; g < OBS_ZONAS; g++) {
        if (por_zona[g] > 0 && consistentes_zona[g] * 100 < por_zona[g] * 60) {
            return 0;   /* una zona no sigue el cambio: hay algo ahí, no es la exposición */
        }
    }
    if (franja_total > 0 && franja_consistente * 100 < franja_total * 80) {
        return 0;
    }
    for (g = 0; g < trabajo->grupos; g++) {
        for (c = 0; c < 3; c++) {
            long nuevo = (long)trabajo->color256[g][c] * mediana / 1000;

            trabajo->color256[g][c] = (int)((nuevo > 255L * 256L) ? 255L * 256L : nuevo);
        }
    }
    *ganancia_mil = mediana;
    return 1;
}

/* Todo el análisis a partir de las celdas ya promediadas (`celdas`) y la rejilla (con la franja ya calculada): es lo MISMO para una imagen completa y para las
 * celdas que manda la cámara del carro (modo CELDAS), así los dos dan el mismo resultado (se prueba en pruebas_celdas.c). `resultado` ya viene limpio. */
static int analizar_celdas_modelo(const ConfigObstaculos *config, ModeloPiso *modelo, MapaColores celdas, Rejilla rejilla,
                                  int avanzando, unsigned int ahora_ms, ResultadoObstaculos *resultado)
{
    MapaCeldas candidatas;
    GruposPiso franja, piso;
    ModeloPiso trabajo;
    int fila_minima, franja_ok, g, c;
    int piso_celdas, total_celdas, hay_obstaculo;

    /* El modelo se trabaja en una copia: si hay un error no cambia. */
    trabajo = *modelo;
    franja_ok = (grupos_de_la_franja(celdas, &rejilla, config->grupos_piso, config->mejoras_piso ? OBS_APOYO_MIN_PCT : 0, &franja) == OBS_OK);
    {   /* Historia de la franja: si lleva mucho tapada ya no se confía en ella para reaprender (J9: la franja tapada 40 a 60 % de los cuadros en la prueba real). */
        int n, k, tapadas = 0;

        trabajo.franja_hist = (trabajo.franja_hist << 1) | (franja_ok ? 0u : 1u);
        trabajo.franja_hist_n = (trabajo.franja_hist_n < OBS_FRANJA_VENTANA) ? trabajo.franja_hist_n + 1 : OBS_FRANJA_VENTANA;
        n = trabajo.franja_hist_n;
        for (k = 0; k < n; k++) {
            tapadas += (int)((trabajo.franja_hist >> k) & 1u);
        }
        resultado->franja_tapada_pct = (n >= OBS_FRANJA_MIN_CUADROS) ? tapadas * 100 / n : 0;
        if (franja_ok && trabajo.valido && resultado->franja_tapada_pct > OBS_FRANJA_TAPADA_MAX_PCT) {
            franja_ok = 0;                    /* se sigue con el último modelo del piso en vez de reaprender con la franja */
            resultado->franja_ignorada = 1;
        }
    }
    if (!trabajo.valido) {
        if (!franja_ok) {
            return OBS_ERR_REFERENCIA;   /* nada que recordar y la franja no sirve */
        }
        modelo_desde_grupos(&trabajo, &franja);
    } else if (franja_ok) {
        if (!config->piso_fijo) {   /* en una ruta el piso no cambia: no se agregan colores nuevos */
            modelo_agregar_grupos(&trabajo, &franja, config->grupos_piso);
        }
        if (!franja_cubierta(&trabajo, &franja)) {
            resultado->franja_contaminada = 1;   /* la franja no se parece a lo aprendido */
        }
    } else {
        resultado->franja_contaminada = 1;   /* algo tapa la franja: se sigue con lo aprendido */
    }

    resultado->columnas = rejilla.columnas;
    resultado->filas = rejilla.filas;
    resultado->rejilla_chica = (rejilla.columnas < OBS_AVISO_COLUMNAS || rejilla.filas < OBS_AVISO_FILAS) ? 1 : 0;
    resultado->ref_columna = rejilla.ref_columna;
    resultado->ref_fila = rejilla.ref_fila;
    resultado->ref_ncolumnas = rejilla.ref_ncolumnas;
    resultado->ref_nfilas = rejilla.ref_nfilas;
    /* La fila mínima nunca pasa de la última fila (con 1000 se mira solo la última). */
    fila_minima = limitar_entre(proporcion(rejilla.filas, config->fila_minima_mil), 0, rejilla.filas - 1);
    resultado->fila_minima_usada = fila_minima;

    /* Cambio de exposición: si TODO el piso cambió de brillo, se escalan los colores aprendidos (y no se marca todo como obstáculo). */
    if (config->mejoras_piso && ajustar_brillo(&trabajo, celdas, &rejilla, fila_minima, &resultado->ganancia_mil)) {
        resultado->motivo |= OBS_MOTIVO_REAJUSTE;
    }
    modelo_a_grupos(&trabajo, &piso);
    piso.mejoras = config->mejoras_piso;
    resultado->celdas_borde = marcar_candidatas(celdas, &rejilla, &piso, config->umbral_color, config->usar_bordes, candidatas);
    aplicar_mascara(config, &rejilla, candidatas);
    confirmar_celdas(candidatas, &rejilla, resultado->no_piso);
    resumir_zonas(resultado->no_piso, &rejilla, config->min_celdas, fila_minima, resultado);
    hay_obstaculo = resultado->zona[0].hay_obstaculo || resultado->zona[1].hay_obstaculo || resultado->zona[2].hay_obstaculo;

    /* Aprendizaje: solo avanzando y sin obstáculo. */
    if (avanzando && !hay_obstaculo) {
        resultado->aprendio = aprender_del_cuadro(&trabajo, celdas, &rejilla, candidatas, config->umbral_color,
                                                  fila_minima, config->aprendizaje_mil);
    }

    /* Cambio de piso: todo "no piso" durante OBS_MS_PISO_NUEVO ms seguidos → reaprender de la franja. */
    contar_piso_en_zona(candidatas, &rejilla, fila_minima, &piso_celdas, &total_celdas);
    if (piso_celdas * 100 <= total_celdas * 2) {
        if (!trabajo.sin_piso_activo) {
            trabajo.sin_piso_activo = 1;
            trabajo.sin_piso_desde_ms = ahora_ms;
        } else if ((unsigned int)(ahora_ms - trabajo.sin_piso_desde_ms) >= (unsigned int)OBS_MS_PISO_NUEVO && franja_ok && !config->piso_fijo) {
            modelo_desde_grupos(&trabajo, &franja);
            trabajo.reaprendidos++;
            resultado->piso_reaprendido = 1;
        }
    } else {
        trabajo.sin_piso_activo = 0;
    }

    /* Diagnóstico del cuadro (PISO_DIAG): brillo medio de la zona de avance, celdas marcadas y el motivo. */
    {
        long suma_brillo = 0;
        int fila_d, columna_d, celdas_zona = 0;

        for (fila_d = fila_minima; fila_d < rejilla.filas; fila_d++) {
            for (columna_d = 0; columna_d < rejilla.columnas; columna_d++) {
                int color_d[3];

                leer_celda(celdas, fila_d, columna_d, color_d);
                suma_brillo += brillo(color_d);
                celdas_zona++;
                resultado->celdas_marcadas += resultado->no_piso[fila_d][columna_d];
            }
        }
        resultado->brillo_medio = (celdas_zona > 0) ? (int)(suma_brillo / (3L * celdas_zona)) : 0;
        if (hay_obstaculo) {
            resultado->motivo |= OBS_MOTIVO_OBSTACULO;
        }
        if (resultado->franja_contaminada) {
            resultado->motivo |= OBS_MOTIVO_FRANJA_TAPADA;
        }
        if (resultado->piso_reaprendido) {
            resultado->motivo |= OBS_MOTIVO_PISO_NUEVO;
        }
        if (resultado->brillo_medio * 3 < BRILLO_OSCURO) {
            resultado->motivo |= OBS_MOTIVO_PISO_OSCURO;
        }
        if (celdas_zona > 0 && resultado->celdas_marcadas * 100 >= celdas_zona * 90) {
            resultado->motivo |= OBS_MOTIVO_TODO_NO_PISO;
        }
    }

    /* El color de referencia que se informa es el del grupo con más celdas. */
    modelo_a_grupos(&trabajo, &piso);
    {
        int principal = 0;

        for (g = 1; g < piso.grupos; g++) {
            if (piso.peso[g] > piso.peso[principal]) {
                principal = g;
            }
        }
        resultado->ref_r = piso.color[principal][0];
        resultado->ref_g = piso.color[principal][1];
        resultado->ref_b = piso.color[principal][2];
    }
    resultado->grupos = piso.grupos;
    for (g = 0; g < piso.grupos; g++) {
        for (c = 0; c < 3; c++) {
            resultado->grupo_color[g][c] = piso.color[g][c];
        }
    }
    *modelo = trabajo;
    return OBS_OK;
}

int obstaculos_analizar_modelo(const ConfigObstaculos *config, ModeloPiso *modelo, const unsigned char *rgb,
                               int ancho, int alto, int avanzando, unsigned int ahora_ms,
                               ResultadoObstaculos *resultado)
{
    MapaColores celdas;
    Rejilla rejilla;
    int codigo;

    if (resultado == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    limpiar_resultado(resultado);
    if (rgb == NULL || modelo == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    codigo = calcular_rejilla(ancho, alto, &rejilla);
    if (codigo != OBS_OK) {
        return codigo;
    }
    if (config == NULL || !config_es_valida(config)) {
        return OBS_ERR_CONFIG;
    }
    calcular_franja(config, &rejilla);
    promediar_celdas(rgb, &rejilla, celdas);
    return analizar_celdas_modelo(config, modelo, celdas, rejilla, avanzando, ahora_ms, resultado);
}

int obstaculos_promediar(const unsigned char *rgb, int ancho, int alto, unsigned char *celdas, int capacidad, int *columnas, int *filas)
{
    Rejilla rejilla;
    MapaColores mapa;
    int codigo, fila, columna, c;

    if (rgb == NULL || celdas == NULL || columnas == NULL || filas == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    codigo = calcular_rejilla(ancho, alto, &rejilla);
    if (codigo != OBS_OK) {
        return codigo;
    }
    if (capacidad < rejilla.columnas * rejilla.filas * 3) {
        return OBS_ERR_TAMANO;
    }
    promediar_celdas(rgb, &rejilla, mapa);
    for (fila = 0; fila < rejilla.filas; fila++) {
        for (columna = 0; columna < rejilla.columnas; columna++) {
            for (c = 0; c < 3; c++) {
                celdas[(fila * rejilla.columnas + columna) * 3 + c] = mapa[fila][columna][c];
            }
        }
    }
    *columnas = rejilla.columnas;
    *filas = rejilla.filas;
    return OBS_OK;
}

int obstaculos_analizar_modelo_celdas(const ConfigObstaculos *config, ModeloPiso *modelo, const unsigned char *celdas, int columnas, int filas,
                                      int avanzando, unsigned int ahora_ms, ResultadoObstaculos *resultado)
{
    MapaColores mapa;
    Rejilla rejilla;
    int codigo, fila, columna, c;

    if (resultado == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    limpiar_resultado(resultado);
    if (celdas == NULL || modelo == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    if (columnas <= 0 || filas <= 0 || columnas > OBS_MAX_COLUMNAS || filas > OBS_MAX_FILAS) {
        return OBS_ERR_TAMANO;
    }
    /* La rejilla de una imagen de (columnas * 10) x (filas * 10) píxeles: la misma que da la imagen original (las filas o columnas sueltas de más no cuentan). */
    codigo = calcular_rejilla(columnas * OBS_LADO_CELDA, filas * OBS_LADO_CELDA, &rejilla);
    if (codigo != OBS_OK) {
        return codigo;
    }
    if (config == NULL || !config_es_valida(config)) {
        return OBS_ERR_CONFIG;
    }
    calcular_franja(config, &rejilla);
    for (fila = 0; fila < filas; fila++) {
        for (columna = 0; columna < columnas; columna++) {
            for (c = 0; c < 3; c++) {
                mapa[fila][columna][c] = celdas[(fila * columnas + columna) * 3 + c];
            }
        }
    }
    return analizar_celdas_modelo(config, modelo, mapa, rejilla, avanzando, ahora_ms, resultado);
}

int obstaculos_analizar_con(const ConfigObstaculos *config, const unsigned char *rgb,
                            int ancho, int alto, ResultadoObstaculos *resultado)
{
    ModeloPiso modelo;

    obstaculos_modelo_iniciar(&modelo);   /* sin memoria: el piso se aprende de este cuadro y se olvida */
    return obstaculos_analizar_modelo(config, &modelo, rgb, ancho, alto, 0, 0, resultado);
}

int obstaculos_analizar(const unsigned char *rgb, int ancho, int alto, ResultadoObstaculos *resultado)
{
    ConfigObstaculos config;

    obstaculos_config_defecto(&config);
    return obstaculos_analizar_con(&config, rgb, ancho, alto, resultado);
}

const char *obstaculos_texto_error(int codigo)
{
    switch (codigo) {
    case OBS_OK:                return "Imagen analizada";
    case OBS_ERR_IMAGEN_NULA:   return "No hay imagen";
    case OBS_ERR_TAMANO:        return "Tamano de imagen no valido (de 30x30 a 640x480)";
    case OBS_ERR_REFERENCIA:    return "El piso de referencia no es uniforme (hay un objeto debajo del carro?)";
    case OBS_ERR_CONFIG:        return "La configuracion no es valida";
    default:                    return "Error desconocido";
    }
}

/* ── Aprender el piso (botón «Aprender piso») ──────────────────────────── */

void obstaculos_aprender_iniciar(AcumuladorPiso *acumulador)
{
    int fila, columna, c;

    if (acumulador == NULL) {
        return;
    }
    for (fila = 0; fila < OBS_MAX_FILAS; fila++) {
        for (columna = 0; columna < OBS_MAX_COLUMNAS; columna++) {
            for (c = 0; c < 3; c++) {
                acumulador->suma[fila][columna][c] = 0;
            }
        }
    }
    acumulador->cuadros = 0;
    acumulador->ancho = 0;
    acumulador->alto = 0;
    acumulador->fila_minima = 0;
}

int obstaculos_aprender_cuadro(AcumuladorPiso *acumulador, const ConfigObstaculos *config, const unsigned char *rgb, int ancho, int alto)
{
    MapaColores celdas;
    Rejilla rejilla;
    int codigo, fila, columna, c, fila_minima;

    if (acumulador == NULL || rgb == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    codigo = calcular_rejilla(ancho, alto, &rejilla);
    if (codigo != OBS_OK) {
        return codigo;
    }
    if (config == NULL || !config_es_valida(config)) {
        return OBS_ERR_CONFIG;
    }
    if (acumulador->cuadros > 0 && (acumulador->ancho != ancho || acumulador->alto != alto)) {
        return OBS_ERR_TAMANO;   /* los cuadros tienen que ser todos del mismo tamaño */
    }
    fila_minima = limitar_entre(proporcion(rejilla.filas, config->fila_minima_mil), 0, rejilla.filas - 1);
    promediar_celdas(rgb, &rejilla, celdas);
    for (fila = fila_minima; fila < rejilla.filas; fila++) {
        for (columna = 0; columna < rejilla.columnas; columna++) {
            for (c = 0; c < 3; c++) {
                acumulador->suma[fila][columna][c] += celdas[fila][columna][c];
            }
        }
    }
    acumulador->ancho = ancho;
    acumulador->alto = alto;
    acumulador->fila_minima = fila_minima;
    acumulador->cuadros++;
    return OBS_OK;
}

int obstaculos_aprender_terminar(const AcumuladorPiso *acumulador, const ConfigObstaculos *config, ModeloPiso *modelo, InformePiso *informe)
{
    static unsigned char muestras[MAX_CELDAS][3];   /* el color promedio de cada celda de la zona (promediado entre los cuadros) */
    static unsigned char en_inmediata[MAX_CELDAS];  /* 1 si la celda cae en la franja inmediata (filas gruesas 8 y 9) */
    Rejilla rejilla;
    GruposPiso grupos, filtrados;
    long suma_brillo = 0;
    int n = 0, i, fila, columna, c, g, intrusos = 0, codigo;

    if (acumulador == NULL || config == NULL || modelo == NULL || informe == NULL) {
        return OBS_ERR_IMAGEN_NULA;
    }
    if (!config_es_valida(config)) {
        return OBS_ERR_CONFIG;
    }
    if (acumulador->cuadros < OBS_APRENDER_MIN_CUADROS) {
        return OBS_ERR_REFERENCIA;   /* muy pocos cuadros para aprender */
    }
    codigo = calcular_rejilla(acumulador->ancho, acumulador->alto, &rejilla);
    if (codigo != OBS_OK) {
        return codigo;
    }
    for (fila = acumulador->fila_minima; fila < rejilla.filas; fila++) {
        int es_abajo = (fila * OBS_MASC_FILAS / rejilla.filas >= 8) ? 1 : 0;

        for (columna = 0; columna < rejilla.columnas; columna++) {
            for (c = 0; c < 3; c++) {
                muestras[n][c] = (unsigned char)(acumulador->suma[fila][columna][c] / acumulador->cuadros);
                suma_brillo += muestras[n][c];
            }
            en_inmediata[n] = (unsigned char)es_abajo;
            n++;
        }
    }
    if (n < 1) {
        return OBS_ERR_TAMANO;
    }
    agrupar_franja(muestras, n, config->grupos_piso, 10, &grupos, &intrusos);
    /* Un color solo es piso si aparece en la franja inmediata (filas gruesas 8 y 9); los grupos que solo están arriba se descartan. */
    memset(&filtrados, 0, sizeof filtrados);
    for (g = 0; g < grupos.grupos; g++) {
        int en_abajo = 0;

        for (i = 0; i < n; i++) {
            if (en_inmediata[i] && centro_mas_cercano(muestras[i], grupos.color, grupos.grupos) == g) {
                int col_i[3] = { muestras[i][0], muestras[i][1], muestras[i][2] };

                if (es_piso(col_i, grupos.color[g], config->umbral_color, config->mejoras_piso)) {
                    en_abajo++;
                }
            }
        }
        if (en_abajo > 0) {
            for (c = 0; c < 3; c++) {
                filtrados.color[filtrados.grupos][c] = grupos.color[g][c];
            }
            filtrados.peso[filtrados.grupos] = grupos.peso[g];
            filtrados.grupos++;
        } else {
            intrusos += grupos.peso[g];
        }
    }
    grupos = filtrados;
    modelo_desde_grupos(modelo, &grupos);
    informe->grupos = grupos.grupos;
    informe->cuadros = acumulador->cuadros;
    informe->brillo_medio = (int)(suma_brillo / (3L * n));
    informe->muy_oscuro = (informe->brillo_medio * 3 < BRILLO_OSCURO) ? 1 : 0;
    informe->descartado_pct = intrusos * 100 / n;
    informe->muy_variado = (informe->descartado_pct > 5 || grupos.grupos >= OBS_MAX_GRUPOS || grupos.grupos == 0) ? 1 : 0;
    for (g = 0; g < OBS_MAX_GRUPOS; g++) {
        for (c = 0; c < 3; c++) {
            informe->color[g][c] = (g < grupos.grupos) ? grupos.color[g][c] : 0;
        }
        informe->peso_pct[g] = (g < grupos.grupos) ? grupos.peso[g] * 100 / n : 0;
    }
    return OBS_OK;
}

/* Agrega un texto al final del buffer (con "+" entre motivos). */
static void agregar_motivo(char *texto, int capacidad, int *largo, const char *nuevo)
{
    if (*largo > 0 && *largo < capacidad - 1) {
        texto[(*largo)++] = '+';
    }
    while (*nuevo != '\0' && *largo < capacidad - 1) {
        texto[(*largo)++] = *nuevo++;
    }
    texto[*largo] = '\0';
}

void obstaculos_texto_motivo(int motivo, char *texto, int capacidad)
{
    int largo = 0;

    if (texto == NULL || capacidad < 2) {
        return;
    }
    texto[0] = '\0';
    if (motivo & OBS_MOTIVO_OBSTACULO) {
        agregar_motivo(texto, capacidad, &largo, "obstaculo");
    }
    if (motivo & OBS_MOTIVO_FRANJA_TAPADA) {
        agregar_motivo(texto, capacidad, &largo, "franja_tapada");
    }
    if (motivo & OBS_MOTIVO_REAJUSTE) {
        agregar_motivo(texto, capacidad, &largo, "reajuste_brillo");
    }
    if (motivo & OBS_MOTIVO_PISO_NUEVO) {
        agregar_motivo(texto, capacidad, &largo, "piso_nuevo");
    }
    if (motivo & OBS_MOTIVO_PISO_OSCURO) {
        agregar_motivo(texto, capacidad, &largo, "piso_oscuro");
    }
    if (motivo & OBS_MOTIVO_TODO_NO_PISO) {
        agregar_motivo(texto, capacidad, &largo, "todo_no_piso");
    }
    if (largo == 0) {
        agregar_motivo(texto, capacidad, &largo, "ninguno");
    }
}
