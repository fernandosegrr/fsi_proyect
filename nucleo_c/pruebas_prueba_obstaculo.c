/* pruebas_prueba_obstaculo.c — pruebas de las cuentas de la prueba de obstáculos (sin hardware). */
#include <stdio.h>
#include "prueba_obstaculo.h"

static int casos = 0, fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

int main(void)
{
    PasadaObstaculo p;
    ResumenPrueba r;
    int retraso;

    /* Un caso con números típicos: video 320 ms + detectar/enviar 70 ms + mitad de un ida y vuelta de 60 ms. */
    p.ms_detectar = 8;
    p.ms_stop = 70;
    p.ms_respuesta = 60;
    p.latencia_video_ms = 320;
    retraso = po_retraso_ms(&p);
    caso("retraso = video 320 + stop 70 + 60/2 = 420 ms", retraso == 420);

    /* Con la calibración medida: 26.3 ms/cm (263 en x10) y 0.9 cm de frenado (9 mm): 420 ms son 15.97 cm + 0.9 = 16.9 cm. */
    caso("420 ms con 26.3 ms/cm y frenado 0.9 cm = 16.9 cm (169 en décimas)", po_cm_x10(retraso, 263, 9) == 169);
    caso("1000 ms con 26.3 ms/cm y frenado 0.9 cm = 38.0 + 0.9 = 38.9 cm (389)", po_cm_x10(1000, 263, 9) == 389);
    caso("retraso 0: solo el frenado (0.9 cm = 9)", po_cm_x10(0, 263, 9) == 9);
    caso("sin calibración válida (ms por cm 0) devuelve -1", po_cm_x10(420, 0, 9) == -1 && po_cm_x10(420, -5, 9) == -1);
    caso("el retraso negativo se trata como 0", po_cm_x10(-100, 263, 9) == 9);

    /* Con la cámara del teléfono (sin retraso de video). */
    p.latencia_video_ms = 0;
    caso("con la cámara del teléfono (video 0): 70 + 30 = 100 ms", po_retraso_ms(&p) == 100);

    /* Tiempos raros no restan. */
    p.ms_stop = -50;
    p.ms_respuesta = -20;
    p.latencia_video_ms = 320;
    caso("tiempos negativos cuentan como 0: solo el video", po_retraso_ms(&p) == 320);
    caso("puntero NULL: retraso 0", po_retraso_ms(NULL) == 0);

    /* Resumen de 3 pasadas. */
    po_resumen_iniciar(&r);
    caso("resumen vacío: promedios 0", po_resumen_retraso_prom_ms(&r) == 0 && po_resumen_cm_prom_x10(&r) == 0 && r.n == 0);
    po_resumen_agregar(&r, 400, 150);
    po_resumen_agregar(&r, 500, 200);
    po_resumen_agregar(&r, 450, 170);
    caso("resumen de 3 pasadas: n = 3", r.n == 3);
    caso("promedio del retraso (400+500+450)/3 = 450 ms", po_resumen_retraso_prom_ms(&r) == 450);
    caso("máximo del retraso 500 ms", r.max_retraso_ms == 500);
    caso("promedio de la distancia (150+200+170)/3 = 173", po_resumen_cm_prom_x10(&r) == 173);
    caso("máxima distancia 200 (20.0 cm)", r.max_cm_x10 == 200);
    po_resumen_agregar(NULL, 1, 1);
    po_resumen_iniciar(NULL);
    caso("punteros NULL en el resumen: no revienta", po_resumen_retraso_prom_ms(NULL) == 0 && po_resumen_cm_prom_x10(NULL) == 0);

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
