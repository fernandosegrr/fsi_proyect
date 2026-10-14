/* pruebas_video_stats.c — pruebas de video_stats.c con llegadas de cuadros simuladas (sin hardware). */
#include <stdio.h>
#include <stdint.h>
#include "video_stats.h"

static int casos = 0, fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* Cuadros cada `periodo_ms` desde `desde_ms` hasta `hasta_ms` (sin incluir). */
static void llegar(VideoStats *vs, uint32_t desde_ms, uint32_t hasta_ms, uint32_t periodo_ms)
{
    uint32_t t;

    for (t = desde_ms; t < hasta_ms; t += periodo_ms) {
        vs_cuadro(vs, t);
    }
}

int main(void)
{
    VideoStats vs;
    ResumenVideo r;

    /* Sin datos. */
    vs_iniciar(&vs);
    r = vs_resumen(&vs, 5000);
    caso("sin cuadros: todo en cero", r.cuadros == 0 && r.fps_x10 == 0 && r.hueco_max_ms == 0 && r.huecos_largos == 0 && r.hueco_actual_ms == 0);

    /* Video sano: 12.5 cuadros por segundo (cada 80 ms) durante 12 s. */
    vs_iniciar(&vs);
    llegar(&vs, 1000, 13000, 80);
    r = vs_resumen(&vs, 12999);   /* justo antes de cerrar el segundo 12: la ventana tiene 10 segundos completos */
    printf("      sano: fps=%d.%d hueco_max=%d largos=%d cuadros=%d\n", r.fps_x10 / 10, r.fps_x10 % 10, r.hueco_max_ms, r.huecos_largos, r.cuadros);
    caso("video sano: ~12.5 fps reales", r.fps_x10 >= 120 && r.fps_x10 <= 130);
    caso("video sano: el hueco más largo es el periodo (80 ms) y no hay tropiezos", r.hueco_max_ms <= 120 && r.huecos_largos == 0);
    caso("video sano: la ventana cuenta solo 10 s de cuadros (~125, no 150)", r.cuadros >= 120 && r.cuadros <= 130);

    /* Un tropiezo de 1.5 s a mitad del video. */
    vs_iniciar(&vs);
    llegar(&vs, 1000, 6000, 80);
    llegar(&vs, 7500, 12000, 80);                 /* hueco entre 5.92 s y 7.5 s ≈ 1.6 s */
    r = vs_resumen(&vs, 12000);
    printf("      tropiezo: fps=%d.%d hueco_max=%d largos=%d\n", r.fps_x10 / 10, r.fps_x10 % 10, r.hueco_max_ms, r.huecos_largos);
    caso("un tropiezo de ~1.6 s: hueco máximo entre 1500 y 1700 ms", r.hueco_max_ms >= 1500 && r.hueco_max_ms <= 1700);
    caso("un tropiezo: cuenta exactamente 1 hueco largo", r.huecos_largos == 1);
    caso("un tropiezo: los fps reales bajan respecto al video sano", r.fps_x10 < 115);

    /* Dos tropiezos de 600 ms y uno de 400 ms (este no es "largo"). */
    vs_iniciar(&vs);
    llegar(&vs, 1000, 3000, 80);
    llegar(&vs, 3600, 5000, 80);
    llegar(&vs, 5600, 7000, 80);
    llegar(&vs, 7400, 9000, 80);
    r = vs_resumen(&vs, 9000);
    caso("huecos de 600, 600 y 400 ms: solo los 2 mayores a 500 cuentan como largos", r.huecos_largos == 2);

    /* Un tropiezo que todavía no termina se ve en el resumen (hueco actual). */
    vs_iniciar(&vs);
    llegar(&vs, 1000, 5000, 80);                  /* último cuadro a los 4.92 s */
    r = vs_resumen(&vs, 6500);                    /* 1.58 s sin cuadros */
    caso("el hueco que está pasando ahora se ve aunque no haya terminado", r.hueco_actual_ms >= 1500 && r.hueco_max_ms >= 1500 && r.huecos_largos == 1);
    r = vs_resumen(&vs, 5100);
    caso("sin tropiezo todavía (180 ms desde el último): no hay hueco largo", r.hueco_actual_ms < 300 && r.huecos_largos == 0);

    /* Los datos viejos salen de la ventana. */
    vs_iniciar(&vs);
    llegar(&vs, 1000, 3000, 80);
    llegar(&vs, 4500, 5000, 80);                  /* hueco de 1.5 s a los 3 s */
    llegar(&vs, 5000, 20000, 80);
    r = vs_resumen(&vs, 20000);
    caso("un tropiezo de hace más de 10 s ya no cuenta", r.huecos_largos == 0 && r.hueco_max_ms < 200);

    /* Al inicio (menos de 10 s) los fps se calculan sobre el tiempo que lleva, no sobre 10 s. */
    vs_iniciar(&vs);
    llegar(&vs, 2000, 5000, 80);                  /* 3 s de video a 12.5 fps */
    r = vs_resumen(&vs, 5000);
    printf("      3 s de video: fps=%d.%d\n", r.fps_x10 / 10, r.fps_x10 % 10);
    caso("a los 3 s de empezar los fps ya son ~12.5 (no 3.7)", r.fps_x10 >= 115 && r.fps_x10 <= 135);

    /* El reloj da la vuelta (uint32): los huecos siguen bien. */
    vs_iniciar(&vs);
    vs_cuadro(&vs, 0xFFFFFF00u);
    vs_cuadro(&vs, 0x00000100u);                  /* 512 ms después, cruzando el desbordamiento */
    r = vs_resumen(&vs, 0x00000200u);
    caso("el desbordamiento del reloj no rompe el hueco (512 ms)", r.hueco_max_ms == 512 && r.huecos_largos == 1);

    caso("punteros NULL: no revienta", (vs_iniciar(NULL), vs_cuadro(NULL, 5), vs_resumen(NULL, 5).cuadros == 0));

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
