/* video_stats.c — ver video_stats.h */
#include <string.h>
#include "video_stats.h"

void vs_iniciar(VideoStats *vs)
{
    if (vs != NULL) {
        memset(vs, 0, sizeof(*vs));
    }
}

/* El cubo que le toca a un segundo; si tenía datos de otro segundo (más de 10 s atrás) se borra primero. */
static CuboVideo *cubo_de(VideoStats *vs, uint32_t segundo)
{
    CuboVideo *c = &vs->cubos[segundo % VS_VENTANA_S];

    if (!c->en_uso || c->segundo != segundo) {
        memset(c, 0, sizeof(*c));
        c->en_uso = 1;
        c->segundo = segundo;
    }
    return c;
}

void vs_cuadro(VideoStats *vs, uint32_t ahora_ms)
{
    CuboVideo *c;

    if (vs == NULL) {
        return;
    }
    c = cubo_de(vs, ahora_ms / 1000u);
    c->cuadros++;
    if (vs->tiene_ultimo) {
        int hueco = (int)(ahora_ms - vs->t_ultimo_ms);   /* la resta sin signo aguanta el desbordamiento del reloj */

        if (hueco > c->hueco_max_ms) {
            c->hueco_max_ms = hueco;
        }
        if (hueco > VS_HUECO_LARGO_MS) {
            c->huecos_largos++;
        }
    }
    if (!vs->tiene_ultimo) {
        vs->t_primero_ms = ahora_ms;
    }
    vs->tiene_ultimo = 1;
    vs->t_ultimo_ms = ahora_ms;
}

ResumenVideo vs_resumen(const VideoStats *vs, uint32_t ahora_ms)
{
    ResumenVideo r;
    uint32_t segundo_ahora = ahora_ms / 1000u;
    int i;

    memset(&r, 0, sizeof(r));
    if (vs == NULL) {
        return r;
    }
    for (i = 0; i < VS_VENTANA_S; i++) {
        const CuboVideo *c = &vs->cubos[i];

        /* Solo cuentan los cubos de los últimos VS_VENTANA_S segundos (el actual y los 9 anteriores). */
        if (c->en_uso && segundo_ahora >= c->segundo && segundo_ahora - c->segundo < VS_VENTANA_S) {
            r.cuadros += c->cuadros;
            r.huecos_largos += c->huecos_largos;
            if (c->hueco_max_ms > r.hueco_max_ms) {
                r.hueco_max_ms = c->hueco_max_ms;
            }
        }
    }
    if (vs->tiene_ultimo) {
        r.hueco_actual_ms = (int)(ahora_ms - vs->t_ultimo_ms);
        if (r.hueco_actual_ms > r.hueco_max_ms) {
            r.hueco_max_ms = r.hueco_actual_ms;
        }
        if (r.hueco_actual_ms > VS_HUECO_LARGO_MS) {
            r.huecos_largos++;
        }
    }
    /* Cuadros por segundo: sobre toda la ventana; si todavía no pasan 10 s desde el primer cuadro, sobre el tiempo que lleva. */
    if (vs->tiene_ultimo) {
        int ventana_ms = (int)(ahora_ms - vs->t_primero_ms);

        if (ventana_ms > VS_VENTANA_S * 1000) {
            ventana_ms = VS_VENTANA_S * 1000;
        }
        if (ventana_ms < 1000) {
            ventana_ms = 1000;      /* menos de un segundo de datos: no se inventan fps enormes */
        }
        r.fps_x10 = r.cuadros * 10 * 1000 / ventana_ms;
    }
    return r;
}
