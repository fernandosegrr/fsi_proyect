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
#include "navegador.h"
#include "calibracion.h"

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
static void calcular_pose(Navegador *nav)
{
    int distancia;

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
        nav->rumbo_grados = normalizar_grados(nav->base_rumbo_grados - grados_girados(nav, CAL_LADO_IZQUIERDA));
        break;
    case NAV_CMD_RIGHT:
        nav->rumbo_grados = normalizar_grados(nav->base_rumbo_grados + grados_girados(nav, CAL_LADO_DERECHA));
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
    int mm;

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
        nav->rumbo_grados = normalizar_grados(nav->rumbo_grados - cal_frenado_grados(&nav->cal, CAL_LADO_IZQUIERDA));
        break;
    case NAV_CMD_RIGHT:
        nav->rumbo_grados = normalizar_grados(nav->rumbo_grados + cal_frenado_grados(&nav->cal, CAL_LADO_DERECHA));
        break;
    default:
        break;
    }
}

/* Cambia el comando actual (si es distinto) cerrando el tramo anterior. */
static void cambiar_comando(Navegador *nav, ComandoNav nuevo)
{
    if (nuevo != nav->cmd_actual) {
        sumar_frenado(nav);
        fijar_pose(nav);
        nav->giro_visual_x10 = 0;        /* un comando nuevo empieza a medir desde cero */
        nav->visual_cubierto_ms = 0;
        nav->cmd_actual = nuevo;
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

/* Para deshacer el giro de la esquivada (angulo_total grados hacia `lado`): si fue de 180 grados o menos se gira de vuelta hacia el lado
 * CONTRARIO esos grados; si fue más (225 tras dos reintentos) es más corto seguir girando hacia el MISMO lado los 360 - total que faltan. */
static int lado_de_vuelta(const Navegador *nav)
{
    return (nav->angulo_total <= 180) ? -nav->lado : nav->lado;
}

static int grados_de_vuelta(const Navegador *nav)
{
    return (nav->angulo_total <= 180) ? nav->angulo_total : 360 - nav->angulo_total;
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
        return cal_cm_a_ms(&nav->cal, NAV_TRAMO_ESQUIVAR_CM);
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

/* Pasa a ERROR. `causa` dice qué pasó en concreto (NAV_CAUSA_*; ver navegador_texto_error). */
static void ir_a_error_causa(Navegador *nav, MotivoErrorNav motivo, int causa)
{
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
    return NAV_OK;
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
    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_celdas[z] = 0;
        nav->zona_frena[z] = 0;
        nav->zona_cerc_frena[z] = 0;
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
    nav->salida_x_mm = nav->salida_y_mm = nav->salida_rumbo = 0;
    nav->salida_cercania = 0;
    nav->paralelo_mm = 0;
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
    nav->frames_libre = 0;
    nav->frames_recibidos = 0;
    nav->cmd_actual = NAV_CMD_STOP;
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
    nav->frames_libre = 0;
    nav->frames_recibidos = 0;

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
static int cuadro_es_inmediato(int zona, int cercania)
{
    return cercania >= 3 || (cercania == 2 && zona == ZONA_CENTRO);
}

/* Guarda lo que se vio en el último cuadro y lleva la cuenta de la RACHA actual: cuántos cuadros
 * seguidos (y desde qué hora, en ms) el frente está bloqueado o libre. Un cuadro de ruido suelto
 * no cambia nada: ver bloqueo_confirmado() y libre_confirmado(). */
static void registrar_cuadro(Navegador *nav, uint32_t ahora_ms, const ResultadoObstaculos *obs)
{
    int z, bloqueado = 0, inmediato = 0, causa_normal = 0;

    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_hay[z] = obs->zona[z].hay_obstaculo;
        nav->zona_cercania[z] = obs->zona[z].cercania;
        nav->zona_celdas[z] = obs->zona[z].celdas;
        /* Corredor (corredor.h): un obstáculo FUERA del corredor se sigue viendo (zona_hay, para elegir lado) pero no frena. */
        nav->zona_frena[z] = obs->zona[z].hay_obstaculo && !obs->zona[z].fuera_corredor;
        nav->zona_cerc_frena[z] = !nav->zona_frena[z] ? 0 :
            ((obs->zona[z].cercania_corredor > 0) ? obs->zona[z].cercania_corredor : obs->zona[z].cercania);
        if (nav->zona_frena[z] && nav->frenar_lejos_centro && z == ZONA_CENTRO && nav->zona_cerc_frena[z] >= 1 &&
            nav->zona_cerc_frena[z] < NAV_CERCANIA_MIN_DETENER) {
            bloqueado = 1;                       /* frenar desde lejos: cercanía 1 en el centro también bloquea (se confirma con menos tiempo) */
        }
        if (nav->zona_frena[z] && nav->zona_cerc_frena[z] >= NAV_CERCANIA_MIN_DETENER) {
            bloqueado = 1;                       /* un obstáculo cerca en cualquier zona (dentro del corredor, si está activo) bloquea */
            causa_normal = 1;
            if (cuadro_es_inmediato(z, nav->zona_cerc_frena[z])) {
                inmediato = 1;
            }
        }
    }
    if (bloqueado) {
        if (nav->frames_bloqueado == 0) {      /* empieza una racha con obstáculo */
            nav->t_bloqueo_ms = ahora_ms;
            nav->bloqueo_inmediato = 0;
            nav->solo_lejos_centro = 1;
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
    }
    /* Filas y columnas de las celdas de obstáculo de cada zona (solo se calculan durante la esquivada, para el registro ESQUIVE_DIAG). */
    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_fila_min[z] = nav->zona_fila_max[z] = nav->zona_col_min[z] = nav->zona_col_max[z] = -1;
    }
    if ((nav->estado == NAV_ESQUIVANDO || nav->estado == NAV_REINCORPORANDO) && obs->filas > 0 && obs->columnas > 0) {
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
    if (nav->esq_en_giro) {
        return 1;
    }
    return nav->esq_cuadros_tras_giro < NAV_ESQ_CUADROS_ESTABLES && (uint32_t)(ahora_ms - nav->esq_t_giro_fin_ms) < (uint32_t)NAV_ESQ_MS_ESTABLE;
}

/* Cuadro nuevo durante la esquivada (ESQUIVANDO o REINCORPORANDO): lleva la racha del obstáculo CENTRAL (cercanía >= NAV_ESQ_CERCANIA_BLOQUEA) sin contar los
 * cuadros que se ignoran, y escribe la línea ESQUIVE_DIAG de lo que se vio (fase, cercanía, zona, celdas, filas y columnas, ms desde el inicio del giro, y qué se
 * hizo con ello). Los lados y lo lejano se anotan pero no cuentan. */
static void esquive_registrar_cuadro(Navegador *nav, uint32_t ahora_ms)
{
    int z, mejor = -1, ignorada;
    const char *accion;

    if (!nav->esq_en_giro && nav->esq_cuadros_tras_giro < LIMITE_CONTADOR) {
        nav->esq_cuadros_tras_giro++;
    }
    ignorada = esq_ignorando(nav, ahora_ms);
    if (ignorada || !(nav->zona_frena[ZONA_CENTRO] && nav->zona_cerc_frena[ZONA_CENTRO] >= NAV_ESQ_CERCANIA_BLOQUEA)) {
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
    } else if (mejor != ZONA_CENTRO && !(nav->zona_frena[ZONA_CENTRO] && nav->zona_cerc_frena[ZONA_CENTRO] >= NAV_ESQ_CERCANIA_BLOQUEA)) {
        accion = "lateral_no_bloquea";
    } else if (!(nav->zona_frena[ZONA_CENTRO] && nav->zona_cerc_frena[ZONA_CENTRO] >= NAV_ESQ_CERCANIA_BLOQUEA)) {
        accion = "lejos_no_bloquea";
    } else {
        accion = "centro_cuenta";
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
static ComandoNav tick_siguiendo(Navegador *nav, uint32_t ahora_ms)
{
    ComandoNav comando = NAV_CMD_STOP;
    int vueltas = 0;

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
        nav->estado = NAV_DETENIDO_OBSTACULO;
        nav->t_detenido_ms = ahora_ms;
        nav->frames_recibidos = 0;
        nav->frames_libre = 0;
        return NAV_CMD_STOP;
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
    nav->frames_libre = 0;
    nav->centro_frames = 0;
    reinc_cambiar_fase(nav, NAV_RF_PLAN, ahora_ms);
}

/* Calcula la vuelta con la pose estimada: el punto de la línea NAV_REINC_ADELANTE_CM cm por delante del punto de salida, el giro mínimo hacia él, la distancia
 * y el giro final al rumbo de la línea. Si el carro está a menos de NAV_REINC_DESVIO_MIN_MM de la línea, solo corrige el rumbo. */
static void reinc_calcular(Navegador *nav)
{
    int tramo_pos, tramo_obj = 0;
    long s_pos, desvio_pos, s_obj, tx = 0, ty = 0;

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

    if (desvio_pos >= NAV_REINC_DESVIO_MIN_MM) {
        long dx, dy, distancia;
        int rumbo_hacia;

        s_obj = s_pos + (long)NAV_REINC_ADELANTE_CM * 10;   /* 30 cm por delante de donde el carro ya pasó el obstáculo (el punto de salida) */
        punto_en_linea(nav, s_obj, &tx, &ty, &tramo_obj);
        dx = tx - nav->x_mm;
        dy = ty - nav->y_mm;
        distancia = raiz_entera(dx * dx + dy * dy);
        if (distancia >= 50) {
            rumbo_hacia = rumbo_del_vector(dx, dy, nav->rumbo_grados);
            nav->reinc_solo_rumbo = 0;
            nav->reinc_obj_x_mm = (int)tx;
            nav->reinc_obj_y_mm = (int)ty;
            nav->reinc_rumbo_final = rumbo_del_tramo(nav, tramo_obj);
            nav->reinc_giro1 = giro_minimo(nav->rumbo_grados, rumbo_hacia);
            nav->reinc_dist_mm = (int)distancia;
            nav->reinc_giro2 = giro_minimo(rumbo_hacia, nav->reinc_rumbo_final);
            nav->reinc_indice_obj = tramo_obj + 1;
        }
    }
    if (nav->reinc_solo_rumbo) {
        nav->reinc_giro1 = giro_minimo(nav->rumbo_grados, nav->reinc_rumbo_final);   /* solo corrige el rumbo */
    }
    if (nav->reinc_giro1 > -NAV_REINC_GIRO_MIN_GRADOS && nav->reinc_giro1 < NAV_REINC_GIRO_MIN_GRADOS) {
        nav->reinc_giro1 = 0;     /* un giro tan chico no se hace: el error de la estimación es mayor */
    }
    if (nav->reinc_giro2 > -NAV_REINC_GIRO_MIN_GRADOS && nav->reinc_giro2 < NAV_REINC_GIRO_MIN_GRADOS) {
        nav->reinc_giro2 = 0;
    }
    diag_escribir(nav, "REINC_DIAG plan pose_antes=(%d,%d,%d) salida=(%d,%d,%d) desvio_mm=%d solo_rumbo=%d objetivo=(%d,%d) dist_mm=%d giro1=%d giro2=%d rumbo_linea=%d esquivadas=%d",
                  nav->x_mm, nav->y_mm, nav->rumbo_grados, nav->salida_x_mm, nav->salida_y_mm, nav->salida_rumbo, nav->reinc_desvio_mm, nav->reinc_solo_rumbo,
                  nav->reinc_obj_x_mm, nav->reinc_obj_y_mm, nav->reinc_dist_mm, nav->reinc_giro1, nav->reinc_giro2, nav->reinc_rumbo_final, nav->esquivadas);
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

/* Terminó la vuelta: anota la pose final, se dirige al siguiente punto de la ruta y sigue (SIGUIENDO). */
static ComandoNav reinc_terminar(Navegador *nav)
{
    long s, desvio;
    int tramo;

    if (!nav->reinc_solo_rumbo) {
        nav->indice_objetivo = nav->reinc_indice_obj;
    }
    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s, &desvio);
    diag_escribir(nav, "REINC_DIAG fin pose_despues=(%d,%d,%d) desvio_mm=%d siguiente_punto=%d", nav->x_mm, nav->y_mm, nav->rumbo_grados, (int)desvio, nav->indice_objetivo);
    if (planificar(nav) != NAV_OK) {
        ir_a_error(nav, NAV_MOTIVO_PLAN);
        return NAV_CMD_STOP;
    }
    nav->estado = NAV_SIGUIENDO;
    return NAV_CMD_STOP;
}

/* La mayor cercanía (1 a 3) con la que algo dentro del corredor frenó al carro. Dice a qué distancia estaba el obstáculo cuando se detuvo. */
static int cercania_que_freno(const Navegador *nav)
{
    int z, mayor = 1;

    for (z = 0; z < OBS_ZONAS; z++) {
        if (nav->zona_frena[z] && nav->zona_cerc_frena[z] > mayor) {
            mayor = nav->zona_cerc_frena[z];
        }
    }
    return mayor;
}

/* Cuánto debe avanzar en paralelo para pasar el obstáculo: desde el punto de salida, hasta la cara del obstáculo (según la cercanía con la que frenó) más su fondo más
 * el margen, sin acercarse al final de la ruta más que NAV_PARALELO_FIN_MM; menos lo que ya avanzó. 0 si no hace falta. */
static int calcular_paralelo_mm(const Navegador *nav)
{
    int tramo;
    long s_salida, s_pos, desvio, hasta, total;
    int cara = (nav->salida_cercania >= 3) ? NAV_CARA_CERCANIA_3_MM : ((nav->salida_cercania == 2) ? NAV_CARA_CERCANIA_2_MM : NAV_CARA_CERCANIA_1_MM);

    ubicar_en_linea(nav, nav->salida_x_mm, nav->salida_y_mm, &tramo, &s_salida, &desvio);
    ubicar_en_linea(nav, nav->x_mm, nav->y_mm, &tramo, &s_pos, &desvio);
    total = distancia_hasta_tramo(nav, nav->n_puntos - 1);
    hasta = s_salida + cara + NAV_OBSTACULO_PROFUNDO_MM + NAV_PASO_MARGEN_MM;
    if (hasta > total - NAV_PARALELO_FIN_MM) {
        hasta = total - NAV_PARALELO_FIN_MM;
    }
    return (hasta - s_pos >= NAV_PARALELO_MIN_MM) ? (int)(hasta - s_pos) : 0;
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
        return tick_siguiendo(nav, ahora_ms);
    }
    if (espera >= NAV_PAUSA_MS && nav->frames_recibidos >= 1) {
        nav->lado = elegir_lado(nav);
        if (nav->lado != 0) {
            if (nav->esquivadas >= NAV_MAX_ESQUIVADAS) {
                ir_a_error(nav, NAV_MOTIVO_MAX_ESQUIVADAS);
                return NAV_CMD_STOP;
            }
            nav->esquivadas++;
            nav->volver_a_reinc = 0;
            nav->salida_x_mm = nav->x_mm;          /* de aquí sale de la línea: a este punto se mide después el regreso */
            nav->salida_y_mm = nav->y_mm;
            nav->salida_rumbo = nav->rumbo_grados;
            nav->salida_cercania = cercania_que_freno(nav);
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
        case NAV_FASE_AVANZAR:
            nav->fase = NAV_FASE_VOLVER;
            esq_iniciar_giro(nav, ahora_ms);
            break;
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
        diag_escribir(nav, "ESQUIVE_DIAG fase=AVANZAR cerc=%d zona=CEN celdas=%d reintentos=%d accion=BLOQUEA_reintento",
                      nav->zona_cerc_frena[ZONA_CENTRO], nav->zona_celdas[ZONA_CENTRO], nav->reintentos);
        return esquive_reintentar(nav);
    }
    if (nav->fase == NAV_FASE_PARALELO && esquive_centro_confirmado(nav, ahora_ms)) {
        diag_escribir(nav, "ESQUIVE_DIAG fase=PARALELO cerc=%d zona=CEN celdas=%d accion=BLOQUEA_se_detiene",
                      nav->zona_cerc_frena[ZONA_CENTRO], nav->zona_celdas[ZONA_CENTRO]);
        nav->estado = NAV_DETENIDO_OBSTACULO;   /* algo bloquea el carril paralelo: se detiene y, si sigue, esquiva otra vez */
        nav->volver_a_reinc = 1;
        nav->t_detenido_ms = ahora_ms;
        nav->frames_recibidos = 0;
        nav->frames_libre = 0;
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
        reinc_calcular(nav);
        siguiente = reinc_siguiente_movimiento(nav, NAV_RF_PLAN);
        if (siguiente < 0) {
            return reinc_terminar(nav);
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
            diag_escribir(nav, "ESQUIVE_DIAG fase=VUELTA_AVANZAR cerc=%d zona=CEN celdas=%d accion=BLOQUEA_se_detiene",
                          nav->zona_cerc_frena[ZONA_CENTRO], nav->zona_celdas[ZONA_CENTRO]);
            nav->estado = NAV_DETENIDO_OBSTACULO;   /* el tramo de vuelta está bloqueado: se detiene y, si sigue, esquiva otra vez */
            nav->volver_a_reinc = 1;
            nav->t_detenido_ms = ahora_ms;
            nav->frames_recibidos = 0;
            nav->frames_libre = 0;
            nav->frames_bloqueado = 0;
            return NAV_CMD_STOP;
        }
        return NAV_CMD_FORWARD;
    default:   /* las tres pausas */
        if ((uint32_t)(ahora_ms - nav->reinc_t_fase_ms) < (uint32_t)NAV_PAUSA_ASENTAR_MS) {
            return NAV_CMD_STOP;
        }
        siguiente = reinc_siguiente_movimiento(nav, fase - 1);
        if (siguiente < 0) {
            return reinc_terminar(nav);
        }
        reinc_cambiar_fase(nav, siguiente, ahora_ms);
        if (siguiente != NAV_RF_AVANZAR) {
            esq_iniciar_giro(nav, ahora_ms);
        }
        return comando_de_reinc(nav);
    }
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

    switch (nav->estado) {
    case NAV_SIGUIENDO:        comando = tick_siguiendo(nav, ahora_ms); break;
    case NAV_DETENIDO_OBSTACULO: comando = tick_detenido(nav, ahora_ms); break;
    case NAV_ESQUIVANDO:       comando = tick_esquivando(nav, ahora_ms); break;
    case NAV_REINCORPORANDO:   comando = tick_reincorporando(nav, ahora_ms); break;
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
    default:
        break;
    }
    return (n < 0) ? 0 : ((n >= capacidad) ? capacidad - 1 : n);
}
