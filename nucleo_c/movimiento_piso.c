/*
 * movimiento_piso.c — El flujo del piso (ver movimiento_piso.h).
 *
 * GUÍA DE LECTURA
 *  1. flujo_esperado: la geometría. De un punto de la imagen saca a qué distancia del carro está en el piso, le resta lo que avanzó el
 *     carro y calcula dónde queda: ese es el desplazamiento esperado si fuera piso.
 *  2. textura_del_bloque y buscar_bloque: lo medido. Un bloque de 16x16 del cuadro anterior se busca en una ventana del cuadro nuevo.
 *  3. clasificar_bloque: junta las dos cosas y decide FLU_SIN_DATO / FLU_PISO / FLU_CERCA / FLU_DUDOSO.
 *  4. flujo_analizar: recorre los bloques de las filas que sirven, cuenta por zona y deja el resultado.
 *  Solo enteros (long long donde un producto puede pasar de 2 mil millones), sin malloc ni recursión.
 */

#include <stddef.h>
#include <string.h>
#include "movimiento_piso.h"

#define ZONA_IZQ_FIN_MIL    333   /* los mismos tercios que usa el detector de color */
#define ZONA_DER_INICIO_MIL 667
#define MUESTREO              2   /* en la búsqueda se compara un píxel de cada 2 (en x y en y): 8x8 = 64 muestras por bloque */
#define MUESTRAS            ((FLU_BLOQUE / MUESTREO) * (FLU_BLOQUE / MUESTREO))
#define HOLGURA_ARRIBA_PX     2   /* la ventana empieza 2 píxeles ANTES de lo esperado (por si el carro avanzó menos) */
#define HOLGURA_LADOS_PX      3   /* y se busca 3 píxeles a cada lado del desplazamiento horizontal esperado */

/* ── Configuración ─────────────────────────────────────────────────────── */

void flujo_config_defecto(ConfigFlujo *config)
{
    if (config == NULL) {
        return;
    }
    config->activo = 0;
    config->velocidad_mm_s = 193;
    config->ventana_px = 8;
    config->ruido_gradiente = 4;
    config->textura_min = 1;
    config->ajuste_max = 14;
    config->margen_unico = 3;
    config->tolerancia_px4 = 6;
    config->tolerancia_pct = 20;
    config->dt_min_ms = 40;
    config->dt_max_ms = 300;
    config->distancia_max_cm = 60;
    config->min_bloques = 3;
}

static int en_rango(int valor, int minimo, int maximo)
{
    return valor >= minimo && valor <= maximo;
}

int flujo_config_es_valida(const ConfigFlujo *config)
{
    return config != NULL && en_rango(config->activo, 0, 1) && en_rango(config->velocidad_mm_s, 0, 2000) &&
           en_rango(config->ventana_px, 1, 24) && en_rango(config->ruido_gradiente, 0, 255) && en_rango(config->textura_min, 0, 255) && en_rango(config->ajuste_max, 0, 255) &&
           en_rango(config->margen_unico, 0, 255) && en_rango(config->tolerancia_px4, 0, 80) && en_rango(config->tolerancia_pct, 0, 200) &&
           config->dt_min_ms >= 1 && config->dt_max_ms >= config->dt_min_ms && config->dt_max_ms <= 5000 &&
           en_rango(config->distancia_max_cm, 5, 500) && en_rango(config->min_bloques, 1, 100);
}

void flujo_gris_de_rgb(const unsigned char *rgb, int ancho, int alto, unsigned char *gris)
{
    int i, n;

    if (rgb == NULL || gris == NULL || ancho <= 0 || alto <= 0) {
        return;
    }
    n = ancho * alto;
    for (i = 0; i < n; i++) {
        /* luminosidad aproximada: (77 R + 150 G + 29 B) / 256 */
        gris[i] = (unsigned char)((rgb[3 * i] * 77 + rgb[3 * i + 1] * 150 + rgb[3 * i + 2] * 29) >> 8);
    }
}

/* ── 1. Lo esperado (geometría) ────────────────────────────────────────── */

/* Distancia hacia adelante (en mm) del piso que se ve en la fila v, o -1 si es el horizonte o más arriba.
 * d = altura * (f * cos - y * sen) / (f * sen + y * cos), con y = v - cy (las milésimas de sen y cos se cancelan). */
static long long distancia_mm_de_fila(const Corredor *c, int v)
{
    long long y = v - c->cy;
    long long a = (long long)c->f * c->seno_mil + y * c->coseno_mil;
    long long arriba = (long long)c->f * c->coseno_mil - y * c->seno_mil;

    if (a <= 0) {
        return -1;
    }
    return 10LL * c->config.altura_camara_cm * arriba / a;
}

int flujo_esperado(const Corredor *corredor, int u, int v, int avance_mm, int *dx4, int *dy4)
{
    long long y, a, d, d2, num, den, y4_nuevo, a_nuevo;
    long long altura_mm;

    if (corredor == NULL || dx4 == NULL || dy4 == NULL) {
        return 0;
    }
    y = v - corredor->cy;
    a = (long long)corredor->f * corredor->seno_mil + y * corredor->coseno_mil;
    d = distancia_mm_de_fila(corredor, v);
    if (a <= 0 || d <= 0) {
        return 0;                                  /* sobre el horizonte: no es piso */
    }
    d2 = d - avance_mm;
    if (d2 <= 0) {
        return 0;                                  /* el punto ya quedó debajo del carro */
    }
    altura_mm = 10LL * corredor->config.altura_camara_cm;
    /* La fila de un punto del piso a distancia d: y = f * (altura * cos - d * sen) / (d * cos + altura * sen). */
    num = (long long)corredor->f * (altura_mm * corredor->coseno_mil - d2 * corredor->seno_mil);
    den = d2 * corredor->coseno_mil + altura_mm * corredor->seno_mil;
    if (den <= 0) {
        return 0;
    }
    y4_nuevo = 4 * num / den;                       /* la fila nueva (relativa al centro), en cuartos de píxel */
    if (corredor->cy + y4_nuevo / 4 >= corredor->alto) {
        return 0;                                  /* salió por abajo de la imagen */
    }
    *dy4 = (int)(y4_nuevo - 4 * y);
    /* A lo ancho el punto se abre en la misma proporción en que crece A(v): u' - cx = (u - cx) * A' / A. */
    a_nuevo = (long long)corredor->f * corredor->seno_mil + (y4_nuevo * corredor->coseno_mil) / 4;
    *dx4 = (int)(4LL * (u - corredor->cx) * (a_nuevo - a) / a);
    return 1;
}

/* ── 2. Lo medido ──────────────────────────────────────────────────────── */

/* Textura del bloque que empieza en (x0, y0): lo que las diferencias entre vecinos (horizontal + vertical) PASAN del nivel de ruido, en promedio por píxel.
 * Un bloque liso o con puro ruido da 0; uno con un borde marcado da varias unidades. */
static int textura_del_bloque(const unsigned char *gris, int ancho, int x0, int y0, int ruido)
{
    int i, j, suma = 0;

    for (j = 0; j < FLU_BLOQUE - 1; j++) {
        for (i = 0; i < FLU_BLOQUE - 1; i++) {
            int p = gris[(y0 + j) * ancho + x0 + i];
            int dx = p - gris[(y0 + j) * ancho + x0 + i + 1];
            int dy = p - gris[(y0 + j + 1) * ancho + x0 + i];
            dx = (dx < 0) ? -dx : dx;
            dy = (dy < 0) ? -dy : dy;
            suma += (dx > ruido ? dx - ruido : 0) + (dy > ruido ? dy - ruido : 0);
        }
    }
    return suma / ((FLU_BLOQUE - 1) * (FLU_BLOQUE - 1));
}

/* SAD (en muestras de 1 de cada 2 píxeles) del bloque anterior en (x0, y0) contra el actual desplazado (dx, dy). */
static int sad_bloque(const unsigned char *anterior, const unsigned char *actual, int ancho, int x0, int y0, int dx, int dy)
{
    int i, j, suma = 0;

    for (j = 0; j < FLU_BLOQUE; j += MUESTREO) {
        for (i = 0; i < FLU_BLOQUE; i += MUESTREO) {
            int d = anterior[(y0 + j) * ancho + x0 + i] - actual[(y0 + j + dy) * ancho + x0 + i + dx];
            suma += (d < 0) ? -d : d;
        }
    }
    return suma;
}

/* ── 3. Decidir ────────────────────────────────────────────────────────── */

/* Evalúa un bloque y devuelve su estado; deja en *dy4_medido el desplazamiento vertical medido (cuartos de píxel). */
static int clasificar_bloque(const ConfigFlujo *config, const unsigned char *anterior, const unsigned char *actual, int ancho, int alto,
                             int bx, int by, int dx4_esp, int dy4_esp, int *dy4_medido)
{
    int x0 = bx * FLU_BLOQUE, y0 = by * FLU_BLOQUE;
    int dx_centro = (dx4_esp >= 0) ? (dx4_esp + 2) / 4 : -((-dx4_esp + 2) / 4);
    int dy_centro = (dy4_esp >= 0) ? (dy4_esp + 2) / 4 : -((-dy4_esp + 2) / 4);
    int dx, dy, mejor = 1 << 30, mejor_dx = 0, mejor_dy = 0, segundo = 1 << 30;
    int tolerancia4, diferencia4, diferencia_x4, medido_dy4;

    *dy4_medido = 0;
    if (textura_del_bloque(anterior, ancho, x0, y0, config->ruido_gradiente) < config->textura_min) {
        return FLU_SIN_DATO;                       /* piso liso o negro: nada que seguir */
    }
    /* Primera pasada: la mejor coincidencia. */
    for (dy = dy_centro - HOLGURA_ARRIBA_PX; dy <= dy_centro + config->ventana_px; dy++) {
        for (dx = dx_centro - HOLGURA_LADOS_PX; dx <= dx_centro + HOLGURA_LADOS_PX; dx++) {
            int s;
            if (x0 + dx < 0 || x0 + dx + FLU_BLOQUE > ancho || y0 + dy < 0 || y0 + dy + FLU_BLOQUE > alto) {
                continue;
            }
            s = sad_bloque(anterior, actual, ancho, x0, y0, dx, dy);
            if (s < mejor) {
                mejor = s;
                mejor_dx = dx;
                mejor_dy = dy;
            }
        }
    }
    if (mejor == (1 << 30)) {
        return FLU_SIN_DATO;                       /* ningún candidato cabía dentro de la imagen */
    }
    /* Segunda pasada: la mejor coincidencia que esté LEJOS de la mejor (a más de 1 píxel). Si casi empata, la mejor no es única. */
    for (dy = dy_centro - HOLGURA_ARRIBA_PX; dy <= dy_centro + config->ventana_px; dy++) {
        for (dx = dx_centro - HOLGURA_LADOS_PX; dx <= dx_centro + HOLGURA_LADOS_PX; dx++) {
            int s, ddx = dx - mejor_dx, ddy = dy - mejor_dy;
            if ((ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1) ||
                x0 + dx < 0 || x0 + dx + FLU_BLOQUE > ancho || y0 + dy < 0 || y0 + dy + FLU_BLOQUE > alto) {
                continue;
            }
            s = sad_bloque(anterior, actual, ancho, x0, y0, dx, dy);
            if (s < segundo) {
                segundo = s;
            }
        }
    }
    /* Afinado de medio píxel: una parábola por los tres SAD de la mejor y sus vecinos de arriba y de abajo (mismo dx) da el desplazamiento en cuartos de píxel. */
    medido_dy4 = 4 * mejor_dy;
    if (y0 + mejor_dy - 1 >= 0 && y0 + mejor_dy + 1 + FLU_BLOQUE <= alto) {
        int arriba = sad_bloque(anterior, actual, ancho, x0, y0, mejor_dx, mejor_dy - 1);
        int abajo = sad_bloque(anterior, actual, ancho, x0, y0, mejor_dx, mejor_dy + 1);
        int curva = arriba - 2 * mejor + abajo;      /* > 0 si la mejor es un mínimo de verdad */
        if (curva > 0 && arriba > mejor && abajo > mejor) {
            int corrimiento4 = (2 * (arriba - abajo) * 4 + (arriba - abajo > 0 ? curva : -curva)) / (4 * curva);   /* 4 * (arriba - abajo) / (2 * curva), redondeado */
            if (corrimiento4 > 2) {
                corrimiento4 = 2;
            } else if (corrimiento4 < -2) {
                corrimiento4 = -2;
            }
            medido_dy4 += corrimiento4;
        }
    }
    *dy4_medido = medido_dy4;
    if (mejor > config->ajuste_max * MUESTRAS) {
        return FLU_DUDOSO;                         /* no se parece a nada: se tapó o cambió la luz */
    }
    if (segundo != (1 << 30) && segundo - mejor < config->margen_unico * MUESTRAS) {
        return FLU_SIN_DATO;                       /* la mejor no es única: borde recto, dibujo repetido o ruido */
    }
    tolerancia4 = config->tolerancia_px4 + ((dy4_esp < 0 ? -dy4_esp : dy4_esp) * config->tolerancia_pct) / 100;
    diferencia4 = medido_dy4 - dy4_esp;
    diferencia_x4 = 4 * mejor_dx - dx4_esp;
    if (diferencia4 > tolerancia4) {
        return FLU_CERCA;                          /* se movió claramente MÁS que el piso */
    }
    if (diferencia4 >= -tolerancia4 && diferencia_x4 >= -tolerancia4 - 8 && diferencia_x4 <= tolerancia4 + 8) {
        return FLU_PISO;
    }
    return FLU_DUDOSO;
}

/* ── 4. Analizar un par de cuadros ─────────────────────────────────────── */

static int zona_de_columna(int bx, int ancho)
{
    int x_mil = (bx * FLU_BLOQUE + FLU_BLOQUE / 2) * 1000 / ancho;

    if (x_mil < ZONA_IZQ_FIN_MIL) {
        return ZONA_IZQUIERDA;
    }
    return (x_mil < ZONA_DER_INICIO_MIL) ? ZONA_CENTRO : ZONA_DERECHA;
}

int flujo_analizar(const ConfigFlujo *config, const Corredor *corredor, const unsigned char *gris_anterior, const unsigned char *gris_actual,
                   int ancho, int alto, int avanzando, int dt_ms, ResultadoFlujo *resultado)
{
    int bx, by, z;

    if (resultado == NULL) {
        return FLU_ERR_ARGUMENTO;
    }
    memset(resultado, 0, sizeof(*resultado));
    if (config == NULL || corredor == NULL || gris_anterior == NULL || gris_actual == NULL || ancho < 48 || alto < 48 || ancho > 640 || alto > 480 ||
        corredor->ancho != ancho || corredor->alto != alto) {
        return FLU_ERR_ARGUMENTO;
    }
    if (!flujo_config_es_valida(config)) {
        return FLU_ERR_CONFIG;
    }
    resultado->columnas = ancho / FLU_BLOQUE;
    resultado->filas = alto / FLU_BLOQUE;
    if (!config->activo) {
        resultado->motivo = FLU_MOT_APAGADO;
        return FLU_OK;
    }
    if (!avanzando || config->velocidad_mm_s <= 0) {
        resultado->motivo = FLU_MOT_QUIETO;
        return FLU_OK;
    }
    if (dt_ms < config->dt_min_ms || dt_ms > config->dt_max_ms) {
        resultado->motivo = FLU_MOT_TIEMPO;
        return FLU_OK;
    }
    resultado->evaluado = 1;
    resultado->motivo = FLU_MOT_EVALUADO;
    resultado->avance_mm = config->velocidad_mm_s * dt_ms / 1000;
    resultado->fila_inicial = resultado->filas;   /* se corrigen abajo con las filas que sí se evalúen */
    resultado->fila_final = 0;

    for (by = 0; by < resultado->filas; by++) {
        int v_centro = by * FLU_BLOQUE + FLU_BLOQUE / 2;
        long long d_mm = distancia_mm_de_fila(corredor, v_centro);
        int dx4_esp = 0, dy4_esp = 0;

        if (d_mm <= 0 || d_mm > 10LL * config->distancia_max_cm) {
            continue;                              /* piso demasiado lejano (casi no se mueve) o sobre el horizonte */
        }
        for (bx = 0; bx < resultado->columnas; bx++) {
            int estado, dy4_medido = 0, ok;

            ok = flujo_esperado(corredor, bx * FLU_BLOQUE + FLU_BLOQUE / 2, v_centro, resultado->avance_mm, &dx4_esp, &dy4_esp);
            /* Con lo esperado el bloque entero (y la ventana de búsqueda) debe caber en la imagen. */
            if (!ok || by * FLU_BLOQUE + FLU_BLOQUE + (dy4_esp + 3) / 4 + config->ventana_px >= alto) {
                continue;
            }
            if (by < resultado->fila_inicial) {
                resultado->fila_inicial = by;
            }
            if (by + 1 > resultado->fila_final) {
                resultado->fila_final = by + 1;
            }
            estado = clasificar_bloque(config, gris_anterior, gris_actual, ancho, alto, bx, by, dx4_esp, dy4_esp, &dy4_medido);
            resultado->estado[by][bx] = (unsigned char)estado;
            resultado->medido_dy4[by][bx] = dy4_medido;
            resultado->esperado_dy4[by][bx] = dy4_esp;
            z = zona_de_columna(bx, ancho);
            switch (estado) {
            case FLU_PISO:
                resultado->zona[z].piso++;
                resultado->total_piso++;
                break;
            case FLU_CERCA: {
                int fondo_mil = (by + 1) * FLU_BLOQUE * 1000 / alto;
                int cercania = (fondo_mil >= OBS_CERCA_MIL) ? 3 : (fondo_mil >= OBS_MEDIA_MIL) ? 2 : 1;
                resultado->zona[z].cerca++;
                resultado->total_cerca++;
                if (cercania > resultado->zona[z].cercania) {
                    resultado->zona[z].cercania = cercania;
                }
                break;
            }
            case FLU_DUDOSO:
                resultado->zona[z].dudoso++;
                resultado->total_dudoso++;
                break;
            default:
                resultado->zona[z].sin_dato++;
                resultado->total_sin_dato++;
                break;
            }
        }
    }
    if (resultado->fila_inicial > resultado->fila_final) {
        resultado->fila_inicial = resultado->fila_final;   /* ninguna fila sirvió */
    }
    for (z = 0; z < OBS_ZONAS; z++) {
        ZonaFlujo *zona = &resultado->zona[z];
        int con_dato = zona->piso + zona->cerca;

        zona->hay_cerca = (zona->cerca > 0) ? 1 : 0;
        zona->piso_consistente = (con_dato >= config->min_bloques && zona->cerca == 0 && zona->dudoso * 2 <= con_dato) ? 1 : 0;
    }
    return FLU_OK;
}

int flujo_cajas(const ResultadoFlujo *resultado, CajaImagen *cajas, int maximo)
{
    int bx, by, n = 0;

    if (resultado == NULL || cajas == NULL || maximo <= 0 || !resultado->evaluado) {
        return 0;
    }
    for (by = 0; by < resultado->filas && n < maximo; by++) {
        for (bx = 0; bx < resultado->columnas && n < maximo; bx++) {
            if (resultado->estado[by][bx] == FLU_CERCA) {
                cajas[n].x0 = bx * FLU_BLOQUE;
                cajas[n].y0 = by * FLU_BLOQUE;
                cajas[n].x1 = (bx + 1) * FLU_BLOQUE;
                cajas[n].y1 = (by + 1) * FLU_BLOQUE;
                cajas[n].peso = 1;
                cajas[n].fuente = COR_FUENTE_FLUJO;
                n++;
            }
        }
    }
    return n;
}
