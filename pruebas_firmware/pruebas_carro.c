/*
 * pruebas_carro.c — Pruebas en PC (sin placa) de ../firmware_carro/carro.c
 *
 * Compila carro.c junto con versiones FALSAS de las funciones hw_...:
 *   - hw_motor_pwm  guarda el último valor de cada pin y cuenta las escrituras.
 *   - hw_millis     devuelve un reloj que las pruebas mueven a mano.
 *   - hw_keepalive  guarda el nivel del pin keep-alive.
 *   - hw_registrar  cuenta los mensajes de serie.
 *
 * Parte 1: casos puntuales (cada uno imprime OK o FALLO).
 * Parte 2: comparación "diferencial" contra una transcripción fiel del
 *          código ORIGINAL (codigo_Carrito.ino, versión con String), usando
 *          secuencias aleatorias de comandos y tiempos.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "carro.h"
#include "config.h"

/* ── Hardware falso ────────────────────────────────────────────────────── */

static int      pwm[64];            /* último PWM escrito en cada pin */
static int      escrituras_pwm;     /* cuántas veces se llamó a hw_motor_pwm */
static int      nivel_keepalive;
static int      mensajes_serie;
static uint32_t reloj_ms;

void hw_motor_pwm(int pin, int valor) { pwm[pin] = valor; escrituras_pwm++; }
uint32_t hw_millis(void)              { return reloj_ms; }
void hw_keepalive(int nivel)          { nivel_keepalive = nivel; }
void hw_registrar(const char *texto)  { (void)texto; mensajes_serie++; }

static void reiniciar_mundo(uint32_t reloj_inicial)
{
    memset(pwm, 0, sizeof pwm);
    escrituras_pwm  = 0;
    nivel_keepalive = 0;
    mensajes_serie  = 0;
    reloj_ms        = reloj_inicial;
    carro_iniciar();
    escrituras_pwm  = 0;   /* no contar las escrituras de carro_iniciar */
}

/* ── Utilidades de prueba ──────────────────────────────────────────────── */

static int fallos = 0;
static int casos  = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) fallos++;
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* Comprueba el PWM de los 4 pines: izq_a, izq_b, der_a, der_b. */
static int pines_son(int ia, int ib, int da, int db)
{
    return pwm[PIN_MOTOR_IZQ_A] == ia && pwm[PIN_MOTOR_IZQ_B] == ib &&
           pwm[PIN_MOTOR_DER_A] == da && pwm[PIN_MOTOR_DER_B] == db;
}

/* Un "ciclo de loop()": mismo orden que en el .ino. */
static void vuelta_loop(void)
{
    carro_vigilar_comunicacion();
    carro_actualizar_keepalive();
}

/* ── Parte 1: casos puntuales ──────────────────────────────────────────── */

static void prueba_movimientos(void)
{
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    caso("FORWARD: izq usa pin B (26), der usa pin A (25), a 255", pines_son(0, VELOCIDAD_IZQ, VELOCIDAD_DER, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("REVERSE");
    caso("REVERSE: izq usa pin A (27), der usa pin B (33), a 255", pines_son(VELOCIDAD_IZQ, 0, 0, VELOCIDAD_DER));

    reiniciar_mundo(0);
    carro_recibir_comando("LEFT");
    caso("LEFT: pivote, solo el motor derecho (pin 25) a 255", pines_son(0, 0, 255, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("RIGHT");
    caso("RIGHT: pivote, solo el motor izquierdo (pin 26) a 255", pines_son(0, 255, 0, 0));
}

static void prueba_stop_y_ebrake(void)
{
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    carro_recibir_comando("STOP");
    caso("STOP detiene los 4 pines", pines_son(0, 0, 0, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("REVERSE");
    carro_recibir_comando("EBRAKE");
    caso("EBRAKE detiene los 4 pines", pines_son(0, 0, 0, 0));

    /* STOP y EBRAKE repetidos SIEMPRE se ejecutan (vuelven a escribir). */
    reiniciar_mundo(0);
    carro_recibir_comando("STOP");
    int a = escrituras_pwm;
    carro_recibir_comando("STOP");
    caso("STOP repetido se vuelve a ejecutar (4 escrituras más)", escrituras_pwm == a + 4);

    reiniciar_mundo(0);
    carro_recibir_comando("EBRAKE");
    a = escrituras_pwm;
    carro_recibir_comando("EBRAKE");
    caso("EBRAKE repetido se vuelve a ejecutar (4 escrituras más)", escrituras_pwm == a + 4);

    /* Si algo dejó PWM en los pines, un STOP repetido igual lo apaga. */
    reiniciar_mundo(0);
    carro_recibir_comando("STOP");
    pwm[PIN_MOTOR_IZQ_B] = 99;   /* simula pin sucio */
    carro_recibir_comando("STOP");
    caso("STOP repetido limpia un pin con PWM residual", pines_son(0, 0, 0, 0));
}

static void prueba_repetidos(void)
{
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    int a = escrituras_pwm;
    carro_recibir_comando("FORWARD");
    carro_recibir_comando("FORWARD");
    caso("FORWARD repetido se ignora (0 escrituras nuevas)", escrituras_pwm == a);

    carro_recibir_comando("LEFT");
    caso("cambiar de FORWARD a LEFT sí actúa", pines_son(0, 0, 255, 0));
    a = escrituras_pwm;
    carro_recibir_comando("LEFT");
    caso("LEFT repetido se ignora", escrituras_pwm == a);
}

static void prueba_watchdog(void)
{
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");          /* t = 0 */
    reloj_ms = 1000; vuelta_loop();
    caso("watchdog: a 1000 ms exactos NO frena", pines_son(0, VELOCIDAD_IZQ, VELOCIDAD_DER, 0));
    reloj_ms = 1001; vuelta_loop();
    caso("watchdog: a 1001 ms frena todo", pines_son(0, 0, 0, 0));
    caso("watchdog: escribe el mensaje de serie", mensajes_serie == 1);
    reloj_ms = 5000; vuelta_loop();
    caso("watchdog: ya parado, no repite el mensaje", mensajes_serie == 1);

    /* Un comando repetido (ignorado) igual renueva el reloj: es el heartbeat. */
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 900;  carro_recibir_comando("FORWARD");
    reloj_ms = 1500; vuelta_loop();
    caso("heartbeat: FORWARD repetido renueva el watchdog", pines_son(0, VELOCIDAD_IZQ, VELOCIDAD_DER, 0));
    reloj_ms = 1901; vuelta_loop();
    caso("heartbeat: 1001 ms después del último, frena", pines_son(0, 0, 0, 0));

    /* Con motores parados el watchdog no hace nada. */
    reiniciar_mundo(0);
    reloj_ms = 9000; vuelta_loop();
    caso("watchdog: con motores parados no actúa ni escribe", escrituras_pwm == 0 && mensajes_serie == 0);
}

static void prueba_ping(void)
{
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 500; carro_recibir_comando("PING");
    reloj_ms = 900; carro_recibir_comando("PING");
    reloj_ms = 1001; vuelta_loop();
    caso("PING no renueva el watchdog: frena a 1001 ms del FORWARD", pines_son(0, 0, 0, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    int a = escrituras_pwm;
    carro_recibir_comando("PING");
    caso("PING no mueve ni cambia los motores", escrituras_pwm == a && pines_son(0, VELOCIDAD_IZQ, VELOCIDAD_DER, 0));
    carro_recibir_comando("FORWARD");
    caso("PING no cambia el comando activo (FORWARD sigue ignorado)", escrituras_pwm == a);

    reiniciar_mundo(0);
    carro_recibir_comando("PING");
    caso("PING con carro parado no escribe nada", escrituras_pwm == 0);
}

static void prueba_respuesta_ping(void)
{
    caso("PING se contesta con PONG", carro_respuesta("PING") != NULL && strcmp(carro_respuesta("PING"), "PONG") == 0);
    caso("los comandos de movimiento y STOP no se contestan", carro_respuesta("FORWARD") == NULL && carro_respuesta("REVERSE") == NULL &&
         carro_respuesta("LEFT") == NULL && carro_respuesta("RIGHT") == NULL && carro_respuesta("STOP") == NULL && carro_respuesta("EBRAKE") == NULL);
    caso("un texto desconocido, vacio, en minusculas o parecido a PING no se contesta", carro_respuesta("") == NULL && carro_respuesta("ping") == NULL &&
         carro_respuesta("PINGX") == NULL && carro_respuesta("PIN") == NULL && carro_respuesta("PONG") == NULL);
    caso("texto NULL: no se contesta ni revienta", carro_respuesta(NULL) == NULL);
    /* Contestar no cambia nada del carro: sigue sin renovar el watchdog ni mover motores. */
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    {
        int antes = escrituras_pwm;

        reloj_ms = 500;
        carro_recibir_comando("PING");
        (void)carro_respuesta("PING");
        reloj_ms = 1001;
        vuelta_loop();
        caso("contestar el PING no renueva el watchdog (frena a 1001 ms del FORWARD) ni mueve los motores", escrituras_pwm > antes && pines_son(0, 0, 0, 0));
    }
}

static void prueba_desconocido(void)
{
    /* Igual que el original: no mueve motores, pero renueva el watchdog y
     * se guarda como comando actual. OJO: pendiente (ver docs). */
    reiniciar_mundo(0);
    carro_recibir_comando("HOLA");
    caso("desconocido con carro parado: no escribe nada", escrituras_pwm == 0 && pines_son(0, 0, 0, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    int a = escrituras_pwm;
    reloj_ms = 600; carro_recibir_comando("HOLA");
    caso("desconocido en marcha: no cambia los motores", escrituras_pwm == a && pines_son(0, VELOCIDAD_IZQ, VELOCIDAD_DER, 0));
    reloj_ms = 1500; vuelta_loop();
    caso("desconocido RENUEVA el watchdog (como el original)", pines_son(0, VELOCIDAD_IZQ, VELOCIDAD_DER, 0));
    reloj_ms = 1601; vuelta_loop();
    caso("... y frena 1001 ms después del desconocido", pines_son(0, 0, 0, 0));

    /* El desconocido pasa a ser el comando actual: FORWARD vuelve a aplicarse. */
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    carro_recibir_comando("HOLA");
    a = escrituras_pwm;
    carro_recibir_comando("FORWARD");
    caso("tras un desconocido, FORWARD se vuelve a aplicar (4 escrituras)", escrituras_pwm == a + 4);

    /* Comando larguísimo: no debe romper nada. */
    reiniciar_mundo(0);
    {
        char largo[LARGO_MAX_COMANDO];
        memset(largo, 'A', sizeof largo - 1);
        largo[sizeof largo - 1] = '\0';
        carro_recibir_comando(largo);
        caso("comando del largo máximo se trata como desconocido", pines_son(0, 0, 0, 0));
    }
}

static void prueba_keepalive(void)
{
    reiniciar_mundo(0);
    reloj_ms = 1999; vuelta_loop();
    caso("keep-alive: a 1999 ms sigue en bajo", nivel_keepalive == 0);
    reloj_ms = 2000; vuelta_loop();
    caso("keep-alive: a 2000 ms sube", nivel_keepalive == 1);
    reloj_ms = 2049; vuelta_loop();
    caso("keep-alive: a 49 ms de pulso sigue en alto", nivel_keepalive == 1);
    reloj_ms = 2050; vuelta_loop();
    caso("keep-alive: a 50 ms de pulso baja", nivel_keepalive == 0);
    reloj_ms = 3999; vuelta_loop();
    caso("keep-alive: a 3999 ms sigue en bajo", nivel_keepalive == 0);
    reloj_ms = 4000; vuelta_loop();
    caso("keep-alive: a 4000 ms vuelve a subir", nivel_keepalive == 1);
}

/* Desbordamiento de millis(): de 0xFFFFFF00 a 0x00000100 y más allá. */
static void prueba_desbordamiento(void)
{
    reiniciar_mundo(0xFFFFFF00u);
    carro_recibir_comando("FORWARD");
    reloj_ms = 0x00000100u; vuelta_loop();            /* pasaron 512 ms */
    caso("desbordamiento: 512 ms después del FORWARD sigue en marcha", pines_son(0, VELOCIDAD_IZQ, VELOCIDAD_DER, 0));
    reloj_ms = 0x000002E8u; vuelta_loop();            /* pasaron 1000 ms exactos */
    caso("desbordamiento: 1000 ms exactos NO frena", pines_son(0, VELOCIDAD_IZQ, VELOCIDAD_DER, 0));
    reloj_ms = 0x000002E9u; vuelta_loop();            /* 1001 ms */
    caso("desbordamiento: 1001 ms frena", pines_son(0, 0, 0, 0));

    reiniciar_mundo(0xFFFFFF00u);
    vuelta_loop();                                    /* primer pulso (hace mucho que ultimo=0) */
    caso("desbordamiento keep-alive: primer pulso sube", nivel_keepalive == 1);
    reloj_ms = 0x00000100u; vuelta_loop();            /* 512 ms después: ya pasaron los 50 ms */
    caso("desbordamiento keep-alive: cruza el cero y baja", nivel_keepalive == 0);
    reloj_ms = 0x000006CFu; vuelta_loop();            /* 1999 ms después */
    caso("desbordamiento keep-alive: a 1999 ms sigue en bajo", nivel_keepalive == 0);
    reloj_ms = 0x000006D0u; vuelta_loop();            /* 2000 ms después */
    caso("desbordamiento keep-alive: a 2000 ms sube otra vez", nivel_keepalive == 1);
}

/* ── Parte 2: comparación con el código original ───────────────────────── */
/*
 * Transcripción fiel de codigo_Carrito.ino (lógica de motores, watchdog y
 * keep-alive) con las mismas variables y la misma estructura de if/else.
 * Las diferencias son solo de lenguaje: String → char[] y millis() → reloj
 * de pruebas. digitalRead() se imita leyendo el nivel que la propia
 * transcripción escribió en su pin.
 */
static int      o_pwm[64];
static int      o_keepalive;
static uint32_t o_ultimo_usuario;
static int      o_motores_en_marcha;
static char     o_comando_actual[64];
static uint32_t o_ultimo_keepalive;

static void o_set_motors(int m1a, int m1b, int m2a, int m2b)
{
    o_pwm[27] = m1a; o_pwm[26] = m1b; o_pwm[25] = m2a; o_pwm[33] = m2b;
    o_motores_en_marcha = (m1a > 0 || m1b > 0 || m2a > 0 || m2b > 0);
}

static void o_apply_motors(const char *dir, int spd)
{
    /* La única diferencia deliberada con el original: avanzar y retroceder usan el ajuste por lado (VELOCIDAD_IZQ/DER) para ir recto. */
    (void)spd;
    if      (!strcmp(dir, "FORWARD")) o_set_motors(0,   VELOCIDAD_IZQ, VELOCIDAD_DER, 0);
    else if (!strcmp(dir, "REVERSE")) o_set_motors(VELOCIDAD_IZQ, 0, 0, VELOCIDAD_DER);
    else if (!strcmp(dir, "LEFT"))    o_set_motors(0,   0,   255, 0);
    else if (!strcmp(dir, "RIGHT"))   o_set_motors(0,   255, 0,   0);
}

static void o_hard_stop(void)
{
    o_set_motors(0, 0, 0, 0);
    strcpy(o_comando_actual, "STOP");
}

static void o_execute(const char *cmd)
{
    if (!strcmp(cmd, o_comando_actual) && strcmp(cmd, "STOP") && strcmp(cmd, "EBRAKE")) {
        return;
    }
    if (!strcmp(cmd, "FORWARD") || !strcmp(cmd, "REVERSE")) {
        o_apply_motors(cmd, 255);
    } else if (!strcmp(cmd, "LEFT") || !strcmp(cmd, "RIGHT")) {
        o_apply_motors(cmd, 255);
    } else if (!strcmp(cmd, "STOP") || !strcmp(cmd, "EBRAKE")) {
        o_hard_stop();
        return;
    } else if (!strcmp(cmd, "PING")) {
        return;
    }
    strncpy(o_comando_actual, cmd, sizeof o_comando_actual - 1);
    o_comando_actual[sizeof o_comando_actual - 1] = '\0';
}

static void o_recibir(const char *msg)
{
    if (strcmp(msg, "PING")) o_ultimo_usuario = hw_millis();
    o_execute(msg);
}

static void o_loop(void)
{
    if (o_motores_en_marcha && (uint32_t)(hw_millis() - o_ultimo_usuario) > 1000) {
        o_hard_stop();
    }
    if ((uint32_t)(hw_millis() - o_ultimo_keepalive) >= 2000) {
        o_keepalive = 1;
        o_ultimo_keepalive = hw_millis();
    } else if (o_keepalive && (uint32_t)(hw_millis() - o_ultimo_keepalive) >= 50) {
        o_keepalive = 0;
    }
}

static void o_reiniciar(void)
{
    memset(o_pwm, 0, sizeof o_pwm);
    o_keepalive = 0;
    o_ultimo_usuario = 0;
    o_motores_en_marcha = 0;
    strcpy(o_comando_actual, "STOP");
    o_ultimo_keepalive = 0;
}

static int mismo_estado(void)
{
    return pwm[27] == o_pwm[27] && pwm[26] == o_pwm[26] &&
           pwm[25] == o_pwm[25] && pwm[33] == o_pwm[33] &&
           nivel_keepalive == o_keepalive;
}

/* xorshift32: números "aleatorios" repetibles. */
static uint32_t semilla = 12345u;
static uint32_t aleatorio(void)
{
    semilla ^= semilla << 13; semilla ^= semilla >> 17; semilla ^= semilla << 5;
    return semilla;
}

static void prueba_diferencial(uint32_t reloj_inicial, const char *nombre)
{
    static const char *entradas[] = {
        "FORWARD", "REVERSE", "LEFT", "RIGHT", "STOP", "EBRAKE", "PING",
        "HOLA", "", "FORWARDX", "forward", "FORWARD", "FORWARD", "PING"
    };
    const int n_entradas = (int)(sizeof entradas / sizeof entradas[0]);
    long pasos = 300000, distintos = 0;

    reiniciar_mundo(reloj_inicial);
    o_reiniciar();

    for (long i = 0; i < pasos; i++) {
        uint32_t r = aleatorio();
        reloj_ms += (r >> 8) % 700;                   /* avanza 0..699 ms */
        if ((r & 3) != 0) {                           /* 3 de cada 4 pasos llega un mensaje */
            const char *msg = entradas[(r >> 4) % n_entradas];
            carro_recibir_comando(msg);
            o_recibir(msg);
        }
        vuelta_loop();
        o_loop();
        if (!mismo_estado()) distintos++;
    }
    {
        char texto[160];
        snprintf(texto, sizeof texto,
                 "comparación con el original (%s): %ld pasos, %ld diferencias",
                 nombre, pasos, distintos);
        caso(texto, distintos == 0);
    }
}

/* ── Programa principal ────────────────────────────────────────────────── */

int main(void)
{
    printf("== Parte 1: casos puntuales ==\n");
    prueba_movimientos();
    prueba_stop_y_ebrake();
    prueba_repetidos();
    prueba_watchdog();
    prueba_ping();
    prueba_respuesta_ping();
    prueba_desconocido();
    prueba_keepalive();
    prueba_desbordamiento();

    printf("== Parte 2: comparación con el código original ==\n");
    prueba_diferencial(0, "reloj desde 0");
    prueba_diferencial(0xFFFFF000u, "reloj cruzando el desbordamiento");

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
