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
#include "navegador.h"
#include "calibracion.h"

#define MM_POR_CELDA   (CELDA_CM * 10)
#define LIMITE_CONTADOR 1000   /* los contadores de cuadros no crecen más que esto */

/* Vector unitario (en milésimas) de cada una de las 8 direcciones: 0 = arriba, sentido horario.
 * x crece hacia la derecha y y hacia abajo, igual que la cuadrícula. */
static const int DIR_X[8] = { 0,  707, 1000,  707,     0,  -707, -1000, -707 };
static const int DIR_Y[8] = { -1000, -707, 0,  707,  1000,   707,     0, -707 };

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

void navegador_aporte_visual(Navegador *nav, int giro_grados_x10, int dt_ms, int confiable)
{
    if (nav == NULL || !confiable || dt_ms <= 0) {
        return;
    }
    if (nav->cmd_actual != NAV_CMD_LEFT && nav->cmd_actual != NAV_CMD_RIGHT) {
        return;   /* solo cuenta mientras gira */
    }
    if (nav->estado != NAV_SIGUIENDO && nav->estado != NAV_ESQUIVANDO) {
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
    int distancia, direccion;

    nav->x_mm = nav->base_x_mm;
    nav->y_mm = nav->base_y_mm;
    nav->rumbo_grados = nav->base_rumbo_grados;

    switch (nav->cmd_actual) {
    case NAV_CMD_FORWARD:
    case NAV_CMD_REVERSE:
        distancia = cal_ms_a_mm(&nav->cal, nav->seg_ms);   /* milímetros recorridos */
        if (nav->cmd_actual == NAV_CMD_REVERSE) {
            distancia = -distancia;
        }
        direccion = indice_direccion(nav->base_rumbo_grados);
        nav->x_mm = nav->base_x_mm + DIR_X[direccion] * distancia / 1000;
        nav->y_mm = nav->base_y_mm + DIR_Y[direccion] * distancia / 1000;
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
    int direccion, mm;

    switch (nav->cmd_actual) {
    case NAV_CMD_FORWARD:
    case NAV_CMD_REVERSE:
        mm = nav->cal.frenado_mm;
        if (nav->cmd_actual == NAV_CMD_REVERSE) {
            mm = -mm;
        }
        direccion = indice_direccion(nav->base_rumbo_grados);
        nav->x_mm += DIR_X[direccion] * mm / 1000;
        nav->y_mm += DIR_Y[direccion] * mm / 1000;
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

/* Comando de la fase de esquivar actual: 0 = girar para apartarse, 1 = avanzar de lado,
 * 2 = girar de vuelta (al lado contrario del primer giro). */
static ComandoNav comando_de_fase(const Navegador *nav)
{
    if (nav->fase == 1) {
        return NAV_CMD_FORWARD;
    }
    if (nav->fase == 0) {
        return (nav->lado < 0) ? NAV_CMD_LEFT : NAV_CMD_RIGHT;
    }
    return (nav->lado < 0) ? NAV_CMD_RIGHT : NAV_CMD_LEFT;
}

/* Cuánto debe durar cada fase de esquivar, en ms. */
static int duracion_fase(const Navegador *nav, int fase)
{
    if (fase == 1) {
        return cal_cm_a_ms(&nav->cal, NAV_TRAMO_ESQUIVAR_CM);
    }
    /* La fase 0 gira hacia el lado elegido y la 2 hacia el contrario: cada lado tiene su tiempo. */
    if (fase == 0) {
        return cal_grados_a_ms(&nav->cal, (nav->lado < 0) ? -NAV_ANGULO_ESQUIVAR : NAV_ANGULO_ESQUIVAR);
    }
    return cal_grados_a_ms(&nav->cal, (nav->lado < 0) ? NAV_ANGULO_ESQUIVAR : -NAV_ANGULO_ESQUIVAR);
}

static void ir_a_error(Navegador *nav, MotivoErrorNav motivo)
{
    nav->estado = NAV_ERROR;
    nav->motivo_error = motivo;
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
    }
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
    nav->esquivadas = 0;
    nav->lado = -1;
    nav->fase = 0;
    nav->ms_fase = 0;
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
    nav->esquivadas = 0;
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
    int z, bloqueado = 0, inmediato = 0;

    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_hay[z] = obs->zona[z].hay_obstaculo;
        nav->zona_cercania[z] = obs->zona[z].cercania;
        if (nav->zona_hay[z] && nav->zona_cercania[z] >= NAV_CERCANIA_MIN_DETENER) {
            bloqueado = 1;                       /* un obstáculo cerca en cualquier zona bloquea */
            if (cuadro_es_inmediato(z, nav->zona_cercania[z])) {
                inmediato = 1;
            }
        }
    }
    if (bloqueado) {
        if (nav->frames_bloqueado == 0) {      /* empieza una racha con obstáculo */
            nav->t_bloqueo_ms = ahora_ms;
            nav->bloqueo_inmediato = 0;
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
    return nav->frames_bloqueado >= 2 && (ahora_ms - nav->t_bloqueo_ms) >= (uint32_t)MS_CONFIRMAR_LEJOS;
}

/* ¿El frente lleva MS_CONFIRMAR_LIBRE ms libre (en al menos 2 cuadros)? */
static int libre_confirmado(const Navegador *nav, uint32_t ahora_ms)
{
    return nav->frames_libre >= 2 && (ahora_ms - nav->t_libre_ms) >= (uint32_t)MS_CONFIRMAR_LIBRE;
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

/* DETENIDO_OBSTACULO: STOP. Si el frente se libera (varios cuadros seguidos), sigue. Si no,
 * tras una pausa elige un lado libre para esquivar; si no hay ninguno, espera hasta el
 * tiempo máximo y entonces ERROR. */
static ComandoNav tick_detenido(Navegador *nav, uint32_t ahora_ms)
{
    uint32_t espera = ahora_ms - nav->t_detenido_ms;

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
        nav->lado = 0;
        if (!nav->zona_hay[ZONA_IZQUIERDA]) {
            nav->lado = -1;   /* izquierda libre (se prefiere sobre la derecha) */
        } else if (!nav->zona_hay[ZONA_DERECHA]) {
            nav->lado = 1;
        }
        if (nav->lado != 0) {
            if (nav->esquivadas >= NAV_MAX_ESQUIVADAS) {
                ir_a_error(nav, NAV_MOTIVO_MAX_ESQUIVADAS);
                return NAV_CMD_STOP;
            }
            nav->esquivadas++;
            nav->fase = 0;
            nav->ms_fase = 0;
            nav->estado = NAV_ESQUIVANDO;
            return comando_de_fase(nav);
        }
        if (espera >= NAV_TIMEOUT_ESQUIVAR_MS) {
            ir_a_error(nav, NAV_MOTIVO_LADOS_BLOQUEADOS);
        }
    }
    return NAV_CMD_STOP;
}

/* ESQUIVANDO: gira hacia el lado libre, avanza un tramo fijo y gira de vuelta. Mientras
 * avanza de lado, si aparece algo MUY cerca al frente, ERROR (no se esquiva en cadena). */
static ComandoNav tick_esquivando(Navegador *nav, uint32_t ahora_ms)
{
    int fase_lista;

    if ((nav->fase == 0 || nav->fase == 2) && nav->cmd_actual == comando_de_fase(nav)) {
        fase_lista = giro_terminado(nav, NAV_ANGULO_ESQUIVAR, duracion_fase(nav, nav->fase), nav->ms_fase);   /* un giro: con la cámara si se puede */
    } else {
        fase_lista = (nav->ms_fase + NAV_TICK_MS / 2 >= duracion_fase(nav, nav->fase));
    }
    if (fase_lista) {
        nav->fase++;
        nav->ms_fase = 0;
        if (nav->fase > 2) {
            nav->estado = NAV_REINCORPORANDO;
            return NAV_CMD_STOP;
        }
    }
    if (nav->fase == 1 && nav->zona_hay[ZONA_CENTRO] && nav->zona_cercania[ZONA_CENTRO] >= 3 &&
        bloqueo_confirmado(nav, ahora_ms)) {
        ir_a_error(nav, NAV_MOTIVO_ESQUIVANDO);
        return NAV_CMD_STOP;
    }
    return comando_de_fase(nav);
}

/* REINCORPORANDO: con la posición estimada, calcula el camino de vuelta al siguiente punto de
 * la ruta original y pasa a SIGUIENDO. Dura un solo tick (con STOP). */
static ComandoNav tick_reincorporando(Navegador *nav)
{
    nav->frames_bloqueado = 0;   /* hacen falta cuadros nuevos para volver a detenerse */
    nav->bloqueo_inmediato = 0;
    nav->frames_libre = 0;
    if (planificar(nav) != NAV_OK) {
        ir_a_error(nav, NAV_MOTIVO_PLAN);
        return NAV_CMD_STOP;
    }
    nav->estado = NAV_SIGUIENDO;
    return NAV_CMD_STOP;
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

    if (obs != NULL) {
        registrar_cuadro(nav, ahora_ms, obs);
    }
    nav->seg_ms += (int)dt;           /* el comando anterior estuvo activo estos dt ms */
    calcular_pose(nav);
    contar_tiempo_ejecutado(nav, (int)dt);

    switch (nav->estado) {
    case NAV_SIGUIENDO:        comando = tick_siguiendo(nav, ahora_ms); break;
    case NAV_DETENIDO_OBSTACULO: comando = tick_detenido(nav, ahora_ms); break;
    case NAV_ESQUIVANDO:       comando = tick_esquivando(nav, ahora_ms); break;
    case NAV_REINCORPORANDO:   comando = tick_reincorporando(nav); break;
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
