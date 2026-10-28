/*
 * simulador_navegador.c — Carro y cámara simulados para probar navegador.c (ver el .h).
 *
 * GUÍA DE LECTURA
 *  1. mover_carro(): aplica un comando durante dt ms (avanzar, retroceder o girar) con la
 *     calibración de calibracion.h, corregida por pct_avance / pct_giro. El rumbo del carro
 *     simulado es continuo (cualquier grado, con una tabla de senos): así un giro corto o largo
 *     de verdad desvía el camino, cosa que el navegador no puede saber.
 *  2. armar_cuadro(): construye el ResultadoObstaculos según los eventos programados.
 *  3. sim_correr(): el ciclo principal. Cada NAV_TICK_MS: arma el cuadro (si toca uno
 *     nuevo), llama a navegador_tick, anota el cambio de comando y mueve el carro.
 *  Es solo para pruebas en PC; no se usa en el firmware ni en la app.
 */

#include <string.h>
#include "simulador_navegador.h"
#include "calibracion.h"

/* Seno de 0 a 90 grados, en milésimas (1000 = 1.0). Tabla fija: no se usa math.h. */
static const int SENO_MILESIMAS[91] = {
       0,   17,   35,   52,   70,   87,  105,  122,  139,  156,  174,  191,  208,  225,  242,
     259,  276,  292,  309,  326,  342,  358,  375,  391,  407,  423,  438,  454,  469,  485,
     500,  515,  530,  545,  559,  574,  588,  602,  616,  629,  643,  656,  669,  682,  695,
     707,  719,  731,  743,  755,  766,  777,  788,  799,  809,  819,  829,  839,  848,  857,
     866,  875,  883,  891,  899,  906,  914,  921,  927,  934,  940,  946,  951,  956,  961,
     966,  970,  974,  978,  982,  985,  988,  990,  993,  995,  996,  998,  999,  999, 1000,
    1000
};

/* Seno (en milésimas) de cualquier ángulo entero en grados. */
static int seno(int grados)
{
    int g = ((grados % 360) + 360) % 360;

    if (g <= 90) {
        return SENO_MILESIMAS[g];
    }
    if (g <= 180) {
        return SENO_MILESIMAS[180 - g];
    }
    if (g <= 270) {
        return -SENO_MILESIMAS[g - 180];
    }
    return -SENO_MILESIMAS[360 - g];
}

/* Coseno = seno desplazado 90 grados. */
static int coseno(int grados)
{
    return seno(grados + 90);
}

void sim_iniciar(Simulador *sim, int columna, int fila, int rumbo_grados)
{
    memset(sim, 0, sizeof *sim);
    sim->x_cmm = (long)columna * CELDA_CM * 1000;
    sim->y_cmm = (long)fila * CELDA_CM * 1000;
    sim->rumbo_cgrados = (long)rumbo_grados * 100;
    sim->rumbo0_cgrados = sim->rumbo_cgrados;
    sim->fw_total_pct = 100;
    sim->pct_avance = 100;
    sim->pct_giro = 100;
    cal_defecto(&sim->real);
    sim->visual_activo = 0;
    sim->visual_error_pct = 0;
    sim->visual_latencia_ms = 0;
    sim->n_pendientes = 0;
    sim->giro_desde_cuadro_cgrados = 0;
    sim->t_cuadro_visual_ms = 0;
    sim->cmd_previo = NAV_CMD_STOP;
    sim->ms_en_comando = 0;
    sim->desbalance_mil = 1000;
    sim->balance_mil = 1000;
    sim->periodo_cuadro_ms = NAV_TICK_MS;
    sim->t_cancelar_ms = 0xFFFFFFFFu;
    sim->recorrido_cmm = 0;
    sim->fw_anticipacion_ms[0] = sim->fw_anticipacion_ms[1] = 20;   /* GIRO_ANTICIPACION_MS de config.h */
    sim->fw_falla_codigo = NAV_GIRO_ERROR_SIN_DATOS;
}

int sim_agregar_obstaculo_celdas(Simulador *sim, uint32_t t_ini_ms, uint32_t t_fin_ms, int zona, int cercania, int celdas)
{
    EventoObstaculo *e;

    if (sim->n_eventos >= SIM_MAX_EVENTOS) {
        return -1;
    }
    e = &sim->eventos[sim->n_eventos++];
    e->t_ini_ms = t_ini_ms;
    e->t_fin_ms = t_fin_ms;
    e->zona = zona;
    e->cercania = cercania;
    e->celdas = celdas;
    e->crece_por_10cm = 0;
    e->base_recorrido_cmm = 0;
    e->visto = 0;
    e->laser_mm = 0;
    return 0;
}

int sim_agregar_obstaculo_laser(Simulador *sim, uint32_t t_ini_ms, uint32_t t_fin_ms, int zona, int cercania, int celdas, int laser_mm)
{
    int codigo = sim_agregar_obstaculo_celdas(sim, t_ini_ms, t_fin_ms, zona, cercania, celdas);

    if (codigo == 0) {
        sim->eventos[sim->n_eventos - 1].laser_mm = laser_mm;
    }
    return codigo;
}

int sim_agregar_obstaculo_creciente(Simulador *sim, uint32_t t_ini_ms, uint32_t t_fin_ms, int zona, int cercania, int celdas, int crece_por_10cm)
{
    int codigo = sim_agregar_obstaculo_celdas(sim, t_ini_ms, t_fin_ms, zona, cercania, celdas);

    if (codigo == 0) {
        sim->eventos[sim->n_eventos - 1].crece_por_10cm = crece_por_10cm;
    }
    return codigo;
}

int sim_agregar_obstaculo(Simulador *sim, uint32_t t_ini_ms, uint32_t t_fin_ms, int zona, int cercania)
{
    return sim_agregar_obstaculo_celdas(sim, t_ini_ms, t_fin_ms, zona, cercania, NAV_CERC3_AREA_INMEDIATA);   /* área grande por defecto: con cercanía 3 un solo cuadro frena (la regla de J9); para probar la de 2 cuadros se da un área chica con sim_agregar_obstaculo_celdas */
}

/* Rumbo en grados enteros, de 0 a 359. */
static int rumbo_entero(const Simulador *sim)
{
    long grados = (sim->rumbo_cgrados / 100) % 360;

    return (int)((grados + 360) % 360);
}

/* Al girar `giro_cgrados` más (centésimas de grado, positivas) hacia `lado`, el centro del carro REAL también se mueve. Se usa el MISMO modelo que el navegador (el modelo de pivote de
 * sim->real; ver cal_desplazamiento_giro): el simulador lleva la cuenta del giro desde que empezó el comando (giro_acum_cgrados, con el rumbo del que partió) y en cada pedacito mueve lo
 * que falta para completar el desplazamiento que el modelo da para ese TOTAL; así, al terminar un giro de θ grados, el centro se movió justo lo que dice el modelo para θ, sea cual sea
 * el ángulo (no es lineal). Entre dos grados enteros interpola. Se llama ANTES de sumar el pedacito al rumbo. */
static void desplazar_al_girar(Simulador *sim, int lado, long giro_cgrados)
{
    int grados, rumbo_ini, dx0 = 0, dy0 = 0, dx1 = 0, dy1 = 0;
    long fraccion, x, y;   /* x e y en centésimas de mm */

    if (giro_cgrados <= 0) {
        return;
    }
    sim->giro_acum_cgrados += giro_cgrados;
    rumbo_ini = (int)((((sim->giro_h0_cgrados / 100) % 360) + 360) % 360);
    grados = (int)(sim->giro_acum_cgrados / 100);
    fraccion = sim->giro_acum_cgrados % 100;
    cal_desplazamiento_giro(&sim->real, lado, grados, rumbo_ini, &dx0, &dy0, NULL, NULL);
    cal_desplazamiento_giro(&sim->real, lado, grados + 1, rumbo_ini, &dx1, &dy1, NULL, NULL);
    x = (long)dx0 * (100 - fraccion) + (long)dx1 * fraccion;
    y = (long)dy0 * (100 - fraccion) + (long)dy1 * fraccion;
    sim->x_cmm += x - sim->desp_x_cmm;
    sim->y_cmm += y - sim->desp_y_cmm;
    sim->desp_x_cmm = x;
    sim->desp_y_cmm = y;
}

/* Mueve el carro: el comando estuvo activo `dt` ms EFECTIVOS (ya sin el arranque). */
static void mover_carro(Simulador *sim, ComandoNav comando, int dt)
{
    int rumbo = rumbo_entero(sim);
    long distancia;   /* centésimas de milímetro */
    long giro;        /* centésimas de grado */

    switch (comando) {
    case NAV_CMD_FORWARD:
    case NAV_CMD_REVERSE:
        /* 1 mm = 100 centésimas; hacia atrás el carro real puede tener otros ms por cm (cal_ms_por_cm_atras_x10) */
        distancia = (long)dt * 10000 / ((comando == NAV_CMD_REVERSE) ? cal_ms_por_cm_atras_x10(&sim->real) : sim->real.ms_por_cm_x10) * sim->pct_avance / 100;
        if (comando == NAV_CMD_REVERSE) {
            distancia = -distancia;
        }
        /* rumbo 0 = arriba (y decrece), 90 = derecha (x crece) */
        sim->x_cmm += seno(rumbo) * distancia / 1000;
        sim->y_cmm -= coseno(rumbo) * distancia / 1000;
        if (distancia > 0) {
            sim->recorrido_cmm += distancia;
        }
        {   /* Si un lado empuja más que el otro el carro se tuerce: el giro es la diferencia de lo que recorre cada lado entre el ancho entre ruedas. Hacia atrás se tuerce al revés. */
            long long razon_mil = (long long)sim->desbalance_mil * sim->balance_mil / 1000;   /* velocidad izquierda / derecha, en milésimas */
            long long giro_cgrados = (long long)distancia * 2 * (razon_mil - 1000) * 4775 / ((razon_mil + 1000) * 10000);   /* 4775 = 57.3 grados por radián / 120 mm de ejes, por 10 000 */

            sim->rumbo_cgrados += (long)giro_cgrados;
        }
        break;
    case NAV_CMD_LEFT:
    case NAV_CMD_RIGHT:
        giro = (long)dt * 1000 / ((comando == NAV_CMD_LEFT) ? sim->real.ms_por_grado_izq_x10 : sim->real.ms_por_grado_der_x10)
               * sim->pct_giro / 100;
        desplazar_al_girar(sim, (comando == NAV_CMD_RIGHT) ? CAL_LADO_DERECHA : CAL_LADO_IZQUIERDA, giro);
        sim->rumbo_cgrados += (comando == NAV_CMD_RIGHT) ? giro : -giro;
        sim->giro_desde_cuadro_cgrados += (comando == NAV_CMD_RIGHT) ? giro : -giro;   /* lo que vería la cámara */
        break;
    default:
        break;
    }
}

/* Arma lo que vería el detector de obstáculos en la hora `t_ms`. */
static void armar_cuadro(Simulador *sim, uint32_t t_ms, ResultadoObstaculos *cuadro)
{
    int i;

    memset(cuadro, 0, sizeof *cuadro);
    for (i = 0; i < sim->n_eventos; i++) {
        EventoObstaculo *e = &sim->eventos[i];

        if (t_ms >= e->t_ini_ms && t_ms < e->t_fin_ms && e->cercania > 0) {   /* cercanía 0 = lo ve solo el láser (la cámara no) */
            int celdas = e->celdas;

            if (!e->visto) {
                e->visto = 1;
                e->base_recorrido_cmm = sim->recorrido_cmm;
                e->base_rumbo_cgrados = sim->rumbo_cgrados;
            }
            celdas += (int)((sim->recorrido_cmm - e->base_recorrido_cmm) * e->crece_por_10cm / 10000);   /* un obstáculo real crece al acercarse */
            cuadro->zona[e->zona].hay_obstaculo = 1;
            if (celdas > cuadro->zona[e->zona].celdas) {
                cuadro->zona[e->zona].celdas = celdas;
            }
            if (e->cercania > cuadro->zona[e->zona].cercania) {
                cuadro->zona[e->zona].cercania = e->cercania;
            }
        }
    }
}

/* Al soltar un comando de movimiento el carro sigue un poco (el frenado del carro real). */
static void deslizar_al_frenar(Simulador *sim, ComandoNav comando)
{
    int rumbo = rumbo_entero(sim);
    long distancia;

    switch (comando) {
    case NAV_CMD_FORWARD:
    case NAV_CMD_REVERSE:
        distancia = (long)sim->real.frenado_mm * 100 * sim->pct_avance / 100;
        if (comando == NAV_CMD_REVERSE) {
            distancia = -distancia;
        }
        sim->x_cmm += seno(rumbo) * distancia / 1000;
        sim->y_cmm -= coseno(rumbo) * distancia / 1000;
        break;
    case NAV_CMD_LEFT:
        desplazar_al_girar(sim, CAL_LADO_IZQUIERDA, (long)sim->real.frenado_grados_izq_x10 * 10 * sim->pct_giro / 100);
        sim->rumbo_cgrados -= (long)sim->real.frenado_grados_izq_x10 * 10 * sim->pct_giro / 100;
        sim->giro_desde_cuadro_cgrados -= (long)sim->real.frenado_grados_izq_x10 * 10 * sim->pct_giro / 100;
        break;
    case NAV_CMD_RIGHT:
        desplazar_al_girar(sim, CAL_LADO_DERECHA, (long)sim->real.frenado_grados_der_x10 * 10 * sim->pct_giro / 100);
        sim->rumbo_cgrados += (long)sim->real.frenado_grados_der_x10 * 10 * sim->pct_giro / 100;
        sim->giro_desde_cuadro_cgrados += (long)sim->real.frenado_grados_der_x10 * 10 * sim->pct_giro / 100;
        break;
    default:
        break;
    }
}

/* ── Firmware simulado: giro por ángulo y láser (J11) ─────────────────────────────────────────────── */

static unsigned azar_fw = 12345u;
static int azar_fw_entre(int minimo, int maximo)
{
    azar_fw = azar_fw * 1664525u + 1013904223u;
    return minimo + (int)((azar_fw >> 8) % (unsigned)(maximo - minimo + 1));
}

/* Arranca un giro del firmware: `grados_izq_positivo` es lo que pidió el navegador (GIRAR:<grados>). Calcula de una vez qué pasará: en cuántos ms frena el firmware con SU giroscopio, cuánto gira
 * el carro de verdad en total (con lo que sigue girando al frenar) y qué ángulo informará; el simulador lo va aplicando poco a poco en fw_avanzar. */
static void fw_iniciar(Simulador *sim, int grados_izq_positivo)
{
    int izquierda = grados_izq_positivo > 0;
    int lado = izquierda ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA;
    int magnitud = izquierda ? grados_izq_positivo : -grados_izq_positivo;
    int x10 = izquierda ? sim->real.ms_por_grado_izq_x10 : sim->real.ms_por_grado_der_x10;
    long mdeg_por_ms = 10000L / x10 * sim->pct_giro / 100;   /* velocidad REAL del giro: milésimas de grado por ms */
    long objetivo_mdeg = (long)magnitud * 1000;
    int max_ms = 500 + 15 * magnitud;
    int arranque = sim->real.arranque_ms;
    long coast_mdeg = (long)(izquierda ? sim->real.frenado_grados_izq_x10 : sim->real.frenado_grados_der_x10) * 100 * sim->pct_giro / 100;
    int ant = sim->fw_anticipacion_ms[izquierda ? 1 : 0];
    long real_mdeg = 0, medido_mdeg = 0;
    int t = 0, codigo = NAV_GIRO_OK;
    long total_mdeg, medido_final;

    sim->fw_n_giros++;
    sim->fw_lado = lado;
    sim->fw_ms = 0;
    sim->fw_aplicado_cgrados = 0;
    sim->fw_sin_motor = 0;
    if (sim->fw_falla_desde > 0 && sim->fw_n_giros >= sim->fw_falla_desde) {
        codigo = sim->fw_falla_codigo;
        if (codigo == NAV_GIRO_ERROR_SIN_DATOS) {
            sim->fw_sin_motor = 1;                       /* el firmware ni lo empezó: el carro no se mueve */
            sim->fw_total_cgrados = 0;
            sim->fw_medido_x10 = 0;
            sim->fw_manejo_ms = 0;
            sim->fw_dur_ms = NAV_TICK_MS;
            sim->fw_codigo = codigo;
            sim->fw_activo = 1;
            return;
        }
    }
    for (t = 1; t < max_ms; t++) {   /* el motor está encendido t ms */
        long paso = (t > arranque) ? mdeg_por_ms : 0;

        real_mdeg += paso;
        medido_mdeg += paso * (100 + sim->fw_escala_pct) / 100 + sim->fw_sesgo_x100 / 100;
        if (codigo != NAV_GIRO_OK && real_mdeg >= objetivo_mdeg / 2) {
            break;                                       /* falla a mitad del giro: se corta ahí e informa el ángulo parcial */
        }
        if (medido_mdeg + mdeg_por_ms * (100 + sim->fw_escala_pct) / 100 * ant >= objetivo_mdeg) {
            break;
        }
    }
    if (codigo == NAV_GIRO_OK && t >= max_ms) {
        codigo = NAV_GIRO_TIMEOUT;
    }
    total_mdeg = real_mdeg + ((codigo == NAV_GIRO_OK || codigo == NAV_GIRO_TIMEOUT) ? coast_mdeg : 0);
    medido_final = medido_mdeg + ((codigo == NAV_GIRO_OK || codigo == NAV_GIRO_TIMEOUT) ? coast_mdeg * (100 + sim->fw_escala_pct) / 100 : 0);
    if (codigo == NAV_GIRO_OK && mdeg_por_ms > 0) {   /* la anticipación se corrige sola (igual que giro.c) */
        long ideal = coast_mdeg / mdeg_por_ms;

        if (ideal > 150) {
            ideal = 150;
        }
        sim->fw_anticipacion_ms[izquierda ? 1 : 0] = ant + (int)((ideal - ant) / 2);
    }
    sim->fw_total_cgrados = total_mdeg / 10 * sim->fw_total_pct / 100;
    if (codigo == NAV_GIRO_OK) {
        int error = izquierda ? sim->fw_error_izq_x10 : sim->fw_error_der_x10;
        sim->fw_total_cgrados += error * 10;
        medido_final += error * 100;
    }
    sim->fw_manejo_ms = t;
    sim->fw_medido_x10 = (int)((medido_final + 50) / 100) + (sim->fw_ruido_x10 > 0 ? azar_fw_entre(-sim->fw_ruido_x10, sim->fw_ruido_x10) : 0);
    sim->fw_codigo = codigo;
    sim->fw_dur_ms = t + 200 + NAV_TICK_MS;           /* lo que gira + los 200 ms de asentamiento + el viaje de la respuesta */
    sim->fw_activo = 1;
}

/* Aplica al carro la parte del giro del firmware que toca en estos `dt` ms y, al terminar, contesta. */
static void fw_avanzar(Simulador *sim, Navegador *nav, int dt)
{
    long nuevo_total, paso;

    if (!sim->fw_activo) {
        return;
    }
    sim->fw_ms += dt;
    nuevo_total = (sim->fw_ms >= sim->fw_dur_ms) ? sim->fw_total_cgrados : sim->fw_total_cgrados * sim->fw_ms / sim->fw_dur_ms;
    paso = nuevo_total - sim->fw_aplicado_cgrados;
    if (paso > 0 && !sim->fw_sin_motor) {
        desplazar_al_girar(sim, sim->fw_lado, paso);
        sim->rumbo_cgrados += (sim->fw_lado == CAL_LADO_DERECHA) ? paso : -paso;
        sim->fw_aplicado_cgrados = nuevo_total;
    }
    if (sim->fw_ms >= sim->fw_dur_ms) {
        sim->fw_activo = 0;
        sim->fw_cerrado = 1;
        if (!sim->fw_sin_respuesta && !sim->fw_pierde_mensaje) {
            /* el firmware cuenta + a la izquierda */
            navegador_aporte_giro(nav, sim->fw_codigo, (sim->fw_lado == CAL_LADO_IZQUIERDA) ? sim->fw_medido_x10 : -sim->fw_medido_x10, sim->fw_manejo_ms, sim->fw_anticipacion_ms[sim->fw_lado == CAL_LADO_IZQUIERDA ? 1 : 0], sim->t_ms);
        }
    }
}

/* Distancia que ve el láser: la del obstáculo más cercano que tenga `laser_mm` y esté visible; TOF_LIBRE (1200) si no ve nada. */
static int laser_ve(Simulador *sim)
{
    if (sim->laser_externo) {
        return sim->laser_externo_mm;
    }
    int i, mm = 1200;

    for (i = 0; i < sim->n_eventos; i++) {
        EventoObstaculo *e = &sim->eventos[i];

        if (e->laser_mm > 0 && sim->t_ms >= e->t_ini_ms && sim->t_ms < e->t_fin_ms) {
            int d;

            long dif;

            if (!e->visto) {
                e->visto = 1;
                e->base_recorrido_cmm = sim->recorrido_cmm;
                e->base_rumbo_cgrados = sim->rumbo_cgrados;
            }
            dif = ((sim->rumbo_cgrados - e->base_rumbo_cgrados) % 36000 + 54000) % 36000 - 18000;
            if (dif > 1500 || dif < -1500) {
                continue;   /* el carro ya no apunta hacia el obstáculo: el láser no lo ve */
            }
            d = e->laser_mm - (int)((sim->recorrido_cmm - e->base_recorrido_cmm) / 100);

            if (d < 8) {
                d = 8;
            }
            if (d < mm) {
                mm = d;
            }
        }
    }
    return mm;
}

/* El ángulo que da el giroscopio (decimas de grado, + a la izquierda): lo que giró el carro REAL desde que empezó, con el error de escala del giroscopio. */
static int angulo_fw_x10(const Simulador *sim)
{
    long cgrados_der = sim->rumbo_cgrados - sim->rumbo0_cgrados;

    return (int)(-(cgrados_der / 10) * (100 + sim->fw_escala_pct) / 100);
}

static int es_estado_sin_movimiento(EstadoNav estado)
{
    return estado == NAV_ESPERANDO || estado == NAV_TERMINADO || estado == NAV_ERROR;
}

ComandoNav sim_correr(Simulador *sim, Navegador *nav, uint32_t duracion_ms)
{
    ComandoNav comando = NAV_CMD_STOP;
    ComandoNav anterior = (sim->n_cambios > 0) ? sim->cambios[sim->n_cambios - 1].comando : NAV_CMD_STOP;
    uint32_t fin = sim->t_ms + duracion_ms;
    ResultadoObstaculos cuadro;
    int hay_primer_cambio = (sim->n_cambios == 0);

    while (sim->t_ms < fin) {
        const ResultadoObstaculos *obs = NULL;

        if (sim->t_ms == sim->t_cancelar_ms) {
            navegador_cancelar(nav);
        }
        if (sim->periodo_cuadro_ms > 0 && sim->t_ms >= sim->proximo_cuadro_ms) {
            armar_cuadro(sim, sim->proximo_cuadro_ms, &cuadro);   /* el cuadro es de su hora nominal, llega en este tick */
            sim->proximo_cuadro_ms += (uint32_t)sim->periodo_cuadro_ms;
            obs = &cuadro;
            if (sim->visual_activo) {   /* la cámara mide cuánto giró el carro desde el cuadro anterior */
                long medido_x10 = sim->giro_desde_cuadro_cgrados / 10 * (100 + sim->visual_error_pct) / 100;
                int perdida = (sim->t_ms >= sim->visual_perdida_desde_ms && sim->t_ms < sim->visual_perdida_hasta_ms);

                if (sim->n_pendientes < 64) {   /* la medida llega al navegador despues del retraso de la camara */
                    sim->pend_medido_x10[sim->n_pendientes] = medido_x10;
                    sim->pend_dt_ms[sim->n_pendientes] = (int)(sim->t_ms - sim->t_cuadro_visual_ms);
                    sim->pend_confiable[sim->n_pendientes] = !perdida;
                    sim->pend_llegada_ms[sim->n_pendientes] = sim->t_ms + (uint32_t)sim->visual_latencia_ms;
                    sim->n_pendientes++;
                }
                sim->giro_desde_cuadro_cgrados = 0;
                sim->t_cuadro_visual_ms = sim->t_ms;
            }
        }
        while (sim->n_pendientes > 0 && sim->pend_llegada_ms[0] <= sim->t_ms) {   /* llegan las medidas de la cámara (con su retraso) */
            int k;

            navegador_aporte_visual(nav, (int)sim->pend_medido_x10[0], sim->pend_dt_ms[0], sim->pend_confiable[0]);
            for (k = 1; k < sim->n_pendientes; k++) {
                sim->pend_medido_x10[k - 1] = sim->pend_medido_x10[k];
                sim->pend_dt_ms[k - 1] = sim->pend_dt_ms[k];
                sim->pend_confiable[k - 1] = sim->pend_confiable[k];
                sim->pend_llegada_ms[k - 1] = sim->pend_llegada_ms[k];
            }
            sim->n_pendientes--;
        }
        if ((sim->tof_activo || sim->fw_giroscopio) && !sim->sens_apagado && sim->t_ms >= sim->proximo_sens_ms) {   /* SENS cada 100 ms (ángulo del giroscopio y láser); el láser mira lo que había en el cuadro anterior */
            int mm = sim->tof_activo ? laser_ve(sim) : 1200;

            sim->proximo_sens_ms = sim->t_ms + 100;
            navegador_aporte_sens(nav, mm, (sim->tof_activo && !sim->tof_perdido) ? 1 : 0, sim->tof_activo && mm < 1200, angulo_fw_x10(sim), sim->t_ms);
            if (sim->tof_activo && !sim->tof_perdido && sim->cmd_previo == NAV_CMD_FORWARD && !sim->tof_parado) {
                sim->tof_cerca_seguidas = (mm < 120) ? sim->tof_cerca_seguidas + 1 : 0;
                if (sim->tof_cerca_seguidas >= 2) {   /* parada de emergencia del firmware: solo avanzando */
                    sim->tof_parado = 1;
                    sim->tof_paro_hecho++;
                    sim->tof_cerca_seguidas = 0;
                    navegador_aporte_tof_paro(nav, mm, sim->t_ms);
                }
            } else if (sim->cmd_previo != NAV_CMD_FORWARD) {
                sim->tof_cerca_seguidas = 0;
            }
        }
        comando = navegador_tick(nav, sim->t_ms, obs);
        if (sim->fw_giroscopio && navegador_tomar_consulta_imu(nav) && !sim->imu_sin_respuesta) {
            navegador_aporte_imu(nav, angulo_fw_x10(sim), sim->t_ms);   /* el firmware contesta IMU? con el ángulo actual */
        }
        if (sim->fw_giroscopio) {   /* si el navegador pide un giro por ángulo, el firmware simulado lo toma */
            int grados_fw;

            if (navegador_tomar_giro_fw(nav, &grados_fw)) {
                fw_iniciar(sim, grados_fw);
            }
        }
        if (comando != NAV_CMD_FORWARD) {
            sim->tof_parado = 0;   /* el firmware suelta el freno cuando llega otro comando */
        }

        if (es_estado_sin_movimiento(nav->estado) && comando != NAV_CMD_STOP) {
            sim->violaciones_seguridad++;
        }
        if ((hay_primer_cambio || comando != anterior) && sim->n_cambios < SIM_MAX_CAMBIOS) {
            sim->cambios[sim->n_cambios].t_ms = sim->t_ms;
            sim->cambios[sim->n_cambios].comando = comando;
            sim->cambios[sim->n_cambios].estado = nav->estado;
            sim->n_cambios++;
        }
        hay_primer_cambio = 0;
        anterior = comando;

        /* El comando se mantiene hasta el siguiente tick. Si cambió, el anterior se suelta (y el carro
         * desliza el frenado); durante el arranque del nuevo comando el carro no se mueve. */
        if (comando != sim->cmd_previo) {
            if (sim->fw_cerrado) {
                sim->fw_cerrado = 0;   /* el firmware ya incluyó lo que siguió girando al frenar */
            } else {
                deslizar_al_frenar(sim, sim->cmd_previo);
            }
            sim->cmd_previo = comando;
            sim->ms_en_comando = 0;
            sim->giro_h0_cgrados = sim->rumbo_cgrados;   /* un comando nuevo: el giro (y su desplazamiento) se cuentan desde cero */
            sim->giro_acum_cgrados = 0;
            sim->desp_x_cmm = 0;
            sim->desp_y_cmm = 0;
        }
        {
            int antes = sim->ms_en_comando;
            int despues = antes + NAV_TICK_MS;
            int desde = (antes > sim->real.arranque_ms) ? antes : sim->real.arranque_ms;
            int efectivo = (despues > desde) ? despues - desde : 0;

            if (nav->giro_fw) {
                fw_avanzar(sim, nav, NAV_TICK_MS);   /* este giro lo hace el firmware con su giroscopio */
            } else if (!(sim->tof_parado && comando == NAV_CMD_FORWARD)) {
                mover_carro(sim, comando, efectivo);
            }
            sim->ms_en_comando = despues;
        }
        sim->t_ms += NAV_TICK_MS;
    }
    return comando;
}

int sim_x_mm(const Simulador *sim)
{
    return (int)(sim->x_cmm / 100);
}

int sim_y_mm(const Simulador *sim)
{
    return (int)(sim->y_cmm / 100);
}

int sim_rumbo_grados(const Simulador *sim)
{
    return rumbo_entero(sim);
}

int sim_contar_cambios_a(const Simulador *sim, ComandoNav comando)
{
    int i, cuenta = 0;

    for (i = 0; i < sim->n_cambios; i++) {
        if (sim->cambios[i].comando == comando) {
            cuenta++;
        }
    }
    return cuenta;
}

long sim_primer_cambio_a(const Simulador *sim, ComandoNav comando, uint32_t desde_ms)
{
    int i;

    for (i = 0; i < sim->n_cambios; i++) {
        if (sim->cambios[i].comando == comando && sim->cambios[i].t_ms >= desde_ms) {
            return (long)sim->cambios[i].t_ms;
        }
    }
    return -1;
}
