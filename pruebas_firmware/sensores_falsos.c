/*
 * sensores_falsos.c — Ver sensores_falsos.h.
 */

#include <string.h>
#include "sensores_falsos.h"
#include "hw_sensores.h"

uint64_t falso_us;

int  falso_imu_presente;
int  falso_imu_quien;
int  falso_imu_cuentas;
int  falso_imu_ruido;
int  falso_imu_fallos;
int  falso_imu_ignora_config;
int  falso_imu_congelado;
int  falso_imu_loco;
int  falso_imu_lecturas_z;

int  falso_tof_presente;
int  falso_tof_mm;
int  falso_tof_modo;
int  falso_tof_lecturas;

int  falso_i2c_transacciones;

static uint8_t  regs[128];
static uint64_t tof_ultima_us;
static uint32_t semilla;

void falso_sensores_reiniciar(void)
{
    falso_us = 1000000u;                    /* se empieza a 1 s para no tener cero en ningún reloj */
    falso_imu_presente = 1;
    falso_imu_quien = 0x68;
    falso_imu_cuentas = 0;
    falso_imu_ruido = 2;                    /* un giroscopio de verdad siempre tiene algo de ruido (si no, parece congelado) */
    falso_imu_fallos = 0;
    falso_imu_ignora_config = 0;
    falso_imu_congelado = 0;
    falso_imu_loco = 0;
    falso_imu_lecturas_z = 0;
    falso_tof_presente = 1;
    falso_tof_mm = 8190;
    falso_tof_modo = 0;
    falso_tof_lecturas = 0;
    falso_i2c_transacciones = 0;
    memset(regs, 0, sizeof regs);
    tof_ultima_us = falso_us;
    semilla = 12345u;
}

static int ruido_aleatorio(int amplitud)
{
    if (amplitud <= 0) {
        return 0;
    }
    semilla = semilla * 1664525u + 1013904223u;
    return (int)((semilla >> 16) % (uint32_t)(2 * amplitud + 1)) - amplitud;
}

uint32_t hw_micros(void)
{
    return (uint32_t)falso_us;
}

void hw_demora_ms(uint32_t ms)
{
    falso_us += (uint64_t)ms * 1000u;
}

int hw_i2c_leer(uint8_t dir, uint8_t reg, uint8_t *datos, int n)
{
    int i;

    falso_i2c_transacciones++;
    if (dir != 0x68 || !falso_imu_presente) {
        return 2;
    }
    if (reg == 0x47 && n == 2) {
        int valor;

        falso_imu_lecturas_z++;
        if (falso_imu_fallos != 0) {
            if (falso_imu_fallos > 0) {
                falso_imu_fallos--;
            }
            return 5;
        }
        if (falso_imu_loco) {
            valor = -32768;
        } else if (falso_imu_congelado) {
            valor = falso_imu_cuentas;
        } else {
            valor = falso_imu_cuentas + ruido_aleatorio(falso_imu_ruido);
        }
        if (valor > 32767) valor = 32767;
        if (valor < -32768) valor = -32768;
        datos[0] = (uint8_t)(((uint16_t)(int16_t)valor) >> 8);
        datos[1] = (uint8_t)(((uint16_t)(int16_t)valor) & 0xFF);
        return 0;
    }
    for (i = 0; i < n; i++) {
        uint8_t r = (uint8_t)(reg + i);

        datos[i] = (r == 0x75) ? (uint8_t)falso_imu_quien : regs[r & 0x7F];
    }
    return 0;
}

int hw_i2c_escribir(uint8_t dir, uint8_t reg, const uint8_t *datos, int n)
{
    int i;

    falso_i2c_transacciones++;
    if (dir != 0x68 || !falso_imu_presente) {
        return 2;
    }
    for (i = 0; i < n; i++) {
        uint8_t r = (uint8_t)((reg + i) & 0x7F);

        if (falso_imu_ignora_config && (r == 0x19 || r == 0x1A || r == 0x1B)) {
            continue;
        }
        regs[r] = datos[i];
    }
    return 0;
}

int hw_i2c_presente(uint8_t dir)
{
    return (dir == 0x68 && falso_imu_presente) || (dir == 0x29 && falso_tof_presente);
}

int hw_tof_iniciar(void)
{
    tof_ultima_us = falso_us;
    return falso_tof_presente ? 1 : 0;
}

int hw_tof_listo(void)
{
    falso_i2c_transacciones++;
    if (falso_tof_modo == 1) {
        return -1;
    }
    if (falso_tof_modo == 2) {
        return 0;
    }
    return (falso_us - tof_ultima_us) >= 33000u ? 1 : 0;
}

int hw_tof_leer(int *mm)
{
    falso_i2c_transacciones++;
    if (falso_tof_modo == 1) {
        return 3;
    }
    *mm = falso_tof_mm;
    tof_ultima_us = falso_us;
    falso_tof_lecturas++;
    return 0;
}
