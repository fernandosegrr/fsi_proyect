# 2. Arquitectura general

Este capítulo explica cómo se divide el sistema en tres nodos físicos independientes, por qué se eligió esta distribución frente a un diseño centralizado en el microcontrolador y cómo se protegen eléctricamente y lógicamente sus enlaces. Su comprensión permite ubicar el papel de cada placa, cada batería y cada protocolo dentro del conjunto.

## 2.1 Topología de tres nodos y reparto de responsabilidades

El vehículo opera mediante la cooperación de tres nodos que se comunican sobre una red inalámbrica local (`SSID: CarroFSI`) creada por la placa principal del chasis. Ningún nodo depende de internet ni de servidores externos.

| Nodo del sistema | Dirección IP y rol de red | Responsabilidad técnica principal | Fuente de energía dedicada |
|---|---|---|---|
| **ESP32 DevKit (control y sensores)** | `192.168.4.1` — Punto de acceso (`AP`), servidor `HTTP :80` y `WebSocket /ws` | Cola determinista de comandos, PWM de motores (`1 kHz`), lectura `I2C` a `400 kHz` (`MPU6050` y `VL53L0X`), lazo `PI` de rumbo en rectas (`RUMBO`), giro cerrado (`GIRO`), freno láser local (`120 mm`) y *watchdogs* (`1000 ms` / `5000 ms`). | **Powerbank USB #1** (`5 V` por puerto micro-USB; alimenta también el bus `I2C` a `3.3 V`). |
| **Freenove ESP32-WROVER (cámara `GC0308`)** | `192.168.4.2` — Cliente (`STA`) unido a `CarroFSI`; servidores `HTTP :80/:81` y `UDP :4210` | Captura continua en resolución `HQVGA` (`240×176` píxeles, `RGB565` a `20 MHz`), compresión `JPEG` por software (calidad `50`), buzón de último cuadro y envío de paquetes de celdas (`1,244 B`). | **Powerbank USB #2** (`5 V` exclusiva para la cámara; aislada galvánicamente del chasis). |
| **Teléfono Android (`APP:0.4-6d1b213`)** | `192.168.4.x` — Cliente (`STA`) con sesiones persistentes `SesionCarro` y `SesionVideo` | Interfaz de usuario, ejecución nativa `JNI` del núcleo en `C99` (planificación en cuadrícula de `20 cm`, integración de pose en décimas de grado, clasificador cromático `YCbCr`, inferencia `EfficientDet-Lite0`, fusión bajo mutex) y supervisión activa de seguridad. | Batería interna del teléfono móvil. |

*(En el cuadro de tracción, los cuatro motores DC de caja reductora `TT` y la etapa de potencia del puente H `TC1508A` reciben energía de un **portapilas independiente de 4 pilas AA** con interruptor físico, unido por tierra común `GND` a la `ESP32 DevKit`).*

[FIGURA:arquitectura|Arquitectura distribuida de tres nodos del sistema LIDIA, enlaces de red e interfaces de hardware.]

### Por qué el teléfono ejecuta el cálculo pesado y la cámara va en otra placa

En un microcontrolador `ESP32` de doble núcleo a `240 MHz`, capturar video de un sensor sin compresión nativa (`GC0308`), comprimir cada cuadro a `JPEG` por software, ejecutar una red neuronal de detección de objetos y atender simultáneamente una pila `WiFi/TCP` consume decenas de milisegundos por ciclo e introduce latencias variables (*jitter*). Si ese mismo procesador tuviera que integrar el giroscopio a `200 Hz` y cerrar el lazo de rumbo a `50 Hz`, cualquier ráfaga de red o de compresión de imagen degradaría la precisión angular y retrasaría el frenado ante un obstáculo.

Separar las funciones resuelve este conflicto de raíz:

1. La **ESP32 DevKit** queda dedicada a tareas de tiempo real estricto: su bucle principal dura en promedio `~0.69 ms` (`690 us`) y solo se alarga durante las lecturas del sensor láser `VL53L0X`, sin sufrir bloqueos por transmisión de imágenes.
2. La **Freenove ESP32-WROVER** dedica sus dos núcleos y su memoria `PSRAM` exclusivamente a leer el sensor `GC0308` y servir el cuadro más reciente al teléfono.
3. El **teléfono Android** aporta procesadores multinúcleo de varios gigahercios y aceleración vectorial para ejecutar el clasificador de piso, la red neuronal `TensorFlow Lite` y la máquina de estados del navegador en `C99`, enviando al carro únicamente consignas compactas (`FORWARD`, `RUMBO:<décimas>`, `GIRO:<grados>`, `STOP`).

## 2.2 Esquema de alimentación eléctrica y separación de fuentes

Uno de los problemas clásicos en robótica móvil a escala es la caída de tensión (*brownout*) cuando los motores arrancan o invierten su sentido de giro: el pico de corriente hace caer el voltaje de la batería y reinicia el microcontrolador o desestabiliza la cámara. Para impedirlo, el vehículo divide su alimentación en **tres suministros físicamente separados**:

1. **Powerbank USB #1 (electrónica de control):** alimenta por USB a la placa `ESP32 DevKit`. Desde el regulador interno de `3.3 V` de esta placa se energizan únicamente el giroscopio `MPU6050` (`0x68`) y el sensor láser `VL53L0X` (`0x29`).
2. **Portapilas de 4 pilas AA (potencia de tracción):** entrega entre `4.8 V` (con cuatro celdas recargables `NiMH` de `1.2 V`) y `6.0 V` (con celdas alcalinas de `1.5 V`) directamente al pin `VDD` del puente H doble `TC1508A` a través de un interruptor mecánico en el chasis. El positivo de las pilas **nunca** se conecta a ningún pin de la `ESP32 DevKit`.
3. **Powerbank USB #2 (exclusiva de la cámara):** alimenta por su propio cable USB a la placa `Freenove ESP32-WROVER`. La cámara no tiene ningún cable de señal ni de tierra unido al resto del carro; su acoplamiento es cien por ciento inalámbrico por `WiFi`.

Para que las cuatro señales `PWM` (`GPIO 27, 26, 25 y 33`) que salen de la `ESP32 DevKit` hacia las entradas `IN1..IN4` del driver `TC1508A` tengan una referencia eléctrica válida, los terminales negativos (`GND`) de la `ESP32 DevKit`, del módulo `TC1508A` y del portapilas de `4×AA` están unidos en un bus de **tierra común**.

[FIGURA:alimentacion|Esquema de los tres circuitos de alimentación (dos powerbanks USB independientes más el portapilas de 4 pilas AA) y el enlace de tierra común.]

## 2.3 Defensa en profundidad: cuatro capas de seguridad física y lógica

Dado que los algoritmos de percepción y navegación corren en el teléfono mientras los motores están en el carro, cualquier pérdida de enlace `WiFi`, bloqueo de aplicación o lectura espuria podría dejar al vehículo avanzando sin control si no existieran barreras locales. El sistema implementa cuatro anillos concéntricos de seguridad:

1. **Capa 1 — Supervisión humana e interruptor físico:** el operador mantiene contacto visual con el vehículo, dispone de un botón permanente `PARO` en la interfaz y puede cortar la energía de tracción desde el interruptor del portapilas `4×AA`.
2. **Capa 2 — Supervisión activa en Android (reforzada en J16):** durante la conducción manual, `ControlLogica` envía un latido de movimiento cada `250 ms` mientras se pulsa un botón, pero **corta inmediatamente el reenvío de `FORWARD`** si recibe `TOF_PARO` del carro. Durante una ruta semiautónoma, `ModoSemiautonomo` aborta la misión y envía `STOP` en tres situaciones adicionales incorporadas en el cierre J16:
   - Si recibe el aviso `WD:<n>` indicando que el *watchdog* del carro se disparó por falta de comandos.
   - Si transcurren más de `1,500 ms` (`SILENCIO_SENS_MS`) sin recibir tramas `SENS` mientras el navegador está activo.
   - Si llega `TOF_PARO` y la máquina de estados del navegador no transiciona a parada o esquive en ese mismo ciclo.
3. **Capa 3 — Vigilancia temporal en el firmware (`ESP32 DevKit`):** todos los mensajes entrantes pasan por una cola determinista de `32` ranuras donde las órdenes `STOP` y `EBRAKE` tienen prioridad absoluta y jamás se descartan (si la cola se llena, desaloja un `PING` antiguo). Si pasan `1,000 ms` (`TIMEOUT_COMANDO_MS`) con los motores encendidos sin recibir una orden válida de movimiento, `carro_vigilar_comunicacion` apaga los motores, restablece el perfil `AUTO` y transmite `WD:<contador>`. Además, un *watchdog* de hardware del `ESP32` (`WDT_HARDWARE_SEG = 5 s`) reinicia el microcontrolador a estado seguro (`STOP`) si el bucle principal llegara a bloquearse.
4. **Capa 4 — Paro reflejo por sensor láser frontal (`VL53L0X`):** cuando el carro avanza hacia el frente en perfil `AUTO` y el sensor láser mide una distancia válida inferior a `120 mm` (`TOF_PARO_MM`), el propio firmware detiene los motores en menos de un ciclo de sensores (`~20 ms`), sin esperar instrucción del teléfono, y notifica el evento mediante `TOF_PARO:<mm>`.

[FIGURA:seguridad|Defensa en profundidad de cuatro capas: operador, supervisión activa en Android, vigilancia temporal en firmware y paro reflejo por láser.]

Dónde está en el código: `config.h`, `carro.c`, `sensores.c`, `ControlLogica.kt`, `ModoSemiautonomo.kt`.
