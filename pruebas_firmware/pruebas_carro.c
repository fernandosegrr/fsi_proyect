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

/* Memoria no volátil falsa: el balance guardado de cada perfil (0 = nada guardado) y cuántas veces se guardó. */
static int      nvs_balance[2];
static int      nvs_guardados;
int  hw_balance_leer(int perfil)                  { return nvs_balance[perfil]; }
void hw_balance_guardar(int perfil, int milesimas) { nvs_balance[perfil] = milesimas; nvs_guardados++; }

static void reiniciar_mundo(uint32_t reloj_inicial)
{
    memset(pwm, 0, sizeof pwm);
    escrituras_pwm  = 0;
    nivel_keepalive = 0;
    mensajes_serie  = 0;
    reloj_ms        = reloj_inicial;
    memset(nvs_balance, 0, sizeof nvs_balance);   /* un carro recién salido de fábrica: sin balance guardado */
    nvs_guardados   = 0;
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
    caso("FORWARD: izq usa pin B (26), der usa pin A (25), a 255", pines_son(0, 255, 255, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("REVERSE");
    caso("REVERSE: izq usa pin A (27), der usa pin B (33), a 255", pines_son(255, 0, 0, 255));

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
    caso("watchdog: a 1000 ms exactos NO frena", pines_son(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));
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
    caso("heartbeat: FORWARD repetido renueva el watchdog", pines_son(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));
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
    caso("PING no mueve ni cambia los motores", escrituras_pwm == a && pines_son(0, 255, 255, 0));
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

/* Arranque a plena potencia: los dos motores al 100 % los primeros PERFIL_AUTO_ARRANQUE_PLENO_MS y después el ajuste por lado. */
static void prueba_arranque_pleno(void)
{
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    caso("FORWARD: al arrancar los dos motores van al 100 %", pines_son(0, 255, 255, 0));
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS - 1;
    vuelta_loop();
    caso("FORWARD: un ms antes de terminar el arranque siguen al 100 %", pines_son(0, 255, 255, 0));
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS;
    vuelta_loop();
    caso("FORWARD: al terminar el arranque se aplica el ajuste por lado", pines_son(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("REVERSE");
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS;
    vuelta_loop();
    caso("REVERSE: igual, ajuste por lado después del arranque", pines_son(PERFIL_AUTO_VELOCIDAD_IZQ, 0, 0, PERFIL_AUTO_VELOCIDAD_DER));

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 100;
    carro_recibir_comando("STOP");
    reloj_ms = 1000;
    vuelta_loop();
    caso("STOP en pleno arranque: se queda detenido (el ajuste no revive los motores)", pines_son(0, 0, 0, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 100;
    carro_recibir_comando("LEFT");
    reloj_ms = 1000;
    carro_recibir_comando("LEFT");
    vuelta_loop();
    caso("girar en pleno arranque: el pivote sigue al 100 % y no se pisa con el ajuste", pines_son(0, 0, 255, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 100;
    carro_recibir_comando("HOLA");
    reloj_ms = 400;
    carro_recibir_comando("HOLA");
    vuelta_loop();
    caso("un comando desconocido en pleno arranque no evita el ajuste al terminar", pines_son(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));
}

/* ── Perfiles de velocidad: MODO_MANUAL (255) y MODO_AUTO (210) ───────────── */

static void prueba_perfiles(void)
{
    /* Por defecto el perfil es AUTO (el seguro) y trae el ajuste de 210. */
    reiniciar_mundo(0);
    caso("al arrancar el perfil es AUTO", carro_perfil() == PERFIL_AUTO);
    carro_recibir_comando("FORWARD");
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("AUTO: FORWARD tras el arranque a plena potencia va a 207 (izq) y 210 (der)", pines_son(0, 207, 210, 0) && PERFIL_AUTO_VELOCIDAD_IZQ == 207 && PERFIL_AUTO_VELOCIDAD_DER == 210);

    /* MODO_MANUAL / MODO_AUTO se contestan con ACK y cambian el perfil. */
    caso("MODO_MANUAL se contesta con ACK_MANUAL", carro_respuesta("MODO_MANUAL") != NULL && strcmp(carro_respuesta("MODO_MANUAL"), "ACK_MANUAL") == 0);
    caso("MODO_AUTO se contesta con ACK_AUTO", carro_respuesta("MODO_AUTO") != NULL && strcmp(carro_respuesta("MODO_AUTO"), "ACK_AUTO") == 0);
    caso("textos parecidos (minusculas, con espacios, con parametro) no se contestan",
         carro_respuesta("modo_manual") == NULL && carro_respuesta("MODO_MANUAL ") == NULL && carro_respuesta("MODO_AUTO:1") == NULL &&
         carro_respuesta("MODO") == NULL && carro_respuesta("ACK_AUTO") == NULL);

    /* Perfil MANUAL: 255 con el ajuste validado (252 / 255) y el mismo arranque. */
    reiniciar_mundo(0);
    carro_recibir_comando("MODO_MANUAL");
    caso("MODO_MANUAL cambia el perfil a MANUAL", carro_perfil() == PERFIL_MANUAL);
    caso("MODO_MANUAL con el carro parado no escribe ningun PWM", escrituras_pwm == 0 && pines_son(0, 0, 0, 0));
    carro_recibir_comando("FORWARD");
    caso("MANUAL: FORWARD arranca los dos motores a 255", pines_son(0, 255, 255, 0));
    reloj_ms = PERFIL_MANUAL_ARRANQUE_PLENO_MS - 1; vuelta_loop();
    caso("MANUAL: un ms antes del fin del arranque siguen a 255", pines_son(0, 255, 255, 0));
    reloj_ms = PERFIL_MANUAL_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("MANUAL: tras el arranque va a 252 (izq) y 255 (der), el ajuste validado", pines_son(0, 252, 255, 0) &&
         PERFIL_MANUAL_VELOCIDAD_IZQ == 252 && PERFIL_MANUAL_VELOCIDAD_DER == 255 && PERFIL_MANUAL_ARRANQUE_PLENO_MS == 250u);
    reiniciar_mundo(0);
    carro_recibir_comando("MODO_MANUAL");
    carro_recibir_comando("REVERSE");
    reloj_ms = PERFIL_MANUAL_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("MANUAL: REVERSE tras el arranque va a 252 (izq) y 255 (der)", pines_son(252, 0, 0, 255));

    /* Los pivotes van a 255 en los dos perfiles. */
    reiniciar_mundo(0);
    carro_recibir_comando("LEFT");
    caso("AUTO: el pivote a la izquierda sigue a 255", pines_son(0, 0, 255, 0));
    carro_recibir_comando("STOP");
    carro_recibir_comando("MODO_MANUAL");
    carro_recibir_comando("RIGHT");
    caso("MANUAL: el pivote a la derecha sigue a 255", pines_son(0, 255, 0, 0));

    /* Cambiar de perfil con el carro en movimiento: primero STOP. */
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 400; vuelta_loop();
    caso("(antes de cambiar) AUTO avanzando a 207 / 210", pines_son(0, 207, 210, 0));
    carro_recibir_comando("MODO_MANUAL");
    caso("MODO_MANUAL con el carro en movimiento: primero STOP (los 4 pines en 0) y perfil MANUAL", pines_son(0, 0, 0, 0) && carro_perfil() == PERFIL_MANUAL);
    reloj_ms = 1500; vuelta_loop();
    caso("... y no se reanuda solo (ni con el ajuste del arranque)", pines_son(0, 0, 0, 0));
    carro_recibir_comando("FORWARD");
    caso("... el siguiente FORWARD se vuelve a aplicar (el MODO no cuenta como comando de movimiento)", pines_son(0, 255, 255, 0));
    reloj_ms += PERFIL_MANUAL_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("... y va a 252 / 255", pines_son(0, 252, 255, 0));
    carro_recibir_comando("MODO_AUTO");
    caso("MODO_AUTO en movimiento (MANUAL -> AUTO): primero STOP", pines_son(0, 0, 0, 0) && carro_perfil() == PERFIL_AUTO);

    /* El mismo perfil otra vez NO frena (la app lo repite de vez en cuando). */
    reiniciar_mundo(0);
    carro_recibir_comando("MODO_MANUAL");
    carro_recibir_comando("FORWARD");
    reloj_ms = 300; vuelta_loop();
    {
        int antes = escrituras_pwm;

        reloj_ms = 400; carro_recibir_comando("MODO_MANUAL");
        caso("MODO_MANUAL repetido en MANUAL y en marcha: no frena ni escribe nada", escrituras_pwm == antes && pines_son(0, 252, 255, 0));
        reloj_ms = 1300; vuelta_loop();
        caso("... pero renueva el watchdog como cualquier comando real (a 900 ms del MODO sigue en marcha)", pines_son(0, 252, 255, 0));
        reloj_ms = 1401; vuelta_loop();
        caso("... y frena 1001 ms despues", pines_son(0, 0, 0, 0));
    }

    /* STOP y EBRAKE no cambian el perfil. */
    reiniciar_mundo(0);
    carro_recibir_comando("MODO_MANUAL");
    carro_recibir_comando("FORWARD");
    carro_recibir_comando("STOP");
    caso("STOP no cambia el perfil", carro_perfil() == PERFIL_MANUAL);
    carro_recibir_comando("FORWARD");
    carro_recibir_comando("EBRAKE");
    caso("EBRAKE no cambia el perfil", carro_perfil() == PERFIL_MANUAL);

    /* PING y comandos desconocidos tampoco. */
    carro_recibir_comando("PING");
    carro_recibir_comando("HOLA");
    caso("PING y un comando desconocido no cambian el perfil", carro_perfil() == PERFIL_MANUAL);
    carro_recibir_comando("MODO_MANUALX");
    carro_recibir_comando("modo_auto");
    caso("MODO_MANUALX y modo_auto (no son comandos) no cambian el perfil", carro_perfil() == PERFIL_MANUAL);

    /* Perder la señal (watchdog) vuelve a AUTO. */
    reiniciar_mundo(0);
    carro_recibir_comando("MODO_MANUAL");
    carro_recibir_comando("FORWARD");
    reloj_ms = 1001; vuelta_loop();
    caso("el watchdog frena y devuelve el perfil a AUTO", pines_son(0, 0, 0, 0) && carro_perfil() == PERFIL_AUTO);

    /* Conectar o desconectar el cliente (lo hace el .ino con carro_detener + carro_poner_perfil_auto). */
    reiniciar_mundo(0);
    carro_recibir_comando("MODO_MANUAL");
    carro_detener();
    caso("carro_detener por si solo no cambia el perfil (lo usa STOP)", carro_perfil() == PERFIL_MANUAL);
    carro_poner_perfil_auto();
    caso("carro_poner_perfil_auto (al conectar o reconectar un cliente) deja el perfil AUTO", carro_perfil() == PERFIL_AUTO);
    carro_recibir_comando("MODO_MANUAL");
    carro_iniciar();
    caso("carro_iniciar (arranque) deja el perfil AUTO", carro_perfil() == PERFIL_AUTO);

    /* En los dos perfiles el arranque a plena potencia es de 250 ms. */
    caso("el arranque a plena potencia es de 250 ms en los dos perfiles", PERFIL_MANUAL_ARRANQUE_PLENO_MS == 250u && PERFIL_AUTO_ARRANQUE_PLENO_MS == 250u);
}

/* ── Balance (equilibrio de los lados al ir recto) ─────────────────────── */

static int es_respuesta(const char *texto, const char *esperada)
{
    const char *r = carro_respuesta(texto);

    return r != NULL && strcmp(r, esperada) == 0;
}

static void prueba_balance(void)
{
    /* Los valores de siempre son los por defecto. */
    reiniciar_mundo(0);
    caso("balance por defecto: AUTO 986 y MANUAL 988 (los de siempre: 207/210 y 252/255)", carro_balance(PERFIL_AUTO) == 986 && carro_balance(PERFIL_MANUAL) == 988 &&
         PERFIL_AUTO_VELOCIDAD_IZQ == 207 && PERFIL_MANUAL_VELOCIDAD_IZQ == 252);
    caso("carro_balance con un perfil que no existe da 0", carro_balance(7) == 0 && carro_balance(-1) == 0);

    /* Cambiar el balance de AUTO: se contesta, se guarda y se aplica al siguiente arranque. */
    reiniciar_mundo(0);
    caso("BAL_AUTO:900 se contesta ACK_BAL_AUTO:900", es_respuesta("BAL_AUTO:900", "ACK_BAL_AUTO:900"));
    carro_recibir_comando("BAL_AUTO:900");
    caso("... el balance de AUTO queda en 900, el de MANUAL no cambia y se guardó una vez en la memoria", carro_balance(PERFIL_AUTO) == 900 && carro_balance(PERFIL_MANUAL) == 988 &&
         nvs_balance[PERFIL_AUTO] == 900 && nvs_balance[PERFIL_MANUAL] == 0 && nvs_guardados == 1);
    caso("... no mueve los motores ni escribe PWM", escrituras_pwm == 0 && pines_son(0, 0, 0, 0));
    carro_recibir_comando("FORWARD");
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("AUTO con balance 900: el lado izquierdo baja a 189 (210 x 0.900) y el derecho se queda en el tope de 210", pines_son(0, 189, 210, 0));
    carro_recibir_comando("STOP");
    carro_recibir_comando("REVERSE");
    reloj_ms += PERFIL_AUTO_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("... y al retroceder igual: izquierdo 189, derecho 210", pines_son(189, 0, 0, 210));
    carro_recibir_comando("STOP");

    /* El balance de MANUAL es aparte. */
    carro_recibir_comando("MODO_MANUAL");
    carro_recibir_comando("BAL_MANUAL:950");
    caso("BAL_MANUAL:950 se contesta ACK_BAL_MANUAL:950 y queda guardado aparte", es_respuesta("BAL_MANUAL:950", "ACK_BAL_MANUAL:950") && carro_balance(PERFIL_MANUAL) == 950 &&
         carro_balance(PERFIL_AUTO) == 900 && nvs_balance[PERFIL_MANUAL] == 950);
    carro_recibir_comando("FORWARD");
    reloj_ms += PERFIL_MANUAL_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("MANUAL con balance 950: el izquierdo baja a 242 (255 x 0.950) y el derecho se queda en 255", pines_son(0, 242, 255, 0));
    carro_recibir_comando("STOP");

    /* Los extremos y el tope: nunca por encima del tope del perfil. */
    reiniciar_mundo(0);
    carro_recibir_comando("BAL_AUTO:1000");
    carro_recibir_comando("FORWARD");
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("balance 1000: los dos lados iguales, 210 y 210 (nunca por encima del tope del perfil)", pines_son(0, 210, 210, 0));
    reiniciar_mundo(0);
    carro_recibir_comando("BAL_AUTO:800");
    carro_recibir_comando("FORWARD");
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("balance 800 (el mas bajo): el izquierdo baja a 168 y el derecho sigue en 210", pines_son(0, 168, 210, 0));
    {
        int perfil, ok = 1;

        for (perfil = 0; perfil < 2; perfil++) {
            int mil;

            for (mil = BALANCE_MIN_MIL; mil <= BALANCE_MAX_MIL; mil++) {
                int tope = (perfil == PERFIL_AUTO) ? PERFIL_AUTO_VELOCIDAD_DER : PERFIL_MANUAL_VELOCIDAD_DER;
                int izq = (tope * mil + 500) / 1000;

                if (izq > tope || izq < tope * 800 / 1000 - 1) {
                    ok = 0;
                }
            }
        }
        caso("con cualquier balance de 800 a 1000 el lado izquierdo nunca pasa del tope y nunca baja de 80 % del tope", ok);
    }

    /* Valores no válidos: NAK_BAL y nada cambia. */
    reiniciar_mundo(0);
    caso("fuera de rango (799, 1001, 0, 99, 5000) se contesta NAK_BAL", es_respuesta("BAL_AUTO:799", "NAK_BAL") && es_respuesta("BAL_AUTO:1001", "NAK_BAL") &&
         es_respuesta("BAL_AUTO:0", "NAK_BAL") && es_respuesta("BAL_MANUAL:99", "NAK_BAL") && es_respuesta("BAL_MANUAL:5000", "NAK_BAL"));
    caso("sin numero, con letras, con espacios, con signo o con decimales se contesta NAK_BAL", es_respuesta("BAL_AUTO:", "NAK_BAL") && es_respuesta("BAL_AUTO:abc", "NAK_BAL") &&
         es_respuesta("BAL_AUTO:9x0", "NAK_BAL") && es_respuesta("BAL_AUTO: 900", "NAK_BAL") && es_respuesta("BAL_AUTO:-900", "NAK_BAL") && es_respuesta("BAL_AUTO:900.5", "NAK_BAL") &&
         es_respuesta("BAL_AUTO:0900", "NAK_BAL") && es_respuesta("BAL_AUTO:900 ", "NAK_BAL"));
    carro_recibir_comando("BAL_AUTO:799");
    carro_recibir_comando("BAL_AUTO:abc");
    carro_recibir_comando("BAL_MANUAL:1001");
    carro_recibir_comando("BAL_AUTO:");
    carro_recibir_comando("BAL_AUTO:99999999999");
    caso("... y el balance no cambia ni se guarda nada", carro_balance(PERFIL_AUTO) == 986 && carro_balance(PERFIL_MANUAL) == 988 && nvs_guardados == 0);
    caso("textos parecidos que no son mensajes de balance no se contestan (BAL_AUTO sin dos puntos, bal_auto:900, BAL_X:900, BAL)", carro_respuesta("BAL_AUTO") == NULL &&
         carro_respuesta("bal_auto:900") == NULL && carro_respuesta("BAL_X:900") == NULL && carro_respuesta("BAL") == NULL && carro_respuesta("BAL?x") == NULL);

    /* BAL? */
    reiniciar_mundo(0);
    carro_recibir_comando("BAL_AUTO:910");
    carro_recibir_comando("BAL_MANUAL:960");
    caso("BAL? se contesta BAL:910:960 (AUTO y luego MANUAL)", es_respuesta("BAL?", "BAL:910:960"));
    reiniciar_mundo(0);
    caso("BAL? sin cambios contesta los valores por defecto BAL:986:988", es_respuesta("BAL?", "BAL:986:988"));

    /* Los mensajes de balance no son un movimiento. */
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 400; vuelta_loop();
    {
        int antes = escrituras_pwm;

        carro_recibir_comando("BAL_AUTO:900");
        caso("BAL_AUTO con el carro en marcha no frena ni toca los motores (se aplica al siguiente arranque)", escrituras_pwm == antes && pines_son(0, 207, 210, 0) && carro_perfil() == PERFIL_AUTO);
        carro_recibir_comando("FORWARD");
        caso("... y no cambia el comando actual (FORWARD repetido se sigue ignorando)", escrituras_pwm == antes);
        carro_recibir_comando("BAL?");
        caso("BAL? tampoco toca nada", escrituras_pwm == antes);
    }
    reloj_ms = 1400; vuelta_loop();
    caso("... renuevan el reloj del watchdog como cualquier comando real (1000 ms despues del BAL_AUTO sigue en marcha)", pines_son(0, 207, 210, 0));
    reloj_ms = 1401; vuelta_loop();
    caso("... y frena 1001 ms despues", pines_son(0, 0, 0, 0));
    reiniciar_mundo(0);
    carro_recibir_comando("MODO_MANUAL");
    carro_recibir_comando("BAL_AUTO:900");
    caso("BAL_AUTO no cambia el perfil", carro_perfil() == PERFIL_MANUAL);

    /* Persistencia: al arrancar se lee lo guardado; lo inválido se ignora. */
    reiniciar_mundo(0);
    nvs_balance[PERFIL_AUTO] = 905;
    nvs_balance[PERFIL_MANUAL] = 970;
    carro_iniciar();
    caso("al arrancar lee el balance guardado (905 y 970)", carro_balance(PERFIL_AUTO) == 905 && carro_balance(PERFIL_MANUAL) == 970);
    carro_recibir_comando("FORWARD");
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("... y lo usa al avanzar (210 x 0.905 = 190)", pines_son(0, 190, 210, 0));
    nvs_balance[PERFIL_AUTO] = 500;
    nvs_balance[PERFIL_MANUAL] = 1500;
    carro_iniciar();
    caso("un valor guardado fuera de rango (500 o 1500) se ignora y queda el por defecto", carro_balance(PERFIL_AUTO) == 986 && carro_balance(PERFIL_MANUAL) == 988);
    nvs_balance[PERFIL_AUTO] = 0;
    nvs_balance[PERFIL_MANUAL] = 800;
    carro_iniciar();
    caso("0 = nada guardado: AUTO por defecto; 800 si es valido en MANUAL", carro_balance(PERFIL_AUTO) == 986 && carro_balance(PERFIL_MANUAL) == 800);
    reiniciar_mundo(0);
    carro_recibir_comando("BAL_AUTO:880");
    carro_iniciar();   /* un reinicio en frio: la memoria falsa conserva lo guardado */
    caso("lo que se guardo con BAL_AUTO:880 sobrevive a un reinicio (carro_iniciar lo vuelve a leer)", carro_balance(PERFIL_AUTO) == 880);

    /* El watchdog y el watchdog de conexion no tocan el balance. */
    reiniciar_mundo(0);
    carro_recibir_comando("BAL_AUTO:900");
    carro_recibir_comando("FORWARD");
    reloj_ms = 1001; vuelta_loop();
    carro_detener();
    carro_poner_perfil_auto();
    caso("el watchdog, carro_detener y carro_poner_perfil_auto no cambian el balance", carro_balance(PERFIL_AUTO) == 900);
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
    caso("desconocido en marcha: no cambia los motores", escrituras_pwm == a && pines_son(0, 255, 255, 0));
    reloj_ms = 1500; vuelta_loop();
    caso("desconocido RENUEVA el watchdog (como el original)", pines_son(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));
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
    caso("desbordamiento: 512 ms después del FORWARD sigue en marcha", pines_son(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));
    reloj_ms = 0x000002E8u; vuelta_loop();            /* pasaron 1000 ms exactos */
    caso("desbordamiento: 1000 ms exactos NO frena", pines_son(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));
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
static int      o_pleno_modo;      /* modelo del arranque a plena potencia */
static uint32_t o_pleno_hasta;
static int      o_perfil;          /* modelo del perfil de velocidad: 0 = MANUAL, 1 = AUTO */

static void o_set_motors(int m1a, int m1b, int m2a, int m2b)
{
    o_pwm[27] = m1a; o_pwm[26] = m1b; o_pwm[25] = m2a; o_pwm[33] = m2b;
    o_motores_en_marcha = (m1a > 0 || m1b > 0 || m2a > 0 || m2b > 0);
}

static void o_apply_motors(const char *dir, int spd)
{
    /* La única diferencia deliberada con el original: avanzar y retroceder usan el ajuste por lado (PERFIL_AUTO_VELOCIDAD_IZQ/DER) para ir recto. */
    (void)spd;
    o_pleno_modo = 0;
    if      (!strcmp(dir, "FORWARD")) { o_set_motors(0, 255, 255, 0); o_pleno_modo = 1; o_pleno_hasta = hw_millis() + 250u; }
    else if (!strcmp(dir, "REVERSE")) { o_set_motors(255, 0, 0, 255); o_pleno_modo = 2; o_pleno_hasta = hw_millis() + 250u; }
    else if (!strcmp(dir, "LEFT"))    o_set_motors(0,   0,   255, 0);
    else if (!strcmp(dir, "RIGHT"))   o_set_motors(0,   255, 0,   0);
}

static void o_hard_stop(void)
{
    o_set_motors(0, 0, 0, 0);
    o_pleno_modo = 0;
    strcpy(o_comando_actual, "STOP");
}

static void o_execute(const char *cmd)
{
    if (!strcmp(cmd, "MODO_MANUAL") || !strcmp(cmd, "MODO_AUTO")) {   /* perfil: si cambia y el carro se mueve, primero STOP */
        int nuevo = !strcmp(cmd, "MODO_AUTO");

        if (nuevo != o_perfil) {
            if (o_motores_en_marcha) {
                o_hard_stop();
            }
            o_perfil = nuevo;
        }
        return;
    }
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
    if (o_pleno_modo != 0 && (int32_t)(hw_millis() - o_pleno_hasta) >= 0) {
        int izq = o_perfil ? 207 : 252, der = o_perfil ? 210 : 255;   /* los dos perfiles de config.h */

        if (o_pleno_modo == 1) o_set_motors(0, izq, der, 0);
        else                  o_set_motors(izq, 0, 0, der);
        o_pleno_modo = 0;
    }
    if (o_motores_en_marcha && (uint32_t)(hw_millis() - o_ultimo_usuario) > 1000) {
        o_hard_stop();
        o_perfil = 1;   /* el watchdog devuelve el perfil AUTO */
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
    o_pleno_modo = 0;
    o_perfil = 1;
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
        "HOLA", "", "FORWARDX", "forward", "FORWARD", "FORWARD", "PING",
        "MODO_MANUAL", "MODO_AUTO", "MODO_MANUAL", "MODO_MANUAL", "MODO_AUTOX"
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

/* ── Arranque escalonado (se compila con -DARRANQUE_ESCALONADO_MS=40: ver el Makefile) ─────────────── */

#if ARRANQUE_ESCALONADO_MS > 0
static void prueba_escalonado(void)
{
    const uint32_t e = ARRANQUE_ESCALONADO_MS;

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    caso("escalonado: al avanzar entra primero SOLO el motor izquierdo (pin B) a 255", pines_son(0, 255, 0, 0));
    reloj_ms = e - 1; vuelta_loop();
    caso("escalonado: un ms antes del tiempo el derecho todavia no entra", pines_son(0, 255, 0, 0));
    reloj_ms = e; vuelta_loop();
    caso("escalonado: al cumplirse el tiempo entra el derecho (pin A) tambien a 255", pines_son(0, 255, 255, 0));
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("escalonado: al terminar el arranque a plena potencia se aplica el ajuste por lado de siempre (207 y 210)", pines_son(0, 207, 210, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("REVERSE");
    caso("escalonado: al retroceder entra primero SOLO el motor izquierdo (pin A) a 255", pines_son(255, 0, 0, 0));
    reloj_ms = e; vuelta_loop();
    caso("escalonado: y luego el derecho (pin B)", pines_son(255, 0, 0, 255));
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("escalonado: retroceso con el ajuste por lado despues del arranque", pines_son(207, 0, 0, 210));

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 10;
    carro_recibir_comando("STOP");
    reloj_ms = 1000; vuelta_loop();
    caso("escalonado: STOP antes de que entre el segundo motor lo deja todo parado (no entra despues)", pines_son(0, 0, 0, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 10;
    carro_recibir_comando("LEFT");
    reloj_ms = 1000; vuelta_loop();
    caso("escalonado: girar antes de que entre el segundo motor cancela el escalon (el pivote queda a 255 solo con el derecho)", pines_son(0, 0, 255, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = e; vuelta_loop();
    carro_recibir_comando("LEFT");
    caso("escalonado: los pivotes no se escalonan (LEFT usa solo el motor derecho a 255)", pines_son(0, 0, 255, 0));
    carro_recibir_comando("RIGHT");
    caso("escalonado: RIGHT usa solo el izquierdo a 255", pines_son(0, 255, 0, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("MODO_MANUAL");
    carro_recibir_comando("BAL_MANUAL:900");
    carro_recibir_comando("FORWARD");
    reloj_ms = e; vuelta_loop();
    reloj_ms = PERFIL_MANUAL_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("escalonado con balance: MANUAL 900 llega a 230 (255 x 0.900) a la izquierda y 255 a la derecha", pines_son(0, 230, 255, 0));

    /* Si el escalon dura mas que el arranque a plena potencia, el segundo motor entra ya con el ajuste por lado. */
    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = PERFIL_AUTO_ARRANQUE_PLENO_MS; vuelta_loop();
    caso("escalonado: si el programa tarda en volver al loop (pasan el escalon y el arranque a plena potencia de golpe) queda con el ajuste por lado", pines_son(0, 207, 210, 0));

    reiniciar_mundo(0);
    carro_recibir_comando("FORWARD");
    reloj_ms = 100; carro_recibir_comando("FORWARD");   /* heartbeat */
    reloj_ms = 400; vuelta_loop();
    caso("escalonado: el watchdog y el heartbeat funcionan igual (a los 400 ms sigue en marcha con el ajuste por lado)", pines_son(0, 207, 210, 0));
    reloj_ms = 1101; vuelta_loop();
    caso("escalonado: 1001 ms despues del ultimo comando frena", pines_son(0, 0, 0, 0));
}
#endif

/* Todas las pruebas de siempre (con el arranque escalonado apagado). Fuera del main para que, al compilar la prueba escalonada, no sobren funciones sin usar. */
int pruebas_normales(void);
int pruebas_normales(void)
{
    printf("== Parte 1: casos puntuales ==\n");
    prueba_movimientos();
    prueba_stop_y_ebrake();
    prueba_repetidos();
    prueba_watchdog();
    prueba_ping();
    prueba_respuesta_ping();
    prueba_arranque_pleno();
    prueba_perfiles();
    prueba_balance();
    prueba_desconocido();
    prueba_keepalive();
    prueba_desbordamiento();

    printf("== Parte 2: comparación con el código original ==\n");
    prueba_diferencial(0, "reloj desde 0");
    prueba_diferencial(0xFFFFF000u, "reloj cruzando el desbordamiento");
    return 0;
}

/* ── Programa principal ────────────────────────────────────────────────── */

int main(void)
{
#if ARRANQUE_ESCALONADO_MS > 0
    printf("== Arranque escalonado (%u ms) ==\n", (unsigned)ARRANQUE_ESCALONADO_MS);
    prueba_escalonado();
#else
    pruebas_normales();
    caso("con el arranque escalonado apagado (0 ms, el valor por defecto) los dos motores arrancan juntos", ARRANQUE_ESCALONADO_MS == 0u);
#endif

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
