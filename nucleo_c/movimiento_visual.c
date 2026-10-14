/*
 * movimiento_visual.c — Giro del carro medido con la cámara (ver movimiento_visual.h).
 *
 * GUÍA DE LECTURA
 *  1. mv_huella(): convierte la mitad superior de un cuadro en una huella de una línea (el brillo sumado de cada columna).
 *  2. mv_corrimiento(): compara dos huellas. Primero las pasa a "bordes" (la diferencia entre columnas vecinas, así un
 *     cambio de luz general no importa), después prueba todos los corrimientos de -rango a +rango midiendo cuánto
 *     difieren (suma de diferencias absolutas, dividida entre las columnas que se traslapan), y se queda con el
 *     de menor costo. Decide si es CONFIABLE con tres pruebas: que la huella tenga bordes (no una pared lisa), que el
 *     mejor costo sea claramente menor que el promedio de todos, y que no haya otro mínimo lejano casi igual
 *     de bueno (textura que se repite, como un piso de baldosas o rayas). El subpíxel es una parábola por los tres
 *     costos alrededor del mínimo.
 *  3. mv_corrimiento_a_grados_x10(): píxeles → grados con el campo de visión.
 *  4. mv_procesar(): lo que usa la app con cada cuadro: compara la huella nueva con la de REFERENCIA (el último cuadro con resultado
 *     válido). Si un cuadro se rechaza, la referencia se conserva y el siguiente se compara contra ella (así el giro de los cuadros
 *     rechazados no se pierde: `salida->cuadros` dice cuántos cuadros abarca el corrimiento). Si una coincidencia es ambigua pero cae cerca de
 *     lo que se esperaba por el movimiento anterior (predicción), se acepta.
 *  Solo enteros, arreglos de tamaño fijo, sin malloc ni recursión.
 */

#include <stddef.h>
#include "movimiento_visual.h"

/* ── Utilidades ────────────────────────────────────────────────────────── */

static int valor_absoluto(int x)
{
    return (x < 0) ? -x : x;
}

void mv_iniciar(EstimadorMovimiento *estimador)
{
    if (estimador == NULL) {
        return;
    }
    estimador->tiene_previo = 0;
    estimador->ancho = 0;
    estimador->cuadros_ref = 1;
    estimador->tiene_pred = 0;
    estimador->pred_x16 = 0;
}

static void limpiar_salida(MovimientoVisual *salida)
{
    salida->valido = 0;
    salida->motivo = MV_SIN_PREVIO;
    salida->corrimiento_x16 = 0;
    salida->confianza_mil = 0;
    salida->giro_grados_x10 = 0;
    salida->energia = 0;
    salida->cuadros = 1;
    salida->por_prediccion = 0;
}

/* ── La huella de un cuadro ────────────────────────────────────────────── */

int mv_huella(const unsigned char *rgb, int ancho, int alto, int *huella)
{
    int x, y;
    int filas;

    if (rgb == NULL || huella == NULL) {
        return MV_ERR_ARGUMENTO;
    }
    if (ancho < MV_MIN_ANCHO || ancho > MV_MAX_ANCHO || alto < 2) {
        return MV_ERR_TAMANO;
    }
    filas = alto / 2;   /* solo la mitad de arriba: el horizonte se mueve más limpio que el piso */
    for (x = 0; x < ancho; x++) {
        int suma = 0;

        for (y = 0; y < filas; y++) {
            const unsigned char *p = rgb + (y * ancho + x) * 3;

            suma += p[0] + p[1] + p[2];
        }
        huella[x] = suma / 3;
    }
    return MV_OK;
}

/* ── Comparar dos huellas ──────────────────────────────────────────────── */

/* Pasa la huella a "bordes": la diferencia entre columnas vecinas (n - 1 valores). Devuelve la energía media. */
static int huella_a_bordes(const int *huella, int n, int *bordes)
{
    long suma = 0;
    int x;

    for (x = 0; x < n - 1; x++) {
        bordes[x] = huella[x + 1] - huella[x];
        suma += valor_absoluto(bordes[x]);
    }
    return (int)(suma / (n - 1));
}

int mv_corrimiento(const int *anterior, const int *nueva, int n, int rango, MovimientoVisual *salida)
{
    return mv_corrimiento_pred(anterior, nueva, n, rango, 0, 0, salida);
}

int mv_corrimiento_pred(const int *anterior, const int *nueva, int n, int rango, int tiene_pred, int pred_x16, MovimientoVisual *salida)
{
    int bordes_a[MV_MAX_ANCHO], bordes_b[MV_MAX_ANCHO];
    int costos[2 * MV_MAX_ANCHO / 2 + 3];   /* un costo por cada corrimiento de -rango a +rango */
    int m, energia_a, energia_b, rango_minimo, s, mejor, i;
    long suma_costos = 0;
    int costo_medio, costo_mejor, costo_segundo;

    if (salida == NULL) {
        return MV_ERR_ARGUMENTO;
    }
    limpiar_salida(salida);
    if (anterior == NULL || nueva == NULL) {
        return MV_ERR_ARGUMENTO;
    }
    if (n < MV_MIN_ANCHO || n > MV_MAX_ANCHO) {
        return MV_ERR_TAMANO;
    }
    m = n - 1;
    energia_a = huella_a_bordes(anterior, n, bordes_a);
    energia_b = huella_a_bordes(nueva, n, bordes_b);
    salida->energia = (energia_a < energia_b) ? energia_a : energia_b;
    if (salida->energia < MV_ENERGIA_MIN) {
        salida->motivo = MV_ESCENA_UNIFORME;
        return MV_OK;
    }

    /* Rango de búsqueda: al menos un tercio del ancho (con ~12 cuadros por segundo un giro rápido corre mucho), y nunca
     * más de la mitad de los bordes (hay que dejar al menos la mitad traslapada). */
    rango_minimo = n / 3;
    if (rango < rango_minimo) {
        rango = rango_minimo;
    }
    if (rango > m / 2) {
        rango = m / 2;
    }

    /* Costo de cada corrimiento s: el contenido de la columna x del cuadro anterior aparece en la x + s del nuevo. */
    for (s = -rango; s <= rango; s++) {
        int desde = (s < 0) ? -s : 0;
        int hasta = (s > 0) ? m - s : m;
        long suma = 0;
        int x;

        for (x = desde; x < hasta; x++) {
            suma += valor_absoluto(bordes_a[x] - bordes_b[x + s]);
        }
        costos[s + rango] = (int)(suma * 16 / (hasta - desde));   /* costo por columna, por 16 */
        suma_costos += costos[s + rango];
    }
    mejor = 0;
    for (i = 1; i <= 2 * rango; i++) {
        if (costos[i] < costos[mejor]) {
            mejor = i;
        }
    }
    costo_mejor = costos[mejor];
    costo_medio = (int)(suma_costos / (2 * rango + 1));
    costo_segundo = -1;
    for (i = 0; i <= 2 * rango; i++) {
        if (valor_absoluto(i - mejor) >= 3 && (costo_segundo < 0 || costos[i] < costo_segundo)) {
            costo_segundo = costos[i];   /* el mejor mínimo que NO es la misma cuenca que el primero */
        }
    }
    salida->confianza_mil = (costo_medio > 0) ? (costo_medio - costo_mejor) * 1000 / costo_medio : 0;
    if (salida->confianza_mil < 0) {
        salida->confianza_mil = 0;
    }

    if (mejor == 0 || mejor == 2 * rango) {
        salida->motivo = MV_FUERA_DE_RANGO;   /* el mínimo toca el borde: el corrimiento real puede ser mayor */
        return MV_OK;
    }
    if ((long)costo_mejor * 100 > (long)costo_medio * MV_MEJOR_MAX_PCT) {
        salida->motivo = MV_COINCIDENCIA_DEBIL;
        return MV_OK;
    }
    if (costo_segundo >= 0 && (long)costo_mejor * 100 > (long)costo_segundo * MV_SEGUNDO_MAX_PCT) {
        /* Ambiguo: hay otra coincidencia casi igual. Se acepta solo si esta cae cerca de lo que se esperaba por el movimiento anterior. */
        int esperado = pred_x16;
        int tolerancia = valor_absoluto(esperado) * MV_PRED_TOL_PCT / 100;
        int elegido = -1;

        if (tolerancia < MV_PRED_TOL_MIN_X16) {
            tolerancia = MV_PRED_TOL_MIN_X16;
        }
        if (tiene_pred) {
            /* Entre los mínimos locales buenos (costo de hasta el doble del mejor y que pasan la prueba de coincidencia débil),
             * el más cercano a lo esperado, si está dentro de la tolerancia. */
            int distancia_elegida = 0;

            for (i = 1; i < 2 * rango; i++) {
                int distancia;

                if (costos[i] > costos[i - 1] || costos[i] > costos[i + 1] || costos[i] > 2 * costo_mejor ||
                    (long)costos[i] * 100 > (long)costo_medio * MV_MEJOR_MAX_PCT) {
                    continue;
                }
                distancia = valor_absoluto((i - rango) * 16 - esperado);
                if (distancia <= tolerancia && (elegido < 0 || distancia < distancia_elegida)) {
                    elegido = i;
                    distancia_elegida = distancia;
                }
            }
        }
        if (elegido < 0) {
            salida->motivo = MV_AMBIGUO;
            return MV_OK;
        }
        mejor = elegido;
        salida->por_prediccion = 1;
    }

    /* Subpíxel: el mínimo de una parábola por los tres costos de alrededor. */
    {
        int c0 = costos[mejor - 1];
        int c1 = costos[mejor];
        int c2 = costos[mejor + 1];
        int denominador = c0 - 2 * c1 + c2;
        int desfase_x16 = 0;

        if (denominador > 0) {
            desfase_x16 = (int)((long)16 * (c0 - c2) / (2L * denominador));
            if (desfase_x16 > 8) {
                desfase_x16 = 8;      /* medio píxel como máximo hacia cada lado */
            } else if (desfase_x16 < -8) {
                desfase_x16 = -8;
            }
        }
        salida->corrimiento_x16 = (mejor - rango) * 16 + desfase_x16;
    }
    salida->valido = 1;
    salida->motivo = MV_OK;
    return MV_OK;
}

int mv_corrimiento_a_grados_x10(int corrimiento_x16, int ancho, int fov_x10)
{
    long numerador, denominador;

    if (ancho <= 0) {
        return 0;
    }
    numerador = -(long)corrimiento_x16 * fov_x10;   /* el contenido va a la izquierda cuando el carro gira a la derecha */
    denominador = 16L * ancho;
    if (numerador >= 0) {
        return (int)((numerador + denominador / 2) / denominador);
    }
    return (int)-((-numerador + denominador / 2) / denominador);
}

/* Deja la huella dada como cuadro de referencia. */
static void guardar_referencia(EstimadorMovimiento *estimador, const int *huella, int ancho)
{
    int x;

    for (x = 0; x < ancho; x++) {
        estimador->huella[x] = huella[x];
    }
    estimador->ancho = ancho;
    estimador->tiene_previo = 1;
    estimador->cuadros_ref = 1;
}

int mv_procesar(EstimadorMovimiento *estimador, const unsigned char *rgb, int ancho, int alto, int fov_x10,
                MovimientoVisual *salida)
{
    int nueva[MV_MAX_ANCHO];
    int codigo;

    if (estimador == NULL || salida == NULL) {
        return MV_ERR_ARGUMENTO;
    }
    limpiar_salida(salida);
    codigo = mv_huella(rgb, ancho, alto, nueva);
    if (codigo != MV_OK) {
        return codigo;
    }
    if (estimador->tiene_previo && estimador->ancho == ancho) {
        /* Si ya hubo cuadros rechazados, el corrimiento esperado abarca varios cuadros. */
        int esperado = estimador->pred_x16 * estimador->cuadros_ref;

        codigo = mv_corrimiento_pred(estimador->huella, nueva, ancho, ancho * 2 / 5, estimador->tiene_pred, esperado, salida);
        salida->cuadros = estimador->cuadros_ref;
        if (codigo == MV_OK && salida->valido) {
            salida->giro_grados_x10 = mv_corrimiento_a_grados_x10(salida->corrimiento_x16, ancho, fov_x10);
            estimador->pred_x16 = salida->corrimiento_x16 / estimador->cuadros_ref;
            estimador->tiene_pred = 1;
            guardar_referencia(estimador, nueva, ancho);   /* el cuadro nuevo pasa a ser la referencia */
        } else if (estimador->cuadros_ref >= MV_MAX_SALTO) {
            estimador->tiene_pred = 0;                       /* demasiados rechazos seguidos: se empieza de nuevo con este cuadro */
            guardar_referencia(estimador, nueva, ancho);
        } else {
            estimador->cuadros_ref++;                        /* se rechazó: la referencia sigue siendo la anterior */
        }
    } else {
        guardar_referencia(estimador, nueva, ancho);
        estimador->tiene_pred = 0;
    }
    return MV_OK;
}
