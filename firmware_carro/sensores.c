/*
 * sensores.c — Une el giroscopio, el láser y el giro por ángulo con la lógica del carro (ver sensores.h).
 *
 * CÓMO SE CONECTA
 *   setup()  → sensores_iniciar()
 *   loop()   → sensores_actualizar()           (todo el acceso al bus I2C ocurre aquí y en nada más)
 *            → carro_procesar_cola()            (los comandos de la app; los de sensores llegan a sensores_comando)
 *            → el .ino saca los mensajes con sensores_extraer_mensaje() y los manda al cliente
 *
 * PARADA DE EMERGENCIA DEL LÁSER: si el perfil es AUTO, el carro va AVANZANDO y tof_cerca_confirmada() dice que hubo TOF_PARO_LECTURAS lecturas válidas
 * seguidas más cerca de TOF_PARO_MM, se encola __STOP_AUTO (el mismo mecanismo interno de parada que ya existía) y se avisa TOF_PARO:<mm>.
 * En reversa, parado, en giro o en perfil MANUAL no frena. La cámara sigue siendo quien decide hacia dónde esquivar.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "sensores.h"
#include "imu.h"
#include "tof.h"
#include "giro.h"
#include "carro.h"
#include "hw_sensores.h"
#include "config.h"

typedef struct {
    char    texto[MENSAJE_SALIDA_MAX];
    uint8_t donde;
} Mensaje;

static Mensaje  salida[COLA_SALIDA_MAX];
static int      salida_inicio = 0;
static int      salida_cantidad = 0;

static char     caps_anunciadas[MENSAJE_SALIDA_MAX];
static int      imu_reintentos = 0;
static int      imu_reintento_programado = 0;
static uint32_t imu_proximo_ms = 0;
static int      sens_serie = 0;             /* 1 = mandar SENS también por el puerto serie */
static uint32_t ultimo_sens_ms = 0;
static uint32_t ultimo_sens_serie_ms = 0;
static int      destino_cal = SENSORES_DESTINO_WS;
static int      tof_uso = 1;                /* 1 = la parada de emergencia del láser está encendida (TOF_USO:0 la apaga: el interruptor «Usar sensor de distancia» de la app); por defecto encendida */
static int      paro_enviado = 0;           /* 1 desde que se pidió la parada hasta que el carro deja de avanzar */
static uint32_t paros_tof = 0;
static uint32_t loop_max_us = 0;
static uint32_t loop_excesos = 0;

/* ── Cola de salida ────────────────────────────────────────────────────── */

void sensores_enviar(int destino, const char *texto)
{
    int pos;

    if (texto == NULL) {
        return;
    }
    if (salida_cantidad >= COLA_SALIDA_MAX) {
        /* Llena: se tira el más viejo (los SENS se repiten solos; lo importante llega casi siempre porque el .ino vacía la cola en cada vuelta). */
        salida_inicio = (salida_inicio + 1) % COLA_SALIDA_MAX;
        salida_cantidad--;
    }
    pos = (salida_inicio + salida_cantidad) % COLA_SALIDA_MAX;
    strncpy(salida[pos].texto, texto, MENSAJE_SALIDA_MAX - 1);
    salida[pos].texto[MENSAJE_SALIDA_MAX - 1] = '\0';
    salida[pos].donde = (uint8_t)destino;
    salida_cantidad++;
}

int sensores_extraer_mensaje(char *destino, int largo, int *donde)
{
    if (salida_cantidad <= 0 || destino == NULL || largo <= 0) {
        return 0;
    }
    strncpy(destino, salida[salida_inicio].texto, (size_t)largo - 1);
    destino[largo - 1] = '\0';
    if (donde != NULL) {
        *donde = salida[salida_inicio].donde;
    }
    salida_inicio = (salida_inicio + 1) % COLA_SALIDA_MAX;
    salida_cantidad--;
    return 1;
}

/* ── Estado y arranque ─────────────────────────────────────────────────── */

void sensores_reiniciar_estado(void)
{
    salida_inicio = 0;
    salida_cantidad = 0;
    caps_anunciadas[0] = '\0';
    imu_reintentos = 0;
    imu_reintento_programado = 0;
    sens_serie = 0;
    ultimo_sens_ms = 0;
    ultimo_sens_serie_ms = 0;
    destino_cal = SENSORES_DESTINO_WS;
    paro_enviado = 0;
    tof_uso = 1;
    paros_tof = 0;
    loop_max_us = 0;
    loop_excesos = 0;
    giro_reiniciar();
}

/* Bit 0 = giroscopio listo, bit 1 = láser vivo; de ahí para arriba, los reintentos del láser (para avisar a la app cuando cambian). */
/* Historial de cinco arranques: igual que TOF, pero actualiza el último
 * dígito cuando el sesgo necesita un reintento en tiempo de ejecución. */
static void imu_anotar_arranque(void)
{
    int h = hw_imu_hist_leer();
    int n = h < 0 ? 0 : (h >> 20) & 7;
    int digitos = h < 0 ? 0 : h & 0xFFFFF;
    if (n < 5) n++;
    hw_imu_hist_guardar((n << 20) | ((digitos << 4) & 0xFFFFF));
}

static void texto_caps(char *dest, int largo)
{
    snprintf(dest, (size_t)largo, "CAPS:IMU=%d,TOF=%d,TOFR=%d,IMUR=%d,IMUM=%d,IMUE=%d",
             imu_estado() == IMU_ESTADO_LISTA, tof_vivo(), tof_reintentos(), imu_reintentos, imu_motivo(), imu_estado());
}

void sensores_iniciar(void)
{
    char linea[MENSAJE_SALIDA_MAX + 24];
    int imu_ok, tof_ok;

    sensores_reiniciar_estado();
    giro_cargar_memoria();   /* las anticipaciones aprendidas (4: perfil x sentido) */
    imu_ok = imu_iniciar();
    imu_anotar_arranque();
    snprintf(linea, sizeof linea, "IMU: WHO_AM_I=0x%02X %s (motivo %d)", imu_quien_soy(), imu_ok ? "calibrando el sesgo" : "MAL", imu_motivo());
    hw_registrar(linea);
    tof_ok = tof_iniciar();
    hw_registrar(tof_ok ? "TOF: VL53L0X listo" : "TOF: VL53L0X MAL (no responde)");
}

/* ── Actualización en cada vuelta de loop() ────────────────────────────── */

static void revisar_eventos(void)
{
    int ei = imu_tomar_eventos();
    int et = tof_tomar_eventos();
    char texto[MENSAJE_SALIDA_MAX];

    if (ei & IMU_EVT_LISTA) {
        snprintf(texto, sizeof texto, "IMU: lista, sesgo %d (x0.01 grados/s)", (int)imu_sesgo_x100());
        hw_registrar(texto);
    }
    if (ei & IMU_EVT_MAL) {
        snprintf(texto, sizeof texto, "IMU: MAL (motivo %d)", imu_motivo());
        hw_registrar(texto);
    }
    if (ei & IMU_EVT_CAL_OK) {
        snprintf(texto, sizeof texto, "IMU_CAL_OK:%d", (int)imu_sesgo_x100());
        sensores_enviar(destino_cal, texto);
    }
    if (ei & IMU_EVT_CAL_FALLO) {
        sensores_enviar(destino_cal, "IMU_CAL_FALLO");
    }
    if (et & TOF_EVT_MAL) {
        hw_registrar("TOF: MAL (dejo de responder)");
    }
    if (et & TOF_EVT_RECUPERADO) {
        hw_registrar("TOF: volvio a funcionar tras un reintento");
    }
    texto_caps(texto, sizeof texto);
    if (strcmp(texto, caps_anunciadas) != 0) {
        strcpy(caps_anunciadas, texto);
        sensores_enviar(SENSORES_DESTINO_WS | SENSORES_DESTINO_SERIE, texto);
    }
}

static void revisar_paro_tof(void)
{
    int mm;
    char texto[MENSAJE_SALIDA_MAX];

    if (!carro_avanzando()) {
        paro_enviado = 0;                       /* ya no avanza: la próxima vez que avance se puede volver a frenar */
        return;
    }
    if (paro_enviado || !tof_uso || carro_perfil() != PERFIL_AUTO) {
        return;
    }
    mm = tof_cerca_confirmada();
    if (mm <= 0) {
        return;
    }
    paro_enviado = 1;
    paros_tof++;
    carro_bloquear();
    carro_encolar_comando("__STOP_AUTO");       /* el STOP interno que nunca se pierde en la cola */
    carro_desbloquear();
    snprintf(texto, sizeof texto, "TOF_PARO:%d", mm);
    sensores_enviar(SENSORES_DESTINO_WS | SENSORES_DESTINO_SERIE, texto);
}

static void mandar_sens(uint32_t ahora_ms)
{
    char texto[MENSAJE_SALIDA_MAX];
    int banderas = 0;
    int cliente, perfil_auto;

    if (imu_estado() == IMU_ESTADO_LISTA) banderas |= 1;
    if (tof_vivo())                       banderas |= 2;
    if (tof_vivo() && tof_valida())       banderas |= 4;
    if (imu_estado() == IMU_ESTADO_CALIBRANDO) banderas |= 8;
    if (giro_ocupado())                   banderas |= 16;
    snprintf(texto, sizeof texto, "SENS:%d,%d,%d", imu_estado() == IMU_ESTADO_LISTA ? (int)imu_angulo_x10() : 0, tof_vivo() ? tof_mm() : 0, banderas);

    carro_bloquear();
    cliente = (carro_cliente_activo() != 0u);
    carro_desbloquear();
    perfil_auto = (carro_perfil() == PERFIL_AUTO);

    if (cliente && perfil_auto && (uint32_t)(ahora_ms - ultimo_sens_ms) >= SENS_PERIODO_MS) {
        ultimo_sens_ms = ahora_ms;
        sensores_enviar(SENSORES_DESTINO_WS, texto);
    }
    if (sens_serie && (uint32_t)(ahora_ms - ultimo_sens_serie_ms) >= SENS_PERIODO_MS) {
        ultimo_sens_serie_ms = ahora_ms;
        sensores_enviar(SENSORES_DESTINO_SERIE, texto);
    }
}

void sensores_actualizar(void)
{
    uint32_t ahora_ms = hw_millis();

    if (imu_estado() == IMU_ESTADO_MAL && imu_motivo() == IMU_MOTIVO_SESGO) {
        if (!imu_reintento_programado) {
            imu_proximo_ms = ahora_ms + IMU_REINTENTO_MS;
            imu_reintento_programado = 1;
        }
        if ((int32_t)(ahora_ms - imu_proximo_ms) >= 0 && !carro_motores_en_marcha() &&
            carro_quieto_ms() >= IMU_CAL_QUIETO_MS && !giro_ocupado() && imu_reintentar_sesgo()) {
            int h = hw_imu_hist_leer();
            int digito;
            if (imu_reintentos < 65535) imu_reintentos++;
            digito = imu_reintentos > 15 ? 15 : imu_reintentos;
            if ((h & 15) != digito) hw_imu_hist_guardar((h & ~15) | digito);
            imu_reintento_programado = 0;
            hw_registrar("IMU: reintento automatico del sesgo con motores detenidos");
        }
    } else {
        imu_reintento_programado = 0;
    }
    imu_actualizar(hw_micros(), carro_quieto_ms());
    tof_actualizar(ahora_ms, carro_quieto_ms() >= TOF_REINTENTO_QUIETO_MS && !giro_ocupado());   /* el reintento del láser solo se hace con el carro detenido */
    giro_actualizar(ahora_ms);
    revisar_eventos();
    revisar_paro_tof();
    mandar_sens(ahora_ms);
}

/* ── Tiempo de loop() ──────────────────────────────────────────────────── */

void sensores_registrar_loop_us(uint32_t us)
{
    if (us > loop_max_us) {
        loop_max_us = us;
    }
    if (us > LOOP_MAX_US) {
        loop_excesos++;
    }
}

uint32_t sensores_loop_max_us(void)  { return loop_max_us; }
uint32_t sensores_loop_excesos(void) { return loop_excesos; }
uint32_t sensores_paros_tof(void)    { return paros_tof; }

/* ── Mensajes ──────────────────────────────────────────────────────────── */

static const char *PREFIJOS[] = { "CAPS?", "IMU?", "TOF?", "GIRAR:", "IMU_CAL", "IMU_CERO", "LOOP?", "LOOP_CERO", "ANT?", "ANT_BORRAR", "ARR?", "SENS_SERIE:", "I2C?", "REG:", "TOF_USO:", NULL };

int sensores_es_comando(const char *texto)
{
    int i;

    if (texto == NULL) {
        return 0;
    }
    for (i = 0; PREFIJOS[i] != NULL; i++) {
        size_t n = strlen(PREFIJOS[i]);

        if (strncmp(texto, PREFIJOS[i], n) == 0) {
            /* Los mensajes sin parámetro deben ser EXACTOS (IMU_CALX no es IMU_CAL); los que llevan ':' llevan algo después. */
            if (PREFIJOS[i][n - 1] == ':' || texto[n] == '\0') {
                return 1;
            }
        }
    }
    return 0;
}

static void ejecutar(const char *texto, int donde)
{
    char respuesta[MENSAJE_SALIDA_MAX];

    if (strcmp(texto, "CAPS?") == 0) {
        texto_caps(respuesta, sizeof respuesta);
        sensores_enviar(donde, respuesta);
    } else if (strcmp(texto, "IMU?") == 0) {
        snprintf(respuesta, sizeof respuesta, "IMU:%d,%d,%d,%d,%d", (int)imu_angulo_x10(), (int)imu_sesgo_x100(), imu_estado(), imu_quien_soy(), imu_motivo());
        sensores_enviar(donde, respuesta);
    } else if (strcmp(texto, "TOF?") == 0) {
        snprintf(respuesta, sizeof respuesta, "TOF:%d,%d", tof_vivo() ? tof_mm() : 0, (tof_vivo() ? 1 : 0) | (tof_vivo() && tof_valida() ? 2 : 0));
        sensores_enviar(donde, respuesta);
    } else if (strncmp(texto, "GIRAR:", 6) == 0) {
        if (donde & SENSORES_DESTINO_SERIE) {
            sensores_enviar(donde, "NAK_SERIE");           /* los motores solo se mueven por el WebSocket */
        } else {
            (void)giro_pedir(texto + 6);
        }
    } else if (strcmp(texto, "IMU_CAL") == 0) {
        if (giro_ocupado() || carro_quieto_ms() < IMU_CAL_QUIETO_MS || !imu_recalibrar()) {
            sensores_enviar(donde, "NAK_IMU_CAL");
        } else {
            destino_cal = donde;
            sensores_enviar(donde, "ACK_IMU_CAL");
        }
    } else if (strcmp(texto, "IMU_CERO") == 0) {
        imu_cero();
        sensores_enviar(donde, "ACK_IMU_CERO");
    } else if (strcmp(texto, "LOOP?") == 0) {
        snprintf(respuesta, sizeof respuesta, "LOOP:%u,%u", (unsigned)loop_max_us, (unsigned)loop_excesos);
        sensores_enviar(donde, respuesta);
    } else if (strcmp(texto, "LOOP_CERO") == 0) {
        loop_max_us = 0;
        loop_excesos = 0;
        sensores_enviar(donde, "ACK_LOOP_CERO");
    } else if (strcmp(texto, "ANT?") == 0) {
        /* ANT:<AUTO izquierda>,<AUTO derecha>,<MANUAL izquierda>,<MANUAL derecha> en ms */
        snprintf(respuesta, sizeof respuesta, "ANT:%d,%d,%d,%d", giro_anticipacion_ms(PERFIL_AUTO, 1), giro_anticipacion_ms(PERFIL_AUTO, -1),
                 giro_anticipacion_ms(PERFIL_MANUAL, 1), giro_anticipacion_ms(PERFIL_MANUAL, -1));
        sensores_enviar(donde, respuesta);
    } else if (strcmp(texto, "ARR?") == 0) {
        /* ARR:<arranques anotados>,<con reintentos del láser>,<reintentos de cada uno en hexadecimal, el más nuevo a la derecha>,<reintentos de este arranque> */
        int n, con, emp;

        tof_historial(&n, &con, &emp);
        {
            int h = hw_imu_hist_leer();
            int ni = h < 0 ? 0 : (h >> 20) & 7;
            int di = h < 0 ? 0 : h & 0xFFFFF;
            int ci = 0, i;
            for (i = 0; i < ni; i++) if ((di >> (4 * i)) & 15) ci++;
            snprintf(respuesta, sizeof respuesta, "ARR:%d,%d,%05X,%d,%d,%d,%05X,%d", n, con, emp, tof_reintentos_arranque(), ni, ci, di, imu_reintentos);
        }
        sensores_enviar(donde, respuesta);
    } else if (strcmp(texto, "ANT_BORRAR") == 0) {
        giro_borrar_anticipaciones();
        sensores_enviar(donde, "ACK_ANT_BORRAR");
    } else if (strncmp(texto, "TOF_USO:", 8) == 0) {
        tof_uso = (texto[8] != '0');
        paro_enviado = 0;
        sensores_enviar(donde, tof_uso ? "ACK_TOF_USO:1" : "ACK_TOF_USO:0");
    } else if (strcmp(texto, "I2C?") == 0) {
        /* Diagnóstico de cableado: qué direcciones contestan en el bus (126 preguntas seguidas; solo por serie, nunca en una ruta). */
        int dir, n = 0;

        n += snprintf(respuesta, sizeof respuesta, "I2C:");
        for (dir = 1; dir < 127 && n < (int)sizeof respuesta - 5; dir++) {
            if (hw_i2c_presente((uint8_t)dir)) {
                n += snprintf(respuesta + n, sizeof respuesta - (size_t)n, "%02X ", dir);
            }
        }
        sensores_enviar(donde, respuesta);
    } else if (strncmp(texto, "REG:", 4) == 0) {
        /* REG:<dirección hex>:<registro hex> lee UN registro (solo por serie). Sirve para ver el WHO_AM_I de cada módulo. */
        unsigned dir = 0, reg = 0;
        uint8_t valor = 0;

        if (!(donde & SENSORES_DESTINO_SERIE) || sscanf(texto + 4, "%x:%x", &dir, &reg) != 2 || dir > 127 || reg > 255) {
            sensores_enviar(donde, "NAK_REG");
        } else if (hw_i2c_leer((uint8_t)dir, (uint8_t)reg, &valor, 1) != 0) {
            snprintf(respuesta, sizeof respuesta, "REG:%02X:%02X=ERROR", dir, reg);
            sensores_enviar(donde, respuesta);
        } else {
            snprintf(respuesta, sizeof respuesta, "REG:%02X:%02X=%02X", dir, reg, valor);
            sensores_enviar(donde, respuesta);
        }
    } else if (strncmp(texto, "SENS_SERIE:", 11) == 0) {
        sens_serie = (texto[11] == '1');
        sensores_enviar(donde, sens_serie ? "ACK_SENS_SERIE:1" : "ACK_SENS_SERIE:0");
    }
}

void sensores_comando(const char *texto)
{
    ejecutar(texto, SENSORES_DESTINO_WS);
}

void sensores_comando_serie(const char *texto)
{
    if (sensores_es_comando(texto)) {
        ejecutar(texto, SENSORES_DESTINO_SERIE);
    }
}
