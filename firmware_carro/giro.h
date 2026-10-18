/*
 * giro.h — Giro por ÁNGULO MEDIDO (lazo cerrado con el giroscopio), en C puro.
 *
 * El comando GIRAR:<grados> (positivo = izquierda, de GIRO_MIN_GRADOS a GIRO_MAX_GRADOS) hace un pivote con el motor del lado correspondiente y lo frena
 * cuando (ángulo girado + velocidad x anticipación) llega al objetivo. Después sigue integrando GIRO_ASENTAR_MS para informar el ángulo FINAL real.
 * Respuestas (por sensores_enviar, hacia la app):
 *   GIRO_OK:<objetivo en grados>:<real en décimas>:<ms>        llegó al ángulo
 *   GIRO_TIMEOUT:<objetivo>:<real x10>:<ms>                      se acabó el tiempo propio del giro (GIRO_TIEMPO_BASE_MS + GIRO_TIEMPO_POR_GRADO_MS x grados)
 *   GIRO_ERROR:<motivo>                                          no empezó: sin_imu, ocupado o angulo
 *   GIRO_ERROR:<motivo>:<objetivo>:<real x10>:<ms>               empezó y se cortó: abortado (STOP, EBRAKE, otro movimiento, cambio de perfil, desconexión) o sin_imu (el sensor falló)
 * El giro NO depende del watchdog de 1 s (un giro de 180° dura más), pero cualquier STOP, EBRAKE, cambio de perfil, desconexión del cliente o fallo del sensor lo aborta.
 */
#ifndef GIRO_H
#define GIRO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Deja el controlador sin giro en curso y con la anticipación de config.h (no lee la memoria: eso lo hace giro_cargar_memoria). */
void giro_reiniciar(void);

/* Lee de la memoria no volátil las 4 anticipaciones (perfil x sentido); lo que no esté guardado queda en GIRO_ANTICIPACION_MS. Se llama en setup(), después de giro_reiniciar. */
void giro_cargar_memoria(void);

/* Vuelve las 4 anticipaciones a GIRO_ANTICIPACION_MS y borra lo guardado (comando ANT_BORRAR). */
void giro_borrar_anticipaciones(void);

/* Pide un giro. texto = lo que sigue a "GIRAR:" (por ejemplo "-90"). Si no puede empezar, manda GIRO_ERROR:<motivo> y devuelve 0; si empezó, devuelve 1. */
int  giro_pedir(const char *texto);

/* Llamar en cada vuelta de loop() con hw_millis(). */
void giro_actualizar(uint32_t ahora_ms);

/* Corta el giro (lo llama carro.c desde carro_detener y antes de otro movimiento). Los motores ya los apagó quien lo llama. Informa GIRO_ERROR:abortado cuando termine de asentarse. */
void giro_abortar(void);

int  giro_activo(void);         /* 1 mientras los motores del giro están encendidos (el watchdog de 1 s no corre en ese tiempo) */
int  giro_ocupado(void);        /* 1 mientras gira o se está asentando (todavía no se puede pedir otro) */
int  giro_anticipacion_ms(int perfil, int sentido);   /* la anticipación actual de ese perfil (PERFIL_MANUAL o PERFIL_AUTO) y sentido (+1 = izquierda, -1 = derecha) */

#ifdef __cplusplus
}
#endif

#endif /* GIRO_H */
