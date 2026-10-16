/*
 * carro.h — Interfaz de la lógica del carro (C estándar).
 *
 * carro.c decide QUÉ hacer (interpretar comandos, mover motores, parar por
 * tiempo, keep-alive). Para tocar el hardware usa las funciones hw_...,
 * que aquí solo se DECLARAN: en el ESP32 las implementa firmware_carro.ino
 * y en las pruebas de PC las implementan versiones falsas.
 */
#ifndef CARRO_H
#define CARRO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {   /* permite incluir este archivo desde el .ino (C++) */
#endif

/* ── Funciones que el hardware debe proporcionar (hw_...) ──────────────── */

/* Escribe un PWM (0 a 255) en un pin de motor. Motivo: carro.c no puede
 * llamar a ledcWrite() porque no conoce Arduino. */
void hw_motor_pwm(int pin, int valor);

/* Milisegundos desde el arranque. Es uint32_t (se desborda cada ~49 días,
 * igual que millis() del ESP32); las restas sin signo lo toleran. */
uint32_t hw_millis(void);

/* Pone el pin keep-alive en alto (1) o bajo (0). */
void hw_keepalive(int nivel);

/* Escribe una línea de texto en el puerto serie (mensajes de depuración). */
void hw_registrar(const char *texto);

/* Memoria no volátil (en el ESP32, Preferences): `perfil` es PERFIL_MANUAL o PERFIL_AUTO. hw_balance_leer devuelve el balance guardado en milésimas, o 0 si no hay ninguno.
 * hw_balance_guardar lo guarda. carro.c solo guarda valores ya validados (BALANCE_MIN_MIL a BALANCE_MAX_MIL). */
int hw_balance_leer(int perfil);
void hw_balance_guardar(int perfil, int milesimas);

/* ── Funciones de la lógica del carro ──────────────────────────────────── */

/* Deja el carro parado y el estado en "STOP". Se llama una vez en setup(),
 * después de configurar el PWM. */
void carro_iniciar(void);

/* Punto de entrada de cada mensaje de texto recibido por WebSocket.
 * Renueva el reloj del watchdog (salvo con PING) y ejecuta el comando. */
void carro_recibir_comando(const char *texto);

/* MENSAJES DE BALANCE (equilibrio de los lados al ir recto, ver config.h; se agregaron sin cambiar los de antes):
 *   "BAL_AUTO:NNN" y "BAL_MANUAL:NNN" (NNN = 3 o 4 cifras, de 800 a 1000) cambian el factor del lado izquierdo de ese perfil, lo guardan en la memoria no volátil y se contestan con
 *   "ACK_BAL_AUTO:NNN" / "ACK_BAL_MANUAL:NNN" (el valor que quedó). Con un valor fuera de rango, sin cifras o con letras NO cambian nada y se contestan "NAK_BAL".
 *   "BAL?" se contesta "BAL:AAA:MMM" (el balance de AUTO y el de MANUAL). No son un movimiento (no frenan ni cambian el comando actual) y se aplican al SIGUIENTE arranque. */

/* Respuesta que hay que mandarle de vuelta al cliente por el WebSocket para ese mensaje, o NULL si no hay que contestar
 * nada. PING → "PONG" (la app mide con ese ida y vuelta la latencia y comprueba que la conexión sigue viva);
 * MODO_MANUAL → "ACK_MANUAL" y MODO_AUTO → "ACK_AUTO" (confirman el perfil de velocidad); los mensajes de balance, como se explica arriba.
 * Llamar DESPUÉS de carro_recibir_comando: el ACK confirma que el cambio ya quedó puesto. No toca los motores ni el watchdog. El texto devuelto de un balance vive en un
 * buffer propio: vale hasta la siguiente llamada. */
const char *carro_respuesta(const char *texto);

/* Perfil de velocidad actual: PERFIL_MANUAL (0) o PERFIL_AUTO (1), ver config.h. */
int carro_perfil(void);

/* El balance (factor del lado izquierdo, en milésimas) de ese perfil. Ver config.h y "Mensajes de balance" arriba de carro_respuesta. */
int carro_balance(int perfil);

/* Vuelve al perfil AUTO (el más seguro). El .ino lo llama junto con carro_detener() al conectarse y desconectarse un cliente WebSocket;
 * carro_iniciar() y el watchdog también lo hacen. STOP y EBRAKE NO cambian el perfil. */
void carro_poner_perfil_auto(void);

/* Frena todo de inmediato y vuelve al estado "STOP". Además de STOP/EBRAKE
 * lo usa el .ino al conectarse o desconectarse un cliente WebSocket. */
void carro_detener(void);

/* Watchdog de software: llamar en cada vuelta de loop(). Frena si hay
 * motores en marcha y pasó demasiado tiempo sin un comando real. */
void carro_vigilar_comunicacion(void);

/* Keep-alive de la powerbank: llamar en cada vuelta de loop(). Genera un
 * pulso corto cada cierto tiempo en el pin keep-alive. */
void carro_actualizar_keepalive(void);

#ifdef __cplusplus
}
#endif

#endif /* CARRO_H */
