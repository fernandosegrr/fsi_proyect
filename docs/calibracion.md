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
