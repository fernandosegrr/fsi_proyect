/* pruebas_latencia_video.c — pruebas de latencia_video.c: la regla «si el video empeora el PONG más de 30 ms, se deja apagado». */
#include <stdio.h>
#include <string.h>
#include "latencia_video.h"

static int casos = 0, fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* Anota `n` medidas del mismo valor. */
static void anotar(MedidaVideo *m, int con_video, int n, int pong_ms)
{
    int i;

    for (i = 0; i < n; i++) {
        lv_muestra(m, con_video, pong_ms);
    }
}

int main(void)
{
    MedidaVideo m;
    ResultadoVideo r;

    lv_iniciar(&m);
    r = lv_resultado(&m);
    caso("sin medidas: faltan medidas", r.veredicto == LV_POCAS_MUESTRAS && r.prom_sin_x10 == 0 && r.prom_con_x10 == 0);

    anotar(&m, 0, 10, 30);
    r = lv_resultado(&m);
    caso("solo sin video: faltan medidas (falta el otro lado)", r.veredicto == LV_POCAS_MUESTRAS && r.prom_sin_x10 == 300);

    anotar(&m, 1, 4, 30);
    r = lv_resultado(&m);
    caso("4 medidas con video (menos de 5): faltan medidas", r.veredicto == LV_POCAS_MUESTRAS);

    lv_muestra(&m, 1, 30);
    r = lv_resultado(&m);
    caso("5 y 10 medidas, igual de rápido: el video se mantiene", r.veredicto == LV_SE_MANTIENE && r.diferencia_x10 == 0);

    /* Sube justo 30 ms: todavía se mantiene. */
    lv_iniciar(&m);
    anotar(&m, 0, 10, 30);
    anotar(&m, 1, 10, 60);
    r = lv_resultado(&m);
    caso("sube exactamente 30 ms: se mantiene (el límite es «más de 30»)", r.diferencia_x10 == 300 && r.veredicto == LV_SE_MANTIENE);

    /* Sube 30.1 ms: ya empeora. */
    lv_iniciar(&m);
    anotar(&m, 0, 10, 30);
    anotar(&m, 1, 9, 60);
    lv_muestra(&m, 1, 61);   /* promedio 60.1 */
    r = lv_resultado(&m);
    caso("sube 30.1 ms: empeora", r.diferencia_x10 == 301 && r.veredicto == LV_EMPEORA);

    /* Sube mucho. */
    lv_iniciar(&m);
    anotar(&m, 0, 12, 28);
    anotar(&m, 1, 12, 95);
    lv_muestra(&m, 1, 240);
    r = lv_resultado(&m);
    caso("sube mucho: empeora y guarda el máximo", r.veredicto == LV_EMPEORA && r.max_con == 240 && r.max_sin == 28);

    /* El video mejora (ruido): se mantiene con diferencia negativa. */
    lv_iniciar(&m);
    anotar(&m, 0, 8, 50);
    anotar(&m, 1, 8, 40);
    r = lv_resultado(&m);
    caso("con video más rápido: se mantiene y la diferencia es negativa", r.veredicto == LV_SE_MANTIENE && r.diferencia_x10 == -100);

    /* Promedio con decimales. */
    lv_iniciar(&m);
    anotar(&m, 0, 5, 30);
    lv_muestra(&m, 1, 31);
    anotar(&m, 1, 4, 32);
    r = lv_resultado(&m);
    caso("promedio en décimas de ms (31.8)", r.prom_con_x10 == 318);

    /* Valores negativos y punteros nulos. */
    lv_iniciar(&m);
    lv_muestra(&m, 0, -5);
    caso("una medida negativa se ignora", m.n_sin == 0);
    lv_iniciar(NULL);
    lv_muestra(NULL, 1, 10);
    r = lv_resultado(NULL);
    caso("punteros nulos: no revienta y faltan medidas", r.veredicto == LV_POCAS_MUESTRAS);

    caso("textos del veredicto", strcmp(lv_texto_veredicto(LV_EMPEORA), "empeora") == 0 && strcmp(lv_texto_veredicto(LV_SE_MANTIENE), "se mantiene") == 0 && strcmp(lv_texto_veredicto(LV_POCAS_MUESTRAS), "faltan medidas") == 0);

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
