# Subsistema G · Seguridad

## G.1 Qué es y para qué sirve

La seguridad es el conjunto de capas que detienen el carro cuando algo sale mal: un objeto muy cerca, un WiFi que se cae, una app que se traba o una persona que toca «PARO». Cada capa cubre una falla que las otras no ven, y las más importantes viven en el ESP32 del carro, así que siguen funcionando aunque el teléfono falle. Sobre la evidencia: lo observado en el carro es de la v1.1 (firmware `FW:6d1b213-20261110`); la seguridad de la v3 (`SEG=3`) solo está compilada y probada en PC: **SIMULADA**.

Más detalle: capítulos 2 y 11 del manual (`docs/manual/capitulos/02_arquitectura.md`, `docs/manual/capitulos/11_operacion.md`) y `docs/seguridad.md`.

## G.2 Cómo funciona, paso a paso

Primero, una vuelta de `loop()`, el ciclo principal del ESP32 (`firmware_carro/firmware_carro.ino`). El paro por láser y la ejecución del STOP caen en la misma vuelta:

```
loop():  esp_task_wdt_reset()           vigilante de hardware (5,000 ms)
         sensores_actualizar()          lee el láser; si toca, encola
                                        __STOP_AUTO y avisa TOF_PARO
         carro_procesar_cola()          ejecuta lo encolado, STOP incluido
         carro_vigilar_comunicacion()   watchdog de comandos (1,000 ms)
         carro_actualizar_keepalive()   pulso para que no se apague la powerbank
         enviar_mensajes_de_sensores()  SENS, TOF_PARO y WD hacia la app
```

Estas son las capas, de la que actúa primero a la que actúa al final. No son pasos de un solo evento: cada una cubre una falla distinta.

1. **Antes de salir** (`EjecutarRutaFragment.kt`). «Iniciar ruta» comprueba conexión y video, pide el perfil AUTO (espera su `ACK_AUTO`) y aprende el piso con el carro quieto; si ve algo delante dice «Despeja el frente del carro». «Confirmar y salir» cuenta 3, 2, 1 y el PARO la cancela.
2. **Paro por láser** (`sensores.c`, `tof.c`). Solo en perfil AUTO, con el carro avanzando y «Usar sensor de distancia» encendido. Con 2 lecturas válidas seguidas bajo `TOF_PARO_MM` (120 mm en v1.1; 150 mm en v3) encola `__STOP_AUTO` y avisa `TOF_PARO:<mm>`, una vez por avance. «Sin eco» cuenta como libre. Corre en el carro, sin el teléfono. Antes, el navegador ya frena con el láser a unos 400 mm (`OBSTACULO_LASER_MM`).
3. **STOP en la cola** (`carro.c`). Todo comando entra a una cola de 32 lugares (`COLA_COMANDOS_MAX`) que `loop()` vacía en orden. `STOP`, `EBRAKE` y `__STOP_AUTO` hacen lo mismo: `carro_detener()` aborta el giro y pone los cuatro PWM en 0 (no hay freno activo en el código). Su «prioridad» es que, con la cola llena, no se descartan: se tira primero un `PING` viejo y luego el comando más viejo que no sea STOP. No se adelantan: la cola es FIFO.
4. **Supervisión de la app** (`ModoSemiautonomo.kt`, cada 50 ms). En una ruta, la app la cancela y manda `STOP` si pasan 1,000 ms sin video (`TIMEOUT_CAMARA_MS`) o 1,500 ms sin `SENS` (`SILENCIO_SENS_MS`), si llega `WD:<n>`, o si llega `TOF_PARO` y el navegador sigue pidiendo `FORWARD`. El navegador (`navegador.c`) se adelanta: se detiene con 700 ms sin video (`NAV_MAX_HUECO_VIDEO_MS`, causa 5) y con el láser mudo 600 ms a mitad de ruta (`NAV_TOF_CAIDO_MS`, causa 19, v3). El botón PARO actúa al tocar, no al soltar.
5. **Watchdog de comandos** (`carro_vigilar_comunicacion`, `TIEMPO_WATCHDOG_MS` = 1,000 ms). Si los motores andan y pasan más de 1,000 ms sin un comando de movimiento, apaga los motores, vuelve al perfil AUTO y avisa `WD:<n>`. Lo renuevan los movimientos, `STOP`, `EBRAKE`, `MODO_*`, el balance y `GIRAR`; no lo renuevan `PING`, `__STOP_AUTO`, `RUMBO` ni las consultas. La app manda un latido cada 250 ms (`HEARTBEAT_MS`): tolera perder tres. Si el WebSocket se cierra de forma explícita, `WS_EVT_DISCONNECT` encola `__STOP_AUTO` al instante.
6. **AYUDA** (navegador, `tick_ayuda`). No es un freno: es el estado al que pasa el navegador cuando no puede seguir con seguridad, por ejemplo con los dos lados ocupados. El carro queda quieto y el teléfono vibra y pide que tomes el control. Hay una cruceta de mantener apretado (soltar es `STOP`), «Continuar la ruta» y PARO. Si nadie actúa en 180,000 ms (`NAV_AYUDA_MAX_MS`) pasa a ERROR.
7. **Watchdog de hardware** (`esp_task_wdt`, `WATCHDOG_HW_MS` = 5,000 ms, `trigger_panic = true`). Si `loop()` deja de ejecutarse 5 s, el chip se reinicia. Tiene un hueco: el watchdog de 1,000 ms corre dentro de `loop()`, así que con `loop()` colgado nadie corta los motores hasta ese reinicio (el PWM probablemente sigue; no se midió).
8. **Interruptor de modo.** No existe un interruptor «autónomo sí o no». En el firmware, `MODO_MANUAL` y `MODO_AUTO` eligen el perfil de velocidad (MANUAL 255/252; AUTO 210/207) y solo AUTO frena por láser; el carro arranca en AUTO y vuelve a AUTO al conectar, desconectar o saltar el watchdog. En la app, lo autónomo es una ruta en marcha que termina por llegada, error, PARO o cancelación. Hay dos ajustes parecidos a un interruptor: «Usar sensor de distancia (láser)» (`TOF_USO`; apagado, no hay freno láser) y el perfil de Manual («rápido» es MANUAL; «lento» es AUTO).
9. **Interruptor del portapilas 4×AA.** Corta el voltaje `VDD` del driver `TC1508A`. El ESP32 y la cámara siguen con la powerbank, así que la sesión de diagnóstico no se pierde (`docs/manual/capitulos/11_operacion.md`). Es la única capa que no depende de software ni de los cables de control.

### Las capas en una tabla

| Capa | Quién la ejecuta | Qué la dispara | Qué hace | Tiempo o valor | Etiqueta |
|---|---|---|---|---|---|
| Antes de salir | App | «Iniciar ruta» | No sale si hay algo delante | Antes de moverse | **VALIDADO EN EL CARRO** (v1.0) |
| Paro por láser | ESP32 | 2 lecturas bajo el umbral | `__STOP_AUTO` y `TOF_PARO` | 120 mm (v1.1), 150 mm (v3); 33 a 103 ms | **NO MEDIDO** (v1.1); **SIMULADO** (v3) |
| STOP en la cola | ESP32 | `STOP`, `EBRAKE` | Cuatro PWM en 0 | Ida por WiFi, unos 15 ms | **NO MEDIDO** (el total) |
| Supervisión de la app | Teléfono | Sin video, sin `SENS`, `WD` | Cancela y manda `STOP` | 700, 1,000 y 1,500 ms | **SIMULADO** |
| Watchdog de comandos | ESP32 | Sin comando de movimiento | Motores a 0, perfil AUTO | 1,000 ms | **VALIDADO EN EL CARRO** (observado) |
| AYUDA | Navegador y persona | Sin salida segura | Detiene y espera | 180,000 ms como máximo | **VALIDADO EN EL CARRO** (v1.1) |
| Watchdog de hardware | ESP32 | `loop()` colgado | Reinicia el chip | 5,000 ms | **NO MEDIDO** |
| Interruptor de modo | Persona y app | Perfil o `TOF_USO` | MANUAL no frena por láser | Sin tiempo | **SIMULADO** (pruebas de PC) |
| Portapilas | Persona | La mano | Corta `VDD` del driver | Sin tiempo | **NO MEDIDO** |

### El orden en un diagrama

```
 Qué pasa                      Quién frena                      Cuándo actúa
 ----------------------------------------------------------------------------
 Algo muy cerca al frente  ─▶  láser del ESP32 (__STOP_AUTO)  ─▶  decenas de ms
 Tocas PARO o sueltas      ─▶  app: STOP por WiFi             ─▶  unos 15 ms
 Falla video, SENS o WD    ─▶  app: cancela la ruta y STOP    ─▶  0.7 a 1.5 s
 Se cae el WiFi o la app   ─▶  watchdog de comandos (ESP32)   ─▶  1.0 s
 Se cuelga loop()          ─▶  watchdog de hardware (reinicio) ─▶ 5.0 s
 Nada de lo anterior sirve ─▶  tú: interruptor del portapilas ─▶ lo que tardes
```

### Si se cae el WiFi

1. Sin cierre del WebSocket, el carro solo deja de recibir comandos: pasado 1,000 ms, el watchdog apaga los motores, pasa a AUTO y cuenta un `WD`. El modelo da un recorrido máximo de 20.7 cm en AUTO y 38.9 cm en MANUAL (`docs/investigacion_autonomo/10_seguridad_funcional.md`): **SIMULADO**.
2. El láser sigue protegiendo en AUTO, porque está en el carro. La app cancela la ruta a los 1,500 ms sin `SENS`, pero su `STOP` no cruza un WiFi caído: quien de verdad frena es el watchdog.
3. Al reconectar, el carro vacía su cola y ejecuta `__STOP_AUTO` (AUTO, `RUMBO` apagado): no debe arrancar solo. Ese es el criterio de la prueba B9 y P11, que **NO SE HA MEDIDO** (`docs/estado_20261110.md`).

### Si se bloquea o se cae la app

Si el proceso muere, Android cierra el socket y el carro frena al instante por `WS_EVT_DISCONNECT`. Si la app queda viva pero congelada, el latido y el ciclo de la ruta corren en el hilo principal: dejan de salir y manda el watchdog de 1,000 ms. Si la app pasa a segundo plano o se gira el teléfono, `onPause` manda `STOP` a propósito (`MainActivity.kt`).

## G.3 Cómo lo hicimos

- **El freno de emergencia está en el carro y usa el láser, no la cámara.** Se descartó frenar solo con visión: la luz del salón cambia y el video llega unos 320 ms tarde (`docs/historia.md` §3, `docs/latencia.md`).
- **De 120 a 150 mm en v3.** El modelo pide `P = 95 + 10·e` mm en crucero (`e`: distancia del sensor al frente, en cm) y de 115 a 135 mm más `10·e` con el pulso de arranque (250 ms a PWM 255, unos 9.5 cm). Con 150 mm se cubre `e` de 2.5 a 4.5 cm según el sesgo, y `TOF_BLOQUEO_MM` sube a 180 mm para guardar la histéresis de 30 mm. `e` y el frenado **NO SE MIDIERON**: es una recomendación de modelo (`firmware_carro/config.h`, `10_seguridad_funcional.md` §6.2). Se revierte con `-DTOF_PARO_MM=120 -DTOF_BLOQUEO_MM=150`.
- **`TOF_BLOQUEO_MM`, desde `SEG=2`.** Antes, cada reintento de `FORWARD` daba un pulso de 250 ms a plena potencia contra el obstáculo; ahora, con el frente confirmado más cerca, el avance no arranca (`firmware_carro/config.h`).
- **Dos lecturas seguidas, no una**, para que un pico suelto no frene (`docs/sensores.md`). En v3, un «sin eco» aislado no rompe la racha (`TOF_PERDIDAS_TOLERADAS` = 1): una pata delgada da «sin eco» entre dos lecturas cercanas.
- **Watchdog de 1,000 ms con latido de 250 ms**: relación de 4 a 1, tolera perder tres latidos. `PING` no renueva el reloj: si lo hiciera, el carro seguiría andando con el botón suelto, porque la página manda `PING` cada 800 ms (`firmware_carro/carro.c`).
- **Supervisión extra en la app (J16, 31-oct).** La conexión no corta por silencio (`readTimeout(0)` en `Enlace.kt`): si el WiFi cae sin cerrar el socket, la app no se entera sola. De ahí salen `SILENCIO_SENS_MS`, `WD:<n>` y el corte del latido tras `TOF_PARO` (`docs/estado_20261110.md` §2.3).
- **AYUDA en vez de ERROR** (15-oct, commit `1699d6f`): si no puede esquivar, el carro queda quieto y una persona toma el control.
- **No se hizo:** rechazar `FORWARD` con el láser caído y poner resistencias pull-down de unos 10 kΩ en los pines del driver (solo propuestas, `10_seguridad_funcional.md` §6.3). La parada por «flanco incierto» de la v4 se descartó (subsistema I).

CONFIRMAR CON EL EQUIPO: ¿por qué se eligió 1,000 ms para `TIEMPO_WATCHDOG_MS`? La constante está desde el primer firmware modular (10-oct) y el repo solo la justifica después.

CONFIRMAR CON EL EQUIPO: ¿el freno por láser se limitó al perfil AUTO porque a velocidad MANUAL 120 mm no alcanzaría? El repo da esa cuenta, pero no dice que fuera la razón.

## G.4 Cómo lo probamos y con qué resultado

| Qué se probó | Cómo | Resultado | Etiqueta |
|---|---|---|---|
| Watchdog con la app caída | Observación en una prueba real, 15-oct (`docs/bitacora.md`) | La app se cerró en plena maniobra y el watchdog detuvo el carro «al segundo»; sin cronómetro | **VALIDADO EN EL CARRO** (firmware previo a v1.0, mismo 1,000 ms) |
| «Iniciar ruta» con la caja a 40 cm | B6, 27-oct (`docs/reporte_J14.md`) | La app no arrancó y pidió despejar | **VALIDADO EN EL CARRO** (v1.0) |
| Límite de tiempo del giro | J13, 21-oct (`docs/reporte_J13_fisico.md`) | Giros atascados de 90° cortados por `GIRO_TIMEOUT` a 1,850 ms | **VALIDADO EN EL CARRO** (firmware de J13) |
| Lectura del láser | J12, 20-oct (`docs/validacion_sensores.md`) | 152 lecturas con la caja a unos 37 cm (sin regla): promedio 371 mm, de 367 a 378; un objeto a 34° del eje salió «libre» | **VALIDADO EN EL CARRO** (firmware de J12) |
| Pérdida de video en marcha | Recta de 150 cm, 15-nov (`docs/pruebas_fisicas_20261114.csv`, archivo local sin versionar, fila 4) | El video se cortó 720 ms; la ruta terminó en ERROR (causa 5) y el carro frenó tras unos 54 cm | **VALIDADO EN EL CARRO** (firmware v1.1; versión de la app sin anotar) |
| AYUDA en una ruta | B6, 10-nov, corrida 5 (`docs/estado_20261110.md`) | Entró en AYUDA sin tocar la caja; se usó la cruceta y «Continuar la ruta» | **VALIDADO EN EL CARRO** (v1.1, una corrida) |
| Ida y vuelta por WiFi | `PING` y `PONG` con el carro real (`docs/latencia.md`) | 30 ms | **VALIDADO EN EL CARRO** (firmware previo a v1.0) |
| `STOP` no se pierde con dos hilos | Estrés de PC con 100,000 comandos (`docs/reporte_J14.md`) | 12,472 `STOP` enviados, 0 perdidos | **SIMULADO** (v1.0) |
| Seguridad de v3 en PC | Pruebas de firmware y compilación, 16-nov (`docs/estado_20261116.md`) | 5 ejecutables, 567 casos, 0 fallos; servidor falso, 60 casos; `SEG=3` compila y no se grabó | **SIMULADO** (v3) |

**Lo que NO se ha medido** (**NO MEDIDO**; bloque M y bloques A a D de `docs/protocolo_prueba_fisica_autonomo_v2.md`):

- El paro por láser aislado: la distancia real de parada y la distancia `e` del sensor al frente (M5), y el pulso de arranque contra un obstáculo cercano (M6). No hay registro de un `TOF_PARO` real en los documentos de pruebas ni en las 7 cajas negras del 15-nov (archivos locales, no versionados).
- El corte de WiFi en marcha (B9 y P11), el reinicio por el watchdog de hardware, el estado de los pines del driver al reiniciar y el tiempo real del botón PARO y del interruptor del portapilas.
- La velocidad real en AUTO y MANUAL (M8: ¿19.3 o unos 30 cm/s?), la latencia de extremo a extremo (M9) y el ritmo del láser (M10).
- Todo el firmware `SEG=3` y todo el autónomo v3 en el carro.

## G.5 Límites conocidos

- **El láser mira casi un punto al frente.** Un objeto a 34° del eje salió «libre» (`docs/validacion_sensores.md`, 20-oct). Si ve patas finas reales no se sabe (M2, **NO MEDIDO**); en el banco, banquito y silla dan contactos con v3 (**SIMULADO**, `docs/estado_20261116.md`).
- **Objetos oscuros, mates o transparentes.** Pueden dar «sin eco», que el firmware toma como libre; la hoja de datos no garantiza nada bajo un gris de 17 % (`docs/investigacion_autonomo/10_seguridad_funcional.md` §2.5). En el banco, un bloque oscuro sin eco dio 40 contactos de 40 con v3 (**SIMULADO**, `docs/estado_v3_pendiente_pruebas.md`).
- **Luz.** Con sol fuerte el láser puede dar «sin eco»: según la hoja de datos, su alcance en exterior es de 40 a 60 % del de interior (`docs/investigacion_autonomo/01_vl53l0x.md`; no se midió en el carro).
- **Sensor caído, sin freno de firmware.** Con 3 errores seguidos o 500 ms sin lectura, el carro sigue como antes; la app solo cancela si la ruta contaba con el láser (causa 19).
- **El arranque.** El pulso de 250 ms recorre hasta unos 9.5 cm y el freno puede no alcanzar si `e` es de 2 cm o más (modelo, **NO MEDIDO**). `TOF_BLOQUEO_MM` solo ayuda con un objeto ya confirmado y no está en el firmware v1.1.
- **Dónde no hay freno.** El láser no frena en MANUAL, en reversa, parado ni en giro (`docs/sensores.md`), y no hay sensor trasero. El watchdog de 1,000 ms tampoco vigila un giro por ángulo, que tiene su propio límite: 500 ms más 15 ms por grado (1,850 ms para 90°).
- **Un `loop()` colgado** deja los motores sin vigilancia hasta 5 s (unos 96 cm en AUTO, modelo **SIMULADO**). Al reiniciar, los pines del driver quedan flotantes según la hoja del ESP32; que bajen a cero no se midió. Si se pierde la tierra común, las entradas del driver flotan y los motores pueden quedar encendidos: solo corta el interruptor del portapilas (`docs/seguridad.md` §1.3).
- **«Continuar» tras una AYUDA sin mover el carro.** Después de una AYUDA por giro (causas 16 a 19), la v3 ignora los cuadros del centro hasta el siguiente giro y solo queda el freno láser del firmware. Se corrigió solo en v6 (`docs/progreso_v6.md`, rama `feat/autonomo-v6`). Regla: mover el carro con la cruceta antes de «Continuar».
- **Firmware en el carro.** El último grabado es el v1.1 (10-nov): sin `SEG=2` ni `SEG=3`, un texto desconocido renueva el watchdog y `FORWARD` no se bloquea cerca de un obstáculo.

CONFIRMAR CON EL EQUIPO: ¿se grabará `SEG=3` antes del 1-dic-2026 o el carro llegará con el firmware v1.1?

## G.6 Términos clave

- **`TOF_PARO_MM`**: distancia bajo la cual el láser frena el carro (120 mm en v1.1; 150 mm en v3).
- **`TOF_BLOQUEO_MM`**: con el frente confirmado más cerca que este valor (180 mm en v3), un `FORWARD` en AUTO no arranca; no existe en v1.1.
- **`__STOP_AUTO`**: `STOP` interno del firmware (láser, conexión y desconexión); además deja el perfil en AUTO.
- **`STOP` y `EBRAKE`**: comandos de paro; hacen lo mismo. La app manda `STOP`; `EBRAKE` viene de la página web del carro (`firmware_carro/pagina.h`).
- **Cola de comandos**: 32 lugares (`COLA_COMANDOS_MAX`) en orden de llegada; un `STOP` no se descarta.
- **Watchdog de comandos**: `TIEMPO_WATCHDOG_MS` = 1,000 ms sin comando de movimiento apaga los motores y avisa `WD:<n>`.
- **Watchdog de hardware**: `WATCHDOG_HW_MS` = 5,000 ms; reinicia el chip si `loop()` se cuelga.
- **`SENS`**: mensaje cada 100 ms en AUTO con ángulo y distancia; 1,500 ms de silencio cancelan la ruta.
- **Perfil AUTO y MANUAL**: dos juegos de velocidad (210/207 y 255/252); solo AUTO frena por láser.
- **AYUDA**: estado del navegador que detiene el carro y pide a una persona que tome el control.
- **«Sin eco»**: lectura del láser sin retorno (8190, 65535 o más de 1,200 mm); el firmware la toma como libre.
- **`SEG=3`**: versión de seguridad del firmware de la v3 (150 y 180 mm); solo compilada.

## G.7 Preguntas probables del profesor

1. **¿Qué pasa si se pierde el WiFi con el carro en marcha?** El watchdog del ESP32 apaga los motores cuando pasan 1,000 ms sin un comando de movimiento; el modelo da un recorrido máximo de unos 21 cm en AUTO. Se observó con la app caída (15-oct), pero el corte de WiFi en marcha no lo probamos en el carro.
2. **¿Cómo frena el carro ante un obstáculo y en cuánto tiempo?** El láser VL53L0X mide cada 33 ms. Con 2 lecturas seguidas bajo el umbral (120 mm en v1.1, 150 mm en v3), el ESP32 corta los motores sin esperar al teléfono. El modelo da de 33 a 103 ms hasta confirmar, más el recorrido; el paro láser aislado no se midió, así que no tenemos una distancia de parada medida.
3. **¿Qué pasa si el teléfono se apaga o la app se cae?** Si Android cierra el socket, el carro frena al instante; si no, frena el watchdog a 1,000 ms. Si la app pasa a segundo plano, `onPause` manda `STOP`.
4. **¿Hay un interruptor para apagar el modo autónomo?** No uno solo. Lo autónomo es una ruta en marcha y se cancela con PARO. Se puede apagar el freno láser («Usar sensor de distancia») o manejar en perfil MANUAL. El interruptor del portapilas corta la energía de los motores.
5. **¿Qué no cubre la seguridad?** Objetos que el láser no ve (patas finas, superficies oscuras o transparentes, cosas fuera del eje), el manejo en MANUAL y un `loop()` colgado, que tarda hasta 5 s en reiniciarse. Y todo lo de la v3 sigue sin probarse en el carro.
