# 4. Firmware del carro

Este capítulo detalla la arquitectura interna del programa que corre en la placa `ESP32 DevKit` (`FW:6d1b213-20261110`), su separación respecto al hardware físico mediante interfaces en `C99`, y el funcionamiento matemático de sus lazos de control y seguridad. Incluye de manera íntegra las innovaciones del cierre **J16**: el controlador proporcional-integral de rumbo en rectas (`rumbo.c`), el factor de escala del giroscopio en partes por millón (`imu.c`), la normalización de freno en giros largos (`giro.c`) y el aviso de *watchdog* (`WD:<n>`).

## 4.1 Separación entre hardware (`hw_*`) y lógica en C99

El código del firmware está dividido deliberadamente en dos estratos:

1. **Capa de plataforma (`firmware_carro.ino`):** es el único archivo que conoce las bibliotecas de Arduino y del `ESP32` (`WiFi.h`, `AsyncTCP.h`, `ESPAsyncWebServer.h`, `Wire.h`, `Preferences.h`, `esp_task_wdt.h`). Configura el punto de acceso `CarroFSI`, atiende el servidor `WebSocket` en `/ws`, inicializa los periféricos `PWM` e `I2C` e implementa las funciones puente con prefijo `hw_*` (`hw_aplicar_motores`, `hw_millis`, `hw_micros`, `hw_imu_leer_gz`, `hw_tof_leer_mm`, `hw_nvs_guardar_escala_imu`, `hw_enviar_texto`, etc.).
2. **Módulos lógicos en `C99` puro (`carro.c`, `sensores.c`, `imu.c`, `tof.c`, `giro.c`, `rumbo.c`):** no incluyen `<Arduino.h>` ni tocan registros físicos; toda su interacción con el exterior ocurre invocando las funciones `hw_*`. Gracias a este diseño, los seis módulos se compilan en computadora personal con `gcc` y se someten a **540 casos de prueba unitarios e integrados** (`pruebas_firmware`) simulando escenarios extremos (saturación de cola, fallos de bus `I2C`, ruido de giroscopio, bloqueo de láser o pérdida de `WiFi`) sin necesidad de energizar el vehículo.

[FIGURA:comandos|Flujo de órdenes desde la aplicación hasta los motores a través de la cola determinista y retorno de telemetría SENS y avisos de seguridad.]

## 4.2 Cola de comandos, perfiles PWM y pulso de arranque

En el `ESP32`, los eventos de red de `ESPAsyncWebServer` se ejecutan en una tarea de fondo (`async_tcp`) distinta de la tarea donde corre `loop()`. Si el callback de red modificara directamente los pines `PWM` o el estado de los controladores mientras `loop()` actualiza los sensores, se producirían condiciones de carrera.

Para evitarlo, el callback de `WebSocket` solo invoca `carro_encolar_comando`, que deposita el mensaje en una cola circular protegida por sección crítica de **32 ranuras de 32 bytes** (`COLA_COMANDOS_CAP = 32`). En cada iteración de `loop()`, `carro_procesar_cola` extrae y ejecuta los mensajes en orden FIFO con una **regla de prioridad de seguridad**:

- Si la cola está llena y llega un comando normal, se descarta el mensaje entrante e incrementa el contador de descartes.
- Si la cola está llena y llega **`STOP` o `EBRAKE`**, el firmware busca en la cola un mensaje `PING` antiguo y lo reemplaza; si no hay ninguno, sobrescribe la última ranura para garantizar que **ninguna orden de paro se pierda jamás**.

### Perfiles de potencia y compensación de asimetría mecánica

El firmware ofrece dos perfiles de ciclo de trabajo `PWM` (rango de 8 bits: `0` a `255`):

| Parámetro | Constante en `config.h` | Valor | Propósito y estado de validación |
|---|---|---|---|
| **Perfil `MANUAL` izquierdo** | `PWM_MANUAL_IZQ` | `252` (`98.8 %`) | Calibrado físicamente en pista: redujo el desvío lateral en recta de `150 cm` de `14.5 cm` (con `255/255`) a `3.0 cm` (`VALIDADO EN EL CARRO`). |
| **Perfil `MANUAL` derecho** | `PWM_MANUAL_DER` | `255` (`100.0 %`) | Referencia máxima para respuesta ágil con botones manuales. |
| **Perfil `AUTO` izquierdo** | `PWM_AUTO_IZQ` | `207` (`81.2 %`) | Usado en rutas semiautónomas (`VALIDADO EN EL CARRO` dentro del esquive B6; su balance aislado en recta libre permanece `NO MEDIDO`). |
| **Perfil `AUTO` derecho** | `PWM_AUTO_DER` | `210` (`82.4 %`) | Velocidad moderada (`~82 %`) que da margen superior e inferior al control `RUMBO` y tiempo de reacción a la cámara y al láser. |
| **Pulso de arranque** | `PWM_ARRANQUE` / `TIEMPO_ARRANQUE_MS` | `255` durante `250 ms` | Rompe la fricción estática de las cuatro cajas reductoras `TT` al pasar de reposo a movimiento (evita que los motores zumben sin girar). |

## 4.3 Control proporcional-integral de rumbo en rectas (`rumbo.c` — Cierre J16)

Aunque el ajuste estático de `PWM` (`252/255` o `207/210`) corrige la asimetría media de los motores en una condición dada, el rozamiento desigual de los rodillos *mecanum*, las juntas del piso y la descarga gradual de las pilas `AA` provocan que el carro tienda a curvarse en tramos largos. Para corregir esta deriva en tiempo real sin necesidad de detenerse, el cierre **J16** incorporó el módulo `rumbo.c`, que ejecuta un controlador **proporcional-integral (PI)** a `50 Hz` (`SENS_CONTROL_MS = 20 ms`) durante cualquier orden `FORWARD` en perfil `AUTO` cuando el control de rumbo está habilitado (`RUMBO:ON`, activo por defecto al iniciar).

Sea $\theta_{\text{ref}}$ la referencia de orientación en décimas de grado (fijada automáticamente al ángulo actual al iniciar `FORWARD`, o actualizada al vuelo por el teléfono mediante `RUMBO:<décimas>`) y sea $\theta$ el ángulo integrado del giroscopio en décimas de grado. El error angular envuelto al intervalo $[-1800, +1800]$ décimas de grado es:

$$e_{x10} = \text{envolver}_{1800}\left(\theta_{\text{ref}} - \theta\right)$$

Como en la convención del vehículo un ángulo positivo ($+\theta$) corresponde a girar hacia la **izquierda**, un error $e_{x10} > 0$ significa que el carro se ha desviado hacia la **derecha** ($\theta < \theta_{\text{ref}}$) y necesita girar hacia la izquierda: es decir, **frenar ligeramente el lado izquierdo y acelerar el lado derecho**. El controlador calcula el ajuste diferencial en milésimas del `PWM` base (`ajuste_mil`) mediante tres términos:

1. **Banda muerta (`RUMBO_BANDA_MUERTA_X10 = 2`, es decir, `0.2°`):** si $|e_{x10}| \le 2$, el error instantáneo se toma como cero para no excitar oscilaciones por ruido del sensor.
2. **Término proporcional ($P$):** con ganancia `RUMBO_KP_MIL_GRADO = 10` (`1.0 %` de ajuste `PWM` por cada grado de error):
   $$P_{\text{mil}} = \frac{10 \cdot e_{\text{ef}}}{10} = e_{\text{ef}}$$
3. **Término integral ($I$) con anti-*windup* y aprendizaje entre tramos:** con constante de tiempo `RUMBO_TI_MS = 1500 ms`, acumula `integral_x10_ms` (acotada para no superar por sí sola el tope de salida):
   $$I_{\text{mil}} = \frac{10 \cdot \text{integral\_x10\_ms}}{10 \cdot 1500}$$
4. **Sesgo aprendido persistente entre tramos (`aprendido_mil`):** cuando termina un tramo de avance (`rumbo_detener`), el valor alcanzado por el integrador $I_{\text{mil}}$ no se pierde: se transfiere al acumulador `aprendido_mil` (acotado a `±120` milésimas, `±12 %`). Así, en el siguiente tramo recto de la misma ruta, el carro arranca aplicando desde el primer milisegundo la compensación de asimetría que ya descubrió para ese piso y esas baterías.

La salida total $u_{\text{mil}} = \text{clamp}_{[-150, +150]}\left(\text{aprendido\_mil} + P_{\text{mil}} + I_{\text{mil}}\right)$ se limita a `±150` milésimas (`±15 %`, `RUMBO_MAX_MIL = 150`) y modifica simétricamente los ciclos de trabajo base ($B_L = 207$, $B_R = 210$):

$$\text{PWM}_{\text{izq}} = \text{clamp}_{[140, 255]}\left(B_L - \text{round}\left(\frac{B_L \cdot u_{\text{mil}}}{1000}\right)\right), \qquad \text{PWM}_{\text{der}} = \text{clamp}_{[140, 255]}\left(B_R + \text{round}\left(\frac{B_R \cdot u_{\text{mil}}}{1000}\right)\right)$$

### Ejemplo numérico del controlador PI de rumbo

Supóngase que el carro avanza en perfil `AUTO` ($B_L = 207$, $B_R = 210$) con referencia $\theta_{\text{ref}} = 0.0^\circ$ y el giroscopio detecta que el chasis se desvió $3.0^\circ$ hacia la derecha ($\theta = -3.0^\circ \Rightarrow e_{x10} = +30$), tras mantener ese desvío durante $0.6\text{ s}$ ($600\text{ ms}$) con `aprendido_mil = 0`:

- El término proporcional aporta $P_{\text{mil}} = +30$ milésimas (`+3.0 %`).
- El integrador acumula $30 \times 600 = 18{,}000\text{ décimas}\cdot\text{ms}$, por lo que $I_{\text{mil}} = 18{,}000 / 1{,}500 = +12$ milésimas (`+1.2 %`).
- El ajuste total es $u_{\text{mil}} = 30 + 12 = +42$ milésimas (`+4.2 %`).
- El motor izquierdo reduce su `PWM` a $207 - \text{round}(207 \times 42 / 1000) = 207 - 9 = 198$, mientras el motor derecho aumenta su `PWM` a $210 + \text{round}(210 \times 42 / 1000) = 210 + 9 = 219$, generando un par corrector suave que devuelve el carro a $0.0^\circ$.

Además, si el operador o el navegador cambia la referencia con `RUMBO:<décimas>` en más de `1.5°` (`RUMBO_UMBRAL_RECTO_X10 = 15`) durante los primeros `250 ms` de avance, `sensores_aplicar_motores_avance` **cancela de inmediato el pulso de arranque a `255`** para poder aplicar la diferencia de `PWM` sin esperar a que termine el cuarto de segundo inicial.

## 4.4 Giroscopio `MPU6050`, factor de escala en ppm (`imu.c`) y lazo de giro (`giro.c`)

El módulo `imu.c` muestrea el registro de velocidad angular en $Z$ del `MPU6050` cada `5 ms` (`200 Hz`, filtro pasa-bajas digital interno `DLPF` a `42 Hz`, escala de `±500 °/s` con sensibilidad nominal de `65.5 LSB/(°/s)`). Al encender o al recibir el comando `CAL_IMU`, el firmware exige que el carro permanezca inmóvil durante `100` muestras (`~500 ms`); si la dispersión pico a pico no supera `80 LSB` (`~1.2 °/s`), promedia las lecturas para fijar el sesgo en reposo (`bias`).

### Factor de escala en partes por millón (`IMU_ESCALA` — Cierre J16)

Los giroscopios MEMS de bajo costo presentan una tolerancia de sensibilidad típica de `±1 %` a `±3 %` respecto al valor nominal de hoja de datos (`65.5 LSB/(°/s)`). En un giro único de `90°`, un error de escala del `2 %` equivale a apenas `1.8°`, pero tras acumular cuatro giros en un cuadrado o múltiples maniobras de esquive, el error sistemático se suma hasta alcanzar de `7°` a `10°`.

Para corregirlo sin usar aritmética de punto flotante lenta ni perder resolución, el cierre **J16** añadió en `imu.c` el parámetro `escala_ppm`, expresado en **partes por millón** dentro del intervalo seguro `[-50000, +50000]` (`±5 %`) y persistido en la memoria no volátil `NVS` de la `ESP32` (espacio `carro_imu`, clave `escala_ppm`). En cada paso de integración de duración $\Delta t_{\mu\text{s}}$ (acotado a `IMU_DT_MAX_US = 100,000 us`), el incremento bruto de ángulo se escala con aritmética entera de 64 bits:

$$\Delta\theta_{\text{corr}} = \frac{\Delta\theta_{\text{bruto}} \cdot \left(1{,}000{,}000 + \text{escala\_ppm}\right)}{1{,}000{,}000}$$

Este factor se consulta con `IMU_ESCALA?`, se ajusta con `IMU_ESCALA:<ppm>` y se reporta automáticamente dentro del mensaje de capacidades `CAPS` como `IMUS=<ppm>`.

### Controlador de giro cerrado (`giro.c`) y normalización para giros largos

Cuando el firmware recibe `GIRO:<grados>` (con `<grados>` entre `-360` y `+360`, distinto de `0`), `giro.c` toma el control de los motores a `50 Hz`:

1. Mientras el error angular $|e|$ supera el umbral de frenado, aplica `PWM_GIRO_MAX` (`207/210` en `AUTO` o `252/255` en `MANUAL`, precedido por el pulso de arranque si parte del reposo).
2. Dentro de la zona de frenado proporcional, reduce linealmente el `PWM` hasta el mínimo capaz de vencer la fricción en giro (`GIRO_PWM_MIN = 175`).
3. En el cierre **J16**, para evitar que un giro largo (por ejemplo, `180°` o `360°`) llegue a la zona de frenado con mayor momento angular y se pase del objetivo (*overshoot*), `umbral_freno_x10` amplía automáticamente la ventana base de `15.0°` (`GIRO_FRENO_GRADOS_X10 = 150`) a razón de **`+1 %` por cada grado por encima de `45°`** (`GIRO_LARGO_POR_GRADO = 1`, hasta el doble del umbral base). Por ejemplo, para un giro de `90°` el frenado comienza a $15.0^\circ \times 1.45 = 21.7^\circ$ antes de la meta, y para `180°` comienza a $15.0^\circ \times 2.0 = 30.0^\circ$.
4. Si el carro rebasa la meta por inercia, `giro.c` invierte el sentido de giro a potencia mínima (`175`) para regresar. El giro se declara completado (`GIRO_FIN:<angulo_entero>`) cuando el error permanece dentro de **`±2.0°`** (`GIRO_TOLERANCIA_X10 = 20`) durante **`80 ms` continuos** (`GIRO_VENTANA_ESTABLE_MS`), o bien se aborta (`GIRO_TIMEOUT` o `GIRO_ERR_IMU`) si se agota el plazo de seguridad (`800 ms + 25 ms/°`).

## 4.5 Freno láser local (`tof.c`), *watchdog* (`WD:<n>`) y diagnóstico `LOOP?`

La seguridad local del firmware se completa con tres mecanismos deterministas:

- **Freno reflejo por distancia láser (`tof.c`):** el sensor `VL53L0X` se lee cada `20 ms` (`50 Hz`), descartando lecturas fuera de `[20, 2000] mm` y exigiendo confirmación de dos lecturas consecutivas o salto menor a `150 mm` para filtrar reflejos espurios. Si durante un avance frontal en modo `AUTO` la distancia filtrada cae por debajo de **`120 mm`** (`TOF_PARO_MM`), el firmware ejecuta `STOP` de inmediato, activa el bloqueo frontal y envía `TOF_PARO:<mm>`. El bloqueo solo se libera con histéresis cuando la vía supera **`150 mm`** (`TOF_LIBRE_MM`).
- **Aviso explícito de *watchdog* de comunicación (`WD:<n>` — Cierre J16):** si pasan `1,000 ms` (`TIMEOUT_COMANDO_MS`) con los motores encendidos sin que llegue ningún comando válido de control (los mensajes `PING` mantienen vivo el socket pero **no** renuevan el *watchdog* de motores), `carro_vigilar_comunicacion` detiene el carro, incrementa el contador de paros por vigilancia y transmite por `WebSocket` el aviso **`WD:<contador>`** para que el teléfono cancele cualquier ruta en curso.
- **Instrumentación de tiempos de bucle (`LOOP?`):** al enviar `LOOP?`, el firmware responde `LOOP:<prom_us>,<max_us>,<lentos>`, donde `lentos` cuenta cuántas llamadas a `sensores_actualizar` superaron `5,000 us` (`5 ms`). Como se verificó en la auditoría (Capítulo 12), el promedio real ronda `~690 us`, y los ciclos que superan `5 ms` (`max_us` de `28` a `41 ms`) corresponden a las transacciones `I2C` del sensor `VL53L0X` cuando entrega una medición nueva; el integrador `imu.c` absorbe esas pausas multiplicando por el $\Delta t$ real medido con `micros()`.

Dónde está en el código: `firmware_carro.ino`, `config.h`, `carro.h`, `carro.c`, `sensores.h`, `sensores.c`, `rumbo.h`, `rumbo.c`, `imu.h`, `imu.c`, `giro.h`, `giro.c`, `tof.h`, `tof.c`.
