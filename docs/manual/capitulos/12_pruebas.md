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
   - En la **Corrida 2**, al concluir la reincorporación el navegador solicitó un ajuste angular menor a `5°`; como en el código actual los giros menores a `5°` usan el respaldo por tiempo (`calgiro_duracion_ms`) en lugar de `GIRO:<grados>`, la fricción estática de las ruedas produjo pequeños intentos cortos de giro antes de dar por terminada la ruta.
   - En la **Corrida 5**, el carro se detuvo en el paso `4/10` transicionando a `AYUDA` sin tocar la caja y habiendo espacio lateral libre. Al no haberse capturado un volcado cuadro a cuadro de la cámara y del láser en ese instante, se registra honestamente como **transición a `AYUDA` sin causa identificada** (posible falso positivo visual transitorio en el carril lateral o lectura puntual de `TOF_PARO`).

### Mediciones físicas de base en sesiones previas (J13 y J14)

Además del cierre B6 en J16, el repositorio conserva en `docs/precision/calibracion_j14.txt` las siguientes caracterizaciones físicas realizadas directamente sobre el carro:

- **Calibración del balance `PWM` en perfil `MANUAL` (`252/255`):** sobre una recta libre de `150 cm`, con `255/255` el carro se desviaba `14.5 cm` a la derecha; al ajustar `PWM_MANUAL_IZQ = 252` y `PWM_MANUAL_DER = 255`, el desvío físico medido con cinta bajó a **`3.0 cm`** (`VALIDADO EN EL CARRO`).
- **Tiempos reales de ejecución de `loop()` con sensores activos (`LOOP?`):** en tres corridas físicas consecutivas con `MPU6050` y `VL53L0X` activos, el firmware reportó:
  - Corrida 1: `LOOP:694,41111,116` (promedio `694 us`, máximo `41.1 ms`, `116` lecturas `I2C` > `5 ms`).
  - Corrida 2: `LOOP:691,34183,276` (promedio `691 us`, máximo `34.2 ms`, `276` lecturas `I2C` > `5 ms`).
  - Corrida 3: `LOOP:686,28399,276` (promedio `686 us`, máximo `28.4 ms`, `276` lecturas `I2C` > `5 ms`).

## 12.2 Pruebas deterministas de software en computadora (`VERIFICADO`)

En la Fase 1 y Fase 3 de este cierre se ejecutaron todas las suites automatizadas del repositorio sobre el código integrado en `main` (`cd90001`), obteniendo **100 % de casos aprobados (`0` fallos)**:

| Suite de verificación | Comando de ejecución | Programas / Módulos | Casos verificados | Fallos |
|---|---|---|---|---|
| **Núcleo de navegación y visión (`nucleo_c`)** | `make -C nucleo_c test` | `22` programas en C99 (incluye `pruebas_precision_j16`) | **`1,655` casos** | `0` |
| **Lógica de firmware (`pruebas_firmware`)** | `make -C pruebas_firmware test` | `5` programas en C99 (`carro`, `sensores`, `rumbo`, `recuperacion`, `integracion`) + `verificar_pagina.py` | **`540` casos** (+ `3/3` checks HTML) | `0` |
| **Servidor falso de protocolo WebSocket** | `python probar_servidor_falso.py` | Simulador completo de protocolo en Python | **`54` casos** | `0` |
| **Verificador criptográfico de versión** | `python scripts/probar_version.py` | Pruebas de `verificar_version.py` (APK, serie, hashes) | **`18` casos** | `0` |
| **Verificador de enlaces de documentación** | `python docs/verificar_enlaces.py` | Revisión de `134` archivos Markdown y `73` enlaces | **`73` enlaces** | `0` |
| **Sanitizadores en Linux (`UBSan`, `ASan`, `TSan`)** | `CFLAGS="-fsanitize=undefined,address"` | Ejecución limpia sin desbordamientos, fugas ni condiciones de carrera en `candado_vision` | Suite completa | `0` |

## 12.3 Resultados SIMULADOS: experimento Monte Carlo J16 (50,000 corridas)

Para cuantificar estadísticamente el efecto de cada mejora de software antes y después de J16 bajo condiciones repetibles de ruido físico, el programa `pruebas_precision_j16` ejecutó en el simulador físico-matemático (`simulador_navegador.c`) **1,000 corridas independientes con semilla fija (`0xF51A2026`) por cada combinación de escenario y configuración** (5 escenarios × 5 configuraciones = **50,000 corridas simuladas**, registradas en `docs/precision/monte_carlo_1000.txt`).

El modelo físico del simulador inyecta simultáneamente: asimetría de motores $\mathcal{N}(+0.8\text{ °/s}, 0.8^2)$, ruido de velocidad angular $\sigma = 0.6\text{ °/s}$ más perturbaciones de piso $\sigma = 1.5\text{ °/s}$, error de escala del giroscopio de `±1.5 %` (`±15,000 ppm`), deriva de sesgo térmico, variación de velocidad longitudinal de `±8 %`, deslizamiento de arranque y frenado, y ruido del sensor láser $\sigma = 12\text{ mm}$ con `2 %` de lecturas perdidas.

> **Advertencia de interpretación:** todas las cifras de la siguiente tabla son **`SIMULADAS`** en computadora. Demuestran la consistencia matemática de los algoritmos de J16 frente al modelo de perturbaciones, pero **no deben presentarse como mediciones en el vehículo real**.

| Escenario simulado (`1,000` corridas c/u) | Configuración evaluada | Mediana error final (`mm`) | Percentil 95 (`mm`) | Error rumbo mediano (`°`) | Colisiones (`%`) | Éxito de misión (`%`) |
|---|---|---|---|---|---|---|
| **1. Recta de 150 cm** (`(0,0) -> (0,1500) mm`) | Sin `RUMBO` (solo tiempo J14) | `94 mm` | `150 mm` | `4.1°` | `0.0 %` | `68.5 %` |
| **1. Recta de 150 cm** | **Con `RUMBO` activo (J16)** | **`8 mm`** | **`16 mm`** | **`0.4°`** | **`0.0 %`** | **`100.0 %`** |
| **2. Ruta en L** (`(0,0) -> (0,600) -> (600,600) mm`) | Base J14 (sin pose IMU ni `RUMBO`) | `64 mm` | `118 mm` | `4.8°` | `0.0 %` | `92.1 %` |
| **2. Ruta en L** | Solo J15 (pose IMU entera, sin `RUMBO`) | `58 mm` | `109 mm` | `4.5°` | `0.0 %` | `94.8 %` |
| **2. Ruta en L** | **J16 completo (`rumbo_x10` + `RUMBO`)** | **`36 mm`** | **`65 mm`** | **`0.6°`** | **`0.0 %`** | **`100.0 %`** |
| **3. Cuadrado de 60 cm** (`4 × 600 mm` + 4 giros de `90°`, regreso al origen) | Base J14 (sin escala IMU ni `RUMBO`) | `142 mm` | `248 mm` | `9.6°` | `0.0 %` | `41.2 %` |
| **3. Cuadrado de 60 cm** | J16 sin calibrar `IMU_ESCALA` (error `±1.5 %`) | `74 mm` | `136 mm` | `5.4°` | `0.0 %` | `84.6 %` |
| **3. Cuadrado de 60 cm** | **J16 con `IMU_ESCALA` calibrada + `RUMBO`** | **`42 mm`** | **`81 mm`** | **`0.9°`** | **`0.0 %`** | **`98.4 %`** |
| **4. Esquive caja 20×20 cm a `y = 60 cm`** (meta `(0,1400) mm`) | Base J14 (reincorporación ciega) | `128 mm` | `235 mm` | `6.2°` | `4.8 %` | `52.0 %` |
| **4. Esquive caja 20×20 cm a `y = 60 cm`** | Solo J15 (margen `100 mm`, sin `RUMBO`) | `61 mm` | `114 mm` | `3.8°` | `0.6 %` | `91.4 %` |
| **4. Esquive caja 20×20 cm a `y = 60 cm`** | **J16 completo (margen `150 mm` + `RUMBO`)** | **`38 mm`** | **`72 mm`** | **`0.7°`** | **`0.0 %`** | **`99.9 %`** |
| **5. Esquive caja 20×20 cm a `y = 80 cm`** (meta `(0,1600) mm`) | **J16 completo (margen `150 mm` + `RUMBO`)** | **`41 mm`** | **`76 mm`** | **`0.7°`** | **`0.0 %`** | **`99.8 %`** |

## 12.4 Escenarios y funciones NO MEDIDOS en el carro físico

Con el mismo rigor, la siguiente tabla enumera los escenarios que cuentan con implementación completa y pruebas en PC o simulación, pero que **no fueron corridos físicamente en el carro** durante el cierre de la versión 1.1:

| Escenario / Función | Estado en software / simulación | Estado físico en el carro | Razón por la que permanece `NO MEDIDO` |
|---|---|---|---|
| **Recta de 150 cm con y sin control `RUMBO` (`B5`)** | `SIMULADO` (`94 mm -> 8 mm` en Monte Carlo) | **`NO MEDIDO`** | La reserva de batería (`~4 %` de *powerbank*) se destinó prioritariamente a las 6 corridas del esquive completo B6. |
| **Ruta en L con cierre J16 (`B7`)** | `SIMULADO` (mediana `36 mm`, éxito `100 %`) | **`NO MEDIDO`** | Existe solo antecedente parcial J14 previo a `RUMBO` y a `rumbo_x10`. |
| **Cuadrado de 60 cm (`B8`) y calibración en piso de `IMU_ESCALA`** | `SIMULADO` (mediana `42 mm`, éxito `98.4 %`) | **`NO MEDIDO`** | Requiere serie dedicada de vueltas de `360°` y recorridos de 4 tramos en pista. |
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
| Banquito 35 × 45 | 245 ok, 596 contactos | 17 ok, 133 contactos | 774 ok, 81 contactos |
| Objeto de 90 cm | 4 ok, 535 contactos | 85 ok, 898 contactos | 1000 ok |
| Silla con patas | 161 ok, 524 contactos | 39 ok, 12 contactos | 318 ok, 40 contactos |
| Dos obstáculos | 359 ok, 67 contactos | 19 ok, 110 contactos | 994 ok, 6 contactos |
| Pared al frente | 1000 contactos | 480 contactos | 0 contactos, ayuda con causa 16 |
| Mancha de sol | 179 ok, 792 ayudas | 996 ok, 4 esquives | 1000 ok, 0 esquives |

Las compuertas del encargo **no se cumplen todas**: muebles con patas, 6 contactos de 1000 con dos obstáculos, 5 con el objeto bajo, paradas momentáneas en 7–12 % de las corridas con manchas de color, y 1 mm más de error lateral medio en el Monte Carlo heredado. El detalle y las razones están en `docs/autonomo_v2_diseno.md` y `docs/limites_conocidos.md`.

Pruebas de software de la rama: `make -C nucleo_c test` 1865 casos; `make -C pruebas_firmware test` 559 casos; servidor falso 60 casos; todos sin fallos.
