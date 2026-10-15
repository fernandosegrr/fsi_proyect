/*
 * latencia_video.c — Ver latencia_video.h.
 * Los promedios se guardan en décimas de milisegundo para no usar decimales.
 */
#include "latencia_video.h"

void lv_iniciar(MedidaVideo *m)
{
    if (!m) {
        return;
    }
    m->n_sin = m->n_con = 0;
    m->suma_sin = m->suma_con = 0;
    m->max_sin = m->max_con = 0;
}

void lv_muestra(MedidaVideo *m, int con_video, int pong_ms)
{
    if (!m || pong_ms < 0) {
        return;
    }
    if (con_video) {
        m->n_con++;
        m->suma_con += pong_ms;
        if (pong_ms > m->max_con) {
            m->max_con = pong_ms;
        }
    } else {
        m->n_sin++;
        m->suma_sin += pong_ms;
        if (pong_ms > m->max_sin) {
            m->max_sin = pong_ms;
        }
    }
}

ResultadoVideo lv_resultado(const MedidaVideo *m)
{
    ResultadoVideo r = {0, 0, 0, 0, 0, LV_POCAS_MUESTRAS};

    if (!m) {
        return r;
    }
    r.max_sin = m->max_sin;
    r.max_con = m->max_con;
    if (m->n_sin > 0) {
        r.prom_sin_x10 = (int)(m->suma_sin * 10 / m->n_sin);
    }
    if (m->n_con > 0) {
        r.prom_con_x10 = (int)(m->suma_con * 10 / m->n_con);
    }
    if (m->n_sin < LV_MIN_MUESTRAS || m->n_con < LV_MIN_MUESTRAS) {
        return r;   /* faltan medidas */
    }
    r.diferencia_x10 = r.prom_con_x10 - r.prom_sin_x10;
    r.veredicto = (r.diferencia_x10 > LV_EMPEORA_MS * 10) ? LV_EMPEORA : LV_SE_MANTIENE;
    return r;
}

const char *lv_texto_veredicto(VeredictoVideo v)
{
    switch (v) {
    case LV_SE_MANTIENE: return "se mantiene";
    case LV_EMPEORA:     return "empeora";
    case LV_POCAS_MUESTRAS: break;
    }
    return "faltan medidas";
}
