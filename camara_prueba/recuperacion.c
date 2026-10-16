/*
 * recuperacion.c — Decisiones de recuperación de la cámara (ver recuperacion.h). C puro, sin malloc ni librerías de la placa.
 */
#include "recuperacion.h"

/* ¿Ya llegó `objetivo`? Resta sin signo: sigue bien cuando millis() da la vuelta. */
static int ya_llego(uint32_t ahora_ms, uint32_t objetivo_ms)
{
    return (int32_t)(ahora_ms - objetivo_ms) >= 0;
}

uint32_t rec_espera_ms(uint32_t intentos)
{
    uint32_t espera = REC_ESPERA_INICIAL_MS;
    uint32_t i;
    for (i = 0; i < intentos && espera < REC_ESPERA_MAX_MS; i++) {
        espera *= 2u;
    }
    return espera > REC_ESPERA_MAX_MS ? REC_ESPERA_MAX_MS : espera;
}

void rec_wifi_iniciar(RecWifi *w)
{
    w->sin_conexion = 0;
    w->desde_ms = 0;
    w->proximo_ms = 0;
    w->intentos = 0;
}

void rec_wifi_arrancar(RecWifi *w, uint32_t ahora_ms)
{
    w->sin_conexion = 1;
    w->desde_ms = ahora_ms;
    w->proximo_ms = ahora_ms + REC_GRACIA_ARRANQUE_MS;
    w->intentos = 0;
}

RecAccion rec_wifi_paso(RecWifi *w, uint32_t ahora_ms, int conectado)
{
    if (conectado) {
        rec_wifi_iniciar(w);
        return REC_NADA;
    }
    if (!w->sin_conexion) {                       /* primera vez que se nota la caída: empieza la cuenta */
        w->sin_conexion = 1;
        w->desde_ms = ahora_ms;
        w->proximo_ms = ahora_ms + rec_espera_ms(0);
        w->intentos = 0;
        return REC_NADA;
    }
    if ((uint32_t)(ahora_ms - w->desde_ms) > REC_SIN_WIFI_MAX_MS) {
        return REC_REINICIAR;
    }
    if (ya_llego(ahora_ms, w->proximo_ms)) {
        w->intentos++;
        w->proximo_ms = ahora_ms + rec_espera_ms(w->intentos);
        return REC_REINTENTAR;
    }
    return REC_NADA;
}

uint32_t rec_wifi_ms_sin_conexion(const RecWifi *w, uint32_t ahora_ms)
{
    return w->sin_conexion ? (uint32_t)(ahora_ms - w->desde_ms) : 0u;
}

RecAccion rec_servidor_paso(RecServidor *s, int respondio)
{
    if (respondio) {
        s->fallos = 0;
        return REC_NADA;
    }
    s->fallos++;
    return s->fallos >= REC_FALLOS_SERVIDOR_MAX ? REC_REINICIAR : REC_NADA;
}

int rec_cuadro_atascado(uint32_t ahora_ms, uint32_t ultima_actividad_ms, int hay_demanda)
{
    return hay_demanda && (uint32_t)(ahora_ms - ultima_actividad_ms) > REC_CUADRO_MAX_MS;
}

int rec_latido_perdido(uint32_t ahora_ms, uint32_t ultimo_latido_ms)
{
    return (uint32_t)(ahora_ms - ultimo_latido_ms) > REC_LATIDO_MAX_MS;
}
