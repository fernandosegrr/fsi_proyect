/*
 * recuperacion.h — Decisiones de recuperación de la cámara (C puro, sin Arduino, probado en PC).
 *
 * GUÍA DE LECTURA
 *  1. La cámara es un cliente del WiFi del carro y a veces se queda sin respuesta (se perdió la red, el servidor de video no atiende o el
 *     bucle principal se colgó). Estas funciones solo DECIDEN qué hacer; quien actúa (WiFi.begin, ESP.restart) es camara_prueba.ino.
 *  2. Recuperar el WiFi: si se pierde, se reintenta con espera creciente (1, 2, 4, 8 y luego 10 s) y, si pasan 60 s sin conectar, se reinicia la placa.
 *  3. Servidor de video: la propia placa se pide /salud cada 10 s; con REC_FALLOS_SERVIDOR_MAX fallos seguidos se reinicia.
 *  4. Vigilante interno: reinicia si el bucle principal no avanza (REC_LATIDO_MAX_MS) o si alguien pide cuadros y la cámara lleva más de
 *     REC_CUADRO_MAX_MS sin entregar uno.
 *  5. Todos los tiempos son de millis() (32 bits que da la vuelta cada 49 días): las comparaciones se hacen con restas, como en carro.c.
 */
#ifndef RECUPERACION_H
#define RECUPERACION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REC_ESPERA_INICIAL_MS     1000u    /* primera espera entre reintentos de WiFi.begin */
#define REC_ESPERA_MAX_MS        10000u    /* la espera sube al doble cada vez (1, 2, 4, 8) y se queda en 10 s */
#define REC_SIN_WIFI_MAX_MS      60000u    /* sin conectar tanto tiempo: se reinicia la placa */
#define REC_GRACIA_ARRANQUE_MS    5000u    /* al arrancar, el primer intento (que ya se hizo) tiene este tiempo antes del primer reintento */
#define REC_CUADRO_MAX_MS         5000u    /* con demanda de video, más de esto sin un cuadro nuevo: se reinicia */
#define REC_LATIDO_MAX_MS         8000u    /* el bucle principal (loop) debe dar señal al menos así de seguido */
#define REC_FALLOS_SERVIDOR_MAX      3     /* /salud sin responder tantas veces seguidas (una cada 10 s): se reinicia */

typedef enum {
    REC_NADA = 0,       /* no hacer nada */
    REC_REINTENTAR,     /* volver a llamar a WiFi.begin */
    REC_REINICIAR       /* reiniciar la placa (ESP.restart) */
} RecAccion;

/* Seguimiento del WiFi: desde cuándo no hay conexión y cuándo toca el siguiente reintento. */
typedef struct {
    int      sin_conexion;     /* 1 si ya se notó que no hay conexión */
    uint32_t desde_ms;         /* hora en que se notó */
    uint32_t proximo_ms;       /* hora del siguiente reintento */
    uint32_t intentos;         /* reintentos hechos en esta caída (sube la espera) */
} RecWifi;

/* Seguimiento del servidor de video: fallos seguidos de /salud. */
typedef struct {
    uint32_t fallos;
} RecServidor;

/* Espera antes del reintento número `intentos` (0 = el primero): 1000, 2000, 4000, 8000, 10000, 10000... ms. */
uint32_t rec_espera_ms(uint32_t intentos);

/* Deja el seguimiento del WiFi como "conectado". */
void rec_wifi_iniciar(RecWifi *w);

/* Para el arranque: ya se hizo el primer intento de conexión en `ahora_ms`; el primer reintento será tras REC_GRACIA_ARRANQUE_MS. */
void rec_wifi_arrancar(RecWifi *w, uint32_t ahora_ms);

/* Se llama seguido (por ejemplo cada segundo) con el estado del WiFi. Devuelve qué hacer:
 *   conectado           → REC_NADA y se olvida la caída;
 *   sin conexión        → REC_REINTENTAR cuando toca (esperas crecientes) y REC_REINICIAR si lleva más de REC_SIN_WIFI_MAX_MS. */
RecAccion rec_wifi_paso(RecWifi *w, uint32_t ahora_ms, int conectado);

/* Cuántos ms lleva sin conexión (0 si hay conexión). */
uint32_t rec_wifi_ms_sin_conexion(const RecWifi *w, uint32_t ahora_ms);

/* Resultado de un /salud: `respondio` = 1 si la placa contestó 200. Devuelve REC_REINICIAR tras REC_FALLOS_SERVIDOR_MAX fallos seguidos. */
RecAccion rec_servidor_paso(RecServidor *s, int respondio);

/* 1 si hay que reiniciar porque alguien pide cuadros (`hay_demanda`) y desde la última actividad de la captura pasaron más de REC_CUADRO_MAX_MS. */
int rec_cuadro_atascado(uint32_t ahora_ms, uint32_t ultima_actividad_ms, int hay_demanda);

/* 1 si el bucle principal lleva más de REC_LATIDO_MAX_MS sin dar señal. */
int rec_latido_perdido(uint32_t ahora_ms, uint32_t ultimo_latido_ms);

#ifdef __cplusplus
}
#endif

#endif /* RECUPERACION_H */
