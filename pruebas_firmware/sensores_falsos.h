/*
 * sensores_falsos.h — Giroscopio MPU6050 y láser VL53L0X FALSOS para las pruebas de PC.
 *
 * Implementan las funciones hw_... de hw_sensores.h. Los programas de prueba mueven el reloj de microsegundos (falso_us) y meten las lecturas que quieren:
 *   - el giroscopio devuelve el valor "falso_imu_cuentas" (más ruido si se pide) en el registro GYRO_ZOUT;
 *   - el láser devuelve "falso_tof_mm" cada vez que pasaron 33 ms desde la lectura anterior.
 * Se pueden simular fallos del bus, un chip de otro tipo, un clon que ignora la configuración, un sensor congelado o lecturas locas.
 */
#ifndef SENSORES_FALSOS_H
#define SENSORES_FALSOS_H

#include <stdint.h>

/* Reloj de microsegundos de los sensores falsos (lo mueven las pruebas; hw_demora_ms también lo avanza). */
extern uint64_t falso_us;

/* Giroscopio. */
extern int  falso_imu_presente;          /* 0 = el bus no contesta */
extern int  falso_imu_quien;             /* valor de WHO_AM_I */
extern int  falso_imu_cuentas;           /* lectura limpia del eje Z (cuentas, 65.5 por °/s) */
extern int  falso_imu_ruido;             /* amplitud del ruido (± cuentas) */
extern int  falso_imu_fallos;            /* cuántas lecturas del eje Z siguientes fallan (-1 = todas) */
extern int  falso_imu_ignora_config;     /* 1 = un clon que no guarda los registros de configuración */
extern int  falso_imu_congelado;         /* 1 = devuelve siempre exactamente el mismo valor, sin ruido */
extern int  falso_imu_loco;              /* 1 = devuelve -32768 */
extern int  falso_imu_lecturas_z;        /* cuántas veces se leyó el eje Z */

/* Láser. */
extern int  falso_tof_presente;          /* 0 = hw_tof_iniciar falla */
extern int  falso_tof_mm;                /* lo que devuelve (8190 y 65535 = sin eco) */
extern int  falso_tof_modo;              /* 0 normal, 1 el bus falla, 2 nunca hay medida lista */
extern int  falso_tof_lecturas;          /* cuántas medidas se leyeron */

/* Contador de todas las transacciones del bus (para ver cuánto se usa). */
extern int  falso_i2c_transacciones;

/* Memoria no volátil falsa de la anticipación del giro: 4 ranuras (-1 = nada) y cuántas escrituras hubo. */
extern int  falso_ant[4];
extern int  falso_ant_escrituras;

/* Deja todo como recién encendido (memoria borrada de fábrica) y con el reloj en cero. */
void falso_sensores_reiniciar(void);

#endif /* SENSORES_FALSOS_H */
