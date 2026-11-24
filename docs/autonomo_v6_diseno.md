# Autónomo v6 — diseño, diagnóstico y verificación

Rama `feat/autonomo-v6`, partiendo de la etiqueta `freeze/v5-carbon-fix` (`606c9f9`). **SIMULADO / NO MEDIDO EN EL CARRO.** Nada de `v3`, `v5` ni `v6` está validado en el carro real; todo número de este documento sale del banco de `nucleo_c` (`pruebas_autonomo_v2.c` + `mundo_v2.c`). `main` sigue en `bd1727b`; `feat/autonomo-v3`, `feat/autonomo-v5-carbon`, `feat/autonomo-v5-carbon-fix` y sus etiquetas no se tocaron.

Para el protocolo del carro: [protocolo_prueba_fisica_v6.md](protocolo_prueba_fisica_v6.md). Para la bitácora de la fase: [progreso_v6.md](progreso_v6.md). Datos crudos: `docs/precision/v6/`.

---

## 1. Qué es v6, en pocas líneas

- `v6` = `v5-carbon-fix` + **tres banderas del navegador**, todas apagadas por defecto (`NAV_V6_F_*` en `navegador.h`). **No toca** el firmware, los registros de la cámara, `TOF_PARO`, el watchdog de 1000 ms, STOP/EBRAKE, ni las capas de color y de fusión (`obstaculos.c`, `fusion.c`).
- **Arranque seguro:** la app arranca con v5 y v6 **apagados** = comportamiento de `v3`. Un solo toque en **Más → Sensores** enciende v6 (que incluye v5) para la comparación A/B. La caja negra de cada corrida anota la versión que de verdad corrió (`v5=0 v6=0 version=v3`, `v5=1 v6=1 version=v6`).
- **Equivalencias exactas (SIMULADO, comparadas línea por línea del banco en 16 matrices = 32 comparaciones, todas idénticas):** con las banderas de v6 apagadas el navegador reproduce **`v5-carbon-fix` exacto**; con v5 apagado reproduce **`v3` exacto**, aunque v6 esté pedido (`navegador_poner_v5(0)` apaga también v6). Salida cruda en [precision/v6/equivalencias_apagado.txt](precision/v6/equivalencias_apagado.txt) (`python scripts/compuertas_v6.py --apagado`).

| bandera | qué hace | dónde |
|---|---|---|
| `GUARDIA_VOLVER` (0x01) | La guardia del giro (láser dentro del arco que barre el chasis) vale también en el giro de **regreso** a la línea, con 60 mm de margen; y tras enderezarse no avanza en paralelo si el láser ve algo a 250 mm o menos al frente | `tick_esquivando` (fases `VOLVER` y `PAUSA_PASAR`) |
| `RACHA_ESTRICTA` (0x02) | La racha vertical de **un** lado ya no cuenta como racha del centro porque la red marque otra zona; la red tiene que estar en el centro | `registrar_cuadro` |
| `LATERAL_PERSISTENTE` (0x04) | Una masa sólida de color a un lado necesita 6 cuadros seguidos (el centro sigue con 2) | `registrar_cuadro` |

---

## 2. El simulador cambió antes que el navegador (y eso cambió el rumbo)

Las conclusiones de v5 (`G1: CUMPLE`) salieron de un banco cuya visión en piso limpio era casi muda. Las únicas mediciones del carro sobre visión son las 7 cajas negras del 15-nov-2026 (recta de 150 cm, piso limpio, **versión v3**): el carro real ve bastante más ruido. No hay mediciones físicas más nuevas que las del 14/15-nov (revisado el 24-nov; se vuelve a revisar justo antes de congelar). Solo se publican **agregados**; los CSV y las cajas negras no se publican.

`AV6_RUIDO=1` calibra el banco contra esas cajas negras (estructura oscura a la izquierda del carril en el 70 % de las corridas; ruido de la red con colas largas). `AV6_RUIDO=2` agrega marcas planas de color en el piso: **eso no sale de ninguna medición**, es un esfuerzo extra que pide el encargo. Detalle y límites de la calibración (el color a la izquierda sigue subestimado: 35.5 % real contra 5.7 %) en [precision/v6/calibracion_v6.md](precision/v6/calibracion_v6.md).

**Hallazgo:** con el ruido calibrado, `v5-carbon-fix` es **peor que `v3` en piso limpio** (más esquives, más `AYUDA`, más paradas), aunque con el banco heredado fueran idénticos. La masa sólida de color sin eco, las rachas verticales y la supresión del vistazo toman como obstáculo lo que `v3` ignoraba, porque saltan la exigencia de que color **y** red coincidan. Esto contradice el `G1: CUMPLE` de la fase anterior, que **era cierto solo con el banco sin ruido**. Atribución por bits de v5: [precision/v6/ablacion_bits_v5_dev_l0.txt](precision/v6/ablacion_bits_v5_dev_l0.txt).

---

## 3. Diagnóstico de los huecos

### 3.1 `pared_al_frente` con láser pesimista: el contacto de más frente a `v3`

Una pared larga cierra el paso; lo correcto es frenar sin tocar y pedir ayuda. Con el láser pesimista (pierde ecos lejanos) `v3` y `v5-fix` rozaban la pared de 3 a 5 veces por cada 240 corridas.

**Mecanismo (traza con `depurar`, semilla 4490337):** el carro frena, gira 90° a la izquierda y el barrido dice «lado libre» porque una pared larga parece terminar cuando el láser pierde ecos; avanza de lado, y el giro de **regreso** (`VOLVER`, 90° a la derecha) apunta de nuevo al frente y la esquina del chasis barre la pared. La guardia del giro existente solo protegía el **primer** giro.

**Arreglo (`GUARDIA_VOLVER`):** la misma guardia, con 60 mm de margen, en el giro de regreso; si el láser (fresco y válido) ya ve algo dentro del arco, deja de girar, frena y pide ayuda (`Causa 18`). Y tras enderezarse (`PAUSA_PASAR`), con una lectura fresca y válida a 250 mm o menos, no avanza en paralelo (`Causa 16`).

**Verificación (SIMULADO, código final, 960 corridas por celda; [precision/v6/pared_al_frente_n960.txt](precision/v6/pared_al_frente_n960.txt)):** suma de contactos de `pared_al_frente` en las 8 celdas (DESARROLLO y RESERVADO × ruido 0 y 1 × láser normal y pesimista, 7680 corridas): `v3` 68, `v5-fix` 59, `v6` 3. Con el láser pesimista y el ruido heredado: DESARROLLO `v3` 17, `v5-fix` 17, `v6` 0; RESERVADO `v3` 14, `v5-fix` 17, `v6` 3. Con láser normal: `v3` 1/2/1/6 y `v6` 0/0/0/0 (DES r0, RES r0, DES r1, RES r1). Prueba unitaria con el láser fresco, válido, viejo, sin eco, a la izquierda y a la derecha (`pruebas_esquive_confirmado.c`).

**Residual honesto:** quedan **3 contactos en 7680** (todos en RESERVADO, ruido 0, láser pesimista: corridas 204, 352 y 768 de 960). Traza de la 204 (semilla 4490337, `depurar 19 204` con `AV5_CONJUNTO=2 AV5_LASER=1`): en el giro de regreso, a los 6100 ms el láser marcaba 276 mm al frente y la esquina del chasis ya tocaba la pared oblicua (`CONTACTO`); la lectura siguiente, 150 ms después, marcó 135 mm (umbral de la guardia en ese instante: 153 mm) y la guardia pidió `AYUDA` (`Causa 18`), pero el contacto ya había ocurrido. La guardia llegó **una lectura de láser tarde**: el láser mira al frente y la pared está oblicua mientras el carro gira. Con margen de 60 mm quedan 3; con 120 mm, 2; con 180 mm, 0, pero cada margen extra pide `AYUDA` en `caja_y_pared_al_lado` y pasillos que `v3` completaba (RESERVADO, ruido 0: 55, 74 y 162 `AYUDA` en `G5`; [precision/v6/margen_guardia_l0.txt](precision/v6/margen_guardia_l0.txt)). Se dejó en 60 mm. **Residual no corregido, no verificado en el carro; el láser real puede tener otra cadencia y otra geometría de la pared.**

### 3.2 `terminado_falso` en `silla_patas`

`terminado_falso = llegó y no pasó el objeto`. En `silla_patas` quedan corridas en las que **ni la cámara ni el láser ven la silla** (patas finas), el carro avanza por debajo (`bajo_mueble=1`, sin esquive ni ayuda) y la ruta termina 40 mm antes de librar la silla porque la velocidad aleatoria del simulador es ~20 % menor que la calibrada.

Son **cifras casi iguales con las tres versiones** (por 480 corridas, ruido 0, láser normal y pesimista dan lo mismo: RESERVADO `v3` 4, `v5-fix` 3, `v6` 4; DESARROLLO `v3` 1, `v5-fix` 2, `v6` 2): no es un error de terminación sino un límite heredado de detección; el estado final es `TERMINADO` sin contacto. Con las compuertas finales (120 corridas) es 1 por versión en RESERVADO con ruido 0 y 0 en el resto de las matrices. Arreglarlo exigiría detectar la silla, que es justo lo que el trozo de sus patas no permite sin pagar con falsos positivos en piso limpio. **No se corrigió (G6 NO CUMPLE, heredado).**

### 3.3 El objeto oscuro sin eco y la mancha oscura plana son lo mismo para el carro

Ni el láser (que no recibe eco de ninguno) ni una cámara de 24×17 celdas distinguen un objeto oscuro de 3 cm del carril de una mancha plana oscura exactamente sobre el carril (`se_charco_oscuro`). Los rasgos de imagen (tamaño, color, brillo, contacto con el borde, solape con el carril) se **traslapan** entre los escenarios falsos y los verdaderos. Por eso v6 es **un punto en una curva de compensación** entre cubrir objetos oscuros sin eco (G3, G4) y no esquivar manchas (G1), no una mejora gratuita. `se_charco_oscuro` se informa como esfuerzo, no como compuerta.

### 3.4 Por qué se descartó seguir tocando la capa de color

El detector de masa de v5 se ajustó contra `bloque_sin_eco` (riesgo de sobreajuste). Por eso se armó la **familia sin eco** (12 escenarios: otros colores, texturas, tamaños, posiciones angulares, pisos y manchas; [precision/v6/familia_sin_eco.md](precision/v6/familia_sin_eco.md)) y los dos conjuntos separados (`DESARROLLO` y `RESERVADO`). Todo lo que sube en uno tiene que valer en el otro.

### 3.5 `falso_sombra`: una parada de más

El criterio estricto de G1 marca `falso_sombra` en `DESARROLLO` con ruido 0 (120 corridas): `v3` 7 paradas, `v5-fix` y `v6` 8. Con 1000 corridas por celda y el código final (mismo banco, `AV6_ESC=falso_sombra`): `DESARROLLO` `v3` 49, `v5-fix` 52, `v6` 52; `RESERVADO` 62, 63 y 63; en las tres versiones y en las cuatro celdas (láser normal y pesimista) **0 contactos, 0 `AYUDA` y 0 esquives**.

Son frenos de precaución momentáneos del estado `SIGUIENDO` ante un cuadro aislado que marca una sombra; el carro reanuda solo. La diferencia (3 sobre ~50; z ≈ 0.3 si se tratan como conteos independientes) cae dentro del ruido: el simulador sortea la percepción con una semilla por corrida y cualquier cambio que adelanta o atrasa un cuadro (aquí, la supresión del vistazo de v5) cambia el resto del hilo aleatorio, así que corridas individuales divergen aunque el mecanismo sea el mismo. **No hay un mecanismo que corregir y no se tocó**, pero el criterio estricto sigue marcando `G1 NO CUMPLE` en esas celdas (§6): una parada de más en 120 corridas, explicada, no corregida.

### 3.6 Dónde ocurren los contactos que quedan (censo; explica por qué G2 no llega a cero)

[precision/v6/censo_contactos_g2_v6.txt](precision/v6/censo_contactos_g2_v6.txt) lista, con el código final de `v6`, el estado del navegador y el comando en cada contacto de los 11 escenarios de sólidos (120 corridas por escenario, 8 celdas):

- **Con ruido 0** quedan pocos: 2 (`DESARROLLO`, láser normal) y 1 (`RESERVADO`), casi siempre un giro de regreso (`REINCORPORANDO`) pegado al final de `objeto_largo90`; con láser pesimista 11 y 10, **19 de los 21 en el primer giro del rodeo** de `objeto_bajo` y `bajo_solo_color` (objetos bajos que solo ve el color y que el láser pesimista pierde; los otros 2 son `objeto_largo90`).
- **Con ruido 1** el total sube a 31–54 y la mayoría ocurre **fuera del rodeo**: `SIGUIENDO` con un giro de la ruta hacia el objeto (6 y 26 con láser normal, 17 y 26 con pesimista; `DESARROLLO` y `RESERVADO`) y `REINCORPORANDO` (11, 6, 11 y 6). Un ejemplo trazado (`objeto_largo90`, `RESERVADO`, ruido 1, corrida 36, `depurar 10 36`): el rodeo se interrumpe en el paso en paralelo (`BLOQUEA_se_detiene`), el navegador vuelve a `SIGUIENDO` y la ruta pide un giro de 135° a la derecha con el objeto a 27 cm; la esquina del chasis lo toca a medio giro.
- **Por qué `v6` no lo corrige:** la guardia del giro (láser dentro del arco que barre el chasis) solo existe en el primer giro del rodeo y, con `GUARDIA_VOLVER`, en el giro de regreso del rodeo; no cubre los giros de la ruta ni los de `REINCORPORANDO`. Extenderla sería **una guardia nueva**, fuera del alcance acordado para v6, y el láser apunta al frente: ve la esquina que barre un objeto lateral una lectura tarde (§3.1). Queda como propuesta (§7), **sin implementar ni probar**.

---

## 4. Diseño de cada bandera: beneficio, riesgo y cómo se verifica

### 4.1 `GUARDIA_VOLVER`

- **Beneficio:** `pared_al_frente` pasa de 68 contactos (`v3`) y 59 (`v5-fix`) a 3 en 7680 corridas (§3.1); en el banco ninguna celda queda peor que `v3`.
- **Riesgo:** pide `AYUDA` donde `v3` rodeaba con el láser leyendo algo cerca al frente. Es una falla segura, con `Causa 16/18` visibles en la app. Usa solo lecturas **frescas y válidas** del láser (`tof_fresco`, `tof_valida`, `tof_mm > 0`): sin láser no hace nada.
- **Verificación en simulación:** unitaria (`pruebas_esquive_confirmado.c`) y en el banco (G1: sin cambios medibles de `AYUDA` en piso limpio; G2/G5 con y sin láser pesimista).
- **En el carro (PENDIENTE):** `P4.1` y `P3.3`/`P3.4` del protocolo; se espera `AYUDA` antes del contacto.

### 4.2 `RACHA_ESTRICTA`

- **Beneficio:** menos esquives de más en piso limpio con ruido de la red: la red marca cajas inventadas en cualquier zona (el ruido cae en las tres) y v5 las tomaba como racha del centro si había una racha vertical de color a un lado. **Por sí sola casi no cambia nada** (ruido 1, láser normal, grupo G1: `DESARROLLO` 768/0/130 e596 → 769/2/130 e602; `RESERVADO` 751/2/130 e611 → 752/2/129 e602); su aporte aparece **sobre la persistencia lateral** (bits 4 → 6): `DESARROLLO` 786/0/117 e535 → 795/0/111 e507 y `RESERVADO` 774/0/113 e535 → 788/0/100 e479, es decir −6 y −13 `AYUDA` y −28 y −56 esquives.
- **Riesgo:** pierde parte de la cobertura de patas delgadas: sobre la persistencia lateral, los contactos de los 4 escenarios de muebles de `RESERVADO` suben de 49 a 73 (≈ +6 por escenario) y su `AYUDA` baja de 277 a 257; en `DESARROLLO` 33 → 32 contactos y 120 → 111 `AYUDA`; en la familia sin eco +7 y +6 contactos de 840. Se eligió por la prioridad de piso limpio.
- **Verificación:** unitaria (`pruebas_navegador.c`) y banco ([precision/v6/ablacion_banderas_final_n120.txt](precision/v6/ablacion_banderas_final_n120.txt): `v6_0` = `v5-fix` idéntico en todas las celdas).

### 4.3 `LATERAL_PERSISTENTE`

- **Beneficio:** una masa a un lado (estructura junto a la ruta, ruido) tiene que persistir 6 cuadros antes de armar la retención del centro. **Es la bandera que más compra de piso limpio:** sola (ruido 1, láser normal, grupo G1) contra `v5-fix`: `DESARROLLO` 768/0/130 e596 → 786/0/117 e535 (−13 `AYUDA`, −61 esquives) y `RESERVADO` 751/2/130 e611 → 774/0/113 e535 (−17, −76).
- **Riesgo:** retrasa 4 cuadros la reacción a una masa lateral real, solo mientras el carro sigue la ruta; el centro no cambia. Costo medido de la bandera sola contra `v5-fix`: contactos de muebles 25 → 33 (`DESARROLLO`) y 43 → 49 (`RESERVADO`), `bloque_sin_eco` 16 → 19 y 17 → 20, familia sin eco +9 y +6.
- **Barrido de `NAV_V6_LATERAL_CUADROS`** (v6 completo, código final, ruido 1, láser normal, 120 corridas; [precision/v6/barrido_cuadros_n120.txt](precision/v6/barrido_cuadros_n120.txt)). G1 `DESARROLLO` éxito/`AYUDA`/esquives: `v5-fix` 768/130/596; 2 → 769/130/601; 3 → 782/119/553; 4 → 788/117/530; **6 → 795/111/507**; 8 → 795/110/504. G1 `RESERVADO`: `v5-fix` 751/130/611; 2 → 752/129/602; 3 → 763/121/561; 4 → 777/110/515; **6 → 788/100/479**; 8 → 788/100/479. Contactos de muebles `DESARROLLO` (`v5-fix` 25): 22, 21, 26, **32**, 35; `RESERVADO` (`v5-fix` 43): 47, 53, 61, **73**, 88. Se eligió 6 porque en `RESERVADO` es el primer valor con el que G1 alcanza a `v3` en éxito y contactos (788/0 contra 788/0), y a partir de ahí no mejora (con 8 es igual) mientras los contactos de muebles siguen subiendo. **Es una elección sobre una curva, no un óptimo:** con 4 el piso limpio mejora a medias (`AYUDA` 110 contra 100 de `RESERVADO`) y los muebles cuestan 12 contactos menos (61 contra 73).
- **Verificación:** unitaria (2 contra 6 cuadros, izquierda y derecha, centro sin cambios) y banco.

---

## 5. Lo que se probó y se descartó (con la razón)

Todo con el banco calibrado (`AV6_RUIDO=1`), láser normal, `DESARROLLO` y `RESERVADO`. Cada intento se midió en piso limpio **y** en los escenarios de cobertura; si mejoraba uno y empeoraba el otro sin ganancia neta, se descartó.

| intento | resultado |
|---|---|
| Solo masas oscuras (`solo_oscuras`) | Pierde cobertura de sin eco; **no** mejora piso limpio. |
| Masa solo en el tercio central (`solo_central` / `masa_en_carril`) | Pierde el banquito (que cae casi entero en los tercios laterales: la cámara está a 115 mm y las tercias son más angostas que el carril cerca); no aporta nada sobre la persistencia. |
| Rachas delgadas (`rachas_delgadas`) | Sin efecto medible. |
| Retención lateral (`RETENCION_LATERAL`) | Sin ganancia más allá de la persistencia. |
| Persistencia también en el **centro** (3, 4 y 5 cuadros seguidos para la masa del carril, solo mientras sigue la ruta; [precision/v6/exp_persistencia_centro_n120.txt](precision/v6/exp_persistencia_centro_n120.txt)) | No mejora el piso limpio (`DESARROLLO`, ruido 1: −1 `AYUDA` y −2 esquives; `RESERVADO`: nada: los esquives de más ya no vienen de la masa del centro) y cuesta cobertura (`bloque_sin_eco` con ruido 0: 25 → 27, 36 y 36 contactos; familia sin eco hasta +13). Descartado, no está en el código. |
| Quitar cada sub-bandera de v5 | Pierde más cobertura de la que gana en piso limpio; solo apagar `coincidencia_estable` ayuda a piso limpio, pero cuesta paradas y G2: se dejó. |
| Subir el margen de la guardia del regreso para el contacto residual | Más `AYUDA` en rodeos que v3 completaba. |

La capa de color (`obstaculos.c`) quedó **igual que en `v5-carbon-fix`**: todos los intentos de esa capa se revirtieron.

---

## 6. Resultados contra las compuertas

**Cómo se midió.** `python scripts/compuertas_v6.py --corridas 120 --ruido 0,1` con el código final de las banderas (`d8877ab`): **16 matrices** = {`DESARROLLO` (24 escenarios), `RESERVADO` (27), familia sin eco de piso y objetos `FDES`/`FRES` (12)} × {ruido 0, ruido 1} × {láser normal, láser pesimista}, 120 corridas por escenario. Criterio **estricto**: cualquier métrica peor que la referencia cuenta, aunque sea un contacto o una parada. Salida cruda sin editar: [precision/v6/compuertas_final_n120_r01.txt](precision/v6/compuertas_final_n120_r01.txt); sumas por grupo: [precision/v6/compuertas_final_n120_r01_sumas.txt](precision/v6/compuertas_final_n120_r01_sumas.txt).

- **Ruido 0** = banco heredado (visión casi muda en piso limpio): con él se reproducen todas las conclusiones previas.
- **Ruido 1** = calibrado contra las 7 cajas negras reales del 15-nov (recta de 150 cm, piso limpio, versión v3); es lo más cercano al carro que hay, y aun así **solo cubre el ruido de la visión en piso limpio**: ni sol, ni sombras, ni reflejos, ni objetos ni muebles reales están medidos. El color a la izquierda sigue subestimado (35.5 % real contra 5.7 %).
- **Ruido 2** (§6.9) = esfuerzo con marcas planas en el piso que **no sale de ninguna medición**.

Cada celda de las sumas es **éxito/contactos/AYUDA**, `e` esquivas y `p` paradas, sumadas por grupo: G1 = 8 escenarios de piso limpio y falsos positivos (más `se_piso_manchas` y `se_reflejo_saturado` en la familia); G2 núcleo = caja 20×20, objeto largo, dos obstáculos y pared al frente (meta: 0 contactos); G3 = sin eco; G4 = muebles de patas; G5 = `caja_y_pared_al_lado` y pasillos. `v6` incluye `v5` completo más las tres banderas.

### 6.1 Veredicto estricto por matriz (SIMULADO)

| matriz | G1 | G2 | G3 | G4 | G5 | G6 | G7 |
|---|---|---|---|---|---|---|---|
| DES r0 L0 | NO CUMPLE | AVISO | AVISO | AVISO | NO CUMPLE | CUMPLE | NO CUMPLE |
| DES r0 L1 | NO CUMPLE | CUMPLE | AVISO | AVISO | NO CUMPLE | CUMPLE | NO CUMPLE |
| DES r1 L0 | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |
| DES r1 L1 | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |
| RES r0 L0 | CUMPLE | AVISO | AVISO | AVISO | CUMPLE | NO CUMPLE | NO CUMPLE |
| RES r0 L1 | CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |
| RES r1 L0 | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |
| RES r1 L1 | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |
| FDES r0 L0 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FDES r0 L1 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FDES r1 L0 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FDES r1 L1 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FRES r0 L0 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FRES r0 L1 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FRES r1 L0 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FRES r1 L1 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| **AGREGADO (peor caso)** | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |


**Ninguna compuerta estricta cumple en el peor caso** (renglón final): G1, G2, G5, G6 y G7 `NO CUMPLE`; G3 y G4 `AVISO` (cubren mucho mejor que `v3`, con costos y fallas seguras explicadas abajo). `CUMPLE` solo aparece en celdas sueltas con ruido 0: G1 en `RESERVADO` (ambos láseres), G2 en `DESARROLLO` con láser pesimista, G5 en `RESERVADO` con láser normal y G6 en `DESARROLLO` (y en las familias, que no tienen muebles). Esto **no es un resultado para el escaparate**: es el resultado.

### 6.2 Sumas por grupo (celdas representativas; el resto en el archivo de sumas)

```text
DES ruido=0 laser=normal
  G1 piso limpio y falsos  v3     960/  0/  0 e   0 p  27  v5fix  960/  0/  0 e   0 p  27  v6     960/  0/  0 e   0 p  27
  G2 solidos (nucleo)      v3     476/  1/123 e 546 p 483  v5fix  476/  1/123 e 545 p 482  v6     476/  1/123 e 545 p 482
  G3 sin eco               v3       0/120/  0 e   0 p  23  v5fix  100/ 20/  0 e 109 p 113  v6      95/ 25/  0 e 107 p 111
  G4 muebles               v3     100/ 48/ 39 e 178 p 207  v5fix  123/ 17/ 54 e 200 p 218  v6      98/ 36/ 42 e 177 p 215
  G5 espacios              v3     139/  1/100 e 266 p 241  v5fix  160/  1/ 80 e 278 p 242  v6     158/  1/ 82 e 278 p 242
DES ruido=1 laser=normal
  G1 piso limpio y falsos  v3     798/  0/101 e 440 p1089  v5fix  768/  0/130 e 596 p 869  v6     795/  0/111 e 507 p 820
  G2 solidos (nucleo)      v3     324/ 17/272 e1030 p 791  v5fix  320/  5/279 e1121 p 664  v6     320/  6/279 e1109 p 668
  G3 sin eco               v3      37/ 63/ 22 e 149 p 227  v5fix   63/ 16/ 44 e 318 p 234  v6      61/ 19/ 43 e 304 p 221
  G4 muebles               v3      56/ 58/ 90 e 354 p 351  v5fix   71/ 25/130 e 503 p 297  v6      55/ 32/111 e 411 p 283
  G5 espacios              v3      72/  5/167 e 377 p 323  v5fix   77/  8/161 e 444 p 305  v6      79/  9/158 e 444 p 311
RES ruido=0 laser=pesimista
  G1 piso limpio y falsos  v3     960/  0/  0 e   0 p  24  v5fix  960/  0/  0 e   0 p  24  v6     960/  0/  0 e   0 p  24
  G2 solidos (nucleo)      v3     476/  4/120 e 538 p 488  v5fix  476/  4/120 e 539 p 486  v6     478/  2/120 e 539 p 484
  G3 sin eco               v3       1/119/  5 e  90 p 115  v5fix   87/ 33/  2 e 116 p 130  v6      85/ 35/  2 e 117 p 130
  G4 muebles               v3     146/153/ 93 e 328 p 406  v5fix  268/ 34/121 e 477 p 467  v6     215/ 90/ 96 e 408 p 437
  G5 espacios              v3     281/  0/ 79 e 402 p 362  v5fix  296/  0/ 64 e 407 p 364  v6     293/  0/ 67 e 406 p 364
RES ruido=1 laser=normal
  G1 piso limpio y falsos  v3     788/  0/105 e 430 p 976  v5fix  751/  2/130 e 611 p 869  v6     788/  0/100 e 479 p 828
  G2 solidos (nucleo)      v3     333/ 23/263 e1022 p 783  v5fix  328/ 13/270 e1089 p 654  v6     327/ 15/271 e1078 p 646
  G3 sin eco               v3      31/ 70/ 43 e 211 p 369  v5fix   56/ 17/ 52 e 313 p 225  v6      56/ 18/ 49 e 306 p 228
  G4 muebles               v3     125/108/243 e 773 p 729  v5fix  162/ 43/276 e 975 p 649  v6     139/ 73/257 e 874 p 670
  G5 espacios              v3     155/ 11/204 e 624 p 512  v5fix  167/  6/192 e 741 p 476  v6     167/  5/192 e 723 p 475
FDES ruido=1 laser=normal
  G1 piso limpio y falsos  v3     164/  0/ 37 e 200 p 423  v5fix  152/  0/ 49 e 253 p 351  v6     157/  0/ 45 e 232 p 348
  G3 sin eco               v3     273/407/246 e1361 p1920  v5fix  422/ 93/363 e2149 p1429  v6     422/107/352 e2108 p1442
FRES ruido=1 laser=normal
  G1 piso limpio y falsos  v3     166/  0/ 37 e 200 p 487  v5fix  149/  0/ 56 e 272 p 371  v6     158/  0/ 42 e 229 p 345
  G3 sin eco               v3     271/420/243 e1311 p1945  v5fix  453/ 65/359 e2225 p1490  v6     451/ 77/345 e2181 p1490
```

### 6.3 G1, piso limpio y falsos positivos: `NO CUMPLE`

- **Ruido 0, `RESERVADO`:** `v6` = `v3` exacto (960/0/0, 24 paradas) con los dos láseres: `CUMPLE`.
- **Ruido 0, `DESARROLLO`:** idéntico a `v3` salvo `falso_sombra` con 8 paradas contra 7: `NO CUMPLE` por el criterio estricto; explicado en §3.5 (ruido estadístico, sin contactos, `AYUDA` ni esquives).
- **Ruido 1 (calibrado), láser normal, sumas del grupo:** `DESARROLLO` `v3` 798/0/101 e440 p1089, `v5-fix` 768/0/130 e596 p869, `v6` 795/0/111 e507 p820. `RESERVADO` `v3` 788/0/105 e430 p976, `v5-fix` 751/2/130 e611 p869, `v6` 788/0/100 e479 p828. Es decir, `v6` recupera entre el 57 % y el 73 % del exceso de esquives de `v5-fix`, y en `AYUDA` el 66 % (`DESARROLLO`) o todo el exceso (`RESERVADO`, 100 contra 105 de `v3`); el éxito iguala a `v3` en `RESERVADO` y queda 3 por debajo en `DESARROLLO`. **Pero sigue con más esquives que `v3` (+67 y +49) y, en `DESARROLLO`, 10 `AYUDA` de más;** las paradas son menores que las de `v3` (−269 y −148).
- **Qué escenarios lo rompen (ruido 1, `DESARROLLO` y `RESERVADO`):** `falso_sol` (esquives 191 contra 142 y 205 contra 164; éxito −3 y −4), `falso_sombra` (esquives 171 contra 144 y 165 contra 158; éxito −3), `falso_reflejo` (`AYUDA` 37 contra 33 y 22 contra 19; esquives 102 contra 81 en `RESERVADO`) y `libre_limpio` (4 esquives contra 3 en `DESARROLLO`).
- **`v6` nunca es peor que `v5-fix` en G1:** no aparece ninguna línea `G1 vs v5fix` en las 16 matrices.
- **Familia de pisos (`FDES`/`FRES`):** `se_piso_manchas` (mancha plana oscura sobre el carril): `v6` es peor que `v3` **incluso sin ruido** (esquives 8 contra 0 y 21 contra 0; paradas 12 contra 5 y 20 contra 7; éxito 112 y 104 de 120 contra 120); con ruido 1, éxito 68 contra 75 y 65 contra 72, `AYUDA` 19 contra 13 y 22 contra 17. Con `se_charco_oscuro` (informativo, no es compuerta) es peor aún: `v3` pasa 120 de 120 sin ruido porque no ve la mancha, `v5-fix` y `v6` la esquivan siempre (éxito 12–13 de 120; con ruido 1, 55 contra 3–6). Es el costo directo de la masa sólida de color sin eco (§3.3), y es **el punto débil de v6**.

### 6.4 G2, sólidos: `NO CUMPLE` (menos contactos que `v3`, pero ni cero ni «nunca peor»)

- **Contactos del núcleo** (caja 20×20, objeto largo, dos obstáculos, pared al frente), `v3` → `v6`: ruido 0 láser pesimista, `DESARROLLO` 2 → 0 y `RESERVADO` 4 → 2; ruido 1, `DESARROLLO` 17 → 6 (normal) y 28 → 14 (pesimista), `RESERVADO` 23 → 15 y 31 → 18. Con ruido 0 y láser normal quedan 1 y 1 (`objeto_largo90`, idéntico en las tres versiones).
- **`pared_al_frente`:** 0 contactos de `v6` en las 8 celdas de 120 corridas; con 960 por celda, 3 en 7680 contra 68 de `v3` (§3.1).
- **Peor que `v3` por un solo contacto en dos celdas:** `caja20`, `DESARROLLO`, ruido 1, láser pesimista (`v3` 4, `v5-fix` 4, `v6` 5) y `aparece_de_repente`, `RESERVADO`, ruido 1, láser normal (`v3` 7, `v6` 8). Con 120 corridas por celda es un contacto de diferencia (ruido estadístico, y en la celda hermana con láser normal `caja20` es 5, 1 y 1); el criterio estricto igual lo cuenta.
- **No llega a cero con ruido 1:** `caja20`, `objeto_largo90` y `dos_obstaculos` conservan de 1 a 9 contactos por celda (meta 0); quedan por debajo de `v3` salvo `caja20` (5 contra 4 en `DESARROLLO` con láser pesimista y 5 contra 5 en `RESERVADO` con láser normal).

### 6.5 G3, sin eco: `AVISO` (contactos muy por debajo de `v3`, algo peores que `v5-fix` por diseño)

- **`bloque_sin_eco` (120 corridas):** contactos `v3` 120 → `v5-fix` 20 → `v6` 25 (`RESERVADO` 119 → 33 → 35) con ruido 0; con ruido 1, `v3` 63 → `v5-fix` 16 → `v6` 19 (`RESERVADO` 70 → 17 → 18).
- **Familia sin eco con eco de color (7 escenarios, 840 corridas por celda; otros colores, texturas, tamaños y pisos):** ruido 0 láser normal, contactos `FDES` `v3` 709, `v5-fix` 109, `v6` 116; `FRES` 718, 56, 71. Ruido 1: `FDES` 407, 93, 107; `FRES` 420, 65, 77. La cobertura se sostiene en los dos conjuntos: el detector **no** está ajustado a un solo escenario. `v6` cede de 7 a 15 contactos a `v5-fix` (por `RACHA_ESTRICTA` y `LATERAL_PERSISTENTE`): es el precio de cuidar el piso limpio.
- **Cobertura parcial (honesto):** `se_piso_oscuro` (piso oscuro con objeto) queda con 30–51 contactos de 120 (`v3`: 72–115; `v5-fix`: 31–51); no se resolvió.
- **Límites físicos, no son compuerta:** `se_transparente` (117–120 contactos de 120) y `se_poco_contraste` (101–120) chocan con las tres versiones casi igual (diferencia máxima de 3 contactos): ni el láser ni la cámara los ven.

### 6.6 G4, muebles de patas: `AVISO` (falla segura con aviso)

Contactos menores que `v3` en todas las celdas, y `AYUDA` algo mayor: ruido 0, `RESERVADO`, láser normal, contactos `v3` 105 → `v5-fix` 33 → `v6` 71 y `AYUDA` 122 → 141 → 133 (pesimista: 153 → 34 → 90 contactos, `AYUDA` 93 → 121 → 96); ruido 1, `DESARROLLO`, 58 → 25 → 32 contactos y `AYUDA` 90 → 130 → 111; `RESERVADO` 108 → 43 → 73 y `AYUDA` 243 → 276 → 257. `v6` conserva entre el 39 % y el 79 % de la ganancia de contactos de `v5-fix` según la celda (mediana ≈ 57 %) y devuelve a cambio parte de la cobertura (más contactos que `v5-fix`, menos `AYUDA`), sobre todo por la persistencia lateral y `RACHA_ESTRICTA` (§4.2 y §4.3). El criterio exige además «`AYUDA` no mayor que `v3`»: no se cumple en el escenario de la silla y otros (`AYUDA` 19 contra 16 con contactos 8 contra 11 en `silla_patas`, `DESARROLLO`, ruido 0); es **falla segura**: se pide ayuda en lugar de rozar. No se afirmó nada sobre la mecánica del roce con muebles reales: **NO MEDIDO**.

### 6.7 G5, espacios: `NO CUMPLE`

- **Sumas del grupo:** con ruido 0, `AYUDA` de `v6` por debajo de `v3` (82 contra 100 en `DESARROLLO` y 55 contra 92 en `RESERVADO` con láser normal; éxito 158 contra 139 y 305 contra 268); con ruido 1, 158 contra 167 y 192 contra 204: menos que `v3`, pero la ganancia casi desaparece.
- **Por escenario el criterio estricto falla:** `caja_y_pared_al_lado` no baja la `AYUDA` de `v3` (47 contra 44, 58 contra 56, 81 contra 78, 77 contra 74; `v5-fix` igual o 1 más que `v6`) mientras `pasillo` la baja mucho con ruido 0 (35 contra 56 en `DESARROLLO`).
- **Contactos de `v6` mayores que los de `v3` con ruido 1:** `pasillo` 4 contra 2 (`DESARROLLO`, normal), 5 contra 3 (pesimista) y 8 contra 5 (`RESERVADO`, pesimista); `caja_y_pared_al_lado` 5 contra 3 (`DESARROLLO`, normal). Causa **no investigada** (hipótesis sin verificar: en esos escenarios `v6` termina más corridas que `v3` en lugar de pedir ayuda, y en algunas roza).

### 6.8 G6 y G7

- **G6, `terminado_falso = 0`: `NO CUMPLE`.** Heredado en `silla_patas` (1 por versión en `RESERVADO`, ruido 0; §3.2). Además `v6` suma 1 en `dos_muebles` y en `banquito35x45` (`RESERVADO`, ruido 0, láser normal; `v3` 0 y `v5-fix` 0) y de 1 a 2 en `caja20_con_sol` (`DESARROLLO`, ruido 1, ambos láseres; `v5-fix` igual, `v3` 0); `mueble_con_pared` 1 (`RESERVADO`, ruido 1; `v3` 1, `v5-fix` 0). Lo único trazado es la corrida 113 de `caja20_con_sol` (ruido 1, láser normal), que es **la misma** en `v5-fix` y `v6`: una maniobra larga con varios esquives y retrocesos bajo visión ruidosa de sol (traza leída solo hasta los 16 s de 36; el motivo exacto del cierre falso **no se investigó**).
- **G7, nada peor que `v5-fix` sin justificación: `NO CUMPLE` en el peor caso.** Lo que `v6` hace peor que `v5-fix` y por qué: (1) muebles: `banquito35x45` 28 contactos contra 8 (`DESARROLLO`, ruido 0) y `silla_patas` éxito 27 contra 41; (2) sin eco: `bloque_sin_eco` 25 contactos contra 20 y +7 a +15 en las familias (`RACHA_ESTRICTA` y `LATERAL_PERSISTENTE`: se pagan con cobertura para **bajar** esquives y `AYUDA` en piso limpio, donde `v6` nunca es peor que `v5-fix`); (3) variaciones de 1 a 2 esquives, paradas o `AYUDA` en sólidos (`objeto_bajo` 132 contra 131 esquives, `pasillo` 35 contra 33 `AYUDA`, `pared_al_frente` 122 contra 120 paradas en `DESARROLLO` con ruido 1 y láser normal: el freno extra de la guardia). Las dos primeras son el intercambio deliberado de v6; la tercera es ruido de ±1–2 eventos por celda.

### 6.9 Esfuerzo con ruido 2 (marcas planas en el piso; NO calibrado)

`python scripts/compuertas_v6.py --corridas 120 --ruido 2` agrega al ruido calibrado **marcas planas de color en el piso** (manchas y rayas) que **no salen de ninguna medición**: sirve para apretar a `v6` más allá de lo calibrado, no para estimar nada. 8 matrices (`DESARROLLO`, `RESERVADO` y las dos familias × láser normal y pesimista); salida cruda sin editar: [precision/v6/compuertas_final_n120_r2.txt](precision/v6/compuertas_final_n120_r2.txt). El veredicto estricto es el mismo que con ruido 1 en el peor caso:

| matriz | G1 | G2 | G3 | G4 | G5 | G6 | G7 |
|---|---|---|---|---|---|---|---|
| DES r2 L0 | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |
| DES r2 L1 | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |
| RES r2 L0 | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |
| RES r2 L1 | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |
| FDES r2 L0 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FDES r2 L1 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FRES r2 L0 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| FRES r2 L1 | NO CUMPLE | - | AVISO | - | - | CUMPLE | NO CUMPLE |
| **AGREGADO (peor caso)** | NO CUMPLE | NO CUMPLE | AVISO | AVISO | NO CUMPLE | NO CUMPLE | NO CUMPLE |


```text
DES ruido=2 laser=normal
  G1 piso limpio y falsos  v3     777/  0/110 e 490 p1140  v5fix  701/  1/165 e 756 p1036  v6     742/  0/131 e 617 p 983
  G2 solidos (nucleo)      v3     323/ 21/272 e1007 p 809  v5fix  311/  9/286 e1107 p 681  v6     312/  8/287 e1103 p 686
  G3 sin eco               v3      33/ 68/ 21 e 138 p 228  v5fix   55/ 17/ 50 e 317 p 234  v6      52/ 20/ 49 e 304 p 225
  G4 muebles               v3      49/ 61/101 e 357 p 393  v5fix   77/ 24/125 e 496 p 319  v6      53/ 30/116 e 422 p 308
  G5 espacios              v3      71/  8/168 e 374 p 330  v5fix   79/  9/159 e 455 p 310  v6      80/ 10/157 e 446 p 316
RES ruido=2 laser=normal
  G1 piso limpio y falsos  v3     765/  0/101 e 483 p1161  v5fix  683/  4/156 e 783 p1040  v6     732/  1/125 e 625 p 981
  G2 solidos (nucleo)      v3     333/ 28/259 e1017 p 805  v5fix  331/ 14/265 e1084 p 672  v6     328/ 17/267 e1080 p 668
  G3 sin eco               v3      36/ 64/ 41 e 217 p 364  v5fix   54/ 19/ 54 e 311 p 231  v6      54/ 20/ 51 e 304 p 230
  G4 muebles               v3     121/120/233 e 747 p 788  v5fix  161/ 43/279 e 976 p 689  v6     139/ 79/254 e 868 p 711
  G5 espacios              v3     150/ 14/207 e 604 p 511  v5fix  162/  9/196 e 720 p 491  v6     162/  8/196 e 699 p 490
FDES ruido=2 laser=normal
  G1 piso limpio y falsos  v3     158/  0/ 36 e 208 p 450  v5fix  133/  0/ 62 e 304 p 370  v6     144/  0/ 51 e 254 p 357
  G3 sin eco               v3     271/400/258 e1342 p1949  v5fix  424/ 93/360 e2177 p1465  v6     422/108/351 e2137 p1472
FRES ruido=2 laser=normal
  G1 piso limpio y falsos  v3     161/  0/ 43 e 218 p 521  v5fix  136/  0/ 64 e 315 p 412  v6     149/  0/ 47 e 257 p 367
  G3 sin eco               v3     264/428/255 e1340 p1988  v5fix  451/ 64/359 e2260 p1530  v6     448/ 79/346 e2193 p1553
```

Lectura (todo **SIMULADO** y con un ruido que **no está calibrado**):

- **G1 empeora para las tres versiones y el orden se mantiene** (`v3` < `v6` < `v5-fix` en esquives y `AYUDA`): `DESARROLLO` `v3` 777/0/110 e490 p1140, `v5-fix` 701/1/165 e756 p1036, `v6` 742/0/131 e617 p983; `RESERVADO` `v3` 765/0/101 e483 p1161, `v5-fix` 683/4/156 e783 p1040, `v6` 732/1/125 e625 p981. `v6` recupera entre el 52 % y el 62 % del exceso de `v5-fix` (esquives y `AYUDA`), pero queda **claramente peor que `v3`** (+21 y +24 `AYUDA`, +127 y +142 esquives, 35 y 33 éxitos menos de 960).
- **Contactos en piso limpio:** con ruido 1 `v6` tiene 0 en todas las celdas de G1; con ruido 2 aparecen de 0 a 3 por celda (todos en `pared_tras_el_final`: un esquive falso desvía al carro hacia la pared del final; `v5-fix` de 1 a 7, `v3` 0).
- **El resto repite el patrón de ruido 1:** contactos de sólidos por debajo de `v3` (`DESARROLLO` 21 → 8, `RESERVADO` 28 → 17 con láser normal; con láser pesimista 32 → 16 y 34 → 19), muebles con menos contactos que `v3` pero más `AYUDA` (`RESERVADO`, láser normal: contactos 120 → `v5-fix` 43 → `v6` 79, `AYUDA` 233 → 279 → 254) y `bloque_sin_eco` muy por debajo de `v3` (`DESARROLLO` 68 → `v5-fix` 17 → `v6` 20).
- **Conclusión:** si el piso real tuviera marcas planas de color como éstas, `v6` pediría ayuda y esquivaría bastante más que `v3`. **Esto no se puede descartar con las mediciones que hay**, y es una razón más para que `v6` solo corra tras la prueba física de piso limpio.

### 6.10 Lectura honesta

`v6` **no** cumple las compuertas estrictas, y no es una mejora gratuita: es otro punto en una curva de compensación. Gana contra `v3` donde `v3` choca (sin eco, muebles, pared al frente con láser pesimista) y recupera del orden de dos tercios de lo que `v5-fix` perdía en piso limpio con ruido realista, pero **todavía esquiva y pide ayuda más que `v3` en piso limpio con sol, sombra y reflejo**, y mucho más con una mancha oscura plana sobre el carril. Por eso el arranque de la app sigue en `v3`, y `v6` solo debe usarse si la prueba física P1 (piso limpio) muestra cero falsos positivos. **Nada de esto está medido en el carro.**

---

## 7. Qué requeriría el firmware (solo propuesta; NO implementado, NO probado)

v6 no cambia el firmware. Lo que sigue son ideas para atacar el hueco de raíz, **sin validar nada** y marcadas como hipótesis:

1. **Publicar el estado y la intensidad de la señal del láser en cada lectura** (el `VL53L0X` distingue «sin blanco en el alcance» de «blanco con señal débil»; la telemetría `TOFE?` de `SEG=4` ya expone parte). Con eso la app podría tratar «señal débil a corta distancia» como evidencia de un objeto oscuro y mate, que es justo lo que hoy no separa el láser (sin eco = libre). **La respuesta real del `VL53L0X` a superficies oscuras NO ESTÁ MEDIDA**; sin esa medición es una hipótesis, no una mejora.
2. **Segunda lectura del láser a otra altura o inclinada hacia el piso** (hardware, no solo firmware): permitiría separar una mancha plana en el piso de un objeto de 3 cm, que hoy se ven igual. No se midió ni se simuló.
3. **Registrar en la caja negra cada lectura cruda del láser con su estado** durante las pruebas físicas del protocolo, para calibrar el simulador contra el láser real (hoy solo se calibró la visión).
4. **(Navegador, no firmware) Extender la guardia del giro a los giros de la ruta y de `REINCORPORANDO`** (censo de §3.6: la mayoría de los contactos que quedan con ruido 1 ocurren ahí). Es una guardia nueva, así que no entra en v6; como el láser mira al frente, probablemente exigiría además un margen mayor o un sensor lateral (hardware). **No implementada, no probada.**

---

## 8. Cómo reproducir

```powershell
make -C nucleo_c test                                  # 24 grupos de pruebas del nucleo
make -C pruebas_firmware test
python scripts/compuertas_v6.py --apagado              # v6 apagado = v5-fix y v5 apagado = v3, linea por linea
python scripts/compuertas_v6.py --corridas 120 --ruido 0,1 --guardar docs/precision/v6/compuertas_final_n120_r01.txt
python scripts/compuertas_v6.py --corridas 120 --ruido 2 --guardar docs/precision/v6/compuertas_final_n120_r2.txt   # esfuerzo, NO calibrado
.\scripts\evaluacion_ciega.ps1 -Semilla <uint64>        # v3 contra v5-fix contra v6 con una semilla externa
```

Variables del banco (`nucleo_c/pruebas_autonomo_v2.c`): `AV5_CONJUNTO=1|2` (DESARROLLO|RESERVADO), `AV5_CIEGA=<u64>`, `AV5_LASER=0|1`, `AV6_FAMILIA=1|2`, `AV6_RUIDO=0|1|2`, `AV6_BITS=<mascara NAV_V6_F_*>|master`, `AV6_ESC=<escenarios>`, `AV6_CORRIDAS=1` (una línea por corrida), `AV6_VIS=1` (estadísticas de visión), `depurar <escenario> <corrida>` (traza).
