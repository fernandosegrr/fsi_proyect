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

#define TOF_EVT_MAL         1   /* el sensor se cayó */
#define TOF_EVT_RECUPERADO  2   /* el sensor volvió a funcionar después de un reintento */

/* Inicia el sensor con hw_tof_iniciar, hasta TOF_INTENTOS_INICIO veces (cuenta los reintentos y los anota en el historial de arranques). Devuelve 1 si quedó vivo. Solo en setup(). */
int  tof_iniciar(void);

/* Llamar en cada vuelta de loop() con hw_millis(). Si el sensor no está vivo, cada TOF_REINTENTO_MS pregunta si contesta (barato) y, solo si contesta y `puede_bloquear` es 1 (el carro lleva
 * TOF_REINTENTO_QUIETO_MS detenido y no gira), lo inicia otra vez (la inicialización de la librería espera unas decenas de ms). Nunca bloquea con el carro en movimiento. */
void tof_actualizar(uint32_t ahora_ms, int puede_bloquear);

/* Reintentos de inicio: los del arranque (el primer intento no cuenta) y el total desde el arranque (los del arranque más los que se hicieron después de una caída o de un arranque fallido). */
int  tof_reintentos_arranque(void);
int  tof_reintentos(void);

/* Historial de los últimos TOF_HIST_ARRANQUES arranques (se guarda en la memoria no volátil): cuántos arranques hay anotados (*n), en cuántos hubo reintentos (*con_reintentos) y los reintentos de cada uno
 * (*empaquetado: un dígito hexadecimal por arranque, el más nuevo a la derecha; 15 = 15 o más). */
void tof_historial(int *n, int *con_reintentos, int *empaquetado);

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
