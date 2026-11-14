/*
 * navegador.c — Máquina de estados del modo semiautónomo (ver navegador.h y docs/navegador.md).
 *
 * GUÍA DE LECTURA
 *  1. Lo que se ve primero son las ayudas de posición: el carro no tiene sensores de
 *     posición, así que se calcula por TIEMPO (calcular_pose): cuánto tiempo estuvo
 *     avanzando o girando, convertido con la calibración del carro (calibracion.c: arranque,
 *     ms por cm, frenado y ms por grado de cada lado).
 *  2. planificar() usa trayectoria.c para sacar los pasos (AVANZAR/GIRAR con su tiempo) desde
 *     la posición y el rumbo actuales hasta el siguiente punto de la ruta.
 *  3. EL TEMPORIZADOR DE TRAMOS está en contar_tiempo_ejecutado() y en tick_siguiendo() /
 *     tick_esquivando(): un paso termina cuando su tiempo ya se ejecutó. Todo lo que dependa
 *     de "cuánto dura cada tramo" está aquí; si algún día el firmware acepta comandos con
 *     duración (por ejemplo MOVE:FWD:2000), solo hay que cambiar esa parte.
 *  4. navegador_tick() es la función que se llama cada ~50 ms: actualiza la posición, mira el
 *     último cuadro de obstáculos y llama a la función del estado actual.
 *  4b. GIRO CON LA CÁMARA: si la app avisa lo que la cámara vio girar (navegador_aporte_visual), un giro se corta cuando la cámara
 *     ve el ángulo pedido (giro_terminado), con el tiempo planeado como respaldo; si la cámara no tiene confianza, el giro sigue
 *     por tiempo y la posición estimada también.
 *  5. Estados: tick_siguiendo, tick_detenido, tick_esquivando y tick_reincorporando (los
 *     demás estados siempre devuelven STOP).
 *  Solo enteros, arreglos de tamaño fijo, sin malloc y sin recursión.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "navegador.h"
#include "calibracion.h"
#include "mascara_vision.h"

#define MM_POR_CELDA   (CELDA_CM * 10)
#define LIMITE_CONTADOR 1000
#define NAV_ESCANEO_SALTO_MM    250    /* V2: entre dos lecturas seguidas del barrido (unos 13 grados) la cara de un mismo objeto no se aleja de lado más que esto */
#define TOF_ALCANCE_ESCANEO_MM 1100   /* V2: lecturas más lejanas que esto no se usan para medir el rodeo (cerca del alcance el dato es poco fiable) */
#define NAV_VENTANA_TRAMOS 12   /* ubicar_en_linea mira desde un tramo antes del actual hasta este número de tramos adelante (el carro pasa el obstáculo y queda más adelante) */   /* los contadores de cuadros no crecen más que esto */

/* Seno de 0 a 90 grados, de grado en grado, en milésimas (1000 = 1.0). Con él se mueve la posición estimada en CUALQUIER rumbo (no solo en múltiplos de 45):
 * así un avance en diagonal suave (por ejemplo al volver a la línea) cambia la x y la y como debe. Los valores de 45 grados (707) son los de siempre. */
static const int SENO_MIL[91] = {
       0,   17,   35,   52,   70,   87,  105,  122,  139,  156,  174,  191,  208,  225,  242,
     259,  276,  292,  309,  326,  342,  358,  375,  391,  407,  423,  438,  454,  469,  485,
     500,  515,  530,  545,  559,  574,  588,  602,  616,  629,  643,  656,  669,  682,  695,
     707,  719,  731,  743,  755,  766,  777,  788,  799,  809,  819,  829,  839,  848,  857,
     866,  875,  883,  891,  899,  906,  914,  921,  927,  934,  940,  946,  951,  956,  961,
     966,  970,  974,  978,  982,  985,  988,  990,  993,  995,  996,  998,  999,  999, 1000,
    1000
};

/* Seno de cualquier ángulo entero en grados (milésimas). */
static int seno_mil(int grados)
{
    int g = ((grados % 360) + 360) % 360;

    if (g <= 90) {
        return SENO_MIL[g];
    }
    if (g <= 180) {
        return SENO_MIL[180 - g];
    }
    if (g <= 270) {
        return -SENO_MIL[g - 180];
    }
    return -SENO_MIL[360 - g];
}

/* Componentes (en milésimas) del vector unitario que apunta hacia el rumbo: 0 = arriba (y decrece), 90 = derecha (x crece). */
static int dir_x_mil(int rumbo_grados)
{
    return seno_mil(rumbo_grados);
}

static int dir_y_mil(int rumbo_grados)
{
    return -seno_mil(rumbo_grados + 90);
}

/* J16: seno de un ángulo en DÉCIMAS de grado, interpolando la tabla entre dos grados enteros. Con grados enteros un rumbo medido de 0.4° no movía la x
 * (y uno de 0.6° la movía como 1°): en 150 cm son hasta ~1 cm de error lateral que la pose no veía. */
static int seno_mil_x10(int x10)
{
    int g = ((x10 % 3600) + 3600) % 3600;
    int s0 = seno_mil(g / 10), s1 = seno_mil(g / 10 + 1);

    return s0 + (s1 - s0) * (g % 10) / 10;
}

static int dir_x_mil_x10(int rumbo_x10)
{
    return seno_mil_x10(rumbo_x10);
}

static int dir_y_mil_x10(int rumbo_x10)
{
    return -seno_mil_x10(rumbo_x10 + 900);
}

/* ── Ayudas numéricas ──────────────────────────────────────────────────── */

/* División entera redondeada hacia abajo (también con numeradores negativos); b > 0. */
static int division_piso(int a, int b)
{
    int q = a / b;

    if ((a % b != 0) && (a < 0)) {
        q--;
    }
    return q;
}

static int normalizar_grados(int grados)
{
    return ((grados % 360) + 360) % 360;
}

/* Índice de dirección (0 a 7) más cercano a un rumbo en grados. */
static int indice_direccion(int rumbo_grados)
{
    return ((normalizar_grados(rumbo_grados) + 22) / 45) % 8;
}

static int signo(int x)
{
    return (x > 0) - (x < 0);
}

static long valor_absoluto_largo(long x)
{
    return (x < 0) ? -x : x;
}

/* Celda más cercana a una coordenada en milímetros. */
static int celda_mas_cercana(int mm)
{
    return division_piso(mm + MM_POR_CELDA / 2, MM_POR_CELDA);
}

static void limitar_contador(int *contador)
{
    if (*contador > LIMITE_CONTADOR) {
        *contador = LIMITE_CONTADOR;
    }
}


/* ── Geometría con enteros (sin math.h) ────────────────────────────────── */

/* Tangente de 0 a 45 grados, de grado en grado, por 1000. Sirve para sacar el ángulo de un vector sin usar atan. */
static const int TANGENTE_MIL[46] = {
       0,   17,   35,   52,   70,   87,  105,  123,  141,  158,  176,  194,  213,  231,  249,  268,
     287,  306,  325,  344,  364,  384,  404,  424,  445,  466,  488,  510,  532,  554,  577,  601,
     625,  649,  675,  700,  727,  754,  781,  810,  839,  869,  900,  933,  966, 1000
};

/* Raíz cuadrada entera (redondeada hacia abajo). */
static long raiz_entera(long valor)
{
    long x = 0, paso = 1L << 30;

    if (valor <= 0) {
        return 0;
    }
    while (paso > valor) {
        paso >>= 2;
    }
    while (paso != 0) {
        if (valor >= x + paso) {
            valor -= x + paso;
            x = (x >> 1) + paso;
        } else {
            x >>= 1;
        }
        paso >>= 2;
    }
    return x;
}

/* Ángulo (0 a 45 grados) cuya tangente es numerador/denominador, con numerador <= denominador. */
static int grados_de_cociente(long numerador, long denominador)
{
    long mil;
    int k, mejor = 0;

    if (denominador <= 0 || numerador <= 0) {
        return 0;
    }
    mil = numerador * 1000 / denominador;
    for (k = 1; k <= 45; k++) {
        if (valor_absoluto_largo(mil - TANGENTE_MIL[k]) < valor_absoluto_largo(mil - TANGENTE_MIL[mejor])) {
            mejor = k;
        }
    }
    return mejor;
}

/* Rumbo (0 = arriba, 90 = derecha, 0 a 359) del vector (dx, dy) de la cuadrícula (y crece hacia abajo). Con el vector en cero devuelve `por_defecto`. */
static int rumbo_del_vector(long dx, long dy, int por_defecto)
{
    long ax = (dx < 0) ? -dx : dx;
    long ay = (dy < 0) ? -dy : dy;
    int desde_vertical;

    if (ax == 0 && ay == 0) {
        return por_defecto;
    }
    desde_vertical = (ax <= ay) ? grados_de_cociente(ax, ay) : 90 - grados_de_cociente(ay, ax);   /* ángulo medido desde la vertical, 0 a 90 */
    if (dx >= 0 && dy < 0) {
        return desde_vertical;                 /* arriba-derecha */
    }
    if (dx >= 0) {
        return 180 - desde_vertical;           /* abajo-derecha */
    }
    if (dy >= 0) {
        return 180 + desde_vertical;           /* abajo-izquierda */
    }
    return normalizar_grados(360 - desde_vertical);   /* arriba-izquierda */
}

/* J16: como grados_de_cociente pero en décimas de grado (0 a 450), interpolando la tabla de tangentes entre dos grados enteros. */
static int grados_de_cociente_x10(long numerador, long denominador)
{
    long mil;
    int k;

    if (denominador <= 0 || numerador <= 0) {
        return 0;
    }
    mil = numerador * 1000 / denominador;
    for (k = 1; k <= 45 && mil > TANGENTE_MIL[k]; k++) {
    }
    if (k > 45) {
        return 450;
    }
    return (k - 1) * 10 + (int)((mil - TANGENTE_MIL[k - 1]) * 10 / (TANGENTE_MIL[k] - TANGENTE_MIL[k - 1]));
}

/* J16: rumbo del vector (dx, dy) en décimas de grado (0 a 3599); con el vector en cero, `por_defecto_x10`. */
static int rumbo_del_vector_x10(long dx, long dy, int por_defecto_x10)
{
    long ax = (dx < 0) ? -dx : dx;
    long ay = (dy < 0) ? -dy : dy;
    int desde_vertical;

    if (ax == 0 && ay == 0) {
        return por_defecto_x10;
    }
    desde_vertical = (ax <= ay) ? grados_de_cociente_x10(ax, ay) : 900 - grados_de_cociente_x10(ay, ax);
    if (dx >= 0 && dy < 0) {
        return desde_vertical;
    }
    if (dx >= 0) {
        return 1800 - desde_vertical;
    }
    if (dy >= 0) {
        return 1800 + desde_vertical;
    }
    return (3600 - desde_vertical) % 3600;
}

/* Giro más corto (con signo, + derecha) para pasar del rumbo `desde` al rumbo `hasta`: de -179 a 180. */
static int giro_minimo(int desde, int hasta)
{
    int d = normalizar_grados(hasta - desde);

    return (d > 180) ? d - 360 : d;
}

/* J15: ¿la pose de los avances se orienta con el giroscopio? */
static int rumbo_medido_activo(const Navegador *nav)
{
    return nav->usa_giroscopio && !nav->giro_fw_caido && !nav->sin_rumbo_medido;
}

/* J16: ¿el firmware sostiene el rumbo al avanzar? Solo con RUMBO pedido y el rumbo medido de J15 (sin él no se sabe a qué ángulo apuntar). */
static int rumbo_fw_activo(const Navegador *nav)
{
    return nav->rumbo_fw && rumbo_medido_activo(nav);
}

/* ── Giro medido con la cámara ─────────────────────────────────────────── */

/* Tiempo que el carro lleva MOVIÉNDOSE bajo el comando actual (sin el retardo de arranque). */
static int ms_de_movimiento(const Navegador *nav)
{
    int ms = nav->seg_ms - nav->cal.arranque_ms;

    return (ms > 0) ? ms : 0;
}

static int ms_por_grado_x10_de(const Navegador *nav, int lado)
{
    return (lado < 0) ? nav->cal.ms_por_grado_izq_x10 : nav->cal.ms_por_grado_der_x10;
}

/* ¿La cámara midió suficiente del giro actual como para fiarse de ella? */
static int giro_visual_confiable(const Navegador *nav)
{
    int movimiento = ms_de_movimiento(nav);

    return movimiento >= NAV_MIN_MS_VISUAL && nav->visual_cubierto_ms * 100 >= movimiento * NAV_COBERTURA_VISUAL_PCT;
}

/* Grados por 10 girados hasta ahora en la dirección del comando de giro actual: lo que vio la cámara más, por tiempo, el pedazo
 * del giro que la cámara no cubrió. */
static int giro_estimado_x10(const Navegador *nav, int lado)
{
    int movimiento = ms_de_movimiento(nav);
    int cubierto = (nav->visual_cubierto_ms < movimiento) ? nav->visual_cubierto_ms : movimiento;

    return nav->giro_visual_x10 + (int)((long)(movimiento - cubierto) * 100 / ms_por_grado_x10_de(nav, lado));
}

/* Grados enteros girados en este comando: con la cámara si es confiable, si no por tiempo (como siempre). */
static int grados_girados(const Navegador *nav, int lado)
{
    if (nav->giro_fw && nav->giro_fw_estado == 1) {   /* el giroscopio del firmware ya midió el ángulo REAL */
        return (nav->giro_fw_real_x10 + (nav->giro_fw_real_x10 >= 0 ? 5 : -5)) / 10;
    }
    if (nav->giro_fw) {   /* esperando la respuesta del firmware: mientras tanto la pose avanza por tiempo pero nunca más allá del ángulo pedido (así no salta de ida y vuelta cuando llega el real) */
        int por_tiempo = cal_ms_a_grados(&nav->cal, lado, nav->seg_ms);

        return (por_tiempo > nav->cmd_pedido) ? nav->cmd_pedido : por_tiempo;
    }
    if (giro_visual_confiable(nav)) {
        return giro_estimado_x10(nav, lado) / 10;
    }
    return cal_ms_a_grados(&nav->cal, lado, nav->seg_ms);
}

int navegador_poner_latencia_visual(Navegador *nav, int ms)
{
    if (nav == NULL || ms < 0 || ms > 1000) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->latencia_visual_ms = ms;
    return NAV_OK;
}

int navegador_poner_frenar_lejos_centro(Navegador *nav, int activo)
{
    if (nav == NULL || (activo != 0 && activo != 1)) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->frenar_lejos_centro = activo;
    return NAV_OK;
}

void navegador_aporte_visual(Navegador *nav, int giro_grados_x10, int dt_ms, int confiable)
{
    if (nav == NULL || !confiable || dt_ms <= 0) {
        return;
    }
    if (nav->cmd_actual != NAV_CMD_LEFT && nav->cmd_actual != NAV_CMD_RIGHT) {
        return;   /* solo cuenta mientras gira */
    }
    if (nav->estado != NAV_SIGUIENDO && nav->estado != NAV_ESQUIVANDO && nav->estado != NAV_REINCORPORANDO) {
        return;
    }
    if (nav->seg_ms <= nav->cal.arranque_ms + nav->latencia_visual_ms) {
        return;   /* el carro todavía no arranca (o el cuadro que llega es de antes de arrancar, por el retraso de la cámara) */
    }
    nav->giro_visual_x10 += (nav->cmd_actual == NAV_CMD_RIGHT) ? giro_grados_x10 : -giro_grados_x10;
    nav->visual_cubierto_ms += (dt_ms > NAV_DT_MAX_MS) ? NAV_DT_MAX_MS : dt_ms;
}

/* ¿Ya terminó este giro (de `grados` grados, con `tiempo_ms` planeado y `ms` ejecutados)?
 *  - Con la cámara confiable: sí cuando lo girado más lo que el carro seguirá girando al frenar llega al ángulo pedido (pero no
 *    antes de NAV_GIRO_MIN_PCT del tiempo planeado), o si ya pasó el tiempo de respaldo (NAV_GIRO_MAX_PCT del planeado).
 *  - Sin cámara confiable: por tiempo, como siempre. */
static int giro_terminado(const Navegador *nav, int grados, int tiempo_ms, int ms)
{
    int lado = (nav->cmd_actual == NAV_CMD_LEFT) ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA;

    if (nav->giro_fw) {
        return nav->giro_fw_estado == 1;   /* lo mide y lo frena el firmware: se termina cuando llega su respuesta (giro_fw_vigilar cubre la espera máxima) */
    }

    if (giro_visual_confiable(nav)) {
        int objetivo_x10 = grados * 10 - ((lado < 0) ? nav->cal.frenado_grados_izq_x10 : nav->cal.frenado_grados_der_x10);

        if (giro_estimado_x10(nav, lado) >= objetivo_x10 && ms * 100 >= tiempo_ms * NAV_GIRO_MIN_PCT) {
            return 1;
        }
        return ms + NAV_TICK_MS / 2 >= tiempo_ms * NAV_GIRO_MAX_PCT / 100;
    }
    return ms + NAV_TICK_MS / 2 >= tiempo_ms;
}

/* ── Posición estimada por tiempo ──────────────────────────────────────── */

/* Calcula la pose actual a partir de la pose al empezar el comando y del tiempo que lleva.
 * Se calcula desde el TOTAL de tiempo del comando (no sumando pedacitos) para no acumular
 * errores de redondeo. */
/* Mueve la pose el desplazamiento del CENTRO durante un giro de `grados` hacia `lado` que empezó con el rumbo `rumbo_inicial` (ver cal_desplazamiento_giro): con ruedas mecanum
 * el carro no gira sobre un punto fijo. Con la calibración sin desplazamiento (ceros) no hace nada. */
static void sumar_desplazamiento_giro(Navegador *nav, int lado, int grados, int rumbo_inicial)
{
    int dx = 0, dy = 0;

    cal_desplazamiento_giro(&nav->cal, lado, grados, rumbo_inicial, &dx, &dy, NULL, NULL);
    nav->x_mm += dx;
    nav->y_mm += dy;
}

/* Diferencia angular en décimas, con vuelta de 360 grados y signo. */
static int diferencia_x10(int desde, int hasta)
{
    int d = (hasta % 3600 - desde % 3600) % 3600;
    if (d > 1800) d -= 3600;
    if (d < -1800) d += 3600;
    return d;
}

static int grados_de_x10(int x10)
{
    return normalizar_grados(((x10 % 3600 + 3600) % 3600 + 5) / 10);
}

/* El giroscopio no mide distancia: solo orienta cada incremento obtenido por
 * tiempo. No se proyecta TODO el avance con el último ángulo (eso inventaría
 * un desplazamiento al cambiar el rumbo); se integra el arco por incrementos. */
static void calcular_avance_medido(Navegador *nav, int distancia)
{
    int rumbo = nav->avance_rumbo_x10;
    int incremento = distancia - nav->avance_mm_previo;
    int fresco = nav->sens_angulo_ok &&
        (uint32_t)(nav->t_ultimo_ms - nav->sens_angulo_t_ms) <= NAV_TOF_FRESCO_MS;
    if (fresco) {
        if (!nav->avance_imu_ref_ok) {
            nav->avance_imu_ref_x10 = nav->sens_angulo_x10;
            nav->avance_rumbo_ref_x10 = rumbo;
            nav->avance_imu_ref_ok = 1;
        }
        rumbo = nav->avance_rumbo_ref_x10 - diferencia_x10(nav->avance_imu_ref_x10, nav->sens_angulo_x10);
        /* Un salto de IMU_CERO o una medida corrupta no debe rotar la pose.
         * Se recupera la referencia conservando el último rumbo conocido. */
        if (diferencia_x10(nav->avance_rumbo_x10, rumbo) > 250 ||
            diferencia_x10(nav->avance_rumbo_x10, rumbo) < -250) {
            nav->avance_imu_ref_ok = 0;
            rumbo = nav->avance_rumbo_x10;
        }
    }
    {
        int medio_x10 = nav->avance_rumbo_x10 + diferencia_x10(nav->avance_rumbo_x10, rumbo) / 2;
        int ux, uy;

        if (nav->sin_trig_fino) {   /* lo de J15: el rumbo medio redondeado a grados enteros */
            ux = dir_x_mil(grados_de_x10(medio_x10));
            uy = dir_y_mil(grados_de_x10(medio_x10));
        } else {
            ux = dir_x_mil_x10(medio_x10);
            uy = dir_y_mil_x10(medio_x10);
        }
        nav->avance_x_mil += (long)ux * incremento;
        nav->avance_y_mil += (long)uy * incremento;
    }
    nav->avance_mm_previo = distancia;
    nav->avance_rumbo_x10 = rumbo;
    nav->x_mm = nav->base_x_mm + (int)(nav->avance_x_mil / 1000);
    nav->y_mm = nav->base_y_mm + (int)(nav->avance_y_mil / 1000);
    nav->rumbo_grados = grados_de_x10(rumbo);
    nav->rumbo_x10 = ((rumbo % 3600) + 3600) % 3600;
}

/* J16: si alguien cambió los grados enteros sin las décimas (una prueba, o un plan que fija el rumbo), mandan los grados. */
static void respetar_grados(int *x10, int grados)
{
    if (grados_de_x10(*x10) != normalizar_grados(grados)) {
        *x10 = normalizar_grados(grados) * 10;
    }
}

/* J16: deja rumbo_x10 y rumbo_grados de acuerdo. Con RUMBO (el firmware sostiene el objetivo que se le pide) rumbo_grados es el redondeo de rumbo_x10 y la fracción se
 * conserva de un comando al siguiente. Sin RUMBO, o con sin_trig_fino, mandan los grados enteros, como antes: en la simulación, sin RUMBO conservar la fracción retrasaba
 * las correcciones de rumbo de más de 4° (el redondeo las adelantaba) y las rectas empeoraban un poco. */
static void sincronizar_rumbo(Navegador *nav)
{
    if (nav->sin_trig_fino || !rumbo_fw_activo(nav)) {
        nav->rumbo_x10 = nav->rumbo_grados * 10;
    } else {
        nav->rumbo_x10 = ((nav->rumbo_x10 % 3600) + 3600) % 3600;
        nav->rumbo_grados = grados_de_x10(nav->rumbo_x10);
    }
}

/* El rumbo con décimas después de girar `grados` hacia `lado` desde el rumbo del inicio del comando: con el ángulo REAL del giroscopio si ya llegó (en décimas), si no con
 * los grados enteros que se estimaron. */
static int rumbo_tras_giro_x10(const Navegador *nav, int lado, int grados)
{
    int girado_x10 = (nav->giro_fw && nav->giro_fw_estado == 1) ? nav->giro_fw_real_x10 : grados * 10;

    return nav->base_rumbo_x10 + lado * girado_x10;
}

static void calcular_pose(Navegador *nav)
{
    int distancia, grados;

    respetar_grados(&nav->base_rumbo_x10, nav->base_rumbo_grados);
    nav->x_mm = nav->base_x_mm;
    nav->y_mm = nav->base_y_mm;
    nav->rumbo_grados = nav->base_rumbo_grados;
    nav->rumbo_x10 = nav->base_rumbo_x10;

    switch (nav->cmd_actual) {
    case NAV_CMD_FORWARD:
    case NAV_CMD_REVERSE:
        if (nav->cmd_actual == NAV_CMD_REVERSE) {
            distancia = -cal_ms_a_mm_atras(&nav->cal, nav->seg_ms);   /* milímetros hacia atrás (con la calibración del retroceso) */
        } else {
            distancia = cal_ms_a_mm(&nav->cal, nav->seg_ms);          /* milímetros recorridos */
        }
        nav->x_mm = nav->base_x_mm + dir_x_mil(nav->base_rumbo_grados) * distancia / 1000;
        nav->y_mm = nav->base_y_mm + dir_y_mil(nav->base_rumbo_grados) * distancia / 1000;
        if (nav->usa_giroscopio && !nav->giro_fw_caido && !nav->sin_rumbo_medido) {
            calcular_avance_medido(nav, distancia);
        }
        break;
    case NAV_CMD_LEFT:
        grados = grados_girados(nav, CAL_LADO_IZQUIERDA);
        nav->rumbo_grados = normalizar_grados(nav->base_rumbo_grados - grados);
        nav->rumbo_x10 = rumbo_tras_giro_x10(nav, CAL_LADO_IZQUIERDA, grados);
        sumar_desplazamiento_giro(nav, CAL_LADO_IZQUIERDA, grados, nav->base_rumbo_grados);
        break;
    case NAV_CMD_RIGHT:
        grados = grados_girados(nav, CAL_LADO_DERECHA);
        nav->rumbo_grados = normalizar_grados(nav->base_rumbo_grados + grados);
        nav->rumbo_x10 = rumbo_tras_giro_x10(nav, CAL_LADO_DERECHA, grados);
        sumar_desplazamiento_giro(nav, CAL_LADO_DERECHA, grados, nav->base_rumbo_grados);
        break;
    default:
        /* V2: parado después de un avance, el rumbo sigue al giroscopio. Antes el rumbo se quedaba con la última lectura recibida AVANZANDO (hasta 150 ms vieja) y lo
         * que el carro giraba al frenar se perdía: si se detenía mientras el firmware corregía el rumbo (o en pleno vistazo) el error pasaba de 5 grados y se
         * arrastraba al resto de la ruta. La referencia es la misma pareja (lectura del giroscopio, rumbo) con la que cerró el avance. */
        if (nav->cmd_actual == NAV_CMD_STOP && (nav->cmd_previo == NAV_CMD_FORWARD || nav->cmd_previo == NAV_CMD_REVERSE) && rumbo_medido_activo(nav) &&
            nav->avance_imu_ref_ok && nav->sens_angulo_ok && (uint32_t)(nav->t_ultimo_ms - nav->sens_angulo_t_ms) <= NAV_TOF_FRESCO_MS) {
            int girado = diferencia_x10(nav->avance_imu_ref_x10, nav->sens_angulo_x10);   /* + izquierda, como el firmware */

            if (girado >= -300 && girado <= 300) {
                int rumbo = nav->base_rumbo_x10 - girado;

                nav->rumbo_x10 = ((rumbo % 3600) + 3600) % 3600;
                nav->rumbo_grados = grados_de_x10(nav->rumbo_x10);
            }
        }
        break;
    }
    sincronizar_rumbo(nav);
}

/* Toma la pose actual como punto de partida del siguiente comando. */
static void fijar_pose(Navegador *nav)
{
    nav->base_x_mm = nav->x_mm;
    nav->base_y_mm = nav->y_mm;
    respetar_grados(&nav->rumbo_x10, nav->rumbo_grados);
    nav->base_rumbo_grados = nav->rumbo_grados;
    nav->base_rumbo_x10 = nav->rumbo_x10;
    nav->seg_ms = 0;
    nav->avance_mm_previo = 0;
    nav->avance_x_mil = nav->avance_y_mil = 0;
    nav->avance_rumbo_x10 = nav->avance_rumbo_ref_x10 = nav->rumbo_x10;   /* J16: con décimas (antes rumbo_grados x 10: hasta 0.5° de error en cada plan nuevo) */
    nav->avance_imu_ref_x10 = nav->sens_angulo_x10;
    nav->avance_imu_ref_ok = nav->sens_angulo_ok &&
        (uint32_t)(nav->t_ultimo_ms - nav->sens_angulo_t_ms) <= NAV_TOF_FRESCO_MS;
}

/* Al soltar un comando de movimiento el carro NO se detiene al instante: sigue avanzando (o
 * girando) el frenado medido. Se suma a la pose para que la posición estimada lo incluya. */
static void sumar_frenado(Navegador *nav)
{
    int mm, grados;

    if ((nav->cmd_actual == NAV_CMD_LEFT || nav->cmd_actual == NAV_CMD_RIGHT) && nav->giro_fw && nav->giro_fw_estado == 1) {
        return;   /* el ángulo que midió el giroscopio ya incluye lo que el carro siguió girando al frenar */
    }
    respetar_grados(&nav->rumbo_x10, nav->rumbo_grados);

    switch (nav->cmd_actual) {
    case NAV_CMD_FORWARD:
    case NAV_CMD_REVERSE:
        mm = nav->cal.frenado_mm;
        if (nav->cmd_actual == NAV_CMD_REVERSE) {
            mm = -mm;
        }
        if (nav->sin_trig_fino) {
            nav->x_mm += dir_x_mil(nav->rumbo_grados) * mm / 1000;
            nav->y_mm += dir_y_mil(nav->rumbo_grados) * mm / 1000;
        } else {
            nav->x_mm += dir_x_mil_x10(nav->rumbo_x10) * mm / 1000;
            nav->y_mm += dir_y_mil_x10(nav->rumbo_x10) * mm / 1000;
        }
        break;
    case NAV_CMD_LEFT:
        grados = cal_frenado_grados(&nav->cal, CAL_LADO_IZQUIERDA);   /* lo que sigue girando al frenar... */
        sumar_desplazamiento_giro(nav, CAL_LADO_IZQUIERDA, grados, nav->rumbo_grados);   /* ...y el centro también se sigue moviendo */
        nav->rumbo_grados = normalizar_grados(nav->rumbo_grados - grados);
        nav->rumbo_x10 -= grados * 10;
        break;
    case NAV_CMD_RIGHT:
        grados = cal_frenado_grados(&nav->cal, CAL_LADO_DERECHA);
        sumar_desplazamiento_giro(nav, CAL_LADO_DERECHA, grados, nav->rumbo_grados);
        nav->rumbo_grados = normalizar_grados(nav->rumbo_grados + grados);
        nav->rumbo_x10 += grados * 10;
        break;
    default:
        break;
    }
    sincronizar_rumbo(nav);
}

static void registrar_fin_comando(Navegador *nav);                      /* se definen más abajo, junto a la cola de diagnóstico */
static void registrar_inicio_comando(Navegador *nav, ComandoNav nuevo);
static void giro_fw_empezar(Navegador *nav, ComandoNav nuevo);

/* Ajusta el tiempo ya ejecutado del paso cuando el carro frena de FORWARD a STOP y luego reanuda: el frenado cuenta una sola vez y ms_paso nunca queda negativo. */
static void ajustar_paso_tras_parar(Navegador *nav)
{
    if (!nav->frenado_pendiente) {
        return;
    }
    nav->frenado_pendiente = 0;
    nav->ms_paso += (int)((long)nav->cal.frenado_mm * nav->cal.ms_por_cm_x10 / 100) - nav->cal.arranque_ms;
    if (nav->ms_paso < 0) {
        nav->ms_paso = 0;
    }
}

/* Cambia el comando actual (si es distinto) cerrando el tramo anterior. */
static void cambiar_comando(Navegador *nav, ComandoNav nuevo)
{
    if (nuevo != nav->cmd_actual) {
        if (nav->cmd_actual == NAV_CMD_LEFT || nav->cmd_actual == NAV_CMD_RIGHT) {
            nav->atras_conocido_mm = 0; /* el espacio recorrido ya no queda detras */
        }
        if (nav->cmd_actual == NAV_CMD_FORWARD && nuevo == NAV_CMD_STOP &&
            (nav->estado == NAV_SIGUIENDO || nav->estado == NAV_DETENIDO_OBSTACULO)) {
            nav->frenado_pendiente = 1;
        }
        sumar_frenado(nav);
        registrar_fin_comando(nav);   /* antes de fijar_pose: necesita la pose de antes y de después del comando */
        fijar_pose(nav);
        nav->giro_fw = 0;                /* el giro por ángulo (si había) ya se cerró en registrar_fin_comando */
        nav->giro_fw_estado = 0;
        nav->giro_fw_pedir = 0;
        nav->giro_fw_consultando = 0;
        nav->giro_fw_consulta_pedir = 0;
        nav->giro_visual_x10 = 0;        /* un comando nuevo empieza a medir desde cero */
        nav->visual_cubierto_ms = 0;
        nav->rumbo_fw_hay_enviado = 0;   /* J16: cada FORWARD nuevo el firmware empieza sosteniendo el rumbo de ese momento */
        nav->rumbo_fw_pedir = 0;
        nav->cmd_previo = (int)nav->cmd_actual;
        nav->cmd_actual = nuevo;
        registrar_inicio_comando(nav, nuevo);
        giro_fw_empezar(nav, nuevo);
    }
}

/* ── Pasos, esquivar y errores ─────────────────────────────────────────── */

/* Comando que corresponde a un paso: AVANZAR = FORWARD; GIRAR + = RIGHT, GIRAR - = LEFT. */
static ComandoNav comando_de_paso(const Paso *paso)
{
    if (paso->tipo == PASO_AVANZAR) {
        return NAV_CMD_FORWARD;
    }
    return (paso->valor > 0) ? NAV_CMD_RIGHT : NAV_CMD_LEFT;
}

/* Para deshacer el giro de la esquivada (angulo_total grados hacia `lado`) se gira SIEMPRE hacia el lado CONTRARIO esos mismos grados, aunque sean 225
 * (tras dos reintentos). Seguir hacia el mismo lado los 360 - total que faltan es más corto, pero entonces el carro da una vuelta entera en un solo sentido y
 * el error de la calibración del giro (que es por tiempo) se SUMA en vez de cancelarse: con un 10 % de error son 36 grados de rumbo equivocado. */
static int lado_de_vuelta(const Navegador *nav)
{
    return -nav->lado;
}

static int grados_de_vuelta(const Navegador *nav)
{
    return nav->angulo_total - nav->giro_extra_grados;   /* V2: lo que el primer giro llevó de más para deshacer un vistazo no se devuelve */
}

/* Comando de la fase de esquivar actual (ver NAV_FASE_*). Mirar y asentar son STOP. */
static ComandoNav comando_de_fase(const Navegador *nav)
{
    switch (nav->fase) {
    case NAV_FASE_AVANZAR:    return NAV_CMD_FORWARD;
    case NAV_FASE_PARALELO:   return NAV_CMD_FORWARD;
    case NAV_FASE_GIRAR:      return (nav->lado < 0) ? NAV_CMD_LEFT : NAV_CMD_RIGHT;
    case NAV_FASE_VOLVER:     return (lado_de_vuelta(nav) < 0) ? NAV_CMD_LEFT : NAV_CMD_RIGHT;
    case NAV_FASE_RETROCEDER: return NAV_CMD_REVERSE;
    default:                  return NAV_CMD_STOP;   /* MIRAR y ASENTAR */
    }
}

/* Tiempo de retroceso: NAV_RETROCESO_CM cm con la calibración del retroceso, sin pasar nunca de NAV_RETROCESO_MAX_MS. */
static int ms_de_retroceso(const Navegador *nav)
{
    int ms = cal_cm_a_ms_atras(&nav->cal, NAV_RETROCESO_CM);

    return (ms > NAV_RETROCESO_MAX_MS) ? NAV_RETROCESO_MAX_MS : ms;
}

/* Cuánto debe durar cada fase de esquivar, en ms (las de STOP las controla la hora, no esta duración). */
static int duracion_fase(const Navegador *nav, int fase)
{
    if (fase == NAV_FASE_AVANZAR) {
        return cal_cm_a_ms(&nav->cal, (nav->tramo_mm + 5) / 10);   /* el tramo de lado planeado con el desplazamiento de los giros (esq_calcular_tramo) */
    }
    /* Girar va hacia el lado elegido y volver hacia el otro (o el mismo, ver lado_de_vuelta): cada lado tiene su tiempo. */
    if (fase == NAV_FASE_GIRAR) {
        return cal_grados_a_ms(&nav->cal, (nav->lado < 0) ? -nav->angulo_giro : nav->angulo_giro);
    }
    if (fase == NAV_FASE_VOLVER) {
        return cal_grados_a_ms(&nav->cal, (lado_de_vuelta(nav) < 0) ? -grados_de_vuelta(nav) : grados_de_vuelta(nav));
    }
    if (fase == NAV_FASE_RETROCEDER) {
        return ms_de_retroceso(nav);
    }
    if (fase == NAV_FASE_PARALELO) {
        return cal_cm_a_ms(&nav->cal, (nav->paralelo_mm + 5) / 10);
    }
    return 0;
}

static void diag_escribir(Navegador *nav, const char *formato, ...);   /* se define más abajo */

/* Pasa a ERROR. `causa` dice qué pasó en concreto (NAV_CAUSA_*; ver navegador_texto_error). */
static void ir_a_error_causa(Navegador *nav, MotivoErrorNav motivo, int causa)
{
    /* Con «pedir ayuda» activo, no poder esquivar no es un ERROR: se queda quieto y pide que el usuario tome el control. */
    if (nav->pedir_ayuda && (motivo == NAV_MOTIVO_LADOS_BLOQUEADOS || motivo == NAV_MOTIVO_MAX_ESQUIVADAS || motivo == NAV_MOTIVO_ESQUIVANDO)) {
        nav->estado = NAV_AYUDA;
        nav->ayuda_motivo = motivo;
        nav->ayuda_cmd = NAV_CMD_STOP;
        nav->ayuda_t_ini_ms = nav->t_ultimo_ms;
        nav->ayudas++;
        nav->error_causa = causa;          /* navegador_texto_error dice por qué pide ayuda */
        diag_escribir(nav, "AYUDA_DIAG pide ayuda motivo=%d causa=%d pose=(%d,%d,%d) esquivadas=%d del_obstaculo=%d ayudas=%d laser_mm=%d laser_valido=%d", (int)motivo, causa, nav->x_mm,
                      nav->y_mm, nav->rumbo_grados, nav->esquivadas, nav->esquivadas_obstaculo, nav->ayudas, nav->tof_mm, nav->tof_valida);
        return;
    }
    nav->estado = NAV_ERROR;
    nav->motivo_error = motivo;
    nav->error_causa = causa;
}

static void ir_a_error(Navegador *nav, MotivoErrorNav motivo)
{
    int causa = NAV_CAUSA_NINGUNA;

    switch (motivo) {
    case NAV_MOTIVO_LADOS_BLOQUEADOS: causa = NAV_CAUSA_LADOS; break;
    case NAV_MOTIVO_MAX_ESQUIVADAS:   causa = NAV_CAUSA_MAX_ESQUIVADAS; break;
    case NAV_MOTIVO_ESQUIVANDO:       causa = NAV_CAUSA_REINTENTOS; break;
    case NAV_MOTIVO_PLAN:             causa = NAV_CAUSA_PLAN; break;
    case NAV_MOTIVO_SIN_VIDEO:        causa = NAV_CAUSA_SIN_VIDEO; break;
    default: break;
    }
    ir_a_error_causa(nav, motivo, causa);
}

/* Comando de la fase de REINCORPORANDO (ver NAV_RF_*). Las fases de pausa y PLAN son STOP. */
static ComandoNav comando_de_reinc(const Navegador *nav)
{
    switch (nav->reinc_fase) {
    case NAV_RF_GIRO1:   return (nav->reinc_giro1 < 0) ? NAV_CMD_LEFT : NAV_CMD_RIGHT;
    case NAV_RF_AVANZAR: return NAV_CMD_FORWARD;
    case NAV_RF_GIRO2:   return (nav->reinc_giro2 < 0) ? NAV_CMD_LEFT : NAV_CMD_RIGHT;
    case NAV_RF_DESHACER: return NAV_CMD_REVERSE;
    default:             return NAV_CMD_STOP;
    }
}

/* El carro ejecutó `dt` ms del comando anterior: si era el del paso (o fase) actual, se
 * suma al tiempo ya ejecutado de ese paso. Así se cuenta el tiempo REAL, no el planeado. */
static void contar_tiempo_ejecutado(Navegador *nav, int dt)
{
    if (nav->sondeo_fase == 1 && nav->cmd_actual == NAV_CMD_FORWARD) {
        nav->ms_sondeo += dt;   /* el sondeo del arranque avanza NAV_SONDEO_CM cm */
    }
    if (nav->estado == NAV_SIGUIENDO && nav->indice_paso < nav->n_pasos &&
        nav->cmd_actual == comando_de_paso(&nav->pasos[nav->indice_paso])) {
        nav->ms_paso += dt;
    } else if (nav->estado == NAV_ESQUIVANDO && nav->cmd_actual == comando_de_fase(nav)) {
        nav->ms_fase += dt;
    } else if (nav->estado == NAV_REINCORPORANDO && nav->cmd_actual == comando_de_reinc(nav)) {
        nav->ms_fase += dt;
    }
}

/* ── Diagnóstico (cola de líneas de texto) ─────────────────────────────── */

/* Escribe una línea en la cola (como printf). La app las saca con navegador_diag_siguiente(). Si la cola se llena, la línea más vieja se pierde. */
static void diag_escribir(Navegador *nav, const char *formato, ...)
{
    va_list args;
    int pos = nav->diag_escritas % NAV_DIAG_LINEAS;

    va_start(args, formato);
    vsnprintf(nav->diag[pos], NAV_DIAG_LARGO, formato, args);
    va_end(args);
    nav->diag_escritas++;
    if (nav->diag_escritas - nav->diag_leidas > NAV_DIAG_LINEAS) {
        nav->diag_leidas = nav->diag_escritas - NAV_DIAG_LINEAS;
    }
}

int navegador_diag_siguiente(Navegador *nav, char *texto, int capacidad)
{
    int i = 0;
    const char *linea;

    if (nav == NULL || texto == NULL || capacidad < 2 || nav->diag_leidas >= nav->diag_escritas) {
        return 0;
    }
    linea = nav->diag[nav->diag_leidas % NAV_DIAG_LINEAS];
    while (linea[i] != '\0' && i < capacidad - 1) {
        texto[i] = linea[i];
        i++;
    }
    texto[i] = '\0';
    nav->diag_leidas++;
    return 1;
}

/* ── Registro de los comandos de movimiento (RUTA_DIAG y TURN_DIAG) ─────── */

static const char *nombre_origen(int estado)
{
    switch (estado) {
    case NAV_SIGUIENDO:      return "ruta";
    case NAV_ESQUIVANDO:     return "esquive";
    case NAV_REINCORPORANDO: return "vuelta";
    case NAV_AYUDA:          return "ayuda";
    default:                 return "otro";
    }
}

/* Qué pidió el programa y cuánto debe durar el comando `cmd` que empieza ahora, según lo que está haciendo el navegador: grados (giros) o mm (avance y retroceso), y ms.
 * A mano (AYUDA) no se pidió nada: los dos quedan en 0. */
static void plan_del_comando(const Navegador *nav, ComandoNav cmd, int *pedido, int *ms)
{
    *pedido = 0;
    *ms = 0;
    if (nav->estado == NAV_SIGUIENDO && nav->indice_paso < nav->n_pasos && cmd == comando_de_paso(&nav->pasos[nav->indice_paso])) {
        const Paso *paso = &nav->pasos[nav->indice_paso];

        *pedido = (paso->tipo == PASO_AVANZAR) ? paso->valor * 10 : ((paso->valor < 0) ? -paso->valor : paso->valor);
        *ms = paso->tiempo_ms;
    } else if (nav->estado == NAV_ESQUIVANDO && cmd == comando_de_fase(nav)) {
        switch (nav->fase) {
        case NAV_FASE_GIRAR:      *pedido = nav->angulo_giro; break;
        case NAV_FASE_VOLVER:     *pedido = grados_de_vuelta(nav); break;
        case NAV_FASE_AVANZAR:    *pedido = nav->tramo_mm; break;
        case NAV_FASE_RETROCEDER: *pedido = NAV_RETROCESO_CM * 10; break;
        case NAV_FASE_PARALELO:   *pedido = nav->paralelo_mm; break;
        default: break;
        }
        *ms = duracion_fase(nav, nav->fase);
    } else if (nav->estado == NAV_REINCORPORANDO && cmd == comando_de_reinc(nav)) {
        if (nav->reinc_fase == NAV_RF_AVANZAR) {
            *pedido = nav->reinc_dist_mm;
            *ms = cal_cm_a_ms(&nav->cal, (nav->reinc_dist_mm + 5) / 10);
        } else if (nav->reinc_fase == NAV_RF_DESHACER) {
            *pedido = nav->reinc_deshacer_mm;
            *ms = cal_cm_a_ms_atras(&nav->cal, (nav->reinc_deshacer_mm + 5) / 10);
        } else {
            int giro = (nav->reinc_fase == NAV_RF_GIRO1) ? nav->reinc_giro1 : nav->reinc_giro2;

            *pedido = (giro < 0) ? -giro : giro;
            *ms = cal_grados_a_ms(&nav->cal, giro);
        }
    }
}

/* Empieza un comando de movimiento: se anota qué se pidió y cuánto debe durar, y se escribe RUTA_DIAG envia. Los STOP no se anotan (son el fin de un movimiento). */
static void registrar_inicio_comando(Navegador *nav, ComandoNav nuevo)
{
    nav->cmd_pedido = 0;
    nav->cmd_plan_ms = 0;
    nav->cmd_origen = (int)nav->estado;
    if (nuevo == NAV_CMD_STOP) {
        return;
    }
    plan_del_comando(nav, nuevo, &nav->cmd_pedido, &nav->cmd_plan_ms);
    diag_escribir(nav, "RUTA_DIAG envia cmd=%s origen=%s pedido=%d plan_ms=%d t_ms=%lu", navegador_texto_comando(nuevo), nombre_origen(nav->cmd_origen),
                  nav->cmd_pedido, nav->cmd_plan_ms, (unsigned long)nav->t_ultimo_ms);
}

/* Termina un comando de movimiento (la pose ya incluye el frenado): se escribe RUTA_DIAG fin con la duración real y lo hecho (estimado) y, si fue un giro, TURN_DIAG con el
 * desplazamiento del centro que se aplicó, y el giro queda anotado para la pantalla de resultado. */
static void registrar_fin_comando(Navegador *nav)
{
    ComandoNav cmd = nav->cmd_actual;
    int dx = nav->x_mm - nav->base_x_mm, dy = nav->y_mm - nav->base_y_mm;
    int hecho;

    if (cmd == NAV_CMD_STOP || nav->cmd_plan_ms < 0) {
        return;   /* un STOP no se anota; y un registro ya cerrado (cmd_plan_ms = -1) no se cierra dos veces */
    }
    if (cmd == NAV_CMD_FORWARD || cmd == NAV_CMD_REVERSE) {
        diag_escribir(nav, "POSE_DIAG avance origen=%s pose=(%d,%d,%d) rumbo_inicio=%d imu_lista=%d imu_edad_ms=%lu por=%s",
                      nombre_origen(nav->cmd_origen), nav->x_mm, nav->y_mm, nav->rumbo_grados, nav->base_rumbo_grados,
                      nav->sens_angulo_ok, (unsigned long)(nav->t_ultimo_ms - nav->sens_angulo_t_ms),
                      nav->usa_giroscopio && !nav->giro_fw_caido && !nav->sin_rumbo_medido ? "rumbo_medido" : "tiempo");
    }
    if (cmd == NAV_CMD_LEFT || cmd == NAV_CMD_RIGHT) {
        int lado = (cmd == NAV_CMD_LEFT) ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA;
        int um_adelante, um_derecha;
        int h0 = nav->base_rumbo_grados;
        int por_fw = (nav->giro_fw && nav->giro_fw_estado == 1);
        NavGiro giro;

        hecho = por_fw ? grados_girados(nav, lado) : grados_girados(nav, lado) + cal_frenado_grados(&nav->cal, lado);
        cal_leer_desplazamiento(&nav->cal, lado, &um_adelante, &um_derecha);
        giro.lado = lado;
        giro.origen = nav->cmd_origen;
        giro.pedido_grados = nav->cmd_pedido;
        giro.hecho_grados = hecho;
        giro.plan_ms = nav->cmd_plan_ms;
        giro.real_ms = nav->seg_ms;
        giro.desp_adelante_mm = (dx * seno_mil(h0) - dy * seno_mil(h0 + 90)) / 1000;    /* el desplazamiento del mapa, en el marco del carro al empezar el giro */
        giro.desp_derecha_mm = (dx * seno_mil(h0 + 90) + dy * seno_mil(h0)) / 1000;
        giro.por_angulo = por_fw;
        diag_escribir(nav, "TURN_DIAG lado=%s origen=%s pedido=%d hecho=%d plan_ms=%d real_ms=%d um_grado=(%d,%d) mm=(%d,%d) carro_mm=(%d,%d) rumbo=%d>%d%s por=%s fw_real_x10=%d fw_ms=%d ant=%d",
                      (lado < 0) ? "IZQ" : "DER", nombre_origen(giro.origen), giro.pedido_grados, hecho, giro.plan_ms, giro.real_ms, um_adelante, um_derecha,
                      dx, dy, giro.desp_adelante_mm, giro.desp_derecha_mm, h0, nav->rumbo_grados,
                      cal_desplazamiento_es_espejo(&nav->cal, lado) ? " aviso=espejo_estimado" : "", por_fw ? "angulo" : "tiempo", por_fw ? nav->giro_fw_real_x10 : 0, por_fw ? nav->giro_fw_ms : 0, por_fw ? nav->giro_fw_ant : 0);
        if (nav->cmd_pedido > 0) {
            if (por_fw) {
                nav->n_giros_angulo++;
                diag_escribir(nav, "SENS_DIAG giro por=angulo objetivo=%d real_x10=%d error_x10=%d fw_ms=%d origen=%s", nav->cmd_pedido, nav->giro_fw_real_x10,
                              nav->giro_fw_real_x10 - nav->cmd_pedido * 10, nav->giro_fw_ms, nombre_origen(giro.origen));
            } else {
                nav->n_giros_tiempo++;
            }
        }
        if (nav->n_giros < NAV_MAX_GIROS) {
            nav->giros[nav->n_giros++] = giro;
        } else {
            nav->giros_perdidos++;
        }
    } else {
        hecho = (int)raiz_entera((long)dx * dx + (long)dy * dy);   /* mm que se movió (con el frenado) */
    }
    diag_escribir(nav, "RUTA_DIAG fin cmd=%s origen=%s pedido=%d plan_ms=%d real_ms=%d hecho=%d", navegador_texto_comando(cmd), nombre_origen(nav->cmd_origen),
                  nav->cmd_pedido, nav->cmd_plan_ms, nav->seg_ms, hecho);
}

int navegador_cuantos_giros(const Navegador *nav)
{
    return (nav == NULL) ? 0 : nav->n_giros;
}

int navegador_giro_datos(const Navegador *nav, int indice, NavGiro *giro)
{
    if (nav == NULL || giro == NULL || indice < 0 || indice >= nav->n_giros) {
        return NAV_ERR_ARGUMENTO;
    }
    *giro = nav->giros[indice];
    return NAV_OK;
}

int navegador_texto_giros(const Navegador *nav, char *texto, int capacidad)
{
    int i, largo = 0;

    if (nav == NULL || texto == NULL || capacidad < 2) {
        return 0;
    }
    texto[0] = '\0';
    for (i = 0; i < nav->n_giros && largo < capacidad - 1; i++) {
        const NavGiro *g = &nav->giros[i];
        int n;

        if (g->origen == NAV_SIGUIENDO) {
            n = snprintf(texto + largo, (size_t)(capacidad - largo), "%sGiro %s: pedido %d°, hecho ~%d°", (i > 0) ? "\n" : "", (g->lado < 0) ? "izquierda" : "derecha",
                         g->pedido_grados, g->hecho_grados);
        } else {
            n = snprintf(texto + largo, (size_t)(capacidad - largo), "%sGiro %s (%s): pedido %d°, hecho ~%d°", (i > 0) ? "\n" : "", (g->lado < 0) ? "izquierda" : "derecha",
                         nombre_origen(g->origen), g->pedido_grados, g->hecho_grados);
        }
        if (n < 0 || n >= capacidad - largo) {
            largo = capacidad - 1;
            break;
        }
        largo += n;
    }
    if (nav->usa_giroscopio && largo < capacidad - 1) {   /* resumen final: cuántos giros midió el giroscopio y cuántos fueron por tiempo */
        int n = snprintf(texto + largo, (size_t)(capacidad - largo), "%sGiros por ángulo (giroscopio): %d, por tiempo: %d", (largo > 0) ? "\n" : "", nav->n_giros_angulo, nav->n_giros_tiempo);

        if (n > 0 && n < capacidad - largo) {
            largo += n;
        } else {
            largo = capacidad - 1;
        }
    }
    texto[largo] = '\0';
    return largo;
}

/* ── Giro por ángulo y láser (J11) ─────────────────────────────────────── */
/* Pide al firmware el giro que acaba de empezar, si corresponde: solo con el giroscopio encendido y sano, y con un ángulo que el firmware acepta. La app lo manda al ver navegador_tomar_giro_fw. */
static void giro_fw_empezar(Navegador *nav, ComandoNav nuevo)
{
    if (nuevo != NAV_CMD_LEFT && nuevo != NAV_CMD_RIGHT) {
        return;
    }
    if (nav->usa_giroscopio && nav->giro_fw_caido && nav->giro_fw_fallos < NAV_GIRO_FW_FALLOS_MAX && nav->sens_angulo_ok &&
        (uint32_t)(nav->t_ultimo_ms - nav->sens_angulo_t_ms) <= (uint32_t)NAV_TOF_FRESCO_MS) {
        /* V2: un giro por ángulo falló antes, pero SENS dice que el giroscopio está listo ahora: se vuelve a intentar (un giro por tiempo es mucho peor que uno medido). */
        nav->giro_fw_caido = 0;
        diag_escribir(nav, "SENS_DIAG giro reintenta_por_angulo fallos=%d: el giroscopio informa listo", nav->giro_fw_fallos);
    }
    if (!nav->usa_giroscopio || nav->giro_fw_caido) {
        return;
    }
    if (nav->cmd_pedido < nav->giro_fw_min || nav->cmd_pedido > NAV_GIRO_FW_MAX_GRADOS) {
        return;   /* uno menor que el mínimo del firmware no se planea (giro_ejecutable); uno de más de 200 se hace por tiempo */
    }
    nav->giro_fw = 1;
    nav->giro_fw_estado = 0;
    nav->giro_fw_pedir = 1;
    nav->giro_fw_grados = (nuevo == NAV_CMD_RIGHT) ? nav->cmd_pedido : -nav->cmd_pedido;
    nav->giro_fw_real_x10 = 0;
    nav->giro_fw_ms = 0;
    nav->giro_fw_t_ms = nav->t_ultimo_ms;
    nav->giro_fw_consultando = 0;
    nav->giro_fw_consulta_pedir = 0;
    nav->giro_fw_ang_ini_ok = nav->sens_angulo_ok && (uint32_t)(nav->t_ultimo_ms - nav->sens_angulo_t_ms) <= (uint32_t)NAV_TOF_FRESCO_MS;   /* el ángulo del giroscopio justo antes del giro */
    nav->giro_fw_ang_ini_x10 = nav->sens_angulo_x10;
    diag_escribir(nav, "SENS_DIAG giro pide=GIRAR objetivo=%d origen=%s t_ms=%lu", nav->giro_fw_grados, nombre_origen(nav->cmd_origen), (unsigned long)nav->t_ultimo_ms);
}

/* El giro por ángulo no se pudo (no hubo respuesta o el firmware ni lo empezó): este giro se hace por tiempo y los siguientes de la ruta también. El carro no giró (o no se sabe cuánto): se
 * vuelve a partir de la pose de antes del giro. */
static void giro_fw_pasar_a_tiempo(Navegador *nav, const char *motivo)
{
    nav->giro_fw = 0;
    nav->giro_fw_pedir = 0;
    nav->giro_fw_estado = 0;
    nav->giro_fw_caido = 1;
    nav->giro_fw_fallos++;
    nav->x_mm = nav->base_x_mm;
    nav->y_mm = nav->base_y_mm;
    nav->rumbo_grados = nav->base_rumbo_grados;
    nav->rumbo_x10 = nav->base_rumbo_x10;
    fijar_pose(nav);
    if (nav->estado == NAV_SIGUIENDO) {
        nav->ms_paso = 0;   /* solo el contador del movimiento en curso: en ESQUIVANDO o REINCORPORANDO ms_paso es del paso de la ruta que quedó interrumpido y no se toca */
    } else {
        nav->ms_fase = 0;
    }
    nav->giro_fw_consultando = 0;
    nav->giro_visual_x10 = 0;
    nav->visual_cubierto_ms = 0;
    diag_escribir(nav, "SENS_DIAG giro_a_tiempo motivo=%s: este giro y los que siguen de la ruta van por tiempo", motivo);
}

/* El mensaje del giro se perdió: con el ángulo actual del giroscopio se sabe cuánto giró de verdad (en la dirección del comando). Si ya giró al menos NAV_GIRO_FW_YA_GIRO_PCT % de lo pedido, ese es el
 * ángulo real y NO se repite nada; si no, la pose parte de lo que giró y se repite por tiempo SOLO lo que falta. */
static void giro_fw_resolver(Navegador *nav, int angulo_ahora_x10, const char *fuente)
{
    int hecho_x10 = (nav->cmd_actual == NAV_CMD_LEFT) ? angulo_ahora_x10 - nav->giro_fw_ang_ini_x10 : nav->giro_fw_ang_ini_x10 - angulo_ahora_x10;
    int pedido_x10 = nav->cmd_pedido * 10;

    nav->giro_fw_consultando = 0;
    nav->giro_fw_consulta_pedir = 0;
    if (hecho_x10 * 100 >= pedido_x10 * NAV_GIRO_FW_YA_GIRO_PCT) {
        nav->giro_fw_real_x10 = hecho_x10;      /* ya giró: lo medido es el ángulo real */
        nav->giro_fw_ms = 0;
        nav->giro_fw_estado = 1;
        diag_escribir(nav, "SENS_DIAG giro mensaje_perdido fuente=%s real_x10=%d pedido_x10=%d: ya giro al menos el %d %%, NO se repite", fuente, hecho_x10, pedido_x10, NAV_GIRO_FW_YA_GIRO_PCT);
        return;
    }
    {   /* giró solo una parte: la pose parte de ahí y por tiempo se hace lo que falta */
        int lado = (nav->cmd_actual == NAV_CMD_LEFT) ? -1 : 1;
        int total_ms = cal_grados_a_ms(&nav->cal, lado * nav->cmd_pedido);
        int falta_ms;
        int parcial = (hecho_x10 > 0) ? hecho_x10 : 0;

        nav->giro_fw_real_x10 = parcial;
        nav->giro_fw_estado = 1;
        calcular_pose(nav);                      /* la pose después de lo que ya giró */
        fijar_pose(nav);
        nav->giro_fw = 0;
        nav->giro_fw_estado = 0;
        nav->giro_fw_pedir = 0;
        nav->giro_fw_caido = 1;                  /* lo que falta de este giro va por tiempo (el siguiente se reintenta si SENS dice que el giroscopio está listo) */
        nav->giro_fw_fallos++;
        falta_ms = cal_grados_a_ms(&nav->cal, lado * ((pedido_x10 - parcial) / 10));
        {
            int hecho_ms = total_ms - falta_ms;

            if (hecho_ms < 0) {
                hecho_ms = 0;
            }
            if (nav->estado == NAV_SIGUIENDO) {
                nav->ms_paso = hecho_ms;         /* el paso termina cuando se cumple su tiempo total: queda solo lo que falta */
            } else {
                nav->ms_fase = hecho_ms;
            }
        }
        nav->giro_visual_x10 = 0;
        nav->visual_cubierto_ms = 0;
        diag_escribir(nav, "SENS_DIAG giro mensaje_perdido fuente=%s real_x10=%d pedido_x10=%d: giro menos del %d %%, se repite por tiempo SOLO lo que falta (%d ms)", fuente, hecho_x10, pedido_x10,
                      NAV_GIRO_FW_YA_GIRO_PCT, falta_ms);
    }
}

/* Si el firmware no contesta en su tiempo máximo más el margen, antes de repetir el giro por tiempo se pregunta cuánto giró (IMU?). */
static void giro_fw_vigilar(Navegador *nav, uint32_t ahora_ms)
{
    int grados = (nav->giro_fw_grados < 0) ? -nav->giro_fw_grados : nav->giro_fw_grados;
    uint32_t espera_max = (uint32_t)(NAV_GIRO_FW_BASE_MS + NAV_GIRO_FW_POR_GRADO_MS * grados + NAV_GIRO_FW_ESPERA_EXTRA_MS);

    if (!nav->giro_fw || nav->giro_fw_estado != 0) {
        return;
    }
    if (!nav->giro_fw_consultando) {
        if ((uint32_t)(ahora_ms - nav->giro_fw_t_ms) > espera_max) {
            if (!nav->giro_fw_ang_ini_ok) {   /* sin el ángulo del inicio no hay con qué comparar: se repite por tiempo como antes */
                giro_fw_pasar_a_tiempo(nav, "sin_respuesta_ni_angulo_inicial");
                return;
            }
            nav->giro_fw_consultando = 1;
            nav->giro_fw_consulta_pedir = 1;
            nav->giro_fw_consulta_t_ms = ahora_ms;
            diag_escribir(nav, "SENS_DIAG giro sin_GIRO_OK: se pregunta IMU? antes de repetirlo (angulo al inicio=%d)", nav->giro_fw_ang_ini_x10);
        }
        return;
    }
    if ((uint32_t)(ahora_ms - nav->giro_fw_consulta_t_ms) > (uint32_t)NAV_GIRO_FW_CONSULTA_MS) {
        if (nav->sens_angulo_ok && (uint32_t)(ahora_ms - nav->sens_angulo_t_ms) <= (uint32_t)NAV_TOF_FRESCO_MS) {
            giro_fw_resolver(nav, nav->sens_angulo_x10, "ultimo_SENS");
        } else {
            giro_fw_pasar_a_tiempo(nav, "sin_respuesta_ni_IMU");
        }
    }
}

/* El láser dice «libre» (varias lecturas seguidas y recientes) o «cerca» (una medida real menor de NAV_TOF_CONFIRMA_MM varias veces seguidas). */
static int tof_fresco(const Navegador *nav, uint32_t ahora_ms)
{
    return nav->usa_tof && nav->tof_vivo && (uint32_t)(ahora_ms - nav->tof_t_ms) <= (uint32_t)NAV_TOF_FRESCO_MS;
}

static int tof_dice_libre(const Navegador *nav, uint32_t ahora_ms)
{
    return tof_fresco(nav, ahora_ms) && nav->tof_libre_lecturas >= NAV_TOF_LIBRE_LECTURAS;
}

static int tof_dice_cerca(const Navegador *nav, uint32_t ahora_ms)
{
    return tof_fresco(nav, ahora_ms) && nav->tof_valida && nav->tof_mm > 0 &&
           nav->tof_mm <= NAV_REACCION_MAX_MM && nav->tof_cerca_lecturas >= NAV_TOF_LIBRE_LECTURAS;
}

static void extender_rebase(Navegador *nav, uint32_t ahora_ms);
static void entrar_detenido(Navegador *nav, uint32_t ahora_ms);

/* ── V2: rodeo medido con el láser ─────────────────────────────────────── */

static int valor_absoluto_int_v2(int x)
{
    return (x < 0) ? -x : x;
}

/* El rumbo del carro AHORA en décimas: durante un giro por ángulo el navegador solo conoce el final, pero SENS trae el ángulo del giroscopio cada 100 ms. */
static int rumbo_instantaneo_x10(const Navegador *nav, uint32_t ahora_ms)
{
    if (nav->giro_fw && nav->giro_fw_estado == 0 && nav->giro_fw_ang_ini_ok && nav->sens_angulo_ok &&
        (uint32_t)(ahora_ms - nav->sens_angulo_t_ms) <= (uint32_t)NAV_TOF_FRESCO_MS) {
        int girado = diferencia_x10(nav->giro_fw_ang_ini_x10, nav->sens_angulo_x10);   /* + izquierda, como el firmware */

        /* La distancia de SENS es la mediana de las 3 últimas lecturas del láser (una cada 33 ms): es unos 50 ms más vieja que el ángulo del mismo mensaje. Se usa el
         * ángulo de hace medio periodo de SENS (a 130 grados por segundo son ~6 grados). */
        girado -= diferencia_x10(nav->sens_angulo_previo_x10, nav->sens_angulo_x10) / 2;
        return nav->base_rumbo_x10 - girado;   /* el navegador cuenta + a la derecha */
    }
    return nav->rumbo_x10;
}

/* Dónde pega el láser (a `mm` del sensor) respecto de la línea de la ruta en el punto de salida del rodeo: `s` a lo largo de la ruta desde la salida y `lado` hacia el
 * lado por el que se rodea (negativo = del otro lado de la línea). */
static void laser_punto(const Navegador *nav, uint32_t ahora_ms, int mm, long *s, long *lado)
{
    int rumbo = rumbo_instantaneo_x10(nav, ahora_ms);
    long alcance = (long)NAV_SENSOR_FRENTE_MM + mm;
    long hx = nav->x_mm + dir_x_mil_x10(rumbo) * alcance / 1000 - nav->salida_x_mm;
    long hy = nav->y_mm + dir_y_mil_x10(rumbo) * alcance / 1000 - nav->salida_y_mm;
    int costado = nav->salida_rumbo + nav->lado * (nav->scan_signo < 0 ? -90 : 90);

    *s = (hx * dir_x_mil(nav->salida_rumbo) + hy * dir_y_mil(nav->salida_rumbo)) / 1000;
    *lado = (hx * dir_x_mil(costado) + hy * dir_y_mil(costado)) / 1000;
}

/* Distancia (mm, desde el centro del carro en la salida) a la cara del obstáculo que detuvo al carro. */
static int cara_desde_salida_mm(const Navegador *nav)
{
    int cara = nav->tof_dist_mm > 0 ? nav->tof_dist_mm : (nav->salida_cercania >= 3 ? NAV_CARA_CERCANIA_3_MM : NAV_CARA_CERCANIA_2_MM);

    return NAV_SENSOR_FRENTE_MM + cara;
}

/* Una lectura válida del láser durante el PRIMER giro del rodeo: si pega en la zona de la cara del obstáculo, dice hasta dónde llega hacia el lado del rodeo; si pega
 * antes de esa zona y bien de lado, hay algo (una pared) de ese lado. */
static void escaneo_muestra(Navegador *nav, uint32_t ahora_ms, int mm, int eco)
{
    long s = 0, lado = 0;
    int rumbo = rumbo_instantaneo_x10(nav, ahora_ms);
    int k = nav->scan_n;

    if (k >= NAV_ESCANEO_MAX) {
        return;
    }
    if (eco) {
        laser_punto(nav, ahora_ms, mm, &s, &lado);
        nav->scan_muestras++;
    }
    nav->scan_s[k] = (int)s;
    nav->scan_lado[k] = (int)lado;
    nav->scan_eco[k] = eco;
    nav->scan_grados[k] = valor_absoluto_int_v2(diferencia_x10(nav->salida_rumbo * 10, rumbo)) / 10;   /* cuánto se apartó el láser del frente */
    nav->scan_sensor_s[k] = (int)((((long)(nav->x_mm - nav->salida_x_mm) * dir_x_mil(nav->salida_rumbo) + (long)(nav->y_mm - nav->salida_y_mm) * dir_y_mil(nav->salida_rumbo)) +
                                   (long)NAV_SENSOR_FRENTE_MM * seno_mil_x10(rumbo - nav->salida_rumbo * 10 + 900)) / 1000);
    nav->scan_n++;
}

/* Al terminar el giro: de las lecturas, desde el frente hacia el costado, saca hasta dónde llega la cara del obstáculo (scan_ext_mm), si hay otra cosa de ese lado
 * (scan_pared_mm) y si el final de la cara NO se pudo comprobar (scan_corte). La cara es lo que se ve SEGUIDO desde el frente: sin huecos ni saltos de lado de más de
 * NAV_ESCANEO_SALTO_MM. Su final solo es de fiar si en la lectura siguiente el láser todavía habría alcanzado a verla (una pared se sale del alcance viéndose). */
static void escaneo_analizar(Navegador *nav, int desde_el_costado)
{
    int cara = cara_desde_salida_mm(nav);
    int i, vista = 0, terminada = 0, ultimo_grados = 0;

    nav->scan_ext_mm = nav->scan_pared_mm = nav->scan_corte = 0;
    for (i = 0; i < nav->scan_n; i++) {
        int k = desde_el_costado ? nav->scan_n - 1 - i : i;
        int en_cara = nav->scan_eco[k] && nav->scan_s[k] >= cara - 150 && nav->scan_s[k] <= cara + 400 && nav->scan_lado[k] <= nav->scan_ext_mm + NAV_ESCANEO_SALTO_MM;

        if (!terminada && en_cara) {
            vista = 1;
            ultimo_grados = nav->scan_grados[k];
            if (nav->scan_lado[k] > nav->scan_ext_mm) {
                nav->scan_ext_mm = nav->scan_lado[k];
            }
            continue;
        }
        if (vista && !terminada) {
            /* Primera lectura que ya no es de la cara: ¿el láser la habría visto si siguiera? Distancia a su plano con este ángulo MÁS una holgura (el ángulo de la
             * lectura tiene unos grados de error y un plano visto muy de lado devuelve poca señal: una pared deja de dar eco bastante antes del alcance nominal). */
            int g = nav->scan_grados[k] + NAV_ESCANEO_HOLGURA_GRADOS > 85 ? 85 : nav->scan_grados[k] + NAV_ESCANEO_HOLGURA_GRADOS;
            long esperado = (long)(cara - nav->scan_sensor_s[k]) * 1000 / (seno_mil(90 - g) > 50 ? seno_mil(90 - g) : 50);

            terminada = 1;
            if (esperado > NAV_ESCANEO_ALCANCE_UTIL_MM) {
                nav->scan_corte = 1;
            }
        }
        if (nav->scan_eco[k] && nav->scan_lado[k] > 250 && (nav->scan_pared_mm == 0 || nav->scan_lado[k] < nav->scan_pared_mm)) {
            nav->scan_pared_mm = nav->scan_lado[k];
        }
    }
    if (vista && !terminada && ultimo_grados < 80) {
        nav->scan_corte = 1;   /* el giro terminó viendo todavía la cara */
    }
    {   /* Las lecturas del barrido al registro (y a la caja negra): grados del frente, si hubo eco, y dónde pegó (a lo largo, de lado). */
        char texto[NAV_DIAG_LARGO - 40];
        int largo = 0;

        texto[0] = '\0';
        for (i = 0; i < nav->scan_n && largo < (int)sizeof texto - 24; i++) {
            int n = nav->scan_eco[i] ? snprintf(texto + largo, sizeof texto - (size_t)largo, " %d:%d,%d", nav->scan_grados[i], nav->scan_s[i], nav->scan_lado[i])
                                      : snprintf(texto + largo, sizeof texto - (size_t)largo, " %d:-", nav->scan_grados[i]);

            if (n < 0) {
                break;
            }
            largo += n;
        }
        diag_escribir(nav, "BARRIDO_DIAG cara=%d lecturas(grados:s,lado)=%s", cara, texto);
    }
}

/* Cuánto hay que avanzar en paralelo, desde un punto donde el costado del objeto todavía podía estar a la vista, para que el giro de regreso ya no lo alcance: a `c` mm
 * de lado la cámara solo ve el piso desde c / tan(FOV / 2) adelante de la lente. */
static int pista_tras_la_vista_mm(const Navegador *nav)
{
    int c = nav->lateral_objetivo_mm - nav->ancho_rodeo_mm;
    long tangente = (long)seno_mil(NAV_CAMARA_FOV_GRADOS / 2) * 1000 / seno_mil(NAV_CAMARA_FOV_GRADOS / 2 + 90);

    if (c < 150) {
        c = 150;
    }
    return (int)((long)c * 1000 / tangente) + NAV_CAMARA_ADELANTE_MM + NAV_PASO_VISTA_MARGEN_MM;
}

/* Una lectura sin eco no demuestra espacio libre. Esperar parado y con el
 * rumbo final evita reutilizar el haz que miraba al costado durante el giro. */
static int verificar_frente(Navegador *nav, uint32_t ahora_ms, int causa)
{
    if (!nav->verificando_frente) {
        nav->verificando_frente = 1;
        nav->verificar_frente_ms = ahora_ms;
        nav->tof_libre_lecturas = 0;
        nav->tof_despejado_lecturas = 0;
        nav->tof_cerca_lecturas = 0;
        nav->vision_sin_coincidencia = 0;
    }
    if (ahora_ms - nav->verificar_frente_ms < NAV_PAUSA_ASENTAR_MS) {
        nav->tof_libre_lecturas = 0;
        nav->tof_despejado_lecturas = 0;
        nav->vision_sin_coincidencia = 0;
        return 0;
    }
    /* V2: para cerrar un rodeo basta que el láser mida, con tres lecturas nuevas, que no hay nada a NAV_FIN_HOLGURA_MM o menos (antes exigía más de 800 mm: una pared o
     * un segundo obstáculo a 60 cm hacían «esquivar» algo que no estorba). Sin eco sigue sin contar como libre: ahí deciden cinco cuadros nuevos sin coincidencia. */
    if ((tof_fresco(nav, ahora_ms) && nav->tof_despejado_lecturas >= NAV_TOF_LIBRE_LECTURAS && (!nav->usa_camara || nav->vision_sin_coincidencia > 0)) ||
        ((!nav->usa_tof || (tof_fresco(nav, ahora_ms) && (!nav->tof_valida || nav->tof_mm <= 0))) &&
         nav->usa_camara && ahora_ms - nav->t_ultimo_cuadro_ms <= NAV_MAX_HUECO_VIDEO_MS &&
         nav->vision_sin_coincidencia >= NAV_VISION_LIBRE_CUADROS)) {
        nav->verificando_frente = 0;
        return 1;
    }
    /* V2: tres lecturas nuevas con algo CERCA y delante, ya sobre la línea y con el avance del rodeo cumplido: no es el objeto que se rodeó (ese quedó atrás por la
     * cuenta de distancia) sino otro. El rodeo se cierra y la ruta lo trata como obstáculo nuevo al retomarla (frena de inmediato: sigue confirmado por el láser). */
    if (causa == NAV_CAUSA_OBSTACULO_NO_PASADO && tof_dice_cerca(nav, ahora_ms)) {
        nav->verificando_frente = 0;
        diag_escribir(nav, "BORDE_DIAG obstaculo_nuevo_delante laser_mm=%d: se cierra el rodeo anterior", nav->tof_mm);
        return 1;
    }
    if (ahora_ms - nav->verificar_frente_ms >= NAV_VERIFICAR_FIN_MS) {
        nav->verificando_frente = nav->fin_pendiente = 0;
        if (nav->esquivadas_obstaculo >= NAV_MAX_ESQUIVADAS || nav->esquivadas >= NAV_MAX_ESQUIVADAS_RUTA) {
            ir_a_error_causa(nav, NAV_MOTIVO_MAX_ESQUIVADAS, NAV_CAUSA_MAX_ESQUIVADAS);
        } else if (nav->obstaculo_pendiente && causa == NAV_CAUSA_OBSTACULO_NO_PASADO) {
            extender_rebase(nav, ahora_ms);
        } else {
            nav->cierre_reintento = 1;
            entrar_detenido(nav, ahora_ms);
        }
    }
    return 0;
}

/* TOF_PARO: el obstáculo está al frente a `mm` mm, de cercanía 3, y no se sabe nada de los lados. */
static void tof_marcar(Navegador *nav, int mm)
{
    nav->tof_paro_activo = 1;
    nav->tof_dist_mm = mm;
    nav->zona_hay[ZONA_CENTRO] = 1;
    nav->zona_frena[ZONA_CENTRO] = 1;
    nav->zona_cerc_frena[ZONA_CENTRO] = 3;
    nav->zona_cercania[ZONA_CENTRO] = 3;
}
/* ── La línea de la ruta (para volver a ella) ──────────────────────────── */

/* La ruta como línea quebrada: el tramo k va del punto k al punto k+1 (en mm). */
static long punto_ruta_x(const Navegador *nav, int i)
{
    return (long)nav->ruta[i].columna * MM_POR_CELDA;
}

static long punto_ruta_y(const Navegador *nav, int i)
{
    return (long)nav->ruta[i].fila * MM_POR_CELDA;
}

static long largo_tramo(const Navegador *nav, int k)
{
    long dx = punto_ruta_x(nav, k + 1) - punto_ruta_x(nav, k);
    long dy = punto_ruta_y(nav, k + 1) - punto_ruta_y(nav, k);

    return raiz_entera(dx * dx + dy * dy);
}

/* Rumbo de la línea en el tramo k (0, 45, 90... según la ruta). */
static int rumbo_del_tramo(const Navegador *nav, int k)
{
    return rumbo_del_vector(punto_ruta_x(nav, k + 1) - punto_ruta_x(nav, k), punto_ruta_y(nav, k + 1) - punto_ruta_y(nav, k), 0);
}

/* Distancia recorrida a lo largo de la ruta hasta el comienzo del tramo k. */
static long distancia_hasta_tramo(const Navegador *nav, int k)
{
    long total = 0;
    int i;

    for (i = 0; i < k; i++) {
        total += largo_tramo(nav, i);
    }
    return total;
}

/* Dónde cae el punto (x, y) sobre la ruta: el tramo más cercano de los vecinos al tramo actual (el carro va del punto indice_objetivo-1 al
 * indice_objetivo), cuánto se ha avanzado a lo largo de la ruta (`s_mm`) y a cuánto está de la línea (`desvio_mm`). Con un solo punto no hay línea. */
static void ubicar_en_linea(const Navegador *nav, long x, long y, int *tramo, long *s_mm, long *desvio_mm)
{
    int k0 = (nav->tramo_base > nav->indice_objetivo - 1) ? nav->tramo_base : nav->indice_objetivo - 1;
    int desde = k0 - 1, hasta = k0 + NAV_VENTANA_TRAMOS, k;
    long mejor_desvio = -1;

    if (desde < 0) {
        desde = 0;
    }
    if (hasta > nav->n_puntos - 2) {
        hasta = nav->n_puntos - 2;
    }
    if (desde > hasta) {
        desde = hasta;
    }
    *tramo = (hasta < 0) ? 0 : desde;
    *s_mm = 0;
    *desvio_mm = 0;
    for (k = desde; k <= hasta; k++) {
        long ax = punto_ruta_x(nav, k), ay = punto_ruta_y(nav, k);
        long vx = punto_ruta_x(nav, k + 1) - ax, vy = punto_ruta_y(nav, k + 1) - ay;
        long largo = raiz_entera(vx * vx + vy * vy);
        long t, cx, cy, dx, dy, desvio;

        if (largo <= 0) {
            continue;
        }
        t = ((x - ax) * vx + (y - ay) * vy) / largo;     /* cuánto a lo largo del tramo cae el punto */
        t = (t < 0) ? 0 : ((t > largo) ? largo : t);
        cx = ax + vx * t / largo;
        cy = ay + vy * t / largo;
        dx = x - cx;
        dy = y - cy;
        desvio = raiz_entera(dx * dx + dy * dy);
        if (mejor_desvio < 0 || desvio < mejor_desvio) {
            mejor_desvio = desvio;
            *tramo = k;
            *s_mm = distancia_hasta_tramo(nav, k) + t;
            *desvio_mm = desvio;
        }
    }
}

/* El punto de la ruta a `s_mm` de distancia desde su inicio (si se pasa del final, el último punto). Devuelve en *tramo el tramo donde cae. */
static void punto_en_linea(const Navegador *nav, long s_mm, long *x, long *y, int *tramo)
{
    int k;
    long resto = s_mm;

    for (k = 0; k < nav->n_puntos - 1; k++) {
        long largo = largo_tramo(nav, k);

        if (resto <= largo || k == nav->n_puntos - 2) {
            long ax = punto_ruta_x(nav, k), ay = punto_ruta_y(nav, k);

            if (resto > largo) {
                resto = largo;
            }
            *x = (largo > 0) ? ax + (punto_ruta_x(nav, k + 1) - ax) * resto / largo : ax;
            *y = (largo > 0) ? ay + (punto_ruta_y(nav, k + 1) - ay) * resto / largo : ay;
            *tramo = k;
            return;
        }
        resto -= largo;
    }
    *x = punto_ruta_x(nav, 0);
    *y = punto_ruta_y(nav, 0);
    *tramo = 0;
}

/* ── Planificación ─────────────────────────────────────────────────────── */

/* El navegador planifica de un punto de la ruta al siguiente. Si el carro YA va avanzando y el tramo
 * nuevo sigue en línea recta, no hay que esperar otro arranque; y si el tramo siguiente también sigue
 * recto, el carro no frena aquí (no se descuenta el frenado). Esta función corrige los tiempos de los
 * pasos AVANZAR de ese tramo con cal_cm_a_ms_tramo. `camino` son las celdas del tramo. */
static void ajustar_tramos_continuos(Navegador *nav, const Punto *camino, int n)
{
    int continua = (nav->cmd_actual == NAV_CMD_FORWARD && nav->n_pasos > 0 && nav->pasos[0].tipo == PASO_AVANZAR);
    int sigue_recto = 0;
    int i, ultimo = nav->n_pasos - 1;

    if (n >= 2 && nav->indice_objetivo + 1 < nav->n_puntos) {
        const Punto *a = &nav->ruta[nav->indice_objetivo];
        const Punto *b = &nav->ruta[nav->indice_objetivo + 1];

        sigue_recto = (signo(camino[n - 1].columna - camino[n - 2].columna) == signo(b->columna - a->columna) &&
                       signo(camino[n - 1].fila - camino[n - 2].fila) == signo(b->fila - a->fila));
    }
    for (i = 0; i < nav->n_pasos; i++) {
        if (nav->pasos[i].tipo == PASO_AVANZAR) {
            nav->pasos[i].tiempo_ms = cal_cm_a_ms_tramo(&nav->cal, nav->pasos[i].valor,
                                                        !(i == 0 && continua), !(i == ultimo && sigue_recto));
        }
    }
    if (continua) {
        nav->seg_ms = nav->cal.arranque_ms;   /* el arranque ya pasó: el tiempo cuenta como movimiento */
    }
}

#define NAV_LAT_TOL_MM              25   /* si tras el giro paralelo al tramo el carro queda a menos de esto de la recta, se usa ese giro de siempre */
#define NAV_LAT_PLAN_MAX_MM        (2 * MM_POR_CELDA)   /* distancia máxima a la recta con la que todavía se planea apuntando (un giro de 180° con pivote de 20 cm desplaza hasta 40 cm) */
#define NAV_MAX_CORR_RUMBO_VERTICE   2   /* tope de giros de corrección de rumbo (más de 4°) por vértice de la ruta */
#define NAV_TRAMO_APUNTAR_CELDAS     3   /* solo los tramos rectos de al menos 3 celdas (60 cm) se planean apuntando: en uno más corto un giro de pivote no se puede compensar de lado */

/* Con el giro `giro` (grados, con signo; de 4° o menos no se gira) desde la pose estimada: a cuántos mm del punto (tx, ty) pasaría el carro si luego avanza recto. *ok = 1 si el punto
 * queda por delante. */
static long lateral_apuntando(const Navegador *nav, int giro, long tx, long ty, int *ok)
{
    PoseCarro p;
    long ux, uy, a_lo_largo, lateral;

    p.x_mm = nav->x_mm;
    p.y_mm = nav->y_mm;
    p.rumbo_grados = nav->rumbo_grados;
    if (giro < -4 || giro > 4) {
        cal_pose_tras_giro(&nav->cal, &p, (giro < 0) ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA, (giro < 0) ? -giro : giro, &p);
    }
    ux = dir_x_mil(p.rumbo_grados);
    uy = dir_y_mil(p.rumbo_grados);
    a_lo_largo = ((tx - p.x_mm) * ux + (ty - p.y_mm) * uy) / 1000;
    lateral = valor_absoluto_largo(((tx - p.x_mm) * (-uy) + (ty - p.y_mm) * ux) / 1000);
    *ok = (a_lo_largo > 0);
    return lateral;
}

/* Calcula los pasos hacia el punto de la ruta al que se dirige (indice_objetivo), sin sobrescribir el rumbo real (si difiere más de 4° del tramo, lo corrige con un giro)
 * y compensando el desplazamiento del giro de pivote en los tramos de toda la ruta. */
static int planificar(Navegador *nav)
{
    Punto camino[MAX_PUNTOS];
    const Punto *meta = &nav->ruta[nav->indice_objetivo];
    int orientacion, columna, fila, n, indice_error;
    int k = nav->indice_objetivo;

    if (nav->cmd_actual != NAV_CMD_STOP) {
        registrar_fin_comando(nav);   /* el comando sigue (avanzar al siguiente punto sin parar), pero se cierra el registro del tramo que terminó */
    }
    fijar_pose(nav);
    nav->frenado_pendiente = 0;

    nav->paso_en_linea = 0;
    if (nav->override_largo_mm > 0 && nav->indice_objetivo > 1) {
        /* Recta con largo fijo (J14). J16: antes se terminaba en CUALQUIER plan con índice mayor que 1, también al volver de esquivar a mitad de la recta (terminaba
         * sin haber llegado). Ahora solo si ya está a NAV_OVERRIDE_TOL_MM o menos del largo pedido, medido a lo largo de la ruta; si no, sigue (el avance se recorta
         * más abajo para no pasarse). */
        int tramo_ov;
        long s_ov, desvio_ov;

        ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo_ov, &s_ov, &desvio_ov);
        if (nav->cmd_actual == NAV_CMD_FORWARD) {
            s_ov += nav->cal.frenado_mm;   /* el avance todavía no frenó: lo que siga al soltarlo ya cuenta */
        }
        if (s_ov + NAV_OVERRIDE_TOL_MM >= nav->override_largo_mm) {
            nav->n_pasos = 0;
            nav->indice_objetivo = nav->n_puntos;
            nav->estado = NAV_TERMINADO;
            return NAV_OK;
        }
        while (nav->indice_objetivo < nav->n_puntos - 1 && distancia_hasta_tramo(nav, nav->indice_objetivo) <= s_ov) {
            nav->indice_objetivo++;   /* nunca se planea hacia un punto de la recta que ya quedó atrás */
        }
        k = nav->indice_objetivo;
        meta = &nav->ruta[k];
    }

    if (!nav->sin_compensar_giro && k >= 1 && k < nav->n_puntos) {
        int rumbo_tramo = rumbo_del_tramo(nav, k - 1);
        int u1x = dir_x_mil(rumbo_tramo), u1y = dir_y_mil(rumbo_tramo);
        long kx = punto_ruta_x(nav, k), ky = punto_ruta_y(nav, k);
        long dist_actual = ((kx - nav->x_mm) * (long)u1x + (ky - nav->y_mm) * (long)u1y) / 1000;
        long lat_actual = valor_absoluto_largo(((kx - nav->x_mm) * (long)(-u1y) + (ky - nav->y_mm) * (long)u1x) / 1000);
        int celdas_del_tramo = 1, j_tramo;   /* celdas del tramo recto completo que contiene este paso */
        int limite_lateral;

        for (j_tramo = k - 1; j_tramo >= 1 && rumbo_del_tramo(nav, j_tramo - 1) == rumbo_tramo; j_tramo--) {
            celdas_del_tramo++;
        }
        for (j_tramo = k; j_tramo + 1 < nav->n_puntos && rumbo_del_tramo(nav, j_tramo) == rumbo_tramo; j_tramo++) {
            celdas_del_tramo++;
        }
        limite_lateral = (celdas_del_tramo >= NAV_TRAMO_APUNTAR_CELDAS) ? NAV_LAT_PLAN_MAX_MM : MM_POR_CELDA;   /* los tramos cortos siguen con el límite de siempre */

        if (dist_actual >= -MM_POR_CELDA / 2 && lat_actual <= limite_lateral) {
            int k_esq = k;
            long d_antes = 0;
            int giro = giro_minimo(nav->rumbo_grados, rumbo_tramo);
            int apunta = 0, uhx = u1x, uhy = u1y;
            PoseCarro p_ini;
            long ex, ey, dist_esq, dist_k, avance_mm, tx, ty;
            int cm, continua, sigue_recto, hasta_el_largo = 0;

            while (k_esq + 1 < nav->n_puntos && rumbo_del_tramo(nav, k_esq) == rumbo_tramo) {
                k_esq++;
            }
            if (k_esq + 1 < nav->n_puntos) {
                int rumbo_sig = rumbo_del_tramo(nav, k_esq);
                int u2x = dir_x_mil(rumbo_sig), u2y = dir_y_mil(rumbo_sig);
                int giro_esq = giro_minimo(rumbo_tramo, rumbo_sig);
                int lado_esq = (giro_esq < 0) ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA;
                int grados_esq = (giro_esq < 0) ? -giro_esq : giro_esq;
                int dx_esq = 0, dy_esq = 0;
                long det = (long)u1x * u2y - (long)u1y * u2x;

                cal_desplazamiento_giro(&nav->cal, lado_esq, grados_esq, rumbo_tramo, &dx_esq, &dy_esq, NULL, NULL);
                if (det != 0) {
                    d_antes = ((long)dx_esq * u2y - (long)dy_esq * u2x) * 1000 / det;
                }
            }
            ex = punto_ruta_x(nav, k_esq);
            ey = punto_ruta_y(nav, k_esq);
            tx = ex - d_antes * (long)u1x / 1000;   /* el punto de la recta donde hay que parar para que el giro de la esquina deje al carro en el vértice */
            ty = ey - d_antes * (long)u1y / 1000;
            /* Un giro de pivote desplaza el centro del carro hacia un lado (hasta 2 x el radio en un giro de 180°). Si tras el giro "paralelo al tramo" el carro queda fuera de la recta, avanzar en paralelo
             * nunca lo corrige: se elige el giro que deja al carro APUNTANDO al punto de parada (entre 6 y 20 cm de radio, esto era el peor error de las rutas en L). Con el carro sobre la recta
             * (lo de siempre: rectas y esquives) el giro es el de antes, sin cambios. */
            {
                int lat_ok = 0;
                long lat_nominal = lateral_apuntando(nav, giro, tx, ty, &lat_ok);

                /* Solo se apunta tras un giro grande de verdad (45° o más: ahí el desplazamiento de pivote es de decenas de cm) o mientras se sigue en el tramo que ya se venía apuntando. Un empujón o
                 * una vuelta de esquive con el rumbo casi igual al del tramo no entran aquí: se planean como siempre. */
                int giro_grande = (giro >= 45 || giro <= -45);

                if (celdas_del_tramo >= NAV_TRAMO_APUNTAR_CELDAS && (giro_grande || nav->apunte_k_esq == k_esq) && (!lat_ok || lat_nominal > NAV_LAT_TOL_MM)) {   /* en un tramo corto no hay espacio para corregir de lado: se queda como antes */
                    int g, mejor_g = giro, hay = 0;
                    long mejor_cubo = 0;

                    for (g = -180; g <= 180; g++) {
                        int ok = 0;
                        long lat = lateral_apuntando(nav, g, tx, ty, &ok);
                        long cubo = lat / 10;
                        int ag = (g < 0) ? -g : g;

                        if (!ok) {
                            continue;
                        }
                        if (!hay || cubo < mejor_cubo || (cubo == mejor_cubo && ag < ((mejor_g < 0) ? -mejor_g : mejor_g))) {
                            hay = 1;
                            mejor_cubo = cubo;
                            mejor_g = g;
                        }
                    }
                    if (hay && lat_nominal / 10 > mejor_cubo) {
                        giro = mejor_g;
                        apunta = 1;
                        nav->apunte_k_esq = k_esq;
                    }
                }
            }
            /* J16: con el firmware sosteniendo el rumbo (RUMBO), un error de rumbo de hasta NAV_RUMBO_FW_SIN_GIRO grados se corrige AVANZANDO (rumbo_fw_actualizar):
             * no hace falta el giro parado, que además mueve el centro por el pivote. Los giros de la ruta (45° o más) no cambian. */
            {
                /* V2: en pleno vistazo (o recién terminado) el carro va torcido a propósito: eso lo deshace el mismo control de rumbo, no un giro parado. */
                int sin_giro = NAV_RUMBO_FW_SIN_GIRO;

                if (nav->vistazo_lado != 0 || (nav->vistazo_alguno && (uint32_t)(nav->t_ultimo_ms - nav->vistazo_fin_ms) < (uint32_t)NAV_VISTAZO_VUELTA_MS)) {
                    sin_giro = NAV_VISTAZO_GRADOS_MAX + 8;
                }
                if (rumbo_fw_activo(nav) && !apunta && giro >= -sin_giro && giro <= sin_giro) {
                    giro = 0;
                }
            }
            /* Tope de NAV_MAX_CORR_RUMBO_VERTICE giros de corrección (más de 4°) por vértice: la corrección no puede entrar en bucle aunque se vuelva a planear el mismo vértice. */
            if (giro < -4 || giro > 4) {
                if (nav->corr_rumbo_vertice != k) {
                    nav->corr_rumbo_vertice = k;
                    nav->corr_rumbo_cuenta = 0;
                }
                if (nav->corr_rumbo_cuenta >= NAV_MAX_CORR_RUMBO_VERTICE) {
                    giro = 0;
                    apunta = 0;
                } else {
                    nav->corr_rumbo_cuenta++;
                }
            }
            p_ini.x_mm = nav->x_mm;
            p_ini.y_mm = nav->y_mm;
            p_ini.rumbo_grados = nav->rumbo_grados;
            nav->n_pasos = 0;
            nav->indice_paso = 0;
            nav->ms_paso = 0;
            nav->codigo_trayectoria = TRAY_OK;
            if ((giro < -4 || giro > 4)) {
                int lado_g = (giro < 0) ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA;
                int grados_g = (giro < 0) ? -giro : giro;

                nav->pasos[nav->n_pasos].tipo = PASO_GIRAR;
                nav->pasos[nav->n_pasos].valor = giro;
                nav->pasos[nav->n_pasos].tiempo_ms = cal_grados_a_ms(&nav->cal, giro);
                nav->n_pasos++;
                cal_pose_tras_giro(&nav->cal, &p_ini, lado_g, grados_g, &p_ini);
            }
            if (apunta) {
                uhx = dir_x_mil(p_ini.rumbo_grados);
                uhy = dir_y_mil(p_ini.rumbo_grados);
                dist_esq = ((tx - p_ini.x_mm) * (long)uhx + (ty - p_ini.y_mm) * (long)uhy) / 1000;
            } else {
                dist_esq = ((ex - p_ini.x_mm) * (long)u1x + (ey - p_ini.y_mm) * (long)u1y) / 1000 - d_antes;
            }
            dist_k = ((kx - p_ini.x_mm) * (long)uhx + (ky - p_ini.y_mm) * (long)uhy) / 1000;
            avance_mm = (k == k_esq || dist_k > dist_esq) ? dist_esq : dist_k;
            continua = (nav->cmd_actual == NAV_CMD_FORWARD && nav->n_pasos == 0);
            if (nav->override_largo_mm > 0 && nav->indice_objetivo == 1 && !continua && !rumbo_medido_activo(nav)) {
                avance_mm = nav->override_largo_mm;   /* sin rumbo medido, un solo avance (J14); con él se planea por celdas como cualquier recta, así se corrige el rumbo en cada una */
            }
            if (nav->override_largo_mm > 0) {   /* J16: nunca más allá del largo fijo de la recta (también al volver de esquivar a mitad de camino) */
                int tramo_ov;
                long s_ov, desvio_ov;

                ubicar_en_linea(nav, p_ini.x_mm, p_ini.y_mm, &tramo_ov, &s_ov, &desvio_ov);
                if (avance_mm + 5 >= nav->override_largo_mm - s_ov) {
                    avance_mm = nav->override_largo_mm - s_ov;
                    hasta_el_largo = 1;   /* este avance termina la recta: frena ahí */
                }
            }
            {   /* J16: la línea que sigue este avance, para que rumbo_fw_actualizar sepa a dónde volver mientras avanza. El final que cuenta es el de la RECTA (el punto
                 * de parada antes de la esquina, o el largo fijo), no el de este paso: en una recta de varias celdas el carro sigue de largo de una celda a otra. */
                long fin_x = p_ini.x_mm + avance_mm * uhx / 1000, fin_y = p_ini.y_mm + avance_mm * uhy / 1000;
                long fin_recta;

                nav->paso_en_linea = 1;
                nav->paso_apunta = apunta;
                nav->paso_fin_x_mm = (int)fin_x;
                nav->paso_fin_y_mm = (int)fin_y;
                nav->linea_ax_mm = (int)punto_ruta_x(nav, k - 1);
                nav->linea_ay_mm = (int)punto_ruta_y(nav, k - 1);
                nav->linea_rumbo = rumbo_tramo;
                fin_recta = ((tx - nav->linea_ax_mm) * (long)u1x + (ty - nav->linea_ay_mm) * (long)u1y) / 1000;
                if (nav->override_largo_mm > 0 && fin_recta > nav->override_largo_mm - distancia_hasta_tramo(nav, k - 1)) {
                    fin_recta = nav->override_largo_mm - distancia_hasta_tramo(nav, k - 1);
                }
                nav->linea_fin_s_mm = (int)fin_recta;
            }
            cm = (int)((avance_mm + 5) / 10);
            sigue_recto = (k < k_esq && dist_esq - avance_mm >= 10 && !hasta_el_largo);
            if (cm > 0) {
                nav->pasos[nav->n_pasos].tipo = PASO_AVANZAR;
                nav->pasos[nav->n_pasos].valor = cm;
                nav->pasos[nav->n_pasos].tiempo_ms = cal_cm_a_ms_tramo(&nav->cal, cm, !continua, !sigue_recto);
                nav->n_pasos++;
            }
            if (continua && nav->n_pasos > 0) {
                nav->seg_ms = nav->cal.arranque_ms;
            }
            if (nav->cmd_actual != NAV_CMD_STOP) {
                if (nav->n_pasos > 0 && nav->cmd_actual == comando_de_paso(&nav->pasos[0])) {
                    registrar_inicio_comando(nav, nav->cmd_actual);
                } else {
                    nav->cmd_plan_ms = -1;
                }
            }
            return NAV_OK;
        }
    }

    orientacion = indice_direccion(nav->rumbo_grados);
    if (nav->sin_compensar_giro) {
        nav->rumbo_grados = orientacion * 45;
        nav->base_rumbo_grados = nav->rumbo_grados;
        nav->rumbo_x10 = nav->base_rumbo_x10 = nav->rumbo_grados * 10;
    }

    columna = celda_mas_cercana(nav->x_mm);
    fila = celda_mas_cercana(nav->y_mm);
    camino[0].columna = columna;
    camino[0].fila = fila;
    n = 1;
    while (columna != meta->columna || fila != meta->fila) {
        if (n >= MAX_PUNTOS) {
            nav->n_pasos = 0;
            return NAV_ERR_RUTA;
        }
        columna += signo(meta->columna - columna);
        fila += signo(meta->fila - fila);
        camino[n].columna = columna;
        camino[n].fila = fila;
        n++;
    }

    nav->codigo_trayectoria = trayectoria_calcular_desde_cal(&nav->cal, orientacion, camino, n,
                                                              nav->pasos, MAX_PASOS, &nav->n_pasos, &indice_error);
    nav->indice_paso = 0;
    nav->ms_paso = 0;
    if (nav->codigo_trayectoria != TRAY_OK) {
        nav->n_pasos = 0;
        return NAV_ERR_RUTA;
    }
    ajustar_tramos_continuos(nav, camino, n);
    if (nav->cmd_actual != NAV_CMD_STOP) {
        if (nav->n_pasos > 0 && nav->cmd_actual == comando_de_paso(&nav->pasos[0])) {
            registrar_inicio_comando(nav, nav->cmd_actual);   /* el comando sigue: empieza el registro del tramo nuevo */
        } else {
            nav->cmd_plan_ms = -1;                            /* el comando se va a soltar: su registro ya se cerró arriba */
        }
    }
    return NAV_OK;
}

/* ── Registro OBS_DIAG y frenos anti-locura (J9) ──────────────────────────────────────────────────────────────────────────────────────────── */

/* Una línea OBS_DIAG en cada decisión de obstáculo y al arrancar: la máscara de 10 x 10 (hexadecimal) de lo que se ve en cada zona, la cercanía y las celdas de cada zona, y si la franja
 * de referencia está tapada (y en qué % de los últimos cuadros). Solo números. `ignorado` dice qué zonas se ignoran como «sospechoso fijo» (izquierda, centro, derecha). */
static void diag_obs(Navegador *nav, const char *evento)
{
    diag_escribir(nav, "OBS_DIAG evento=%s mascara_izq=%s mascara_cen=%s mascara_der=%s cercania=(%d,%d,%d) celdas=(%d,%d,%d) franja_tapada=%d franja_pct=%d ignorado=%d%d%d iguales=%d",
                  evento, nav->obs_hex[ZONA_IZQUIERDA], nav->obs_hex[ZONA_CENTRO], nav->obs_hex[ZONA_DERECHA],
                  nav->zona_cercania[ZONA_IZQUIERDA], nav->zona_cercania[ZONA_CENTRO], nav->zona_cercania[ZONA_DERECHA],
                  nav->zona_celdas[ZONA_IZQUIERDA], nav->zona_celdas[ZONA_CENTRO], nav->zona_celdas[ZONA_DERECHA],
                  nav->obs_franja_tapada, nav->obs_franja_pct, nav->fijo_ignorar[ZONA_IZQUIERDA], nav->fijo_ignorar[ZONA_CENTRO], nav->fijo_ignorar[ZONA_DERECHA], nav->arranque_iguales);
}

/* Freno anti-locura: para y pide AYUDA (o ERROR si no se pidió) con la causa. */
static void frenar_por_locura(Navegador *nav, int causa, const char *que)
{
    diag_escribir(nav, "AYUDA_DIAG freno_anti_locura causa=%d %s pose=(%d,%d,%d) recorrido_mm=%ld ruta_mm=%ld indice=%d esquivadas=%d", causa, que, nav->x_mm, nav->y_mm, nav->rumbo_grados,
                  nav->recorrido_mm, nav->ruta_largo_mm, nav->indice_objetivo, nav->esquivadas);
    ir_a_error_causa(nav, NAV_MOTIVO_ESQUIVANDO, causa);
}

/* El mayor giro (grados) de los pasos planeados que NO está en la ruta: pasa del límite NAV_LOCURA_GIRO_MAX (o del mayor giro que pide la propia ruta). 0 si no hay. */
static int giro_fuera_de_ruta(const Navegador *nav)
{
    int i, limite = (nav->max_giro_ruta > NAV_LOCURA_GIRO_MAX) ? nav->max_giro_ruta : NAV_LOCURA_GIRO_MAX;

    for (i = 0; i < nav->n_pasos; i++) {
        if (nav->pasos[i].tipo == PASO_GIRAR) {
            int g = (nav->pasos[i].valor < 0) ? -nav->pasos[i].valor : nav->pasos[i].valor;

            if (g > limite) {
                return g;
            }
        }
    }
    return 0;
}

/* Cada tick: ¿el carro se está volviendo loco? Índice de la ruta que retrocede, distancia recorrida de más de 2.5 veces la de la ruta o pose a más de 1 m de la ruta. (Los giros planeados de
 * más de 150 grados se revisan donde se planean: la vuelta a la línea y la ruta que se retoma después; y las esquivadas de más de NAV_MAX_ESQUIVADAS, donde ya se contaban.) */
static void revisar_frenos(Navegador *nav)
{
    long limite, s, desvio;
    int tramo;

    if (nav->estado != NAV_SIGUIENDO && nav->estado != NAV_DETENIDO_OBSTACULO && nav->estado != NAV_ESQUIVANDO && nav->estado != NAV_REINCORPORANDO) {
        return;
    }
    if (nav->indice_objetivo > nav->indice_max) {
        nav->indice_max = nav->indice_objetivo;
    } else if (nav->indice_objetivo < nav->indice_max) {
        frenar_por_locura(nav, NAV_CAUSA_INDICE, "indice_retrocede");
        return;
    }
    limite = ((nav->ruta_largo_mm > NAV_LOCURA_RECORRIDO_MIN_MM) ? nav->ruta_largo_mm : NAV_LOCURA_RECORRIDO_MIN_MM) * NAV_LOCURA_RECORRIDO_X10 / 10;
    if (nav->recorrido_mm > limite) {
        frenar_por_locura(nav, NAV_CAUSA_RECORRIDO, "recorrido_de_mas");
        return;
    }
    if (nav->n_puntos >= 2) {
        ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s, &desvio);
        if (desvio > NAV_LOCURA_DESVIO_MM) {
            frenar_por_locura(nav, NAV_CAUSA_LEJOS, "lejos_de_la_ruta");
        }
    }
}

/* ── Configuración ─────────────────────────────────────────────────────── */

void navegador_iniciar(Navegador *nav)
{
    int z;

    if (nav == NULL) {
        return;
    }
    cal_defecto(&nav->cal);
    nav->latencia_visual_ms = 0;
    nav->frenar_lejos_centro = 0;
    nav->autoridad_laser = 0;
    nav->contradiccion_laser = 0;
    nav->solo_lejos_centro = 0;
    nav->n_puntos = 0;
    nav->indice_objetivo = 0;
    nav->n_pasos = 0;
    nav->indice_paso = 0;
    nav->ms_paso = 0;
    nav->estado = NAV_ESPERANDO;
    nav->motivo_error = NAV_MOTIVO_NINGUNO;
    nav->codigo_trayectoria = TRAY_OK;
    nav->cmd_actual = NAV_CMD_STOP;
    nav->t_ultimo_ms = 0;
    nav->t_detenido_ms = 0;
    nav->asentando = 0;
    nav->asentar_hasta_ms = 0;
    nav->usa_camara = 0;
    nav->t_ultimo_cuadro_ms = 0;
    nav->x_mm = nav->y_mm = nav->rumbo_grados = 0;
    nav->base_x_mm = nav->base_y_mm = nav->base_rumbo_grados = 0;
    nav->rumbo_x10 = nav->base_rumbo_x10 = 0;
    nav->sin_rumbo_medido = 0;
    nav->avance_imu_ref_ok = nav->avance_imu_ref_x10 = nav->avance_rumbo_ref_x10 = 0;
    nav->avance_rumbo_x10 = nav->avance_mm_previo = 0;
    nav->avance_x_mil = nav->avance_y_mil = 0;
    nav->seg_ms = 0;
    nav->giro_visual_x10 = 0;
    nav->visual_cubierto_ms = 0;
    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_hay[z] = 0;
        nav->zona_cercania[z] = 0;
    }
    nav->frames_bloqueado = 0;
    nav->frames_libre = 0;
    nav->frames_recibidos = 0;
    nav->t_bloqueo_ms = 0;
    nav->t_libre_ms = 0;
    nav->bloqueo_inmediato = 0;
    nav->bloqueo_cerc3 = 0;
    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_celdas[z] = 0;
        nav->zona_frena[z] = 0;
        nav->zona_cerc_frena[z] = 0;
        nav->zona_en_corredor[z] = 0;
    }
    nav->esquivadas = 0;
    nav->lado = -1;
    nav->fase = NAV_FASE_GIRAR;
    nav->ms_fase = 0;
    nav->reintentos = 0;
    nav->angulo_giro = NAV_ANGULO_ESQUIVAR;
    nav->angulo_total = 0;
    nav->t_mirar_ms = 0;
    nav->error_causa = NAV_CAUSA_NINGUNA;
    nav->esq_en_giro = 0;
    nav->esq_t_giro_ini_ms = nav->esq_t_giro_fin_ms = 0;
    nav->esq_cuadros_tras_giro = NAV_ESQ_CUADROS_ESTABLES;
    nav->centro_frames = 0;
    nav->centro_t_ms = 0;
    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_fila_min[z] = nav->zona_fila_max[z] = nav->zona_col_min[z] = nav->zona_col_max[z] = -1;
    }
    nav->pedir_ayuda = 0;
    nav->ayuda_cmd = NAV_CMD_STOP;
    nav->ayuda_t_ini_ms = 0;
    nav->ayuda_motivo = NAV_MOTIVO_NINGUNO;
    nav->ayudas = 0;
    nav->desde_ayuda = 0;
    nav->salida_x_mm = nav->salida_y_mm = nav->salida_rumbo = 0;
    nav->salida_cercania = 0;
    nav->cerc_detenido_max = 0;
    nav->fin_obstaculo_mm = -1;
    nav->obstaculo_pendiente = nav->verificando_frente = nav->fin_pendiente = nav->fin_verificado = 0;
    memset(nav->fuentes_vision, 0, sizeof nav->fuentes_vision);
    nav->vision_sin_coincidencia = nav->precaucion = 0;
    nav->borde_reintentos = nav->fondo_observado_mm = nav->cara_obstaculo_s_mm = 0;
    nav->lateral_objetivo_mm = NAV_ESQ_MARGEN_LATERAL_MM;
    nav->lateral_tope_mm = NAV_BORDEAR_LATERAL_MAX;
    nav->mirada_fallida = nav->mirada_previa_fallida = 0;
    nav->cmd_previo = (int)NAV_CMD_STOP;
    nav->reinc_deshacer_mm = 0;
    nav->vistazo_retrocedio = nav->rodeo_por_vistazo = 0;
    nav->salida_linea_rumbo = nav->salida_linea_ok = 0;
    nav->vistazo_lado = nav->vistazo_ecos = nav->vistazo_mm = nav->vistazo_confirmado = nav->vistazos = nav->vistazos_con_eco = nav->giro_extra_grados = 0;
    memset(nav->coinc_seguidas, 0, sizeof nav->coinc_seguidas);
    nav->coinc_detenido = 0;
    nav->vistazo_cuadros[0] = nav->vistazo_cuadros[1] = nav->vistazo_hecho[0] = nav->vistazo_hecho[1] = 0;
    nav->vistazo_cerc[0] = nav->vistazo_cerc[1] = nav->vistazo_n[0] = nav->vistazo_n[1] = 0;
    nav->vistazo_fase = nav->vistazo_lateral_mm[0] = nav->vistazo_lateral_mm[1] = nav->vistazo_vacio[0] = nav->vistazo_vacio[1] = 0;
    nav->vistazo_solo_red[0] = nav->vistazo_solo_red[1] = nav->vistazo_red_hecho[0] = nav->vistazo_red_hecho[1] = 0;
    nav->vistazo_fase_ms = 0;
    nav->vistazo_grados_lado[0] = nav->vistazo_grados_lado[1] = nav->vistazo_grados = NAV_VISTAZO_GRADOS;
    nav->vistazo_t_ms = nav->vistazo_ultimo_ms[0] = nav->vistazo_ultimo_ms[1] = nav->vistazo_fin_ms = 0;
    nav->vistazo_alguno = 0;
    nav->extension_ruta_mm = nav->cierre_reintento = 0;
    nav->atras_conocido_mm = nav->escape_atras = 0;
    nav->precaucion_pausa_ms = 0;
    nav->verificar_frente_ms = 0;
    nav->requiere_camara = 0;
    nav->paralelo_mm = 0;
    nav->reinc_pasar_primero = 0;
    nav->reinc_plan_x_mm[0] = nav->reinc_plan_x_mm[1] = nav->reinc_plan_x_mm[2] = 0;
    nav->reinc_plan_y_mm[0] = nav->reinc_plan_y_mm[1] = nav->reinc_plan_y_mm[2] = 0;
    nav->reinc_correcciones = 0;
    nav->corr_rumbo_vertice = -1;
    nav->corr_rumbo_cuenta = 0;
    nav->apunte_k_esq = -1;
    nav->reinc_rumbo_importa = 1;
    nav->tramo_mm = NAV_TRAMO_ESQUIVAR_CM * 10;
    nav->sin_red_seguridad = 0;
    nav->sin_arreglo_indice = 0;
    nav->reinc_rumbo_tol = NAV_REINC_GIRO_MIN_GRADOS - 1;
    nav->usa_giroscopio = 0;                 /* J11: por defecto todo como antes (giros por tiempo, sin láser) */
    nav->usa_tof = 0;
    nav->giro_fw_caido = nav->giro_fw = nav->giro_fw_estado = nav->giro_fw_pedir = 0;
    nav->giro_fw_min = NAV_GIRO_FW_MIN_GRADOS;
    nav->giro_fw_fallos = 0;
    nav->scan_ext_mm = nav->scan_pared_mm = nav->scan_muestras = nav->scan_corte = nav->scan_n = nav->ancho_rodeo_mm = 0;
    nav->lados_probados = nav->cambio_lado = 0;
    nav->scan_signo = 1;
    nav->tramo_base = 0;
    nav->retroceso_mm = NAV_RETROCESO_CM * 10;
    nav->borde_lejos[0] = nav->borde_lejos[1] = 0;
    nav->flanco_s_mm = 0;
    nav->flanco_vistas = 0;
    nav->esquivadas_obstaculo = 0;
    nav->tof_despejado_lecturas = 0;
    nav->fin_delante_mm = 0;
    nav->giro_fw_grados = nav->giro_fw_real_x10 = nav->giro_fw_ms = nav->giro_fw_ant = 0;
    nav->giro_fw_t_ms = 0;
    nav->n_giros_angulo = nav->n_giros_tiempo = 0;
    nav->giro_fw_ang_ini_x10 = nav->giro_fw_ang_ini_ok = nav->giro_fw_consultando = nav->giro_fw_consulta_pedir = 0;
    nav->giro_fw_consulta_t_ms = 0;
    nav->sens_angulo_x10 = nav->sens_angulo_ok = 0;
    nav->sens_angulo_t_ms = 0;
    nav->tof_vivo = nav->tof_mm = nav->tof_valida = nav->tof_libre_lecturas = nav->tof_cerca_lecturas = 0;
    nav->tof_t_ms = 0;
    nav->tof_paro_activo = nav->tof_pendiente = nav->tof_paros = nav->tof_dist_mm = 0;
    nav->racha_solo_centro2 = nav->tof_racha_nota = 0;
    nav->arranque_ok = 1;
    nav->arranque_iguales = nav->arranque_firma = 0;
    nav->arranque_t0_ms = 0;
    nav->bloqueo_cerc3 = 0;
    nav->sondeo_hecho = 1;
    nav->sondeo_fase = nav->sondeo_ms = nav->ms_sondeo = nav->sondeo_zonas = nav->sondeo_ref = nav->sondeo_n = 0;
    nav->sondeo_suma = 0;
    nav->sondeo_listo_ms = 0;
    for (z = 0; z < OBS_ZONAS; z++) {
        nav->fijo_ignorar[z] = 0;
        nav->fijo_celdas[z] = 0;
        memset(nav->fijo_hex[z], '0', MASC_HEX_LARGO);
        nav->fijo_hex[z][MASC_HEX_LARGO] = '\0';
        memset(nav->obs_hex[z], '0', MASC_HEX_LARGO);
        nav->obs_hex[z][MASC_HEX_LARGO] = '\0';
    }
    nav->frenado_pendiente = 0;
    nav->sin_compensar_giro = 0;
    nav->indice_max = 0;
    nav->recorrido_mm = 0;
    nav->prev_x_mm = nav->prev_y_mm = 0;
    nav->ruta_largo_mm = 0;
    nav->max_giro_ruta = 0;
    nav->obs_franja_tapada = nav->obs_franja_pct = 0;
    nav->arco_adelante_mm = nav->arco_libre_mm = 0;
    nav->reinc_fase = NAV_RF_PLAN;
    nav->reinc_t_fase_ms = 0;
    nav->reinc_solo_rumbo = 0;
    nav->reinc_obj_x_mm = nav->reinc_obj_y_mm = 0;
    nav->reinc_rumbo_final = 0;
    nav->reinc_giro1 = nav->reinc_giro2 = 0;
    nav->reinc_dist_mm = 0;
    nav->reinc_indice_obj = 0;
    nav->reinc_desvio_mm = 0;
    nav->reinc_antes_x_mm = nav->reinc_antes_y_mm = nav->reinc_antes_rumbo = 0;
    nav->volver_a_reinc = 0;
    nav->cmd_pedido = 0;
    nav->cmd_plan_ms = 0;
    nav->cmd_origen = NAV_ESPERANDO;
    nav->n_giros = 0;
    nav->giros_perdidos = 0;
    nav->diag_escritas = 0;
    nav->diag_leidas = 0;
    nav->diag_firma = -1;
    nav->diag_t_ms = 0;
    nav->diag[0][0] = '\0';
    nav->override_largo_mm = 0;
    nav->rumbo_fw = 0;
    nav->rumbo_fw_pedir = 0;
    nav->rumbo_fw_objetivo_x10 = nav->rumbo_fw_enviado_x10 = 0;
    nav->rumbo_fw_hay_enviado = 0;
    nav->rumbo_fw_t_ms = 0;
    nav->paso_en_linea = 0;
    nav->paso_apunta = nav->paso_fin_x_mm = nav->paso_fin_y_mm = 0;
    nav->linea_ax_mm = nav->linea_ay_mm = nav->linea_rumbo = nav->linea_fin_s_mm = 0;
    nav->sin_volver_a_linea = 0;
    nav->rumbo_en_vuelta = 0;
    nav->sin_margen_paso_j16 = 0;
    nav->sin_trig_fino = 0;
    nav->sin_arreglo_dt_giro = 0;
    nav->descartar_dt = 0;
}

int navegador_retroceso_calibrado(const Navegador *nav)
{
    return nav != NULL && cal_retroceso_medido(&nav->cal);
}

int navegador_poner_calibracion(Navegador *nav, const CalibracionCarro *cal)
{
    if (nav == NULL || cal == NULL || !cal_es_valida(cal)) {
        return NAV_ERR_ARGUMENTO;
    }
    if (nav->estado != NAV_ESPERANDO && nav->estado != NAV_TERMINADO && nav->estado != NAV_ERROR) {
        return NAV_ERR_ESTADO;
    }
    nav->cal = *cal;
    return NAV_OK;
}

int navegador_cargar_ruta(Navegador *nav, const Punto *puntos, int n_puntos)
{
    int i, indice_error;

    if (nav == NULL || puntos == NULL || n_puntos < 1 || n_puntos > MAX_PUNTOS) {
        return NAV_ERR_ARGUMENTO;
    }
    /* Se valida la ruta completa con trayectoria.c (saltos, puntos repetidos...). */
    nav->codigo_trayectoria = trayectoria_calcular(puntos, n_puntos, nav->pasos, MAX_PASOS,
                                                    &nav->n_pasos, &indice_error);
    nav->n_pasos = 0;
    if (nav->codigo_trayectoria != TRAY_OK) {
        nav->n_puntos = 0;
        return NAV_ERR_RUTA;
    }

    for (i = 0; i < n_puntos; i++) {
        nav->ruta[i] = puntos[i];
    }
    nav->n_puntos = n_puntos;
    nav->indice_objetivo = 1;
    nav->corr_rumbo_vertice = -1;
    nav->corr_rumbo_cuenta = 0;
    nav->apunte_k_esq = -1;
    nav->indice_paso = 0;
    nav->ms_paso = 0;
    nav->estado = NAV_ESPERANDO;
    nav->motivo_error = NAV_MOTIVO_NINGUNO;
    nav->error_causa = NAV_CAUSA_NINGUNA;
    nav->esquivadas = 0;
    nav->volver_a_reinc = 0;
    nav->frames_bloqueado = 0;
    nav->bloqueo_inmediato = 0;
    nav->bloqueo_cerc3 = 0;
    nav->frames_libre = 0;
    nav->frames_recibidos = 0;
    nav->cmd_actual = NAV_CMD_STOP;
    nav->n_giros = 0;                  /* los giros de la ruta anterior ya no cuentan */
    nav->giros_perdidos = 0;
    /* El carro se supone en el primer punto, mirando hacia ORIENTACION_INICIAL. */
    nav->x_mm = puntos[0].columna * MM_POR_CELDA;
    nav->y_mm = puntos[0].fila * MM_POR_CELDA;
    nav->rumbo_grados = ORIENTACION_INICIAL * 45;
    nav->base_x_mm = nav->x_mm;
    nav->base_y_mm = nav->y_mm;
    nav->base_rumbo_grados = nav->rumbo_grados;
    nav->rumbo_x10 = nav->base_rumbo_x10 = nav->rumbo_grados * 10;
    nav->seg_ms = 0;
    return NAV_OK;
}

int navegador_comenzar(Navegador *nav, uint32_t ahora_ms)
{
    if (nav == NULL) {
        return NAV_ERR_ARGUMENTO;
    }
    if (nav->estado != NAV_ESPERANDO || nav->n_puntos < 1) {
        return NAV_ERR_ESTADO;
    }
    nav->t_ultimo_ms = ahora_ms;
    nav->usa_camara = 0;               /* se activa con el primer cuadro de obstáculos de esta ruta */
    nav->t_ultimo_cuadro_ms = ahora_ms;
    nav->cmd_actual = NAV_CMD_STOP;
    nav->seg_ms = 0;
    nav->frames_bloqueado = 0;
    nav->bloqueo_inmediato = 0;
    nav->bloqueo_cerc3 = 0;
    nav->frames_libre = 0;
    nav->frames_recibidos = 0;
    nav->cerc_detenido_max = 0;
    nav->fin_obstaculo_mm = -1;
    nav->obstaculo_pendiente = nav->verificando_frente = nav->fin_pendiente = nav->fin_verificado = 0;
    memset(nav->fuentes_vision, 0, sizeof nav->fuentes_vision);
    nav->vision_sin_coincidencia = nav->precaucion = 0;
    nav->borde_reintentos = nav->fondo_observado_mm = nav->cara_obstaculo_s_mm = 0;
    nav->lateral_objetivo_mm = NAV_ESQ_MARGEN_LATERAL_MM;
    nav->lateral_tope_mm = NAV_BORDEAR_LATERAL_MAX;
    nav->mirada_fallida = nav->mirada_previa_fallida = 0;
    nav->cmd_previo = (int)NAV_CMD_STOP;
    nav->reinc_deshacer_mm = 0;
    nav->vistazo_retrocedio = nav->rodeo_por_vistazo = 0;
    nav->salida_linea_rumbo = nav->salida_linea_ok = 0;
    nav->vistazo_lado = nav->vistazo_ecos = nav->vistazo_mm = nav->vistazo_confirmado = nav->vistazos = nav->vistazos_con_eco = nav->giro_extra_grados = 0;
    memset(nav->coinc_seguidas, 0, sizeof nav->coinc_seguidas);
    nav->coinc_detenido = 0;
    nav->vistazo_cuadros[0] = nav->vistazo_cuadros[1] = nav->vistazo_hecho[0] = nav->vistazo_hecho[1] = 0;
    nav->vistazo_cerc[0] = nav->vistazo_cerc[1] = nav->vistazo_n[0] = nav->vistazo_n[1] = 0;
    nav->vistazo_fase = nav->vistazo_lateral_mm[0] = nav->vistazo_lateral_mm[1] = nav->vistazo_vacio[0] = nav->vistazo_vacio[1] = 0;
    nav->vistazo_solo_red[0] = nav->vistazo_solo_red[1] = nav->vistazo_red_hecho[0] = nav->vistazo_red_hecho[1] = 0;
    nav->vistazo_fase_ms = 0;
    nav->vistazo_grados_lado[0] = nav->vistazo_grados_lado[1] = nav->vistazo_grados = NAV_VISTAZO_GRADOS;
    nav->vistazo_t_ms = nav->vistazo_ultimo_ms[0] = nav->vistazo_ultimo_ms[1] = nav->vistazo_fin_ms = 0;
    nav->vistazo_alguno = 0;
    nav->extension_ruta_mm = nav->cierre_reintento = 0;
    nav->atras_conocido_mm = nav->escape_atras = 0;
    nav->precaucion_pausa_ms = 0;
    nav->tof_libre_lecturas = nav->tof_cerca_lecturas = 0;
    nav->reinc_pasar_primero = 0;
    {   /* Por ruta: el arranque espera cuadros iguales, el sondeo se puede hacer una vez y los frenos anti-locura parten de cero. */
        int k, previo = nav->rumbo_grados, z;

        nav->giro_fw_caido = nav->giro_fw = nav->giro_fw_estado = nav->giro_fw_pedir = 0;   /* por ruta: el giroscopio vuelve a intentarse y los contadores parten de cero */
        nav->giro_fw_fallos = 0;
        nav->esquivadas_obstaculo = 0;
        nav->fin_delante_mm = 0;
        nav->n_giros_angulo = nav->n_giros_tiempo = 0;
        nav->giro_fw_consultando = nav->giro_fw_consulta_pedir = nav->giro_fw_ang_ini_ok = 0;
        nav->tof_paro_activo = nav->tof_pendiente = nav->tof_paros = nav->tof_dist_mm = 0;
        nav->rumbo_fw_pedir = nav->rumbo_fw_hay_enviado = 0;   /* J16 */
        nav->paso_en_linea = 0;
        nav->descartar_dt = 0;
        nav->racha_solo_centro2 = nav->tof_racha_nota = 0;
        diag_escribir(nav, "SENS_DIAG config giroscopio=%d sensor_distancia=%d rumbo_fw=%d", nav->usa_giroscopio, nav->usa_tof, nav->rumbo_fw);
        nav->arranque_ok = 0;
        nav->arranque_iguales = 0;
        nav->arranque_firma = 0;
        nav->arranque_t0_ms = ahora_ms;
        nav->bloqueo_cerc3 = 0;
        nav->sondeo_hecho = 0;
        nav->sondeo_fase = 0;
        nav->frenado_pendiente = 0;
        for (z = 0; z < OBS_ZONAS; z++) {
            nav->fijo_ignorar[z] = 0;
            nav->fijo_celdas[z] = 0;
            memset(nav->fijo_hex[z], '0', MASC_HEX_LARGO);
            nav->fijo_hex[z][MASC_HEX_LARGO] = '\0';
        }
        nav->indice_max = nav->indice_objetivo;
        nav->recorrido_mm = 0;
        nav->prev_x_mm = nav->x_mm;
        nav->prev_y_mm = nav->y_mm;
        if (nav->override_largo_mm > 0) {
            nav->ruta_largo_mm = nav->override_largo_mm;
        } else {
            nav->ruta_largo_mm = (nav->n_puntos >= 2) ? distancia_hasta_tramo(nav, nav->n_puntos - 1) : 0;
        }
        nav->ruta_largo_original_mm = nav->ruta_largo_mm;
        nav->max_giro_ruta = 0;
        for (k = 0; k + 1 < nav->n_puntos; k++) {
            int rumbo = rumbo_del_tramo(nav, k);
            int g = giro_minimo(previo, rumbo);

            if (g < 0) {
                g = -g;
            }
            if (g > nav->max_giro_ruta) {
                nav->max_giro_ruta = g;
            }
            previo = rumbo;
        }
    }
    if (cal_desplazamiento_es_espejo(&nav->cal, CAL_LADO_IZQUIERDA) || cal_desplazamiento_es_espejo(&nav->cal, CAL_LADO_DERECHA)) {
        diag_escribir(nav, "TURN_DIAG aviso=%s_estimada_como_espejo_de_la_%s: no tiene medida propia, midela en Calibracion",
                      cal_desplazamiento_es_espejo(&nav->cal, CAL_LADO_IZQUIERDA) ? "izquierda" : "derecha", cal_desplazamiento_es_espejo(&nav->cal, CAL_LADO_IZQUIERDA) ? "derecha" : "izquierda");
    }

    if (nav->indice_objetivo >= nav->n_puntos) {
        nav->estado = NAV_TERMINADO;   /* ruta de un solo punto: ya está en el destino */
        return NAV_OK;
    }
    if (planificar(nav) != NAV_OK) {
        ir_a_error(nav, NAV_MOTIVO_PLAN);
        return NAV_ERR_RUTA;
    }
    nav->estado = NAV_SIGUIENDO;
    return NAV_OK;
}

void navegador_cancelar(Navegador *nav)
{
    if (nav == NULL) {
        return;
    }
    nav->estado = NAV_ESPERANDO;
    cambiar_comando(nav, NAV_CMD_STOP);
}

/* ── Cuadros de obstáculos ─────────────────────────────────────────────── */

/* ¿Este cuadro, por sí solo, ya justifica detenerse? Sí si el obstáculo está muy cerca (3) con al menos
 * NAV_CERC3_AREA_INMEDIATA celdas; cercanía 2 en el centro exige 2 cuadros seguidos. */
static int cuadro_es_inmediato(int zona, int cercania, int celdas)
{
    (void)zona;
    return cercania >= 3 && celdas >= NAV_CERC3_AREA_INMEDIATA;
}

/* Una zona lateral solo contiene el carril del láser si TODAS sus celdas
 * aceptadas están en columnas 3..6 (proyectadas a 10 columnas). Sin rejilla
 * fiable no se suprimen obstáculos laterales. Los datos de lado no se cambian. */
static int solo_carril_central(const ResultadoObstaculos *obs, int zona)
{
    int f, c, dentro = 0;
    if (obs->filas < 1 || obs->filas > OBS_MAX_FILAS || obs->columnas < 1 || obs->columnas > OBS_MAX_COLUMNAS ||
        obs->fila_minima_usada < 0 || obs->fila_minima_usada >= obs->filas) return 0;
    for (f = obs->fila_minima_usada; f < obs->filas; f++) {
        for (c = 0; c < obs->columnas; c++) {
            int col = c * 10 / obs->columnas;
            if (!obs->no_piso[f][c] || c * OBS_ZONAS / obs->columnas != zona) continue;
            if (col < 3 || col > 6) return 0;
            dentro = 1;
        }
    }
    return dentro;
}

/* Guarda lo que se vio en el último cuadro y lleva la cuenta de la RACHA actual: cuántos cuadros
 * seguidos (y desde qué hora, en ms) el frente está bloqueado o libre. Un cuadro de ruido suelto
 * no cambia nada: ver bloqueo_confirmado() y libre_confirmado(). */
/* V2: cuánto falta (mm) para el final de la ruta si el carro va SIGUIENDO por la recta final (todos los tramos que quedan tienen el mismo rumbo); -1 si quedan giros
 * o no está siguiendo la ruta. Sirve para saber si lo que mide el láser está antes o después del destino. */
static long resto_de_recta_final(const Navegador *nav)
{
    int tramo, k;
    long s, desvio, largo;

    if (nav->estado != NAV_SIGUIENDO || nav->n_puntos < 2 || nav->obstaculo_pendiente) {
        return -1;
    }
    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s, &desvio);
    for (k = tramo; k + 1 <= nav->n_puntos - 2; k++) {
        if (rumbo_del_tramo(nav, k + 1) != rumbo_del_tramo(nav, tramo)) {
            return -1;
        }
    }
    if (valor_absoluto_largo(giro_minimo(nav->rumbo_grados, rumbo_del_tramo(nav, tramo))) > 20) {
        return -1;   /* el carro no mira a lo largo de la recta: el láser no apunta al destino */
    }
    largo = (nav->override_largo_mm > 0) ? nav->override_largo_mm : distancia_hasta_tramo(nav, nav->n_puntos - 1);
    return (largo > s) ? largo - s : 0;
}

/* V2: ¿lo que el láser mide a `mm` del frente queda MÁS ALLÁ del destino con holgura (NAV_FIN_HOLGURA_MM entre el frente del carro ya detenido y el objeto)? */
static int laser_tras_el_destino(const Navegador *nav, int mm)
{
    long resto = resto_de_recta_final(nav);

    return resto >= 0 && (long)mm - resto >= NAV_FIN_HOLGURA_MM;
}

static void registrar_cuadro(Navegador *nav, uint32_t ahora_ms, const ResultadoObstaculos *obs)
{
    int z, bloqueado = 0, inmediato = 0, cerc3_chico = 0, causa_normal = 0, firma = 0, otra_causa = 0;
    int autoridad = tof_fresco(nav, ahora_ms) && nav->tof_valida && nav->tof_mm > 0;
    int laser_cerca = (autoridad && nav->tof_mm <= OBSTACULO_LASER_MM && !laser_tras_el_destino(nav, nav->tof_mm)) || nav->tof_paro_activo;
    int solo_central[OBS_ZONAS] = { 0, 0, 0 };
    int coincidencia = 0;
    nav->precaucion = 0;

    if (autoridad) {
        int camara_centro = obs->zona[ZONA_CENTRO].hay_obstaculo;
        int contradiccion;
        for (z = 0; z < OBS_ZONAS; z++) {
            solo_central[z] = solo_carril_central(obs, z);
            if (solo_central[z] && obs->zona[z].hay_obstaculo) camara_centro = 1;
        }
        contradiccion = camara_centro == laser_cerca ? 0 : (laser_cerca ? 1 : 2);
        if (contradiccion && contradiccion != nav->contradiccion_laser) {
            diag_escribir(nav, "CONTRADICCION_LASER_CAMARA laser_mm=%d laser_obstaculo=%d camara_obstaculo=%d columnas=3..6", nav->tof_mm, laser_cerca, camara_centro);
        }
        nav->contradiccion_laser = contradiccion;
    } else {
        nav->contradiccion_laser = 0;
    }

    /* Para el registro OBS_DIAG y la firma de celdas del fantasma fijo: la máscara de cada zona y la franja de referencia de lo último que se vio. */
    for (z = 0; z < OBS_ZONAS; z++) {
        if (masc_zona_a_hex(obs, z, nav->obs_hex[z], (int)sizeof nav->obs_hex[z]) != MASC_OK) {
            memset(nav->obs_hex[z], '0', MASC_HEX_LARGO);
            nav->obs_hex[z][MASC_HEX_LARGO] = '\0';
        }
    }
    nav->obs_franja_tapada = (obs->franja_contaminada || obs->franja_ignorada) ? 1 : 0;
    nav->obs_franja_pct = obs->franja_tapada_pct;

    for (z = 0; z < OBS_ZONAS; z++) {
        int fuentes = obs->fuentes_vision[z] & 3;
        int hay;
        if (!fuentes && obs->zona[z].hay_obstaculo) fuentes = 1; /* entrada de color sin fusión */
        nav->fuentes_vision[z] = fuentes;
        nav->color_cercania[z] = fuentes ? obs->zona[z].cercania : 0;
        nav->coinc_seguidas[z] = (fuentes == 3) ? nav->coinc_seguidas[z] + 1 : 0;
        limitar_contador(&nav->coinc_seguidas[z]);
        /* V2: la coincidencia cuenta desde el segundo cuadro seguido (una caja inventada por la red sobre una mancha de color suele durar un solo cuadro). Mientras
         * el carro rodea o vuelve a la línea vale desde el primero: ahí lo prudente es reaccionar antes. */
        hay = fuentes == 3 && (nav->coinc_seguidas[z] >= NAV_COINCIDENCIA_CUADROS_FRENAR || nav->estado == NAV_ESQUIVANDO || nav->estado == NAV_REINCORPORANDO ||
                               nav->estado == NAV_DETENIDO_OBSTACULO);
        coincidencia |= hay;
        /* PRECAUCIÓN (avanzar a ratos). V2: una sola fuente a un LADO ya no la pide si hay vistazo (lo resuelve el láser sin frenar), y en el CENTRO solo cuando lo
         * visto está cerca (cercanía 2 o 3): ahí el láser ya está mirando y no ve nada, así que puede ser algo bajo y conviene dar tiempo a la otra fuente. */
        if (fuentes == 3 && (!autoridad || nav->tof_mm > NAV_TOF_LIBRE_MM)) {
            nav->precaucion = 1;
        } else if (fuentes && !autoridad) {
            int modo = (nav->rumbo_fw && nav->usa_giroscopio && nav->usa_tof) ? nav->precaucion_modo : NAV_PRECAUCION_SIEMPRE;

            if (modo == NAV_PRECAUCION_SIEMPRE || (modo == NAV_PRECAUCION_COLOR_CERCA && z == ZONA_CENTRO && (fuentes & 1) && obs->zona[z].cercania >= 2)) {
                nav->precaucion = 1;
            }
        }
        int celdas = obs->zona[z].celdas;
        if (fuentes == 3 && obs->zona[z].cercania >= 3) inmediato = 1;   /* V2: con cercanía 2 hacen falta dos cuadros seguidos (ver NAV_COINCIDENCIA_CUADROS_ESQUIVAR) */

        if (autoridad && z == ZONA_CENTRO) {
            hay = hay || laser_cerca; /* el laser puede confirmar, nunca borrar una sospecha visual */
            nav->fijo_ignorar[z] = 0;  /* el láser nunca se descarta como un fantasma de color */
        }
        if (nav->vistazo_confirmado != 0 && nav->estado == NAV_DETENIDO_OBSTACULO && z == ((nav->vistazo_confirmado < 0) ? ZONA_IZQUIERDA : ZONA_DERECHA)) {
            hay = 1;                   /* V2: el láser lo confirmó al mirar hacia ese lado: por ahí no se rodea */
            nav->fijo_ignorar[z] = 0;
        }

        /* «Sospechoso fijo» (sondeo del arranque): vale toda la corrida mientras la firma de celdas siga igual, con tope de NAV_SONDEO_TOPE_CELDAS celdas por zona,
         * y se cancela si crece más del 30 %. Al ignorar una zona, su zona_celdas también se pone en 0 para no sesgar elegir_lado. */
        if (hay && nav->fijo_ignorar[z]) {
            int limite_celdas = nav->fijo_celdas[z] * NAV_SONDEO_IGNORAR_PCT / 100 + NAV_SONDEO_IGNORAR_MAS;

            if (nav->estado == NAV_SIGUIENDO && (celdas > limite_celdas || celdas > NAV_SONDEO_TOPE_CELDAS)) {
                nav->fijo_ignorar[z] = 0;
            } else if (celdas <= limite_celdas && celdas <= NAV_SONDEO_TOPE_CELDAS &&
                       strcmp(nav->obs_hex[z], nav->fijo_hex[z]) == 0) {
                hay = 0;
                celdas = 0;
            }
        }
        nav->zona_hay[z] = hay;
        nav->zona_cercania[z] = hay ? ((laser_cerca && z == ZONA_CENTRO) ? 3 : obs->zona[z].cercania) : 0;
        nav->zona_celdas[z] = celdas;
        firma |= (hay ? (nav->zona_cercania[z] & 3) : 0) << (2 * z);
        /* Corredor (corredor.h): un obstáculo FUERA del corredor se sigue viendo (zona_hay, para elegir lado) pero no frena. */
        nav->zona_frena[z] = hay && !obs->zona[z].fuera_corredor;
        nav->zona_en_corredor[z] = nav->zona_frena[z] && obs->zona[z].cercania_corredor > 0;   /* el corredor (activo) dice que está en el camino del carro */
        nav->zona_cerc_frena[z] = !nav->zona_frena[z] ? 0 :
            ((obs->zona[z].cercania_corredor > 0) ? obs->zona[z].cercania_corredor : obs->zona[z].cercania);
        if (laser_cerca && z == ZONA_CENTRO) {
            nav->zona_frena[z] = laser_cerca;
            nav->zona_en_corredor[z] = laser_cerca;
            nav->zona_cerc_frena[z] = laser_cerca ? 3 : 0;
            if (laser_cerca) inmediato = 1;
        }
        if (nav->zona_cerc_frena[z] == 1 && !laser_cerca) nav->zona_frena[z] = nav->zona_en_corredor[z] = 0;
        if (nav->zona_frena[z] && nav->frenar_lejos_centro && z == ZONA_CENTRO && nav->zona_cerc_frena[z] >= 1 &&
            nav->zona_cerc_frena[z] < NAV_CERCANIA_MIN_DETENER) {
            bloqueado = 1;                       /* frenar desde lejos: cercanía 1 en el centro también bloquea (se confirma con menos tiempo) */
        }
        if (nav->zona_frena[z] && nav->zona_cerc_frena[z] >= NAV_CERCANIA_MIN_DETENER) {
            bloqueado = 1;                       /* un obstáculo cerca en cualquier zona (dentro del corredor, si está activo) bloquea */
            causa_normal = 1;
            if (!(z == ZONA_CENTRO && nav->zona_cerc_frena[z] == 2)) {
                otra_causa = 1;                  /* algo que NO es cercanía 2 en el centro: el láser no lo puede corregir */
            }
            if (cuadro_es_inmediato(z, nav->zona_cerc_frena[z], nav->zona_celdas[z])) {
                inmediato = 1;
            } else if (nav->zona_cerc_frena[z] >= 3 || (nav->zona_cerc_frena[z] == 2 && z == ZONA_CENTRO)) {
                cerc3_chico = 1;                 /* cercanía 3 con pocas celdas o cercanía 2 en el centro: frena con 2 cuadros seguidos */
            }
        }
    }
    if (bloqueado) {
        if (nav->frames_bloqueado == 0) {      /* empieza una racha con obstáculo */
            nav->t_bloqueo_ms = ahora_ms;
            nav->bloqueo_inmediato = 0;
            nav->bloqueo_cerc3 = 0;
            nav->solo_lejos_centro = 1;
            nav->racha_solo_centro2 = 1;
            nav->tof_racha_nota = 0;
        }
        if (!causa_normal || otra_causa) {
            nav->racha_solo_centro2 = 0;         /* la racha ya no es solo «cercanía 2 en el centro» */
        }
        if (cerc3_chico) {
            nav->bloqueo_cerc3 = 1;
        }
        if (causa_normal) {
            nav->solo_lejos_centro = 0;          /* la racha ya incluye un obstáculo de los de siempre: tiempos de siempre */
        }
        if (inmediato) {
            nav->bloqueo_inmediato = 1;
        }
        nav->frames_bloqueado++;
        nav->frames_libre = 0;
        if (nav->usa_tof && nav->racha_solo_centro2) {   /* el láser corrige o confirma a la cámara (solo cercanía 2 en el centro; con cercanía 3 no hay confirmación que valga) */
            if (tof_dice_cerca(nav, ahora_ms) && nav->tof_racha_nota != 2) {
                nav->tof_racha_nota = 2;
                diag_escribir(nav, "SENS_DIAG confirma_tof camara=cercania2_centro laser_mm=%d lecturas=%d: frena con un cuadro", nav->tof_mm, nav->tof_cerca_lecturas);
            } else if (nav->frames_bloqueado == 2 && tof_dice_libre(nav, ahora_ms) && nav->tof_racha_nota == 0) {
                nav->tof_racha_nota = 1;
                diag_escribir(nav, "SENS_DIAG veto_tof camara=cercania2_centro laser_mm=%d libre_lecturas=%d: se pide un cuadro mas de la camara", nav->tof_mm, nav->tof_libre_lecturas);
            } else if (nav->frames_bloqueado == 3 && nav->tof_racha_nota == 1) {
                nav->tof_racha_nota = 3;
                diag_escribir(nav, "SENS_DIAG camara_confirma tras_veto_tof laser_mm=%d: frena", nav->tof_mm);
            }
        }
    } else {
        nav->racha_solo_centro2 = 0;
        if (nav->frames_libre == 0) {          /* empieza una racha con el frente libre */
            nav->t_libre_ms = ahora_ms;
        }
        nav->frames_libre++;
        nav->frames_bloqueado = 0;
        nav->bloqueo_inmediato = 0;
        nav->bloqueo_cerc3 = 0;
    }
    /* Arranque: cuántos cuadros seguidos son iguales (la misma cercanía con obstáculo en cada zona). */
    if (!nav->arranque_ok) {
        if (nav->arranque_iguales > 0 && firma == nav->arranque_firma) {
            nav->arranque_iguales++;
        } else {
            nav->arranque_firma = firma;
            nav->arranque_iguales = 1;
        }
    }
    /* Sondeo: después de avanzar y asentarse, se suman las celdas de las zonas del obstáculo para compararlas con las de antes. */
    if (nav->sondeo_fase == 2 && ahora_ms - nav->sondeo_listo_ms < 0x80000000u) {
        int celdas_sondeo = 0;

        for (z = 0; z < OBS_ZONAS; z++) {
            if (nav->sondeo_zonas & (1 << z)) {
                celdas_sondeo += obs->zona[z].celdas;
            }
        }
        nav->sondeo_suma += celdas_sondeo;
        nav->sondeo_n++;
    }
    /* V2: celdas «no piso» en el borde lejano de cada lado (las 4 primeras filas de la zona de avance, la sexta parte exterior de las columnas): ahí asoma el costado
     * de un objeto que se está pasando en paralelo. */
    nav->borde_lejos[0] = nav->borde_lejos[1] = 0;
    if (obs->filas > 0 && obs->filas <= OBS_MAX_FILAS && obs->columnas >= 6 && obs->columnas <= OBS_MAX_COLUMNAS &&
        obs->fila_minima_usada >= 0 && obs->fila_minima_usada < obs->filas) {
        int fila, columna, orilla = obs->columnas / 6;

        for (fila = obs->fila_minima_usada; fila < obs->fila_minima_usada + 4 && fila < obs->filas; fila++) {
            for (columna = 0; columna < orilla; columna++) {
                nav->borde_lejos[0] += obs->no_piso[fila][columna] ? 1 : 0;
                nav->borde_lejos[1] += obs->no_piso[fila][obs->columnas - 1 - columna] ? 1 : 0;
            }
        }
    }
    /* Filas y columnas de las celdas de obstáculo de cada zona (solo se calculan durante la esquivada, para el registro ESQUIVE_DIAG y las columnas 0 y 9). */
    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_fila_min[z] = nav->zona_fila_max[z] = nav->zona_col_min[z] = nav->zona_col_max[z] = -1;
        nav->zona_borde_cerca[z] = 0;
    }
    if ((nav->estado == NAV_ESQUIVANDO || nav->estado == NAV_REINCORPORANDO) && obs->filas > 0 && obs->filas <= OBS_MAX_FILAS && obs->columnas > 0 && obs->columnas <= OBS_MAX_COLUMNAS &&
        obs->fila_minima_usada >= 0 && obs->fila_minima_usada < obs->filas) {   /* solo con un tamaño de cuadrícula creíble: un resultado armado a medias no debe hacer leer fuera del arreglo */
        int fila, columna;
        unsigned char borde_izq[3] = { 0, 0, 0 }, borde_der[3] = { 0, 0, 0 };

        for (fila = obs->fila_minima_usada; fila < obs->filas && fila < OBS_MAX_FILAS; fila++) {
            int fg = fila * OBS_MASC_FILAS / obs->filas;

            for (columna = 0; columna < obs->columnas && columna < OBS_MAX_COLUMNAS; columna++) {
                if (obs->no_piso[fila][columna]) {
                    int zona = columna * OBS_ZONAS / obs->columnas;
                    int cg = columna * OBS_MASC_COLUMNAS / obs->columnas;

                    if (nav->zona_fila_min[zona] < 0 || fila < nav->zona_fila_min[zona]) {
                        nav->zona_fila_min[zona] = fila;
                    }
                    if (fila > nav->zona_fila_max[zona]) {
                        nav->zona_fila_max[zona] = fila;
                    }
                    if (nav->zona_col_min[zona] < 0 || columna < nav->zona_col_min[zona]) {
                        nav->zona_col_min[zona] = columna;
                    }
                    if (columna > nav->zona_col_max[zona]) {
                        nav->zona_col_max[zona] = columna;
                    }
                    if (fg >= 7 && fg < OBS_MASC_FILAS) {
                        if (cg == 0) {
                            borde_izq[fg - 7] = 1;
                        } else if (cg == OBS_MASC_COLUMNAS - 1) {
                            borde_der[fg - 7] = 1;
                        }
                    }
                }
            }
        }
        /* Las columnas 0 y 9 en las filas 7 a 9 se evalúan solo durante esquive y reincorporación (si esa zona no está ignorada como fantasma fijo). */
        if (nav->fuentes_vision[ZONA_IZQUIERDA] == 3 && !nav->fijo_ignorar[ZONA_IZQUIERDA] && (borde_izq[0] + borde_izq[1] + borde_izq[2] >= 2)) {
            nav->zona_borde_cerca[ZONA_IZQUIERDA] = 1;
            nav->zona_hay[ZONA_IZQUIERDA] = 1;
            nav->zona_frena[ZONA_IZQUIERDA] = 1;
            if (nav->zona_cercania[ZONA_IZQUIERDA] < 3) {
                nav->zona_cercania[ZONA_IZQUIERDA] = 3;
            }
            if (nav->zona_cerc_frena[ZONA_IZQUIERDA] < 3) {
                nav->zona_cerc_frena[ZONA_IZQUIERDA] = 3;
            }
        }
        if (nav->fuentes_vision[ZONA_DERECHA] == 3 && !nav->fijo_ignorar[ZONA_DERECHA] && (borde_der[0] + borde_der[1] + borde_der[2] >= 2)) {
            nav->zona_borde_cerca[ZONA_DERECHA] = 1;
            nav->zona_hay[ZONA_DERECHA] = 1;
            nav->zona_frena[ZONA_DERECHA] = 1;
            if (nav->zona_cercania[ZONA_DERECHA] < 3) {
                nav->zona_cercania[ZONA_DERECHA] = 3;
            }
            if (nav->zona_cerc_frena[ZONA_DERECHA] < 3) {
                nav->zona_cerc_frena[ZONA_DERECHA] = 3;
            }
        }
    }
    if (nav->estado == NAV_DETENIDO_OBSTACULO) {   /* el último cuadro antes de decidir puede venir vacío: se recuerda lo más cerca que se vio */
        for (z = 0; z < OBS_ZONAS; z++) {
            if (nav->zona_cerc_frena[z] > nav->cerc_detenido_max) {
                nav->cerc_detenido_max = nav->zona_cerc_frena[z];
            }
        }
    }
    {   /* V2, VISTAZO: cuadros seguidos con algo sin confirmar a cada lado (lo confirmado por las dos fuentes ya frena por su cuenta) */
        int lado_i;

        for (lado_i = 0; lado_i < 2; lado_i++) {
            int zl = lado_i ? ZONA_DERECHA : ZONA_IZQUIERDA;
            int n = 0, racha_max = 0, fila_baja = -1, col_adentro = -1, una = 0;

            /* Las celdas «no piso» del tercio de ese lado, tal cual (aunque no lleguen al mínimo que hace «obstáculo» a una zona): cuántas, la raya vertical más
             * larga (una pata es una raya delgada), la fila más baja (qué tan cerca) y la columna más cercana al centro (su orilla de adentro). */
            if (obs->filas > 0 && obs->filas <= OBS_MAX_FILAS && obs->columnas >= 6 && obs->columnas <= OBS_MAX_COLUMNAS &&
                obs->fila_minima_usada >= 0 && obs->fila_minima_usada < obs->filas) {
                int fila, columna, c0 = lado_i ? obs->columnas - obs->columnas / 3 : 0, c1 = lado_i ? obs->columnas : obs->columnas / 3;

                for (columna = c0; columna < c1; columna++) {
                    int racha = 0;

                    for (fila = obs->fila_minima_usada; fila < obs->filas; fila++) {
                        if (obs->no_piso[fila][columna]) {
                            n++;
                            racha++;
                            if (racha > racha_max) racha_max = racha;
                            if (fila > fila_baja) fila_baja = fila;
                            if (col_adentro < 0 || (lado_i ? columna < col_adentro : columna > col_adentro)) col_adentro = columna;
                        } else {
                            racha = 0;
                        }
                    }
                }
            }
            nav->vistazo_n[lado_i] = n;
            nav->vistazo_cerc[lado_i] = 0;
            nav->vistazo_grados_lado[lado_i] = NAV_VISTAZO_GRADOS;
            nav->vistazo_lateral_mm[lado_i] = 0;
            if (n > 0) {
                int permil = (fila_baja + 1) * 1000 / obs->filas;
                int cerc = (permil >= 834) ? 3 : ((permil >= 667) ? 2 : 1);
                int lejos_mm = (cerc >= 3) ? 250 : ((cerc == 2) ? 375 : 600);     /* de la lente a lo visto, por bandas (SUPUESTO: cámara a 115 mm, 12 grados) */
                int columnas_al_eje = lado_i ? col_adentro - obs->columnas / 2 : obs->columnas / 2 - (col_adentro + 1);
                int grados = (columnas_al_eje < 0 ? 0 : columnas_al_eje) * NAV_CAMARA_FOV_GRADOS / obs->columnas;   /* de su orilla de adentro al eje */
                int tuerce = grados + 5;                                          /* el cono del láser (unos 12 grados a cada lado) cubre de ahí hacia afuera */

                if (grados > 60) grados = 60;
                nav->vistazo_cerc[lado_i] = cerc;
                nav->vistazo_lateral_mm[lado_i] = (int)((long)lejos_mm * seno_mil(grados) / (seno_mil(90 - grados) > 100 ? seno_mil(90 - grados) : 100));
                nav->vistazo_grados_lado[lado_i] = (tuerce < NAV_VISTAZO_GRADOS) ? NAV_VISTAZO_GRADOS : ((tuerce > NAV_VISTAZO_GRADOS_MAX) ? NAV_VISTAZO_GRADOS_MAX : tuerce);
                una = nav->fuentes_vision[zl] != 3 && !nav->fijo_ignorar[zl] &&
                      (n >= NAV_VISTAZO_CELDAS_MIN || (n >= NAV_VISTAZO_CELDAS_RACHA && racha_max >= NAV_VISTAZO_RACHA));
            }
            nav->vistazo_solo_red[lado_i] = 0;
            if (!una && (nav->fuentes_vision[zl] & 2) && nav->fuentes_vision[zl] != 3 && !nav->fijo_ignorar[zl] && nav->color_cercania[zl] >= 1) {
                una = 1;                                   /* solo la red (una caja suya en ese lado, sin celdas de color): también merece el vistazo */
                nav->vistazo_solo_red[lado_i] = 1;
                if (nav->vistazo_cerc[lado_i] < nav->color_cercania[zl]) nav->vistazo_cerc[lado_i] = nav->color_cercania[zl];
            }
            if (una) {
                nav->vistazo_cuadros[lado_i]++;
                nav->vistazo_vacio[lado_i] = 0;
            } else {
                nav->vistazo_cuadros[lado_i] = 0;
                nav->vistazo_vacio[lado_i]++;
                if (nav->vistazo_vacio[lado_i] >= NAV_VISTAZO_VACIO_CUADROS) {
                    nav->vistazo_hecho[lado_i] = 0;        /* lo que se miraba ya no está: lo siguiente que aparezca es otra cosa */
                    nav->vistazo_red_hecho[lado_i] = 0;
                }
            }
            limitar_contador(&nav->vistazo_cuadros[lado_i]);
            limitar_contador(&nav->vistazo_vacio[lado_i]);
        }
    }
    if (coincidencia && nav->estado == NAV_DETENIDO_OBSTACULO) {
        nav->coinc_detenido++;
        limitar_contador(&nav->coinc_detenido);
    }
    nav->vision_sin_coincidencia = coincidencia ? 0 : nav->vision_sin_coincidencia + 1;
    limitar_contador(&nav->vision_sin_coincidencia);
    nav->frames_recibidos++;
    limitar_contador(&nav->frames_bloqueado);
    limitar_contador(&nav->frames_libre);
    limitar_contador(&nav->frames_recibidos);
}

/* ¿Hay que tratar el frente como bloqueado? Inmediato con un solo cuadro (obstáculo cerca); si
 * es lejano, solo cuando la racha dura MS_CONFIRMAR_LEJOS ms y tiene al menos 2 cuadros.
 * Usa el reloj `ahora_ms` que recibe el navegador, no la cantidad de cuadros. */
static int bloqueo_confirmado(const Navegador *nav, uint32_t ahora_ms)
{
    if (nav->frames_bloqueado == 0) {
        return 0;
    }
    if (nav->bloqueo_inmediato) {
        return 1;
    }
    if (nav->racha_solo_centro2) {           /* solo cercanía 2 en el centro: aquí el láser confirma o corrige a la cámara */
        if (tof_dice_cerca(nav, ahora_ms)) {
            return 1;                        /* el láser ve algo cerca: un cuadro basta */
        }
        if (nav->bloqueo_cerc3 && nav->frames_bloqueado >= 2 && tof_dice_libre(nav, ahora_ms)) {
            return nav->frames_bloqueado >= 3;   /* el láser ve libre: UN cuadro más de la cámara (nunca se ignora por más) */
        }
    }
    if (nav->bloqueo_cerc3 && nav->frames_bloqueado >= 2) {
        return 1;                            /* cercanía 3 con pocas celdas: 2 cuadros seguidos */
    }
    return nav->frames_bloqueado >= 2 &&
           (ahora_ms - nav->t_bloqueo_ms) >= (uint32_t)(nav->solo_lejos_centro ? NAV_MS_CONFIRMAR_CENTRO_LEJOS : MS_CONFIRMAR_LEJOS);
}

/* ¿El frente lleva MS_CONFIRMAR_LIBRE ms libre (en al menos 2 cuadros)? */
static int libre_confirmado(const Navegador *nav, uint32_t ahora_ms)
{
    return nav->frames_libre >= 2 && (ahora_ms - nav->t_libre_ms) >= (uint32_t)MS_CONFIRMAR_LIBRE;
}

/* ── Detección durante la esquivada ────────────────────────────────────── */

static const char *nombre_zona(int zona)
{
    return (zona == ZONA_IZQUIERDA) ? "IZQ" : ((zona == ZONA_CENTRO) ? "CEN" : "DER");
}

static const char *nombre_fase_esquivar(const Navegador *nav)
{
    if (nav->estado == NAV_REINCORPORANDO) {
        return (nav->reinc_fase == NAV_RF_AVANZAR) ? "VUELTA_AVANZAR" : ((nav->reinc_fase == NAV_RF_GIRO1 || nav->reinc_fase == NAV_RF_GIRO2) ? "VUELTA_GIRO" : "VUELTA_PAUSA");
    }
    switch (nav->fase) {
    case NAV_FASE_GIRAR:      return "GIRAR";
    case NAV_FASE_AVANZAR:    return "AVANZAR";
    case NAV_FASE_VOLVER:     return "VOLVER";
    case NAV_FASE_MIRAR:      return "MIRAR";
    case NAV_FASE_RETROCEDER: return "RETROCEDER";
    case NAV_FASE_PARALELO:   return "PARALELO";
    case NAV_FASE_PAUSA_PASAR: return "PAUSA_PASAR";
    case NAV_FASE_PAUSA_VOLVER: return "PAUSA_VOLVER";
    case NAV_FASE_PAUSA_RETROCEDER: return "PAUSA_RETROCEDER";
    default:                  return "ASENTAR";
    }
}

/* Marca el comienzo de un giro de la maniobra (GIRAR, VOLVER o los giros de la vuelta a la línea): mientras gira se ignoran los cuadros. */
static void esq_iniciar_giro(Navegador *nav, uint32_t ahora_ms)
{
    nav->esq_en_giro = 1;
    nav->esq_t_giro_ini_ms = ahora_ms;
    nav->centro_frames = 0;
}

/* Marca el final del giro: desde aquí se cuentan los cuadros estables (NAV_ESQ_CUADROS_ESTABLES) o los NAV_ESQ_MS_ESTABLE ms. */
static void esq_terminar_giro(Navegador *nav, uint32_t ahora_ms)
{
    nav->esq_en_giro = 0;
    nav->esq_t_giro_fin_ms = ahora_ms;
    nav->esq_cuadros_tras_giro = 0;
}

/* ¿Hay que ignorar los cuadros que llegan ahora? Sí mientras gira y, después, hasta tener NAV_ESQ_CUADROS_ESTABLES cuadros o NAV_ESQ_MS_ESTABLE ms. */
static int esq_ignorando(const Navegador *nav, uint32_t ahora_ms)
{
    uint32_t desde_fin = ahora_ms - nav->esq_t_giro_fin_ms;

    if (nav->esq_en_giro) {
        return 1;
    }
    if (desde_fin < (uint32_t)nav->latencia_visual_ms) {
        return 1;   /* con el video del carro, lo que llega ahora todavía muestra el giro (~300 ms de retraso) */
    }
    return nav->esq_cuadros_tras_giro < NAV_ESQ_CUADROS_ESTABLES && desde_fin - (uint32_t)nav->latencia_visual_ms < (uint32_t)NAV_ESQ_MS_ESTABLE;
}

/* Lo que bloquea durante la esquivada: el CENTRO con cercanía NAV_ESQ_CERCANIA_BLOQUEA o más; las columnas 0 y 9 en las filas 7 a 9 durante esquive y reincorporación;
 * y mientras VUELVE a la línea (REINCORPORANDO, avanza en diagonal), también un LADO con cercanía NAV_ESQ_CERCANIA_LADO que el CORREDOR dice que está en el camino del carro.
 * Devuelve la zona que bloquea o -1. */
static int esq_zona_que_bloquea(const Navegador *nav)
{
    if (nav->zona_frena[ZONA_CENTRO] && nav->zona_cerc_frena[ZONA_CENTRO] >= NAV_ESQ_CERCANIA_BLOQUEA) {
        return ZONA_CENTRO;
    }
    if (nav->estado == NAV_ESQUIVANDO || nav->estado == NAV_REINCORPORANDO) {
        if (nav->zona_borde_cerca[ZONA_IZQUIERDA]) {
            return ZONA_IZQUIERDA;
        }
        if (nav->zona_borde_cerca[ZONA_DERECHA]) {
            return ZONA_DERECHA;
        }
    }
    if (nav->estado != NAV_REINCORPORANDO) {
        return -1;
    }
    if (nav->zona_frena[ZONA_IZQUIERDA] && nav->zona_en_corredor[ZONA_IZQUIERDA] && nav->zona_cerc_frena[ZONA_IZQUIERDA] >= NAV_ESQ_CERCANIA_LADO) {
        return ZONA_IZQUIERDA;
    }
    if (nav->zona_frena[ZONA_DERECHA] && nav->zona_en_corredor[ZONA_DERECHA] && nav->zona_cerc_frena[ZONA_DERECHA] >= NAV_ESQ_CERCANIA_LADO) {
        return ZONA_DERECHA;
    }
    return -1;
}

/* Cuadro nuevo durante la esquivada (ESQUIVANDO o REINCORPORANDO): lleva la racha del obstáculo CENTRAL (cercanía >= NAV_ESQ_CERCANIA_BLOQUEA) sin contar los
 * cuadros que se ignoran, y escribe la línea ESQUIVE_DIAG de lo que se vio (fase, cercanía, zona, celdas, filas y columnas, ms desde el inicio del giro, y qué se
 * hizo con ello). Los lados y lo lejano se anotan pero no cuentan. */
static void esquive_registrar_cuadro(Navegador *nav, uint32_t ahora_ms)
{
    int z, mejor = -1, ignorada, bloquea;
    const char *accion;

    if (!nav->esq_en_giro && nav->esq_cuadros_tras_giro < LIMITE_CONTADOR &&
        (uint32_t)(ahora_ms - nav->esq_t_giro_fin_ms) >= (uint32_t)nav->latencia_visual_ms) {   /* solo cuentan los cuadros que ya muestran el carro sin girar */
        nav->esq_cuadros_tras_giro++;
    }
    ignorada = esq_ignorando(nav, ahora_ms);
    bloquea = esq_zona_que_bloquea(nav);
    if (ignorada || bloquea < 0) {
        nav->centro_frames = 0;
    } else {
        if (nav->centro_frames == 0) {
            nav->centro_t_ms = ahora_ms;
        }
        nav->centro_frames++;
        limitar_contador(&nav->centro_frames);
    }
    for (z = 0; z < OBS_ZONAS; z++) {                       /* la zona más cercana es la que se anota */
        if (nav->zona_hay[z] && (mejor < 0 || nav->zona_cercania[z] > nav->zona_cercania[mejor])) {
            mejor = z;
        }
    }
    if (mejor < 0) {
        return;                                              /* cuadro sin nada: no hay nada que anotar */
    }
    if (ignorada) {
        accion = nav->esq_en_giro ? "ignorada_girando" : "ignorada_espera_estable";
    } else if (bloquea < 0 && mejor != ZONA_CENTRO) {
        accion = "lateral_no_bloquea";
    } else if (bloquea < 0) {
        accion = "lejos_no_bloquea";
    } else {
        accion = (bloquea == ZONA_CENTRO) ? "centro_cuenta" : "lado_cuenta";
    }
    {
        char filas[24], columnas[24];

        if (nav->zona_fila_min[mejor] < 0) {
            snprintf(filas, sizeof filas, "?");
            snprintf(columnas, sizeof columnas, "?");
        } else {
            snprintf(filas, sizeof filas, "%d-%d", nav->zona_fila_min[mejor], nav->zona_fila_max[mejor]);
            snprintf(columnas, sizeof columnas, "%d-%d", nav->zona_col_min[mejor], nav->zona_col_max[mejor]);
        }
        /* No se anota cada cuadro: solo cuando cambia algo (fase, zona, cercanía o lo que se hizo con él) o cada NAV_DIAG_REPETIR_MS ms. */
        int firma = (nav->estado * 8 + (int)(accion[0] - 'a')) * 64 + mejor * 8 + nav->zona_cercania[mejor] + nav->fase * 4096 + nav->reinc_fase * 65536;

        if (firma != nav->diag_firma || (uint32_t)(ahora_ms - nav->diag_t_ms) >= (uint32_t)NAV_DIAG_REPETIR_MS) {
            nav->diag_firma = firma;
            nav->diag_t_ms = ahora_ms;
            diag_escribir(nav, "ESQUIVE_DIAG fase=%s cerc=%d zona=%s celdas=%d filas=%s cols=%s ms_giro=%ld cuadros_tras_giro=%d accion=%s",
                          nombre_fase_esquivar(nav), nav->zona_cercania[mejor], nombre_zona(mejor), nav->zona_celdas[mejor], filas, columnas,
                          (long)(uint32_t)(ahora_ms - nav->esq_t_giro_ini_ms), nav->esq_cuadros_tras_giro, accion);
        }
    }
}

/* ¿El obstáculo del centro ya está CONFIRMADO durante la esquivada? (cercanía suficiente, MS_CONFIRMAR_LEJOS ms seguidos y al menos 2 cuadros, sin contar los ignorados) */
static int esquive_centro_confirmado(const Navegador *nav, uint32_t ahora_ms)
{
    return nav->centro_frames >= 2 && (uint32_t)(ahora_ms - nav->centro_t_ms) >= (uint32_t)MS_CONFIRMAR_LEJOS;
}

/* La zona que se anota en ESQUIVE_DIAG cuando algo bloquea (la que bloquea; si ninguna, el centro). */
static int zona_para_diag(const Navegador *nav)
{
    int z = esq_zona_que_bloquea(nav);

    return (z < 0) ? ZONA_CENTRO : z;
}

/* ── Los estados ───────────────────────────────────────────────────────── */

/* ¿Terminó el paso actual del camino? Un giro con la cámara confiable se corta al ver el ángulo; lo demás, por tiempo. */
static int paso_terminado(const Navegador *nav)
{
    const Paso *paso = &nav->pasos[nav->indice_paso];

    if (paso->tipo == PASO_GIRAR && nav->cmd_actual == comando_de_paso(paso)) {
        return giro_terminado(nav, (paso->valor < 0) ? -paso->valor : paso->valor, paso->tiempo_ms, nav->ms_paso);
    }
    return nav->ms_paso + NAV_TICK_MS / 2 >= paso->tiempo_ms;
}

/* SIGUIENDO: ejecuta los pasos del camino. Cuando se acaban, pasa al siguiente punto de la
 * ruta; cuando no quedan puntos, TERMINADO. Si el frente se bloquea mientras avanza, se
 * detiene. */
/* Se detiene por obstáculo (DETENIDO_OBSTACULO): la cercanía del cuadro que lo detuvo es la primera que se recuerda (ver cerc_detenido_max). */
static void entrar_detenido(Navegador *nav, uint32_t ahora_ms)
{
    int z;

    nav->coinc_detenido = 0;   /* V2: los cuadros con coincidencia se cuentan desde que se detiene */

    nav->estado = NAV_DETENIDO_OBSTACULO;
    nav->t_detenido_ms = ahora_ms;
    if (!nav->tof_paro_activo) {   /* si el láser tiene una medida real reciente, es la distancia al obstáculo (sirve para saber cuánto debe pasar en paralelo) */
        nav->tof_dist_mm = (tof_fresco(nav, ahora_ms) && nav->tof_valida) ? nav->tof_mm : 0;
    }
    diag_obs(nav, "detiene");
    nav->frames_recibidos = 0;
    nav->frames_libre = 0;
    nav->cerc_detenido_max = 0;
    for (z = 0; z < OBS_ZONAS; z++) {
        if (nav->zona_cerc_frena[z] > nav->cerc_detenido_max) {
            nav->cerc_detenido_max = nav->zona_cerc_frena[z];
        }
    }
}

/* ── Arranque y sondeo del obstáculo fijo (J9) ────────────────────────────────────────────────────────────────────────────────────────────── */

/* ¿Hay que sondear? El obstáculo ya estaba en los cuadros del arranque, el carro no se ha movido ni esquivado, el centro está libre y la cercanía es chica (o el obstáculo está solo en los lados,
 * donde avanzar 4 cm no lo acerca al centro): avanzar un poco y ver si las celdas crecen. Un obstáculo real crece al acercarse; un artefacto de la imagen no. */
static int sondeo_aplica(const Navegador *nav)
{
    int z, hay = 0, lateral_solo = 1, cerc_max = 0;

    if (!tof_dice_cerca(nav, nav->t_ultimo_ms) || nav->sondeo_hecho || nav->esquivadas != 0 || nav->recorrido_mm > NAV_SONDEO_QUIETO_MM || nav->arranque_firma == 0 || nav->zona_hay[ZONA_CENTRO]) {
        return 0;
    }
    for (z = 0; z < OBS_ZONAS; z++) {
        if (nav->zona_frena[z]) {
            hay = 1;
            if (nav->zona_cerc_frena[z] > cerc_max) {
                cerc_max = nav->zona_cerc_frena[z];
            }
            if (z == ZONA_CENTRO) {
                lateral_solo = 0;
            }
        }
    }
    return hay && (cerc_max <= NAV_SONDEO_CERC_MAX || lateral_solo);
}

static void iniciar_sondeo(Navegador *nav, uint32_t ahora_ms)
{
    int z;

    nav->sondeo_hecho = 1;
    nav->sondeo_fase = 1;
    nav->sondeo_ms = cal_cm_a_ms(&nav->cal, NAV_SONDEO_CM);
    nav->ms_sondeo = 0;
    nav->sondeo_zonas = 0;
    nav->sondeo_ref = 0;
    nav->sondeo_suma = 0;
    nav->sondeo_n = 0;
    for (z = 0; z < OBS_ZONAS; z++) {
        if (nav->zona_frena[z]) {
            nav->sondeo_zonas |= 1 << z;
            nav->sondeo_ref += nav->zona_celdas[z];
        }
    }
    (void)ahora_ms;
    diag_obs(nav, "sondeo_inicio");
}

/* El sondeo en marcha: avanza NAV_SONDEO_CM cm, se asienta, mira unos cuadros y decide. */
static ComandoNav tick_sondeo(Navegador *nav, uint32_t ahora_ms)
{
    if (nav->sondeo_fase == 1) {
        if (nav->zona_frena[ZONA_CENTRO] && bloqueo_confirmado(nav, ahora_ms)) {   /* algo apareció en el centro mientras avanzaba: es real */
            nav->sondeo_fase = 0;
            diag_obs(nav, "sondeo_centro");
            entrar_detenido(nav, ahora_ms);
            return NAV_CMD_STOP;
        }
        if (nav->ms_sondeo + NAV_TICK_MS / 2 >= nav->sondeo_ms) {
            nav->sondeo_fase = 2;
            nav->sondeo_listo_ms = ahora_ms + (uint32_t)(NAV_PAUSA_ASENTAR_MS + nav->latencia_visual_ms);
            return NAV_CMD_STOP;
        }
        return NAV_CMD_FORWARD;
    }
    /* Fase 2: quieto; espera al menos 2 cuadros (y NAV_SONDEO_MIRAR_MS) o hasta NAV_SONDEO_MAX_ESPERA_MS (1500 ms). */
    if ((uint32_t)(ahora_ms - nav->sondeo_listo_ms) >= 0x80000000u ||
        ahora_ms - nav->sondeo_listo_ms < (uint32_t)NAV_SONDEO_MIRAR_MS ||
        (nav->sondeo_n < 2 && ahora_ms - nav->sondeo_listo_ms < (uint32_t)NAV_SONDEO_MAX_ESPERA_MS)) {
        return NAV_CMD_STOP;
    }
    if (nav->sondeo_n < 2) {   /* no llegaron 2 cuadros en 1500 ms: repite el sondeo sin declarar nada */
        ajustar_paso_tras_parar(nav);
        iniciar_sondeo(nav, ahora_ms);
        return NAV_CMD_FORWARD;
    }
    {
        int antes = nav->sondeo_ref, despues = (int)(nav->sondeo_suma / nav->sondeo_n);
        int crece = antes * NAV_SONDEO_CRECE_PCT / 100;
        int z, excede_tope = (antes > NAV_SONDEO_TOPE_CELDAS || despues > NAV_SONDEO_TOPE_CELDAS);

        if (crece < NAV_SONDEO_CRECE_MIN) {
            crece = NAV_SONDEO_CRECE_MIN;
        }
        for (z = 0; z < OBS_ZONAS; z++) {
            if ((nav->sondeo_zonas & (1 << z)) && nav->zona_celdas[z] > NAV_SONDEO_TOPE_CELDAS) {
                excede_tope = 1;
            }
        }
        nav->sondeo_fase = 0;
        nav->frames_bloqueado = 0;
        nav->frames_libre = 0;
        nav->bloqueo_inmediato = 0;
        nav->bloqueo_cerc3 = 0;
        if (excede_tope || despues >= antes + crece) {   /* pasa del tope de 12 celdas o creció: es real */
            diag_escribir(nav, "OBS_DIAG sondeo=real celdas_antes=%d celdas_despues=%d zonas=%d", antes, despues, nav->sondeo_zonas);
            diag_obs(nav, "sondeo_real");
            entrar_detenido(nav, ahora_ms);
            return NAV_CMD_STOP;
        }
        ajustar_paso_tras_parar(nav);
        for (z = 0; z < OBS_ZONAS; z++) {
            if (nav->sondeo_zonas & (1 << z)) {
                int c = (nav->zona_celdas[z] > antes) ? nav->zona_celdas[z] : antes;

                nav->fijo_ignorar[z] = 1;
                nav->fijo_celdas[z] = (c > NAV_SONDEO_TOPE_CELDAS) ? NAV_SONDEO_TOPE_CELDAS : c;
                memcpy(nav->fijo_hex[z], nav->obs_hex[z], sizeof nav->fijo_hex[z]);
                nav->zona_hay[z] = 0;
                nav->zona_frena[z] = 0;
                nav->zona_cercania[z] = 0;
                nav->zona_cerc_frena[z] = 0;
                nav->zona_celdas[z] = 0;
            }
        }
        diag_escribir(nav, "OBS_DIAG sondeo=sospechoso_fijo celdas_antes=%d celdas_despues=%d zonas=%d: se ignora en esta corrida y no se reaprende el piso con el", antes, despues, nav->sondeo_zonas);
        diag_obs(nav, "sondeo_fijo");
        return NAV_CMD_STOP;
    }
}

static ComandoNav tick_siguiendo(Navegador *nav, uint32_t ahora_ms)
{
    ComandoNav comando = NAV_CMD_STOP;
    int vueltas = 0;

    if (nav->sondeo_fase != 0) {
        return tick_sondeo(nav, ahora_ms);
    }
    /* ARRANQUE: si el primer cuadro ya muestra un obstáculo no se decide con él: el carro espera NAV_ARRANQUE_CUADROS cuadros seguidos iguales (o NAV_ARRANQUE_MAX_MS). Con el frente libre arranca de una vez. */
    if (!nav->arranque_ok && nav->usa_camara && nav->arranque_firma != 0) {
        if (nav->arranque_iguales < NAV_ARRANQUE_CUADROS && (uint32_t)(ahora_ms - nav->arranque_t0_ms) < (uint32_t)NAV_ARRANQUE_MAX_MS) {
            return NAV_CMD_STOP;
        }
        nav->arranque_ok = 1;
        diag_obs(nav, "arranque");
    }

    for (;;) {
        vueltas++;
        if (vueltas > MAX_PASOS + MAX_PUNTOS + 2) {   /* protección: el bucle siempre termina */
            ir_a_error(nav, NAV_MOTIVO_PLAN);
            return NAV_CMD_STOP;
        }
        if (nav->indice_paso >= nav->n_pasos) {
            /* Llegó al punto objetivo: seguir con el siguiente. */
            nav->indice_objetivo++;
            if (nav->indice_objetivo >= nav->n_puntos) {
                nav->estado = NAV_TERMINADO;
                return NAV_CMD_STOP;
            }
            if (planificar(nav) != NAV_OK) {
                ir_a_error(nav, NAV_MOTIVO_PLAN);
                return NAV_CMD_STOP;
            }
            continue;
        }
        /* El paso termina cuando su tiempo ya se ejecutó (redondeado a medio tick); un giro con la cámara, cuando ve el ángulo. */
        if (paso_terminado(nav)) {
            nav->indice_paso++;
            nav->ms_paso = 0;
            nav->frenado_pendiente = 0;
            continue;
        }
        comando = comando_de_paso(&nav->pasos[nav->indice_paso]);
        break;
    }

    /* Entre avanzar y girar (o al revés) el carro se detiene un momento: así cada movimiento arranca desde cero, como en la
     * calibración, y no hereda la inercia del anterior (con eso el giro de un cuadrado variaba 30 a 60 grados entre corridas). */
    if (nav->asentando) {
        if ((int32_t)(ahora_ms - nav->asentar_hasta_ms) < 0) {
            return NAV_CMD_STOP;
        }
        nav->asentando = 0;
    } else if (nav->cmd_actual != NAV_CMD_STOP && nav->cmd_actual != comando) {
        nav->asentando = 1;
        nav->asentar_hasta_ms = ahora_ms + NAV_PAUSA_ASENTAR_MS;
        return NAV_CMD_STOP;
    }

    if (comando == NAV_CMD_FORWARD && bloqueo_confirmado(nav, ahora_ms)) {
        /* ARRANQUE: al arrancar no se decide (detener, sondear o esquivar) con el primer cuadro: hacen falta NAV_ARRANQUE_CUADROS cuadros seguidos iguales o que pase NAV_ARRANQUE_MAX_MS. */
        if (!nav->arranque_ok) {
            if (nav->arranque_iguales < NAV_ARRANQUE_CUADROS && (uint32_t)(ahora_ms - nav->arranque_t0_ms) < (uint32_t)NAV_ARRANQUE_MAX_MS) {
                return NAV_CMD_STOP;
            }
            nav->arranque_ok = 1;
            diag_obs(nav, "arranque");
        }
        if (sondeo_aplica(nav)) {
            iniciar_sondeo(nav, ahora_ms);   /* ¿obstáculo real o artefacto fijo? avanza un poco y lo compara */
            return NAV_CMD_FORWARD;
        }
        entrar_detenido(nav, ahora_ms);
        return NAV_CMD_STOP;
    }
    if (comando != NAV_CMD_STOP) {
        nav->arranque_ok = 1;   /* el carro ya arrancó sin obstáculo: el arranque pasó */
    }
    return comando;
}

/* ── Volver a la línea (REINCORPORANDO) ────────────────────────────────── */

/* Cambia de fase de la vuelta a la línea y anota la hora (las pausas se miden con la hora, no con ticks). */
static void reinc_cambiar_fase(Navegador *nav, int fase, uint32_t ahora_ms)
{
    nav->reinc_fase = fase;
    nav->reinc_t_fase_ms = ahora_ms;
    nav->ms_fase = 0;
}

/* Empieza la vuelta a la línea: primero queda quieto NAV_PAUSA_ASENTAR_MS (para que la pose estimada incluya el frenado) y después calcula. */
static void entrar_reincorporando(Navegador *nav, uint32_t ahora_ms)
{
    nav->estado = NAV_REINCORPORANDO;
    nav->frames_bloqueado = 0;   /* hacen falta cuadros nuevos para volver a detenerse */
    nav->bloqueo_inmediato = 0;
    nav->bloqueo_cerc3 = 0;
    nav->frames_libre = 0;
    nav->centro_frames = 0;
    nav->reinc_correcciones = 0;   /* una vuelta nueva empieza con las correcciones en cero */
    reinc_cambiar_fase(nav, NAV_RF_PLAN, ahora_ms);
}

/* ── Plan con pivote (J8) ──────────────────────────────────────────────────
 * El carro gira de pivote: el centro se desplaza al girar (ver cal_desplazamiento_giro). Estas funciones PLANEAN con ese desplazamiento: dada una pose, qué giro hay que hacer para
 * apuntar a un punto, dónde termina el centro, y desde dónde hay que girar para que el centro caiga en un punto. Todo con las funciones públicas cal_pose_tras_giro y
 * cal_pose_antes_del_giro de calibracion.h. Con la calibración sin desplazamiento todo reduce a los giros en el sitio de siempre. */

static PoseCarro pose_actual(const Navegador *nav)
{
    PoseCarro p;

    p.x_mm = nav->x_mm;
    p.y_mm = nav->y_mm;
    p.rumbo_grados = nav->rumbo_grados;
    return p;
}

static int valor_absoluto_int(int x)
{
    return (x < 0) ? -x : x;
}

/* La pose después de girar `giro` grados con signo (+ derecha, - izquierda), con el desplazamiento del pivote. */
static PoseCarro pose_tras_giro_con_signo(const Navegador *nav, PoseCarro p, int giro)
{
    if (giro > 0) {
        cal_pose_tras_giro(&nav->cal, &p, CAL_LADO_DERECHA, giro, &p);
    } else if (giro < 0) {
        cal_pose_tras_giro(&nav->cal, &p, CAL_LADO_IZQUIERDA, -giro, &p);
    }
    return p;
}

/* La pose de la que hay que partir para llegar a `p` después de girar `giro` grados con signo (la inversa de la anterior). */
static PoseCarro pose_antes_de_giro_con_signo(const Navegador *nav, PoseCarro p, int giro)
{
    if (giro > 0) {
        cal_pose_antes_del_giro(&nav->cal, &p, CAL_LADO_DERECHA, giro, &p);
    } else if (giro < 0) {
        cal_pose_antes_del_giro(&nav->cal, &p, CAL_LADO_IZQUIERDA, -giro, &p);
    }
    return p;
}

/* Rumbo desde la pose hasta el punto (ox, oy); si ya está ahí, el rumbo que tiene. */
static int rumbo_hacia_punto(const PoseCarro *p, long ox, long oy)
{
    return rumbo_del_vector(ox - p->x_mm, oy - p->y_mm, p->rumbo_grados);
}

/* Un giro de menos de NAV_REINC_GIRO_MIN_GRADOS no se hace (el error de la estimación es mayor): vale 0. */
static int giro_util(int giro)
{
    return (giro > -NAV_REINC_GIRO_MIN_GRADOS && giro < NAV_REINC_GIRO_MIN_GRADOS) ? 0 : giro;
}

/* V2: ¿los giros se le pueden pedir al firmware por ángulo ahora mismo? */
static int giroscopio_sano(const Navegador *nav)
{
    return nav->usa_giroscopio && !nav->giro_fw_caido;
}

/* V2: ¿se ejecuta un giro de `giro` grados (con signo)? No si es menor que NAV_REINC_GIRO_MIN_GRADOS (como siempre) ni, con el giroscopio sano, si es menor que el mínimo
 * que acepta el firmware: ese giro iría POR TIEMPO, con el pivote a plena potencia y sin freno (en B6 un ajuste de 4 grados giró 18). Lo que falte de rumbo lo corrige RUMBO
 * al avanzar o el plan siguiente. */
static int giro_ejecutable(const Navegador *nav, int giro)
{
    int g = (giro < 0) ? -giro : giro;

    if (g < NAV_REINC_GIRO_MIN_GRADOS) {
        return 0;
    }
    return !(giroscopio_sano(nav) && g < nav->giro_fw_min);
}

/* El giro `g` (grados con signo) que, hecho desde `p` con el desplazamiento del pivote, deja el rumbo apuntando al punto (ox, oy) DESDE LA POSICIÓN NUEVA (no desde la de antes del giro:
 * el centro se movió). Es un punto fijo: g = giro_minimo(rumbo, rumbo hacia el punto desde la pose después de girar g). Se itera NAV_PLAN_ITERACIONES pasos empezando por el giro en el sitio;
 * si no converge (por ejemplo un giro grande que cambia de lado a mitad de la iteración) se barre de grado en grado y se queda el que deja menos error. Escribe el giro y la pose después de
 * girarlo y devuelve el error de rumbo que queda (grados, valor absoluto). */
static int girar_hacia(const Navegador *nav, const PoseCarro *p, long ox, long oy, int *giro, PoseCarro *despues)
{
    int g, k, nuevo, error, mejor_g = 0, mejor_error = 1000;
    PoseCarro q;

    g = giro_util(giro_minimo(p->rumbo_grados, rumbo_hacia_punto(p, ox, oy)));
    for (k = 0; k < NAV_PLAN_ITERACIONES; k++) {
        q = pose_tras_giro_con_signo(nav, *p, g);
        nuevo = giro_util(giro_minimo(p->rumbo_grados, rumbo_hacia_punto(&q, ox, oy)));
        if (valor_absoluto_int(giro_minimo(g, nuevo)) <= 1) {
            g = nuevo;
            break;
        }
        g = nuevo;
    }
    q = pose_tras_giro_con_signo(nav, *p, g);
    error = valor_absoluto_int(giro_minimo(q.rumbo_grados, rumbo_hacia_punto(&q, ox, oy)));
    if (error > 2) {
        for (g = -179; g <= 180; g++) {
            q = pose_tras_giro_con_signo(nav, *p, g);
            error = valor_absoluto_int(giro_minimo(q.rumbo_grados, rumbo_hacia_punto(&q, ox, oy)));
            if (error < mejor_error || (error == mejor_error && valor_absoluto_int(g) < valor_absoluto_int(mejor_g))) {
                mejor_error = error;
                mejor_g = g;
            }
        }
        g = giro_util(mejor_g);
        q = pose_tras_giro_con_signo(nav, *p, g);
        error = valor_absoluto_int(giro_minimo(q.rumbo_grados, rumbo_hacia_punto(&q, ox, oy)));
    }
    *giro = g;
    *despues = q;
    return error;
}

/* Una vuelta a la línea planeada: giro 1 hacia el punto previo, avance, giro 2 al rumbo de la línea. `p1` es la pose después del giro 1, `q` la pose después de avanzar (el punto previo) y `e`
 * la pose final, con el centro sobre la línea. */
typedef struct {
    int giro1, giro2, dist_mm;
    PoseCarro p1, q, e;
} PlanVuelta;

/* Llena el plan con los dos giros y las poses; devuelve cuánto avanza el centro entre ellos (mm; negativo = el punto previo q quedó detrás del carro). */
static long completar_plan(PlanVuelta *plan, int a1, int a2, const PoseCarro *p1, const PoseCarro *q, const PoseCarro *e, int rumbo_final)
{
    long vx = (long)q->x_mm - p1->x_mm, vy = (long)q->y_mm - p1->y_mm;
    long distancia = raiz_entera(vx * vx + vy * vy);
    long avance = (vx * dir_x_mil(p1->rumbo_grados) + vy * dir_y_mil(p1->rumbo_grados) >= 0) ? distancia : -distancia;

    plan->giro1 = a1;
    plan->giro2 = a2;
    plan->dist_mm = (avance > 0) ? (int)avance : 0;
    plan->p1 = *p1;
    plan->q = *q;
    plan->e = *e;
    plan->e.rumbo_grados = (a2 == 0) ? p1->rumbo_grados : rumbo_final;
    return avance;
}

/* Plan por iteración de punto fijo (el camino normal): converge en 2 o 3 pasos cuando el punto queda delante. */
static int planear_vuelta_iterando(const Navegador *nav, const PoseCarro *p0, long ex, long ey, int rumbo_final, PlanVuelta *plan)
{
    int ha, a1 = 0, a2 = 0, k, converge = 0, error = 0;
    PoseCarro e = *p0, q = *p0, p1 = *p0;

    ha = rumbo_hacia_punto(p0, ex, ey);   /* primer supuesto del rumbo con el que llega: el directo */
    for (k = 0; k < NAV_PLAN_ITERACIONES; k++) {
        a2 = giro_util(giro_minimo(ha, rumbo_final));
        e.x_mm = (int)ex;
        e.y_mm = (int)ey;
        e.rumbo_grados = (a2 == 0) ? ha : rumbo_final;
        q = pose_antes_de_giro_con_signo(nav, e, a2);
        error = girar_hacia(nav, p0, q.x_mm, q.y_mm, &a1, &p1);
        if (valor_absoluto_int(giro_minimo(ha, p1.rumbo_grados)) <= 1) {
            converge = 1;
            break;
        }
        ha = p1.rumbo_grados;
    }
    return completar_plan(plan, a1, a2, &p1, &q, &e, rumbo_final) >= -30 && converge && error <= 3;
}

/* Plan por barrido de 1 grado del giro 1 (cuando la iteración no converge: el punto queda casi al lado o detrás y el rumbo de llegada salta de un lado a otro). Para cada giro 1 candidato
 * el rumbo de llegada y el giro 2 quedan definidos; vale el que deja el punto previo q justo delante, en el rumbo con el que llega (error de 2 grados o menos), con el menor giro total. */
static int planear_vuelta_barriendo(const Navegador *nav, const PoseCarro *p0, long ex, long ey, int rumbo_final, PlanVuelta *plan)
{
    int g, a1, a2, error, mejor_a1 = 0, mejor_a2 = 0, mejor_costo = 100000, hay = 0;
    PoseCarro e, q, p1, mejor_p1 = *p0, mejor_q = *p0, mejor_e = *p0;
    long vx, vy;

    for (g = -179; g <= 180; g++) {
        a1 = giro_util(g);
        if (a1 != g) {
            continue;   /* los giros de menos de NAV_REINC_GIRO_MIN_GRADOS no existen: solo vale 0 */
        }
        p1 = pose_tras_giro_con_signo(nav, *p0, a1);
        a2 = giro_util(giro_minimo(p1.rumbo_grados, rumbo_final));
        e.x_mm = (int)ex;
        e.y_mm = (int)ey;
        e.rumbo_grados = (a2 == 0) ? p1.rumbo_grados : rumbo_final;
        q = pose_antes_de_giro_con_signo(nav, e, a2);
        vx = (long)q.x_mm - p1.x_mm;
        vy = (long)q.y_mm - p1.y_mm;
        if (vx * dir_x_mil(p1.rumbo_grados) + vy * dir_y_mil(p1.rumbo_grados) < 0) {
            continue;   /* q quedó detrás del carro */
        }
        error = (vx == 0 && vy == 0) ? 0 : valor_absoluto_int(giro_minimo(p1.rumbo_grados, rumbo_del_vector(vx, vy, p1.rumbo_grados)));
        if (error <= 2 && valor_absoluto_int(a1) + valor_absoluto_int(a2) < mejor_costo) {
            hay = 1;
            mejor_costo = valor_absoluto_int(a1) + valor_absoluto_int(a2);
            mejor_a1 = a1;
            mejor_a2 = a2;
            mejor_p1 = p1;
            mejor_q = q;
            mejor_e = e;
        }
    }
    if (hay) {
        completar_plan(plan, mejor_a1, mejor_a2, &mejor_p1, &mejor_q, &mejor_e, rumbo_final);
    }
    return hay;
}

/* Planea llegar al punto (ex, ey) SIN giro final: un giro hacia él y el avance. Es la vuelta cuando el punto es el final de la ruta (ahí el rumbo con el que se llega no importa). */
static int planear_hasta_punto(const Navegador *nav, const PoseCarro *p0, long ex, long ey, PlanVuelta *plan)
{
    int a1 = 0, error;
    PoseCarro e, p1 = *p0;

    e.x_mm = (int)ex;
    e.y_mm = (int)ey;
    error = girar_hacia(nav, p0, ex, ey, &a1, &p1);
    e.rumbo_grados = p1.rumbo_grados;
    return completar_plan(plan, a1, 0, &p1, &e, &e, p1.rumbo_grados) >= -30 && error <= 3;
}

/* Planea la vuelta desde `p0` para terminar con el centro en (ex, ey) y el rumbo `rumbo_final` de la línea. Los dos giros desplazan el centro, así que:
 *   - el avance termina en el PUNTO PREVIO q, el que cae en e después del giro final con su desplazamiento (la inversa del modelo, cal_pose_antes_del_giro);
 *   - el giro 1 apunta a q desde la posición posterior al giro (girar_hacia), y el rumbo con el que llega a q define el giro 2: por eso se itera NAV_PLAN_ITERACIONES veces hasta que el
 *     rumbo de llegada deje de cambiar (1 grado); si no converge se barre el giro 1 de grado en grado.
 * Devuelve 1 si hay solución (el punto previo queda delante del carro, a lo más 3 cm detrás) y 0 si no. */
static int planear_vuelta(const Navegador *nav, const PoseCarro *p0, long ex, long ey, int rumbo_final, PlanVuelta *plan)
{
    if (planear_vuelta_iterando(nav, p0, ex, ey, rumbo_final, plan)) {
        return 1;
    }
    return planear_vuelta_barriendo(nav, p0, ex, ey, rumbo_final, plan);
}

/* La ruta se retoma desde la CELDA más cercana a la pose (planificar), así que si la vuelta termina entre dos celdas el carro se corre hasta media celda a lo largo de la línea al retomarla.
 * Por eso el punto de la línea al que se vuelve se pega al vértice (centro de celda) más cercano a `s_mm` que esté al menos `minimo_mm` más adelante; si no hay ninguno, el final de la ruta. */
static long vertice_mas_cercano(const Navegador *nav, long s_mm, long minimo_mm)
{
    long mejor = distancia_hasta_tramo(nav, nav->n_puntos - 1), s_k;
    int k;

    for (k = 0; k < nav->n_puntos; k++) {
        s_k = distancia_hasta_tramo(nav, k);
        if (s_k >= minimo_mm && valor_absoluto_largo(s_k - s_mm) < valor_absoluto_largo(mejor - s_mm)) {
            mejor = s_k;
        }
    }
    return mejor;
}

/* ¿Hay desplazamiento del centro al girar (en algún lado)? Si no, el esquive se planea como siempre. */
static int hay_pivote(const Navegador *nav)
{
    int ax = 0, ay = 0, bx = 0, by = 0;

    cal_desplazamiento_giro(&nav->cal, CAL_LADO_IZQUIERDA, 90, 0, &ax, &ay, NULL, NULL);
    cal_desplazamiento_giro(&nav->cal, CAL_LADO_DERECHA, 90, 0, &bx, &by, NULL, NULL);
    return ax != 0 || ay != 0 || bx != 0 || by != 0;
}

/* Cuánto debe avanzar de lado (J8): la distancia lateral del centro a la línea al pasar el obstáculo es NAV_ESQ_MARGEN_LATERAL_MM, y en ella cuenta lo que el centro se movió de lado
 * al girar para apartarse (la pose actual ya lo incluye) y lo que se moverá al enderezarse (el modelo para el giro de vuelta). Sin desplazamiento (o si el carro quedó casi mirando
 * a lo largo de la línea) es el tramo de siempre, NAV_TRAMO_ESQUIVAR_CM. */
static int esq_calcular_tramo(Navegador *nav)
{
    int normal = nav->lateral_objetivo_mm;
    long sx, sy, lat_giro, lat_vuelta, por_mm, tramo;
    PoseCarro c, v;

    if (!hay_pivote(nav)) {
        return normal;
    }
    sx = dir_x_mil(nav->salida_rumbo + nav->lado * 90);   /* hacia el lado que esquiva, perpendicular a la línea */
    sy = dir_y_mil(nav->salida_rumbo + nav->lado * 90);
    c = pose_actual(nav);
    lat_giro = ((long)(c.x_mm - nav->salida_x_mm) * sx + (long)(c.y_mm - nav->salida_y_mm) * sy) / 1000;
    por_mm = ((long)dir_x_mil(c.rumbo_grados) * sx + (long)dir_y_mil(c.rumbo_grados) * sy) / 1000;   /* cuánto de cada mm que avanza es de lado (milésimas) */
    if (por_mm < 300) {
        return normal;
    }
    cal_pose_tras_giro(&nav->cal, &c, lado_de_vuelta(nav), grados_de_vuelta(nav), &v);
    lat_vuelta = ((long)(v.x_mm - c.x_mm) * sx + (long)(v.y_mm - c.y_mm) * sy) / 1000;
    tramo = ((long)nav->lateral_objetivo_mm - lat_giro - lat_vuelta) * 1000 / por_mm;
    if (tramo < NAV_ESQ_TRAMO_MIN_MM) {
        tramo = NAV_ESQ_TRAMO_MIN_MM;
    }
    if (tramo > 2L * normal) {
        tramo = 2L * normal;
    }
    diag_escribir(nav, "ESQUIVE_DIAG tramo lado=%d lateral_giro_mm=%ld lateral_vuelta_mm=%ld tramo_mm=%ld margen_mm=%d", nav->lado, lat_giro, lat_vuelta, tramo, NAV_ESQ_MARGEN_LATERAL_MM);
    return (int)tramo;
}

/* Cuánto cabe hacia adelante antes del obstáculo (mm): la cara según la cercanía con la que frenó, menos un colchón. */
static int libre_hasta_obstaculo_mm(const Navegador *nav)
{
    int cara = nav->tof_dist_mm > 0 ? nav->tof_dist_mm : (nav->salida_cercania >= 3 ? NAV_CARA_CERCANIA_3_MM : NAV_CARA_CERCANIA_2_MM);
    return cara - NAV_ARCO_MARGEN_MM;
}

/* ¿Cabe el giro de salida hacia `lado`? El centro se mueve hacia adelante lo que dice el modelo para NAV_ANGULO_ESQUIVAR grados (si se mueve hacia atrás, siempre cabe). */
static int arco_cabe(Navegador *nav, int lado, int *adelante_mm, int *libre_mm)
{
    int adelante = 0;

    cal_desplazamiento_giro(&nav->cal, lado, NAV_ANGULO_ESQUIVAR, nav->rumbo_grados, NULL, NULL, &adelante, NULL);
    *adelante_mm = adelante;
    *libre_mm = libre_hasta_obstaculo_mm(nav);
    return adelante <= *libre_mm;
}

/* Calcula la vuelta con la pose estimada y el modelo de pivote: el punto de la línea NAV_REINC_ADELANTE_CM cm por delante del punto de salida, el giro hacia él, la distancia y el giro final
 * al rumbo de la línea (ver planear_vuelta: los dos giros desplazan el centro y el plan lo cuenta). Si el carro está a menos de NAV_REINC_DESVIO_MIN_MM de la línea, solo corrige el rumbo
 * (salvo que el desplazamiento de ese giro lo saque de la línea). Si el punto no sirve prueba con otros más adelante. Devuelve 1, o 0 si la vuelta no tiene solución (el llamador pide ayuda). */
static int reinc_calcular(Navegador *nav)
{
    int tramo_pos, tramo_obj = 0, etapa;
    long s_pos, desvio_pos, s_obj, tx = 0, ty = 0;
    PoseCarro p0 = pose_actual(nav), p1;
    PlanVuelta plan;
    int hay_plan = 0, plan_fallido = 0, giro_solo, usar_plan = 0;

    nav->reinc_antes_x_mm = nav->x_mm;
    nav->reinc_antes_y_mm = nav->y_mm;
    nav->reinc_antes_rumbo = nav->rumbo_grados;
    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo_pos, &s_pos, &desvio_pos);
    nav->reinc_desvio_mm = (int)desvio_pos;
    nav->reinc_giro1 = 0;
    nav->reinc_giro2 = 0;
    nav->reinc_dist_mm = 0;
    nav->reinc_solo_rumbo = 1;
    nav->reinc_obj_x_mm = nav->x_mm;
    nav->reinc_obj_y_mm = nav->y_mm;
    nav->reinc_indice_obj = nav->indice_objetivo;
    nav->reinc_rumbo_final = rumbo_del_tramo(nav, tramo_pos);
    for (etapa = 0; etapa < 3; etapa++) {   /* si una etapa no se hace, la red de seguridad espera estar donde está */
        nav->reinc_plan_x_mm[etapa] = nav->x_mm;
        nav->reinc_plan_y_mm[etapa] = nav->y_mm;
    }

    nav->reinc_pasar_primero = 0;
    nav->reinc_rumbo_importa = 1;
    nav->reinc_rumbo_tol = NAV_REINC_GIRO_MIN_GRADOS - 1;
    giro_solo = giro_util(giro_minimo(nav->rumbo_grados, nav->reinc_rumbo_final));
    if (desvio_pos >= NAV_REINC_DESVIO_MIN_MM && nav->fin_obstaculo_mm >= 0 && nav->fin_obstaculo_mm - s_pos >= NAV_PARALELO_MIN_MM) {
        /* Todavía no pasó el obstáculo (pasa tras «toma el control» si el usuario lo dejó al lado o detrás): volver en diagonal 30 cm adelante lo llevaría contra el
         * obstáculo (en la prueba real pidió ayuda 5 veces seguidas por eso). Primero se endereza al rumbo de la línea y avanza en paralelo hasta pasarlo. */
        long avance = nav->fin_obstaculo_mm - s_pos;

        nav->reinc_pasar_primero = 1;
        nav->reinc_dist_mm = (int)avance;
        nav->reinc_giro1 = giro_minimo(nav->rumbo_grados, nav->reinc_rumbo_final);
        p1 = pose_tras_giro_con_signo(nav, p0, giro_util(nav->reinc_giro1));   /* el giro de enderezarse también desplaza el centro */
        nav->reinc_obj_x_mm = p1.x_mm + (int)(dir_x_mil(nav->reinc_rumbo_final) * avance / 1000);
        nav->reinc_obj_y_mm = p1.y_mm + (int)(dir_y_mil(nav->reinc_rumbo_final) * avance / 1000);
        nav->reinc_plan_x_mm[0] = p1.x_mm;
        nav->reinc_plan_y_mm[0] = p1.y_mm;
        nav->reinc_plan_x_mm[1] = nav->reinc_obj_x_mm;
        nav->reinc_plan_y_mm[1] = nav->reinc_obj_y_mm;
        if (nav->borde_reintentos && desvio_pos < nav->lateral_objetivo_mm) {
            long lateral = nav->lateral_objetivo_mm - desvio_pos;
            long ex = nav->reinc_obj_x_mm + dir_x_mil(nav->reinc_rumbo_final + nav->lado * 90) * lateral / 1000;
            long ey = nav->reinc_obj_y_mm + dir_y_mil(nav->reinc_rumbo_final + nav->lado * 90) * lateral / 1000;
            if (planear_vuelta(nav, &p0, ex, ey, nav->reinc_rumbo_final, &plan)) {
                nav->reinc_giro1 = plan.giro1;
                nav->reinc_giro2 = plan.giro2;
                nav->reinc_dist_mm = plan.dist_mm;
                nav->reinc_obj_x_mm = (int)ex; nav->reinc_obj_y_mm = (int)ey;
                nav->reinc_plan_x_mm[0] = plan.p1.x_mm; nav->reinc_plan_y_mm[0] = plan.p1.y_mm;
                nav->reinc_plan_x_mm[1] = plan.q.x_mm; nav->reinc_plan_y_mm[1] = plan.q.y_mm;
                nav->reinc_plan_x_mm[2] = plan.e.x_mm; nav->reinc_plan_y_mm[2] = plan.e.y_mm;
            }
        }
    } else {
        if (desvio_pos < NAV_REINC_DESVIO_MIN_MM && giro_solo != 0) {
            /* Cerca de la línea: solo se endereza, pero ese giro desplaza el centro; si lo saca de la línea, la vuelta se planea completa. */
            long dp, sp;
            int tp;

            p1 = pose_tras_giro_con_signo(nav, p0, giro_solo);
            ubicar_en_linea(nav, p1.x_mm, p1.y_mm, &tp, &sp, &dp);
            usar_plan = (dp >= NAV_REINC_DESVIO_MIN_MM);
        } else {
            usar_plan = (desvio_pos >= NAV_REINC_DESVIO_MIN_MM);
        }
        if (usar_plan) {
            int intento;

            for (intento = 0; intento < NAV_PLAN_INTENTOS && !hay_plan; intento++) {
                long dx, dy, distancia;

                s_obj = s_pos + (long)NAV_REINC_ADELANTE_CM * 10 + (long)intento * NAV_PLAN_MAS_MM;   /* 30 cm por delante de donde el carro ya pasó el obstáculo (y, si no sirve, más adelante) */
                if (nav->obstaculo_pendiente) {
                    /* V2: regreso en DIAGONAL a 45 grados (avanza a lo largo lo mismo que le falta de lado). El paso en paralelo ya dejó atrás el objeto por la cuenta
                     * de la pista; la diagonal mantiene el láser y la cámara mirando hacia donde se va a entrar (un segundo obstáculo junto al punto de regreso se ve
                     * antes de llegar) y sus giros de 45 grados barren la mitad que los de 85 del cruce casi transversal de antes. */
                    long diagonal = (desvio_pos > NAV_REINC_ADELANTE_CM * 10) ? desvio_pos : NAV_REINC_ADELANTE_CM * 10;

                    if (s_pos + diagonal + NAV_DIAGONAL_RESTO_MIN_MM <= nav->ruta_largo_original_mm) {
                        s_obj = s_pos + diagonal + (long)intento * NAV_PLAN_MAS_MM;
                    } else {
                        /* Junto al final dibujado no hay sitio para la diagonal y además un tramo de línea donde afinar la posición: cruce casi transversal, que
                         * entra a la línea antes (el objeto ya quedó atrás por la cuenta de la pista y la mirada comprueba el cruce). */
                        s_obj = s_pos + NAV_PARALELO_MIN_MM + (long)intento * NAV_PLAN_MAS_MM;
                    }
                } else {
                    s_obj = vertice_mas_cercano(nav, s_obj, s_pos + NAV_REINC_VERTICE_MIN_MM);
                }
                punto_en_linea(nav, s_obj, &tx, &ty, &tramo_obj);
                dx = tx - nav->x_mm;
                dy = ty - nav->y_mm;
                distancia = raiz_entera(dx * dx + dy * dy);
                if (distancia < 50) {
                    break;   /* ya está en el punto: solo corrige el rumbo */
                }
                hay_plan = planear_vuelta(nav, &p0, tx, ty, rumbo_del_tramo(nav, tramo_obj), &plan);
                if (hay_plan && nav->obstaculo_pendiente && intento + 1 < NAV_PLAN_INTENTOS && s_obj < distancia_hasta_tramo(nav, nav->n_puntos - 1) &&
                    valor_absoluto_int(plan.giro1) > NAV_VUELTA_GIRO_MAX) {
                    hay_plan = 0;   /* V2: girar más de un cuarto de vuelta junto a lo que se rodea barre el chasis hacia atrás: mejor un punto más adelante */
                    continue;
                }
                if (!hay_plan && s_obj >= distancia_hasta_tramo(nav, nav->n_puntos - 1)) {
                    /* Al final de la ruta el rumbo puede quedar a NAV_RUMBO_FINAL_TOL grados del último tramo: se prueba con 15 grados de un lado y del otro antes de rendirse. */
                    int lado_tol;

                    for (lado_tol = -1; lado_tol <= 1 && !hay_plan; lado_tol += 2) {
                        hay_plan = planear_vuelta(nav, &p0, tx, ty, normalizar_grados(rumbo_del_tramo(nav, tramo_obj) + lado_tol * (NAV_RUMBO_FINAL_TOL * 3 / 4)), &plan);
                        if (hay_plan) {
                            nav->reinc_rumbo_tol = NAV_RUMBO_FINAL_TOL;   /* se llega a 15 grados del último tramo y solo se corrige si el error pasa de 20 */
                        }
                    }
                }
                if (!hay_plan && s_obj >= distancia_hasta_tramo(nav, nav->n_puntos - 1)) {
                    /* El punto es el final de la ruta y no se puede llegar con el rumbo de la línea (el carro ya está muy cerca del final): ahí el rumbo con el que se llega no importa. */
                    hay_plan = planear_hasta_punto(nav, &p0, tx, ty, &plan);
                    intento = NAV_PLAN_INTENTOS;
                    nav->reinc_rumbo_importa = 0;   /* PENDIENTE (J9, punto 5): el giro final de hasta NAV_RUMBO_FINAL_TOL grados desplaza el centro hasta 22 cm; mientras tanto el rumbo no se corrige al final de la ruta */
                }
                plan_fallido = !hay_plan;
            }
        }
        if (hay_plan) {
            nav->reinc_solo_rumbo = 0;
            nav->reinc_obj_x_mm = (int)tx;
            nav->reinc_obj_y_mm = (int)ty;
            nav->reinc_rumbo_final = rumbo_del_tramo(nav, tramo_obj);
            nav->reinc_giro1 = plan.giro1;
            nav->reinc_dist_mm = plan.dist_mm;
            nav->reinc_giro2 = plan.giro2;
            nav->reinc_indice_obj = tramo_obj + 1;
            nav->reinc_plan_x_mm[0] = plan.p1.x_mm;
            nav->reinc_plan_y_mm[0] = plan.p1.y_mm;
            nav->reinc_plan_x_mm[1] = plan.q.x_mm;
            nav->reinc_plan_y_mm[1] = plan.q.y_mm;
            nav->reinc_plan_x_mm[2] = plan.e.x_mm;
            nav->reinc_plan_y_mm[2] = plan.e.y_mm;
        } else if (usar_plan && plan_fallido) {
            diag_escribir(nav, "REINC_DIAG sin_solucion pose_antes=(%d,%d,%d) desvio_mm=%d rumbo_linea=%d: ningun punto de la linea se alcanza con los giros de pivote",
                          nav->x_mm, nav->y_mm, nav->rumbo_grados, nav->reinc_desvio_mm, nav->reinc_rumbo_final);
            return 0;
        }
    }
    if (nav->reinc_solo_rumbo && !nav->reinc_pasar_primero) {
        nav->reinc_giro1 = giro_minimo(nav->rumbo_grados, nav->reinc_rumbo_final);   /* solo corrige el rumbo */
    }
    if (!giro_ejecutable(nav, nav->reinc_giro1)) {
        nav->reinc_giro1 = 0;     /* un giro tan chico no se hace: el error de la estimación es mayor (y con giroscopio, uno menor que el mínimo del firmware iría por tiempo) */
    }
    if (!giro_ejecutable(nav, nav->reinc_giro2)) {
        nav->reinc_giro2 = 0;
    }
    if (giroscopio_sano(nav) && nav->reinc_rumbo_tol < nav->giro_fw_min) {
        nav->reinc_rumbo_tol = nav->giro_fw_min;   /* no se vuelve a planear por un error de rumbo que el giro mínimo no puede corregir */
    }
    if (nav->reinc_solo_rumbo && !nav->reinc_pasar_primero) {   /* solo rumbo: la pose planeada es la de después de enderezarse */
        p1 = pose_tras_giro_con_signo(nav, p0, nav->reinc_giro1);
        nav->reinc_plan_x_mm[0] = p1.x_mm;
        nav->reinc_plan_y_mm[0] = p1.y_mm;
    }
    diag_escribir(nav, "REINC_DIAG plan pose_antes=(%d,%d,%d) salida=(%d,%d,%d) desvio_mm=%d solo_rumbo=%d objetivo=(%d,%d) dist_mm=%d giro1=%d giro2=%d rumbo_linea=%d esquivadas=%d pasar_primero=%d fin_obstaculo=%d",
                  nav->x_mm, nav->y_mm, nav->rumbo_grados, nav->salida_x_mm, nav->salida_y_mm, nav->salida_rumbo, nav->reinc_desvio_mm, nav->reinc_solo_rumbo,
                  nav->reinc_obj_x_mm, nav->reinc_obj_y_mm, nav->reinc_dist_mm, nav->reinc_giro1, nav->reinc_giro2, nav->reinc_rumbo_final, nav->esquivadas,
                  nav->reinc_pasar_primero, nav->fin_obstaculo_mm);
    if (hay_plan) {
        diag_escribir(nav, "REINC_DIAG pivote giro1_pose=(%d,%d,%d) previo=(%d,%d) final=(%d,%d,%d) correcciones=%d", plan.p1.x_mm, plan.p1.y_mm, plan.p1.rumbo_grados,
                      plan.q.x_mm, plan.q.y_mm, plan.e.x_mm, plan.e.y_mm, plan.e.rumbo_grados, nav->reinc_correcciones);
    }
    return 1;
}

/* ¿El carro ya está más allá del punto final de la ruta (por NAV_REINC_PASADO_MM o más, medido a lo largo del último tramo)? Entonces ya no hay a dónde volver. */
static int reinc_ya_paso_el_final(const Navegador *nav)
{
    long fx, fy, vx, vy, largo, a_lo_largo, lado;

    if (nav->n_puntos < 2) {
        return 0;
    }
    fx = punto_ruta_x(nav, nav->n_puntos - 1);
    fy = punto_ruta_y(nav, nav->n_puntos - 1);
    vx = fx - punto_ruta_x(nav, nav->n_puntos - 2);
    vy = fy - punto_ruta_y(nav, nav->n_puntos - 2);
    largo = raiz_entera(vx * vx + vy * vy);
    if (largo <= 0) {
        return 0;
    }
    a_lo_largo = ((nav->x_mm - fx) * vx + (nav->y_mm - fy) * vy) / largo;   /* positivo = más allá del punto final, siguiendo la dirección del último tramo */
    lado = ((nav->x_mm - fx) * (-vy) + (nav->y_mm - fy) * vx) / largo;   /* distancia lateral a la línea del último tramo (con signo) */
    if (lado < 0) {
        lado = -lado;
    }
    return a_lo_largo >= NAV_REINC_PASADO_MM && a_lo_largo <= NAV_REINC_PASADO_MAX_MM && lado <= NAV_REINC_PASADO_LADO_MM;
}

/* Siguiente fase de MOVIMIENTO después de `actual` (NAV_RF_PLAN, GIRO1, AVANZAR o GIRO2); -1 si ya no queda ninguna (las que no hacen falta se saltan). */
static int reinc_siguiente_movimiento(const Navegador *nav, int actual)
{
    if (actual < NAV_RF_GIRO1 && nav->reinc_giro1 != 0) {
        return NAV_RF_GIRO1;
    }
    if (actual < NAV_RF_AVANZAR && nav->reinc_dist_mm > 0) {
        return NAV_RF_AVANZAR;
    }
    if (actual < NAV_RF_GIRO2 && nav->reinc_giro2 != 0) {
        return NAV_RF_GIRO2;
    }
    return -1;
}

/* Vuelve a planear la vuelta desde donde está (una corrección de la red de seguridad). Si el plan anterior ya había elegido un punto de la línea más adelante, ese punto es ahora el
 * objetivo de la ruta: un plan nuevo que solo corrige el rumbo no debe volver al punto de antes del obstáculo. */
static void reinc_replanear(Navegador *nav, uint32_t ahora_ms)
{
    if (!nav->reinc_solo_rumbo) {
        nav->indice_objetivo = nav->reinc_indice_obj;
    }
    nav->reinc_correcciones++;
    reinc_cambiar_fase(nav, NAV_RF_PLAN, ahora_ms);
}

/* Al terminar la vuelta el siguiente punto de la ruta no puede quedar ATRÁS del carro. Mientras esquivaba o pasaba el obstáculo en paralelo el carro avanzó a lo largo de la línea, pero
 * `indice_objetivo` se queda en el punto al que iba al detenerse (si la vuelta solo corrige el rumbo ni se toca): con ese índice viejo el carro daba media vuelta de 180 grados y recorría la
 * ruta hacia atrás (prueba real del 16 oct: «se volvió loco» al final). Se saltan los puntos que el carro ya pasó por NAV_REINC_PUNTO_PASADO_MM o más (medido a lo largo de la ruta). Si no
 * queda ninguno, la ruta ya terminó. Solo cuenta con el carro cerca de la línea (a 30 cm o menos): lejos de ella la posición a lo largo de la ruta no es confiable. */
static void reinc_saltar_puntos_pasados(Navegador *nav)
{
    int tramo, saltados = 0;
    long s, desvio;

    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s, &desvio);
    if (desvio > 300) {
        return;
    }
    while (nav->indice_objetivo < nav->n_puntos && distancia_hasta_tramo(nav, nav->indice_objetivo) + NAV_REINC_PUNTO_PASADO_MM <= s) {
        nav->indice_objetivo++;
        saltados++;
    }
    if (saltados > 0) {
        diag_escribir(nav, "REINC_DIAG saltados=%d puntos de la ruta que el carro ya paso (s=%ld mm): siguiente_punto=%d", saltados, s, nav->indice_objetivo);
    }
}

/* Terminó la vuelta: anota la pose final, se dirige al siguiente punto de la ruta y sigue (SIGUIENDO). */
static ComandoNav reinc_terminar(Navegador *nav, uint32_t ahora_ms)
{
    long s, desvio;
    int tramo;

    if (nav->reinc_pasar_primero) {   /* solo pasó el obstáculo en paralelo: ahora sí calcula la vuelta a la línea */
        nav->reinc_pasar_primero = 0;
        diag_escribir(nav, "REINC_DIAG paso_el_obstaculo pose=(%d,%d,%d)", nav->x_mm, nav->y_mm, nav->rumbo_grados);
        entrar_reincorporando(nav, ahora_ms);
        return NAV_CMD_STOP;
    }
    if (nav->obstaculo_pendiente) {
        ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s, &desvio);
        if (s < nav->fin_obstaculo_mm - NAV_FIN_OBSTACULO_TOL_MM && desvio >= NAV_REINC_DESVIO_MIN_MM) {
            /* Todavía al costado y sin haber pasado: más carril. (V2: con tolerancia, y solo si el carro sigue fuera de la línea. Si ya está SOBRE la línea no hay
             * carril que alargar: decide la verificación del frente, y lo que haya delante se trata como obstáculo de la ruta.) */
            extender_rebase(nav, ahora_ms);
            return NAV_CMD_STOP;
        }
        if (!verificar_frente(nav, ahora_ms, NAV_CAUSA_OBSTACULO_NO_PASADO)) {
            if (nav->estado == NAV_REINCORPORANDO && nav->verificando_frente) nav->reinc_fase = NAV_RF_VERIFICAR;
            return NAV_CMD_STOP;
        }
        nav->obstaculo_pendiente = 0;
        nav->esquivadas_obstaculo = 0;   /* V2: este obstáculo quedó atrás y verificado: el siguiente tiene su propio presupuesto */
        if (s - nav->fin_obstaculo_mm >= 250 && nav->atras_conocido_mm < NAV_ATRAS_TRAS_RODEO_MM) {
            /* El carro entró a la línea al menos 25 cm más allá de donde terminó de pasar el objeto (que ya llevaba su margen): ese tramo de línea, justo detrás,
             * está libre. Sirve si hay que retroceder porque lo siguiente quedó demasiado cerca para girar. */
            nav->atras_conocido_mm = NAV_ATRAS_TRAS_RODEO_MM;
        }
        if (nav->override_largo_mm == 0 && nav->extension_ruta_mm > 0 && s + NAV_OVERRIDE_TOL_MM >= nav->ruta_largo_original_mm) {
            /* El rodeo obligó a prolongar la última recta y el carro ya entró a la línea más allá del destino dibujado: la ruta termina aquí. */
            diag_escribir(nav, "REINC_DIAG fin tras_el_destino s=%ld destino=%ld: el rodeo termino mas alla del final dibujado", s, nav->ruta_largo_original_mm);
            nav->desde_ayuda = 0;
            nav->indice_objetivo = nav->n_puntos;
            nav->estado = NAV_TERMINADO;
            return NAV_CMD_STOP;
        }
        if (nav->override_largo_mm == 0 && nav->extension_ruta_mm > 0) {
            /* V2: la última recta se había prolongado por si el rodeo terminaba más allá del destino, pero el carro volvió a la línea ANTES del destino dibujado:
             * la prolongación se quita y la ruta termina donde el usuario la dibujó (antes seguía hasta el final prolongado, hasta 60 cm de más). */
            int de_mas = nav->extension_ruta_mm / MM_POR_CELDA;

            if (de_mas > 0 && nav->n_puntos - de_mas >= 2) {
                nav->n_puntos -= de_mas;
                nav->extension_ruta_mm = 0;
                nav->ruta_largo_mm = distancia_hasta_tramo(nav, nav->n_puntos - 1);
                if (nav->reinc_indice_obj > nav->n_puntos - 1) {
                    nav->reinc_indice_obj = nav->n_puntos - 1;
                }
                if (nav->indice_objetivo > nav->n_puntos - 1) {
                    nav->indice_objetivo = nav->n_puntos - 1;
                }
                diag_escribir(nav, "REINC_DIAG prolongacion_quitada puntos=%d: volvio a la linea antes del destino dibujado", de_mas);
            }
        }
        diag_escribir(nav, "BORDE_DIAG obstaculo_superado esquivadas_ruta=%d laser_mm=%d laser_valido=%d", nav->esquivadas, nav->tof_mm, nav->tof_valida);
    }
    nav->desde_ayuda = 0;
    if (!nav->reinc_solo_rumbo) {
        nav->indice_objetivo = nav->reinc_indice_obj;
    }
    if (!nav->sin_arreglo_indice) {
        reinc_saltar_puntos_pasados(nav);
    }
    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s, &desvio);
    diag_escribir(nav, "REINC_DIAG fin pose_despues=(%d,%d,%d) desvio_mm=%d siguiente_punto=%d", nav->x_mm, nav->y_mm, nav->rumbo_grados, (int)desvio, nav->indice_objetivo);
    if (nav->indice_objetivo >= nav->n_puntos) {
        nav->estado = NAV_TERMINADO;   /* ya pasó el último punto de la ruta */
        return NAV_CMD_STOP;
    }
    if (planificar(nav) != NAV_OK) {
        ir_a_error(nav, NAV_MOTIVO_PLAN);
        return NAV_CMD_STOP;
    }
    {
        int giro_loco = giro_fuera_de_ruta(nav);

        if (giro_loco > 0) {   /* la ruta que se retoma pide un giro enorme que la propia ruta no tiene: no se ejecuta */
            char que[48];

            snprintf(que, sizeof que, "giro_de_%d_grados_fuera_de_la_ruta", giro_loco);
            nav->n_pasos = 0;
            frenar_por_locura(nav, NAV_CAUSA_GIRO_LOCO, que);
            return NAV_CMD_STOP;
        }
    }
    nav->estado = NAV_SIGUIENDO;
    return NAV_CMD_STOP;
}

/* La mayor cercanía (1 a 3) con la que algo dentro del corredor frenó al carro. Dice a qué distancia estaba el obstáculo cuando se detuvo. */
static int cercania_que_freno(const Navegador *nav)
{
    int z, mayor = (nav->cerc_detenido_max > 1) ? nav->cerc_detenido_max : 1;   /* la mayor desde que se detuvo, no solo la del último cuadro */

    for (z = 0; z < OBS_ZONAS; z++) {
        if (nav->zona_frena[z] && nav->zona_cerc_frena[z] > mayor) {
            mayor = nav->zona_cerc_frena[z];
        }
    }
    return mayor;
}

/* Amplia solo la ultima recta: nunca recorta el rebase para encajarlo en una
 * ruta corta. El limite de extension y el de intentos acotan la exploracion. */
static int extender_recta_final(Navegador *nav, long hasta)
{
    int n = nav->n_puntos, dc, df;
    if (n < 2 || nav->override_largo_mm) return 0;
    dc = nav->ruta[n - 1].columna - nav->ruta[n - 2].columna;
    df = nav->ruta[n - 1].fila - nav->ruta[n - 2].fila;
    while (hasta + NAV_REINC_ADELANTE_CM * 10 > distancia_hasta_tramo(nav, nav->n_puntos - 1) &&
           nav->n_puntos < MAX_PUNTOS && nav->extension_ruta_mm < NAV_EXTENSION_RUTA_MM) {
        n = nav->n_puntos;
        nav->ruta[n].columna = nav->ruta[n - 1].columna + dc;
        nav->ruta[n].fila = nav->ruta[n - 1].fila + df;
        nav->n_puntos++;
        nav->extension_ruta_mm += MM_POR_CELDA;
    }
    nav->ruta_largo_mm = distancia_hasta_tramo(nav, nav->n_puntos - 1);
    return hasta <= nav->ruta_largo_mm - NAV_PARALELO_FIN_MM;
}

/* Extremo inicial: pose + sensor + distancia medida + primer tramo + margen.
 * Devuelve -1 si no cabe en la recta disponible; nunca reduce esa distancia para terminar la ruta. */
static long calcular_fin_obstaculo(Navegador *nav, int cercania)
{
    int tramo, k, rumbo;
    long s_pos, desvio, hasta, limite;
    int cara = nav->tof_dist_mm > 0 ? nav->tof_dist_mm : (cercania >= 3 ? NAV_CARA_CERCANIA_3_MM : NAV_CARA_CERCANIA_2_MM);
    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s_pos, &desvio);
    nav->cara_obstaculo_s_mm = (int)s_pos + NAV_SENSOR_FRENTE_MM + cara;
    hasta = nav->cara_obstaculo_s_mm + NAV_BORDEAR_TRAMO_MM + (nav->sin_margen_paso_j16 ? NAV_PASO_MARGEN_J15_MM : NAV_PASO_MARGEN_MM);
    if (nav->n_puntos < 2) {
        return s_pos;
    }
    rumbo = rumbo_del_tramo(nav, tramo);
    k = tramo;
    while (k + 1 <= nav->n_puntos - 2 && rumbo_del_tramo(nav, k + 1) == rumbo) {
        k++;   /* los tramos siguientes con el mismo rumbo son la misma recta */
    }
    limite = distancia_hasta_tramo(nav, k + 1);
    if (k + 1 >= nav->n_puntos - 1) {
        limite -= NAV_PARALELO_FIN_MM;
    }
    if (hasta > limite && k + 1 == nav->n_puntos - 1 && nav->override_largo_mm == 0) {
        extender_recta_final(nav, hasta);
        limite = distancia_hasta_tramo(nav, nav->n_puntos - 1) - NAV_PARALELO_FIN_MM;
    }
    return hasta > limite ? -1 : hasta; /* nunca truncar el rebase */
}

/* Cuánto debe avanzar en paralelo para pasar el obstáculo: hasta fin_obstaculo_mm (ver calcular_fin_obstaculo) menos lo que ya avanzó. 0 si no hace falta. */
static int calcular_paralelo_mm(const Navegador *nav)
{
    int tramo;
    long s_pos, desvio;

    if (nav->fin_obstaculo_mm < 0) {
        return 0;
    }
    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s_pos, &desvio);
    return (nav->fin_obstaculo_mm - s_pos >= NAV_PARALELO_MIN_MM) ? (int)(nav->fin_obstaculo_mm - s_pos) : 0;
}

/* V2: ¿sigue a la vista el costado del objeto (del lado de la línea)? Celdas en el borde lejano de ese lado, o la red en esa zona. Si se ve, la pista del paso en
 * paralelo vuelve a empezar desde aquí: deja el nuevo final en fin_obstaculo_mm y devuelve cuánto falta avanzar desde ahora (mm); -1 si no hay que alargar nada.
 * `hecho_mm` es lo que ya lleva avanzado este paso (para el tope NAV_PASO_PARALELO_MAX_MM). */
static long costado_a_la_vista(Navegador *nav, uint32_t ahora_ms, long hecho_mm)
{
    int lado_linea = (nav->lado < 0) ? 1 : 0;   /* se rodea por la izquierda: el objeto queda a la derecha */
    int zona_linea = (nav->lado < 0) ? ZONA_DERECHA : ZONA_IZQUIERDA;
    int tramo;
    long s, desvio, objetivo;

    if (!(nav->borde_lejos[lado_linea] >= NAV_FLANCO_CELDAS_MIN || (nav->fuentes_vision[zona_linea] & 2)) ||
        (uint32_t)(ahora_ms - nav->t_ultimo_cuadro_ms) > (uint32_t)NAV_MAX_HUECO_VIDEO_MS) {
        return -1;
    }
    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s, &desvio);
    objetivo = s + pista_tras_la_vista_mm(nav);
    if (objetivo <= nav->fin_obstaculo_mm || hecho_mm + (objetivo - s) > NAV_PASO_PARALELO_MAX_MM) {
        return -1;
    }
    if (objetivo + NAV_REINC_ADELANTE_CM * 10 > nav->ruta_largo_mm) {
        extender_recta_final(nav, objetivo);
    }
    if (objetivo > nav->ruta_largo_mm - NAV_PARALELO_FIN_MM) {
        return -1;
    }
    if (nav->flanco_vistas == 0) {
        diag_escribir(nav, "BORDE_DIAG costado_a_la_vista s=%ld celdas_borde=%d red=%d: el paso en paralelo se alarga", s, nav->borde_lejos[lado_linea],
                      (nav->fuentes_vision[zona_linea] & 2) ? 1 : 0);
    }
    nav->flanco_vistas++;
    nav->flanco_s_mm = s;
    nav->fin_obstaculo_mm = (int)objetivo;
    return objetivo - s;
}

/* Elige hacia dónde apartarse: -1 izquierda, +1 derecha, 0 ninguno. Solo cuentan los lados SIN obstáculo (zona libre). Si los dos
 * están libres, se prefiere el que tiene MENOS celdas de obstáculo en la imagen (más espacio libre); con el mismo número, la izquierda
 * (como siempre). Si ninguno está libre no se gira: el carro espera y, pasado NAV_TIMEOUT_ESQUIVAR_MS, ERROR (pared que ocupa todo el frente). */
static int elegir_lado(const Navegador *nav)
{
    int izquierda_libre = !nav->zona_hay[ZONA_IZQUIERDA];
    int derecha_libre = !nav->zona_hay[ZONA_DERECHA];

    if (izquierda_libre && derecha_libre) {
        return (nav->zona_celdas[ZONA_DERECHA] < nav->zona_celdas[ZONA_IZQUIERDA]) ? 1 : -1;
    }
    if (izquierda_libre) {
        return -1;
    }
    return derecha_libre ? 1 : 0;
}

/* Retrocede solamente por el tramo recto que acaba de recorrer. No hay sensor
 * trasero: esta evidencia solo vale en un entorno estatico y se borra al girar. */
static int intentar_escape_atras(Navegador *nav)
{
    int falta;

    if (nav->atras_conocido_mm < NAV_RETROCESO_CM * 10 + 30) return 0;
    /* V2: lo que falta para que quepa el giro de salida (más 30), nunca menos que los 15 cm de siempre ni más de lo que se sabe libre detrás menos 50. */
    falta = nav->arco_adelante_mm - nav->arco_libre_mm + 30;
    nav->retroceso_mm = NAV_RETROCESO_CM * 10;
    if (falta > nav->retroceso_mm) {
        nav->retroceso_mm = falta;
    }
    if (nav->retroceso_mm > nav->atras_conocido_mm - 50) {
        nav->retroceso_mm = nav->atras_conocido_mm - 50;
    }
    if (nav->retroceso_mm > NAV_RETROCESO_MAX_MM) {
        nav->retroceso_mm = NAV_RETROCESO_MAX_MM;
    }
    if (nav->retroceso_mm < NAV_RETROCESO_CM * 10) {
        nav->retroceso_mm = NAV_RETROCESO_CM * 10;
    }
    nav->escape_atras = 1;
    diag_escribir(nav, "BORDE_DIAG retroceso_conocido=%d mm", nav->retroceso_mm);
    return 1;
}

/* STOP antes de planear. Distancia laser cuando existe; si no, banda visual
 * coincidente. Ambos lados ocupados: esperar y agotar el retroceso conocido. */
static ComandoNav tick_detenido(Navegador *nav, uint32_t ahora_ms)
{
    uint32_t espera = ahora_ms - nav->t_detenido_ms;

    ajustar_paso_tras_parar(nav);
    if (nav->escape_atras) {
        int ms_escape = cal_cm_a_ms_atras(&nav->cal, (nav->retroceso_mm + 5) / 10);

        if (ms_escape > NAV_RETROCESO_MAX_MS) ms_escape = NAV_RETROCESO_MAX_MS;
        if (nav->cmd_actual != NAV_CMD_REVERSE || nav->seg_ms < ms_escape) return NAV_CMD_REVERSE;
        cambiar_comando(nav, NAV_CMD_STOP);
        nav->escape_atras = 0;
        if (nav->tof_dist_mm > 0) {
            nav->tof_dist_mm += nav->retroceso_mm;   /* V2: la distancia medida era de antes de retroceder (con TOF_PARO o un vistazo no se vuelve a medir) */
        }
        nav->t_detenido_ms = ahora_ms;
        nav->frames_recibidos = 0;
        planificar(nav);
        return NAV_CMD_STOP;
    }
    /* Una medición cercana O consenso visual fresco autoriza; el desacuerdo no bloquea. */
    if (!nav->tof_paro_activo && !tof_dice_cerca(nav, ahora_ms) && !nav->zona_frena[ZONA_CENTRO] &&
        !nav->zona_frena[ZONA_IZQUIERDA] && !nav->zona_frena[ZONA_DERECHA] && !nav->cierre_reintento) {
        if (libre_confirmado(nav, ahora_ms)) {
            nav->estado = NAV_SIGUIENDO;
            nav->tof_paro_activo = 0;
            return tick_siguiendo(nav, ahora_ms);
        }
        return NAV_CMD_STOP;
    }
    if (libre_confirmado(nav, ahora_ms) && nav->volver_a_reinc && !nav->tof_paro_activo && !nav->cierre_reintento && !tof_dice_cerca(nav, ahora_ms)) {
        /* Se detuvo mientras volvía a la línea y el frente ya está libre: vuelve a calcular la vuelta desde donde quedó. */
        nav->volver_a_reinc = 0;
        nav->frames_bloqueado = 0;
        nav->bloqueo_inmediato = 0;
        nav->bloqueo_cerc3 = 0;
        entrar_reincorporando(nav, ahora_ms);
        return NAV_CMD_STOP;
    }
    if (libre_confirmado(nav, ahora_ms) && !nav->tof_paro_activo && !nav->cierre_reintento && !tof_dice_cerca(nav, ahora_ms)) {   /* tras TOF_PARO la cámara no puede «liberar» el frente: el láser vio algo a menos de 12 cm */
        nav->estado = NAV_SIGUIENDO;   /* el obstáculo se fue: continuar donde iba */
        ajustar_paso_tras_parar(nav);
        nav->frames_bloqueado = 0;
        nav->bloqueo_inmediato = 0;
        nav->bloqueo_cerc3 = 0;
        return tick_siguiendo(nav, ahora_ms);
    }
    /* Tras TOF_PARO también hace falta cámara para elegir el lado; nunca se usa un lado por defecto a ciegas. */
    if (espera >= NAV_PAUSA_MS && !nav->tof_paro_activo && !nav->cierre_reintento && !tof_dice_cerca(nav, ahora_ms) &&
        !(tof_fresco(nav, ahora_ms) && nav->tof_valida && nav->tof_mm > 0 && nav->tof_mm <= OBSTACULO_LASER_MM) &&
        nav->coinc_detenido < NAV_COINCIDENCIA_CUADROS_ESQUIVAR && espera < (uint32_t)NAV_TIMEOUT_ESQUIVAR_MS) {
        /* V2: lo único que sostiene el bloqueo es la coincidencia de color y red, sin láser: todavía no se han juntado los cuadros que hacen falta para esquivar.
         * Se sigue esperando (si deja de verse, arriba se reanuda la ruta). */
        return NAV_CMD_STOP;
    }
    if (espera >= NAV_PAUSA_MS && (nav->frames_recibidos >= 1 || (nav->tof_paro_activo && espera >= NAV_TOF_ESPERA_LADO_MS))) {
        if (nav->frames_recibidos < 1) {
            ir_a_error_causa(nav, NAV_MOTIVO_SIN_VIDEO, NAV_CAUSA_SIN_VIDEO);
            return NAV_CMD_STOP;
        }
        nav->salida_cercania = cercania_que_freno(nav);
        if (!nav->tof_paro_activo) {
            nav->tof_dist_mm = (tof_fresco(nav, ahora_ms) && nav->tof_valida && nav->tof_mm > 0 &&
                               nav->tof_mm <= (nav->cierre_reintento ? NAV_TOF_LIBRE_MM : NAV_REACCION_MAX_MM)) ? nav->tof_mm : 0;
        }
        nav->lado = elegir_lado(nav);
        if (nav->lado != 0) {
            /* El giro de salida mueve el centro hacia adelante (pivote): si no cabe antes del obstáculo se prueba el otro lado y, si tampoco, se pide ayuda en vez de chocar. */
            int adelante_mm, libre_mm;

            if (!arco_cabe(nav, nav->lado, &adelante_mm, &libre_mm)) {
                int otro = -nav->lado;
                int otro_libre = (otro < 0) ? !nav->zona_hay[ZONA_IZQUIERDA] : !nav->zona_hay[ZONA_DERECHA];
                int otro_adelante, otro_libre_mm;

                diag_escribir(nav, "ESQUIVE_DIAG plan lado=%d giro_adelante_mm=%d libre_mm=%d cercania=%d cabe=0", nav->lado, adelante_mm, libre_mm, nav->salida_cercania);
                if (otro_libre && arco_cabe(nav, otro, &otro_adelante, &otro_libre_mm)) {
                    diag_escribir(nav, "ESQUIVE_DIAG plan cambia_de_lado lado=%d giro_adelante_mm=%d libre_mm=%d", otro, otro_adelante, otro_libre_mm);
                    nav->lado = otro;
                } else {
                    nav->arco_adelante_mm = adelante_mm;
                    nav->arco_libre_mm = libre_mm;
                    if (intentar_escape_atras(nav)) return NAV_CMD_REVERSE;
                    ir_a_error_causa(nav, NAV_MOTIVO_ESQUIVANDO, NAV_CAUSA_ARCO);
                    return NAV_CMD_STOP;
                }
            }
            {
                /* V2, BARRIDO DEL GIRO DE SALIDA (ver navegador.h): con lo confirmado más cerca que el arco de la esquina y camino conocido detrás, primero retrocede.
                 * Solo para lo que confirmó un VISTAZO (algo a un lado: una pata, una caja corrida); un obstáculo de frente sigue con la cuenta de siempre. */
                int barrido = ((nav->lado > 0) ? NAV_BARRIDO_DER_MM : NAV_BARRIDO_IZQ_MM) + NAV_BARRIDO_HOLGURA_MM;
                int cara = libre_hasta_obstaculo_mm(nav) + NAV_ARCO_MARGEN_MM;

                if (nav->vistazo_confirmado != 0 && !nav->vistazo_retrocedio && nav->atras_conocido_mm >= NAV_RETROCESO_CM * 10 + 30) {
                    nav->vistazo_retrocedio = 1;
                    nav->arco_adelante_mm = NAV_RETROCESO_MAX_MM;
                    nav->arco_libre_mm = 30;             /* intentar_escape_atras retrocede (adelante - libre + 30): aquí, el máximo */
                    diag_escribir(nav, "ESQUIVE_DIAG plan vistazo lado=%d eco_mm=%d barrido_mm=%d: retrocede antes de girar", nav->lado, cara, barrido);
                    if (intentar_escape_atras(nav)) return NAV_CMD_REVERSE;
                }
            }
            if (nav->esquivadas_obstaculo >= NAV_MAX_ESQUIVADAS || nav->esquivadas >= NAV_MAX_ESQUIVADAS_RUTA) {
                ir_a_error(nav, NAV_MOTIVO_MAX_ESQUIVADAS);
                return NAV_CMD_STOP;
            }
            nav->fin_obstaculo_mm = (int)calcular_fin_obstaculo(nav, nav->salida_cercania);
            if (nav->fin_obstaculo_mm < 0) {
                ir_a_error_causa(nav, NAV_MOTIVO_ESQUIVANDO, NAV_CAUSA_OBSTACULO_NO_PASADO);
                return NAV_CMD_STOP;
            }
            if (nav->tof_paro_activo) {
                diag_escribir(nav, "SENS_DIAG tof_paro decide lado=%s por=%s laser_mm=%d celdas_izq=%d celdas_der=%d", (nav->lado < 0) ? "izquierda" : "derecha",
                              (nav->frames_recibidos >= 1) ? "camara" : "ultima_rejilla_o_izquierda", nav->tof_dist_mm, nav->zona_celdas[ZONA_IZQUIERDA], nav->zona_celdas[ZONA_DERECHA]);
                nav->tof_paro_activo = 0;
            }
            diag_obs(nav, "esquiva");
            nav->esquivadas++;
            nav->esquivadas_obstaculo++;
            nav->cierre_reintento = 0;
            if (!nav->obstaculo_pendiente) {   /* V2: un obstáculo NUEVO empieza con el rodeo de partida (los ensanches eran del anterior) */
                nav->borde_reintentos = 0;
                nav->lateral_objetivo_mm = NAV_ESQ_MARGEN_LATERAL_MM;
                nav->lateral_tope_mm = NAV_BORDEAR_LATERAL_MAX;
            }
            nav->mirada_fallida = nav->mirada_previa_fallida = 0;
            nav->obstaculo_pendiente = 1;
            nav->volver_a_reinc = 0;
            nav->salida_x_mm = nav->x_mm;          /* de aquí sale de la línea: a este punto se mide después el regreso */
            nav->salida_y_mm = nav->y_mm;
            nav->salida_rumbo = nav->rumbo_grados;
            nav->fase = NAV_FASE_GIRAR;
            nav->ms_fase = 0;
            nav->reintentos = 0;
            nav->angulo_giro = NAV_ANGULO_ESQUIVAR;
            nav->angulo_total = 0;
            nav->giro_extra_grados = 0;
            nav->salida_linea_rumbo = nav->rumbo_grados;
            nav->salida_linea_ok = 0;
            if (nav->n_puntos >= 2) {
                /* V2: si el carro quedó torcido respecto de la línea (se detuvo en pleno vistazo), el rodeo se hace respecto de la LÍNEA: el primer giro lleva los
                 * grados de más o de menos y el paso en paralelo sale paralelo a la ruta, no a como quedó el carro. */
                int tramo_l, torcido;
                long s_l, d_l;

                ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo_l, &s_l, &d_l);
                torcido = giro_minimo(rumbo_del_tramo(nav, tramo_l), nav->rumbo_grados);   /* + = mira a la derecha de la línea */
                nav->salida_linea_rumbo = rumbo_del_tramo(nav, tramo_l);
                nav->salida_linea_ok = 1;
                if (valor_absoluto_int(torcido) >= NAV_SALIDA_TORCIDO_MIN && valor_absoluto_int(torcido) <= NAV_SALIDA_TORCIDO_MAX) {
                    nav->salida_rumbo = rumbo_del_tramo(nav, tramo_l);
                    nav->giro_extra_grados = -nav->lado * torcido;
                    nav->angulo_giro = NAV_ANGULO_ESQUIVAR + nav->giro_extra_grados;
                    diag_escribir(nav, "ESQUIVE_DIAG salida torcido=%d lado=%d primer_giro=%d", torcido, nav->lado, nav->angulo_giro);
                }
            }
            nav->rodeo_por_vistazo = (nav->vistazo_confirmado != 0);
            nav->vistazo_confirmado = 0;
            nav->scan_ext_mm = nav->scan_pared_mm = nav->scan_muestras = nav->scan_corte = nav->scan_n = 0;   /* V2: el primer giro mide el ancho con el láser */
            nav->lados_probados = nav->cambio_lado = 0;
            nav->scan_signo = 1;
            nav->ancho_rodeo_mm = 0;
            nav->flanco_vistas = 0;
            nav->estado = NAV_ESQUIVANDO;
            esq_iniciar_giro(nav, ahora_ms);
            return comando_de_fase(nav);
        }
        if (espera >= NAV_TIMEOUT_ESQUIVAR_MS) {
            if (intentar_escape_atras(nav)) return NAV_CMD_REVERSE;
            ir_a_error(nav, NAV_MOTIVO_LADOS_BLOQUEADOS);
        }
    }
    return NAV_CMD_STOP;
}

/* Un reintento de esquive: el frente sigue muy cerca, así que retrocede un poco (ciego), gira del MISMO lado un ángulo extra (45° y luego 90°) y vuelve a mirar.
 * Con NAV_MAX_REINTENTOS agotados: ERROR con la causa concreta. Devuelve el comando de la fase siguiente. */
static ComandoNav esquive_reintentar(Navegador *nav)
{
    nav->ms_fase = 0;
    nav->centro_frames = 0;
    if (nav->reintentos >= NAV_MAX_REINTENTOS) {
        ir_a_error_causa(nav, NAV_MOTIVO_ESQUIVANDO, NAV_CAUSA_REINTENTOS);   /* se agotaron los reintentos y el frente sigue ocupado */
        return NAV_CMD_STOP;
    }
    nav->reintentos++;
    nav->angulo_giro = (nav->reintentos == 1) ? NAV_ANGULO_REINTENTO_1 : NAV_ANGULO_REINTENTO_2;
    if (nav->cmd_actual != NAV_CMD_STOP) {
        nav->fase = NAV_FASE_PAUSA_RETROCEDER;   /* venía avanzando: primero se detiene (no pasa de FORWARD a REVERSE de golpe) */
        return NAV_CMD_STOP;
    }
    nav->fase = NAV_FASE_RETROCEDER;
    return NAV_CMD_REVERSE;
}

/* MIRAR (dentro de ESQUIVANDO): el carro acaba de girar y está en STOP. Antes de avanzar hacia donde apunta hay que MIRAR de verdad, y
 * mirar no es solo leer el último cuadro: con el video del carro ese cuadro todavía muestra el carro girando (llega ~320 ms tarde). Se espera
 * NAV_PAUSA_ASENTAR_MS (el carro termina de moverse) más el retraso de la cámara, y se usa un cuadro que haya llegado DESPUÉS de eso.
 *   - Frente libre (o la cámara no mandó cuadros nuevos a tiempo): sigue la maniobra normal, avanzar de lado.
 *   - Frente muy cerca (zona central, cercanía NAV_CERCANIA_MUY_CERCA): reintento (retroceder + girar más) o, si ya se agotaron, ERROR. */
static ComandoNav tick_mirar(Navegador *nav, uint32_t ahora_ms)
{
    uint32_t espera = ahora_ms - nav->t_mirar_ms;
    uint32_t listo_ms = (uint32_t)(NAV_PAUSA_ASENTAR_MS + nav->latencia_visual_ms);
    int cuadro_nuevo, bloqueado;

    if (espera < listo_ms) {
        return NAV_CMD_STOP;
    }
    /* cuadro_nuevo: el último cuadro llegó después de la espera (es decir, muestra el carro ya quieto). */
    cuadro_nuevo = nav->usa_camara && (uint32_t)(nav->t_ultimo_cuadro_ms - (nav->t_mirar_ms + listo_ms)) < 0x80000000u;
    if (!cuadro_nuevo && espera < listo_ms + NAV_MIRAR_MAX_ESPERA_MS) {
        return NAV_CMD_STOP;   /* todavía no llega un cuadro útil: se sigue esperando (un poco más) */
    }
    bloqueado = cuadro_nuevo && nav->zona_frena[ZONA_CENTRO] && nav->zona_cerc_frena[ZONA_CENTRO] >= NAV_CERCANIA_MUY_CERCA;

    nav->ms_fase = 0;
    if (nav->cambio_lado == 2 || (nav->reintentos == 0 && !nav->borde_reintentos)) {
        /* V2: el desvío lateral sale del ancho que midió el láser durante el giro (más lo que barre el giro de enderezarse). Sin medida (el obstáculo lo confirmó la
         * visión, o el láser no lo alcanzó al girar) vale el de siempre. */
        int otro = (nav->cambio_lado == 2);   /* lo medido es del lado contrario: el carro acaba de dar la vuelta completa y mira otra vez al frente */
        int ancho, lateral = NAV_ESQ_MARGEN_LATERAL_MM, tope = NAV_BORDEAR_LATERAL_MAX;

        escaneo_analizar(nav, otro);   /* en el cambio de lado el último cuarto de vuelta barre del costado hacia el frente */
        if (nav->rodeo_por_vistazo && !otro) {
            /* V2: este rodeo empezó por un vistazo: lo confirmado está del OTRO lado del eje (el láser, de frente, no veía nada) y el giro se aparta de él. Lo que el
             * barrido alcanzó a ver al empezar es eso mismo, no una «cara» que se extienda hacia este lado: no se toma su ancho ni se da el lado por cerrado.
             * Una pared de este lado sí cuenta. */
            nav->scan_ext_mm = 0;
            nav->scan_corte = 0;
        }
        ancho = nav->scan_ext_mm > 0 ? nav->scan_ext_mm + NAV_ESCANEO_RELLENO_MM : 0;
        if (ancho + NAV_ESQ_HOLGURA_VOLVER_MM > lateral) {
            lateral = ancho + NAV_ESQ_HOLGURA_VOLVER_MM;
        }
        if (nav->scan_corte) {
            lateral = NAV_BORDEAR_LATERAL_MAX + 1;   /* no se pudo comprobar dónde termina: se toma como cerrado */
        }
        if (nav->scan_pared_mm > 0 && nav->scan_pared_mm - NAV_ESCANEO_PARED_HOLGURA_MM < tope) {
            tope = nav->scan_pared_mm - NAV_ESCANEO_PARED_HOLGURA_MM;
        }
        diag_escribir(nav, "ESQUIVE_DIAG escaneo lado=%d lecturas=%d con_eco=%d ancho_mm=%d sin_final=%d pared_lateral_mm=%d lateral_mm=%d tope_mm=%d", otro ? -nav->lado : nav->lado,
                      nav->scan_n, nav->scan_muestras, nav->scan_ext_mm, nav->scan_corte, nav->scan_pared_mm, lateral, tope);
        if (otro) {
            int adelante_mm, libre_mm;

            nav->cambio_lado = 0;
            nav->scan_signo = 1;
            if (lateral > tope || !arco_cabe(nav, -nav->lado, &adelante_mm, &libre_mm)) {
                /* Los dos lados cerrados (o el giro de salida no cabe): no hay por dónde. El carro quedó como se detuvo, de frente y sin haberse acercado. */
                ir_a_error_causa(nav, NAV_MOTIVO_LADOS_BLOQUEADOS, NAV_CAUSA_FRENTE_ANCHO);
                return NAV_CMD_STOP;
            }
            nav->lado = -nav->lado;            /* rodeo nuevo por el otro lado, desde donde está */
            nav->giro_extra_grados = 0;
            nav->ancho_rodeo_mm = ancho > 0 ? ancho : 100;
            nav->lateral_tope_mm = tope;
            nav->lateral_objetivo_mm = lateral;
            nav->angulo_total = 0;
            nav->angulo_giro = NAV_ANGULO_ESQUIVAR;
            nav->scan_ext_mm = nav->scan_pared_mm = nav->scan_muestras = nav->scan_corte = nav->scan_n = 0;
            nav->salida_x_mm = nav->x_mm;
            nav->salida_y_mm = nav->y_mm;
            nav->salida_rumbo = nav->rumbo_grados;
            diag_escribir(nav, "ESQUIVE_DIAG cambio_de_lado: el otro lado esta libre, se rodea por lado=%d", nav->lado);
            nav->fase = NAV_FASE_ASENTAR;
            return NAV_CMD_STOP;
        }
        nav->ancho_rodeo_mm = ancho > 0 ? ancho : 100;   /* sin medida: medio objeto de 20 cm (SUPUESTO) */
        nav->lateral_tope_mm = tope;
        if (lateral > tope) {
            /* Pared u objeto más ancho que el rodeo máximo, o sin espacio de ese lado: no se avanza hacia allí. Si todavía no se midió el otro lado, se mide:
             * media vuelta y un cuarto más en el MISMO sentido (sobre la misma rueda, sin acercarse); el último cuarto barre el otro lado con el láser. */
            if (nav->lados_probados == 0) {
                nav->lados_probados = 1;
                nav->cambio_lado = 1;
                nav->angulo_giro = 180;
                diag_escribir(nav, "ESQUIVE_DIAG lado_cerrado lado=%d: se mide el otro lado (vuelta completa en el mismo sentido)", nav->lado);
                nav->fase = NAV_FASE_ASENTAR;
                return NAV_CMD_STOP;
            }
            ir_a_error_causa(nav, NAV_MOTIVO_LADOS_BLOQUEADOS, NAV_CAUSA_FRENTE_ANCHO);
            return NAV_CMD_STOP;
        }
        nav->lateral_objetivo_mm = lateral;
    }
    if (!bloqueado) {
        nav->tramo_mm = esq_calcular_tramo(nav);   /* cuánto avanza de lado, contando lo que el centro se movió al girar y se moverá al enderezarse */
        nav->fase = NAV_FASE_AVANZAR;
        nav->centro_frames = 0;
        return NAV_CMD_FORWARD;
    }
    diag_escribir(nav, "ESQUIVE_DIAG fase=MIRAR cerc=%d zona=CEN celdas=%d reintentos=%d accion=BLOQUEA_reintento",
                  nav->zona_cerc_frena[ZONA_CENTRO], nav->zona_celdas[ZONA_CENTRO], nav->reintentos);
    return esquive_reintentar(nav);
}

/* ESQUIVANDO: gira hacia el lado libre, MIRA, avanza un tramo fijo y gira de vuelta. Si al mirar el frente sigue muy cerca, reintenta
 * (retrocede NAV_RETROCESO_CM, gira más y vuelve a mirar). Mientras avanza de lado solo cuenta un obstáculo CENTRAL de cercanía NAV_ESQ_CERCANIA_BLOQUEA o más,
 * confirmado (los cuadros del giro y los siguientes hasta estabilizarse se ignoran; uno lateral o lejano no aborta): si bloquea, usa los mismos reintentos y
 * solo falla al agotarlos. */
static ComandoNav tick_esquivando(Navegador *nav, uint32_t ahora_ms)
{
    int fase_lista;

    if (nav->tof_pendiente) {   /* TOF_PARO mientras avanzaba de lado: el obstáculo está delante; se trata como el centro bloqueado de siempre */
        nav->tof_pendiente = 0;
        if (nav->fase == NAV_FASE_AVANZAR && nav->cmd_actual == NAV_CMD_FORWARD) {
            diag_escribir(nav, "SENS_DIAG tof_paro fase=AVANZAR laser_mm=%d: reintento de esquive", nav->tof_dist_mm);
            return esquive_reintentar(nav);
        }
        if (nav->fase == NAV_FASE_PARALELO && nav->cmd_actual == NAV_CMD_FORWARD) {
            diag_escribir(nav, "SENS_DIAG tof_paro fase=PARALELO laser_mm=%d: se detiene", nav->tof_dist_mm);
            tof_marcar(nav, nav->tof_dist_mm);
            entrar_detenido(nav, ahora_ms);
            nav->volver_a_reinc = 1;
            nav->frames_bloqueado = 0;
            return NAV_CMD_STOP;
        }
    }
    if (nav->fase == NAV_FASE_MIRAR) {
        return tick_mirar(nav, ahora_ms);
    }
    if (nav->fase == NAV_FASE_PAUSA_PASAR) {   /* quieto un momento después de enderezarse; luego avanza en paralelo hasta pasar el obstáculo, o ya vuelve */
        if (nav->ms_fase + NAV_TICK_MS / 2 < NAV_PAUSA_ASENTAR_MS) {
            return NAV_CMD_STOP;
        }
        nav->paralelo_mm = calcular_paralelo_mm(nav);
        {   /* V2: el costado puede estar a la vista desde aquí: como mínimo, la pista hasta donde el giro de regreso ya no lo alcanza. */
            int pista = pista_tras_la_vista_mm(nav);
            int tramo_v2;
            long s_v2, desvio_v2;

            ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo_v2, &s_v2, &desvio_v2);
            nav->flanco_s_mm = s_v2;
            if (nav->paralelo_mm < pista && s_v2 + pista + NAV_REINC_ADELANTE_CM * 10 > nav->ruta_largo_mm) {
                extender_recta_final(nav, s_v2 + pista);
            }
            if (nav->paralelo_mm < pista && s_v2 + pista <= nav->ruta_largo_mm - NAV_PARALELO_FIN_MM) {
                nav->paralelo_mm = pista;
                nav->fin_obstaculo_mm = (int)(s_v2 + pista);
            }
        }
        nav->ms_fase = 0;
        diag_escribir(nav, "REINC_DIAG pasar paralelo_mm=%d pose=(%d,%d,%d) salida=(%d,%d) cercania_salida=%d", nav->paralelo_mm, nav->x_mm, nav->y_mm, nav->rumbo_grados,
                      nav->salida_x_mm, nav->salida_y_mm, nav->salida_cercania);
        if (nav->paralelo_mm <= 0) {
            entrar_reincorporando(nav, ahora_ms);
            return NAV_CMD_STOP;
        }
        nav->fase = NAV_FASE_PARALELO;
        nav->centro_frames = 0;
        return NAV_CMD_FORWARD;
    }
    if (nav->fase == NAV_FASE_PAUSA_VOLVER || nav->fase == NAV_FASE_PAUSA_RETROCEDER) {   /* quieto un momento entre avanzar y el movimiento siguiente */
        if (nav->ms_fase + NAV_TICK_MS / 2 < NAV_PAUSA_ASENTAR_MS) {
            return NAV_CMD_STOP;
        }
        nav->ms_fase = 0;
        if (nav->fase == NAV_FASE_PAUSA_RETROCEDER) {
            nav->fase = NAV_FASE_RETROCEDER;
            return NAV_CMD_REVERSE;
        }
        nav->fase = NAV_FASE_VOLVER;
        esq_iniciar_giro(nav, ahora_ms);
        return comando_de_fase(nav);
    }
    if (nav->fase == NAV_FASE_ASENTAR) {   /* quieto un momento después de retroceder, y luego gira más */
        if (nav->ms_fase + NAV_TICK_MS / 2 < NAV_PAUSA_ASENTAR_MS) {
            return NAV_CMD_STOP;
        }
        nav->fase = NAV_FASE_GIRAR;
        nav->ms_fase = 0;
        esq_iniciar_giro(nav, ahora_ms);
        return comando_de_fase(nav);
    }

    if ((nav->fase == NAV_FASE_GIRAR || nav->fase == NAV_FASE_VOLVER) && nav->cmd_actual == comando_de_fase(nav)) {
        int grados = (nav->fase == NAV_FASE_GIRAR) ? nav->angulo_giro : grados_de_vuelta(nav);

        fase_lista = giro_terminado(nav, grados, duracion_fase(nav, nav->fase), nav->ms_fase);   /* un giro: con la cámara si se puede */
    } else {
        fase_lista = (nav->ms_fase + NAV_TICK_MS / 2 >= duracion_fase(nav, nav->fase));
    }
    if (fase_lista) {
        nav->ms_fase = 0;
        switch (nav->fase) {
        case NAV_FASE_GIRAR:        /* terminó de girar: queda quieto a mirar si el frente está libre */
            nav->angulo_total += nav->angulo_giro;
            esq_terminar_giro(nav, ahora_ms);
            if (nav->cambio_lado == 1) {   /* V2: media vuelta hecha; falta el último cuarto, que mide el otro lado con el láser */
                nav->cambio_lado = 2;
                nav->angulo_giro = 90;
                nav->scan_signo = -1;
                nav->scan_ext_mm = nav->scan_pared_mm = nav->scan_muestras = nav->scan_corte = nav->scan_n = 0;
                nav->fase = NAV_FASE_ASENTAR;
                return NAV_CMD_STOP;
            }
            nav->fase = NAV_FASE_MIRAR;
            nav->t_mirar_ms = ahora_ms;
            return NAV_CMD_STOP;
        case NAV_FASE_RETROCEDER:   /* terminó de retroceder: quieto un momento y gira más */
            nav->fase = NAV_FASE_ASENTAR;
            return NAV_CMD_STOP;
        case NAV_FASE_AVANZAR:      /* terminó de avanzar de lado: quieto un momento y luego se endereza (sin la pausa el giro variaba 30 a 60 grados) */
            nav->fase = NAV_FASE_PAUSA_VOLVER;
            return NAV_CMD_STOP;
        case NAV_FASE_PARALELO:     /* terminó de pasar el obstáculo: ahora sí vuelve a la línea */
            entrar_reincorporando(nav, ahora_ms);
            return NAV_CMD_STOP;
        default:                    /* VOLVER: terminó de enderezarse; quieto un momento y calcula cuánto avanza en paralelo */
            esq_terminar_giro(nav, ahora_ms);
            nav->fase = NAV_FASE_PAUSA_PASAR;
            return NAV_CMD_STOP;
        }
    }
    if (nav->fase == NAV_FASE_AVANZAR && esquive_centro_confirmado(nav, ahora_ms)) {
        int z = zona_para_diag(nav);

        diag_escribir(nav, "ESQUIVE_DIAG fase=AVANZAR cerc=%d zona=%s celdas=%d reintentos=%d accion=BLOQUEA_reintento",
                      nav->zona_cerc_frena[z], nombre_zona(z), nav->zona_celdas[z], nav->reintentos);
        return esquive_reintentar(nav);
    }
    if (nav->fase == NAV_FASE_PARALELO && nav->cmd_actual == NAV_CMD_FORWARD) {
        long hecho = cal_ms_a_mm(&nav->cal, nav->ms_fase);
        long falta = costado_a_la_vista(nav, ahora_ms, hecho);   /* V2: mientras el costado se vea, la pista vuelve a empezar */

        if (falta >= 0) {
            nav->paralelo_mm = (int)(hecho + falta);
        }
    }
    if (nav->fase == NAV_FASE_PARALELO && esquive_centro_confirmado(nav, ahora_ms)) {
        int z = zona_para_diag(nav);

        diag_escribir(nav, "ESQUIVE_DIAG fase=PARALELO cerc=%d zona=%s celdas=%d accion=BLOQUEA_se_detiene",
                      nav->zona_cerc_frena[z], nombre_zona(z), nav->zona_celdas[z]);
        entrar_detenido(nav, ahora_ms);   /* algo bloquea el carril paralelo: se detiene y, si sigue, esquiva otra vez */
        nav->volver_a_reinc = 1;
        nav->frames_bloqueado = 0;
        return NAV_CMD_STOP;
    }
    return comando_de_fase(nav);
}

/* Un costado aún presente mide un límite inferior del fondo. Avanzar otro intervalo
 * antes de volver a inspeccionar, conservando el rumbo de la recta y STOP al replantear. */
static void extender_rebase(Navegador *nav, uint32_t ahora_ms)
{
    int tramo;
    long avance, desvio;
    /* Un STOP interrumpe GIRAR antes de GIRO_OK. Cerrar la pose con SENS,
     * sin inventar el giro parcial a partir de la velocidad calibrada. */
    if (nav->giro_fw && nav->giro_fw_estado == 0 && nav->giro_fw_ang_ini_ok && nav->sens_angulo_ok &&
        ahora_ms - nav->sens_angulo_t_ms <= NAV_TOF_FRESCO_MS) {
        int medido = nav->cmd_actual == NAV_CMD_LEFT ? nav->sens_angulo_x10 - nav->giro_fw_ang_ini_x10 :
                                                               nav->giro_fw_ang_ini_x10 - nav->sens_angulo_x10;
        while (medido < -1800) medido += 3600;
        while (medido > 1800) medido -= 3600;
        if (medido >= 0) {
            nav->giro_fw_real_x10 = medido;
            nav->giro_fw_estado = 1;
            calcular_pose(nav);
        }
    }
    cambiar_comando(nav, NAV_CMD_STOP);
    if (nav->esquivadas_obstaculo >= NAV_MAX_ESQUIVADAS || nav->esquivadas >= NAV_MAX_ESQUIVADAS_RUTA) {
        ir_a_error_causa(nav, NAV_MOTIVO_MAX_ESQUIVADAS, NAV_CAUSA_MAX_ESQUIVADAS);
        return;
    }
    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &avance, &desvio);
    nav->fondo_observado_mm = (int)avance - nav->cara_obstaculo_s_mm;
    if (nav->fondo_observado_mm < 0) nav->fondo_observado_mm = 0;
    nav->esquivadas++;
    nav->esquivadas_obstaculo++;
    nav->borde_reintentos++;
    {
        /* V2: lo que obliga a alargar puede ser el costado del mismo objeto o un obstáculo NUEVO más adelante. Con una lectura del láser se sabe dónde está (a lo largo
         * de la ruta); desde 22 cm antes de él hace falta otra pista entera, porque de ese objeto no se sabe el fondo. Sin láser (lo vio la visión) se supone a la
         * banda cercana. Solo se ensancha el rodeo si lo visto está al costado (un objeto nuevo más adelante se pasa por el mismo carril). */
        long s_obstaculo = avance;
        int al_costado = 1;
        int tope = (nav->lateral_tope_mm > 0 && nav->lateral_tope_mm < NAV_BORDEAR_LATERAL_MAX) ? nav->lateral_tope_mm : NAV_BORDEAR_LATERAL_MAX;

        if (nav->mirada_fallida) {
            /* La MIRADA vio ocupada la entrada (otro objeto, o el final del que se rodea): no hace falta una pista entera a ciegas, porque la mirada siguiente
             * comprueba otra vez la entrada. Un paso más por el carril (más abierto solo si lo visto está pegado al costado: puede ser más ancho que lo medido). */
            if (nav->mirada_fallida == 2 && nav->lateral_objetivo_mm + NAV_BORDEAR_EXTRA_MM <= tope) {
                nav->lateral_objetivo_mm += NAV_BORDEAR_EXTRA_MM;
            }
            nav->fin_obstaculo_mm = (int)avance + ((nav->mirada_fallida == 3) ? NAV_MIRADA_PASO_LEJOS_MM :
                                                   ((nav->mirada_fallida == 4) ? NAV_MIRADA_PASO_CONFIRMA_MM : NAV_MIRADA_PASO_MM));
            nav->mirada_previa_fallida = (nav->mirada_fallida == 1 || nav->mirada_fallida == 3);
            nav->mirada_fallida = 0;
        } else {
        if (tof_fresco(nav, ahora_ms) && nav->tof_valida && nav->tof_mm > 0 && nav->tof_mm <= NAV_TOF_LIBRE_MM) {
            long alcance = (long)NAV_SENSOR_FRENTE_MM + nav->tof_mm, s_eco, desvio_eco;
            int tramo_eco;

            ubicar_en_linea(nav, nav->x_mm + dir_x_mil_x10(nav->rumbo_x10) * alcance / 1000, nav->y_mm + dir_y_mil_x10(nav->rumbo_x10) * alcance / 1000, &tramo_eco, &s_eco, &desvio_eco);
            if (s_eco > s_obstaculo) {
                s_obstaculo = s_eco;
            }
            al_costado = (s_eco <= avance + 150);
        } else {
            s_obstaculo = avance + NAV_CARA_CERCANIA_3_MM;
            al_costado = 0;
        }
        if (al_costado) {
            nav->lateral_objetivo_mm += NAV_BORDEAR_EXTRA_MM;
            if (nav->lateral_objetivo_mm > tope) nav->lateral_objetivo_mm = tope;
        }
        nav->fin_obstaculo_mm = (int)(s_obstaculo - NAV_PASO_INICIO_MM + pista_tras_la_vista_mm(nav));
        if (nav->fin_obstaculo_mm < avance + NAV_BORDEAR_TRAMO_MM) {
            nav->fin_obstaculo_mm = (int)avance + NAV_BORDEAR_TRAMO_MM;
        }
        }
    }
    if (nav->fin_obstaculo_mm + NAV_REINC_ADELANTE_CM * 10 > nav->ruta_largo_mm &&
        !extender_recta_final(nav, nav->fin_obstaculo_mm)) {
        ir_a_error_causa(nav, NAV_MOTIVO_MAX_ESQUIVADAS, NAV_CAUSA_MAX_ESQUIVADAS);
        return;
    }
    nav->verificando_frente = nav->fin_pendiente = 0;
    nav->tof_pendiente = 0;
    diag_escribir(nav, "BORDE_DIAG intento=%d fondo_observado=%d siguiente_s=%d lateral=%d", nav->borde_reintentos, nav->fondo_observado_mm, nav->fin_obstaculo_mm, nav->lateral_objetivo_mm);
    entrar_reincorporando(nav, ahora_ms);
}

/* V2, MIRADA ANTES DE ENTRAR: tras el primer giro del regreso el carro mira hacia donde va a entrar a la línea. La cámara, girada unos 55 grados hacia la línea, ve
 * justo la banda por donde va a pasar: la diagonal, el sitio donde queda al final y lo que barre la cola en el segundo giro (lo que queda detrás de la lente ya no
 * importa: por ahí no pasa). Si el color (aunque sea la única fuente) ve algo cerca (cercanía 2 o 3) en CUALQUIER zona, ahí no se entra: a esa distancia cada zona
 * mide unos 12 cm y el chasis 20. No es frenar la ruta por una sola fuente: es elegir dónde volver a ella (se avanza un paso más por el carril y se mira otra vez).
 * Devuelve la zona ocupada o -1. */
static int entrada_ocupada_por_color(const Navegador *nav, uint32_t ahora_ms, int parado)
{
    /* La zona del lado hacia donde sigue la ruta: girado a la izquierda es la derecha de la imagen. Lo que se ve ahí, aunque sea lejos (cercanía 1), está sobre la
     * línea a menos de medio metro de donde va a quedar el frente del carro: entrar ahí es quedar pegado a otro obstáculo, sin sitio para rodearlo. */
    int zona_adelante = (nav->reinc_giro1 < 0) ? ZONA_DERECHA : ZONA_IZQUIERDA;
    int z;

    if (!nav->usa_camara || esq_ignorando(nav, ahora_ms) || (uint32_t)(ahora_ms - nav->t_ultimo_cuadro_ms) > (uint32_t)NAV_MAX_HUECO_VIDEO_MS) {
        return -1;
    }
    for (z = 0; z < OBS_ZONAS; z++) {
        if ((nav->fuentes_vision[z] & 1) && nav->zona_celdas[z] >= NAV_ENTRADA_CELDAS_MIN &&
            nav->color_cercania[z] >= ((parado && z == zona_adelante) ? 1 : 2)) {
            return z;
        }
    }
    return -1;
}

/* V2: la entrada a la línea se ocupó (o se vio ocupada). Si el carro ya iba avanzando por la diagonal, la DESHACE en reversa: por ahí acaba de pasar, y girar junto a
 * lo que apareció barrería el chasis contra ello. Después sigue de largo por el carril con un paso corto y otra mirada. */
static ComandoNav reinc_abortar_entrada(Navegador *nav, uint32_t ahora_ms)
{
    long hecho = (nav->reinc_fase == NAV_RF_AVANZAR) ? cal_ms_a_mm(&nav->cal, nav->ms_fase) : 0;

    if (hecho >= 40) {
        nav->reinc_deshacer_mm = (int)((hecho > NAV_DESHACER_MAX_MM) ? NAV_DESHACER_MAX_MM : hecho);
        diag_escribir(nav, "BORDE_DIAG deshace_la_entrada mm=%d: reversa por donde acaba de pasar", nav->reinc_deshacer_mm);
        reinc_cambiar_fase(nav, NAV_RF_PAUSA_DESHACER, ahora_ms);
        return NAV_CMD_STOP;
    }
    nav->mirada_fallida = 1;
    extender_rebase(nav, ahora_ms);
    return NAV_CMD_STOP;
}

/* REINCORPORANDO: vuelve a la línea de la ruta con la pose estimada (ver reinc_calcular): PLAN → GIRO1 → pausa → AVANZAR → pausa → GIRO2 → pausa → SIGUIENDO.
 * Entre cada movimiento hay la pausa de siempre. Mientras avanza hacia la línea vigila el frente con la misma regla de la esquivada: si el centro se bloquea,
 * se detiene y (si el obstáculo sigue) esquiva otra vez; si se libera, vuelve a calcular la vuelta. */
static ComandoNav tick_reincorporando(Navegador *nav, uint32_t ahora_ms)
{
    int fase = nav->reinc_fase, siguiente, giro;
    int z, borde_visible = 0;
    for (z = 0; z < OBS_ZONAS; z++) {
        if (nav->fuentes_vision[z] == 3 && nav->zona_cercania[z] >= 2 &&
            ahora_ms - nav->t_ultimo_cuadro_ms <= NAV_MAX_HUECO_VIDEO_MS) borde_visible = 1;
    }

    /* Ya se avanzo el tramo exploratorio. Ver otra superficie cercana al
     * barrer hacia la linea exige extender el rebase, incluso DURANTE el giro:
     * esperar al avance puede perder la pata/costado del haz y tocarlo. */
    if (nav->obstaculo_pendiente && !nav->reinc_pasar_primero && (fase == NAV_RF_GIRO1 || fase == NAV_RF_PAUSA1 || fase == NAV_RF_AVANZAR)) {
        int costado = 0;

        if (tof_fresco(nav, ahora_ms) && nav->tof_valida && nav->tof_mm > 0 && nav->tof_mm <= NAV_REACCION_MAX_MM) {
            /* V2: ¿dónde pega el láser? Si el punto queda delante del carro (a lo largo de la ruta) no es el costado de lo que se rodea: es otra cosa más adelante. */
            long s_eco, lado_eco, s_carro, lado_carro;

            laser_punto(nav, ahora_ms, nav->tof_mm, &s_eco, &lado_eco);
            s_carro = ((long)(nav->x_mm - nav->salida_x_mm) * dir_x_mil(nav->salida_rumbo) + (long)(nav->y_mm - nav->salida_y_mm) * dir_y_mil(nav->salida_rumbo)) / 1000;
            lado_carro = ((long)(nav->x_mm - nav->salida_x_mm) * dir_x_mil(nav->salida_rumbo + nav->lado * 90) +
                          (long)(nav->y_mm - nav->salida_y_mm) * dir_y_mil(nav->salida_rumbo + nav->lado * 90)) / 1000;
            costado = (s_eco <= s_carro + 150 && lado_eco >= -150 && lado_eco <= lado_carro + 100);
        }
        if (fase == NAV_RF_AVANZAR && nav->cmd_actual == NAV_CMD_FORWARD && (costado || borde_visible)) {
            diag_escribir(nav, "BORDE_DIAG entrada_ocupada laser_costado=%d coincidencia_cerca=%d", costado, borde_visible);
            return reinc_abortar_entrada(nav, ahora_ms);
        }
        if (costado || (borde_visible && fase == NAV_RF_AVANZAR)) {
            extender_rebase(nav, ahora_ms);
            return NAV_CMD_STOP;
        }
    }

    if (nav->tof_pendiente) {   /* TOF_PARO mientras avanzaba hacia la línea: se detiene y, si el obstáculo sigue, esquiva otra vez */
        nav->tof_pendiente = 0;
        if (fase == NAV_RF_AVANZAR && nav->cmd_actual == NAV_CMD_FORWARD) {
            diag_escribir(nav, "SENS_DIAG tof_paro fase=VUELTA_AVANZAR laser_mm=%d: se detiene", nav->tof_dist_mm);
            tof_marcar(nav, nav->tof_dist_mm);
            entrar_detenido(nav, ahora_ms);
            nav->volver_a_reinc = 1;
            nav->frames_bloqueado = 0;
            return NAV_CMD_STOP;
        }
    }
    switch (fase) {
    case NAV_RF_VERIFICAR:
        return reinc_terminar(nav, ahora_ms);
    case NAV_RF_PAUSA_DESHACER:
        if ((uint32_t)(ahora_ms - nav->reinc_t_fase_ms) < (uint32_t)NAV_PAUSA_ASENTAR_MS) {
            return NAV_CMD_STOP;
        }
        reinc_cambiar_fase(nav, NAV_RF_DESHACER, ahora_ms);
        return NAV_CMD_REVERSE;
    case NAV_RF_DESHACER: {
        int ms_deshacer = cal_cm_a_ms_atras(&nav->cal, (nav->reinc_deshacer_mm + 5) / 10);

        if (ms_deshacer > NAV_RETROCESO_MAX_MS) {
            ms_deshacer = NAV_RETROCESO_MAX_MS;
        }
        if (nav->ms_fase + NAV_TICK_MS / 2 >= ms_deshacer) {
            nav->mirada_fallida = 1;
            extender_rebase(nav, ahora_ms);
            return NAV_CMD_STOP;
        }
        return NAV_CMD_REVERSE;
    }
    case NAV_RF_PLAN:
        if ((uint32_t)(ahora_ms - nav->reinc_t_fase_ms) < (uint32_t)NAV_PAUSA_ASENTAR_MS) {
            return NAV_CMD_STOP;
        }
        if (nav->desde_ayuda && reinc_ya_paso_el_final(nav)) {   /* rodeó el obstáculo y ya quedó más allá del punto final: no regresa a él */
            diag_escribir(nav, "REINC_DIAG fin ya_paso_el_final pose=(%d,%d,%d): la ruta se da por terminada", nav->x_mm, nav->y_mm, nav->rumbo_grados);
            nav->estado = NAV_TERMINADO;
            return NAV_CMD_STOP;
        }
        if (!reinc_calcular(nav)) {
            ir_a_error_causa(nav, NAV_MOTIVO_ESQUIVANDO, NAV_CAUSA_VUELTA);   /* sin solución: pide ayuda (o ERROR si no se pidió) en vez de ejecutar un plan inválido */
            return NAV_CMD_STOP;
        }
        if (valor_absoluto_int(nav->reinc_giro1) > NAV_LOCURA_GIRO_MAX || valor_absoluto_int(nav->reinc_giro2) > NAV_LOCURA_GIRO_MAX) {
            frenar_por_locura(nav, NAV_CAUSA_GIRO_LOCO, "giro_de_la_vuelta_fuera_de_la_ruta");   /* un giro así no es de una vuelta a la línea: no se da */
            return NAV_CMD_STOP;
        }
        siguiente = reinc_siguiente_movimiento(nav, NAV_RF_PLAN);
        if (siguiente < 0) {
            return reinc_terminar(nav, ahora_ms);
        }
        reinc_cambiar_fase(nav, siguiente, ahora_ms);
        if (siguiente != NAV_RF_AVANZAR) {
            esq_iniciar_giro(nav, ahora_ms);
        }
        return comando_de_reinc(nav);
    case NAV_RF_GIRO1:
    case NAV_RF_GIRO2:
        giro = (fase == NAV_RF_GIRO1) ? nav->reinc_giro1 : nav->reinc_giro2;
        if (nav->cmd_actual != comando_de_reinc(nav)) {
            return comando_de_reinc(nav);          /* todavía no arranca el comando de giro: nada que medir */
        }
        if (giro_terminado(nav, (giro < 0) ? -giro : giro, cal_grados_a_ms(&nav->cal, giro), nav->ms_fase)) {
            esq_terminar_giro(nav, ahora_ms);
            reinc_cambiar_fase(nav, fase + 1, ahora_ms);     /* la pausa que sigue a este giro */
            return NAV_CMD_STOP;
        }
        return comando_de_reinc(nav);
    case NAV_RF_AVANZAR:
        if (nav->obstaculo_pendiente && !nav->reinc_pasar_primero && esquive_centro_confirmado(nav, ahora_ms)) {
            diag_escribir(nav, "BORDE_DIAG entrada_ocupada centro_confirmado=1");
            return reinc_abortar_entrada(nav, ahora_ms);
        }
        if (nav->obstaculo_pendiente && !nav->reinc_pasar_primero && nav->cmd_actual == NAV_CMD_FORWARD && nav->frames_recibidos >= 2 &&
            entrada_ocupada_por_color(nav, ahora_ms, 0) >= 0) {
            int zo = entrada_ocupada_por_color(nav, ahora_ms, 0);

            /* V2: entrando en diagonal, el color ve algo cerca justo donde se va a entrar: no se entra ahí, se sigue de largo por el carril del rodeo. */
            diag_escribir(nav, "BORDE_DIAG entrada_ocupada_por_color zona=%s celdas=%d cercania=%d: se sigue de largo", nombre_zona(zo), nav->zona_celdas[zo], nav->color_cercania[zo]);
            return reinc_abortar_entrada(nav, ahora_ms);
        }
        if (nav->obstaculo_pendiente && nav->reinc_pasar_primero && nav->cmd_actual == NAV_CMD_FORWARD) {
            long hecho = cal_ms_a_mm(&nav->cal, nav->ms_fase);
            long falta = costado_a_la_vista(nav, ahora_ms, hecho);   /* V2: el paso en paralelo de una extensión se alarga igual que el del rodeo */

            if (falta >= 0 && hecho + falta > nav->reinc_dist_mm) {
                long extra = hecho + falta - nav->reinc_dist_mm;

                nav->reinc_dist_mm += (int)extra;
                nav->reinc_obj_x_mm += (int)(dir_x_mil(nav->reinc_rumbo_final) * extra / 1000);
                nav->reinc_obj_y_mm += (int)(dir_y_mil(nav->reinc_rumbo_final) * extra / 1000);
                nav->reinc_plan_x_mm[1] += (int)(dir_x_mil(nav->reinc_rumbo_final) * extra / 1000);
                nav->reinc_plan_y_mm[1] += (int)(dir_y_mil(nav->reinc_rumbo_final) * extra / 1000);
                nav->reinc_plan_x_mm[2] = nav->reinc_plan_x_mm[1];
                nav->reinc_plan_y_mm[2] = nav->reinc_plan_y_mm[1];
            }
        }
        if (nav->ms_fase + NAV_TICK_MS / 2 >= cal_cm_a_ms(&nav->cal, (nav->reinc_dist_mm + 5) / 10)) {
            reinc_cambiar_fase(nav, NAV_RF_PAUSA2, ahora_ms);
            return NAV_CMD_STOP;
        }
        if (esquive_centro_confirmado(nav, ahora_ms)) {
            int z = zona_para_diag(nav);

            diag_escribir(nav, "ESQUIVE_DIAG fase=VUELTA_AVANZAR cerc=%d zona=%s celdas=%d accion=BLOQUEA_se_detiene",
                          nav->zona_cerc_frena[z], nombre_zona(z), nav->zona_celdas[z]);
            entrar_detenido(nav, ahora_ms);   /* el tramo de vuelta está bloqueado: se detiene y, si sigue, esquiva otra vez */
            nav->volver_a_reinc = 1;
            nav->frames_bloqueado = 0;
            return NAV_CMD_STOP;
        }
        return NAV_CMD_FORWARD;
    default:   /* las tres pausas */
        if ((uint32_t)(ahora_ms - nav->reinc_t_fase_ms) < (uint32_t)NAV_PAUSA_ASENTAR_MS) {
            return NAV_CMD_STOP;
        }
        {   /* RED DE SEGURIDAD: terminó un tramo (el giro 1, el avance o el giro 2) y la pose ya incluye el frenado. Si la pose estimada se alejó del punto planeado, vuelve a planear desde donde está. */
            int etapa = (fase - NAV_RF_PAUSA1) / 2;
            long ex = (long)nav->x_mm - nav->reinc_plan_x_mm[etapa], ey = (long)nav->y_mm - nav->reinc_plan_y_mm[etapa];
            long error = raiz_entera(ex * ex + ey * ey);

            if (error > NAV_PLAN_ERROR_MM && !nav->sin_red_seguridad) {
                if (nav->reinc_correcciones >= NAV_PLAN_CORRECCIONES) {
                    diag_escribir(nav, "REINC_DIAG correccion_agotada etapa=%d error_mm=%ld planeado=(%d,%d) estimado=(%d,%d,%d): se pide ayuda", etapa + 1, error,
                                  nav->reinc_plan_x_mm[etapa], nav->reinc_plan_y_mm[etapa], nav->x_mm, nav->y_mm, nav->rumbo_grados);
                    ir_a_error_causa(nav, NAV_MOTIVO_ESQUIVANDO, NAV_CAUSA_CORRECCIONES);
                    return NAV_CMD_STOP;
                }
                reinc_replanear(nav, ahora_ms);
                diag_escribir(nav, "REINC_DIAG correccion n=%d etapa=%d error_mm=%ld planeado=(%d,%d) estimado=(%d,%d,%d): vuelve a planear", nav->reinc_correcciones, etapa + 1, error,
                              nav->reinc_plan_x_mm[etapa], nav->reinc_plan_y_mm[etapa], nav->x_mm, nav->y_mm, nav->rumbo_grados);
                return NAV_CMD_STOP;
            }
        }
        if (fase == NAV_RF_PAUSA1 && nav->obstaculo_pendiente && !nav->reinc_pasar_primero) {
            /* V2, MIRADA ANTES DE ENTRAR (ver entrada_ocupada_por_color): se espera a tener cuadros que ya muestren el carro quieto y girado (con tope de tiempo: si la
             * cámara no manda nada se sigue como antes, con el láser de guardia) y entonces se decide. */
            int zo;

            if (nav->usa_camara && (nav->esq_cuadros_tras_giro < NAV_ESQ_CUADROS_ESTABLES || esq_ignorando(nav, ahora_ms)) &&
                (uint32_t)(ahora_ms - nav->esq_t_giro_fin_ms) < (uint32_t)nav->latencia_visual_ms + (uint32_t)NAV_MIRADA_MAX_MS) {
                return NAV_CMD_STOP;
            }
            zo = entrada_ocupada_por_color(nav, ahora_ms, 1);
            if (zo >= 0) {
                int zona_adelante = (nav->reinc_giro1 < 0) ? ZONA_DERECHA : ZONA_IZQUIERDA;

                diag_escribir(nav, "BORDE_DIAG mirada entrada_ocupada_por_color zona=%s celdas=%d cercania=%d: no entra, sigue de largo", nombre_zona(zo), nav->zona_celdas[zo],
                              nav->color_cercania[zo]);
                {
                    /* ¿Solo se ve algo del lado de ATRÁS (hacia donde quedó lo que se rodeó)? Entonces es el final de eso mismo: al avanzar queda más atrás todavía
                     * y la mirada siguiente, si sale despejada, no necesita confirmación. */
                    int z2, solo_atras = 1;

                    for (z2 = 0; z2 < OBS_ZONAS; z2++) {
                        if ((z2 == ZONA_CENTRO || z2 == zona_adelante) && (nav->fuentes_vision[z2] & 1) && nav->zona_celdas[z2] >= NAV_ENTRADA_CELDAS_MIN &&
                            nav->color_cercania[z2] >= ((z2 == zona_adelante) ? 1 : 2)) {
                            solo_atras = 0;
                        }
                    }
                    if (solo_atras) {
                        nav->mirada_fallida = (nav->color_cercania[zo] >= 3) ? 2 : 5;   /* 2: pegado al costado del carril, el carril se abre un poco */
                    } else if (zo == zona_adelante && nav->color_cercania[zo] < 2) {
                        nav->mirada_fallida = 3;      /* está más adelante sobre la línea: hay que pasarlo entero */
                    } else {
                        nav->mirada_fallida = 1;
                    }
                }
                extender_rebase(nav, ahora_ms);
                return NAV_CMD_STOP;
            }
            if (tof_fresco(nav, ahora_ms) && nav->tof_valida && nav->tof_mm > 0 && nav->tof_mm <= NAV_FIN_HOLGURA_MM) {
                diag_escribir(nav, "BORDE_DIAG mirada entrada_ocupada_por_laser laser_mm=%d: no entra, sigue de largo", nav->tof_mm);
                nav->mirada_fallida = 1;
                extender_rebase(nav, ahora_ms);
                return NAV_CMD_STOP;
            }
            if (nav->mirada_previa_fallida && nav->usa_camara) {
                diag_escribir(nav, "BORDE_DIAG mirada despejada_tras_ocupada: paso de confirmacion (punto ciego junto al carro)");
                nav->mirada_fallida = 4;
                extender_rebase(nav, ahora_ms);
                return NAV_CMD_STOP;
            }
        }
        siguiente = reinc_siguiente_movimiento(nav, fase - 1);
        if (siguiente < 0 && nav->reinc_rumbo_importa && !nav->sin_red_seguridad && nav->reinc_correcciones < NAV_PLAN_CORRECCIONES &&
            valor_absoluto_int(giro_minimo(nav->rumbo_grados, nav->reinc_rumbo_final)) > nav->reinc_rumbo_tol) {
            /* Terminó la vuelta pero el rumbo quedó a NAV_REINC_GIRO_MIN_GRADOS o más del de la línea (los giros por tiempo salen de a 5 grados): un rumbo chueco se paga después
             * en todo el resto de la ruta (4 grados son 33 cm en 5 m), así que se corrige con otro plan. Si ya se agotaron las correcciones se sigue como está. */
            reinc_replanear(nav, ahora_ms);
            diag_escribir(nav, "REINC_DIAG correccion_rumbo n=%d rumbo=%d rumbo_linea=%d: vuelve a planear", nav->reinc_correcciones, nav->rumbo_grados, nav->reinc_rumbo_final);
            return NAV_CMD_STOP;
        }
        if (siguiente < 0) {
            return reinc_terminar(nav, ahora_ms);
        }
        reinc_cambiar_fase(nav, siguiente, ahora_ms);
        if (siguiente != NAV_RF_AVANZAR) {
            esq_iniciar_giro(nav, ahora_ms);
        }
        return comando_de_reinc(nav);
    }
}

/* AYUDA: el usuario maneja con la cruceta. El carro hace lo que mande navegador_ayuda_comando (soltar = STOP) y sigue calculando su posición por tiempo; si pasan
 * NAV_AYUDA_MAX_MS sin que nadie continúe, termina en ERROR con el motivo con el que pidió ayuda. */
static ComandoNav tick_ayuda(Navegador *nav, uint32_t ahora_ms)
{
    if ((uint32_t)(ahora_ms - nav->ayuda_t_ini_ms) > (uint32_t)NAV_AYUDA_MAX_MS) {
        nav->pedir_ayuda = 0;
        ir_a_error_causa(nav, nav->ayuda_motivo, nav->error_causa);
        nav->pedir_ayuda = 1;
        return NAV_CMD_STOP;
    }
    return nav->ayuda_cmd;
}

int navegador_poner_requiere_camara(Navegador *nav, int activo)
{
    if (nav == NULL || (activo != 0 && activo != 1)) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->requiere_camara = activo;
    return NAV_OK;
}

int navegador_poner_red_seguridad(Navegador *nav, int activo)
{
    if (nav == NULL || (activo != 0 && activo != 1)) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->sin_red_seguridad = !activo;
    return NAV_OK;
}

int navegador_poner_pedir_ayuda(Navegador *nav, int activo)
{
    if (nav == NULL || (activo != 0 && activo != 1)) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->pedir_ayuda = activo;
    return NAV_OK;
}

int navegador_poner_giroscopio(Navegador *nav, int activo)
{
    if (nav == NULL || (activo != 0 && activo != 1)) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->usa_giroscopio = activo;
    return NAV_OK;
}

int navegador_poner_sensor_distancia(Navegador *nav, int activo)
{
    if (nav == NULL || (activo != 0 && activo != 1)) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->usa_tof = activo;
    return NAV_OK;
}

int navegador_poner_autoridad_laser(Navegador *nav, int activo)
{
    if (nav == NULL || (activo != 0 && activo != 1)) return NAV_ERR_ARGUMENTO;
    nav->autoridad_laser = activo;
    nav->contradiccion_laser = 0;
    return NAV_OK;
}

int navegador_poner_vistazo(Navegador *nav, int modo)
{
    if (nav == NULL || modo < NAV_VISTAZO_NO || modo > NAV_VISTAZO_TODOS) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->vistazo_modo_mas1 = modo + 1;
    return NAV_OK;
}

int navegador_poner_precaucion(Navegador *nav, int modo)
{
    if (nav == NULL || modo < NAV_PRECAUCION_NUNCA || modo > NAV_PRECAUCION_SIEMPRE) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->precaucion_modo = modo;
    return NAV_OK;
}

int navegador_poner_giro_fw_min(Navegador *nav, int grados)
{
    if (nav == NULL || grados < 1 || grados > 10) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->giro_fw_min = grados;
    return NAV_OK;
}

void navegador_poner_largo_recta_mm(Navegador *nav, int mm)
{
    if (nav != NULL) {
        nav->override_largo_mm = (mm > 0) ? mm : 0;
    }
}

/* ── Rumbo sostenido por el firmware (J16) ─────────────────────────────── */

int navegador_poner_rumbo_fw(Navegador *nav, int activo)
{
    if (nav == NULL || (activo != 0 && activo != 1)) {
        return NAV_ERR_ARGUMENTO;
    }
    nav->rumbo_fw = activo;
    nav->rumbo_fw_pedir = 0;
    nav->rumbo_fw_hay_enviado = 0;
    return NAV_OK;
}

int navegador_tomar_rumbo_fw(Navegador *nav, int *objetivo_x10_fw)
{
    if (nav == NULL || objetivo_x10_fw == NULL || !nav->rumbo_fw_pedir) {
        return 0;
    }
    nav->rumbo_fw_pedir = 0;
    *objetivo_x10_fw = nav->rumbo_fw_objetivo_x10;
    return 1;
}

/* El rumbo (décimas, convención del navegador: + derecha) que el carro debe llevar mientras avanza, o 0 si no hay uno (el firmware sostiene el del arranque):
 *  - SIGUIENDO, en un avance planeado sobre la línea: apuntar al punto de la línea NAV_RUMBO_MIRA_MM más adelante (sin pasar del final del avance), así un carro
 *    que quedó de lado vuelve a la línea avanzando; a menos de NAV_RUMBO_MIRA_MIN_MM del final, solo el rumbo de la línea. Nunca se aparta más de NAV_RUMBO_CORR_MAX.
 *  - REINCORPORANDO, en el avance hacia la línea: apuntar al punto planeado (hasta quedar a NAV_RUMBO_PUNTO_MIN_MM de él).
 *  - ESQUIVANDO, en el paso en paralelo: el rumbo de la línea. */
static int rumbo_deseado_x10(const Navegador *nav, int *deseado_x10)
{
    if (nav->estado == NAV_SIGUIENDO) {
        long ux, uy, rx, ry, s, falta, mira, px, py;
        int d;

        if (!nav->paso_en_linea || nav->sondeo_fase != 0 || nav->indice_paso >= nav->n_pasos || nav->pasos[nav->indice_paso].tipo != PASO_AVANZAR) {
            return 0;
        }
        if (nav->vistazo_lado != 0 && nav->vistazo_fase == 1) {   /* V2: vistazo con el láser: el rumbo de la línea torcido hacia lo que vio la cámara */
            *deseado_x10 = normalizar_grados(nav->linea_rumbo) * 10 + nav->vistazo_lado * nav->vistazo_grados * 10;
            return 1;
        }
        if (nav->paso_apunta && !nav->sin_volver_a_linea) {   /* el avance va en diagonal a su punto de parada (tras un pivote grande): se apunta a ese punto */
            long dx = (long)nav->paso_fin_x_mm - nav->x_mm, dy = (long)nav->paso_fin_y_mm - nav->y_mm;

            if (raiz_entera(dx * dx + dy * dy) < NAV_RUMBO_PUNTO_MIN_MM) {
                return 0;
            }
            d = diferencia_x10(nav->base_rumbo_grados * 10, rumbo_del_vector_x10(dx, dy, nav->base_rumbo_grados * 10));
            if (d > NAV_RUMBO_CORR_MAX * 10) {
                d = NAV_RUMBO_CORR_MAX * 10;
            } else if (d < -NAV_RUMBO_CORR_MAX * 10) {
                d = -NAV_RUMBO_CORR_MAX * 10;
            }
            *deseado_x10 = nav->base_rumbo_grados * 10 + d;
            return 1;
        }
        ux = dir_x_mil(nav->linea_rumbo);
        uy = dir_y_mil(nav->linea_rumbo);
        rx = (long)nav->x_mm - nav->linea_ax_mm;
        ry = (long)nav->y_mm - nav->linea_ay_mm;
        s = (rx * ux + ry * uy) / 1000;
        falta = nav->linea_fin_s_mm - s;
        if (nav->sin_volver_a_linea || falta < NAV_RUMBO_MIRA_MIN_MM) {
            *deseado_x10 = nav->linea_rumbo * 10;
            return 1;
        }
        mira = (falta < NAV_RUMBO_MIRA_MM) ? falta : NAV_RUMBO_MIRA_MM;
        px = nav->linea_ax_mm + ux * (s + mira) / 1000;
        py = nav->linea_ay_mm + uy * (s + mira) / 1000;
        d = diferencia_x10(nav->linea_rumbo * 10, rumbo_del_vector_x10(px - nav->x_mm, py - nav->y_mm, nav->linea_rumbo * 10));
        if (d > NAV_RUMBO_CORR_MAX * 10) {
            d = NAV_RUMBO_CORR_MAX * 10;
        } else if (d < -NAV_RUMBO_CORR_MAX * 10) {
            d = -NAV_RUMBO_CORR_MAX * 10;
        }
        *deseado_x10 = normalizar_grados(nav->linea_rumbo) * 10 + d;
        return 1;
    }
    if (nav->estado == NAV_REINCORPORANDO && nav->reinc_fase == NAV_RF_AVANZAR && nav->rumbo_en_vuelta) {
        long dx = (long)nav->reinc_plan_x_mm[1] - nav->x_mm, dy = (long)nav->reinc_plan_y_mm[1] - nav->y_mm;
        int d;

        if (raiz_entera(dx * dx + dy * dy) < NAV_RUMBO_PUNTO_MIN_MM) {
            return 0;
        }
        d = diferencia_x10(nav->base_rumbo_grados * 10, rumbo_del_vector_x10(dx, dy, nav->base_rumbo_grados * 10));
        if (d > NAV_RUMBO_CORR_MAX * 10) {
            d = NAV_RUMBO_CORR_MAX * 10;
        } else if (d < -NAV_RUMBO_CORR_MAX * 10) {
            d = -NAV_RUMBO_CORR_MAX * 10;
        }
        *deseado_x10 = nav->base_rumbo_grados * 10 + d;
        return 1;
    }
    if (nav->estado == NAV_REINCORPORANDO && nav->reinc_fase == NAV_RF_AVANZAR && nav->reinc_pasar_primero && nav->obstaculo_pendiente && nav->borde_reintentos > 0) {
        /* V2: un tramo más por el carril del rodeo (tras una mirada o una extensión). Cada giro hacia la línea y de vuelta deja al carro unos 12 cm más cerca de la
         * línea (los dos pivotes no son el mismo): el carril se recupera avanzando, apuntando a un punto del carril NAV_RUMBO_MIRA_MM adelante, sin más giros. */
        int costado = (nav->salida_linea_ok ? nav->salida_linea_rumbo : nav->salida_rumbo) + nav->lado * 90;
        long lado_carro = ((long)(nav->x_mm - nav->salida_x_mm) * dir_x_mil(costado) + (long)(nav->y_mm - nav->salida_y_mm) * dir_y_mil(costado)) / 1000;
        long falta = (long)nav->lateral_objetivo_mm - lado_carro;
        long dx, dy;
        int d;

        if (falta > 150) {
            falta = 150;
        } else if (falta < -150) {
            falta = -150;
        }
        dx = dir_x_mil(nav->reinc_rumbo_final) * (long)NAV_RUMBO_MIRA_MM / 1000 + dir_x_mil(costado) * falta / 1000;
        dy = dir_y_mil(nav->reinc_rumbo_final) * (long)NAV_RUMBO_MIRA_MM / 1000 + dir_y_mil(costado) * falta / 1000;
        d = diferencia_x10(nav->reinc_rumbo_final * 10, rumbo_del_vector_x10(dx, dy, nav->reinc_rumbo_final * 10));
        if (d > NAV_RUMBO_CORR_MAX * 10) {
            d = NAV_RUMBO_CORR_MAX * 10;
        } else if (d < -NAV_RUMBO_CORR_MAX * 10) {
            d = -NAV_RUMBO_CORR_MAX * 10;
        }
        *deseado_x10 = normalizar_grados(nav->reinc_rumbo_final) * 10 + d;
        return 1;
    }
    if (nav->estado == NAV_ESQUIVANDO && nav->fase == NAV_FASE_PARALELO) {
        if (nav->salida_linea_ok && !nav->sin_volver_a_linea) {
            /* V2: el paso en paralelo va paralelo a la LÍNEA (no al rumbo con que el carro salió de ella, que trae su error) y sostiene el carril: apunta a un punto
             * del carril NAV_RUMBO_MIRA_MM adelante. Con 3 grados de error, en metro y medio de paso el carro se cerraba 8 cm contra lo que estaba rodeando. */
            int costado = nav->salida_linea_rumbo + nav->lado * 90;
            long lado_carro = ((long)(nav->x_mm - nav->salida_x_mm) * dir_x_mil(costado) + (long)(nav->y_mm - nav->salida_y_mm) * dir_y_mil(costado)) / 1000;
            long falta = (long)nav->lateral_objetivo_mm - lado_carro;
            long dx, dy;
            int d;

            if (falta > 150) {
                falta = 150;
            } else if (falta < -150) {
                falta = -150;
            }
            dx = dir_x_mil(nav->salida_linea_rumbo) * (long)NAV_RUMBO_MIRA_MM / 1000 + dir_x_mil(costado) * falta / 1000;
            dy = dir_y_mil(nav->salida_linea_rumbo) * (long)NAV_RUMBO_MIRA_MM / 1000 + dir_y_mil(costado) * falta / 1000;
            d = diferencia_x10(nav->salida_linea_rumbo * 10, rumbo_del_vector_x10(dx, dy, nav->salida_linea_rumbo * 10));
            if (d > NAV_RUMBO_CORR_MAX * 10) {
                d = NAV_RUMBO_CORR_MAX * 10;
            } else if (d < -NAV_RUMBO_CORR_MAX * 10) {
                d = -NAV_RUMBO_CORR_MAX * 10;
            }
            *deseado_x10 = normalizar_grados(nav->salida_linea_rumbo) * 10 + d;
            return 1;
        }
        *deseado_x10 = nav->salida_rumbo * 10;
        return 1;
    }
    return 0;
}

/* V2, VISTAZO CON EL LÁSER (ver NAV_VISTAZO_* en navegador.h). Se llama en cada tick de SIGUIENDO con el comando que decidió la ruta. */
static void vistazo_terminar(Navegador *nav, uint32_t ahora_ms)
{
    if (nav->vistazo_lado != 0) {
        nav->vistazo_ultimo_ms[nav->vistazo_lado < 0 ? 0 : 1] = ahora_ms;
        nav->vistazo_fin_ms = ahora_ms;
        nav->vistazo_alguno = 1;
        nav->vistazo_lado = 0;
        nav->vistazo_fase = 0;
    }
}

static ComandoNav vistazo_tick(Navegador *nav, uint32_t ahora_ms, ComandoNav comando)
{
    int puede = nav->estado == NAV_SIGUIENDO && comando == NAV_CMD_FORWARD && nav->cmd_actual == NAV_CMD_FORWARD && rumbo_fw_activo(nav) && nav->usa_tof &&
                nav->avance_imu_ref_ok && nav->paso_en_linea && nav->sondeo_fase == 0 && nav->indice_paso < nav->n_pasos &&
                nav->pasos[nav->indice_paso].tipo == PASO_AVANZAR;
    int laser_ya_ve = tof_fresco(nav, ahora_ms) && nav->tof_valida && nav->tof_mm > 0 && nav->tof_mm <= NAV_VISTAZO_ALCANCE_MM;
    int modo_vistazo = (nav->vistazo_modo_mas1 > 0) ? nav->vistazo_modo_mas1 - 1 : NAV_VISTAZO_POR_DEFECTO;
    int i, elegido = -1;

    if (!puede) {
        vistazo_terminar(nav, ahora_ms);
        return comando;
    }
    if (nav->vistazo_lado != 0) {
        if (nav->vistazo_ecos >= NAV_VISTAZO_ECOS) {
            /* El láser lo confirmó: se detiene y se rodea por el otro lado. La distancia es la del eco. */
            diag_escribir(nav, "VISTAZO_DIAG confirma lado=%d laser_mm=%d ecos=%d ms=%lu", nav->vistazo_lado, nav->vistazo_mm, nav->vistazo_ecos,
                          (unsigned long)(uint32_t)(ahora_ms - nav->vistazo_t_ms));
            nav->vistazo_confirmado = nav->vistazo_lado;
            nav->vistazo_retrocedio = 0;
            nav->vistazos_con_eco++;
            vistazo_terminar(nav, ahora_ms);
            nav->sondeo_hecho = 1;
            nav->arranque_ok = 1;
            tof_marcar(nav, nav->vistazo_mm);
            entrar_detenido(nav, ahora_ms);
            return NAV_CMD_STOP;
        }
        if (nav->vistazo_fase == 1) {
            int torcido = giro_minimo(nav->linea_rumbo, nav->rumbo_grados) * nav->vistazo_lado;   /* cuánto lleva torcido hacia ese lado */

            if (torcido >= nav->vistazo_grados - 2 || (uint32_t)(ahora_ms - nav->vistazo_t_ms) >= (uint32_t)NAV_VISTAZO_MS) {
                nav->vistazo_fase = 2;            /* llegó: el rumbo vuelve al de la línea y el láser sigue escuchando un momento */
                nav->vistazo_fase_ms = ahora_ms;
            }
        } else if ((uint32_t)(ahora_ms - nav->vistazo_fase_ms) >= (uint32_t)NAV_VISTAZO_ESCUCHA_MS) {
            diag_escribir(nav, "VISTAZO_DIAG sin_eco lado=%d ms=%lu: sigue la ruta", nav->vistazo_lado, (unsigned long)(uint32_t)(ahora_ms - nav->vistazo_t_ms));
            vistazo_terminar(nav, ahora_ms);
        }
        return comando;
    }
    if (laser_ya_ve) {
        return comando;   /* el láser ya tiene algo delante: decide él por las reglas de siempre */
    }
    if (nav->vistazo_alguno && (uint32_t)(ahora_ms - nav->vistazo_fin_ms) < (uint32_t)NAV_VISTAZO_ENTRE_MS) {
        return comando;   /* todavía está volviendo del anterior */
    }
    for (i = 0; i < 2; i++) {
        if (nav->vistazo_cuadros[i] < NAV_VISTAZO_CUADROS || nav->vistazo_lateral_mm[i] > NAV_VISTAZO_LATERAL_MAX_MM) {
            continue;   /* nada persistente de ese lado, o queda demasiado afuera para estorbar */
        }
        if (modo_vistazo == NAV_VISTAZO_NO) {
            continue;
        }
        if (nav->vistazo_solo_red[i]) {
            if (nav->vistazo_red_hecho[i]) {
                continue;   /* a la caja de la red ya se le dio el suyo */
            }
        } else if (nav->vistazo_hecho[i] >= 2 || (nav->vistazo_hecho[i] == 1 && nav->vistazo_cerc[i] < 2)) {
            continue;   /* a esta cosa ya se le dio su vistazo (uno de lejos y, a lo más, otro de cerca) */
        } else if (nav->vistazo_hecho[i] == 1 && modo_vistazo < NAV_VISTAZO_LEJOS_Y_MEDIO) {
            continue;   /* solo el de lejos */
        } else if (nav->vistazo_cerc[i] >= 3 && modo_vistazo < NAV_VISTAZO_TODOS) {
            continue;   /* ya está encima: torcer ahora solo acerca el carro a lo que haya (y el eco llegaría tarde) */
        }
        if (elegido < 0 || nav->vistazo_cerc[i] > nav->vistazo_cerc[elegido]) {
            elegido = i;   /* si hay de los dos lados, primero lo más cercano */
        }
    }
    if (elegido >= 0) {
        int z = elegido ? ZONA_DERECHA : ZONA_IZQUIERDA;

        nav->vistazo_lado = elegido ? 1 : -1;
        nav->vistazo_fase = 1;
        nav->vistazo_t_ms = ahora_ms;
        nav->vistazo_ecos = 0;
        nav->vistazo_mm = 0;
        nav->vistazo_grados = nav->vistazo_grados_lado[elegido];
        if (nav->vistazo_solo_red[elegido]) {
            nav->vistazo_red_hecho[elegido] = 1;
        } else {
            nav->vistazo_hecho[elegido] = (nav->vistazo_cerc[elegido] >= 2) ? 2 : 1;
        }
        nav->vistazos++;
        diag_escribir(nav, "VISTAZO_DIAG empieza lado=%d zona=%s fuentes=%d celdas=%d cercania=%d cuadros=%d grados=%d lateral_mm=%d", nav->vistazo_lado, nombre_zona(z),
                      nav->fuentes_vision[z], nav->vistazo_n[elegido], nav->vistazo_cerc[elegido], nav->vistazo_cuadros[elegido], nav->vistazo_grados,
                      nav->vistazo_lateral_mm[elegido]);
    }
    return comando;
}

/* Después de cada tick: si el firmware sostiene el rumbo y el carro avanza, el objetivo nuevo (en la convención del firmware) queda listo para
 * navegador_tomar_rumbo_fw cuando cambió al menos NAV_RUMBO_FW_CAMBIO_X10 y pasó NAV_RUMBO_FW_MIN_MS desde el último. La conversión usa la misma referencia que
 * la pose medida de J15 (ángulo de SENS y rumbo al empezar el avance), así que solo vale con una lectura fresca del giroscopio. */
static void rumbo_fw_actualizar(Navegador *nav, uint32_t ahora_ms)
{
    int deseado_x10, objetivo;

    if (!rumbo_fw_activo(nav) || nav->cmd_actual != NAV_CMD_FORWARD || !nav->avance_imu_ref_ok || !rumbo_deseado_x10(nav, &deseado_x10)) {
        return;
    }
    {   /* V2: el objetivo se acerca al deseado sin pasar de NAV_RUMBO_FW_SALTO_MAX grados del rumbo medido ahora (ver la constante) */
        int salto = diferencia_x10(nav->rumbo_x10, deseado_x10);

        if (salto > NAV_RUMBO_FW_SALTO_MAX * 10) {
            deseado_x10 = nav->rumbo_x10 + NAV_RUMBO_FW_SALTO_MAX * 10;
        } else if (salto < -NAV_RUMBO_FW_SALTO_MAX * 10) {
            deseado_x10 = nav->rumbo_x10 - NAV_RUMBO_FW_SALTO_MAX * 10;
        }
    }
    objetivo = nav->avance_imu_ref_x10 - diferencia_x10(nav->avance_rumbo_ref_x10, deseado_x10);
    if (nav->rumbo_fw_hay_enviado) {
        int cambio = objetivo - nav->rumbo_fw_enviado_x10;

        if ((cambio < NAV_RUMBO_FW_CAMBIO_X10 && cambio > -NAV_RUMBO_FW_CAMBIO_X10) || (uint32_t)(ahora_ms - nav->rumbo_fw_t_ms) < (uint32_t)NAV_RUMBO_FW_MIN_MS) {
            return;
        }
    }
    nav->rumbo_fw_objetivo_x10 = objetivo;
    nav->rumbo_fw_enviado_x10 = objetivo;
    nav->rumbo_fw_hay_enviado = 1;
    nav->rumbo_fw_t_ms = ahora_ms;
    nav->rumbo_fw_pedir = 1;
}

int navegador_tomar_giro_fw(Navegador *nav, int *grados_izq_positivo)
{
    if (nav == NULL || grados_izq_positivo == NULL || !nav->giro_fw || !nav->giro_fw_pedir) {
        return 0;
    }
    nav->giro_fw_pedir = 0;
    *grados_izq_positivo = -nav->giro_fw_grados;   /* el navegador cuenta + a la derecha; el firmware, + a la izquierda */
    return 1;
}

int navegador_giro_fw_en_curso(const Navegador *nav)
{
    return nav != NULL && nav->giro_fw && nav->giro_fw_estado == 0;
}

void navegador_aporte_giro(Navegador *nav, int codigo, int real_x10_fw, int ms, int ant, uint32_t ahora_ms)
{
    int real_x10;

    (void)ahora_ms;
    if (nav == NULL) {
        return;
    }
    if (!nav->giro_fw || nav->giro_fw_estado != 0 || (nav->cmd_actual != NAV_CMD_LEFT && nav->cmd_actual != NAV_CMD_RIGHT)) {
        diag_escribir(nav, "SENS_DIAG giro respuesta_ignorada codigo=%d real_x10=%d (no hay un giro por angulo esperando)", codigo, real_x10_fw);
        return;
    }
    if (codigo == NAV_GIRO_ERROR_SIN_DATOS) {
        giro_fw_pasar_a_tiempo(nav, "el_firmware_no_pudo_girar");
        nav->descartar_dt = !nav->sin_arreglo_dt_giro;   /* J16: llega entre ticks; el giro por tiempo empieza cuando la app mande LEFT/RIGHT en el siguiente */
        return;
    }
    real_x10 = (nav->cmd_actual == NAV_CMD_LEFT) ? real_x10_fw : -real_x10_fw;   /* + a la izquierda del firmware → grados en la dirección del comando */
    nav->giro_fw_real_x10 = real_x10;
    nav->giro_fw_ms = ms;
    nav->giro_fw_ant = ant;
    nav->giro_fw_estado = 1;
    if (codigo == NAV_GIRO_OK) {
        nav->giro_fw_fallos = 0;
    } else {
        nav->giro_fw_caido = 1;   /* V2: el siguiente giro se reintenta por ángulo si SENS dice que el giroscopio está listo (hasta NAV_GIRO_FW_FALLOS_MAX fallos) */
        nav->giro_fw_fallos++;
        diag_escribir(nav, "SENS_DIAG giro aviso=%s real_x10=%d objetivo=%d: se usa el angulo real informado y los giros que siguen van por tiempo",
                      (codigo == NAV_GIRO_TIMEOUT) ? "GIRO_TIMEOUT" : "GIRO_ERROR", real_x10, nav->cmd_pedido);
    }
}

void navegador_aporte_sens(Navegador *nav, int mm, int tof_vivo, int tof_valida, int angulo_x10_fw, uint32_t ahora_ms)
{
    navegador_aporte_sens_imu(nav, mm, tof_vivo, tof_valida, angulo_x10_fw, 1, ahora_ms);
}

void navegador_aporte_sens_imu(Navegador *nav, int mm, int tof_vivo, int tof_valida, int angulo_x10_fw, int imu_lista, uint32_t ahora_ms)
{
    if (nav == NULL) {
        return;
    }
    nav->sens_angulo_previo_x10 = nav->sens_angulo_ok ? nav->sens_angulo_x10 : angulo_x10_fw;
    nav->sens_angulo_x10 = angulo_x10_fw;
    nav->sens_angulo_t_ms = ahora_ms;
    nav->sens_angulo_ok = imu_lista ? 1 : 0;
    if (!imu_lista) nav->avance_imu_ref_ok = 0;
    if ((uint32_t)(ahora_ms - nav->tof_t_ms) > NAV_TOF_FRESCO_MS) {
        nav->tof_libre_lecturas = nav->tof_cerca_lecturas = nav->tof_despejado_lecturas = 0;
    }
    /* Un duplicado de SENS no es otra confirmacion. El angulo ya se actualizo. */
    if (ahora_ms == nav->tof_t_ms && (nav->tof_libre_lecturas || nav->tof_cerca_lecturas) &&
        tof_vivo && tof_valida && mm > 0) return;
    nav->tof_vivo = tof_vivo ? 1 : 0;
    nav->tof_mm = mm;
    nav->tof_valida = tof_valida ? 1 : 0;
    nav->tof_t_ms = ahora_ms;
    if (!nav->tof_vivo || !nav->tof_valida || mm <= 0) {
        nav->tof_libre_lecturas = 0;
        nav->tof_cerca_lecturas = 0;
        nav->tof_despejado_lecturas = 0;
        if (nav->usa_tof && nav->tof_vivo && nav->estado == NAV_ESQUIVANDO && nav->fase == NAV_FASE_GIRAR && nav->reintentos == 0 && nav->cambio_lado != 1 && nav->giro_fw &&
            nav->giro_fw_estado == 0 && nav->giro_fw_ang_ini_ok && imu_lista) {
            escaneo_muestra(nav, ahora_ms, 0, 0);   /* V2: una lectura sin eco del barrido también cuenta: ahí no hay nada al alcance */
        }
        return;
    }
    nav->tof_despejado_lecturas = (mm > NAV_FIN_HOLGURA_MM) ? nav->tof_despejado_lecturas + 1 : 0;
    limitar_contador(&nav->tof_despejado_lecturas);
    if (nav->vistazo_lado != 0 && nav->usa_tof && mm <= NAV_VISTAZO_ALCANCE_MM && (uint32_t)(ahora_ms - nav->vistazo_t_ms) >= (uint32_t)NAV_VISTAZO_GIRO_MIN_MS &&
        !laser_tras_el_destino(nav, mm)) {   /* lo que queda más allá del destino (una pared al fondo) no es un obstáculo de la ruta */
        nav->vistazo_ecos++;           /* V2: el láser, torcido hacia lo que vio la cámara, tiene eco válido y cercano */
        if (nav->vistazo_mm == 0 || mm < nav->vistazo_mm) {
            nav->vistazo_mm = mm;
        }
    }
    if (nav->usa_tof && nav->estado == NAV_ESQUIVANDO && nav->fase == NAV_FASE_GIRAR && nav->reintentos == 0 && nav->cambio_lado != 1 && nav->giro_fw &&
        nav->giro_fw_estado == 0 && nav->giro_fw_ang_ini_ok && imu_lista) {
        escaneo_muestra(nav, ahora_ms, mm, mm < TOF_ALCANCE_ESCANEO_MM);   /* V2: el primer giro del rodeo mide el ancho del obstáculo */
    }
    if (mm > NAV_TOF_LIBRE_MM) {
        nav->tof_libre_lecturas++;
        nav->tof_cerca_lecturas = 0;
    } else {
        nav->tof_libre_lecturas = 0;
        if (mm <= NAV_REACCION_MAX_MM) {
            nav->tof_cerca_lecturas++;
        } else {
            nav->tof_cerca_lecturas = 0;
        }
    }
    limitar_contador(&nav->tof_libre_lecturas);
    limitar_contador(&nav->tof_cerca_lecturas);
}

int navegador_tomar_consulta_imu(Navegador *nav)
{
    if (nav == NULL || !nav->giro_fw_consulta_pedir) {
        return 0;
    }
    nav->giro_fw_consulta_pedir = 0;
    return 1;
}

void navegador_aporte_imu(Navegador *nav, int angulo_x10_fw, uint32_t ahora_ms)
{
    if (nav == NULL) {
        return;
    }
    nav->sens_angulo_x10 = angulo_x10_fw;   /* IMU? también es una lectura fresca del ángulo */
    nav->sens_angulo_t_ms = ahora_ms;
    nav->sens_angulo_ok = 1;
    if (nav->giro_fw && nav->giro_fw_estado == 0 && nav->giro_fw_consultando) {
        giro_fw_resolver(nav, angulo_x10_fw, "IMU?");
        if (!nav->giro_fw && !nav->sin_arreglo_dt_giro) {
            nav->descartar_dt = 1;   /* J16: cayó a tiempo para lo que falta, entre ticks: el carro está quieto hasta el siguiente */
        }
    }
}

void navegador_aporte_tof_paro(Navegador *nav, int mm, uint32_t ahora_ms)
{
    if (nav == NULL || !nav->usa_tof) {
        return;
    }
    nav->tof_paros++;
    diag_escribir(nav, "SENS_DIAG tof_paro mm=%d estado=%s cmd=%s t_ms=%lu", mm, navegador_texto_estado(nav->estado), navegador_texto_comando(nav->cmd_actual), (unsigned long)ahora_ms);
    if (nav->cmd_actual != NAV_CMD_FORWARD) {
        return;   /* el firmware solo frena avanzando; si el navegador no iba avanzando no hay nada que hacer */
    }
    switch (nav->estado) {
    case NAV_SIGUIENDO:
        nav->sondeo_fase = 0;     /* si era el sondeo del arranque, aquí no hay duda: es real */
        nav->sondeo_hecho = 1;
        nav->arranque_ok = 1;
        tof_marcar(nav, mm);
        entrar_detenido(nav, ahora_ms);
        break;
    case NAV_ESQUIVANDO:
    case NAV_REINCORPORANDO:
        nav->tof_dist_mm = mm;
        nav->tof_pendiente = 1;   /* lo atiende el siguiente tick según la fase */
        break;
    default:
        break;
    }
}

int navegador_ayuda_comando(Navegador *nav, ComandoNav comando)
{
    if (nav == NULL || (int)comando < (int)NAV_CMD_STOP || (int)comando > (int)NAV_CMD_RIGHT) {
        return NAV_ERR_ARGUMENTO;
    }
    if (nav->estado != NAV_AYUDA) {
        return NAV_ERR_ESTADO;
    }
    nav->ayuda_cmd = comando;
    return NAV_OK;
}

int navegador_ayuda_continuar(Navegador *nav, uint32_t ahora_ms)
{
    if (nav == NULL) {
        return NAV_ERR_ARGUMENTO;
    }
    if (nav->estado != NAV_AYUDA) {
        return NAV_ERR_ESTADO;
    }
    nav->ayuda_cmd = NAV_CMD_STOP;
    nav->verificando_frente = nav->fin_pendiente = 0;
    nav->esquivadas = 0;          /* tras la ayuda se vuelve a poder esquivar */
    nav->esquivadas_obstaculo = 0;
    nav->reintentos = 0;
    nav->error_causa = NAV_CAUSA_NINGUNA;
    nav->desde_ayuda = 1;
    nav->indice_max = nav->indice_objetivo;   /* tras tomar el control el carro pudo quedar en otro lugar: el índice parte de aquí */
    diag_escribir(nav, "AYUDA_DIAG continua pose=(%d,%d,%d)", nav->x_mm, nav->y_mm, nav->rumbo_grados);
    entrar_reincorporando(nav, ahora_ms);   /* quieto 300 ms y calcula la vuelta a la línea con la pose estimada */
    return NAV_OK;
}

/* ── Función principal ─────────────────────────────────────────────────── */

ComandoNav navegador_tick(Navegador *nav, uint32_t ahora_ms, const ResultadoObstaculos *obs)
{
    uint32_t dt;
    ComandoNav comando;

    if (nav == NULL) {
        return NAV_CMD_STOP;
    }
    dt = ahora_ms - nav->t_ultimo_ms;
    if (dt > NAV_DT_MAX_MS) {
        dt = NAV_DT_MAX_MS;
    }
    if (nav->descartar_dt) {   /* J16: el giro cayó a tiempo entre dos ticks: el carro estuvo quieto hasta ahora (la app manda LEFT o RIGHT en este tick) */
        dt = 0;
        nav->descartar_dt = 0;
    }
    nav->t_ultimo_ms = ahora_ms;

    /* Vigilancia del video: si ya llegaban cuadros y pasa más de NAV_MAX_HUECO_VIDEO_MS sin uno nuevo, el carro no ve lo que
     * hay delante: se detiene y queda en ERROR. ERROR no se sale solo (ni cuando el video vuelva): hace falta una ruta nueva. */
    if (nav->usa_camara && nav->estado != NAV_ESPERANDO && nav->estado != NAV_TERMINADO && nav->estado != NAV_ERROR &&
        (uint32_t)(ahora_ms - nav->t_ultimo_cuadro_ms) > NAV_MAX_HUECO_VIDEO_MS) {
        ir_a_error(nav, NAV_MOTIVO_SIN_VIDEO);
    }
    if (obs != NULL) {
        nav->usa_camara = 1;
        nav->t_ultimo_cuadro_ms = ahora_ms;
        registrar_cuadro(nav, ahora_ms, obs);
        if (nav->estado == NAV_ESQUIVANDO || nav->estado == NAV_REINCORPORANDO) {
            esquive_registrar_cuadro(nav, ahora_ms);
        }
    }
    nav->seg_ms += (int)dt;           /* el comando anterior estuvo activo estos dt ms */
    calcular_pose(nav);
    contar_tiempo_ejecutado(nav, (int)dt);
    {   /* distancia que ha recorrido el centro (para el freno anti-locura de «recorrió demasiado») */
        long ddx = (long)nav->x_mm - nav->prev_x_mm, ddy = (long)nav->y_mm - nav->prev_y_mm;

        if (nav->cmd_actual == NAV_CMD_FORWARD || nav->cmd_actual == NAV_CMD_REVERSE) {   /* solo lo que avanza o retrocede: el centro que se mueve al pivotar no es «recorrer de más» (con pivotes grandes sumaba 40 cm por giro y frenaba rutas buenas) */
            int distancia = (int)raiz_entera(ddx * ddx + ddy * ddy);
            nav->recorrido_mm += distancia;
            nav->atras_conocido_mm += nav->cmd_actual == NAV_CMD_FORWARD ? distancia : -distancia;
            if (nav->atras_conocido_mm > 300) nav->atras_conocido_mm = 300;
            if (nav->atras_conocido_mm < 0) nav->atras_conocido_mm = 0;
        }
        nav->prev_x_mm = nav->x_mm;
        nav->prev_y_mm = nav->y_mm;
    }
    if (nav->estado == NAV_ESQUIVANDO || nav->estado == NAV_REINCORPORANDO) {   /* V2: durante un rodeo el carro avanza por la ruta sin tocar indice_objetivo */
        int tramo_v2;
        long s_v2, desvio_v2;

        ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo_v2, &s_v2, &desvio_v2);
        if (tramo_v2 > nav->tramo_base && desvio_v2 < NAV_LOCURA_DESVIO_MM) {
            nav->tramo_base = tramo_v2;
        }
    } else if (nav->estado == NAV_SIGUIENDO) {
        nav->tramo_base = 0;
    }
    revisar_frenos(nav);

    /* Con cámara, nada se mueve hasta el primer cuadro de la ruta; si no llega a tiempo, ERROR sin video (antes recorría la ruta entera a ciegas). */
    if (nav->requiere_camara && !nav->usa_camara && nav->estado != NAV_ESPERANDO && nav->estado != NAV_TERMINADO && nav->estado != NAV_ERROR) {
        if ((uint32_t)(ahora_ms - nav->t_ultimo_cuadro_ms) > (uint32_t)NAV_PRIMER_CUADRO_MAX_MS) {
            ir_a_error(nav, NAV_MOTIVO_SIN_VIDEO);
        }
        cambiar_comando(nav, NAV_CMD_STOP);
        return NAV_CMD_STOP;
    }

    giro_fw_vigilar(nav, ahora_ms);   /* un giro por ángulo sin respuesta cae al método por tiempo */

    if (nav->estado == NAV_ERROR || nav->estado == NAV_AYUDA || nav->estado == NAV_ESPERANDO) {
        nav->fin_pendiente = nav->verificando_frente = 0;
    }

    if (nav->fin_pendiente) {
        comando = NAV_CMD_STOP;
        if (verificar_frente(nav, ahora_ms, NAV_CAUSA_FIN_NO_LIBRE)) {
            nav->fin_verificado = 1;
            nav->fin_pendiente = 0;
            nav->estado = NAV_TERMINADO;
        }
        if (nav->estado == NAV_AYUDA || nav->estado == NAV_ERROR) nav->fin_pendiente = 0;
    } else if (nav->estado == NAV_SIGUIENDO && nav->precaucion_pausa_ms &&
               ahora_ms - nav->precaucion_pausa_ms < NAV_PRECAUCION_PAUSA_MS) {
        comando = NAV_CMD_STOP;
    } else switch (nav->estado) {
    case NAV_SIGUIENDO:
        if (nav->precaucion_pausa_ms) { ajustar_paso_tras_parar(nav); nav->precaucion_pausa_ms = 0; }
        comando = tick_siguiendo(nav, ahora_ms);
        comando = vistazo_tick(nav, ahora_ms, comando);
        if (nav->precaucion && nav->vistazo_lado == 0 && comando == NAV_CMD_FORWARD && nav->cmd_actual == NAV_CMD_FORWARD && nav->seg_ms >= NAV_PRECAUCION_AVANCE_MS) {
            nav->precaucion_pausa_ms = ahora_ms;
            comando = NAV_CMD_STOP;
        }
        break;
    case NAV_DETENIDO_OBSTACULO: comando = tick_detenido(nav, ahora_ms); break;
    case NAV_ESQUIVANDO:       comando = tick_esquivando(nav, ahora_ms); break;
    case NAV_REINCORPORANDO:   comando = tick_reincorporando(nav, ahora_ms); break;
    case NAV_AYUDA:            comando = tick_ayuda(nav, ahora_ms); break;
    default:                   comando = NAV_CMD_STOP; break;   /* ESPERANDO, TERMINADO, ERROR */
    }

    if (nav->estado == NAV_TERMINADO && !nav->fin_verificado &&
        (nav->requiere_camara || nav->usa_camara || nav->usa_tof)) {
        int tramo;
        long s, desvio;
        ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s, &desvio);
        if (nav->obstaculo_pendiente && s < nav->fin_obstaculo_mm) {
            extender_rebase(nav, ahora_ms);
        } else if (nav->obstaculo_pendiente) {
            nav->estado = NAV_SIGUIENDO;   /* cerrando un rodeo: Terminado solo con el frente inmediato comprobado */
            nav->fin_pendiente = 1;
            nav->verificando_frente = 0;
        } else {
            /* V2: no hay ningún rodeo abierto: el carro llegó a su destino. Lo que el láser vea más adelante (una pared, otro mueble) no es parte de la ruta;
             * se anota para que la app lo diga. Antes esto «reintentaba» esquivando la pared. */
            nav->fin_verificado = 1;
            nav->fin_delante_mm = (tof_fresco(nav, ahora_ms) && nav->tof_valida && nav->tof_mm > 0) ? nav->tof_mm : 0;
            diag_escribir(nav, "RUTA_DIAG fin destino_alcanzado sin_rodeo_pendiente laser_mm=%d laser_valido=%d pose=(%d,%d,%d)", nav->tof_mm, nav->tof_valida,
                          nav->x_mm, nav->y_mm, nav->rumbo_grados);
        }
        comando = NAV_CMD_STOP;
    }

    /* Regla de seguridad: sin movimiento en estos tres estados, pase lo que pase arriba. */
    if (nav->estado == NAV_ESPERANDO || nav->estado == NAV_TERMINADO || nav->estado == NAV_ERROR) {
        comando = NAV_CMD_STOP;
    }
    cambiar_comando(nav, comando);
    rumbo_fw_actualizar(nav, ahora_ms);   /* J16: el rumbo que el firmware debe sostener en este avance (si lo sostiene) */
    return comando;
}

/* ── Textos ────────────────────────────────────────────────────────────── */

const char *navegador_texto_comando(ComandoNav comando)
{
    switch (comando) {
    case NAV_CMD_FORWARD: return "FORWARD";
    case NAV_CMD_REVERSE: return "REVERSE";
    case NAV_CMD_LEFT:    return "LEFT";
    case NAV_CMD_RIGHT:   return "RIGHT";
    default:              return "STOP";
    }
}

const char *navegador_texto_estado(EstadoNav estado)
{
    switch (estado) {
    case NAV_ESPERANDO:          return "ESPERANDO";
    case NAV_SIGUIENDO:          return "SIGUIENDO";
    case NAV_DETENIDO_OBSTACULO: return "DETENIDO_OBSTACULO";
    case NAV_ESQUIVANDO:         return "ESQUIVANDO";
    case NAV_REINCORPORANDO:     return "REINCORPORANDO";
    case NAV_TERMINADO:          return "TERMINADO";
    case NAV_ERROR:              return "ERROR";
    case NAV_AYUDA:              return "AYUDA";
    default:                     return "DESCONOCIDO";
    }
}

/* Frase con la causa concreta del ERROR (con los números), lista para mostrar en pantalla. */
int navegador_texto_error(const Navegador *nav, char *texto, int capacidad)
{
    int n = 0;

    if (nav == NULL || texto == NULL || capacidad < 2) {
        return 0;
    }
    texto[0] = '\0';
    switch (nav->error_causa) {
    case NAV_CAUSA_SIN_CONFIRMACION:
        n = snprintf(texto, (size_t)capacidad, "codigo historico: falta confirmacion (ya no se emite por desacuerdo visual)");
        break;
    case NAV_CAUSA_OBSTACULO_NO_PASADO:
        n = snprintf(texto, (size_t)capacidad, "el rebase no cabe en la recta y su extension disponible: no se recorto el plan");
        break;
    case NAV_CAUSA_FIN_NO_LIBRE:
        n = snprintf(texto, (size_t)capacidad, "ruta sin cerrar: el laser no confirma el frente libre con el carro detenido");
        break;
    case NAV_CAUSA_FRENTE_ANCHO:
        n = snprintf(texto, (size_t)capacidad, "pared u objeto muy ancho al frente: el láser lo midió al girar y no hay por dónde rodearlo");
        break;
    case NAV_CAUSA_SIN_PERCEPCION:
        n = snprintf(texto, (size_t)capacidad, "el detector de objetos dejó de dar resultados: no se puede confirmar el frente por visión");
        break;
    case NAV_CAUSA_LADOS:
        n = snprintf(texto, (size_t)capacidad, "los dos lados estuvieron ocupados %d segundos y no había por dónde esquivar", NAV_TIMEOUT_ESQUIVAR_MS / 1000);
        break;
    case NAV_CAUSA_MAX_ESQUIVADAS:
        n = snprintf(texto, (size_t)capacidad, "ya esquivó %d veces y el obstáculo seguía en el camino", NAV_MAX_ESQUIVADAS);
        break;
    case NAV_CAUSA_REINTENTOS:
        n = snprintf(texto, (size_t)capacidad, "se agotaron los %d reintentos, el obstáculo seguía en el centro", NAV_MAX_REINTENTOS);
        break;
    case NAV_CAUSA_PLAN:
        n = snprintf(texto, (size_t)capacidad, "no se pudo calcular el camino para volver a la ruta");
        break;
    case NAV_CAUSA_SIN_VIDEO:
        n = snprintf(texto, (size_t)capacidad, "la cámara dejó de mandar imágenes más de %d ms", NAV_MAX_HUECO_VIDEO_MS);
        break;
    case NAV_CAUSA_ARCO:
        n = snprintf(texto, (size_t)capacidad, "el giro para apartarse mueve el carro %d cm hacia adelante y solo hay %d cm libres hasta el obstáculo", (nav->arco_adelante_mm + 5) / 10,
                     (nav->arco_libre_mm + 5) / 10);
        break;
    case NAV_CAUSA_CORRECCIONES:
        n = snprintf(texto, (size_t)capacidad, "se corrigió la vuelta a la línea %d veces y el carro seguía a más de %d cm del punto planeado", NAV_PLAN_CORRECCIONES, NAV_PLAN_ERROR_MM / 10);
        break;
    case NAV_CAUSA_VUELTA:
        n = snprintf(texto, (size_t)capacidad, "no hay camino para volver a la línea con los giros de pivote del carro");
        break;
    case NAV_CAUSA_GIRO_LOCO:
        n = snprintf(texto, (size_t)capacidad, "se planeó un giro de más de %d grados que la ruta no tiene: se paró en vez de darlo", NAV_LOCURA_GIRO_MAX);
        break;
    case NAV_CAUSA_INDICE:
        n = snprintf(texto, (size_t)capacidad, "el siguiente punto de la ruta retrocedió: se paró para no recorrerla hacia atrás");
        break;
    case NAV_CAUSA_RECORRIDO:
        n = snprintf(texto, (size_t)capacidad, "recorrió más de %d.%d veces la distancia de la ruta (%ld cm contra %ld cm de la ruta)", NAV_LOCURA_RECORRIDO_X10 / 10, NAV_LOCURA_RECORRIDO_X10 % 10,
                     nav->recorrido_mm / 10, nav->ruta_largo_mm / 10);
        break;
    case NAV_CAUSA_LEJOS:
        n = snprintf(texto, (size_t)capacidad, "la posición estimada quedó a más de %d cm de la ruta", NAV_LOCURA_DESVIO_MM / 10);
        break;
    default:
        break;
    }
    return (n < 0) ? 0 : ((n >= capacidad) ? capacidad - 1 : n);
}
