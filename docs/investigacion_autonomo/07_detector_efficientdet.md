# 07. Detector neuronal EfficientDet-Lite0 y TensorFlow Lite

Investigación técnica para el carro FSI (rama `feat/autonomo-v3`). Fecha de consulta de todas las fuentes externas: **2026-11-16**.
Convenciones: **[n]** remite a la sección 5 (fuentes). Confianza: **alta** = hoja de datos, documentación oficial, código oficial o artículo revisado; **media** = tutorial o varias fuentes secundarias coherentes; **baja** = inferencia mía (marcada **INFERENCIA**). **HECHO (proyecto)** = medido o leído por mí en este repositorio. Todo lo que no se midió en el carro está marcado **NO MEDIDO** o **SIMULADO**.

Aviso sobre cómo leí las fuentes: las páginas web pasaron por un resumidor automático; los PDF de arXiv y de ST los extraje a texto en local con `pdftotext`. Donde solo vi un resumen del resultado de búsqueda, lo digo en "NO CONSULTADA".

---

## 1. Cómo funciona

**Arquitectura (EfficientDet).** Es un detector de una etapa con tres bloques: backbone EfficientNet, red de características BiFPN y cabezas compartidas de caja y de clase [1]. Las dos ideas del artículo son (a) la **BiFPN**, una pirámide de características bidireccional con fusión ponderada, y (b) el **escalado compuesto**, que sube a la vez resolución de entrada, profundidad y anchura de backbone, BiFPN y cabezas [1]. La fusión "rápida normalizada" de la BiFPN usa pesos no negativos (ReLU) divididos entre su suma más 0.0001, y el artículo dice que es entre 26 y 31 % más rápida en GPU que un softmax [1]. La configuración base D0 es: entrada 512x512, backbone EfficientNet-B0, 64 canales y 3 capas de BiFPN, 3 capas en las cabezas [1]. Como la BiFPN usa los niveles 3 a 7, la entrada debe ser múltiplo de 128 en la familia original [1].

**Variante "Lite" (la que usa el carro).** EfficientNet-Lite cambia el backbone para móvil: quita las capas squeeze-and-excitation (poco soportadas en el hardware objetivo), cambia swish por ReLU6 (la causa de que la cuantización posterior al entrenamiento casi no pierda precisión: en la clasificación ImageNet de EfficientNet-Lite0 pasó de 75.1 % en flotante a 74.4 % cuantizado) y fija el stem y la cabeza al escalar [4]. EfficientDet-Lite0 **no es D0 con otro nombre**: el README oficial le da 3.2 M de parámetros contra los 3.9 M de D0 [2]. HECHO (proyecto): el archivo que usa la app (`efficientdet_lite0.tflite`, 4 563 519 bytes) tiene entrada **UINT8 1x320x320x3** y 4 salidas (cajas 1x25x4, clases 1x25, confianzas 1x25, cantidad) con el postproceso dentro (`docs/benchmark_ELS-NX9.md`). Que Lite0 trabaja con 320x320 y relleno gris coincide con una ficha de terceros del mismo modelo para otro acelerador [17] (confianza media, es otro runtime).

**Postproceso dentro del modelo.** HECHO (proyecto, leído del propio .tflite con `flatbuffers.flexbuffers`): el operador `TFLite_Detection_PostProcess` trae `max_detections=25`, `max_classes_per_detection=1`, `num_classes=90`, `nms_score_threshold=-inf`, `nms_iou_threshold=0.5`, `use_regular_nms=0` y escalas 1.0. Por la descripción del núcleo del operador [6], `use_regular_nms=0` significa **NMS rápido**: una sola pasada de supresión usando la mejor clase de cada ancla, sin hacerlo clase por clase. **INFERENCIA:** por eso dos cajas casi iguales con clases distintas ("person" y "refrigerator" sobre la misma región) compiten entre sí y la que gana puede cambiar de un cuadro a otro. Como el umbral de puntuación del modelo es `-inf`, **el único filtro de confianza es el de la app** (0.6 por defecto en `DetectorTflite.UMBRAL_CAJAS`; 0.01 en el modo «cualquier objeto»).

**Salida y metadatos.** La documentación de la Task Library exige para estos modelos una entrada uint8 o float32 de lote 1 con 3 canales RGB, y cuatro salidas: ubicaciones `[1 x n x 4]` en proporciones, clases, puntuaciones y número de detecciones [7]. La misma página escribe el orden de la caja como `[top, left, right, bottom]` [7]; el código del proyecto lee `[ymin, xmin, ymax, xmax]` (`DetectorTflite.postprocesarCajas`). **INFERENCIA:** el orden estándar del operador es `ymin, xmin, ymax, xmax`, pero no lo confirmé en una fuente que abriera; ver recomendación R8.

**Cuantización int8.** Con cuantización entera completa se cuantizan pesos, activaciones y entrada/salida; hace falta un conjunto representativo de ~100 a 500 muestras para calibrar rangos, y la página oficial avisa de que puede haber pérdida de precisión sobre todo en redes pequeñas [8]. El README de EfficientDet da para Lite0 mAP 26.41 (flotante) y 26.10 (int8): la cuantización cuesta ~0.3 puntos en COCO [2].

**Preprocesado de la app (HECHO, proyecto).** `DetectorTflite.preparar()` llama `Bitmap.createScaledBitmap(imagen, 320, 320, true)` sobre el cuadro de la cámara (240x176 en HQVGA) y pasa los píxeles RGB crudos (0 a 255) al tensor uint8. Es un **estirado anisotrópico sin relleno**: x se multiplica por 1.333 e y por 1.818, es decir, los objetos quedan ~1.36 veces más altos relativos a su ancho. El entrenamiento oficial hace lo contrario: la función de carga de datos del repositorio de EfficientDet escala con el factor menor de los dos lados, **conserva la proporción y rellena** abajo y a la derecha hasta el tamaño de entrada [5].

---

## 2. Parámetros y límites relevantes

### 2.1 Métricas oficiales y latencia

| Dato | Valor | Fuente |
|---|---|---|
| EfficientDet-D0 (512), COCO test-dev | AP 34.6; AP50 53.0; AP75 37.1; **APs 12.4; APm 39.0; APl 52.7**; 3.9 M parámetros; 2.54 B FLOPs | [1][2] |
| D0 latencia | 10.2 ms (V100, lote 1, incluye preprocesado y NMS) | [1][2] |
| Evaluación del artículo | un modelo, una escala, **soft-NMS** | [1] |
| Lite0 (TFLite Model Maker, int8) | **4.4 MB, 37 ms, AP 25.69 %** (mAP COCO 2017 val); latencia medida "en Pixel 4 con 4 hilos en CPU" | [3] |
| Lite1 / Lite2 (misma tabla) | 5.8 MB, 49 ms, 30.55 % / 7.2 MB, 69 ms, 33.97 % | [3] |
| SSD MobileNetV2 320x320 (comparación, misma tabla) | 6.7 MB, 24 ms, 20.2 % | [3] |
| Lite0 en README oficial | mAP 26.41 flotante, 26.10 int8; 3.2 M parámetros; latencia móvil 36 ms | [2] |
| COCO: categorías | el artículo habla de 91 tipos de objeto; EfficientDet-Lite0 trae 90 posiciones de etiqueta con 80 clases reales | [9]; HECHO (proyecto): `labelmap.txt` del modelo, 90 posiciones con "???" en las vacías |
| COCO: "pequeño" | área < 32² px (1 024); mediano 32² a 96²; grande > 96² (en píxeles de la imagen original); AP promedia IoU 0.50 a 0.95 en pasos de 0.05 | [10] |

Dos cifras de mAP de Lite0 (25.69 y 26.10/26.41) vienen de dos páginas oficiales distintas; la diferencia no está explicada en lo que leí. Los AP de D0 por tamaño (12.4 contra 52.7) son del modelo de 512 y flotante, no de Lite0 a 320; **INFERENCIA:** Lite0 a 320 px rinde peor en objetos pequeños que D0 a 512, pero no encontré la cifra por tamaño de Lite0 (**NO MEDIDO**).

### 2.2 Qué tamaño ve el modelo (INFERENCIA, cálculo mío)

Supuestos del proyecto (no medidos en el carro): campo horizontal 56°, cámara a ~11.5 cm, imagen de 240 px de ancho. Con F = 120 / tan 28° = 225.7 px, el ancho aparente de un objeto de ancho W a distancia d es 225.7·W/d (en el cuadro de 240) y 1.333 veces eso en la entrada de 320:

| Objeto (ancho) | d = 20 cm | 40 cm | 80 cm | 120 cm |
|---|---|---|---|---|
| Caja de 20 cm | 301 px | 151 | 75 | 50 |
| Objeto angosto de 7 cm (ejemplo, el termo real NO MEDIDO) | 105 | 53 | 26 | 18 |
| Pata de mueble de 3 cm | 45 | 23 | 11 | 7.5 |

(ancho en la entrada de 320 px). La altura aparente depende de la altura del objeto y de la inclinación; una pata de 3 cm a 80 cm ocupa ~11 px de ancho y es una raya vertical: para un detector entrenado con objetos de COCO (donde la mayoría tiene proporciones y contexto de fotos de Flickr) es un caso fuera de distribución.

### 2.3 Datos medidos del proyecto (HECHO)

| Dato | Valor | Dónde |
|---|---|---|
| Lite0, CPU 1 hilo, Kirin 990 (P40 Pro), inferencia | 39.07 ms; cuadro completo 43.4 ms (JPEG 320x240) y 45.4 ms (640x480) | `docs/benchmark_ELS-NX9.md` |
| Lite0 con 2 y 4 hilos | 46.7 a 47 ms de inferencia: **más hilos no ayudan** en este teléfono | ídem |
| Lite0 GPU / NNAPI | 148.95 ms por cuadro / 62.5 ms; apertura 1050 / 536 ms | ídem |
| Lite0 60 s seguidos, CPU 1 hilo | 43.3 ms primeros 10 s → 51.8 ms últimos 10 s (+20 %) | `docs/modelo_objetos.md` §5 |
| Lite1 / Lite2 CPU 1 hilo | 79.5 / 120.4 ms mediana (hasta 96 / 147 ms al final) | ídem |
| SSD MobileNetV2 COCO cuantizado por NNAPI `liteadaptor` | 11.9 ms, sin degradación en 60 s (rama `investigacion/comparativa-detectores`); detecta mejor o peor en el piso del carro: **NO MEDIDO** | `docs/comparativa_detectores.md` de esa rama |
| Modelo en el hilo de la app | como máximo 10 veces por segundo (`INTERVALO_MIN_MS = 100`) | `ModeloEnHilo.kt` |
| Marca de tiempo del resultado | la hora a la que el teléfono ofreció el cuadro al modelo (`SystemClock.elapsedRealtime()` en `CamaraObstaculos.kt`), **no** la de captura en la cámara | `CamaraObstaculos.kt` ~l. 965-971 |
| Vigencia de un resultado | 300 ms (`vigencia_ms`); sin resultado vigente 2.5 s → causa 17 | `fusion.c`, `navegador.h` |
| Cámara → teléfono (JPEG, HQVGA 240x176) | 320 ms medidos con cronómetro (una sola medida); JPEG medio 2 386 B por cuadro | `docs/latencia.md` |
| Resultado con el termo, cámara del carro, carro quieto | a ~80 cm: "person" 24 a 39 % con caja pegada a la esquina izquierda; más cerca: "refrigerator" 35 a 65 % con caja que cubre casi toda la imagen (pasó 60 % dos veces); **nunca "bottle"**; sin termo, "person" 24 a 46 % en la esquina | `docs/bitacora.md` línea "modelo de objetos con el termo" |
| Falso positivo de pared lisa | "refrigerator 43 %" sobre pared; llevó el umbral de 0.4 a 0.6 | `docs/bitacora.md`, `docs/progreso.md` |
| Recall con muebles vistos desde 11 cm | **NO MEDIDO** | `docs/estado_20261113.md` |
| Pasadas del protocolo «cualquier objeto» (falsos por minuto, % antes de 40 cm) | pendiente de hardware | `docs/modelo_objetos.md` §7-8 |

---

## 3. Errores típicos y modos de falla conocidos

1. **Objetos fuera del vocabulario.** COCO no tiene "caja", "pared", "pata de mesa", "cable", "zapato" ni "termo" (HECHO, proyecto: el `labelmap.txt` solo trae 80 nombres; los más cercanos a una caja grande y lisa son "refrigerator", "microwave", "suitcase", "tv", "oven"). El detector hará dos cosas: no emitir nada, o asignar la clase más parecida con puntuación media. La bitácora del proyecto ya lo documenta (el "refrigerator" sobre pared lisa). TIDE, una herramienta de diagnóstico de errores de detección, clasifica los errores en seis tipos y mide cuánto aporta cada uno al desempeño [12] (el resumen que leí no nombra los seis tipos). **INFERENCIA:** los dos problemas del carro (un falso "refrigerator" sobre una pared y un termo no detectado) son, en la terminología usual, un falso positivo de fondo y un objeto no detectado; conviene etiquetarlos así al medir.
2. **Tamaño.** El análisis clásico de errores de detectores (Hoiem et al., ECCV 2012) concluye que la sensibilidad al tamaño, los errores de localización y la confusión con objetos parecidos son las fuentes de error más dañinas [11]. En D0 el AP de objetos pequeños es 12.4 contra 52.7 de los grandes [2]. Esto apoya que un objeto angosto o lejano en un cuadro de 240x176 reescalado se pierde.
3. **Objetos parciales y vista rasante.** COCO son fotos tomadas a altura humana; la cámara del carro está a ~11 cm (SUPUESTO del proyecto, **NO MEDIDO**). **INFERENCIA:** vistas rasantes con la base del objeto cortada por el borde inferior del cuadro y proporciones raras (patas, bordes de sillas) están lejos de la distribución de entrenamiento; no encontré una cifra publicada de EfficientDet-Lite0 para este caso. Hoiem et al. estimaron que ser totalmente robusto a la oclusión mejoraría el AP solo unos pocos puntos en los detectores que estudiaron [11] (eran detectores de 2012 sobre PASCAL VOC; no es extrapolable a Lite0).
4. **Confianza mal calibrada.** Las redes modernas suelen estar mal calibradas: la puntuación no es una probabilidad de acierto [13]. Consecuencia práctica: el 0.6 del proyecto es un umbral empírico, no "60 % de probabilidad". El propio registro del proyecto lo muestra: "refrigerator" llegó a pasar 0.6 dos veces con el termo cerca.
5. **Estirado de proporciones (propio del proyecto).** El entrenamiento usa proporción conservada con relleno [5]; el carro estira 240x176 a 320x320 (x1.33 en ancho, x1.82 en alto). **INFERENCIA:** es un cambio de distribución que puede bajar el recall y mover las clases (todo se ve más alto y delgado). **NO MEDIDO**: no hay comparación de las dos preparaciones en el piso del carro.
6. **Calidad de imagen.** Las fuentes que abrí solo confirman en general que la compresión JPEG o H.264 daña de forma notable a muchos modelos de visión, con hasta más de 80 % de pérdida de mIoU en segmentación a compresión alta [14]; para detección con EfficientDet-Lite0 y JPEG calidad ~50 a 240x176 **no hay cifra en lo que leí**. Un estudio sobre YOLO cuantizado a INT8 midió, con datos limpios, una caída de ~3 a 7 % en mAP50-95 respecto a FP32, y no encontró que calibrar con imágenes degradadas mejorara la robustez de forma consistente [15]. **NO MEDIDO** en el carro; ver R6.
7. **Desfase temporal.** El resultado se marca con la hora a la que el teléfono ofreció el cuadro; el cuadro ya traía ~320 ms de retraso de cámara+WiFi (una sola medida, `docs/latencia.md`). A 19.3 cm/s (perfil AUTO 210) 320 + ~50 a 100 ms de modelo son ~7 a 8 cm de avance antes de que la caja exista. HECHO (cálculo con datos del proyecto).
8. **Caída del hilo o térmica.** En CPU de 1 hilo la inferencia sube ~20 % en 60 s; el estado térmico oficial no cambió (`docs/modelo_objetos.md`). Con el hilo a ≤ 10 Hz y 45 a 70 ms (`docs/latencia.md`) hay holgura; la nota de la rama de investigación dice que en la app completa el tiempo por cuadro (preparar + inferir) fue de 45 a 76 ms con el SSD por NNAPI, "parecidos a los de Lite0": casi todo es preparar la imagen, no inferir (esa conclusión es del proyecto, medida con cámara falsa).
9. **Orden y tope de detecciones.** El operador devuelve hasta 25 cajas; la app guarda las primeras 10 en el orden de salida (`ModeloEnHilo.inferir`, `FUS_MAX_CAJAS = 10`). Si el orden no es por puntuación descendente, en el modo «cualquier objeto» (umbral 0.01) se podrían perder las mejores. **NO VERIFICADO** (ver R9).

---

## 4. Buenas prácticas reconocidas

- **Usar el mismo preprocesado que el entrenamiento** (escala uniforme y relleno) y desmapear las cajas al sistema de la imagen original [5]. Si se cambia el relleno, hay que corregir las coordenadas (no basta con ymin/ymax del tensor).
- **Calibrar con muestras representativas y verificar contra el modelo flotante** al cuantizar; si el modelo cuantizado da resultados raros mientras el flotante es bueno, el culpable son los operadores cuantizados [8].
- **Leer el umbral y el NMS como parte del modelo desplegado**: aquí el operador ya trae NMS duro de IoU 0.5 [6]; el artículo evaluó con soft-NMS [1], por lo que los AP publicados no son idénticos a los del modelo desplegado (**INFERENCIA**, efecto no cuantificado).
- **No confiar en la puntuación como probabilidad**; si se necesita, calibrar con datos propios (la calibración por temperatura es el método simple que recomienda el artículo de Guo et al.) [13].
- **Diagnosticar por tipo de error** (falso positivo de fondo, no detectado, localización, confusión de clase) con datos propios en vez de solo mAP [11][12].
- **Medir en el dispositivo real y reportar condiciones**: la cifra oficial de 37 ms es de un Pixel 4 con 4 hilos [3]; en el Kirin 990 del proyecto 4 hilos fueron peor que 1.
- **Elegir el modelo por la tarea, no por el mAP en COCO**: el SSD MobileNetV2 tiene menos AP (20.2 contra 25.69) pero 24 contra 37 ms [3]; en el teléfono del proyecto corrió en 11.9 ms por NNAPI (rama de investigación).

---

## 5. Fuentes

(Fecha de consulta de todas: 2026-11-16.)

1. Mingxing Tan, Ruoming Pang, Quoc V. Le. *EfficientDet: Scalable and Efficient Object Detection.* CVPR 2020 (arXiv:1911.09070v7, 27-jul-2020). https://arxiv.org/abs/1911.09070 — leí el resumen y el texto completo del PDF (extraído en local) y una copia HTML (ar5iv); Table 1 y Table 2 coinciden en ambos. **Confianza: alta.**
2. Google, repositorio *automl/efficientdet* (README). https://github.com/google/automl/tree/master/efficientdet — tablas de D0-D7x (APs/APm/APl, parámetros, FLOPs, latencia V100) y de EfficientDet-Lite (mAP flotante/int8, parámetros, latencia móvil). Leído vía resumen automático. **Confianza: alta** (documentación oficial del autor; cifras no cotejadas con otra copia).
3. TensorFlow Blog. *Easier object detection on mobile with TensorFlow Lite.* junio 2021 (autor no capturado en mi lectura). https://blog.tensorflow.org/2021/06/easier-object-detection-on-mobile-with-tf-lite.html — tabla Lite0-Lite4 con tamaño, latencia y AP, y notas al pie (Pixel 4, 4 hilos, CPU, COCO 2017 val). **Confianza: alta.**
4. TensorFlow Blog. *Higher accuracy on vision models with EfficientNet-Lite.* marzo 2020 (autor no capturado). https://blog.tensorflow.org/2020/03/higher-accuracy-on-vision-models-with-efficientnet-lite.html — cambios de EfficientNet-Lite y cuantización (75.1 % → 74.4 % top-1 en Lite0). **Confianza: alta.**
5. Google, `automl/efficientdet/dataloader.py` (código). https://raw.githubusercontent.com/google/automl/master/efficientdet/dataloader.py — escala por el menor de los dos factores, redimensiona y rellena abajo/derecha hasta `image_size`. Leído vía resumen automático, sin ver el código literal completo. **Confianza: media-alta.**
6. TensorFlow, `tensorflow/lite/kernels/detection_postprocess.cc`. https://raw.githubusercontent.com/tensorflow/tensorflow/master/tensorflow/lite/kernels/detection_postprocess.cc — parámetros del operador y diferencia entre NMS regular y rápido. Leído vía resumen automático. **Confianza: alta.**
7. Google, *LiteRT Task Library: Integrate object detectors.* https://developers.google.com/edge/litert/inference_with_metadata/task_library/object_detector — requisitos de entrada y de las 4 salidas. **Confianza: alta.**
8. Google, *LiteRT: Post-training quantization.* https://developers.google.com/edge/litert/models/post_training_quantization — cuantización entera, conjunto representativo, advertencias de precisión. **Confianza: alta.**
9. Lin et al. *Microsoft COCO: Common Objects in Context.* arXiv:1405.0312 (2014, v3 feb-2015). https://arxiv.org/abs/1405.0312 — solo el resumen (91 tipos, 328k imágenes, 2.5 M instancias). **Confianza: alta** (solo resumen).
10. `pycocotools/cocoeval.py` (cocoapi). https://raw.githubusercontent.com/cocodataset/cocoapi/master/PythonAPI/pycocotools/cocoeval.py — rangos de área y IoU. **Confianza: alta.**
11. Hoiem, Chodpathumwan, Dai. *Diagnosing Error in Object Detectors.* ECCV 2012, pp. 340-353, doi:10.1007/978-3-642-33712-3_25. https://mlanthology.org/eccv/2012/hoiem2012eccv-diagnosing — solo el resumen (el PDF de la universidad no respondió). **Confianza: alta** (resumen); las afirmaciones sobre oclusión salen de un resultado de búsqueda, **media**.
12. Bolya, Foley, Hays, Hoffman. *TIDE: A General Toolbox for Identifying Object Detection Errors.* ECCV 2020. http://www.ecva.net/papers/eccv_2020/papers_ECCV/html/849_ECCV_2020_paper.php — solo el resumen (seis tipos de error; la página no los nombra). **Confianza: alta** para la existencia y propósito; los nombres de los tipos NO los vi en la fuente.
13. Guo, Pleiss, Sun, Weinberger. *On Calibration of Modern Neural Networks.* ICML 2017 (arXiv:1706.04599). https://arxiv.org/abs/1706.04599 — resumen. **Confianza: alta.**
14. Reich, Hahn, Cremers, Roth, Debnath. *A Perspective on Deep Vision Performance with Standard Image and Video Codecs.* arXiv:2404.12330 (2024). https://arxiv.org/abs/2404.12330 — resumen: la compresión JPEG/H.264 daña de forma notable a muchos modelos; a compresión alta la segmentación cae más de 80 % en mIoU; la página no da detección en COCO. **Confianza: alta** (para lo dicho); el efecto en detección NO está confirmado.
15. Karimov, Imani, Kazakov. *Quantization Robustness to Input Degradations for Object Detection.* arXiv:2508.19600 (2025, v3 mayo 2026). https://arxiv.org/abs/2508.19600 — resumen (YOLO, FP32/FP16/INT8, ruido, desenfoque, bajo contraste y JPEG; en datos limpios INT8 estático 3 a 7 % menos mAP50-95). **Confianza: media** (resumen de un preprint).
16. Proyecto (HECHO): `app_android/.../DetectorTflite.kt`, `ModeloEnHilo.kt`, `CamaraObstaculos.kt`, `nucleo_c/fusion.c/.h`, `nucleo_c/cualquiera.c/.h`, `docs/modelo_objetos.md`, `docs/benchmark_ELS-NX9.md`, `docs/datos_benchmark/`, `docs/latencia.md`, `docs/bitacora.md`, `docs/limites_conocidos.md`, rama `investigacion/comparativa-detectores` (lectura con `git show`), y el archivo `efficientdet_lite0.tflite` del repositorio (opciones del operador leídas con `flatbuffers.flexbuffers` y `labelmap.txt` leído con `zipfile`). **Confianza: alta** para lo medido o leído; **NO MEDIDO** donde se dice.
17. SenseCraft AI (Seeed), ficha *EfficientDet-Lite0 (Hailo-8)*. https://sensecraft.seeed.cc/ai-lab/en/models/efficientdet_lite0 — entrada 320x320x3, relleno "letterbox" gris 127, 80 clases en 89 posiciones. Es el mismo modelo en otro acelerador, no TFLite. **Confianza: media-baja.**

**NO CONSULTADA (no pude abrirla o solo vi un resumen de búsqueda):**
- Página de TensorFlow Hub / Kaggle del modelo `efficientdet/lite0/detection` (solo cargó el título).
- Página de Model Maker de LiteRT (404 en la ruta final) y el `lite/README.md` de automl (404).
- Dodge y Karam, *Understanding How Image Quality Affects Deep Neural Networks* (QoMEX 2016, arXiv:1604.04004): solo vi el resumen dentro de un resultado de búsqueda (reporta que desenfoque y ruido dañan más que JPEG en clasificación); no la abrí.
- Gandor y Nalepa (estudio de nueve detectores con compresión JPEG), citado de segunda mano en un resultado de búsqueda; no la abrí.
- Hoiem et al.: el PDF completo (la universidad no respondió); por eso no cito sus resultados sobre truncamiento.
- Neubeck y Van Gool (NMS eficiente) y Bodla et al. (Soft-NMS): no los busqué.

---

## 6. Qué implica para este carro

### 6.1 Valor usado vs. lo que dice la fuente

| Parámetro | Valor usado en el carro | Lo que dice la fuente | Comentario |
|---|---|---|---|
| Entrada | 240x176 → **estirado** a 320x320 (x1.33, x1.82) | Entrenamiento: escala uniforme + relleno [5]; ficha de terceros: letterbox gris 127 [17] | Cambio de distribución propio del proyecto. **NO MEDIDO** su efecto |
| Normalización | uint8 crudo 0-255 | El modelo es uint8 (HECHO, tensor de entrada) [7] | Coherente |
| Latencia | 39-52 ms (CPU 1 hilo, Kirin 990) | 37 ms (Pixel 4, 4 hilos, CPU) [3] | Comparable; en este teléfono 4 hilos empeoran |
| mAP | **NO MEDIDO** en el piso del carro | 25.69 % [3] (COCO val, fotos normales) | La cifra oficial no se transfiere a la vista rasante de 11 cm |
| Umbral de confianza de cajas | 0.6 (frenar); 0.15 por defecto en «cualquier objeto» (rango 0.01-0.50); diagnóstico 0.2; `UMBRAL` 0.4 | Ejemplo de inferencia oficial usa `min_score_thresh=0.4` (valor de ejemplo, no recomendación) [2]; puntuación ≠ probabilidad [13] | Los umbrales son empíricos; el 0.6 se eligió tras un "refrigerator 43 %" |
| NMS | duro, IoU 0.5, rápido, 25 cajas máx. (HECHO, modelo) | Artículo: soft-NMS [1] | AP oficial es algo optimista para el modelo desplegado (INFERENCIA) |
| Cajas guardadas | primeras 10 | 25 máx. | Verificar que salen ordenadas por puntuación (R9) |
| Vigencia del resultado | 300 ms desde que el teléfono ofreció el cuadro | — | No incluye los ~320 ms de la cámara; ver R7 |
| Frecuencia | ≤ 10 Hz (45-76 ms por inferencia) | — | Holgura suficiente; CPU se degrada 20 % en 60 s |

### 6.2 Recomendaciones (cada una con su verificación)

**R1. Mantener al detector como fuente de confirmación, no de veto ni de frenado único** (ya es así en `fusion.c`: solo color + red frena, una fuente aislada avisa). Respaldo: sus fallas conocidas son falsos positivos de fondo y objetos fuera de COCO (sección 3, puntos 1 y 2) y el recall con muebles bajos es **NO MEDIDO**. Verificable: el log `OBJ_ANY`/`MODELO` ya registra "mejor clase y %" por segundo.

**R2. Medir A/B del preprocesado (estirar contra "letterbox") con las mismas imágenes en vivo.** Hipótesis (INFERENCIA): conservar la proporción con relleno mejora el recall o las clases de objetos delgados. Prueba: con el carro quieto, objeto fijo a 40, 80 y 120 cm, dos instancias de `DetectorTflite` (o un interruptor) que preparen cada cuadro de las dos formas, y registrar solo texto (clase, %, caja). Criterio de éxito: mayor porcentaje de cuadros con caja sobre el objeto y menor "refrigerator/person" espurio en piso vacío. Nota técnica: con relleno hay que **invertir la transformación** de las cajas antes de pasarlas a `fusion.c` (hoy ymin/ymax salen directos porque el estirado es lineal y sin desplazamiento).

**R3. Cuantificar el efecto del JPEG y del tamaño de entrada, sin imágenes en el repositorio.** Los modos RAW y celdas ya existen (Trabajos D/I). Prueba: la misma escena con video JPEG (calidad 50) y con video RAW, mismo objeto, y comparar la distribución de puntuaciones del modelo (solo números). Si la diferencia es nula, el JPEG no es el problema y la resolución sí; si es grande, subir calidad o cambiar de compresor (`docs/latencia.md` §1).

**R4. Calibrar el umbral con datos propios, no con el 0.6 de un caso.** El protocolo de `docs/modelo_objetos.md` §7 ya lo propone (falsos por minuto en piso vacío y % detectado antes de 40 cm). Añadir: histograma de puntuaciones máximas por cuadro en piso vacío (liso, negro, 2 colores) para ver la cola de falsos. Respaldo: [13].

**R5. Si se quiere un detector más rápido, medir recall antes de cambiar.** El SSD MobileNetV2 cuantizado por NNAPI corrió en 11.9 ms contra 39-52 ms; pero tiene menos mAP en COCO (20.2 contra 25.69 [3]) y el recall en el piso del carro es **NO MEDIDO**. Hasta tener la tabla de `docs/comparativa_detectores.md` §6 llena, no cambiar el modelo.

**R6. Probar un recorte de la mitad inferior de la imagen** (donde están los objetos sobre el piso, ya que `cualquiera.c` descarta cajas con el borde de abajo arriba de la mitad) reescalado a 320: **INFERENCIA** que da más píxeles por objeto. Es barato de probar junto con R2.

**R7. Sellar la edad del cuadro desde la captura.** `ResultadoModelo.marcaMs` toma la hora a la que el teléfono ofreció el cuadro; añadir la edad estimada de cámara (320 ms medidos, una sola medida) a la edad que ve la fusión o, mejor, un sello por cuadro en el firmware de la cámara (ya está como "diseño" en `docs/autonomo_v2_diseno.md`). Verificable: caja negra con `cu` (edad del último cuadro) frente a un cronómetro.

**R8. Verificar con un ejemplo conocido el orden de la caja.** La documentación de la Task Library escribe `[top, left, right, bottom]` [7] y `DetectorTflite.kt` lee `[ymin, xmin, ymax, xmax]`. Prueba de un minuto: una imagen de prueba con un objeto alto en un lado conocido (cualquier foto propia, no del carro) y comparar la caja dibujada con la real. Si la fusión usa un orden equivocado, las zonas y la cercanía saldrían mal sin que se note con el termo.

**R9. Verificar que las salidas del modelo vienen ordenadas por confianza** (primer elemento = mayor). En un cuadro con muchas cajas (modo 0.01), registrar las 25 confianzas y comprobar que son decrecientes; si no, ordenar antes de truncar a 10.

**R10. Documentar el límite en el manual:** el detector no reconoce objetos fuera de COCO ni objetos angostos con esta cámara (hecho observado con el termo) y no es un sensor de seguridad; la seguridad la dan el láser (≤ 400 mm) y la coincidencia color+red.
