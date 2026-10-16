# Calibración del carro

El carro no tiene encoders, IMU ni GPS: el navegador sabe cuánto avanzó o giró **solo por el
tiempo** que le mandó el comando. Para convertir "avanza 40 cm" o "gira 90 grados" en milisegundos
de motor hay que **medir el carro real**. Estas medidas cambian con el peso, el piso y la batería:
hay que repetirlas si algo de eso cambia.

Código: `nucleo_c/calibracion.c/.h` (C puro, probado en PC: `nucleo_c/pruebas_calibracion.c`) y en la
app `CalibracionActivity` (la lista de partes y cada prueba), `CalibracionNativa`, `CalibracionGuardada`.

## Qué se mide

1. **ARRANQUE** (ms): el tiempo entre ordenar avanzar y que el carro realmente se mueve (WiFi +
   firmware + motores que vencen la inercia). Durante ese tiempo el carro no avanza.
2. **MS_POR_CM** (ms por cm): cuánto tiempo de motor hace falta para avanzar 1 cm en línea recta.
3. **FRENADO** (cm): cuánto sigue avanzando el carro después del STOP.
4. **MS_POR_GRADO** (ms por grado) **de cada lado**: izquierda y derecha por separado, porque el giro se
   hace con un solo motor y no es simétrico. También se mide cuántos grados sigue girando tras el STOP.
5. **MS_POR_CM_ATRÁS** (opcional): cuánto tiempo de motor hace falta para RETROCEDER 1 cm. Lo usa el reintento de esquive (el carro
   retrocede 15 cm, ver `docs/navegador.md`). Si no se mide se usa el del avance y la app avisa.

6. **DESPLAZAMIENTO DEL CENTRO AL GIRAR** (opcional, por lado y por perfil): cuánto se mueve el centro del carro por cada grado que gira (ruedas mecanum). Ver su sección.

El modelo es:  `distancia = (tiempo − arranque) × velocidad + frenado`  (y lo mismo con grados).

## Procedimiento (lo guía la app: Más → «Calibración del carro»)

**Calibración por partes (J5, 16 oct).** Antes había un asistente de una sola tirada y quien quería repetir una cosa sentía que debía repetir todas. Ahora la pantalla es **una lista de partes
independientes**: cada tarjeta muestra el **valor actual**, la **fecha y hora de la última calibración**, si **depende de las pilas** («Repite si cambiaste pilas») y un botón **«Calibrar solo esto»**.

| Parte | Qué mide | Cuánto toma | Depende de las pilas |
|---|---|---|---|
| Arranque del motor | reacción (3 toques) + arranque (5 toques) | 1 min | sí |
| Velocidad de avance | ms por cm: avanza 1, 2 y 3 s, 3 veces cada uno (9 pruebas) | 5 min | sí |
| Frenado de avance | cm que sigue avanzando tras el STOP: avanza 2 s, 3 veces | 2 min | sí |
| Giro a la izquierda / a la derecha | ms por grado de ese lado (y el frenado del giro con la prueba de tres tiempos) | 2 min (3 giros) | sí |
| Retroceso | ms por cm hacia atrás: igual que la velocidad pero en reversa | 5 min | sí |
| Giro con la cámara | campo de visión de la cámara (`docs/giro_visual.md`) | 3 min | no |
| Desplazamiento al girar | cuánto se mueve el centro al girar (más abajo) | 5 min | sí |
| Ajuste fino del giro de 90° | factor del giro por lado y perfil (más abajo) | 1 min | sí |
| Ajuste fino de la recta (balance) | balance del lado izquierdo por perfil (`docs/perfiles_velocidad.md`) | 2 min | sí |

Reglas que se cumplen en todas (están en `CalibracionActivity.kt`):

- **Cada prueba es independiente**: no exige haber hecho otra antes (el ajuste fino del giro de 90° y de la recta ya no piden una calibración completa; sin giro medido usan el valor de ejemplo y lo dicen), **no borra
  los otros valores** (solo cambia lo suyo: la velocidad no toca el frenado ni el arranque; el frenado solo el frenado; un giro solo ese lado) y **guarda al terminar**.
- **Una prueba cancelada no cambia nada**: salir de la parte (botón «Cancelar», la flecha de la barra de título o el botón atrás del teléfono) descarta lo medido y los valores guardados quedan igual. Si ya mediste algo
  pide confirmación. Las partes que se ajustan de a poco (desplazamiento, giro de 90° y recta) guardan en cada repetición y se cierran con «Listo».
- Las demás terminan en un **resultado** que dice qué cambia (antes → después) con «Guardar esta calibración». Antes de guardar, el código en C revisa que los ms por grado de cada lado estén entre 3 y 20 (más abajo).
- **«Calibrar todo, en orden»** recorre las partes una tras otra (arranque, velocidad, frenado, giro izquierdo, giro derecho, retroceso, desplazamiento, giro de 90° y recta; el giro con la cámara queda aparte
  porque pide la cámara abierta). Cada parte se guarda sola al terminar y «Omitir esta parte» salta una; si abandonas a la mitad, lo ya guardado se queda guardado.
- Al final de la lista, «Copiar los valores como texto» (el texto de `calibracion.txt`) y «Borrar la calibración guardada» (con confirmación; no toca el ajuste fino, el desplazamiento ni el balance).

Necesitas: el carro en el piso donde va a andar y con la batería cargada, ~2 m libres, una cinta métrica y cinta adhesiva para marcar el piso (salida del avance, y marcas de 90 grados para los giros).
Conecta el carro (ws://192.168.4.1/ws por defecto). El botón **STOP** de la pantalla siempre detiene el carro.

Cómo se hace cada parte:

1. **Arranque**: *reacción* (3 veces): toca el botón grande cuando se ponga verde; sirve para descontar tu tiempo de reacción. *Arranque* (5 veces): toca «Probar» (el carro recibe FORWARD) y toca «YA SE MOVIÓ» en
   cuanto veas moverse las ruedas. Arranque = promedio de los toques (sin datos raros) − tu reacción. Si cambia más de 40 ms, el resultado avisa que la velocidad y los giros guardados se midieron con el arranque anterior.
2. **Velocidad de avance**: el carro avanza 1000, 2000 y 3000 ms, **3 veces cada tiempo** (9 pruebas). Después de cada una, espera a que se detenga **por completo**, mide con la cinta cuántos cm avanzó (de la marca de salida
   a las ruedas) y escríbelo. Vuelve a poner el carro en la marca cada vez. Solo se guardan los **ms por cm**: el frenado y el arranque que ya había se conservan.
3. **Frenado de avance**: el carro avanza 2000 ms, **3 veces**; escribe cuántos cm avanzó **en total**. Con la velocidad y el arranque guardados, lo que sobra sobre lo que debía avanzar es el frenado (`cal_ajustar_frenado`).
   Si el carro avanzó mucho *menos* de lo esperado (más de 3 cm), la velocidad guardada ya no vale y el resultado dice que calibres primero la velocidad.
4. **Giro a la izquierda** y **Giro a la derecha**: eliges el **giro de prueba** (siguiente sección), alineas el carro con una marca y anotas los grados que giró, aproximados.
5. **Retroceso**: se mide **igual que la velocidad**, pero el carro recibe REVERSE (1000, 2000 y 3000 ms, 3 veces cada uno). Deja unos **80 cm libres detrás** del carro (no ve hacia atrás), mide cuántos cm retrocedió y escríbelo.
6. **Giro con la cámara**: calibra el campo de visión de la cámara para medir el giro con ella (`docs/giro_visual.md`): el carro gira 90° a la derecha según la calibración guardada mientras la cámara mide; tú escribes los grados
   que giró de verdad (hasta 3 pruebas).
7. **Desplazamiento al girar**, **ajuste fino del giro de 90°** y **ajuste fino de la recta**: ver sus secciones.

«Borrar el último dato / repetir» quita el último número si te equivocaste. Si una medida sale muy rara, repítela: el cálculo descarta los datos que se alejan mucho de los demás (más de 30 % de la
mediana), pero no puede arreglar varias medidas malas.

### El giro de prueba (J5)

En las pruebas de **giro** (izquierda y derecha) y en la de **desplazamiento al girar** se elige con qué giro se prueba:

- **«Giro de 90° real»** (por defecto): el mismo comando que usa el navegador para un giro de 90°, con la calibración y el ajuste fino guardados (`gradosAMs(leerAjustada(), ±90)`). Es lo que más hace el navegador.
- **«180°»**: lo mismo con 180°. Mide mejor el desplazamiento grande, pero el arranque pesa menos que en un giro corto.
- **«Libre»**: una duración de 500 a 1500 ms (de 100 en 100). Dices cuántos grados giró.
- **Solo en el giro: «Completa: tres tiempos»**: la prueba de siempre (500, 1000 y 1500 ms, 3 veces cada uno = 9 pruebas, recta por mínimos cuadrados): es la única que mide además cuánto sigue girando el carro al frenar.

Con un solo giro de prueba son **3 repeticiones** y los **ms por grado salen de los grados que TÚ dices que giró**: `ms por grado = (ms − arranque) / (grados − frenado del giro)` (`cal_ajustar_giro_fijo`, con el arranque y el frenado
ya guardados; un frenado de giro de más de 20°, como el 38.9° que quedó guardado, se descarta al volver a medir ese lado). Ejemplo: un carro de 8 ms por grado con 150 ms de arranque gira 112.5° cuando se le piden 1050 ms; dices
112 y el ajuste da 8.0 ms por grado, con lo que el siguiente giro de 90° dura 870 ms y gira 90° justos (probado en `pruebas_calibracion.c`). Si lo mides más de una vez, cada pasada parte de la anterior.

**No se mezclan sin avisar.** Los ms por grado dependen de la duración con la que se midieron (el arranque pesa más en un giro corto). La app guarda con qué giro de prueba se midió cada lado (`CalibracionGuardada`,
`GiroPrueba`) y, si el otro lado se midió con una prueba distinta —o dos repeticiones de un lado se quieren hacer con pruebas distintas—, lo avisa en el panel de la prueba y en el resultado. Cambiar el giro de prueba a mitad
de una medición **borra** las repeticiones de ese lado (mezclarlas no sirve).

### Ms por grado entre 3 y 20 (J5)

Al **guardar** una calibración, `cal_validar_para_guardar` (en C) rechaza —y la app avisa, sin guardar nada— si los ms por grado de algún lado salen de **3 a 20** ms por grado (`CAL_MS_GRADO_GUARDAR_*`): un carro de este tamaño gira
~1° cada 7 a 10 ms; fuera de ese rango casi seguro se anotó mal un dato. Es más estricto a propósito que `cal_es_valida` (1 a 200), que solo protege al LEER lo ya guardado. Lo mismo aplica al ajuste fino del giro de 90°: si
el factor nuevo dejaría los ms por grado fuera de 3 a 20, no se guarda.

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

**Qué se agregó: «Giro 90°».** Es la parte «Ajuste fino del giro de 90°» de la lista (J5: ya no hace falta pasar por una calibración completa; parte de la calibración **guardada** y, si el giro
nunca se midió, usa el valor de ejemplo y lo avisa).

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

> **El giro es de PIVOTE y el centro se mueve (medido en el carro real, 16 oct).** `LEFT`/`RIGHT` mueven las ruedas de UN solo lado y las del otro quedan fijas. El giro sobre el eje (lados opuestos) ya se probó y
> el carro no se mueve por falta de par, así que se queda el pivote. Medido 6 veces, siempre igual: giro a la derecha, AUTO 210, giro de 90° real: el centro del carro se desplaza **15 cm adelante y 16.5 cm al lado**
> (22.3 cm en total, un arco de ~16 cm de radio), de **centro inicial a centro final**, en el marco del carro AL EMPEZAR el giro. «El carro avanza 22 cm mientras gira, no pivota sobre sí mismo.» Ese
> desplazamiento NO es un error que haya que quitar: **se compensa en la pose** (`calcular_pose` del navegador y el simulador suman el vector del lado que giró) y por eso hay que medirlo.
> **El límite de validez era demasiado chico.** La pantalla rechazaba esa medida: en `adb logcat -d -s TURN_DIAG:I` salía `resultado=medida_rechazada motivo=medida_no_valida codigo=-3 medida_mm=(150,165) grados=90`
> (código −3 = `CAL_ERR_RANGO`). Lo que se superó fue **`CAL_DESP_UM_MAX` = 2000 µm por grado por componente**: la medida da **2474 µm/° hacia adelante** en el marco de mitad de giro (22.3 cm / 90° y la cuerda va
> en la dirección del rumbo medio), un 24 % más. Ahora `CAL_DESP_UM_MAX` = **5000** y el vector entero tampoco pasa de `CAL_DESP_TOTAL_UM_MAX` = **5000 µm/°** (**45 cm de desplazamiento total en un giro de 90°**; 40 cm
> se aceptan). **Aviso no bloqueante** desde 15 cm (`CAL_DESP_AVISO_MM`): «Desplazamiento grande: confirma que mediste del centro inicial al centro final. El carro avanza X cm mientras gira (arco de unos R cm de
> radio), no pivota sobre sí mismo.» (aparece debajo de los contadores, sin moverlos, y queda en el resumen y en `TURN_DIAG`: `total_mm`, `radio_arco_mm`, `aviso=desplazamiento_grande`). El radio sale de la cuerda:
> `radio = total / (2 x sen(grados/2))` (`cal_desplazamiento_arco`; 15 y 16.5 cm en 90° dan 22.3 cm y 15.8 cm).
> **Marco de referencia (verificado):** «adelante/atrás» e «izquierda/derecha» son los del carro en su posición y orientación **INICIALES**; la app y el modelo los interpretan así (`cal_desplazamiento_de_medida` pasa
> la medida al marco de mitad de giro y `cal_desplazamiento_giro` la devuelve al marco inicial). Pruebas en C (`pruebas_calibracion.c`, 253 casos): con los cuatro rumbos iniciales (0, 90, 180, 270) el desplazamiento vuelve a
> dar 15 adelante y 16.5 a la derecha en el marco inicial y gira en el mapa ((165,−150), (150,165), (−165,150), (−150,−165)), y NO coincide con lo que se mediría en el marco final. Con 15 y 16.5 cm la pose final del
> navegador queda 16.5 cm a la derecha y 15 cm arriba (±3 mm; el simulador igual) y un esquive completo con estos datos termina a **267 mm** de la línea con el modelo contra **613 mm** sin él (la pose estimada coincide con la
> real a 3 mm; lo que queda es la geometría de reincorporación del navegador con giros de 22 cm).
> **Límite del modelo:** es **lineal en el ángulo** (µm por grado). Un pivote real da una cuerda de `2R sen(θ/2)`, que no es lineal: medido a 90°, el modelo subestima ~8 % a 45°, sobrestima ~15 % a 135° y ~41 % a 180°
> (con R = 15.8 cm: unos 13 cm de más en un giro de 180°). Por eso la medida se hace con el giro de prueba más parecido al que se usa (90° real por defecto). Si hace falta exactitud en otros ángulos, el siguiente paso es
> guardar el punto de pivote y calcular la rotación alrededor de él (hecho en J8: ver «Modelo de pivote» al final; la frase de arriba sobre el modelo lineal describe cómo era antes).

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
   reparte entre los grados), las **compara** y, si coinciden, las **promedia**. Coinciden si la distancia entre los dos desplazamientos es de **30 % o menos del mayor, O de 1 cm o menos** (se aplica la MAYOR de las dos tolerancias:
   con medidas chicas hechas con cinta manda el 1 cm; con medidas grandes, el 30 %). La comparación se hace en cm, al ángulo medio de las dos repeticiones (en C: `cal_desplazamiento_promediar`).
   **Si no coinciden NUNCA se borra nada en silencio**: sale un cuadro con el motivo exacto («repetición 1: 1.5 cm, repetición 2: 3.5 cm, difieren 2.0 cm (se acepta hasta 1.1 cm: el 30 % o 1 cm, lo que sea mayor)») y
   tres opciones: **Repetir la medición** (descarta las dos y empieza otra vez), **Guardar el promedio de todos modos** (avisa que la calibración será menos confiable) y **Cancelar** (no se guarda nada y lo guardado antes sigue igual).
   El cuadro no se cierra tocando fuera: hay que elegir. Si la medida de UNA repetición no sirve (por ejemplo, sale de rango), tampoco se pierde: se queda con los contadores para corregirla y volver a guardar.
   Si se elige guardar de todos modos, el ítem «Desplazamiento al girar» de la lista se marca **«Poca precisión»** con el texto «Calibrado con poca precisión (AUTO derecha): las repeticiones no coincidían»; la marca
   se quita sola la próxima vez que ese lado se calibra con dos repeticiones que sí coinciden, o con «Restaurar valor por defecto». Cada paso queda en el registro `TURN_DIAG` (ver `docs/navegador.md`).
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

La parte «Ajuste fino de la recta (balance)» de la lista corrige que el carro se tuerza al avanzar (ya no pide una calibración completa antes): se hace sobre una línea de cinta, «Avanzar 4 s», se dice hacia dónde se desvió y cuántos cm, y la app calcula y manda al carro el **balance**
del lado izquierdo (800 a 1000), por perfil. Explicación, protocolo, geometría y pruebas en `docs/perfiles_velocidad.md`. Igual que con el giro, se hace con las pilas de la demo («Recalibra con las pilas que usarás en la demo»).

El factor de giro **no** está en `calibracion.txt` (ese archivo solo trae lo medido); vive en las preferencias de la app. Igual que el desplazamiento al girar, el balance, **la fecha y hora de la última calibración
de cada parte** (`fecha_velocidad`, `fecha_giro_izq`, `fecha_giro_90_auto`...: milisegundos desde 1970) y **con qué giro de prueba se midió cada lado** (`giro_prueba_izq`, `giro_prueba_der`: `90`, `180`,
`libre:1000` o `tres`; `desp_prueba_*` para el desplazamiento). Una parte medida antes de que la app anotara las fechas aparece como «medido, sin fecha» hasta que se vuelva a calibrar.

## Una ruta que no giró «una vez de tres» (J5, 16 oct)

**Qué pasó.** En la prueba real, una ruta de 40 cm a la derecha con 1 giro giró bien 2 de 3 veces; en una avanzó recto. Probablemente es **contacto eléctrico intermitente** (hardware), así que **no se rediseñó el navegador**.
Solo se agregó lo necesario para separar «el programa mandó mal el comando» de «el comando fue correcto y el carro no lo hizo»:

1. **Simulación en C** (`pruebas_navegador.c`, `pruebas_ruta_derecha_40cm`): la ruta `(2,8) → (3,8) → (4,8)` (40 cm a la derecha, 1 giro) manda **RIGHT como primer comando, en el primer tick (t = 0)**, durante **900 ms**
   con la calibración de ejemplo (10 ms por grado) o **738 ms** con 8.2 ms por grado (la medida del 14 oct); después una pausa de 300 ms y FORWARD, nunca FORWARD antes del giro y un solo giro en toda la ruta.
2. **Registro `RUTA_DIAG`** (log de la app: `adb logcat -d -s RUTA_DIAG:I`): una línea al **enviar** cada comando de movimiento y otra al **terminarlo**:
   ```
   RUTA_DIAG envia cmd=RIGHT origen=ruta pedido=90 plan_ms=900 t_ms=18420
   TURN_DIAG lado=DER origen=ruta pedido=90 hecho=90 plan_ms=900 real_ms=900 um_grado=(0,0) mm=(0,0) carro_mm=(0,0) rumbo=0>90
   RUTA_DIAG fin cmd=RIGHT origen=ruta pedido=90 plan_ms=900 real_ms=900 hecho=90
   ```
   `pedido` son grados (giros) o mm (avance y retroceso); `plan_ms` la duración planeada y `real_ms` la real (si se cancela a mitad, menos); `hecho` lo estimado (grados o mm). Un tramo que sigue en línea recta hasta el
   siguiente punto de la ruta tiene su propio envío y fin.
3. **Pantalla de resultado** («Ejecutar ruta» al terminar o cancelar): bajo el resultado, la duración y los obstáculos esquivados, **«Giro pedido / Giro hecho (estimado)»**, una línea por giro: `Giro derecha: pedido 90°,
   hecho ~90°` (con el origen entre paréntesis si fue al esquivar o al volver a la línea). «Hecho» es lo que el **navegador estima** (por tiempo, o con la cámara si era confiable): no mide el carro real.
4. **Cómo leerlo.** Si el log de la corrida que falló dice `envia cmd=RIGHT ... pedido=90 plan_ms=...` y `fin ... real_ms=` cercano al planeado, **el programa mandó bien el giro**: el fallo está en el carro (contacto en el motor,
   conector, pilas), no en el software; se anota en `docs/bitacora.md` como «no software». Si falta el `envia cmd=RIGHT` o el primer comando fue FORWARD, el fallo sí es del programa. **PENDIENTE HARDWARE**: repetir
   la ruta de 40 cm a la derecha varias veces con el teléfono conectado y guardar el log de una corrida que falle.

## Si no se calibró

El navegador usa los valores por defecto (`MS_POR_CM` 50, `MS_POR_GRADO` 10, sin arranque ni frenado) y la
pantalla de control (modo semiautónomo) muestra un aviso con lo que falta: «Carro sin calibrar (...)».

## Pruebas en PC (datos simulados con ruido de ±10 %)

`cd nucleo_c; make test` (212 casos en `pruebas_calibracion.c`, y casos de calibración en `pruebas_navegador.c`, 234):

- estadística (promedio, mediana, atípicos), validez, ajuste con datos exactos y con errores (tiempos iguales,
  datos que decrecen, pendiente absurda), conversiones de ida y vuelta, texto exportado e importado;
- **simulación completa del procedimiento con ruido de ±10 %** en cada medida (400 carros simulados, con
  muchas semillas): el ms por cm sale con error medio de 3 % y en el peor caso 15 %; pidiendo 40 cm con la
  calibración calculada, el carro real avanza 40 cm con un error máximo de 6 %. Con un 10 % de medidas
  disparatadas (un dato multiplicado por 3) el rechazo de atípicos mantiene 398 de 400 calibraciones dentro de 15 %;
- navegador con un carro simulado que tiene arranque, frenado y giros distintos a cada lado: sin calibrar queda a
  ~185 mm del destino de una ruta recta de 1 m; con su calibración, a 6 mm; zigzag y cuadrado con giros a ambos
  lados llegan al destino; una detención a mitad de camino y reanudación también;
- **por partes (J5)**: la velocidad guarda solo los ms por cm y conserva el frenado y el arranque; el frenado con un solo tiempo (492 mm en 2000 ms con 462 esperados = 30 mm); el giro con un solo tiempo
  (un carro de 8 ms por grado medido con el «giro de 90° real» queda justo en la siguiente pasada); el frenado de giro imposible se descarta al volver a medir; ms por grado de 3 a 20 al guardar
  (2.9 y 21 se rechazan, 3.0 y 20.0 se aceptan);
- **desplazamiento (J4)**: un giro de 90° con 1 mm por grado hacia adelante mueve el centro 64 mm a la derecha y 64 mm arriba (cuenta a mano 90 × 0.707); una medida pasada a µm por grado y aplicada al mismo giro
  vuelve a dar lo medido (±1 mm); dos repeticiones se promedian si difieren 30 % o menos, o 1 cm o menos (la mayor de las dos tolerancias; 1.0 y 2.0 cm se aceptan, 1.0 y 2.1 no; 10.0 y 12.5 cm se aceptan por el porcentaje, 10.0 y 14.5 no; 1.5 y 3.5 cm se rechazan con el detalle exacto), el promedio «de todos modos» y el flujo de dos repeticiones.

## PENDIENTE HARDWARE

Todo lo que necesita el carro real:

1. Instalar la app en el teléfono y hacer el procedimiento completo con el carro (los números de arriba son
   de un carro simulado, no del real).
2. Revisar que la reacción y el toque de arranque dan datos parecidos entre repeticiones (si no, la medida del
   arranque a mano es poco confiable; ver la limitación).
3. Probar una ruta (cuadrado) con el navegador y la calibración guardada y medir cuánto se desvía.
4. **Desplazamiento al girar**: medir con cinta los dos lados en el piso de la demo (2 repeticiones por lado), AUTO y, si se va a usar, MANUAL. Comparar `carro_mm` de `TURN_DIAG` con lo medido; el simulador usa
   el desplazamiento medido, así que mientras no se mide el simulador y el navegador suponen giro en el sitio, como antes.
5. **Giro de prueba**: medir los dos lados con el mismo giro de prueba («Giro de 90° real»), repetir el ajuste fino del giro de 90° y comprobar que `RUTA_DIAG` y la pantalla de resultado coinciden con el carro.
6. **Ruta que no giró una vez de tres**: repetir la ruta de 40 cm a la derecha y guardar el `RUTA_DIAG` de una corrida que falle (ver «Una ruta que no giró»).
7. (Hecho el 16 oct) La app nueva se instaló en el teléfono y las pantallas de Calibración (lista, giro de prueba, desplazamiento, ajuste fino) se revisaron con el carro falso; ver `docs/bitacora.md`. Falta repetirlo con el carro real.

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

## Modelo de pivote (J8, 16 oct)

El giro es de pivote (un lado gira, el otro queda fijo), así que el centro del carro describe un arco alrededor de un **punto fijo del chasis**. El modelo lineal de antes (µm por grado) fallaba ~8 % a 45°, ~15 % a 135° y
~41 % a 180°; ahora el desplazamiento se calcula girando el centro alrededor de ese punto.

- **Cómo sale el punto fijo.** Con la medida de la calibración (el desplazamiento del centro `D` en el marco del carro AL EMPEZAR y los grados `θ0` de la prueba) se despeja el vector del punto fijo al centro `v` de `(R(θ0) − I) v = D`
  (`R` = rotación hacia el lado del giro). Con la medida real (15 cm adelante y 16.5 cm a la derecha en 90°): punto fijo a **(-7.5, 157.5) mm del centro**, es decir sobre las ruedas de la derecha, y R = 157.7 mm.
- **Para cualquier ángulo θ** el centro se mueve `D(θ) = (R(θ) − I) v`: la cuerda es `2 R sen(θ/2)`. A los grados de la prueba reproduce EXACTO la medida; a 45°, 120.7 mm; a 135°, 291.3 mm; a 180°, 315.4 mm (el centro
  queda 15 mm atrás y 315 a la derecha). Se probó en C contra esas cuerdas (±2 mm).
- **Por lado y por perfil.** Cada lado y perfil tiene su vector y su ángulo de medida (`desp_grados_izq/der` en `CalibracionCarro`; la app guarda el ángulo medio de las dos repeticiones junto a los µm por grado).
- **Calibraciones ya guardadas.** Al cargarlas, si no tienen ángulo anotado se suponen medidas a 90° (el giro de prueba de siempre) y se convierten solas: dan lo mismo a 90° y cambian a los demás ángulos.
- **Casos límite (sin dividir entre cero).** Se usa el modelo lineal de antes (µm por grado) si la medida se hizo con menos de 20° o más de 340° (`2 − 2 cos θ0` casi 0), si el desplazamiento medido es de menos de 5 mm (casi nulo) o si el
  punto fijo saldría a más de 60 cm del centro. La validación (±5000 µm/° y 45 cm en total) y el aviso de «desplazamiento grande» (desde 15 cm) siguen igual.
- **Funciones públicas** (`calibracion.h`): `cal_pose_tras_giro(cal, pose, lado, grados)`, su inversa `cal_pose_antes_del_giro` (de dónde hay que partir para llegar a una pose; exacta salvo el redondeo a mm),
  `cal_desplazamiento_giro`, `cal_pivote_punto` (el punto fijo, para el log y la pantalla) y `cal_desplazamiento_es_espejo`. El navegador y el simulador usan el MISMO modelo.
- **Izquierda sin medir = espejo de la derecha.** Si un lado tiene sus dos valores en 0 y el otro no, se usa el espejo (adelante igual, de lado al revés). Es una suposición: la pantalla de Calibración lo dice —
  «**Izquierda: estimada como espejo de la derecha, mídela**» (y «Derecha: estimada como espejo de la izquierda, mídela» al revés) — y el navegador lo anota en el log: `TURN_DIAG aviso=izquierda_estimada_como_espejo_de_la_derecha`
  al comenzar la ruta y ` aviso=espejo_estimado` en el `TURN_DIAG` de cada giro que lo usa. Mide el giro a la izquierda para quitarlo.
- La pantalla de la lista muestra ahora lo que da el modelo a 90° (no el promedio lineal) y el punto fijo del giro a la derecha.
- **Sigue siendo una aproximación**: supone que el pivote no se mueve entre un giro y otro del mismo lado (el punto fijo depende del piso y las pilas, como todo lo demás) y que el giro es de ruedas de un solo lado.
