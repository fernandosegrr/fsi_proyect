/*
 * sensores.h — Pegamento entre los sensores (imu.c, tof.c, giro.c) y el resto del firmware, en C puro.
 *
 * QUÉ HACE
 *   - sensores_iniciar() inicia el giroscopio y el láser (en setup(), después de abrir el bus I2C).
 *   - sensores_actualizar() va en cada vuelta de loop(): lee los sensores, mueve el giro por ángulo, aplica la PARADA DE EMERGENCIA del láser y manda SENS.
 *   - sensores_comando() entiende los mensajes nuevos que llegan por la misma cola de comandos que los demás (carro.c los pasa aquí).
 *   - Todo lo que hay que decirle a la app sale por una cola de salida (sensores_extraer_mensaje); la vacía el .ino en cada vuelta de loop()
 *     y la manda por el WebSocket al cliente activo (o por el puerto serie, para las pruebas con el USB).
 *
 * MENSAJES (ver docs/sensores.md)
 *   CAPS?        → CAPS:IMU=<0|1>,TOF=<0|1>       lo que funciona (se repite solo cuando cambia)
 *   IMU?         → IMU:<ángulo x10>,<sesgo x100 °/s>,<estado 0..3>,<WHO_AM_I>,<motivo>
 *   TOF?         → TOF:<mm>,<banderas: 1 = sensor vivo, 2 = medida válida>
 *   SENS:<ángulo x10>,<mm>,<banderas>   cada 100 ms con cliente y perfil AUTO. Banderas: 1 IMU lista, 2 láser vivo, 4 última medida válida (si no, "libre"),
 *                                       8 IMU calibrando, 16 giro en curso.
 *   GIRAR:<grados con signo>     → GIRO_OK / GIRO_TIMEOUT / GIRO_ERROR (ver giro.h)
 *   IMU_CAL      → ACK_IMU_CAL (empezó) o NAK_IMU_CAL (el carro no está quieto); al terminar IMU_CAL_OK:<sesgo x100> o IMU_CAL_FALLO
 *   IMU_CERO     → ACK_IMU_CERO (el ángulo vuelve a 0)
 *   TOF_PARO:<mm>               (lo manda el firmware solo) frenó por algo muy cerca al frente
 *   LOOP?        → LOOP:<el loop() más largo en µs>,<cuántos pasaron de LOOP_MAX_US>        LOOP_CERO → ACK_LOOP_CERO
 *   ANT?         → ANT:<AUTO izquierda>,<AUTO derecha>,<MANUAL izquierda>,<MANUAL derecha> (anticipación del giro en ms, una por sentido y por perfil)
 *   CAPS?        → CAPS:IMU=<0|1>,TOF=<0|1>,TOFR=<reintentos láser>,IMUR=<reintentos sesgo>,IMUM=<motivo>,IMUE=<estado>
 *   ARR?         → ARR:<n TOF>,<con reintentos TOF>,<detalle hex TOF>,<actual TOF>,<n IMU>,<con reintentos IMU>,<detalle hex IMU>,<actual IMU>
 *   ANT_BORRAR   → ACK_ANT_BORRAR   las cuatro vuelven a GIRO_ANTICIPACION_MS y se borran de la memoria no volátil
 *   TOF_USO:<0|1> → ACK_TOF_USO:<n>   enciende (1, por defecto) o apaga (0) la parada de emergencia del láser (el interruptor «Usar sensor de distancia» de la app); el láser sigue midiendo y mandando SENS
 */
#ifndef SENSORES_H
#define SENSORES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A dónde va un mensaje de salida (se pueden sumar). */
#define SENSORES_DESTINO_WS     1   /* el cliente WebSocket activo */
#define SENSORES_DESTINO_SERIE  2   /* el puerto serie (USB) */

void sensores_iniciar(void);
void sensores_actualizar(void);

/* 1 si el texto es un mensaje de sensores (los que se listan arriba). */
int  sensores_es_comando(const char *texto);

/* Ejecuta un mensaje de sensores que llegó por el WebSocket (desde la cola de comandos, ya en loop()). */
void sensores_comando(const char *texto);

/* Lo mismo, pero llegado por el puerto serie. GIRAR no se acepta por aquí (los motores solo se mueven por el WebSocket). */
void sensores_comando_serie(const char *texto);

/* Pone un mensaje en la cola de salida. */
void sensores_enviar(int destino, const char *texto);

/* Saca el siguiente mensaje de salida: lo copia a destino (hasta largo bytes), pone en *donde el destino y devuelve 1; 0 si no hay. */
int  sensores_extraer_mensaje(char *destino, int largo, int *donde);

/* El .ino anota aquí cuánto tardó cada vuelta de loop() (microsegundos). */
void sensores_registrar_loop_us(uint32_t us);
uint32_t sensores_loop_max_us(void);
uint32_t sensores_loop_excesos(void);

/* Cuántas paradas de emergencia hizo el láser. */
uint32_t sensores_paros_tof(void);

/* Deja todo como recién encendido (para las pruebas). */
void sensores_reiniciar_estado(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSORES_H */
