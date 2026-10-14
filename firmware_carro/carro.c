/*
 * carro.c — Lógica del carro en C estándar (sin Arduino, sin String, sin malloc).
 *
 * CÓMO SE CONECTAN LAS FUNCIONES PÚBLICAS
 *   setup()  → carro_iniciar()                (motores a 0, estado "STOP")
 *   mensaje WebSocket → carro_recibir_comando(texto)
 *                         ├─ guarda la hora del comando (salvo PING)
 *                         └─ ejecutar_comando(texto)
 *                              ├─ FORWARD/REVERSE/LEFT/RIGHT → avanzar(),
 *                              │   retroceder(), girar_...() → poner_motores()
 *                              │   → hw_motor_pwm() por cada pin
 *                              └─ STOP/EBRAKE → carro_detener()
 *   conectar/desconectar cliente → carro_detener()
 *   loop()   → carro_vigilar_comunicacion()   (freno si no llegan comandos)
 *            → carro_actualizar_keepalive()   (pulso para la powerbank)
 *
 * Todo el contacto con el ESP32 pasa por hw_motor_pwm, hw_millis,
 * hw_keepalive y hw_registrar (declaradas en carro.h).
 */

#include <string.h>
#include "carro.h"
#include "config.h"

/* ── Estado del carro ──────────────────────────────────────────────────── */

static int      motores_en_marcha   = 0;   /* 1 si algún pin de motor tiene PWM > 0 */
static char     comando_actual[LARGO_MAX_COMANDO] = "STOP";
static uint32_t ultimo_comando_ms   = 0;   /* hora del último comando real (no PING) */
static uint32_t ultimo_keepalive_ms = 0;   /* hora del último pulso keep-alive */
static int      keepalive_alto      = 0;   /* 1 mientras el pulso está en alto */

/* ── Motores ───────────────────────────────────────────────────────────── */

/* Escribe el PWM de los 4 pines y recuerda si algún motor quedó en marcha
 * (el watchdog solo actúa cuando hay motores en marcha). */
static void poner_motores(int izq_a, int izq_b, int der_a, int der_b)
{
    hw_motor_pwm(PIN_MOTOR_IZQ_A, izq_a);
    hw_motor_pwm(PIN_MOTOR_IZQ_B, izq_b);
    hw_motor_pwm(PIN_MOTOR_DER_A, der_a);
    hw_motor_pwm(PIN_MOTOR_DER_B, der_b);
    motores_en_marcha = (izq_a > 0 || izq_b > 0 || der_a > 0 || der_b > 0);
}

/* Ambos motores hacia adelante. El izquierdo avanza por su pin B y el
 * derecho por su pin A (cableado probado en hardware; ver config.h). */
static void avanzar(void)
{
    poner_motores(0, VELOCIDAD_IZQ, VELOCIDAD_DER, 0);
}

/* Ambos motores hacia atrás (los pines contrarios a avanzar). */
static void retroceder(void)
{
    poner_motores(VELOCIDAD_IZQ, 0, 0, VELOCIDAD_DER);
}

/* Pivote a la izquierda: solo el motor derecho empuja; el izquierdo queda
 * suelto (PWM 0). */
static void girar_izquierda(void)
{
    poner_motores(0, 0, VELOCIDAD_GIRO, 0);
}

/* Pivote a la derecha: solo el motor izquierdo empuja; el derecho queda
 * suelto (PWM 0). */
static void girar_derecha(void)
{
    poner_motores(0, VELOCIDAD_GIRO, 0, 0);
}

/* ── Comandos ──────────────────────────────────────────────────────────── */

/* Copia el comando al buffer de estado, truncándolo si es muy largo, y
 * siempre terminándolo con '\0'. */
static void guardar_comando_actual(const char *texto)
{
    strncpy(comando_actual, texto, LARGO_MAX_COMANDO - 1);
    comando_actual[LARGO_MAX_COMANDO - 1] = '\0';
}

/* Un comando repetido (el mismo que ya está activo) se ignora, para que el
 * heartbeat de 250 ms de la página no reinicie nada. STOP y EBRAKE son la
 * excepción: SIEMPRE se ejecutan, aunque se repitan. */
static int es_comando_repetido(const char *texto)
{
    if (strcmp(texto, "STOP") == 0 || strcmp(texto, "EBRAKE") == 0) {
        return 0;
    }
    return strcmp(texto, comando_actual) == 0;
}

/* Interpreta el texto y actúa. No toca el reloj del watchdog (eso lo hace
 * carro_recibir_comando). */
static void ejecutar_comando(const char *texto)
{
    if (es_comando_repetido(texto)) {
        return;
    }

    if (strcmp(texto, "FORWARD") == 0) {
        avanzar();
    } else if (strcmp(texto, "REVERSE") == 0) {
        retroceder();
    } else if (strcmp(texto, "LEFT") == 0) {
        girar_izquierda();
    } else if (strcmp(texto, "RIGHT") == 0) {
        girar_derecha();
    } else if (strcmp(texto, "STOP") == 0 || strcmp(texto, "EBRAKE") == 0) {
        carro_detener();   /* STOP y EBRAKE hacen lo mismo */
        return;
    } else if (strcmp(texto, "PING") == 0) {
        return;            /* solo mantiene viva la conexión; no cambia nada */
    }
    /* OJO: pendiente — un comando desconocido no mueve los motores, pero
     * igual llega hasta aquí: renueva el reloj del watchdog (en
     * carro_recibir_comando) y se guarda como comando actual. Es el
     * comportamiento del código original; se deja igual a propósito.
     * Ver docs/pendientes_firmware.md. */
    guardar_comando_actual(texto);
}

/* ── Funciones públicas ────────────────────────────────────────────────── */

/* Estado de arranque: motores parados, comando "STOP", relojes en cero. */
void carro_iniciar(void)
{
    poner_motores(0, 0, 0, 0);
    guardar_comando_actual("STOP");
    ultimo_comando_ms   = 0;
    ultimo_keepalive_ms = 0;
    keepalive_alto      = 0;
}

/* Cada mensaje recibido pasa por aquí. PING NO renueva el reloj: si lo
 * hiciera, el carro seguiría moviéndose con el botón suelto, porque la
 * página envía PING cada 800 ms (más rápido que el watchdog de 1000 ms). */
void carro_recibir_comando(const char *texto)
{
    if (strcmp(texto, "PING") != 0) {
        ultimo_comando_ms = hw_millis();
    }
    ejecutar_comando(texto);
}

const char *carro_respuesta(const char *texto)
{
    if (texto != NULL && strcmp(texto, "PING") == 0) {
        return "PONG";
    }
    return NULL;
}

/* Freno inmediato (el antiguo hardStop). Lo usan STOP, EBRAKE, el watchdog
 * y los eventos de conexión/desconexión del WebSocket. */
void carro_detener(void)
{
    poner_motores(0, 0, 0, 0);
    guardar_comando_actual("STOP");
}

/* Watchdog de software. La resta sin signo en uint32_t funciona bien aunque
 * el reloj dé la vuelta (desbordamiento). */
void carro_vigilar_comunicacion(void)
{
    uint32_t ahora = hw_millis();

    if (motores_en_marcha && (uint32_t)(ahora - ultimo_comando_ms) > TIEMPO_WATCHDOG_MS) {
        carro_detener();
        hw_registrar("Watchdog: Freno activado.");
    }
}

/* Keep-alive: sube el pin cada KEEPALIVE_PERIODO_MS y lo baja tras
 * KEEPALIVE_PULSO_MS. Se usa una variable (keepalive_alto) en lugar de leer
 * el pin con digitalRead, que en C no tenemos. */
void carro_actualizar_keepalive(void)
{
    uint32_t ahora = hw_millis();

    if ((uint32_t)(ahora - ultimo_keepalive_ms) >= KEEPALIVE_PERIODO_MS) {
        hw_keepalive(1);
        keepalive_alto = 1;
        ultimo_keepalive_ms = ahora;
    } else if (keepalive_alto && (uint32_t)(ahora - ultimo_keepalive_ms) >= KEEPALIVE_PULSO_MS) {
        hw_keepalive(0);
        keepalive_alto = 0;
    }
}
