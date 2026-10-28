# Protocolo de comunicación del carro FSI

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Canal principal:** WebSocket sobre TCP/IP (`ws://192.168.4.1/ws`)  
**Formato:** Texto plano en codificación ASCII/UTF-8, sin envoltorios JSON ni binarios  
**Fecha de referencia:** 27 de octubre de 2026  

---

## 1. Principios de diseño

1. **Simplicidad de procesamiento:** El ESP32 es un microcontrolador que no debe desperdiciar ciclos ni memoria dinámica parseando estructuras JSON complejas. Los mensajes son palabras clave sencillas delimitadas por retornos de línea o separadores de dos puntos (`:`).
2. **Asincronía total:** A través de un único socket WebSocket bidireccional, la app envía órdenes de movimiento y el carro emite ráfagas de telemetría de sensores sin requerir un esquema de petición-respuesta bloqueante.
3. **Prioridad absoluta para la seguridad:** Los comandos de detención (`STOP` y `EBRAKE`) tienen vía libre en la cola y se ejecutan de manera inmediata, incluso si la cola de mensajes se encuentra saturada.

---

## 2. Comandos de control de movimiento

| Comando enviado | Efecto en motores | Respuesta del carro | Notas de seguridad |
|---|---|---|---|
| `FORWARD` | Avanza en línea recta aplicando el balance de potencia del perfil activo. | Ninguna | Arranca a potencia 255 durante 250 ms para vencer inercia inicial y luego baja al balance. |
| `REVERSE` | Retrocede en línea recta. | Ninguna | Mismo comportamiento de arranque pleno y balance. |
| `LEFT` | Pivote hacia la izquierda (rueda derecha empuja adelante a potencia 255; rueda izquierda parada). | Ninguna | No se aplica escalonamiento en pivotes. |
| `RIGHT` | Pivote hacia la derecha (rueda izquierda empuja adelante a potencia 255; rueda derecha parada). | Ninguna | Giro con un solo lado activo. |
| `STOP` | Apaga el PWM de los cuatro pines de motor (0 %). | Ninguna | Se ejecuta SIEMPRE. Si se envía repetido, limpia cualquier residuo en los registros. |
| `EBRAKE` | Freno de emergencia (idéntico a `STOP`). | Ninguna | Comando de máxima prioridad para botones de pánico. |

---

## 3. Modos de conducción y calibración de balance

El carro cuenta con dos perfiles de velocidad independientes (`firmware_carro/config.h`):

| Comando enviado | Respuesta | Descripción del perfil |
|---|---|---|
| `MODO_AUTO` | `ACK_AUTO` | Perfil para navegación autónoma y pruebas de ruta. Velocidad base 210, motor izquierdo ajustado a 207. Envío activo de telemetría `SENS`. Si el carro estaba en marcha al conmutar, primero se detiene. |
| `MODO_MANUAL` | `ACK_MANUAL` | Perfil de velocidad máxima para conducción manual desde la app (255 derecho / 252 izquierdo medido). No emite ráfagas `SENS` para no saturar el canal de teleoperación. |
| `BAL_AUTO:NNN` | `ACK_BAL_AUTO:NNN` o `NAK_BAL` | Ajusta el factor de balance del perfil AUTO en milésimas (rango permitido: 800 a 1000). Se guarda en memoria no volátil del ESP32. |
| `BAL_MANUAL:NNN` | `ACK_BAL_MANUAL:NNN` o `NAK_BAL` | Ajusta el factor de balance del perfil MANUAL en milésimas (800 a 1000) y lo almacena. |
| `BAL?` | `BAL:AAA:MMM` | Consulta los balances guardados: `AAA` = factor AUTO, `MMM` = factor MANUAL (ej. `BAL:986:988`). |

---

## 4. Giro de precisión en bucle cerrado

Para realizar giros angulares exactos sin depender de aproximaciones de tiempo, el firmware implementa un control cerrado supervisado por el giroscopio MPU6050:

- **Sintaxis:** `GIRAR:<grados>`
- **Rango admitido:** `5` a `200` grados enteros.
- **Convenio de signo:** Grados positivos indican giro a la **izquierda** (sentido antihorario visto desde arriba); grados negativos indican giro a la **derecha**.
- **Respuestas posibles:**
  - `GIRO_OK`: El vehículo completó el giro angular dentro de la tolerancia programada.
  - `GIRO_TIMEOUT`: Expiró el tiempo límite de maniobra antes de alcanzar el ángulo deseado (ej. ruedas patinando o carro atorado).
  - `GIRO_ERROR:<motivo>`: Falla en la ejecución (ej. `GIRO_ERROR:sin_imu` si el giroscopio no está disponible o se encuentra descalibrado).

---

## 5. Telemetría periódica y eventos de seguridad

### 5.1 Trama de telemetría `SENS`

En perfil `AUTO`, si hay un cliente WebSocket enlazado, el ESP32 transmite periódicamente cada **100 ms** un mensaje de estado compuesto por tres campos:

```text
SENS:<angulo_x10>,<distancia_mm>,<banderas>
```

Ejemplo: `SENS:450,320,7`

- **Campo 1 (`angulo_x10`):** Ángulo actual acumulado por el giroscopio multiplicado por 10. En el ejemplo, `450` representa `45.0` grados.
- **Campo 2 (`distancia_mm`):** Lectura milimétrica del sensor de distancia láser VL53L0X. En el ejemplo, `320` representa 320 mm (32 cm). Si no hay objeto en rango, reporta un valor fuera de rango.
- **Campo 3 (`banderas`):** Entero decimal que representa una máscara de bits con el estado del subsistema:
  - **Bit 0 (valor 1):** MPU6050 inicializado y en estado operativo (`IMU_LISTO`).
  - **Bit 1 (valor 2):** Sensor láser VL53L0X vivo y respondiendo transacciones I2C.
  - **Bit 2 (valor 4):** Medición de distancia válida y dentro de rango óptico confiable.
  - **Bit 4 (valor 16):** Maniobra de giro en curso (`GIRO_ACTIVO`).

*Nota:* Un valor de banderas `7` (1 + 2 + 4) indica salud perfecta: IMU lista, láser respondiendo y distancia válida.

### 5.2 Notificación de parada de emergencia ToF

Cuando el sensor láser detecta un obstáculo a una distancia inferior al umbral crítico mientras el vehículo avanza, el firmware detiene los motores de manera autónoma y difunde por el WebSocket:

```text
TOF_PARO:<distancia_mm>
```

---

## 6. Monitoreo de latencia y Watchdog

- **Comando `PING`:** La aplicación Android envía periódicamente `PING` para estimar el retardo de la red inalámbrica.
- **Respuesta `PONG`:** El ESP32 responde inmediatamente `PONG`.
- **Regla crítica del Watchdog:** El comando `PING` **NO renueva el temporizador del watchdog**. Para que el carro mantenga los motores encendidos durante un avance manual, la app debe repetir el comando `FORWARD` cada 250 ms. Si pasan **1000 ms** sin recibir un comando de movimiento real, el vigilante apaga los motores automáticamente.

---

## 7. Consultas de diagnóstico y sensores

| Comando | Formato de respuesta | Descripción |
|---|---|---|
| `CAPS?` | `CAPS:IMU=<0\|1>,TOF=<0\|1>` | Reporta qué sensores fueron detectados en el bus I2C durante el arranque. |
| `IMU?` | `IMU:estado,angulo,sesgo,id` | Diagnóstico del giroscopio: estado, ángulo actual, sesgo estimado y registro WHO_AM_I. |
| `TOF?` | `TOF:distancia_mm,valida` | Diagnóstico de distancia cruda del láser. |
| `IMU_CAL` | `ACK_IMU_CAL` o `NAK_IMU_CAL` | Fuerza recalibración del sesgo del giroscopio (solo admitido con motores parados por >300 ms). |
| `IMU_CERO` | `ACK_IMU_CERO` | Establece el ángulo actual del giroscopio como nueva referencia de 0 grados. |
| `TOF_USO:<0\|1>` | `ACK_TOF_USO:<0\|1>` | Activa (`1`) o desactiva temporalmente (`0`) la autoridad de parada de emergencia del láser ToF. |
| `LOOP?` | `LOOP:peor_us,excesos` | Reporta el peor tiempo de ejecución de una vuelta del bucle `loop()` en microsegundos. |
| `ARR?` | `ARR:reintentos,motivo` | Registro de incidencias ocurridas durante la inicialización de periféricos. |

---

## 8. Gestión y descarte en la cola de comandos

El firmware dispone de un buffer circular de 32 ranuras (`COLA_COMANDOS_MAX`). Si una ráfaga saturara la cola:
1. Se descartan prioritariamente mensajes `PING` antiguos no despachados.
2. Si continúa saturada, se descartan comandos de consulta de diagnóstico.
3. **Bajo ninguna circunstancia se descarta un comando `STOP` o `EBRAKE`**.

---

## 9. Referencias visuales

- [`docs/figuras/fig_03_flujo_comando.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_03_flujo_comando.png): Recorrido de los mensajes desde la app hasta el hardware.
- [`docs/figuras/fig_11_protocolo.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_11_protocolo.png): Secuencia temporal y periodicidad de mensajes WebSocket.
