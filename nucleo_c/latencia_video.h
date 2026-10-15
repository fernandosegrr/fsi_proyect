/*
 * latencia_video.h — ¿El video abierto empeora la respuesta del control del carro?
 *
 * GUÍA DE LECTURA
 *  1. El video de la cámara y el WebSocket de control comparten el WiFi del carro. Si el video estorba, el PONG del control tarda más.
 *  2. La app mide el PONG (lo que tarda la respuesta a un PING) en dos tiempos: primero SIN video y luego CON video. Cada medida se anota con
 *     lv_muestra(); lv_resultado() compara los promedios.
 *  3. Regla del proyecto: si el promedio CON video es más de LV_EMPEORA_MS (30 ms) mayor que SIN video, el video de la pantalla Manual se deja
 *     APAGADO por defecto (queda como un interruptor). Con menos de LV_MIN_MUESTRAS medidas de cada lado no se decide nada.
 *  4. Solo enteros, sin malloc.
 */
#ifndef LATENCIA_VIDEO_H
#define LATENCIA_VIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

#define LV_EMPEORA_MS     30   /* si el promedio sube más que esto con el video abierto, el video se deja apagado por defecto */
#define LV_MIN_MUESTRAS    5   /* medidas mínimas de cada lado para decidir */

typedef enum {
    LV_POCAS_MUESTRAS = -1,   /* faltan medidas: no se puede decidir */
    LV_SE_MANTIENE = 0,       /* el video no estorba (sube 30 ms o menos): se puede dejar encendido */
    LV_EMPEORA = 1            /* el video estorba (sube más de 30 ms): dejarlo apagado por defecto */
} VeredictoVideo;

typedef struct {
    int  n_sin, n_con;
    long suma_sin, suma_con;   /* ms */
    int  max_sin, max_con;     /* ms */
} MedidaVideo;

typedef struct {
    int prom_sin_x10, prom_con_x10;   /* promedios en décimas de ms */
    int max_sin, max_con;             /* máximos en ms */
    int diferencia_x10;               /* promedio con video menos promedio sin video, en décimas de ms */
    VeredictoVideo veredicto;
} ResultadoVideo;

void lv_iniciar(MedidaVideo *m);

/* Anota un PONG de `pong_ms` milisegundos. `con_video` = 1 si el video estaba abierto. Los valores negativos se ignoran. */
void lv_muestra(MedidaVideo *m, int con_video, int pong_ms);

/* Compara los dos lados. */
ResultadoVideo lv_resultado(const MedidaVideo *m);

/* Texto corto del veredicto en español ("se mantiene", "empeora", "faltan medidas"). */
const char *lv_texto_veredicto(VeredictoVideo v);

#ifdef __cplusplus
}
#endif

#endif /* LATENCIA_VIDEO_H */
