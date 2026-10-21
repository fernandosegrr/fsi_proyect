/*
 * hw_sensores.h — Lo que el hardware debe proporcionar a los sensores (hw_...).
 *
 * Igual que en carro.h: imu.c, tof.c, giro.c y sensores.c son C puro y solo DECLARAN estas funciones. En el ESP32 las implementa
 * firmware_carro.ino (Wire y la librería de Pololu para el VL53L0X) y en las pruebas de PC las implementa pruebas_firmware/sensores_falsos.c.
 * Todas devuelven rápido: ninguna espera al sensor (el bus I2C tiene un tiempo máximo de I2C_TIMEOUT_MS).
 */
#ifndef HW_SENSORES_H
#define HW_SENSORES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Microsegundos desde el arranque (uint32_t: se desborda cada ~71 minutos; las restas sin signo lo toleran). */
uint32_t hw_micros(void);

/* Espera ms milisegundos. SOLO se usa durante el arranque (setup), nunca en loop(). */
void hw_demora_ms(uint32_t ms);

/* Lee n bytes a partir del registro reg del dispositivo dir. Devuelve 0 si salió bien y otro número si el bus falló. */
int hw_i2c_leer(uint8_t dir, uint8_t reg, uint8_t *datos, int n);

/* Escribe n bytes a partir del registro reg. Devuelve 0 si salió bien. */
int hw_i2c_escribir(uint8_t dir, uint8_t reg, const uint8_t *datos, int n);

/* 1 si algo contesta en la dirección dir (sirve para el escaneo del bus). */
int hw_i2c_presente(uint8_t dir);

/* Sensor láser. hw_tof_iniciar: 1 si se inició bien. hw_tof_listo: 1 si hay una medida lista, 0 si todavía no, -1 si el bus falló.
 * hw_tof_leer: guarda en *mm el valor CRUDO del sensor (8190 o 65535 = sin eco) y devuelve 0 si salió bien. */
int hw_tof_iniciar(void);
int hw_tof_listo(void);
int hw_tof_leer(int *mm);

/* Memoria no volátil del historial de reintentos del láser en los últimos arranques (un número empaquetado, ver tof.c): hw_tof_hist_leer devuelve -1 si no hay nada. */
int  hw_tof_hist_leer(void);
void hw_tof_hist_guardar(int valor);
int  hw_imu_hist_leer(void);
void hw_imu_hist_guardar(int valor);

/* Memoria no volátil de la anticipación del giro: 4 ranuras (perfil x sentido, ver giro.c). hw_ant_leer devuelve los ms guardados o -1 si no hay nada; hw_ant_borrar las borra todas. */
int  hw_ant_leer(int ranura);
void hw_ant_guardar(int ranura, int ms);
void hw_ant_borrar(void);

#ifdef __cplusplus
}
#endif

#endif /* HW_SENSORES_H */
