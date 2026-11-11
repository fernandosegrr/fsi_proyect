# 3. Hardware y conexiones

Este capítulo describe los componentes mecánicos, electrónicos y sensores que integran el vehículo físico, así como el mapa exacto de conexiones entre la placa `ESP32 DevKit`, el puente H `TC1508A` y el bus `I2C`. También deja asentado qué circuitos auxiliares están solamente declarados en software pero permanecen sin medición física en el prototipo actual.

## 3.1 Plataforma mecánica y etapa de tracción

El vehículo utiliza un chasis rígido de cuatro ruedas omnidireccionales (*mecanum*) acopladas a cuatro motoreductores de corriente directa tipo **TT** (relación de reducción típica 1:48, tensión nominal de `3 V` a `6 V`). A diferencia de un vehículo con dirección tipo Ackermann, la orientación del carro se modifica por **tracción diferencial** entre el costado izquierdo y el costado derecho:

- **Avance recto (`FORWARD`):** los dos motores izquierdos y los dos motores derechos giran hacia adelante.
- **Retroceso (`BACKWARD`):** ambos costados giran hacia atrás.
- **Giro sobre su propio eje (`LEFT` / `RIGHT`):** un costado gira hacia adelante y el costado opuesto gira hacia atrás (pivote diferencial).
- **Freno activo (`EBRAKE`):** ambos terminales de cada canal se ponen en alto (`HIGH` / `HIGH`) para cortocircuitar la fuerza contraelectromotriz del inducido y detener el giro con rapidez.
- **Paro libre (`STOP`):** ambos terminales se ponen en bajo (`LOW` / `LOW`), cortando toda corriente hacia los motores.

Como el puente H empleado (`TC1508A`) dispone de **dos canales de potencia** (y no de cuatro canales independientes), los dos motores del lado izquierdo están conectados en paralelo al **Canal 1 (`OUT1` / `OUT2`)** y los dos motores del lado derecho están conectados en paralelo al **Canal 2 (`OUT3` / `OUT4`)**. Por esta razón eléctrica, aunque las llantas físicas tienen rodillos oblicuos tipo *mecanum*, el prototipo opera cinemáticamente como un vehículo diferencial de cuatro ruedas motrices y no puede realizar desplazamiento lateral puro (*strafe*) sin girar.

[FIGURA:lamina_carro|Vistas físicas del vehículo LIDIA: (a) frente con cámara y sensor láser, (b) distribución superior de placas y baterías, (c) chasis y ruedas, (d) cableado en protoboard.]

## 3.2 Electrónica de control, sensores I2C y cámara

El inventario electrónico embarcado sobre el chasis consta de cinco módulos principales:

| Componente | Modelo / Especificación | Interfaz y dirección / pines | Función en el vehículo | Estado de evidencia |
|---|---|---|---|---|
| **Microcontrolador principal** | `ESP32 DevKit V1` (`ESP-WROOM-32`, 240 MHz, WiFi 2.4 GHz) | `PWM`: `GPIO 27, 26, 25, 33`<br>`I2C`: `GPIO 21 (SDA), 22 (SCL)` | Punto de acceso `CarroFSI` (`192.168.4.1`), control de tracción, lectura de sensores y seguridad local. | `VALIDADO EN EL CARRO` |
| **Driver de motores** | Puente H doble `TC1508A` (reemplazo funcional de `L298N` en baja tensión) | `IN1=27`, `IN2=26` (izq.)<br>`IN3=25`, `IN4=33` (der.)<br>`VDD` a `4×AA`; `GND` común | Conmuta la corriente del portapilas `4×AA` hacia los 4 motores `TT` con modulación `PWM` a `1,000 Hz` (8 bits, `0..255`). | `VALIDADO EN EL CARRO` |
| **Giroscopio MEMS** | `MPU6050` (6 ejes; se emplea el eje $Z$ de velocidad angular $\omega_z$) | Bus `I2C` a `400 kHz`<br>Dirección `0x68`<br>Alimentación `3.3 V` | Mide velocidad de guiñada en el rango de `±500 °/s` (`65.5 LSB/(°/s)`), muestreado a `200 Hz` para giros y rumbo. | `VALIDADO EN EL CARRO` |
| **Sensor de distancia** | Láser de tiempo de vuelo (*ToF*) `VL53L0X` (`940 nm`) | Bus `I2C` a `400 kHz`<br>Dirección `0x29`<br>Alimentación `3.3 V` | Mide distancia frontal en milímetros (ventana válida `20..2000 mm`) cada `20 ms`; dispara `TOF_PARO` bajo `120 mm`. | `VALIDADO EN EL CARRO` |
| **Módulo de cámara** | `Freenove ESP32-WROVER` con cámara `GC0308` y `PSRAM` | Enlace `WiFi` (`192.168.4.2`)<br>`D0..D7`, `XCLK=21`, `SIOD=26`, `SIOC=27` internos | Captura imágenes `HQVGA` (`240×176` px) y las sirve por `HTTP` (`:80/:81`) o `UDP` (`:4210`) al teléfono. | `VALIDADO EN EL CARRO` |

## 3.3 Mapa detallado de pines y conexiones

Todas las asignaciones de pines del microcontrolador de control están centralizadas en `config.h`. La siguiente tabla resume cada conexión física del chasis:

| Pin en `ESP32 DevKit` | Constante en firmware | Destino físico | Nivel / Protocolo | Observaciones de montaje |
|---|---|---|---|---|
| `GPIO 27` | `PIN_MOTOR_IZQ_A` | `TC1508A IN1` | `PWM` `1000 Hz` (canal `0`) | Avance del par de motores izquierdos. |
| `GPIO 26` | `PIN_MOTOR_IZQ_B` | `TC1508A IN2` | `PWM` `1000 Hz` (canal `1`) | Retroceso del par de motores izquierdos. |
| `GPIO 25` | `PIN_MOTOR_DER_A` | `TC1508A IN3` | `PWM` `1000 Hz` (canal `2`) | Avance del par de motores derechos. |
| `GPIO 33` | `PIN_MOTOR_DER_B` | `TC1508A IN4` | `PWM` `1000 Hz` (canal `3`) | Retroceso del par de motores derechos. |
| `GPIO 21` | `PIN_I2C_SDA` | `SDA` de `MPU6050` y `VL53L0X` | `I2C` `400 kHz` (`3.3 V`) | Línea de datos compartida en protoboard. |
| `GPIO 22` | `PIN_I2C_SCL` | `SCL` de `MPU6050` y `VL53L0X` | `I2C` `400 kHz` (`3.3 V`) | Línea de reloj compartida en protoboard. |
| `GPIO 2` | `PIN_CARGA_POWERBANK` | LED interno / carga opcional | Salida digital (`3.3 V`) | Pulso de `200 ms` cada `8,000 ms`; circuito externo y efecto real **`NO MEDIDOS`**. |
| `3V3` | — | `VCC` de `MPU6050` y `VL53L0X` | Alimentación regulada `3.3 V` | Exclusivo para sensores de bajo consumo. |
| `GND` | — | `GND` de `TC1508A`, sensores y `-` de `4×AA` | Referencia común (`0 V`) | Imprescindible para que el `TC1508A` interprete los pulsos `PWM`. |

### Orientación física de los sensores en el chasis

1. **Giroscopio (`MPU6050`):** va montado horizontalmente cerca del centro geométrico del chasis. Dado que según la cara en que se fije la placa sobre la protoboard el eje $Z$ puede apuntar hacia arriba o hacia abajo, el firmware incluye la constante `IMU_SIGNO_Z = 1` (o `-1`) para garantizar la convención de signos de todo el sistema: **giro hacia la izquierda positivo ($+\theta$)** y **giro hacia la derecha negativo ($-\theta$)**.
2. **Láser frontal (`VL53L0X`):** va colocado en el borde delantero del chasis apuntando horizontalmente en la dirección de avance (`0°`), por debajo del lente de la cámara `Freenove`, de modo que detecte cajas de `20×20 cm` situadas frente al vehículo.

## 3.4 Elementos eléctricos no medidos en el prototipo

Siguiendo la regla de trazabilidad del manual, dos aspectos eléctricos deben declararse explícitamente como **`NO MEDIDOS`**:

- **Circuito de mantenimiento de *powerbank* en `GPIO 2`:** algunas baterías portátiles comerciales apagan su salida USB si el consumo cae por debajo de un umbral durante varios segundos. El firmware conmuta `GPIO 2` en alto durante `200 ms` cada `8,000 ms` (`CARGA_PERIODO_MS = 8000`, `CARGA_PULSO_MS = 200`), pensando en conmutar un resistor de carga hacia tierra. Sin embargo, el consumo de la `ESP32 DevKit` con el radio `WiFi` activo en modo punto de acceso (`AP`) basta por sí mismo para mantener encendida la *powerbank* empleada en las pruebas, por lo que la eficacia del pulso en `GPIO 2` permanece **`NO MEDIDA`**.
- **Consumo de corriente y autonomía en miliamperios-hora:** no se han efectuado mediciones instrumentadas con multímetro o pinza amperimétrica de la corriente de arranque ni de régimen de los cuatro motores `TT`, ni del consumo exacto de las placas `ESP32`. En su lugar, la operación se rige por el criterio experimental de reemplazo o recarga de las `4 pilas AA` cada **20 corridas o 90 minutos de uso** (descrito en el Capítulo 11).

Dónde está en el código: `config.h`, `firmware_carro.ino`, `camara_prueba.ino`.
