# Recorrido de código: Visión e inferencia neuronal (`CamaraObstaculos` y `DetectorTflite`)

**Ubicación:** `app_android/app/src/main/java/com/fsi/carro/CamaraObstaculos.kt` (1292 líneas) y `DetectorTflite.kt` (317 líneas)  
**Lenguaje:** Kotlin y TensorFlow Lite runtime para Android  
**Propósito:** Procesamiento visual en tiempo real, segmentación de color de piso, rasterización del corredor y ejecución de redes neuronales convolucionales en el dispositivo móvil.

---

## 1. El pipeline de visión en `CamaraObstaculos.kt`

`CamaraObstaculos` actúa como el procesador central de imágenes de la aplicación Android:

1. **Recepción de fotogramas:** Se suscribe como oyente de `SesionVideo.kt`, recibiendo los cuadros JPEG descomprimidos en formato `Bitmap` o buffers de píxeles RGB.
2. **Segmentación y máscara cromática:**
   - Extrae el histograma de color de la región inferior central del fotograma (la zona inmediatamente contigua a las ruedas).
   - Compara cada bloque contra el color de suelo aprendido.
   - Genera una máscara binaria indicando qué zonas corresponden a suelo transitable y cuáles a discontinuidades.
3. **Proyección del corredor trapezoidal:**
   - Consulta los parámetros geométricos del vehículo (altura de montaje de la cámara, ángulo de inclinación y ancho del chasis).
   - Filtra la máscara binaria: solo las anomalías situadas dentro del trapecio de trayectoria se contabilizan como obstáculos.
4. **Alimentación hacia la inferencia:** Si la detección por redes neuronales está activada, redimensiona el cuadro a las dimensiones de entrada del modelo (habitualmente 300×300 píxeles) y lo despacha a `DetectorTflite`.

---

## 2. `DetectorTflite.kt`: Inferencia de objetos con redes neuronales

`DetectorTflite.kt` encapsula el motor de inferencia de **TensorFlow Lite**:

### 2.1 Modelo y delegados de hardware
Permite seleccionar el acelerador de cómputo del teléfono:
```kotlin
enum class Delegado { CPU, GPU, NNAPI }
class Configuracion(val nombre: String, val delegado: Delegado, val hilos: Int)
```
- **CPU (Multi-hilo):** Ejecuta la inferencia en 4 núcleos del procesador del móvil (tiempo típico de 40 a 60 ms en procesadores Kirin / Snapdragon).
- **GPU (OpenGL / OpenCL):** Delega las capas convolucionales en la unidad gráfica del teléfono para liberar al procesador principal.
- **NNAPI (Android Neural Networks API):** Aprovecha aceleradores dedicados (NPU) en terminales compatibles.

### 2.2 Proceso de inferencia y descarte de ruido
1. Normaliza los canales de píxeles a valores flotantes $[0.0, 1.0]$ o tensores cuantizados de 8 bits `UINT8`.
2. Ejecuta el intérprete (`interpreter.runForMultipleInputsOutputs`).
3. Extrae:
   - Cajas delimitadoras normalizadas: $[y_{\text{min}}, x_{\text{min}}, y_{\text{max}}, x_{\text{max}}]$.
   - Identificadores de clase (cajas de cartón, obstáculos generales, personas).
   - Puntuaciones de certeza (*scores*).
4. Aplica supresión no máxima (*Non-Maximum Suppression*, NMS) para eliminar cuadros duplicados sobre el mismo objeto.
5. Filtra aquellas detecciones cuya probabilidad supere el umbral configurado (típicamente $\ge 0.50$) y se ubiquen sobre el corredor de avance.

---

## 3. Integración con el Modo Semiautónomo

La salida consolidada de ambos algoritmos se condensa en un objeto de estado que `ModoSemiautonomo` lee en cada *tick* de 50 ms. Esto asegura que el vehículo cuente con información visual redundante y actualizada para decidir oportunamente maniobras de detención y esquive.
