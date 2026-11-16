# 10 — Seguridad funcional básica: fallo seguro, perros guardianes y distancia de frenado del freno láser

Investigación del 16-nov-2026 (fecha de consulta de todas las fuentes: **2026-11-16**). Rama `feat/autonomo-v3`.
Confianza: **alta** = norma/hoja de datos/manual oficial o documento institucional revisado; **media** = artículo de autor reconocido o respuesta técnica de proveedor; **baja** = inferencia mía (**INFERENCIA**). Lo que no se midió en el carro es **NO MEDIDO** o **SIMULADO**.

**Alcance y advertencia.** IEC 61508, ISO 13849-1, ISO 13855, ISO 3691-4 e IEC 60204-1 se usan **solo como orientación de vocabulario y de método**. Ninguna de esas normas se leyó completa (son de pago); lo que cito de ellas viene de documentos institucionales o de proveedores que sí pude abrir, y lo digo en cada caso. **Este carro no cumple ni pretende cumplir ninguna de ellas.** El paro por láser es una función de apoyo, no una función de seguridad de personas.

**Cómo se leyó:** PDF (ESP32 Series Datasheet v5.3, IFA Report 2/2017e) leídos como texto/imagen; páginas web con `WebFetch` (extracto de un modelo pequeño, confianza media salvo indicación). Datos del proyecto: archivos del repositorio, citados por ruta.

---

## 1. Cómo funciona (patrones de fallo seguro)

**1.1 Vocabulario mínimo.** *Seguridad funcional* es la parte de la seguridad que depende de que una protección automática actúe bien ante sus entradas o **falle de forma predecible** (fail-safe), cubriendo errores sistemáticos, fallos de hardware y estrés ambiental [3]. ISO 13849-1 es la norma, independiente de la tecnología, para las partes de control relacionadas con la seguridad de máquinas, y se considera adecuada para sistemas más simples que los de IEC 61508 [3]. El informe IFA 2/2017e (DGUV, Alemania) pide documentar para cada función de seguridad, entre otros: el estado seguro de la máquina, los **tiempos de respuesta y distancias de sobrerrecorrido** como datos característicos, el comportamiento al perder energía y las condiciones de rearranque [5 §2, §4 de la lista de documentación].

**1.2 Categorías de arquitectura de ISO 13849-1 (según IFA).** B: la categoría básica; la resistencia a fallos viene solo de elegir buenos componentes; un fallo puede anular la función; **PL máximo b**; sin monitoreo. 1: igual pero con componentes «bien probados»; **PL máximo c**. 2: añade pruebas automáticas de la función de seguridad **al arrancar, antes de cada situación peligrosa y/o periódicamente**; un fallo entre pruebas puede anular la función; **PL máximo d**; ante un fallo detectado la salida de prueba debe llevar a un estado seguro (siempre en PLr d) [5 §6.2.3–6.2.5, págs. ~50–52]. IFA aclara que componentes complejos como microprocesadores o ASIC **no se consideran «bien probados» en general** [5 §6.2.4].

**1.3 Patrones concretos usados en robots móviles pequeños** (mapa con lo que ya hace el carro):

| Patrón | Idea | En el carro |
|---|---|---|
| Energizar para andar («hombre muerto» / latido) | Los motores solo se mueven mientras llegan órdenes frescas; el silencio = parada | Watchdog de comunicación 1000 ms (`carro.c:781`), latido de 250 ms (`pagina.h:181`) |
| Estado seguro por defecto | Todas las salidas a «sin energía» ante duda | `poner_motores(0,0,0,0)`; STOP deja las ruedas **libres** (sin freno activo del TC1508A: `docs/reporte_J16.md:23`) |
| Perro guardián de hardware | Reinicia el procesador si el lazo principal deja de correr | `esp_task_wdt` 5000 ms, `trigger_panic = true` (`firmware_carro.ino:287-295`) |
| Reflejo local independiente del enlace | La parada ante obstáculo no pasa por la red ni el teléfono | `__STOP_AUTO` desde `sensores.c:revisar_paro_tof` en el mismo `loop()` |
| Parada que no se pierde | El STOP no puede ser expulsado de la cola | `es_stop_prioritario` en `carro_encolar_comando` (`carro.c:511-575`) |
| Rearme controlado + histéresis | Tras frenar no se reanuda solo ni oscila en el borde | `TOF_BLOQUEO_MM` 150 > `TOF_PARO_MM` 120 (`config.h`); el app deja de repetir FORWARD tras `TOF_PARO` |
| Perfil seguro tras perder el enlace | Al reconectar o tras el watchdog vuelve a la velocidad baja | `cambiar_perfil`, `carro_poner_perfil_auto` (`carro.c:720,787`) |

**1.4 Modelo de distancia de seguridad (ISO 13855 como plantilla).** Para dispositivos de protección sin contacto, la distancia mínima es **S = K × T + C**: K = velocidad de aproximación (1600 mm/s caminando, 2000 mm/s mano), T = tiempo total de parada = tiempo de respuesta del dispositivo + tiempo máximo de parada de la máquina, C = distancia adicional de intrusión (850 mm en el ejemplo de cuerpo completo); si con K = 2000 sale ≥ 500 mm se puede recalcular con 1600 mm/s pero con S ≥ 500 mm [1]. Para vehículos sin conductor, un artículo de proveedor describe el mismo esquema: **longitud del campo = distancia de reacción + distancia de frenado + márgenes**; ejemplo ilustrativo a 1,5 m/s: 0,375 m + 0,65 m + 0,25 m = 1,275 m [2]; la norma ISO 3691-4 (edición 2023 según ese artículo) limita a 0,3 m/s cuando la detección está inhibida y pide que cada función de seguridad alcance el PL asignado, a menudo PL d [2]. **Aplicación a este carro (mi analogía, INFERENCIA):** la «máquina» es el carro, K es su propia velocidad (el obstáculo está quieto), T es el retardo del sensor + del firmware, y «C» es el sesgo del sensor más la distancia sensor–frente; el cálculo de §2.5 sigue exactamente esa estructura.

**1.5 Perros guardianes: reglas reconocidas.**
- Atender el perro guardián desde el camino principal del código, **no** desde una interrupción ni desde una tarea baja que siga corriendo cuando el programa principal ya falló; en sistemas con varias tareas, cada tarea marca su avance y solo una verificación conjunta «patea» al perro [4]. Todo reinicio visto en pruebas debe investigarse [6]. El timeout debe estar cerca del periodo de la tarea más lenta más un pequeño margen y ser lo más corto posible [6].
- Un perro guardián solo vigila **vivacidad y oportunidad**, no corrección de resultados, y no sustituye el análisis de tiempos [6].
- Si el procesador se colgó, el reinicio puede no bastar; el equipo peligroso debe quedar en estado seguro (deshabilitado, aparcado) y las salidas deben volver a un estado conocido; los perros externos no dependen del software [4].
- En ESP-IDF, el TWDT por defecto **solo avisa** (mensaje y traza) salvo que se active el pánico/reinicio (`CONFIG_ESP_TASK_WDT_PANIC` o, en la API, `trigger_panic`) [7]. El firmware del carro sí lo pone en `true`.

## 2. Parámetros y límites relevantes

### 2.1 Datos medidos o fijados en el proyecto

| Dato | Valor | Origen | Estado |
|---|---|---|---|
| Velocidad PERFIL_MANUAL (PWM 255, 252 izq.) | **38 cm/s** (26,3 ms/cm) | `docs/calibracion.md` (13-oct), `docs/latencia.md` | MEDIDO (cinta + cronómetro del usuario; arranque ±50 ms, ±1 cm) |
| Velocidad PERFIL_AUTO (210/207) | **19,3 cm/s** (51,7 ms/cm) | `docs/calibracion.md` (14-oct) | MEDIDO |
| «Frenado» tras STOP | **0,9 cm** a 255; **1,4 cm** a 210 | `docs/calibracion.md`, `docs/latencia.md` | MEDIDO **con incertidumbre alta**: sale como residuo de un ajuste lineal, el propio documento dice que un error de 100 ms en el arranque mueve el frenado ≈ 2 cm; que sea **menor a 255 que a 210** contradice la física (más velocidad, más recorrido) y huele a ruido |
| Ida y vuelta PING/PONG por WiFi | **30 ms** (STOP ≈ 15 ms) | `docs/latencia.md` etapa 4 | MEDIDO, una sola serie; **no hay percentiles (p95/p99)** |
| Latido de la app / de la página | 250 ms | `pagina.h:181`; `ControlLogica` (manual cap. 2) | MEDIDO en código; jitter de Android **NO MEDIDO** |
| Watchdog de comunicación | 1000 ms (`TIEMPO_WATCHDOG_MS`) | `config.h:140`, `carro.c:781` | código |
| Watchdog de hardware | 5000 ms (`WATCHDOG_HW_MS`) | `config.h:143` | código |
| Láser | `TOF_PERIODO_MS` 33, `TOF_SONDEO_MS` 4, `TOF_PARO_MM` 120, `TOF_PARO_LECTURAS` 2, `TOF_BLOQUEO_MM` 150 | `config.h:247-255` | código; **tasa real de lecturas NO MEDIDA** |
| `LOOP_MAX_US` | 5000 µs (umbral de diseño) | `config.h:269` | **No se cumple en las medidas del proyecto:** `LOOP?` dio máximos de 3,3–12,9 ms con la app conectada (`docs/validacion_sensores.md`; 8,0–12,9 ms en reposo con app, 3,3–6,8 ms en las pruebas con giros), 1,7–1,8 ms sin cliente (`docs/sensores.md`) y la tabla del manual (`12_pruebas.md`) cita 28,4–41,1 ms; **ese formato de tres campos (`694,41111,116`) no coincide con la respuesta `LOOP:<máx>,<excesos>` de `sensores.c:402`, así que la procedencia de los 41 ms es dudosa** |
| Promedio de `loop()` | ≈ 0,69 ms | `12_pruebas.md` (mismas tres corridas) | MEDIDO (dudoso por lo anterior) |
| Cola de comandos | 32 entradas de 32 B (`COLA_COMANDOS_MAX`, `LARGO_MAX_COMANDO`) | `config.h:153-154` | código |
| Geometría | sensor a 120 mm **del centro** (`MV_LASER_ADELANTE_MM`), chasis 200 × 240 mm, sensor a 55 mm de altura, «borde delantero» | `nucleo_c/mundo_v2.h:36-37`; `docs/auditoria_autonomo_v2.md:105`; manual `03_hardware.md` | **SUPUESTO/NO MEDIDO**; el 120 mm es sensor→centro de giro, **no** sensor→parachoques |
| Distancia de paro con cinta del freno láser aislado | — | `12_pruebas.md` fila «Freno autónomo aislado por láser» | **NO MEDIDO** (lo dice el propio manual) |

### 2.2 Datos de las fuentes externas que condicionan el cálculo
- Presupuesto del láser 33 ms por defecto, medida de rango ≈ 23 ms dentro de ese presupuesto; desviación estándar 4–7 % (33 ms, interior) y offset típico ±10 mm / máximo ±15 mm (voltaje) o ±30 mm (temperatura) [8 = DS11555 Tablas 13 y 15; detalle en `01_vl53l0x.md`].
- ESP32 (datasheet v5.3, Apéndice A.4 IO_MUX, pág. 69): para GPIO25, GPIO26, GPIO27 y GPIO33 (los cuatro pines de motor) la columna «At Reset» y «After Reset» dice **`oe=0, ie=0`** (salida y entrada deshabilitadas, sin pull-up ni pull-down), y el texto añade que **durante el reset todos los pines tienen la salida deshabilitada** [9].

### 2.3 Cadena de tiempos del freno láser (cálculo propio)
Notación: T = 33 ms (periodo de medida), N = 2 (`TOF_PARO_LECTURAS`), p ∈ [0, 4] ms (retraso de sondeo, `TOF_SONDEO_MS`), a ∈ [0, T] = **edad efectiva** de una medida (la medida refleja la distancia en algún instante de su ventana, no en el instante en que se lee), φ ∈ [0, 1) = fase en que el objeto cruza el umbral respecto al reloj del sensor.

- Una medida k abarca una ventana de ≈ T y queda disponible al final; las lecturas son consecutivas (modo continuo, sin pausa) [8 §3.4].
- Sea t₀ el instante en que el objeto cruza `TOF_PARO_MM`. La primera lectura cuyo instante de muestreo cae después de t₀ termina entre a y a + T después de t₀; la segunda lectura confirmatoria termina T después. Por tanto
  **t_sense = a + (N − 1)·T + φ·T + p**
  (límite inferior con a = 0, φ = 0, p = 0: **33 ms**; superior con a = T, φ → 1, p = 4: **103 ms**; central a = T/2, φ = 0,5, p = 2: **68 ms**).
- `sensores_actualizar()` → `revisar_paro_tof()` encola `__STOP_AUTO` y **en la misma vuelta de `loop()`** `carro_procesar_cola()` la ejecuta y pone los PWM en 0 (`firmware_carro.ino:561-563`). Retraso de firmware **t_lazo** = duración de esa vuelta: ≈ 0,7–1 ms típico; 5 ms = límite de diseño; 12,9 ms = máximo visto con la app conectada en `docs/validacion_sensores.md`; 41,1 ms = cifra dudosa del manual.
- La cola no suma latencia por profundidad: `carro_procesar_cola()` **vacía toda la cola en cada vuelta**; la profundidad máxima 32 ejecuta cada comando en microsegundos. El STOP se ejecuta **en orden FIFO** (no adelanta a los comandos ya encolados): «STOP prioritario» significa que **no puede ser expulsado** cuando la cola se llena (primero se descartan PING, luego el comando viejo que no sea STOP; `carro.c:540-570`). Un FORWARD encolado *después* del STOP en la misma vuelta volvería a arrancar el carro; por eso la app debe dejar de repetir FORWARD (ya hecho: `docs/reporte_J16.md`).
- **Distancia total:** **D = v·(t_sense + t_lazo) + d_fren**, con d_fren = distancia que sigue recorriendo el carro tras poner PWM = 0 (incluye la inercia de motores y ruedas; el carro queda en «rueda libre»: `docs/reporte_J16.md:23`). Ojo: el «frenado» de la calibración se midió enviando STOP por WiFi, así que incluye ≈ 15 ms de red (≈ 0,3 cm a 19,3 cm/s) que el freno local no tiene → usarlo es ligeramente conservador (**INFERENCIA**).
- **Holgura = (TOF_PARO_MM/10) − sesgo − D − e**, con **e** = distancia del sensor al punto más delantero del carro (positiva si el sensor queda «metido»; **NO MEDIDA**: la rueda delantera o un borde del chasis puede sobresalir del sensor) y **sesgo** = cuánto de más «ve de lejos» el sensor en la dirección peligrosa (offset ±10 mm típico, ±30 mm máximo con temperatura [8]).
- **Umbral mínimo:** **TOF_PARO_MM ≥ 10·[ v·(t_sense,máx + t_lazo,máx) + d_fren,máx + sesgo,máx + e + m ]** con m ≈ 1 cm de margen (todo en cm y s).

### 2.4 Resultados (script `docs/investigacion_autonomo/calculos/frenado.py`, solo biblioteca estándar; reproduce todas las tablas)

**Tabla A. D = distancia desde que el objeto cruza 120 mm hasta que el carro queda quieto (cm), con el frenado MEDIDO del proyecto.**

| Perfil | Optimista (33 + 0,7 ms) | Central (68 + 1 ms) | Conservador (103 + 5 ms) | Peor medido (103 + 41 ms) |
|---|---:|---:|---:|---:|
| AUTO 19,3 cm/s, frenado 1,4 cm | 2,05 | 2,73 | 3,48 | 4,18 |
| MANUAL 38 cm/s, frenado 0,9 cm | 2,18 | 3,52 | 5,00 | 6,38 |
| MANUAL con frenado escalado por v² desde AUTO = 5,43 cm (**INFERENCIA**, cota alta: a = v²/2d = 133 cm/s²) | — | 8,05 | 9,53 | 10,90 |

(Con la cifra más creíble de `loop()` 12,9 ms en vez de 41 ms, AUTO conservador sale 3,6 cm.)

**Tabla B. Holgura con TOF_PARO_MM = 120 mm (cm; negativo = choca).** Combinaciones sesgo/e: [1/0], [1/2], [1/4], [3/0], [3/2], [3/4] cm.

| Caso | [1/0] | [1/2] | [1/4] | [3/0] | [3/2] | [3/4] |
|---|---:|---:|---:|---:|---:|---:|
| AUTO central | 8,3 | 6,3 | 4,3 | 6,3 | 4,3 | 2,3 |
| AUTO conservador | 7,5 | 5,5 | 3,5 | 5,5 | 3,5 | 1,5 |
| AUTO peor medido | 6,8 | 4,8 | 2,8 | 4,8 | 2,8 | **0,8** |
| MANUAL central, frenado medido | 7,5 | 5,5 | 3,5 | 5,5 | 3,5 | 1,5 |
| MANUAL conservador, frenado medido | 6,0 | 4,0 | 2,0 | 4,0 | 2,0 | 0,0 |
| MANUAL central, frenado v² | 3,0 | 1,0 | **−1,0** | 1,0 | **−1,0** | **−3,0** |
| MANUAL conservador, frenado v² | 1,5 | **−0,5** | **−2,5** | **−0,5** | **−2,5** | **−4,5** |
| MANUAL peor medido, frenado v² | 0,1 | −1,9 | −3,9 | −1,9 | −3,9 | −5,9 |

**Tabla C. TOF_PARO_MM mínimo (mm) con margen de 1 cm:** P = 10·(D + sesgo + e + 1).

| Perfil / escenario | sesgo 1 cm: e = 0 / 2 / 4 | sesgo 3 cm: e = 0 / 2 / 4 |
|---|---|---|
| AUTO central | 47 / 67 / 87 | 67 / 87 / 107 |
| AUTO conservador | 55 / 75 / 95 | 75 / 95 / 115 |
| AUTO peor medido | 62 / 82 / 102 | 82 / 102 / **122** |
| MANUAL conservador (frenado v²) | 115 / 135 / 155 | 135 / 155 / 175 |
| MANUAL peor medido (frenado v²) | 129 / 149 / 169 | 149 / 169 / 189 |

**Tabla D. Monte Carlo (SIMULADO, 200 000 corridas, semilla 20261116; distribuciones SUPUESTAS descritas en el script; e = 0).** D en cm: AUTO p50 = 2,75, p95 = 4,46, p99 = 5,17, máx = 7,79; holgura p1 = 6,2, P(<0) = 0. MANUAL con frenado medido: p50 = 3,58, p99 = 6,18, holgura p1 = 5,2, P(<0) = 0. MANUAL con frenado v²: p50 = 8,09, p99 = 10,71, holgura p1 = 0,64, **P(<0) = 0,25 %**. El resultado depende por completo de las distribuciones elegidas; sirve para ver el orden de magnitud, no como medida.

### 2.5 Casos que empeoran el cálculo
**a) Pulso de arranque a plena potencia en AUTO (INFERENCIA, cota).** Todo avance desde reposo en AUTO empieza con **250 ms a PWM 255** (`PERFIL_AUTO_ARRANQUE_PLENO_MS`) antes de bajar a 210/207; el arranque medido es 0 ms (el carro responde enseguida). Si el objeto se ve por primera vez durante ese pulso, la velocidad puede ser hasta 38 cm/s y el frenado mayor. Con v = 38 cm/s y frenado escalado por v²: D = **8,05 / 9,53 / 10,90 cm** (central / conservador / peor), holgura con sesgo 1 y e = 0 de **3,0 / 1,5 / 0,1 cm**; con e ≥ 2 cm o sesgo 3 cm **no hay holgura**. La distancia máxima recorrida en el pulso es 38 × 0,25 = 9,5 cm. `TOF_BLOQUEO_MM` (150) impide arrancar solo si ya hay un objeto confirmado a < 150 mm; un objeto a 150–250 mm sí permite el arranque. Esto ocurre en cada reintento de avance del navegador (**NO MEDIDO** en el carro).

**b) Superficies de baja reflectancia («sin eco»).** Las lecturas inválidas (8190, 65535) cuentan como «libre» y **ponen a cero la racha de lecturas cercanas** (`tof.c procesar`). Si cada lectura es válida y < 120 mm con probabilidad p (independientes, **SUPUESTO**), las lecturas esperadas hasta tener 2 seguidas son (1 + p)/p²:

| p | lecturas esperadas | retraso extra (ms) | extra AUTO (cm) | extra MANUAL (cm) |
|---|---:|---:|---:|---:|
| 1,0 | 2,00 | 0 | 0 | 0 |
| 0,94 (mínimo garantizado de detección con blanco 88 %/gris 17 % en los rangos de la tabla 12) | 2,20 | 6,5 | 0,12 | 0,25 |
| 0,9 | 2,35 | 11 | 0,22 | 0,43 |
| 0,7 | 3,47 | 48 | 0,94 | 1,84 |
| 0,5 | 6,00 | 132 | 2,55 | 5,02 |
| 0,3 | 14,44 | 411 | 7,93 | 15,61 |

Con p fija < 0,5 el carro puede llegar al objeto sin frenar; en la realidad p sube al acercarse (la señal crece ~1/d²: **INFERENCIA**), por lo que la tabla es una cota. **La hoja de datos no garantiza nada para blancos más oscuros que gris 17 % ni para superficies especulares ni transparentes**; ST dice que el negro mate es ≈ 5 % reflectante y un vidrio especular leyó la mitad de la distancia (hacia el lado seguro) (fuente 10 de `01_vl53l0x.md`, empleado de ST en foro oficial, confianza media-alta). Por eso **el freno láser no puede presentarse como cobertura de «cualquier objeto»**; es una capa más, sin garantía de detección.

**c) Sensor caído = sin protección (hecho de código).** Con `tof_vivo() == 0`, `tof_cerca_confirmada()` devuelve 0 y el carro **sigue avanzando como antes** (`docs/sensores.md`, pruebas de PC). Es lo contrario de fallo seguro para esta función (aviso en `CAPS`/`SENS`, sin freno).

**d) Persona o cosa que se acerca.** Si el obstáculo también se mueve hacia el carro, la distancia relativa recorrida en T_total crece: con K = 1600 mm/s de ISO 13855 [1] y T_total = 0,108 s (conservador) serían **17 cm** adicionales. No es el caso de las pruebas del proyecto (cajas quietas).

### 2.6 Pérdida de enlace y reinicio por hardware
**Watchdog de comunicación (1000 ms) frente al latido (250 ms).** Recorrido máximo tras perder el enlace = v·1,000 s + d_fren (el último comando llega justo antes del corte): **AUTO 19,3 + 1,4 = 20,7 cm; MANUAL 38,0 + 0,9 = 38,9 cm** (más hasta 0,8 / 1,6 cm si `loop()` tarda 41 ms). Relación 1000/250 = 4: toleran perderse 3 latidos seguidos; un solo intervalo de hasta 750 ms entre latidos no frena (margen frente al jitter de WiFi/Android: **NO MEDIDO**; la única cifra medida es 30 ms de RTT medio). Koopman recomienda timeout cercano al periodo de la tarea más lenta más un margen pequeño [6]: 4× es razonable, pero **no hay datos de la cola larga del retardo** para justificarlo.
- Los `PING` **no** renuevan el watchdog (`carro.c:664-665`); sí lo renuevan `FORWARD/REVERSE/LEFT/RIGHT`, `STOP`, `EBRAKE`, `MODO_*`, mensajes de balance, `BAL?` y `GIRAR:` (`renueva_watchdog`, `carro.c:236-246`). El comentario de `pagina.h:144` («ping cada 800 ms para mantener el watchdog») es **incorrecto** respecto del código (los PING solo mantienen vivo el socket).
- El watchdog no actúa durante un `GIRAR` por ángulo (tiene su propio tiempo máximo, 500 ms + 15 ms/°: 3,5 s a 200°; `carro.c:780`).
- Una desconexión explícita del WebSocket dispara `__STOP_AUTO` de inmediato (`firmware_carro.ino:386-395`); si el WiFi cae sin cierre de TCP, solo queda el watchdog de 1 s.

**Watchdog de hardware (5000 ms).** Cubre un `loop()` colgado, pero:
1. El watchdog de 1 s corre **dentro** de `loop()`: si `loop()` se cuelga, no existe protección por 5 s. Si los PWM (periférico LEDC) siguen generando con el último ciclo de trabajo mientras la CPU está colgada (**INFERENCIA**: es lo habitual en periféricos de PWM; no lo comprobé en la documentación de ESP-IDF), el carro seguiría avanzando **5 s = 96 cm en AUTO o 190 cm en MANUAL** antes del reinicio.
2. Al reiniciar, los pines 25/26/27/33 quedan **sin salida ni entrada habilitadas y sin pull** (`oe=0, ie=0`) hasta que `setup()` ejecute `hw_iniciar_pwm()` [9]. Si el módulo TC1508A no tiene pull-down propias en sus entradas (**NO CONSULTADO**: no encontré su hoja de datos), las entradas **flotan** y el comportamiento de los motores en ese lapso es indefinido. `docs/seguridad.md:65` afirma que el reinicio lleva los pines de motor «a nivel bajo por hardware»: **eso contradice el datasheet del ESP32 salvo que haya resistencias externas**; no las menciona ningún documento del proyecto que leí.

## 3. Errores típicos y modos de falla conocidos

1. **Perro guardián pateado desde el sitio equivocado** (ISR, tarea baja): da falsa tranquilidad [4, 6]. En el carro se patea al inicio de `loop()` (`esp_task_wdt_reset()`): vigila que `loop()` gire, no que las funciones de sensores y comandos hagan su trabajo; si `sensores_actualizar()` fallara siempre, el perro seguiría contento (modo de falla de «liveness ≠ corrección» [6]).
2. **Reinicio sin estado seguro de las salidas** (ver §2.6 punto 2) [4].
3. **Parada = quitar PWM = rueda libre.** Sin freno activo, la distancia de frenado crece con la inercia y con la pendiente/piso resbaloso (**INFERENCIA**); el proyecto decidió no cambiarlo para no invalidar calibraciones (`docs/reporte_J16.md:23`).
4. **Tiempo de respuesta subestimado.** El manual dice que el paro láser actúa «en menos de un ciclo de sensores (~20 ms)» (`02_arquitectura.md:51`, `11_operacion.md:48`): el cálculo de §2.3 da **33–103 ms hasta confirmar, más el recorrido**. Y dice «`< 20 ms` locales», que solo describe la parte de firmware.
5. **Confirmación que no confirma.** `TOF_PARO_LECTURAS = 2` con un periodo de 33 ms exige que el objeto esté a < 120 mm en **dos lecturas seguidas**; un pico fuera de rango o un «sin eco» entre ambas reinicia la cuenta (§2.5 b).
6. **Colas.** Cola llena: se expulsan PING y comandos viejos; si todo fueran STOP se expulsaría el más viejo (`victima = 0`). No ocurre con tráfico real (la cola se vacía en cada vuelta), pero sí con un `loop()` bloqueado (>~8 s con 32 comandos de 250 ms) — que el watchdog de hardware ya habría reiniciado.
7. **Dependencia de un solo sensor (categoría B).** Una sola cadena sin pruebas: si el láser da un valor erróneo constante (por ejemplo siempre 1200) no hay detección cruzada (la cámara es otra capa, pero comparte la decisión en el teléfono).
8. **Valores de documentación inconsistentes** (riesgo de decisión equivocada): ver `01_vl53l0x.md` §6.1, y `docs/manual/capitulos/04_firmware.md:94` que dice que el paro usa «la distancia filtrada» mientras `tof.c` cuenta la racha con lecturas **crudas** (la mediana solo alimenta `tof_mm()` y `SENS`).

## 4. Buenas prácticas reconocidas

- Documentar para cada función de seguridad: estado seguro, tiempo de respuesta y distancia de sobrerrecorrido, comportamiento al perder energía, condiciones de rearranque [5].
- Medir T en la máquina real y no estimarlo [1] (el cálculo de este documento es **estimación**; hay que sustituirlo por medidas).
- Estructura distancia = reacción + frenado + márgenes [1, 2] y ajustar el campo/umbral a la velocidad.
- Un perro guardián externo o independiente del software, que ponga las salidas en estado seguro [4]; patearlo solo tras comprobar que todas las partes avanzan [4, 6].
- Pruebas automáticas de la función de seguridad al arrancar y antes de iniciar un movimiento peligroso (Categoría 2) [5].
- No depender de componentes complejos como «bien probados» [5].

## 5. Fuentes

1. Keyence, **Safety Knowledge – «Light Curtain Installation and Safety Distance (Minimum Distance)»** https://www.keyence.com/ss/products/safetyknowledge/caution/ (fórmula S = K·T + C de ISO 13855, K = 1600/2000 mm/s, C = 850 mm, regla de 500 mm; T = parada de máquina + respuesta del dispositivo). Leída 2026-11-16 vía `WebFetch`. **Media** (proveedor; **no** es el texto de la norma; ISO 13855 completa **NO CONSULTADA**).
2. «ISO 3691-4 driverless industrial trucks», blog de Fabrico (sin autor visible; publicado 09-jul-2026, actualizado 14-nov-2026; edición 2023 de la norma) https://www.fabrico.io/blog/iso-3691-4-driverless-industrial-trucks/ (campo = reacción + frenado + márgenes; ejemplo ilustrativo; 0,3 m/s con detección inhibida; PL d). Leída 2026-11-16 vía `WebFetch`. **Media-baja** (blog comercial; ISO 3691-4 **NO CONSULTADA**; intenté la muestra de iTeh https://cdn.standards.iteh.ai/samples/70660/7c26a1bc79ae4d948f9b4752eb44433c/ISO-3691-4-2020.pdf pero el texto no era legible con `WebFetch` y no lo procesé más).
3. Wikipedia, **«Functional safety»** https://en.wikipedia.org/wiki/Functional_safety (definición; IEC 61508 base de normas sectoriales; ISO 13849 independiente de la tecnología). Leída 2026-11-16 vía `WebFetch`. **Media** (IEC 61508 **NO CONSULTADA**).
4. Jack Ganssle (contribuido por Convergence Promotions), **«A Designer's Guide to Watchdog Timers»**, DigiKey, 2-may-2012 https://www.digikey.com/en/articles/a-designers-guide-to-watchdog-timers (perros internos/externos/de ventana; no patear desde ISR/RTOS; estado seguro de salidas; reset duro). Leída 2026-11-16 vía `WebFetch` (los últimos 10 721 de 110 721 caracteres no se leyeron). **Media-alta** (autor reconocido). El artículo del propio sitio de Ganssle (`ganssle.com/item/great-watchdog-timers.htm`) **no se pudo leer** (redirige a «missing»).
5. IFA/DGUV, **IFA Report 2/2017e «Functional safety of machine controls – Application of EN ISO 13849»**, julio de 2019, https://publikationen.dguv.de/widgets/pdf/download/article/4150 (categorías B, 1, 2, 3; PL máximos; pruebas de la Cat. 2; documentación de funciones de seguridad). PDF descargado y leído como texto (14 388 líneas; cité §6.2.3–6.2.5 y la lista de documentación). Consultado 2026-11-16. **Alta** (informe institucional sobre ISO 13849-1:2015; la norma en sí **NO CONSULTADA**).
6. Philip Koopman, **«Proper Watchdog Timer Use»**, *Better Embedded System Software*, 26-may-2014 https://betterembsw.blogspot.com/2014/05/proper-watchdog-timer-use.html. Leída 2026-11-16 vía `WebFetch`. **Media-alta**.
7. Espressif, **ESP-IDF Programming Guide – Watchdogs (TWDT, IWDT)** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html (versión «latest» al 2026-11-16; el TWDT por defecto solo avisa; con pánico habilitado reinicia). Leída vía `WebFetch`. **Alta** (oficial; no verifiqué la versión exacta de ESP-IDF del core Arduino usado).
8. STMicroelectronics, **VL53L0X Datasheet DS11555 Rev 6** y **UM2039 Rev 1** (ver `01_vl53l0x.md`, fuentes 1 y 2). **Alta**.
9. Espressif, **ESP32 Series Datasheet, versión 5.3**, Apéndice A.4 «IO_MUX» (pág. 69) y texto de pie del apéndice («During reset, all pins are output-disabled»), https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf (descargado con `curl`, leído como texto y como imagen de página). Consultado 2026-11-16. **Alta**. Nota: la revisión del chip del DevKit usado no se verificó.
10. Datos del proyecto (repositorio, rama `feat/autonomo-v3`): `firmware_carro/config.h`, `carro.c`, `sensores.c`, `tof.c`, `firmware_carro.ino`, `pagina.h`; `docs/seguridad.md`, `docs/latencia.md`, `docs/latencia_modos.md`, `docs/perfiles_velocidad.md`, `docs/calibracion.md`, `docs/sensores.md`, `docs/validacion_sensores.md`, `docs/reporte_J16.md`, `docs/limites_conocidos.md`, `docs/auditoria_autonomo_v2.md`, `docs/manual/capitulos/02_arquitectura.md`, `03_hardware.md`, `04_firmware.md`, `11_operacion.md`, `12_pruebas.md`; `nucleo_c/mundo_v2.h`. Leídos 2026-11-16. Confianza: **alta** para lo que dice el código; la calidad de las **medidas** está en la tabla §2.1.

**NO CONSULTADAS / no encontradas:** ISO 13849-1, ISO 13855, ISO 3691-4, ISO 13850, IEC 61508 e IEC 60204-1 (de pago; solo secundarias, arriba). Las **categorías de parada 0/1/2 de IEC 60204-1**: intenté leer páginas de Kollmorgen y TÜV SÜD (HTTP 403) y no las pude abrir; **no las cito**: la equivalencia «quitar PWM = parada sin control» es solo mi analogía (INFERENCIA). **Hoja de datos del TC1508A**: no la encontré (la búsqueda solo devolvió un extracto en chino que no abrí) → se desconoce si sus entradas tienen pull-down internas y cómo se comporta con IN1 = IN2 = 0. **Documentación de ESP-IDF sobre el comportamiento del LEDC con la CPU detenida:** no consultada. **Latencias de cola larga del WebSocket** y la **tasa real del láser:** no medidas.

## 6. Qué implica para este carro

**6.1 Tabla: valor usado vs. lo que dice la fuente.**

| Concepto | Proyecto | Fuente / cálculo | Veredicto |
|---|---|---|---|
| `TOF_PARO_MM` | 120 mm | §2.3: P ≥ 10·[v·(t_sense,máx + t_lazo,máx) + d_fren,máx + sesgo,máx + e + m] | **AUTO estacionario (19,3 cm/s): suficiente** en todas las combinaciones del modelo (holgura 0,8–8,3 cm), pero con **margen < 2 cm** en las combinaciones extremas (e ≥ 4 cm con sesgo 3 cm); **AUTO en el pulso de arranque: insuficiente** si e ≥ 2 cm (holgura 0,1–3,0 cm con e = 0); **MANUAL: no aplica** (el freno láser no actúa) y de aplicarse **120 mm no alcanzaría** con el frenado escalado por v² |
| `TOF_PARO_LECTURAS` | 2 | confirmación en ≥ 33–66 ms; con p ≈ 0,94 de lecturas válidas [8] cuesta +6,5 ms | aceptable; el costo real es la brecha de «sin eco» (§2.5 b) |
| Edad de la medida | no considerada («~20 ms» en el manual) | §2.3: 33–103 ms hasta confirmar | el manual debe corregirse |
| `TIEMPO_WATCHDOG_MS` / latido | 1000 / 250 ms | [6] timeout ≈ periodo + margen | razonable; recorrido máx. 20,7 cm AUTO, 38,9 cm MANUAL |
| `WATCHDOG_HW_MS` | 5000 ms | [6] «lo más corto posible»; ver §2.6 | largo: 96 cm AUTO / 190 cm MANUAL si los PWM siguen con la CPU colgada (INFERENCIA) |
| Estado de pines al reiniciar | «a nivel bajo por hardware» (`seguridad.md:65`) | [9] `oe=0, ie=0`, sin pull | **contradicción**, salvo resistencias externas no documentadas |
| `LOOP_MAX_US` | 5000 µs | medidas hasta 12,9 ms con app (y 41 ms dudoso) | el umbral de diseño no se cumple hoy; cada 10 ms de `loop()` suman 0,19 cm (AUTO) / 0,38 cm (MANUAL) |
| `COLA_COMANDOS_MAX` | 32 | cálculo §2.3 | suficiente; STOP no se pierde, pero no adelanta (FIFO) |

**6.2 ¿Es suficiente 120 mm? Conclusión y valor más conservador.**
- **AUTO a velocidad de crucero (19,3 cm/s): sí, con margen positivo en todo el modelo**, D = 2,1–4,2 cm (frenado medido) y holgura 0,8–8,3 cm según sesgo y e. El resultado depende de dos números **no medidos**: e (distancia sensor→punto más delantero) y el frenado real (±2 cm de incertidumbre propia de la calibración): con +2 cm de frenado la combinación extrema (sesgo 3, e 4, peor `loop()`) pasa a **−1,2 cm**.
- **AUTO al arrancar con el pulso de 250 ms (cota, INFERENCIA): no es suficiente de forma robusta** (holgura 0,1–3 cm con e = 0 y sesgo 1; negativa con e ≥ 2 cm o sesgo 3 cm).
- **MANUAL:** el firmware no frena por láser (`sensores.c:204`); `docs/perfiles_velocidad.md` ya dice que el operador debe frenar él. Si algún día se activara, 120 mm exigiría **≈ 115–190 mm** (Tabla C, frenado v²).
- **Valor más conservador propuesto para `TOF_PARO_MM` en AUTO:** usar la fórmula de §2.3 con los extremos medidos/supuestos: **P = 10·[19,3·(0,103 + 0,005) + 3,4 + 3,0 + e + 1,0] = 95 + 10·e mm** para régimen de crucero (e en cm), y para el pulso de arranque **P = 10·[9,5 + 1 a 3 + e + 1] = 115–135 + 10·e mm**. Con e entre 0 y 4 cm el rango es **95–175 mm**; un valor único razonable es **150 mm**, que cubre el pulso con e ≤ 2,5–4,5 cm según el sesgo, y obliga a subir `TOF_BLOQUEO_MM` a ≥ 180 mm para conservar la histéresis de 30 mm. **Es una recomendación de modelo**, no un resultado medido; antes de cambiar hay que medir e y el frenado (6.3) y revisar el navegador, que usa 120 mm como supuesto en varios textos (`07_navegacion.md`, `13_limites.md`). Alternativa sin tocar el umbral: reducir `PERFIL_AUTO_ARRANQUE_PLENO_MS` o limitar la potencia del pulso en AUTO (**INFERENCIA**; afecta las calibraciones de arranque).
- **Incertidumbre del propio cálculo:** ±2 cm en d_fren, ±1,5 cm en e (sin medir), sesgo +1 a +3 cm y ±35 % en t_sense según a y φ: en conjunto el D real del régimen de crucero puede caer entre ≈ 2 y ≈ 6 cm y el del pulso entre ≈ 8 y ≈ 13 cm.

**6.3 Recomendaciones concretas y verificables** (todas **NO MEDIDAS** hoy):
1. **Medir e** con regla (sensor → punto más delantero: rueda, chasis, cámara) y anotarlo en `docs/`. Criterio: e ≤ 2 cm mantiene 120 mm válido en crucero.
2. **Medir la distancia de parada real del freno láser aislado** (la fila NO MEDIDO de `12_pruebas.md`): carro en AUTO contra una pared blanca mate a 40 cm, avanzar 10 veces, registrar con cinta la distancia final al frente (no al centro) y `TOF_PARO:<mm>`; repetirlo (a) desde parado (con pulso) y (b) con el carro ya en crucero. Aceptación sugerida: distancia final ≥ 15 mm en las 10 corridas. Repetir con cartulina negra mate y plástico negro brillante (§2.5 b).
3. **Medir la tasa real de lecturas y el tiempo del lazo con el carro en marcha:** `tof_lecturas()` durante 10 s y `LOOP?` tras `LOOP_CERO` con motores encendidos y app conectada; aclarar el formato de `LOOP:` frente a la tabla del manual (la cifra de 41 ms).
4. **Fallo seguro del propio sensor (INFERENCIA de diseño):** en AUTO y con `TOF_USO` = 1, rechazar `FORWARD` (o limitar la velocidad) si `tof_vivo() == 0` o si no hubo lectura en los últimos ~100 ms (3 periodos); responder con un NAK explícito. Verificar en `pruebas_firmware` con el sensor falso que el carro no avanza con el láser caído y que el aviso llega a la app.
5. **Pines del driver al reiniciar:** con el carro en soportes, mantener `EN` del DevKit a GND (reset) y medir con multímetro la tensión en IN1–IN4 del TC1508A; si no son ≈ 0 V, añadir resistencias pull-down de ≈ 10 kΩ en los cuatro pines. Además revisar la hoja del TC1508A (**NO CONSULTADA**).
6. **Acortar `WATCHDOG_HW_MS`** a 1500–2000 ms (INFERENCIA): con la peor vuelta vista (12,9 ms, o 41 ms dudoso) sigue habiendo > 30× de margen; verificar que ninguna operación bloqueante (escritura en memoria no volátil, `init()` del láser) pase de 500 ms y que no haya reinicios espurios en una corrida de 10 min.
7. **Medir la cola larga del WebSocket:** registrar el intervalo entre mensajes `FORWARD` recibidos en el carro (percentiles p50/p95/p99/máx.) durante una ruta de 2 min con el teléfono en pantalla apagada y encendida. Criterio: máx. < 750 ms; si no, el watchdog de 1 s causará paros espurios.
8. **Corregir la documentación:** `seguridad.md:65` (pines a nivel bajo), `pagina.h:144` (PING no mantiene el watchdog), manual `02_arquitectura.md:51` y `11_operacion.md:48` (~20 ms), `04_firmware.md:94` (distancia «filtrada»).
9. **Añadir a las pruebas de PC** el caso de pulso de arranque con un objeto que aparece a 20 cm (para fijar el criterio de `TOF_BLOQUEO_MM`), y el caso 100, 8190, 100, 100 para cuantificar el efecto del «sin eco» (ver `01_vl53l0x.md` §6.2).
