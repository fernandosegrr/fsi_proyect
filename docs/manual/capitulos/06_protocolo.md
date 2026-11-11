# 6. Protocolo de comunicación

Este capítulo especifica el protocolo de mensajes de texto que une a la aplicación Android con el firmware de la placa `ESP32 DevKit`. Documenta la capa de transporte, la temporización de latidos y vigilancia, el catálogo completo de órdenes y respuestas —incluidos todos los mensajes incorporados en el cierre **J16** (`RUMBO`, `IMU_ESCALA`, `WD:<n>` y la cadena `CAPS` ampliada)— y muestra la traza comentada de una sesión real.

## 6.1 Capa de transporte y canales de enlace

La comunicación de control entre el teléfono y el carro se realiza mediante mensajes cortos de texto ASCII sobre una sesión persistente **`WebSocket`** en la dirección **`ws://192.168.4.1/ws`** (puerto TCP `80`). El mismo intérprete de comandos (`carro_recibir_comando` y `sensores_comando`) escucha también en el puerto serie `USB` a **`115,200 baudios`**, lo que permite consultar versiones (`VERSION?`), leer diagnósticos (`SENS?`, `LOOP?`) o calibrar sensores desde una computadora sin abrir la red `WiFi`. Adicionalmente, el servidor `HTTP` en `http://192.168.4.1/` ofrece una página web embebida de respaldo verificada por `verificar_pagina.py`.

El diseño utiliza texto legible en lugar de tramas binarias opacas por tres razones de ingeniería:

1. **Inspección inmediata:** cualquier mensaje enviado o recibido puede leerse directamente en el monitor serie o en el registro de la aplicación sin decodificadores especiales.
2. **Pruebas deterministas:** el servidor falso en Python (`probar_servidor_falso.py`) y las suites en `C99` pueden inyectar cadenas literales (`"RUMBO:125"`, `"SENS:-32,480,7"`) para verificar cada rama lógica.
3. **Bajo sobrecosto:** con un máximo de `31` caracteres por comando (`COLA_COMANDO_MAX = 32` incluyendo el terminador nulo) y una cadencia de `4` a `10` mensajes por segundo, el tráfico total de control es inferior a `1 KB/s`.

## 6.2 Temporización de latidos, sondas y vigilancia

El protocolo separa estrictamente la **salud de la conexión de red** de la **autorización para mantener los motores encendidos**:

- **Sonda de enlace (`PING` → `PONG` cada `800 ms`):** la clase `Enlace.kt` en Android envía `PING` cada `800 ms` y espera `PONG`. Si no recibe respuesta dentro de `2,000 ms`, declara el socket caído e inicia la reconexión automática. Sin embargo, **`PING` no renueva el *watchdog* de motores** en el firmware: así, aunque la red siga viva, si el hilo de navegación se detiene, el carro frena solo.
- **Latido de movimiento (`250 ms`):** mientras el usuario mantiene presionado un botón en conducción manual o mientras el navegador ejecuta un avance en ruta, la aplicación reenvía el comando activo (`FORWARD`, `BACKWARD`, `LEFT` o `RIGHT`) cada **`250 ms`**, dando cuatro oportunidades de renovación antes de que venza el plazo del firmware.
- **Plazo de *watchdog* en el carro (`1,000 ms`) y aviso `WD:<n>` (Cierre J16):** si pasan `1,000 ms` (`TIMEOUT_COMANDO_MS`) con los motores activos sin recibir un comando de control válido, el firmware apaga los motores (`STOP`), restablece el perfil `AUTO` y envía por `WebSocket` el mensaje **`WD:<contador>`** (por ejemplo, `WD:1`). Al recibir `WD:<n>`, la aplicación Android detiene inmediatamente cualquier ruta semiautónoma para no quedar desincronizada respecto al carro.
- **Vigilancia de silencio de telemetría en Android (`1,500 ms` — Cierre J16):** en modo `AUTO`, el carro envía `SENS` cada `100 ms` (`10 Hz`). Si durante una ruta activa pasan **`1,500 ms`** (`SILENCIO_SENS_MS`) sin recibir ninguna trama `SENS`, `ModoSemiautonomo.kt` aborta la misión y transmite `STOP`.

## 6.3 Catálogo completo de comandos y respuestas (incluyendo J16)

La siguiente tabla reúne todos los comandos aceptados por el firmware y las respuestas o eventos asíncronos que emite:

| Comando enviado al carro | Respuesta / Evento emitido | Función técnica y parámetros |
|---|---|---|
| `FORWARD`, `BACKWARD`, `LEFT`, `RIGHT` | `OK` *(o `BLOQUEADO_TOF` si láser < 120 mm en `FORWARD` + `AUTO`)* | Activa el movimiento solicitado, aplica pulso de arranque (`255` por `250 ms`) si parte de reposo y renueva el *watchdog* de `1,000 ms`. En `FORWARD` + `AUTO` con `RUMBO` activo, fija o mantiene la referencia de rumbo. |
| `STOP` | `OK` | Apaga inmediatamente los cuatro motores (`LOW/LOW`), cancela cualquier giro `GIRO` o control `RUMBO` en curso y transfiere el integrador de rumbo a `aprendido_mil`. |
| `EBRAKE` | `OK_EBRAKE` | Aplica freno eléctrico activo (`HIGH/HIGH` en ambos canales del `TC1508A`) y detiene cualquier lazo activo. |
| `MODO_MANUAL` / `MODO_AUTO` | `ACK_MANUAL` / `ACK_AUTO` | Cambia el perfil de potencia (`252/255` en `MANUAL`; `207/210` en `AUTO`). Si había motores en marcha, por seguridad los detiene al cambiar de perfil. |
| `ESTADO` | `MANUAL` o `AUTO` *(más `CAPS:...` al conectar)* | Consulta el perfil activo y las capacidades del firmware. |
| `PING` | `PONG` | Comprueba que el enlace `WebSocket` sigue abierto sin renovar el *watchdog* de tracción. |
| `VERSION?` | `FW:<rev>-<fecha>` | Devuelve la revisión compilada (en el cierre actual: `FW:6d1b213-20261110`). |
| `SENS?` | `SENS:<ang_x10>,<mm>,<flags>` | Consulta puntual de telemetría; además, en perfil `AUTO` el firmware la emite automáticamente cada `100 ms`. |
| `CAL_IMU` | `CAL_OK` o `CAL_ERR_MOV` | Solicita recalibrar el sesgo en reposo (`bias`) del `MPU6050` (requiere ~`500 ms` sin movimiento) y pone a cero el ángulo integrado. |
| `GIRO:<grados>` | `GIRO_INICIO:<g>` → `GIRO_FIN:<ang>` *(o `GIRO_TIMEOUT` / `GIRO_ERR_IMU`)* | Ejecuta un giro cerrado por giroscopio de `<grados>` enteros en `[-360, +360] \ {0}` (`+` izquierda, `-` derecha) con tolerancia de `±2.0°` durante `80 ms`. |
| `RUMBO:ON` / `RUMBO:OFF` *(J16)* | `RUMBO_OK:ON` / `RUMBO_OK:OFF` | Habilita o deshabilita el controlador `PI` de rumbo durante los avances rectos `FORWARD` en modo `AUTO`. |
| `RUMBO:<décimas>` *(J16)* | `RUMBO_REF:<décimas>` *(o `RUMBO_ERR`)* | Fija en caliente la referencia $\theta_{\text{ref}}$ del controlador `PI` en décimas de grado (hasta 6 dígitos, p. ej. `RUMBO:-45` para `-4.5°`). Si el cambio supera `1.5°`, corta el pulso de arranque para corregir de inmediato. |
| `RUMBO?` *(J16)* | `RUMBO:<on>,<act>,<ref>,<err>,<aj>,<ap>` | Devuelve estado habilitado (`0/1`), activo (`0/1`), referencia (`x10`), error actual (`x10`), ajuste total en milésimas (`±150`) y sesgo aprendido (`±120`). |
| `IMU_ESCALA:<ppm>` *(J16)* | `IMU_ESCALA_OK:<ppm>` *(o `IMU_ESCALA_ERR`)* | Fija el factor de escala del giroscopio en partes por millón dentro de `[-50000, +50000]` (`±5 %`) y lo guarda en memoria no volátil `NVS`. |
| `IMU_ESCALA?` *(J16)* | `IMU_ESCALA:<ppm>` | Devuelve el factor de escala vigente en partes por millón. |
| `LOOP?` | `LOOP:<prom_us>,<max_us>,<lentos>` | Devuelve promedio y máximo de duración del ciclo de sensores en microsegundos y conteo de ciclos `> 5,000 us`, reiniciando la estadística. |
| *(Evento asíncrono)* | `TOF_PARO:<mm>` | Emitido cuando el láser `VL53L0X` mide `< 120 mm` avanzando en `AUTO` y frena localmente los motores. |
| *(Evento asíncrono J16)* | `WD:<contador>` | Emitido cada vez que vence el *watchdog* de `1,000 ms` por falta de comandos de movimiento con motores encendidos. |

## 6.4 Estructura de las tramas `SENS` y `CAPS`

### Trama periódica de sensores (`SENS:<ang_x10>,<mm>,<flags>`)

Cada `100 ms` en modo `AUTO` (o al enviar `SENS?`), el firmware transmite tres enteros separados por comas:

1. **`<ang_x10>`:** orientación acumulada desde la última calibración `CAL_IMU`, expresada en **décimas de grado** (por ejemplo, `452` significa `+45.2°` hacia la izquierda; `-18` significa `-1.8°` hacia la derecha). En el cierre **J16**, el núcleo de navegación consume este valor directamente en décimas de grado (`rumbo_x10`) sin truncarlo a grados enteros.
2. **`<mm>`:** distancia frontal medida por el sensor `VL53L0X` en milímetros (`20` a `2000`), o **`-1`** si el sensor no está disponible o el objeto está fuera de rango.
3. **`<flags>`:** máscara de bits entera (de `0` a `31`) que codifica en un solo número el estado de salud de los subsistemas:
   - **Bit 0 (`valor 1`):** `imu_ok` — el giroscopio `MPU6050` responde en el bus `I2C`.
   - **Bit 1 (`valor 2`):** `imu_calibrada` — el sesgo en reposo (`bias`) fue calibrado con éxito.
   - **Bit 2 (`valor 4`):** `tof_ok` — el sensor láser `VL53L0X` responde correctamente.
   - **Bit 3 (`valor 8`):** `giro_activo` — el controlador `giro.c` está ejecutando una maniobra `GIRO:<grados>`.
   - **Bit 4 (`valor 16`):** `tof_bloqueado` — el freno frontal por láser (`< 120 mm`) está activo e impide avanzar.

Por ejemplo, la trama **`SENS:-12,340,7`** indica que el carro tiene un rumbo de **`-1.2°`**, ve un obstáculo o pared a **`340 mm`** (`34.0 cm`) y sus banderas valen `7 = 1 + 2 + 4` (`MPU6050` presente, calibrado y `VL53L0X` operativo, sin giro en curso ni bloqueo frontal).

### Cadena de capacidades al conectar (`CAPS`)

Al abrir el `WebSocket` o al recibir `ESTADO`, el firmware envía después del modo activo (`AUTO` o `MANUAL`) la línea de capacidades:

`CAPS:IMU=1,CAL=1,TOF=1,GIRO=1,RUMBO=1,IMUS=0,FW:6d1b213-20261110`

Esta cadena permite que la aplicación Android verifique en el primer instante si el carro cuenta con giroscopio, si ya está calibrado, si tiene láser activo, si soporta control de rumbo `RUMBO`, cuál es su escala `IMUS` guardada en `NVS` y si la revisión `FW:` coincide con el commit de la aplicación (`6d1b213`).

Dónde está en el código: `carro.c`, `sensores.c`, `firmware_carro.ino`, `Enlace.kt`, `probar_servidor_falso.py`.
