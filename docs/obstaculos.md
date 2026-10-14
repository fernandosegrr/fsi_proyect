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

## Valores por defecto validados en el carro real

Son los que trae el código (`nucleo_c/obstaculos.h`) y los que usa una app recién instalada: un teléfono nuevo funciona **sin tocar sliders**.
Salen de lo que el usuario ajustó en vivo con el carro real, la cámara del carro (HQVGA 240x176, 12 fps) y su piso: con fila mínima ~440,
celdas mínimas entre 6 y 8 (se tomó 7, el centro) y umbral de color 40 el piso vacío no dio falsas alarmas (0 de 270 muestras) y la mochila
se vio desde ~80 cm. Son valores de ESE piso y ESA luz: en otro lugar puede hacer falta retocarlos.

| Valor | Defecto | Unidades | Por qué |
|---|---|---|---|
| `OBS_FILA_MINIMA_MIL` (fila mínima) | **440** (antes 500) | milésimas del alto (440 = 44 %; en 17 filas, desde la fila 7; en 24, desde la 10) | Mira un poco más arriba de la mitad de la imagen: el objeto entra en la zona de avance antes (más margen para frenar con ~0.4 s de retraso) sin llegar al fondo del cuarto. Más bajo (número mayor) = ignora más lo lejano. |
| `MIN_CELDAS` | **7** (antes 6) | celdas de 10x10 px | Cuántas celdas "no piso" hacen falta en una zona para contarla. Con 240x176 un objeto ocupa pocas celdas. Más alto = menos falsas alarmas pero no ve objetos chicos; entre 6 y 8 funcionó en las pruebas. |
| `UMBRAL_COLOR` | **40** (antes 60) | suma \|dR\|+\|dG\|+\|dB\| de 0 a 765 | Qué tanto debe cambiar el color de una celda para dejar de ser "piso". Más bajo = ve objetos más parecidos al piso; si el piso vacío da falsas alarmas, súbelo. (El teléfono de pruebas quedó con 119 porque se retocó después; el defecto del código es 40.) |
| Modelo de objetos (`usarModelo`) | **NO** | sí/no (preferencia de la app) | EfficientDet-Lite0 no reconoce objetos con la cámara del carro (nunca vio el termo; inventa «person» y «refrigerator»). Solo suma retraso y falsos positivos. |
| «Frenar desde lejos en el centro» | **activo con el video del carro**, apagado con la cámara del teléfono | automático (la app lo pone al iniciar la ruta) | Con el retraso del video y 38 cm/s el STOP llega ~17 cm tarde: un obstáculo lejano (cercanía 1) al centro ya frena, tras 2 cuadros y 60 ms (`NAV_MS_CONFIRMAR_CENTRO_LEJOS`). |
| Grupos del piso (K) / aprendizaje | 4 / 20 milésimas por cuadro | colores / ‰ por cuadro | Sin cambios: ya eran los defectos (`OBS_GRUPOS_DEFECTO`, `OBS_APRENDIZAJE_DEFECTO`). |

Notas: las preferencias del teléfono (`shared_prefs/vision.xml`) tienen prioridad sobre estos valores; en un teléfono con valores viejos, el botón
«Restablecer» de la pantalla de visión vuelve a ellos. Las pruebas de PC de la mecánica de la fila mínima y de los objetos de 6 celdas usan una
configuración explícita (`config_clasica`: 500, 6 y 60) y hay una prueba que comprueba estos defectos.

## Parámetros a calibrar con el carro real

Todos están en `nucleo_c/obstaculos.h`. Los valores actuales son de partida, pensados para
imágenes sintéticas; hay que ajustarlos con la cámara y el piso reales.

**Se pueden cambiar en vivo**: `UMBRAL_COLOR`, `MIN_CELDAS`, la franja de referencia y `fila_minima` viajan en una
estructura `ConfigObstaculos` (`obstaculos_config_defecto` da los valores por defecto y
`obstaculos_analizar_con` la usa; `obstaculos_analizar` equivale a usar la configuración por defecto).
La pantalla de visión de la app los ajusta con barras sin recompilar (ver `docs/vision_huawei.md`).
Una configuración no válida da `OBS_ERR_CONFIG`. Los demás parámetros siguen siendo constantes.

- `UMBRAL_COLOR` = 40 (antes 60; suma de diferencias por canal, 0 a 765). Súbelo si el piso limpio da
  falsas alarmas; bájalo si no ve cajas parecidas al piso.
- `UMBRAL_CROMA` = 30 y `UMBRAL_BRILLO` = 150: controlan cuánto cambio de luz se tolera.
  Si hay falsas alarmas por degradados de luz, sube `UMBRAL_BRILLO`; si una caja oscura o
  clara del mismo tono pasa desapercibida, bájalo.
- `SUMA_MIN_CROMA` = 90: debajo de este brillo (R+G+B) no se usa la comparación de tono.
- `MAX_VARIACION_REF` = 40: cuánto puede variar la franja de referencia. Con un piso con
  textura o baldosas hay que subirlo; con uno muy liso se puede bajar para detectar antes
  un objeto debajo del carro.
- `MIN_CELDAS` = 7 (antes 6): tamaño mínimo (en celdas de 10x10) para contar una zona. Más alto = menos
  falsas alarmas pero no ve objetos pequeños.
- `OBS_MEDIA_MIL` = 667 y `OBS_CERCA_MIL` = 834 (proporciones del alto): dónde cambian las
  cercanías. Hay que relacionarlas con la distancia real (por ejemplo, a cuántos cm del carro está la fila 20) y
  con la distancia de frenado del carro.
- `fila_minima_mil` (fila mínima) = 440 (antes 500; `OBS_FILA_MINIMA_MIL`): **proporción del alto** (0 a 1000
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

## Corredor de trayectoria (APAGADO por defecto)

Código: `nucleo_c/corredor.c/.h` (C puro, probado en PC: `nucleo_c/pruebas_corredor.c`). En la app: `CorredorNativa`, `ConfigVision` y los controles de la pantalla de visión.

### Para qué
Hasta ahora el navegador decidía con tres zonas (izquierda / centro / derecha, tercios de la imagen): un objeto que pasa **por un lado**, fuera de por
donde va a pasar el carro, también lo frenaba. El corredor es la franja de piso por la que el carro **sí va a pasar** (su ancho más un margen), dibujada en
la imagen con la perspectiva: **ancha cerca y angosta lejos**. Un obstáculo frena o hace esquivar solo si está dentro; el que pasa por un lado **solo avisa**.

### Geometría (cámara de agujero, enteros)
Parámetros configurables: `ANCHO_CARRO_CM`, `ALTURA_CAMARA_CM`, `INCLINACION_GRADOS` (hacia abajo), `FOV_GRADOS` (horizontal) y `MARGEN_LATERAL_CM`.
Con `f = (ancho_imagen / 2) / tan(FOV / 2)` y `cy = alto / 2`, para la fila `v`:

- `A(v) = f · sen(incl) + (v − cy) · cos(incl)`
- distancia del piso en esa fila: `d = altura · (f · cos(incl) − (v − cy) · sen(incl)) / A(v)`
- ancho de `x` cm en esa fila: `x · A(v) / altura` píxeles. Como `A(v)` crece hacia abajo, el corredor se **estrecha** hacia arriba y vale 0 en el
  horizonte (`A = 0`).

Con los valores de partida (240x176, 18 cm de carro, cámara a 10 cm y 20° hacia abajo, 60° de campo) la mitad del ancho del carro mide en píxeles:
fila 176 → 139 px (a 11 cm del carro: el corredor ya cubre todo el cuadro), 150 → 117 (13 cm), 120 → 92 (18 cm), 100 → 75 (23 cm), 80 → 58 (31 cm),
60 → 41 (46 cm), 40 → 24 (81 cm). El horizonte queda en la fila 12. Estos números salen de `pruebas_corredor` y se comprobaron contra una referencia
independiente (proyección con decimales y bisección).

### Cómo se decide
Una **caja** (rectángulo en píxeles) toca el piso por su borde de abajo: esa fila da la distancia y el ancho del corredor ahí.

- **DENTRO**: la caja cruza la huella del carro (ancho / 2 a cada lado del centro) → frena.
- **BORDE**: cruza solo el margen lateral → **en la duda, frena** (cuenta como dentro del corredor).
- **FUERA**: no toca ni el margen → solo avisa (`fuera_corredor`).
- Borde de abajo en el horizonte o sobre él: no se puede ubicar en el piso → FUERA (`sobre_horizonte`); el detector ya descarta esas filas con la fila mínima.
- Los anchos se redondean **hacia arriba** (más ancho = más prudente).

### Interfaz común: `CajaImagen`
Una sola interfaz para todas las fuentes: un rectángulo en píxeles con un **peso** y la **fuente** (`COR_FUENTE_DETECTOR`, `_MODELO`, `_FLUJO`).

| Fuente | Cajas | Peso |
|---|---|---|
| Detector de color | una por celda "no piso" (10x10 px), de la fila mínima hacia abajo (`corredor_aplicar_mascara` / `corredor_aplicar_resultado`) | 1 por celda |
| Modelo de objetos | una por objeto (`corredor_caja_de_milesimas` convierte las milésimas del modelo a píxeles) | `min_peso` (una sola basta) |
| Flujo del piso | una por bloque de 16x16 | lo que decida quien llama |

`corredor_aplicar_zonas` reparte las cajas en las tres zonas (tercio del ancho donde cae el centro de la caja), suma el peso de las que quedan dentro o
en el borde y rellena en cada `ZonaObstaculo`: `celdas_corredor` (el peso dentro), `fuera_corredor` (hay obstáculo en la zona pero el peso dentro no llega a
`min_peso` = 2 celdas) y `cercania_corredor` (1 a 3 por la fila más baja de lo que está dentro, con los mismos límites del detector: `OBS_MEDIA_MIL` y
`OBS_CERCA_MIL`). Con el corredor apagado esos tres campos se limpian y el navegador usa lo de siempre.

### En el navegador
`zona_hay` (hay obstáculo, de cualquier tipo) sigue igual y la usa **elegir lado**: un obstáculo a un lado fuera del corredor **sigue contando como ocupado** y el
carro no se aparta hacia él. Para **frenar** se usa `zona_frena` (hay obstáculo y no está fuera del corredor) y `zona_cerc_frena` (la cercanía del corredor si
hay; si no, la de siempre). Lo mismo en el reintento de esquive (mirar) y en avanzar de lado.

### En la app
- Pantalla de **Visión**, debajo de «Fuente del video»: botón **«Corredor de trayectoria: SÍ/NO»** y un campo con las medidas
  `ancho_cm altura_cm inclinación° campo_de_visión° margen_cm` (por defecto `18 10 20.0 60.0 3`; se aplica con «Listo» del teclado). Debajo, una línea por zona:
  `libre`, `obstáculo FUERA (solo avisa)` u `obstáculo DENTRO (cercanía n)`.
- `CamaraObstaculos` calcula el corredor en cada cuadro (si está activo) y `ModoSemiautonomo` se lo pasa al navegador (`NavegadorNativa.ponerCorredor`). Si el
  modelo de objetos subió una zona (fusión), esa zona cuenta entera (el corredor solo se aplica a lo que decidió el detector de color). La racha de
  `OBST_PRUEBA` ignora los obstáculos fuera del corredor.

### Hay que MEDIR (PENDIENTE HARDWARE)
Los valores de partida **no están medidos**: `ANCHO_CARRO_CM` 18, `ALTURA_CAMARA_CM` 10, `INCLINACION_GRADOS` 20, `FOV_GRADOS` 60 y margen 3 cm.

1. **Ancho del carro:** con una cinta, de la parte más ancha de una rueda a la más ancha de la otra.
2. **Altura de la cámara:** del piso al centro de la lente.
3. **Inclinación:** pon el carro en el piso mirando una pared; con un celular o transportador mide el ángulo del eje de la cámara contra la horizontal (hacia abajo es positivo).
4. **Campo de visión:** coloca dos marcas en el piso (por ejemplo a 1 m de la cámara, 70 cm una de otra), mira cuántos píxeles ocupan y calcula, o deja 60.
5. **Comprobar:** con el corredor activo, mira en la pantalla de visión una caja a 30 y 60 cm, centrada (DENTRO), a 12 cm del eje (BORDE) y a 30 cm del eje (FUERA).

### Pruebas (`pruebas_corredor.c`, 59 casos; `pruebas_navegador.c`, 9 casos del corredor)
Configuración y validación; la perspectiva contra la referencia con decimales (distancia de cada fila 100 a 176 y semiancho de la huella en 35 filas); el corredor se
estrecha de abajo hacia arriba sin excepciones; **45 clasificaciones** de objetos de 6 cm a 20, 30, 50, 80 y 120 cm y a 0, ±8 (dentro), ±13.5 (borde), ±18 y ±30 cm
(fuera) más otra cámara (320x240, 78°, carro de 22 cm); cajas sobre el horizonte y que se salen por abajo; las zonas con cajas del modelo y del flujo; el detector de
color con imágenes sintéticas (caja centrada dentro, caja a la izquierda lejos fuera, la misma caja cerca dentro); el corredor apagado; 100 000 cajas al azar
(estado en rango, coherente con los anchos, y un margen mayor nunca deja fuera lo que estaba dentro). En el navegador: fuera del corredor no frena y la ruta
termina; la cercanía del corredor manda sobre la de la zona; elegir lado sigue evitando el lado ocupado aunque esté fuera del corredor.

## Pisos negros y de 2 colores (trabajo E, lote 4)

Problemas reales: (1) con piso negro el detector marcaba objeto aunque no hubiera; (2) con un piso de 2 colores (visto en la universidad) uno de los colores
se marcaba como objeto. Código: `nucleo_c/obstaculos.c` (mejoras) y `nucleo_c/pisos_sinteticos.c` + `pruebas_pisos_dificiles.c` (la reproducción en PC con cuadros
sintéticos; solo números, ninguna imagen se guarda ni se mira).

### Qué se reprodujo y qué no (hechos medidos en PC, antes de arreglar)
Escenas sintéticas de 240x176 con ruido por píxel (sigma 3 a 8), viñeteo, degradado de luz, reflejos, error de color por bloque de 16x16 (como el JPEG), cambios de
exposición, piso negro, liso, baldosas, franjas y damero de 2 colores (con color MEZCLADO en el borde de las baldosas). «ANTES» = el detector de antes
(`mejoras_piso = 0` y la franja de 188 milésimas).

| Hipótesis | Resultado |
|---|---|
| 1. En celdas oscuras la normalización de brillo amplifica el ruido de color | **NO se reprodujo con ruido**: 0 falsos obstáculos en 60 cuadros con sigma 3 a 8, viñeteo 0 a 40 %, degradado ±30 % y error de color de ±6 niveles. La regla «mismo tono» no se usa con brillo menor a 90 y la regla de distancia de color acepta antes. |
| 1b. (la que sí salió) un piso negro con **luz que cambia en valor absoluto** | **Reproducido**: un reflejo de +40 o +60 niveles sobre la franja de referencia (abajo al centro) da falso obstáculo en 60 de 60 cuadros. En un piso negro cualquier variación de luz es pequeña en relación pero grande contra el umbral de color (40: tres canales × 13 niveles). Que el umbral bajara de 60 a 40 lo hace más sensible. |
| 2. Una referencia de un solo color deja el otro como obstáculo | **Confirmado**: con baldosas de 40 a 120 px, en 12 de 16 posiciones de la franja (cae sobre una sola baldosa) un color daba falso obstáculo en 30 de 30 cuadros. |
| 3. La exposición automática cambia el brillo entre cuadros | **Solo en ruta**: en el modo normal el detector de antes ya la absorbía (la franja agrega el color nuevo como un grupo más). Con el piso fijo de una ruta (no se agregan colores) un cambio de ×1.35 o ×0.5 marcaba todo como obstáculo en 4 de 8 escenas (39 de 39 cuadros). |

### Arreglos (`ConfigObstaculos.mejoras_piso`, 1 por defecto; con 0 se tiene el detector de antes)
1. **`BRILLO_OSCURO` = 150** (suma R+G+B; unos 50 por canal): si la celda y la referencia son oscuras NO se compara el color, solo la luminancia, con una diferencia
   mínima tolerada `DIF_MIN_OSCURO` = 90 (o el umbral de color si es mayor). Sin croma.
2. **Tira de referencia más ancha** (`OBS_REF_ANCHO_MIL` 188 → **800**, el 80 % del ancho abajo) y **apoyo mínimo `OBS_APOYO_MIN_PCT` = 15 %**: de los hasta K = 4 grupos de color
   que salen de la tira se descartan los respaldados por menos del 15 % de sus celdas (menos el más grande). Los dos colores de un piso de baldosas quedan aprendidos y una
   mancha o un objeto chico en la tira NO se aprende como piso (esas celdas, además, no cuentan para el desvío de la tira). Un objeto grande (≥ 15 % y de otro tono) sigue dando
   franja contaminada (el modelo ya aprendido se conserva). El **color intermedio entre dos grupos** (el borde de una baldosa) ya contaba como piso desde antes (distancia a la recta entre los dos
   grupos ≤ umbral / 2): las pruebas con color mezclado en el borde no dan falsos.
3. **Reajuste de brillo** (`OBS_GANANCIA_MIN_MIL` = 180, `_CONSISTENCIA_PCT` = 70, `_TOLERANCIA_MIL` = 150): si TODO el piso de la zona de avance cambia de brillo de golpe, se escalan los colores
   aprendidos por la razón mediana en vez de marcar todo como obstáculo. Solo si: la razón se aleja ≥ 18 % de 1, al menos el 70 % de las celdas (y el 60 % en cada una de las 3 zonas)
   coinciden con ella, más de la mitad de la zona tiene tono de piso, **y la tira de abajo cambió igual** (80 %). Una pared del mismo tono que el piso que llena la zona NO cumple lo último
   (la tira no cambió): ya hubo una prueba que lo atrapó. Una mochila en el cuadro durante el cambio de exposición se sigue detectando.
4. **`piso_fijo`** (1 durante una ruta): no se agregan colores nuevos ni se reaprende el piso por «todo se ve no piso» (la ruta no cambia de piso por su cuenta); el brillo sí se reajusta.
   Prueba: piso que cambia de golpe a negro a media prueba: sin `piso_fijo` se reaprende (0 falsos en los cuadros 25 a 39), con `piso_fijo` sigue marcando (15 de 15).

### Números después de arreglar (`pruebas_pisos_dificiles`, 24 casos; ANTES / NUEVO)
- Piso negro vacío con reflejo de +40 y +60 niveles (y error de color de 0, 3 y 6 niveles por bloque): ANTES 60 de 60 cuadros falsos en los 6 casos; AHORA 0 en las 12 combinaciones de reflejo (0, 20, 40, 60) y error de color. 54 escenas de ruido 3 a 8, viñeteo y degradado con un reflejo de +30: 0 falsos.
- Baldosas de 40 a 120 px en 16 posiciones: antes 30 de 30 cuadros falsos en 12 posiciones; ahora 0 en las 16. Franjas, damero y baldosas de 40 px con ruido 4 y 8: 0 falsos.
- Exposición en ruta (×1.35 y ×0.5 en liso, baldosas, franjas y damero): antes 39 de 39 en 4 de 8 escenas; ahora 0.
- **Objetos** (mano, libreta amarilla, caja azul-verdosa, mochila azul oscura) sobre los 5 pisos con ruido 5 y viñeteo 20 %: 19 de 20 combinaciones se detectan en 30 de 30 cuadros; la peor es la **mano sobre baldosas
  beige/gris (22 de 30 cuadros, nunca más de 3 seguidos sin verla)** porque la baldosa clara tiene casi el tono de la piel (límite conocido de un detector por color). La **sombra suave** y el **piso vacío**
  de cada tipo: 0 falsos.

### Botón «Aprender piso»
- **Dónde:** pantalla de **Visión** (debajo de «Fuente del video») y panel semiautónomo de **Control** (arriba de «Iniciar»). Con el carro **quieto** sobre el piso por el que va a pasar y sin objetos
  en la zona de abajo; dura ~1 s (`APRENDER_PISO_MS`).
- **Qué hace:** suma los colores de cada celda de TODA la zona de avance (no solo de la tira) en ~12 cuadros, los promedia (menos ruido), los agrupa en hasta K = 4 colores y descarta los de poco apoyo (< 15 %);
  ese es el modelo del piso (`obstaculos_aprender_iniciar / _cuadro / _terminar`, en C; pruebas con los 5 pisos).
- **Qué muestra:** cuántos colores aprendió con su RGB y el % de la zona de cada uno, el brillo medio (0 a 255) y los avisos **«MUY OSCURO»** (brillo medio por canal < 50) y **«MUY VARIADO»**
  (más del 20 % de las celdas quedó en colores de poco apoyo, o hizo falta el máximo de 4 colores).
- **La ruta lo exige:** al tocar «Iniciar» (con la cámara como fuente) la app pide haber aprendido el piso DESDE la ruta anterior; si no, avisa y un **segundo toque en 10 s confirma usar el último piso**.
  El piso aprendido «se gasta» al empezar cada ruta. Durante la ruta rige `piso_fijo`.

### Registro `PISO_DIAG`
Una línea por cuadro (etiqueta `PISO_DIAG`, nivel D): `brillo=<0..255> ganancia=<1000 = sin reajuste> grupos=<n> celdas_marcadas=<n> zonas=<izq,cen,der: 0/1> motivo=<obstaculo|franja_tapada|reajuste_brillo|piso_nuevo|piso_oscuro|todo_no_piso|ninguno, con +> piso_fijo=<0/1>`.
Se lee con `adb logcat -d -s PISO_DIAG:D` (antes: `adb logcat -G 16M` y `adb logcat -c`).

### Lo que NO resuelve
- Un objeto del mismo tono que el piso con poca diferencia de brillo (una caja beige sobre un piso beige) sigue sin verse: es el límite de un detector por color.
- Con un piso muy oscuro, un obstáculo oscuro (mochila negra sobre piso negro) tampoco: el aviso «MUY OSCURO» existe para saberlo.
- Los valores se probaron con cuadros sintéticos: **faltan los números del piso real** (casa y universidad).

### PENDIENTE HARDWARE (con pasos y números a reportar)
**En casa** (piso de prueba de 1.5 m):
1. Arma un tramo de 1.5 m con **2 colores** (cartulinas, hojas o toallas: por ejemplo cuadros de ~20 cm blancos y gris oscuro) y otro tramo de ~50 cm **negro** (cartulina o tela negra, mate).
2. App: **Control** → Semiautónomo → «Obstáculos: cámara» (y el video del carro). Carro quieto al inicio del piso de 2 colores, nada adelante: **Aprender piso**. Anota: colores aprendidos, brillo medio y los avisos.
3. **3 pasadas sin obstáculo** («Prueba de obstáculo (1 m)», aprendiendo el piso antes de cada una): el carro **no** debe frenar. Anota cuántas frenaron de 3.
4. **3 pasadas con una caja** (≥ 15 cm) a 60 cm de frente: debe frenar. Anota cuántas frenaron de 3 y a cuántos cm quedó de la caja.
5. Repite 2 a 4 sobre el tramo negro.
**En la universidad:** aprender el piso real; 3 pasadas sin obstáculo y 3 con caja; mismos números.
**Qué reportar:** por piso (2 colores, negro, universidad): colores aprendidos, brillo medio, avisos, falsas paradas de 3 y paradas con caja de 3 (+ cm), y el log:
`adb logcat -d -v time PISO_DIAG:D OBST_PRUEBA:D NAV:D VISION:I *:S`.
