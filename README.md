# Proyecto carro WALL-E (ESP32)

Carro de 4 ruedas con ESP32, controlado desde el celular por WiFi y
WebSocket. Este repositorio tiene el firmware, su lógica en C, un módulo para
convertir rutas de una cuadrícula en pasos con tiempos, y las pruebas de PC.

## Qué hay en cada carpeta

- `firmware_carro/` — el sketch de Arduino (la carpeta se llama igual que el `.ino`).
  - `firmware_carro.ino`: único C++; WiFi, servidor, WebSocket, `setup()` y `loop()`.
  - `carro.c` / `carro.h`: la lógica en C estándar (comandos, motores, parada automática, keep-alive).
  - `config.h`: pines, tiempos, velocidad, SSID y contraseña (cambiarlos antes de publicar).
  - `pagina.h`: la página web de control (HTML/JS), sin cambios respecto al original.
- `pruebas_firmware/` — pruebas en PC de `carro.c` con hardware falso, y comparación con el código original.
- `nucleo_c/` — módulo de trayectoria (C estándar, sin librerías) para usar luego desde Android:
  `trayectoria.c/.h`, `calibracion.h` (valores de ejemplo, hay que medirlos) y sus pruebas.
- `docs/` — `pendientes_firmware.md` (problemas por resolver con la placa y diferencias con el
  original), `bitacora.md` (decisiones), `progreso.md` y `resumen_sesion.md`.
- `original_backup/` — el código y el PDF originales. No se modifica.

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

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 firmware_carro
```

Para subirlo hace falta la placa (`arduino-cli upload`); todavía no se ha probado en hardware.

## En qué orden leer el código

1. `firmware_carro/config.h` — los números del carro.
2. `firmware_carro/carro.h` — qué ofrece la lógica y qué pide al hardware (`hw_...`).
3. `firmware_carro/carro.c` — la lógica; el resumen de arriba explica cómo se conectan las funciones.
4. `firmware_carro/firmware_carro.ino` — la parte C++ que conecta WiFi y WebSocket con `carro.c`.
5. `pruebas_firmware/pruebas_carro.c` — ejemplos ejecutables de cada comportamiento.
6. `nucleo_c/trayectoria.h`, `trayectoria.c` y `pruebas_trayectoria.c` — el módulo de rutas.
7. `docs/pendientes_firmware.md` — lo que falta verificar o mejorar.

## Comandos del carro (protocolo por WebSocket, texto plano, sin parámetros)

`FORWARD`, `REVERSE`, `LEFT`, `RIGHT`, `STOP`, `EBRAKE` y `PING`. Velocidad
fija al máximo. Si pasan 1000 ms sin un comando real, el carro se detiene.
