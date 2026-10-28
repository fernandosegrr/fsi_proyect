# Arquitectura general del sistema

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Proyecto:** Carro con navegación asistida por visión y sensores  
**Fecha de consolidación:** 27 de octubre de 2026  

---

## 1. Visión general del vehículo

El sistema consiste en un vehículo terrestre de cuatro ruedas motrices capaz de navegar rutas planificadas en una cuadrícula bidimensional, sortear obstáculos mediante visión computacional y sensores de proximidad, y reportar telemetría en tiempo real a una aplicación móvil Android.

El procesamiento no se concentra en una única computadora monolítica, sino que se distribuye estratégicamente entre tres nodos autónomos comunicados mediante una red inalámbrica local:

```
                            ┌────────────────────────────────┐
                            │        Teléfono Android        │
                            │   (App FSI / Núcleo en C)      │
                            │   192.168.4.10 (cliente DHCP)  │
                            └───────▲────────────────┬───────┘
                                    │                │
                      HTTP /stream  │                │ WebSocket /ws
                      o UDP :4210   │                │ (comandos y SENS)
                                    │                │
            ┌───────────────────────┴──┐     ┌───────▼────────────────────────┐
            │ Freenove ESP32-WROVER    │     │ ESP32 DevKit (WROOM-32)        │
            │ Cámara GC0308 + PSRAM    │     │ Punto de Acceso WiFi (AP)      │
            │ 192.168.4.2 (IP estática)│     │ 192.168.4.1 · Servidor Web :80 │
            └──────────────────────────┘     └───────┬────────────┬───────────┘
                                                     │            │
                                         I2C 21/22   │            │ GPIO 27/26/25/33
                                         (400 kHz)   │            │ PWM (1500 Hz)
                                        ┌────────────┴──┐         ▼
                                        │ MPU6050 (IMU) │     TC1508A
                                        │ VL53L0X (ToF) │  (Driver puente H)
                                        └───────────────┘         │
                                                                  ▼
                                                            4 motores TT
```

---

## 2. Los tres nodos computacionales

### 2.1 Nodo de Control y Tracción: ESP32 DevKit

Es el controlador de tiempo real del vehículo, basado en un microcontrolador de 32 bits de doble núcleo (ESP32-D0WD-V3) con 4 MB de memoria flash.

- **Generador de red:** Crea el punto de acceso WiFi local bajo el estándar 802.11b/g/n en canal fijo, asignándose la dirección `192.168.4.1`.
- **Controlador de potencia:** Transforma las órdenes de alto nivel en señales PWM dirigidas a las compuertas lógicas del driver de motores TC1508A.
- **Adquisición sensorial I2C:** Muestrea a 100 Hz el giroscopio MPU6050 (`0x68`) para mantener el cómputo de rumbo e interroga periódicamente el sensor láser VL53L0X (`0x29`).
- **Seguridad autónoma:** Contiene la lógica del watchdog de comunicación (1000 ms) y la parada de emergencia por proximidad física del láser.

### 2.2 Nodo de Visión: Freenove ESP32-WROVER v3.0

Es una placa independiente montada en la parte frontal superior del chasis, equipada con un sensor de imagen GalaxyCore GC0308 y 4 MB de memoria PSRAM.

- **Operación como cliente:** Se conecta como estación (STA) a la red del carro con la IP estática `192.168.4.2`.
- **Transmisión de cuadros:** Captura imágenes en formato RGB565 a resolución HQVGA (240×176 píxeles), comprime cuadros en JPEG por software gracias a la memoria PSRAM y los sirve por HTTP multipart (`/stream`) o por paquetes compactos UDP (`:4210`).
- **Aislamiento de tareas:** Al ejecutarse en un procesador separado, las exigentes rutinas de lectura de sensor y compresión JPEG no interfieren con la precisión de los tiempos de PWM ni con el muestreo del giroscopio del carro.

### 2.3 Nodo Supervisor: Teléfono Android

Dispositivo móvil montado en el chasis o sostenido por el operador que ejecuta la aplicación del proyecto.

- **Planificación de rutas:** Proporciona un lienzo interactivo con cuadrícula de 20 cm donde se trazan los puntos de destino.
- **Núcleo de navegación (JNI):** Ejecuta nativamente los algoritmos escritos en C99 (`navegador.c`), calculando rumbos, corrigiendo desviaciones laterales y gestionando maniobras de esquive de obstáculos.
- **Inferencia visual:** Analiza el flujo de video en tiempo real mediante segmentación cromática del piso y modelos convolucionales TensorFlow Lite para confirmar la presencia de obstáculos en el corredor de paso.

---

## 3. Topología de red y protocolos

Toda la comunicación ocurre dentro de una subred cerrada clase C (`192.168.4.0/24`):

| Dispositivo | Rol en red | Dirección IP | Puerto / Ruta | Protocolo | Contenido transmitido |
|---|---|---|---|---|---|
| **ESP32 DevKit** | Servidor AP | `192.168.4.1` | `:80 /ws` | WebSocket | Comandos (`FORWARD`, `STOP`, `GIRAR`), telemetría periódica (`SENS`) y pings. |
| **ESP32 DevKit** | Servidor AP | `192.168.4.1` | `:80 /` | HTTP | Página web básica de teleoperación almacenada en flash (`pagina.h`). |
| **Cámara Freenove** | Cliente STA | `192.168.4.2` | `:80 /stream` | HTTP | Transmisión de video continuo MJPEG (240×176 px a ~12 fps). |
| **Cámara Freenove** | Cliente STA | `192.168.4.2` | `:4210` | UDP | Paquetes de celdas de piso (`CEL1`) de 1244 bytes. |
| **Teléfono móvil** | Cliente STA | Asignación DHCP (`.10` a `.20`) | Efímero | TCP/UDP | Conexión bidireccional a los dos nodos del carro. |

---

## 4. Esquema de alimentación separada

Para evitar que el ruido inductivo y las caídas bruscas de tensión generadas por los motores reinicien los microcontroladores (efecto *brownout*), la alimentación está físicamente desacoplada en dos circuitos:

1. **Circuito de control (Electrónica):**
   - Una batería portátil USB (powerbank) de 5 V entrega corriente limpia a la placa ESP32 DevKit y a la cámara Freenove a través de sus puertos micro-USB.
   - El regulador de tensión interno de la placa ESP32 baja los 5 V a 3.3 V regulados para alimentar los sensores I2C (MPU6050 y VL53L0X).
2. **Circuito de potencia (Motores):**
   - Un portapilas con 4 pilas AA entrega aproximadamente 6 V nominales directamente al pin `VDD` del driver TC1508A.
   - La corriente de arranque y bloqueo de los motores fluye exclusivamente a través de las pilas AA.
3. **Masa común (GND):**
   - Las tierras (GND) del circuito de la powerbank y del portapilas de motores están soldadas o unidas rígidamente en un bus común. Esto garantiza una referencia de voltaje compartida de 0 V para que las señales lógicas PWM enviadas desde el ESP32 al driver sean interpretadas correctamente.

---

## 5. Jerarquía de control y autoridad de parada

La toma de decisiones de movimiento sigue un esquema de seguridad por capas:

1. **Capa 1: App y Navegador Nativo:**  
   Determina hacia dónde avanzar y planifica desvíos de 40 cm si la cámara detecta un obstáculo sobre la ruta trazada.
2. **Capa 2: Firmware del Carro (`carro.c`):**  
   Aplica rampas de aceleración a plena potencia (250 ms), balance de potencia por lado y apaga los motores si el canal WebSocket pierde señal por más de 1000 ms (Watchdog).
3. **Capa 3: Sensor ToF Láser (Máxima prioridad física):**  
   El sensor VL53L0X realiza lecturas directas cada ~33 ms. Si detecta un objeto al frente a menos de la distancia crítica de seguridad, el firmware fuerza una parada inmediata (`TOF_PARO`), anulando cualquier comando de avance activo emitido por la app.

---

## 6. Referencias visuales

- [`docs/figuras/fig_01_sistema.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_01_sistema.png): Diagrama esquemático integral del sistema.
- [`docs/figuras/fig_02_capas_firmware.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_02_capas_firmware.png): Separación de capas lógicas y de hardware.
- [`docs/figuras/fig_10_alimentacion.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_10_alimentacion.png): Esquema de conexiones de tierra y circuitos de alimentación.
