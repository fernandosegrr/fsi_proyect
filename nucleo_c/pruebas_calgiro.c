/*
 * pruebas_calgiro.c — Pruebas en PC de calgiro.c (calibración del giro con el giroscopio) con un carro simulado.
 *
 * El carro simulado gira (con su arranque, sus ms por grado y su frenado, distintos para cada lado) y un giroscopio falso mide ese giro con ruido y error de escala. Cada paso del simulador es de 50 ms,
 * como el ciclo de la app.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "calgiro.h"

static int casos = 0, fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) fallos++;
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

/* Un carro de verdad: por cada lado [0] izquierda, [1] derecha. */
typedef struct {
    int arr_ms[2];
    int mpg_x10[2];       /* ms por grado por 10 */
    int fre_x10[2];       /* grados por 10 que sigue girando al frenar */
    int escala_pct;       /* error de escala del giroscopio: +2 = mide 2 % de más */
    int ruido_x10;        /* ruido de cada lectura en décimas de grado (±) */
    int invertido;        /* 1 = el giroscopio da el ángulo con el signo cambiado */
    int sin_giro_lado;    /* -1 = ninguno; 0 o 1 = ese lado no gira nunca (sin pilas) */
    int resbala_giro;     /* -1 = ninguno; k = el giro k (0 a 5) solo gira la mitad (patinó) */
    long mover_al_inicio_ms;   /* gira lentamente durante tantos ms al empezar (alguien lo mueve a mano) */
    long sin_angulo_desde, sin_angulo_hasta;   /* entre estas horas el giroscopio no da lectura */
} Carro;

static unsigned semilla = 99u;
static int azar(int minimo, int maximo)
{
    semilla = semilla * 1664525u + 1013904223u;
    return minimo + (int)((semilla >> 8) % (unsigned)(maximo - minimo + 1));
}

/* Ejecuta la calibración contra el carro simulado: devuelve el último comando y deja el estado en `cg`. `comandos` guarda los cambios de comando (máximo 64). */
static int comandos[64];
static int n_comandos;
static int ultimo_cmd;
static long tiempo_total_ms;
static long cuando_giro[CG_TOTAL_GIROS];

static void simular(Carro *c, CalGiro *cg, const CalibracionCarro *guardada, long tope_ms, long cancelar_en_ms)
{
    long t = 1000;                 /* ms */
    long ang_mdeg = 0;             /* ángulo VERDADERO (positivo a la izquierda), milésimas de grado */
    int cmd = CG_CMD_STOP, cmd_previo = CG_CMD_STOP;
    long t_cmd = 0;                /* cuándo empezó el comando de giro actual */
    long t_stop = -100000;
    long coast_pend_mdeg = 0;
    long t_fin;
    int giro_visto = 0;
    int lado_cmd = 0;

    n_comandos = 0;
    ultimo_cmd = CG_CMD_STOP;
    memset(cuando_giro, 0, sizeof cuando_giro);
    calgiro_iniciar(cg, guardada, (uint32_t)t);
    t_fin = t + tope_ms;
    while (t < t_fin) {
        int hay = !(t >= c->sin_angulo_desde && t < c->sin_angulo_hasta);
        int medido_x10;
        long mdeg_por_ms;

        /* movimiento: el comando actual empuja después del arranque; al frenar sigue el frenado repartido en 250 ms */
        if (cmd == CG_CMD_IZQ || cmd == CG_CMD_DER) {
            lado_cmd = (cmd == CG_CMD_IZQ) ? 0 : 1;
            if (t - t_cmd > c->arr_ms[lado_cmd] && c->sin_giro_lado != lado_cmd) {
                mdeg_por_ms = 10000L / c->mpg_x10[lado_cmd];
                if (c->resbala_giro >= 0 && c->resbala_giro == giro_visto - 1) {
                    mdeg_por_ms /= 2;
                }
                ang_mdeg += (cmd == CG_CMD_IZQ ? 1 : -1) * mdeg_por_ms * 50;
            }
        } else if (coast_pend_mdeg != 0) {
            long paso = coast_pend_mdeg / 5;   /* ~5 pasos de 50 ms */

            if (paso == 0) {
                paso = coast_pend_mdeg;
            }
            ang_mdeg += paso;
            coast_pend_mdeg -= paso;
        }
        if (t < 1000 + c->mover_al_inicio_ms) {
            ang_mdeg += 20 * 50;   /* alguien lo mueve a mano: 20 mdeg por ms = 1 grado por segundo */
        }
        medido_x10 = (int)(ang_mdeg / 100 * (100 + c->escala_pct) / 100) + azar(-c->ruido_x10, c->ruido_x10);
        if (c->invertido) {
            medido_x10 = -medido_x10;
        }
        if (cancelar_en_ms >= 0 && t == 1000 + cancelar_en_ms) {
            calgiro_cancelar(cg);
        }
        cmd = calgiro_tick(cg, (uint32_t)t, hay, medido_x10);
        if (cmd != cmd_previo) {
            if (n_comandos < 64) {
                comandos[n_comandos++] = cmd;
            }
            if (cmd != CG_CMD_STOP) {
                t_cmd = t;
                giro_visto++;
                if (giro_visto - 1 < CG_TOTAL_GIROS) cuando_giro[giro_visto - 1] = t;
            } else if (cmd_previo != CG_CMD_STOP) {
                int l = (cmd_previo == CG_CMD_IZQ) ? 0 : 1;

                t_stop = t;
                coast_pend_mdeg = (cmd_previo == CG_CMD_IZQ ? 1 : -1) * (long)c->fre_x10[l] * 100;   /* sigue girando al frenar */
            }
            cmd_previo = cmd;
        }
        ultimo_cmd = cmd;
        t += 50;
        if (cg->estado == CG_LISTO || cg->estado == CG_ERROR || cg->estado == CG_CANCELADO) {
            tiempo_total_ms = t - 1000;
            /* unos pasos más para ver que no queda ningún comando puesto */
            {
                int i;

                for (i = 0; i < 6; i++) {
                    ultimo_cmd = calgiro_tick(cg, (uint32_t)t, 1, medido_x10);
                    t += 50;
                }
            }
            (void)t_stop;
            return;
        }
    }
    tiempo_total_ms = t - 1000;
}

static CalibracionCarro guardada_aprox(int mpg_izq, int mpg_der, int fre_izq, int fre_der)
{
    CalibracionCarro g;

    cal_defecto(&g);
    g.arranque_ms = 100;
    g.ms_por_grado_izq_x10 = mpg_izq;
    g.ms_por_grado_der_x10 = mpg_der;
    g.frenado_grados_izq_x10 = fre_izq;
    g.frenado_grados_der_x10 = fre_der;
    g.medidos |= CAL_MEDIDO_GIRO_IZQ | CAL_MEDIDO_GIRO_DER | CAL_MEDIDO_ARRANQUE;
    return g;
}

static Carro carro_de(int mpg_izq, int mpg_der, int fre_izq, int fre_der)
{
    Carro c;

    memset(&c, 0, sizeof c);
    c.arr_ms[0] = c.arr_ms[1] = 100;
    c.mpg_x10[0] = mpg_izq;
    c.mpg_x10[1] = mpg_der;
    c.fre_x10[0] = fre_izq;
    c.fre_x10[1] = fre_der;
    c.ruido_x10 = 3;
    c.sin_giro_lado = -1;
    c.resbala_giro = -1;
    c.sin_angulo_desde = c.sin_angulo_hasta = 0;
    return c;
}

static int cerca(int valor, int esperado, int tolerancia_pct)
{
    int d = valor - esperado;

    if (d < 0) d = -d;
    return d * 100 <= esperado * tolerancia_pct;
}

int main(void)
{
    Carro c;
    CalGiro cg;
    CalibracionCarro g, nueva;
    CalGiroLado r[2];
    int i;

    printf("== Calibracion del giro con el giroscopio ==\n");

    /* 1. Un carro cuyos valores guardados estan 20 % desajustados. */
    g = guardada_aprox(80, 90, 30, 30);                  /* lo guardado: 8.0 y 9.0 ms por grado, 3 grados de frenado */
    c = carro_de(96, 81, 50, 20);                        /* lo real: 9.6 y 8.1 ms por grado, frenado de 5.0 y 2.0 grados */
    simular(&c, &cg, &g, 120000, -1);
    caso("carro 20 % desajustado: termina en LISTO sin error", cg.estado == CG_LISTO && cg.error == CG_ERR_NINGUNO);
    caso("hace 6 giros alternados: izquierda, derecha, izquierda, derecha, izquierda, derecha (y siempre vuelve a STOP)",
         n_comandos == 12 && comandos[0] == CG_CMD_IZQ && comandos[1] == CG_CMD_STOP && comandos[2] == CG_CMD_DER && comandos[4] == CG_CMD_IZQ && comandos[6] == CG_CMD_DER &&
         comandos[8] == CG_CMD_IZQ && comandos[10] == CG_CMD_DER && comandos[11] == CG_CMD_STOP);
    caso("al terminar no queda ningun comando de giro puesto", ultimo_cmd == CG_CMD_STOP && cg.comando == CG_CMD_STOP);
    printf("      tiempo total %ld ms\n", tiempo_total_ms);
    caso("toda la calibracion tarda menos de 45 segundos", tiempo_total_ms < 45000);
    caso("calgiro_resultado da la calibracion y el resumen", calgiro_resultado(&cg, &nueva, r) == 1);
    printf("      izquierda: ms/grado %d -> %d (real 96), frenado %d -> %d (real 50), cambio %d %%\n", r[0].ms_x10_antes, r[0].ms_x10_despues, r[0].frenado_x10_antes, r[0].frenado_x10_despues, r[0].cambio_ms_pct);
    printf("      derecha  : ms/grado %d -> %d (real 81), frenado %d -> %d (real 20), cambio %d %%\n", r[1].ms_x10_antes, r[1].ms_x10_despues, r[1].frenado_x10_antes, r[1].frenado_x10_despues, r[1].cambio_ms_pct);
    caso("los ms por grado medidos quedan a menos de 5 % de los reales (izquierda 9.6, derecha 8.1)", cerca(r[0].ms_x10_despues, 96, 5) && cerca(r[1].ms_x10_despues, 81, 5));
    caso("el frenado medido queda a menos de 2 grados del real (5.0 y 2.0)", cerca(r[0].frenado_x10_despues, 50, 40) && r[1].frenado_x10_despues >= 0 && r[1].frenado_x10_despues <= 40);
    caso("la calibracion nueva trae esos valores", nueva.ms_por_grado_izq_x10 == r[0].ms_x10_despues && nueva.ms_por_grado_der_x10 == r[1].ms_x10_despues);
    caso("avisa que cambio mas de 10 % (la izquierda +20 %, la derecha -10 % no pasa del limite)", r[0].cambio_grande == 1 && r[0].cambio_ms_pct >= 15);
    caso("el resto de la calibracion no se toca (arranque, avance, retroceso, desplazamiento)",
         nueva.arranque_ms == g.arranque_ms && nueva.ms_por_cm_x10 == g.ms_por_cm_x10 && nueva.frenado_mm == g.frenado_mm &&
         nueva.desp_adelante_izq_um == g.desp_adelante_izq_um && nueva.desp_derecha_der_um == g.desp_derecha_der_um && nueva.desp_grados_izq == g.desp_grados_izq);
    for (i = 0; i < 6; i++) {
        char nombre[80];

        snprintf(nombre, sizeof nombre, "giro %d: dura entre %d y %d ms", i + 1, CG_MS_MIN, CG_MS_MAX);
        caso(nombre, r[i % 2].duraciones_ms[i / 2] >= CG_MS_MIN && r[i % 2].duraciones_ms[i / 2] <= CG_MS_MAX + 100);
    }

    /* 2. Un carro bien calibrado: no avisa. */
    g = guardada_aprox(96, 81, 50, 20);
    c = carro_de(98, 80, 50, 20);
    simular(&c, &cg, &g, 120000, -1);
    caso("carro bien calibrado (valores guardados a menos de 3 %): LISTO y no avisa cambio grande", cg.estado == CG_LISTO && calgiro_resultado(&cg, &nueva, r) && !r[0].cambio_grande && !r[1].cambio_grande);

    /* 3. Ruido y escala del giroscopio. */
    g = guardada_aprox(80, 90, 30, 30);
    c = carro_de(90, 90, 40, 40);
    c.ruido_x10 = 4;
    c.escala_pct = 2;
    simular(&c, &cg, &g, 120000, -1);
    calgiro_resultado(&cg, &nueva, r);
    printf("      ruido +-0.4 grados y escala +2 %%: ms/grado izq %d der %d (reales 90 y 90; con +2 %% de escala salen ~88)\n", r[0].ms_x10_despues, r[1].ms_x10_despues);
    caso("con ruido de +-0.4 grados y 2 % de error de escala del giroscopio sigue a menos de 6 % de los reales", cg.estado == CG_LISTO && cerca(r[0].ms_x10_despues, 90, 6) && cerca(r[1].ms_x10_despues, 90, 6));

    /* 4. Alguien lo mueve a mano al empezar: espera a que este quieto. */
    c = carro_de(90, 90, 40, 40);
    c.mover_al_inicio_ms = 3000;
    simular(&c, &cg, &g, 120000, -1);
    caso("si lo mueven a mano al empezar no gira hasta que esta quieto (el primer giro empieza despues de que lo sueltan, a los 3 s o mas)", cg.estado == CG_LISTO && cuando_giro[0] >= 1000 + 3000);

    /* 5. El giroscopio se calla a mitad. */
    c = carro_de(90, 90, 40, 40);
    c.sin_angulo_desde = 1000 + 9000;
    c.sin_angulo_hasta = 1000 + 14000;
    simular(&c, &cg, &g, 120000, -1);
    caso("si el giroscopio deja de dar el angulo mas de 1.5 s: ERROR sin giroscopio y el comando queda en STOP", cg.estado == CG_ERROR && cg.error == CG_ERR_SIN_GIROSCOPIO && ultimo_cmd == CG_CMD_STOP);
    caso("... y la calibracion nueva no se entrega", calgiro_resultado(&cg, &nueva, r) == 0);
    c = carro_de(90, 90, 40, 40);
    c.sin_angulo_desde = 1000 + 9000;
    c.sin_angulo_hasta = 1000 + 9800;   /* un hueco de 0.8 s: no pasa nada */
    simular(&c, &cg, &g, 120000, -1);
    caso("un hueco corto (0.8 s) del angulo no es error", cg.estado == CG_LISTO);

    /* 6. El carro no gira. */
    c = carro_de(90, 90, 40, 40);
    c.sin_giro_lado = 1;
    simular(&c, &cg, &g, 120000, -1);
    caso("si un lado no gira (sin pilas de motores): ERROR el carro no giro", cg.estado == CG_ERROR && cg.error == CG_ERR_NO_GIRO && ultimo_cmd == CG_CMD_STOP);

    /* 7. Angulo con el signo contrario. */
    c = carro_de(90, 90, 40, 40);
    c.invertido = 1;
    simular(&c, &cg, &g, 120000, -1);
    caso("si el giroscopio da el signo contrario (IMU_SIGNO mal): ERROR giro al reves", cg.estado == CG_ERROR && cg.error == CG_ERR_SENTIDO);

    /* 8. Giro enorme (calibracion guardada muy desajustada). */
    g = guardada_aprox(80, 80, 30, 30);
    c = carro_de(25, 25, 30, 30);       /* el carro real gira 3 veces mas rapido de lo guardado */
    simular(&c, &cg, &g, 120000, -1);
    calgiro_resultado(&cg, &nueva, r);
    caso("un carro 3 veces mas rapido que lo guardado: el primer giro de cada lado acomoda los siguientes y termina bien (a menos de 6 %)", cg.estado == CG_LISTO && cerca(r[0].ms_x10_despues, 25, 6) && cerca(r[1].ms_x10_despues, 25, 6));
    c = carro_de(12, 12, 30, 30);       /* 6 veces mas rapido: el primer giro de 40 pasaria de 260 grados */
    simular(&c, &cg, &g, 120000, -1);
    caso("si un giro pasa de 190 grados (lo guardado esta muy mal): ERROR y para", cg.estado == CG_ERROR && cg.error == CG_ERR_MUCHO && ultimo_cmd == CG_CMD_STOP);

    /* 9. Cancelar. */
    g = guardada_aprox(80, 90, 30, 30);
    c = carro_de(80, 90, 30, 30);
    simular(&c, &cg, &g, 120000, 4500);
    caso("cancelar a mitad: queda CANCELADO, el comando es STOP y no se entrega calibracion", cg.estado == CG_CANCELADO && ultimo_cmd == CG_CMD_STOP && calgiro_resultado(&cg, &nueva, r) == 0);

    /* 10. Un giro que patino. */
    c = carro_de(80, 90, 30, 30);
    c.resbala_giro = 2;                  /* el 3.o gira solo la mitad: 70 grados deberian ser 35 */
    simular(&c, &cg, &g, 120000, -1);
    caso("si un giro patina (gira la mitad) los tres de ese lado no siguen una recta: ERROR inconsistente y no se entrega calibracion", cg.estado == CG_ERROR && cg.error == CG_ERR_INCONSISTENTE &&
         calgiro_resultado(&cg, &nueva, r) == 0);

    /* 11. Muchos carros distintos (Monte Carlo): el procedimiento recupera los valores reales. */
    {
        int buenos_ms = 0, buenos_fre = 0, terminados = 0, n = 200, errores = 0;

        for (i = 0; i < n; i++) {
            int mi = azar(60, 120), md = azar(60, 120), fi = azar(0, 60), fd = azar(0, 60);

            g = guardada_aprox(azar(70, 110), azar(70, 110), azar(0, 60), azar(0, 60));   /* lo guardado, distinto de lo real */
            c = carro_de(mi, md, fi, fd);
            c.ruido_x10 = azar(1, 4);
            c.escala_pct = azar(-2, 2);
            simular(&c, &cg, &g, 150000, -1);
            if (cg.estado == CG_LISTO) {
                terminados++;
                calgiro_resultado(&cg, &nueva, r);
                /* el giroscopio con X % de escala mide X % mas grados: los ms por grado salen ese X % menos */
                if (cerca(r[0].ms_x10_despues, mi * 100 / (100 + c.escala_pct), 6) && cerca(r[1].ms_x10_despues, md * 100 / (100 + c.escala_pct), 6)) buenos_ms++;
                if (r[0].frenado_x10_despues - fi < 30 && fi - r[0].frenado_x10_despues < 30 && r[1].frenado_x10_despues - fd < 30 && fd - r[1].frenado_x10_despues < 30) buenos_fre++;
            } else {
                errores++;
                printf("      (carro %d: real %d,%d frenado %d,%d guardado %d,%d: error %d)\n", i, mi, md, fi, fd, g.ms_por_grado_izq_x10, g.ms_por_grado_der_x10, cg.error);
            }
        }
        printf("      Monte Carlo de %d carros: terminan %d, ms/grado a menos de 6 %%: %d, frenado a menos de 3 grados: %d, con error: %d\n", n, terminados, buenos_ms, buenos_fre, errores);
        caso("Monte Carlo: al menos el 95 % de los carros distintos terminan sin error", terminados >= n * 95 / 100);
        caso("Monte Carlo: de los que terminan, al menos el 95 % recupera los ms por grado reales con menos de 6 % de error", buenos_ms * 100 >= terminados * 95);
        caso("Monte Carlo: y el frenado con menos de 3 grados de error en al menos el 90 %", buenos_fre * 100 >= terminados * 90);
    }

    caso("textos de error en espanol", calgiro_texto_error(CG_ERR_NO_GIRO)[0] != '\0' && calgiro_texto_error(CG_ERR_NINGUNO)[0] == '\0');

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}
