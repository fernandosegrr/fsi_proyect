# Subsistema H · Simulador y pruebas

## H.1 Qué es y para qué sirve

H reúne lo que se usa para probar el software sin mover el carro: un carro virtual y un mundo sintético escritos en C (`simulador_navegador.c`, `mundo_v2.c`, `pisos_sinteticos.c`), el Monte Carlo (repetir miles de corridas con una semilla fija), las pruebas del núcleo y del firmware, los verificadores y el protocolo de pruebas físicas. Lo que miden estas herramientas es **SIMULADO**; lo único validado en el carro es lo de v1.1 (10-nov-2026).

Más detalle: capítulo 12 del manual (`docs/manual/capitulos/12_pruebas.md`).

## H.2 Cómo funciona, paso a paso

```
 semilla ──▶ escena (mundo_v2.c): ruta, objetos, piso, luz
                 │
   cámara virtual 24x17 celdas · láser de cono 25° · detector estadístico
                 ▼
┌──────────────────────────────┐ comando    ┌─────────────────────────────┐
│ núcleo REAL: obstaculos.c,   │───────────▶│ carro virtual               │
│ fusion.c, navegador.c        │◀───────────│ simulador_navegador.c (J16) │
└──────────────────────────────┘ SENS, giro └─────────────────────────────┘
                 │
                 ▼
   contacto del chasis con los objetos ──▶ UNA categoría por corrida
```

Una corrida del banco autónomo (`nucleo_c/pruebas_autonomo_v2.c`) sigue estos pasos:

1. **Escena.** Con la semilla arma ruta, objetos, piso y luz: prismas rectos con color, altura y reflectancia, piso de losetas con juntas, manchas de sol, sombras y reflejo de ventana (`mundo_v2.c`).
2. **Aprender piso.** Doce cuadros sintéticos desde la salida, como la app; el piso queda fijo durante la ruta.
3. **Cada 50 ms.** Cámara virtual (rejilla de 24×17 celdas, la misma del paquete `CEL1`; unos 12 cuadros por segundo que llegan 110 a 170 ms tarde, 2 % perdidos) → `obstaculos_analizar_modelo_celdas` → `fusion_decidir` con el detector neuronal estadístico (inferencia de 60 a 120 ms; no corre ninguna red) → `navegador_tick`. El láser es un cono de 25° con `SENS` periódico.
4. **Carro virtual.** `simulador_navegador.c` con el modelo físico J16 (`sim_j16_activar`): AUTO a 19.3 cm/s, pivotes medidos, el lazo RUMBO replicado de `firmware_carro/rumbo.c`, latencias y ruido.
5. **Contacto.** En cada tick compara el rectángulo del chasis (20 × 24 cm) con los prismas bajos.
6. **Categoría.** Cada corrida cae en UNA: ok, contacto, bajo_mueble (pasó entre las patas sin tocar), ayuda, error, sin_terminar, falso_fin, esquive_falso u otro.
7. **Repetir y comparar.** El Monte Carlo repite con semillas distintas y resume media, p50, p95 y máximo. `scripts/compuertas_autonomo.py` compila el mismo banco con cuatro núcleos (v1.1 `bd1727b`, la rama de partida `6bea71e`, v2 `33bdb77` y el actual) y aplica las compuertas.

### Las piezas en C

| Archivo | Qué simula | Dónde corre |
|---|---|---|
| `simulador_navegador.c` | Carro (tiempo, frenado, pivote) y firmware (giroscopio, láser, mensajes); cuenta `violaciones_seguridad` | Pruebas del núcleo y banco |
| `sim_ruta.c` | Cámara geométrica y entrada de rutas de celdas, hasta 16 obstáculos | Pruebas y app («Rutas → Simular») |
| `mundo_v2.c` | Mundo del banco autónomo; usa `double` y `math.h` | Solo PC |
| `pisos_sinteticos.c` | Cuadros de 240×176 con pisos difíciles (negro, liso, baldosas, franjas, damero), ruido y cambios de exposición | Pruebas del detector y cámara falsa |

### Monte Carlo y compuertas

El programa de precisión (`nucleo_c/pruebas_precision_j16.c`) tiene tres partes: A compara la réplica de RUMBO del simulador con `firmware_carro/rumbo.c` en 20,000 pasos al azar; B comprueba propiedades de la pose; C es el Monte Carlo de 10 escenarios (rectas, giros, ruta en L, cuadrado, esquives) en 5 configuraciones. `make test` lo corre con 150 corridas y `make precision` con 1,000 (unos 20 s, `docs/precision/simulador.md`). Sus métricas son el error en mm (media, p50, p95, máximo), el error lateral y longitudinal, el rumbo, cuántas corridas llegan y cuántos toques (contactos con la caja) hubo.

Los 23 escenarios del banco autónomo (`ESC` en `pruebas_autonomo_v2.c`) son de cuatro tipos: 7 de falsos positivos (piso limpio, juntas, sol, sombra, reflejo, exposición, detector), 14 con obstáculo (cajas, banquito, silla, objeto largo, pasillo, pared al lado...), 1 imposible (`pared_al_frente`) y 1 límite (`bajo_solo_color`).

Las compuertas son metas numéricas (función `compuertas` de `scripts/compuertas_autonomo.py`): 0 contactos; 0 «Terminado» sin dejar atrás el objeto; con obstáculo, éxito de al menos 99 % y AYUDA menor a 1 %; con falsos positivos, 0 esquives, 0 ayudas y paradas en menos de 1 % de las corridas; y nunca peor que v1.1, la partida o v2.

### Otras capas de prueba

- **Núcleo.** `make -C nucleo_c test` corre 24 ejecutables compilados con `-Werror`. El banco autónomo queda fuera: se corre con `make -C nucleo_c autonomo`.
- **Firmware.** `make -C pruebas_firmware test` corre `pruebas_carro`, `pruebas_sensores`, `pruebas_rumbo`, `pruebas_escalonado` y `pruebas_camara`, más `scripts/verificar_pagina.py`. Compilan el C del firmware sin modificarlo, con un MPU6050 y un VL53L0X falsos (`sensores_falsos.c`) que permiten errores de bus y tiempo controlado.
- **Servidor y cámara falsos.** `pruebas_firmware/servidor_falso.py` imita al carro por WebSocket (puerto 8080, ruta `/ws`, watchdog de 1,000 ms) y `probar_servidor_falso.py` comprueba que se porte como el carro. `scripts/camara_falsa.py` y `scripts/camara_falsa_udp.c` dan video sintético; `scripts/probar_modos_video.py` lo prueba en el teléfono.
- **Versiones y enlaces.** `scripts/probar_version.py` prueba, sin USB ni teléfono, los lectores de versión de `scripts/verificar_version.py` (firmware y APK). `docs/verificar_enlaces.py` busca enlaces Markdown rotos entre los `.md`.

### Protocolo de prueba física

`docs/protocolo_prueba_fisica_autonomo_v2.md` va de menor a mayor riesgo y no se pasa al siguiente bloque si el anterior no cumplió. **A**: rectas sin obstáculos. **B**: un obstáculo (caja de 20×20, caja corrida, banquito, objeto largo, pared, caja con pared al lado). **C**: falsos positivos provocados (sombra, sol, linterna, luz distinta al aprender). **D**: fallas y giros (cortar la WiFi, tapar la cámara, ruta en L, cuadrado, dos cajas). **M**: diez mediciones baratas: M1 deriva del giroscopio, M2 si el láser ve patas reales, M3 láser contra una pared de lado, M4 deriva de la exposición, M5 distancia de frenado por láser, M6 arranque junto a una pared, M7 giros cortos, M8 velocidad real (¿19.3 o cerca de 30 cm/s?), M9 latencia de extremo a extremo y M10 ritmo del láser. Siempre con una mano en STOP; la sesión se aborta si el carro avanza sin orden o no frena con STOP. Hojas de registro: `docs/pruebas_fisicas_plantilla.csv` y `docs/pruebas_fisicas_mediciones_v3_plantilla.csv`. **PENDIENTE**: el propio documento dice que nada se ha ejecutado.

## H.3 Cómo lo hicimos

- **El simulador no copia al navegador.** Usa el navegador, el detector de color y la fusión reales; solo el mundo y el carro son sintéticos (`docs/referencia/simulador.md` §2; cabecera de `pruebas_autonomo_v2.c`). Así una prueba ejercita el código que corre en el teléfono.
- **Supuestos a la vista.** Las medidas del mundo están marcadas SUPUESTOS en `nucleo_c/mundo_v2.h`: carro de 200 × 240 mm; láser a 120 mm del centro, cono de 25° y alcance de 1,200 mm; cámara inclinada 12° con campo de 56°.
- **Mismas condiciones para comparar.** Misma semilla, mismo mundo y mismo simulador para cada versión; solo cambia el núcleo (`scripts/compuertas_autonomo.py`).
- **Categorías excluyentes.** En v2 las columnas contactos, ayuda y bajo_mueble se traslapaban. v3 las hizo excluyentes para que sumen las corridas (commit `6cf1850`; `docs/limites_conocidos.md`).
- **Calibración con lo medido.** J14 y J16 son rondas de trabajo del proyecto (`docs/reporte_J14.md`). El simulador se ajustó con el carro de J14: la diferencia de empuje entre lados se eligió de 0.3 a 2.1 %, siempre el izquierdo (B4-1 se desvió a la derecha). Los cuatro desvíos medidos en B4 (5.5, 15, 18 y 20 cm en 160 cm) caen en los percentiles 9, 57, 73 y 82 de las rectas simuladas; el esquive B6-4 (unos 40 cm) cae en el 99. Reproduce la magnitud, pero solo en la cola (`docs/precision/simulador.md`; `docs/precision/calibracion_j14.txt`). La velocidad de 19.3 cm/s y los pivotes (derecha: 15 cm adelante y 16.5 de lado a 90°; izquierda: radio de unos 8.7 a 9 cm) salen de las calibraciones de octubre (`docs/calibracion.md`).
- **Cajas negras reales.** Las de las corridas no se publican (`cajas_negras/` está en `.gitignore`). El repo solo guarda documentos que las resumen, como `docs/investigacion_autonomo/90_hallazgos.md` (H-22: las del 15-nov sugieren cerca de 30 cm/s; no se adoptó; **NO MEDIDO**).
- **Decisión para la exposición: Opción 2 (pilas AA nuevas y calibración nueva, incluyendo obstáculos), no v1.1.** Aunque v3 dejó 23 compuertas en rojo en el banco simulado y v4, v5 y v6 se descartaron en simulación con ruido realista (ramas archivadas), se expone v3 autónomo con `FW:b396390` (`SEG=3`, `TOFP=150`, `TOF_BLOQUEO=180`), las correcciones del 26-nov y el arreglo de sombras en `aprenderPiso` (v1.2.5; **Plan B:** el mismo firmware v3 con el autónomo apagado en manual o semiautónomo; si va a golpear, pasar a manual o apagar). v1.1 **no** es respaldo seguro porque su paro de 120 mm deja solo 1.5 a 3.5 cm de margen y puede golpear el sensor. Condiciones operativas: pilas AA alcalinas nuevas y un juego de repuesto; piso con al menos `60 cm` libres al frente según el láser (`OBS_APRENDER_TOF_LIBRE_MM = 600` mm); obstáculo blando (caja de cartón o cojín), nunca algo frágil; encender primero la *powerbank* con el carro quieto 3 s para el giroscopio y luego conectar las pilas; revisar cables; confirmar la recta de `1.5 m` en unos `3 s` (`46–50 cm/s`). **NO MEDIDO** el frenado aislado a `46–50 cm/s` (frenado `M5` 10/10 medido a ~28 cm/s; a esa velocidad existe posibilidad de toque suave; el 27-nov se midió `esquive a 46–50 cm/s: 3 corridas, 1 con toque`).

### Semillas: cuál se usa y para qué

- **Monte Carlo de precisión J16** (`nucleo_c/pruebas_precision_j16.c`, función `monte_carlo`, línea 675): `160000 + índice_de_escenario × 10007 + corrida`. Es la misma en las cinco configuraciones: corridas pareadas, donde lo único que cambia entre dos configuraciones es el software.
- **Banco autónomo v2 y v3** (`nucleo_c/pruebas_autonomo_v2.c`, línea 695): `170000 + índice × 10007 + corrida`. El azar del simulador se deriva con `semilla ^ 0x5EEDu` (línea 436).
- Con la misma semilla el resultado se repite exacto. Una corrida se reproduce con `./pruebas_autonomo_v2 depurar <escenario> <corrida>`.
- **`0xF51A2026`** es un valor que citaba la documentación de v1.1 como semilla fija (commits `adfbda1` y `bd1727b`, 10-nov-2026). No está en el código ni en su historial: no es una semilla real. (La semilla real de precisión J16 es `160000 + índice × 10007 + corrida`, y la del banco autónomo `170000 + índice × 10007 + corrida`.)

### Conjuntos DESARROLLO y RESERVADO

Son dos juegos de escenarios y semillas. Con DESARROLLO se ajusta el software; RESERVADO se abre solo al cerrar cada etapa y no se elige ningún parámetro con sus resultados (`docs/precision/plan_v4.md`, 18-nov, en el archivo v4). **En el árbol de la v1.2/v1.2.1 no existen como código**: v1.2/v1.2.1 usa el Monte Carlo simple de v3 (23 escenarios y una sola serie de semillas). Se definen en otras referencias de git, que se leen con `git show <ref>:<ruta>`:

- `archive/feat-autonomo-v4-a200df4`, `docs/precision/plan_v4.md`: DESARROLLO tiene 23 familias y semillas `1200000 + índice × 10007 + repetición`. RESERVADO usa `3200000 + …`, con posiciones y patas nuevas, pasillos de otros anchos y tres escenarios más (`dos_muebles`, `mueble_con_pared`, `bloque_sin_eco`). Cada conjunto se corre con láser «normal» y «pesimista» (más pérdidas y mayor señal mínima), 40 repeticiones por familia y perfil (`AV4_PESIMISTA=1`; argumento `desarrollo` o `reservado`).
- `freeze/v5-carbon`, `docs/precision/calibracion_v5.md`: `AV5_CONJUNTO=1` (24 escenarios, `2100000 + …`) y `=2` (27 escenarios, `4300000 + …`).
- `freeze/v6`, `docs/precision/v6/familia_sin_eco.md`: `6100000 + …` y `8300000 + …` para una familia sin eco.

En el árbol actual solo queda `docs/estado_v3_pendiente_pruebas.md`, que usa resultados de esos conjuntos (40 corridas por escenario y perfil, tomadas del archivo v4).

## H.4 Cómo lo probamos y con qué resultado

| Qué se probó | Cómo | Resultado | Etiqueta |
|---|---|---|---|
| Núcleo en PC (v3 + 26-nov + sombras v1.2.5) | `make -C nucleo_c test` | 24 ejecutables en la suite (`1,813` casos); `pruebas_pisos_dificiles` (42, 0 fallos), `pruebas_esquive_confirmado` (277, 0 fallos), `pruebas_reincorporacion_j15` (21, 0 fallos), `pruebas_precision_j16` (46, 0 fallos); **1 fallo conocido** en `pruebas_sim_ruta` (`61 casos, 1 fallos`: «las quince cajas obligan a esquivar»). Histórico 16-nov: 1,901 casos, 0 fallos (**calculado con v3 antes de las correcciones del 26-nov, sin recalcular**) | **SIMULADO** |
| Precisión J16 (código incluido en v1.1) | Monte Carlo, 1,000 corridas por escenario y configuración, 31-oct-2026 (**calculado antes de las correcciones del 26-nov, sin recalcular**) | Recta de 150 cm, error medio 137 mm (J14), 80 (J15) y 10 (con RUMBO). Esquive de caja 20×20: 200, 37 y 17 mm; toques 49, 9 y 0 de 1,000. 46 casos, 0 fallos (`docs/precision/monte_carlo_1000.txt`) | **SIMULADO** |
| Simulador contra el carro | Desvíos medidos en B4 y B6-4 contra 1,000 corridas (**calculado antes de las correcciones del 26-nov, sin recalcular**) | Percentiles 9, 57, 73, 82 y 99 (`docs/precision/calibracion_j14.txt`) | **SIMULADO** |
| Banco autónomo (v3, histórico 16-nov-2026) | 23 escenarios × 1,000 (**calculado con v3 antes de las correcciones del 26-nov, sin recalcular**) | 23 compuertas en rojo. Banquito: 849 ok y 43 contactos (v1.1: 245 y 596). Silla: 435 ok y 21 contactos. Caja con pared al lado: 613 ok y 386 ayudas. Paradas con sol, sombra y reflejo en 93, 82 y 126 corridas (`docs/precision/autonomo_v3_comparacion.txt`) | **SIMULADO** |
| Conjuntos de 40 corridas (v3) | DESARROLLO y RESERVADO, láser normal y pesimista | `bloque_sin_eco`: contacto en 40 de 40 en reservado. Banquito con láser normal: contacto en 12 de 40 (desarrollo) y 9 de 40 (reservado) (`docs/estado_v3_pendiente_pruebas.md`) | **SIMULADO** |
| Firmware en PC (v3) | `make -C pruebas_firmware test` | 5 ejecutables, 567 casos, 0 fallos, 16-nov-2026 | **SIMULADO** |
| Servidor falso y versiones | `probar_servidor_falso.py`; `scripts/probar_version.py` | 60 casos y 21 pruebas, sin fallos, 16-nov-2026 | **SIMULADO** |
| Enlaces de los documentos | `docs/verificar_enlaces.py` | 181 enlaces en 154 archivos, todos válidos, 16-nov-2026 | **SIMULADO** |
| Revisión de UI | Huawei ELS-NX9 con carro y cámara falsos, 24-nov-2026 | Cuadros muestreados con una pasada lenta de «medir y acomodar»: 19.2 % (app anterior) y 3.3 % (nueva). «Ruta en L» completa en 6 s (`docs/revision_ui_android.md`) | **SIMULADO** |
| Esquive en el carro (v1.1) | B6, 10-nov-2026 | 6 de 6 sin tocar la caja; 5 de 6 completas; error lateral de unos 0 a 10 cm, medido de forma aproximada (`docs/estado_20261110.md`) | **VALIDADO EN EL CARRO** |
| ASan, UBSan y TSan sobre v3 | No se pudieron correr en Windows | Existe `.github/workflows/sanitizadores.yml`; sin resultado para v3 | **NO MEDIDO** |
| v3 en el carro; M1 a M10 | `docs/protocolo_prueba_fisica_autonomo_v2.md` | Ninguna corrida | **PENDIENTE** |

Las cifras salen de los archivos de resultados; si otro documento da cifras distintas, vale el archivo. En el Monte Carlo heredado (**calculado con v3 antes de las correcciones del 26-nov, sin recalcular**), el error lateral tras esquivar es 18/44 mm (media/p95) en v3 contra 17/40 en v1.1 (`docs/estado_20261116.md`).

### FALLO CONOCIDO en `pruebas_sim_ruta`

- **Qué prueba es:** `make -C nucleo_c test` → `pruebas_sim_ruta` (`61 casos, 1 fallos`: `FALLO  las quince cajas obligan a esquivar`).
- **Qué esperaba:** esquivar las 15 posiciones de cajas (centradas y laterales) solo con visión cuando las laterales quedan fuera del haz del láser.
- **Qué hace ahora el código:** con ToF fresco (`solo_laser`), la coincidencia visual sola no dispara esquive: manda el láser, y la visión sola solo avisa.
- **Por qué se hizo así:** evitó el contacto con la mochila negra (`3/3` terminado, 0 contactos) y los falsos positivos en piso limpio (`5/5` terminado, 0 falsos positivos) en las pruebas físicas del 25–26 nov.
- **Consecuencia:** los objetos laterales fuera del haz estrecho del láser (`VL53L0X`, ~25°) son **PENDIENTE** de validar en el carro; en simulación el caso es solo **SIMULADO**.
- **Nota:** `pruebas_sim_ruta.c` no se actualizó porque no se diagnosticó si es una prueba obsoleta o una pérdida de comportamiento real.

## H.5 Límites conocidos

- **Mundo supuesto.** Patas de 25 a 35 mm con asiento sobre el haz del láser, y un láser que lee «el grupo más cercano dentro del cono», son suposiciones. Con el modelo alterno (`AV3_LASER_MEZCLA=1`) los contactos son iguales y las ayudas suben (banquito 23 y silla 14, contra 7 y 4 en 300 corridas; `docs/limites_conocidos.md`). Si el láser ve patas reales: **NO MEDIDO** (M2).
- **Lo que no modela.** Deslizamiento de ruedas, imágenes y detector reales, la red neuronal, la física del choque, WiFi, batería y latencia real (`docs/referencia/simulador.md` §10). La cámara es geométrica y el láser es un rayo ideal.
- **Casos sin medir.** `bloque_sin_eco` (objeto oscuro o mate sin eco) y las manchas de sol, sombra y reflejo solo se han visto en simulación (`docs/estado_v3_pendiente_pruebas.md`; `docs/limites_conocidos.md`).
- **Muestras chicas.** Con 40 corridas por escenario, «cero en esta muestra no certifica riesgo físico cero» (`plan_v4.md`).
- **Nadie pulsa «Continuar».** El banco enciende «pedir ayuda» pero no llama a `navegador_ayuda_continuar`: una corrida que pide AYUDA termina ahí. Ese caso solo se prueba en `pruebas_navegador.c` y `pruebas_esquive_confirmado.c`, y el problema de «Continuar» sin mover el carro sigue abierto (subsistema E).
- **Banco fuera de `make test`.** Un comentario de `pruebas_autonomo_v2.c` dice que `make test` lo corre, pero el `Makefile` no lo incluye: hay que correr `make autonomo`.
- **Sanitizadores.** En Windows no corren ASan, UBSan ni TSan; se usó `-ftrapv` y el analizador estático de GCC, que cubren menos (`docs/estado_20261116.md`).
- **Poca evidencia física.** En el carro solo hay 6 corridas de B6 (v1.1, medición aproximada); de v3, ninguna.

## H.6 Términos clave

- **Monte Carlo**: repetir una prueba miles de veces con pequeñas variaciones al azar y resumir los resultados.
- **semilla**: número que arranca el generador de azar; con la misma semilla el resultado se repite.
- **corridas pareadas**: la misma semilla en cada configuración, para que solo cambie el software.
- **banco**: programa que corre muchos escenarios y cuenta los resultados (`pruebas_autonomo_v2`).
- **escenario**: situación de prueba, como `caja20` o `banquito35x45`.
- **categoría excluyente**: cada corrida cae en una sola (ok, contacto, bajo_mueble, ayuda...).
- **compuerta**: meta numérica que el banco debe cumplir.
- **p95**: valor bajo el cual queda el 95 % de las corridas.
- **DESARROLLO y RESERVADO**: conjunto para ajustar y conjunto que no se toca hasta el cierre.
- **servidor falso**: programa de PC que imita al carro por WebSocket.
- **hardware falso**: MPU6050 y VL53L0X simulados para probar el firmware en PC.
- **caja negra**: archivo con lo que pasó en cada tick de una corrida real; no se publica.
- **`-ftrapv`**: bandera de GCC que detiene el programa si un entero con signo se desborda.

## H.7 Preguntas probables del profesor

1. **¿Qué tan confiable es el simulador?** Sirve para comparar versiones con las mismas condiciones y encontrar errores de lógica, no para demostrar cómo se porta el carro. Se calibró con cuatro rectas y un esquive medidos en J14, y lo demás son supuestos marcados. Por eso toda cifra de v3 es **SIMULADO**.
2. **¿Qué es el Monte Carlo y para qué sirve la semilla?** Repite la misma prueba con variaciones al azar (ruido de giro, retrasos, posición inicial) y resume media y p95. La semilla hace que el azar se pueda repetir: misma semilla, mismo resultado. Se usan `160000 + índice × 10007 + corrida` en precisión J16 y `170000 + …` en el banco autónomo; `0xF51A2026` solo aparecía en documentos.
3. **¿Qué validaron realmente en el carro?** Solo v1.1: en B6 (10-nov-2026) 6 de 6 corridas sin tocar la caja y 5 de 6 completas, con error lateral de unos 0 a 10 cm medido de forma aproximada. v3 no se ha movido en el carro; el protocolo está escrito y no se ha ejecutado.
4. **Si v3 tiene 23 compuertas en rojo, ¿por qué es la base?** En simulación v3 mejora a v1.1 con muebles de patas (banquito: 849 ok contra 245) pero sigue fallando en muebles, pared al lado y falsos positivos, y el repo dice que v4 se descartó. Por qué se prefirió v3 a v1.1 no está escrito; lo debe contestar el equipo.
5. **¿Cómo repiten las pruebas?** `make -C nucleo_c test`, `make -C pruebas_firmware test`, `make -C nucleo_c autonomo` (banco de 1,000 corridas), `python pruebas_firmware/probar_servidor_falso.py` y `python scripts/probar_version.py`. Las cifras de este capítulo son de los documentos del 16-nov-2026, no de una ejecución nueva.
