/*
 * giro.c — Controlador del giro por ángulo (ver giro.h).
 *
 * ESTADOS
 *   INACTIVO  → GIRAR válido → MOVIENDO (el pivote está encendido)
 *   MOVIENDO  → llegó al ángulo / se acabó el tiempo / falló el giroscopio → frena y pasa a ASENTANDO
 *   MOVIENDO  → STOP, EBRAKE, otro movimiento, cambio de perfil o desconexión (giro_abortar) → ASENTANDO (los motores ya están apagados)
 *   ASENTANDO → pasan GIRO_ASENTAR_MS (el carro termina de girar por inercia) → informa el resultado y vuelve a INACTIVO
 *
 * REGLA DE PARADA: se frena cuando  girado + velocidad x anticipación / 1000  >= objetivo  (todo en décimas de grado y décimas de °/s).
 * La anticipación se corrige sola después de cada giro bueno: si el carro siguió girando c décimas de grado a la velocidad v, la anticipación
 * ideal es c / v, y la usada se acerca la mitad del camino (con tope, por lado). Se puede apagar con GIRO_ANTICIPACION_AUTO 0.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "giro.h"
#include "imu.h"
#include "sensores.h"
#include "carro.h"
#include "config.h"

#define G_INACTIVO   0
#define G_MOVIENDO   1
#define G_ASENTANDO  2

#define R_OK         0
#define R_TIMEOUT    1
#define R_ABORTADO   2
#define R_SIN_IMU    3

static int      estado        = G_INACTIVO;
static int      resultado     = R_OK;
static int      sentido       = 1;         /* +1 izquierda, -1 derecha */
static int      objetivo_grados = 0;       /* con signo */
static int32_t  objetivo_x10  = 0;         /* valor absoluto en décimas de grado */
static int32_t  angulo_ini_x10 = 0;        /* ángulo del giroscopio al empezar */
static int32_t  angulo_ultimo_x10 = 0;     /* el último ángulo bueno (por si el sensor falla) */
static uint32_t inicio_ms     = 0;
static uint32_t tiempo_max_ms = 0;
static uint32_t freno_ms      = 0;
static int32_t  angulo_freno_x10 = 0;
static int32_t  velocidad_freno_x10 = 0;
static int      duracion_ms   = 0;
static int      anticipacion_ms[2] = { GIRO_ANTICIPACION_MS, GIRO_ANTICIPACION_MS };   /* [0] = derecha, [1] = izquierda */

static int indice(int s) { return s > 0 ? 1 : 0; }

void giro_reiniciar(void)
{
    estado = G_INACTIVO;
    anticipacion_ms[0] = GIRO_ANTICIPACION_MS;
    anticipacion_ms[1] = GIRO_ANTICIPACION_MS;
}

int giro_activo(void)  { return estado == G_MOVIENDO; }
int giro_ocupado(void) { return estado != G_INACTIVO; }
int giro_anticipacion_ms(int s) { return anticipacion_ms[indice(s)]; }

/* Lee "+90", "-45" o "90" (solo cifras, hasta 3, signo opcional). Devuelve 1 si es válido y deja el número con signo en *valor. */
static int leer_grados(const char *t, int *valor)
{
    int negativo = 0, cifras = 0, v = 0;

    if (*t == '-') {
        negativo = 1;
        t++;
    } else if (*t == '+') {
        t++;
    }
    while (*t >= '0' && *t <= '9' && cifras < 4) {
        v = v * 10 + (*t - '0');
        cifras++;
        t++;
    }
    if (cifras == 0 || *t != '\0') {
        return 0;
    }
    *valor = negativo ? -v : v;
    return 1;
}

int giro_pedir(const char *texto)
{
    int grados = 0;
    int magnitud;

    if (!leer_grados(texto, &grados)) {
        sensores_enviar(SENSORES_DESTINO_WS, "GIRO_ERROR:angulo");
        return 0;
    }
    magnitud = grados < 0 ? -grados : grados;
    if (magnitud < GIRO_MIN_GRADOS || magnitud > GIRO_MAX_GRADOS) {
        sensores_enviar(SENSORES_DESTINO_WS, "GIRO_ERROR:angulo");
        return 0;
    }
    if (imu_estado() != IMU_ESTADO_LISTA) {
        sensores_enviar(SENSORES_DESTINO_WS, "GIRO_ERROR:sin_imu");
        return 0;
    }
    if (estado != G_INACTIVO || carro_motores_en_marcha()) {
        sensores_enviar(SENSORES_DESTINO_WS, "GIRO_ERROR:ocupado");
        return 0;
    }

    sentido = grados > 0 ? 1 : -1;
    objetivo_grados = grados;
    objetivo_x10 = (int32_t)magnitud * 10;
    angulo_ini_x10 = imu_angulo_x10();
    angulo_ultimo_x10 = angulo_ini_x10;
    inicio_ms = hw_millis();
    tiempo_max_ms = (uint32_t)GIRO_TIEMPO_BASE_MS + (uint32_t)GIRO_TIEMPO_POR_GRADO_MS * (uint32_t)magnitud;
    estado = G_MOVIENDO;
    carro_pivote(sentido);
    return 1;
}

/* Pasa a ASENTANDO: el carro ya no recibe empuje y se sigue midiendo un rato. */
static void entrar_asentando(int por_que, uint32_t ahora_ms)
{
    resultado = por_que;
    estado = G_ASENTANDO;
    freno_ms = ahora_ms;
    angulo_freno_x10 = angulo_ultimo_x10;
    velocidad_freno_x10 = sentido * imu_velocidad_x10();
    duracion_ms = (int)(ahora_ms - inicio_ms);
}

void giro_abortar(void)
{
    if (estado == G_MOVIENDO) {
        entrar_asentando(R_ABORTADO, hw_millis());
    }
}

/* Corrige la anticipación con lo que el carro giró de más después de frenar. */
static void corregir_anticipacion(int32_t angulo_final_x10)
{
    int32_t extra_x10 = sentido * (angulo_final_x10 - angulo_freno_x10);   /* lo que siguió girando por inercia */
    int32_t ideal;
    int nueva;

    if (!GIRO_ANTICIPACION_AUTO || velocidad_freno_x10 < 150) {
        return;                                   /* con menos de 15 °/s no se puede estimar bien */
    }
    ideal = (extra_x10 * 1000) / velocidad_freno_x10;
    if (ideal < 0) {
        ideal = 0;
    }
    if (ideal > GIRO_ANTICIPACION_MAX_MS) {
        ideal = GIRO_ANTICIPACION_MAX_MS;
    }
    nueva = anticipacion_ms[indice(sentido)];
    nueva += (int)(ideal - nueva) / 2;
    anticipacion_ms[indice(sentido)] = nueva;
}

static void informar(uint32_t ahora_ms)
{
    char texto[MENSAJE_SALIDA_MAX];
    int32_t real_x10;
    const char *palabra;
    (void)ahora_ms;

    real_x10 = angulo_ultimo_x10 - angulo_ini_x10;
    if (resultado == R_OK) {
        corregir_anticipacion(angulo_ultimo_x10);
    }
    if (resultado == R_OK || resultado == R_TIMEOUT) {
        palabra = (resultado == R_OK) ? "GIRO_OK" : "GIRO_TIMEOUT";
        snprintf(texto, sizeof texto, "%s:%d:%d:%d", palabra, objetivo_grados, (int)real_x10, duracion_ms);
    } else {
        palabra = (resultado == R_ABORTADO) ? "abortado" : "sin_imu";
        snprintf(texto, sizeof texto, "GIRO_ERROR:%s:%d:%d:%d", palabra, objetivo_grados, (int)real_x10, duracion_ms);
    }
    sensores_enviar(SENSORES_DESTINO_WS, texto);
    estado = G_INACTIVO;
}

void giro_actualizar(uint32_t ahora_ms)
{
    if (estado == G_INACTIVO) {
        return;
    }
    if (imu_estado() == IMU_ESTADO_LISTA) {
        angulo_ultimo_x10 = imu_angulo_x10();
    }

    if (estado == G_MOVIENDO) {
        int32_t girado_x10;
        int32_t velocidad_x10;

        if (imu_estado() != IMU_ESTADO_LISTA) {
            carro_pivote(0);                       /* el sensor falló: se frena y se informa (nunca se gira a ciegas) */
            entrar_asentando(R_SIN_IMU, ahora_ms);
            return;
        }
        girado_x10 = sentido * (angulo_ultimo_x10 - angulo_ini_x10);
        velocidad_x10 = sentido * imu_velocidad_x10();
        if (velocidad_x10 < 0) {
            velocidad_x10 = 0;
        }
        if (girado_x10 + (velocidad_x10 * anticipacion_ms[indice(sentido)] + 500) / 1000 >= objetivo_x10) {
            carro_pivote(0);
            entrar_asentando(R_OK, ahora_ms);
            return;
        }
        if ((uint32_t)(ahora_ms - inicio_ms) >= tiempo_max_ms) {
            carro_pivote(0);
            entrar_asentando(R_TIMEOUT, ahora_ms);
        }
        return;
    }

    /* ASENTANDO */
    if ((uint32_t)(ahora_ms - freno_ms) >= GIRO_ASENTAR_MS || imu_estado() != IMU_ESTADO_LISTA) {
        informar(ahora_ms);
    }
}
