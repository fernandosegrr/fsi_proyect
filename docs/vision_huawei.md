# Visión con la cámara del Huawei: probar el detector de obstáculos sobre el piso real

Con esto se apunta la cámara trasera del teléfono al piso, se ve en pantalla lo que detecta el
detector de obstáculos (`nucleo_c/obstaculos.c`), se ajustan los umbrales sin recompilar y se usa
la cámara como fuente de obstáculos del modo semiautónomo. Sustituye a la ESP32-CAM mientras no
esté montada en el carro.

## Pantallas

- **Visión: cámara y detector de obstáculos** (pantalla principal → primer botón). Siempre en
  **horizontal**. Muestra el cuadro de 320x240 que realmente analiza el detector (sirve de vista
  previa) y encima:
  - la cuadrícula de celdas de 10x10 píxeles;
  - **rojo semitransparente** en las celdas que no parecen piso;
  - **azul**: la franja de referencia del piso (6 celdas de ancho x 2 de alto, abajo al centro);
  - las tres zonas de la mitad inferior (izquierda, centro, derecha): borde **verde** si están libres
    y **rojo** con la palabra OBSTACULO y la cercanía (1 lejos, 2 media, 3 muy cerca) si hay obstáculo.

  - las filas **ignoradas** en **gris** (todo lo que está arriba de **FILA_MINIMA**), con una **línea
    blanca** que marca el límite; las tres zonas empiezan en esa línea.

  A la derecha: estado de la cámara, cuadros por segundo, ms por cuadro (conversión + análisis), el
  color del piso de referencia y el código del detector (0 = bien, -3 = el piso de referencia no es
  uniforme). Tres barras cambian **UMBRAL_COLOR** (0 a 300), **MIN_CELDAS** (1 a 40) y **FILA_MINIMA**
  (0 a 23; la primera fila, de arriba hacia abajo, que cuenta como zona de avance) al instante y se
  guardan (SharedPreferences). "Valores por defecto" las restablece (60, 6 y 12). "Guardar cuadro" escribe
  un JPEG en la carpeta privada de la app.
- **Control del carro → Semiautónomo → "Obstáculos: botones / cámara"**: con "cámara", el resultado del
  detector (con los mismos umbrales de arriba) alimenta al navegador en vez de los botones simulados.

## Prueba física: 7 pasos para hacerla tú

Necesitas el teléfono con la app instalada (`adb install -r app_android\app\build\outputs\apk\debug\app-debug.apk`),
un piso y unos cuantos objetos. Si el teléfono pide permiso de cámara, pulsa **Permitir**.

1. **Montar.** Abre "Visión" y sostén el teléfono en horizontal, con la cámara trasera a la altura
   prevista de la cámara del carro (unos 10 a 15 cm del piso) y apuntando hacia adelante y un poco hacia
   abajo, de modo que el piso cercano quede en la parte baja de la imagen. Anota la altura y la inclinación.
2. **Inclinación y fila mínima.** Antes de ajustar umbrales, ajusta la **inclinación** del teléfono y la barra
   **FILA_MINIMA** hasta que el **fondo del cuarto** (paredes lejanas, muebles, ventanas) salga en **gris**
   (arriba de la línea blanca) y el piso cercano quede por debajo. Anota la inclinación y la fila elegida.
   Si la línea queda muy abajo se pierde piso útil; si queda muy arriba, el fondo da falsas alarmas.
3. **Franja azul sobre piso libre.** Comprueba que la franja azul (abajo, al centro) cae sobre piso limpio
   y que "Código" dice 0. Si dice -3 ("el piso de referencia no es uniforme"), hay un objeto, una sombra
   fuerte o una junta del piso en la franja: cambia el encuadre o la luz. Anota qué piso es (liso, baldosa,
   madera, alfombra) y qué luz hay.
4. **Piso despejado, sin objetos.** Mira un minuto. No debería haber celdas rojas ni zonas en rojo.
   - Si salen **falsas alarmas**, sube **UMBRAL_COLOR** de 10 en 10 hasta que desaparezcan y **anota el valor
     más bajo que las quita**. Si quedan manchas sueltas de pocas celdas, sube **MIN_CELDAS**.
   - Pasa la mano por delante para hacer **sombra**, enciende y apaga una luz, y enfoca un **reflejo** de
     ventana o lámpara. Anota qué provoca falsas alarmas.
5. **Objetos uno por uno.** Pon cada uno en el centro a unos 60, 40 y 20 cm del teléfono (y también a un
   lado): una caja de cartón, una mochila, un zapato, una botella, un libro, una hoja de papel blanca
   sobre el piso, y algo del **mismo color que el piso**. Para cada uno anota: ¿se detecta?, ¿en qué zona
   y con qué cercanía (1 a 3)?, ¿con qué UMBRAL_COLOR y MIN_CELDAS?, y la distancia a la que empieza a verse.
   Busca el umbral más bajo que sigue sin falsas alarmas y que todavía ve todos los objetos que importan.
6. **Casos difíciles.** Apunta a una pared de frente (cerca, a 30 cm) y a una pared del mismo color que el
   piso; a una alfombra o baldosa con dibujo; al piso mojado o muy brillante. Anota cuáles se detectan y
   cuáles no. Usa **Guardar cuadro** en las falsas alarmas y en los objetos que no vea; sácalos con
   `adb pull /sdcard/Android/data/com.fsi.carro/files/ .\cuadros_prueba` (son tus fotos: **no las subas al
   repositorio**).
7. **Con el modo semiautónomo.** Con el servidor falso corriendo y `adb reverse` (ver `docs/pruebas_control.md`),
   en "Control del carro" → Semiautónomo → "Obstáculos: cámara" → Iniciar. Con el piso libre debe avanzar; al
   poner un objeto delante debe detenerse (`STOP`) y buscar un lado libre (el servidor muestra la secuencia).
   Si el piso cambia mucho de luz o no es uniforme, parará con "ERROR": es el comportamiento seguro.

**Qué anotar** (una línea por prueba): piso y luz; altura e inclinación del teléfono; el **UMBRAL_COLOR** y el
**MIN_CELDAS** y la **FILA_MINIMA** que funcionaron; falsas alarmas (dónde, con qué causa); sombras y reflejos que engañan;
objetos que no se detectan (y su color frente al del piso); cercanía que marca a 20, 40 y 60 cm; cuadros por
segundo. Con eso se calibran los valores por defecto de `nucleo_c/obstaculos.h` y las filas de cercanía
(`OBS_FILA_MEDIA`, `OBS_FILA_CERCA`).

## Qué se probó en el Huawei (sin apuntar a un piso)

La cámara estaba apuntando a lo que había en el escritorio, así que NO se probó la calidad de la detección
sobre un piso real; solo el funcionamiento técnico. Resultados medidos (Huawei ELS-NX9, Android 10):

- La cámara abre sin errores (cámara trasera, id 0, YUV de 320x240 directo, sin reducción). El permiso se
  pide en ejecución.
- **60 s seguidos** (65 s de registro, 1607 cuadros): **24,9 cuadros por segundo** de media (24,0 a 25,4;
  es el ritmo de la cámara) y **6,85 ms por cuadro** de media entre conversión YUV→RGB y análisis en C
  (mediana 6,90 ms, p95 8,18 ms, mínimo 5,07 ms, máximo 8,82 ms). No hubo ninguna advertencia ni error en el
  registro (`adb logcat -s VISION`). Es solo una sexta parte de los 40 ms que separan dos cuadros de la cámara, así que sobra tiempo.
- Las barras cambian los valores en vivo (de 60/6 a 202/20 y de vuelta) y quedan guardados. La tercera barra
  (FILA_MINIMA, 12 → 20) se comprobó con el registro (`fila_minima=20`) y el archivo de preferencias; la
  superposición gris con la línea blanca se vio solo en una captura pixelada. Una nueva corrida de 33 s
  (con la barra nueva) dio 25.0 fps y 6.50 ms por cuadro, sin errores.
- "Guardar cuadro" crea el JPEG en la carpeta de la app (unos 19 KB).
- **Segundo plano**: al pasar a segundo plano la cámara se cierra (`cámara cerrada` en el registro y sin
  clientes de cámara activos en el sistema).
- **Integración con el modo semiautónomo** (servidor falso + `adb reverse`): con "Obstáculos: cámara" la ruta
  arranca (`FORWARD`), la cámara marcó obstáculo al centro (con los umbrales puestos muy sensibles a propósito),
  el navegador envió `STOP` unos 115 ms después y, sin lado libre, terminó en ERROR a los 4 s. Al ir a
  segundo plano se envía `STOP` y se cierra la cámara.

## Qué NO se probó

- La detección sobre un **piso real**, con objetos reales, sombras y reflejos (eso es la prueba física de arriba).
- La orientación del teléfono: se supone en horizontal (la imagen del sensor sale derecha); en vertical la
  imagen saldría girada 90 grados y el detector no serviría.
- Otros teléfonos o tabletas (la cámara de este entrega 320x240 directo; otros reducirán desde 640x480).
- La ESP32-CAM: color, JPEG y lente serán distintos, y habrá que recalibrar.
- El modo semiautónomo con el carro real y con una cámara montada en él.
