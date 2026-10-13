/*
 * navegador.c — Máquina de estados del modo semiautónomo (ver navegador.h y docs/navegador.md).
 *
 * GUÍA DE LECTURA
 *  1. Lo que se ve primero son las ayudas de posición: el carro no tiene sensores de
 *     posición, así que se calcula por TIEMPO (calcular_pose): cuánto tiempo estuvo
 *     avanzando o girando, convertido con MS_POR_CM y MS_POR_GRADO.
 *  2. planificar() usa trayectoria.c para sacar los pasos (AVANZAR/GIRAR con su tiempo) desde
 *     la posición y el rumbo actuales hasta el siguiente punto de la ruta.
 *  3. EL TEMPORIZADOR DE TRAMOS está en contar_tiempo_ejecutado() y en tick_siguiendo() /
 *     tick_esquivando(): un paso termina cuando su tiempo ya se ejecutó. Todo lo que dependa
 *     de "cuánto dura cada tramo" está aquí; si algún día el firmware acepta comandos con
 *     duración (por ejemplo MOVE:FWD:2000), solo hay que cambiar esa parte.
 *  4. navegador_tick() es la función que se llama cada ~50 ms: actualiza la posición, mira el
 *     último cuadro de obstáculos y llama a la función del estado actual.
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
        distancia = nav->seg_ms * 10 / MS_POR_CM;   /* milímetros recorridos */
        if (nav->cmd_actual == NAV_CMD_REVERSE) {
            distancia = -distancia;
        }
        direccion = indice_direccion(nav->base_rumbo_grados);
        nav->x_mm = nav->base_x_mm + DIR_X[direccion] * distancia / 1000;
        nav->y_mm = nav->base_y_mm + DIR_Y[direccion] * distancia / 1000;
        break;
    case NAV_CMD_LEFT:
        nav->rumbo_grados = normalizar_grados(nav->base_rumbo_grados - nav->seg_ms / MS_POR_GRADO);
        break;
    case NAV_CMD_RIGHT:
        nav->rumbo_grados = normalizar_grados(nav->base_rumbo_grados + nav->seg_ms / MS_POR_GRADO);
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

/* Cambia el comando actual (si es distinto) cerrando el tramo anterior. */
static void cambiar_comando(Navegador *nav, ComandoNav nuevo)
{
    if (nuevo != nav->cmd_actual) {
        fijar_pose(nav);
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
static int duracion_fase(int fase)
{
    if (fase == 1) {
        return NAV_TRAMO_ESQUIVAR_CM * MS_POR_CM;
    }
    return NAV_ANGULO_ESQUIVAR * MS_POR_GRADO;
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

    nav->codigo_trayectoria = trayectoria_calcular_desde(orientacion, camino, n,
                                                          nav->pasos, MAX_PASOS, &nav->n_pasos, &indice_error);
    nav->indice_paso = 0;
    nav->ms_paso = 0;
    if (nav->codigo_trayectoria != TRAY_OK) {
        nav->n_pasos = 0;
        return NAV_ERR_RUTA;
    }
    return NAV_OK;
}

/* ── Configuración ─────────────────────────────────────────────────────── */

void navegador_iniciar(Navegador *nav)
{
    int z;

    if (nav == NULL) {
        return;
    }
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
    nav->x_mm = nav->y_mm = nav->rumbo_grados = 0;
    nav->base_x_mm = nav->base_y_mm = nav->base_rumbo_grados = 0;
    nav->seg_ms = 0;
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

/* ¿Este cuadro, por sí solo, ya justifica detenerse? Sí si el obstáculo está muy cerca (3), o
 * a media distancia (2) en la zona central. */
static int cuadro_es_inmediato(int zona, int cercania)
{
    return cercania >= 3 || (cercania == 2 && zona == ZONA_CENTRO);
}

/* Guarda lo que se vio en el último cuadro y lleva la cuenta de la RACHA actual: cuántos cuadros
 * seguidos (y desde qué hora, en ms) el frente está bloqueado o libre. Un cuadro de ruido suelto
 * no cambia nada: ver bloqueo_confirmado() y libre_confirmado(). */
static void registrar_cuadro(Navegador *nav, uint32_t ahora_ms, const ResultadoObstaculos *obs)
{
    int z, bloqueado;

    for (z = 0; z < OBS_ZONAS; z++) {
        nav->zona_hay[z] = obs->zona[z].hay_obstaculo;
        nav->zona_cercania[z] = obs->zona[z].cercania;
    }
    bloqueado = nav->zona_hay[ZONA_CENTRO] && nav->zona_cercania[ZONA_CENTRO] >= NAV_CERCANIA_MIN_DETENER;
    if (bloqueado) {
        if (nav->frames_bloqueado == 0) {      /* empieza una racha con obstáculo */
            nav->t_bloqueo_ms = ahora_ms;
            nav->bloqueo_inmediato = 0;
        }
        if (cuadro_es_inmediato(ZONA_CENTRO, nav->zona_cercania[ZONA_CENTRO])) {
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
        /* El paso termina cuando su tiempo ya se ejecutó (redondeado a medio tick). */
        if (nav->ms_paso + NAV_TICK_MS / 2 >= nav->pasos[nav->indice_paso].tiempo_ms) {
            nav->indice_paso++;
            nav->ms_paso = 0;
            continue;
        }
        comando = comando_de_paso(&nav->pasos[nav->indice_paso]);
        break;
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
    if (nav->ms_fase + NAV_TICK_MS / 2 >= duracion_fase(nav->fase)) {
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
