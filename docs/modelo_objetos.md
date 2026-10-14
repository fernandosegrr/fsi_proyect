# Modelo de objetos: modo «cualquier objeto» (Trabajo F)

## 1. Qué es y por qué

El modelo de objetos (EfficientDet) casi nunca acierta la CLASE con la cámara del carro (a ras de piso, 240x176), pero suele dibujar una caja sobre lo que está en el piso.
El modo «cualquier objeto» **ignora la clase**: usa solo la caja, la filtra con reglas geométricas y pide que aparezca en el mismo sitio en 2 cuadros seguidos del modelo.
Está **APAGADO por defecto** (botón «Cualquier objeto» en la pantalla de visión; el umbral se cambia con una barra de 0.01 a 0.50).

Código: `nucleo_c/cualquiera.c/.h` (C puro, solo enteros), `nucleo_c/pruebas_cualquiera.c` (64 casos), `nucleo_c/fusion.c` (`fusion_decidir_cualquiera`),
app: `CualquieraNativa.kt`, `CamaraObstaculos.kt` (registro `OBJ_ANY`).

## 2. Las reglas (valores por defecto)

| Regla | Valor | Por qué |
|---|---|---|
| Confianza mínima (se debe SUPERAR) | 0.15 (rango 0.01 a 0.50) | el termo da ~0.20; 0.30 lo pierde |
| Área máxima de la caja | 60 % del cuadro | el «refrigerator» de 0.43 sobre una pared cubre todo |
| Borde de abajo | en la mitad inferior | un objeto sobre el piso toca el piso abajo |
| Área mínima / ancho mínimo | 1.5 % / 4 % | motas y ruido |
| Persistencia | 2 cuadros seguidos del modelo, IoU > 50 % | las cajas que parpadean no cuentan |
| Cercanía 3 | basta 1 cuadro | ya casi se choca: no esperar |

El mismo cuadro del modelo consultado dos veces no cuenta como dos; un resultado caducado se ignora y borra la historia.
Mientras este modo está activo, `DetectorTflite.umbralCajas` baja a 0.01 para que el filtro (umbral) se haga en C y se pueda barrer.

## 3. La regla de la fusión (el color manda en seguridad)

`fusion_decidir_cualquiera` (ver `nucleo_c/fusion.h`):

1. El color ve obstáculo en una zona y «cualquier objeto» también: la zona queda con la **mayor** de las dos cercanías.
2. El color **duda** (algunas celdas, menos del mínimo) y «cualquier objeto» ve algo: se sube al valor de «cualquier objeto».
3. «Cualquier objeto» ve algo y el color no ve nada: **solo AVISO**, nunca frena.
4. El color ve obstáculo con cercanía baja (1), «cualquier objeto» tiene resultado vigente y no ve nada: se **desmiente** el falso positivo de color (con cercanía 2 o 3 nunca).
5. Sin resultado vigente: sale lo que vio el color.

## 4. Licencias y origen de los modelos

Los tres modelos son los EfficientDet-Lite «con metadata y postproceso incluido» de los ejemplos oficiales de TensorFlow Lite
(`https://storage.googleapis.com/download.tensorflow.org/models/tflite/task_library/object_detection/android/lite-model_efficientdet_lite{0,1,2}_detection_metadata_1.tflite`).

| Archivo | Bytes | SHA-256 | ¿Está en el repositorio? |
|---|---|---|---|
| `efficientdet_lite0.tflite` | 4 563 519 | `2e04c53bfeac0ac2a30c057c7e2a777594ce39baaac35a92f74fb1e8c4fc4e0b` | sí (`app_android/app/src/main/assets/`, menos de 15 MB) |
| `efficientdet_lite1.tflite` | 6 077 535 | `183afa66aad648b0dc16519ae1f55646d9cd38d2b544b917a9ab1da320ebed75` | **no** (solo para medir; se baja con el script) |
| `efficientdet_lite2.tflite` | 7 557 887 | `6fd32c84ab1eb0f7e7f3a7a20a20d7df1530daa8378728f7c79571096286bd52` | **no** |

- Licencia: la ficha pública de estos modelos en TensorFlow Hub / TensorFlow Lite Model Maker los publica con **Apache 2.0** (HECHO que figura en la ficha; **verifícalo antes de redistribuir** Lite1 y Lite2, que por eso no se guardan en el repositorio). Entrenados con COCO 2017 (anotaciones CC BY 4.0).
- Lite0 es el que se usa y el que está en el repositorio; Lite1 y Lite2 solo se bajan para medir:
  ```bash
  powershell -ExecutionPolicy Bypass -File app_android/descargar_modelos_extra.ps1
  ```
  (verifica el hash de cada uno, los deja en `app_android/modelos_extra/` —ignorada por git— y los copia al teléfono a `…/files/modelos/`, donde la app los busca primero).
- Ningún archivo de más de 15 MB está en el repositorio.

## 5. Benchmark Lite0 / Lite1 / Lite2 (Huawei P40 Pro, ELS-NX9, 60 s seguidos por configuración, enchufado)

Medido con `BenchmarkActivity` en modo `termico` (`adb shell am start -n com.fsi.carro/.BenchmarkActivity --ez bench_auto true --es bench_modo termico --es bench_modelo efficientdet_liteN.tflite`).
Datos crudos: `docs/datos_benchmark/termico_modelos_crudo.txt`. ms por inferencia (incluye el postproceso/NMS que trae el modelo).

| Modelo | Configuración | Inferencias en 60 s | ms primeros 10 s | ms últimos 10 s | ms mediana | Apertura (ms) |
|---|---|---|---|---|---|---|
| Lite0 | CPU 1 hilo | 1264 | 43.3 | 51.8 | 44.7 | 29 |
| Lite0 | GPU | 435 | 139.1 | 136.2 | 138.0 | 1026 |
| Lite0 | NNAPI | 991 | 60.6 | 60.6 | 60.5 | 293 |
| Lite1 | CPU 1 hilo | 696 | 78.8 | 95.7 | 79.5 | 34 |
| Lite1 | GPU | 276 | 218.9 | 218.4 | 220.5 | 785 |
| Lite1 | NNAPI | 576 | 104.2 | 104.2 | 104.1 | 247 |
| Lite2 | CPU 1 hilo | 454 | 119.9 | 146.7 | 120.4 | 38 |
| Lite2 | GPU | 257 | 234.5 | 220.5 | 237.3 | 768 |
| Lite2 | NNAPI | 387 | 155.3 | 155.6 | 155.2 | 330 |

**Temperatura.** El teléfono estaba enchufado y la batería marcó 29 a 32 °C (subió 1 a 3 °C en total) y el estado térmico del sistema se mantuvo en 0 (sin limitación) durante todas las corridas. Es la **temperatura de la batería** (única que el teléfono deja leer sin permisos de sistema), no la del procesador.

Lo que se ve (HECHOS medidos):
- En CPU de un hilo los tres modelos se **degradan** durante los 60 s (Lite0 43 → 52 ms, Lite1 79 → 96 ms, Lite2 120 → 147 ms: +20 %): el procesador baja su frecuencia aunque el estado térmico oficial no cambie.
- GPU y NNAPI casi no cambian de principio a fin (estables) pero son **más lentas** que la CPU en los tres modelos con este teléfono y sin servicios de Google.
- Lite1 y Lite2 cuestan 1.8x y 2.7x lo que cuesta Lite0 en CPU. A ~10 cuadros por segundo del video, Lite0 (45 ms) deja holgura; Lite2 (120–150 ms) ya no cabe sin saltar cuadros.
- **No se midió** si Lite1/Lite2 detectan MEJOR en el piso real (eso requiere las pasadas de abajo). Con estos números, Lite0 sigue siendo la opción por defecto.

## 6. Cómo se analiza una sesión (sin imágenes, solo números)

La app escribe una línea `OBJ_ANY` por cuadro con, para los umbrales 0.01, 0.05, 0.10, 0.15, 0.20 y 0.30, cuántas cajas pasan los filtros (f), cuántas persisten (p), la zona y la cercanía de la mejor.

```bash
adb logcat -G 16M
adb logcat -c
# ... hacer las pasadas ...
python scripts/analizar_obj_any.py --adb --pasadas vacio,vacio,termo,termo,caja,caja,mochila,mochila --distancia-inicial 120
python scripts/analizar_obj_any.py --autoprueba
```
Da por umbral: falsos obstáculos por minuto (pasadas `vacio`) y porcentaje de pasadas en que se detecta el objeto antes de 40 cm.
La distancia es aproximada (velocidad del perfil AUTO × tiempo; el video llega con ~0.3 s de retraso y no se descuenta).

## 7. PENDIENTE HARDWARE (lo que tienes que hacer tú)

Con el carro cargado y la app instalada (modo «cualquier objeto» encendido, umbral 0.15), `adb logcat -G 16M` y `adb logcat -c` antes de empezar:

1. 3 pasadas **sin obstáculo** de ~1 minuto en piso liso, 3 en piso negro y 3 en piso de 2 colores (mide los falsos por minuto).
2. 3 pasadas con **termo** a 120 cm del arranque, 3 con **caja**, 3 con **mochila**, en piso liso; repetir 1 pasada de cada una en piso negro y en piso de 2 colores.
3. Al terminar: `python scripts/analizar_obj_any.py --adb --pasadas <la lista en el mismo orden>` y pegar la tabla en este archivo, sección 8.
4. Decidir el umbral: el más bajo que dé 0 falsos por minuto en piso vacío y detecte los tres objetos antes de 40 cm.

## 8. Resultados de las pasadas

(vacío: pendiente de las pasadas del punto 7)
