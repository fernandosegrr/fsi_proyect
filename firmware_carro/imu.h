/*
 * imu.h — Giroscopio MPU6050 (solo el eje Z), en C puro.
 *
 * Lee la velocidad de giro, le quita el sesgo, la integra en el tiempo y entrega el ÁNGULO (positivo = girar a la izquierda). Todo con enteros.
 * No espera nunca al sensor: imu_actualizar() se llama en cada vuelta de loop() y solo lee cuando toca una muestra (cada IMU_PERIODO_US).
 */
#ifndef IMU_H
#define IMU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Estados del giroscopio. */
#define IMU_ESTADO_APAGADA     0   /* todavía no se inició */
#define IMU_ESTADO_CALIBRANDO  1   /* promediando el sesgo con el carro quieto */
#define IMU_ESTADO_LISTA       2   /* funciona: el ángulo es válido */
#define IMU_ESTADO_MAL         3   /* no contesta, es de otro chip o dio lecturas locas */

/* Cosas que pasaron desde la última vez (se leen y se borran con imu_tomar_eventos). */
#define IMU_EVT_LISTA          1   /* terminó la primera calibración: ya sirve */
#define IMU_EVT_MAL            2   /* se cayó */
#define IMU_EVT_CAL_OK         4   /* terminó bien una recalibración pedida (IMU_CAL) */
#define IMU_EVT_CAL_FALLO      8   /* una recalibración pedida no pudo; sigue el sesgo anterior */

/* Motivos de IMU_ESTADO_MAL (imu_motivo). */
#define IMU_MOTIVO_NINGUNO     0
#define IMU_MOTIVO_NO_RESPONDE 1   /* el bus no contestó al iniciar */
#define IMU_MOTIVO_OTRO_CHIP   2   /* WHO_AM_I desconocido */
#define IMU_MOTIVO_CONFIG      3   /* los registros no quedaron como se escribieron */
#define IMU_MOTIVO_SESGO       4   /* el sesgo no se pudo medir (se movió o ruido) */
#define IMU_MOTIVO_ERRORES     5   /* IMU_ERRORES_MAX errores seguidos del bus */
#define IMU_MOTIVO_LOCAS       6   /* lecturas imposibles (-32768) seguidas */
#define IMU_MOTIVO_CONGELADO   7   /* la misma lectura sin ruido muchas veces seguidas */

/* Conversiones (se exponen para probarlas por separado). imu_juntar_bytes arma el entero con signo de 16 bits con el byte alto y el bajo del registro;
 * imu_cuentas_a_mdps pasa cuentas del sensor (65.5 por °/s a ±500 °/s) a milésimas de °/s. */
int16_t imu_juntar_bytes(uint8_t alto, uint8_t bajo);
int32_t imu_cuentas_a_mdps(int32_t cuentas_por_256);   /* ENTRADA en 1/256 de cuenta, para poder restar el sesgo con decimales */

/* Inicia el chip (reinicio, reloj, filtro, rango, muestreo; verifica WHO_AM_I y que los registros quedaron bien). Devuelve 1 si quedó calibrando
 * y 0 si quedó en IMU_ESTADO_MAL (ver imu_motivo). Usa hw_demora_ms: solo en setup(). */
int imu_iniciar(void);

/* Llamar en cada vuelta de loop(). ahora_us = hw_micros(); quieto_ms = cuánto lleva el carro con los motores apagados (0 si se mueve). */
void imu_actualizar(uint32_t ahora_us, uint32_t quieto_ms);

/* Pide una recalibración del sesgo (comando IMU_CAL). El que llama debe haber comprobado que el carro está quieto. Devuelve 1 si empezó. Si el chip estaba
 * en MAL intenta iniciarlo otra vez (usa hw_demora_ms). El resultado llega como IMU_EVT_CAL_OK / IMU_EVT_CAL_FALLO. */
int imu_recalibrar(void);

int      imu_estado(void);
int      imu_motivo(void);
int      imu_quien_soy(void);          /* el WHO_AM_I que se leyó (0 si no se pudo) */
int32_t  imu_sesgo_x100(void);         /* sesgo actual en centésimas de °/s */
int32_t  imu_angulo_x10(void);         /* ángulo en décimas de grado, positivo = izquierda, desde el último IMU_CERO */
int32_t  imu_velocidad_x10(void);      /* velocidad en décimas de °/s (promedio de las últimas 2 muestras), positivo = izquierda */
void     imu_cero(void);               /* el ángulo vuelve a 0 */
int      imu_tomar_eventos(void);
uint32_t imu_muestras(void);           /* cuántas muestras buenas se han leído (para ver que sigue vivo) */

#ifdef __cplusplus
}
#endif

#endif /* IMU_H */
