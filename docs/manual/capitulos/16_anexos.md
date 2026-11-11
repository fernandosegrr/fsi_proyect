# 16. Anexos

Este capítulo reúne las referencias de consulta rápida del proyecto: (16.1) el glosario de términos técnicos, (16.2) el resumen completo de comandos del protocolo y de pines de hardware, (16.3) la tabla de correspondencia entre los capítulos de este manual y las rutas exactas de archivos en el repositorio, y (16.4) la lista consolidada de mediciones físicas y tareas pendientes.

## 16.1 Glosario técnico

- **Anti-*windup* (saturación de integrador):** técnica de control que impide que el término integral de un controlador `PI` siga acumulando error cuando el actuador ya alcanzó su límite físico (`±150` milésimas en `rumbo.c`).
- **Buzón de último cuadro (*latest-frame mailbox*):** estructura de memoria compartida en la cámara `Freenove` que conserva únicamente el cuadro más reciente capturado por el sensor, descartando cuadros anteriores no consumidos para evitar latencia acumulada.
- **Corredor de perspectiva:** proyección trapezoidal sobre la imagen (`corredor.c`) que delimita la franja exacta del piso por la que pasarán las ruedas del carro (`150 mm` de ancho físico más `50 mm` de margen por lado).
- **Cuerda de pivote ($2R\sin(\Delta\theta/2)$):** desplazamiento lineal que experimenta un punto del chasis situado a una distancia $R$ del centro instantáneo de rotación cuando el vehículo gira un ángulo $\Delta\theta$.
- **Décimas de grado (`rumbo_x10` / `ang_x10`):** unidad angular entera empleada en la telemetría `SENS`, en el controlador `rumbo.c` y en el estimador de pose de `navegador.c` (`10` unidades equivalen a `1.0°`), que evita el uso de punto flotante en el microcontrolador sin perder resolución por truncamiento.
- **Escala del giroscopio en ppm (`escala_ppm` / `IMU_ESCALA`):** corrección multiplicativa en partes por millón (`10,000 ppm = 1.0 %`) almacenada en la memoria `NVS` de la `ESP32` para compensar el error de sensibilidad de fábrica del `MPU6050`.
- **Histéresis de desbloqueo (`TOF_LIBRE_MM = 150 mm`):** diferencia deliberada entre el umbral que activa el freno láser (`120 mm`) y el umbral que permite volver a avanzar (`150 mm`), necesaria para evitar oscilaciones de encendido/apagado frente a un obstáculo cercano.
- **JNI (*Java Native Interface*):** interfaz binaria (`puente.c`) que permite a la aplicación Android en Kotlin invocar directamente las funciones compiladas en `C99` de `nucleo_c` protegidas por `candado_vision`.
- **NVS (*Non-Volatile Storage*):** partición de memoria flash interna de la `ESP32` donde el firmware conserva parámetros entre apagados, como `escala_ppm` del giroscopio.
- **Punto de mira (*lookahead* de `400 mm`):** punto objetivo situado `40 cm` por delante sobre el segmento ideal de la ruta hacia el cual `navegador.c` orienta continuamente la referencia `RUMBO:<décimas>`.
- **Sesgo aprendido (`aprendido_mil`):** valor de compensación diferencial de `PWM` (acotado a `±120` milésimas) que el controlador `PI` de `rumbo.c` transfiere al finalizar un tramo recto para arrancar el siguiente tramo ya compensado.
- **ToF (*Time of Flight*, tiempo de vuelo):** principio físico del sensor láser `VL53L0X`, que determina la distancia en milímetros midiendo el tiempo que tarda un pulso de luz infrarroja (`940 nm`) en ir hasta el objeto y regresar al fotodiodo.
- **Watchdog de comunicación (`1,000 ms`):** temporizador de seguridad en `carro.c` que apaga los motores y emite `WD:<n>` si transcurre un segundo sin recibir órdenes de movimiento válidas.

## 16.2 Referencia rápida de comandos y pines

### Resumen compacto de comandos del protocolo (`ws://192.168.4.1/ws` y serie `115200`)

| Categoría | Comando | Respuesta normal | Descripción rápida |
|---|---|---|---|
| **Tracción** | `FORWARD`, `BACKWARD`, `LEFT`, `RIGHT` | `OK` *(o `BLOQUEADO_TOF`)* | Mueve el carro y renueva el *watchdog* de `1000 ms` (latido cada `250 ms`). |
| **Parada** | `STOP` / `EBRAKE` | `OK` / `OK_EBRAKE` | Paro libre (`LOW/LOW`) o freno activo (`HIGH/HIGH`) con prioridad en cola. |
| **Perfil** | `MODO_AUTO` / `MODO_MANUAL` / `ESTADO` | `ACK_AUTO` / `ACK_MANUAL` / `AUTO`+`CAPS` | Selecciona `PWM` `207/210` (`AUTO`) o `252/255` (`MANUAL`) y reporta capacidades. |
| **Enlace y versión** | `PING` / `VERSION?` / `LOOP?` | `PONG` / `FW:6d1b213-20261110` / `LOOP:...` | Sonda de red (`800 ms`), revisión compilada y tiempos de bucle en microsegundos. |
| **Sensores e IMU** | `SENS?` / `CAL_IMU` | `SENS:<ang_x10>,<mm>,<flags>` / `CAL_OK` | Telemetría en décimas de grado y milímetros (`10 Hz` en `AUTO`) y calibración en reposo. |
| **Giro y escala** | `GIRO:<g>` / `IMU_ESCALA:<ppm>` / `IMU_ESCALA?` | `GIRO_FIN:<a>` / `IMU_ESCALA_OK:<ppm>` | Giro cerrado en `[-360,360]` (`±2.0°`) y factor de escala en `[-50000,50000] ppm` en `NVS`. |
| **Rumbo en rectas** | `RUMBO:ON` / `RUMBO:OFF` / `RUMBO:<d>` / `RUMBO?` | `RUMBO_OK:...` / `RUMBO_REF:<d>` / `RUMBO:...` | Activa/desactiva el lazo `PI` en rectas, fija referencia en décimas de grado o consulta estado. |

### Resumen de pines en la placa `ESP32 DevKit` y direcciones de red

| Recurso | Pin / Dirección | Función asignada | Parámetros clave |
|---|---|---|---|
| **Motor izquierdo (`IN1`, `IN2`)** | `GPIO 27`, `GPIO 26` | Canal 1 del puente H `TC1508A` | `PWM` `1000 Hz`, 8 bits (`MANUAL=252`, `AUTO=207`). |
| **Motor derecho (`IN3`, `IN4`)** | `GPIO 25`, `GPIO 33` | Canal 2 del puente H `TC1508A` | `PWM` `1000 Hz`, 8 bits (`MANUAL=255`, `AUTO=210`). |
| **Bus `I2C` (`SDA`, `SCL`)** | `GPIO 21`, `GPIO 22` | `MPU6050` (`0x68`) y `VL53L0X` (`0x29`) | Reloj `400 kHz`, alimentación `3.3 V`. |
| **Carga de *powerbank*** | `GPIO 2` | Pulso de `200 ms` cada `8,000 ms` | Declarado en `config.h`; efecto físico `NO MEDIDO`. |
| **Red `CarroFSI` (carro / cámara)** | `192.168.4.1` / `192.168.4.2` | Control `:80/ws`; video `:81/stream`, `:80/raw`, `UDP :4210` | Un cliente de video simultáneo; paquetes `CEL1` de `1,244 B`. |

## 16.3 Correspondencia entre capítulos y archivos del repositorio

Esta tabla concentra todas las rutas de archivos del repositorio correspondientes a cada capítulo del manual:

| Capítulos | Subsistema | Rutas exactas de archivos en el repositorio |
|---|---|---|
| **Cap. 1 y 2** | Documentación general, arquitectura y seguridad | `README.md`, `LICENSE`, `docs/README.md`, `docs/entrega.md`, `docs/arquitectura.md`, `docs/seguridad_fisica.md`, `docs/estado_20261110.md`, `docs/limites_conocidos.md`, `docs/reporte_J16.md`. |
| **Cap. 3 y 4** | Hardware y firmware del carro (`ESP32 DevKit`) | `firmware_carro/firmware_carro.ino`, `firmware_carro/config.h`, `firmware_carro/secretos.ejemplo.h`, `firmware_carro/pagina.h`, `firmware_carro/carro.h`, `firmware_carro/carro.c`, `firmware_carro/sensores.h`, `firmware_carro/sensores.c`, `firmware_carro/rumbo.h`, `firmware_carro/rumbo.c`, `firmware_carro/imu.h`, `firmware_carro/imu.c`, `firmware_carro/giro.h`, `firmware_carro/giro.c`, `firmware_carro/tof.h`, `firmware_carro/tof.c`. |
| **Cap. 5** | Firmware de cámara (`Freenove GC0308`) | `camara_prueba/camara_prueba.ino`, `camara_prueba/recuperacion.h`, `camara_prueba/recuperacion.c`, `camara_prueba/secretos.ejemplo.h`, `camara_prueba/src/JPEGENC/`, `nucleo_c/paquete_celdas.h`, `nucleo_c/paquete_celdas.c`. |
| **Cap. 6** | Protocolo y pruebas de enlace | `docs/protocolo.md`, `pruebas_firmware/Makefile`, `pruebas_firmware/pruebas.c`, `pruebas_firmware/pruebas_sensores.c`, `pruebas_firmware/pruebas_rumbo.c`, `pruebas_firmware/pruebas_recuperacion.c`, `pruebas_firmware/pruebas_integracion.c`, `pruebas_firmware/probar_servidor_falso.py`. |
| **Cap. 7** | Núcleo de navegación, calibración y simulación en C99 | `nucleo_c/Makefile`, `nucleo_c/navegador.h`, `nucleo_c/navegador.c`, `nucleo_c/trayectoria.h`, `nucleo_c/trayectoria.c`, `nucleo_c/calibracion.h`, `nucleo_c/calibracion.c`, `nucleo_c/calgiro.h`, `nucleo_c/calgiro.c`, `nucleo_c/ruta_editor.h`, `nucleo_c/ruta_editor.c`, `nucleo_c/simulador_navegador.h`, `nucleo_c/simulador_navegador.c`, `nucleo_c/sim_ruta.h`, `nucleo_c/sim_ruta.c`, `nucleo_c/pruebas_precision_j16.c`, `nucleo_c/pruebas_reincorporacion_j15.c`. |
| **Cap. 8** | Visión, corredor y puente JNI | `nucleo_c/obstaculos.h`, `nucleo_c/obstaculos.c`, `nucleo_c/corredor.h`, `nucleo_c/corredor.c`, `nucleo_c/fusion.h`, `nucleo_c/fusion.c`, `nucleo_c/cualquiera.h`, `nucleo_c/cualquiera.c`, `nucleo_c/mascara_vision.h`, `nucleo_c/mascara_vision.c`, `nucleo_c/movimiento_piso.c`, `nucleo_c/movimiento_visual.c`, `app_android/app/src/main/cpp/puente.c`, `app_android/app/src/main/cpp/CMakeLists.txt`, `app_android/app/src/main/assets/efficientdet_lite0.tflite`. |
| **Cap. 9** | Aplicación Android (`Kotlin` y vistas) | `app_android/app/build.gradle.kts`, `app_android/app/src/main/AndroidManifest.xml`, `app_android/app/src/main/java/com/example/carro_ilib/MainActivity.kt`, `InicioFragment.kt`, `ManualFragment.kt`, `RutasFragment.kt`, `EjecutarRutaFragment.kt`, `MasFragment.kt`, `AjustesFragment.kt`, `VisionActivity.kt`, `CalibracionActivity.kt`, `CalibrarGiroActivity.kt`, `SensoresActivity.kt`, `BenchmarkActivity.kt`, `ControlLogica.kt`, `ModoSemiautonomo.kt`, `CalibracionGuardada.kt`, `SesionCarro.kt`, `SesionVideo.kt`, `Enlace.kt`, `DetectorTflite.kt`, `ModeloEnHilo.kt`, `*Nativa.kt`. |
| **Cap. 10 a 15** | Scripts, artefactos `dist/`, pruebas físicas y generador del manual | `scripts/cargar.ps1`, `scripts/compilar_app.ps1`, `scripts/verificar_version.py`, `scripts/probar_version.py`, `scripts/verificar_pagina.py`, `dist/carro_fsi_0.4-6d1b213.apk`, `dist/SHA256SUMS.txt`, `dist/FIRMWARE_LOCAL.txt`, `dist/LEEME.txt`, `docs/pruebas_fisicas_20261110.csv`, `docs/precision/monte_carlo_1000.txt`, `docs/precision/calibracion_j14.txt`, `docs/figuras/generar_figuras.py`, `docs/manual/generar_manual.py`, `docs/manual/Manual_Proyecto_Carro_FSI.pdf`. |

## 16.4 Lista consolidada de pruebas y mediciones pendientes

| Identificador | Descripción de la medición o mejora pendiente | Estado actual | Prioridad sugerida |
|---|---|---|---|
| **P-01 (`B5`)** | Medir con cinta métrica en el carro físico la recta de `150 cm` con `RUMBO:OFF` vs `RUMBO:ON` (5 corridas por modo con pilas al 100 %). | `SIMULADO` / `NO MEDIDO` en carro | Alta |
| **P-02 (`B7` / `B8`)** | Ejecutar en el carro físico la ruta en L (`60×60 cm`) y el cuadrado de `60 cm` con el cierre J16 completo. | `SIMULADO` / `NO MEDIDO` en carro | Alta |
| **P-03 (`IMU_ESCALA`)** | Realizar la calibración física de `360°` en piso desde `SensoresActivity` y medir la mejora de cierre angular en el cuadrado. | `VERIFICADO` en código / `NO MEDIDO` en piso | Media |
| **P-04 (`B9` / `TOF`)** | Medir físicamente la distancia de paro ante corte inducido de WiFi en marcha y en serie aislada de freno láser a `120 mm`. | `VERIFICADO` en software / `NO MEDIDO` aislado | Media |
| **P-05 (Telemetría)** | Añadir exportación automática de bitácora `.csv`/`.jsonl` por corrida en Android para diagnosticar transiciones a `AYUDA` como la Corrida 5 de B6. | `PENDIENTE` | Media |
| **P-06 (Hardware)** | Medir con amperímetro el consumo de arranque y régimen de los 4 motores `TT` y verificar en banco el circuito externo de `GPIO 2`. | `NO MEDIDO` | Baja |
