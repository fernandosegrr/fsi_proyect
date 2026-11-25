# Subsistema B · Firmware del carro

## B.1 Qué es y para qué sirve

El firmware es el programa que corre dentro de la ESP32 DevKit del carro. Crea la red WiFi `CarroFSI`, recibe órdenes de texto por un WebSocket (conexión que queda abierta entre teléfono y carro), mueve los cuatro motores, lee el giroscopio y el láser y frena solo cuando algo falla, sin esperar al teléfono. En el carro está grabada la v1.1 (`FW:6d1b213-20261110`, 10-nov-2026). El repositorio trae la v3 (`SEG=3`), solo compilada y probada en PC: **SIMULADO / NO MEDIDO** en el carro.

Más detalle: capítulos 4 y 6 del manual (`docs/manual/capitulos/04_firmware.md` y `docs/manual/capitulos/06_protocolo.md`).

## B.2 Cómo funciona, paso a paso

La lógica (`carro.c`, `sensores.c`, `imu.c`, `tof.c`, `giro.c`, `rumbo.c`) es C99 puro; solo `firmware_carro/firmware_carro.ino` toca la ESP32, mediante funciones `hw_*`. Pines y driver: subsistema A.

```
 teléfono o página                       ESP32 DevKit (firmware_carro)
 ┌─────────────┐  texto por WebSocket   ┌────────────────────────────────────┐
 │ FORWARD     │ ─────────────────────▶ │ tarea de red: recorta y encola     │
 │ GIRAR:90    │                        │ (32 ranuras) y contesta PONG o ACK │
 │ PING        │ ◀───────────────────── │                                    │
 └─────────────┘   SENS, CAPS, WD,      │ loop(), cada vuelta:               │
                   TOF_PARO             │  1 sensores_actualizar()           │
                                        │  2 carro_procesar_cola()           │
                                        │  3 carro_vigilar_comunicacion()    │
                                        │  4 enviar_mensajes_de_sensores()   │
                                        └─────────────────┬──────────────────┘
                                                          ▼
                                     4 salidas PWM ─▶ TC1508A ─▶ 4 motores
```

### El camino de un comando

1. **Arranque.** `setup()` baja la CPU a 160 MHz, configura los cuatro PWM (pulsos que fijan la potencia; 1,500 Hz, 8 bits), deja los motores en 0 y el perfil en AUTO, inicia I2C, giroscopio, láser y watchdog de hardware (vigilante que reinicia el chip si se cuelga), y al final crea la red y el servidor.
2. **Red.** La ESP32 es un SoftAP (hace de router): red `CarroFSI`, canal 1 fijo, IP `192.168.4.1`, DHCP de `.10` a `.20` para dejar libre la `.2` de la cámara (subsistema C); la clave vive fuera del repositorio. El puerto 80 entrega la página de `pagina.h` en `/` (otra ruta da 404); el WebSocket vive en `/ws` y admite un solo cliente: uno nuevo cierra a los demás, y conectarse o desconectarse el activo encola `__STOP_AUTO` (frena, vuelve a AUTO, apaga `RUMBO`).
3. **Entra un mensaje.** La tarea de red (`async_tcp`) acepta solo texto completo, lo recorta a 31 caracteres y lo mete a la cola de 32 ranuras (`COLA_COMANDOS_MAX`), protegida con una sección crítica; así solo `loop()` mueve los motores. Ahí mismo contesta (`PONG`, `ACK_*`), antes de ejecutar. La cola es FIFO (primero en entrar, primero en salir); si se llena, tira un `PING` viejo y, si no hay, el comando más viejo que no sea `STOP`, `EBRAKE` ni `__STOP_AUTO`. La «prioridad» del paro es que no se descarta, no que se salte la fila.
4. **`loop()` ejecuta** lo del diagrama; `carro_vigilar_comunicacion()` también termina el arranque a plena potencia y aplica `RUMBO`.
5. **Movimiento.** Un comando igual al activo se ignora (así el latido de 250 ms no reinicia nada), salvo `STOP` y `EBRAKE`. `FORWARD` y `REVERSE` ponen los dos lados a 255 durante 250 ms (con menos, desde parado, los motores solo zumbaban) y luego el perfil: MANUAL 255 derecho y 252 izquierdo, AUTO 210 y 207. `LEFT` y `RIGHT` pivotan con un solo lado a 255; `STOP` y `EBRAKE` ponen los cuatro PWM en 0. `MODO_*` cambia el perfil (en movimiento, antes se detiene).

### Los mensajes que entiende el carro

| Mensaje (nombre real) | Qué hace | Respuesta |
|---|---|---|
| `FORWARD`, `REVERSE`, `LEFT`, `RIGHT` | Movimiento; renuevan el watchdog | Ninguna |
| `STOP`, `EBRAKE` | Cuatro PWM a 0; hacen lo mismo | Ninguna |
| `PING` | No renueva el watchdog | `PONG` |
| `MODO_MANUAL`, `MODO_AUTO` | Perfil de velocidad | `ACK_MANUAL`, `ACK_AUTO` |
| `BAL_AUTO:NNN`, `BAL_MANUAL:NNN`, `BAL?` | Balance de lados (800 a 1,000) | `ACK_BAL_*:NNN`, `NAK_BAL`, `BAL:AAA:MMM` |
| `GIRAR:<grados>` | Giro cerrado de 5 a 200; positivo = izquierda | `GIRO_OK`, `GIRO_TIMEOUT`, `GIRO_ERROR` |
| `RUMBO:ON`, `RUMBO:OFF`, `RUMBO:<n>`, `RUMBO?` | Control de rumbo en recta | `ACK_RUMBO:1`, `ACK_RUMBO:0`, ninguna, `RUMBO:a,b,c,d,e` |
| `IMU_ESCALA:<ppm>`, `IMU_ESCALA?` | Escala del giroscopio | `ACK_IMU_ESCALA:<ppm>`, `NAK_IMU_ESCALA`, `IMU_ESCALA:<ppm>` |
| `CAPS?`, `IMU?`, `TOF?`, `ANT?`, `ARR?`, `LOOP?` | Consultas; no renuevan el watchdog | `CAPS:`, `IMU:`, `TOF:`, `ANT:`, `ARR:`, `LOOP:` |

Sobre los nombres: «girar» es el comando `GIRAR:<grados>` y `GIRO_` es el prefijo de sus respuestas; `RUMBO` son cuatro mensajes; `STOP` y `EBRAKE` existen los dos y hacen lo mismo, sin freno eléctrico. `BACKWARD`, `GIRO:<grados>`, `CAL_IMU`, `ESTADO`, `VERSION?`, `SENS?`, `OK`, `TIMEOUT_COMANDO_MS` y `WDT_HARDWARE_SEG` no existen en `firmware_carro/` (revisado con `git grep`); los reales son `REVERSE`, `GIRAR`, `IMU_CAL`, `TIEMPO_WATCHDOG_MS` y `WATCHDOG_HW_MS`.

### Giro cerrado: `GIRAR`

«Cerrado» quiere decir que mide el resultado. `GIRAR` rechaza un ángulo que no sea entero de 5 a 200, un giroscopio no listo o motores en marcha (`GIRO_ERROR`). Si procede, pivota con un solo lado a 255 y en cada vuelta suma el ángulo medido. Frena cuando `girado + velocidad × anticipación ≥ objetivo`. La anticipación son los milisegundos que el carro sigue girando tras frenar: empieza en 20 ms (MANUAL) o 100 ms (AUTO) y se corrige sola tras cada giro bueno de 30° o más (mediana de las últimas 5, una por sentido y perfil, tope 250 ms, guardada en la memoria no volátil, NVS). Tiene su propio plazo, 500 ms más 15 ms por grado (1,850 ms para 90°), y mientras gira el watchdog de 1,000 ms no actúa. Tras frenar mide 750 ms más y responde `GIRO_OK:<objetivo>:<real x10>:<ms>:<anticipación>`. `STOP`, otro movimiento, cambio de perfil, desconexión o falla del sensor lo abortan.

### Rumbo en recta: `RUMBO` e `IMU_ESCALA`

Sin encoders, el carro no sabe si se tuerce. Con `RUMBO:ON`, cada `FORWARD` toma como objetivo el ángulo de ese momento y, con cada muestra del giroscopio (100 Hz), un control PI (proporcional-integral) calcula `error = objetivo − ángulo`. La parte proporcional da 1 % de diferencia de potencia por grado (`RUMBO_KP_MIL_GRADO` = 10); la integral acumula el error (`RUMBO_TI_MS` = 1,500 ms) y aprende el desbalance de los lados. La corrección se limita a ±15 % (`RUMBO_MAX_MIL` = 150): el lado izquierdo se multiplica por (1 − c), el derecho por (1 + c), sin pasar de 255; con 3.0° de error en AUTO quedan 201 y 216. Con más de 30° de error o sin giroscopio vuelve a potencias fijas. Arranca apagado y se apaga al (re)conectar; `RUMBO:<n>` mueve el objetivo hasta ±20°; lo aprendido pasa al siguiente avance.

`IMU_ESCALA:<ppm>` corrige la escala del giroscopio: multiplica la velocidad angular por (1,000,000 + ppm) / 1,000,000 (ppm = partes por millón; +10,000 = ×1.01; tope ±50,000, pues el fabricante promete ±3 %). La mide la app con vueltas completas, solo se acepta con el carro detenido y se guarda en NVS; `CAPS` la anuncia como `IMUS=`.

### Paro por láser

El láser mide cada 33 ms (`TOF_PERIODO_MS`) sin detener `loop()`. Menos de 8 mm es basura; 8190, 65535 o más de 1,200 mm es «libre»; lo demás es medida válida, y la distancia informada es la mediana de las últimas 3. Frena si el perfil es AUTO, va avanzando, `TOF_USO:1` está activo (`TOF_USO:0` lo apaga) y hay 2 lecturas válidas seguidas menores de `TOF_PARO_MM` (120 mm en v1.1; 150 mm en v3): encola `__STOP_AUTO`, que `loop()` ejecuta en esa vuelta, y avisa `TOF_PARO:<mm>` una vez por avance. Desde `SEG=2`, un `FORWARD` en AUTO no arranca con el frente confirmado a menos de `TOF_BLOQUEO_MM` (150 mm; 180 en v3). En v3, un «sin eco» aislado entre dos lecturas cercanas no rompe la racha. En MANUAL, en reversa, parado o girando no frena.

### Los dos vigilantes

El watchdog de comandos apaga los motores, vuelve a AUTO y avisa `WD:<n>` si hay motores encendidos y pasan más de 1,000 ms sin un comando que lo renueve (movimientos, `STOP`, `EBRAKE`, `MODO_*`, `BAL_*`, `GIRAR`; no `PING`, `RUMBO`, consultas ni, desde `SEG=2`, texto desconocido). La app y la página repiten el comando cada 250 ms: toleran perder tres. El watchdog de hardware (`esp_task_wdt`) reinicia el chip si `loop()` pasa 5,000 ms sin avisar que sigue vivo. Capas completas: subsistema G.

### Tramas `SENS` y `CAPS`

`SENS:<ángulo x10>,<mm>,<banderas>` sale cada 100 ms (`SENS_PERIODO_MS`), solo con cliente y perfil AUTO. El ángulo va en décimas de grado (positivo = izquierda; 0 si el giroscopio no está listo); `<mm>` es la distancia filtrada (1,200 = libre; 0 si el láser cayó). Las banderas suman 1 (giroscopio listo), 2 (láser vivo), 4 (última medida válida), 8 (giroscopio calibrando) y 16 (giro en curso).

`CAPS` dice qué funciona y qué firmware es. Formato de v3 (una sola línea):

```
CAPS:IMU=<0|1>,TOF=<0|1>,TOFR=<n>,IMUR=<n>,IMUM=<n>,IMUE=<n>,RUMBO=1,IMUS=<ppm>,
SEG=3,TUSO=<0|1>,TOFP=<mm>,FW:<hash>-<fecha>
```

Sale al recibir `CAPS?` (la app lo pide al conectar) y se repite sola cuando cambia. `TOFR` e `IMUR` cuentan reintentos del láser y del sesgo; `IMUM` es el motivo de falla del giroscopio (0 = ninguno); `IMUE`, su estado (2 = listo). v1.1 no trae `SEG`, `TUSO` ni `TOFP`; su respuesta real del 10-nov fue `CAPS:IMU=1,TOF=1,TOFR=1,IMUR=0,IMUM=0,IMUE=2,RUMBO=1,IMUS=0,FW:6d1b213-20261110`. Las claves nuevas van antes de `FW:` y sin `IMU=`, `TOF=` ni `RUMBO=`, que busca la app.

## B.3 Cómo lo hicimos

- **Lógica en C99 y capa fina de Arduino.** Las librerías de WiFi y servidor son C++, pero la lógica no las necesita: así se compila y prueba en PC con un `hw_*` falso (`firmware_carro/firmware_carro.ino`, `pruebas_firmware/README.md`).
- **Texto plano, no JSON.** La ESP32 no debe gastar ciclos ni memoria en estructuras complejas (`docs/protocolo.md` §1). Los movimientos no se contestan porque el firmware modular se escribió para comportarse como el original (`docs/pendientes_firmware.md`).
- **Red propia.** El repo dice que así los clientes se conectan sin router externo (`docs/glosario.md`) ni internet (`docs/manual/capitulos/02_arquitectura.md`). Canal fijo, para que la cámara no busque canal tras una caída; DHCP desde `.10`, para reservar la `.2`; 13 dBm, porque algunas powerbanks leen los picos del radio como cortocircuito (`firmware_carro/config.h`).
- CONFIRMAR CON EL EQUIPO: ¿hubo otra razón para crear una red propia y no usar el WiFi de la escuela, además de no depender de un router externo?
- **`PING` no renueva el watchdog.** La página lo manda cada 800 ms, menos que 1,000 ms: si lo renovara, el carro seguiría avanzando con el botón suelto. Un texto desconocido tampoco lo renueva desde `SEG=2`: antes el carro seguía avanzando sin latido, sin freno láser y sin control de rumbo (`firmware_carro/carro.c`).
- CONFIRMAR CON EL EQUIPO: ¿por qué 1,000 ms para el watchdog de comandos y 5,000 ms para el de hardware? El repo solo pide que el latido de 250 ms sea mucho menor (`firmware_carro/config.h`).
- **Balance por perfil.** En AUTO el carro se desviaba unos 6 cm a la derecha en 2 s y 207/210 corregía ~1.5 %: por eso el balance es un factor de 800 a 1,000 por perfil, calibrado desde la app y guardado en NVS (`docs/perfiles_velocidad.md`).
- **Giro por ángulo, no por tiempo.** El 18-oct, por tiempo, 10 de 14 giros a la izquierda y 2 de 17 a la derecha quedaron a ±3° de 90° (calibración de la derecha vencida); por ángulo, 10 de 10 y 5 de 8 (`docs/validacion_sensores.md`, firmware anterior a v1.1).
- **`RUMBO` con tope y apagado.** En J14, rectas de 150 cm se desviaron de 5 a 20 cm (`firmware_carro/rumbo.h`); las ganancias se eligieron en simulación, no en el carro (`nucleo_c/pruebas_precision_j16.c`). La app lo enciende solo si `CAPS` trae `RUMBO=1` (`docs/protocolo.md` §4b).
- **Paro láser más conservador en v3.** El pulso de arranque (250 ms, unos 9.5 cm) y la distancia del sensor al frente (`e`, no medida) podían hacer corto 120 mm: v3 usa 150 mm y bloqueo de 180 mm (`firmware_carro/config.h`, `docs/investigacion_autonomo/10_seguridad_funcional.md`).
- CONFIRMAR CON EL EQUIPO: ¿por qué el paro por láser solo actúa en perfil AUTO y avanzando, y no en MANUAL?
- CONFIRMAR CON EL EQUIPO: ¿qué significan las letras SEG en `SEG=2` y `SEG=3`?

## B.4 Cómo lo probamos y con qué resultado

| Qué se probó | Cómo | Resultado | Etiqueta |
|---|---|---|---|
| Carga y conexión de v1.1 | `FW:6d1b213-20261110` grabado; USB y app (10-nov) | `CAPS` con `RUMBO=1,IMUS=0`; giroscopio listo, láser vivo | **VALIDADO EN EL CARRO** (v1.1, `docs/estado_20261110.md`) |
| Esquive con caja a ~100 cm (B6) | 6 corridas del 10-nov en recta de 200 cm, con `RUMBO` y láser | 6 de 6 sin tocar la caja; 5 de 6 terminaron solas; error lateral de 0 a 10 cm, a ojo | **VALIDADO EN EL CARRO** (v1.1; sistema completo, `docs/pruebas_fisicas_20261110.csv`) |
| `GIRAR` | 23 giros de esas corridas: pedido contra medido por el giroscopio | Error de −3° a +5°; 21 de 23 dentro de ±3° (cuenta propia) | **VALIDADO EN EL CARRO** (v1.1) |
| `RUMBO` | Mismas corridas; `RUMBO?` al final | Término aprendido de −13 a +28 milésimas; sin comparar con y sin `RUMBO` | **VALIDADO EN EL CARRO** (corre); **NO MEDIDO** (cuánto mejora) |
| `IMU_ESCALA` | 10-nov: `IMUS=0`; PC: 25 casos | Nunca se usó una corrección distinta de 0 en el carro | **SIMULADO**; **NO MEDIDO** |
| Paro por láser | B6 con láser en uso (v1.1); caja a 37 cm: láser 371 mm (20-oct) | Sin prueba aislada de frenado (P12); el CSV no registra `TOF_PARO` | **NO MEDIDO** (frenado); **VALIDADO EN EL CARRO** (exactitud, firmware anterior a v1.1) |
| Watchdog de 1,000 ms | 15-oct: la app se cayó a media maniobra | El carro se detuvo «al segundo», sin cronómetro; corte de WiFi en marcha (B9): no se hizo | **VALIDADO EN EL CARRO** (observado, firmware anterior a v1.0); **NO MEDIDO** (B9) |
| Watchdog de hardware | El arranque imprime `5000 ms` (13-oct) | No se provocó el reinicio ni se midieron los pines de motor al reiniciar | **NO MEDIDO** |
| `PING` y `PONG` | La app midió con el carro (13-oct) | 30 ms de ida y vuelta; el efecto de `TCP_NODELAY` no se midió | **VALIDADO EN EL CARRO** (firmware anterior a v1.1); **PENDIENTE** |
| Tiempo de `loop()` | `LOOP?` por USB | 1,797 µs sin cliente (20-oct); 8,000 µs con la app (21-oct); 12,936 µs al arrancar v1.1 (10-nov) | **VALIDADO EN EL CARRO**; con la app pasa de 5,000 µs |
| v3 y página, en PC | `make -C pruebas_firmware test` (16-nov); compilación; `scripts/verificar_pagina.py` | 567 casos, 0 fallos; 1,034,622 bytes (78 %); servidor falso 60 casos; hash de la página ok. La v3 nunca se grabó; la página, sin prueba de manejo | **SIMULADO**; **NO MEDIDO** |

## B.5 Límites conocidos

- **La v3 nunca corrió en el carro.** Sus umbrales (150 y 180 mm) y la tolerancia al «sin eco» son **SIMULADO**; la distancia del sensor al frente (`e`) sigue sin medirse (`docs/estado_20261116.md`).
- **El láser no ve todo.** Mira un cono angosto: una pata delgada o un objeto oscuro puede no dar eco, y sin eco no frena. Solo lee milímetros (8190 y 65535 cuentan como libre) y no hay sensor trasero: en reversa no frena (`docs/limites_conocidos.md`).
- **`STOP` no es instantáneo.** Pone los PWM en 0 y el carro rueda por inercia; no se midió la distancia de frenado ni los pines del driver al reiniciar el chip (`docs/limites_conocidos.md`).
- **Un solo cliente.** Un segundo dispositivo expulsa al primero y dispara un `STOP` (`docs/pendientes_firmware.md`).
- **`loop()` pasa de 5 ms** con la app conectada (`LOOP_MAX_US` es métrica, no garantía), y el `PONG` lo manda la tarea de red: prueba que el enlace vive, no que `loop()` ejecute (`docs/investigacion_autonomo/03_esp32_arduino.md`).
- **La página tiene el JavaScript fijado por hash.** Pendiente conocido: al apagarse la pantalla manda `EBRAKE` pero no detiene su latido (`docs/pendientes_firmware.md`).
- **Depende del core Arduino-ESP32 3.3.12** (con 2.x no compila). Un comando de más de 31 caracteres se trunca y queda como desconocido.

## B.6 Términos clave

- **SoftAP**: modo en que la ESP32 crea su propia red WiFi (`CarroFSI`, `192.168.4.1`).
- **`loop()`**: función que Arduino repite sin parar; aquí hace todo el trabajo.
- **Cola de comandos**: 32 ranuras FIFO entre la tarea de red y `loop()` (`COLA_COMANDOS_MAX`).
- **Perfil AUTO o MANUAL**: dos juegos de velocidad (210 y 207; 255 y 252); el carro arranca en AUTO.
- **Balance**: factor del lado izquierdo (800 a 1,000) que endereza la recta; vive en NVS.
- **Watchdog**: vigilante que frena o reinicia si algo no ocurre a tiempo (1,000 ms de comandos; 5,000 ms de hardware).
- **`__STOP_AUTO`**: paro interno del firmware (láser, conexión, desconexión); la app no lo manda.
- **Anticipación**: milisegundos que el carro sigue girando tras frenar; el firmware la aprende.
- **PI y ppm**: control proporcional-integral (error de ahora más acumulado); partes por millón (10,000 ppm = 1 %).
- **`TOF_PARO_MM`**: umbral del paro por láser (120 mm en v1.1; 150 mm en v3); `TOF_BLOQUEO_MM`, el de no arrancar.
- **`SENS` y `CAPS`**: telemetría cada 100 ms y lista de capacidades con la versión.

## B.7 Preguntas probables del profesor

1. **¿Por qué el carro crea su propia red?** Así el teléfono, la cámara y el carro se hablan sin router ni internet (`docs/glosario.md`). No hay otro motivo registrado, por ejemplo frente al WiFi de la escuela.
2. **¿Qué pasa si el teléfono deja de mandar órdenes?** Si el WebSocket se cierra, el carro frena al instante; si sigue abierto pero callado, el watchdog apaga los motores a los 1,000 ms. Lo vimos una vez con la app caída (15-oct); el corte de WiFi en marcha no se midió.
3. **¿`STOP` tiene prioridad?** Entra a la misma cola FIFO y no se salta la fila; su prioridad es que, con la cola llena, no se descarta. En PC, 100,000 comandos con dos hilos no perdieron ningún `STOP` (**SIMULADO**, `docs/sensores.md`).
4. **¿Cómo va recto y cómo gira un ángulo exacto?** Recto con `RUMBO`, un control PI sobre el giroscopio limitado a ±15 %; el giro con `GIRAR`, que frena cuando el ángulo medido más la inercia aprendida llega al objetivo. Ambos corrieron en v1.1 (10-nov); cuánto mejora `RUMBO` no se midió.
5. **¿Qué está probado en el carro?** La v1.1: conexión, giros, `RUMBO` y el esquive del 10-nov. La v3 solo en PC. No medimos el frenado por láser aislado, la escala distinta de 0 ni el reinicio por el watchdog de hardware.
