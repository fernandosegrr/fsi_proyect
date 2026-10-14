# Comparativa de detectores de objetos (Trabajo H: investigación, rama `investigacion/comparativa-detectores`)

Todo está en una **rama aparte**: no cambia lo que funciona (por defecto la app sigue usando EfficientDet-Lite0 en CPU de 1 hilo). Aquí se separan **HECHOS** (medidos o leídos en la fuente) de **HIPÓTESIS** (lo que habría que comprobar). No se repite el trabajo F (Lite0/1/2: `docs/modelo_objetos.md`).

## 1. Conclusión corta

- **HECHO:** con este teléfono (Huawei P40 Pro, Kirin 990, Android 10, sin servicios de Google) un modelo **cuantizado a 8 bits** (SSD MobileNetV2 COCO de Coral) corre por NNAPI en **11.9 ms** y sin degradarse en 60 s, contra 39 a 45 ms de EfficientDet-Lite0 en CPU y 80 a 97 ms del mismo SSD en CPU. NNAPI le ofrece a Android un dispositivo `liteadaptor` de tipo ACELERADOR además de la CPU de referencia.
- **HECHO:** EfficientDet-Lite0 **no** gana nada con ese acelerador (52.7 ms contra 39 ms en CPU) y YOLO11n (ONNX Runtime, float32, 320x320) tarda 55 a 79 ms en CPU y **no** corre en el delegado NNAPI de ONNX Runtime (falla).
- **HIPÓTESIS (no demostrada):** `liteadaptor` es la NPU del Kirin 990 (Huawei HiAI). Lo que se midió lo sugiere con fuerza (7 veces más rápido que la CPU de referencia de NNAPI en el mismo modelo), pero **no hay una prueba directa** (ni contador de la NPU, ni herramienta de Huawei): por eso no se afirma que use la NPU.
- **Qué recomendar:** SSD MobileNetV2 cuantizado por NNAPI **sí** supera a Lite0 en velocidad sin pasar de 100 ms (y libera la CPU del teléfono). **Pero no se sabe si detecta tan bien en el piso del carro**: la comparación por el protocolo «cualquier objeto» es la parte PENDIENTE HARDWARE (sección 6). Hasta tenerla, **no se cambia el modelo por defecto**.

## 2. Qué se midió y cómo

`BenchmarkGenerico.kt` (modo `generico` de `BenchmarkActivity`): le da al modelo una entrada fija (un patrón repetible, **no una foto**) y cronometra solo la inferencia, 60 s seguidos por configuración. Escribe una línea `DET_MS|...` por configuración (`docs/datos_benchmark/det_ms_*.txt`). Teléfono enchufado, estado térmico 0 y batería de 30 a 31 °C durante todas las corridas.

```bash
adb push <modelo> /sdcard/Android/data/com.fsi.carro/files/modelos/
adb shell am start -n com.fsi.carro/.BenchmarkActivity --ez bench_auto true --es bench_modo generico --es bench_modelo <archivo> [--es bench_acelerador liteadaptor,nnapi-reference]
adb shell cat /sdcard/Android/data/com.fsi.carro/files/det_ms_<archivo sin extensión>.txt
```

(Si la actividad ya estaba abierta hay que hacer antes `adb shell am force-stop com.fsi.carro`; la salida de logcat se pierde con facilidad: se lee del archivo.)

| Modelo | Formato / entrada | Tamaño | Configuración | ms medio (60 s) | ms primeros 10 s → últimos 10 s | p95 | Memoria (MB, PSS) |
|---|---|---|---|---|---|---|---|
| EfficientDet-Lite0 | tflite uint8 320x320 | 4.4 MB | CPU 1 hilo | 39.5 | 36.1 → 43.1 | — | — |
| | | | CPU 4 hilos | 43.1 | 43.2 → 43.1 | — | — |
| | | | GPU | 125.5 | 125.7 → 122.4 | — | — |
| | | | NNAPI (por defecto) | 48.4 | 44.9 → 52.7 | — | — |
| | | | NNAPI `liteadaptor` | 52.7 | 52.7 → 52.7 | — | — |
| | | | NNAPI `nnapi-reference` | 1371.9 | 1371.5 → 1371.6 | — | — |
| SSD MobileNetV2 COCO (Coral) | tflite uint8 300x300 | 5.9 MB | CPU 1 hilo | 87.6 | 80.2 → 96.7 | 96.9 | 28 → 51 |
| | | | CPU 4 hilos | 96.7 | 96.7 → 96.7 | 97.0 | 51 → 53 |
| | | | GPU | 76.5 | 76.6 → 76.6 | 81.4 | 53 → 57 |
| | | | NNAPI (por defecto) | **11.9** | 11.9 → 12.0 | 12.3 | 57 → 61 |
| | | | NNAPI `liteadaptor` | **11.9** | 11.9 → 11.9 | 12.3 | 54 → 64 |
| | | | NNAPI `nnapi-reference` | 81.9 | 80.3 → 78.6 | 154.6 | 58 → 72 |
| YOLO11n (Ultralytics 8.4.177) | ONNX float32 1x3x320x320 | 10.1 MB | ONNX Runtime 1.20.0, CPU 1 hilo | 79.4 | 74.1 → 85.8 | — | — |
| | | | ONNX Runtime, CPU 4 hilos | 55.1 | 55.8 → 55.0 | — | — |
| | | | ONNX Runtime, NNAPI | **FALLO** | — | — | `AddNnapiSplit count [0] does not evenly divide dimension 3 [32]` |

Notas:
- Los números de Lite0 de esta corrida (39 ms en CPU 1 hilo) salen más bajos que los de `docs/modelo_objetos.md` (47.5 ms): el mismo modelo varía ~20 % entre corridas con el teléfono en distinto estado. Compara dentro de la misma tabla, no entre corridas.
- **SSD en CPU se degrada** (80 → 97 ms en 60 s con 1 hilo): el procesador baja su frecuencia. En NNAPI/`liteadaptor` no.
- `nnapi-reference` es la implementación de referencia en CPU de NNAPI (muy lenta con las operaciones de postproceso de Lite0).
- Los ms de la tabla son solo la inferencia. En la app completa (preparar el cuadro + inferencia + fusión) el modelo tarda más: con el SSD por NNAPI en la app (cámara falsa) se vieron `ms_modelo` de 45 a 76 ms, parecidos a los de Lite0 (`docs/latencia.md`): casi todo es preparar la imagen, no la inferencia. **Hipótesis:** hay que optimizar la preparación del cuadro (ya está medida en `BenchmarkActivity` normal) si se quiere aprovechar el 12 ms.
- YOLO11n **no se probó en TFLite**: el exportador de Ultralytics para TFLite solo funciona en Linux x86 o macOS (`AssertionError: LiteRT export only supported on Linux x86 and macOS` en esta PC con Windows), así que se midió el mismo modelo en **ONNX Runtime**. Tampoco se probó una versión cuantizada a 8 bits de YOLO (que es lo que podría correr en `liteadaptor`): **pendiente**, se puede hacer en Linux/WSL.

## 3. Cómo se obtuvieron los modelos

| Modelo | Origen | SHA-256 | En el repositorio |
|---|---|---|---|
| SSD MobileNetV2 COCO cuantizado con postproceso | [google-coral/test_data](https://github.com/google-coral/test_data) (`ssd_mobilenet_v2_coco_quant_postprocess.tflite`) | `42fb3d70ffb7bb37dd518f730f7be784b831c2078f30c497d0019cc2e987fa26` | no (6.2 MB; se baja del origen) |
| YOLO11n | `yolo11n.pt` de Ultralytics ([ficha en Hugging Face](https://huggingface.co/Ultralytics/YOLO11)), exportado a ONNX con `ultralytics` 8.4.177 en un entorno virtual aparte (`model.export(format='onnx', imgsz=320)`) | `.pt` `0ebbc80d4a7680d14987a577cd21342b65ecfd94632bd9a8da63ae6417644ee1`, `.onnx` `3dc6877acef5ef5e5871e063a1dcf29876c9e81e49c0ab3d59f7f8013def808c` | **no** (por la licencia, ver abajo) |
| EfficientDet-Lite0 | el de siempre (`docs/modelo_objetos.md`) | | sí |

## 4. Licencias

| Qué | Licencia | Qué implica para este repositorio público |
|---|---|---|
| YOLO11 y el paquete `ultralytics` | **AGPL-3.0** (o licencia empresarial de pago) — [docs.ultralytics.com](https://docs.ultralytics.com/), [ficha del modelo](https://huggingface.co/Ultralytics/YOLO11) | HECHO (lo que dice Ultralytics): AGPL-3.0 exige abrir el código de lo que se construya con él y ofrecérselo a quien use el servicio por red; si no se quiere eso, hay que comprar la licencia empresarial. **No es asesoría legal.** Consecuencia práctica aquí: **no se suben los pesos ni el código de Ultralytics al repositorio**, solo se mide y se documenta. Usarlo en el carro de una tesis o proyecto personal sin distribuirlo es un caso distinto de publicarlo; si se va a publicar la app con YOLO dentro, el repositorio completo tendría que ser AGPL-3.0 o hay que pedir la licencia empresarial. |
| SSD MobileNetV2 (archivo de Coral) | El repositorio `google-coral/test_data` trae `LICENSE` **Apache-2.0** (HECHO). El modelo viene del zoo de TensorFlow Object Detection (COCO). | Apache-2.0 permite usarlo y redistribuirlo con el aviso de licencia. **No se verificó la licencia individual del archivo del modelo** más allá del `LICENSE` del repositorio: confírmala antes de redistribuirlo. COCO: anotaciones CC BY 4.0, imágenes bajo los términos de Flickr. |
| ONNX Runtime | MIT ([github.com/microsoft/onnxruntime](https://github.com/microsoft/onnxruntime)) | Sin obligación de abrir nada. Agrega ~15 MB a la app: **solo está en esta rama**. |
| EfficientDet-Lite0 | Apache-2.0 según la ficha pública (`docs/modelo_objetos.md`) | Ya está en el repositorio. |

## 5. Huawei HiAI y la NPU (sin servicios de Google)

**HECHOS comprobados en el teléfono** (`scripts/nnapi_dispositivos.c`, compilado con el NDK y ejecutado por `adb shell`):

```
NNAPI_DISPOSITIVOS total=2
NNAPI_DISPOSITIVO indice=0 nombre=liteadaptor    tipo=ACCELERATOR nivel=29    version=100.336.020.100
NNAPI_DISPOSITIVO indice=1 nombre=nnapi-reference tipo=CPU        nivel=10000 version=102.0.0.386C605
```

Android (`ro.hardware=kirin990`, Android 10) ofrece a NNAPI **un acelerador** (`liteadaptor`) y la CPU de referencia. TensorFlow Lite lo usa solo con el delegado NNAPI, **sin Google Play Services** (el delegado viene dentro del APK).

**Evidencia a favor de que es una NPU (no una prueba):** el mismo modelo cuantizado SSD MobileNetV2 tarda 11.9 ms en `liteadaptor`, 81.9 ms en `nnapi-reference` (CPU), 77 ms en el delegado GPU de TensorFlow Lite y 87 a 97 ms en CPU con 1 o 4 hilos; y la velocidad no cae en 60 s. Un acelerador de hardware dedicado explicaría eso. **Pero** el nombre «liteadaptor» no dice qué hardware es, el controlador es de Huawei y no tengo una herramienta que mida qué unidad ejecutó el modelo. Por eso en este proyecto se dice «el acelerador NNAPI `liteadaptor`» y **no se afirma que sea la NPU**.

**Lo que dice la documentación oficial de HiAI** (no se pudo abrir la página con la herramienta de lectura: solo se leyó un resumen de búsqueda de [la guía de conversión de modelos de HiAI](https://developer.huawei.com/consumer/en/doc/hiai-Guides/model-conversion-example-0000001053125647)): el SDK/DDK de HiAI se descarga con cuenta de desarrollador de Huawei y trae el **OMG** (Offline Model Generator, en Linux de 64 bits) que convierte modelos Caffe y TensorFlow 2.x (y otros) al formato de modelo offline `.om` que la NPU ejecuta; los operadores no soportados se detectan al convertir. **No se probó**: requiere registro y descarga del DDK y conversión en Linux. Es el camino de más rendimiento posible pero el de más trabajo; el delegado NNAPI ya da un resultado muy bueno para modelos cuantizados.

**Prueba mínima propuesta para demostrar el uso de la NPU (PENDIENTE):** (1) correr el SSD cuantizado con el delegado fijado a `liteadaptor` mientras se mide el consumo de CPU del proceso (`adb shell top -H -d 1`): si la CPU casi no sube y los ms son 12, la inferencia no está en la CPU; (2) hacer la misma prueba con el modelo convertido por OMG al formato `.om` con la API de HiAI Foundation y comparar. Mientras no se haga, es hipótesis.

## 6. PENDIENTE HARDWARE: el protocolo «cualquier objeto» (umbral 0.15) con cada detector

Medir **porcentaje detectado antes de 40 cm** y **falsos por minuto en piso vacío** con el termo, la caja y la mochila, como en `docs/modelo_objetos.md` sección 7, pero cambiando el modelo con las preferencias (sin recompilar):

```bash
# SSD MobileNetV2 por NNAPI (el SSD no trae etiquetas: no importa, «cualquier objeto» ignora la clase)
adb push ssd_mobilenet_v2_coco_quant_postprocess.tflite /sdcard/Android/data/com.fsi.carro/files/modelos/
# poner en vision.xml (adb shell run-as com.fsi.carro): <string name="modelo_archivo">ssd_mobilenet_v2_coco_quant_postprocess.tflite</string> y <string name="modelo_delegado">nnapi</string>
```
(Para volver a Lite0: quitar esas dos líneas.) Cada detector: 3 pasadas sin obstáculo de 1 minuto y 3 con cada objeto a 120 cm; `adb logcat -d -v time > sesion.txt` y `python scripts/analizar_obj_any.py sesion.txt --pasadas ...`. Pegar abajo la tabla por detector y umbral. La app ya prueba que abre y corre el SSD por NNAPI (cámara falsa: se ve el registro `OBJ_ANY`); lo que falta es la medición con objetos reales.

| Detector | Umbral | Falsos por minuto (piso vacío) | % antes de 40 cm: termo | caja | mochila |
|---|---|---|---|---|---|
| EfficientDet-Lite0, CPU 1 hilo | 0.15 | | | | |
| SSD MobileNetV2 cuantizado, NNAPI | 0.15 | | | | |

## 7. Enlaces

- Ultralytics: [documentación y licencias](https://docs.ultralytics.com/), [YOLO11 en Hugging Face](https://huggingface.co/Ultralytics/YOLO11).
- Coral: [test_data (modelos y LICENSE)](https://github.com/google-coral/test_data).
- ONNX Runtime: [repositorio](https://github.com/microsoft/onnxruntime).
- HiAI: [guía de conversión de modelos](https://developer.huawei.com/consumer/en/doc/hiai-Guides/model-conversion-example-0000001053125647).
- NNAPI: [documentación de Android](https://developer.android.com/ndk/guides/neuralnetworks).
