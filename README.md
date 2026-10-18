# Proyecto carro FSI (ESP32)

Carro de 4 ruedas con ESP32, controlado desde el celular por WiFi y
WebSocket. Este repositorio tiene el firmware, su lógica en C, un módulo para
convertir rutas de una cuadrícula en pasos con tiempos, y las pruebas de PC.

## Qué hay en cada carpeta

- `firmware_carro/` — el sketch de Arduino (la carpeta se llama igual que el `.ino`).
  - `firmware_carro.ino`: único C++; WiFi, servidor, WebSocket, `setup()` y `loop()`.
  - `carro.c` / `carro.h`: la lógica en C estándar (comandos, motores, parada automática, keep-alive).
  - `imu.c`, `tof.c`, `giro.c`, `sensores.c` (+ `hw_sensores.h`): giroscopio MPU6050, sensor láser VL53L0X y giro por ángulo medido, también en C puro (J11, ver `docs/sensores.md`).
  - `config.h`: pines, tiempos y velocidad. El nombre y la contraseña del WiFi están en `secretos.h`
    (no se sube a GitHub; se crea copiando `secretos.ejemplo.h`).
  - `pagina.h`: la página web de control (HTML/JS), sin cambios respecto al original.
- `pruebas_firmware/` — pruebas en PC de `carro.c` con hardware falso, y comparación con el código original.
  También `servidor_falso.py` (imita al carro por WebSocket para probar la app sin el carro; ver
  `docs/pruebas_control.md`).
- `nucleo_c/` — módulos en C estándar (sin librerías) para usar desde Android: trayectoria
  (`trayectoria.c/.h`, `calibracion.c/.h` con la calibración medida del carro: arranque, ms por cm, frenado y giro por lado) y detector de obstáculos
  por color de piso (`obstaculos.c/.h`, ver `docs/obstaculos.md`) y navegador del modo semiautónomo
  (`navegador.c/.h` y un simulador de carro para las pruebas, ver `docs/navegador.md`) fusión del detector de color con el modelo de objetos (`fusion.c/.h`, ver `docs/fusion.md`) y giro medido con la cámara (`movimiento_visual.c/.h`, ver `docs/giro_visual.md`), cada uno con sus pruebas.
- `app_android/` — esqueleto de la app Android (Kotlin, vistas clásicas, sin librerías): dibujas una
  ruta con el dedo en una cuadrícula y el botón "Calcular" muestra los pasos. Llama por JNI
  (`app/src/main/cpp/puente.c`) al módulo de `nucleo_c`, que se compila desde su sitio sin copiarse.
  Trae la pantalla de visión (`VisionActivity`: cámara del teléfono + detector de obstáculos con umbrales ajustables), la pantalla de control (`ControlActivity`: botones + WebSocket) y el benchmark del detector de objetos (`BenchmarkActivity`, modelo en `app/src/main/assets/`).
- `docs/` — `vision_huawei.md` (probar el detector con la cámara del teléfono sobre el piso real), `navegador.md` (modo semiautónomo: estados, parámetros y límites), `obstaculos.md` (detector de obstáculos por color de piso), `pruebas_control.md` (pruebas de la pantalla de control), `benchmark_ELS-NX9.md` (cuánto tarda el detector en el Huawei, datos en `datos_benchmark/`),
  `instalar_app_huawei.md` (cómo instalar la app), `pendientes_firmware.md` (problemas por resolver con la placa y diferencias con el
  original), `bitacora.md` (decisiones), `progreso.md` y `resumen_sesion.md`.
- `original_backup/` — el código y el PDF originales. No se modifica y **no se publica en GitHub**
  (está en `.gitignore`) porque contienen la contraseña del WiFi; solo existe en el equipo local.

## Cómo correr las pruebas (en PC, sin placa; requiere `gcc` y `make`)

```bash
cd pruebas_firmware
make test
```

```bash
cd nucleo_c
make test
```

Cada caso imprime `OK` o `FALLO` y al final el número de casos y fallos.

## Cómo compilar el firmware (requiere `arduino-cli`, core esp32 3.3.12)

Librerías: Async TCP y ESP Async WebServer.

Antes de compilar, copia `firmware_carro/secretos.ejemplo.h` como `firmware_carro/secretos.h` y pon ahí el
nombre y la contraseña del WiFi del carro (`secretos.h` no se sube a GitHub). Sin ese archivo el proyecto
compila igual, pero con el nombre y la clave de ejemplo.

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 firmware_carro
```

Para subirlo hace falta la placa (`arduino-cli upload`); todavía no se ha probado en hardware.

## Cómo compilar la app Android (requiere Android Studio y el SDK; ver docs/instalar_app_huawei.md)

```powershell
cd app_android
```

```powershell
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
```

```powershell
.\gradlew.bat assembleDebug
```

## En qué orden leer el código

1. `firmware_carro/config.h` — los números del carro.
2. `firmware_carro/carro.h` — qué ofrece la lógica y qué pide al hardware (`hw_...`).
3. `firmware_carro/carro.c` — la lógica; el resumen de arriba explica cómo se conectan las funciones.
4. `firmware_carro/firmware_carro.ino` — la parte C++ que conecta WiFi y WebSocket con `carro.c`.
5. `pruebas_firmware/pruebas_carro.c` — ejemplos ejecutables de cada comportamiento.
6. `nucleo_c/trayectoria.h`, `trayectoria.c` y `pruebas_trayectoria.c` — el módulo de rutas; después
   `obstaculos.h`, `obstaculos.c` y `pruebas_obstaculos.c` — el detector de obstáculos; y por último
   `navegador.h`, `navegador.c` y `pruebas_navegador.c` — el navegador que los une.
7. `app_android/app/src/main/cpp/puente.c`, luego `TrayectoriaNativa.kt`, `CuadriculaView.kt` y
   `MainActivity.kt` — cómo la app llama al módulo en C; después `ModoSemiautonomo.kt` y `NavegadorNativa.kt`.
8. `docs/pendientes_firmware.md` — lo que falta verificar o mejorar.

## Comandos del carro (protocolo por WebSocket, texto plano, sin parámetros)

`FORWARD`, `REVERSE`, `LEFT`, `RIGHT`, `STOP`, `EBRAKE` y `PING`. Velocidad
fija al máximo. Si pasan 1000 ms sin un comando real, el carro se detiene.

Con el giroscopio y el láser (J11, `docs/sensores.md`) hay mensajes nuevos, con parámetros: `GIRAR:<grados>` (positivo = izquierda, 5 a 200; contesta `GIRO_OK`, `GIRO_TIMEOUT` o `GIRO_ERROR`),
`CAPS?`, `IMU?`, `TOF?`, `IMU_CAL`, `IMU_CERO`, `TOF_USO:<0|1>`, `LOOP?` y `ANT?`; el carro manda `SENS:<ángulo x10>,<mm>,<banderas>` cada 100 ms y `TOF_PARO:<mm>` cuando el láser lo frena.
Un firmware viejo no entiende estos mensajes: la app lo toma como «sin sensores» y todo sigue por tiempo como antes.
