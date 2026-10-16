/*
 * pruebas_camara.c — Pruebas en PC (sin placa) de ../camara_prueba/recuperacion.c
 *
 * Comprueba las DECISIONES de recuperación de la cámara: esperas crecientes del WiFi, reinicio a los 60 s sin conexión, fallos seguidos del servidor
 * de video, cuadro atascado (5 s) y bucle principal sin señal. Todo con un reloj que las pruebas mueven a mano, incluido el cruce de los 49 días de millis().
 */
#include <stdio.h>
#include <stdint.h>
#include "recuperacion.h"

static int fallos = 0;
static int casos  = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) fallos++;
    printf("%s %s\n", ok ? "OK    " : "FALLO ", nombre);
}

/* Recorre el reloj de a `paso` ms desde `desde` hasta `hasta` (sin conexión) y cuenta reintentos; guarda la hora de cada uno y si hubo reinicio. */
typedef struct {
    int reintentos;
    uint32_t horas[32];
    int reinicio;
    uint32_t hora_reinicio;
} Corrida;

static Corrida correr(RecWifi *w, uint32_t desde, uint32_t hasta, uint32_t paso)
{
    Corrida c = { 0, { 0 }, 0, 0 };
    uint32_t t;
    for (t = desde; (uint32_t)(t - desde) <= (uint32_t)(hasta - desde); t += paso) {
        RecAccion a = rec_wifi_paso(w, t, 0);
        if (a == REC_REINTENTAR && c.reintentos < 32) c.horas[c.reintentos++] = t;
        if (a == REC_REINICIAR) { c.reinicio = 1; c.hora_reinicio = t; break; }
    }
    return c;
}

static void prueba_esperas(void)
{
    printf("-- esperas crecientes --\n");
    caso("la primera espera es de 1 s", rec_espera_ms(0) == 1000u);
    caso("luego 2 s", rec_espera_ms(1) == 2000u);
    caso("luego 4 s", rec_espera_ms(2) == 4000u);
    caso("luego 8 s", rec_espera_ms(3) == 8000u);
    caso("despues se queda en 10 s", rec_espera_ms(4) == 10000u && rec_espera_ms(5) == 10000u);
    caso("con un numero enorme de intentos sigue en 10 s (sin desbordar)", rec_espera_ms(4000000000u) == 10000u);
}

static void prueba_wifi(void)
{
    RecWifi w;
    Corrida c;
    printf("-- WiFi: reintentos y reinicio --\n");

    rec_wifi_iniciar(&w);
    caso("conectado: no hace nada", rec_wifi_paso(&w, 1000, 1) == REC_NADA && rec_wifi_ms_sin_conexion(&w, 1000) == 0u);

    /* La red se cae en t = 100000. La primera llamada solo anota la caída. */
    c = correr(&w, 100000u, 100000u + 59000u, 100);
    caso("sigue sin conexion: reintenta a 1, 3, 7, 15, 25, 35, 45 y 55 s de la caida (esperas 1, 2, 4, 8, 10, 10...)",
         c.reintentos == 8 && c.horas[0] == 101000u && c.horas[1] == 103000u && c.horas[2] == 107000u &&
         c.horas[3] == 115000u && c.horas[4] == 125000u && c.horas[5] == 135000u && c.horas[6] == 145000u && c.horas[7] == 155000u);
    caso("antes de 60 s no reinicia", !c.reinicio);
    caso("cuenta los ms sin conexion", rec_wifi_ms_sin_conexion(&w, 100000u + 59000u) == 59000u);

    /* Sigue la caída: a los 60 s justos aún no, un poco después sí. */
    caso("a los 60 s justos todavia no reinicia", rec_wifi_paso(&w, 160000u, 0) != REC_REINICIAR);
    caso("pasados los 60 s reinicia la placa", rec_wifi_paso(&w, 160001u, 0) == REC_REINICIAR);

    /* Si vuelve la conexión, se olvida todo. */
    rec_wifi_paso(&w, 161000u, 1);
    caso("al reconectar se olvida la caida", rec_wifi_ms_sin_conexion(&w, 161000u) == 0u);
    caso("una caida nueva empieza de cero (1 s de espera, no 10 s)", rec_wifi_paso(&w, 200000u, 0) == REC_NADA && rec_wifi_paso(&w, 200999u, 0) == REC_NADA &&
         rec_wifi_paso(&w, 201000u, 0) == REC_REINTENTAR);

    /* Una caída corta que se arregla en el primer reintento no reinicia. */
    rec_wifi_iniciar(&w);
    rec_wifi_paso(&w, 5000u, 0);
    caso("caida de 1 s: un reintento y vuelve", rec_wifi_paso(&w, 6000u, 0) == REC_REINTENTAR && rec_wifi_paso(&w, 6500u, 1) == REC_NADA);
}

static void prueba_arranque(void)
{
    RecWifi w;
    printf("-- WiFi: arranque --\n");
    rec_wifi_arrancar(&w, 3000u);
    caso("en el arranque el primer intento tiene 5 s antes del primer reintento", rec_wifi_paso(&w, 7999u, 0) == REC_NADA && rec_wifi_paso(&w, 8000u, 0) == REC_REINTENTAR);
    caso("el siguiente reintento espera 2 s", rec_wifi_paso(&w, 9999u, 0) == REC_NADA && rec_wifi_paso(&w, 10000u, 0) == REC_REINTENTAR);
    caso("sin conectar en el arranque, a los 60 s reinicia (contados desde el primer intento)", rec_wifi_paso(&w, 3000u + 60000u, 0) != REC_REINICIAR &&
         rec_wifi_paso(&w, 3000u + 60001u, 0) == REC_REINICIAR);
}

static void prueba_vuelta_del_reloj(void)
{
    RecWifi w;
    Corrida c;
    const uint32_t inicio = 0xFFFFFFFFu - 20000u;   /* la caida empieza 20 s antes de que millis() de la vuelta */
    printf("-- WiFi: millis() da la vuelta --\n");
    rec_wifi_iniciar(&w);
    c = correr(&w, inicio, inicio + 70000u, 100);
    caso("cruzando el desbordamiento sigue reintentando con las mismas esperas", c.reintentos >= 8 && c.horas[0] == inicio + 1000u && c.horas[1] == inicio + 3000u);
    caso("y reinicia a los 60 s aunque el reloj haya dado la vuelta", c.reinicio && c.hora_reinicio - inicio >= 60000u && c.hora_reinicio - inicio <= 60200u);
}

static void prueba_servidor(void)
{
    RecServidor s = { 0 };
    printf("-- servidor de video --\n");
    caso("responde: nada", rec_servidor_paso(&s, 1) == REC_NADA);
    caso("1er fallo: todavia nada", rec_servidor_paso(&s, 0) == REC_NADA);
    caso("2do fallo seguido: todavia nada", rec_servidor_paso(&s, 0) == REC_NADA);
    caso("3er fallo seguido: reinicia", rec_servidor_paso(&s, 0) == REC_REINICIAR);
    caso("si responde entre fallos, la cuenta vuelve a cero", rec_servidor_paso(&s, 1) == REC_NADA && rec_servidor_paso(&s, 0) == REC_NADA && rec_servidor_paso(&s, 0) == REC_NADA &&
         rec_servidor_paso(&s, 1) == REC_NADA && rec_servidor_paso(&s, 0) == REC_NADA);
}

static void prueba_vigilante(void)
{
    printf("-- vigilante interno --\n");
    caso("sin demanda de video, el cuadro viejo no importa", !rec_cuadro_atascado(100000u, 1000u, 0));
    caso("con demanda y cuadro de hace 5 s justos: todavia no", !rec_cuadro_atascado(105000u, 100000u, 1));
    caso("con demanda y cuadro de hace mas de 5 s: reinicia", rec_cuadro_atascado(105001u, 100000u, 1));
    caso("con demanda y cuadros frescos: no", !rec_cuadro_atascado(100100u, 100050u, 1));
    caso("el cuadro viejo tambien se detecta cruzando la vuelta de millis()", rec_cuadro_atascado(10000u, 0xFFFFFFFFu - 4000u, 1) && !rec_cuadro_atascado(100u, 0xFFFFFFFFu - 4000u, 1));
    caso("bucle con senal reciente: bien", !rec_latido_perdido(50000u, 49000u));
    caso("bucle a los 8 s justos: todavia bien", !rec_latido_perdido(58000u, 50000u));
    caso("bucle sin senal mas de 8 s: reinicia", rec_latido_perdido(58001u, 50000u));
    caso("el bucle tambien se vigila cruzando la vuelta de millis()", !rec_latido_perdido(1000u, 0xFFFFFFFFu - 1000u) && rec_latido_perdido(20000u, 0xFFFFFFFFu - 1000u));
}

int main(void)
{
    prueba_esperas();
    prueba_wifi();
    prueba_arranque();
    prueba_vuelta_del_reloj();
    prueba_servidor();
    prueba_vigilante();
    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
