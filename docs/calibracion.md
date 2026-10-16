# Calibración del carro

El carro no tiene encoders, IMU ni GPS: el navegador sabe cuánto avanzó o giró **solo por el
tiempo** que le mandó el comando. Para convertir "avanza 40 cm" o "gira 90 grados" en milisegundos
de motor hay que **medir el carro real**. Estas medidas cambian con el peso, el piso y la batería:
hay que repetirlas si algo de eso cambia.

Código: `nucleo_c/calibracion.c/.h` (C puro, probado en PC: `nucleo_c/pruebas_calibracion.c`) y en la
app `CalibracionActivity` (asistente paso a paso), `CalibracionNativa`, `CalibracionGuardada`.

## Qué se mide

1. **ARRANQUE** (ms): el tiempo entre ordenar avanzar y que el carro realmente se mueve (WiFi +
   firmware + motores que vencen la inercia). Durante ese tiempo el carro no avanza.
2. **MS_POR_CM** (ms por cm): cuánto tiempo de motor hace falta para avanzar 1 cm en línea recta.
3. **FRENADO** (cm): cuánto sigue avanzando el carro después del STOP.
4. **MS_POR_GRADO** (ms por grado) **de cada lado**: izquierda y derecha por separado, porque el giro se
   hace con un solo motor y no es simétrico. También se mide cuántos grados sigue girando tras el STOP.
5. **MS_POR_CM_ATRÁS** (opcional): cuánto tiempo de motor hace falta para RETROCEDER 1 cm. Lo usa el reintento de esquive (el carro
   retrocede 15 cm, ver `docs/navegador.md`). Si no se mide se usa el del avance y la app avisa.

El modelo es:  `distancia = (tiempo − arranque) × velocidad + frenado`  (y lo mismo con grados).

## Procedimiento (lo guía la app: pantalla principal → «Calibración del carro»)

Necesitas: el carro en el piso donde va a andar y con la batería cargada, ~2 m libres, una cinta
métrica y cinta adhesiva para marcar el piso (salida del avance, y marcas de 90 grados para los giros).
Conecta el carro (ws://192.168.4.1/ws por defecto). El botón **STOP** de la pantalla siempre detiene el carro.

1. **Reacción** (3 veces): toca el botón grande cuando se ponga verde. Sirve para descontar tu tiempo de
   reacción del paso siguiente.
2. **Arranque** (5 veces): toca «Probar» (el carro recibe FORWARD) y toca «YA SE MOVIÓ» en cuanto veas
   moverse las ruedas. Arranque = promedio de los toques (sin datos raros) − tu reacción.
3. **Avance**: el carro avanza 1000, 2000 y 3000 ms, **3 veces cada tiempo** (9 pruebas). Después de cada una,
   espera a que se detenga **por completo**, mide con la cinta cuántos cm avanzó (de la marca de salida
   a las ruedas) y escríbelo. Vuelve a poner el carro en la marca cada vez.
4. **Giro a la izquierda** y **Giro a la derecha**: igual, con 500, 1000 y 1500 ms (3 veces cada uno).
   Alinea el carro con una marca de 90 grados y anota los grados que giró, aproximados.
5. **Retroceso (opcional)**: se mide **igual que el avance**, pero el carro recibe REVERSE (1000, 2000 y 3000 ms, 3 veces cada uno). Deja
   unos **80 cm libres detrás** del carro (no ve hacia atrás), mide cuántos cm retrocedió y escríbelo. Si no quieres medirlo toca «Siguiente».
6. **Giro con la cámara (opcional)**: calibra el campo de visión de la cámara para medir el giro con ella (`docs/giro_visual.md`): el carro gira 90°
   a la derecha según la calibración mientras la cámara mide; tú escribes los grados que giró de verdad (hasta 3 pruebas).
7. **Resumen**: la app calcula todo en C y muestra los valores. «Usar estos valores» los guarda y el
   navegador los usa en la próxima ruta. «Copiar» deja el texto en el portapapeles. «Borrar» vuelve a los
   valores por defecto.

«Borrar el último dato / repetir» quita el último número si te equivocaste. Si una medida sale muy
rara, repítela: el cálculo descarta los datos que se alejan mucho de los demás (más de 30 % de la
mediana), pero no puede arreglar varias medidas malas.

### Cómo se calcula

- **Promedio sin atípicos** de las 3 repeticiones de cada tiempo (se descarta el dato que se aleja más de
  30 % de la mediana).
- **Recta por mínimos cuadrados** de distancia contra tiempo con los 3 tiempos. La **pendiente** son los
  ms por cm (no la ensucian ni el arranque ni el frenado). El **frenado** es lo que sobra al llegar al
  instante t = arranque. Por eso conviene medir bien el arranque primero: si se equivoca 100 ms, el
  frenado se equivoca unos 2 cm.
- **Conversión** que usa el navegador: `ms = arranque + (distancia − frenado) × ms_por_cm`. Si la distancia
  es menor que el frenado, no se pide motor (el carro llega solo). Un avance que sigue en línea recta
  con el tramo siguiente no repite el arranque ni descuenta el frenado.
- La posición estimada suma el frenado al soltar cada comando, y al reanudar tras una detención por
  obstáculo se compensa el arranque.

## Ajuste fino: giro de 90° (ajustes del 15 oct)

**Qué pasó.** En la prueba real los giros de «90°» salían de unos 110°. Se revisó el camino completo del tiempo de giro y estaba bien:

- **La calibración que usa es la del perfil AUTO (210).** La pantalla de calibración pide siempre AUTO y no deja probar hasta que el carro confirma `ACK_AUTO`; al abrir una ruta
  (`EjecutarRutaFragment.onStart`) la app le pasa al navegador la calibración guardada. Los pivotes van a 255 en los dos perfiles (el perfil solo cambia avanzar y retroceder), así que
  la medida del giro no depende del perfil. Lo que sí envejece es **la medida**: con pilas o contactos nuevos el carro gira más rápido y los 8.2 ms/° de la calibración del 14 de octubre ya
  no valen (por ejemplo, a un carro de 8.2 ms/° que se le piden 900 ms, los 10 ms/° por defecto, gira ~110°). No se pudo comprobar con el carro cuál fue la causa exacta; por eso se agrega el ajuste.
- **La compensación de frenado se aplica UNA sola vez por giro**: `cal_grados_a_ms` descuenta los grados que el carro sigue girando al soltar (`frenado_grados`) al pedir el tiempo;
  `cal_ms_a_grados` no los cuenta y `sumar_frenado` los agrega a la pose una vez al soltar el comando. Con la cámara (giro visual) el objetivo también los descuenta una sola vez
  (y el respaldo por tiempo usa el tiempo ya descontado, sin repetirlo). Probado en `pruebas_calibracion.c` y `pruebas_navegador.c`.
- **La pausa de 300 ms** (`NAV_PAUSA_ASENTAR_MS`) es la misma entre todos los movimientos: entre avanzar y girar o dos giros distintos al seguir la ruta, después de retroceder, al
  enderezarse, antes de calcular la vuelta a la línea y entre los tres movimientos de la vuelta.

**Qué se agregó: «Giro 90°».** Después del resumen (botón «Ajuste fino: giro y recta»), o directo desde la primera pantalla («Solo el ajuste fino»), hay un paso por prueba. Parte de la calibración
**guardada** (si no hay una completa, avisa y no deja probar).

1. Elige el **perfil** (AUTO por defecto; MANUAL también se puede) y el **lado** (izquierda o derecha).
2. Toca «Probar»: el carro gira lo que la calibración (con el factor de ese lado y perfil) dice que son 90° y se detiene.
3. Di cómo giró: **«Se pasó mucho»** (se supone 120°), **«Se pasó un poco»** (105°), **«Quedó justo»** (90°) o **«Se quedó corto»** (78°), o escribe los **grados** que giró y toca «Aplicar los grados».
4. El **factor** de ese lado y perfil se corrige: `factor nuevo = factor × 90 / grados que giró`, dentro de 500 a 1500 milésimas (1000 = sin corrección). Con 110° por 90°, 1000 pasa a 818 y el
   giro de 90° dura 18 % menos. Repite hasta que quede justo; el factor nuevo parte del anterior. Con solo las opciones, cuatro pasadas dejan un carro que se pasa 22 % en 90 ± 10°; escribiendo
   los grados, dos pasadas dejan 90 ± 3° (probado en `pruebas_calibracion.c`).
5. **«Restaurar valor por defecto»** vuelve los dos lados del perfil a 1000.

El factor **no cambia la calibración medida**: se guarda aparte, por lado y por perfil, en el teléfono (`giro_factor_auto_izq`, `giro_factor_auto_der`, `giro_factor_manual_izq`, `giro_factor_manual_der` en las
preferencias «calibracion»), y se aplica a los ms por grado cuando hacen falta: `CalibracionGuardada.leerAjustada()` (código en C: `cal_con_factor_giro`) es lo que reciben el **navegador** al abrir una
ruta, el **simulador** de la pantalla Rutas y la propia prueba del giro. Así la simulación y el carro usan los mismos valores. Si se vuelve a medir el giro con la calibración completa y se guarda, los
factores vuelven a 1000 (los giros nuevos ya traen el efecto).

**Cuándo hacerlo.** Hoy se cambiaron pilas y contactos, así que la calibración vieja puede estar desfasada. La recalibración real se hace al final, con las pilas de la demo (primero la calibración
completa y después este ajuste, que tarda un minuto). Si cambian las pilas otra vez, repite el ajuste.

## Desplazamiento del centro al girar (J4, 16 oct)

**Qué pasó.** En la prueba real, con un giro de 1500 ms a la derecha (~200°) **todo el chasis se desplaza varios cm** y las ruedas traseras tampoco quedan fijas: con ruedas mecanum el carro no
gira sobre un punto fijo (además aquí el giro se hace con un solo lado, ver `firmware_carro/carro.c`: LEFT usa solo el motor derecho y RIGHT solo el izquierdo, así que el carro pivota alrededor de
las ruedas que quedan quietas y el centro describe un arco). El navegador estimaba la pose suponiendo giro en el sitio, así que **cada giro metía error de POSICIÓN** (no de ángulo). Un esquive tiene 3 o 4 giros.

**El modelo** (`nucleo_c/calibracion.c`: `cal_desplazamiento_*`). Por **lado de giro** (izquierda y derecha) y por **perfil** (AUTO y MANUAL) hay dos números, en **micrómetros por grado girado** (µm/°):

- `adelante` (negativo = atrás): cuánto se mueve el centro hacia donde apunta el carro;
- `derecha` (negativo = izquierda): cuánto se mueve hacia su derecha.

Están medidos **en el marco del carro a mitad del giro** (rumbo inicial + θ/2 hacia el lado del giro). Durante un giro de θ grados el navegador suma `θ × (adelante, derecha)` a la posición, rotado a ese
rumbo medio. Por defecto los cuatro valen 0: el carro gira en su sitio, como antes. Es una **aproximación lineal**: el desplazamiento real no es exactamente proporcional al ángulo, así que el modelo
vale mejor para giros parecidos al que se usó al medirlo (por eso la prueba usa por defecto el «Giro de 90° real», el comando que más usa el navegador). **El desplazamiento depende del piso (agarre de las
ruedas), de las pilas y varía de un giro a otro**: es un promedio, no una constante física. Se vuelve a medir si cambia el piso o las pilas.

Ejemplo hecho a mano (el mismo de `pruebas_calibracion.c`): giro de 90° a la derecha con 1 mm de desplazamiento hacia adelante por grado (1000 µm/°), carro mirando hacia arriba (rumbo 0). El rumbo medio
es 45°; el centro se mueve 90 mm en esa dirección: 90 × 0.707 = **64 mm a la derecha y 64 mm hacia arriba**. En el marco del carro al empezar el giro: 64 mm adelante y 64 mm a la derecha.

**La prueba guiada** (pantalla Calibración → «Desplazamiento al girar» → «Calibrar solo esto»; una prueba por pantalla):

1. Marca el centro del carro con cinta en el piso (y alinea el carro con una junta de loseta, que sirve de guía para los ángulos).
2. Elige el **perfil** (AUTO por defecto), el **lado** y el **giro de prueba**: «Giro de 90° real» (por defecto: el mismo comando que usa el navegador, con la calibración y el ajuste fino guardados), «Giro de 180°» o
   «Duración libre» de 500 a 1500 ms.
3. Toca «Girar»: el carro gira y se detiene. Mide cuánto se movió el **centro** (cinta nueva donde quedó) respecto de donde estaba, en el marco de la posición INICIAL del carro: **adelante o atrás** y **izquierda o
   derecha** (con − y +, de 0.5 cm en 0.5 cm), y anota el **ángulo final** que giró (botones de 45° en 45°; las juntas de la loseta sirven de guía).
4. «Guardar esta repetición». Son **2 repeticiones por lado**. La app calcula los mm por grado de cada una (en C: `cal_desplazamiento_de_medida`, que pasa la medida del marco inicial al marco de mitad de giro y la
   reparte entre los grados), las **promedia** y **rechaza** las dos si difieren más de **30 %** (la distancia entre los dos vectores contra el mayor de los dos; con valores casi cero, menos de 60 µm/° de
   diferencia —5 mm en un giro de 90°, menos que lo que se mide con cinta— no se rechaza). Si se rechazan, se borran y se repite.
5. Al aceptarse, se **guarda por lado y perfil** (no hace falta hacer el otro lado). **«Restaurar valor por defecto»** vuelve los dos lados del perfil a 0.

**Dónde se guarda.** No va en `calibracion.txt` (ese archivo trae lo medido del carro): igual que el factor del giro, vive en las preferencias de la app (`desp_adelante_auto_izq`, `desp_derecha_auto_izq`,
`desp_adelante_auto_der`, `desp_derecha_auto_der` y las cuatro de `manual`), junto con con qué giro de prueba se midió cada lado. `CalibracionGuardada.leerAjustada()` los pone en la calibración que reciben el
**navegador**, el **simulador** de la pantalla Rutas y las pruebas, igual que el factor del giro. La calibración viaja a C como 14 enteros (los 4 últimos son estos).

**Qué NO hace.** No corrige el **ángulo** (eso es el giro de 90° y los ms por grado) ni cambia lo que el navegador *planea* (los giros y avances de la vuelta a la línea se siguen calculando como si cada giro fuera
en el sitio; con el modelo la pose *estimada* es la correcta y el siguiente plan parte de ella). Ver `docs/navegador.md`, «Desplazamiento del centro al girar».

## Dónde se guarda

En el teléfono, como **texto** (`clave=valor`, una por línea): SharedPreferences ("calibracion") y el archivo
`calibracion.txt` en la carpeta de la app. Para sacarlo a la PC:

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" pull /sdcard/Android/data/com.fsi.carro/files/calibracion.txt .
```

(no se sube al repositorio). Ejemplo del contenido:

```
medidos=15
arranque_ms=140
ms_por_cm_x10=420
frenado_mm=25
ms_por_grado_izq_x10=90
ms_por_grado_der_x10=115
frenado_grados_izq_x10=60
frenado_grados_der_x10=40
fov_grados_x10=600
ms_por_cm_atras_x10=420
```

`_x10` quiere decir "por 10": `ms_por_cm_x10=420` son 42.0 ms por cm. `medidos` suma bits: 1 arranque, 2 avance,
4 giro izquierdo, 8 giro derecho (15 = todo medido), 16 el campo de visión de la cámara y 32 el retroceso (los dos últimos son opcionales y no cuentan para «todo medido»). `fov_grados_x10` es el campo de visión
horizontal de la cámara (600 = 60.0 grados); `ms_por_cm_atras_x10` son los ms por cm hacia atrás (vale solo con el bit 32; si no, se usa `ms_por_cm_x10`). Los textos guardados
sin esas líneas se siguen leyendo.

## Ajuste fino: recta (equilibrio de los lados)

El segundo paso del ajuste fino («Siguiente: la recta») corrige que el carro se tuerza al avanzar: se hace sobre una línea de cinta, «Avanzar 4 s», se dice hacia dónde se desvió y cuántos cm, y la app calcula y manda al carro el **balance**
del lado izquierdo (800 a 1000), por perfil. Explicación, protocolo, geometría y pruebas en `docs/perfiles_velocidad.md`. Igual que con el giro, se hace con las pilas de la demo («Recalibra con las pilas que usarás en la demo»).

El factor de giro **no** está en `calibracion.txt` (ese archivo solo trae lo medido); vive en las preferencias de la app.

## Si no se calibró

El navegador usa los valores por defecto (`MS_POR_CM` 50, `MS_POR_GRADO` 10, sin arranque ni frenado) y la
pantalla de control (modo semiautónomo) muestra un aviso con lo que falta: «Carro sin calibrar (...)».

## Pruebas en PC (datos simulados con ruido de ±10 %)

`cd nucleo_c; make test` (92 casos en `pruebas_calibracion.c`, y casos de calibración en `pruebas_navegador.c`):

- estadística (promedio, mediana, atípicos), validez, ajuste con datos exactos y con errores (tiempos iguales,
  datos que decrecen, pendiente absurda), conversiones de ida y vuelta, texto exportado e importado;
- **simulación completa del procedimiento con ruido de ±10 %** en cada medida (400 carros simulados, con
  muchas semillas): el ms por cm sale con error medio de 3 % y en el peor caso 15 %; pidiendo 40 cm con la
  calibración calculada, el carro real avanza 40 cm con un error máximo de 6 %. Con un 10 % de medidas
  disparatadas (un dato multiplicado por 3) el rechazo de atípicos mantiene 398 de 400 calibraciones dentro de 15 %;
- navegador con un carro simulado que tiene arranque, frenado y giros distintos a cada lado: sin calibrar queda a
  ~185 mm del destino de una ruta recta de 1 m; con su calibración, a 6 mm; zigzag y cuadrado con giros a ambos
  lados llegan al destino; una detención a mitad de camino y reanudación también.

## PENDIENTE HARDWARE

Todo lo que necesita el carro real:

1. Instalar la app en el teléfono y hacer el procedimiento completo con el carro (los números de arriba son
   de un carro simulado, no del real).
2. Revisar que la reacción y el toque de arranque dan datos parecidos entre repeticiones (si no, la medida del
   arranque a mano es poco confiable; ver la limitación).
3. Probar una ruta (cuadrado) con el navegador y la calibración guardada y medir cuánto se desvía.

## Límites

- **El arranque se mide con tu reflejo**: aunque se descuente tu reacción, el error es de unos ±50 ms (≈ 1 cm a
  20 cm/s). Una medición mejor sería con la cámara (ver `docs/giro_visual.md`; la cámara ya puede corregir los giros).
- Los grados de los giros son aproximados (a ojo, con marcas de 90°): error de unos 5 a 10°.
- El carro cambia con la batería: hay que recalibrar cuando se descarga, cambia el piso o el peso.
- Se supone que el arranque es el mismo para avanzar y para girar.

## Valores medidos en el carro real (leídos de `calibracion.txt` del teléfono)

| Valor | 13 oct, motores a 255 | 14 oct 09:10, perfil AUTO (210) |
|---|---|---|
| `medidos` | 15 | 47 (todo + retroceso) |
| arranque | 0 ms | 0 ms |
| avance | 26.3 ms/cm (38 cm/s) | **51.7 ms/cm (19.3 cm/s)** |
| frenado al avanzar | 0.9 cm | **1.4 cm** |
| giro izquierda | 7.8 ms/° | **12.0 ms/°** |
| giro derecha | 7.4 ms/° | **8.2 ms/°** |
| frenado del giro izq. / der. | 0 / 0 ° | **38.9° / 0°** |
| retroceso | sin medir | **44.4 ms/cm (22.5 cm/s)** |
| campo de visión | 60° (sin calibrar) | 60° (sin calibrar) |

Notas:
- El avance y el retroceso bajaron al ir a 210 (los pivotes siguen a 255). Con el retroceso medido, los reintentos de esquive retroceden
  15 cm con 44.4 ms/cm.
- **Los giros no deberían haber cambiado** (los pivotes siguen a 255) y cambiaron mucho: el izquierdo pasó de 7.8 a 12.0 ms/° y su «frenado»
  de 0° a 38.9°. Eso parece ruido de la medición (pocas pruebas, o la batería) y no una propiedad del carro. Con 38.9° de frenado el navegador
  pide un giro izquierdo de 90° de solo ~610 ms, y si el frenado real es ~0 el carro se quedará corto. **Conviene repetir el giro a la
  izquierda** (paso 4 de la calibración) antes de confiar en las rutas con giros.
- Cm hasta frenar con estos valores: ver `docs/latencia.md` (8.5 cm con el obstáculo cerca, a 210).

## Frenado de giro imposible (15 oct)

`cal_ajustar_giro` rechaza (CAL_ERR_RANGO) un ajuste cuyo frenado de giro pase de 20° (`FRENADO_GRADOS_AJUSTE_MAX_X10`). El teléfono tenía guardado
`frenado_grados_izq_x10=389` y `frenado_grados_der_x10=0` con el mismo carro: no es inercia, es un ajuste malo, y hacía que un giro de 45° a la izquierda se
ordenara por 82 ms mientras la pose estimada sumaba 39° que no ocurrían. Una calibración ya guardada con ese valor sigue siendo válida al cargarse
(`cal_es_valida` no cambió); hay que repetir la medición del giro izquierdo.
