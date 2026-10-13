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
 * Llena `grupos` (colores y pesos) y devuelve el desvío máximo: lo más lejos que quedó una muestra de
 * su grupo (para decidir si la franja es uniforme). */
static int agrupar_franja(unsigned char muestras[][3], int n, int k_max, GruposPiso *grupos)
{
    int centros[OBS_MAX_GRUPOS][3];
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

        if (d > desvio) {
            desvio = d;
        }
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
static int grupos_de_la_franja(MapaColores celdas, const Rejilla *rejilla, int k_max, GruposPiso *grupos)
{
    unsigned char muestras[MAX_CELDAS][3];
    int n = reunir_franja(celdas, rejilla, muestras);
    int desvio = agrupar_franja(muestras, n, k_max, grupos);

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
}

/* Deja en el modelo exactamente los grupos dados (aprende desde cero). */
static void modelo_desde_grupos(ModeloPiso *modelo, const GruposPiso *grupos)
{
    int g, c;
    int reaprendidos = modelo->reaprendidos;

    obstaculos_modelo_iniciar(modelo);
    modelo->reaprendidos = reaprendidos;
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
static int es_piso(const int color[3], const int referencia[3], int umbral_color)
{
    if (distancia_color(color, referencia) <= umbral_color) {
        return 1;
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
        if (es_piso(color, piso->color[a], umbral_color)) {
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
    resultado->piso_reaprendido = 0;
    resultado->aprendio = 0;
    resultado->celdas_borde = 0;
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
           (config->usar_bordes == 0 || config->usar_bordes == 1);
}

int obstaculos_analizar_modelo(const ConfigObstaculos *config, ModeloPiso *modelo, const unsigned char *rgb,
                               int ancho, int alto, int avanzando, unsigned int ahora_ms,
                               ResultadoObstaculos *resultado)
{
    MapaColores celdas;
    MapaCeldas candidatas;
    Rejilla rejilla;
    GruposPiso franja, piso;
    ModeloPiso trabajo;
    int fila_minima, codigo, franja_ok, g, c;
    int piso_celdas, total_celdas, hay_obstaculo;

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

    /* El modelo se trabaja en una copia: si hay un error no cambia. */
    trabajo = *modelo;
    franja_ok = (grupos_de_la_franja(celdas, &rejilla, config->grupos_piso, &franja) == OBS_OK);
    if (!trabajo.valido) {
        if (!franja_ok) {
            return OBS_ERR_REFERENCIA;   /* nada que recordar y la franja no sirve */
        }
        modelo_desde_grupos(&trabajo, &franja);
    } else if (franja_ok) {
        modelo_agregar_grupos(&trabajo, &franja, config->grupos_piso);
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

    modelo_a_grupos(&trabajo, &piso);
    resultado->celdas_borde = marcar_candidatas(celdas, &rejilla, &piso, config->umbral_color, config->usar_bordes, candidatas);
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
        } else if ((unsigned int)(ahora_ms - trabajo.sin_piso_desde_ms) >= (unsigned int)OBS_MS_PISO_NUEVO && franja_ok) {
            modelo_desde_grupos(&trabajo, &franja);
            trabajo.reaprendidos++;
            resultado->piso_reaprendido = 1;
        }
    } else {
        trabajo.sin_piso_activo = 0;
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
