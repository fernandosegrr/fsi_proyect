# 12. Pruebas y resultados

**Revisión del 11-nov-2026:** rama `fix/navegacion-esquive-confirmado`, basada en v1.1. El arreglo y la prueba con banquito de 35×45 cm están **NO MEDIDOS**: primero debe repetirse con caja de 20×20 cm, previa autorización. Los resultados físicos J16 de abajo son históricos y no validan esta rama. No se instala APK, graba firmware ni declara v1.2.

Este capítulo presenta de manera íntegra y separada por nivel de evidencia todos los resultados de verificación del sistema hasta el cierre **J16** (10 de noviembre de 2026). De acuerdo con la convención establecida en el Capítulo 1, se distinguen en secciones independientes: (12.1) lo **`VALIDADO EN EL CARRO`** mediante corridas físicas sobre el vehículo real, (12.2) las suites deterministas de software ejecutadas en computadora personal, (12.3) los resultados **`SIMULADOS`** en el experimento Monte Carlo de 50,000 corridas y (12.4) el listado explícito de escenarios que permanecen **`NO MEDIDOS`** en el carro físico.

## 12.1 Resultados VALIDADO EN EL CARRO

### Validación física del esquive con reincorporación (Escenario B6 — 10 de noviembre de 2026)

Con el firmware **`FW:6d1b213-20261110`** grabado en la `ESP32 DevKit` y la aplicación **`APP:0.4-6d1b213`** (`carro_fsi_0.4-6d1b213.apk`) instalada en el teléfono Android, el 10 de noviembre de 2026 se ejecutó en el carro físico la campaña de validación del escenario **B6** (ruta recta con un obstáculo real de `20×20 cm` colocado sobre la trayectoria para evaluar detección, frenado, rodeo lateral en tres tramos con margen de `150 mm` y reincorporación a la línea original).

La sesión se realizó bajo restricción real de energía en la batería portátil USB de la lógica (`~4 %` restante en la *powerbank* al iniciar la serie y encendido verificado del portapilas `4×AA`), completándose **6 corridas físicas** registradas en `docs/pruebas_fisicas_20261110.csv`:

| Corrida | Identificador en CSV | Estado final | ¿Tocó la caja? | Error lateral final (medida aproximada) | Orientación final y observaciones de campo |
|---|---|---|---|---|---|
| **1** | `J16_B6_r1` | `TERMINADO` (`10/10` pasos) | **No** (`0` toques) | **`~10 cm`** a la derecha (`~100 mm`) | Completó detección, esquive y reincorporación de forma autónoma y estable. |
| **2** | `J16_B6_r2` | `TERMINADO` (`10/10` pasos) | **No** (`0` toques) | **`~10 cm`** a la derecha (`~100 mm`) | Comportamiento prácticamente idéntico a la Corrida 1; al final ejecutó pequeños pulsos de ajuste angular por tiempo. |
| **3** | `J16_B6_r3` | `TERMINADO` (`10/10` pasos) | **No** (`0` toques) | **`~5 cm`** a la derecha (`~50 mm`) | Reincorporación más cerrada hacia el eje original; terminó con una ligera inclinación residual hacia la derecha. |
| **4** | `J16_B6_r4` | `TERMINADO` (`10/10` pasos) | **No** (`0` toques) | **`~0 cm`** (sobre la línea) | Esquive y regreso alineados con la trayectoria recta original (`"quedó perfecto"`). |
| **5** | `J16_B6_r5` | `AYUDA` (en paso `4/10`) | **No** (`0` toques) | *(No aplica; se detuvo antes de reincorporar)* | Se detuvo pidiendo ayuda durante el esquive pese a tener espacio físico suficiente; **sin causa identificada**. |
| **6** | `J16_B6_r6` | `TERMINADO` (`10/10` pasos) | **No** (`0` toques) | **`~0 cm`** (sobre la línea) | Completó los 10 pasos sin contacto y sobre el eje original (`"no tocó y quedó perfecto"`). |

### Síntesis y matices técnicos de la validación física B6

1. **Seguridad anticolisión (`6/6` sin contacto):** en las **6 corridas (`100 %`)**, el vehículo detectó la caja oportunamente, frenó a distancia segura e inició la maniobra sin tocar ni rozar el obstáculo en ninguna ocasión.
2. **Tasa de éxito autónomo completo (`5/6` = `83.3 %`):** cinco de las seis ejecuciones completaron de principio a fin los 10 pasos del plan (avance inicial, parada, desvío lateral de `40 cm`, rebase paralelo con margen J16 de `150 mm`, regreso diagonal y alineación final) terminando en estado `TERMINADO`.
3. **Precisión lateral aproximada (`~0 cm` a `~10 cm`):** en las cinco corridas completadas, la desviación lateral final respecto a la prolongación de la línea recta original estuvo entre **`~0 cm` y `~10 cm` hacia la derecha** (`~10, ~10, ~5, ~0, ~0 cm`; mediana aproximada de **`~5 cm`**). Tal como establece la regla de honestidad del manual, se subraya que estas cifras son **medidas aproximadas** observadas en pista al concluir cada corrida.
4. **Análisis de los dos comportamientos particulares observados:**
   - En la **Corrida 2**, al concluir la reincorporación el navegador solicitó un ajuste angular menor a `5°`; como en el código actual los giros menores a `5°` usan el respaldo por tiempo (`calgiro_duracion_ms`) en lugar de `GIRAR:<grados>`, la fricción estática de las ruedas produjo pequeños intentos cortos de giro antes de dar por terminada la ruta.
   - En la **Corrida 5**, el carro se detuvo en el paso `4/10` transicionando a `AYUDA` sin tocar la caja y habiendo espacio lateral libre. Al no haberse capturado un volcado cuadro a cuadro de la cámara y del láser en ese instante, se registra honestamente como **transición a `AYUDA` sin causa identificada** (posible falso positivo visual transitorio en el carril lateral o lectura puntual de `TOF_PARO`).

### Mediciones físicas de base en sesiones previas (J13 y J14)

Además del cierre B6 en J16, el repositorio conserva en `docs/precision/calibracion_j14.txt` las siguientes caracterizaciones físicas realizadas directamente sobre el carro:

- **Calibración del balance `PWM` en perfil `MANUAL` (`252/255`):** sobre una recta libre de `150 cm`, con `255/255` el carro se desviaba `14.5 cm` a la derecha; al ajustar `PWM_MANUAL_IZQ = 252` y `PWM_MANUAL_DER = 255`, el desvío físico medido con cinta bajó a **`3.0 cm`** (`VALIDADO EN EL CARRO`).
- **Tiempos reales de ejecución de `loop()` con sensores activos (`LOOP?`):** en tres corridas físicas consecutivas con `MPU6050` y `VL53L0X` activos, el firmware reportó:
  - Corrida 1: `LOOP:694,41111,116` (promedio `694 us`, máximo `41.1 ms`, `116` lecturas `I2C` > `5 ms`).
  - Corrida 2: `LOOP:691,34183,276` (promedio `691 us`, máximo `34.2 ms`, `276` lecturas `I2C` > `5 ms`).
  - Corrida 3: `LOOP:686,28399,276` (promedio `686 us`, máximo `28.4 ms`, `276` lecturas `I2C` > `5 ms`).

## 12.2 Pruebas deterministas de software en computadora (`VERIFICADO`)

En la Fase 1 y Fase 3 de este cierre se ejecutaron todas las suites automatizadas del repositorio sobre el código integrado en `main` (`cd90001`), obteniendo **100 % de casos aprobados (`0` fallos)**. Las cifras de esta tabla son las del cierre v1.1; en v1.2 (24-nov-2026, autónomo v3 más la revisión de la interfaz) salieron `1,901` casos en `24` programas del núcleo, `567` en `5` programas del firmware, `60` del servidor falso y `21` de `probar_version.py`, todos sin fallos (detalle en la página {{pag:H.4}}):

| Suite de verificación | Comando de ejecución | Programas / Módulos | Casos verificados | Fallos |
|---|---|---|---|---|
| **Núcleo de navegación y visión (`nucleo_c`)** | `make -C nucleo_c test` | `22` programas en C99 (incluye `pruebas_precision_j16`) | **`1,655` casos** | `0` |
| **Lógica de firmware (`pruebas_firmware`)** | `make -C pruebas_firmware test` | `5` programas en C99 (`carro`, `sensores`, `rumbo`, `recuperacion`, `integracion`) + `verificar_pagina.py` | **`540` casos** (+ `3/3` checks HTML) | `0` |
| **Servidor falso de protocolo WebSocket** | `python probar_servidor_falso.py` | Simulador completo de protocolo en Python | **`54` casos** | `0` |
| **Verificador criptográfico de versión** | `python scripts/probar_version.py` | Pruebas de `verificar_version.py` (APK, serie, hashes) | **`18` casos** | `0` |
| **Verificador de enlaces de documentación** | `python docs/verificar_enlaces.py` | Revisión de `134` archivos Markdown y `73` enlaces | **`73` enlaces** | `0` |
| **Sanitizadores en Linux (`UBSan`, `ASan`, `TSan`)** | `CFLAGS="-fsanitize=undefined,address"` | Ejecución limpia sin desbordamientos, fugas ni condiciones de carrera en `candado_vision` | Suite completa | `0` |

## 12.3 Resultados SIMULADOS: experimento Monte Carlo J16 (50,000 corridas)

Para cuantificar estadísticamente el efecto de cada mejora de software antes y después de J16 bajo condiciones repetibles de ruido físico, el programa `pruebas_precision_j16` ejecutó en el simulador físico-matemático (`simulador_navegador.c`) **1,000 corridas independientes con semillas fijas (`160000 + índice del escenario × 10007 + corrida`, la misma en todas las configuraciones: corridas pareadas; la constante `0xF51A2026` que citaba la v1.1 no está en el código) por cada combinación de escenario y configuración** (10 escenarios × 5 configuraciones × 1,000 corridas = **50,000 corridas simuladas**; la tabla muestra cinco de los diez escenarios, registradas en `docs/precision/monte_carlo_1000.txt`).

El modelo físico del simulador inyecta simultáneamente: asimetría de motores $\mathcal{N}(+0.8\text{ °/s}, 0.8^2)$, ruido de velocidad angular $\sigma = 0.6\text{ °/s}$ más perturbaciones de piso $\sigma = 1.5\text{ °/s}$, error de escala del giroscopio de `±1.5 %` (`±15,000 ppm`), deriva de sesgo térmico, variación de velocidad longitudinal de `±8 %`, deslizamiento de arranque y frenado, y ruido del sensor láser $\sigma = 12\text{ mm}$ con `2 %` de lecturas perdidas.

> **Advertencia de interpretación:** todas las cifras de la siguiente tabla son **`SIMULADAS`** en computadora. Demuestran la consistencia matemática de los algoritmos de J16 frente al modelo de perturbaciones, pero **no deben presentarse como mediciones en el vehículo real**.

| Escenario simulado (`1,000` corridas c/u) | Configuración evaluada | Mediana del error final (`mm`) | Percentil 95 (`mm`) | Error de rumbo medio (`°`) | Llegan a la meta (de `1,000`) | Toques a la caja (de `1,000`) |
|---|---|---|---|---|---|---|
| **1. Recta de 150 cm** (`recta_150`) | Base J14 (solo tiempo, sin `RUMBO`) | `137 mm` | `232 mm` | `9.4°` | `1000` | `0` |
| **1. Recta de 150 cm** | J15 actual (pose del giroscopio, sin `RUMBO`) | `81 mm` | `126 mm` | `3.6°` | `1000` | `0` |
| **1. Recta de 150 cm** | **J16 con `RUMBO` activo** | **`9 mm`** | **`26 mm`** | **`0.5°`** | `1000` | `0` |
| **2. Ruta en L** (`ruta_L`, `(0,0) -> (0,600) -> (600,600) mm`) | Base J14 | `145 mm` | `233 mm` | `11.2°` | `1000` | `0` |
| **2. Ruta en L** | J15 actual | `43 mm` | `86 mm` | `3.0°` | `1000` | `0` |
| **2. Ruta en L** | J16 con `RUMBO` | `32 mm` | `71 mm` | `0.8°` | `1000` | `0` |
| **2. Ruta en L** | **J16 con `RUMBO` e `IMU_ESCALA` calibrada** | **`31 mm`** | **`68 mm`** | **`0.6°`** | `1000` | `0` |
| **3. Cuadrado de 60 cm** (`cuadrado`, 4 tramos de `600 mm` y 4 giros de `90°`) | Base J14 | `65 mm` | `125 mm` | `10.8°` | `1000` | `0` |
| **3. Cuadrado de 60 cm** | J15 actual | `32 mm` | `80 mm` | `4.6°` | `1000` | `0` |
| **3. Cuadrado de 60 cm** | J16 con `RUMBO` (escala sin calibrar) | `23 mm` | `59 mm` | `2.3°` | `1000` | `0` |
| **3. Cuadrado de 60 cm** | **J16 con `RUMBO` e `IMU_ESCALA` calibrada** | **`19 mm`** | **`43 mm`** | **`1.3°`** | `1000` | `0` |
| **4. Esquive de caja 20×20 cm** (`esquive_20x20`) | Base J14 | `200 mm` | `324 mm` | `11.9°` | `1000` | `49` |
| **4. Esquive de caja 20×20 cm** | J15 actual | `33 mm` | `92 mm` | `3.7°` | `1000` | `9` |
| **4. Esquive de caja 20×20 cm** | **J16 con `RUMBO`** | **`16 mm`** | **`40 mm`** | **`0.6°`** | `1000` | `0` |
| **5. Esquive de caja ancha** (`esquive_ancha`) | Base J14 | `203 mm` | `320 mm` | `12.0°` | `998` | `46` |
| **5. Esquive de caja ancha** | **J16 con `RUMBO`** | **`15 mm`** | **`40 mm`** | **`0.6°`** | `1000` | `0` |

El archivo trae diez escenarios (`recta_100`, `recta_150`, `recta_200`, `giro_45`, `giro_90`, `giro_180`, `ruta_L`, `cuadrado`, `esquive_20x20` y `esquive_ancha`); aquí se muestran cinco. La mediana es la columna `p50` de `error_mm` (error lateral en rectas y esquives; posición final en rutas con giros) y el error de rumbo es la media de `rumbo_x10` (décimas de grado, valor absoluto). «J15 actual» y «J16 con `RUMBO`» son las configuraciones `ANTES: J15 actual` y `DESPUES con RUMBO (firmware nuevo)` del archivo.

> **Nota:** las cifras de esta tabla en el manual v1.1 (`94 mm -> 8 mm`, `64 -> 36`, `142 -> 42`, `128 -> 38`, con porcentajes de éxito) quedaron **no localizadas; reemplazadas por `docs/precision/monte_carlo_1000.txt`**.

## 12.4 Escenarios y funciones NO MEDIDOS en el carro físico

Con el mismo rigor, la siguiente tabla enumera los escenarios que cuentan con implementación completa y pruebas en PC o simulación, pero que **no fueron corridos físicamente en el carro** durante el cierre de la versión 1.1:

| Escenario / Función | Estado en software / simulación | Estado físico en el carro | Razón por la que permanece `NO MEDIDO` |
|---|---|---|---|
| **Recta de 150 cm con y sin control `RUMBO` (`B5`)** | `SIMULADO` (mediana `137 mm -> 9 mm` en Monte Carlo) | **`NO MEDIDO`** | La reserva de batería (`~4 %` de *powerbank*) se destinó prioritariamente a las 6 corridas del esquive completo B6. |
| **Ruta en L con cierre J16 (`B7`)** | `SIMULADO` (mediana `145 mm -> 31 mm` en Monte Carlo) | **`NO MEDIDO`** | Existe solo antecedente parcial J14 previo a `RUMBO` y a `rumbo_x10`. |
| **Cuadrado de 60 cm (`B8`) y calibración en piso de `IMU_ESCALA`** | `SIMULADO` (mediana `65 mm -> 19 mm` en Monte Carlo) | **`NO MEDIDO`** | Requiere serie dedicada de vueltas de `360°` y recorridos de 4 tramos en pista. |
| **Corte inducido de WiFi en plena marcha (`B9`)** | `VERIFICADO` en `pruebas_firmware` (`WD:<n>` a los `1000 ms`) | **`NO MEDIDO`** | No se realizó la prueba física de apagar el WiFi del teléfono con el carro avanzando. |
| **Freno autónomo aislado por láser (`TOF_PARO` a `120 mm`)** | `VERIFICADO` en `pruebas_firmware` y activo en B6 | **`NO MEDIDO` aislado** | No se registró una serie con cinta métrica dedicada exclusivamente a medir la distancia de parada pura a `120 mm`. |
| **Obstáculo ancho (`40×20 cm`) y calibración automática (`B2`)** | `SIMULADO` / `PENDIENTE` | **`NO MEDIDO`** | Fuera de la sesión física rápida del 10 de noviembre de 2026. |
| **Circuito `GPIO 2` y consumo eléctrico en amperios** | Configurado en `config.h` | **`NO MEDIDO`** | Sin instrumentación de corriente ni resistor externo verificado en `GPIO 2`. |

Dónde está en el código: `pruebas_fisicas_20261110.csv`, `estado_20261110.md`, `monte_carlo_1000.txt`, `calibracion_j14.txt`, `pruebas_precision_j16.c`, `pruebas_rumbo.c`.

## 12.5 Autonomía del 12-nov-2026: resultados solo de software

La primera corrección (458d5b3) mandaba a AYUDA el banquito que v1.1 superaba en el simulador. Se reemplazó por consenso por zona y rebase iterativo. Ahora la caja 20×20, el banquito 35×45 y el objeto de 90 cm de fondo terminan después del objeto, con 1, 3 y 6 intentos respectivamente y cero contactos. El banquito termina en (1001,-17,0°), a los 32,10 s; su cola supera el objeto por 527 mm.

Los falsos de solo color, solo red y las tres zonas de solo color continúan sin STOP intermedio con láser libre. La coincidencia color/red permite esquive con láser libre o sin eco; una fuente sin eco continúa con precaución. La ruta corta se prolonga para completar el rebase, sin falso Terminado.

Monte Carlo pareado contra bd1727b: 1000 corridas por escenario y configuración. Sin RUMBO, media/p95 lateral de caja pasan de 33/78 a 32/76 mm; ancho, de 33/79 a 32/77 mm. Con RUMBO, caja pasa de 17/40 a 17/37; ancho, de 17/40 a 16/38. Con escala calibrada, caja pasa de 17/40 a 16/37; ancho, de 17/39 a 16/38. Las seis comparaciones conservan 1000 llegadas y cero contactos, sin tolerancia a empeorar. No son medidas físicas ni precisión longitudinal.

Pruebas y registros reproducibles: `docs/diagnostico_esquive_20261111.md` y `scripts/comparar_esquives.py`. **Banquito: NO MEDIDA. Arreglo: NO MEDIDO en el carro.** Primero repetir con caja de 20×20 cm y telemetría, previa autorización; la validación B6 histórica no se transfiere a esta rama. APK y firmware no compilados ni instalados en esta revisión.

## 12.6 Autónomo v2: banco realista y compuertas (`SIMULADO`)

Rama `feat/autonomo-v2`. El banco `nucleo_c/pruebas_autonomo_v2.c` corre el navegador, el detector de color y la fusión reales contra un mundo sintético (`mundo_v2.c`): objetos con volumen, baldosas con juntas, manchas de sol, sombras, reflejos, cámara con exposición automática, láser con su cono y una red neuronal estadística. **Ninguno de estos números se midió en el carro.**

`python scripts/compuertas_autonomo.py --corridas 1000` compila tres versiones del núcleo (v1.1, `6bea71e` y la actual) contra el mismo banco y deja los resultados en `docs/precision/autonomo_v2_*.txt`.

| Escenario (1000 corridas) | v1.1 | `6bea71e` | Autónomo v2 |
|---|---|---|---|
| Caja 20 × 20 | 629 ok, 2 contactos | 1000 ok | 1000 ok |
| Caja corrida de lado | 991 ok, 3 contactos | 663 ok, 337 contactos | 998 ok, 2 contactos |
| Banquito 35 × 45 | 245 ok, 596 contactos | 17 ok, 133 contactos | 765 ok, 79 contactos |
| Objeto de 90 cm | 4 ok, 535 contactos | 85 ok, 898 contactos | 1000 ok |
| Silla con patas | 161 ok, 524 contactos | 39 ok, 12 contactos | 316 ok, 34 contactos |
| Dos obstáculos | 359 ok, 67 contactos | 19 ok, 110 contactos | 1000 ok |
| Objeto bajo | 960 ok, 25 contactos | 990 ok, 4 contactos | 997 ok, 1 contacto |
| Caja antes de una esquina | 586 ok, 2 contactos | 999 ok | 1000 ok |
| Caja con pared pegada a un lado | 470 ok, 79 contactos | 632 ok, 216 contactos | 615 ok, 2 contactos, 385 ayudas |
| Pared al frente | 1000 contactos | 480 contactos | 0 contactos, ayuda con causa 16 |
| Mancha de sol | 179 ok, 792 ayudas | 996 ok, 4 esquives | 1000 ok, 0 esquives |

Dieciséis de los 23 escenarios quedan en 1000 de 1000 sin contactos. Las compuertas del encargo **no se cumplen todas** (19 en rojo): muebles con patas, 2 contactos de 1000 con la caja corrida y 1 con el objeto bajo, la caja con una pared pegada (el carro pide ayuda en vez de tocarla), paradas momentáneas en 8–13 % de las corridas con manchas de color, y 1 mm más de error lateral medio en el Monte Carlo heredado. El detalle y las razones están en `docs/autonomo_v2_diseno.md` y `docs/limites_conocidos.md`.

Los dos últimos escenarios de la tabla y varias correcciones salieron de una revisión independiente del código hecha el 14-nov (sin hallazgos críticos, dos altos, tres medios y tres bajos, todos corregidos).

Pruebas de software de la rama: `make -C nucleo_c test` 1884 casos; `make -C pruebas_firmware test` 564 casos; servidor falso 60 casos; todos sin fallos.

## 12.7 Autónomo v3 (`SIMULADO`; firmware y app solo compilados)

Rama `feat/autonomo-v3`. Las columnas del banco ahora son **excluyentes y suman las corridas**: `ok`, `contacto`, `bajo_mueble` (pasó entre las patas sin tocar), `ayuda`, `error`, `sin_terminar`, `falso_fin`, `esquive_falso` y `otro`, en ese orden de prioridad. El script `scripts/compuertas_autonomo.py --corridas 1000` compara cuatro versiones (v1.1, `6bea71e`, v2 y la actual) y deja los resultados en `docs/precision/autonomo_v3_*.txt` (los de v2 no se tocan).

Verificación de software de la rama: `make -C nucleo_c test` 1901 casos y `make -C pruebas_firmware test` sin fallos, el núcleo también con `-ftrapv` (desbordes enteros) y el analizador estático de GCC sin avisos; servidor falso 60 casos; `scripts/probar_version.py` 21 casos; la app y los dos firmwares compilan (no se instalaron ni se grabaron). **No se pudieron correr ASan, UBSan ni TSan** (el compilador de Windows de este equipo no trae sus bibliotecas y no hay WSL). Revisión independiente de seguridad (un subagente que no vio cómo se hizo el cambio): sin hallazgos críticos ni altos; dos medios y cuatro menores, corregidos o documentados.

Compuertas del encargo contra el banco: **siguen rojas** (23 líneas; v2 tenía 19): muebles con patas, paradas momentáneas con sol, sombra y reflejo (8–13 % de las corridas), la caja con pared a un lado (613 ok, 387 ayudas) y, por comparar contra v2, una ayuda de más en la caja corrida de lado, una en el banquito y nueve en la silla (las ayudas «finales» bajan: 44 → 33 y 32 → 15, y los contactos se reducen a la mitad o más). Error lateral final del Monte Carlo heredado: 18/44 mm contra 17/40 de v1.1 (SIMULADO); con un margen de paso menor (180 mm) se iguala, pero produce 2 contactos en el objeto largo: no se aplica.

## 12.8 Pruebas físicas del 25–26 de noviembre de 2026 (v3 + correcciones del 26-nov)

Versiones probadas: `main` `b396390`, cámara `cam-v2` (GC0308, ~12.4–12.5 fps HQVGA), carro `FW:b396390` (`SEG=3`, `TOFP=150`, `TOF_BLOQUEO=180`), app `0.4-b396390` (`versionCode` 3) más las dos correcciones de código del 26-nov (`navegador.c` y `ManualFragment.kt`). Operó el carro: Jaime.

> **Regla de rigor:** las corridas posteriores a las correcciones del 26-nov se presentan como **con las correcciones del 26-nov; mismos escenarios donde ocurrió la falla; `n` pequeño**, no como validación independiente completa.

| Prueba | Condición y `n` | Resultado agregado | Etiqueta |
|---|---|---|---|
| Línea base recta 2 m con `IMU+RUMBO` | `n=3` (tras reparar cable SCL roto; 2 previas sin IMU derivaron +43 cm) | `3/3` OK, 186 cm, 28.5 cm/s, deriva 0 / 11 / 0 cm, 0 falsos positivos | **VALIDADO EN EL CARRO** |
| M8 velocidad en AUTO | `n=5` | `5/5` completas, 140 cm en 4.82–4.91 s, media 28.7 cm/s, deriva 0 cm en `5/5` | **VALIDADO EN EL CARRO** |
| M8 velocidad en MANUAL | Sin corridas por límite de seguridad | Sin medición | **NO MEDIDO** |
| M5 frenado láser (`TOFP=150`) | `n=10` | `10/10` sin contacto; `TOF_PARO` media 144.7 mm; margen del chasis 8.0–10.0 cm (media 9.1 cm), de la punta del láser 4.5–6.5 cm (media 5.6 cm) | **VALIDADO EN EL CARRO** |
| M6 bloqueo en arranque (`TOF_BLOQUEO=180`) | `n=3` por distancia (20, 18 y 16 cm) | A 20 y 18 cm: primera corrida avanzó 355–415 ms sin tocar y frenó a 9–10 cm, las otras bloqueo instantáneo (6–19 ms); a 16 cm `3/3` bloqueo instantáneo. Sesgo positivo del VL53L0X de +40 a +51 mm contra la punta del láser (+67 a +102 mm con tela negra mate) | **VALIDADO EN EL CARRO** |
| M1 deriva del giroscopio | `n=1` de 10 min quieto + 32.3 s acumulados en AUTO | 0.0° en 0, 1, 2, 5 y 10 min quieto; 0.0° tras 32.3 s en AUTO; muestreo 10.0 Hz | **VALIDADO EN EL CARRO** |
| M2 patas | Omitido por el dueño | Sin medición en el carro; sigue en banco | **NO MEDIDO** / **SIMULADO** |
| M3 pared lateral a 30 cm | `n=20` por ángulo | 0°: 100 % libre; ~20°: 85 % libre; ~31°, ~43° y ~61°: 0 % libre (detectada) | **VALIDADO EN EL CARRO** |
| Objeto negro mate (mochila 42×14 cm), estático | 20–80 cm, `n=100` | `100/100` con eco sólido (0 % sin eco). El caso de 0 % de eco sigue solo en banco | **VALIDADO EN EL CARRO** (eco); **SIMULADO** (0 % eco) |
| Objeto negro mate (mochila 42×14 cm), dinámico | `main` original (`n=3`) vs. corrección 26-nov (`n=3`) | Original: r1 terminó sin tocar, r2 AYUDA causa 14, r3 fallo grave con contacto. Con corrección: `3/3` terminado, 0 contactos, 0 AYUDA, reincorporación a 1–2 cm de la línea | **VALIDADO EN EL CARRO** (correcciones 26-nov; mismo escenario; `n` pequeño) |
| Objetos bajos (2 cm y 6 cm) | Estático a 40 cm (`n=20`); dinámico a 90 cm (`n=3` c/u) | Láser vio ambos en `20/20` a 40 cm (`aprenderPiso` rechaza a 40 cm: 18 % > 5 %). Dinámico 2 cm: `3/3` terminado, 0 contactos. Dinámico 6 cm: r1 AYUDA causa 2 sin contacto (antes de corrección); r2 y r3 terminados, 0 contactos (con corrección) | **VALIDADO EN EL CARRO** (correcciones 26-nov; mismo escenario; `n` pequeño) |
| Piso limpio 1.5 m | `n=5` | `5/5` terminado, `0/5` falsos positivos, 0 paradas falsas, 0 esquives; recorrió 133–139 cm de 150 por desgaste de pilas (recalibración `ms_por_cm_x10=355` aplicada después, sin repetir corridas) | **VALIDADO EN EL CARRO** |

- **Límite conocido de `aprenderPiso`:** rechaza el arranque con obstáculos a 40–80 cm (`descartadoPct > 5`, medido en 18 % > 5 % a 40 cm); el ajuste está pendiente y no se aplicó, por lo que las rutas arrancan con al menos 100 cm libres delante.
- **Siguen `SIMULADO`, `NO MEDIDO` o `PENDIENTE`:** M8 manual, M2 patas, objeto con 0 % de eco IR, M4, M7, M9, bloques C y D (luz cambiante, ruta en L, cuadrado) y la prueba física de `RUMBO:ON` en modo Manual (**PENDIENTE**).
