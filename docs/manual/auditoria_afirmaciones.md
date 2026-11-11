# Auditoría de afirmaciones, cifras y resolución de contradicciones (Manual v1.1)

Fecha: `2026-11-10`  
Rama: `docs/manual-final`  
Propósito: verificar contra el código fuente, los registros CSV y los reportes técnicos del repositorio cada cifra, versión, parámetro y afirmación técnica que aparece en el **Manual Técnico — Versión final 1.1**, así como dejar constancia explícita de cómo se resolvieron las contradicciones detectadas en documentos previos.

---

## 1. Resolución de contradicciones históricas de la documentación

### 1.1 Baterías portátiles USB (*powerbanks*) y alimentación
- **Contradicción previa:** en versiones anteriores de la documentación convivían dos afirmaciones incompatibles: una decía que una sola *powerbank* alimentaba tanto la ESP32 DevKit como la cámara Freenove, y otra decía que se desconocía si compartían batería.
- **Evidencia real:** el montaje físico utiliza **tres fuentes de energía independientes**:
  1. **Powerbank USB #1:** alimenta la placa **ESP32 DevKit** (y desde los pines de `3.3 V` y `GND` de la DevKit se alimentan los sensores I2C `MPU6050` y `VL53L0X`).
  2. **Powerbank USB #2 (propia de la cámara):** alimenta exclusivamente la placa **Freenove ESP32-WROVER con cámara GC0308**. Su único acoplamiento con el resto del carro es inalámbrico (WiFi).
  3. **Portapilas de 4×AA (con interruptor físico):** alimenta la etapa de potencia del puente H **TC1508A** y los **4 motores TT**. Comparte tierra común (`GND`) con el módulo TC1508A y con el pin `GND` de la ESP32 DevKit para referenciar las señales PWM (`GPIO 27, 26, 25, 33`).
- **Resolución en el manual v1.1:** unificado en texto (Capítulos 2, 3, 11 y 15) y en diagramas (`Figura 1` y `Figura 2`).

### 1.2 Resolución de operación de la cámara (`HQVGA` vs `QVGA`)
- **Contradicción previa:** algunos textos introductorios mencionaban `QVGA (320×240)` como modo por defecto de la cámara, mientras que el firmware de cámara operaba en `HQVGA (240×176)`.
- **Evidencia en código (`camara_prueba/camara_prueba.ino` y `docs/camara.md`):** el sensor `GC0308` se inicializa en `FRAMESIZE_HQVGA` (`240×176` píxeles, formato nativo `PIXFORMAT_RGB565`, reloj `XCLK` a `20 MHz`, compresión JPEG por software con `JPEGENC` a calidad `50`). El barrido experimental de 100 cuadros demostró que `HQVGA` a `20 MHz` entrega `12.4 fps` y una edad media de cuadro de `137 ms` (p95 `165 ms`), frente a `8.8 fps` y `252 ms` (p95 `317 ms`) de `QVGA`.
- **Resolución en el manual v1.1:** se documenta `HQVGA (240×176)` como la resolución operativa del sistema y se explica con los datos del *benchmark* por qué se descartó `QVGA`.

### 1.3 Tiempo de ciclo `loop()` del firmware y umbral de `5 ms`
- **Contradicción previa:** comentarios antiguos sugerían que el bucle principal del firmware tardaba siempre menos de `1 ms`, pero las mediciones reales con sensores activos mostraban picos mayores a `5 ms`.
- **Evidencia en código y mediciones (`firmware_carro/sensores.c`, `docs/precision/calibracion_j14.txt`):** el contador ` lentos` de `LOOP?` incrementa cada vez que una iteración de `sensores_actualizar` supera `5,000 us` (`5 ms`), lo cual ocurre de manera normal cuando coincide una lectura bloqueante o transacción I2C del sensor láser `VL53L0X` (`~33 ms` de presupuesto de medición configurado). En las pruebas físicas J14 se registraron promedios de `~690 us` con máximos de `28,399 us` a `41,111 us`.
- **Resolución en el manual v1.1:** se explica en el Capítulo 4 y en el Capítulo 12 que `lentos > 0` refleja las lecturas I2C del `VL53L0X` y no una falla del microcontrolador, razón por la cual la integración del giroscopio usa el $\Delta t$ real en microsegundos (`micros()`) con tope de seguridad `IMU_DT_MAX_US = 100,000 us`.

### 1.4 Consumo de corriente y circuito `GPIO 2`
- **Estado real (`firmware_carro/config.h`, `docs/limites_conocidos.md`):** no se han realizado mediciones con amperímetro del consumo de corriente de los motores TT, de la ESP32 DevKit ni de la cámara Freenove. Asimismo, aunque el firmware define `PIN_CARGA_POWERBANK = 2` (pulso de `200 ms` cada `8,000 ms`), el resistor externo de carga en `GPIO 2` no ha sido verificado ni medido en el montaje actual.
- **Resolución en el manual v1.1:** ambos puntos se declaran estrictamente como **`NO MEDIDO`** en los Capítulos 3, 4, 12, 13 y Anexo 16.4, sin inventar cifras de corriente ni de autonomía en miliamperios-hora.

### 1.5 Error reportado en la Ruta en L histórica (J14) vs estado en J16
- **Contexto:** en `docs/precision/calibracion_j14.txt` (antes de J16), la corrida física de la ruta en L (`(0,0) -> (0,600) -> (600,600) mm`) reportó un error estimado por el navegador de `dx = 9 mm, dy = 3 mm, err = 9 mm`, pero esa cifra comparaba la pose integrada contra el punto intermedio `(0, 200) mm` donde la corrida terminó en `AYUDA` tras un esquive, no contra una medición con cinta métrica en `(600, 600) mm`. Además, tras incorporar los cambios de J16 (`RUMBO` en rectas, pose en décimas de grado `rumbo_x10`, margen `150 mm`), la ruta en L **no se volvió a correr en el carro**.
- **Resolución en el manual v1.1:** se distingue con total transparencia: la ruta en L con el cierre J16 está **`SIMULADA`** (1,000 corridas Monte Carlo: mediana `36 mm`, p95 `65 mm`, 100 % de éxito en simulación) y **`NO MEDIDA` en el carro**; y cuando se menciona el antecedente histórico J14 se aclara qué midió exactamente aquel registro.

---

## 2. Tabla maestra de afirmaciones y cifras verificadas

| Concepto / Parámetro | Valor verificado | Estado | Fuente exacta en el repositorio |
|---|---|---|---|
| **Versión de APK validada en el carro** | `APP:0.4-6d1b213` (`dist/carro_fsi_0.4-6d1b213.apk`, `14,881,402` bytes) | `VALIDADO EN EL CARRO` | `app_android/app/build.gradle.kts`, `dist/SHA256SUMS.txt` |
| **SHA-256 de la APK validada** | `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F` | `VERIFICADO` | `dist/SHA256SUMS.txt`, `scripts/verificar_version.py` |
| **Versión de firmware validada en el carro** | `FW:6d1b213-20261110` (`1,034,112` bytes locales) | `VALIDADO EN EL CARRO` | `dist/FIRMWARE_LOCAL.txt`, `docs/estado_20261110.md` |
| **SHA-256 del firmware compilado (local)** | `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC` | `VERIFICADO` | `dist/FIRMWARE_LOCAL.txt` |
| **Tag histórico conservado** | `v1.0` en `eb88151f438048190c26c9ed9ba26fbb6ad9424e` | `VERIFICADO` | `git rev-list -n 1 v1.0` |
| **Pruebas del núcleo en C (`nucleo_c`)** | `22` programas, `1,655` casos, `0` fallos | `VERIFICADO` | `make -C nucleo_c test` (`nucleo_c/Makefile`) |
| **Pruebas de lógica de firmware (`pruebas_firmware`)** | `5` programas, `540` casos, `0` fallos (+ `verificar_pagina.py`: 3/3 OK) | `VERIFICADO` | `make -C pruebas_firmware test` |
| **Pruebas del servidor falso (`probar_servidor_falso.py`)** | `54` casos, `0` fallos | `VERIFICADO` | `pruebas_firmware/probar_servidor_falso.py` |
| **Pruebas del verificador de versión (`probar_version.py`)** | `18` casos, `0` fallos | `VERIFICADO` | `scripts/probar_version.py` |
| **Esquive con reincorporación B6 en el carro (2026-11-10)** | `6` corridas: `0/6` toques de caja, `5/6` completas autónomas con error lateral aproximado entre `~0` y `~10 cm` (`~10, ~10, ~5, ~0, ~0 cm`), `1/6` en `AYUDA` en punto `4/10` sin toque y sin causa identificada | `VALIDADO EN EL CARRO` | `docs/pruebas_fisicas_20261110.csv`, `docs/estado_20261110.md`, `docs/reporte_J16.md` |
| **Monte Carlo J16 — Recta 150 cm (sin `RUMBO` vs con `RUMBO`)** | Mediana de error lateral final: `94 mm -> 8 mm` (p95: `150 mm -> 16 mm`; éxito: `68.5 % -> 100.0 %` en 1,000 corridas) | `SIMULADO` | `docs/precision/monte_carlo_1000.txt` |
| **Monte Carlo J16 — Ruta en L (con J16 completo)** | Mediana de error final: `36 mm` (p95: `65 mm`; éxito: `100.0 %` en 1,000 corridas) | `SIMULADO` | `docs/precision/monte_carlo_1000.txt` |
| **Monte Carlo J16 — Cuadrado 60 cm (con J16 + escala IMU ±1.5 %)** | Mediana de error final: `42 mm` (p95: `81 mm`; éxito: `98.4 %` en 1,000 corridas) | `SIMULADO` | `docs/precision/monte_carlo_1000.txt` |
| **Monte Carlo J16 — Esquive de caja a 60 cm (con J16 completo)** | `0` colisiones (`0.0 %`), éxito `99.9 %`, mediana de error final `38 mm` (p95 `72 mm`) en 1,000 corridas | `SIMULADO` | `docs/precision/monte_carlo_1000.txt` |
| **Pines PWM del puente H TC1508A** | Izquierdo: `GPIO 27` (`IN1`), `GPIO 26` (`IN2`); Derecho: `GPIO 25` (`IN3`), `GPIO 33` (`IN4`); `1000 Hz`, `8 bits` | `VERIFICADO` | `firmware_carro/config.h` |
| **Bus I2C en ESP32 DevKit** | `SDA = GPIO 21`, `SCL = GPIO 22`, `400 kHz`; `MPU6050 = 0x68`, `VL53L0X = 0x29` | `VERIFICADO` | `firmware_carro/config.h` |
| **Perfiles PWM y arranque** | `MANUAL`: `252` izq / `255` der; `AUTO`: `207` izq (`VALIDADO EN EL CARRO` en B6; balance en recta aislada `NO MEDIDO`) / `210` der; arranque `255` durante `250 ms` | `VERIFICADO` | `firmware_carro/config.h`, `firmware_carro/carro.c` |
| **Controlador PI de rumbo en rectas (`rumbo.c`)** | `RUMBO_KP_MIL_GRADO = 10` (`1.0 %/°`), `RUMBO_TI_MS = 1500 ms`, `RUMBO_MAX_MIL = 150` (`±15 %`), `RUMBO_BANDA_MUERTA_X10 = 2` (`0.2°`), `RUMBO_UMBRAL_RECTO_X10 = 15` (`1.5°`), `RUMBO_APRENDIDO_MAX_MIL = 120` (`±12 %`) | `VERIFICADO` | `firmware_carro/rumbo.h`, `firmware_carro/rumbo.c` |
| **Controlador de giro por giroscopio (`giro.c`)** | `GIRO_TOLERANCIA_X10 = 20` (`2.0°`), `GIRO_FRENO_GRADOS_X10 = 150` (`15.0°`), `GIRO_PWM_MIN = 175`, `GIRO_VENTANA_ESTABLE_MS = 80 ms`, `GIRO_LARGO_POR_GRADO = 1` (`+1 %` de umbral de freno por cada `100°` sobre `45°`) | `VERIFICADO` | `firmware_carro/giro.h`, `firmware_carro/giro.c` |
| **Factor de escala del giroscopio (`imu.c` / `calgiro.c`)** | `escala_ppm` en `[-50000, +50000]` (`±5 %`), persistido en NVS (`carro_imu` / `escala_ppm`), comando `IMU_ESCALA:<ppm>` | `VERIFICADO` | `firmware_carro/imu.h`, `firmware_carro/imu.c`, `nucleo_c/calgiro.c` |
| **Tiempos de vigilancia y seguridad** | Latido de mando `250 ms`; *watchdog* de comunicación `1000 ms` (emite `WD:<n>`); silencio de `SENS` en Android `1500 ms`; WDT de hardware `5000 ms`; freno láser `TOF_PARO` a `120 mm` (histéresis `150 mm`) | `VERIFICADO` | `firmware_carro/config.h`, `firmware_carro/carro.c`, `ModoSemiautonomo.kt` |
| **Parámetros geométricos de navegación y esquive** | Celda `200 mm`; desvío lateral `400 mm` (2 celdas); profundidad supuesta de obstáculo `250 mm`; margen longitudinal J16 `NAV_PASO_MARGEN_MM = 150 mm`; punto de mira `RUMBO` `400 mm` (tope `±12°`, suprime giros detenidos $\le 8^\circ$) | `VERIFICADO` | `nucleo_c/navegador.h`, `nucleo_c/navegador.c` |
| **Paquete UDP de celdas de cámara** | Puerto `4210`, `1,244` bytes (`20` bytes de cabecera `"CEL1"` + `408` celdas × `3` bytes RGB en rejilla `24×17`) | `VERIFICADO` | `nucleo_c/paquete_celdas.h`, `camara_prueba/camara_prueba.ino` |
