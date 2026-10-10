# Bitácora de decisiones

- `git init` en `fsi_proyecto` (no había repositorio) para poder hacer commits locales por hito, como autorizaste; no se hace push.
- `.gitignore` para el ejecutable de las pruebas, para no versionar binarios.
- No se instalaron librerías: `Async TCP 3.5.0` y `ESP Async WebServer 3.12.0` ya estaban instaladas y compilan con el core 3.3.12 (hay 3.12.1 disponible; no se actualizó porque no hace falta).
- `extern "C"` va en cada función `hw_...` y no en un bloque: arduino-cli inserta prototipos dentro del bloque y rompía `setup()`/`loop()`.
- `config.h` define `WIFI_POTENCIA_TX` como macro hacia `WIFI_POWER_13dBm` para que el archivo siga siendo C puro.
- `hw_millis()` devuelve `uint32_t` para que el desbordamiento sea idéntico en PC y ESP32.
- Las pruebas incluyen una transcripción del código original (String → char[]) para comparar 600 000 pasos aleatorios; es mi transcripción, no el original ejecutado.
- Se probó una mutación (`>` a `>=` en el watchdog) para confirmar que las pruebas detectan errores: falló como debía.
- Parte C del mensaje (TRABAJOS 1-3) y el final de la sección 5 llegaron cortados: solo se ejecutó la parte A. Bloqueo anotado; ver resumen.
