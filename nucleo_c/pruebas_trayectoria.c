/*
 * pruebas_trayectoria.c — Pruebas de trayectoria.c en PC.
 *
 * GUÍA DE LECTURA
 *  1. Cada caso arma una ruta (lista de puntos) y la lista de pasos esperada.
 *  2. verificar_ruta() calcula la trayectoria y compara paso por paso.
 *  3. Los tiempos esperados se calculan con MS_POR_CM y MS_POR_GRADO, así que
 *     las pruebas siguen valiendo cuando se reemplace la calibración.
 *  4. Los errores se prueban con verificar_error() (código e índice).
 *  Convención: fila crece hacia abajo; "arriba" es fila menor.
 */

#include <stdio.h>
#include "trayectoria.h"
#include "calibracion.h"

static int casos  = 0;
static int fallos = 0;

/* Paso esperado: avanzar `cm` centímetros. */
static Paso avanzar(int cm)
{
    Paso p = { PASO_AVANZAR, cm, cm * MS_POR_CM };
    return p;
}

/* Paso esperado: girar `grados` (positivo = derecha). */
static Paso girar(int grados)
{
    Paso p = { PASO_GIRAR, grados, (grados < 0 ? -grados : grados) * MS_POR_GRADO };
    return p;
}

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

static int pasos_iguales(const Paso *a, const Paso *b)
{
    return a->tipo == b->tipo && a->valor == b->valor && a->tiempo_ms == b->tiempo_ms;
}

/* Calcula la ruta y compara con los pasos esperados. */
static void verificar_ruta(const char *nombre, const Punto *puntos, int n_puntos,
                           const Paso *esperados, int n_esperados)
{
    Paso obtenidos[MAX_PASOS];
    int n_obtenidos = -1;
    int indice = -1;
    int ok;
    int i;

    int codigo = trayectoria_calcular(puntos, n_puntos, obtenidos, MAX_PASOS,
                                      &n_obtenidos, &indice);
    ok = (codigo == TRAY_OK && n_obtenidos == n_esperados);
    for (i = 0; ok && i < n_esperados; i++) {
        ok = pasos_iguales(&obtenidos[i], &esperados[i]);
    }
    caso(nombre, ok);
    if (!ok) {
        printf("      código=%d, pasos obtenidos=%d, esperados=%d\n", codigo, n_obtenidos, n_esperados);
        for (i = 0; i < n_obtenidos; i++) {
            printf("      [%d] tipo=%d valor=%d ms=%d\n", i, obtenidos[i].tipo,
                   obtenidos[i].valor, obtenidos[i].tiempo_ms);
        }
    }
}

/* Calcula la ruta y espera un código de error concreto (y su índice). */
static void verificar_error(const char *nombre, const Punto *puntos, int n_puntos,
                            int codigo_esperado, int indice_esperado)
{
    Paso obtenidos[MAX_PASOS];
    int n_obtenidos = -1;
    int indice = -1;
    int codigo = trayectoria_calcular(puntos, n_puntos, obtenidos, MAX_PASOS,
                                      &n_obtenidos, &indice);

    caso(nombre, codigo == codigo_esperado && n_obtenidos == 0 && indice == indice_esperado);
}

/* ── Casos ─────────────────────────────────────────────────────────────── */

static void prueba_linea_recta(void)
{
    const Punto ruta[] = { {0, 5}, {0, 4}, {0, 3}, {0, 2} };   /* 3 celdas hacia arriba */
    const Paso esperado[] = { avanzar(3 * CELDA_CM) };
    verificar_ruta("línea recta: 3 celdas hacia arriba, sin giro",
                   ruta, 4, esperado, 1);
}

static void prueba_giro_derecha(void)
{
    const Punto ruta[] = { {0, 3}, {0, 2}, {1, 2}, {2, 2} };   /* arriba 1, derecha 2 */
    const Paso esperado[] = { avanzar(CELDA_CM), girar(90), avanzar(2 * CELDA_CM) };
    verificar_ruta("giro a la derecha: +90 grados", ruta, 4, esperado, 3);
}

static void prueba_giro_izquierda(void)
{
    const Punto ruta[] = { {3, 3}, {3, 2}, {2, 2}, {1, 2} };   /* arriba 1, izquierda 2 */
    const Paso esperado[] = { avanzar(CELDA_CM), girar(-90), avanzar(2 * CELDA_CM) };
    verificar_ruta("giro a la izquierda: -90 grados", ruta, 4, esperado, 3);
}

static void prueba_ruta_en_u(void)
{
    /* Arriba 2, derecha 2, abajo 2. */
    const Punto ruta[] = { {0, 2}, {0, 1}, {0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2} };
    const Paso esperado[] = {
        avanzar(2 * CELDA_CM), girar(90), avanzar(2 * CELDA_CM),
        girar(90), avanzar(2 * CELDA_CM)
    };
    verificar_ruta("ruta en U: dos giros a la derecha", ruta, 7, esperado, 5);
}

static void prueba_un_solo_punto(void)
{
    const Punto ruta[] = { {4, 4} };
    verificar_ruta("un solo punto: ruta vacía, sin pasos", ruta, 1, NULL, 0);
}

static void prueba_diagonales(void)
{
    /* Dos celdas hacia arriba-derecha: giro +45 y 2*20*1414/1000 = 56 cm. */
    const Punto diag[] = { {0, 3}, {1, 2}, {2, 1} };
    const Paso esp_diag[] = { girar(45), avanzar(2 * CELDA_CM * DIAGONAL_CM_MIL / 1000) };
    verificar_ruta("diagonal: giro +45 y 2 celdas diagonales juntas", diag, 3, esp_diag, 2);

    /* Una celda diagonal = celda * 1414 / 1000 = 28 cm (con celda de 20). */
    {
        const Punto una[] = { {0, 1}, {1, 0} };
        const Paso esp_una[] = { girar(45), avanzar(CELDA_CM * DIAGONAL_CM_MIL / 1000) };
        verificar_ruta("diagonal: una celda = celda*1414/1000", una, 2, esp_una, 2);
    }

    /* Tres diagonales se calculan sobre el total: 3*20*1414/1000 = 84. */
    {
        const Punto tres[] = { {0, 3}, {1, 2}, {2, 1}, {3, 0} };
        const Paso esp_tres[] = { girar(45), avanzar(3 * CELDA_CM * DIAGONAL_CM_MIL / 1000) };
        verificar_ruta("diagonal: tres celdas, redondeo sobre el total", tres, 4, esp_tres, 2);
    }

    /* Cinco diagonales: 5*20*1414/1000 = 141 cm (celda por celda serían 140). */
    {
        const Punto cinco[] = { {0, 5}, {1, 4}, {2, 3}, {3, 2}, {4, 1}, {5, 0} };
        const Paso esp_cinco[] = { girar(45), avanzar(141) };
        verificar_ruta("diagonal: cinco celdas = 141 cm (redondeo sobre el total)", cinco, 6, esp_cinco, 2);
    }

    /* Recta, luego diagonal hacia abajo-derecha: de arriba a abajo-derecha = +135. */
    {
        const Punto mixta[] = { {0, 2}, {0, 1}, {1, 2} };
        const Paso esp_mixta[] = {
            avanzar(CELDA_CM), girar(135), avanzar(CELDA_CM * DIAGONAL_CM_MIL / 1000)
        };
        verificar_ruta("diagonal: recta y luego diagonal hacia abajo, +135", mixta, 3, esp_mixta, 3);
    }
}

static void prueba_giros_extremos(void)
{
    /* Empieza mirando arriba y la ruta va hacia abajo: media vuelta (+180). */
    const Punto atras[] = { {0, 0}, {0, 1} };
    const Paso esp_atras[] = { girar(180), avanzar(CELDA_CM) };
    verificar_ruta("media vuelta: ir hacia abajo desde arriba, +180", atras, 2, esp_atras, 2);

    /* Giro normalizado: de derecha(2) a arriba-izquierda(7) = -135, no +225. */
    {
        const Punto ruta[] = { {0, 2}, {1, 2}, {0, 1} };
        const Paso esp[] = { girar(90), avanzar(CELDA_CM),
                             girar(-135), avanzar(CELDA_CM * DIAGONAL_CM_MIL / 1000) };
        verificar_ruta("giro normalizado a -135 (no +225)", ruta, 3, esp, 4);
    }
}

static void prueba_errores(void)
{
    const Punto salto[]     = { {0, 0}, {2, 0} };
    const Punto salto_mid[] = { {0, 0}, {1, 0}, {1, 3} };
    const Punto repetido[]  = { {0, 0}, {1, 0}, {1, 0} };
    int n = -1;
    Paso p[MAX_PASOS];

    verificar_error("salto inválido (2 celdas): TRAY_ERR_SALTO, índice 1",
                    salto, 2, TRAY_ERR_SALTO, 1);
    verificar_error("salto inválido en medio de la ruta: índice 2",
                    salto_mid, 3, TRAY_ERR_SALTO, 2);
    verificar_error("punto repetido: TRAY_ERR_REPETIDO, índice 2",
                    repetido, 3, TRAY_ERR_REPETIDO, 2);

    caso("sin puntos: TRAY_ERR_ARGUMENTO",
         trayectoria_calcular(salto, 0, p, MAX_PASOS, &n, NULL) == TRAY_ERR_ARGUMENTO);
    caso("puntero nulo: TRAY_ERR_ARGUMENTO",
         trayectoria_calcular(NULL, 2, p, MAX_PASOS, &n, NULL) == TRAY_ERR_ARGUMENTO);
    caso("sin capacidad de salida: TRAY_ERR_ARGUMENTO",
         trayectoria_calcular(salto, 2, p, 0, &n, NULL) == TRAY_ERR_ARGUMENTO);
}

static void prueba_exceso_de_limites(void)
{
    Punto zigzag[MAX_PUNTOS + 1];
    int i;

    /* Zigzag arriba-derecha / abajo-derecha: cada tramo es un paso y cada
     * cambio de dirección otro: 63 avances + 63 giros > MAX_PASOS. */
    for (i = 0; i < MAX_PUNTOS + 1; i++) {
        zigzag[i].columna = i;
        zigzag[i].fila    = (i % 2 == 0) ? 1 : 0;
    }
    verificar_error("exceso de MAX_PASOS: TRAY_ERR_DEMASIADOS_PASOS",
                    zigzag, MAX_PUNTOS, TRAY_ERR_DEMASIADOS_PASOS, 0);
    verificar_error("exceso de MAX_PUNTOS: TRAY_ERR_DEMASIADOS_PUNTOS",
                    zigzag, MAX_PUNTOS + 1, TRAY_ERR_DEMASIADOS_PUNTOS, 0);
}

int main(void)
{
    prueba_linea_recta();
    prueba_giro_derecha();
    prueba_giro_izquierda();
    prueba_ruta_en_u();
    prueba_un_solo_punto();
    prueba_diagonales();
    prueba_giros_extremos();
    prueba_errores();
    prueba_exceso_de_limites();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
