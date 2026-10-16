/*
 * sim_ruta.c — Ver sim_ruta.h para la idea general.
 *
 * GUÍA DE LECTURA
 *  - sr_vista(): el «ojo» del carro virtual: de los obstáculos de la cuadrícula arma lo que vería el detector (zona, cercanía, celdas).
 *  - sr_simular(): un ciclo de 50 ms igual al de la app: ver (sr_vista) → navegador_tick → mover el carro virtual (simulador_navegador.c).
 *  - sr_dispersion(): repite sr_simular con ruidos de giro distintos y mide qué tan abierta queda la nube de puntos finales.
 *  - Coordenadas: milímetros; (0,0) es el centro de la celda (columna 0, fila 0); la fila crece hacia abajo; rumbo 0 = arriba, 90 = derecha.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "sim_ruta.h"
#include "simulador_navegador.h"

#define CELDA_MM        (CELDA_CM * 10)
#define MITAD_CELDA_MM  (CELDA_MM / 2)
#define TAN30_MIL       577      /* tangente de 30 grados x 1000: la mitad del campo de visión de la cámara */
#define VISTA_MAX_MM    800      /* más lejos que esto no se ve */
#define CERC3_MM        300      /* hasta esta distancia a la cara del obstáculo: muy cerca */
#define CERC2_MM        500      /* hasta esta: media */
#define SOLAPE_MIN_MM    80      /* cuánto de la celda (de 200 mm) debe caer dentro de una zona para marcarla */
#define DET_COLUMNAS     24      /* la cuadrícula del detector con la cámara del carro (240x176 con celdas de 10 px): sirve para decir cuántas celdas ocupa una caja */
#define DET_FILAS        17
#define ALTO_CAJA_MM    200      /* la caja virtual mide 20 cm de alto */

/* seno de 0 a 90 grados, de 5 en 5, por 1000 */
static const int TABLA_SENO[19] = {0, 87, 174, 259, 342, 423, 500, 574, 643, 707, 766, 819, 866, 906, 940, 966, 985, 996, 1000};

static int seno_mil(int grados)
{
    int g = ((grados % 360) + 360) % 360;
    int signo = 1;
    int i, resto;

    if (g > 180) {
        g -= 180;
        signo = -1;
    }
    if (g > 90) {
        g = 180 - g;
    }
    i = g / 5;
    resto = g % 5;
    if (i >= 18) {
        return signo * 1000;
    }
    return signo * (TABLA_SENO[i] + (TABLA_SENO[i + 1] - TABLA_SENO[i]) * resto / 5);
}

static int coseno_mil(int grados)
{
    return seno_mil(grados + 90);
}

static int raiz_entera(long v)
{
    long x = 0;

    if (v <= 0) {
        return 0;
    }
    while ((x + 1) * (x + 1) <= v) {
        x += (x < 1000) ? 1 : 50;   /* subida rápida: la precisión que importa es de milímetros */
        if (x > 100000) {
            break;
        }
    }
    while (x * x > v) {
        x--;
    }
    return (int)x;
}

static int minimo(int a, int b)
{
    return a < b ? a : b;
}

static int maximo(int a, int b)
{
    return a > b ? a : b;
}

/* ── El ojo del carro virtual ──────────────────────────────────────────── */

void sr_vista(int x_mm, int y_mm, int rumbo_grados, const Punto *obstaculos, int n_obstaculos, int hay[3], int cercania[3], int celdas[3])
{
    int i, z;
    int s = seno_mil(rumbo_grados);
    int c = coseno_mil(rumbo_grados);

    for (z = 0; z < 3; z++) {
        hay[z] = 0;
        cercania[z] = 0;
        celdas[z] = 0;
    }
    for (i = 0; i < n_obstaculos; i++) {
        long dx = (long)obstaculos[i].columna * CELDA_MM - x_mm;
        long dy = (long)obstaculos[i].fila * CELDA_MM - y_mm;
        int adelante = (int)((dx * s - dy * c) / 1000);   /* mm hacia donde mira el carro */
        int lado = (int)((dx * c + dy * s) / 1000);       /* mm a la derecha del carro */
        int cara = adelante - MITAD_CELDA_MM;              /* distancia a la cara cercana */
        int borde = maximo(adelante, MITAD_CELDA_MM) * TAN30_MIL / 1000 / 3;   /* un tercio de la mitad del campo: límite entre zonas */
        int izq = lado - MITAD_CELDA_MM;
        int der = lado + MITAD_CELDA_MM;
        int cerc;
        int solape[3];

        int mitad_campo = maximo(adelante, MITAD_CELDA_MM) * TAN30_MIL / 1000;   /* mitad del ancho que ve la cámara a esa distancia */

        if (adelante < -MITAD_CELDA_MM / 2 || cara >= VISTA_MAX_MM) {
            continue;   /* detrás o demasiado lejos */
        }
        if (izq > mitad_campo || der < -mitad_campo) {
            continue;   /* fuera del campo de visión: ni una parte entra en el cono */
        }
        cerc = (cara <= CERC3_MM) ? 3 : (cara <= CERC2_MM) ? 2 : 1;
        /* cuánto de la celda cae en cada zona: izquierda (hasta -borde), centro y derecha (desde +borde) */
        solape[0] = maximo(0, minimo(der, -borde) - izq);
        solape[1] = maximo(0, minimo(der, borde) - maximo(izq, -borde));
        solape[2] = maximo(0, der - maximo(izq, borde));
        for (z = 0; z < 3; z++) {
            if (solape[z] >= SOLAPE_MIN_MM) {   /* un pedazo chico de la celda no alcanza para marcar la zona (como MIN_CELDAS del detector) */
                /* Cuántas celdas del detector ocuparía la caja: columnas = lo que cae en la zona respecto del ancho que ve la cámara a esa distancia; filas = su alto
                 * (20 cm) respecto del alto que ve la cámara a esa distancia. Una caja de 20 cm a 50 cm ocupa unas 50 celdas; lejos, menos. */
                int distancia = maximo(adelante, MITAD_CELDA_MM);
                int columnas = maximo(1, solape[z] * DET_COLUMNAS / (2 * maximo(1, mitad_campo)));
                int filas = minimo(DET_FILAS, maximo(1, ALTO_CAJA_MM * DET_FILAS * 10 / (7 * distancia)));
                int n = minimo(99, columnas * filas);
                hay[z] = 1;
                cercania[z] = maximo(cercania[z], cerc);
                celdas[z] = maximo(celdas[z], n);
            }
        }
    }
}

/* ── La simulación ─────────────────────────────────────────────────────── */

/* Un número «al azar» entre -1000 y 1000 que depende solo de la semilla (siempre el mismo para la misma semilla). */
static int azar_mil(uint32_t semilla)
{
    uint32_t s = semilla * 1103515245u + 12345u;

    s = s * 1103515245u + 12345u;
    return (int)((s >> 16) & 0x7FFF) * 2000 / 32767 - 1000;
}

/* La simulación usa memoria estática (cabe en la pila del hilo principal de la app, pero no se repite en otros hilos): se llama siempre desde el mismo hilo. */
static Navegador nav_sim;
static Simulador car_sim;
static int desbalance_global_mil = 1000;   /* ver sr_poner_balance */
static int balance_global_mil = 1000;

void sr_poner_balance(int desbalance_mil, int balance_mil)
{
    if (desbalance_mil >= 500 && desbalance_mil <= 2000 && balance_mil >= 500 && balance_mil <= 2000) {
        desbalance_global_mil = desbalance_mil;
        balance_global_mil = balance_mil;
    }
}

int sr_simular(const Punto *ruta, int n_ruta, const CalibracionCarro *cal, const Punto *obstaculos, int n_obstaculos, int ruido_giro_grados, uint32_t semilla, ResultadoSimRuta *r)
{
    CalibracionCarro calibracion;
    uint32_t t;
    int anterior = NAV_CMD_STOP;
    int hay_cambio_inicial = 1;
    int n_obs = n_obstaculos;

    if (ruta == NULL || n_ruta < 2 || r == NULL || (obstaculos == NULL && n_obstaculos > 0)) {
        return SR_ERR_ARGUMENTO;
    }
    memset(r, 0, sizeof *r);
    if (n_obs > SR_MAX_OBSTACULOS) {
        n_obs = SR_MAX_OBSTACULOS;
    }
    if (cal == NULL || !cal_es_valida(cal)) {
        cal_defecto(&calibracion);
        r->advertencias |= SR_ADV_CALIBRACION;
    } else {
        calibracion = *cal;
    }
    navegador_iniciar(&nav_sim);
    navegador_poner_calibracion(&nav_sim, &calibracion);
    if (navegador_cargar_ruta(&nav_sim, ruta, n_ruta) != NAV_OK) {
        r->codigo = SR_ERR_RUTA;
        r->codigo_ruta = nav_sim.codigo_trayectoria;
        return SR_ERR_RUTA;
    }
    sim_iniciar(&car_sim, ruta[0].columna, ruta[0].fila, 0);
    car_sim.real = calibracion;
    car_sim.desbalance_mil = desbalance_global_mil;
    car_sim.balance_mil = balance_global_mil;
    car_sim.periodo_cuadro_ms = NAV_TICK_MS;
    if (semilla != 0 && ruido_giro_grados > 0) {   /* un giro de 90 grados sale 90 + d: d entre -ruido y +ruido */
        int d_mil = azar_mil(semilla) * ruido_giro_grados;   /* grados x 1000 */

        car_sim.pct_giro = 100 + d_mil * 100 / (90 * 1000);
    }
    navegador_comenzar(&nav_sim, 0);

    for (t = 0; t < SR_DURACION_MAX_MS; t += NAV_TICK_MS) {
        int hay[3], cerc[3], celdas[3];
        int z, comando;
        int x = sim_x_mm(&car_sim);
        int y = sim_y_mm(&car_sim);

        sr_vista(x, y, sim_rumbo_grados(&car_sim), obstaculos, n_obs, hay, cerc, celdas);
        car_sim.n_eventos = 0;
        for (z = 0; z < 3; z++) {
            if (hay[z]) {
                sim_agregar_obstaculo_celdas(&car_sim, t, t + NAV_TICK_MS, z, cerc[z], celdas[z]);
            }
        }
        comando = (int)sim_correr(&car_sim, &nav_sim, NAV_TICK_MS);
        x = sim_x_mm(&car_sim);
        y = sim_y_mm(&car_sim);
        if ((hay_cambio_inicial || comando != anterior) && r->n_cambios < SR_MAX_CAMBIOS) {
            r->cambios[r->n_cambios].t_ms = t;
            r->cambios[r->n_cambios].comando = comando;
            r->n_cambios++;
            if (!hay_cambio_inicial) {
                r->n_comandos++;
            }
        }
        hay_cambio_inicial = 0;
        anterior = comando;
        if (t % SR_MUESTRA_CADA_MS == 0 && r->n_muestras < SR_MAX_MUESTRAS) {
            MuestraSim *m = &r->muestras[r->n_muestras++];

            m->t_ms = t;
            m->x_mm = x;
            m->y_mm = y;
            m->rumbo_grados = sim_rumbo_grados(&car_sim);
            m->comando = comando;
            m->estado = (int)nav_sim.estado;
        }
        if (x < -MITAD_CELDA_MM || x > (RE_COLUMNAS - 1) * CELDA_MM + MITAD_CELDA_MM || y < -MITAD_CELDA_MM || y > (RE_FILAS - 1) * CELDA_MM + MITAD_CELDA_MM) {
            r->advertencias |= SR_ADV_SALE_CUADRICULA;
        }
        if (nav_sim.estado == NAV_TERMINADO || nav_sim.estado == NAV_ERROR) {
            t += NAV_TICK_MS;
            break;
        }
    }
    r->duracion_ms = t;
    r->estado_final = (int)nav_sim.estado;
    r->motivo_error = (int)nav_sim.motivo_error;
    r->error_causa = nav_sim.error_causa;
    r->llego = nav_sim.estado == NAV_TERMINADO;
    r->esquivadas = nav_sim.esquivadas;
    r->x_final_mm = sim_x_mm(&car_sim);
    r->y_final_mm = sim_y_mm(&car_sim);
    {
        long dx = (long)r->x_final_mm - (long)ruta[n_ruta - 1].columna * CELDA_MM;
        long dy = (long)r->y_final_mm - (long)ruta[n_ruta - 1].fila * CELDA_MM;

        r->error_final_mm = raiz_entera(dx * dx + dy * dy);
    }
    if (!r->llego) {
        r->advertencias |= SR_ADV_NO_LLEGO;
    }
    if (t >= SR_DURACION_MAX_MS) {
        r->advertencias |= SR_ADV_CORTADA;
    }
    return SR_OK;
}

static ResultadoSimRuta resultado_dispersion;

int sr_dispersion(const Punto *ruta, int n_ruta, const CalibracionCarro *cal, const Punto *obstaculos, int n_obstaculos, int ruido_giro_grados, int corridas, DispersionSim *d)
{
    int k, i, j;
    long suma_error = 0;
    long mayor = 0;

    if (d == NULL) {
        return SR_ERR_ARGUMENTO;
    }
    memset(d, 0, sizeof *d);
    if (corridas < 1) {
        return SR_ERR_ARGUMENTO;
    }
    if (corridas > SR_MAX_CORRIDAS) {
        corridas = SR_MAX_CORRIDAS;
    }
    for (k = 0; k < corridas; k++) {
        int codigo = sr_simular(ruta, n_ruta, cal, obstaculos, n_obstaculos, ruido_giro_grados, (uint32_t)(k + 1), &resultado_dispersion);

        if (codigo != SR_OK) {
            return codigo;
        }
        d->corridas[k].llego = resultado_dispersion.llego;
        d->corridas[k].x_final_mm = resultado_dispersion.x_final_mm;
        d->corridas[k].y_final_mm = resultado_dispersion.y_final_mm;
        d->corridas[k].error_final_mm = resultado_dispersion.error_final_mm;
        d->corridas[k].duracion_ms = resultado_dispersion.duracion_ms;
        d->llegaron += resultado_dispersion.llego;
        suma_error += resultado_dispersion.error_final_mm;
        if (resultado_dispersion.error_final_mm > d->error_max_mm) {
            d->error_max_mm = resultado_dispersion.error_final_mm;
        }
    }
    d->n = corridas;
    d->error_medio_mm = (int)(suma_error / corridas);
    for (i = 0; i < corridas; i++) {
        for (j = i + 1; j < corridas; j++) {
            long dx = (long)d->corridas[i].x_final_mm - d->corridas[j].x_final_mm;
            long dy = (long)d->corridas[i].y_final_mm - d->corridas[j].y_final_mm;

            if (dx * dx + dy * dy > mayor) {
                mayor = dx * dx + dy * dy;
            }
        }
    }
    d->dispersion_mm = raiz_entera(mayor);
    return SR_OK;
}

int sr_texto_advertencia(int bit, char *texto, int capacidad)
{
    const char *frase;
    int i = 0;

    switch (bit) {
    case SR_ADV_SALE_CUADRICULA: frase = "El carro se sale de la cuadrícula: deja más espacio en los bordes."; break;
    case SR_ADV_NO_LLEGO:        frase = "El carro no llegó al final de la ruta."; break;
    case SR_ADV_CORTADA:         frase = "La simulación se cortó por tardar demasiado."; break;
    case SR_ADV_CALIBRACION:     frase = "Sin calibración guardada: se usaron valores de ejemplo."; break;
    default: return 0;
    }
    if (texto == NULL || capacidad < 2) {
        return 0;
    }
    while (frase[i] != '\0' && i < capacidad - 1) {
        texto[i] = frase[i];
        i++;
    }
    texto[i] = '\0';
    return i;
}

/* Escribe `frase` en `texto` (cortándola si no cabe) y devuelve el largo. */
static int copiar_frase(const char *frase, char *texto, int capacidad)
{
    int i = 0;

    if (texto == NULL || capacidad < 2) {
        return 0;
    }
    while (frase[i] != '\0' && i < capacidad - 1) {
        texto[i] = frase[i];
        i++;
    }
    texto[i] = '\0';
    return i;
}

int sr_texto_causa(const ResultadoSimRuta *r, char *texto, int capacidad)
{
    char numero[96];

    if (r == NULL || texto == NULL || capacidad < 2) {
        return 0;
    }
    texto[0] = '\0';
    if (r->llego) {
        return 0;
    }
    switch (r->error_causa) {
    case NAV_CAUSA_LADOS:
        return copiar_frase("No había por dónde esquivar: los dos lados estaban ocupados.", texto, capacidad);
    case NAV_CAUSA_MAX_ESQUIVADAS:
        snprintf(numero, sizeof numero, "El obstáculo bloqueó el camino y se agotaron las %d esquivadas.", NAV_MAX_ESQUIVADAS);
        return copiar_frase(numero, texto, capacidad);
    case NAV_CAUSA_REINTENTOS:
        snprintf(numero, sizeof numero, "El obstáculo bloqueó el camino y se agotaron los %d reintentos.", NAV_MAX_REINTENTOS);
        return copiar_frase(numero, texto, capacidad);
    case NAV_CAUSA_PLAN:
        return copiar_frase("No encontró cómo volver a la ruta.", texto, capacidad);
    default:
        break;
    }
    if ((r->advertencias & SR_ADV_CORTADA) != 0) {
        return copiar_frase("La simulación se cortó por tardar demasiado.", texto, capacidad);
    }
    return 0;
}
