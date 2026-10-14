/* prueba_obstaculo.c — ver prueba_obstaculo.h */
#include <string.h>
#include "prueba_obstaculo.h"

static int no_negativo(int x)
{
    return (x < 0) ? 0 : x;
}

int po_retraso_ms(const PasadaObstaculo *p)
{
    if (p == NULL) {
        return 0;
    }
    return no_negativo(p->latencia_video_ms) + no_negativo(p->ms_stop) + no_negativo(p->ms_respuesta) / 2;
}

int po_cm_x10(int retraso_ms, int ms_por_cm_x10, int frenado_mm)
{
    long avance_x10;

    if (ms_por_cm_x10 <= 0) {
        return -1;
    }
    /* retraso_ms / (ms_por_cm_x10 / 10) cm  =  retraso_ms * 10 / ms_por_cm_x10 cm  =  retraso_ms * 100 / ms_por_cm_x10 décimas de cm,
     * redondeado al más cercano. El frenado viene en mm, que son décimas de cm. */
    avance_x10 = ((long)no_negativo(retraso_ms) * 100L + ms_por_cm_x10 / 2) / ms_por_cm_x10;
    return (int)avance_x10 + no_negativo(frenado_mm);
}

void po_resumen_iniciar(ResumenPrueba *r)
{
    if (r != NULL) {
        memset(r, 0, sizeof(*r));
    }
}

void po_resumen_agregar(ResumenPrueba *r, int retraso_ms, int cm_x10)
{
    if (r == NULL) {
        return;
    }
    r->n++;
    r->suma_retraso_ms += retraso_ms;
    r->suma_cm_x10 += cm_x10;
    if (retraso_ms > r->max_retraso_ms) {
        r->max_retraso_ms = retraso_ms;
    }
    if (cm_x10 > r->max_cm_x10) {
        r->max_cm_x10 = cm_x10;
    }
}

int po_resumen_retraso_prom_ms(const ResumenPrueba *r)
{
    return (r == NULL || r->n == 0) ? 0 : (int)((r->suma_retraso_ms + r->n / 2) / r->n);
}

int po_resumen_cm_prom_x10(const ResumenPrueba *r)
{
    return (r == NULL || r->n == 0) ? 0 : (int)((r->suma_cm_x10 + r->n / 2) / r->n);
}
