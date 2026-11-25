# Glosario general

Todos los términos clave de los subsistemas A a I en orden alfabético. Al final de cada término se indica la sección donde se explica y la página del manual (el número es un enlace dentro del PDF); si aparece en varios subsistemas, la definición es la del primero.

## Términos


### A

- **ACK_AUTO**: respuesta con la que el carro confirma el perfil AUTO; sin ella la ruta no arranca. *(D.6 p. {{pag:D.6}})*
- **Anticipación**: milisegundos que el carro sigue girando tras frenar; el firmware la aprende. *(B.6 p. {{pag:B.6}})*
- **`archive/*`**: tag de una rama que se guarda y no se usa (`archive/feat-autonomo-v4-a200df4`). *(I.6 p. {{pag:I.6}})*
- **autoridad del láser**: el VL53L0X fresco y válido a ≤ 400 mm confirma; nunca borra lo que ve la cámara. *(F.6 p. {{pag:F.6}})*
- **AYUDA**: estado en que el navegador le cede el mando a la persona. *(D.6 p. {{pag:D.6}}, E.6 p. {{pag:E.6}}, F.6 p. {{pag:F.6}}, G.6 p. {{pag:G.6}})*

### B

- **Balance**: factor del lado izquierdo (800 a 1,000) que endereza la recta; vive en NVS. *(B.6 p. {{pag:B.6}})*
- **banco**: programa que corre muchos escenarios y cuenta los resultados (`pruebas_autonomo_v2`). *(H.6 p. {{pag:H.6}})*
- **Banco simulado**: programa en C que corre miles de rutas con obstáculos falsos (`nucleo_c/pruebas_autonomo_v2.c`). *(I.6 p. {{pag:I.6}})*
- **Bandera**: interruptor de código que activa un cambio; apagada, la versión se comporta como la anterior. *(I.6 p. {{pag:I.6}})*

### C

- **caja negra**: archivo de texto por ruta con números del navegador, del carro y de la visión. *(D.6 p. {{pag:D.6}}, H.6 p. {{pag:H.6}}, I.6 p. {{pag:I.6}})*
- **CalibracionCarro**: medidas que pasan distancia y giro a tiempo. *(E.6 p. {{pag:E.6}})*
- **`CAMERA_GRAB_LATEST`**: modo del driver que siempre entrega el cuadro más nuevo. *(C.6 p. {{pag:C.6}})*
- **categoría excluyente**: cada corrida cae en una sola (ok, contacto, bajo_mueble, ayuda...). *(H.6 p. {{pag:H.6}})*
- **causa**: código `NAV_CAUSA_*` (0 a 19) que dice por qué se detuvo. *(E.6 p. {{pag:E.6}})*
- **celda**: cuadro de 20 cm de la cuadrícula de rutas (`CELDA_CM`). *(E.6 p. {{pag:E.6}}, F.6 p. {{pag:F.6}})*
- **Cola de comandos**: 32 ranuras FIFO entre la tarea de red y `loop()` (`COLA_COMANDOS_MAX`). *(B.6 p. {{pag:B.6}}, G.6 p. {{pag:G.6}})*
- **ComandoNav**: los cinco comandos del protocolo (STOP, FORWARD, REVERSE, LEFT, RIGHT). *(E.6 p. {{pag:E.6}})*
- **compuerta**: meta numérica que el banco debe cumplir. *(H.6 p. {{pag:H.6}}, I.6 p. {{pag:I.6}})*
- **`ControlLogica`**: decide qué comando mandar y cuándo (heartbeat, PING, perfiles, reintentos). *(D.6 p. {{pag:D.6}})*
- **corredor**: huella del carro proyectada con perspectiva en la imagen; apagado por defecto. *(F.6 p. {{pag:F.6}})*
- **corridas pareadas**: la misma semilla en cada configuración, para que solo cambie el software. *(H.6 p. {{pag:H.6}})*

### D

- **DESARROLLO y RESERVADO**: conjunto para ajustar y conjunto que no se toca hasta el cierre. *(H.6 p. {{pag:H.6}}, I.6 p. {{pag:I.6}})*

### E

- **edad del cuadro**: ms desde que empezó la captura hasta que termina su envío. *(C.6 p. {{pag:C.6}})*
- **EfficientDet-Lite0**: red neuronal de detección de objetos, entrenada con COCO; su resultado vale 300 ms (`vigencia_ms`). *(F.6 p. {{pag:F.6}})*
- **escenario**: situación de prueba, como `caja20` o `banquito35x45`. *(H.6 p. {{pag:H.6}})*
- **ESP32 DevKit**: placa con el microcontrolador ESP32 (WiFi, 160 MHz en el carro) donde corre el firmware; un GPIO es un pin que el firmware sube o baja. *(A.6 p. {{pag:A.6}})*
- **EstadoNav**: los ocho estados (ESPERANDO, SIGUIENDO, DETENIDO_OBSTACULO, ESQUIVANDO, REINCORPORANDO, TERMINADO, ERROR, AYUDA). *(E.6 p. {{pag:E.6}})*
- **Etiquetas de evidencia**: **VALIDADO EN EL CARRO**, **SIMULADO**, **NO MEDIDO** y **PENDIENTE**. *(I.6 p. {{pag:I.6}})*
- **Evaluación ciega**: se congela el código, se generan semillas nuevas y se corre una ronda sin ajustar nada después. *(I.6 p. {{pag:I.6}})*

### F

- **`fila_minima` y `MIN_CELDAS`**: desde qué fila se mira (44 % del alto) y cuántas celdas «no piso» hacen falta (7). *(F.6 p. {{pag:F.6}})*
- **`frame2jpg`**: función de la librería esp32-camera que comprime a JPEG por software; es el compresor de siempre. *(C.6 p. {{pag:C.6}})*
- **`freeze/*`**: tag del código congelado antes de una evaluación ciega (`freeze/v5-carbon`, `freeze/v5-carbon-fix`, `freeze/v6`). *(I.6 p. {{pag:I.6}})*
- **freno anti-locura**: revisión de cordura de las causas 9 a 12. *(E.6 p. {{pag:E.6}})*
- **`-ftrapv`**: bandera de GCC que detiene el programa si un entero con signo se desborda. *(H.6 p. {{pag:H.6}})*
- **fusión**: regla por zona `hay_obstaculo = color && red`; una fuente sola es un aviso. *(F.6 p. {{pag:F.6}})*

### G

- **GC0308**: sensor de imagen de 0.3 megapíxeles de la Freenove; no comprime JPEG. *(C.6 p. {{pag:C.6}})*
- **GND común**: tierra compartida por pilas, driver y ESP32; sin ella el driver no entiende las señales PWM. *(A.6 p. {{pag:A.6}})*
- **grupos de piso (K)**: colores del piso aprendidos con un k-means corto; hasta 4, 3 en la app. *(F.6 p. {{pag:F.6}})*

### H

- **hardware falso**: MPU6050 y VL53L0X simulados para probar el firmware en PC. *(H.6 p. {{pag:H.6}})*
- **heartbeat**: comando repetido cada 250 ms mientras se aprieta un botón; sin él el carro frena a los 1,000 ms. *(D.6 p. {{pag:D.6}})*
- **HQVGA**: tamaño de 240x176 píxeles, el que usa la cámara por defecto. *(C.6 p. {{pag:C.6}})*

### I

- **I2C**: bus de dos cables (SDA y SCL) que comparten el giroscopio y el láser. *(A.6 p. {{pag:A.6}})*

### J

- **JNI**: mecanismo de Android para que Kotlin llame funciones en C; aquí, `puente.c`. *(D.6 p. {{pag:D.6}}, E.6 p. {{pag:E.6}})*
- **Jornada (J1 a J16)**: bloque numerado de trabajo de la v1.0 y la v1.1 (`docs/historia.md`). *(I.6 p. {{pag:I.6}})*
- **JPEGENC**: biblioteca de terceros (Apache-2.0) incluida solo para comparar compresores. *(C.6 p. {{pag:C.6}})*

### L

- **`loop()`**: función que Arduino repite sin parar; aquí hace todo el trabajo. *(B.6 p. {{pag:B.6}})*

### M

- **MJPEG (`/stream`)**: video como una conexión HTTP que manda un JPEG tras otro. *(C.6 p. {{pag:C.6}})*
- **`ModoSemiautonomo`**: ciclo de 50 ms que alimenta al navegador en C y manda sus comandos. *(D.6 p. {{pag:D.6}})*
- **Monte Carlo**: repetir una prueba miles de veces con pequeñas variaciones al azar y resumir los resultados. *(H.6 p. {{pag:H.6}})*
- **Motor TT**: motorreductor de plástico (típico 1:48, de 3 a 6 V) sin encoder. *(A.6 p. {{pag:A.6}})*
- **MPU6050**: giroscopio y acelerómetro (módulo GY-521); el carro solo usa el eje Z del giroscopio. *(A.6 p. {{pag:A.6}})*

### N

- **navegador_tick**: función que se llama cada 50 ms y devuelve un comando. *(E.6 p. {{pag:E.6}})*
- **NDK**: herramienta de Android que compila el C para el teléfono. *(E.6 p. {{pag:E.6}})*
- **nucleo_c**: carpeta con la lógica de navegación, calibración, visión y simulador, en C99. *(E.6 p. {{pag:E.6}})*

### P

- **p95**: valor bajo el cual queda el 95 % de las corridas. *(H.6 p. {{pag:H.6}})*
- **paquete `CEL1`**: paquete UDP de 1,244 B con el promedio RGB de cada celda de 10x10 píxeles. *(C.6 p. {{pag:C.6}})*
- **PARO**: botón rojo que manda STOP y cancela la ruta (`SesionCarro.pararTodo`). *(D.6 p. {{pag:D.6}})*
- **Perfil AUTO o MANUAL**: dos juegos de velocidad (210 y 207; 255 y 252); el carro arranca en AUTO. *(B.6 p. {{pag:B.6}})*
- **Perfil AUTO y MANUAL**: dos juegos de velocidad (210/207 y 255/252); solo AUTO frena por láser. *(G.6 p. {{pag:G.6}})*
- **perfil MANUAL y AUTO**: velocidades 255 («Rápido») y 210 («Lento»). *(D.6 p. {{pag:D.6}})*
- **PI y ppm**: control proporcional-integral (error de ahora más acumulado); partes por millón (10,000 ppm = 1 %). *(B.6 p. {{pag:B.6}})*
- **Pivote**: giro apoyado en un solo lado (`LEFT`, `RIGHT`); el centro del carro se desplaza. *(A.6 p. {{pag:A.6}})*
- **pose**: posición estimada (x, y en mm) y rumbo (grados; `rumbo_x10` en décimas). *(E.6 p. {{pag:E.6}})*
- **Protocolo físico**: guía para probar en el carro por bloques A a D y mediciones M1 a M10. *(I.6 p. {{pag:I.6}})*
- **PWM**: señal que se enciende y apaga muy rápido; el porcentaje encendido (0 a 255) fija la potencia. En el carro, 1,500 Hz y 8 bits. *(A.6 p. {{pag:A.6}})*

### R

- **`RefrescoAcotado`**: deja redibujar la pantalla como mucho una vez cada 100 ms. *(D.6 p. {{pag:D.6}})*
- **RGB565**: formato sin comprimir de 2 bytes por píxel que entrega el sensor. *(C.6 p. {{pag:C.6}})*
- **Rueda libre**: motor sin corriente (PWM 0 en las dos entradas); es lo que hacen `STOP` y `EBRAKE`. *(A.6 p. {{pag:A.6}})*
- **Rueda mecanum**: rueda con rodillos a 45°; controlada por separado permite moverse de lado, pero en este carro no. *(A.6 p. {{pag:A.6}})*

### S

- **`SEG=3`**: versión de seguridad del firmware de la v3 (150 y 180 mm); solo compilada. *(G.6 p. {{pag:G.6}})*
- **semilla**: número que arranca el generador de azar; con la misma semilla el resultado se repite. *(H.6 p. {{pag:H.6}}, I.6 p. {{pag:I.6}})*
- **`SENS`**: mensaje cada 100 ms en AUTO con ángulo y distancia; 1,500 ms de silencio cancelan la ruta. *(G.6 p. {{pag:G.6}})*
- **`SENS` y `CAPS`**: telemetría cada 100 ms y lista de capacidades con la versión. *(B.6 p. {{pag:B.6}})*
- **servidor falso**: programa de PC que imita al carro por WebSocket. *(H.6 p. {{pag:H.6}})*
- **Sesgo (bias)**: lectura del giroscopio con el carro quieto (cerca de −0.45 °/s); se mide al arrancar. *(A.6 p. {{pag:A.6}})*
- **`SesionCarro`**: objeto único que mantiene la conexión con el carro y guarda `ControlLogica` y `ModoSemiautonomo`. *(D.6 p. {{pag:D.6}})*
- **`SesionVideo`**: objeto de la app que abre un solo video y lo comparte entre pantallas. *(C.6 p. {{pag:C.6}})*
- **«sin eco»**: lectura inválida o fuera de alcance del láser; no cuenta como libre. *(F.6 p. {{pag:F.6}}, G.6 p. {{pag:G.6}})*
- **SoftAP**: modo en que la ESP32 crea su propia red WiFi (`CarroFSI`, `192.168.4.1`). *(B.6 p. {{pag:B.6}})*
- **`STOP` y `EBRAKE`**: comandos de paro; hacen lo mismo. La app manda `STOP`; `EBRAKE` viene de la página web del carro (`firmware_carro/pagina.h`). *(G.6 p. {{pag:G.6}})*
- **`__STOP_AUTO`**: paro interno del firmware (láser, conexión, desconexión); la app no lo manda. *(B.6 p. {{pag:B.6}}, G.6 p. {{pag:G.6}})*

### T

- **TC1508A**: driver (puente H doble) que convierte las señales de 3.3 V de la ESP32 en corriente para los motores; un puente H invierte la corriente. *(A.6 p. {{pag:A.6}})*
- **TICK_MS**: 50 ms entre decisiones, en `ModoSemiautonomo.kt`. *(E.6 p. {{pag:E.6}})*
- **`TOF_BLOQUEO_MM`**: con el frente confirmado más cerca que este valor (180 mm en v3), un `FORWARD` en AUTO no arranca; no existe en v1.1. *(G.6 p. {{pag:G.6}})*
- **`TOF_PARO_MM`**: umbral del paro por láser (120 mm en v1.1; 150 mm en v3); `TOF_BLOQUEO_MM`, el de no arrancar. *(B.6 p. {{pag:B.6}}, G.6 p. {{pag:G.6}})*
- **Tracción diferencial**: avanzar y girar variando la potencia de cada lado. *(A.6 p. {{pag:A.6}})*

### U

- **`UMBRAL_COLOR` y croma**: distancia de color que separa piso de no piso (40) y regla de tono que tolera cambios de luz. *(F.6 p. {{pag:F.6}})*

### V

- **vistazo**: el carro tuerce 10 a 16° para apuntar el láser a lo que la cámara vio de lado; dos ecos a ≤ 650 mm confirman. *(F.6 p. {{pag:F.6}})*
- **VL53L0X**: sensor láser de tiempo de vuelo (ToF) que mide la distancia al frente. *(A.6 p. {{pag:A.6}})*

### W

- **Watchdog**: vigilante que frena o reinicia si algo no ocurre a tiempo (1,000 ms de comandos; 5,000 ms de hardware). *(B.6 p. {{pag:B.6}})*
- **Watchdog de comandos**: `TIEMPO_WATCHDOG_MS` = 1,000 ms sin comando de movimiento apaga los motores y avisa `WD:<n>`. *(G.6 p. {{pag:G.6}})*
- **Watchdog de hardware**: `WATCHDOG_HW_MS` = 5,000 ms; reinicia el chip si `loop()` se cuelga. *(G.6 p. {{pag:G.6}})*

### Z

- **zona y cercanía**: tercio izquierdo, central o derecho; banda 1, 2 o 3 según qué tan abajo llega el obstáculo (no son centímetros). *(F.6 p. {{pag:F.6}})*
