/*
 * pruebas_sensores.c — Pruebas en PC (sin placa) del giroscopio, el giro por ángulo y el láser (J11).
 *
 * Compila imu.c, tof.c, giro.c, sensores.c y carro.c JUNTOS con:
 *   - sensores_falsos.c: un MPU6050 y un VL53L0X falsos (bus I2C simulado, con ruido y fallos);
 *   - un modelo sencillo del carro: la velocidad de giro responde a los motores como un sistema de primer orden y, al soltar, el carro sigue
 *     girando por inercia. El giroscopio falso lee exactamente esa velocidad (más sesgo y ruido).
 * El tiempo es simulado: cada paso de avanzar_ms() es 1 ms y corre una vuelta de loop() igual que el .ino.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "carro.h"
#include "config.h"
#include "sensores.h"
#include "imu.h"
#include "tof.h"
#include "giro.h"
#include "hw_sensores.h"
#include "sensores_falsos.h"

/* ── Hardware falso del carro ──────────────────────────────────────────── */

static int pwm[64];
static int escrituras_pwm;

void hw_motor_pwm(int pin, int valor)    { pwm[pin] = valor; escrituras_pwm++; }
uint32_t hw_millis(void)                 { return (uint32_t)(falso_us / 1000u); }
void hw_keepalive(int nivel)             { (void)nivel; }
int  hw_balance_leer(int perfil)         { (void)perfil; return 0; }
void hw_balance_guardar(int perfil, int milesimas) { (void)perfil; (void)milesimas; }
void hw_bloquear(void)                   { }
void hw_desbloquear(void)                { }

static char registro[16][96];
static int  registro_n;
void hw_registrar(const char *texto)
{
    if (registro_n < 16) {
        strncpy(registro[registro_n], texto, 95);
        registro[registro_n][95] = '\0';
        registro_n++;
    }
}

/* ── Utilidades de prueba ──────────────────────────────────────────────── */

static int fallos = 0;
static int casos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) fallos++;
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

static int pines_son(int ia, int ib, int da, int db)
{
    return pwm[PIN_MOTOR_IZQ_A] == ia && pwm[PIN_MOTOR_IZQ_B] == ib && pwm[PIN_MOTOR_DER_A] == da && pwm[PIN_MOTOR_DER_B] == db;
}

static int motores_parados(void)
{
    return pines_son(0, 0, 0, 0);
}

/* Mensajes que salieron hacia la app (se guardan todos para mirarlos). */
#define MAX_MSG 4000
static char mensajes[MAX_MSG][MENSAJE_SALIDA_MAX];
static int  donde_msg[MAX_MSG];
static int  n_msg;

static void juntar_mensajes(void)
{
    char t[MENSAJE_SALIDA_MAX];
    int donde;

    while (sensores_extraer_mensaje(t, sizeof t, &donde)) {
        if (n_msg < MAX_MSG) {
            memcpy(mensajes[n_msg], t, sizeof t);
            donde_msg[n_msg] = donde;
            n_msg++;
        }
    }
}

/* Cuántos mensajes guardados empiezan con el prefijo (desde el índice `desde`). */
static int contar_mensajes(const char *prefijo, int desde)
{
    int i, c = 0;

    for (i = desde; i < n_msg; i++) {
        if (strncmp(mensajes[i], prefijo, strlen(prefijo)) == 0) c++;
    }
    return c;
}

/* El último mensaje guardado con ese prefijo, o NULL. */
static const char *ultimo_mensaje(const char *prefijo)
{
    int i;

    for (i = n_msg - 1; i >= 0; i--) {
        if (strncmp(mensajes[i], prefijo, strlen(prefijo)) == 0) return mensajes[i];
    }
    return NULL;
}

/* ── Modelo del carro y del giroscopio ─────────────────────────────────── */

static double omega_dps;            /* velocidad de giro REAL (positivo = izquierda) */
static double angulo_real_deg;      /* ángulo REAL acumulado */
static double omega_tope = 160.0;   /* velocidad de giro con el motor al 100 % */
static double tau_acelera_ms = 80.0;
static double omega_tope_der = 0.0;     /* velocidad de giro a la DERECHA al 100 %; 0 = la misma que a la izquierda (omega_tope) */
static double tau_inercia_der_ms = 0.0; /* inercia a la derecha; 0 = la misma que a la izquierda */
static double tau_inercia_ms = 50.0;    /* al soltar, la velocidad cae así: inercia = omega x tau (160 x 0.05 = 8 grados) */
static double sesgo_cuentas = 0.0;      /* sesgo verdadero del giroscopio en cuentas */
static int    ruido_cuentas = 3;
static int    seguir_giro_real = 1;     /* 0 = el motor está trabado (el carro no gira) */

static void paso_modelo_1ms(void)
{
    double objetivo = 0.0;
    int impulsa_izq = (pwm[PIN_MOTOR_DER_A] > 0 && pwm[PIN_MOTOR_IZQ_B] == 0 && pwm[PIN_MOTOR_IZQ_A] == 0 && pwm[PIN_MOTOR_DER_B] == 0);
    int impulsa_der = (pwm[PIN_MOTOR_IZQ_B] > 0 && pwm[PIN_MOTOR_DER_A] == 0 && pwm[PIN_MOTOR_IZQ_A] == 0 && pwm[PIN_MOTOR_DER_B] == 0);
    double tau;

    if (impulsa_izq) objetivo = omega_tope * pwm[PIN_MOTOR_DER_A] / 255.0;
    if (impulsa_der) objetivo = -(omega_tope_der > 0.0 ? omega_tope_der : omega_tope) * pwm[PIN_MOTOR_IZQ_B] / 255.0;
    if (!seguir_giro_real) objetivo = 0.0;
    tau = (impulsa_izq || impulsa_der) ? tau_acelera_ms : ((omega_dps < 0.0 && tau_inercia_der_ms > 0.0) ? tau_inercia_der_ms : tau_inercia_ms);
    omega_dps += (objetivo - omega_dps) / tau;
    angulo_real_deg += omega_dps * 0.001;
    {
        double cuentas = omega_dps * 65.5 + sesgo_cuentas;
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

/* Avanza el tiempo simulado `ms` milisegundos, con una vuelta de loop() cada ms. */
static void avanzar_ms(int ms)
{
    int i;

    for (i = 0; i < ms; i++) {
        paso_modelo_1ms();
        falso_us += 1000u;
        una_vuelta_loop();
    }
}

static void orden(const char *texto)
{
    carro_encolar_comando(texto);
}

/* Enciende todo desde cero: sensores falsos, carro, modelo y los 3 s de calibración del sesgo. */
static void arrancar(double sesgo, int ruido)
{
    memset(pwm, 0, sizeof pwm);
    escrituras_pwm = 0;
    registro_n = 0;
    n_msg = 0;
    omega_dps = 0.0;
    angulo_real_deg = 0.0;
    omega_tope = 160.0;
    tau_acelera_ms = 80.0;
    tau_inercia_ms = 50.0;
    omega_tope_der = 0.0;
    tau_inercia_der_ms = 0.0;
    seguir_giro_real = 1;
    sesgo_cuentas = sesgo;
    ruido_cuentas = ruido;
    falso_sensores_reiniciar();
    falso_imu_ruido = ruido;
    carro_poner_bloqueo(hw_bloquear, hw_desbloquear);
    carro_iniciar();
    sensores_iniciar();
    avanzar_ms(3200);                 /* 250 muestras a 100 Hz + margen */
}

/* ── 1. Conversiones ───────────────────────────────────────────────────── */

static void prueba_conversiones(void)
{
    printf("-- conversion de registros y de unidades --\n");
    caso("juntar_bytes: 0x00 0x01 = 1", imu_juntar_bytes(0x00, 0x01) == 1);
    caso("juntar_bytes: 0xFF 0xFF = -1", imu_juntar_bytes(0xFF, 0xFF) == -1);
    caso("juntar_bytes: 0x80 0x00 = -32768", imu_juntar_bytes(0x80, 0x00) == -32768);
    caso("juntar_bytes: 0x7F 0xFF = 32767", imu_juntar_bytes(0x7F, 0xFF) == 32767);
    caso("juntar_bytes: 0x16 0xE7 (5863) positivo", imu_juntar_bytes(0x16, 0xE7) == 5863);
    caso("juntar_bytes: 0xE9 0x19 (-5863) negativo", imu_juntar_bytes(0xE9, 0x19) == -5863);
    caso("65.5 cuentas = 1 grado/s: 131 cuentas/2 en 1/256 = 1000 mdps", imu_cuentas_a_mdps(16768) == 1000);
    caso("5895 cuentas (90 grados/s) = 90000 mdps", imu_cuentas_a_mdps(5895 * 256) == 90000);
    caso("-5895 cuentas = -90000 mdps (simetrico)", imu_cuentas_a_mdps(-5895 * 256) == -90000);
    caso("0 cuentas = 0 mdps", imu_cuentas_a_mdps(0) == 0);
    caso("32767 cuentas (tope del rango) = 500 grados/s +-0.3", imu_cuentas_a_mdps(32767 * 256) >= 500000 && imu_cuentas_a_mdps(32767 * 256) <= 500300);
}

/* ── 2. Inicio del chip y WHO_AM_I ─────────────────────────────────────── */

static void prueba_inicio(void)
{
    printf("-- inicio del MPU6050 --\n");
    falso_sensores_reiniciar();
    caso("WHO_AM_I 0x68 se acepta y queda calibrando", imu_iniciar() == 1 && imu_estado() == IMU_ESTADO_CALIBRANDO && imu_quien_soy() == 0x68);
    falso_sensores_reiniciar();
    falso_imu_quien = 0x70;
    caso("WHO_AM_I 0x70 (clon) se acepta y se registra cual fue", imu_iniciar() == 1 && imu_quien_soy() == 0x70);
    falso_sensores_reiniciar();
    falso_imu_quien = 0x72;
    caso("WHO_AM_I 0x72 (clon) se acepta", imu_iniciar() == 1 && imu_quien_soy() == 0x72);
    falso_sensores_reiniciar();
    falso_imu_quien = 0x34;
    caso("WHO_AM_I desconocido (0x34): MAL por otro chip", imu_iniciar() == 0 && imu_estado() == IMU_ESTADO_MAL && imu_motivo() == IMU_MOTIVO_OTRO_CHIP);
    falso_sensores_reiniciar();
    falso_imu_presente = 0;
    caso("el bus no contesta: MAL por no responde", imu_iniciar() == 0 && imu_estado() == IMU_ESTADO_MAL && imu_motivo() == IMU_MOTIVO_NO_RESPONDE);
    falso_sensores_reiniciar();
    falso_imu_ignora_config = 1;
    caso("un clon que ignora los registros se descubre al leerlos de vuelta: MAL por configuracion", imu_iniciar() == 0 && imu_motivo() == IMU_MOTIVO_CONFIG);
    caso("el evento MAL se avisa una vez", (imu_tomar_eventos() & IMU_EVT_MAL) != 0 && imu_tomar_eventos() == 0);
}

/* ── 3. Sesgo y ángulo integrado ───────────────────────────────────────── */

/* Un giroscopio sin motores: calibra y deja el ángulo en 0. quieto_ms grande. */
static void imu_sola(double sesgo, int ruido)
{
    int i;

    falso_sensores_reiniciar();
    falso_imu_ruido = ruido;
    falso_imu_cuentas = (int)sesgo;
    imu_iniciar();
    for (i = 0; i < 400; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
}

/* Mantiene una velocidad de giro constante (en grados/s) durante ms milisegundos, 100 muestras por segundo. */
static void girar_sintetico(double grados_por_s, double sesgo, int ms)
{
    int i, n = ms / 10;

    falso_imu_cuentas = (int)(grados_por_s * 65.5 + sesgo + (grados_por_s >= 0 ? 0.5 : -0.5));
    for (i = 0; i < n; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 0u);
    }
}

static void prueba_sesgo_y_angulo(void)
{
    int i, dentro = 0;

    printf("-- sesgo, angulo y deriva con datos sinteticos --\n");
    imu_sola(12.0, 3);
    caso("calibrar con sesgo 12 cuentas y ruido +-3: queda LISTA", imu_estado() == IMU_ESTADO_LISTA);
    caso("el sesgo medido es 12/65.5 = 0.18 grados/s (18 +-3 centesimas)", imu_sesgo_x100() >= 15 && imu_sesgo_x100() <= 21);
    caso("recien calibrado el angulo es 0", imu_angulo_x10() == 0);
    girar_sintetico(90.0, 12.0, 1000);
    caso("girar 90 grados/s durante 1 s da 90 +-0.5 grados", imu_angulo_x10() >= 895 && imu_angulo_x10() <= 905);
    imu_cero();
    caso("IMU_CERO deja el angulo en 0", imu_angulo_x10() == 0);
    girar_sintetico(0.0, 12.0, 100);                  /* cambiar de +90 a -90 grados/s pasa por reposo (en 10 ms no existe) */
    imu_cero();
    girar_sintetico(-90.0, 12.0, 1000);
    caso("girar -90 grados/s durante 1 s da -90 +-0.5 (a la derecha es negativo)", imu_angulo_x10() >= -905 && imu_angulo_x10() <= -895);
    girar_sintetico(0.0, 12.0, 100);
    imu_cero();
    girar_sintetico(200.0, 12.0, 900);
    caso("girar 200 grados/s durante 0.9 s da 180 +-1", imu_angulo_x10() >= 1790 && imu_angulo_x10() <= 1810);
    girar_sintetico(0.0, 12.0, 500);
    caso("la velocidad se reporta en decimas de grado/s: 90 grados/s = 900 +-10", (girar_sintetico(90.0, 12.0, 100), imu_velocidad_x10() >= 890 && imu_velocidad_x10() <= 910));

    /* Ruido: 60 s quieto con ruido +-4 cuentas (muy por encima de lo normal): la zona muerta evita que sume. */
    imu_sola(12.0, 4);
    for (i = 0; i < 6000; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("60 s quieto con ruido +-4 cuentas: el angulo no pasa de 1 grado", imu_angulo_x10() >= -10 && imu_angulo_x10() <= 10);

    /* Sesgo mal estimado pero dentro de la zona muerta: la deriva sigue siendo despreciable. */
    imu_sola(12.0, 2);
    falso_imu_cuentas = 12 + 6;                       /* el sesgo real subio 6 cuentas = 0.09 grados/s */
    for (i = 0; i < 6000; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 0u);              /* sin ZUPT (quieto_ms = 0): el peor caso */
    }
    caso("sesgo 0.09 grados/s mal estimado, 60 s: deriva menor de 1 grado (zona muerta)", imu_angulo_x10() >= -10 && imu_angulo_x10() <= 10);

    /* Sesgo mal estimado FUERA de la zona muerta y sin ZUPT: la deriva existe y se ve. Es el motivo de recalibrar. */
    imu_sola(12.0, 2);
    falso_imu_cuentas = 12 + 30;                      /* 0.46 grados/s */
    for (i = 0; i < 6000; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 0u);
    }
    caso("sesgo 0.46 grados/s mal estimado SIN ZUPT: deriva de ~27 grados en 60 s (por eso existe IMU_CAL)", imu_angulo_x10() > 200 && imu_angulo_x10() < 320);

    /* ZUPT: con el carro quieto, un sesgo que cambió 16 cuentas (0.24 grados/s) se corrige solo. */
    imu_sola(12.0, 2);
    falso_imu_cuentas = 12 + 16;
    for (i = 0; i < 12000; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    {
        int32_t a1 = imu_angulo_x10();

        for (i = 0; i < 6000; i++) {
            falso_us += 10000u;
            imu_actualizar(hw_micros(), 100000u);
        }
        caso("ZUPT: el sesgo cambia 16 cuentas con el carro quieto y se corrige solo (angulo ya no crece en 60 s mas)", imu_angulo_x10() - a1 >= -5 && imu_angulo_x10() - a1 <= 5);
        caso("ZUPT: el angulo acumulado mientras se corregia es menor de 4 grados", a1 < 40 && a1 > -40);
    }
    /* ZUPT no actua en movimiento: con quieto_ms = 0 el sesgo no se toca aunque la lectura sea estable. */
    imu_sola(12.0, 2);
    {
        int32_t s0 = imu_sesgo_x100();

        falso_imu_cuentas = 12 + 16;
        for (i = 0; i < 3000; i++) {
            falso_us += 10000u;
            imu_actualizar(hw_micros(), 100u);       /* quieto menos de 400 ms */
        }
        caso("ZUPT no corrige el sesgo si el carro lleva menos de 400 ms quieto", imu_sesgo_x100() == s0);
    }
    /* ZUPT no se deja engañar por un giro lento real (mayor que el umbral): no lo absorbe. */
    imu_sola(12.0, 2);
    {
        int32_t s0 = imu_sesgo_x100();

        girar_sintetico(5.0, 12.0, 20000);            /* 5 grados/s con el carro "quieto" */
        caso("ZUPT no absorbe un giro lento de 5 grados/s (el sesgo no cambia) y el angulo suma 100", imu_sesgo_x100() == s0 && imu_angulo_x10() > 990 && imu_angulo_x10() < 1010);
    }
    (void)dentro;
}

/* ── 4. Reloj de microsegundos que da la vuelta ────────────────────────── */

static void prueba_reloj(void)
{
    printf("-- desbordamiento del reloj de microsegundos --\n");
    falso_sensores_reiniciar();
    falso_us = 0xFFFFFFFFull - 1200000ull;            /* faltan 1.2 s para que micros() dé la vuelta (cada 71 min) */
    falso_imu_cuentas = 0;
    imu_iniciar();
    {
        int i;

        for (i = 0; i < 400; i++) {
            falso_us += 10000u;
            imu_actualizar(hw_micros(), 100000u);
        }
    }
    girar_sintetico(90.0, 0.0, 2000);
    caso("el reloj da la vuelta a mitad de un giro de 90 grados/s durante 2 s: 180 +-1", imu_angulo_x10() >= 1790 && imu_angulo_x10() <= 1810);
}

/* ── 5. Calibración: movimiento, reintentos, recalibración ─────────────── */

static void prueba_calibracion(void)
{
    int i;

    printf("-- calibracion del sesgo: carro en movimiento, reintentos, IMU_CAL --\n");
    falso_sensores_reiniciar();
    falso_imu_cuentas = 5;
    imu_iniciar();
    for (i = 0; i < 400; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 0u);              /* el carro se mueve: no se puede calibrar */
    }
    caso("con el carro en movimiento no termina de calibrar", imu_estado() == IMU_ESTADO_CALIBRANDO);
    for (i = 0; i < 400; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("al quedar quieto calibra", imu_estado() == IMU_ESTADO_LISTA);

    /* Se mueve durante la calibración: el promedio se rechaza 3 veces y queda MAL. */
    falso_sensores_reiniciar();
    imu_iniciar();
    for (i = 0; i < 1000; i++) {
        falso_imu_cuentas = (i % 40 < 20) ? 400 : -400;      /* como girarlo a mano de un lado a otro */
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("si el carro se mueve mientras se mide el sesgo, se rechaza y tras 3 intentos queda MAL por sesgo", imu_estado() == IMU_ESTADO_MAL && imu_motivo() == IMU_MOTIVO_SESGO);

    /* Un sesgo absurdo (20 grados/s) se rechaza aunque no varíe. */
    falso_sensores_reiniciar();
    falso_imu_cuentas = 1500;
    falso_imu_ruido = 3;
    imu_iniciar();
    for (i = 0; i < 1000; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("un sesgo absurdo (23 grados/s) se rechaza: MAL", imu_estado() == IMU_ESTADO_MAL && imu_motivo() == IMU_MOTIVO_SESGO);

    /* Recalibrar: mismo carro, otro sesgo. */
    imu_sola(10.0, 2);
    falso_imu_cuentas = 30;
    caso("imu_recalibrar() empieza", imu_recalibrar() == 1 && imu_estado() == IMU_ESTADO_CALIBRANDO);
    for (i = 0; i < 400; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("termina bien con el sesgo nuevo (30/65.5 = 0.46 grados/s) y avisa CAL_OK", imu_estado() == IMU_ESTADO_LISTA && imu_sesgo_x100() >= 43 && imu_sesgo_x100() <= 49 && (imu_tomar_eventos() & IMU_EVT_CAL_OK) != 0);

    /* Recalibrar con el carro moviéndose: falla y conserva el sesgo anterior. */
    imu_sola(10.0, 2);
    {
        int32_t antes = imu_sesgo_x100();

        imu_recalibrar();
        for (i = 0; i < 1000; i++) {
            falso_imu_cuentas = (i % 40 < 20) ? 400 : -400;
            falso_us += 10000u;
            imu_actualizar(hw_micros(), 100000u);
        }
        caso("una recalibracion que falla conserva el sesgo anterior, sigue LISTA y avisa CAL_FALLO", imu_estado() == IMU_ESTADO_LISTA && imu_sesgo_x100() == antes && (imu_tomar_eventos() & IMU_EVT_CAL_FALLO) != 0);
    }
}

/* ── 6. Fallos del giroscopio ──────────────────────────────────────────── */

static void prueba_fallos_imu(void)
{
    int i;

    printf("-- fallos del giroscopio --\n");
    imu_sola(8.0, 2);
    falso_imu_fallos = 2;
    for (i = 0; i < 10; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("2 errores seguidos del bus y luego bien: sigue LISTA", imu_estado() == IMU_ESTADO_LISTA);
    falso_imu_fallos = 3;
    for (i = 0; i < 10; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("3 errores seguidos: MAL por errores y avisa el evento", imu_estado() == IMU_ESTADO_MAL && imu_motivo() == IMU_MOTIVO_ERRORES && (imu_tomar_eventos() & IMU_EVT_MAL) != 0);

    imu_sola(8.0, 2);
    falso_imu_loco = 1;
    for (i = 0; i < 10; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("lecturas locas (-32768) seguidas: MAL", imu_estado() == IMU_ESTADO_MAL && imu_motivo() == IMU_MOTIVO_LOCAS);

    imu_sola(8.0, 2);
    falso_imu_ruido = 0;
    falso_imu_congelado = 1;
    for (i = 0; i < 300; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("una lectura idéntica 200 veces seguidas (sensor congelado): MAL", imu_estado() == IMU_ESTADO_MAL && imu_motivo() == IMU_MOTIVO_CONGELADO);

    /* Un MAL se puede intentar de nuevo con IMU_CAL (re-inicia el chip). */
    falso_sensores_reiniciar();
    falso_imu_presente = 0;
    imu_iniciar();
    falso_imu_presente = 1;
    falso_imu_ruido = 2;
    caso("desde MAL, imu_recalibrar() vuelve a iniciar el chip si ya contesta", imu_recalibrar() == 1 && imu_estado() == IMU_ESTADO_CALIBRANDO);
    for (i = 0; i < 400; i++) {
        falso_us += 10000u;
        imu_actualizar(hw_micros(), 100000u);
    }
    caso("... y termina LISTA", imu_estado() == IMU_ESTADO_LISTA);
}

/* ── 7. Controlador del giro por ángulo contra el modelo del carro ─────── */

/* Interpreta GIRO_OK:/GIRO_TIMEOUT:/GIRO_ERROR: ultimo guardado (desde el índice d). Devuelve cuántos campos numéricos leyó. */
static int leer_giro(const char *prefijo, int *objetivo, int *real_x10, int *ms)
{
    const char *m = ultimo_mensaje(prefijo);

    if (m == NULL) return -1;
    m += strlen(prefijo);
    if (*m != ':') return 0;
    m++;
    return sscanf(m, "%d:%d:%d", objetivo, real_x10, ms);
}

/* Pide un giro, espera hasta 4 s y devuelve el error en grados respecto al ángulo REAL del modelo. */
static double girar_y_medir(const char *comando, int *resultado_ok)
{
    char texto[48];
    double antes = angulo_real_deg;
    int objetivo = 0, real_x10 = 0, ms = 0, k = 0;

    n_msg = 0;
    snprintf(texto, sizeof texto, "%s", comando);
    orden(texto);
    avanzar_ms(4500);
    *resultado_ok = (leer_giro("GIRO_OK", &objetivo, &real_x10, &ms) == 3);
    (void)k;
    (void)objetivo;
    return angulo_real_deg - antes;
}

static void prueba_giro_basico(void)
{
    int ok, objetivo = 0, real_x10 = 0, ms = 0;
    double real;
    int i;

    printf("-- giro por angulo: GIRAR contra el modelo del carro --\n");
    arrancar(10.0, 3);
    caso("tras 3 s el giroscopio esta LISTA y se avisa CAPS:IMU=1", imu_estado() == IMU_ESTADO_LISTA && ultimo_mensaje("CAPS:IMU=1") != NULL);
    n_msg = 0;
    orden("GIRAR:90");
    avanzar_ms(20);
    caso("GIRAR:90 enciende SOLO el motor derecho (pivote a la izquierda)", pines_son(0, 0, 255, 0));
    caso("... y la bandera de giro en curso se reporta en SENS", giro_ocupado() && giro_activo());
    avanzar_ms(4000);
    real = angulo_real_deg;
    ok = (leer_giro("GIRO_OK", &objetivo, &real_x10, &ms) == 3);
    caso("termina con GIRO_OK:90:<real x10>:<ms>", ok && objetivo == 90);
    caso("los motores quedan en 0", motores_parados());
    printf("      (primer giro de 90: angulo REAL del modelo %.1f, medido por el giroscopio %.1f, %d ms, anticipacion %d ms)\n", real, real_x10 / 10.0, ms, giro_anticipacion_ms(PERFIL_AUTO, 1));
    caso("el giroscopio acierta el angulo real del modelo a +-2 grados", ok && (real_x10 / 10.0 - real) < 2.0 && (real_x10 / 10.0 - real) > -2.0);

    /* La anticipación aprende: los siguientes giros de 90 quedan cerca. */
    for (i = 0; i < 5; i++) {
        girar_y_medir("GIRAR:90", &ok);
    }
    real = girar_y_medir("GIRAR:90", &ok);
    printf("      (despues de 6 giros: giro real %.1f grados, anticipacion izquierda %d ms)\n", real, giro_anticipacion_ms(PERFIL_AUTO, 1));
    caso("tras unos giros la anticipacion se corrige sola: el giro de 90 queda a +-3 grados", ok && real > 87.0 && real < 93.0);

    /* Derecha. */
    arrancar(10.0, 3);
    n_msg = 0;
    orden("GIRAR:-90");
    avanzar_ms(20);
    caso("GIRAR:-90 enciende SOLO el motor izquierdo (pivote a la derecha)", pines_son(0, 255, 0, 0));
    avanzar_ms(4000);
    ok = (leer_giro("GIRO_OK", &objetivo, &real_x10, &ms) == 3);
    caso("GIRO_OK:-90 con ángulo real negativo", ok && objetivo == -90 && real_x10 < -700);
    for (i = 0; i < 5; i++) {
        girar_y_medir("GIRAR:-90", &ok);
    }
    real = girar_y_medir("GIRAR:-90", &ok);
    caso("a la derecha tambien se corrige sola (±3 grados)", ok && real < -87.0 && real > -93.0);
    printf("      (derecha despues de 6 giros: %.1f grados; anticipacion izq %d ms, der %d ms)\n", real, giro_anticipacion_ms(PERFIL_AUTO, 1), giro_anticipacion_ms(PERFIL_AUTO, -1));
}

static void prueba_giro_angulos(void)
{
    static const int angulos[] = { 5, 10, 45, 135, 180, 200, -5, -45, -180, -200 };
    unsigned i;

    printf("-- giro por angulo: distintos angulos (con la anticipacion ya aprendida) --\n");
    arrancar(10.0, 3);
    for (i = 0; i < 6; i++) {
        int ok;
        girar_y_medir("GIRAR:90", &ok);
        girar_y_medir("GIRAR:-90", &ok);
    }
    for (i = 0; i < sizeof angulos / sizeof angulos[0]; i++) {
        char cmd[24], nombre[96];
        int ok;
        double real, esperado = angulos[i];
        double tol = (angulos[i] > -10 && angulos[i] < 10) ? 4.0 : 6.0;     /* angulos chicos y grandes: la inercia pesa mas */

        snprintf(cmd, sizeof cmd, "GIRAR:%d", angulos[i]);
        real = girar_y_medir(cmd, &ok);
        snprintf(nombre, sizeof nombre, "GIRAR:%d queda en %.1f (error %+.1f, tolerancia %.0f)", angulos[i], real, real - esperado, tol);
        caso(nombre, ok && real - esperado < tol && real - esperado > -tol);
    }
}

static void prueba_giro_errores(void)
{
    int objetivo = 0, real_x10 = 0, ms = 0, c;
    const char *malos[] = { "GIRAR:4", "GIRAR:-4", "GIRAR:0", "GIRAR:201", "GIRAR:-201", "GIRAR:abc", "GIRAR:", "GIRAR:9x", "GIRAR:--5", "GIRAR:90 ", "GIRAR:1000" };
    unsigned i;

    printf("-- giro por angulo: errores, ocupado, timeout, abortos --\n");
    arrancar(10.0, 3);
    for (i = 0; i < sizeof malos / sizeof malos[0]; i++) {
        char nombre[64];

        n_msg = 0;
        orden(malos[i]);
        avanzar_ms(20);
        snprintf(nombre, sizeof nombre, "%s -> GIRO_ERROR:angulo y no mueve los motores", malos[i]);
        caso(nombre, ultimo_mensaje("GIRO_ERROR:angulo") != NULL && motores_parados() && !giro_ocupado());
    }
    n_msg = 0;
    orden("GIRAR:5");
    avanzar_ms(2500);
    caso("el minimo (5 grados) si se hace", ultimo_mensaje("GIRO_OK:5:") != NULL);
    n_msg = 0;
    orden("GIRAR:+30");
    avanzar_ms(2500);
    caso("el signo + se acepta", ultimo_mensaje("GIRO_OK:30:") != NULL);

    /* Ocupado. */
    n_msg = 0;
    orden("GIRAR:90");
    avanzar_ms(100);
    orden("GIRAR:45");
    avanzar_ms(20);
    caso("un GIRAR mientras gira -> GIRO_ERROR:ocupado y el primero sigue", ultimo_mensaje("GIRO_ERROR:ocupado") != NULL && giro_activo());
    avanzar_ms(4000);
    caso("... y el primero termina bien", ultimo_mensaje("GIRO_OK:90:") != NULL);
    n_msg = 0;
    orden("FORWARD");
    avanzar_ms(30);
    orden("GIRAR:45");
    avanzar_ms(20);
    caso("GIRAR con el carro avanzando -> GIRO_ERROR:ocupado y no lo toca", ultimo_mensaje("GIRO_ERROR:ocupado") != NULL && carro_avanzando());
    orden("STOP");
    avanzar_ms(300);

    /* Sin IMU. */
    arrancar(10.0, 3);
    falso_imu_fallos = -1;
    avanzar_ms(100);
    caso("al caerse el giroscopio se avisa CAPS:IMU=0", ultimo_mensaje("CAPS:IMU=0") != NULL);
    n_msg = 0;
    orden("GIRAR:90");
    avanzar_ms(30);
    caso("con el giroscopio caido GIRAR -> GIRO_ERROR:sin_imu y los motores no se mueven", ultimo_mensaje("GIRO_ERROR:sin_imu") != NULL && motores_parados());

    /* Tiempo máximo: el motor está trabado. */
    arrancar(10.0, 3);
    seguir_giro_real = 0;
    n_msg = 0;
    orden("GIRAR:90");
    avanzar_ms(1800);
    caso("motor trabado: a los 1800 ms (menos del maximo de 500+15x90) sigue intentando", giro_activo());
    avanzar_ms(300);
    caso("... y a los ~1850 ms frena", motores_parados());
    avanzar_ms(400);
    c = leer_giro("GIRO_TIMEOUT", &objetivo, &real_x10, &ms);
    caso("informa GIRO_TIMEOUT:90:<real>:<ms> con el angulo real casi 0", c == 3 && objetivo == 90 && real_x10 < 50 && real_x10 > -50 && ms >= 1840 && ms <= 1900);
    seguir_giro_real = 1;

    /* Abortos. */
    {
        const char *cortes[] = { "STOP", "EBRAKE", "FORWARD", "REVERSE", "LEFT", "RIGHT", "MODO_MANUAL", "__STOP_AUTO" };
        unsigned k;

        for (k = 0; k < sizeof cortes / sizeof cortes[0]; k++) {
            char nombre[96];
            int c2;

            arrancar(10.0, 3);
            n_msg = 0;
            orden("GIRAR:180");
            avanzar_ms(300);
            orden(cortes[k]);
            avanzar_ms(30);
            c2 = (strcmp(cortes[k], "STOP") == 0 || strcmp(cortes[k], "EBRAKE") == 0 || strcmp(cortes[k], "MODO_MANUAL") == 0 || strcmp(cortes[k], "__STOP_AUTO") == 0);
            snprintf(nombre, sizeof nombre, "%s a mitad de un giro lo aborta%s", cortes[k], c2 ? " y deja los motores en 0" : " y el nuevo movimiento manda");
            caso(nombre, !giro_activo() && (c2 ? motores_parados() : !motores_parados()));
            avanzar_ms(400);
            if (c2) {
                caso("... informa GIRO_ERROR:abortado con el angulo parcial real", leer_giro("GIRO_ERROR:abortado", &objetivo, &real_x10, &ms) == 3 && objetivo == 180 && real_x10 > 100 && real_x10 < 1700);
            } else {
                caso("... informa GIRO_ERROR:abortado", ultimo_mensaje("GIRO_ERROR:abortado") != NULL);
            }
            orden("STOP");
            avanzar_ms(50);
        }
    }

    /* Desconexión del cliente activo a mitad del giro. */
    arrancar(10.0, 3);
    carro_conectar_cliente(7);
    avanzar_ms(30);
    n_msg = 0;
    orden("GIRAR:180");
    avanzar_ms(300);
    caso("antes de desconectar el giro esta en curso", giro_activo());
    carro_bloquear();
    (void)carro_desconectar_cliente(7);
    carro_desbloquear();
    avanzar_ms(30);
    caso("al desconectarse el cliente activo el giro se aborta y los motores quedan en 0", !giro_activo() && motores_parados());

    /* El sensor se cae a mitad del giro. */
    arrancar(10.0, 3);
    n_msg = 0;
    orden("GIRAR:180");
    avanzar_ms(300);
    falso_imu_fallos = -1;
    avanzar_ms(100);
    caso("si el giroscopio se cae a mitad del giro, los motores se apagan (nunca gira a ciegas)", motores_parados() && !giro_activo());
    avanzar_ms(300);
    caso("... y se informa GIRO_ERROR:sin_imu con el angulo parcial", leer_giro("GIRO_ERROR:sin_imu", &objetivo, &real_x10, &ms) == 3 && real_x10 > 50);
}

static void prueba_giro_sin_watchdog(void)
{
    int ok, objetivo = 0, real_x10 = 0, ms = 0;

    printf("-- el giro no depende del watchdog de 1 s --\n");
    arrancar(10.0, 3);
    omega_tope = 120.0;                               /* un carro mas lento: 180 grados tardan ~1.7 s */
    n_msg = 0;
    orden("GIRAR:180");
    avanzar_ms(1100);
    caso("a los 1100 ms el giro de 180 sigue con los motores encendidos (watchdog de 1 s no actua)", giro_activo() && !motores_parados());
    avanzar_ms(3000);
    ok = (leer_giro("GIRO_OK", &objetivo, &real_x10, &ms) == 3);
    caso("termina con GIRO_OK", ok && ms > 1100);

    /* El watchdog sigue funcionando para los demás movimientos. */
    arrancar(10.0, 3);
    orden("FORWARD");
    avanzar_ms(500);
    caso("FORWARD avanzando", carro_avanzando());
    avanzar_ms(700);
    caso("sin mas ordenes el watchdog frena a los ~1000 ms", motores_parados());
    orden("FORWARD");
    avanzar_ms(100);
    {
        int i;

        for (i = 0; i < 6; i++) {
            orden("CAPS?");
            orden("IMU?");
            avanzar_ms(250);
        }
    }
    caso("las consultas CAPS?/IMU? NO renuevan el watchdog (frena aunque las mande cada 250 ms)", motores_parados());
}

/* Varios giros con un carro que varía (velocidad ±10 %, inercia ±25 %): se informa media y desviación. */
static void prueba_giro_variable(void)
{
    int lado, i, buenos[2] = { 0, 0 };
    double suma[2] = { 0, 0 }, suma2[2] = { 0, 0 };
    unsigned semilla = 777u;

    printf("-- 10 giros de 90 a cada lado con un carro que varia entre giros (informativo) --\n");
    arrancar(10.0, 3);
    for (lado = 0; lado < 2; lado++) {
        for (i = 0; i < 4; i++) {
            int ok;
            girar_y_medir(lado ? "GIRAR:-90" : "GIRAR:90", &ok);        /* calentamiento: la anticipación aprende */
        }
    }
    for (lado = 0; lado < 2; lado++) {
        for (i = 0; i < 10; i++) {
            int ok;
            double real, error;

            semilla = semilla * 1664525u + 1013904223u;
            omega_tope = 160.0 * (0.9 + 0.2 * ((semilla >> 16) % 1000) / 1000.0);
            semilla = semilla * 1664525u + 1013904223u;
            tau_inercia_ms = 50.0 * (0.75 + 0.5 * ((semilla >> 16) % 1000) / 1000.0);
            real = girar_y_medir(lado ? "GIRAR:-90" : "GIRAR:90", &ok);
            error = (lado ? -real : real) - 90.0;
            suma[lado] += error;
            suma2[lado] += error * error;
            if (ok && error > -3.0 && error < 3.0) buenos[lado]++;
        }
        printf("      %s: error medio %+.2f grados, desviacion %.2f, dentro de +-3: %d de 10\n", lado ? "derecha " : "izquierda", suma[lado] / 10.0,
               sqrt(suma2[lado] / 10.0 - (suma[lado] / 10.0) * (suma[lado] / 10.0)), buenos[lado]);
    }
    caso("con un carro que varia (+-10 % de velocidad, +-25 % de inercia) al menos 8 de 10 giros quedan a +-3 grados a la izquierda", buenos[0] >= 8);
    caso("... y a la derecha", buenos[1] >= 8);
}

/* ── J12: la anticipación es independiente por sentido y por perfil, aprende con la mediana y se guarda ───────── */

static int ant_auto_izq(void) { return giro_anticipacion_ms(PERFIL_AUTO, 1); }
static int ant_auto_der(void) { return giro_anticipacion_ms(PERFIL_AUTO, -1); }

/* Un giro de `grados` (con signo) y cuánto quedó de verdad: error en grados respecto al pedido (el carro del modelo). */
static double error_de_giro(int grados)
{
    char cmd[24];
    int ok;
    double real;

    snprintf(cmd, sizeof cmd, "GIRAR:%d", grados);
    real = girar_y_medir(cmd, &ok);
    return (grados > 0 ? real : -real) - (grados > 0 ? grados : -grados);
}

static void prueba_anticipacion_por_sentido(void)
{
    int lado, i, buenos[2] = { 0, 0 };
    double suma[2] = { 0, 0 };
    unsigned semilla = 4242u;
    int guardados_antes;
    char texto[MENSAJE_SALIDA_MAX];

    printf("-- J12: anticipacion independiente por sentido y por perfil, con mediana y memoria --\n");
    arrancar(10.0, 3);
    /* Un carro ASIMETRICO: a la izquierda 160 grados/s con inercia de 50 ms; a la derecha 130 grados/s con inercia de 95 ms (frena peor). */
    omega_tope = 160.0;
    tau_inercia_ms = 50.0;
    omega_tope_der = 130.0;
    tau_inercia_der_ms = 95.0;
    for (i = 0; i < 4; i++) {
        (void)error_de_giro(90);
        (void)error_de_giro(-90);
    }
    printf("      anticipaciones aprendidas tras 4 giros por lado: izquierda %d ms, derecha %d ms\n", ant_auto_izq(), ant_auto_der());
    caso("con un motor asimetrico cada sentido aprende su propia anticipacion (la derecha, que frena peor, la tiene mas larga)", ant_auto_der() > ant_auto_izq() + 15);
    caso("el perfil MANUAL no se tocó: sus anticipaciones siguen en 20 ms", giro_anticipacion_ms(PERFIL_MANUAL, 1) == 20 && giro_anticipacion_ms(PERFIL_MANUAL, -1) == 20);
    for (lado = 0; lado < 2; lado++) {
        for (i = 0; i < 10; i++) {
            double e;

            semilla = semilla * 1664525u + 1013904223u;
            omega_tope = 160.0 * (0.95 + 0.1 * ((semilla >> 16) % 1000) / 1000.0);
            omega_tope_der = 130.0 * (0.95 + 0.1 * ((semilla >> 8) % 1000) / 1000.0);
            tau_inercia_ms = 50.0 * (0.85 + 0.3 * ((semilla >> 4) % 1000) / 1000.0);
            tau_inercia_der_ms = 95.0 * (0.85 + 0.3 * ((semilla >> 2) % 1000) / 1000.0);
            e = error_de_giro(lado ? -90 : 90);
            suma[lado] += e;
            if (e > -3.0 && e < 3.0) buenos[lado]++;
        }
        printf("      %s: error medio %+.2f grados, dentro de +-3: %d de 10\n", lado ? "derecha " : "izquierda", suma[lado] / 10.0, buenos[lado]);
    }
    caso("motor asimetrico y ruido: a la izquierda al menos 9 de 10 giros quedan a +-3 grados", buenos[0] >= 9);
    caso("motor asimetrico y ruido: a la derecha al menos 9 de 10 giros quedan a +-3 grados", buenos[1] >= 9);

    /* ANT? muestra las cuatro. */
    n_msg = 0;
    orden("ANT?");
    avanzar_ms(5);
    {
        int a, b, c, d;
        const char *m = ultimo_mensaje("ANT:");

        caso("ANT? -> ANT:<auto izq>,<auto der>,<manual izq>,<manual der> con los valores aprendidos",
             m != NULL && sscanf(m, "ANT:%d,%d,%d,%d", &a, &b, &c, &d) == 4 && a == ant_auto_izq() && b == ant_auto_der() && c == 20 && d == 20);
    }

    /* Memoria: se guardó, no se escribe de más y sobrevive a un reinicio. */
    guardados_antes = falso_ant_escrituras;
    printf("      escrituras a la memoria durante 28 giros: %d (limite: solo cuando cambia 3 ms o mas)\n", guardados_antes);
    caso("las anticipaciones se guardaron en la memoria no volatil (ranuras AUTO izq y der, con menos de 3 ms de diferencia: solo se escribe si cambia 3 ms o mas)",
         falso_ant[3] >= 0 && falso_ant[2] >= 0 && abs(falso_ant[3] - ant_auto_izq()) < GIRO_ANT_GUARDAR_DIF_MS && abs(falso_ant[2] - ant_auto_der()) < GIRO_ANT_GUARDAR_DIF_MS);
    caso("no se escribe en cada giro: menos de 20 escrituras en 28 giros", guardados_antes > 0 && guardados_antes < 20);
    {
        int izq = falso_ant[3], der = falso_ant[2];   /* lo guardado */

        sensores_reiniciar_estado();    /* un reinicio del carro: se pierde la RAM pero no la memoria no volatil */
        carro_iniciar();
        sensores_iniciar();
        caso("tras reiniciar el carro las anticipaciones aprendidas se leen de la memoria", ant_auto_izq() == izq && ant_auto_der() == der);
    }

    /* Mediana: un giro disparatado (inercia enorme, un resbalon) casi no mueve la anticipacion. */
    arrancar(10.0, 3);
    omega_tope = 160.0;
    tau_inercia_ms = 50.0;
    for (i = 0; i < 5; i++) {
        (void)error_de_giro(90);
    }
    {
        int antes = ant_auto_izq();

        tau_inercia_ms = 400.0;                 /* un giro con muchisima inercia (se resbalo) */
        (void)error_de_giro(90);
        tau_inercia_ms = 50.0;
        printf("      mediana: anticipacion %d ms antes de un giro raro y %d ms despues\n", antes, ant_auto_izq());
        caso("la mediana no se deja llevar por UN giro raro (cambia menos de 15 ms)", ant_auto_izq() - antes < 15 && antes - ant_auto_izq() < 15);
    }

    /* Un giro corto no aprende (la velocidad no llega a su valor normal). */
    {
        int antes = ant_auto_izq();

        (void)error_de_giro(15);
        caso("un giro de menos de 30 grados no cambia la anticipacion", ant_auto_izq() == antes);
    }

    /* Perfil MANUAL: aprende en su propia ranura sin tocar la de AUTO. */
    arrancar(10.0, 3);
    omega_tope = 160.0;
    tau_inercia_ms = 50.0;
    for (i = 0; i < 3; i++) {
        (void)error_de_giro(90);
    }
    {
        int auto_izq = ant_auto_izq();

        orden("MODO_MANUAL");
        avanzar_ms(30);
        tau_inercia_ms = 90.0;
        for (i = 0; i < 4; i++) {
            (void)error_de_giro(90);
        }
        tau_inercia_ms = 50.0;
        printf("      MANUAL aprendio %d ms (AUTO sigue en %d ms)\n", giro_anticipacion_ms(PERFIL_MANUAL, 1), ant_auto_izq());
        caso("en perfil MANUAL aprende en su propia ranura y la de AUTO no cambia", giro_anticipacion_ms(PERFIL_MANUAL, 1) > auto_izq + 10 && ant_auto_izq() == auto_izq);
        orden("MODO_AUTO");
        avanzar_ms(30);
    }

    /* Borrar. */
    n_msg = 0;
    orden("ANT_BORRAR");
    avanzar_ms(5);
    caso("ANT_BORRAR -> ACK_ANT_BORRAR", ultimo_mensaje("ACK_ANT_BORRAR") != NULL);
    caso("... las 4 vuelven a 20 ms y la memoria queda vacia", ant_auto_izq() == 20 && ant_auto_der() == 20 && giro_anticipacion_ms(PERFIL_MANUAL, 1) == 20 && giro_anticipacion_ms(PERFIL_MANUAL, -1) == 20 &&
         falso_ant[0] == -1 && falso_ant[1] == -1 && falso_ant[2] == -1 && falso_ant[3] == -1);
    (void)texto;
    caso("sensores_es_comando reconoce ANT_BORRAR y no ANT_BORRARX", sensores_es_comando("ANT_BORRAR") && !sensores_es_comando("ANT_BORRARX"));
}

/* ── 8. Sensor láser: filtro, regla de 2 lecturas, solo AUTO y solo avanzando ── */

static void prueba_laser_lecturas(void)
{
    printf("-- laser: libre, validas y filtro de picos --\n");
    arrancar(10.0, 3);
    falso_tof_mm = 8190;
    avanzar_ms(200);
    caso("8190 = libre: 1200 mm y medida no valida", tof_mm() == TOF_LIBRE_MM && !tof_valida() && tof_vivo());
    falso_tof_mm = 65535;
    avanzar_ms(200);
    caso("65535 = libre", tof_mm() == TOF_LIBRE_MM && !tof_valida());
    falso_tof_mm = 1500;
    avanzar_ms(200);
    caso("1500 mm (mas lejos de 1200) = libre", tof_mm() == TOF_LIBRE_MM && !tof_valida());
    falso_tof_mm = 1200;
    avanzar_ms(200);
    caso("1200 mm exactos es una medida valida", tof_mm() == 1200 && tof_valida());
    falso_tof_mm = 300;
    avanzar_ms(200);
    caso("300 mm: tof_mm = 300 y medida valida", tof_mm() == 300 && tof_valida());
    caso("a unas 30 lecturas por segundo (200 ms dan 5 a 7)", tof_lecturas() > 0);
    {
        uint32_t antes = tof_lecturas();

        avanzar_ms(1000);
        caso("1 s de lecturas: entre 25 y 32", tof_lecturas() - antes >= 25 && tof_lecturas() - antes <= 32);
    }

    /* Un pico suelto no cambia la distancia informada (mediana de 3). */
    falso_tof_mm = 500;
    avanzar_ms(300);
    falso_tof_mm = 90;
    avanzar_ms(34);
    falso_tof_mm = 500;
    avanzar_ms(300);
    caso("un pico suelto de 90 entre lecturas de 500 no mueve la distancia informada", tof_mm() == 500);
    /* Se verifica metiendo lecturas a mano. */
    tof_probar_lectura(500, 1u);
    tof_probar_lectura(500, 2u);
    tof_probar_lectura(90, 3u);
    caso("mediana: 500, 500, 90 -> 500", tof_mm() == 500);
    tof_probar_lectura(90, 4u);
    caso("mediana: 500, 90, 90 -> 90", tof_mm() == 90);
    tof_probar_lectura(500, 5u);
    tof_probar_lectura(500, 6u);

    /* Lecturas basura (menos de 8 mm) se ignoran. */
    tof_probar_lectura(300, 7u);
    tof_probar_lectura(300, 8u);
    tof_probar_lectura(300, 9u);
    tof_probar_lectura(3, 10u);
    caso("una lectura de 3 mm es basura y se ignora (la distancia sigue en 300)", tof_mm() == 300);
}

/* Prepara el carro avanzando en AUTO con el láser libre. */
static void avanzando_en_auto(void)
{
    arrancar(10.0, 3);
    falso_tof_mm = 8190;
    avanzar_ms(100);
    orden("FORWARD");
    avanzar_ms(50);
}

static void prueba_laser_paro(void)
{
    int i, t;

    printf("-- laser: parada de emergencia (solo AUTO y solo avanzando) --\n");
    avanzando_en_auto();
    caso("avanza en AUTO con el laser libre", carro_avanzando());
    falso_tof_mm = 100;
    for (t = 0; t < 200 && !motores_parados(); t += 1) {
        avanzar_ms(1);
        if (t % 40 == 0) orden("FORWARD");        /* la app repite FORWARD para que no salte el watchdog */
    }
    printf("      (frena %d ms despues de aparecer el obstaculo a 100 mm)\n", t);
    caso("2 lecturas seguidas menores de 120 mm frenan en menos de 100 ms", motores_parados() && t < 100);
    caso("avisa TOF_PARO:100", ultimo_mensaje("TOF_PARO:100") != NULL && sensores_paros_tof() == 1);
    caso("el perfil sigue AUTO y el STOP salio por la cola interna", carro_perfil() == PERFIL_AUTO);
    caso("el aviso va a la app y al puerto serie", donde_msg[n_msg - 1] != 0);

    /* Una sola lectura cercana no frena (puede ser un pico). */
    avanzando_en_auto();
    n_msg = 0;
    falso_tof_mm = 100;
    avanzar_ms(34);
    falso_tof_mm = 8190;
    avanzar_ms(200);
    orden("FORWARD");
    avanzar_ms(200);
    caso("una lectura cercana suelta no frena", carro_avanzando() && sensores_paros_tof() == 0);

    /* Cercana, libre, cercana, libre... nunca hay 2 seguidas. */
    for (i = 0; i < 8; i++) {
        falso_tof_mm = (i % 2 == 0) ? 100 : 8190;
        avanzar_ms(35);
        if (i % 4 == 0) orden("FORWARD");
    }
    caso("cercana/libre alternadas (nunca 2 seguidas cercanas) no frenan", carro_avanzando() && sensores_paros_tof() == 0);

    /* 130 mm no es menor de 120. */
    falso_tof_mm = 130;
    avanzar_ms(200);
    orden("FORWARD");
    avanzar_ms(100);
    caso("130 mm no frena (el umbral es 120)", carro_avanzando());
    falso_tof_mm = 119;
    avanzar_ms(120);
    caso("119 mm si frena", motores_parados());

    /* Lecturas basura entre dos cercanas no las separan ni las suman. */
    avanzando_en_auto();
    n_msg = 0;
    tof_probar_lectura(100, hw_millis());
    tof_probar_lectura(3, hw_millis());
    caso("una cercana + basura: todavia no hay racha de 2", tof_cerca_confirmada() == 0);
    tof_probar_lectura(100, hw_millis());
    caso("... la segunda cercana valida completa la racha", tof_cerca_confirmada() == 100);

    /* MANUAL: no frena. */
    arrancar(10.0, 3);
    orden("MODO_MANUAL");
    avanzar_ms(30);
    falso_tof_mm = 100;
    orden("FORWARD");
    for (i = 0; i < 6; i++) {
        avanzar_ms(100);
        orden("FORWARD");
    }
    caso("en perfil MANUAL el laser NO frena (la emergencia es solo de AUTO)", carro_avanzando() && sensores_paros_tof() == 0);

    /* Reversa y parado: no frena. */
    arrancar(10.0, 3);
    falso_tof_mm = 100;
    orden("REVERSE");
    for (i = 0; i < 6; i++) {
        avanzar_ms(100);
        orden("REVERSE");
    }
    caso("en reversa no frena", !motores_parados() && sensores_paros_tof() == 0);
    orden("STOP");
    avanzar_ms(300);
    caso("parado con algo cerca: no manda TOF_PARO", ultimo_mensaje("TOF_PARO") == NULL);

    /* En giro: no frena. */
    arrancar(10.0, 3);
    falso_tof_mm = 100;
    avanzar_ms(100);
    orden("GIRAR:90");
    avanzar_ms(100);
    caso("en un giro por angulo el laser no lo interrumpe", giro_activo() && sensores_paros_tof() == 0);
    avanzar_ms(4000);

    /* TOF_USO:0 apaga la parada de emergencia (interruptor de la app); TOF_USO:1 la vuelve a encender. */
    avanzando_en_auto();
    n_msg = 0;
    orden("TOF_USO:0");
    avanzar_ms(20);
    caso("TOF_USO:0 se confirma con ACK_TOF_USO:0", ultimo_mensaje("ACK_TOF_USO:0") != NULL);
    falso_tof_mm = 100;
    for (i = 0; i < 6; i++) {
        avanzar_ms(100);
        orden("FORWARD");
    }
    caso("con TOF_USO:0 el laser ya no frena aunque haya algo a 10 cm (apagado = como antes)", carro_avanzando() && sensores_paros_tof() == 0);
    caso("... pero sigue midiendo (SENS/TOF? informan 100 mm)", tof_mm() == 100);
    orden("TOF_USO:1");
    avanzar_ms(150);
    caso("TOF_USO:1 la vuelve a encender y frena", motores_parados() && sensores_paros_tof() == 1);

    /* Obstáculo aún delante: FORWARD otra vez frena de inmediato; sin obstáculo sigue. */
    avanzando_en_auto();
    falso_tof_mm = 100;
    avanzar_ms(150);
    caso("primer paro", motores_parados() && sensores_paros_tof() == 1);
    orden("FORWARD");
    avanzar_ms(100);
    caso("con el obstaculo todavia ahi, otro FORWARD vuelve a frenar de inmediato", motores_parados() && sensores_paros_tof() == 2);
    falso_tof_mm = 8190;
    avanzar_ms(120);
    orden("FORWARD");
    for (i = 0; i < 5; i++) {
        avanzar_ms(100);
        orden("FORWARD");
    }
    caso("quitado el obstaculo, FORWARD sigue avanzando", carro_avanzando() && sensores_paros_tof() == 2);
}

static void prueba_laser_fallos(void)
{
    int i;
    uint32_t t0;

    printf("-- laser: sensor caido --\n");
    arrancar(10.0, 3);
    falso_tof_modo = 1;
    avanzar_ms(100);
    caso("3 errores seguidos del bus: el laser se marca caido", !tof_vivo());
    caso("... y se avisa CAPS:...,TOF=0", ultimo_mensaje("CAPS:IMU=1,TOF=0") != NULL);
    falso_tof_mm = 50;
    orden("FORWARD");
    for (i = 0; i < 5; i++) {
        avanzar_ms(100);
        orden("FORWARD");
    }
    caso("con el laser caido el carro avanza como antes (no se queda trabado ni frena a ciegas)", carro_avanzando());

    arrancar(10.0, 3);
    falso_tof_modo = 2;
    t0 = hw_millis();
    while (tof_vivo() && hw_millis() - t0 < 2000u) {
        avanzar_ms(10);
    }
    caso("si el sensor deja de dar medidas, a los 500 ms se marca caido", !tof_vivo() && hw_millis() - t0 >= 450u && hw_millis() - t0 <= 700u);

    arrancar(10.0, 3);
    falso_tof_presente = 0;
    caso("si el laser no inicia, queda caido desde el principio", !tof_iniciar() && !tof_vivo());

    /* Un error suelto no lo tira. */
    arrancar(10.0, 3);
    falso_tof_modo = 1;
    avanzar_ms(8);
    falso_tof_modo = 0;
    avanzar_ms(200);
    caso("1 o 2 errores seguidos y luego bien: el laser sigue vivo", tof_vivo());
}

static void prueba_no_bloquea(void)
{
    int i, peor = 0, ahora;

    printf("-- loop() no se bloquea con los sensores --\n");
    arrancar(10.0, 3);
    for (i = 0; i < 5000; i++) {
        paso_modelo_1ms();
        falso_us += 200u;                              /* una vuelta cada 0.2 ms: mucho mas rapido que el .ino real */
        ahora = falso_i2c_transacciones;
        sensores_actualizar();
        carro_procesar_cola();
        carro_vigilar_comunicacion();
        if (falso_i2c_transacciones - ahora > peor) {
            peor = falso_i2c_transacciones - ahora;
        }
        juntar_mensajes();
    }
    printf("      (el peor caso fue %d transacciones del bus en una sola vuelta)\n", peor);
    caso("una vuelta de loop() hace como mucho 3 transacciones del bus (1 del giroscopio + 2 del laser)", peor <= 3);
    {
        int antes = falso_i2c_transacciones;

        for (i = 0; i < 1000; i++) {
            falso_us += 1000u;
            sensores_actualizar();
        }
        printf("      (1 s: %d transacciones del bus)\n", falso_i2c_transacciones - antes);
        caso("en 1 s el bus hace menos de 260 transacciones (giroscopio 100 + laser ~120 entre preguntas y lecturas)", falso_i2c_transacciones - antes < 260);
    }
}

/* ── 9. Mensajes ───────────────────────────────────────────────────────── */

static void prueba_mensajes(void)
{
    int i, n, donde;
    char t[MENSAJE_SALIDA_MAX];
    int ang, sesgo, est, who, a, b, c;
    const char *m;

    printf("-- mensajes nuevos --\n");
    falso_sensores_reiniciar();
    sensores_reiniciar_estado();
    carro_iniciar();
    registro_n = 0;
    n_msg = 0;
    sensores_iniciar();
    orden("CAPS?");
    una_vuelta_loop();
    m = ultimo_mensaje("CAPS:");
    caso("CAPS? recien encendido (calibrando) -> CAPS:IMU=0,TOF=1", m != NULL && strcmp(m, "CAPS:IMU=0,TOF=1") == 0);
    caso("al iniciar se escribe en el registro serie lo que se encontro (IMU y TOF)", registro_n >= 2 && strstr(registro[0], "WHO_AM_I=0x68") != NULL);
    avanzar_ms(3200);
    orden("CAPS?");
    avanzar_ms(5);
    m = ultimo_mensaje("CAPS:");
    caso("CAPS? con todo listo -> CAPS:IMU=1,TOF=1", m != NULL && strcmp(m, "CAPS:IMU=1,TOF=1") == 0);
    orden("IMU?");
    avanzar_ms(5);
    m = ultimo_mensaje("IMU:");
    caso("IMU? -> IMU:<angulo x10>,<sesgo x100>,2,104 (WHO_AM_I 0x68 = 104)", m != NULL && sscanf(m, "IMU:%d,%d,%d,%d", &ang, &sesgo, &est, &who) == 4 && est == IMU_ESTADO_LISTA && who == 104);
    orden("TOF?");
    avanzar_ms(5);
    m = ultimo_mensaje("TOF:");
    caso("TOF? -> TOF:<mm>,<banderas>", m != NULL && sscanf(m, "TOF:%d,%d", &a, &b) == 2 && a == TOF_LIBRE_MM && b == 1);
    falso_tof_mm = 400;
    avanzar_ms(200);
    orden("TOF?");
    avanzar_ms(5);
    m = ultimo_mensaje("TOF:400");
    caso("TOF? con algo a 400 mm -> TOF:400,3 (vivo y medida valida)", m != NULL && strcmp(m, "TOF:400,3") == 0);
    orden("ANT?");
    avanzar_ms(5);
    m = ultimo_mensaje("ANT:");
    caso("ANT? -> ANT:20,20,20,20 (la anticipacion de config.h en las 4 ranuras)", m != NULL && strcmp(m, "ANT:20,20,20,20") == 0);

    /* IMU_CERO. */
    orden("GIRAR:45");
    avanzar_ms(2500);
    caso("despues de girar 45 el angulo del giroscopio no es 0", imu_angulo_x10() > 300);
    orden("IMU_CERO");
    avanzar_ms(5);
    caso("IMU_CERO -> ACK_IMU_CERO y el angulo vuelve a 0", ultimo_mensaje("ACK_IMU_CERO") != NULL && imu_angulo_x10() == 0);

    /* IMU_CAL. */
    n_msg = 0;
    avanzar_ms(400);
    orden("IMU_CAL");
    avanzar_ms(5);
    caso("IMU_CAL con el carro quieto -> ACK_IMU_CAL", ultimo_mensaje("ACK_IMU_CAL") != NULL && imu_estado() == IMU_ESTADO_CALIBRANDO);
    caso("... mientras calibra, GIRAR -> GIRO_ERROR:sin_imu", (orden("GIRAR:45"), avanzar_ms(5), ultimo_mensaje("GIRO_ERROR:sin_imu") != NULL));
    avanzar_ms(3200);
    m = ultimo_mensaje("IMU_CAL_OK:");
    caso("al terminar -> IMU_CAL_OK:<sesgo x100>", m != NULL && sscanf(m, "IMU_CAL_OK:%d", &a) == 1 && a >= 10 && a <= 20 && imu_estado() == IMU_ESTADO_LISTA);
    n_msg = 0;
    orden("FORWARD");
    avanzar_ms(30);
    orden("IMU_CAL");
    avanzar_ms(5);
    caso("IMU_CAL con el carro en marcha -> NAK_IMU_CAL (solo con el carro detenido)", ultimo_mensaje("NAK_IMU_CAL") != NULL && imu_estado() == IMU_ESTADO_LISTA);
    orden("STOP");
    avanzar_ms(100);
    n_msg = 0;
    orden("IMU_CAL");
    avanzar_ms(5);
    caso("... y recien detenido (menos de 300 ms) tambien NAK_IMU_CAL", ultimo_mensaje("NAK_IMU_CAL") != NULL);
    avanzar_ms(400);

    /* SENS: solo con cliente y perfil AUTO. */
    n_msg = 0;
    avanzar_ms(1000);
    caso("sin cliente conectado no se manda SENS", contar_mensajes("SENS:", 0) == 0);
    carro_conectar_cliente(1);
    avanzar_ms(20);
    n_msg = 0;
    avanzar_ms(1000);
    n = contar_mensajes("SENS:", 0);
    caso("con cliente y perfil AUTO se manda SENS cada 100 ms (9 a 11 en 1 s)", n >= 9 && n <= 11);
    m = ultimo_mensaje("SENS:");
    caso("formato SENS:<angulo x10>,<mm>,<banderas>: IMU lista + laser vivo + medida valida = 7", m != NULL && sscanf(m, "SENS:%d,%d,%d", &a, &b, &c) == 3 && c == 7 && b == 400);
    caso("los SENS van solo al WebSocket", donde_msg[n_msg - 1] == SENSORES_DESTINO_WS);
    falso_tof_mm = 8190;
    avanzar_ms(300);
    m = ultimo_mensaje("SENS:");
    caso("sin medida valida (libre) la bandera 4 se apaga: 3", m != NULL && sscanf(m, "SENS:%d,%d,%d", &a, &b, &c) == 3 && c == 3 && b == TOF_LIBRE_MM);
    orden("MODO_MANUAL");
    avanzar_ms(30);
    n_msg = 0;
    avanzar_ms(500);
    caso("en perfil MANUAL no se manda SENS", contar_mensajes("SENS:", 0) == 0);
    orden("MODO_AUTO");
    avanzar_ms(30);
    n_msg = 0;
    orden("GIRAR:90");
    avanzar_ms(150);
    m = ultimo_mensaje("SENS:");
    caso("durante un giro la bandera 16 se enciende (7+16... o 3+16 = 19)", m != NULL && sscanf(m, "SENS:%d,%d,%d", &a, &b, &c) == 3 && (c & 16) != 0);
    avanzar_ms(4000);

    /* SENS por serie. */
    carro_bloquear();
    (void)carro_desconectar_cliente(1);
    carro_desbloquear();
    avanzar_ms(30);
    n_msg = 0;
    sensores_comando_serie("SENS_SERIE:1");
    avanzar_ms(1000);
    n = contar_mensajes("SENS:", 0);
    caso("SENS_SERIE:1 manda SENS por el puerto serie sin cliente (9 a 11 en 1 s)", n >= 9 && n <= 11 && donde_msg[n_msg - 1] == SENSORES_DESTINO_SERIE);
    sensores_comando_serie("SENS_SERIE:0");
    n_msg = 0;
    avanzar_ms(500);
    caso("SENS_SERIE:0 lo apaga", contar_mensajes("SENS:", 0) == 0);

    /* Por serie los motores no se mueven. */
    n_msg = 0;
    sensores_comando_serie("GIRAR:90");
    avanzar_ms(100);
    caso("GIRAR por el puerto serie se rechaza (NAK_SERIE) y los motores no se mueven", ultimo_mensaje("NAK_SERIE") != NULL && motores_parados());
    sensores_comando_serie("IMU?");
    juntar_mensajes();
    caso("las consultas por serie contestan por serie", contar_mensajes("IMU:", 0) == 1 && donde_msg[n_msg - 1] == SENSORES_DESTINO_SERIE);

    /* LOOP. */
    sensores_registrar_loop_us(300u);
    sensores_registrar_loop_us(4800u);
    sensores_registrar_loop_us(5100u);
    orden("LOOP?");
    avanzar_ms(5);
    caso("LOOP? -> LOOP:5100,1 (el peor loop y cuantos pasaron de 5 ms)", ultimo_mensaje("LOOP:5100,1") != NULL);
    orden("LOOP_CERO");
    orden("LOOP?");
    avanzar_ms(5);
    caso("LOOP_CERO reinicia la medida", ultimo_mensaje("LOOP:0,0") != NULL);

    /* Qué es un mensaje de sensores. */
    caso("sensores_es_comando: los mensajes validos", sensores_es_comando("CAPS?") && sensores_es_comando("IMU?") && sensores_es_comando("TOF?") && sensores_es_comando("GIRAR:90") &&
         sensores_es_comando("IMU_CAL") && sensores_es_comando("IMU_CERO") && sensores_es_comando("LOOP?") && sensores_es_comando("ANT?") && sensores_es_comando("SENS_SERIE:1") && sensores_es_comando("TOF_USO:0"));
    caso("sensores_es_comando: IMU_CALX, CAPS, IMU, GIRAR (sin :), FORWARD y vacio no lo son", !sensores_es_comando("IMU_CALX") && !sensores_es_comando("CAPS") && !sensores_es_comando("IMU") &&
         !sensores_es_comando("GIRAR") && !sensores_es_comando("FORWARD") && !sensores_es_comando("") && !sensores_es_comando("TOF"));

    /* Cola de salida llena: sin drenar, nunca pasa de COLA_SALIDA_MAX y no se rompe. */
    for (i = 0; i < 100; i++) {
        snprintf(t, sizeof t, "PRUEBA:%d", i);
        sensores_enviar(SENSORES_DESTINO_WS, t);
    }
    n = 0;
    while (sensores_extraer_mensaje(t, sizeof t, &donde)) {
        n++;
    }
    caso("la cola de salida guarda como mucho COLA_SALIDA_MAX mensajes y no se rompe", n == COLA_SALIDA_MAX);
    caso("... y los que se guardan son los mas nuevos (ultimo PRUEBA:99)", strcmp(t, "PRUEBA:99") == 0);

    /* Sin giroscopio ni láser: comportamiento de siempre. */
    falso_sensores_reiniciar();
    falso_imu_presente = 0;
    falso_tof_presente = 0;
    sensores_reiniciar_estado();
    carro_iniciar();
    sensores_iniciar();
    n_msg = 0;
    orden("CAPS?");
    avanzar_ms(30);
    caso("sin ninguno de los dos sensores: CAPS:IMU=0,TOF=0", ultimo_mensaje("CAPS:IMU=0,TOF=0") != NULL);
    orden("FORWARD");
    avanzar_ms(100);
    caso("... y FORWARD/STOP funcionan como siempre", carro_avanzando());
    orden("STOP");
    avanzar_ms(30);
    caso("... STOP frena", motores_parados());
    orden("GIRAR:90");
    avanzar_ms(30);
    caso("... y GIRAR responde GIRO_ERROR:sin_imu sin mover nada", ultimo_mensaje("GIRO_ERROR:sin_imu") != NULL && motores_parados());
}

/* ── Programa principal ────────────────────────────────────────────────── */

int main(void)
{
    prueba_conversiones();
    prueba_inicio();
    prueba_sesgo_y_angulo();
    prueba_reloj();
    prueba_calibracion();
    prueba_fallos_imu();
    prueba_giro_basico();
    prueba_giro_angulos();
    prueba_giro_errores();
    prueba_giro_sin_watchdog();
    prueba_giro_variable();
    prueba_anticipacion_por_sentido();
    prueba_laser_lecturas();
    prueba_laser_paro();
    prueba_laser_fallos();
    prueba_no_bloquea();
    prueba_mensajes();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
