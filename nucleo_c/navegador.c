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

/* Giro más corto (con signo, + derecha) para pasar del rumbo `desde` al rumbo `hasta`: de -179 a 180. */
static int giro_minimo(int desde, int hasta)
{
    int d = normalizar_grados(hasta - desde);

    return (d > 180) ? d - 360 : d;
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

static void calcular_pose(Navegador *nav)
{
    int distancia, grados;

    nav->x_mm = nav->base_x_mm;
    nav->y_mm = nav->base_y_mm;
    nav->rumbo_grados = nav->base_rumbo_grados;

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
        break;
    case NAV_CMD_LEFT:
        grados = grados_girados(nav, CAL_LADO_IZQUIERDA);
        nav->rumbo_grados = normalizar_grados(nav->base_rumbo_grados - grados);
        sumar_desplazamiento_giro(nav, CAL_LADO_IZQUIERDA, grados, nav->base_rumbo_grados);
        break;
    case NAV_CMD_RIGHT:
        grados = grados_girados(nav, CAL_LADO_DERECHA);
        nav->rumbo_grados = normalizar_grados(nav->base_rumbo_grados + grados);
        sumar_desplazamiento_giro(nav, CAL_LADO_DERECHA, grados, nav->base_rumbo_grados);
        break;
    default:
        break;
    }
}

/* Toma la pose actual como punto de partida del siguiente comando. */
static void fijar_pose(Navegador *nav)
{
    nav->base_x_mm = nav->x_mm;
    nav->base_y_mm = nav->y_mm;
    nav->base_rumbo_grados = nav->rumbo_grados;
    nav->seg_ms = 0;
}

/* Al soltar un comando de movimiento el carro NO se detiene al instante: sigue avanzando (o
 * girando) el frenado medido. Se suma a la pose para que la posición estimada lo incluya. */
static void sumar_frenado(Navegador *nav)
{
    int mm, grados;

    switch (nav->cmd_actual) {
    case NAV_CMD_FORWARD:
    case NAV_CMD_REVERSE:
        mm = nav->cal.frenado_mm;
        if (nav->cmd_actual == NAV_CMD_REVERSE) {
            mm = -mm;
        }
        nav->x_mm += dir_x_mil(nav->base_rumbo_grados) * mm / 1000;
        nav->y_mm += dir_y_mil(nav->base_rumbo_grados) * mm / 1000;
        break;
    case NAV_CMD_LEFT:
        grados = cal_frenado_grados(&nav->cal, CAL_LADO_IZQUIERDA);   /* lo que sigue girando al frenar... */
        sumar_desplazamiento_giro(nav, CAL_LADO_IZQUIERDA, grados, nav->rumbo_grados);   /* ...y el centro también se sigue moviendo */
        nav->rumbo_grados = normalizar_grados(nav->rumbo_grados - grados);
        break;
    case NAV_CMD_RIGHT:
        grados = cal_frenado_grados(&nav->cal, CAL_LADO_DERECHA);
        sumar_desplazamiento_giro(nav, CAL_LADO_DERECHA, grados, nav->rumbo_grados);
        nav->rumbo_grados = normalizar_grados(nav->rumbo_grados + grados);
        break;
    default:
        break;
    }
}

static void registrar_fin_comando(Navegador *nav);                      /* se definen más abajo, junto a la cola de diagnóstico */
static void registrar_inicio_comando(Navegador *nav, ComandoNav nuevo);

/* Cambia el comando actual (si es distinto) cerrando el tramo anterior. */
static void cambiar_comando(Navegador *nav, ComandoNav nuevo)
{
    if (nuevo != nav->cmd_actual) {
        sumar_frenado(nav);
        registrar_fin_comando(nav);   /* antes de fijar_pose: necesita la pose de antes y de después del comando */
        fijar_pose(nav);
        nav->giro_visual_x10 = 0;        /* un comando nuevo empieza a medir desde cero */
        nav->visual_cubierto_ms = 0;
        nav->cmd_actual = nuevo;
        registrar_inicio_comando(nav, nuevo);
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
    return nav->angulo_total;
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
        diag_escribir(nav, "AYUDA_DIAG pide ayuda motivo=%d pose=(%d,%d,%d) esquivadas=%d ayudas=%d", (int)motivo, nav->x_mm, nav->y_mm, nav->rumbo_grados, nav->esquivadas, nav->ayudas);
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
    if (cmd == NAV_CMD_LEFT || cmd == NAV_CMD_RIGHT) {
        int lado = (cmd == NAV_CMD_LEFT) ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA;
        int um_adelante, um_derecha;
        int h0 = nav->base_rumbo_grados;
        NavGiro giro;

        hecho = grados_girados(nav, lado) + cal_frenado_grados(&nav->cal, lado);
        cal_leer_desplazamiento(&nav->cal, lado, &um_adelante, &um_derecha);
        giro.lado = lado;
        giro.origen = nav->cmd_origen;
        giro.pedido_grados = nav->cmd_pedido;
        giro.hecho_grados = hecho;
        giro.plan_ms = nav->cmd_plan_ms;
        giro.real_ms = nav->seg_ms;
        giro.desp_adelante_mm = (dx * seno_mil(h0) - dy * seno_mil(h0 + 90)) / 1000;    /* el desplazamiento del mapa, en el marco del carro al empezar el giro */
        giro.desp_derecha_mm = (dx * seno_mil(h0 + 90) + dy * seno_mil(h0)) / 1000;
        diag_escribir(nav, "TURN_DIAG lado=%s origen=%s pedido=%d hecho=%d plan_ms=%d real_ms=%d um_grado=(%d,%d) mm=(%d,%d) carro_mm=(%d,%d) rumbo=%d>%d%s",
                      (lado < 0) ? "IZQ" : "DER", nombre_origen(giro.origen), giro.pedido_grados, hecho, giro.plan_ms, giro.real_ms, um_adelante, um_derecha,
                      dx, dy, giro.desp_adelante_mm, giro.desp_derecha_mm, h0, nav->rumbo_grados,
                      cal_desplazamiento_es_espejo(&nav->cal, lado) ? " aviso=espejo_estimado" : "");
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
    texto[largo] = '\0';
    return largo;
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
    int k0 = nav->indice_objetivo - 1;
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

/* Calcula los pasos desde la celda más cercana a la posición estimada hasta el punto de la
 * ruta al que se dirige (indice_objetivo), mirando hacia el rumbo actual (ajustado al
 * múltiplo de 45 grados más cercano, para no arrastrar errores de giro).
 * El camino entre celdas avanza en diagonal mientras puede y luego recto. */
static int planificar(Navegador *nav)
{
    Punto camino[MAX_PUNTOS];
    const Punto *meta = &nav->ruta[nav->indice_objetivo];
    int orientacion, columna, fila, n, indice_error;

    if (nav->cmd_actual != NAV_CMD_STOP) {
        registrar_fin_comando(nav);   /* el comando sigue (avanzar al siguiente punto sin parar), pero se cierra el registro del tramo que terminó */
    }
    fijar_pose(nav);
    orientacion = indice_direccion(nav->rumbo_grados);
    nav->rumbo_grados = orientacion * 45;
    nav->base_rumbo_grados = nav->rumbo_grados;

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
    nav->requiere_camara = 0;
    nav->paralelo_mm = 0;
    nav->reinc_pasar_primero = 0;
    nav->reinc_plan_x_mm[0] = nav->reinc_plan_x_mm[1] = nav->reinc_plan_x_mm[2] = 0;
    nav->reinc_plan_y_mm[0] = nav->reinc_plan_y_mm[1] = nav->reinc_plan_y_mm[2] = 0;
    nav->reinc_correcciones = 0;
    nav->reinc_rumbo_importa = 1;
    nav->tramo_mm = NAV_TRAMO_ESQUIVAR_CM * 10;
    nav->sin_red_seguridad = 0;
    nav->sin_arreglo_indice = 0;
    nav->reinc_rumbo_tol = NAV_REINC_GIRO_MIN_GRADOS - 1;
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
        memset(nav->obs_hex[z], '0', MASC_HEX_LARGO);
        nav->obs_hex[z][MASC_HEX_LARGO] = '\0';
    }
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
    nav->reinc_pasar_primero = 0;
    {   /* Por ruta: el arranque espera cuadros iguales, el sondeo se puede hacer una vez y los frenos anti-locura parten de cero. */
        int k, previo = nav->rumbo_grados, z;

        nav->arranque_ok = 0;
        nav->arranque_iguales = 0;
        nav->arranque_firma = 0;
        nav->arranque_t0_ms = ahora_ms;
        nav->bloqueo_cerc3 = 0;
        nav->sondeo_hecho = 0;
        nav->sondeo_fase = 0;
        for (z = 0; z < OBS_ZONAS; z++) {
            nav->fijo_ignorar[z] = 0;
            nav->fijo_celdas[z] = 0;
        }
        nav->indice_max = nav->indice_objetivo;
        nav->recorrido_mm = 0;
        nav->prev_x_mm = nav->x_mm;
        nav->prev_y_mm = nav->y_mm;
        nav->ruta_largo_mm = (nav->n_puntos >= 2) ? distancia_hasta_tramo(nav, nav->n_puntos - 1) : 0;
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

/* ¿Este cuadro, por sí solo, ya justifica detenerse? Sí si el obstáculo está muy cerca (3) en cualquier
 * zona, o a media distancia (2) en la zona central. */
static int cuadro_es_inmediato(int zona, int cercania, int celdas)
{
    return (cercania >= 3 && celdas >= NAV_CERC3_AREA_INMEDIATA) || (cercania == 2 && zona == ZONA_CENTRO);
}

/* Guarda lo que se vio en el último cuadro y lleva la cuenta de la RACHA actual: cuántos cuadros
 * seguidos (y desde qué hora, en ms) el frente está bloqueado o libre. Un cuadro de ruido suelto
 * no cambia nada: ver bloqueo_confirmado() y libre_confirmado(). */
static void registrar_cuadro(Navegador *nav, uint32_t ahora_ms, const ResultadoObstaculos *obs)
{
    int z, bloqueado = 0, inmediato = 0, cerc3_chico = 0, causa_normal = 0, firma = 0;

    for (z = 0; z < OBS_ZONAS; z++) {
        int hay = obs->zona[z].hay_obstaculo;

        /* «Sospechoso fijo» (sondeo del arranque): mientras sus celdas no crezcan, esa zona se ignora en esta corrida. */
        if (hay && nav->fijo_ignorar[z] && obs->zona[z].celdas <= nav->fijo_celdas[z] * NAV_SONDEO_IGNORAR_PCT / 100 + NAV_SONDEO_IGNORAR_MAS) {
            hay = 0;
        }
        nav->zona_hay[z] = hay;
        nav->zona_cercania[z] = hay ? obs->zona[z].cercania : 0;
        nav->zona_celdas[z] = obs->zona[z].celdas;
        firma |= (hay ? (obs->zona[z].cercania & 3) : 0) << (2 * z);
        /* Corredor (corredor.h): un obstáculo FUERA del corredor se sigue viendo (zona_hay, para elegir lado) pero no frena. */
        nav->zona_frena[z] = hay && !obs->zona[z].fuera_corredor;
        nav->zona_en_corredor[z] = nav->zona_frena[z] && obs->zona[z].cercania_corredor > 0;   /* el corredor (activo) dice que está en el camino del carro */
        nav->zona_cerc_frena[z] = !nav->zona_frena[z] ? 0 :
            ((obs->zona[z].cercania_corredor > 0) ? obs->zona[z].cercania_corredor : obs->zona[z].cercania);
        if (nav->zona_frena[z] && nav->frenar_lejos_centro && z == ZONA_CENTRO && nav->zona_cerc_frena[z] >= 1 &&
            nav->zona_cerc_frena[z] < NAV_CERCANIA_MIN_DETENER) {
            bloqueado = 1;                       /* frenar desde lejos: cercanía 1 en el centro también bloquea (se confirma con menos tiempo) */
        }
        if (nav->zona_frena[z] && nav->zona_cerc_frena[z] >= NAV_CERCANIA_MIN_DETENER) {
            bloqueado = 1;                       /* un obstáculo cerca en cualquier zona (dentro del corredor, si está activo) bloquea */
            causa_normal = 1;
            if (cuadro_es_inmediato(z, nav->zona_cerc_frena[z], nav->zona_celdas[z])) {
                inmediato = 1;
            } else if (nav->zona_cerc_frena[z] >= 3) {
                cerc3_chico = 1;                 /* cercanía 3 con pocas celdas: frena con 2 cuadros seguidos */
            }
        }
    }
    if (bloqueado) {
        if (nav->frames_bloqueado == 0) {      /* empieza una racha con obstáculo */
            nav->t_bloqueo_ms = ahora_ms;
            nav->bloqueo_inmediato = 0;
            nav->bloqueo_cerc3 = 0;
            nav->solo_lejos_centro = 1;
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
    } else {
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
    /* Para el registro OBS_DIAG: la máscara de cada zona y la franja de referencia de lo último que se vio. */
    for (z = 0; z < OBS_ZONAS; z++) {
        if (masc_zona_a_hex(obs, z, nav->obs_hex[z], (int)sizeof nav->obs_hex[z]) != MASC_OK) {
            memset(nav->obs_hex[z], '0', MASC_HEX_LARGO);
            nav->obs_hex[z][MASC_HEX_LARGO] = '\0';
        }
    }
    nav->obs_franja_tapada = (obs->franja_contaminada || obs->franja_ignorada) ? 1 : 0;
    nav->obs_franja_pct = obs->franja_tapada_pct;
    /* Filas y columnas de las celdas de obstáculo de cada zona (solo se calculan durante la esquivada, para el registro ESQUIVE_DIAG). */
    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_fila_min[z] = nav->zona_fila_max[z] = nav->zona_col_min[z] = nav->zona_col_max[z] = -1;
    }
    if ((nav->estado == NAV_ESQUIVANDO || nav->estado == NAV_REINCORPORANDO) && obs->filas > 0 && obs->filas <= OBS_MAX_FILAS && obs->columnas > 0 && obs->columnas <= OBS_MAX_COLUMNAS &&
        obs->fila_minima_usada >= 0 && obs->fila_minima_usada < obs->filas) {   /* solo con un tamaño de cuadrícula creíble: un resultado armado a medias no debe hacer leer fuera del arreglo */
        int fila, columna;

        for (fila = obs->fila_minima_usada; fila < obs->filas && fila < OBS_MAX_FILAS; fila++) {
            for (columna = 0; columna < obs->columnas && columna < OBS_MAX_COLUMNAS; columna++) {
                if (obs->no_piso[fila][columna]) {
                    int zona = columna * OBS_ZONAS / obs->columnas;

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
                }
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

/* Lo que bloquea durante la esquivada: el CENTRO con cercanía NAV_ESQ_CERCANIA_BLOQUEA o más; y mientras VUELVE a la línea (REINCORPORANDO, avanza en diagonal),
 * también un LADO con cercanía NAV_ESQ_CERCANIA_LADO que el CORREDOR dice que está en el camino del carro: muy cerca y de lado es justo donde queda la esquina de
 * lo que acaba de pasar. Sin corredor (apagado por defecto) un lado no cuenta nunca: la cámara no distingue "al lado del camino" de "en el camino" y frenaría
 * cada vuelta junto al obstáculo (el simulador lo mostró). Al avanzar de lado o en paralelo tampoco: ahí el obstáculo queda de lado a propósito.
 * Devuelve la zona que bloquea o -1. */
static int esq_zona_que_bloquea(const Navegador *nav)
{
    if (nav->zona_frena[ZONA_CENTRO] && nav->zona_cerc_frena[ZONA_CENTRO] >= NAV_ESQ_CERCANIA_BLOQUEA) {
        return ZONA_CENTRO;
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

    nav->estado = NAV_DETENIDO_OBSTACULO;
    nav->t_detenido_ms = ahora_ms;
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

    if (nav->sondeo_hecho || nav->esquivadas != 0 || nav->recorrido_mm > NAV_SONDEO_QUIETO_MM || nav->arranque_firma == 0 || nav->zona_hay[ZONA_CENTRO]) {
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
    /* Fase 2: quieto; los cuadros que llegan después de asentarse se suman (registrar_cuadro). */
    if ((uint32_t)(ahora_ms - nav->sondeo_listo_ms) >= 0x80000000u || ahora_ms - nav->sondeo_listo_ms < (uint32_t)NAV_SONDEO_MIRAR_MS) {
        return NAV_CMD_STOP;
    }
    {
        int antes = nav->sondeo_ref, despues = (nav->sondeo_n > 0) ? (int)(nav->sondeo_suma / nav->sondeo_n) : -1;
        int crece = antes * NAV_SONDEO_CRECE_PCT / 100;
        int z;

        if (crece < NAV_SONDEO_CRECE_MIN) {
            crece = NAV_SONDEO_CRECE_MIN;
        }
        nav->sondeo_fase = 0;
        nav->frames_bloqueado = 0;
        nav->frames_libre = 0;
        nav->bloqueo_inmediato = 0;
        nav->bloqueo_cerc3 = 0;
        if (despues < 0 || despues >= antes + crece) {   /* sin cuadros para comparar, o crecieron: es real (en la duda, se frena) */
            diag_escribir(nav, "OBS_DIAG sondeo=real celdas_antes=%d celdas_despues=%d zonas=%d", antes, despues, nav->sondeo_zonas);
            diag_obs(nav, "sondeo_real");
            entrar_detenido(nav, ahora_ms);
            return NAV_CMD_STOP;
        }
        for (z = 0; z < OBS_ZONAS; z++) {
            if (nav->sondeo_zonas & (1 << z)) {
                nav->fijo_ignorar[z] = 1;
                nav->fijo_celdas[z] = (nav->zona_celdas[z] > antes) ? nav->zona_celdas[z] : antes;
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
    int normal = NAV_TRAMO_ESQUIVAR_CM * 10;
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
    tramo = ((long)NAV_ESQ_MARGEN_LATERAL_MM - lat_giro - lat_vuelta) * 1000 / por_mm;
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
    int cara = (nav->salida_cercania >= 3) ? NAV_CARA_CERCANIA_3_MM : ((nav->salida_cercania == 2) ? NAV_CARA_CERCANIA_2_MM : NAV_CARA_CERCANIA_1_MM);

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
                s_obj = vertice_mas_cercano(nav, s_obj, s_pos + NAV_REINC_VERTICE_MIN_MM);                /* ...pegado a un centro de celda, que es desde donde se retoma la ruta */
                punto_en_linea(nav, s_obj, &tx, &ty, &tramo_obj);
                dx = tx - nav->x_mm;
                dy = ty - nav->y_mm;
                distancia = raiz_entera(dx * dx + dy * dy);
                if (distancia < 50) {
                    break;   /* ya está en el punto: solo corrige el rumbo */
                }
                hay_plan = planear_vuelta(nav, &p0, tx, ty, rumbo_del_tramo(nav, tramo_obj), &plan);
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
    if (nav->reinc_giro1 > -NAV_REINC_GIRO_MIN_GRADOS && nav->reinc_giro1 < NAV_REINC_GIRO_MIN_GRADOS) {
        nav->reinc_giro1 = 0;     /* un giro tan chico no se hace: el error de la estimación es mayor */
    }
    if (nav->reinc_giro2 > -NAV_REINC_GIRO_MIN_GRADOS && nav->reinc_giro2 < NAV_REINC_GIRO_MIN_GRADOS) {
        nav->reinc_giro2 = 0;
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

/* Hasta dónde (distancia a lo largo de la ruta) hay que llegar para haber pasado el obstáculo que detuvo al carro en la pose actual: la cara (según la cercanía con la
 * que frenó) más su fondo más el margen. Nunca más allá del final del tramo RECTO en el que está: el paralelo avanza en línea recta y la ruta puede dar vuelta
 * (en «Ida y vuelta» o en una esquina el paralelo seguía de frente hasta 55 cm fuera de la ruta). Si ese tramo recto es el último, se queda NAV_PARALELO_FIN_MM antes del final. */
static long calcular_fin_obstaculo(const Navegador *nav, int cercania)
{
    int tramo, k, rumbo;
    long s_pos, desvio, hasta, limite;
    int cara = (cercania >= 3) ? NAV_CARA_CERCANIA_3_MM : ((cercania == 2) ? NAV_CARA_CERCANIA_2_MM : NAV_CARA_CERCANIA_1_MM);

    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s_pos, &desvio);
    hasta = s_pos + cara + NAV_OBSTACULO_PROFUNDO_MM + NAV_PASO_MARGEN_MM;
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
    return (hasta > limite) ? limite : hasta;
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

/* DETENIDO_OBSTACULO: STOP. Si el frente se libera (varios cuadros seguidos), sigue. Si no,
 * tras una pausa elige un lado libre para esquivar; si no hay ninguno, espera hasta el
 * tiempo máximo y entonces ERROR. */
static ComandoNav tick_detenido(Navegador *nav, uint32_t ahora_ms)
{
    uint32_t espera = ahora_ms - nav->t_detenido_ms;

    if (libre_confirmado(nav, ahora_ms) && nav->volver_a_reinc) {
        /* Se detuvo mientras volvía a la línea y el frente ya está libre: vuelve a calcular la vuelta desde donde quedó. */
        nav->volver_a_reinc = 0;
        nav->frames_bloqueado = 0;
        nav->bloqueo_inmediato = 0;
        nav->bloqueo_cerc3 = 0;
        entrar_reincorporando(nav, ahora_ms);
        return NAV_CMD_STOP;
    }
    if (libre_confirmado(nav, ahora_ms)) {
        nav->estado = NAV_SIGUIENDO;   /* el obstáculo se fue: continuar donde iba */
        /* El carro siguió un poco al frenar (se gana tiempo de ese paso) y tendrá que arrancar otra vez
         * (se pierde): se ajusta el tiempo ya ejecutado del paso. */
        nav->ms_paso += (int)((long)nav->cal.frenado_mm * nav->cal.ms_por_cm_x10 / 100) - nav->cal.arranque_ms;
        nav->frames_bloqueado = 0;
        nav->bloqueo_inmediato = 0;
        nav->bloqueo_cerc3 = 0;
        return tick_siguiendo(nav, ahora_ms);
    }
    if (espera >= NAV_PAUSA_MS && nav->frames_recibidos >= 1) {
        nav->salida_cercania = cercania_que_freno(nav);
        nav->fin_obstaculo_mm = (int)calcular_fin_obstaculo(nav, nav->salida_cercania);   /* lo usan el paralelo y la vuelta tras «toma el control» */
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
                    ir_a_error_causa(nav, NAV_MOTIVO_ESQUIVANDO, NAV_CAUSA_ARCO);
                    return NAV_CMD_STOP;
                }
            }
            if (nav->esquivadas >= NAV_MAX_ESQUIVADAS) {
                ir_a_error(nav, NAV_MOTIVO_MAX_ESQUIVADAS);
                return NAV_CMD_STOP;
            }
            diag_obs(nav, "esquiva");
            nav->esquivadas++;
            nav->volver_a_reinc = 0;
            nav->salida_x_mm = nav->x_mm;          /* de aquí sale de la línea: a este punto se mide después el regreso */
            nav->salida_y_mm = nav->y_mm;
            nav->salida_rumbo = nav->rumbo_grados;
            nav->fase = NAV_FASE_GIRAR;
            nav->ms_fase = 0;
            nav->reintentos = 0;
            nav->angulo_giro = NAV_ANGULO_ESQUIVAR;
            nav->angulo_total = 0;
            nav->estado = NAV_ESQUIVANDO;
            esq_iniciar_giro(nav, ahora_ms);
            return comando_de_fase(nav);
        }
        if (espera >= NAV_TIMEOUT_ESQUIVAR_MS) {
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

    if (nav->fase == NAV_FASE_MIRAR) {
        return tick_mirar(nav, ahora_ms);
    }
    if (nav->fase == NAV_FASE_PAUSA_PASAR) {   /* quieto un momento después de enderezarse; luego avanza en paralelo hasta pasar el obstáculo, o ya vuelve */
        if (nav->ms_fase + NAV_TICK_MS / 2 < NAV_PAUSA_ASENTAR_MS) {
            return NAV_CMD_STOP;
        }
        nav->paralelo_mm = calcular_paralelo_mm(nav);
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

/* REINCORPORANDO: vuelve a la línea de la ruta con la pose estimada (ver reinc_calcular): PLAN → GIRO1 → pausa → AVANZAR → pausa → GIRO2 → pausa → SIGUIENDO.
 * Entre cada movimiento hay la pausa de siempre. Mientras avanza hacia la línea vigila el frente con la misma regla de la esquivada: si el centro se bloquea,
 * se detiene y (si el obstáculo sigue) esquiva otra vez; si se libera, vuelve a calcular la vuelta. */
static ComandoNav tick_reincorporando(Navegador *nav, uint32_t ahora_ms)
{
    int fase = nav->reinc_fase, siguiente, giro;

    switch (fase) {
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
    nav->esquivadas = 0;          /* tras la ayuda se vuelve a poder esquivar */
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

        nav->recorrido_mm += raiz_entera(ddx * ddx + ddy * ddy);
        nav->prev_x_mm = nav->x_mm;
        nav->prev_y_mm = nav->y_mm;
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

    switch (nav->estado) {
    case NAV_SIGUIENDO:        comando = tick_siguiendo(nav, ahora_ms); break;
    case NAV_DETENIDO_OBSTACULO: comando = tick_detenido(nav, ahora_ms); break;
    case NAV_ESQUIVANDO:       comando = tick_esquivando(nav, ahora_ms); break;
    case NAV_REINCORPORANDO:   comando = tick_reincorporando(nav, ahora_ms); break;
    case NAV_AYUDA:            comando = tick_ayuda(nav, ahora_ms); break;
    default:                   comando = NAV_CMD_STOP; break;   /* ESPERANDO, TERMINADO, ERROR */
    }

    /* Regla de seguridad: sin movimiento en estos tres estados, pase lo que pase arriba. */
    if (nav->estado == NAV_ESPERANDO || nav->estado == NAV_TERMINADO || nav->estado == NAV_ERROR) {
        comando = NAV_CMD_STOP;
    }
    cambiar_comando(nav, comando);
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
