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
- TRABAJO 1 se entendió como el firmware ya entregado; el firmware no se tocó en esta tanda.
- nucleo_c se creó en la raíz de fsi_proyecto, al lado de firmware_carro (no dentro del sketch, para que Arduino no lo compile).
- `trayectoria_calcular` recibe además `capacidad` e `indice_error` (opcional): permite probar el exceso de pasos y decirle al usuario qué punto falló.
- La diagonal se calcula sobre el total del tramo (n*celda*1414/1000), no celda por celda: una celda da 28 cm igual que la fórmula pedida, pero 5 celdas dan 141 cm y no 5*28 = 140 (así el redondeo no se acumula).
- Media vuelta (180°) se da como +180 (derecha), porque la diferencia normalizada es ambigua.
- MAX_PUNTOS = 64 y MAX_PASOS = 100: con 64 puntos el peor caso (zigzag) da 126 pasos, así que el límite de pasos se puede probar y puede dispararse en rutas reales muy quebradas.
- Si un tramo no cabe en la salida, se devuelve error y `n_pasos` = 0 (no se entrega una ruta parcial, que haría que el carro avance a medias).
- La ruta se valida completa antes de calcular, para que un error no deje salida a medias.
