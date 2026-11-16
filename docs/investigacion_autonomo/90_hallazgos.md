# 90 — Hallazgos de la revisión de código (Fase 2)

Rama `feat/autonomo-v3`. Fecha de la revisión: 15–16 nov 2026. **Todo lo marcado SIMULADO o NO MEDIDO no se midió en el carro.** El carro no se movió.

Cómo leer cada hallazgo: gravedad (crítico, alto, medio, bajo), archivo y línea, qué pasa hoy, fuente y confianza (alta = documentación primaria o medición propia; media = documentación secundaria o modelo propio con datos medidos; baja = inferencia mía, marcada), qué falla puede causar, recomendación, esfuerzo, riesgo de regresión, cómo verificar, y **estado en v3** (CORREGIDO, ENDURECIDO, APAGADO tras bandera, DISEÑO, o ABIERTO).

Las referencias «doc NN» son los documentos de esta carpeta; ahí están las fuentes completas con URL y fecha de consulta. Los cálculos de frenado salen de [calculos/frenado.py](calculos/frenado.py) (modelo propio; `e`, la distancia del sensor al frente del carro, NO está medida).

Hallazgos de gravedad **crítica**: ninguno. Con el modelo propio, a velocidad de crucero AUTO el carro frena dentro de TOF_PARO (distancia de frenado de 2.1 a 4.2 cm, doc 10). Los hallazgos altos son los que, en un caso concreto, hacen que el freno llegue tarde o no llegue.

---

## Respuestas a las preguntas del encargo

**¿Por qué el banquito y la silla siguen dando contactos y AYUDA, y cuánto es código y cuánto simulador?** Tres causas, ordenadas por lo que se pudo demostrar con el banco (SIMULADO):
1. *Código (corregido en parte).* Con el láser confirmando (por ejemplo 209 mm) y luego un «sin eco» más cámara despejada, el carro reanudaba el avance. El «sin eco» llega seguido en una pata fina. Se arregló con memoria del eco más retroceso antes de girar (etapa 2): banquito 765 → 849 ok y 79 → 43 contactos; silla 316 → 435 ok y 34 → 21 contactos, ayudas 58 → 33 y 64 → 15.
2. *Política aprobada, no error.* En la mayoría de las corridas que aún fallan el carro pasa **entre** las patas sin esquivar (esquivas=0): las patas quedan a los lados, fuera del cono del láser; el color las ve (cercanía 2–3, 10–60 celdas) pero una sola fuente con láser libre no detiene el carro. Cambiar esto es cambiar la política que decidiste; se deja como está y se anota. En el banco esa corrida cuenta como `bajo_mueble` (entró entre las patas sin tocar).
3. *Modelo del simulador, probablemente optimista y pesimista a la vez.* El simulador modela el láser como «el grupo más cercano dentro del cono»; la documentación primaria dice que el VL53L0X entrega un **promedio** de lo que cae en su cono (doc 01), así que una pata delgada «jala» la lectura hacia el fondo. Eso no está en el banco: no se sabe si el láser real verá las patas (NO MEDIDO). Hasta medirlo, ninguna cifra de banquito o silla se debe leer como del carro. Esto pesa más que cualquier ajuste de código.

**¿Exposición y balance de blancos 100 % automáticos con piso aprendido?** Es aceptable **como está** (el aprendizaje del piso absorbe la deriva lenta; las pruebas SIMULADAS de exposición pasan), pero no se debe confiar sin medir: no hay datos de cuánto deriva la exposición entre sol y sombra (doc 06). Bloquear la exposición **no es seguro de hacer a ciegas**: el driver y la hoja de datos discrepan en los bits de AEC, AGC y el layout del registro manual (doc 06, confianza alta). Si hace falta, se hará solo con el balance de blancos (el único bit en el que los tres coinciden), tras leer registros, con diseño en dos fases. En v3 queda como DISEÑO, no código.

**¿La máquina de estados tiene estados muertos u oscilaciones, o presupuestos sin acotar?** Estados muertos: no encontré ninguno alcanzable en la simulación (el banco termina todas las corridas en ok, contacto, bajo_mueble, ayuda; sin_terminar=0 en los 23 escenarios). Presupuestos: acotados por obstáculo (6) y por ruta (18), con un hueco: `navegador_ayuda_continuar` los reinicia (H-12) y no hay tiempo máximo global ni detección de atasco (H-13). Oscilación posible: el umbral de retroceso del eco (350 mm) tiene una sensibilidad no monótona en el simulador (H-19), señal de ajuste fino al simulador.

**¿La estrategia de rodeo es sólida frente a alternativas más simples?** Para un carro sin odometría real y con un láser de un solo rayo, el rodeo por tramos (girar, avanzar paralelo hasta que el costado salga de la vista, volver) es razonable y está más acotado que VFH/DWA, que necesitan un mapa de ocupación local o un escáner (doc 09). La alternativa más simple, Bug (seguir el borde del objeto), exige tocar o medir el borde, que el láser de un rayo no da. Lo que sí es frágil: depende de que la cámara «vea» el costado (constantes de geometría inconsistentes, H-16) y de un tiempo de avance (sin odometría). El margen de 230 mm cuesta ~1 mm de error lateral final y evita contactos (se probó 180 mm: iguala a v1.1 pero produce 2 contactos en `objeto_largo90`).

---

## Hallazgos de gravedad alta

### H-01 — El pulso de arranque a plena potencia y TOF_BLOQUEO 150 mm
- Dónde: [config.h:123,128](../../firmware_carro/config.h) (ARRANQUE_PLENO 250 ms), [config.h:249-254](../../firmware_carro/config.h) (TOF_PARO 120, TOF_BLOQUEO 150).
- Hoy: un FORWARD con el frente libre entre 150 y ~190 mm arranca a plena potencia 250 ms (38 cm/s, 9.5 cm sin frenar) antes de que el perfil AUTO baje a 19–30 cm/s.
- Fuente: cálculo propio (doc 10 y `calculos/frenado.py`) con la velocidad de 38 cm/s medida en el proyecto. Confianza **media**: depende de `e` (NO MEDIDO). Si `e` ≥ 2 cm, 120 mm es insuficiente para un arranque.
- Falla: golpe contra un objeto que apareció a menos de ~20 cm justo antes de un avance.
- Recomendación: TOF_PARO 150 y TOF_BLOQUEO 180 (más conservador, nunca menos). Esfuerzo bajo. Riesgo de regresión: más paradas cerca de objetos; el banco y las pruebas del firmware lo dirán.
- Verificación: pruebas de `pruebas_firmware` con el sensor simulado; medir `e` y la distancia de frenado reales (protocolo).
- Estado v3: ver etapa de firmware (se endurece, solo compilado).

### H-02 — Un «sin eco» rompe la racha de «cerca»
- Dónde: [tof.c:141-146](../../firmware_carro/tof.c).
- Hoy: 8190, 65535 o más de TOF_LIBRE ponen `cerca_seguidas = 0`. En una pata o un borde, una lectura perdida entre dos cercanas obliga a otras 2 lecturas válidas (+33 ms a 30 Hz, +100 ms al ritmo real de 100 ms) antes de frenar.
- Fuente: lectura de código + doc 01 (el sensor devuelve estados de error en superficies angostas o a ángulo; ST community y hoja ST) + doc 10 (presupuesto de latencia). Confianza **media**.
- Falla: el freno por láser llega 33–100 ms tarde justo en objetos delgados (el caso del banquito).
- Recomendación: no reiniciar la racha con **un** «sin eco» seguido; reiniciar solo con dos «sin eco» seguidos o una lectura válida lejana. Siempre en el sentido de frenar más, nunca menos. Esfuerzo bajo; riesgo: paradas extra por una lectura cercana aislada rodeada de pérdidas (un objeto realmente cerca).
- Estado v3: ver etapa de firmware.

### H-03 — El firmware no lee el estado de la medición (RangeStatus)
- Dónde: [firmware_carro.ino:242](../../firmware_carro/firmware_carro.ino) (`readRangeContinuousMillimeters`).
- Hoy: la biblioteca de Pololu devuelve solo el milímetro crudo. 8190 equivale a 8192 menos el estado 2 (inferencia mía, confianza baja); 65535 es el tiempo agotado de la biblioteca. No se distingue «sin objeto» de «señal débil» ni «fase fuera de rango».
- Fuente: doc 01 (hoja ST, foros ST y Pololu, código de la biblioteca). Confianza **alta** en que no se lee, **baja** en la interpretación de 8190.
- Falla: lecturas de baja confianza tratadas como válidas o como libre.
- Recomendación: leer el registro de estado (0x14) tras cada medición y exponerlo en `SENS` como bandera nueva (campo opcional, compatible). Esfuerzo medio; riesgo bajo si es solo diagnóstico.
- Estado v3: DISEÑO (no cabe sin medir con el sensor real qué estados aparecen en las patas).

### H-04 — Láser caído a mitad de ruta
- Dónde: [navegador.c](../../nucleo_c/navegador.c) `navegador_aporte_sens_imu`/`navegador_tick`, firmware `tof.c` (reintento solo con el carro quieto).
- Hoy (antes de v3): si el láser muere durante una ruta, el carro sigue con cámara sola, sin aviso.
- Fuente: doc 10 (principio de «fallar a seguro»; ISO 3691-4 como referencia general, no consultada completa) y doc 03 (sin recuperación de bus). Confianza **media**.
- Recomendación: detener la ruta con causa clara. Estado v3: **CORREGIDO** en la app (causa 19, a los 600 ms sin láser vivo en estados en movimiento; bandera `laser_caido_para`, por defecto encendida; 6 pruebas nuevas).

### H-05 — Sin recuperación del bus I2C
- Dónde: [firmware_carro.ino:482-523](../../firmware_carro/firmware_carro.ino), `tof_actualizar`.
- Hoy: ante SDA atascada solo se reintenta; no hay 9 pulsos de SCL ni XSHUT.
- Fuente: UM10204 (NXP), sección de bloqueo de bus (doc 01/03). Confianza **alta**.
- Falla: IMU o láser quedan muertos hasta apagar el carro; con H-04 el carro ya se detiene, pero no se recupera.
- Recomendación: 9 pulsos de SCL con el carro quieto y reintento. Esfuerzo medio; riesgo medio (toca pines).
- Estado v3: DISEÑO (se documenta; tocar pines sin poder probar en el carro se aplaza).

### H-06 — Pausas falsas del banco por coincidencia color + red
- Dónde: política de coincidencia en [fusion.c](../../nucleo_c/fusion.c) y `registrar_cuadro` en navegador.c.
- Hoy (SIMULADO): sol, sombra y reflejo producen paradas breves en 8–13 % de las corridas por la regla aprobada «color Y detector en la misma zona confirman aunque el láser esté libre»; sin esquives ni ayudas.
- Fuente: banco propio; el doc 08 recomienda diversidad de fuentes (Primatech 2012 sobre salvaguardas redundantes; confianza media).
- Falla: parada de 1–2 s sin razón (no un contacto).
- Recomendación: dejar la regla (la decidiste); considerar histéresis por duración si la prueba física confirma el problema.
- Estado v3: ABIERTO (no empeora respecto de v2).

### H-07 — Pata delgada: el color no la ve más allá de ~50 cm
- Dónde: `corredor.c` y MIN_CELDAS 7 en `obstaculos.c`.
- Hoy (SIMULADO por geometría): una pata de 30 mm ocupa 2–4 celdas a 50 cm y 0 a 80 cm.
- Fuente: doc 05 y `calculos/geometria_monocular.py`. Confianza **media** (la geometría usa campo, altura e inclinación SUPUESTOS).
- Recomendación: continuidad vertical (una raya de 3 celdas sirve; ya existe `NAV_VISTAZO_RACHA` para el vistazo) y medir la cámara real antes de cambiar el clasificador.
- Estado v3: DISEÑO.

---

## Hallazgos de gravedad media

### H-08 — Latencia de bucle superior a LOOP_MAX_US con la app conectada
- Dónde: [config.h:269](../../firmware_carro/config.h) (5000 µs) contra 6.8–8.4 ms medidos con la app (1.7 ms sin cliente).
- Fuente: medición del proyecto citada en doc 03 y su análisis (confianza **media**; causas, inferencia **baja**).
- Falla: el freno por láser tarda hasta un ciclo más (~3 ms), pequeño frente a los 100 ms entre lecturas.
- Recomendación: medir por fases con temporizadores (protocolo). Estado: NO MEDIDO.

### H-09 — WebSocket atascado y STOP de la app
- Dónde: cola de 32 comandos en [carro.c:547](../../firmware_carro/carro.c); AsyncTCP cierra a los 5 s.
- Hoy: STOP nunca se expulsa de la cola pero no se adelanta a lo anterior; con pérdida de paquetes TCP puede tardar hasta 1 s (RFC 6298).
- Fuente: doc 03 (RFC 6298, repos ESP32Async), confianza **alta** en la regla del RFC, **baja** en el efecto exacto con este SoftAP.
- Mitigación existente: el watchdog de 1 s (≈19 cm en AUTO, ≈38 cm en MANUAL) y el freno local por láser. Estado: sin cambio; riesgo residual documentado.

### H-10 — Watchdog no actúa durante GIRAR
- Dónde: [carro.c:781](../../firmware_carro/carro.c).
- Hoy: durante un giro por ángulo el watchdog de 1 s no actúa; el giro tiene su propio máximo (500 ms + 15 ms por grado, 200° máximo → 3.5 s) y su tope de seguridad.
- Fuente: lectura de código; doc 10. Confianza **alta**.
- Falla: un giro con un sensor de ángulo congelado dura hasta su tope (3.5 s); una pérdida de WiFi durante ese tiempo no lo detiene antes.
- Recomendación: el tope ya existe; verificar con una prueba de firmware que el giro termina por tiempo si el IMU se congela. Estado: ABIERTO (bajo riesgo).

### H-11 — Reinicio del chip por colgado: PWM puede seguir corriendo
- Dónde: [config.h:143](../../firmware_carro/config.h) (WATCHDOG_HW_MS 5000).
- Hoy: si `loop()` se cuelga con el PWM encendido, los motores siguen hasta el reinicio: 5 s ≈ 96 cm en AUTO (modelo propio, doc 10; confianza **media**).
- Recomendación: acortar a 2000–3000 ms tras medir el peor `loop()` real (H-08) o apagar el PWM desde la interrupción del watchdog. Estado: DISEÑO.

### H-12 — `navegador_ayuda_continuar` reinicia presupuestos
- Dónde: [navegador.c:5792](../../nucleo_c/navegador.c).
- Hoy: cada «continuar» devuelve las 6 esquivadas por obstáculo y las 18 por ruta.
- Falla: un obstáculo imposible puede consumir intentos sin límite si el usuario pulsa continuar; sin riesgo de contacto, sí de tiempo y desgaste.
- Recomendación: tope global (por ejemplo 3 continuaciones por ruta). Esfuerzo bajo; riesgo bajo.
- Estado v3: ABIERTO.

### H-13 — Sin detección de atasco ni de congelamiento
- Hoy: no hay comprobación de «el carro lleva N segundos sin avanzar según la pose ni el giroscopio» ni de lecturas del IMU idénticas congeladas.
- Fuente: doc 09 (progress checker de Nav2 como patrón; confianza media) y doc 02.
- Recomendación: temporizador de progreso en `navegador_tick`. Estado: DISEÑO.

### H-14 — Frescura medida desde la llegada de SENS, no desde la medición
- Dónde: [navegador.c:428,537,566,1012,1147](../../nucleo_c/navegador.c) (NAV_TOF_FRESCO_MS 400).
- Hoy: el reloj de frescura cuenta desde que llega el mensaje al teléfono; una lectura de 300 ms de antigüedad en el firmware parece nueva. Latencia WebSocket + teléfono NO MEDIDA.
- Recomendación: que el firmware mande la edad de la lectura (campo opcional) o medir la latencia de extremo a extremo (protocolo).
- Estado v3: DISEÑO + medición propuesta.

### H-15 — Pines del TC1508A al reiniciar
- Dónde: [docs/seguridad.md:65](../seguridad.md) afirma un estado de pines que contradice la hoja del ESP32 (los GPIO 25/26/27/33 arrancan sin salida ni entrada habilitada, flotantes) — doc 03, confianza **media** (la hoja del TC1508A no se consultó).
- Falla: con pines flotantes el driver puede ver ruido al reiniciar; mitigación: resistencias de pull-down en el hardware.
- Recomendación: corregir la documentación; revisar el cableado (los pull-down son una decisión de hardware tuya). Estado: documentación corregida en la Fase 4.

### H-16 — Constantes de geometría del clasificador inconsistentes; documentación dice YCbCr
- Dónde: `corredor.h` (10 cm de altura, 20° de inclinación, 60° de campo) contra el manual (115 mm, 12°, 56°); altura en centímetros enteros; `obstaculos.c` clasifica con **distancia RGB (suma de valores absolutos) más regla de crominancia normalizada con K = 4 grupos**, no YCbCr con 3 grupos (como dicen el manual §8.2 y el encargo).
- Fuente: lectura de código (alta) y doc 05 (geometría; media).
- Recomendación: unificar las constantes en un solo lugar y medir la cámara real; corregir el manual. Estado: manual corregido en Fase 4; unificación de constantes ABIERTA (cambia distancias del clasificador; no sin medir).

### H-17 — Posible orden de cajas del detector invertido
- Dónde: `app/…/Detector*` (doc 07): la documentación dice `[top, left, right, bottom]` y el código `[ymin, xmin, ymax, xmax]`. NMS con IoU 0.5, máximo 25 cajas, un solo umbral 0.6, y estira 240×176 a 320×320.
- Fuente: doc 07 (TFLite Task Library; confianza **media**; el orden depende del modelo exacto).
- Recomendación: verificar con una imagen **sintética** (no del carro) y una caja conocida. Estado: ABIERTO, verificación propuesta.

### H-18 — Banco: modelo de láser no coincide con el sensor real
- Doc 01: el VL53L0X promedia el cono (~25°); el banco usa «el grupo más cercano». Estado: DISEÑO para v3-etapa de simulador; mientras tanto, cada cifra de patas es SIMULADA.

### H-19 — Sensibilidad no monótona del umbral de retroceso del eco
- Barrido (SIMULADO): 250 mm empeora AYUDA, 400 mm → ayuda 56, 350 mm → 33. Probable ajuste al simulador. Se deja 350 mm con bandera (`eco_retroceder_mm`, 0 apaga) y se anota.

### H-20 — Caja con pared pegada a un lado
- Banco: `caja_y_pared_al_lado` 246 ok, 153 ayudas de 400 (≈38 %). Es el caso en que los dos lados están ocupados (la ayuda es la respuesta de diseño). Pero 38 % de ayudas con 1 contacto es mucho; la mejora real exige medir el alcance del láser a una pared lateral (NO MEDIDO).

---

## Hallazgos de gravedad baja

- **H-21 (tof.c:141)** El par (1200 mm, válida = 1): `crudo == 1200` es «válido» aunque TOF_LIBRE_MM sea 1200; visto en 7 de 107 ticks de las cajas negras del 15-nov. Corregir con `>=`. Estado: ver etapa de firmware.
- **H-22** Velocidad de 19.3 cm/s en AUTO posiblemente obsoleta; las cajas negras del 15-nov sugieren ~30 cm/s. Afecta el modelo de frenado y la conversión tiempo-distancia. NO MEDIDO; medir en el protocolo.
- **H-23** El pivote es asimétrico (derecha R ≈ 15.8 cm; izquierda ≈ 8.7–9 cm); un «espejo» del giro es incorrecto. Ya está en la calibración; no tocar.
- **H-24** Manual: 20 ms de periodo, rango 20..2000 mm, TOF_LIBRE 150: discrepan con el código (TOF_PERIODO_MS 100 ms según el protocolo, TOF_LIBRE 1200). Corregir en Fase 4.
- **H-25** Cajas negras: la rejilla VTEL llega como 32×24 y la documentación dice 24×17. Revisar qué rejilla usa de verdad la app (Fase 4, documentación).
- **H-26** Error lateral final del Monte Carlo heredado: v2 y v3 dan 18/44 mm (media/p95) con RUMBO contra 17/40 de v1.1 (SIMULADO). Causa: el avance paralelo con margen de 230 mm recorre más. Con 180 mm se iguala pero hay 2 contactos nuevos en `objeto_largo90` → **no se aplica**; se documenta como compromiso de seguridad. Diferencia: 1 mm.
- **H-27** `scripts/verificar_version.py` exige el teléfono con `--puerto`; ver etapa de herramientas.
- **H-28** El servidor falso ya anuncia RUMBO=1 y SEG=2 (`probar_servidor_falso.py:122` lo prueba): el pendiente conocido ya estaba resuelto. No requiere cambio.
- **H-29** Giros por tiempo menores de 5° pueden pasarse (un dato aislado B6: 4° dio 18°). **CORREGIDO**: bajo NAV_GIRO_TIEMPO_MIN_GRADOS = 10 sin giroscopio sano no se gira (umbral SUPUESTO).

## Lo que NO revisé a fondo

Kotlin de la app (interfaz y JNI) salvo lo que consume las causas y los textos; el rendimiento de TFLite en el teléfono; el firmware de la cámara más allá de lo ya verificado en `cam-v2`; hojas de datos del TC1508A y normas ISO completas (doc 10); libros y artículos que los documentos marcan como no consultados.
