# Giro medido con la cámara (giro visual)

El carro gira "por tiempo" (ms por grado, de la calibración) y no tiene IMU ni encoders. Un giro de 90 grados puede salir de
70 o 110 según el piso, la batería y la carga. La cámara sí ve girar el cuarto: aquí se usa para **medir cuánto gira el carro** y
corregir el error del giro por tiempo.

Código: `nucleo_c/movimiento_visual.c/.h` (C puro, `pruebas_movimiento.c`), la integración en `navegador.c`
(`navegador_aporte_visual`, `giro_terminado`) con pruebas en `pruebas_navegador.c`, el campo de visión en `calibracion.c`
(`cal_ajustar_fov`), y en la app `MovimientoVisualNativa`, `CamaraObstaculos`, `ModoSemiautonomo`, la pantalla de visión y un paso
nuevo en `CalibracionActivity`.

## Cómo se mide el giro entre dos cuadros

1. **Huella de un cuadro** (`mv_huella`): solo la **mitad de arriba** (el horizonte, paredes y muebles se mueven limpio; el piso
   brilla y cambia). Se suma el brillo de cada columna: queda una línea de números.
2. **Corrimiento** (`mv_corrimiento`): la huella se pasa a "bordes" (diferencia entre columnas vecinas, así un cambio de luz
   general no importa) y se corre la huella del cuadro anterior de −rango a +rango píxeles midiendo cuánto difiere de la nueva
   (suma de diferencias absolutas). El corrimiento con el menor costo es el movimiento. El **subpíxel** sale de una parábola por los
   tres costos alrededor del mínimo (±medio píxel).
3. **Rango**: al menos un tercio del ancho; `mv_procesar` usa 2/5 del ancho (96 px en 240) para que un corrimiento de 80 px
   (un tercio) no quede en el borde. Con ~10 a 12 cuadros por segundo y un giro rápido el corrimiento entre cuadros puede ser grande
   (a 90°/s y 12 fps son 7.5° por cuadro = 30 px en 240 de ancho).
4. **Cuadros rechazados**: `mv_procesar` compara cada cuadro con la **referencia** (el último cuadro con resultado válido). Si un cuadro se
   rechaza, la referencia se conserva y el siguiente se compara contra ella: el corrimiento abarca varios cuadros (`cuadros` = 2, 3...) y el giro de
   los cuadros rechazados no se pierde. Con datos reales (giro a mano de 90°) ~20 % de los cuadros a la velocidad pico (15 px por cuadro) se
   rechazaban y el ángulo acumulado salía en 36° en vez de 90°. Después de 4 rechazos seguidos (`MV_MAX_SALTO`) se empieza de nuevo. La app calcula el
   tiempo (`dt`) desde la hora del cuadro de referencia, no del cuadro anterior.
5. **Confianza**: el resultado solo es **válido** si pasa tres pruebas; si no, se devuelve "sin confianza" y NO se suma:
   - la huella tiene bordes (energía ≥ 60; una pared lisa tiene ~8): si no, `ESCENA_UNIFORME`;
   - el mejor costo es menor que el 55 % del costo medio de todos los corrimientos: si no, `COINCIDENCIA_DEBIL`;
   - el mejor costo es menor que el 88 % del segundo mejor mínimo lejano (a más de 2 px): si no, `AMBIGUO` (rayas o baldosas que se
     repiten), **salvo** que haya un mínimo local bueno (costo hasta el doble del mejor y que pasa la prueba de coincidencia débil) cerca de
     lo que se esperaba por el movimiento anterior (a menos de 4 px o 40 % del corrimiento esperado): ese se acepta (`por_prediccion` = 1);
   - el mínimo no está justo en el borde del rango: si lo está, `FUERA_DE_RANGO` (el corrimiento real podría ser mayor).
6. **Grados**: `giro = corrimiento × FOV / ancho` (FOV horizontal, 60° de partida). Un corrimiento del contenido hacia la
   **izquierda** de la imagen es un giro a la **derecha** (positivo, igual que el navegador).

**Error de la aproximación lineal**: con un FOV de 60° la relación píxel-ángulo no es exactamente lineal. Para una característica
justo al centro el giro sale 9 % corto, a ±20° 3 % largo, a ±30° (el borde) 21 % largo; promediando todas las columnas el error es de
sólo −0.7 % (calculado con la geometría de una cámara de agujero). Por eso, y por si el FOV real de la cámara no es 60°, el FOV se
**calibra** con un giro medido (siguiente sección).

## Campo de visión en la calibración (`CalibracionCarro.fov_grados_x10`)

El giro visual es proporcional al FOV, así que con un giro de 90 grados reales que la cámara midió como 80 el FOV correcto es
60 × 90 / 80 = 67.5° (`cal_ajustar_fov`). Se guarda en la calibración (`fov_grados_x10`, con su bit 16). Es **opcional**: los
textos de calibración guardados antes (sin esa clave) se siguen leyendo con el FOV por defecto; no cuenta para "todo calibrado".

En la app: **Calibración del carro → paso «6. Giro con la cámara (opcional)»**: con el carro alineado a una marca de 90 grados y
mirando una pared con detalles, toca «Probar»: gira a la derecha lo que la calibración dice que son 90 grados mientras se suma lo que la
cámara ve girar (se sigue midiendo 800 ms después del STOP, porque el carro sigue girando un poco). Tú escribes cuántos grados giró de
verdad. Hasta 3 pruebas; el resumen calcula el FOV con la suma de las pruebas.

## Cómo corrige el navegador el giro (`navegador.c`)

- La app avisa con `navegador_aporte_visual(giro_x10, dt_ms, confiable)` cuánto vio girar la cámara. La cámara puede dar más cuadros
  de los que la pantalla lee: `CamaraObstaculos.tomarGiroNuevo()` **suma** los giros confiables de todos los cuadros que llegaron
  (no se pierde ninguno) y el tiempo que cubren.
- Solo cuenta mientras el carro **gira** (LEFT o RIGHT) y **después del retardo de arranque** medido en la calibración: los cuadros
  durante el arranque se ignoran (el carro todavía no se mueve).
- El ángulo girado = lo que vio la cámara + (por tiempo, con los ms por grado de ese lado) el pedazo del giro que la cámara no cubrió
  (cuadros sin confianza).
- Se fía de la cámara si cubrió al menos el 50 % del tiempo de giro (y 200 ms); si no, el giro sigue **por tiempo** como siempre.
- **El giro se corta** cuando el ángulo girado más lo que el carro seguirá girando al frenar (calibración) llega al pedido, pero **no antes del
  50 %** del tiempo planeado (por si la cámara se equivoca). El **respaldo** es el tiempo planeado: si la cámara dice que falta, el giro se
  alarga hasta el 150 % del tiempo planeado como máximo. Sin cámara confiable, el tiempo planeado manda.
- Lo mismo vale para el giro de apartarse al esquivar.
- La posición estimada (rumbo) usa el giro de la cámara cuando es confiable.

## Pruebas en PC

`pruebas_movimiento.c` (66 casos) con imágenes sintéticas: un panorama de texturas y cuadros que son ventanas desplazadas:

- corrimientos de **1, 2, 5, 10, 20, 40 y 80 px** a los dos lados, en 240x176 y 320x240: error de menos de 0.25 px, sin ruido; con **ruido** de ±10
  por canal, 160 de 160 corrimientos de 1, 10, 40 y 80 px con error de medio píxel o menos;
- cambio de luz (+40, −30) entre cuadros, cuadros iguales (giro 0), la mitad de abajo (el "piso") distinta en cada cuadro;
- **escena casi uniforme** (pared lisa con ruido de ±2: energía 8 contra un mínimo de 60), perfectamente uniforme, una sola marca: sin confianza;
  rayas que se repiten cada 16 px: ambiguo; 40 pares de escenas sin relación: 40 de 40 sin confianza;
- conversión a grados y signos; errores de argumentos y tamaños; un **giro de 90 grados** (12 cuadros de 7.5°) suma 90.0°; con un cuadro tapado la referencia se conserva y se recuperan los 90.0° completos (el cuadro siguiente abarca 2 cuadros); rayas que se repiten:
  ambiguas sin predicción y aceptadas con una predicción que coincide.

`pruebas_navegador.c` (con un carro simulado cuyo giro real no coincide con la calibración y una cámara simulada que le avisa lo que gira):

- carro que gira 20 % menos de lo calibrado: **sin cámara 72°, con cámara 92°** (92 si se pidieron 90); 25 % más: sin cámara 112°, con cámara 93°;
- giro a la izquierda y al esquivar (268° contra 270°); con la cámara perdida todo el giro (queda en 72°, por tiempo); con la cámara solo al final;
  con la cámara perdida 300 ms a mitad del giro (queda en 92°); una cámara que mide 10 % de más (84° en vez de 82°, mejor que sin cámara); un
  carro muy flojo (35 %): el giro se corta por el tiempo de respaldo y la ruta termina; los cuadros durante el arranque se ignoran.

## PENDIENTE HARDWARE

1. **Con el teléfono solo** (sin carro): pantalla de visión → gira el teléfono a mano 90° con una marca → «Poner el ángulo acumulado en 0» →
   el ángulo acumulado debe marcar cerca de 90° con una escena con detalles (y no avanzar con una pared lisa: «sin confianza»).
2. Hacer el **paso 6 de la calibración** con el carro (y la cámara que se vaya a usar) y ver el FOV que sale.
3. Con la cámara del carro (Freenove) cuando la app ya reciba su video: el giro visual hay que medirlo sobre los cuadros de 240x176 que
   llegan por WiFi, poniendo `ponerLatenciaVisual` con el retraso medido (~320 ms).
4. Una ruta con giros reales (cuadrado) con y sin cámara, midiendo los grados de cada giro.

## Límites

- **Retraso de la cámara**: el cuadro de la cámara del carro llega con ~0.3 s de retraso (docs/latencia.md): lo que mide es lo que pasó hace 0.3 s.
  Por eso el navegador tiene `navegador_poner_latencia_visual(ms)` (JNI: `NavegadorNativa.ponerLatenciaVisual`): los cuadros que llegan antes
  de `arranque + latencia` todavía muestran al carro sin girar y no cuentan, y el pedazo de giro que la cámara aún no ve (los últimos ~320 ms) se
  completa por tiempo con los ms por grado de la calibración. Probado en PC con un retraso de 320 ms: un carro 20 % más lento que lo calibrado
  llega a 108° SIN compensar y a 84° compensando (pidiendo 90°); un carro 25 % más rápido, a 137° contra 100°. **Con la cámara del teléfono la
  latencia es 0 (valor por defecto); cuando la app reciba el video de la Freenove hay que poner ~320 ms** (el valor real se mide con el cronómetro).
- Con escenas lisas (una pared blanca) o muy repetitivas la cámara no ayuda: el giro sigue por tiempo.
- Si algo se mueve en el cuadro (una persona caminando), el corrimiento se contamina: por eso se pide un rechazo por ambigüedad y confianza.
- Solo mide giros en un plano horizontal (el horizonte); si el carro se inclina, la huella cambia.
- La huella usa la mitad de arriba: con una cámara muy inclinada hacia el piso esa mitad no ve el cuarto.
