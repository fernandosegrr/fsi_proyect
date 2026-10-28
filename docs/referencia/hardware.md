# Referencia de hardware

**Estado:** documento de referencia · **Fecha:** 27 de octubre de 2026 · **Rama:** `docs/documentacion-completa`
**Fuentes principales:** `firmware_carro/config.h`, `docs/conexiones.md`, `docs/sensores.md`, `docs/camara_prueba.md`, `docs/hardware_cargar.md` y los reportes J13/J14/J15.

Este documento describe lo que el repositorio permite afirmar sobre el hardware del carro. No sustituye a las hojas de datos ni a una medición con multímetro: cuando un dato no está comprobado, se dice. Las etiquetas **VERIFICADO**, **SIMULADO**, **HIPÓTESIS**, **NO MEDIDO** y **PENDIENTE** se explican en `docs/plan_documentacion.md`, sección 1.3.

## 1. Qué es el carro

El carro es un vehículo de cuatro ruedas, todas motrices, sobre un chasis tipo TT. Lo controla una placa ESP32 DevKit por WiFi. Lleva dos sensores conectados por un bus llamado I2C (un giroscopio MPU6050 y un sensor de distancia VL53L0X) y una segunda placa con cámara (Freenove ESP32-WROVER v3.0) que transmite video por su propia página web. La alimentación está separada: una batería portátil USB (powerbank) alimenta la electrónica, y un portapilas de cuatro pilas AA alimenta solo los motores a través de un driver TC1508A.

```
                         Teléfono Android
                    (app FSI o navegador web)
                              │  WiFi 2.4 GHz
                              ▼
                 ┌──────────────────────────┐
                 │ ESP32 DevKit (WROOM-32)  │  AP 192.168.4.1
                 │ HTTP :80 · WebSocket /ws │
                 └───────┬──────────┬───────┘
                         │          │
             I2C 21/22   │          │  GPIO 27/26/25/33
              ┌──────────┴──┐       ▼
              │ MPU6050     │   TC1508A ──► 4 motores TT
              │ VL53L0X     │   (VDD = 4 pilas AA ~6 V)
              └─────────────┘
                         │ WiFi (cliente)
                         ▼
              Freenove ESP32-WROVER v3.0
              cámara GC0308 · 192.168.4.2 · HTTP :80
```

El diagrama es un esquema lógico, no un plano de montaje. El cableado físico paso a paso está en `docs/conexiones.md`, secciones 2 y 6.

### 1.1 Palabras básicas

Estas palabras aparecen muchas veces en el resto de la documentación. Aquí va su significado en una línea, sin dar por sabido nada:

| Palabra | Qué es, en sencillo |
|---|---|
| **ESP32** | Un pequeño computador con WiFi y Bluetooth. Es el «cerebro» del carro. |
| **DevKit** | La placa que lleva la ESP32 y los conectores para soldar o enchufar cables. |
| **Driver / puente H** | Un interruptor electrónico que deja pasar la corriente a los motores en uno u otro sentido. Sin él, la ESP32 no puede mover un motor. |
| **PWM** | Una señal que se enciende y apaga muy rápido. Cambiando el porcentaje de tiempo encendida se cambia la velocidad del motor. |
| **I2C** | Un bus de dos cables (SDA y SCL) por el que varios sensores comparten datos con la ESP32. |
| **AP (punto de acceso)** | Modo en el que la ESP32 crea su propia red WiFi, como un router pequeño. |
| **DHCP** | El servicio que reparte direcciones IP a los dispositivos que se conectan a esa red. |
| **IP** | El número que identifica a cada dispositivo dentro de la red (por ejemplo, 192.168.4.1). |
| **PSRAM** | Memoria adicional de la placa de la cámara, necesaria para guardar imágenes grandes. |
| **Powerbank** | Batería portátil con salida USB, la que da energía a la electrónica. |

## 2. Lista de materiales

La columna «Evidencia» dice de dónde sale cada elemento. No se encontró en el repositorio una lista de compra con marcas, modelos exactos ni precios: esa lista es **PENDIENTE**.

| Elemento | Cantidad | Para qué | Evidencia |
|---|---|---|---|
| ESP32 DevKit (ESP32-D0WD-V3, 38 pines, 4 MB de flash, sin PSRAM) | 1 | Control, WiFi, servidor HTTP/WebSocket | `docs/hardware_cargar.md` §«Qué se probó»; `config.h` |
| Módulo driver TC1508A | 1 | Puente H doble para los cuatro motores | `docs/conexiones.md` §1 y §5; `config.h:66-76` |
| Motor TT con reductora y rueda | 4 | Tracción (2 por canal, en paralelo) | `docs/conexiones.md` §5; `config.h:68-70` |
| Chasis con portapilas de 4 AA | 1 | Estructura y alimentación de motores | `docs/conexiones.md` §2 y §4 |
| Powerbank USB 5 V | 1 | Alimentación de la ESP32 (y de la cámara) | `docs/conexiones.md` §4; `config.h:145-148` |
| MPU6050 en módulo GY-521 | 1 | Giroscopio de tres ejes; se usa el eje Z | `config.h:156-174`; `docs/sensores.md` |
| VL53L0X en módulo VL53LXX-V2 | 1 | Distancia al frente (láser) | `config.h:156-165`, `223-240`; `docs/sensores.md` |
| Freenove ESP32-WROVER v3.0 con cámara GC0308 y PSRAM de 4 MB | 1 | Video por WiFi | `docs/camara_prueba.md` §«Primera parte (H2)» |
| Teléfono Android | 1 | App de control, visión y rutas | `app_android/`; `docs/entrega.md` |
| Cables dupont, soldadura o regleta para I2C | — | Unir SDA/SCL de los dos módulos | `docs/conexiones.md` §8 |
| Cable USB de datos | 2 | Cargar y alimentar las placas | `docs/hardware_cargar.md` §«Si no aparece ningún puerto COM» |

Los cuatro motores van **dos por canal y en paralelo**, con la misma polaridad: los izquierdos a OUTA/OUTB y los derechos a OUTC/OUTD (`docs/conexiones.md` §5). El TC1508A es un **puente H doble**: tiene dos canales y cada uno puede girar su par de motores hacia adelante o hacia atrás. Admite 1.8 A continuos y 2.5 A de pico por canal según su hoja de datos; el consumo real de estos motores no está medido (**NO MEDIDO**).

## 3. Pines usados por la ESP32

Todos los números salen de `firmware_carro/config.h`. Los pines 25, 26, 27 y 33 no son pines de arranque; el GPIO 2 sí lo es y por eso es el delicado del proyecto.

| GPIO | Función | Dirección | Nota |
|---|---|---|---|
| 27 | Motor izquierdo, entrada A | Salida PWM | Va a INA del TC1508A (`config.h:73`) |
| 26 | Motor izquierdo, entrada B | Salida PWM | Va a INB; el motor izquierdo **avanza** con este pin (`config.h:74`) |
| 25 | Motor derecho, entrada A | Salida PWM | Va a INC; el motor derecho **avanza** con este pin (`config.h:75`) |
| 33 | Motor derecho, entrada B | Salida PWM | Va a IND (`config.h:76`) |
| 2 | Pulso para la powerbank | Salida | 50 ms en alto cada 2 s (`config.h:141-148`) |
| 21 | I2C SDA | Bidireccional | MPU6050 y VL53L0X (`config.h:160`) |
| 22 | I2C SCL | Salida | MPU6050 y VL53L0X (`config.h:161`) |

Reglas de conexión:

- El positivo de las pilas va **solo** al VDD del TC1508A; nunca a un pin de la ESP32 (`docs/conexiones.md` §4).
- El negativo de las pilas, el GND del driver y el GND de la ESP32 van unidos. Sin tierra común no hay referencia (`docs/conexiones.md` §4).
- No se mueven los motores con las pilas desconectadas: una señal de 3.3 V sobre un driver sin alimentar puede superar su tensión máxima de entrada (`docs/conexiones.md` §4).
- Nunca se pone PWM en las dos entradas del mismo motor a la vez (`config.h:71`).

La señal PWM se repite 1500 veces por segundo y se puede graduar en 256 pasos (8 bits), de 0 a 255 (`config.h:81-82`). Un valor de 255 significa «encendido todo el tiempo»; 128, «la mitad». La velocidad base es 255 y los perfiles aplican topes y balance: MANUAL 255/252 y AUTO 210/207 (`config.h:109-128`). Los pivotes usan un solo motor a 255 (`config.h:88`).

## 4. Alimentación

Hay dos circuitos de alimentación separados a propósito, para que el ruido y el consumo de los motores no reinicien la electrónica:

1. **Electrónica:** powerbank de 5 V por USB a la ESP32 DevKit; la cámara Freenove se alimenta por su propio USB (la documentación no dice si comparte la misma powerbank; **PENDIENTE** de confirmar en el montaje).
2. **Motores:** cuatro pilas AA en serie (alrededor de 6 V nominales) al VDD/VCC del TC1508A. El driver no tiene alimentación lógica separada: VDD alimenta lógica y motores (`docs/conexiones.md` §4).

Consecuencias y límites:

- El TC1508A admite de 2.4 a 7.2 V; 7.2 V es el máximo absoluto. Cuatro alcalinas nuevas rondan 6 V, así que hay poco margen; no se debe añadir una quinta pila (`docs/conexiones.md` §4).
- La ESP32 se configura a 160 MHz y el WiFi a 13 dBm para reducir picos de corriente (`config.h:54-64`). El AP usa el canal 1 fijo (`config.h:61`).
- El GPIO 2 produce un pulso de 50 ms cada 2000 ms para que la powerbank no corte la salida (`config.h:141-148` y `carro.c`). Qué hay conectado realmente a ese pin —resistencia, LED o nada— es **NO MEDIDO** y **PENDIENTE** (`docs/conexiones.md` §3).
- La corriente de arranque y de motor bloqueado de los TT no está medida (**NO MEDIDO**). Con dos motores por canal, el cálculo del documento de conexiones pide medir antes de exigir el driver al máximo (`docs/conexiones.md` §5).
- No hay medición registrada del consumo total ni de la autonomía. Cualquier cifra de duración sería una **HIPÓTESIS**.

El orden seguro de conexión, con las ruedas en el aire y sin pilas al principio, está en `docs/conexiones.md` §6. Resumido: tierras primero, señales después, un solo motor, revisar cortos con multímetro, encender la ESP32 sin pilas, probar un motor, y solo al final los cuatro y el carro en el suelo.

## 5. Sensores I2C

Los dos módulos comparten el mismo bus I2C de la DevKit y se alimentan a 3V3. El giroscopio mide cuánto y hacia dónde gira el carro; el láser mide la distancia al objeto que tiene delante. Cada módulo trae sus propias resistencias de pull-up, que mantienen los dos cables en un nivel conocido cuando nadie está hablando (`config.h:156-159`).

| Señal | GPIO | GY-521 (MPU6050) | VL53LXX-V2 (VL53L0X) |
|---|---|---|---|
| SDA | 21 | SDA | SDA |
| SCL | 22 | SCL | SCL |
| Alimentación | 3V3 | VCC | VIN |
| Tierra | GND | GND | GND |

- Frecuencia del bus: 400 kHz; tiempo máximo por transacción: 5 ms (`config.h:162-163`).
- Direcciones: MPU6050 en 0x68 y VL53L0X en 0x29 (`config.h:164-165`). El firmware acepta además los identificadores 0x70 y 0x72 de algunos clones del MPU6050 (`config.h:167-170`).
- Montaje: el giroscopio va plano sobre la caja de pilas, con la flecha X hacia el frente y el eje Z hacia arriba; el láser va al frente, a 6-8 cm del piso, inclinado 5-8° hacia arriba (`docs/conexiones.md` §8).
- El sentido del ángulo se corrige con `IMU_SIGNO` (`config.h:172-174`). El valor actual es 1. Si en una prueba física el sentido sale invertido, se cambia el signo en la configuración; no se cambia el cableado.
- Los empalmes deben ser soldados o con regleta: un SCL flojo ya dejó al giroscopio invisible aunque el LED del módulo encendiera (`docs/conexiones.md` §8).

Para el detalle de mensajes, calibración y diagnóstico, ver `docs/sensores.md` y `docs/referencia/firmware.md`. Los comandos `I2C?`, `PINES?` y `PRUEBAI2C?` permiten revisar el bus por USB (`docs/conexiones.md` §8).

## 6. Cámara

La cámara es una placa **Freenove ESP32-WROVER v3.0** con sensor **GC0308** de 0.3 MP, PSRAM de 4 MB y chip USB-serie CH340 (`docs/camara_prueba.md` §«Primera parte (H2)»). No es una ESP32-CAM genérica con OV2640.

- El GC0308 **no genera JPEG**: entrega RGB565 o YUV422 y la ESP32 comprime cada cuadro por software. Con el formato JPEG la inicialización falla con `0x106 ESP_ERR_NOT_SUPPORTED` (`camara_prueba.ino:2-15`; `docs/camara_prueba.md` §H2).
- Mapa de pines: `CAMERA_MODEL_WROVER_KIT`; XCLK 21, SIOD 26, SIOC 27, datos Y2..Y9 = 4, 5, 18, 19, 36, 39, 34, 35, VSYNC 25, HREF 23, PCLK 22, sin PWDN ni RESET (`docs/camara_prueba.md` §H2).
- Se conecta como cliente a la red del carro con IP fija **192.168.4.2** y puerto HTTP 80 (`camara_prueba.ino:89-91`). El DHCP del DevKit reparte de .10 a .20 para no chocar con ella (`config.h:42-52`).
- Configuración por defecto: 20 MHz de reloj, RGB565, HQVGA 240×176, JPEG de calidad 50 (`camara_prueba.ino:105-108`). El documento histórico H3 recomendaba QVGA; el código actual usa HQVGA. La diferencia se explica en `docs/referencia/camara.md`.
- La placa se alimenta por USB y se comunica por WiFi; no comparte pines con la DevKit. El montaje físico de la cámara sobre el carro no está documentado (**PENDIENTE**).

## 7. Qué está verificado y qué no

| Afirmación | Estado | Evidencia |
|---|---|---|
| La DevKit es un ESP32-D0WD-V3 con 4 MB de flash y sin PSRAM | VERIFICADO | `docs/hardware_cargar.md` §«Qué se probó» (13 oct 2026) |
| Los pines de motor, I2C y keep-alive son los de la tabla | VERIFICADO | `config.h:66-76`, `160-161`, `148` |
| El carro se mueve con los cuatro motores y responde a los comandos | VERIFICADO en J13/J14 (pruebas físicas B0-B6) | `docs/reporte_J14.md` |
| El ajuste recto MANUAL 252/255 dio 3 cm de desvío a 150 cm | VERIFICADO según `config.h:103-104` | Comentario de configuración, sin registro de medición aparte |
| El ajuste recto AUTO 207/210 está calibrado en el carro | NO MEDIDO | `config.h:96` lo declara pendiente |
| La corriente de los motores y del driver es segura en todos los casos | NO MEDIDO | `docs/conexiones.md` §5 |
| El GPIO 2 necesita una carga y de qué valor | NO MEDIDO | `docs/conexiones.md` §3 |
| La cámara GC0308 da 12.4 fps a HQVGA y ~137 ms de edad | VERIFICADO por barrido local | `docs/camara_prueba.md` §«Lo medido» |
| El retraso total del video en el teléfono | NO MEDIDO (solo una muestra de ~530 ms) | `docs/camara_prueba.md` §«Cómo repetir la prueba del cronómetro» |
| La autonomía del carro | NO MEDIDO | No hay registro en el repositorio |
| El comportamiento del driver con dos motores bloqueados | NO MEDIDO / HIPÓTESIS de riesgo | `docs/conexiones.md` §5 |

## 8. Pendientes de hardware

1. Medir corriente por motor en libre y bloqueado, y temperatura del TC1508A (`docs/conexiones.md` §5).
2. Confirmar qué hay conectado al GPIO 2 y si la powerbank lo necesita (`docs/conexiones.md` §3).
3. Confirmar el rótulo real de las entradas del módulo (IN1..IN4, INT1..INT4 o INA..IND) (`docs/conexiones.md` §7).
4. Verificar la polaridad y el sentido de cada motor con las ruedas en el aire antes de bajar el carro (`docs/conexiones.md` §6).
5. Documentar con fotos el montaje, el cableado y la sujeción de la cámara. La lista está en `docs/lista_de_fotos.md`.
6. Medir la autonomía real con una rutina repetible y anotar fecha, batería y resultado (**NO MEDIDO**).

Los defectos y huecos detectados en el código o en los documentos se anotan, sin corregirlos, en `docs/pendientes_documentacion.md`.
