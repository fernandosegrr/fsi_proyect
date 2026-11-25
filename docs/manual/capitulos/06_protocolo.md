# 6. Protocolo de comunicación

Este capítulo especifica el protocolo de mensajes de texto que une a la aplicación Android con el firmware de la placa `ESP32 DevKit`. Documenta la capa de transporte, la temporización de latidos y vigilancia, el catálogo completo de órdenes y respuestas —incluidos todos los mensajes incorporados en el cierre **J16** (`RUMBO`, `IMU_ESCALA`, `WD:<n>` y la cadena `CAPS` ampliada)— y muestra la traza comentada de una sesión real.

## 6.1 Capa de transporte y canales de enlace

La comunicación de control entre el teléfono y el carro se realiza mediante mensajes cortos de texto ASCII sobre una sesión persistente **`WebSocket`** en la dirección **`ws://192.168.4.1/ws`** (puerto TCP `80`). El mismo intérprete de comandos (`carro_recibir_comando` y `sensores_comando`) escucha también en el puerto serie `USB` a **`115,200 baudios`**, lo que permite consultar las capacidades y la versión (`CAPS?`, cuya última parte es `FW:<hash>-<fecha>`), leer diagnósticos (`IMU?`, `TOF?`, `LOOP?`) o calibrar el giroscopio (`IMU_CAL`) desde una computadora sin abrir la red `WiFi`. Adicionalmente, el servidor `HTTP` en `http://192.168.4.1/` ofrece una página web embebida de respaldo verificada por `verificar_pagina.py`.

El diseño utiliza texto legible en lugar de tramas binarias opacas por tres razones de ingeniería:

1. **Inspección inmediata:** cualquier mensaje enviado o recibido puede leerse directamente en el monitor serie o en el registro de la aplicación sin decodificadores especiales.
2. **Pruebas deterministas:** el servidor falso en Python (`probar_servidor_falso.py`) y las suites en `C99` pueden inyectar cadenas literales (`"RUMBO:125"`, `"SENS:-32,480,7"`) para verificar cada rama lógica.
3. **Bajo sobrecosto:** con un máximo de `31` caracteres por comando (`COLA_COMANDO_MAX = 32` incluyendo el terminador nulo) y una cadencia de `4` a `10` mensajes por segundo, el tráfico total de control es inferior a `1 KB/s`.

## 6.2 Temporización de latidos, sondas y vigilancia

El protocolo separa estrictamente la **salud de la conexión de red** de la **autorización para mantener los motores encendidos**:

- **Sonda de enlace (`PING` → `PONG` cada `800 ms`):** la clase `Enlace.kt` en Android envía `PING` cada `800 ms` y espera `PONG`. Si no recibe respuesta dentro de `2,000 ms`, declara el socket caído e inicia la reconexión automática. Sin embargo, **`PING` no renueva el *watchdog* de motores** en el firmware: así, aunque la red siga viva, si el hilo de navegación se detiene, el carro frena solo.
- **Latido de movimiento (`250 ms`):** mientras el usuario mantiene presionado un botón en conducción manual o mientras el navegador ejecuta un avance en ruta, la aplicación reenvía el comando activo (`FORWARD`, `REVERSE`, `LEFT` o `RIGHT`) cada **`250 ms`**, dando cuatro oportunidades de renovación antes de que venza el plazo del firmware.
- **Plazo de *watchdog* en el carro (`1,000 ms`) y aviso `WD:<n>` (Cierre J16):** si pasan `1,000 ms` (`TIEMPO_WATCHDOG_MS`) con los motores activos sin recibir un comando de control válido, el firmware apaga los motores (`STOP`), restablece el perfil `AUTO` y envía por `WebSocket` el mensaje **`WD:<contador>`** (por ejemplo, `WD:1`). Al recibir `WD:<n>`, la aplicación Android detiene inmediatamente cualquier ruta semiautónoma para no quedar desincronizada respecto al carro.
- **Vigilancia de silencio de telemetría en Android (`1,500 ms` — Cierre J16):** en modo `AUTO`, el carro envía `SENS` cada `100 ms` (`10 Hz`). Si durante una ruta activa pasan **`1,500 ms`** (`SILENCIO_SENS_MS`) sin recibir ninguna trama `SENS`, `ModoSemiautonomo.kt` aborta la misión y transmite `STOP`.

## 6.3 Catálogo completo de comandos y respuestas (incluyendo J16)

La siguiente tabla reúne todos los comandos aceptados por el firmware y las respuestas o eventos asíncronos que emite:

| Comando enviado al carro | Respuesta / Evento emitido | Función técnica y parámetros |
|---|---|---|
| `FORWARD`, `REVERSE`, `LEFT`, `RIGHT` | *(ninguna)* | Activa el movimiento solicitado. Desde reposo, `FORWARD` y `REVERSE` arrancan con un pulso a plena potencia (`255` durante `250 ms`) y luego pasan al perfil activo. Renuevan el *watchdog* de `1,000 ms`; un comando igual al activo no reinicia nada. En `FORWARD` + `AUTO` con `RUMBO` activo se fija o se mantiene la referencia de rumbo. En el firmware v3 (`SEG=3`, solo compilado), `FORWARD` en `AUTO` no arranca si el frente está confirmado a menos de `180 mm` y contesta `TOF_PARO:<mm>`. |
| `STOP`, `EBRAKE` | *(ninguna)* | Los dos hacen lo mismo: ponen los cuatro `PWM` en `0` (no hay freno eléctrico) y nunca se descartan de la cola de comandos. |
| `PING` | `PONG` | Comprueba que el enlace `WebSocket` sigue abierto; no renueva el *watchdog* de motores. |
| `MODO_MANUAL` / `MODO_AUTO` | `ACK_MANUAL` / `ACK_AUTO` | Cambia el perfil de potencia (`252/255` en `MANUAL`; `207/210` en `AUTO`, izquierdo/derecho). Si había motores en marcha, los detiene antes de cambiar. |
| `BAL_AUTO:<n>`, `BAL_MANUAL:<n>`, `BAL?` | `ACK_BAL_AUTO:<n>` o `ACK_BAL_MANUAL:<n>`, `NAK_BAL`, `BAL:<auto>:<manual>` | Ajusta o consulta el balance entre los dos lados (de `800` a `1000`). |
| `GIRAR:<grados>` | `GIRO_OK:<objetivo>:<real×10>:<ms>:<anticipación>`, `GIRO_TIMEOUT:...` o `GIRO_ERROR:<causa>` | Giro cerrado por giroscopio, de `5` a `200` grados (positivo = izquierda, negativo = derecha). Causas de error: `angulo`, `sin_sesgo`, `sin_imu`, `ocupado` y `abortado`. |
| `RUMBO:ON` / `RUMBO:OFF` | `ACK_RUMBO:1` / `ACK_RUMBO:0` | Habilita o deshabilita el controlador `PI` de rumbo durante los avances rectos `FORWARD` en modo `AUTO`. |
| `RUMBO:<décimas>` | *(ninguna)* | Fija la referencia $\theta_{\text{ref}}$ del controlador `PI` en décimas de grado (p. ej. `RUMBO:-45` para `-4.5°`). |
| `RUMBO?` | `RUMBO:<encendido>,<en_marcha>,<objetivo_x10>,<corrección_mil>,<aprendido_mil>` | Devuelve el estado del control: encendido (`0/1`), en marcha (`0/1`), objetivo (`x10`), corrección actual y sesgo aprendido, ambos en milésimas. |
| `IMU_CAL` | `ACK_IMU_CAL` o `NAK_IMU_CAL`; al terminar, `IMU_CAL_OK:<sesgo×100>` o `IMU_CAL_FALLO` | Recalibra el sesgo en reposo del `MPU6050`; solo con el carro quieto (`NAK_IMU_CAL` si no lo está). |
| `IMU_CERO` | `ACK_IMU_CERO` | Pone el ángulo integrado otra vez en `0`. |
| `IMU_ESCALA:<ppm>` / `IMU_ESCALA?` | `ACK_IMU_ESCALA:<ppm>` o `NAK_IMU_ESCALA` / `IMU_ESCALA:<ppm>` | Fija o consulta el factor de escala del giroscopio en partes por millón, de `-50000` a `+50000` (`±5 %`), solo con el carro detenido; se guarda en memoria no volátil `NVS`. |
| `TOF_USO:0` / `TOF_USO:1` | `ACK_TOF_USO:<n>` | Apaga o enciende (por defecto) el freno de emergencia por láser; el láser sigue midiendo y mandando `SENS`. En la app es «Usar sensor de distancia (láser)». |
| `CAPS?`, `IMU?`, `TOF?`, `ANT?`, `ARR?`, `LOOP?` | `CAPS:...`, `IMU:<ang×10>,<sesgo×100>,<estado>,<id>,<motivo>`, `TOF:<mm>,<banderas>`, `ANT:...`, `ARR:...`, `LOOP:<máx µs>,<excesos>` | Consultas de diagnóstico; no renuevan el *watchdog*. `CAPS` lleva al final la revisión `FW:<hash>-<fecha>`. |
| *(Telemetría periódica)* | `SENS:<ang_x10>,<mm>,<flags>` | Cada `100 ms` con un cliente conectado y el perfil `AUTO` (ver 6.4). |
| *(Evento asíncrono)* | `TOF_PARO:<mm>` | Lo manda el carro solo cuando el láser confirma un obstáculo por debajo de `TOF_PARO_MM` (`120 mm` en v1.1; `150 mm` en v3) avanzando en `AUTO`: frena los motores sin esperar al teléfono. |
| *(Evento asíncrono J16)* | `WD:<contador>` | Lo manda el carro cada vez que vence el *watchdog* de `1,000 ms` por falta de comandos de movimiento con los motores encendidos; el contador son los frenos desde el encendido. |

## 6.4 Estructura de las tramas `SENS` y `CAPS`

### Trama periódica de sensores (`SENS:<ang_x10>,<mm>,<flags>`)

Cada `100 ms`, con un cliente conectado y el perfil `AUTO`, el firmware transmite tres enteros separados por comas:

1. **`<ang_x10>`:** orientación acumulada desde la última puesta a cero (`IMU_CERO`) o calibración (`IMU_CAL`), y `0` mientras el giroscopio no está listo, expresada en **décimas de grado** (por ejemplo, `452` significa `+45.2°` hacia la izquierda; `-18` significa `-1.8°` hacia la derecha). En el cierre **J16**, el núcleo de navegación consume este valor directamente en décimas de grado (`rumbo_x10`) sin truncarlo a grados enteros.
2. **`<mm>`:** distancia frontal medida por el sensor `VL53L0X` en milímetros; vale **`0`** si el láser no está vivo y `1200` cuando no hay nada a la vista («libre», con la bandera de medida válida apagada).
3. **`<flags>`:** máscara de bits entera (de `0` a `31`) que resume el estado de los sensores:
   - **Bit 0 (`valor 1`):** el giroscopio `MPU6050` está listo (calibrado y respondiendo en el bus `I2C`).
   - **Bit 1 (`valor 2`):** el sensor láser `VL53L0X` está vivo.
   - **Bit 2 (`valor 4`):** la última medida del láser es válida (si no, el frente se considera «libre»).
   - **Bit 3 (`valor 8`):** el giroscopio se está calibrando.
   - **Bit 4 (`valor 16`):** hay un giro `GIRAR` en curso.

Por ejemplo, la trama **`SENS:-12,340,7`** indica que el carro tiene un rumbo de **`-1.2°`**, ve un obstáculo o pared a **`340 mm`** (`34.0 cm`) y sus banderas valen `7 = 1 + 2 + 4` (giroscopio listo, láser vivo y medida válida, sin calibrar ni girar).

### Cadena de capacidades al conectar (`CAPS`)

Al abrir el `WebSocket`, cuando algo cambia o al recibir `CAPS?`, el firmware envía la línea de capacidades (aquí, la del firmware v1.1 validado en el carro):

`CAPS:IMU=1,TOF=1,TOFR=<n>,IMUR=<n>,IMUM=<n>,IMUE=<n>,RUMBO=1,IMUS=0,FW:6d1b213-20261110`

Esta cadena permite que la aplicación Android verifique en el primer instante si el carro tiene el giroscopio (`IMU=`) y el láser (`TOF=`) listos, cuántos reintentos hizo cada uno al arrancar (`TOFR`, `IMUR`), el motivo y el estado del giroscopio (`IMUM`, `IMUE`), si soporta el control de rumbo (`RUMBO=1`), cuál es su escala guardada en `NVS` (`IMUS`) y si la revisión `FW:` coincide con el commit de la aplicación (`6d1b213`).

Dónde está en el código: `carro.c`, `sensores.c`, `firmware_carro.ino`, `Enlace.kt`, `probar_servidor_falso.py`.

## 6.5 Autónomo v2: `SEG=2`, `TUSO` y `cam-v2` (`COMPILADO`, no grabado)

Rama `feat/autonomo-v2`. El firmware del carro con estos cambios **solo se compiló** (1 034 526 bytes, 78 %) y pasó 564 pruebas en computadora; no se grabó.

| Cambio | Antes (v1.1) | Con `SEG=2` |
|---|---|---|
| Texto desconocido o vacío | Renovaba el watchdog y quedaba como comando actual: el carro seguía avanzando sin latido, sin freno láser y sin control de rumbo | No mueve nada, no renueva el watchdog, no cambia el comando actual |
| `FORWARD` en perfil AUTO con el frente confirmado a menos de 150 mm | Daba un pulso a plena potencia y el freno lo cortaba una vuelta después | No arranca; contesta `TOF_PARO:<mm>` |
| Freno láser tras un paro | Podía quedar desarmado si el `STOP` del freno y un `FORWARD` se atendían en la misma vuelta | Se rearma en cada avance nuevo |
| `CAPS` | `...,RUMBO=1,IMUS=<ppm>,FW:<hash>-<fecha>` | `...,RUMBO=1,IMUS=<ppm>,SEG=2,TUSO=<0\|1>,FW:<hash>-<fecha>` |

`TOF_PARO` sigue en 120 mm y el watchdog en 1000 ms. Retroceder, girar y el perfil MANUAL no se bloquean. `SENS` no cambia: la app v1.1 exige exactamente tres campos. Las claves nuevas de `CAPS` van antes de `FW:` y no contienen `IMU=`, `TOF=` ni `RUMBO=`, que son las subcadenas que busca la app. `TUSO` dice si el freno láser está en uso (`TOF_USO`).

La cámara `cam-v2` agrega claves al final de `/stats` (ver 5.5). La app nueva funciona con los firmwares actuales, y los firmwares nuevos con la app v1.1.

## 6.6 Autónomo v3: `SEG=3` y `TOFP` (`COMPILADO`, no grabado)

Rama `feat/autonomo-v3`. El firmware del carro con estos cambios **solo se compiló** (1 034 622 bytes, 78 %) y pasó las pruebas en computadora; no se grabó.

| Cambio | v1.1 y `SEG=2` | Con `SEG=3` |
|---|---|---|
| Freno por láser (`TOF_PARO_MM`) | 120 mm | **150 mm** |
| Bloqueo de arranque (`TOF_BLOQUEO_MM`, `SEG=2`) | 150 mm | **180 mm** |
| Un «sin eco» entre dos lecturas cercanas | Rompe la racha de «cerca» (hacían falta otras dos lecturas) | **No la rompe** (`TOF_PERDIDAS_TOLERADAS` = 1); dos «sin eco» seguidos o una lectura lejana real sí |
| `CAPS` | `...,SEG=2,TUSO=<0\|1>,FW:...` | `...,SEG=3,TUSO=<0\|1>,TOFP=150,FW:...` |

**Por qué.** El pulso de arranque (255 por 250 ms, unos 9.5 cm) y la distancia del sensor al frente del carro (`e`, **NO MEDIDA**) hacen que 120 mm pueda no alcanzar si `e` pasa de unos 2 cm (modelo propio, `docs/investigacion_autonomo/10_seguridad_funcional.md`; confianza media). Más conservador, nunca menos. Volver a los valores anteriores: compilar con `-DTOF_PARO_MM=120 -DTOF_BLOQUEO_MM=150` (el compilador rechaza un bloqueo menor o igual al freno).

**Compatibilidad.** `TOFP=` no contiene «IMU=», «TOF=» ni «RUMBO=» (la app v1.1 busca esas subcadenas) y la app nueva no lo necesita. `SENS` no cambia. El peor `CAPS` mide ~119 caracteres contra 128 del buffer. El servidor falso del repositorio imita `SEG=3` y las pruebas del firmware cubren los umbrales y la racha. `python scripts/verificar_version.py --puerto COM6 --solo-fw` comprueba el firmware cargado sin teléfono.
