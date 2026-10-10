# Pendientes del firmware (para hacer de uno en uno con la placa)

Ninguno de estos puntos se corrigió: todos cambian el comportamiento externo
y el firmware nuevo debe comportarse igual que `original_backup/codigo_Carrito.ino`.
Hacer cada cambio por separado y probarlo en la placa antes del siguiente.

## Problemas que cambian el comportamiento

1. **Carrera entre tareas (variables compartidas sin protección).**
   Los eventos del WebSocket corren en la tarea `async_tcp` y `loop()` en otra.
   Ambas leen/escriben `comando_actual`, `motores_en_marcha` y
   `ultimo_comando_ms` (carro.c) sin `volatile` ni exclusión mutua. Antes era
   peor (un `String`); ahora un `char[]` fijo no corrompe el heap, pero sigue
   pudiendo leerse un estado a medias. Solución a evaluar: spinlock (`portMUX`)
   o cola de comandos hacia `loop()`.

2. **EBRAKE por pantalla apagada no cancela el heartbeat (pagina.h).**
   En `visibilitychange` (oculta) la página envía `EBRAKE` pero no borra
   `heartbeatInterval` ni pone `activeCmd = "STOP"`. Si el navegador sigue
   ejecutando el temporizador, el siguiente heartbeat reanuda el movimiento.
   Pendiente: no se tocó el JavaScript (`pagina.h` debe quedar idéntico).

3. **Comandos desconocidos renuevan el watchdog.**
   Cualquier texto que no sea `PING` actualiza `ultimo_comando_ms`, incluso si
   no es un comando válido (`HOLA`). Además se guarda como comando actual. Está
   marcado con "OJO: pendiente" en `carro.c` (`ejecutar_comando`).

4. **Sin pausa al cambiar de sentido.** `FORWARD` ↔ `REVERSE` invierte los
   motores de golpe: pico de corriente y esfuerzo mecánico. Evaluar una pausa
   corta o pasar por PWM 0 sin usar `delay()`.

5. **GPIO 2 como pin de arranque (strapping).** Es el pin del keep-alive y
   además un pin de arranque del ESP32 y el LED de muchas placas. No se sabe
   qué hay conectado (LED, resistencia o nada) ni si la powerbank realmente lo
   necesita. Verificar en la placa antes de cambiar nada.

6. **Dependencia del core 3.x.** `ledcAttach(pin, freq, bits)` y
   `esp_task_wdt_config_t` solo existen en Arduino-ESP32 3.x. Probado solo con
   core **3.3.12**, Async TCP 3.5.0 y ESP Async WebServer 3.12.0 (hay 3.12.1
   disponible, sin actualizar). Con core 2.x no compila.

7. **Cambios de tiempos.** Cualquier cambio exige revisar la relación entre
   ellos: heartbeat 250 ms (JS) < `TIEMPO_WATCHDOG_MS` 1000 ms; PING 800 ms
   (JS); `WATCHDOG_HW_MS` 5000 ms; keep-alive 2000/50 ms; reconexión 2000 ms.

## Cosas por verificar en la placa

- **¿LEFT gira de verdad a la izquierda?** El código asume motor izquierdo =
  pines 27/26 y derecho = 25/33; no está confirmado. Si gira al revés,
  intercambiar LEFT y RIGHT (o los cables), no el orden de pines.
- Dirección de avance de cada motor: el orden de pines en `avanzar()`
  (izq por pin B, der por pin A) está probado según el original; solo se
  corrigió el comentario.
- Si loop() se cuelga con motores en marcha, el PWM sigue hasta que el
  watchdog de hardware reinicia (hasta 5 s).
- La interfaz muestra "ONLINE" mientras el socket esté abierto; el ESP32
  nunca responde nada, así que la página no detecta un enlace roto.
- Pines de motor flotantes entre el encendido y `ledcAttach` (milisegundos).
- El nombre y la contraseña del WiFi ya no están en el repositorio: viven en
  `firmware_carro/secretos.h` (ignorado por git). Con `secretos.ejemplo.h` el carro
  crea una red distinta a la de hoy; ver el README. Conviene usar una contraseña
  propia y no reutilizar la del prototipo.
- `docs`/PDF: `WALL-E_Documentacion_v2.pdf` está desactualizado (describe una
  rampa de aceleración que ya no existe).

## Diferencias respecto al original

Para verificar cuando se tenga la placa. Ninguna debería notarse con la página
actual.

1. **Truncado de comandos.** El original guardaba cualquier largo en un
   `String`. Ahora el `.ino` copia como máximo `LARGO_MAX_COMANDO - 1` = 31
   caracteres antes de pasar a C (y `carro.c` guarda con el mismo límite). Un
   mensaje de más de 31 caracteres se trunca; como ningún comando válido es tan
   largo, el resultado es siempre "desconocido", igual que antes.
2. **`\0` incrustado.** Un `String` conserva bytes `\0` dentro del mensaje; en C
   el texto termina en el primer `\0`. Un mensaje `STOP\0xyz` antes se
   trataba como desconocido y ahora como `STOP`. La página nunca los envía.
3. **Keep-alive con variable de estado.** El original leía el pin con
   `digitalRead(KEEPALIVE_PIN)`; ahora `carro.c` usa la variable
   `keepalive_alto`. Mismo resultado mientras nadie más escriba en GPIO 2.
4. **`hw_millis()` se lee una vez por función.** Las condiciones del
   keep-alive original llamaban a `millis()` varias veces; ahora se lee una
   sola vez por llamada (diferencia de microsegundos, sin efecto práctico).
5. **Mismos mensajes de serie**, ahora con `hw_registrar()` para el aviso del
   watchdog. El orden de `setup()` es el original (con `hw_iniciar_pwm()` y
   `carro_iniciar()` en lugar de los cuatro `ledcAttach` y `setMotors`).

## Pendientes que vienen de la pantalla de control (app)

- **El firmware no contesta `PONG` a `PING`.** La app mide la latencia PING-PONG y el servidor falso sí contesta; con el carro real la latencia se verá "—". Para medirla por WiFi habría que hacer que el firmware conteste `PONG` (cambia el protocolo: hacerlo aparte y con la placa).
- **Silencio del carro y tiempo de espera de lectura.** La app no cierra la conexión por silencio (readTimeout 0 en OkHttp). Comprobarlo con el carro real durante más de 10 s sin tráfico de vuelta.
- **Un solo cliente.** Al conectar la app, el firmware cierra al cliente anterior (página web u otro teléfono); la app lo toma como una caída y reintenta cada 2 s, lo que podría "pelearse" con la otra conexión. Probar con el carro.
