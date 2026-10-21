/*
 * imu.c — Giroscopio MPU6050 (eje Z) en C puro, sin Arduino y sin números con decimales.
 *
 * CÓMO FUNCIONA
 *   imu_iniciar()      escribe los registros del chip (±500 °/s, filtro de ~44 Hz, 100 muestras por segundo) y los lee de vuelta.
 *   imu_actualizar()   en cada vuelta de loop(): si pasaron 10 ms lee el eje Z (2 bytes) y
 *                        - CALIBRANDO: promedia IMU_SESGO_MUESTRAS lecturas con el carro quieto (el sesgo) y comprueba que casi no varían;
 *                        - LISTA: resta el sesgo, pasa a °/s, aplica la zona muerta y suma la velocidad por el tiempo (regla del trapecio).
 *   Con el carro detenido un rato y lecturas estables, el sesgo se corrige muy despacio (ZUPT).
 *
 * UNIDADES: las lecturas del sensor son "cuentas" (65.5 por °/s). El sesgo se guarda en 1/256 de cuenta, la velocidad en milésimas de °/s
 * y el ángulo en millonésimas de grado (int64_t). Positivo = girar a la izquierda (IMU_SIGNO en config.h).
 */

#include <stdint.h>
#include "imu.h"
#include "hw_sensores.h"
#include "config.h"

/* Registros del MPU6050. */
#define REG_SMPLRT_DIV   0x19
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_GYRO_ZOUT_H  0x47
#define REG_PWR_MGMT_1   0x6B
#define REG_WHO_AM_I     0x75

/* Valores que se escriben: muestreo 1 kHz / (1 + 9) = 100 Hz, filtro digital 3 (~44 Hz) y rango ±500 °/s (FS_SEL = 1). */
#define VAL_SMPLRT_DIV   9
#define VAL_CONFIG       0x03
#define VAL_GYRO_CONFIG  0x08

static int      estado          = IMU_ESTADO_APAGADA;
static int      motivo          = IMU_MOTIVO_NINGUNO;
static int      quien           = 0;
static int      eventos         = 0;
static uint32_t ultima_us       = 0;     /* hora del último intento de lectura */
static uint32_t ultima_buena_us = 0;     /* hora de la última lectura buena (para el tiempo entre muestras) */
static int      hay_ultima      = 0;
static int      fallos          = 0;     /* errores seguidos (bus o lecturas locas) */
static int      motivo_fallo    = IMU_MOTIVO_ERRORES;
static int      iguales         = 0;     /* cuántas lecturas seguidas idénticas */
static int16_t  raw_anterior    = 0;
static int      hay_anterior    = 0;

static int64_t  sesgo256        = 0;     /* sesgo en 1/256 de cuenta */
static int64_t  angulo_udeg     = 0;     /* ángulo en millonésimas de grado */
static int32_t  mdps_anterior   = 0;     /* velocidad anterior (para el trapecio), en milésimas de °/s */
static int32_t  velocidad_x10   = 0;
static uint32_t muestras_buenas = 0;

/* Calibración del sesgo en curso. */
static int      cal_n           = 0;
static int64_t  cal_suma        = 0;
static int64_t  cal_suma2       = 0;
static int      cal_intento     = 0;
static int      cal_por_comando = 0;     /* 1 si la pidió IMU_CAL (si falla se conserva el sesgo anterior) */
static int      cal_previo_lista = 0;    /* 1 si antes de recalibrar el giroscopio estaba LISTA */

/* ZUPT: corrección lenta del sesgo con el carro detenido. */
static int      zupt_estables   = 0;
static int64_t  zupt_acumulado  = 0;

/* ── Conversiones ──────────────────────────────────────────────────────── */

int16_t imu_juntar_bytes(uint8_t alto, uint8_t bajo)
{
    /* Se arma sin signo y se pasa a con signo restando 65536 si el bit más alto está en 1 (así no depende de cómo el compilador convierta). */
    uint16_t sin_signo = (uint16_t)(((uint16_t)alto << 8) | bajo);

    if (sin_signo >= 0x8000u) {
        return (int16_t)((int32_t)sin_signo - 65536);
    }
    return (int16_t)sin_signo;
}

/* cuentas / 65.5 = °/s  →  mdps = cuentas x 1000 / 65.5 = cuentas x 2000 / 131. Con la entrada en 1/256 de cuenta:
 * mdps = c256 x 2000 / (131 x 256) = c256 x 125 / 2096. Se redondea al más cercano. */
int32_t imu_cuentas_a_mdps(int32_t cuentas_por_256)
{
    int64_t numerador = (int64_t)cuentas_por_256 * 125;

    if (numerador >= 0) {
        return (int32_t)((numerador + 1048) / 2096);
    }
    return (int32_t)(-((-numerador + 1048) / 2096));
}

static int64_t c256_a_mdps(int64_t c256)
{
    int64_t numerador = c256 * 125;

    if (numerador >= 0) {
        return (numerador + 1048) / 2096;
    }
    return -((-numerador + 1048) / 2096);
}

/* ── Inicio del chip ───────────────────────────────────────────────────── */

static void caer(int por_que)
{
    if (estado != IMU_ESTADO_MAL) {
        eventos |= IMU_EVT_MAL;
    }
    estado = IMU_ESTADO_MAL;
    motivo = por_que;
    velocidad_x10 = 0;
    mdps_anterior = 0;
}

static void empezar_promedio(void)
{
    cal_n = 0;
    cal_suma = 0;
    cal_suma2 = 0;
}

static int quien_es_valido(int v)
{
    return v == IMU_QUIEN_1 || v == IMU_QUIEN_2 || v == IMU_QUIEN_3;
}

int imu_iniciar(void)
{
    uint8_t dato = 0;
    uint8_t escribir;
    uint8_t v1 = 0, v2 = 0, v3 = 0;

    estado = IMU_ESTADO_APAGADA;
    motivo = IMU_MOTIVO_NINGUNO;
    quien = 0;
    fallos = 0;
    iguales = 0;
    hay_anterior = 0;
    hay_ultima = 0;
    zupt_estables = 0;
    zupt_acumulado = 0;
    mdps_anterior = 0;
    velocidad_x10 = 0;
    angulo_udeg = 0;
    sesgo256 = 0;
    muestras_buenas = 0;

    if (hw_i2c_leer(IMU_DIRECCION, REG_WHO_AM_I, &dato, 1) != 0) {
        caer(IMU_MOTIVO_NO_RESPONDE);
        return 0;
    }
    quien = dato;
    if (!quien_es_valido(quien)) {
        caer(IMU_MOTIVO_OTRO_CHIP);
        return 0;
    }

    /* Reinicio del chip, 100 ms para que termine, y despertar con el reloj del propio giroscopio (más estable que el interno). */
    escribir = 0x80;
    if (hw_i2c_escribir(IMU_DIRECCION, REG_PWR_MGMT_1, &escribir, 1) != 0) {
        caer(IMU_MOTIVO_NO_RESPONDE);
        return 0;
    }
    hw_demora_ms(100);
    escribir = 0x01;
    if (hw_i2c_escribir(IMU_DIRECCION, REG_PWR_MGMT_1, &escribir, 1) != 0) {
        caer(IMU_MOTIVO_NO_RESPONDE);
        return 0;
    }
    hw_demora_ms(20);

    escribir = VAL_SMPLRT_DIV;
    (void)hw_i2c_escribir(IMU_DIRECCION, REG_SMPLRT_DIV, &escribir, 1);
    escribir = VAL_CONFIG;
    (void)hw_i2c_escribir(IMU_DIRECCION, REG_CONFIG, &escribir, 1);
    escribir = VAL_GYRO_CONFIG;
    (void)hw_i2c_escribir(IMU_DIRECCION, REG_GYRO_CONFIG, &escribir, 1);
    hw_demora_ms(10);

    /* Se leen de vuelta: un clon que ignora los registros se descubre aquí y no con un ángulo equivocado. */
    if (hw_i2c_leer(IMU_DIRECCION, REG_SMPLRT_DIV, &v1, 1) != 0 ||
        hw_i2c_leer(IMU_DIRECCION, REG_CONFIG, &v2, 1) != 0 ||
        hw_i2c_leer(IMU_DIRECCION, REG_GYRO_CONFIG, &v3, 1) != 0) {
        caer(IMU_MOTIVO_NO_RESPONDE);
        return 0;
    }
    if (v1 != VAL_SMPLRT_DIV || (v2 & 0x07) != VAL_CONFIG || (v3 & 0x18) != VAL_GYRO_CONFIG) {
        caer(IMU_MOTIVO_CONFIG);
        return 0;
    }

    estado = IMU_ESTADO_CALIBRANDO;
    cal_intento = 0;
    cal_por_comando = 0;
    empezar_promedio();
    return 1;
}

/* ── Calibración del sesgo ─────────────────────────────────────────────── */

/* Termina un promedio de IMU_SESGO_MUESTRAS lecturas: lo acepta o lo rechaza. */
static void cerrar_promedio(void)
{
    int64_t n = cal_n;
    int64_t varianza_n2 = cal_suma2 * n - cal_suma * cal_suma;        /* n² x varianza */
    int64_t media256 = (cal_suma * 256) / n;
    int64_t tope = (int64_t)IMU_SESGO_DESV_MAX_LSB * IMU_SESGO_DESV_MAX_LSB * n * n;
    int bueno = 1;

    if (varianza_n2 > tope) {
        bueno = 0;                                   /* se movió o hay mucho ruido */
    }
    if (media256 > (int64_t)IMU_SESGO_MAX_LSB * 256 || media256 < -(int64_t)IMU_SESGO_MAX_LSB * 256) {
        bueno = 0;                                   /* sesgo absurdo */
    }
    if (bueno) {
        sesgo256 = media256;
        estado = IMU_ESTADO_LISTA;
        motivo = IMU_MOTIVO_NINGUNO;
        mdps_anterior = 0;
        velocidad_x10 = 0;
        zupt_estables = 0;
        zupt_acumulado = 0;
        if (cal_por_comando) {
            eventos |= IMU_EVT_CAL_OK;
        } else {
            angulo_udeg = 0;
            eventos |= IMU_EVT_LISTA;
        }
        cal_por_comando = 0;
        return;
    }
    cal_intento++;
    if (cal_intento < IMU_SESGO_REINTENTOS) {
        empezar_promedio();                          /* otro intento */
        return;
    }
    /* Se acabaron los intentos. */
    if (cal_por_comando) {
        eventos |= IMU_EVT_CAL_FALLO;
        cal_por_comando = 0;
        if (cal_previo_lista) {
            estado = IMU_ESTADO_LISTA;               /* se conserva el sesgo de antes */
            return;
        }
    }
    caer(IMU_MOTIVO_SESGO);
}

int imu_reintentar_sesgo(void)
{
    if (estado != IMU_ESTADO_MAL || motivo != IMU_MOTIVO_SESGO) return 0;
    estado = IMU_ESTADO_CALIBRANDO;
    motivo = IMU_MOTIVO_NINGUNO;
    cal_intento = 0;
    cal_por_comando = 0;
    hay_ultima = 0;
    hay_anterior = 0;
    iguales = 0;
    fallos = 0;
    empezar_promedio();
    return 1;
}

int imu_recalibrar(void)
{
    if (estado == IMU_ESTADO_APAGADA) {
        return 0;
    }
    cal_previo_lista = (estado == IMU_ESTADO_LISTA);
    if (estado == IMU_ESTADO_MAL) {
        if (!imu_iniciar()) {
            eventos |= IMU_EVT_CAL_FALLO;
            return 0;
        }
    }
    estado = IMU_ESTADO_CALIBRANDO;
    cal_intento = 0;
    cal_por_comando = 1;
    empezar_promedio();
    return 1;
}

/* ── Una muestra ───────────────────────────────────────────────────────── */

/* Lee el eje Z. 0 = bien. */
static int leer_z(int16_t *raw)
{
    uint8_t b[2];

    if (hw_i2c_leer(IMU_DIRECCION, REG_GYRO_ZOUT_H, b, 2) != 0) {
        return 1;
    }
    *raw = imu_juntar_bytes(b[0], b[1]);
    return 0;
}

static void anotar_fallo(int por_que)
{
    fallos++;
    motivo_fallo = por_que;
    if (fallos >= IMU_ERRORES_MAX) {
        caer(por_que);
    }
}

void imu_actualizar(uint32_t ahora_us, uint32_t quieto_ms)
{
    int16_t raw = 0;
    uint32_t dt_us;
    int64_t net256;
    int32_t mdps;
    int64_t dif256;

    if (estado != IMU_ESTADO_CALIBRANDO && estado != IMU_ESTADO_LISTA) {
        return;
    }
    if (!hay_ultima) {
        hay_ultima = 1;
        ultima_us = ahora_us;
        ultima_buena_us = ahora_us;
        return;
    }
    if ((uint32_t)(ahora_us - ultima_us) < IMU_PERIODO_US) {
        return;
    }
    ultima_us = ahora_us;

    if (leer_z(&raw) != 0) {
        anotar_fallo(IMU_MOTIVO_ERRORES);
        return;
    }
    if (raw == -32768) {                      /* el valor que da un bus que falla "sin avisar" */
        anotar_fallo(IMU_MOTIVO_LOCAS);
        return;
    }
    fallos = 0;
    muestras_buenas++;

    /* Un giroscopio vivo siempre tiene algo de ruido: la misma cuenta muchas veces seguidas es un sensor congelado. */
    if (hay_anterior && raw == raw_anterior) {
        iguales++;
        if (iguales >= IMU_CONGELADO_MUESTRAS) {
            caer(IMU_MOTIVO_CONGELADO);
            return;
        }
    } else {
        iguales = 0;
    }
    hay_anterior = 1;

    dt_us = (uint32_t)(ahora_us - ultima_buena_us);
    ultima_buena_us = ahora_us;
    if (dt_us > IMU_DT_MAX_US) {
        dt_us = IMU_DT_MAX_US;
    }

    if (estado == IMU_ESTADO_CALIBRANDO) {
        raw_anterior = raw;
        if (quieto_ms < IMU_CAL_QUIETO_MS) {
            empezar_promedio();               /* el carro se está moviendo (o acaba de parar): se empieza de nuevo */
            return;
        }
        cal_n++;
        cal_suma += raw;
        cal_suma2 += (int64_t)raw * raw;
        if (cal_n >= IMU_SESGO_MUESTRAS) {
            cerrar_promedio();
        }
        return;
    }

    /* LISTA: velocidad sin sesgo, en milésimas de °/s, con el signo del montaje y la zona muerta. */
    net256 = (int64_t)raw * 256 - sesgo256;
    mdps = (int32_t)c256_a_mdps(net256) * IMU_SIGNO;
    if (mdps > -IMU_ZONA_MUERTA_MDPS && mdps < IMU_ZONA_MUERTA_MDPS) {
        mdps = 0;
    }
    angulo_udeg += ((int64_t)(mdps_anterior + mdps) * (int64_t)dt_us) / 2000;
    velocidad_x10 = (mdps_anterior + mdps) / 200;      /* promedio de 2 muestras, en décimas de °/s */
    mdps_anterior = mdps;

    /* ZUPT: carro quieto un buen rato y lecturas estables → el sesgo se acerca muy despacio a lo que se lee. */
    dif256 = (int64_t)raw * 256 - sesgo256;
    if (quieto_ms >= IMU_ZUPT_QUIETO_MS && dif256 < (int64_t)IMU_ZUPT_UMBRAL_LSB * 256 && dif256 > -(int64_t)IMU_ZUPT_UMBRAL_LSB * 256 &&
        (raw - raw_anterior) <= 8 && (raw - raw_anterior) >= -8) {
        if (zupt_estables < IMU_ZUPT_ESTABLES) {
            zupt_estables++;
        }
    } else {
        zupt_estables = 0;
        zupt_acumulado = 0;
    }
    raw_anterior = raw;
    if (zupt_estables >= IMU_ZUPT_ESTABLES) {
        int64_t paso;

        zupt_acumulado += dif256;
        paso = zupt_acumulado / IMU_ZUPT_DIV;
        zupt_acumulado -= paso * IMU_ZUPT_DIV;
        sesgo256 += paso;
    }
}

/* ── Consultas ─────────────────────────────────────────────────────────── */

int imu_estado(void)      { return estado; }
int imu_motivo(void)      { return motivo; }
int imu_quien_soy(void)   { return quien; }

int32_t imu_sesgo_x100(void)
{
    return (int32_t)(c256_a_mdps(sesgo256) / 10);
}

int32_t imu_angulo_x10(void)
{
    int64_t a = angulo_udeg;

    /* redondeo al más cercano de millonésimas a décimas de grado (÷ 100 000) */
    if (a >= 0) {
        return (int32_t)((a + 50000) / 100000);
    }
    return (int32_t)(-((-a + 50000) / 100000));
}

int32_t imu_velocidad_x10(void)
{
    return velocidad_x10;
}

void imu_cero(void)
{
    angulo_udeg = 0;
}

int imu_tomar_eventos(void)
{
    int e = eventos;

    eventos = 0;
    return e;
}

uint32_t imu_muestras(void)
{
    return muestras_buenas;
}
