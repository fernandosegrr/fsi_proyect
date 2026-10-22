/*
 * calgiro.c — Calibración del giro con el giroscopio (ver calgiro.h).
 *
 * CÓMO FUNCIONA (por cada uno de los 8 giros)
 *   PREPARANDO → espera a que el ángulo lleve CG_QUIETO_MS sin cambiar (el carro está quieto) y anota el ángulo de salida.
 *   GIRANDO    → devuelve LEFT o RIGHT hasta que pasa el tiempo planeado.
 *   ASENTANDO  → devuelve STOP; cuando pasaron CG_ASENTAR_MS y el ángulo lleva CG_QUIETO_FINAL_MS quieto, lo que giró es la diferencia con el ángulo de salida.
 * Al terminar el octavo giro se ajusta una recta por lado sin el calentamiento.
 */

#include <string.h>
#include "calgiro.h"

static const int OBJETIVO_GRADOS[CG_MEDICIONES_POR_LADO] = { 40, 40, 70, 100 };

static int valor_absoluto(int x)
{
    return x < 0 ? -x : x;
}

static int lado_del_giro(int k)
{
    return (k < CG_MEDICIONES_POR_LADO) ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA;
}

static int indice_lado(int lado)
{
    return lado == CAL_LADO_IZQUIERDA ? 0 : 1;
}

static int comando_del_giro(int k)
{
    return (k < CG_MEDICIONES_POR_LADO) ? CG_CMD_IZQ : CG_CMD_DER;
}

static void fallar(CalGiro *cg, int error)
{
    cg->estado = CG_ERROR;
    cg->error = error;
    cg->comando = CG_CMD_STOP;
}

void calgiro_iniciar(CalGiro *cg, const CalibracionCarro *guardada, uint32_t ahora_ms)
{
    int k;

    memset(cg, 0, sizeof *cg);
    if (guardada != NULL) {
        cg->base = *guardada;
    } else {
        cal_defecto(&cg->base);
    }
    cg->nueva = cg->base;
    for (k = 0; k < CG_TOTAL_GIROS; k++) {
        int m = k % CG_MEDICIONES_POR_LADO;
        int ms = cal_grados_a_ms(&cg->base, lado_del_giro(k) * OBJETIVO_GRADOS[m]);   /* lo que pide un giro de ese tamaño con la calibración guardada */

        if (ms < CG_MS_MIN) ms = CG_MS_MIN;
        if (ms > CG_MS_MAX) ms = CG_MS_MAX;
        cg->plan_ms[k] = ms;
    }
    cg->estado = CG_PREPARANDO;
    cg->t_inicio_ms = ahora_ms;
    cg->t_estado_ms = ahora_ms;
    cg->t_ref_ms = ahora_ms;
    cg->t_angulo_ms = ahora_ms;
}

void calgiro_cancelar(CalGiro *cg)
{
    if (cg->estado != CG_LISTO && cg->estado != CG_ERROR) {
        cg->estado = CG_CANCELADO;
    }
    cg->comando = CG_CMD_STOP;
}

/* Sigue el ángulo: si cambió más de CG_QUIETO_TOL_X10 desde la referencia, la referencia se mueve y el reloj de «quieto» vuelve a cero. */
static void seguir_quieto(CalGiro *cg, uint32_t ahora_ms)
{
    if (valor_absoluto(cg->ang_x10 - cg->ang_ref_x10) > CG_QUIETO_TOL_X10) {
        cg->ang_ref_x10 = cg->ang_x10;
        cg->t_ref_ms = ahora_ms;
    }
}

/* Revisa la recta sin truncar el frenado negativo. Dos puntos no bastan si
 * implican una inercia imposible. Devuelve el peor punto y el error máximo. */
static int revisar_recta(const CalGiro *cg, int l, int excluir, int *peor, int *maximo)
{
    int i, n = 0, ok = 1;
    int64_t sx = 0, sy = 0, sxx = 0, sxy = 0, den, num, intercepto;

    *peor = -1;
    *maximo = 0;
    for (i = 1; i < CG_MEDICIONES_POR_LADO; i++) {
        int x = cg->ms_real[l][i] - cg->base.arranque_ms;
        int y = cg->grados_x10[l][i];
        if (i == excluir) continue;
        if (x <= 0) return 0;
        n++; sx += x; sy += y; sxx += (int64_t)x * x; sxy += (int64_t)x * y;
    }
    den = n * sxx - sx * sx;
    num = n * sxy - sx * sy;
    if (den <= 0 || num <= 0) return 0;
    intercepto = (sy * den - num * sx) / (n * den);
    if (intercepto < -CG_INCONSISTENTE_X10 || intercepto > 200) ok = 0;
    for (i = 1; i < CG_MEDICIONES_POR_LADO; i++) {
        int predicho, error;
        if (i == excluir) continue;
        predicho = (int)(intercepto + num * (cg->ms_real[l][i] - cg->base.arranque_ms) / den);
        error = valor_absoluto(cg->grados_x10[l][i] - predicho);
        if (*peor < 0 || error > *maximo) { *peor = i; *maximo = error; }
        if (error > CG_INCONSISTENTE_X10 && error * 100 > cg->grados_x10[l][i] * 6) ok = 0;
    }
    return ok;
}

/* Ajusta las dos rectas sin calentamiento; permite quitar solo el peor punto. */
static void calcular(CalGiro *cg)
{
    int l;

    cg->nueva = cg->base;
    for (l = 0; l < 2; l++) {
        int lado = (l == 0) ? CAL_LADO_IZQUIERDA : CAL_LADO_DERECHA;
        CalGiroLado *r = &cg->resumen[l];
        int ms_antes = (l == 0) ? cg->base.ms_por_grado_izq_x10 : cg->base.ms_por_grado_der_x10;
        int fre_antes = (l == 0) ? cg->base.frenado_grados_izq_x10 : cg->base.frenado_grados_der_x10;
        int ms_des, fre_des, codigo, i, peor, error_max, descartar = -1, n = 0;
        int ms[CG_GIROS_POR_LADO], grados[CG_GIROS_POR_LADO];

        if (!revisar_recta(cg, l, -1, &peor, &error_max)) {
            int otro;
            descartar = peor;
            if (descartar < 1 || !revisar_recta(cg, l, descartar, &otro, &error_max)) {
                fallar(cg, CG_ERR_INCONSISTENTE);
                return;
            }
        }
        for (i = 1; i < CG_MEDICIONES_POR_LADO; i++) {
            if (i == descartar) continue;
            ms[n] = cg->ms_real[l][i];
            grados[n++] = cg->grados_x10[l][i];
        }
        codigo = cal_ajustar_giro(&cg->nueva, lado, n, ms, grados);
        if (codigo != CAL_OK) {
            cg->codigo_ajuste = codigo;
            fallar(cg, CG_ERR_AJUSTE);
            return;
        }
        ms_des = (l == 0) ? cg->nueva.ms_por_grado_izq_x10 : cg->nueva.ms_por_grado_der_x10;
        fre_des = (l == 0) ? cg->nueva.frenado_grados_izq_x10 : cg->nueva.frenado_grados_der_x10;
        for (i = 1; i < CG_MEDICIONES_POR_LADO; i++) {
            int predicho, error;
            if (i == descartar) continue;
            predicho = fre_des + (cg->ms_real[l][i] - cg->base.arranque_ms) * 100 / ms_des;
            error = valor_absoluto(cg->grados_x10[l][i] - predicho);
            if (error > CG_INCONSISTENTE_X10 && error * 100 > cg->grados_x10[l][i] * 6) {
                fallar(cg, CG_ERR_INCONSISTENTE);
                return;
            }
        }
        r->lado = lado;
        r->ms_x10_antes = ms_antes;
        r->ms_x10_despues = ms_des;
        r->frenado_x10_antes = fre_antes;
        r->frenado_x10_despues = fre_des;
        r->cambio_ms_pct = (ms_antes > 0) ? (int)(((long)(ms_des - ms_antes) * 100 + (ms_des >= ms_antes ? ms_antes / 2 : -ms_antes / 2)) / ms_antes) : 0;
        r->cambio_frenado_pct = (fre_antes > 0) ? (int)(((long)(fre_des - fre_antes) * 100) / fre_antes) : 0;
        r->cambio_grande = valor_absoluto(r->cambio_ms_pct) > CG_CAMBIO_AVISO_PCT;
        r->error_max_x10 = error_max;
        r->descartado = descartar < 0 ? 0 : descartar + 1;
        for (i = 0; i < CG_GIROS_POR_LADO; i++) {
            r->duraciones_ms[i] = cg->ms_real[l][i + 1];
            r->grados_x10[i] = cg->grados_x10[l][i + 1];
        }
    }
    cg->estado = CG_LISTO;
    cg->comando = CG_CMD_STOP;
}

int calgiro_tick(CalGiro *cg, uint32_t ahora_ms, int hay_angulo, int angulo_x10)
{
    int k = cg->giro_actual;

    if (cg->estado == CG_PARADO || cg->estado == CG_LISTO || cg->estado == CG_ERROR || cg->estado == CG_CANCELADO) {
        cg->comando = CG_CMD_STOP;
        return CG_CMD_STOP;
    }
    if (hay_angulo) {
        if (!cg->tiene_angulo) {
            cg->tiene_angulo = 1;
            cg->ang_ref_x10 = angulo_x10;   /* la primera lectura: de aquí se empieza a mirar si está quieto */
            cg->t_ref_ms = ahora_ms;
        }
        cg->ang_x10 = angulo_x10;
        cg->t_angulo_ms = ahora_ms;
    } else if ((uint32_t)(ahora_ms - cg->t_angulo_ms) > (uint32_t)CG_SIN_ANGULO_MS) {
        fallar(cg, CG_ERR_SIN_GIROSCOPIO);
        return CG_CMD_STOP;
    }
    if ((uint32_t)(ahora_ms - cg->t_inicio_ms) > (uint32_t)CG_TIEMPO_MAX_MS) {
        fallar(cg, CG_ERR_TIEMPO);
        return CG_CMD_STOP;
    }
    if (hay_angulo) {
        seguir_quieto(cg, ahora_ms);
    }

    switch (cg->estado) {
    case CG_PREPARANDO:
        if (hay_angulo && (uint32_t)(ahora_ms - cg->t_ref_ms) >= (uint32_t)CG_QUIETO_MS && (uint32_t)(ahora_ms - cg->t_estado_ms) >= (uint32_t)CG_QUIETO_MS) {
            cg->ang0_x10 = cg->ang_x10;
            cg->estado = CG_GIRANDO;
            cg->t_cmd_ms = ahora_ms;
            cg->t_estado_ms = ahora_ms;
            cg->comando = comando_del_giro(k);
            return cg->comando;
        }
        cg->comando = CG_CMD_STOP;
        return CG_CMD_STOP;
    case CG_GIRANDO:
        if ((uint32_t)(ahora_ms - cg->t_cmd_ms) >= (uint32_t)cg->plan_ms[k]) {
            int l = indice_lado(lado_del_giro(k));

            cg->ms_real[l][k % CG_MEDICIONES_POR_LADO] = (int)(ahora_ms - cg->t_cmd_ms);
            cg->estado = CG_ASENTANDO;
            cg->t_estado_ms = ahora_ms;
            cg->ang_ref_x10 = cg->ang_x10;
            cg->t_ref_ms = ahora_ms;
            cg->comando = CG_CMD_STOP;
            return CG_CMD_STOP;
        }
        return cg->comando;
    case CG_PAUSA:
        if ((uint32_t)(ahora_ms - cg->t_estado_ms) >= (uint32_t)CG_PAUSA_MS) {
            cg->estado = CG_PREPARANDO;
            cg->t_estado_ms = ahora_ms;
            cg->ang_ref_x10 = cg->ang_x10;
            cg->t_ref_ms = ahora_ms;
        }
        cg->comando = CG_CMD_STOP;
        return CG_CMD_STOP;
    default:   /* CG_ASENTANDO */
        if (hay_angulo && (uint32_t)(ahora_ms - cg->t_estado_ms) >= (uint32_t)CG_ASENTAR_MS && (uint32_t)(ahora_ms - cg->t_ref_ms) >= (uint32_t)CG_QUIETO_FINAL_MS) {
            int lado = lado_del_giro(k);
            int m = k % CG_MEDICIONES_POR_LADO;
            int girado_x10 = (lado == CAL_LADO_IZQUIERDA) ? cg->ang_x10 - cg->ang0_x10 : cg->ang0_x10 - cg->ang_x10;   /* grados que giró en la dirección pedida */

            cg->grados_x10[indice_lado(lado)][m] = girado_x10; /* también se conserva si provoca un error */
            cg->medido[indice_lado(lado)][m] = 1;
            if (girado_x10 < -CG_MIN_GRADOS_X10) {
                fallar(cg, CG_ERR_SENTIDO);
                return CG_CMD_STOP;
            }
            if (girado_x10 < CG_MIN_GRADOS_X10) {
                fallar(cg, CG_ERR_NO_GIRO);
                return CG_CMD_STOP;
            }
            if (girado_x10 > CG_MAX_GRADOS * 10) {
                fallar(cg, CG_ERR_MUCHO);
                return CG_CMD_STOP;
            }
            if (k == 1 || k == 5) {   /* solo el primer giro válido de cada bloque acomoda los siguientes; el calentamiento nunca cambia la recta ni los tiempos */
                int j;
                long esperado_x10 = (long)OBJETIVO_GRADOS[1] * 10;
                int fin = (k == 1) ? CG_MEDICIONES_POR_LADO : CG_TOTAL_GIROS;

                for (j = k + 1; j < fin; j++) {
                    long util = cg->plan_ms[j] - cg->base.arranque_ms;
                    long nuevo;

                    if (util < 0) util = 0;
                    nuevo = cg->base.arranque_ms + util * esperado_x10 / girado_x10;   /* si giró de más el tiempo útil se acorta, si giró de menos se alarga */
                    if (nuevo < CG_MS_MIN) nuevo = CG_MS_MIN;
                    if (nuevo > CG_MS_MAX) nuevo = CG_MS_MAX;
                    cg->plan_ms[j] = (int)nuevo;
                }
            }
            cg->giro_actual = k + 1;
            if (cg->giro_actual >= CG_TOTAL_GIROS) {
                calcular(cg);
                return CG_CMD_STOP;
            }
            if (cg->giro_actual == CG_MEDICIONES_POR_LADO) {
                cg->estado = CG_PAUSA;
                cg->t_estado_ms = ahora_ms;
                cg->comando = CG_CMD_STOP;
                return CG_CMD_STOP;
            }
            cg->estado = CG_PREPARANDO;
            cg->t_estado_ms = ahora_ms;
            cg->ang_ref_x10 = cg->ang_x10;
            cg->t_ref_ms = ahora_ms;
        }
        cg->comando = CG_CMD_STOP;
        return CG_CMD_STOP;
    }
}

int calgiro_resultado(const CalGiro *cg, CalibracionCarro *nueva, CalGiroLado resumen[2])
{
    if (cg == NULL || cg->estado != CG_LISTO) {
        return 0;
    }
    if (nueva != NULL) {
        *nueva = cg->nueva;
    }
    if (resumen != NULL) {
        resumen[0] = cg->resumen[0];
        resumen[1] = cg->resumen[1];
    }
    return 1;
}

const char *calgiro_texto_error(int error)
{
    switch (error) {
    case CG_ERR_SIN_GIROSCOPIO: return "el giroscopio dejó de dar el ángulo";
    case CG_ERR_NO_GIRO:        return "el carro no giró (¿están puestas las pilas de los motores?)";
    case CG_ERR_SENTIDO:        return "el carro giró al revés de lo pedido (revisa IMU_SIGNO)";
    case CG_ERR_MUCHO:          return "un giro pasó de 190 grados: la calibración guardada está muy desajustada, hazla a mano primero";
    case CG_ERR_TIEMPO:         return "tardó demasiado";
    case CG_ERR_INCONSISTENTE:  return "los giros de un lado salieron muy distintos entre sí (¿patinó el carro?); repite la calibración";
    case CG_ERR_AJUSTE:         return "el ajuste dio un valor fuera de lo razonable";
    default:                    return "";
    }
}
