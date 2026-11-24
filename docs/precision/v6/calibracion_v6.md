# Calibración del banco contra las cajas negras reales (v6)

**Estado de la evidencia:** el banco es SIMULADO. Las cajas negras reales del 15-nov-2026 (7 corridas rectas de 150 cm sobre piso limpio, sin objetos, 518 ticks) son
la única medición del carro que existe para la visión; se usaron como verdad contra el simulador. No hay mediciones físicas más nuevas que las del
14/15-nov (revisado el 24-nov; se vuelve a revisar justo antes de congelar). De ellas **solo se publican agregados**; los CSV y las cajas negras no se publican.

## Qué contradecía el simulador

El banco heredado de v2/v3/v5 producía una visión casi muda en piso limpio. Las cajas negras reales muestran bastante más ruido, sobre todo de la red:

| medida (piso limpio, recta de 150 cm)                         | real (7 corridas) | `AV6_RUIDO=0` (heredado) | `AV6_RUIDO=1` (calibrado) | `AV6_RUIDO=2` (con marcas) |
| ------------------------------------------------------------- | ----------------- | ------------------------ | ------------------------- | -------------------------- |
| ticks con alguna fuente de visión (fv) en alguna zona         | 54.1 %            | 0.9 %                    | 40.8 %                    | 44.3 %                     |
| ticks con alguna celda de color (nz) en alguna zona           | 73.7 %            | 0.0 %                    | 48.0 %                    | 49.6 %                     |
| ticks con `hay` (el navegador ve obstáculo)                   | 2.3 %             | 0.0 %                    | **0.4 %**                 | 2.2 %                      |
| ticks con precaución (`pr`)                                   | 2.3 %             | 0.0 %                    | **0.4 %**                 | 2.4 %                      |
| corridas con al menos un vistazo                              | 6 de 7 (86 %)     | 10 de 120 (8 %)          | 90 de 120 (75 %)          | 94 de 120 (78 %)           |
| zona CEN: solo la red (fv=2)                                  | 27.6 %            | 0.5 %                    | 30.0 %                    | 28.7 %                     |
| zona DER: solo la red (fv=2)                                  | 35.1 %            | 0.3 %                    | 36.5 %                    | 36.2 %                     |
| zona IZQ: solo el color (fv=1)                                | 35.5 %            | 0.0 %                    | **2.9 %**                 | **5.7 %**                  |
| zona IZQ: solo la red (fv=2)                                  | 4.8 %             | 0.5 %                    | 4.3 %                     | 4.8 %                      |
| zona IZQ: color Y red a la vez (fv=3)                         | 3.5 %             | 0.0 %                    | 0.2 %                     | 0.5 %                      |

(v3, libre_limpio, conjunto DESARROLLO, 120 corridas, láser normal. Reproducible con
`AV6_VIS=1 AV6_RUIDO=0|1|2 AV5_CONJUNTO=1 AV6_ESC=libre_limpio nucleo_c/pruebas_autonomo_v2 120`.)

**Calibración parcial, dicho sin adornos:** la red por zona (CEN y DER solo red) queda muy cerca de lo real con los dos niveles. Lo que **no** se reproduce:

- el **color a la izquierda**: 35.5 % real contra 2.9 % (nivel 1) y 5.7 % (nivel 2), y las coincidencias color y red a la izquierda (3.5 % real contra 0.2 % y 0.5 %);
- los ticks con **`hay` y precaución**: el nivel 1 los **subestima** (0.4 % contra 2.3 % real); el nivel 2 los alcanza (2.2 %), pero lo logra con marcas planas en el piso que **no salen de ninguna medición** (no hay marcas en las 7 cajas negras). Es decir: el banco calibrado de verdad queda **más tranquilo** que el carro en lo que más importa para piso limpio, y el banco que llega al número real lo hace por una causa supuesta. **Por eso los veredictos de piso limpio (G1) se reportan con los dos niveles y no se escoge el más favorable.**

Es una limitación conocida, con v3, v5 y v6 por igual. Que el color espurio real a la izquierda sea 6 a 12 veces mayor que el simulado puede favorecer o perjudicar a v6 (su persistencia lateral ataca justo eso): **NO MEDIDO**.

## Qué agrega `AV6_RUIDO` (solo percepción)

Nada de esto lo ven el láser, el contacto ni la ruta (los prismas `solo_vista` de `mundo_v2`); solo la cámara y el detector. Dos niveles:

**`AV6_RUIDO=1` (el CALIBRADO con las cajas negras):**

1. **Algo que no es piso a la izquierda del carril** (70 % de las corridas): una estructura oscura (colores 30 a 120), 250 a 450 mm del carril, 80 a 500 mm de alto,
   que corre a lo largo de toda la recta. Es lo que explica el color espurio a la izquierda de las cajas negras. Desde el carro se ve como una masa grande de un lado.
2. **Ruido de la red** con colas largas, como el real: cajas inventadas grandes y bajas (casi siempre cercanía 3) que parpadean en rachas cortas (70 %), medias
   (17 %) y largas de 1.1 a 1.8 s (13 %). La fracción de tiempo marcado varía por corrida: 30 % de las corridas casi sin ruido (0 a 15 %) y 70 % con mucho (38 a 62 %).

**`AV6_RUIDO=2` (ESFUERZO, NO calibrado):** todo lo anterior más

3. **Marcas planas en el piso** (25 % de las corridas; 10 % de ellas sobre el carril): cinta, papel o mancha de color cualquiera, de 6 a 34 cm. **Esto NO sale de las
   cajas negras** (no hay ninguna marca en las 7 corridas); es una suposición de esfuerzo que pidió el encargo («pisos con manchas»). **No se usa para decidir nada**: solo
   se informa. (`AV6_R_MARCAS_PCT` fija el porcentaje a mano.)

Cada parte se puede apagar por separado (`AV6_R_IZQ_PCT`, `AV6_R_DUTY_ESCALA`, `AV6_R_MARCAS_PCT`) para atribuir efectos. No se tocó ninguna física del simulador.

## Efecto sobre las versiones (el hallazgo que cambió el rumbo de v6)

Con el banco heredado (sin ruido) v5-fix y v3 son idénticos en piso limpio (0 ayudas, 0 esquives). Con el ruido realista **v5-fix es peor que v3 en piso limpio**:
la masa sólida sin eco, las rachas verticales y la supresión del vistazo toman como obstáculo lo que v3 ignoraba. Atribución por componente del ruido (DESARROLLO,
8 escenarios de piso limpio y falsos, 120 corridas c/u = 960, láser normal; éxitos / ayudas / esquives de v3, de v5-fix y de v6, con las banderas finales de v6):

| ruido encendido                                       | v3              | v5-fix          | v6              |
| ----------------------------------------------------- | --------------- | --------------- | --------------- |
| solo la red                                           | 789 / 87 / 419  | 785 / 94 / 452  | 785 / 94 / 452  |
| solo la estructura izquierda                          | 959 / 1 / 3     | 917 / 37 / 146  | 957 / 3 / 10    |
| `AV6_RUIDO=1` completo (red + estructura): CALIBRADO  | 798 / 101 / 440 | 768 / 130 / 596 | 795 / 111 / 507 |
| solo marcas planas (`AV6_RUIDO=2`, NO calibrado)      | 958 / 1 / 4     | 853 / 65 / 304  | 906 / 23 / 130  |
| `AV6_RUIDO=2` completo (NO calibrado)                 | 777 / 110 / 490 | 701 / 165 / 756 | 742 / 131 / 617 |

Lectura honesta: v6 recupera casi todo lo que v5-fix perdía con la estructura izquierda (es lo que ataca `LATERAL_PERSISTENTE`), pero **no toca el ruido de la red** (en esa
fila v6 y v5-fix son iguales) y **sigue por debajo de v3** con las marcas planas y con los tres ruidos juntos. Salidas crudas: `docs/precision/v6/ablacion_ruido_dev_l0.txt`
(por componente de ruido, regenerada con la semántica final de `AV6_RUIDO`) y `docs/precision/v6/ablacion_bits_v5_dev_l0.txt` (por parte de v5; **medida con la semántica
anterior de `AV6_RUIDO=1`, que incluía las marcas planas: hoy equivale a `AV6_RUIDO=2`**). Detalle del diseño y de lo que se hizo con esto en `docs/autonomo_v6_diseno.md`.
