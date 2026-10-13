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

1. **Entrada**: una imagen RGB de **cualquier tamaño de 30x30 a 640x480** (3 bytes por píxel:
   R, G, B, fila por fila). El ancho y el alto se pasan en cada llamada; el detector no supone
   320x240. La cámara real del carro entrega 240x176 (HQVGA).
2. **Celdas**: se divide en celdas de 10x10 píxeles: columnas = ancho / 10 y filas = alto / 10
   (los píxeles que sobran no se miran). 320x240 son 32x24 celdas, 240x176 son 24x17 y 160x120
   son 16x12. Se calcula el color promedio de cada celda. Promediar 100 píxeles elimina casi
   todo el ruido de la cámara.
3. **Color del piso**: se promedian las celdas de una franja de referencia abajo y al centro.
   Su tamaño es una **proporción** de la imagen (ancho 188 milésimas, alto 84 milésimas,
   centrada y pegada abajo): en 320x240 son 6x2 celdas (columnas 13 a 18, filas 22 y 23), en
   240x176 son 4x1 y en 160x120 son 3x1. Siempre hay al menos 1 celda. Se asume que esa franja
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
6. **Zonas**: solo la zona de avance, de la fila mínima hacia abajo. La fila mínima es una
   **proporción del alto** (`fila_minima_mil`, en milésimas; por defecto 500 = 0.5 = la mitad
   inferior) y se convierte a fila al analizar: en 24 filas es la fila 12, en 17 filas la 8, en
   12 filas la 6. Las filas de arriba (el fondo del cuarto, paredes lejanas) se ignoran. Se divide en tres tercios de ancho: `ZONA_IZQUIERDA`, `ZONA_CENTRO` y
   `ZONA_DERECHA`. Una zona tiene obstáculo si hay al menos `MIN_CELDAS` celdas "no piso"
   confirmadas desde esa fila hacia abajo.
7. **Cercanía** (0 a 3) según la fila más baja que alcanza el obstáculo (no depende de la fila
   mínima). Los límites también son proporciones del alto (`OBS_MEDIA_MIL` 667 y `OBS_CERCA_MIL`
   834): 0 = no hay; 1 = lejos; 2 = media (desde 2/3 de la altura); 3 = muy cerca (desde 5/6).
   En 24 filas son las de siempre (1 = filas 12 a 15, 2 = 16 a 19, 3 = 20 a 23); en 17 filas
   (HQVGA) 1 = filas 8 a 10, 2 = 11 a 13 y 3 = 14 a 16.

El resultado (`ResultadoObstaculos`) trae las tres zonas (hay obstáculo, cercanía y número de
celdas), el color de referencia del piso, el tamaño de la rejilla de ese análisis (`columnas` y
`filas`), la fila mínima y la franja de referencia que se usaron, el aviso `rejilla_chica` y una
máscara con las celdas "no piso" (hasta 48x64; solo se usa la parte `filas` x `columnas`; sirve para
dibujar o depurar). Los errores son códigos: `OBS_ERR_IMAGEN_NULA`, `OBS_ERR_TAMANO` (ancho o
alto no positivos, mayores que 640x480 o con menos de 3x3 celdas: **nunca se cuelga ni lee fuera de
la imagen**), `OBS_ERR_REFERENCIA` y `OBS_ERR_CONFIG`.

### Tamaños de imagen: qué pasa con cada uno

- **320x240** (32x24 celdas) y **240x176** (24x17, el modo de la cámara real): funcionan igual y
  pasan el mismo juego de pruebas.
- **160x120** (16x12 celdas): **funciona, pero con aviso** (`rejilla_chica` = 1, porque tiene
  menos de 20 columnas o 15 filas). Cada celda pesa más, la franja de referencia es de 3x1
  celdas y un objeto pequeño ocupa pocas celdas (con `min_celdas` 6 una caja de 4 celdas ya no
  cuenta). Por eso QQVGA se descartó para detectar. La pantalla de visión avisa "Rejilla chica".
- **30x30** (3x3, el mínimo): funciona con aviso; **menos de 3x3 celdas** (por ejemplo 29x30 o
  20x240) da `OBS_ERR_TAMANO`.

## Piso con varios colores, aprendizaje continuo y cambio de piso

El primer detector suponía un piso de UN solo color. Con baldosas, vetas o alfombras eso daba falsas alarmas o
`OBS_ERR_REFERENCIA`. Ahora el detector tiene un **modelo del piso** (`ModeloPiso`) con hasta K = 4 grupos de color:

1. **Grupos desde la franja** (`agrupar_franja`): un k-medias CORTO sobre las celdas de la franja de referencia: el
   primer centro es la celda más clara, cada siguiente es la celda más lejos de los ya elegidos (y solo si está al menos
   `OBS_DIST_FUSION` = 24 lejos), 3 vueltas de "cada celda al centro más cercano / cada centro al promedio", y los grupos
   que quedan a menos de 24 se funden. Un piso liso da 1 grupo (igual que antes); unas baldosas de 2 tonos, 2; baldosas
   no alineadas con las celdas (bordes mezclados), 3.
2. **¿Es piso?** Una celda es piso si cumple la regla de color y de luz (A o B, igual que antes) con **cualquiera** de los grupos,
   o si está cerca (la mitad del umbral) de un punto **entre dos grupos** (el borde entre dos baldosas mezcla sus colores).
3. **Franja contaminada**: todos los grupos de la franja deben ser **compatibles** entre sí (distancia de color de
   hasta `OBS_MAX_DIST_GRUPOS` = 150, o el mismo tono con distinta luz, como una baldosa clara y una oscura) y cada celda debe quedar a
   menos de `MAX_VARIACION_REF` de su grupo. Una caja roja sobre un piso café no es compatible: da `OBS_ERR_REFERENCIA`
   (con la franja medio tapada). Unas baldosas clara y oscura del mismo tono sí se aceptan (2 grupos); con K = 1 no se pueden
   representar y dan error: por eso K = 4 es el valor por defecto.
4. **Aprendizaje continuo seguro** (`obstaculos_analizar_modelo`, con el modelo guardado entre cuadros): solo cuando el carro
   **avanza** y **no hay obstáculo** en ninguna zona, cada grupo se mueve un poco (`aprendizaje_mil`, por defecto 20 = 2 % por
   cuadro) hacia el promedio de las celdas de piso que se le parecen. **Nunca** usa: celdas marcadas como obstáculo ni sus vecinas,
   celdas "dudosas" (a más de la mitad del umbral del grupo) ni las de arriba de la fila mínima. Con la luz subiendo 40 por canal
   en 300 cuadros el modelo la sigue (queda a ~24 del color real, sin dar obstáculos) con 1 grupo; sin aprender necesita 3 grupos
   para seguirla. Una zona grande (65 % de la zona de avance) casi del color del piso o un objeto rojo de 48 celdas que no se
   reporta no se aprenden (el modelo se mueve menos de 10).
5. **Si la franja se tapa** (algo pegado al carro) y el modelo ya aprendió el piso, el detector **no da error**: usa lo
   aprendido, avisa (`franja_contaminada`) y el objeto se ve como obstáculo muy cerca. Sin modelo (primer cuadro) sigue dando
   `OBS_ERR_REFERENCIA`.
6. **Cambio de piso**: si en la zona de avance TODAS las celdas (salvo el 2 %) quedan lejos de todos los grupos durante
   `OBS_MS_PISO_NUEVO` = 1000 ms seguidos, el modelo se reaprende desde la franja (`piso_reaprendido` = 1, la app muestra
   "PISO NUEVO APRENDIDO"). En la prueba (una alfombra azul que aparece a los 500 ms) se reaprende a los 1500 ms; mientras tanto las tres zonas
   salen con obstáculo y el navegador ya se detuvo. Si solo cambia parte del piso, o si el piso original reaparece antes de 1 s, no se reaprende.
   **Riesgo conocido**: una pared que llena TODA la imagen (franja incluida) tampoco se distingue de un piso nuevo y se reaprende como piso a
   los 1000 ms (el navegador ya se detuvo en ese segundo; no vuelve a avanzar mientras no haya un cuadro libre, pero ese cuadro libre llega).
7. Un piso de colores **compatibles** que cambia (de café claro a café oscuro) se absorbe solo: la franja nueva se agrega como grupo nuevo (si
   cabe en K) sin esperar 1 s.

Configuración nueva (`ConfigObstaculos`): `grupos_piso` (K, 1 a 4, por defecto 4), `aprendizaje_mil` (0 a 1000, por defecto 20; 0 = no
aprende) y `usar_bordes` (0 o 1, por defecto 0). En la app: dos barras en la pantalla de visión (K y aprendizaje). `obstaculos_analizar_con`
sigue sin memoria (cada llamada aprende el piso de ese cuadro y lo olvida); la app usa el modelo con memoria.

### La señal de bordes (objetos del color del piso): se deja APAGADA

Un objeto casi del color del piso (una libreta) no se ve por color. Se probó una segunda señal: una celda "dudosa" (algo distinta del piso)
con un salto de color brusco (`OBS_UMBRAL_BORDE` = 24) hacia una celda de piso seguro cuenta como contorno de un objeto. Resultados medidos en
las pruebas (`pruebas_piso.c`):

- Libreta de 70x60 px casi del color del piso (+22, +14, -6) con borde oscuro: **sin bordes 0 celdas detectadas, con bordes 24** (se detecta).
- Pero en 5 escenas SIN objetos hay **0 celdas falsas sin bordes y 115 con bordes**: la sombra suave (rampa de luz de 40) da 10 celdas y las
  baldosas de 25 px no alineadas con las celdas dan 105 celdas falsas en las tres zonas.

Mejora la libreta pero empeora escenas sin objetos, así que **`usar_bordes` queda en 0 por defecto**: es un interruptor de la configuración
para experimentar. Con el promedio por celdas de 10x10 no hay forma de separar el contorno de una libreta (un salto de ~45) de una sombra
suave o de una junta de baldosas. Para detectar este tipo de objetos haría falta trabajar con la resolución de píxeles dentro de la celda, o el
modelo de objetos (parte 4).

### Pruebas del piso (`nucleo_c/pruebas_piso.c`, 57 casos)

Baldosas de 2 tonos (40 px y 25 px no alineadas), claro/oscuro del mismo tono, K = 1 contra K = 4, K = 2; vetas finas y gruesas; sombra suave
(2 tamaños); mano y caja roja sobre baldosas (se detectan, cercanía 3); escalera al fondo (arriba de la fila mínima: sin obstáculos) y escalera
lejana dentro de la zona de avance (cercanía 1: el navegador solo frena desde cercanía 2); libreta; falsos positivos de los bordes; aprendizaje
(sigue la luz, no aprende parado ni con obstáculos, no aprende objetos ni zonas dudosas); cambio de piso (tiempo exacto, parcial, interrumpido,
pared); franja tapada con y sin modelo; validación de los campos nuevos; y el piso de baldosas a 240x176. Con una mutación (aprender las celdas dudosas)
una prueba falla como debe. Las 144 pruebas anteriores (`pruebas_obstaculos.c`) siguen pasando sin cambios.

## Parámetros a calibrar con el carro real

Todos están en `nucleo_c/obstaculos.h`. Los valores actuales son de partida, pensados para
imágenes sintéticas; hay que ajustarlos con la cámara y el piso reales.

**Se pueden cambiar en vivo**: `UMBRAL_COLOR`, `MIN_CELDAS`, la franja de referencia y `fila_minima` viajan en una
estructura `ConfigObstaculos` (`obstaculos_config_defecto` da los valores por defecto y
`obstaculos_analizar_con` la usa; `obstaculos_analizar` equivale a usar la configuración por defecto).
La pantalla de visión de la app los ajusta con barras sin recompilar (ver `docs/vision_huawei.md`).
Una configuración no válida da `OBS_ERR_CONFIG`. Los demás parámetros siguen siendo constantes.

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
- `OBS_MEDIA_MIL` = 667 y `OBS_CERCA_MIL` = 834 (proporciones del alto): dónde cambian las
  cercanías. Hay que relacionarlas con la distancia real (por ejemplo, a cuántos cm del carro está la fila 20) y
  con la distancia de frenado del carro.
- `fila_minima_mil` (fila mínima) = 500 (`OBS_FILA_MINIMA_MIL`): **proporción del alto** (0 a 1000
  milésimas; 500 = 0.5 = la mitad inferior; el antiguo "12 de 24" equivale a 500). Se convierte a
  la primera fila de celdas, contando desde arriba, que cuenta como zona de avance
  (`filas * fila_minima_mil / 1000`). Las filas de arriba se ignoran, aunque tengan
  celdas "no piso" (la máscara `no_piso` sigue mostrándolas, para dibujar). Súbela si el fondo del
  cuarto (una pared lejana, un mueble) da falsas alarmas; bájala si la cámara apunta muy abajo y
  se pierde piso útil. Ajusta primero la inclinación del teléfono o la cámara y después esta fila,
  hasta que el fondo del cuarto quede en la parte gris de la pantalla de visión. Como la
  cercanía usa la fila más baja del obstáculo, no cambia al mover la fila mínima. Un valor fuera
  de 0 a 1000 da `OBS_ERR_CONFIG` (1000 = solo se mira la última fila). Para que un objeto
  cuente, al menos `MIN_CELDAS` de sus celdas confirmadas deben estar en la fila mínima o más abajo (el filtro de celdas aisladas mira también
  las vecinas de arriba).
- Franja de referencia: `ref_ancho_mil` (188), `ref_alto_mil` (84), `ref_centro_x_mil` (500) y
  `ref_borde_inf_mil` (1000), todas proporciones. Si se saldría de la imagen se corre hacia
  adentro. Depende de cómo quede montada la cámara. Con una rejilla chica conviene una franja
  más alta (por ejemplo `ref_alto_mil` 170) para promediar más celdas.
- `MIN_CELDAS` se cuenta en celdas absolutas: con 240x176 los objetos se ven con menos celdas que
  en 320x240 (el área baja a ~56 %), así que si una caja real no se detecta, baja `min_celdas`
  a 4 en la pantalla de visión.

## Pruebas hechas

- **En PC** (`cd nucleo_c; make test`): 144 casos del detector, todos pasan. Son los 71 anteriores
  (3 se reescribieron porque la configuración de la franja y la fila mínima ahora son proporciones;
  se agregaron 6 de configuración) y 67 nuevos por tamaño: la misma escena (piso, ruido, luz, caja
  al centro/izquierda/derecha cerca, media y lejos, pared, celda aislada, objeto arriba de la
  fila mínima, franja contaminada, máscara, aviso) en 320x240, 240x176 y 160x120, más tamaños
  raros (30x30, 640x480, 245x179, tamaños no positivos o fuera de rango). Piso liso, con ruido leve y
  fuerte, cambio de luz global (+30), degradado de luz, caja al centro, a la izquierda y a la
  derecha, caja lejos y a media distancia, caja entre dos zonas, objeto oscuro del mismo tono,
  pared que ocupa todo, celda aislada, celdas sueltas dispersas, objeto pequeño, objeto solo en
  la mitad superior, imagen nula, tamaño inválido, referencia contaminada y repetibilidad. Para `fila_minima`: un objeto por encima del límite se ignora, uno justo en la fila límite cuenta, una fila por encima no cuenta, cambiar el valor cambia el resultado, la cercanía no cambia y los valores -1, 24 y 1000 dan `OBS_ERR_CONFIG`. Se
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
- **Pisos con dibujo**: baldosas de 2 a 4 tonos compatibles (mismo tono con distinta luz, o colores cercanos) y vetas finas se
  manejan con los grupos de color. Un piso de baldosas de tonos muy distintos (por ejemplo azul y blanco) o con más de 4 colores
  puede seguir dando falsas alarmas o `OBS_ERR_REFERENCIA`; la franja de 240x176 solo tiene 4 celdas (el piso se completa con el
  aprendizaje). Un objeto cuyo color queda ENTRE los dos tonos de las baldosas no se detecta.
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

- **C**: `obstaculos_analizar(rgb, ancho, alto, &resultado)` devuelve `OBS_OK` o un código de error.
- **Kotlin**: `ObstaculosNativa.analizar(bytesRgb, ancho, alto)` devuelve 10 enteros
  `[código, hayIzq, cercaníaIzq, hayCentro, cercaníaCentro, hayDer, cercaníaDer, refR, refG, refB]`
  (los índices tienen nombre en `ObstaculosNativa`).
- **Pruebas en PC**: `cd nucleo_c` y `make test` (corre también las de trayectoria).
