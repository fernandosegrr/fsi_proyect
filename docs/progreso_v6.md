# Bitácora de progreso — autónomo v6 (rama `feat/autonomo-v6`)

**SIMULADO / NO MEDIDO EN EL CARRO.** El carro no se movió, no se instaló la APK, no se grabó firmware y la PC no se unió a ninguna red de las placas: solo se compilaron la app y los dos firmwares. `main` sigue en `bd1727b`; `feat/autonomo-v3`, `feat/autonomo-v5-carbon`, `feat/autonomo-v5-carbon-fix` y las etiquetas `freeze/v5-carbon`, `freeze/v5-carbon-fix` y `archive/feat-autonomo-v4-a200df4` no se tocaron. No se fusionó nada a `main`.

Diseño, diagnóstico y resultados por compuerta: [autonomo_v6_diseno.md](autonomo_v6_diseno.md). Protocolo del carro: [protocolo_prueba_fisica_v6.md](protocolo_prueba_fisica_v6.md). Datos crudos: `docs/precision/v6/`.

---

## 0. Punto de partida

- Etiqueta `freeze/v5-carbon-fix` (`606c9f9`), rama `feat/autonomo-v6`. Base segura: `v3` (`e37f77b`). `v4` descartado.
- Reglas que no se rompieron: nada de `secretos.h` (los firmwares se compilaron en una carpeta temporal con `secretos.ejemplo.h`), nada de `original_backup/`, ningún binario de firmware, caja negra ni CSV de pruebas físicas publicado; commits con la identidad `noreply` de GitHub; sin `--force`; láser con autoridad encendida por defecto; `TOF_PARO`, watchdog de 1000 ms y STOP/EBRAKE intactos; registros de la cámara intactos.

## 1. Fase 0 — verificación, datos reales y bancos (24-nov, mañana)

- **Datos físicos nuevos:** se buscaron mediciones posteriores al 14/15-nov (`docs/pruebas_fisicas_*.csv` y `cajas_negras/`). **No hay ninguna más nueva** que las cajas negras del 15-nov (7 corridas rectas de 150 cm en piso limpio, versión v3). Se volverá a buscar justo antes de congelar (resultado en la sección 5). De ellas se publican solo agregados ([real_estadistica_vision_20261115.json](precision/v6/real_estadistica_vision_20261115.json)).
- **El simulador contradecía las cajas negras** (la visión en piso limpio era casi muda frente a la real). Se corrigió y se documentó en [precision/v6/calibracion_v6.md](precision/v6/calibracion_v6.md): `AV6_RUIDO=1` calibra contra lo real; `AV6_RUIDO=2` agrega marcas planas que NO salen de ninguna medición. Con el banco heredado (`AV6_RUIDO=0`) todo se sigue pudiendo reproducir.
- **Familia sin eco** (12 escenarios nuevos, otros colores, texturas, tamaños y pisos; [precision/v6/familia_sin_eco.md](precision/v6/familia_sin_eco.md)) para no ajustar el detector contra un solo escenario. Conjuntos separados: `DESARROLLO` (semillas A, B, C, A2, B2, C2 ya usadas en rondas anteriores cuentan como desarrollo) y `RESERVADO`.
- **Verificación base (antes de cambiar nada):** pruebas del núcleo y del firmware en verde, `probar_version.py` y `verificar_enlaces.py` en verde, `probar_servidor_falso.py` en verde ([precision/v6/fase0_*.txt](precision/v6/)). La comparación Monte Carlo heredada (`scripts/comparar_esquives.py`, 6 comparaciones) da **4 FALLO** que ya existían antes de esta fase (las variantes con `RUMBO` del firmware, diferencias de 1 a 4 mm en media y p95 del error lateral): no tienen que ver con v6 y no se tocaron.
- **Líneas base** de `v3`, `v5-carbon` y `v5-carbon-fix` con el simulador actualizado: [precision/v6/baseline_fase0.txt](precision/v6/baseline_fase0.txt).

## 2. Fase 1 — diagnóstico de los huecos

Ver [autonomo_v6_diseno.md](autonomo_v6_diseno.md), sección 3: `pared_al_frente` con láser pesimista (mecanismo del giro de regreso y la guardia), `terminado_falso` de `silla_patas` (límite heredado de detección), la parada de más en `falso_sombra` (explicada: ruido estadístico, no mecanismo), la regresión de v5 en piso limpio con ruido realista y por qué el objeto oscuro sin eco y la mancha plana oscura no se pueden separar.

## 3. Fase 2 — etapas y commits

| etapa | commit | qué |
|---|---|---|
| 0 | `6489cb9` | simulador con ruido perceptivo real, familia sin eco, banco ampliado, datos de calibración |
| 1 | `2ec3c8c` | tres banderas del navegador (`GUARDIA_VOLVER`, `RACHA_ESTRICTA`, `LATERAL_PERSISTENTE`), pruebas, `compuertas_v6.py`, `evaluacion_ciega_v6.py` |
| 2 | `0a375c0` | app: arranque seguro como v3 (v5 y v6 apagados) e interruptor v6 |
| 2b | `3efea6a` | app: la caja negra anota la versión efectiva (`v5=`, `v6=`, `version=`) |

Después de cada etapa: pruebas del núcleo y del firmware en verde, equivalencias exactas (apagado = v5-fix y v5 apagado = v3) y banco en `DESARROLLO` y `RESERVADO`.

**Verificación (SIMULADO), resultado final:**

- `make -C nucleo_c test`: 24 grupos de pruebas, **0 fallos** (`pruebas_navegador` 229 casos, `pruebas_esquive_confirmado` 287).
- `make -C pruebas_firmware test`: 5 grupos, 573 casos, **0 fallos**.
- APK: `scripts/compilar_app.ps1` → `BUILD SUCCESSFUL` (**no instalada**).
- Firmwares: `firmware_carro` (`esp32:esp32:esp32`, 78 % del programa) y `camara_prueba` (`esp32:esp32:esp32wrover`, 82 %) **compilan** en una carpeta temporal con `secretos.ejemplo.h`; no se subió nada. No cambian desde `v5-carbon-fix`.
- **Equivalencias exactas (banco, 16 matrices: DESARROLLO, RESERVADO y las dos familias × ruido 0/1 × láser normal/pesimista):** `python scripts/compuertas_v6.py --apagado` → **32 de 32 comparaciones idénticas** ([precision/v6/equivalencias_apagado.txt](precision/v6/equivalencias_apagado.txt)): v6 apagado es `v5-carbon-fix` línea por línea y v5 apagado (aunque se pida v6) es `v3` línea por línea.

**Revisión independiente de seguridad** (un subagente nuevo, sin mis conclusiones, sobre el rango `freeze/v5-carbon-fix..HEAD`):

- **ALTO (condicional, ya existía en v3 y v5):** la guardia del giro deja `esq_en_giro = 1` al detener el giro; tras `AYUDA` y «Continuar», los cuadros del centro se ignoran hasta el siguiente giro y solo queda el freno del láser del firmware. v6 añadía una segunda entrada a ese defecto (la guardia del regreso). **Corregido en v6** (cierra el giro en las dos guardias, solo con la bandera encendida: con v6 apagado se conserva exactamente el comportamiento de v3 y v5) con prueba unitaria. **En `v3`/`v5` sigue existiendo**: por eso el protocolo pide no pulsar «Continuar» tras una ayuda `Causa 18`.
- **MEDIO:** la persistencia lateral valía en todos los estados (retrasaba ~0.4 s la reacción a lo lateral durante el rodeo y el regreso). **Corregido:** ahora solo vale mientras sigue la ruta; en el banco no cambia ningún número y no se retrasa nada mientras el carro maniobra.
- **MEDIO (por diseño, documentado):** la racha estricta quita evidencia de patas delgadas cuando la red las atribuye a otra zona: es el costo en cobertura de muebles que se aceptó a cambio de piso limpio.
- **BAJO:** la guardia del paso en paralelo reutiliza la `Causa 16` (su frase no describe exactamente esa situación); el lazo guardia → ayuda → continuar → rodeo está acotado por las 3 ayudas sin avance de v5; la ventana de frescura del láser es de 400 ms (puede dar un freno de más tras una pérdida de lectura: falla segura); el simulador de rutas de la app (`sim_ruta.c`) usa los valores por defecto del núcleo y no sigue los interruptores (no mueve motores). Comentario con una función inexistente: corregido.
- **Verificado por el revisor:** con las banderas apagadas el navegador es idéntico a `freeze/v5-carbon-fix` (prueba diferencial de 500 semillas × 6000 ticks, v5 encendido y apagado, con un resumen por tick de comando, estado, fase, arreglos de zonas, contadores, temporizadores, pose y líneas de diagnóstico); las guardias solo agregan STOPs; las claves de la app arrancan apagadas; compilación C99 limpia.

**Compuertas estrictas con el código final (SIMULADO, 16 matrices × 120 corridas por escenario; detalle por compuerta en [autonomo_v6_diseno.md](autonomo_v6_diseno.md), sección 6):** agregado en el peor caso, **G1 NO CUMPLE, G2 NO CUMPLE, G3 AVISO, G4 AVISO, G5 NO CUMPLE, G6 NO CUMPLE, G7 NO CUMPLE**; con el ruido de esfuerzo sin calibrar (`ruido 2`) el veredicto agregado es el mismo. En concreto:

- Piso limpio con ruido calibrado: `v6` recupera del orden de dos tercios de lo que `v5-fix` perdía, **pero sigue peor que `v3`** en esquives y `AYUDA` con sol, sombra y reflejo (`DESARROLLO` 795/0/111 e507 contra 798/0/101 e440; `RESERVADO` 788/0/100 e479 contra 788/0/105 e430); con ruido 0 es idéntico a `v3` salvo una parada de más en `falso_sombra` de `DESARROLLO` (8 contra 7; explicada en el diseño, sección 3.5). `v6` **nunca es peor que `v5-fix` en G1**.
- Sólidos: menos contactos que `v3` en el núcleo (ruido 1: 17 → 6, 28 → 14, 23 → 15, 31 → 18) y `pared_al_frente` 3 contactos en 7680 contra 68 de `v3`; **no llega a cero** y en dos celdas queda un contacto por encima de `v3` (`caja20`, `aparece_de_repente`).
- Sin eco y muebles: contactos muy por debajo de `v3` (`bloque_sin_eco` 120 → 25; muebles `RESERVADO` 105 → 71 con ruido 0), con más `AYUDA` que `v3` en muebles (falla segura) y algo peores que `v5-fix` por diseño.
- `caja_y_pared_al_lado`: `AYUDA` igual o 3 más que `v3`; `terminado_falso` no es cero.
- Datos crudos: [compuertas_final_n120_r01.txt](precision/v6/compuertas_final_n120_r01.txt) (+ [sumas](precision/v6/compuertas_final_n120_r01_sumas.txt)), [compuertas_final_n120_r2.txt](precision/v6/compuertas_final_n120_r2.txt), [ablacion_banderas_final_n120.txt](precision/v6/ablacion_banderas_final_n120.txt), [barrido_cuadros_n120.txt](precision/v6/barrido_cuadros_n120.txt), [pared_al_frente_n960.txt](precision/v6/pared_al_frente_n960.txt), [margen_guardia_l0.txt](precision/v6/margen_guardia_l0.txt).

## 4. Fase 3 — protocolo físico

[protocolo_prueba_fisica_v6.md](protocolo_prueba_fisica_v6.md) y [pruebas_fisicas_v6_plantilla.csv](pruebas_fisicas_v6_plantilla.csv): A/B de `v3` contra `v6` en el mismo orden de menor a mayor riesgo, con qué interruptor en cada corrida, criterios de aceptación y de aborto, mano en el interruptor, comandos `adb` **sin ejecutar** y detenerse sin desinstalar si falla la firma.

## 5. Fase 4 — congelamiento y evaluación ciega

(se completa al congelar)

## 6. Lo que falló o no se pudo hacer (sin adornos)

- **Las compuertas estrictas no se cumplen** con ruido realista (G1, G2, G5, G6 y G7 en el peor caso; G3 y G4 con aviso de falla segura). Detalle por compuerta, por matriz y con las diferencias que caben en el ruido estadístico en [autonomo_v6_diseno.md](autonomo_v6_diseno.md), sección 6.
- v6 **no iguala a v3 en piso limpio** con ruido realista (más esquives y más `AYUDA` en sol, sombra y reflejo; en manchas oscuras duras también sin ruido): es la compensación inevitable por cubrir los objetos oscuros sin eco y los muebles de patas.
- `terminado_falso` de `silla_patas`: sigue (límite heredado de detección).
- El contacto residual de `pared_al_frente` con láser pesimista: **3 en 7680 corridas** (`v3` 68, `v5-fix` 59), los tres en RESERVADO con ruido 0; la guardia llegó una lectura de láser (150 ms) tarde. No se subió su margen (60 mm) porque cuesta `AYUDA` en `G5` ([precision/v6/pared_al_frente_n960.txt](precision/v6/pared_al_frente_n960.txt), [precision/v6/margen_guardia_l0.txt](precision/v6/margen_guardia_l0.txt)).
- **Nada de lo anterior está medido en el carro.** La calibración del banco cubre el ruido de la visión (7 cajas negras de v3, piso limpio); no cubre sol, sombras, reflejos, objetos ni muebles reales.
- Propuestas de firmware (no implementadas, solo propuesta): ver [autonomo_v6_diseno.md](autonomo_v6_diseno.md), sección 7.
