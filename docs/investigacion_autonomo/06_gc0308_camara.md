# 06. Sensor GC0308 y su configuración en la cámara del carro

Investigación técnica para el carro FSI (rama `feat/autonomo-v3`). Fecha de consulta de todas las fuentes externas: **2026-11-16**.
Convenciones: **[n]** remite a la sección 5. Confianza: **alta** = hoja de datos, código oficial o artículo revisado; **media** = resumen, fuente secundaria o lectura a través de un resumidor automático de algo que no pude contrastar; **baja** = inferencia mía, marcada **INFERENCIA**. **HECHO (proyecto)** = medido y escrito en los documentos del repositorio. **NO MEDIDO / SIMULADO** = no se hizo en el carro.

Cómo leí: la hoja de datos [1] la leí como imágenes de página (pp. 1–6, 9–23, 25–36 de 39). El código de Espressif [2] y el del kernel de Linux [4] pasaron por el resumidor de `WebFetch`; los cuerpos de `set_exposure_ctrl`, `set_gain_ctrl`, `set_aec_value`, `set_agc_gain` y de `set_framesize` (rama de submuestreo) los pedí **textuales** y los leí tal cual. Las tablas de registros (`gc0308_settings.h`) las leí por resumen y ese resumen mezcló las páginas 0 y 1 del sensor: esos valores son confianza **media**.

## Resumen de hallazgos (lo que cambia respecto a lo que dice el proyecto)

1. **Sí existe una hoja de datos del GC0308** (copia de un tercero, V1.0 del 28-ene-2010, marcada «Only for FAE» y «GALAXYCORE CONFIDENTIAL») [1]. `docs/camara_prueba.md`, `docs/auditoria_autonomo_v2.md` y `docs/autonomo_v2_diseno.md` dicen que no hay y que escribir registros «a ciegas» es inseguro. Con ella ya no son a ciegas, con dos reservas (ver 3): es una copia no oficial de una versión inicial con varios campos «TBD», y **no la copies al repositorio** por la marca de confidencial; basta el enlace.
2. **El driver de Espressif y la hoja de datos no coinciden** en tres cosas que importan para fijar la exposición: el bit de AEC, el bit de AGC y el formato del registro de exposición manual (1.4 y 2.2). No se debe fijar nada con `set_exposure_ctrl`, `set_gain_ctrl` ni `set_aec_value` sin leer los registros antes y después.
3. **No sé qué versión del driver está compilada en el firmware.** El soporte de los controles de imagen del GC0308 se completó en el PR #814 (fusionado el **2025-12-01**) [3]; el driver de Arduino-ESP32 3.3.12 puede ser anterior o posterior. Verificable en ejecución mirando el valor de retorno de los `set_*` (2.2).
4. **El campo de visión depende del modo.** Con la configuración por defecto del driver (submuestreo), HQVGA 240×176 ve el **75 % del ancho** que ve QVGA [2]. El «56°» supuesto vale como mucho para uno de los dos modos (1.6).
5. **Recomendación sobre fijar exposición/balance de blancos:** primero **medir** (6.2), con lo que el firmware `cam-v2` ya publica; si hace falta, fijar **solo el balance de blancos** primero (es el único control cuyo bit coincide entre hoja de datos, driver y kernel), y la exposición solo con diseño de bandera apagada y liberación segura (6.3).

---

## 1. Cómo funciona

### 1.1 El sensor

Según la hoja de datos [1, pp. 3–6]: formato óptico 1/6,5", matriz activa de 648×488 píxeles (640×480 útiles), píxel de 3,4 µm, ADC de 10 bits, procesador de imagen integrado (interpolación Bayer, reducción de ruido, corrección de color, gamma, realce de bordes), salidas YCbCr 4:2:2, RGB565 y Bayer crudo, hasta 30 cuadros/s en VGA con reloj de 24 MHz, ángulo de rayo principal óptimo del lente (CRA) de 27°, alimentación 2,8 V, ventanas y espejo. SNR, sensibilidad, corriente oscura y consumo aparecen como «TBD» [1, p. 4]. **No genera JPEG**: por eso el firmware comprime por software (HECHO, proyecto: `0x106 ESP_ERR_NOT_SUPPORTED` con JPEG). Lo confirma el driver [2] (`support_jpeg`; la tabla del README lista GC0308 como YUV422, RAW Bayer y RGB565/Grayscale, 640×480, 1/6,5").

### 1.2 Lo que el sensor hace solo (hoja de datos)

Hay varios procesos automáticos que dependen del contenido de la imagen, no solo AEC y AWB:

| Bloque | Registro (página 0) | Qué hace | Fuente |
|---|---|---|---|
| **AEC** (exposición automática) | `0xd2` bit 7 `AEC_en`; objetivo de luminancia `0xd3` (defecto 0x48); promedio de luminancia del cuadro (solo lectura) `0xd4`; «actúa cada N cuadros» `0xd1[6:4]`; márgenes y velocidades `0xdb`–`0xe1`; ventana de medición `0xf7`–`0xfa` | Compara la luminancia media del cuadro con el objetivo y ajusta exposición y ganancias | [1, pp. 29–32] |
| Exposición manual | `0x03` (bits 3:0 = exposición[11:8]) y `0x04` (exposición[7:0]), en **tiempos de línea**; «controlada por AEC si AEC funciona» | 12 bits | [1, p. 10] |
| Niveles de exposición y antiparpadeo | `0xe2`–`0xeb` (paso de parpadeo y 4 niveles), `0xec` (nivel máximo), `0xed` (mínimo) | Fija exposición en múltiplos del paso de red eléctrica | [1, p. 31] |
| Límites de ganancia digital | `0xee` (post, máx.) y `0xef` (pre, máx.) | — | [1, pp. 31–32] |
| Ganancia global | `0x50` (6 bits, 2,4 bits de punto fijo, 0x10 = 1,0×, defecto 0x12); `0x51`/`0x52` pre/post ganancia «controlada por AEC, manual si se desactiva AEC» | — | [1, p. 20] |
| **AWB** | `0x22` bit 1; ganancias `0x5a`/`0x5b`/`0x5c` (R/G/B, 2,6 bits); página 1 `0x00`–`0x1e`, `0x70`–`0x79` | Elige bloques «grises», usa una curva de temperatura de color y límites de ganancia por canal, ajusta a una velocidad configurable | [1, pp. 15, 20, 33–36] |
| **ABS** (estiramiento de luminancia) | `0x22` bit 2 `ABS enable`; `0xf0`–`0xf5` | Estira la luminancia (esquina `Y_tilt`, pendiente `Y_stretch_K`) según el cuadro | [1, pp. 15, 32] |
| Saturación, ruido y bordes automáticos | `0x22` bit 4 `auto_SA`, bit 5 `auto_EE`, bit 6 `auto_DNDD`; ASDE `0x69`–`0x71` | Con ganancia total sobre un umbral (`0x69`, «modo de poca luz») sube la reducción de ruido y baja la saturación | [1, pp. 15, 21] |
| Negro automático | ABB `0x80`–`0x8a` | Corrige el nivel de negro | [1, pp. 25–26] |
| Corrección de piel y matiz | `0x21` bits 5 (piel) y 4 (`CbCr_HUE_en`) | Retoca colores en ciertos matices | [1, p. 15] |

**Efecto práctico (INFERENCIA, de leer la tabla):** el color de un píxel del piso depende de al menos AEC, AWB, ABS, ASDE y quizá la corrección de piel/matiz; los cuatro primeros dependen del *resto de la imagen*. Fijar «exposición y balance de blancos» no congela todo lo que se mueve.

### 1.3 Cómo y por qué cambian (hoja de datos + inferencia)

- **AEC.** Mide una luminancia media sobre una ventana grande (por defecto `0xf7`–`0xfa` = 0x04, 0x02, 0x98, 0x70, cada uno ×4 filas/columnas: de la columna 16 a la 608 y de la fila 8 a la 448 de 640×480; la tabla del driver [2] pone 0x04, 0x02, 0x9f, 0x78) y la compara con `0xd3` [1, pp. 30–32]. **INFERENCIA:** como la ventana es casi todo el cuadro, **un objeto grande y oscuro (o claro) que entra al encuadre cambia la exposición y por tanto el brillo del piso**, aunque el piso no cambie. Es la interacción más peligrosa para un detector por color: justo cuando un obstáculo grande se acerca, el piso se vuelve más claro. No lo medí.
- **Cuadros malos.** El sensor puede no entregar cuadros mientras aplica una exposición nueva (`0x15[1:0]`, `0xd1[3:2]` «cerrar N cuadros para eliminar cuadro malo») [1, pp. 11, 30]. Efecto esperado: huecos ocasionales en el flujo de cuadros. **INFERENCIA**, sin medir.
- **Frecuencia de cuadros.** Si la exposición es ≤ alto de ventana + blanking vertical, el período lo fija la ventana; si es mayor, lo fija la exposición [1, p. 9]. Por tanto con poca luz el período puede crecer (más latencia y más desenfoque por movimiento). El kernel de Linux lo confirma: «la frecuencia real depende sobre todo del tiempo de exposición» [4].
- **AWB.** Selecciona píxeles que cumplen criterios de «gris» (`P1:0x00`, `0x02`) y bloques pequeños válidos, y mueve las ganancias R/G/B dentro de límites (`P1:0x1c`–`0x1e`) [1, pp. 34–36]. **INFERENCIA:** un piso grande y uniforme de un solo color ocupa la mayor parte de los bloques; si el piso no es neutro, el AWB puede «corregirlo» hacia gris, y cualquier cosa que entre en el encuadre mueve esa estimación. La estimación de la luz desde una sola imagen está sub-determinada y se apoya en supuestos tipo «mundo gris» [8].
- **ASDE/ABS** dependen de la ganancia y del histograma de luminancia (ver tabla 1.2): bajan la saturación con poca luz y estiran la luminancia.

### 1.4 El driver de `esp32-camera`

Lectura del `master` (Apache-2.0) [2], con la salvedad de que **la versión compilada en el carro puede diferir** (ver 3 de los hallazgos). El historial de `sensors/gc0308.c` muestra, entre otros, «Proper support for more sensor parameters of GC0308 (#814)», 2025-12-01, autor mback2k, motivado por ESPHome en M5Stack AtomS3R-CAM y probado por su autor en esa placa; los valores por defecto se alinearon con el driver del kernel de Linux, no con la hoja de datos [3].

- **Inicio.** `reset` hace reset por software (0xf0 en `0xfe`), espera 80 ms, carga la tabla `gc0308_settings.h` (≈239 pares dirección/valor), espera otros 80 ms y selecciona la página 0. Solo en ESP32 fija el divisor de PCLK en 1 con el comentario de asegurar PCLK ≤ 15 MHz [2]. La tabla deja (lectura media) `0x22 = 0x57`, `0xd0 = 0xcb`, `0xd1 = 0x10`, `0xd2 = 0x90`, `0xd3 = 0x48`, `0x50 = 0x14`, `0x5a/0x5b/0x5c = 0x56/0x40/0x4a`, `0x24 = 0xa2`, `0x01 = 0x32`, `0x02 = 0x0c`, `0x28 = 0x00`, y la ventana de AEC `0xf7`–`0xfa` = 0x04, 0x02, 0x9f, 0x78 [2]. Con la hoja de datos, `0xd2 = 0x90` significa AEC activado (bit 7) y `0x22 = 0x57` activa AWB (bit 1), ABS (bit 2), `auto_SA` (bit 4) y `auto_DNDD` (bit 6): **los automáticos están todos encendidos de fábrica** (**INFERENCIA** cruzando [1] y [2]).
- **Controles.** Ver la tabla 2.2. En resumen: `set_whitebal`, `set_wb_mode`, `set_ae_level`, `set_agc_gain`, `set_aec_value`, `set_exposure_ctrl`, `set_gain_ctrl`, contraste/saturación/nitidez, espejo/volteo y `get_reg/set_reg` son reales en el `master`; `set_gainceiling`, `set_awb_gain`, `set_brightness`, `set_quality`, `set_aec2`, `set_denoise`, `set_dcw`, `set_bpc`, `set_wpc`, `set_raw_gma` y `set_lenc` son «dummies» que devuelven -1 [2].
- **Conflictos con la hoja de datos** (ambos leídos; cuál es correcto para esta placa **NO lo sé**):
  - `set_exposure_ctrl` cambia el **bit 0** del registro `0x22` (llamado `AAAA_EN` en el driver); la hoja de datos marca ese bit como «NA» y pone `AEC_en` en `0xd2` bit 7 [1, pp. 15, 30]. El kernel de Linux deja AEC «siempre activo» escribiendo los registros `0xd0`–`0xd2` al arrancar y no tiene control de AEC [4].
  - `set_gain_ctrl` cambia el **bit 2** de `0x22`; la hoja de datos lo llama **`ABS enable`** (estiramiento de luminancia), no AGC [1, p. 15].
  - `set_aec_value` escribe `0x03 = valor >> 4` y `0x04 = (valor << 4) & 0xF0`; la hoja de datos dice `0x03[3:0]` = exposición[11:8] y `0x04` = exposición[7:0] [1, p. 10]. Con la hoja de datos, el valor de la API no equivale a lo que el sensor interpreta. Además `set_aec_value` **no desactiva** el AEC por sí solo [2].
  - `set_whitebal` cambia el bit 1 de `0x22`: **coincide** con la hoja de datos y con el kernel (que lo usa para `V4L2_CID_AUTO_WHITE_BALANCE`) [1, 2, 4].
  - `set_agc_gain` escribe `0x50 = ganancia·63/30`; la hoja de datos define 0x10 como 1,0× y 0x3f como ≈3,94× [1, p. 20]. La tabla del driver deja 0x14 (1,25×) [2]. El rango 0–30 de la API no es una escala natural del sensor.
- **Formatos y ventana.** `set_pixformat` acepta RGB565, YUV422 y gris. `set_framesize` usa, según la opción de Kconfig, **submuestreo** (por defecto, «mayor perspectiva») o **ventana** («mayor frecuencia de cuadros») [2]. Con submuestreo la rama elige el primer cociente de una tabla (1/5, 1/4, 1/3, 1/2, 4/7, 3/5, 2/3, 1/1) cuya ventana escalada cubra el tamaño pedido; para 240×176 resulta **1/2 con una ventana centrada de 480×352** (480/640 = 75 % del ancho, 352/480 = 73 % del alto); para 320×240 resulta 1/2 con 640×480 (todo el sensor) [2, lectura textual].

### 1.5 La tubería del proyecto (lectura de `camara_prueba.ino`)

`iniciar_camara`: `xclk 20 MHz`, RGB565, HQVGA, `fb_count = 3`, `CAMERA_GRAB_LATEST`, `fb_location = PSRAM`, `jpeg_quality = 12` (solo aplica con JPEG: no se usa [2]). **No se llama ningún `set_*` ni `set_reg`** (líneas 467–492; confirmado por `docs/auditoria_autonomo_v2.md`). `tarea_captura` (núcleo 0) toma el cuadro, mide la luz (`medir_foto`, ~660 muestras cada 500 ms), y deja el cuadro en una casilla; `tarea_stream` lo comprime con `frame2jpg` (calidad 50) y lo envía; en modo CELDAS convierte RGB565→RGB888 por replicación de bits, promedia celdas de 10×10 y manda 1244 bytes por UDP. Reinicios: cualquier cambio de tamaño o formato hace `esp_camera_deinit()` + `init`, y eso recarga la tabla (borra cualquier ajuste de registros).

### 1.6 Campo de visión según el modo (INFERENCIA)

Con la lectura de 1.4: el «56°» del manual (SUPUESTO, nadie lo midió) puede corresponder al sensor completo (QVGA/VGA) o a HQVGA, pero no a los dos. Si es el de QVGA, HQVGA vería unos 43,5°; si es el de HQVGA, QVGA vería ~70,6° (`calculos/geometria_monocular.py`). El carro usa HQVGA por defecto. Si el firmware compilado usa modo ventana (recorte), el campo de HQVGA sería aún menor: 240 de 640 columnas. Tampoco encontré la ficha del lente de la placa [5], así que el campo del lente es **NO MEDIDO / NO ENCONTRADO**.

---

## 2. Parámetros y límites relevantes

### 2.1 Cifras de la hoja de datos y del driver

| Dato | Valor | Fuente |
|---|---|---|
| Matriz activa / útil | 648×488 / 640×480; píxel 3,4 µm; ADC 10 bits | [1, pp. 4–6] |
| Cuadros máximos | 30 fps en VGA con reloj de 24 MHz | [1, p. 3] |
| Reloj que espera el kernel | 24 MHz (avisa si es otro); 640×480, 320×240 y 160×120 (los dos últimos por submuestreo) | [4] |
| Exposición | 12 bits, en tiempos de línea (defecto de la hoja de datos 0x096 = 150); AEC la controla | [1, p. 10] |
| Ganancia global | 6 bits 2,4; 0x10 = 1,0×; defecto 0x12; tabla del driver 0x14 | [1, p. 20; 2] |
| Ganancias AWB | 2,6 bits en `0x5a`–`0x5c`; defecto de la hoja 0x50/0x40/0x48; tabla del driver 0x56/0x40/0x4a | [1, p. 20; 2] |
| Objetivo de AEC | `0xd3` = 0x48 (72), «valor de luminancia esperado»; el driver mapea `ae_level` −2..+2 a 0x38..0x58 | [1, p. 30; 2] |
| Salida | `0x24[4:0]`: 0x06 RGB565, 0x00–0x03 YCbCr 4:2:2 (orden de bytes); `0x24 = 0xa2` por defecto | [1, pp. 15–16] |
| Divisor de reloj | `0x28[6:4]+1`; el driver lo pone en 1 (÷2) en ESP32 | [1, p. 17; 2] |
| Rangos de la API | `quality` 0–63, `aec_value` 0–1200, `agc_gain` 0–30, `gainceiling` 0–6, `ae_level` −2..2, `wb_mode` 0–4 | [2, `sensor.h`] |

### 2.2 Qué soporta realmente el GC0308 en `esp32-camera` (master, lectura textual de los cuerpos marcados con *)

| Función | ¿Implementada? | Qué escribe | ¿Coincide con la hoja de datos? |
|---|---|---|---|
| `set_exposure_ctrl`* | Sí | `0x22` bit 0 | **NO**: ese bit es «NA»; AEC es `0xd2` bit 7 [1] |
| `set_gain_ctrl`* | Sí | `0x22` bit 2 | **NO**: es `ABS enable` [1] |
| `set_whitebal` | Sí | `0x22` bit 1 | Sí |
| `set_aec_value`* | Sí (no apaga AEC) | `0x03` = v>>4, `0x04` = (v<<4)&0xF0 | **NO**: otro reparto de bits [1, p. 10] |
| `set_agc_gain`* | Sí | `0x50` = g·63/30 | Registro sí; escala distinta |
| `set_ae_level` | Sí | `0xd3` = 0x48 + 8·nivel | Sí (objetivo de AEC) |
| `set_wb_mode` | Sí | modo 0: AWB on; 1–4: AWB off + ganancias fijas en `0x5a`–`0x5c` (soleado, nublado, oficina, casa) | Registros sí |
| `set_awb_gain` | **No** (dummy, -1) | — | — |
| `set_gainceiling` | **No** (dummy, -1) | — (los límites reales son `0xee`/`0xef`/`0xd2[1:0]`) | — |
| `set_brightness` | **No** (dummy) | — (el kernel usa `0xb5` y `0xd3` como «sesgo de exposición» [4]) | — |
| `set_quality` | **No** (dummy) | — (irrelevante: sin JPEG de sensor) | — |
| `set_contrast`, `set_saturation`, `set_sharpness` | Sí | `0xb3`, `0xb1`/`0xb2`, bordes `0x77`–`0x79` | Registros sí |
| `set_hmirror`, `set_vflip`, `set_colorbar`, `set_special_effect` | Sí | | |
| `get_reg`, `set_reg` | Sí | acceso directo con máscara (si la máscara pasa de 0xFF no escribe y devuelve 0) | — |

Todas las que escriben seleccionan primero la página 0 (`0xfe = 0x00`) [2]. **Antes del PR #814** (2025-12-01) el soporte puede haber sido menor [3]; **cómo saberlo en el carro:** llamar `s->set_exposure_ctrl(s, 1)` y `s->set_ae_level(s, 0)` (sin cambiar nada) y registrar el valor de retorno por el puerto serie; -1 o error = versión anterior.

### 2.3 Búferes, modo de captura y retraso

- `fb_count` > 1: el DMA corre continuo (hasta el doble de fps); el README de Espressif dice «úsalo solo con JPEG» porque YUV/RGB estresan el chip y pueden perder datos con WiFi [2]. El proyecto usa 3 con RGB565 y lo midió estable (12,4 fps, HQVGA).
- `CAMERA_GRAB_LATEST` con `fb_count` > 1: la cola tiene `fb_count − 1` entradas y siempre deja un búfer libre para capturar; `esp_camera_fb_get` entrega la **cabeza** de la cola (el cuadro completo más antiguo en cola); lo «más reciente» sale de que, si la cola está llena, el productor expulsa el más viejo [2, lectura de `cam_hal.c`, resumida]. Con `fb_count = 3` la cola guarda 2 cuadros: si el consumidor se retrasa, puede recibir uno de hasta ~1 período de cuadro (~80 ms a 12,4 fps) de edad extra (**INFERENCIA**; en el firmware el consumidor devuelve el cuadro a la casilla enseguida, así que normalmente no ocurre).
- `fb->timestamp` es el instante del **inicio** del primer bloque DMA del cuadro [2]: la «edad del cuadro» del proyecto incluye la lectura del sensor (~80 ms por cuadro). **HECHO (proyecto):** HQVGA 20 MHz RGB565: 12,4 fps, compresión 56 ms, edad ~137 ms; QVGA: 8,8 fps, 107 ms, 252 ms; retraso hasta la pantalla ~320 ms en una sola medida con cronómetro (`docs/camara_prueba.md`, `docs/latencia.md`).
- El sensor es de **persiana rodante** (lectura por filas, «Row decoder» en el diagrama [1, p. 5]); **INFERENCIA:** la fila de arriba y la de abajo se exponen con hasta un período de cuadro de diferencia (~80 ms) y, en un giro, la imagen se inclina; a 193 mm/s eso son ~15 mm de avance entre arriba y abajo. No medido.

### 2.4 Calidad JPEG, submuestreo de croma y ruido sobre el color

- **Compresor por defecto:** `frame2jpg` → `jpge`; submuestreo por defecto `H2V2` (4:2:0); calidad 1–100 (el valor 0 se vuelve 1); RGB565 se expande **llenando con ceros los bits bajos** (no replica bits); YUV422 se convierte primero a RGB [2, `to_jpg.cpp`]. 4:2:0 reduce a la mitad la resolución de croma en horizontal y vertical (un cuarto de las muestras) [6]; los errores de croma «sangran» en los bordes saturados [6].
- **Efecto sobre un detector que promedia celdas de 10×10** (todo **INFERENCIA**, cálculo mío): (a) la cuantización de RGB565 tiene pasos de 8 niveles en R y B y 4 en G; el error del promedio de una celda de 100 píxeles es ≤ ±4/±2/±4 en el peor caso (suma |Δ| ≤ ~10 contra el umbral 40) y el relleno con ceros de `frame2jpg` mete un sesgo de ~−3,5/−1,5/−3,5 por canal, **igual** para piso y objeto, así que casi no afecta a la *diferencia*; el modo celdas replica bits (sin ese sesgo); la diferencia entre los dos caminos es ~9 en suma de canales (¼ del umbral): importa si se aprende el piso por un camino y se evalúa por el otro. (b) El DC de cada bloque de 8×8 se cuantiza con paso 16 en luminancia a calidad 50 (primer elemento de la matriz estándar) [7]; como el DC es 8 veces la media del bloque, el error de media por bloque es ≤ ~1 nivel; para croma el paso estándar de DC es 17 (de memoria, **no lo verifiqué en una fuente abierta**). Por eso la medición del proyecto de que «el JPEG solo cambia los bordes finos» es plausible para celdas lisas. (c) 4:2:0 hace que una celda en un borde de color tenga croma mezclado con el vecino; afecta a las celdas del contorno de un objeto, no a las del interior.
- **Ruido y procesado del sensor:** el sensor aplica reducción de ruido y realce de bordes por defecto (tabla 1.2; `0x20` = 0x7f según la lectura de la tabla, que activa defectos, ruido, interpolación, realce, CC, gamma y LSC [1, p. 15]) y baja la saturación con ganancia alta [1, p. 21]. La saturación del piso depende por tanto de la luz.

### 2.5 XCLK, PCLK y frecuencia de cuadros

- README de Espressif: ejemplo a 20 MHz [2]. En el ESP32, `ll_cam.c` usa «filtros de alta velocidad» (menos bytes por píxel muestreados) cuando XCLK > 10 MHz y filtros estándar a 10 MHz o menos [2]. El kernel espera 24 MHz [4]; la hoja de datos da 30 fps a 24 MHz [1].
- **HECHO (proyecto):** las fps de captura escalan con XCLK (QVGA: 5,0 / 8,0 / 9,4 fps a 10 / 16 / 20 MHz; HQVGA: 8,0 / 12,8 / 12,4).
- **Cruce con la hoja de datos (INFERENCIA, valores de registro leídos por resumen):** con las fórmulas de [1, p. 9] (`row_time = Hb + Sh_delay + win_width + 4`; `Ft = VB + Vt + 8`) y la tabla del driver (Hb = 0x132 = 306; `Sh_delay` = 0x2a = 42; VB = 12; ventana de 640 o 480 columnas + 8), una fila dura 1000 (QVGA) u 840 (HQVGA) ciclos de PCLK y un cuadro 500 u 372 filas. Con **PCLK = XCLK/4** (el divisor ÷2 del driver más un ÷2 que no pude explicar; **no verificado**) salen 5,0 / 8,0 / 10,0 fps en QVGA y 8,0 / 12,8 / 16,0 en HQVGA: coinciden con las medidas en 5 de 6 casos y el sexto (HQVGA a 20 MHz: 12,4 medido contra 16,0) queda sin explicar. Una hipótesis (más débil) es que ahí manda la exposición máxima del AEC (480 líneas ≈ 80 ms ≈ 12,4 fps) y no el sensor. **Esto es comprobable gratis:** si la hipótesis es cierta, el fps de HQVGA a 20 MHz cambiará con la luz (sube con más luz, baja con menos). El proyecto solo midió con Y ≈ 125 constante, así que no puede afirmar que «los fps no dependen de la luz». NO CONCILIADO.

### 2.6 Valor usado en el proyecto vs. lo que dice la fuente

| Parámetro | Valor en el proyecto (`camara_prueba.ino`) | Lo que dicen las fuentes |
|---|---|---|
| XCLK | 20 MHz (`CONFIG_DEFECTO`) | Ejemplo del README 20 MHz [2]; hoja de datos 30 fps a 24 MHz [1]; kernel espera 24 MHz [4] |
| Formato | RGB565 | El sensor da YUV422/RGB565/Bayer [1]; el README prefiere capturar JPEG y convertir (no aplica: sin JPEG de sensor) [2] |
| Tamaño | HQVGA 240×176 | En submuestreo = ventana de 480×352 reducida ½ (75 % del ancho de VGA) [2]; en ventana = recorte de 240×176 |
| `fb_count` / `grab_mode` | 3 / `GRAB_LATEST` | >1 «solo con JPEG» [2]; con LATEST la cola guarda `fb_count − 1` [2] |
| `jpeg_quality` de `camera_config_t` | 12 | Solo con formato JPEG; aquí se ignora [2] |
| Calidad de `frame2jpg` | 50 (1–100) | Rango 1–100 [2] |
| Submuestreo de croma | 4:2:0 (`jpge`, por defecto) | [2, 6] |
| RGB565→RGB888 | `frame2jpg`: bits bajos en 0; celdas: replicación | [2] |
| Exposición, ganancia, AWB | 100 % automáticos; ningún `set_*` | Tabla del driver: `0xd2 = 0x90`, `0x22 = 0x57` (AEC, AWB, ABS, auto_SA, auto_DNDD activos según [1]) |
| Telemetría fotométrica | 660 muestras/500 ms, medias RGB, mitad inferior, quemadas y oscuras | Sin equivalente en las fuentes |
| Pines (`WROVER_KIT`) | XCLK 21, SIOD 26, SIOC 27, VSYNC 25, HREF 23, PCLK 22, Y2..Y9 = 4, 5, 18, 19, 36, 39, 34, 35 | Coinciden con la tabla de Freenove [5] |
| Campo 56° | SUPUESTO | Ninguna fuente para este módulo; depende del modo (1.6) |

---

## 3. Errores típicos y modos de falla conocidos

1. **Llamar `set_*` de AEC/AGC creyendo que fijan lo que dicen** (conflicto con la hoja de datos, 1.4): el efecto observable puede ser ninguno (bit «NA») o cambiar el estiramiento de luminancia (ABS). Reportado por mí; **no probado en la placa**.
2. **`set_aec_value` sin apagar AEC:** el AEC lo sobreescribe en pocos cuadros [2, el driver no lo apaga; 1, p. 10 dice que 0x04 «lo controla AEC si funciona»].
3. **Exposición fijada mal:** imagen negra o quemada; recuperable volviendo a activar AEC o reiniciando el sensor (recarga la tabla).
4. **Reinicio del sensor borra el ajuste** (cambio de tamaño, `hacer_reinicio`, recuperación): el bloqueo se pierde sin aviso si no se publica en `/stats`.
5. **Poca luz:** exposición larga, período de cuadro mayor, desenfoque por movimiento, más ruido y menos saturación (ASDE) [1].
6. **Cuadros viejos tras demanda inactiva:** el driver puede entregar uno de hace segundos (el firmware ya los descarta por edad > 450 ms).
7. **Páginas de registros:** olvidar volver a la página 0 tras tocar la página 1 deja lecturas/escrituras en otro banco [1, 2]; el driver siempre selecciona la página 0 antes de escribir.
8. **Versiones de driver distintas** (antes/después de #814 [3]): un `set_*` que funciona en una puede devolver -1 en otra.
9. **`fb_count` > 1 con RGB/YUV** puede perder datos si el sistema se carga (README [2]); medir `descartados` y `reconexiones`.

---

## 4. Buenas prácticas reconocidas

- **Leer antes de escribir y verificar después** (mirar el registro, esperar unos cuadros y ver que la imagen no se salió de rango). Es lo que hace el propio driver para el estado (`init_status` lee y no escribe) [2].
- **Fijar primero lo que cambia color sin riesgo de imagen negra:** el AWB (si se congela, queda un tinte; no se pierde la imagen). La exposición manual, solo con límites y con retorno automático.
- **Dejar que AEC/AWB converjan antes de aprender o bloquear:** el AEC actúa cada N cuadros y descarta cuadros malos [1, pp. 11, 30]; aprender el piso en el primer segundo tras un cambio puede fotografiar un estado transitorio (**INFERENCIA**).
- **Un único dueño del sensor** (en el proyecto, `tarea_captura`) para que ninguna otra tarea toque registros a la mitad de un cuadro.
- **Publicar el estado real** (valores leídos de los registros, no los pedidos) en la telemetría.
- **No guardar ajustes del sensor en memoria no volátil:** un reinicio debe devolver siempre al modo automático.
- **Medir antes de optimizar:** número de cuadros, luz media, razón R/G y B/G, no imágenes (privacidad).
- **Citar versiones:** registrar la versión del driver y del firmware en cada prueba.

---

## 5. Fuentes

1. GalaxyCore Inc. *1/6.5'' VGA CMOS Image Sensor GC0308 DataSheet*, V1.0, 2010-01-28, 39 páginas, portada «Only for FAE», marcas de agua «GALAXYCORE CONFIDENTIAL». Copia de un tercero: https://server7.whycan.com/files/members/3/GC0308_Datasheet.pdf — consultada 2026-11-16 (descargada por la herramienta de lectura; leí como imágenes las páginas 1–6, 9–23, 25–36; **NO leí** pp. 7–8 [bus serie], 24 [DNDD/EEINTP] ni 37–39 [pines y encapsulado]). Otra copia, https://wiki.linux-sunxi.org/A31s/HDK/A31s_HDK-20140222/device_data/camera/GC0308_DataSheet_V1.0_release_0311.pdf, devolvió HTTP 521 y no la leí. Confianza: **alta** para lo que dice el fabricante, con la salvedad de que es una copia no oficial de la versión 1.0 y puede diferir del silicio actual.
2. Espressif. Repositorio `esp32-camera`, rama `master` (Apache-2.0), consultado 2026-11-16 vía `WebFetch` en `raw.githubusercontent.com/espressif/esp32-camera/master/`: `sensors/gc0308.c`, `sensors/private_include/gc0308_regs.h`, `.../gc0308_settings.h`, `driver/include/sensor.h`, `driver/include/esp_camera.h`, `driver/esp_camera.c`, `driver/cam_hal.c`, `target/esp32/ll_cam.c`, `conversions/to_jpg.cpp`, `Kconfig`, `README.md` (https://github.com/espressif/esp32-camera). Los cuerpos de funciones marcados con * en 2.2 y la rama de submuestreo de `set_framesize` los leí textuales; lo demás por resumen del auxiliar (puede contener errores; la tabla de `gc0308_settings.h` mezcló las páginas 0 y 1). Confianza: **alta** para el código leído textual; **media** para el resto. **No es la versión compilada en el carro.**
3. Espressif. Historial de `sensors/gc0308.c` (https://github.com/espressif/esp32-camera/commits/master/sensors/gc0308.c; 9 entradas visibles, de 2021-07-15 a 2026-02-13) y PR #814 «Proper support for more sensor parameters of GC0308» (https://github.com/espressif/esp32-camera/pull/814), fusionado 2025-12-01 por me-no-dev; autor mback2k; probado en M5Stack AtomS3R-CAM con ESPHome. Confianza: **media-alta** (lectura por resumen; el PR no lista los controles que eran dummies antes).
4. Sebastian Reichel. Driver de Linux `drivers/media/i2c/gc0308.c` (GPL-2.0, 2023): https://raw.githubusercontent.com/torvalds/linux/master/drivers/media/i2c/gc0308.c — consultada 2026-11-16 por resumen. Confianza: **media** (código oficial del kernel; el propio código marca como «undocumented» muchas escrituras de inicio).
5. Freenove. Documentación FNK0047, página de pines: https://docs.freenove.com/projects/fnk0047/en/latest/fnk0047/codes/C/Preface.html (tabla de pines de la cámara; **no nombra el sensor**) y repositorio https://github.com/Freenove/Freenove_ESP32_WROVER_Board (README sin datos del sensor); consultadas 2026-11-16. Confianza: **alta** para los pines. Resultados de búsqueda (sin abrir) describen la placa Freenove con OV2640; la versión del proyecto es 3.0 con GC0308 (**HECHO, proyecto**: PID 0x9B, `docs/camara_prueba.md`).
6. Wikipedia, *Chroma subsampling*: https://en.wikipedia.org/wiki/Chroma_subsampling — consultada 2026-11-16 (definición de 4:2:0 y 4:2:2, «sangrado» de croma en bordes). Confianza: **media**.
7. Wikipedia, *JPEG*: https://en.wikipedia.org/wiki/JPEG — consultada 2026-11-16 (solo la primera mitad de la página: matriz de cuantización de calidad 50, primer elemento 16; 4:2:0 como razón más común). Confianza: **media**.
8. Gijsenij, Gevers, van de Weijer, *Computational Color Constancy: Survey and Experiments*, IEEE TIP 20(9):2475–2489, 2011, https://staff.fnwi.uva.nl/th.gevers/pub/GeversTIP11.pdf — leídas pp. 1–2 (la estimación del iluminante desde una imagen está sub-determinada). Confianza: **alta** para lo citado.

**NO CONSULTADAS / NO ENCONTRADAS:** esquema o ficha del lente de la placa Freenove v3.0 (campo de visión, distancia focal); página oficial de GalaxyCore; la copia de linux-sunxi (521); ITU-T T.81 (JPEG) y la documentación de `jpge`; la **versión de `esp32-camera` incluida en el core Arduino-ESP32 3.3.12** (su `idf_component.yml` no lista `esp32-camera`, lo que sugiere que viene precompilada en las bibliotecas del core: no verifiqué dónde) y el valor de Kconfig compilado (submuestreo o ventana); docs.espressif.com; el resto de la hoja de datos (pp. 7–8, 24, 37–39).

**Fuentes internas leídas:** `camara_prueba/camara_prueba.ino`, `camara_prueba/src/JPEGENC/LEEME.txt`, `docs/camara_prueba.md`, `docs/exposicion.md` (es el guion de la presentación oral, **no** trata la exposición de la cámara), `docs/latencia_modos.md`, `docs/auditoria_autonomo_v2.md`, `docs/autonomo_v2_diseno.md`, `cajas_negras/cn_20261115_*.txt` (solo números).

---

## 6. Qué implica para este carro

### 6.1 ¿Conviene fijar o limitar exposición y balance de blancos después de «Aprender piso»?

**Hoy no se puede afirmar que sí ni que no: falta medir.** Lo que hay:

- **A favor de fijar (INFERENCIA):** (1) el AEC mide casi todo el cuadro, así que un obstáculo grande al entrar cambia el brillo del piso justo cuando más importa; (2) ASDE/ABS retocan saturación y luminancia con la ganancia; (3) el piso se aprende con la exposición del momento y la ruta puede empezar hasta 63 s después (`docs/auditoria_autonomo_v2.md`); (4) un cambio de balance de blancos no tiene compensación en el detector, solo el reajuste escalar de brillo (≥ 18 %); (5) una exposición larga en poca luz también causa desenfoque y fps variable.
- **Indicios medidos, pero no concluyentes (HECHO, proyecto, 7 cajas negras del 15-nov, firmware de cámara «anterior», sin `foto`):** en una corrida (la 5, pared 60 cm tras el destino) el color de referencia del piso en `VTEL` fue (203,183,190), luego (244,245,245), (186,186,187) y (155,139,144); en la corrida 1 hubo un solo cuadro con reajuste de brillo ×1,237 (`gan=1237`, referencia (232,205,212) contra ~(186,165,173) el resto). No se puede separar exposición/balance de blancos de «el piso/obstáculo entró en la franja»: justo para eso sirve la telemetría `foto`, que esas corridas no tuvieron.
- **En contra de fijar:** el detector ya tolera brillo del mismo tono dentro de ±29 % (`UMBRAL_BRILLO` 150 sobre un piso de 524) y croma hasta 30; un AEC que mueva el piso menos que eso no produce falsos positivos. Fijar la exposición quita la adaptación a cambios reales de luz (sol, lámpara): puede quemar o aplastar y empeorar justo lo que la regla de brillo cubre hoy. Y el riesgo de escribir registros no desaparece.
- **Riesgo si se fija mal (cámara sin imagen):** exposición muy baja/alta (imagen negra o blanca), AEC fuera de rango, ajuste que sobrevive a un cambio de tamaño sin aviso, o una escritura a la página equivocada. Mitigaciones en 6.3. Peor caso con el AWB: tinte de color, nunca imagen negra.
- **Veredicto provisional:** medir (6.2); si el piso cambia más de lo tolerado sin que cambie la escena, fijar **primero solo el AWB** (`set_whitebal(0)`: coincide en [1], [2] y [4]); la exposición, solo después y solo con la bandera y la liberación de 6.3.

### 6.2 Medición barata en el carro para decidir (no requiere escribir registros)

0. **Cargar `cam-v2`** (ya compilado en el repositorio, commit `af786c7`: publica en `/stats` la clave `foto` con medias RGB del cuadro y de su mitad inferior, quemadas y oscuras cada 500 ms). Las corridas del 15-nov usaron firmware «anterior» (`foto=sin_datos`).
1. **Prueba estática** (10 min; carro quieto sobre su piso; un guion que lea `/stats` a 2 Hz y guarde `foto.r_abajo/g_abajo/b_abajo`, `foto.quemados_mil`, `fps_captura`): (a) piso libre y estable (control); (b) una cartulina oscura o clara que cubra ≥ 30 % del encuadre durante 10 s y se retira (simula el obstáculo grande); (c) lámpara encendida/apagada; (d) mancha de sol o linterna sobre el piso. Para cada caso anotar el cambio máximo de la mitad inferior en % y en razón R/G y B/G, y el tiempo de recuperación.
2. **Criterio de decisión** (INFERENCIA, de la tolerancia del detector): hay problema que justifica fijar algo si, **con el piso igual**, la mitad inferior cambia más de ~15 % de brillo, o R/G o B/G más de ~5 %, en el caso (b) o (c). Si en (b) cambia < 5 %, el AEC no es la causa de los falsos positivos y no se bloquea nada. Referencia: `UMBRAL_COLOR` 40 equivale a ~7 % de un piso de (186,165,173).
3. **Control de fps con la luz** (gratis, resuelve 2.5): observar `fps_captura` y la edad en las pruebas (c) y (d). Si cambian con la luz, el período de cuadro depende de la exposición.
4. **Lectura de solo-lectura de registros** (cambio mínimo, bandera `LEER_REGISTROS_SENSOR`, **sin escribir nada salvo la página 0**): publicar en `/stats` cada 500 ms los valores de `0x22`, `0xd2`, `0xd3`, `0xd4` (luminancia media del sensor), `0x03`/`0x04`, `0x50`–`0x52`, `0x5a`–`0x5c`. Sirve para (i) comprobar que el mapa de la hoja de datos [1] vale para esta placa (se espera `0x22 = 0x57`, `0xd2 = 0x90`, `0xd3 = 0x48`, `0x50 = 0x14`, `0x5a/0x5b/0x5c = 0x56/0x40/0x4a` justo tras arrancar, si la tabla es la de [2]); (ii) separar «cambió la luz» de «cambió el AEC/AWB» (si `r_abajo` cambia y `0x5a`–`0x5c` no, no es AWB); (iii) saber qué versión del driver hay (los `set_*` que devuelven -1).
5. **Prueba C4** del protocolo (`cambio_de_luz_tras_aprender`, hoy pendiente): repetirla dos veces, con y sin el bloqueo de 6.3 (solo si 1–3 dicen que hace falta). Métrica: paradas sin motivo de 3 pasadas.
6. Medir también el **campo de visión en HQVGA y en QVGA** (05, 6.3) y anotar `config.tam` en la caja negra.

### 6.3 Diseño propuesto: bloqueo con bandera apagada y liberación segura (no implementado)

**Principios:** apagado por defecto; un solo dueño del sensor; leer antes de escribir; nada en memoria no volátil; cualquier duda libera.

- **Bandera de compilación** `FOTOMETRIA_BLOQUEO` = 0. Con 0 no se compila nada y `/control?fotometria=` contesta 400 (un cliente actual lo trata como «firmware anterior» y sigue como hoy, igual que el diseño de `docs/autonomo_v2_diseno.md`). Con 1 solo actúa si la sonda de solo-lectura (6.2, 4) vio los valores esperados y el PID 0x9B.
- **Quién ejecuta:** una petición pone `fotometriaPedida` y `tarea_captura` (único dueño de la cámara, mismo patrón que `hacer_reinicio`) lo hace entre dos cuadros tomando `candadoUso`.
- **Secuencia de bloqueo:** (1) con AEC y AWB activos, leer y guardar una instantánea de `0x22`, `0xd2`, `0x03`/`0x04`, `0x50`–`0x52`, `0x5a`–`0x5c`; (2) **AWB primero:** `set_whitebal(0)`; leer `0x5a`–`0x5c` dos veces con 100 ms de separación y exigir que no cambien; (3) pasados 5 cuadros con luminancia estable (cambio < 3 %), **exposición:** poner a 0 solo `0xd2` bit 7 con `set_reg` (dirección de [1], no `set_exposure_ctrl`), leer de vuelta `0x03`/`0x04`/`0x50` y exigir que sigan iguales; (4) con 5 cuadros más, la luminancia de `medir_foto` debe quedar dentro de ±5 % y sin cuadros quemados/oscuros por encima de un umbral; si algo falla, **liberar** y responder error.
- **Latido y arrendamiento:** el bloqueo caduca solo. El teléfono manda cada 1 s un mensaje UDP dedicado (por ejemplo `FOTO <n>`, por el puerto 4210 que ya atiende `tarea_udp`; hoy `PING` ya llega cada segundo pero no guarda la hora) y la cámara guarda `ultimoLatidoMs`. El vigilante que ya existe (tarea de núcleo 0, prioridad 5) libera si pasan > 3 s sin latido o si el bloqueo dura más de 120 s sin renovación explícita de la app. El tope existe para que un teléfono colgado no deje la cámara bloqueada con la luz cambiando.
- **Liberación:** restaurar en orden inverso lo guardado (AEC primero, luego AWB) y verificar por lectura; si no verifica, `hacer_reinicio(cfg)` (`esp_camera_deinit` + `init` recarga la tabla con todo automático); si tampoco, el vigilante actual por cuadro viejo (> 5 s) reinicia la placa y vuelve a los valores de fábrica. Libera además por: cambio de tamaño o formato, reconexión de WiFi, cambio de cliente UDP, error de lectura o escritura de registros, cuadros quemados u oscuros en más de la mitad de las muestras durante > 1 s, y fin o aborto de la ruta en la app.
- **Estado visible:** `/stats` añade `fotometria: {estado: libre|bloqueado, desde_ms, motivo_liberacion, liberaciones, expo, ganancia, awb:[r,g,b], y_sensor}` (valores leídos, no pedidos) y la caja negra de la app copia esa línea en `CAB`/`VTEL` (en las corridas de bloqueo).
- **Orden en la app:** «Aprender piso» (con automáticos) → esperar luminancia estable → pedir bloqueo → ruta → liberar siempre al terminar, abortar o ir a segundo plano. Si el bloqueo falla, la ruta sigue como hoy, con aviso, y la caja negra anota `fotometria=no_bloqueada`.
- **Lo que NO hace:** no elige valores nuevos de exposición o ganancia (congela los del instante), no cambia el límite de exposición ni el antiparpadeo, y no toca ABS, ASDE ni la corrección de piel (siguen automáticos: ver 1.2).
- **Pruebas antes de cargar:** en PC con un sensor simulado (registros como un arreglo, caídas de latido, fallos de verificación, doble solicitud, reinicio del sensor a medio bloqueo); en el carro, primero con la ruta desconectada (motores sin pila) y la cámara mirando una escena fija, comprobando por `/stats` que el bloqueo se libera al cortar el latido a los 3 s.

### 6.4 Otras recomendaciones verificables

1. **Corregir el texto del proyecto:** `docs/camara_prueba.md`, `docs/auditoria_autonomo_v2.md` y `docs/autonomo_v2_diseno.md` dicen «no hay hoja de datos del GC0308». Cambiar a «hay una copia no oficial V1.0 (enlace en esta investigación); falta validarla contra la placa».
2. **No usar `set_exposure_ctrl`, `set_gain_ctrl` ni `set_aec_value`** en este sensor hasta comprobar con la sonda de registros qué escriben realmente (1.4).
3. **Decidir el modo de submuestreo:** averiguar si el `sdkconfig` del core usa submuestreo o ventana y medir el campo en cada modo; ajustar el campo del corredor (05, 6.2–6.3).
4. **Probar `fb_count = 2`** (cola de 1 con LATEST) solo si se mide edad extra por cola; el README no lo recomienda con RGB565 [2].
5. **Aprender el piso por el mismo camino que usa la ruta** (JPEG con JPEG): el relleno de bits de `frame2jpg` y la replicación del modo celdas difieren ~9 en suma de canales (2.4, INFERENCIA).
6. **Esperar a que la luz converja** antes de terminar «Aprender piso» (luminancia estable en 5 cuadros).
7. **Registrar la versión del driver** (retorno de `set_ae_level(0)` y `set_exposure_ctrl(1)`) y del firmware en cada caja negra.
8. **Lo que sigue NO MEDIDO:** cuánto cambia el piso por AEC/AWB en el carro; la dependencia de fps con la luz; el campo de visión por modo; la versión del driver compilada; el comportamiento real de los bits de `0x22`; el retraso de cuadros con `fb_count` distinto; el efecto real de la persiana rodante en giros.
