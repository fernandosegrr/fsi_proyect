# Sensores: giroscopio MPU6050 y láser VL53L0X (J11)

## Estado (17 oct 2026)

- **Código y pruebas de PC: hechos** (Fase A). `imu.c`, `tof.c`, `giro.c`, `sensores.c` son C puro; el `.ino` solo pone el bus I2C.
- **Hardware: los dos módulos funcionan** (comprobado por USB con el DevKit en COM6, tras corregir el cableado):
  - Escaneo del bus: `0x29` (láser) y `0x68` (giroscopio).
  - **MPU6050:** WHO_AM_I = `0x68` (original, no clon), sesgo medido −0.44 a −0.46 °/s (igual en 5 arranques), deriva del ángulo con el carro quieto 60 s: **0.0°** (repetida sin tocar nada; dos medidas anteriores, 0.8° y 25°, eran manos moviendo el carro: el ángulo se quedaba en -0.1° y saltaba de golpe).
  - **VL53L0X:** 8 de 8 arranques iniciaron bien (con 5 reintentos de `init()`; sin reintentos fallaba 1 de cada 3 aunque aparecía en el escaneo). Lecturas con algo delante: 66 a 77 mm, estables (sin comparar todavía con una regla).
  - **Tiempo de `loop()`: cumple.** En arranques limpios `LOOP?` informa un máximo de **1.8 ms** y 0 vueltas sobre 5 ms (también durante 60 s transmitiendo `SENS` por USB: 1.7 ms). El 92 ms que se vio una vez fue en un arranque donde se había corrido `PRUEBAI2C?`, un comando de diagnóstico que escanea 4 veces las 126 direcciones (nunca se usa en una ruta).
- Causas del problema de cableado que hubo: el cable SCL del GY-521 no llegaba al DevKit (con el láser desconectado, SCL no subía). Sirvió el comando `PINES?` (nivel eléctrico de SDA/SCL) y `PRUEBAI2C?` (escaneo cruzado y a 100/400 kHz).

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

`CAPS?`, `IMU?`, `TOF?`, `SENS:<ángulo x10>,<mm>,<banderas>` (cada 100 ms con cliente y perfil AUTO), `GIRAR:<grados>` → `GIRO_OK` / `GIRO_TIMEOUT` / `GIRO_ERROR`, `IMU_CAL`, `IMU_CERO`, `TOF_PARO:<mm>`, `LOOP?`, `ANT?`. Solo por USB: `SENS_SERIE:1`, `I2C?` (escaneo del bus), `REG:<dir>:<reg>` (lee un registro), `PINES?` y `PRUEBAI2C?` (diagnóstico de cableado). **Por USB nunca se mueven los motores** (`GIRAR` se rechaza con `NAK_SERIE`). Detalle de cada mensaje en `firmware_carro/sensores.h` y `giro.h`.

## Pruebas de PC (Fase A)

`pruebas_firmware`: `make test` = `pruebas_carro` 152 (149 de antes + 3 del estrés con GIRAR), `pruebas_escalonado` 15, `pruebas_camara` 34 y `pruebas_sensores` 187. Resultados con sensores y motor SIMULADOS (no son medidas del carro real):
- 90 °/s durante 1 s → 90° ±0.5; 60 s quieto con ruido → deriva menor de 1°; un sesgo mal estimado dentro de la zona muerta no deriva, uno de 0.46 °/s sin ZUPT sí (~27° en 60 s: por eso existe `IMU_CAL`).
- Giro por ángulo contra un modelo de motor (160 °/s, inercia 50 ms): el primer giro de 90° pasa unos 6°; con la anticipación aprendida queda a ±3° en 9 de 10 giros (carro que varía ±10 % velocidad y ±25 % inercia).
- Láser: pico suelto no frena, 2 lecturas seguidas menores de 120 mm frenan en ~50 ms, no frena en MANUAL, en reversa, parado ni en giro; con el sensor caído el carro sigue como antes.
- Una vuelta de `loop()` hace como mucho 3 transacciones del bus.
- Estrés de 100 000 comandos con 2 hilos y `GIRAR` mezclado con STOP: 0 STOP perdidos (cuenta exacta de paradas: STOP + giros terminados solos + watchdogs).
- **ThreadSanitizer**: esta PC (Windows, MinGW) no tiene `libtsan` ni WSL, así que se corre en Linux con GitHub Actions (`.github/workflows/sanitizadores.yml`): ThreadSanitizer sobre el estrés de 2 hilos con `GIRAR` mezclado con STOP (`carro.c` + `imu.c` + `tof.c` + `giro.c` + `sensores.c`) y AddressSanitizer + UBSan sobre `pruebas_sensores`, `pruebas_carro` y `pruebas_navegador`. Resultado del 18 oct: **sin errores en los tres** (https://github.com/fernandosegrr/fsi_proyect/actions/runs/39150275890).

## Errores típicos

- `0x68` no aparece: GY-521 sin VCC/GND, SDA/SCL cruzados o sin soldar; AD0 a 3V3 lo mueve a 0x69.
- El láser aparece a ratos: cable flojo, soldadura fría, o el otro módulo arrastrando el bus.
- `WHO_AM_I=0x00` en el arranque: el bus no contestó (ver arriba). Otro valor distinto de 0x68/0x70/0x72: chip distinto.
- `IMU: MAL (motivo 3)`: el clon no guarda los registros de configuración.
- `IMU: MAL (motivo 4)`: el carro se movió durante los 2.5 s de calibración del sesgo.
- Ángulo que crece solo con el carro quieto: recalibrar con `IMU_CAL` (carro quieto).

## Licencia

Láser: librería VL53L0X de Pololu 1.3.1 (MIT), copia de la licencia en `docs/licencias/VL53L0X-Pololu-MIT.txt`. El MPU6050 va con registros directos, sin librería.

## Fase B: app y navegador (J11, 18 oct)

- **Mensajes en la app** (`ControlLogica.kt`): al conectar manda `CAPS?` y `TOF_USO:<0|1>`; lee `CAPS`, `SENS`, `GIRO_OK / GIRO_TIMEOUT / GIRO_ERROR`, `TOF_PARO`, `IMU_CAL_OK / FALLO` e `IMU:`. Un firmware viejo no contesta `CAPS?`: la app lo toma como «sin sensores» y todo queda como antes. `girar(grados)` manda `GIRAR` una sola vez (no hay heartbeat; el watchdog de 1 s no corre durante el giro).
- **Pantalla Sensores** (Calibración → «Sensores (giroscopio y láser)»): interruptores «Usar giroscopio» y «Usar sensor de distancia» (guardados en `Ajustes`), estado en vivo, «Recalibrar giroscopio», «Poner el ángulo en cero» y prueba de giro de 90° por ángulo o por tiempo que registra en el log `SENS_DIAG prueba_giro ... medido_x10 ... ms`.
- **Navegador** (`nucleo_c/navegador.c`): giros por ángulo, `TOF_PARO` y confirmación de la cámara, con respaldos; ver `docs/navegador.md` («Giroscopio y sensor láser»).
- **Firmware:** se agregó `TOF_USO:<0|1>` (apaga/enciende el freno de emergencia con el interruptor de la app).
- **Servidor falso de la app:** `pruebas_firmware/servidor_falso.py` entiende `CAPS?`, `SENS`, `GIRAR`, `GIRO_*`, `TOF_PARO`, `IMU_CAL`, `IMU_CERO` y `TOF_USO` para probar las pantallas sin el carro.
- **Instalada y revisada en el Huawei (18 oct, con el servidor falso `--sensores` por `adb reverse`; sin cámara ni carro real):** `probar_pantallas.py` abre todas las pantallas sin errores (incluida `sensores`). En la pantalla Sensores se comprobó: chip «Los dos funcionan», giroscopio y láser en vivo, interruptores, y la prueba de giro de 90° (por ángulo: `GIRAR:90` → `GIRO_OK:90:909:735` → «medido 90.9° en 735 ms» en pantalla y en el log `SENS_DIAG prueba_giro`; por tiempo: 918 ms de LEFT y STOP). Se corrigieron dos textos que se cortaban (título y botones). El servidor falso no mueve el ángulo con LEFT, así que la prueba por tiempo muestra 0° ahí (con el carro real sí lo mide). Lo que falta es probarla con el carro.
- **Monte Carlo (simulación, no el carro real):** 400 rutas en L, sin giroscopio error medio 153 mm / p95 378 mm; con giroscopio 45 mm / p95 116 mm. Detalle y límites en `docs/limites_conocidos.md` §11.

## J12: ajustes después de la primera prueba real (18 oct)

Después de probar los giros con el carro real salieron cuatro ajustes. Aquí van por fase.

### 1. Anticipación del giro: por sentido, por perfil, con mediana y en memoria

- **Problema:** el giro a la derecha se pasaba ~2.7° y la anticipación no convergía (una sola, corregida a medias con cada giro y con ruido).
- **Ahora** (`firmware_carro/giro.c`): hay **4 anticipaciones**, una por sentido (izquierda/derecha) y por perfil (AUTO/MANUAL). Cada una es la **mediana de las últimas 5 lecturas** de «cuántos ms de anticipación habrían dado justo el ángulo»: la que se usó en ese giro más el error final (con signo) dividido entre la velocidad. Solo aprenden los giros bien terminados (`GIRO_OK`) de **30° o más** (con uno chico la velocidad no llega a su valor normal) y con más de 15 °/s. El primer giro ya corrige casi todo (con 1 lectura la mediana es esa lectura) y un giro raro (un resbalón) no la mueve (mediana).
- **Memoria no volátil:** se guardan cuando cambian **3 ms o más** (así no se gasta la memoria: 3 escrituras en 28 giros en la simulación) y se leen al arrancar. Tope `GIRO_ANTICIPACION_MAX_MS` = 150 ms.
- **Mensajes:** `ANT?` → `ANT:<AUTO izq>,<AUTO der>,<MANUAL izq>,<MANUAL der>` en ms; `ANT_BORRAR` → `ACK_ANT_BORRAR` (las 4 vuelven a 20 ms y se borra la memoria).
- **Pruebas** (`pruebas_sensores.c`, modelo de motor ASIMÉTRICO: izquierda 160 °/s con inercia de 50 ms; derecha 130 °/s con inercia de 95 ms, y ruido de ±5 % en la velocidad y ±15 % en la inercia): tras 4 giros por lado aprende 52 ms a la izquierda y 91 ms a la derecha; después **10 de 10 giros a ±3° a la izquierda y 9 de 10 a la derecha** (objetivo: 9 de 10 por sentido). También: MANUAL no se toca con giros en AUTO y al revés, sobrevive a un reinicio, `ANT_BORRAR` deja todo en 20 ms y vacía la memoria, un giro disparatado casi no la mueve, y un giro de menos de 30° no la cambia.
- **Límite honesto:** el giroscopio informa el ángulo 200 ms después de frenar; si el carro todavía sigue girando después de eso (mucha inercia) esa parte final no se ve y el ángulo real queda un poco por encima (≈1.5° en la simulación con inercia de 95 ms). Con el carro real la prueba del dueño no vio diferencia entre el giroscopio y la cinta.

### 2. Giro duplicado: qué hace el navegador si se pierde el mensaje de un giro

- **Problema (J11):** si el firmware giraba pero su `GIRO_OK` no llegaba, el navegador repetía el giro por tiempo y el carro giraba dos veces.
- **Ahora** (`nucleo_c/navegador.c`): al empezar un giro por ángulo guarda el ángulo del giroscopio (lo último que dijo `SENS`, que ahora también lleva el ángulo al navegador). Si pasa la espera máxima (500 ms + 15 ms por grado + 1.5 s) sin `GIRO_OK` ni `GIRO_TIMEOUT`, **pide `IMU?`** (la app lo manda con `navegador_tomar_consulta_imu`) y compara el ángulo actual con el del inicio:
  - si ya giró **al menos el 80 %** de lo pedido, **NO repite**: usa lo medido como ángulo real (`SENS_DIAG giro mensaje_perdido fuente=IMU? ... NO se repite`);
  - si giró menos, la pose parte de lo que ya giró y repite **por tiempo solo lo que falta**;
  - si `IMU?` no contesta en 1 s se usa el último `SENS` si es reciente (`fuente=ultimo_SENS`);
  - si no hay ángulo del inicio (no llegaba `SENS`) o no hay ninguna respuesta, cae al método por tiempo como antes (`motivo=sin_respuesta_ni_angulo_inicial` o `sin_respuesta_ni_IMU`); esa es la única forma de que se duplique, y solo cuando no hay ningún dato del giroscopio.
- **Arreglo de un fallo mío de J11 que apareció al revisarlo:** al caer a tiempo en medio de una esquivada se ponía en cero el contador del paso de la ruta interrumpido (`ms_paso`). Ahora solo se toca el contador del movimiento en curso.
- **Pruebas del simulador** (`pruebas_navegador.c`, mensaje perdido): (a) el carro giró completo y el mensaje se perdió: pregunta `IMU?`, no repite, el cuadrado termina a 9 mm del inicio (19 mm sin perder el mensaje); (b) el carro giró solo el 40 %: repite solo lo que falta y termina a 71 mm; (c) `IMU?` sin respuesta pero con `SENS`: usa el último `SENS`; (d) sin `SENS` ni `IMU?`: cae a tiempo y la ruta termina.
