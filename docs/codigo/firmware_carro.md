# Recorrido de código: Firmware del carro (`firmware_carro`)

**Ubicación:** `firmware_carro/` (`carro.c`, `carro.h`, `sensores.c`, `sensores.h`, `imu.c`, `imu.h`, `tof.c`, `tof.h`, `giro.c`, `giro.h`, `config.h`, `firmware_carro.ino`)  
**Lenguaje:** C estándar (C99) en la lógica de control; C++/Arduino en la integración con el microcontrolador  
**Propósito:** Control de potencia de motores, gestión de la cola protegida de comandos, vigilancia por watchdog, adquisición I2C y bucle cerrado de giro.

---

## 1. Desacoplamiento de la lógica (`carro.c` y `hw_sensores.h`)

La arquitectura de `firmware_carro` prohíbe que la lógica de movimiento invoque directamente APIs propietarias de Arduino como `analogWrite()` o `digitalWrite()`. En su lugar, `carro.c` interactúa mediante funciones virtuales `hw_...` definidas en `carro.h:18-47`:

- `hw_motor_pwm(int pin, int pwm)`: Escribe el ciclo de trabajo (0 a 255) en los pines del driver TC1508A.
- `hw_millis()`: Retorna el reloj de tiempo de ejecución en milisegundos.
- `hw_keepalive(int estado)`: Controla el nivel lógico del pin de mantenimiento de la powerbank (GPIO 2).
- `hw_balance_leer(int perfil)` / `hw_balance_guardar(...)`: Persiste los factores de balance en memoria no volátil.

En la placa física, el archivo `firmware_carro.ino` proporciona la implementación real; en el PC, `pruebas_firmware/sensores_falsos.c` suministra las réplicas simuladas.

---

## 2. Cola protegida y despachador de comandos

`firmware_carro/carro.c:380-460`:
- **El reto de concurrencia:** El servidor WebSocket corre en una tarea asíncrona de FreeRTOS (`AsyncWebSocket`), mientras que el control de motores reside en el hilo principal (`loop()`). Tocar los pines desde ambas tareas generaría condiciones de carrera.
- **Buffer circular protegido:** Cuando la app envía un mensaje, el WebSocket llama a `carro_encolar_comando()`, depositando el texto en una cola de 32 elementos (`COLA_COMANDOS_MAX`).
- **Política de descarte selectivo:** Si la cola se llena debido a una ráfaga de red:
  1. Busca y descarta el comando `PING` más antiguo no procesado.
  2. Si no hay pings, descarta consultas no críticas.
  3. **Los comandos `STOP` y `EBRAKE` nunca se descartan bajo ninguna circunstancia**.
- **Despacho seguro:** En cada vuelta de `loop()`, la función `carro_procesar_cola()` extrae los comandos uno a uno y los ejecuta sincrónicamente.

---

## 3. Temporización del Watchdog y Keep-alive

`firmware_carro/carro.c:230-265`:
- **Comprobación periódica:** En cada ciclo se evalúa:
  $$\Delta t = \text{hw\_millis}() - \text{ultimo\_comando\_movimiento\_ms}$$
- Si $\Delta t > 1000\text{ ms}$ (`TIEMPO_WATCHDOG_MS`) y los motores están encendidos, se invoca `carro_detener()`, se apaga el PWM a 0 % y se restablece el perfil al modo seguro `AUTO`.
- **Pulso de powerbank (Keep-alive):** Cada 2000 ms (`KEEPALIVE_PERIODO_MS`), `carro.c` conmuta a nivel alto el GPIO 2 durante exactamente 50 ms (`KEEPALIVE_PULSO_MS`) para drenar un pulso de corriente que impide la suspensión de baterías portátiles inteligentes.

---

## 4. Subsistema sensorial: `imu.c`, `tof.c`, `giro.c` y `sensores.c`

### 4.1 MPU6050 (`imu.c`): Adquisición y seguimiento de rumbo
- **Configuración I2C:** Inicializa el sensor en `0x68`, valida el identificador WHO_AM_I (acepta 0x68 oficial o 0x70/0x72 de clones) y configura la escala a $\pm 500^\circ/\text{s}$ (65.5 LSB por °/s) con filtro pasabajas digital DLPF a 44 Hz.
- **Calibración inicial de sesgo:** En el encendido, promedia 250 muestras a 100 Hz (2.5 s) con el carro en reposo (`IMU_SESGO_MUESTRAS`).
- **Compensación ZUPT:** Si los motores llevan apagados más de 400 ms y las lecturas son estables, actualiza lentamente el sesgo para eliminar la deriva térmica.
- **Zona muerta:** Lecturas inferiores a $\pm 0.15^\circ/\text{s}$ se filtran a cero para evitar que el ruido eléctrico acumule ángulo artificialmente.

### 4.2 Sensor láser ToF (`tof.c`)
- Lee distancias milimétricas en `0x29`.
- Si el objeto se ubica dentro del margen de colisión inminente, frena de inmediato y despacha la alarma `TOF_PARO:<distancia_mm>`.

### 4.3 Control de giro en bucle cerrado (`giro.c`)
- Al recibir `GIRAR:<grados>`, enciende el motor del lado opuesto a potencia 255.
- En cada ciclo de `loop()`, integra el ángulo girado según el MPU6050.
- Aplica la anticipación aprendida: corta el motor antes de alcanzar los grados solicitados para que la inercia detenga el vehículo con precisión.
- Emite `GIRO_OK` al estabilizarse o `GIRO_TIMEOUT` si supera el tiempo máximo permitido.

### 4.4 Telemetría periódica (`sensores.c`)
- Si hay un cliente conectado por WebSocket y el perfil activo es `AUTO`, emite cada 100 ms el paquete `SENS:<angulo_x10>,<mm>,<banderas>`.

---

## 5. El archivo maestro: `firmware_carro.ino`

`firmware_carro/firmware_carro.ino` unifica todos los módulos con el hardware del ESP32:
- **`setup()` (líneas 405-434):** Inicializa UART serie a 115200 baudios, reduce reloj del CPU a 160 MHz para ahorrar energía, configura 4 canales PWM a 1500 Hz (8 bits), abre el bus I2C a 400 kHz con timeout de 5 ms, inicia el AP WiFi (`192.168.4.1`), arranca el servidor HTTP en el puerto 80 y el WebSocket en `/ws`.
- **`loop()` (líneas 535-551):** Ejecuta estrictamente en orden:
  1. `esp_task_wdt_reset()` (refresca el watchdog de hardware de 5000 ms).
  2. Limpieza de clientes WebSocket inactivos.
  3. Muestreo de sensores I2C (`sensores_actualizar()`).
  4. Procesamiento de la cola de comandos (`carro_procesar_cola()`).
  5. Verificación del watchdog de comunicación (`carro_verificar_watchdog()`).
  6. Actualización del pulso keep-alive (`carro_actualizar_keepalive()`).
  7. Despacho de telemetría hacia WebSocket (`sensores_emitir_mensajes()`).
  8. Diagnóstico de tiempo de ciclo (`LOOP?`).
