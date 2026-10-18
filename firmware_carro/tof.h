/*
 * tof.h — Sensor láser VL53L0X (distancia al frente), en C puro.
 *
 * Una lectura cada TOF_PERIODO_MS SIN bloquear loop(): tof_actualizar() pregunta si hay una medida lista (una lectura corta del bus) y solo entonces la lee.
 * Filtra los picos con la mediana de las últimas 3 lecturas y aplica la regla de la parada de emergencia (TOF_PARO_LECTURAS lecturas válidas seguidas más cerca de TOF_PARO_MM).
 * Quién frena y cuándo (solo perfil AUTO y solo avanzando) lo decide sensores.c; aquí solo se dice "hay algo cerca confirmado".
 */
#ifndef TOF_H
#define TOF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOF_EVT_MAL   1   /* el sensor se cayó */

/* Inicia el sensor con hw_tof_iniciar. Devuelve 1 si quedó vivo. Solo en setup(). */
int  tof_iniciar(void);

/* Llamar en cada vuelta de loop() con hw_millis(). */
void tof_actualizar(uint32_t ahora_ms);

int  tof_vivo(void);            /* 1 si el sensor funciona */
int  tof_mm(void);              /* distancia filtrada en mm; nunca más de TOF_LIBRE_MM (ese valor = libre) */
int  tof_valida(void);          /* 1 si la última lectura fue una medida real, 0 si fue "libre" (sin eco o más lejos de TOF_LIBRE_MM) */
int  tof_cerca_confirmada(void);/* la lectura más grande de las últimas TOF_PARO_LECTURAS si todas fueron válidas y menores de TOF_PARO_MM; 0 si no */
int  tof_tomar_eventos(void);
uint32_t tof_lecturas(void);    /* cuántas lecturas se han recibido */

/* Para las pruebas: mete una lectura cruda como si la hubiera dado el sensor. Devuelve 0. */
void tof_probar_lectura(int mm_crudo, uint32_t ahora_ms);

#ifdef __cplusplus
}
#endif

#endif /* TOF_H */
