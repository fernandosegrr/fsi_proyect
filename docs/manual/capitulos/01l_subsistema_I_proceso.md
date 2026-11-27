# Subsistema I · Proceso de desarrollo

## I.1 Qué es y para qué sirve

Este capítulo cuenta cómo se construyó el carro: qué versiones hubo, en qué fechas, cómo se probó cada una y por qué algunas se descartaron. Sirve para contestar «¿cómo lo hicieron?» con evidencia y sin inflar nada. Lo validado en el carro es lo de la v1.1 (cierre del 10-nov-2026); el autónomo v3 (base de la v1.2) y las versiones v4, v5 y v6 solo se probaron en simulador: **SIMULADO**.

Más detalle: capítulo 14 del manual (`docs/manual/capitulos/14_desarrollo.md`) y `docs/historia.md`.

## I.2 Cómo funciona, paso a paso

Cada versión nueva siguió el mismo ciclo. Los pasos 3 a 5 son el banco simulado (subsistema H); el paso 2 usa el firmware y el núcleo en C (subsistemas B y E).

1. **Investigar y escribir el diseño.** Antes del código se leen hojas de datos y artículos y se anotan los supuestos: `docs/investigacion_autonomo/` (11 temas, del láser a la seguridad funcional) y los documentos de diseño, como `docs/autonomo_v2_diseno.md`.
2. **Programar en C y probar en la PC.** La lógica del carro (`firmware_carro/carro.c`) y la del navegador (`nucleo_c/`) se prueban con hardware falso, sin placa ni teléfono (`pruebas_firmware/`, `make -C nucleo_c test`).
3. **Medir en el banco simulado.** `nucleo_c/pruebas_autonomo_v2.c` y `mundo_v2.c` corren miles de rutas por escenario con semillas fijas (`170000 + índice × 10007 + corrida`). Dos versiones se comparan con las mismas situaciones.
4. **Fijar compuertas antes de mirar.** Las compuertas dicen qué debe cumplirse (en v4 eran seis metas; desde v5, G1 a G7). Son estrictas: un solo caso malo basta para «NO CUMPLE».
5. **Dejar cada cambio apagable.** Cada mejora lleva una bandera. Apagada, la versión debe reproducir a la anterior línea por línea; en v4, v5 y v6 se comprobó con el banco.
6. **Revisar y analizar.** Hubo revisiones independientes de código y de seguridad, y en Linux (GitHub Actions) corren ASan, UBSan y TSan (`.github/workflows/sanitizadores.yml`).
7. **Probar en el carro, de menor a mayor riesgo.** `docs/protocolo_prueba_fisica_autonomo_v2.md` ordena la sesión en bloques A a D y mediciones M1 a M10, y dice cuándo abortar.
8. **Congelar y evaluar a ciegas.** Se pone un tag `freeze/*`; después se generan semillas nuevas, se corre una sola ronda y la salida se publica sin editar.
9. **Decidir y dejar rastro.** Se adopta o se archiva con tag. Todo queda en las bitácoras (`docs/progreso_*.md`, `docs/estado_*.md`, `docs/bitacora.md`) con su etiqueta de evidencia.

Si el paso 7 no se ha hecho, la versión sigue en **SIMULADO**. Hoy es el caso de la v3.

```
 v1.0 ───▶ v1.1 ───▶ v2 ───▶ v3 ──────────────────────▶ v1.2 = v3 + UI
 27-oct    10-nov   14-nov  18-nov                         24-nov
 carro     carro    sim.    sim.      │                    sim.
                                      ├──▶ v4 (18-nov)
                                      │
                                      └──▶ v5-carbon ──▶ v5-carbon-fix ──▶ v6
                                           23-nov         24-nov           24-nov
                       v4, v5-carbon, v5-carbon-fix y v6: probadas y descartadas
```

### Línea de tiempo

| Fecha (2026) | Versión o rama | Qué se hizo | Etiqueta |
|---|---|---|---|
| 10 a 16 oct | J1 a J10 | El sketch monolítico pasa a módulos en C99 con pruebas en PC; nacen `nucleo_c` y la app | **SIMULADO** |
| 17 a 21 oct | J11 a J13 | Giroscopio, láser y giro por ángulo; primeros giros y lecturas con el carro | **VALIDADO EN EL CARRO** (firmware previo a v1.0) |
| 27 oct | J14, tag `v1.0` (`eb88151`) | Versión 1.0 y pruebas B0 a B6 en pista; B2, B7, B8 y B9 no se hicieron | **VALIDADO EN EL CARRO** (v1.0) |
| 27 oct | J15 | Pose con el rumbo medido: error lateral de 13.2 a 1.9 cm en 400 rutas simuladas | **SIMULADO** |
| 31 oct a 10 nov | J16, tag `v1.1` (`bd1727b`) | `RUMBO`, `IMU_ESCALA` y aviso `WD:<n>`; B6 validado en el carro el 10-nov | **VALIDADO EN EL CARRO** (v1.1) |
| 11 a 14 nov | v2, `feat/autonomo-v2` | Rodeo medido con el láser, `SEG=2`, banco con mundo sintético, caja negra por corrida | **SIMULADO** |
| 15 nov | Firmware v1.1 | Siete rectas con el carro; sirvieron para calibrar el banco | **VALIDADO EN EL CARRO** (app sin versión anotada) |
| 16 a 18 nov | v3, `feat/autonomo-v3` (`e37f77b`) | Memoria del eco, retroceso antes de girar, causa 19, `SEG=3` solo compilado, investigación | **SIMULADO** |
| 18 nov | v4, tag `archive/feat-autonomo-v4-a200df4` | Guardia por percepción incierta con consulta `TED`; archivada | **SIMULADO** |
| 23 nov | v5-carbon, tag `freeze/v5-carbon` | Detector de masas sólidas y de patas; primera evaluación ciega | **SIMULADO** |
| 24 nov | v5-carbon-fix y v6, tags `freeze/*` | Retención de 500 a 200 ms; v6 con tres banderas; dos evaluaciones ciegas más | **SIMULADO** |
| 24 nov | `fix/ui-android-parpadeos` y `release/v1.2` (`6c849ce`) | Revisión de la interfaz de la app, pasada a la base v3 | **SIMULADO** (carro y cámara falsos) |
| 1 dic | Exposición | Presentación ante el Dr. Rostro González | **PENDIENTE** |

## I.3 Cómo lo hicimos

- **Del sketch monolítico a C probado en PC.** Al inicio todo vivía en un archivo de Arduino (red, servidor y pines): no se podía probar sin el carro ni reproducir fallos de conexión. Se separó la lógica en C99, con funciones `hw_*` para el hardware, y nació `pruebas_firmware/` (`docs/historia.md` §1 y §2).
- **Alternativas descartadas desde el inicio** (`docs/historia.md` §3): encoders ópticos (holguras y cableado frágil; se usó el giroscopio), visión en la nube o en una PC (latencias de más de 300 ms; todo corre en el teléfono), paro de emergencia solo por visión (se usa el láser), mapa SLAM (demasiada memoria; política reactiva) y arranque suave (los motores zumbaban; golpe de 255 durante 250 ms, `ARRANQUE_PLENO_MS`).
- **Probado y descartado en J16 y v2** (**SIMULADO**): descontar el retraso del video y el frenado en la cara del obstáculo (empeoraba el esquive), corregir la velocidad con el láser (pide un objeto quieto a menos de 1.2 m), «coincidencia con memoria» (23 esquives por cada 200 corridas con manchas de sol) y deshacer el giro (con pared pegada, de 615 a 534 llegadas de 1,000) (`docs/progreso_J16.md`, `docs/autonomo_v2_diseno.md` §4).
- **Un banco calibrado con el carro cambió el rumbo.** El «G1 cumple» de v5 salió de un banco con visión casi muda en piso limpio. Calibrado con las 7 cajas negras del 15-nov, v5-carbon-fix resultó peor que v3 en piso limpio (`docs/autonomo_v6_diseno.md` §2, rama `feat/autonomo-v6`).
- **La evaluación ciega encontró lo que el banco de desarrollo no.** En v5-carbon, una detección falsa de 350 ms más una retención de 500 ms sostuvo 850 ms y provocó un esquive falso en `falso_reflejo`. De ahí salió v5-carbon-fix, con retención de 200 ms (`docs/progreso_v5.md`, rama `feat/autonomo-v5-carbon-fix`).

Lo probado y descartado entre el 18 y el 24 de noviembre:

| Qué se probó | Por qué no se usó | Dónde queda |
|---|---|---|
| v4: guardia con consulta `TED` | Bajó los contactos simulados, pero 94 de 98 combinaciones quedaron con una compuerta en rojo (con v3, 86) y subieron las AYUDA: 31 de 40 en ruta libre. Texto del reporte: «Conservar v3 para la exposición» (`docs/reporte_v4.txt`) | Tag `archive/feat-autonomo-v4-a200df4` |
| v5-carbon: masas sólidas, patas y retención | A ciegas bajó los contactos (238 a 60 con láser normal; 385 a 70 con pesimista), pero solo G3 cumplió; hubo un esquive falso nuevo y más AYUDA con muebles | Tag `freeze/v5-carbon`; rama `feat/autonomo-v5-carbon` |
| v5-carbon-fix: retención de 200 ms | Nueva ronda ciega: contactos de 228 a 74 y de 362 a 83; solo G7 cumplió. G1 falló por una parada de más en `falso_sombra` (6 a 7) y G3 por 41 contactos en `bloque_sin_eco` (tope: 29) | Tag `freeze/v5-carbon-fix`; rama `feat/autonomo-v5-carbon-fix` |
| v6: tres banderas sobre v5-fix | G1 y G7 no cumplen en las 12 matrices; en piso limpio con ruido calibrado es peor que v3 (2 AYUDA en 360 corridas contra 0). Ganó en `pared_al_frente` con láser pesimista | Tag `freeze/v6`; rama `feat/autonomo-v6` |

- **Por qué v3 queda como base.** Ninguna versión nueva cumplió las compuertas a ciegas. Cubrir objetos oscuros sin eco y muebles de patas cuesta más AYUDA y paradas falsas en piso limpio, y ahí v3 salió mejor que v5-fix y v6 (`docs/progreso_v6.md` §6, rama `feat/autonomo-v6`). El reporte de v4 deja la decisión por escrito; contra v5-carbon y v5-carbon-fix no hay decisión escrita aparte: se decidió tras la evaluación ciega y el reporte de v6 con ruido calibrado.
- **Revisión de la interfaz (v1.2).** Se pidió revisar los parpadeos de la app. La rama `fix/ui-android-parpadeos` salió de la punta de v6; su único commit (`21c1b2d`) pasó a `release/v1.2` con `git cherry-pick -x` (`6c849ce`). Solo cambia interfaz y texto: no toca `nucleo_c/`, el firmware ni la cámara. `docs/revision_ui_android.md` lista 12 hallazgos (P1 a P12) y 8 observaciones sin cambio (N1 a N8).
- **Las compuertas G1 a G7 de este capítulo no tienen relación con el subsistema G (seguridad).**
- **Versión de la app en las 7 rectas del 15-nov:** no registrado por el equipo (la caja negra no la anota).
- **Operación en pruebas físicas:** Jaime operó el carro en las pruebas físicas (y lo operará en la demostración).

## I.4 Cómo lo probamos y con qué resultado

| Qué se probó | Cómo | Resultado | Etiqueta |
|---|---|---|---|
| v1.0 en pista | Bloques B0 a B6, 27-oct (`docs/reporte_J14.md`) | 28 de 28 giros dentro de ±3°; 4 de 4 rectas terminadas; en B6 (caja a ~100 cm) ningún rodeo limpio y una corrida quizá tocó la caja | **VALIDADO EN EL CARRO** (v1.0) |
| Esquive B6 | 6 corridas, caja de 20 cm a ~100 cm, 10-nov (`docs/estado_20261110.md`) | 6 de 6 sin tocar la caja; 5 de 6 completas, error lateral de 0 a 10 cm; 1 pasó por AYUDA | **VALIDADO EN EL CARRO** (v1.1) |
| Siete rectas | Rutas de 150 cm, 15-nov (`docs/pruebas_fisicas_20261114.csv`, archivo local sin versionar) | 6 terminaron y 1 acabó en ERROR por perder el video; 0 contactos | **VALIDADO EN EL CARRO** (firmware v1.1; app sin versión anotada) |
| Pruebas de PC de la v3 | `make -C nucleo_c test` y `make -C pruebas_firmware test`, 16-nov (`docs/estado_20261116.md`) | 1,901 casos en 24 ejecutables y 567 casos en 5; 0 fallos | **SIMULADO** (v3) |
| v4 en el banco | 11,760 corridas pareadas con v1.1 y v3 (`docs/reporte_v4.txt`) | 94 de 98 combinaciones con una compuerta en rojo (con v3, 86) | **SIMULADO** |
| v5-carbon a ciegas | 3 semillas, 120 corridas por escenario y por modelo de láser | Contactos de 238 a 60 y de 385 a 70; solo G3 cumple | **SIMULADO** |
| v5-carbon-fix a ciegas | 3 semillas nuevas, mismo diseño | Contactos de 228 a 74 y de 362 a 83; solo G7 cumple | **SIMULADO** |
| v6 a ciegas | 3 semillas, 4 matrices cada una | G1 y G7 no cumplen en 12 de 12; `pared_al_frente` con láser pesimista: 7, 9 y 0 contactos (v3, v5-fix, v6) | **SIMULADO** |
| Interfaz de la app | Teléfono Android 10 con carro y cámara falsos, 24-nov (`docs/revision_ui_android.md`) | Cuadros con pasada de «medir y acomodar» de más de 4 ms: de 19.2 % a 3.3 % o menos; 24 aperturas de pantalla sin cierres | **SIMULADO** (carro y cámara falsos) |

Los resultados de v5 y v6 salen de `docs/evaluacion_ciega_resultados.txt`, `docs/evaluacion_ciega_resultados_fix.txt` y `docs/evaluacion_ciega_resultados_v6.txt`, que existen en sus ramas. La prueba de la interfaz también sirvió para algo más: encontró un cierre de la app (Manual en horizontal) que la lectura del código no vio (P9).

**Lo que NO se ha medido:**

- La v3 y las ramas v4 a v6 con el carro. El protocolo `docs/protocolo_prueba_fisica_autonomo_v2.md` (bloques A a D y M1 a M10) dice: «Nada de esto se ha ejecutado». **NO MEDIDO**.
- La v1.2 instalada en un teléfono y la interfaz con el carro y la cámara reales. La prueba del 24-nov se hizo con la rama de v6; los archivos tocados son idénticos entre v3 y v6, pero la v1.2 solo se compiló. **PENDIENTE**.
- Si el banco se parece al piso: se calibró con 7 rectas de piso limpio, sin sol, sombras, objetos ni muebles reales (`docs/progreso_v6.md` §6).
- Con la v1.1, según `docs/README.md` (10-nov): el corte de WiFi en marcha, el freno láser aislado y el consumo en amperios.

## I.5 Límites conocidos

- **De v3 a v6 todo es simulador.** El carro no se movió con ninguna. «Descartada» quiere decir que no cumplió las compuertas del banco, no que fallara en el piso.
- **El banco se calibró con poco.** Siete rectas en piso limpio. El color a la izquierda sigue subestimado: 35.5 % en el carro contra 5.7 % en el banco (`docs/autonomo_v6_diseno.md` §2).
- **Las compuertas son criterios del proyecto y son estrictas.** Una parada de más (6 a 7) bastó para un «NO CUMPLE». A cambio, evitan declarar como mejora lo que cabe en el ruido.
- **Posible sobreajuste.** En v6, la diferencia contra v3 fue mayor con semillas ciegas que en los conjuntos DESARROLLO y RESERVADO (`docs/progreso_v6.md`).
- **Lo más intentado es lo menos conocido.** Patas finas y objetos oscuros sin eco: el láser frente a patas reales no se ha medido (M2) y v4 a v6 no cumplieron sus compuertas.
- **«Continuar» tras una AYUDA sin mover el carro.** Una revisión independiente encontró el defecto; sigue en la v1.2 (v6 solo lo cerraba en parte). Regla: mover el carro con la cruceta antes de «Continuar» (subsistema G).
- **Poca evidencia física.** De la v1.1 hay 6 corridas de B6 y 7 rectas con luz de día sin focos.
- **Interfaz.** Queda el botón «atrás» de Manual en horizontal, que se corta 42 px en ese teléfono (N8), y siete observaciones más sin cambio.
- **Los documentos están repartidos.** Los de v4 a v6 solo existen en sus ramas y tags: se leen con `git show <ref>:<ruta>`. Las fechas de las jornadas J1 a J16 salen de las fechas de los commits.

## I.6 Términos clave

- **Jornada (J1 a J16)**: bloque numerado de trabajo de la v1.0 y la v1.1 (`docs/historia.md`).
- **Banco simulado**: programa en C que corre miles de rutas con obstáculos falsos (`nucleo_c/pruebas_autonomo_v2.c`).
- **Semilla**: número que fija el azar de una corrida para repetirla igual.
- **DESARROLLO y RESERVADO**: dos conjuntos de escenarios del banco (v4 a v6); el segundo solo se abre al cerrar cada etapa.
- **Compuerta**: condición que una versión debe cumplir en el banco; seis metas en v4 y G1 a G7 desde v5.
- **Evaluación ciega**: se congela el código, se generan semillas nuevas y se corre una ronda sin ajustar nada después.
- **`freeze/*`**: tag del código congelado antes de una evaluación ciega (`freeze/v5-carbon`, `freeze/v5-carbon-fix`, `freeze/v6`).
- **`archive/*`**: tag de una rama que se guarda y no se usa (`archive/feat-autonomo-v4-a200df4`).
- **Bandera**: interruptor de código que activa un cambio; apagada, la versión se comporta como la anterior.
- **Caja negra**: archivo de texto que la app guarda por corrida (`CajaNegra.kt`); no se versiona (`cajas_negras/` está en `.gitignore`).
- **Protocolo físico**: guía para probar en el carro por bloques A a D y mediciones M1 a M10.
- **Etiquetas de evidencia**: **VALIDADO EN EL CARRO**, **SIMULADO**, **NO MEDIDO** y **PENDIENTE**.

## I.7 Preguntas probables del profesor

1. **¿Cómo saben que lo que dicen que funciona, funciona?** Cada resultado lleva una etiqueta de evidencia. Lo validado en el carro es lo de la v1.1: B6 con 6 corridas el 10-nov y 7 rectas el 15-nov. Del autónomo v3 solo hay simulador y pruebas de PC; su protocolo físico no se ha ejecutado.
2. **¿Por qué la v1.2 usa la v3 y no la v6, que es más nueva?** Porque ninguna versión nueva cumplió las compuertas a ciegas. v4 subía las AYUDA, v5 y v5-fix bajaron contactos pero fallaron compuertas, y v6 es peor que v3 en piso limpio. Todo eso es simulador: ninguna se probó en el carro.
3. **¿Por qué simulan en vez de probar siempre en el carro?** En el banco se repiten mil rutas por escenario con la misma semilla; en el carro cada corrida necesita una persona, piso y pilas. Pero el banco se calibró con 7 rectas, así que sirve para comparar versiones y no para prometer resultados en el piso.
4. **¿Qué cambió en la app para la v1.2?** Solo la interfaz: se quitaron redibujos y saltos. En un teléfono con carro y cámara falsos, los cuadros lentos pasaron de 19.2 % a 3.3 % o menos. No se probó con el carro real, y la prueba fue con la rama de v6; la v1.2 solo se compiló.
5. **¿Qué salió mal en el camino?** El esquive de J14 (27-oct) no hizo un rodeo limpio; se corrigió en J15 y J16 y se validó el 10-nov. En v5, el banco sin ruido daba un «G1 cumple» que no se sostuvo al calibrarlo con el carro. Y la prueba en el teléfono encontró un cierre de la app.
