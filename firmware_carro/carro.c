/*
 * carro.c — Lógica del carro en C estándar (sin Arduino, sin String, sin malloc).
 *
 * CÓMO SE CONECTAN LAS FUNCIONES PÚBLICAS
 *   setup()  → carro_iniciar()                (motores a 0, estado "STOP")
 *   mensaje WebSocket → carro_recibir_comando(texto)
 *                         ├─ guarda la hora del comando (salvo PING)
 *                         └─ ejecutar_comando(texto)
 *                              ├─ MODO_MANUAL / MODO_AUTO → cambiar_perfil() (STOP primero si se mueve)
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

#include <stdio.h>
#include <string.h>
#include "carro.h"
#include "config.h"

/* ── Estado del carro ──────────────────────────────────────────────────── */

static int      motores_en_marcha   = 0;   /* 1 si algún pin de motor tiene PWM > 0 */
static char     comando_actual[LARGO_MAX_COMANDO] = "STOP";
static uint32_t ultimo_comando_ms   = 0;   /* hora del último comando real (no PING) */
static uint32_t ultimo_keepalive_ms = 0;   /* hora del último pulso keep-alive */
static int      keepalive_alto      = 0;   /* 1 mientras el pulso está en alto */
static int      pleno_modo          = 0;   /* 1 = avanzando y 2 = retrocediendo con el arranque a plena potencia; 0 = no */
static uint32_t pleno_hasta_ms      = 0;   /* hora en que termina el arranque a plena potencia */
static int      perfil_actual       = PERFIL_AUTO;   /* PERFIL_MANUAL o PERFIL_AUTO; siempre arranca en AUTO */
static int      escalon_modo        = 0;   /* arranque escalonado: 1 = avanzando y 2 = retrocediendo con el 2.º motor todavía sin arrancar; 0 = no */
static uint32_t escalon_hasta_ms    = 0;   /* hora en que arranca el 2.º motor */
static volatile int      balance_mil[2]      = { PERFIL_MANUAL_BALANCE_DEFECTO, PERFIL_AUTO_BALANCE_DEFECTO };   /* factor del lado izquierdo de cada perfil, en milésimas (índice = perfil) */
static char              respuesta_balance[LARGO_MAX_COMANDO];   /* el texto de la respuesta a un mensaje de balance */
static char              cola_comandos[COLA_COMANDOS_MAX][LARGO_MAX_COMANDO];
static volatile int      cola_inicio         = 0;
static volatile int      cola_fin            = 0;
static volatile int      cola_cantidad       = 0;
static volatile int      en_vigilancia       = 0;
static volatile int      procesando_cola     = 0;
static volatile int      hay_cliente_activo  = 0;
static volatile uint32_t cliente_activo_id   = 0;
static carro_bloqueo_fn  fn_bloquear         = hw_bloquear;
static carro_bloqueo_fn  fn_desbloquear      = hw_desbloquear;

void carro_poner_bloqueo(carro_bloqueo_fn bloquear, carro_bloqueo_fn desbloquear)
{
    fn_bloquear = (bloquear != NULL) ? bloquear : hw_bloquear;
    fn_desbloquear = (desbloquear != NULL) ? desbloquear : hw_desbloquear;
}

void carro_bloquear(void)
{
    if (fn_bloquear != NULL) {
        fn_bloquear();
    }
}

void carro_desbloquear(void)
{
    if (fn_desbloquear != NULL) {
        fn_desbloquear();
    }
}

/* Lo propio de cada perfil de velocidad (ver config.h). Índice = PERFIL_MANUAL o PERFIL_AUTO. */
typedef struct {
    int      velocidad_der;      /* potencia del lado derecho al avanzar/retroceder después del arranque: es el TOPE del perfil (el izquierdo nunca pasa de aquí) */
    uint32_t arranque_pleno_ms;  /* cuánto dura el arranque a plena potencia */
    const char *nombre;
} PerfilVelocidad;

static const PerfilVelocidad PERFILES[2] = {
    { PERFIL_MANUAL_VELOCIDAD_DER, PERFIL_MANUAL_ARRANQUE_PLENO_MS, "MANUAL" },
    { PERFIL_AUTO_VELOCIDAD_DER,   PERFIL_AUTO_ARRANQUE_PLENO_MS,   "AUTO" }
};

/* Potencia del lado izquierdo del perfil: el tope por el factor de balance (redondeado). Como el factor es de 1000 o menos, nunca pasa del tope. */
static int velocidad_izquierda(int perfil)
{
    return (PERFILES[perfil].velocidad_der * balance_mil[perfil] + 500) / 1000;
}

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
    if (ARRANQUE_ESCALONADO_MS > 0u) {
        poner_motores(0, VELOCIDAD, 0, 0);       /* escalonado: primero solo el motor izquierdo; el derecho entra después (carro_vigilar_comunicacion) */
        escalon_modo = 1;
        escalon_hasta_ms = hw_millis() + ARRANQUE_ESCALONADO_MS;
    } else {
        poner_motores(0, VELOCIDAD, VELOCIDAD, 0);   /* plena potencia al arrancar; luego el ajuste por lado (carro_vigilar_comunicacion) */
        escalon_modo = 0;
    }
    pleno_modo = 1;
    pleno_hasta_ms = hw_millis() + PERFILES[perfil_actual].arranque_pleno_ms;
}

/* Ambos motores hacia atrás (los pines contrarios a avanzar). */
static void retroceder(void)
{
    if (ARRANQUE_ESCALONADO_MS > 0u) {
        poner_motores(VELOCIDAD, 0, 0, 0);       /* escalonado: primero el motor izquierdo */
        escalon_modo = 2;
        escalon_hasta_ms = hw_millis() + ARRANQUE_ESCALONADO_MS;
    } else {
        poner_motores(VELOCIDAD, 0, 0, VELOCIDAD);
        escalon_modo = 0;
    }
    pleno_modo = 2;
    pleno_hasta_ms = hw_millis() + PERFILES[perfil_actual].arranque_pleno_ms;
}

/* Pivote a la izquierda: solo el motor derecho empuja; el izquierdo queda
 * suelto (PWM 0). */
static void girar_izquierda(void)
{
    poner_motores(0, 0, VELOCIDAD_GIRO, 0);
    pleno_modo = 0;
    escalon_modo = 0;
}

/* Pivote a la derecha: solo el motor izquierdo empuja; el derecho queda
 * suelto (PWM 0). */
static void girar_derecha(void)
{
    poner_motores(0, VELOCIDAD_GIRO, 0, 0);
    pleno_modo = 0;
    escalon_modo = 0;
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

/* Cambia el perfil de velocidad. Si ya es ese, no hace nada (la app lo repite de vez en cuando y no debe frenar al carro). Si el
 * carro se está moviendo, PRIMERO se detiene: nunca se cambia de velocidad en movimiento. */
static void cambiar_perfil(int nuevo)
{
    if (nuevo == perfil_actual) {
        return;
    }
    if (motores_en_marcha) {
        carro_detener();
    }
    perfil_actual = nuevo;
    hw_registrar(nuevo == PERFIL_MANUAL ? "Perfil: MANUAL (velocidad maxima)" : "Perfil: AUTO (velocidad limitada)");
}

/* ── Mensajes de balance ───────────────────────────────────────────────── */

/* Si el texto es "BAL_AUTO:..." o "BAL_MANUAL:..." devuelve 1 y deja en *perfil el perfil y en *valor el número; *valido dice si el número sirve (solo cifras, 3 o 4, entre
 * BALANCE_MIN_MIL y BALANCE_MAX_MIL). Cualquier otro texto devuelve 0. */
static int leer_mensaje_balance(const char *texto, int *perfil, int *valor, int *valido)
{
    const char *numero;
    int cifras = 0, acumulado = 0;

    if (strncmp(texto, "BAL_AUTO:", 9) == 0) {
        *perfil = PERFIL_AUTO;
        numero = texto + 9;
    } else if (strncmp(texto, "BAL_MANUAL:", 11) == 0) {
        *perfil = PERFIL_MANUAL;
        numero = texto + 11;
    } else {
        return 0;
    }
    while (numero[cifras] >= '0' && numero[cifras] <= '9' && cifras < 6) {
        acumulado = acumulado * 10 + (numero[cifras] - '0');
        cifras++;
    }
    *valor = acumulado;
    *valido = (numero[cifras] == '\0') && numero[0] != '0' && (cifras == 3 || cifras == 4) && acumulado >= BALANCE_MIN_MIL && acumulado <= BALANCE_MAX_MIL;
    return 1;
}

int carro_balance(int perfil)
{
    return (perfil == PERFIL_MANUAL || perfil == PERFIL_AUTO) ? balance_mil[perfil] : 0;
}

/* Interpreta el texto y actúa. No toca el reloj del watchdog (eso lo hace
 * carro_recibir_comando). */
static void ejecutar_comando(const char *texto)
{
    int perfil_balance, valor_balance, balance_valido;

    if (strcmp(texto, "__STOP_AUTO") == 0) {
        carro_detener();
        carro_poner_perfil_auto();
        return;
    }
    /* Los cambios de perfil no son un movimiento: no se guardan como comando actual (así un FORWARD que ya estaba activo no se ignora). */
    if (strcmp(texto, "MODO_MANUAL") == 0) {
        cambiar_perfil(PERFIL_MANUAL);
        return;
    }
    if (strcmp(texto, "MODO_AUTO") == 0) {
        cambiar_perfil(PERFIL_AUTO);
        return;
    }
    /* Los mensajes de balance tampoco son un movimiento: se aplican al siguiente arranque y no se guardan como comando actual. */
    if (leer_mensaje_balance(texto, &perfil_balance, &valor_balance, &balance_valido)) {
        if (balance_valido) {
            balance_mil[perfil_balance] = valor_balance;
            hw_balance_guardar(perfil_balance, valor_balance);
            hw_registrar(perfil_balance == PERFIL_AUTO ? "Balance AUTO cambiado" : "Balance MANUAL cambiado");
        }
        return;
    }
    if (strcmp(texto, "BAL?") == 0) {
        return;
    }
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
    pleno_modo = 0;
    perfil_actual = PERFIL_AUTO;   /* al arrancar siempre AUTO: la app pide MODO_MANUAL cuando lo necesita */
    escalon_modo = 0;
    cola_inicio = 0;
    cola_fin = 0;
    cola_cantidad = 0;
    en_vigilancia = 0;
    procesando_cola = 0;
    hay_cliente_activo = 0;
    cliente_activo_id = 0;
    balance_mil[PERFIL_MANUAL] = PERFIL_MANUAL_BALANCE_DEFECTO;   /* el balance guardado en la memoria no volátil, si es válido; si no, el de siempre */
    balance_mil[PERFIL_AUTO] = PERFIL_AUTO_BALANCE_DEFECTO;
    for (int p = 0; p < 2; p++) {
        int guardado = hw_balance_leer(p);

        if (guardado >= BALANCE_MIN_MIL && guardado <= BALANCE_MAX_MIL) {
            balance_mil[p] = guardado;
        }
    }
    guardar_comando_actual("STOP");
    ultimo_comando_ms   = 0;
    ultimo_keepalive_ms = 0;
    keepalive_alto      = 0;
}

static int es_stop_prioritario(const char *texto)
{
    return strcmp(texto, "STOP") == 0 || strcmp(texto, "EBRAKE") == 0 || strcmp(texto, "__STOP_AUTO") == 0;
}

static void descartar_ranura_cola(int desplazamiento)
{
    int i;

    for (i = desplazamiento; i < cola_cantidad - 1; i++) {
        int dst = (cola_inicio + i) % COLA_COMANDOS_MAX;
        int src = (cola_inicio + i + 1) % COLA_COMANDOS_MAX;

        memcpy(cola_comandos[dst], cola_comandos[src], LARGO_MAX_COMANDO);
    }
    cola_fin = (cola_fin + COLA_COMANDOS_MAX - 1) % COLA_COMANDOS_MAX;
    cola_cantidad--;
}

void carro_encolar_comando(const char *texto)
{
    int perfil_balance, valor_balance, balance_valido;

    if (texto == NULL) {
        return;
    }
    if (leer_mensaje_balance(texto, &perfil_balance, &valor_balance, &balance_valido) && balance_valido) {
        balance_mil[perfil_balance] = valor_balance;
    }
    if (cola_cantidad >= COLA_COMANDOS_MAX) {
        int i, victima = -1;

        /* Si la cola está llena, PING no desplaza ningún comando real. */
        if (strcmp(texto, "PING") == 0) {
            return;
        }
        /* Primero se descarta un PING viejo; si no hay, el comando más viejo que NO sea STOP, para no perder nunca un STOP. */
        for (i = 0; i < cola_cantidad; i++) {
            int idx = (cola_inicio + i) % COLA_COMANDOS_MAX;

            if (strcmp(cola_comandos[idx], "PING") == 0) {
                victima = i;
                break;
            }
        }
        if (victima < 0) {
            for (i = 0; i < cola_cantidad; i++) {
                int idx = (cola_inicio + i) % COLA_COMANDOS_MAX;

                if (!es_stop_prioritario(cola_comandos[idx])) {
                    victima = i;
                    break;
                }
            }
        }
        if (victima < 0) {
            victima = 0;
        }
        descartar_ranura_cola(victima);
    }
    strncpy(cola_comandos[cola_fin], texto, LARGO_MAX_COMANDO - 1);
    cola_comandos[cola_fin][LARGO_MAX_COMANDO - 1] = '\0';
    cola_fin = (cola_fin + 1) % COLA_COMANDOS_MAX;
    cola_cantidad++;
}

int carro_extraer_comando_cola(char *destino)
{
    if (cola_cantidad <= 0 || destino == NULL) {
        return 0;
    }
    strncpy(destino, cola_comandos[cola_inicio], LARGO_MAX_COMANDO - 1);
    destino[LARGO_MAX_COMANDO - 1] = '\0';
    cola_inicio = (cola_inicio + 1) % COLA_COMANDOS_MAX;
    cola_cantidad--;
    return 1;
}

int carro_cola_cantidad(void)
{
    return cola_cantidad;
}

/* Único punto de extracción de la cola: loop() vacía la cola bajo el bloqueo
 * antes de llamar a carro_vigilar_comunicacion(). */
void carro_procesar_cola(void)
{
    char cmd[LARGO_MAX_COMANDO];

    if (procesando_cola || en_vigilancia) {
        return;
    }
    procesando_cola = 1;
    for (;;) {
        int hay;

        carro_bloquear();
        hay = carro_extraer_comando_cola(cmd);
        carro_desbloquear();
        if (!hay) {
            break;
        }
        carro_recibir_comando(cmd);
    }
    procesando_cola = 0;
}

void carro_conectar_cliente(uint32_t id)
{
    hay_cliente_activo = 1;
    cliente_activo_id = id;
    cola_inicio = 0;
    cola_fin = 0;
    cola_cantidad = 0;
    carro_encolar_comando("__STOP_AUTO");
}

int carro_desconectar_cliente(uint32_t id)
{
    if (!hay_cliente_activo || id != cliente_activo_id) {
        return 0;
    }
    hay_cliente_activo = 0;
    cliente_activo_id = 0;
    cola_inicio = 0;
    cola_fin = 0;
    cola_cantidad = 0;
    carro_encolar_comando("__STOP_AUTO");
    return 1;
}

uint32_t carro_cliente_activo(void)
{
    return hay_cliente_activo ? cliente_activo_id : 0u;
}

/* Cada mensaje recibido pasa por aquí. PING NO renueva el reloj: si lo
 * hiciera, el carro seguiría moviéndose con el botón suelto, porque la
 * página envía PING cada 800 ms (más rápido que el watchdog de 1000 ms). */
void carro_recibir_comando(const char *texto)
{
    if (texto == NULL) {
        return;
    }
    if (en_vigilancia) {
        carro_bloquear();
        carro_encolar_comando(texto);
        carro_desbloquear();
        return;
    }
    if (strcmp(texto, "PING") != 0 && strcmp(texto, "__STOP_AUTO") != 0) {
        ultimo_comando_ms = hw_millis();
    }
    ejecutar_comando(texto);
}

const char *carro_respuesta(const char *texto)
{
    if (texto == NULL) {
        return NULL;
    }
    if (strcmp(texto, "PING") == 0) {
        return "PONG";
    }
    if (strcmp(texto, "MODO_MANUAL") == 0) {
        return "ACK_MANUAL";
    }
    if (strcmp(texto, "MODO_AUTO") == 0) {
        return "ACK_AUTO";
    }
    {
        int perfil, valor, valido;

        if (leer_mensaje_balance(texto, &perfil, &valor, &valido)) {
            if (!valido) {
                return "NAK_BAL";
            }
            snprintf(respuesta_balance, sizeof respuesta_balance, "ACK_BAL_%s:%d", PERFILES[perfil].nombre, valor);
            return respuesta_balance;
        }
    }
    if (strcmp(texto, "BAL?") == 0) {
        snprintf(respuesta_balance, sizeof respuesta_balance, "BAL:%d:%d", balance_mil[PERFIL_AUTO], balance_mil[PERFIL_MANUAL]);
        return respuesta_balance;
    }
    return NULL;
}

int carro_perfil(void)
{
    return perfil_actual;
}

void carro_poner_perfil_auto(void)
{
    perfil_actual = PERFIL_AUTO;   /* sin detener: quien lo llama ya detiene (conexión, desconexión) */
}

/* Freno inmediato (el antiguo hardStop). Lo usan STOP, EBRAKE, el watchdog
 * y los eventos de conexión/desconexión del WebSocket. */
void carro_detener(void)
{
    if (en_vigilancia) {
        carro_bloquear();
        carro_encolar_comando("STOP");
        carro_desbloquear();
        return;
    }
    poner_motores(0, 0, 0, 0);
    pleno_modo = 0;
    escalon_modo = 0;
    guardar_comando_actual("STOP");
}

/* Watchdog de software. La resta sin signo en uint32_t funciona bien aunque
 * el reloj dé la vuelta (desbordamiento). No extrae nada de la cola: eso lo
 * hace únicamente carro_procesar_cola() en loop() bajo el mutex. */
void carro_vigilar_comunicacion(void)
{
    uint32_t ahora;

    en_vigilancia = 1;
    ahora = hw_millis();

    /* Arranque escalonado: ya pasó el tiempo del primer motor, entra el segundo (a plena potencia si todavía dura el arranque a plena potencia; si no, con el ajuste por lado). */
    if (escalon_modo != 0 && (int32_t)(ahora - escalon_hasta_ms) >= 0) {
        int izq = (pleno_modo != 0) ? VELOCIDAD : velocidad_izquierda(perfil_actual);
        int der = (pleno_modo != 0) ? VELOCIDAD : PERFILES[perfil_actual].velocidad_der;

        if (escalon_modo == 1) {
            poner_motores(0, izq, der, 0);
        } else {
            poner_motores(izq, 0, 0, der);
        }
        escalon_modo = 0;
    }

    /* Terminó el arranque a plena potencia: se aplica el ajuste por lado (el tope del perfil a la derecha y el tope por el balance a la izquierda). */
    if (pleno_modo != 0 && (int32_t)(ahora - pleno_hasta_ms) >= 0) {
        if (pleno_modo == 1) {
            poner_motores(0, velocidad_izquierda(perfil_actual), PERFILES[perfil_actual].velocidad_der, 0);
        } else {
            poner_motores(velocidad_izquierda(perfil_actual), 0, 0, PERFILES[perfil_actual].velocidad_der);
        }
        pleno_modo = 0;
    }

    if (motores_en_marcha && (uint32_t)(ahora - ultimo_comando_ms) > TIEMPO_WATCHDOG_MS) {
        poner_motores(0, 0, 0, 0);
        pleno_modo = 0;
        escalon_modo = 0;
        guardar_comando_actual("STOP");
        perfil_actual = PERFIL_AUTO;   /* tras perder la señal vuelve al perfil seguro; la app lo repone con MODO_MANUAL */
        hw_registrar("Watchdog: Freno activado.");
    }
    en_vigilancia = 0;
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
