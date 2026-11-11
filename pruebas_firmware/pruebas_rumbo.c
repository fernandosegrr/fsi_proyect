/*
 * pruebas_rumbo.c — Pruebas en PC (sin placa) del RUMBO al avanzar (J16, firmware_carro/rumbo.c) y del filtro giro_largo por grado (giro.c).
 *
 * Parte 1: el controlador solo (rumbo.c): signos, proporcional, integral con tope y sin carga de más, objetivo recortado, lo aprendido, el reparto
 *          de potencias y la lectura del número; y propiedades con miles de entradas al azar (semilla fija).
 * Parte 2: integrado con carro.c, imu.c, tof.c, giro.c y sensores.c (sin modificar) y sensores_falsos.c. Un modelo sencillo del carro AVANZANDO:
 *          la velocidad de giro es proporcional a la diferencia de potencia de los lados (con un lado que empuja más) y responde con un retraso de
 *          primer orden; el giroscopio falso lee esa velocidad. También el pivote, para el giro por ángulo. 1 ms por paso, una vuelta de loop() por paso.
 * El modelo del carro es una suposición para probar el código, no una medición.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "carro.h"
#include "config.h"
#include "sensores.h"
#include "imu.h"
#include "giro.h"
#include "rumbo.h"
#include "hw_sensores.h"
#include "sensores_falsos.h"

/* ── Hardware falso del carro ──────────────────────────────────────────── */

static int pwm[64];

void hw_motor_pwm(int pin, int valor)    { pwm[pin] = valor; }
uint32_t hw_millis(void)                 { return (uint32_t)(falso_us / 1000u); }
void hw_keepalive(int nivel)             { (void)nivel; }
int  hw_balance_leer(int perfil)         { (void)perfil; return 0; }
void hw_balance_guardar(int perfil, int milesimas) { (void)perfil; (void)milesimas; }
void hw_bloquear(void)                   { }
void hw_desbloquear(void)                { }
void hw_registrar(const char *texto)     { (void)texto; }

static int fallos = 0;
static int casos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) fallos++;
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

#define MAX_MSG 6000
static char mensajes[MAX_MSG][MENSAJE_SALIDA_MAX];
static int  n_msg;

static void juntar_mensajes(void)
{
    char t[MENSAJE_SALIDA_MAX];
    int donde;

    while (sensores_extraer_mensaje(t, sizeof t, &donde)) {
        if (n_msg < MAX_MSG) {
            memcpy(mensajes[n_msg], t, sizeof t);
            n_msg++;
        }
    }
}

static int contar_mensajes(const char *prefijo, int desde)
{
    int i, c = 0;

    for (i = desde; i < n_msg; i++) {
        if (strncmp(mensajes[i], prefijo, strlen(prefijo)) == 0) c++;
    }
    return c;
}

/* ── Modelo del carro ──────────────────────────────────────────────────── */

static double omega_dps;          /* velocidad de giro REAL (+ = izquierda) */
static double angulo_real_deg;    /* ángulo REAL acumulado (+ = izquierda) */
static double desbalance = 1.0;   /* cuánto más empuja el lado izquierdo con la misma potencia (1.016 = 1.6 %) */
static double grados_s_por_pct = 1.75;   /* °/s por cada 1 % de diferencia de potencia a 210 (supuesto: 19.3 cm/s, ganancia 1.9, ejes de 120 mm) */
static double tau_ms = 80.0;
static int    ruido_cuentas = 2;

static void paso_modelo_1ms(void)
{
    double objetivo = 0.0;
    int izq_adelante = pwm[PIN_MOTOR_IZQ_B], der_adelante = pwm[PIN_MOTOR_DER_A];
    int izq_atras = pwm[PIN_MOTOR_IZQ_A], der_atras = pwm[PIN_MOTOR_DER_B];

    if (izq_adelante > 0 && der_adelante > 0 && izq_atras == 0 && der_atras == 0) {
        objetivo = grados_s_por_pct * 100.0 * (der_adelante - izq_adelante * desbalance) / 210.0;   /* el izquierdo gana: gira a la derecha (negativo) */
    } else if (der_adelante > 0 && izq_adelante == 0 && izq_atras == 0 && der_atras == 0) {
        objetivo = 160.0 * der_adelante / 255.0;    /* pivote a la izquierda */
    } else if (izq_adelante > 0 && der_adelante == 0 && izq_atras == 0 && der_atras == 0) {
        objetivo = -160.0 * izq_adelante / 255.0;   /* pivote a la derecha */
    }
    omega_dps += (objetivo - omega_dps) / tau_ms;
    angulo_real_deg += omega_dps * 0.001;
    {
        double cuentas = omega_dps * 65.5;

        falso_imu_cuentas = (int)(cuentas >= 0 ? cuentas + 0.5 : cuentas - 0.5);
    }
}

static void una_vuelta_loop(void)
{
    sensores_actualizar();
    carro_procesar_cola();
    carro_vigilar_comunicacion();
    carro_actualizar_keepalive();
    juntar_mensajes();
}

static void avanzar_ms(int ms)
{
    int i;

    for (i = 0; i < ms; i++) {
        paso_modelo_1ms();
        falso_us += 1000u;
        una_vuelta_loop();
    }
}

/* Como el .ino: se encola, se contesta y loop() lo ejecuta. Devuelve la respuesta (o NULL). */
static const char *orden(const char *texto)
{
    carro_encolar_comando(texto);
    return carro_respuesta(texto);
}

/* Con heartbeat de FORWARD cada 200 ms (como la app) durante `ms`. */
static void avanzar_con_latido(int ms)
{
    int t;

    for (t = 0; t < ms; t += 200) {
        carro_recibir_comando("FORWARD");
        avanzar_ms((ms - t >= 200) ? 200 : ms - t);
    }
}

static void arrancar(double desb)
{
    memset(pwm, 0, sizeof pwm);
    n_msg = 0;
    omega_dps = 0.0;
    angulo_real_deg = 0.0;
    desbalance = desb;
    falso_sensores_reiniciar();
    falso_imu_ruido = ruido_cuentas;
    carro_poner_bloqueo(hw_bloquear, hw_desbloquear);
    carro_iniciar();
    sensores_iniciar();
    avanzar_ms(3200);                 /* calibración del sesgo con el carro quieto */
}


/* ── Parte 1: el controlador solo ──────────────────────────────────────── */

static uint32_t azar = 1601u;
static int azar_entre(int a, int b)
{
    azar = azar * 1664525u + 1013904223u;
    return a + (int)((azar >> 8) % (uint32_t)(b - a + 1));
}

static void parte_controlador(void)
{
    Rumbo r;
    int izq, der, i, c, ok;
    int32_t v;

    printf("-- Parte 1: controlador (rumbo.c) --\n");
    rumbo_iniciar(&r);
    rumbo_empezar(&r, 100, 0);
    caso("apagado: empezar no pone el lazo en marcha", !r.encendido && !r.en_marcha && rumbo_corregir(&r, 50, 10) == 0);
    r.encendido = 1;
    rumbo_empezar(&r, 100, 1000);
    caso("encendido: empezar toma el angulo actual como objetivo", r.en_marcha && r.objetivo_x10 == 100 && r.inicio_x10 == 100);
    caso("sin error no corrige", rumbo_corregir(&r, 100, 1010) == 0);
    rumbo_iniciar(&r); r.encendido = 1; rumbo_empezar(&r, 0, 0);
    c = rumbo_corregir(&r, -10, 0);   /* 1 grado a la derecha (el angulo baja), dt = 0: solo el proporcional */
    caso("se tuerce a la derecha 1 grado: corrige +RUMBO_KP_MIL_GRADO (girar a la izquierda)", c == RUMBO_KP_MIL_GRADO);
    rumbo_repartir(c, 207, 210, 255, &izq, &der);
    caso("corregir a la izquierda baja el lado izquierdo y sube el derecho", izq < 207 && der > 210);
    rumbo_iniciar(&r); r.encendido = 1; rumbo_empezar(&r, 0, 0);
    c = rumbo_corregir(&r, 10, 0);
    rumbo_repartir(c, 207, 210, 255, &izq, &der);
    caso("se tuerce a la izquierda: corrige al reves (el izquierdo sube, el derecho baja)", c == -RUMBO_KP_MIL_GRADO && izq > 207 && der < 210);

    rumbo_iniciar(&r); r.encendido = 1; rumbo_empezar(&r, 0, 0);
    for (i = 1; i <= 100; i++) {
        c = rumbo_corregir(&r, -10, (uint32_t)(i * 10));   /* error sostenido de 1 grado durante 1 s */
    }
    caso("con error sostenido el integral crece (1 s con 1 grado: mas que el proporcional)", c > RUMBO_KP_MIL_GRADO);
    caso("...con la cuenta exacta: proporcional + integral = 10 + 10000/1500", c == RUMBO_KP_MIL_GRADO + (int)((int64_t)10 * 1000 * RUMBO_KP_MIL_GRADO / (10 * RUMBO_TI_MS)));
    for (i = 101; i <= 5000; i++) {
        c = rumbo_corregir(&r, -100, (uint32_t)(i * 10));   /* 10 grados durante 49 s: satura */
    }
    caso("con error grande y largo la correccion se queda en el tope", c == RUMBO_MAX_MIL);
    caso("...y el integral no pasa de su propio tope", r.integral <= (int32_t)((int64_t)RUMBO_MAX_MIL * 10 * RUMBO_TI_MS / RUMBO_KP_MIL_GRADO));
    c = rumbo_corregir(&r, 50, 50010);   /* el error cambia de signo: el integral no estaba cargado de más */
    caso("al cambiar de signo el error, la correccion baja enseguida (sin carga de mas)", c < RUMBO_MAX_MIL);

    rumbo_iniciar(&r); r.encendido = 1; rumbo_empezar(&r, 0, 0);
    c = rumbo_corregir(&r, -301, 10);
    caso("con mas de RUMBO_ERROR_MAX_X10 de error no corrige y marca fuera", c == 0 && r.fuera == 1);
    c = rumbo_corregir(&r, -5, 20);
    caso("al volver a un error normal corrige otra vez", c > 0 && r.fuera == 0);

    rumbo_iniciar(&r); r.encendido = 1;
    caso("sin avance en curso el objetivo se ignora", rumbo_poner_objetivo(&r, 50) == 0);
    rumbo_empezar(&r, 1000, 0);
    ok = rumbo_poner_objetivo(&r, 1000 + RUMBO_MAX_OBJETIVO_X10 + 77);
    caso("un objetivo muy lejos se recorta a +RUMBO_MAX_OBJETIVO_X10 del inicio", ok && r.objetivo_x10 == 1000 + RUMBO_MAX_OBJETIVO_X10);
    rumbo_poner_objetivo(&r, 1000 - RUMBO_MAX_OBJETIVO_X10 - 1);
    caso("...y a -RUMBO_MAX_OBJETIVO_X10", r.objetivo_x10 == 1000 - RUMBO_MAX_OBJETIVO_X10);
    rumbo_poner_objetivo(&r, 1012);
    caso("un objetivo cercano se aplica tal cual", r.objetivo_x10 == 1012);

    rumbo_iniciar(&r); r.encendido = 1; rumbo_empezar(&r, 0, 0);
    for (i = 1; i <= 300; i++) {
        rumbo_corregir(&r, -10, (uint32_t)(i * 10));
    }
    rumbo_terminar(&r);
    caso("al terminar guarda lo aprendido (el termino integral)", r.aprendido_mil == (int32_t)((int64_t)10 * 3000 * RUMBO_KP_MIL_GRADO / (10 * RUMBO_TI_MS)) && !r.en_marcha);
    rumbo_empezar(&r, 500, 5000);
    c = rumbo_corregir(&r, 500, 5000);
    caso("el siguiente avance empieza con lo aprendido (sin error ya corrige)", c == r.aprendido_mil && c > 0);
    rumbo_corregir(&r, 500, 5010);
    i = (int)r.integral;
    rumbo_corregir(&r, 500, 900000);   /* un hueco enorme: solo se integran RUMBO_DT_MAX_MS */
    caso("un hueco de tiempo enorme no se integra entero", r.integral == i);
    rumbo_terminar(&r);
    rumbo_terminar(&r);
    caso("terminar dos veces no cambia lo aprendido", r.aprendido_mil == c);

    rumbo_repartir(0, 207, 210, 255, &izq, &der);
    caso("sin correccion las potencias son las de siempre", izq == 207 && der == 210);
    rumbo_repartir(100, 255, 255, 255, &izq, &der);
    /* 255 x 0.9 = 230 y 255 x 1.1 = 281 > 255: se escalan los dos por 255/281 → 209 y 255 */
    caso("en el arranque a plena potencia solo baja un lado (ninguno pasa del tope)", der == 255 && izq == 209);
    rumbo_repartir(-RUMBO_MAX_MIL * 3, 207, 210, 255, &izq, &der);
    caso("una correccion fuera de rango se recorta a RUMBO_MAX_MIL", izq == (207 * (1000 + RUMBO_MAX_MIL) + 500) / 1000 && der == (210 * (1000 - RUMBO_MAX_MIL) + 500) / 1000);
    rumbo_repartir(50, -5, 210, 255, &izq, &der);
    caso("una base negativa da 0, nunca negativo", izq == 0 && der > 0);

    caso("leer_numero: -1234", rumbo_leer_numero("-1234", &v) && v == -1234);
    caso("leer_numero: +7 y 0", rumbo_leer_numero("+7", &v) && v == 7 && rumbo_leer_numero("0", &v) && v == 0);
    caso("leer_numero: 999999 vale, 1234567 no (mas de 6 cifras)", rumbo_leer_numero("999999", &v) && v == 999999 && !rumbo_leer_numero("1234567", &v));
    caso("leer_numero: vacio, solo signo, letras, espacio y decimales no valen",
         !rumbo_leer_numero("", &v) && !rumbo_leer_numero("-", &v) && !rumbo_leer_numero("ON", &v) && !rumbo_leer_numero("12 ", &v) && !rumbo_leer_numero("1.5", &v));

    /* Propiedades: 20000 casos al azar (semilla fija). */
    {
        int malos_tope = 0, malos_reparto = 0, malos_monotono = 0, malos_signo = 0;

        for (i = 0; i < 20000; i++) {
            int base_i = azar_entre(0, 255), base_d = azar_entre(0, 255), tope = 255;
            int c1 = azar_entre(-400, 400), c2 = azar_entre(-400, 400);
            int i1, d1, i2, d2;
            int32_t ang = azar_entre(-5000, 5000);

            rumbo_iniciar(&r); r.encendido = 1; rumbo_empezar(&r, azar_entre(-5000, 5000), 0);
            r.integral = azar_entre(-300000, 300000);
            c = rumbo_corregir(&r, ang, (uint32_t)azar_entre(0, 200));
            if (c > RUMBO_MAX_MIL || c < -RUMBO_MAX_MIL) malos_tope++;
            {   /* con el integral en cero y sin tiempo transcurrido solo actúa el proporcional: el signo de la corrección es el del error */
                Rumbo q;
                int32_t e = azar_entre(-RUMBO_ERROR_MAX_X10, RUMBO_ERROR_MAX_X10);
                int cq;

                rumbo_iniciar(&q); q.encendido = 1; rumbo_empezar(&q, 0, 0);
                cq = rumbo_corregir(&q, -e, 0);
                if ((e > 0 && cq <= 0) || (e < 0 && cq >= 0) || (e == 0 && cq != 0)) malos_signo++;
            }
            rumbo_repartir(c1, base_i, base_d, tope, &i1, &d1);
            rumbo_repartir(c2, base_i, base_d, tope, &i2, &d2);
            if (i1 < 0 || d1 < 0 || i1 > tope || d1 > tope) malos_reparto++;
            /* más corrección a la izquierda nunca deja al izquierdo más fuerte respecto del derecho */
            if (c1 < c2 && (int64_t)i2 * d1 > (int64_t)i1 * d2 + (int64_t)d1 + d2 + 1) malos_monotono++;
        }
        caso("propiedad: la correccion nunca pasa de +-RUMBO_MAX_MIL (20000 casos)", malos_tope == 0);
        caso("propiedad: con integral cero el signo sigue al error (20000 casos)", malos_signo == 0);
        caso("propiedad: el reparto nunca da negativos ni pasa del tope (40000 repartos)", malos_reparto == 0);
        caso("propiedad: mas correccion a la izquierda nunca refuerza el lado izquierdo (20000 pares)", malos_monotono == 0);
    }
}

/* ── Parte 2: integrado con carro.c ────────────────────────────────────── */

static int pwm_es(int ia, int ib, int da, int db)
{
    return pwm[PIN_MOTOR_IZQ_A] == ia && pwm[PIN_MOTOR_IZQ_B] == ib && pwm[PIN_MOTOR_DER_A] == da && pwm[PIN_MOTOR_DER_B] == db;
}

static void parte_integrada(void)
{
    const char *resp;
    double sin_rumbo, con_rumbo, inicio;
    int desde, izq_fijo, der_fijo;
    char texto[32];

    printf("-- Parte 2: integrado con carro.c --\n");
    arrancar(1.03);   /* con el balance por defecto (986) queda un lado ~1.6 % mas fuerte, como en las rectas de J14 */
    caso("el giroscopio quedo LISTA tras el arranque", imu_estado() == IMU_ESTADO_LISTA);

    /* Sin RUMBO: exactamente lo de siempre. */
    inicio = angulo_real_deg;
    carro_recibir_comando("FORWARD");
    avanzar_ms(10);
    caso("sin RUMBO: arranque a plena potencia 255/255", pwm_es(0, VELOCIDAD, VELOCIDAD, 0));
    avanzar_con_latido(3000);
    izq_fijo = pwm[PIN_MOTOR_IZQ_B];
    der_fijo = pwm[PIN_MOTOR_DER_A];
    caso("sin RUMBO: despues del arranque, potencias fijas del perfil AUTO (207/210)", izq_fijo == PERFIL_AUTO_VELOCIDAD_IZQ && der_fijo == PERFIL_AUTO_VELOCIDAD_DER);
    sin_rumbo = angulo_real_deg - inicio;
    if (getenv("DEPURAR")) printf("DBG sin_rumbo=%.2f pwm=%d/%d omega=%.3f\n", sin_rumbo, pwm[PIN_MOTOR_IZQ_B], pwm[PIN_MOTOR_DER_A], omega_dps);
    carro_recibir_comando("STOP");
    avanzar_ms(800);
    caso("sin RUMBO y con un lado ~1.6 % mas fuerte (tras el balance), en 3 s el carro se tuerce varios grados a la derecha", sin_rumbo < -2.0);

    /* RUMBO:ON */
    resp = orden("RUMBO:ON");
    avanzar_ms(5);
    caso("RUMBO:ON contesta ACK_RUMBO:1", resp != NULL && strcmp(resp, "ACK_RUMBO:1") == 0);
    resp = carro_respuesta("RUMBO?");
    caso("RUMBO? dice encendido y sin avance en curso", resp != NULL && strncmp(resp, "RUMBO:1,0,", 10) == 0);
    inicio = angulo_real_deg;
    carro_recibir_comando("FORWARD");
    avanzar_ms(5);
    caso("con RUMBO: el arranque sigue siendo a plena potencia en el lado que no se corrige", pwm[PIN_MOTOR_DER_A] == VELOCIDAD || pwm[PIN_MOTOR_IZQ_B] == VELOCIDAD);
    avanzar_con_latido(3000);
    con_rumbo = angulo_real_deg - inicio;
    if (getenv("DEPURAR")) printf("DBG con_rumbo=%.2f pwm=%d/%d\n", con_rumbo, pwm[PIN_MOTOR_IZQ_B], pwm[PIN_MOTOR_DER_A]);
    caso("con RUMBO el mismo carro se tuerce menos de 1 grado en 3 s", con_rumbo > -1.0 && con_rumbo < 1.0);
    caso("...y reparte la potencia: el izquierdo (el fuerte) va por debajo del derecho mas que el balance fijo",
         pwm[PIN_MOTOR_IZQ_B] * 1000 / pwm[PIN_MOTOR_DER_A] < PERFIL_AUTO_VELOCIDAD_IZQ * 1000 / PERFIL_AUTO_VELOCIDAD_DER);
    caso("...sin pasar del tope ni apagar un lado", pwm[PIN_MOTOR_IZQ_B] > 0 && pwm[PIN_MOTOR_DER_A] <= VELOCIDAD && pwm[PIN_MOTOR_IZQ_A] == 0 && pwm[PIN_MOTOR_DER_B] == 0);
    caso("el comando actual sigue siendo FORWARD (el laser puede seguir frenando)", carro_avanzando());

    /* Objetivo nuevo: 5 grados a la izquierda. */
    snprintf(texto, sizeof texto, "RUMBO:%d", (int)imu_angulo_x10() + 50);
    resp = orden(texto);
    inicio = angulo_real_deg;
    avanzar_con_latido(2000);
    caso("RUMBO:<n> no se contesta", resp == NULL);
    caso("RUMBO:<n> lleva el carro al objetivo (gira ~5 grados a la izquierda avanzando)", angulo_real_deg - inicio > 4.0 && angulo_real_deg - inicio < 6.0);
    caso("...y sigue siendo FORWARD", carro_avanzando());

    /* RUMBO no renueva el watchdog. */
    desde = 0;
    {
        int t;

        for (t = 0; t < 1300; t += 100) {
            snprintf(texto, sizeof texto, "RUMBO:%d", (int)imu_angulo_x10());
            orden(texto);
            avanzar_ms(100);
            if (!carro_motores_en_marcha() && desde == 0) desde = t + 100;
        }
    }
    caso("RUMBO:<n> no renueva el watchdog: sin FORWARD el carro frena a ~1 s", desde > 0 && desde <= 1200 && pwm_es(0, 0, 0, 0));
    resp = carro_respuesta("RUMBO?");
    caso("tras el watchdog el lazo ya no esta en marcha pero sigue encendido", resp != NULL && strncmp(resp, "RUMBO:1,0,", 10) == 0);

    /* RUMBO:OFF en marcha: vuelve a las potencias fijas. */
    carro_recibir_comando("FORWARD");
    avanzar_con_latido(1000);
    orden("RUMBO:OFF");
    avanzar_ms(3);
    caso("RUMBO:OFF en marcha vuelve enseguida a las potencias fijas", pwm_es(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));
    avanzar_con_latido(600);
    caso("...y se quedan fijas", pwm_es(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));
    carro_recibir_comando("STOP");
    avanzar_ms(800);

    /* Giroscopio caído durante el avance: potencias fijas. */
    orden("RUMBO:ON");
    carro_recibir_comando("FORWARD");
    avanzar_con_latido(1000);
    falso_imu_presente = 0;
    avanzar_con_latido(600);
    caso("con el giroscopio caido el avance vuelve a las potencias fijas", imu_estado() != IMU_ESTADO_LISTA && pwm_es(0, PERFIL_AUTO_VELOCIDAD_IZQ, PERFIL_AUTO_VELOCIDAD_DER, 0));
    carro_recibir_comando("STOP");
    avanzar_ms(200);

    /* REVERSE, LEFT y RIGHT no cambian con RUMBO encendido. */
    arrancar(1.03);
    orden("RUMBO:ON");
    avanzar_ms(2);
    carro_recibir_comando("REVERSE");
    avanzar_ms(400);
    caso("con RUMBO: REVERSE igual que siempre (fijo, sin corregir)", pwm_es(PERFIL_AUTO_VELOCIDAD_IZQ, 0, 0, PERFIL_AUTO_VELOCIDAD_DER));
    carro_recibir_comando("LEFT");
    avanzar_ms(50);
    caso("con RUMBO: LEFT igual que siempre", pwm_es(0, 0, VELOCIDAD_GIRO, 0));
    carro_recibir_comando("RIGHT");
    avanzar_ms(50);
    caso("con RUMBO: RIGHT igual que siempre", pwm_es(0, VELOCIDAD_GIRO, 0, 0));
    carro_recibir_comando("STOP");
    avanzar_ms(800);

    /* Paro del láser (__STOP_AUTO sin conexión): RUMBO sigue encendido. Reconexión: se apaga. */
    orden("__STOP_AUTO");
    avanzar_ms(5);
    resp = carro_respuesta("RUMBO?");
    caso("un __STOP_AUTO del laser no apaga RUMBO", resp != NULL && strncmp(resp, "RUMBO:1,", 8) == 0);
    carro_conectar_cliente(7);
    avanzar_ms(5);
    resp = carro_respuesta("RUMBO?");
    caso("al conectarse un cliente RUMBO queda apagado", resp != NULL && strncmp(resp, "RUMBO:0,", 8) == 0);
    orden("RUMBO:ON");
    avanzar_ms(5);
    carro_desconectar_cliente(7);
    avanzar_ms(5);
    resp = carro_respuesta("RUMBO?");
    caso("al desconectarse tambien", resp != NULL && strncmp(resp, "RUMBO:0,", 8) == 0);

    /* Lo aprendido pasa al siguiente avance: el segundo avance se tuerce menos al principio. */
    arrancar(1.05);
    orden("RUMBO:ON");
    avanzar_ms(2);
    inicio = angulo_real_deg;
    carro_recibir_comando("FORWARD");
    avanzar_con_latido(600);
    sin_rumbo = angulo_real_deg - inicio;   /* el primero aprende */
    avanzar_con_latido(3000);
    carro_recibir_comando("STOP");
    avanzar_ms(800);
    inicio = angulo_real_deg;
    carro_recibir_comando("FORWARD");
    avanzar_con_latido(600);
    con_rumbo = angulo_real_deg - inicio;
    carro_recibir_comando("STOP");
    avanzar_ms(800);
    caso("lo aprendido en un avance reduce el error del principio del siguiente", (con_rumbo < 0 ? -con_rumbo : con_rumbo) < (sin_rumbo < 0 ? -sin_rumbo : sin_rumbo));

    /* Mensajes mal escritos: no son movimiento y no cambian nada. */
    orden("RUMBO:XYZ");
    orden("RUMBO:");
    avanzar_ms(5);
    caso("RUMBO mal escrito no mueve los motores ni cambia el estado", pwm_es(0, 0, 0, 0) && !carro_avanzando());
}

/* ── Parte 3: filtro giro_largo por grado (giro.c) ─────────────────────── */

static void parte_giro_largo(void)
{
    int i, desde;

    printf("-- Parte 3: giro_largo por grado --\n");
    arrancar(1.0);
    for (i = 0; i < 3; i++) {
        orden("GIRAR:10");          /* tres giros chicos: no aprenden la anticipación pero antes llenaban el historial de duraciones */
        avanzar_ms(1500);
    }
    desde = n_msg;
    for (i = 0; i < 3; i++) {
        orden("GIRAR:90");
        avanzar_ms(2500);
    }
    caso("tras tres giros chicos, los giros de 90 grados NO se marcan como giro_largo", contar_mensajes("ANT_IGNORADO:giro_largo", desde) == 0 && contar_mensajes("GIRO_OK:90", desde) == 3);
    desde = n_msg;
    orden("GIRAR:180");
    avanzar_ms(4000);
    caso("un giro de 180 grados tampoco (la duracion se compara por grado)", contar_mensajes("ANT_IGNORADO:giro_largo", desde) == 0 && contar_mensajes("GIRO_OK:180", desde) == 1);
}

int main(void)
{
    parte_controlador();
    parte_integrada();
    parte_giro_largo();
    printf("%d casos, %d fallos\n", casos, fallos);
    return fallos ? EXIT_FAILURE : EXIT_SUCCESS;
}
