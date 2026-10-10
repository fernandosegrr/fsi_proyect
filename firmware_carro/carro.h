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

/* ── Funciones de la lógica del carro ──────────────────────────────────── */

/* Deja el carro parado y el estado en "STOP". Se llama una vez en setup(),
 * después de configurar el PWM. */
void carro_iniciar(void);

/* Punto de entrada de cada mensaje de texto recibido por WebSocket.
 * Renueva el reloj del watchdog (salvo con PING) y ejecuta el comando. */
void carro_recibir_comando(const char *texto);

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
