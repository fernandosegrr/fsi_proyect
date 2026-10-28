# Glosario técnico de términos y unidades

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Propósito:** Definición rigurosa y accesible del vocabulario técnico, acrónimos y magnitudes empleadas en la documentación del proyecto.  
**Fecha:** 27 de octubre de 2026  

---

## 1. Conceptos de hardware y electrónica

| Término | Definición y contexto en el carro |
|---|---|
| **ESP32** | Familia de microcontroladores de 32 bits de doble núcleo con conectividad WiFi y Bluetooth integrados, fabricados por Espressif. Es el procesador central del sistema. |
| **DevKit** | Placa de desarrollo comercial que incorpora el microcontrolador ESP32, regulador de voltaje a 3.3 V, interfaz USB-serie y pines accesibles para prototipado. |
| **WROVER** | Variante del módulo ESP32 que añade 4 u 8 MB de memoria RAM pseudoestática externa (PSRAM). Es la placa empleada en el nodo de la cámara Freenove. |
| **Driver / Puente H** | Circuito integrado de potencia que permite aplicar voltaje a un motor de corriente continua en ambos sentidos de giro mediante interruptores electrónicos de estado sólido. En el carro se utiliza el chip **TC1508A**. |
| **PWM (Pulse Width Modulation)** | Modulación por ancho de pulsos. Técnica digital que enciende y apaga rápidamente un pin para regular la velocidad de giro de los motores según el porcentaje de tiempo en alto (ciclo de trabajo). |
| **Bus I2C (Inter-Integrated Circuit)** | Bus serie sincrónico de dos hilos: línea de datos (`SDA`, GPIO 21) y línea de reloj (`SCL`, GPIO 22). Permite conectar múltiples sensores compartiendo los mismos cables mediante direcciones hexadecimales únicas. |
| **MPU6050** | Módulo sensor que integra un acelerómetro de tres ejes y un giroscopio de tres ejes (GY-521). En este proyecto se utiliza exclusivamente la medición de velocidad angular sobre el eje vertical Z para conocer la orientación del chasis. |
| **VL53L0X** | Sensor de distancia láser basado en tecnología de tiempo de vuelo (Time-of-Flight). Mide el tiempo que tarda un pulso de luz infrarroja invisible en rebotar contra un obstáculo frontal. |
| **ToF (Time-of-Flight)** | Principio físico de medición de distancia fundamentado en calcular la distancia a partir de la velocidad de la luz y el tiempo transcurrido entre la emisión y recepción del pulso lumínico. |
| **Powerbank** | Batería recargable portátil con salida regulada de 5 V mediante puerto USB, empleada exclusivamente para energizar los microcontroladores y sensores del carro. |
| **Tierras comunes (GND)** | Interconexión física de las líneas de retorno eléctrico (0 voltios) de la powerbank y del portapilas de motores para establecer una referencia de potencial idéntica en todo el sistema. |
| **Keep-alive (GPIO 2)** | Pulso digital de 50 ms emitido cada 2 segundos por el microcontrolador para forzar un consumo transitorio y evitar que los circuitos ahorradores de la powerbank apaguen la alimentación. |

---

## 2. Redes y protocolos de comunicación

| Término | Definición |
|---|---|
| **Punto de Acceso (AP)** | Modo de funcionamiento inalámbrico en el cual el ESP32 crea su propia red WiFi independiente (`192.168.4.1`), permitiendo la conexión de clientes sin depender de un router externo. |
| **Estación (STA)** | Modo de red en el cual un dispositivo opera como cliente inalámbrico conectado a un punto de acceso. La cámara y el teléfono funcionan como estaciones. |
| **DHCP** | Protocolo de configuración dinámica de host. Servicio que asigna automáticamente direcciones IP a los dispositivos que se enlazan a la red del carro (rango configurado: `.10` a `.20`). |
| **WebSocket** | Protocolo de red sobre TCP que establece una conexión bidireccional continua y de baja latencia entre el teléfono y el microcontrolador (`/ws`), permitiendo el envío de comandos y telemetría sin renegociar conexiones. |
| **MJPEG (Motion JPEG)** | Flujo de video transmitido por HTTP donde cada cuadro es emitido como una fotografía JPEG independiente delimitada por cabeceras multipart. |
| **RGB565** | Formato de almacenamiento de color donde cada píxel ocupa 16 bits (2 bytes): 5 bits para el canal rojo, 6 bits para el verde y 5 bits para el azul. |
| **HQVGA** | Resolución de imagen de 240×176 píxeles (High Quarter VGA). Resolución estándar de la cámara en este proyecto. |
| **Celdas UDP (`CEL1`)** | Paquete binario compacto de 1244 bytes enviado por el puerto UDP 4210 que resume la imagen de la cámara en una matriz de 24×17 bloques de 10×10 píxeles promediando su color RGB. |

---

## 3. Algoritmos, control y navegación

| Término | Definición |
|---|---|
| **Pose** | Vector matemático $(x, y, \theta)$ que describe la posición bidimensional del centro del vehículo en coordenadas métricas y su orientación angular (rumbo) respecto al sistema de coordenadas de partida. |
| **Rumbo (Heading)** | Ángulo de orientación horizontal del vehículo respecto al eje inicial, expresado en grados sexagesimales. |
| **Waypoint** | Punto de paso o coordenada objetivo en el plano de navegación trazado en la cuadrícula de la app. |
| **Corredor de seguridad** | Franja de paso trapezoidal proyectada en la perspectiva de la cámara que delimita el ancho real del vehículo más un margen lateral preventivo. Un obstáculo solo detiene la marcha si se localiza dentro de este corredor. |
| **ZUPT (Zero Velocity Update)** | Algoritmo que actualiza el cálculo del sesgo del giroscopio cuando se comprueba que los motores llevan más de 400 ms apagados y las lecturas no presentan variaciones significativas. |
| **Watchdog** | Temporizador de seguridad por software o hardware que supervisa el correcto funcionamiento del sistema. Si no es reiniciado periódicamente por una señal válida, apaga los actuadores o reinicia la placa para evitar estados fuera de control. |
| **JNI (Java Native Interface)** | Mecanismo que permite al código escrito en Kotlin/Java en la app Android invocar directamente bibliotecas nativas de alto rendimiento compiladas en C (`nucleo_c`). |
| **NDK (Native Development Kit)** | Conjunto de herramientas para compilar código C/C++ en la plataforma Android. |

---

## 4. Unidades de medida empleadas

| Símbolo | Magnitud física y equivalencia |
|---|---|
| **ms** | Milisegundo ($10^{-3}$ segundos). Unidad estándar de temporización de motores y bucles de control. |
| **$\mu$s** | Microsegundo ($10^{-6}$ segundos). Unidad de medición de tiempo de ejecución del bucle `loop()`. |
| **mm** | Milímetro ($10^{-3}$ metros). Unidad de resolución del sensor ToF láser VL53L0X y del arrastre por inercia (`frenado_mm`). |
| **cm** | Centímetro ($10^{-2}$ metros). Unidad de trabajo de la cuadrícula de la app (20 cm por celda). |
| **ms/cm** | Milisegundos por centímetro. Inverso de la velocidad lineal del carro; constante de calibración de avance. |
| **ms/grado** | Milisegundos por grado sexagesimal. Constante cinemática para calcular la duración de giros en pivote por tiempo. |
| **°/s** | Grados por segundo. Velocidad angular reportada por el giroscopio MPU6050. |
| **LSB** | Least Significant Bit (Bit menos significativo). Cuentas digitales del convertidor analógico-digital del MPU6050 (sensibilidad: 65.5 LSB por °/s en rango $\pm 500^\circ/\text{s}$). |
| **Hz / kHz / MHz** | Hertz, kiloHertz y MegaHertz. Frecuencias del PWM (1500 Hz), bus I2C (400 kHz) y reloj del CPU (160 MHz). |
| **dBm** | Decibelio-milivatio. Unidad de potencia de transmisión de radio WiFi (13 dBm configurados en el ESP32). |
