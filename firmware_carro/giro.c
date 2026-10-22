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
 * La anticipación se corrige sola después de cada giro bueno de GIRO_ANT_MIN_GRADOS o más: del error final (con signo, + = se pasó) y la velocidad v sale la anticipación que habría dado justo
 * el ángulo (la usada + error / v); la que se usa de ahí en adelante es la MEDIANA de las últimas GIRO_ANT_MUESTRAS de esas lecturas. Hay una por sentido y por perfil (4), se guardan en la memoria
 * no volátil y se pueden borrar (giro_borrar_anticipaciones). Se puede apagar con GIRO_ANTICIPACION_AUTO 0.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "giro.h"
#include "imu.h"
#include "hw_sensores.h"
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
static int      anticipacion_usada_ms = GIRO_ANTICIPACION_MS;   /* la que se usó en el giro en curso (para aprender de su error) */
static int      duracion_ms   = 0;
/* Las 4 ranuras de la anticipación: ranura = perfil x 2 + (izquierda ? 1 : 0), con perfil PERFIL_MANUAL (0) o PERFIL_AUTO (1). */
#define RANURAS 4
static int      anticipacion_ms[RANURAS];
static int      muestras[RANURAS][GIRO_ANT_MUESTRAS];   /* las últimas lecturas de la anticipación ideal de cada ranura */
static int      n_muestras[RANURAS];
static int      sig_muestra[RANURAS];
static int      guardado_ms[RANURAS];                    /* lo último que se escribió en la memoria no volátil (-1 = nada) */
static int      ranura_actual = 0;                       /* la del giro en curso */
static int      duraciones_hist[RANURAS][5];             /* historial de duraciones para descartar giros largos */
static int      n_duraciones[RANURAS];
static int      sig_duracion[RANURAS];

static int ranura_de(int perfil, int s) { return (perfil == PERFIL_AUTO ? 2 : 0) + (s > 0 ? 1 : 0); }

static int mediana_duracion(int r)
{
    int orden[5];
    int n = n_duraciones[r], i, j;

    for (i = 0; i < n; i++) {
        orden[i] = duraciones_hist[r][i];
    }
    for (i = 1; i < n; i++) {
        int v = orden[i];

        for (j = i - 1; j >= 0 && orden[j] > v; j--) {
            orden[j + 1] = orden[j];
        }
        orden[j + 1] = v;
    }
    return (n % 2 == 1) ? orden[n / 2] : (orden[n / 2 - 1] + orden[n / 2]) / 2;
}

static void anticipaciones_a_defecto(void)
{
    int r;

    for (r = 0; r < RANURAS; r++) {
        anticipacion_ms[r] = r >= 2 ? GIRO_ANTICIPACION_AUTO_INICIAL_MS : GIRO_ANTICIPACION_MS;
        n_muestras[r] = 0;
        sig_muestra[r] = 0;
        n_duraciones[r] = 0;
        sig_duracion[r] = 0;
        guardado_ms[r] = -1;
    }
}

void giro_reiniciar(void)
{
    estado = G_INACTIVO;
    anticipaciones_a_defecto();
}

void giro_cargar_memoria(void)
{
    int r;

    for (r = 0; r < RANURAS; r++) {
        int ms = hw_ant_leer(r);

        if (ms >= 0 && ms <= GIRO_ANTICIPACION_MAX_MS) {
            anticipacion_ms[r] = ms;
            guardado_ms[r] = ms;
        }
    }
}

void giro_borrar_anticipaciones(void)
{
    anticipaciones_a_defecto();
    hw_ant_borrar();
}

int giro_activo(void)  { return estado == G_MOVIENDO; }
int giro_ocupado(void) { return estado != G_INACTIVO; }
int giro_anticipacion_ms(int perfil, int s) { return anticipacion_ms[ranura_de(perfil, s)]; }

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
        sensores_enviar(SENSORES_DESTINO_WS, imu_motivo() == IMU_MOTIVO_SESGO || imu_estado() == IMU_ESTADO_CALIBRANDO ?
                        "GIRO_ERROR:sin_sesgo" : "GIRO_ERROR:sin_imu");
        return 0;
    }
    if (estado != G_INACTIVO || carro_motores_en_marcha()) {
        sensores_enviar(SENSORES_DESTINO_WS, "GIRO_ERROR:ocupado");
        return 0;
    }

    sentido = grados > 0 ? 1 : -1;
    ranura_actual = ranura_de(carro_perfil(), sentido);
    objetivo_grados = grados;
    objetivo_x10 = (int32_t)magnitud * 10;
    angulo_ini_x10 = imu_angulo_x10();
    angulo_ultimo_x10 = angulo_ini_x10;
    inicio_ms = hw_millis();
    tiempo_max_ms = (uint32_t)GIRO_TIEMPO_BASE_MS + (uint32_t)GIRO_TIEMPO_POR_GRADO_MS * (uint32_t)magnitud;
    anticipacion_usada_ms = anticipacion_ms[ranura_actual];
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

/* Mediana de las lecturas guardadas de una ranura (con un número par, el promedio de las dos de en medio). */
static int mediana_ranura(int r)
{
    int orden[GIRO_ANT_MUESTRAS];
    int n = n_muestras[r], i, j;

    for (i = 0; i < n; i++) {
        orden[i] = muestras[r][i];
    }
    for (i = 1; i < n; i++) {   /* orden por inserción: son 5 números */
        int v = orden[i];

        for (j = i - 1; j >= 0 && orden[j] > v; j--) {
            orden[j + 1] = orden[j];
        }
        orden[j + 1] = v;
    }
    return (n % 2 == 1) ? orden[n / 2] : (orden[n / 2 - 1] + orden[n / 2]) / 2;
}

/* Aprende de un giro bueno: la anticipación que habría dado justo el ángulo pedido es la usada más el error final dividido entre la velocidad (error + = se pasó, así que hace falta frenar antes). */
static void corregir_anticipacion(int32_t angulo_final_x10)
{
    int32_t error_x10 = sentido * (angulo_final_x10 - angulo_ini_x10) - objetivo_x10;   /* + = se pasó, - = se quedó corto */
    int32_t ideal;
    int r = ranura_actual;
    int nueva;

    if (!GIRO_ANTICIPACION_AUTO || velocidad_freno_x10 < 150 || objetivo_x10 < (int32_t)GIRO_ANT_MIN_GRADOS * 10) {
        return;   /* con menos de 15 °/s o en un giro corto no se puede estimar bien */
    }
    ideal = anticipacion_usada_ms + (int32_t)((int64_t)error_x10 * 1000 / velocidad_freno_x10);
    if (ideal < 0) {
        ideal = 0;
    }
    if (ideal > GIRO_ANTICIPACION_MAX_MS) {
        ideal = GIRO_ANTICIPACION_MAX_MS;
    }
    muestras[r][sig_muestra[r]] = (int)ideal;
    sig_muestra[r] = (sig_muestra[r] + 1) % GIRO_ANT_MUESTRAS;
    if (n_muestras[r] < GIRO_ANT_MUESTRAS) {
        n_muestras[r]++;
    }
    nueva = mediana_ranura(r);
    anticipacion_ms[r] = nueva;
    if (guardado_ms[r] < 0 || nueva - guardado_ms[r] >= GIRO_ANT_GUARDAR_DIF_MS || guardado_ms[r] - nueva >= GIRO_ANT_GUARDAR_DIF_MS) {
        hw_ant_guardar(r, nueva);
        guardado_ms[r] = nueva;
    }
}

static void informar(uint32_t ahora_ms)
{
    char texto[MENSAJE_SALIDA_MAX];
    int32_t real_x10;
    const char *palabra;
    int r = ranura_actual;
    (void)ahora_ms;

    real_x10 = angulo_ultimo_x10 - angulo_ini_x10;
    if (resultado == R_TIMEOUT) {
        sensores_enviar(SENSORES_DESTINO_WS | SENSORES_DESTINO_SERIE, "ANT_IGNORADO:timeout");
    } else if (resultado == R_ABORTADO) {
        sensores_enviar(SENSORES_DESTINO_WS | SENSORES_DESTINO_SERIE, "ANT_IGNORADO:interrumpido");
    } else if (resultado == R_OK) {
        int32_t error_x10 = sentido * real_x10 - objetivo_x10;
        int ignorar = 0;

        if (error_x10 > 200 || error_x10 < -200) {
            sensores_enviar(SENSORES_DESTINO_WS | SENSORES_DESTINO_SERIE, "ANT_IGNORADO:error_grande");
            ignorar = 1;
        } else if (n_duraciones[r] >= 3) {
            int med = mediana_duracion(r);

            if (duracion_ms > (med * 3) / 2) {
                sensores_enviar(SENSORES_DESTINO_WS | SENSORES_DESTINO_SERIE, "ANT_IGNORADO:giro_largo");
                ignorar = 1;
            }
        }
        if (!ignorar) {
            duraciones_hist[r][sig_duracion[r]] = duracion_ms;
            sig_duracion[r] = (sig_duracion[r] + 1) % 5;
            if (n_duraciones[r] < 5) {
                n_duraciones[r]++;
            }
            corregir_anticipacion(angulo_ultimo_x10);
        }
    }
    if (resultado == R_OK || resultado == R_TIMEOUT) {
        palabra = (resultado == R_OK) ? "GIRO_OK" : "GIRO_TIMEOUT";
        snprintf(texto, sizeof texto, "%s:%d:%d:%d:%d", palabra, objetivo_grados, (int)real_x10, duracion_ms, anticipacion_ms[r]);
    } else {
        palabra = (resultado == R_ABORTADO) ? "abortado" : "sin_imu";
        snprintf(texto, sizeof texto, "GIRO_ERROR:%s:%d:%d:%d:%d", palabra, objetivo_grados, (int)real_x10, duracion_ms, anticipacion_ms[r]);
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
        if (girado_x10 + (velocidad_x10 * anticipacion_usada_ms + 500) / 1000 >= objetivo_x10) {
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
