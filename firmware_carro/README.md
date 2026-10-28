# Firmware del carro (`firmware_carro`)

Este directorio contiene el software embebido que se ejecuta en la placa **ESP32 DevKit** montada en el chasis del carro.

---

## 1. Propósito y funciones clave

- **Punto de acceso WiFi:** Crea la red local del carro con IP estática `192.168.4.1` (servidor DHCP que reparte de `.10` a `.20`, reservando `.2` para la cámara).
- **Servidor Web y WebSocket:** Atiende en el puerto HTTP `80` la página web embebida (`pagina.h`) y en `/ws` el canal bidireccional WebSocket para recibir comandos y emitir telemetría.
- **Control de motores:** Maneja cuatro motores TT mediante un driver puente H **TC1508A** en los pines GPIO 27, 26, 25 y 33 con señales PWM a 1500 Hz y resolución de 8 bits.
- **Sensores I2C en bus común (SDA 21, SCL 22):**
  - **Giroscopio MPU6050 (0x68):** Muestreo a 100 Hz, calibración de sesgo en reposo y seguimiento de rumbo angular.
  - **Sensor ToF láser VL53L0X (0x29):** Medición de distancia frontal milimétrica y parada de emergencia autónoma (`TOF_PARO`).
- **Seguridad y robustez:**
  - Watchdog de comunicación de **1000 ms**: Si se interrumpe la señal, frena inmediatamente y restablece perfil AUTO.
  - Watchdog de hardware del microcontrolador de **5000 ms**.
  - Pulso periódico keep-alive en GPIO 2 (50 ms cada 2 s) para evitar suspensión de powerbank.
  - Cola protegida de 32 comandos: descarta pings obsoletos antes que comandos críticos y nunca descarta un `STOP`.

---

## 2. Mapa de archivos

| Archivo | Lenguaje | Propósito |
|---|---|---|
| `firmware_carro.ino` | C++ / Arduino | Inicialización (`setup`), bucle principal (`loop`), WiFi AP, servidor WebSocket, I2C de hardware e interrupciones. |
| `config.h` | C | Configuración centralizada: pines, tiempos, velocidades, umbrales y carga de credenciales. |
| `carro.h` / `carro.c` | C estándar | Lógica de comandos de movimiento, cola protegida, perfiles de velocidad, balance y watchdog. Sin dependencias de hardware. |
| `sensores.h` / `sensores.c` | C estándar | Integrador de giroscopio y láser, despacho de comandos de consulta y emisión periódica de `SENS`. |
| `imu.h` / `imu.c` | C estándar | Lectura, integración de velocidad angular en eje Z, filtrado y detección de congelamiento del MPU6050. |
| `tof.h` / `tof.c` | C estándar | Comunicación y lógica de parada de emergencia con el sensor de distancia VL53L0X. |
| `giro.h` / `giro.c` | C estándar | Bucle de control cerrado para giros por ángulo relativo (`GIRAR:<grados>`). |
| `hw_sensores.h` | C | Interfaz de funciones de hardware (`hw_...`) requeridas por la lógica en C. |
| `pagina.h` | C / HTML / JS | Interfaz web de respaldo alojada en memoria flash del ESP32. |
| `secretos.ejemplo.h` | C | Plantilla para definir SSID y contraseña WiFi. |

---

## 3. Compilación y carga

Requiere `arduino-cli` con el core `esp32:esp32` versión `3.3.12` y las bibliotecas `AsyncTCP` y `ESPAsyncWebServer`.

```powershell
# 1. Configurar credenciales locales (ignoradas en git)
Copy-Item firmware_carro\secretos.ejemplo.h firmware_carro\secretos.h
# Editar firmware_carro\secretos.h con el SSID y contraseña deseados

# 2. Compilar
arduino-cli compile --fqbn esp32:esp32:esp32 firmware_carro

# 3. Cargar al ESP32 (especificando puerto COM)
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32 firmware_carro
```

Para verificar la lógica sin hardware físico, véase el directorio [`pruebas_firmware/`](file:///e:/Dev/fsi_proyecto/pruebas_firmware/README.md).
