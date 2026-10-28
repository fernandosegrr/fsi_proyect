# Referencia del firmware del carro

**Estado:** documento de referencia · **Fecha:** 27 de octubre de 2026 · **Rama:** `docs/documentacion-completa`
**Código documentado:** `firmware_carro/` en `deae8b6`. **Fuentes:** `config.h`, `carro.h`, `carro.c`, `firmware_carro.ino`, `sensores.h`, `sensores.c`, `imu.*`, `tof.*`, `giro.*`, `docs/sensores.md` y `docs/pendientes_firmware.md`.

## 1. En una frase

El firmware recibe órdenes de texto por WiFi, las convierte en movimiento de cuatro motores y las corta solo si el teléfono deja de dar señales de vida; además lee un giroscopio y un sensor de distancia, y avisa a la app de lo que ve. Ese «cortar solo» es el **watchdog**: un vigilante interno que apaga los motores cuando pasa demasiado tiempo sin recibir una orden real.

La idea central es **separar las decisiones del hardware**. `carro.c` sabe *qué* hacer (avanzar, girar, parar, vigilar el tiempo) pero no sabe encender un pin: para eso llama a funciones `hw_...` que el archivo `.ino` implementa con Arduino. Gracias a ese corte, la lógica se puede probar en un PC con hardware falso, sin placa.

## 2. Archivos y capas

| Archivo | Qué contiene | Se puede probar en PC |
|---|---|---|
| `config.h` | Todos los números ajustables: pines, tiempos, velocidades, umbrales | — (solo constantes) |
| `carro.h` / `carro.c` | Comandos, motores, perfiles, watchdog, cola y keep-alive | Sí, con `pruebas_firmware/` |
| `sensores.h` / `sensores.c` | Une los sensores con los comandos y los mensajes hacia la app | Sí |
| `imu.c` / `tof.c` / `giro.c` | Giroscopio, láser y giro por ángulo | Sí |
| `hw_sensores.h` | Lo que el `.ino` debe darle a los sensores (I2C, tiempo, memoria) | — (interfaz) |
| `firmware_carro.ino` | WiFi, servidor HTTP, WebSocket, I2C real y `setup()`/`loop()` | No: necesita la placa |
| `pagina.h` | Página web de control guardada en flash | No |

La regla se ve en `carro.h:18-47`: `hw_motor_pwm`, `hw_millis`, `hw_keepalive`, `hw_registrar`, `hw_balance_leer/guardar` y los bloqueos. En el ESP32 las implementa el `.ino` (`firmware_carro.ino:250-350`, entre otras); en las pruebas las implementa un archivo falso.

## 3. Qué pasa al encender

`setup()` (`firmware_carro.ino:405-434`) hace, en este orden:

1. Abre el puerto serie a 115200 y escribe la versión (`FW_VERSION`).
2. Baja la CPU a 160 MHz para consumir menos (`config.h:63-64`).
3. Configura los cuatro canales PWM a 1500 Hz y 8 bits (`firmware_carro.ino:252-258`).
4. Deja el carro parado y el estado en `STOP` con `carro_iniciar()`.
5. Abre el bus I2C y arranca los sensores (`iniciar_i2c`, `sensores_iniciar`).
6. Configura el watchdog de hardware a 5000 ms.
7. Crea la red WiFi propia y configura el DHCP de .10 a .20.
8. Configura el pin keep-alive.
9. Publica la página web en `/`, el WebSocket en `/ws` y arranca el servidor.

El arranque siempre deja el perfil en **AUTO**, que es el más lento y el más seguro (`carro.c:345`). La app debe pedir `MODO_MANUAL` de forma explícita si quiere velocidad máxima.

## 4. El bucle principal

`loop()` (`firmware_carro.ino:535-551`) se repite sin parar y hace siempre lo mismo, en este orden:

```text
revisar el watchdog de hardware (esp_task_wdt_reset)
limpiar clientes WebSocket viejos
leer sensores y mover el giro por ángulo   ← único lugar que toca el I2C
vaciar la cola de comandos
aplicar el watchdog de comunicación
actualizar el pulso de la powerbank
enviar mensajes de sensores a la app
leer comandos de diagnóstico por USB
anotar cuánto tardó esta vuelta (LOOP?)
```

El orden importa: los sensores se leen antes de ejecutar comandos, y el watchdog se revisa después de vaciar la cola. Así un `STOP` que acaba de llegar se ejecuta antes de que el vigilante decida frenar por su cuenta.

## 5. Cómo viaja un comando

La app envía texto por WebSocket, que es un canal abierto entre el teléfono y el carro: una vez conectado, los dos pueden mandar mensajes cuando quieran, sin preguntar y responder cada vez. El camino es:

```text
teléfono → WebSocket /ws
        → al_evento_websocket()                  (firmware_carro.ino:353)
        → carro_encolar_comando()                (carro.c:389)
        → loop(): carro_procesar_cola()          (carro.c:455)
        → carro_recibir_comando()                (carro.c:509)
        → ejecutar_comando()                     (carro.c:273)
        → avanzar()/retroceder()/girar_...()     (carro.c:115-160)
        → poner_motores() → hw_motor_pwm()       (carro.c:98)
```

La cola existe porque el WebSocket corre en otra tarea del sistema y no debe tocar los motores a la vez que `loop()`. Solo `loop()` cambia el estado del carro. La cola tiene 32 espacios (`config.h:154`) y, si se llena, primero tira un `PING` viejo y nunca un `STOP` (`carro.c:399-429`). Esa protección se probó con 100 000 comandos en estrés, sin perder ningún STOP (ver `docs/reporte_J14.md`).

Los comandos repetidos se ignoran (`carro.c:217-223`), salvo `STOP` y `EBRAKE`, que siempre se ejecutan. Así el heartbeat de la página no reinicia el movimiento una y otra vez.

## 6. Comandos que entiende el firmware

Los comandos son texto plano, sin JSON. La tabla siguiente es la lista completa; los detalles de sensores están en `docs/sensores.md` y el protocolo de la app en `docs/protocolo.md`.

| Comando | Efecto | Respuesta |
|---|---|---|
| `FORWARD` | Avanza con los dos motores | — |
| `REVERSE` | Retrocede | — |
| `LEFT` | Pivota a la izquierda (solo empuja el motor derecho) | — |
| `RIGHT` | Pivota a la derecha (solo empuja el motor izquierdo) | — |
| `STOP` | Frena ya | — |
| `EBRAKE` | Igual que `STOP` | — |
| `PING` | Mantiene viva la conexión; **no** renueva el watchdog | `PONG` |
| `MODO_MANUAL` | Perfil rápido (255/252) | `ACK_MANUAL` |
| `MODO_AUTO` | Perfil limitado (210/207) | `ACK_AUTO` |
| `BAL_AUTO:NNN` | Cambia el balance del perfil AUTO (800-1000) y lo guarda | `ACK_BAL_AUTO:NNN` o `NAK_BAL` |
| `BAL_MANUAL:NNN` | Igual para el perfil MANUAL | `ACK_BAL_MANUAL:NNN` o `NAK_BAL` |
| `BAL?` | Consulta ambos balances | `BAL:AAA:MMM` |
| `GIRAR:<grados>` | Gira un ángulo medido (5 a 200; positivo = izquierda) | `GIRO_OK`, `GIRO_TIMEOUT` o `GIRO_ERROR` |
| `CAPS?` | Qué sensores funcionan | `CAPS:...` |
| `IMU?` | Ángulo, sesgo, estado e identificación del MPU6050 | `IMU:...` |
| `TOF?` | Distancia y banderas del láser | `TOF:...` |
| `IMU_CAL` | Recalibra el sesgo (solo con el carro quieto) | `ACK_IMU_CAL` o `NAK_IMU_CAL` |
| `IMU_CERO` | Pone el ángulo actual en cero | `ACK_IMU_CERO` |
| `TOF_USO:<0|1>` | Enciende o apaga la parada de emergencia del láser | `ACK_TOF_USO:<n>` |
| `LOOP?` | Peor duración de `loop()` y excesos | `LOOP:...` |
| `ANT?` | Anticipaciones de giro aprendidas | `ANT:...` |
| `ARR?` | Historial de reintentos al arrancar | `ARR:...` |
| `ANT_BORRAR` | Borra las anticipaciones aprendidas | `ACK_ANT_BORRAR` |

Además, el firmware manda solo `SENS:<ángulo x10>,<mm>,<banderas>` cada 100 ms mientras hay cliente y el perfil es AUTO (`config.h:242-247`, `sensores.h:11-27`), y `TOF_PARO:<mm>` cuando el láser frena el carro.

Los comandos de diagnóstico `I2C?`, `PINES?` y `PRUEBAI2C?` se aceptan por el **puerto serie USB**, no por WiFi (`firmware_carro.ino:508-533`). `GIRAR` por USB se rechaza con `NAK_SERIE`: los motores solo se mueven por WebSocket (`sensores.c:311-316`).

## 7. Perfiles, balance y arranque

El carro tiene dos formas de andar:

- **MANUAL:** pensada para conducir con los botones. Tope derecho 255 y factor de balance 988, que deja el izquierdo en 252 (`config.h:120-123`). El ajuste se midió a 150 cm: 3 cm de desvío (`config.h:103-104`).
- **AUTO:** pensada para rutas y para el modo semiautónomo. Tope derecho 210 y factor 986, que deja el izquierdo en 207 (`config.h:125-128`). Este ajuste **no se midió en el carro**; está pendiente (`config.h:96`).

En los dos perfiles, al empezar a avanzar o retroceder los motores van al 100 % durante 250 ms y después se aplica el ajuste de cada lado (`config.h:106-108`, `carro.c:115-142`). El motivo es práctico: desde parado, con menos de 255 los motores solo zumban y no arrancan.

El **balance** es un factor de 800 a 1000 milésimas que solo reduce el lado izquierdo, porque en este carro el izquierdo empuja más (`config.h:112-118`). Se puede cambiar desde la app con `BAL_AUTO`/`BAL_MANUAL`, se guarda en memoria no volátil y se aplica en el siguiente arranque (`carro.c:292-299`, `341-368`).

## 8. El watchdog: por qué el carro no se escapa

Hay dos vigilantes distintos, y conviene no confundirlos:

1. **Watchdog de comunicación (software, 1000 ms).** Si hay motores en marcha y pasa más de un segundo sin un comando real, `carro_vigilar_comunicacion()` apaga los motores y vuelve al perfil AUTO (`carro.c:589-629`). `PING` no cuenta como comando real: si contara, el carro seguiría avanzando con el botón suelto, porque la página manda `PING` cada 800 ms (`carro.c:506-508`). Las consultas de sensores tampoco renuevan el reloj, salvo `GIRAR`, que sí es una orden de movimiento (`carro.c:520-523`).
2. **Watchdog de hardware (5000 ms).** Si `loop()` se cuelga por completo, el chip se reinicia solo (`firmware_carro.ino:266-285`). Se reconfigura el vigilante que el núcleo de Arduino ya trae, en vez de crear otro.

Durante un giro por ángulo, el watchdog de 1 s **no** actúa: un giro largo puede durar más. El giro tiene su propio tiempo máximo y se aborta con `STOP` o con otro movimiento (`carro.c:311-314`, `619-620`).

Cuando un cliente WebSocket se conecta o se desconecta, el firmware frena y vuelve a AUTO (`carro.c:477-499`, `firmware_carro.ino:356-376`). Si se cae el WiFi, el watchdog de comunicación hace el resto.

## 9. Sensores, en corto

- **MPU6050 (giroscopio):** mide la velocidad de giro y la integra para estimar el ángulo. Al arrancar promedia 250 lecturas con el carro quieto para calcular el sesgo; si el carro se mueve, reintenta (`config.h:181-188`). El ángulo se usa en `GIRAR` y se envía en `SENS`.
- **VL53L0X (láser):** mide distancia al frente cada 33 ms sin bloquear el bucle. Si hay dos lecturas seguidas por debajo de 120 mm y el perfil es AUTO y el carro avanza, frena con `__STOP_AUTO` y avisa `TOF_PARO:<mm>` (`config.h:223-240`). El comando `TOF_USO:0` apaga esa parada, pero el sensor sigue midiendo.
- **Giro por ángulo:** `GIRAR` usa un solo motor, frena con una anticipación aprendida y después deja asentar el carro para informar el ángulo final (`config.h:202-221`). Las anticipaciones se guardan por sentido y por perfil.

La explicación completa, con mensajes y fallos típicos, está en `docs/referencia/` (sensores) y en `docs/sensores.md`.

## 10. WiFi y servidor

- La ESP32 crea su propia red (AP). La IP del carro es **192.168.4.1**, el HTTP va en el puerto **80** y el WebSocket en **`/ws`** (`config.h:38-52`).
- El DHCP reparte de **.10 a .20** para dejar libre la **.2** de la cámara (`firmware_carro.ino:287-310`).
- El canal es fijo (1) y la potencia de radio se baja a 13 dBm para no exigir picos a la powerbank (`config.h:54-64`).
- Solo se atiende **un cliente WebSocket a la vez**: al conectarse uno nuevo, se cierran los demás (`firmware_carro.ino:356-364`).
- La página de control se sirve desde flash y sin caché (`firmware_carro.ino:321-330`).

El nombre y la contraseña de la red **no están en el repositorio**: viven en `firmware_carro/secretos.h`, que está ignorado por git. Si ese archivo no existe, se usa `secretos.ejemplo.h` (`config.h:20-36`). No se debe publicar el binario compilado porque contiene esos datos (ver `docs/entrega.md`).

## 11. Versión del firmware

`scripts/cargar.ps1` copia el sketch a una carpeta temporal, obtiene el hash corto del commit y la fecha, y escribe `version_compilada.h` con algo como `FW:453ed20-20261027` (`scripts/cargar.ps1:163-173`). `config.h` incluye ese archivo si existe (`config.h:10-18`). Así la versión que se ve en el puerto serie y en la app corresponde al commit con el que se compiló, sin ensuciar el repositorio.

La versión conservada en J15 es **FW:453ed20-20261027**. Su binario no se publica; el hash y el motivo están en `docs/entrega.md` §4. La lectura de la versión en la placa durante J15 quedó pendiente.

## 12. Límites y cosas que no hace

- No hay encoders: el avance se estima por tiempo, no por distancia medida. Por eso la pose puede desviarse y J15 añadió corrección con el rumbo del giroscopio (`docs/reporte_J15.md`).
- El giroscopio no detecta deslizamiento lateral puro; la corrección de pose no puede arreglar todo (`docs/entrega.md` §6).
- El láser solo mira al frente y a una altura fija; no ve huecos, escalones ni objetos fuera del haz.
- El perfil AUTO no se calibró en el carro: 207/210 es una extrapolación del ajuste manual (`config.h:96`).
- El firmware no distingue un comando desconocido de uno válido para el watchdog: un texto raro renueva el reloj y se guarda como comando actual (`carro.c:330-335`). Es el comportamiento original y está anotado en `docs/pendientes_firmware.md`.
- No se ha medido el consumo, la autonomía ni la temperatura del driver (ver `docs/referencia/hardware.md`).

## 13. Para seguir leyendo

- `docs/referencia/firmware.md` (este documento): vista de conjunto.
- `docs/codigo/firmware_carro.md`: recorrido línea a línea de los archivos.
- `docs/sensores.md`: MPU6050 y VL53L0X en detalle.
- `docs/protocolo.md`: todos los mensajes y el comportamiento esperado de la app.
- `docs/pendientes_firmware.md` y `docs/pendientes_documentacion.md`: lo que falta o está mal.
