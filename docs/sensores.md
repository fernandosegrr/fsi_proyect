# Sensores: giroscopio MPU6050 y láser VL53L0X (J11)

## Estado (17 oct 2026)

- **Código y pruebas de PC: hechos** (Fase A). `imu.c`, `tof.c`, `giro.c`, `sensores.c` son C puro; el `.ino` solo pone el bus I2C.
- **Hardware: NO verificado.** Al cargar el firmware al DevKit (COM6) el escaneo del bus mostró:
  - **MPU6050 (0x68): nunca contesta** (0 de 6 arranques; tampoco 0x69).
  - **VL53L0X (0x29): contesta a ratos.** En 6 arranques seguidos: 1 vez no apareció nada en el escaneo, 4 veces apareció y `init()` de Pololu funcionó 2 veces y falló 2 (aparece pero no inicia).
- Por eso **no se midieron** el sesgo, la deriva, el tiempo de `loop()` ni las lecturas del láser (los pasos siguientes de A4 quedan bloqueados hasta arreglar el cableado). No se inventa ningún número.

### Diagnóstico

1. Un láser que aparece y desaparece entre arranques y un MPU que nunca aparece apuntan a **conexión eléctrica**, no a software: el mismo código inicia bien en las pruebas de PC y el bus I2C sí funciona (el láser contesta).
2. Sospechas, de más a menos probable:
   - **GY-521 sin alimentación** (VCC o GND sin soldar o en falso) pero con SDA/SCL conectados: un módulo apagado "arrastra" las líneas por sus diodos de protección y vuelve inestable a los demás. Explicaría los dos síntomas a la vez.
   - Soldadura fría o cable flojo en SDA o SCL del GY-521 (o del láser).
   - SDA y SCL cruzados en el GY-521 (el láser funciona porque va bien).
   - Pines de la tira del GY-521 sin soldar (suele venir suelta y hay que soldarla).
3. Pruebas que se piden al dueño del carro (ver el informe): foto del cableado; desconectar SDA y SCL del GY-521 y repetir `estado` (si el láser queda 5 de 5, el problema es el GY-521); medir con multímetro 3.3 V entre VCC y GND del GY-521.

## Cableado y pines

| Señal | GPIO del DevKit | GY-521 (MPU6050) | VL53LXX-V2 |
|---|---|---|---|
| SDA | 21 | SDA | SDA |
| SCL | 22 | SCL | SCL |
| 3V3 | 3V3 | VCC | VIN |
| GND | GND | GND | GND |

Direcciones: MPU6050 = 0x68 (AD0 a GND; si AD0 va a 3V3 sería 0x69), VL53L0X = 0x29. Los dos módulos traen sus pull-ups. Pines ya usados: 27, 26, 25, 33 (motores) y 2 (keep-alive); 21 y 22 estaban libres.

## Montaje

Giroscopio plano sobre la caja de pilas, flecha X al frente, eje Z hacia arriba; **girar a la izquierda = ángulo positivo**. Si la prueba física da el signo contrario: `IMU_SIGNO` en `config.h`. Láser al frente, a 6-8 cm del piso, inclinado 5-8° hacia arriba.

## Parámetros (todos en `firmware_carro/config.h`)

MPU6050: ±500 °/s (65.5 cuentas por °/s), filtro de ~44 Hz, 100 muestras/s, solo el eje Z; zona muerta 0.15 °/s; sesgo = promedio de 250 muestras, aceptado con desviación ≤ 12 cuentas y sesgo ≤ 1000 cuentas, 3 intentos; corrección lenta del sesgo (ZUPT) con el carro quieto más de 400 ms. Acepta WHO_AM_I 0x68, 0x70 y 0x72 y registra cuál leyó.
Giro: `GIRAR:<grados>` de 5 a 200, tiempo máximo propio 500 ms + 15 ms por grado, anticipación inicial 20 ms que se corrige sola por lado (tope 150 ms), 200 ms de asentamiento para informar el ángulo final.
Láser: una lectura cada ~33 ms sin bloquear `loop()`; 8190, 65535 o más de 1200 mm = libre; parada de emergencia con 2 lecturas válidas seguidas menores de 120 mm, solo en perfil AUTO y solo avanzando.

## Mensajes (WebSocket y, los de consulta, también por USB)

`CAPS?`, `IMU?`, `TOF?`, `SENS:<ángulo x10>,<mm>,<banderas>` (cada 100 ms con cliente y perfil AUTO), `GIRAR:<grados>` → `GIRO_OK` / `GIRO_TIMEOUT` / `GIRO_ERROR`, `IMU_CAL`, `IMU_CERO`, `TOF_PARO:<mm>`, `LOOP?`, `ANT?`. Solo por USB: `SENS_SERIE:1`, `I2C?` (escaneo del bus) y `REG:<dir>:<reg>` (lee un registro). **Por USB nunca se mueven los motores** (`GIRAR` se rechaza con `NAK_SERIE`). Detalle de cada mensaje en `firmware_carro/sensores.h` y `giro.h`.

## Pruebas de PC (Fase A)

`pruebas_firmware`: `make test` = `pruebas_carro` 152 (149 de antes + 3 del estrés con GIRAR), `pruebas_escalonado` 15, `pruebas_camara` 34 y `pruebas_sensores` 187. Resultados con sensores y motor SIMULADOS (no son medidas del carro real):
- 90 °/s durante 1 s → 90° ±0.5; 60 s quieto con ruido → deriva menor de 1°; un sesgo mal estimado dentro de la zona muerta no deriva, uno de 0.46 °/s sin ZUPT sí (~27° en 60 s: por eso existe `IMU_CAL`).
- Giro por ángulo contra un modelo de motor (160 °/s, inercia 50 ms): el primer giro de 90° pasa unos 6°; con la anticipación aprendida queda a ±3° en 9 de 10 giros (carro que varía ±10 % velocidad y ±25 % inercia).
- Láser: pico suelto no frena, 2 lecturas seguidas menores de 120 mm frenan en ~50 ms, no frena en MANUAL, en reversa, parado ni en giro; con el sensor caído el carro sigue como antes.
- Una vuelta de `loop()` hace como mucho 3 transacciones del bus.
- Estrés de 100 000 comandos con 2 hilos y `GIRAR` mezclado con STOP: 0 STOP perdidos (cuenta exacta de paradas: STOP + giros terminados solos + watchdogs).
- **ThreadSanitizer no se pudo correr**: esta PC (Windows, MinGW) no tiene `libtsan` ni WSL. Pendiente correrlo en Linux.

## Errores típicos

- `0x68` no aparece: GY-521 sin VCC/GND, SDA/SCL cruzados o sin soldar; AD0 a 3V3 lo mueve a 0x69.
- El láser aparece a ratos: cable flojo, soldadura fría, o el otro módulo arrastrando el bus.
- `WHO_AM_I=0x00` en el arranque: el bus no contestó (ver arriba). Otro valor distinto de 0x68/0x70/0x72: chip distinto.
- `IMU: MAL (motivo 3)`: el clon no guarda los registros de configuración.
- `IMU: MAL (motivo 4)`: el carro se movió durante los 2.5 s de calibración del sesgo.
- Ángulo que crece solo con el carro quieto: recalibrar con `IMU_CAL` (carro quieto).

## Licencia

Láser: librería VL53L0X de Pololu 1.3.1 (MIT), copia de la licencia en `docs/licencias/VL53L0X-Pololu-MIT.txt`. El MPU6050 va con registros directos, sin librería.
