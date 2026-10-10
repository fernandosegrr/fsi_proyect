# Detector de obstáculos por color de piso

Código: `nucleo_c/obstaculos.h` y `nucleo_c/obstaculos.c` (C estándar, solo enteros, sin
`malloc`, sin recursión). Pruebas: `nucleo_c/pruebas_obstaculos.c`. Se llama desde la app por
JNI (`app_android/.../cpp/puente.c` y `ObstaculosNativa.kt`).

## Para qué sirve

El modelo de objetos (EfficientDet con las clases de COCO) solo reconoce lo que conoce:
personas, sillas, botellas. No ve una caja de cartón ni una pared. Este detector usa una idea
más simple: **el carro ve el piso libre en la parte baja de la imagen; todo lo que no se
parezca al color del piso y esté en la zona de avance es un obstáculo.** No dice qué es, solo
que hay algo y de qué lado.

## Cómo funciona (en el orden en que lo hace el código)

1. **Entrada**: una imagen RGB de 320x240, 3 bytes por píxel (R, G, B), fila por fila.
2. **Celdas**: se divide en celdas de 10x10 píxeles (32 columnas x 24 filas) y se calcula el
   color promedio de cada una. Promediar 100 píxeles elimina casi todo el ruido de la cámara.
3. **Color del piso**: se promedian las celdas de una franja de referencia abajo y al centro
   (6 celdas de ancho x 2 de alto: columnas 13 a 18, filas 22 y 23). Se asume que esa franja
   es piso libre. Si sus celdas no son parecidas entre sí (alguna se aleja más de
   `MAX_VARIACION_REF`), hay algo encima y se devuelve el error `OBS_ERR_REFERENCIA`.
4. **¿Es piso esta celda?** Es piso si cumple una de dos condiciones:
   - **A)** Su color es casi igual al de referencia: la suma de las diferencias absolutas de
     R, G y B es menor o igual que `UMBRAL_COLOR`.
   - **B)** Tiene el mismo tono pero distinta luz (una zona más clara por una lámpara, o más
     oscura por la distancia). Para saberlo se compara el color **normalizado por brillo**
     (cada canal dividido entre R+G+B; así dos grises de distinta luz dan casi lo mismo) con
     `UMBRAL_CROMA`, y se exige que el brillo (R+G+B) no difiera más de `UMBRAL_BRILLO`.
     Solo se usa si ambos colores tienen brillo suficiente (`SUMA_MIN_CROMA`), porque en la
     oscuridad el tono no es confiable.

   Si no cumple ninguna, la celda es "no piso". Se eligió así (y no solo la suma de
   diferencias) porque una lámpara o la distancia hacen el piso lejano más claro, y la suma
   sola lo marcaría como obstáculo. La condición B lo tolera sin dejar de detectar una caja
   muy oscura o de otro color.
5. **Ruido**: una celda "no piso" sin ninguna vecina "no piso" (en las 8 de alrededor) se
   descarta. Un reflejo suelto o un pixel malo no cuenta.
6. **Zonas**: solo la mitad inferior de la imagen (filas 12 a 23), en tres tercios de ancho:
   `ZONA_IZQUIERDA`, `ZONA_CENTRO` y `ZONA_DERECHA`. Una zona tiene obstáculo si hay al menos
   `MIN_CELDAS` celdas "no piso" confirmadas.
7. **Cercanía** (0 a 3) según la fila más baja que alcanza el obstáculo: 0 = no hay;
   1 = lejos (filas 12 a 15); 2 = media (filas 16 a 19); 3 = muy cerca (filas 20 a 23).

El resultado (`ResultadoObstaculos`) trae las tres zonas (hay obstáculo, cercanía y número de
celdas), el color de referencia del piso y una máscara de 24x32 con las celdas "no piso" (sirve
para dibujar o depurar). Los errores son códigos: `OBS_ERR_IMAGEN_NULA`, `OBS_ERR_TAMANO`
(la imagen no es de 320x240) y `OBS_ERR_REFERENCIA`.

## Parámetros a calibrar con el carro real

Todos están en `nucleo_c/obstaculos.h`. Los valores actuales son de partida, pensados para
imágenes sintéticas; hay que ajustarlos con la cámara y el piso reales.

- `UMBRAL_COLOR` = 60 (suma de diferencias por canal, 0 a 765). Súbelo si el piso limpio da
  falsas alarmas; bájalo si no ve cajas parecidas al piso.
- `UMBRAL_CROMA` = 30 y `UMBRAL_BRILLO` = 150: controlan cuánto cambio de luz se tolera.
  Si hay falsas alarmas por degradados de luz, sube `UMBRAL_BRILLO`; si una caja oscura o
  clara del mismo tono pasa desapercibida, bájalo.
- `SUMA_MIN_CROMA` = 90: debajo de este brillo (R+G+B) no se usa la comparación de tono.
- `MAX_VARIACION_REF` = 40: cuánto puede variar la franja de referencia. Con un piso con
  textura o baldosas hay que subirlo; con uno muy liso se puede bajar para detectar antes
  un objeto debajo del carro.
- `MIN_CELDAS` = 6: tamaño mínimo (en celdas de 10x10) para contar una zona. Más alto = menos
  falsas alarmas pero no ve objetos pequeños.
- `OBS_FILA_MEDIA` = 16 y `OBS_FILA_CERCA` = 20: dónde cambian las cercanías. Hay que
  relacionarlas con la distancia real (por ejemplo, a cuántos cm del carro está la fila 20) y
  con la distancia de frenado del carro.
- Geometría: franja de referencia (`OBS_REF_COLUMNAS`, `OBS_REF_FILAS`) y mitad inferior
  (`OBS_FILA_INICIAL_ZONAS`). Dependen de cómo quede montada la cámara.

## Pruebas hechas

- **En PC** (`cd nucleo_c; make test`): 32 casos, todos pasan. Piso liso, con ruido leve y
  fuerte, cambio de luz global (+30), degradado de luz, caja al centro, a la izquierda y a la
  derecha, caja lejos y a media distancia, caja entre dos zonas, objeto oscuro del mismo tono,
  pared que ocupa todo, celda aislada, celdas sueltas dispersas, objeto pequeño, objeto solo en
  la mitad superior, imagen nula, tamaño inválido, referencia contaminada y repetibilidad. Se
  comprobó con mutaciones (quitar el filtro de celdas aisladas y quitar la condición de tono)
  que las pruebas fallan cuando el código se rompe.
- **En el Huawei** (`docs/benchmark_ELS-NX9.md`): con 4 imágenes generadas por código y JPEG de
  calidad 70, el resultado fue el esperado en todas, y un cuadro completo (decodificar,
  convertir, analizar) tarda 1.24 a 1.60 ms de media; solo el análisis en C, unos 0.6 ms, contra
  43 ms del modelo.

## Límites conocidos

- **Pared o piso del mismo color**: un objeto cuyo color y brillo se parece al del piso (más
  o menos 50 por canal) no se detecta. Una pared del mismo color que el piso es invisible.
- **Si la pared (o el objeto) cubre también la franja de referencia**, el carro "aprende" ese
  color como piso y no ve nada. Si la franja es desigual da `OBS_ERR_REFERENCIA`; si es
  uniforme, no hay forma de saberlo. Por eso la franja se asume piso libre: hay que frenar
  antes de llegar a esa situación.
- **Sombras**: una sombra oscura y bien marcada (el carro proyecta la suya, o una mesa) puede
  pasar de `UMBRAL_BRILLO` y verse como obstáculo.
- **Pisos con dibujo**: alfombras estampadas, baldosas de colores o tablas muy contrastadas
  varían más que la franja de referencia y dan falsas alarmas o `OBS_ERR_REFERENCIA`.
- **Reflejos y brillos**: un piso brillante refleja lámparas y ventanas; esas manchas se ven
  como obstáculos.
- **Cámara**: el balance de blancos y la exposición automáticos pueden cambiar entre cuadros y
  desplazar los colores; dentro de un cuadro no afecta porque el piso de referencia se toma del
  mismo cuadro. Supone una cámara baja, mirando hacia adelante, con el piso visible abajo, y
  que no se ve el propio carro (ruedas o chasis) en la franja de referencia.
- **No mide distancias**: la cercanía sale de la fila de la imagen, no de centímetros.
- **Objetos pequeños o muy lejanos**: menos de `MIN_CELDAS` celdas (600 píxeles de la imagen) no
  cuentan, y todo lo que está en la mitad superior se ignora.
- **Huecos y escalones**: un hueco oscuro se ve como "no piso" (conviene), pero un escalón
  del mismo color que el piso no.

## Qué NO se probó

- Con la **cámara real** (ESP32-CAM): ni con su color, su JPEG, su lente ni su altura.
- Con una **foto real de pasillo**: el benchmark acepta `bench_pasillo.jpg`, pero no se probó
  (no se revisaron fotos personales del teléfono). Para hacerlo: `adb push foto.jpg
  /sdcard/Android/data/com.fsi.carro/files/bench_pasillo.jpg` y lanzar el benchmark de obstáculos.
- Con el **carro en movimiento**, ni la decisión de frenar o girar a partir del resultado: aquí
  solo se calcula si hay obstáculo y de qué lado.
- La calibración de los umbrales con el piso real, y la relación entre la cercanía y los
  centímetros reales.
- La combinación con el modelo de objetos (qué hacer cuando ambos opinan distinto).

## Cómo usarlo

- **C**: `obstaculos_analizar(rgb, 320, 240, &resultado)` devuelve `OBS_OK` o un código de error.
- **Kotlin**: `ObstaculosNativa.analizar(bytesRgb, 320, 240)` devuelve 10 enteros
  `[código, hayIzq, cercaníaIzq, hayCentro, cercaníaCentro, hayDer, cercaníaDer, refR, refG, refB]`
  (los índices tienen nombre en `ObstaculosNativa`).
- **Pruebas en PC**: `cd nucleo_c` y `make test` (corre también las de trayectoria).
