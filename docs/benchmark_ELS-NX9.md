# Benchmark del detector de objetos en el Huawei ELS-NX9

Fecha: 2026-10-10. Mide cuantos milisegundos tarda un detector de objetos en este telefono, para repartir el presupuesto de 300 ms (captura, WiFi, inferencia, decision y comando). Todo se hizo con la app `app_android` (pantalla `BenchmarkActivity`) lanzada por `adb`.

## Conclusion

- **Mejor configuracion: CPU con 1 hilo** (con 1 hilo el modelo tarda 39.07 ms de inferencia). Mas hilos no ayudan en este telefono con este modelo: 2 y 4 hilos igualan o empeoran (hasta 46.75 ms).
- **Consumo del presupuesto**: un cuadro completo (decodificar + preparar + inferir + postprocesar) tarda **45.40 ms de media con JPEG de 640x480** (p95 45.64 ms, maximo 48.12 ms) y **43.38 ms con JPEG de 320x240**. Eso es el 15 % de los 300 ms.
- **Queda para captura, WiFi y comando: unos 255 ms** (252 ms en el peor cuadro medido). Esos tres pasos NO se midieron aqui; hay que medirlos con la ESP32-CAM.
- **Resolucion de entrada**: el modelo siempre reduce a 320x320, asi que la resolucion casi no cambia el tiempo (2.02 ms de diferencia). Conviene enviar **320x240**: el JPEG pesa 21210 bytes contra 65399 bytes de 640x480 (con esta foto y calidad 70), es decir menos tiempo en el WiFi.
- La **GPU** funciona pero es mas lenta que la CPU (148.95 ms contra 45.40 ms por cuadro) y tarda 1050.16 ms en abrirse. **NNAPI** tambien funciona (62.54 ms por cuadro), sin mejorar a la CPU.

## Equipo

- fabricante: HUAWEI
- modelo: ELS-NX9
- dispositivo: HWELS
- android: 10 (API 29)
- hardware: kirin990
- placa: ELS
- nucleos: 8
- abi: arm64-v8a,armeabi-v7a,armeabi
- cpuinfo_Hardware: vendor Kirin990
- cpu_part_por_nucleo: 0xd05,0xd05,0xd05,0xd05,0xd40,0xd40,0xd40,0xd40
- frecuencia_max_mhz_por_nucleo: 1954,1954,1954,1954,2360,2360,2861,2861
- estado_inicial: termico=0 bateria=26.0C enchufado=true
- Lectura de los nucleos: 8 nucleos; 4 con frecuencia maxima 1954 MHz (parte 0xd05, los pequenos) y 4 a 2360-2861 MHz (parte 0xd40, los grandes). Es el Kirin 990.
- Condiciones: pantalla encendida, telefono conectado por USB (cargando), bateria a 26-28 C, estado termico 0 (normal) en todas las mediciones. No se uso ningun modo de rendimiento especial.

## Modelo, libreria e imagenes

- Modelo: EfficientDet-Lite0 de los ejemplos oficiales de TensorFlow Lite (`lite-model_efficientdet_lite0_detection_metadata_1.tflite`, 4563519 bytes, SHA-256 `2e04c53bfeac0ac2a30c057c7e2a777594ce39baaac35a92f74fb1e8c4fc4e0b`). Descarga: `app_android/descargar_modelo.ps1`. Esta en el repositorio porque pesa menos de 15 MB.
- Entrada del modelo: UINT8 1x320x320x3. Salidas: 4 tensores (cajas 1x25x4, clases 1x25, confianzas 1x25 y cantidad), con el decodificador de cajas y el NMS dentro del propio modelo, y 25 detecciones como maximo.
- Por que ese archivo y no el de MediaPipe: el `efficientdet_lite0.tflite` de MediaPipe (float32, 13.8 MB) no trae el postproceso dentro (salidas 1x19206x90 y 1x19206x4); necesita decodificar anclas y hacer NMS aparte, y eso lo hace la libreria de MediaPipe sin dejar medir cada etapa. Se probo y se descarto.
- Libreria: TensorFlow Lite 2.17.0 (`org.tensorflow:tensorflow-lite`, `-gpu` y `-gpu-api`), sin Google Play services. Se uso el interprete directo y no Task Vision ni MediaPipe, porque esas librerias hacen preparar + inferir + postprocesar en una sola llamada y no permiten medir cada etapa por separado.
- Imagen: foto con CC0 (dominio publico) de Wikimedia Commons, "2014TIBE Day6 Hall1 Visitors Set on Chair Eating Lunch Boxes 20140210.jpg" (https://commons.wikimedia.org/wiki/File:2014TIBE_Day6_Hall1_Visitors_Set_on_Chair_Eating_Lunch_Boxes_20140210.jpg), unas personas sentadas en bancas. No esta en git: se copia al telefono con adb. Contiene personas y bancas/sillas, pero no hay botellas.
- Como la mandaria la ESP32-CAM: la app reduce la foto a 640x480 y a 320x240 y la comprime a JPEG calidad 70 (21210 bytes y 65399 bytes). Esos bytes son los que se decodifican en cada pasada.

## Metodo

- Por cada configuracion y cada tamano: 20 pasadas de calentamiento (no cuentan) y 200 mediciones. NNAPI se probo solo para anotar si funciona: 5 de calentamiento y 20 mediciones.
- Cada pasada mide por separado, con `System.nanoTime()`: decodificar el JPEG, preparar la imagen, inferencia y postproceso. Todo en milisegundos.
- "Cuadros por segundo posibles" = 1000 / media del total, con un solo cuadro a la vez (sin solapar etapas).
- Se hicieron dos corridas completas, una tras otra, para ver la repeticion.

## Resultados (corrida 1)

### CPU-1-hilo

- Apertura del interprete (primera vez): 24.48 ms
- Entrada 640x480 (JPEG de 65399 bytes, 200 mediciones):
  - Decodificar el JPEG: media 2.71, mediana 2.70, p95 2.80, minimo 2.61, maximo 3.10
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 3.59, mediana 3.58, p95 3.73, minimo 3.44, maximo 5.49
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 39.07, mediana 39.06, p95 39.27, minimo 38.67, maximo 39.87
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.04, mediana 0.04, p95 0.04, minimo 0.03, maximo 0.07
  - TOTAL por cuadro: media 45.40, mediana 45.38, p95 45.64, minimo 44.96, maximo 48.12, **22.02 cuadros por segundo posibles**
- Entrada 320x240 (JPEG de 21210 bytes, 200 mediciones):
  - Decodificar el JPEG: media 0.86, mediana 0.85, p95 0.90, minimo 0.82, maximo 0.96
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 3.44, mediana 3.44, p95 3.52, minimo 3.35, maximo 3.64
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 39.05, mediana 39.06, p95 39.25, minimo 38.74, maximo 39.40
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.02, mediana 0.02, p95 0.03, minimo 0.02, maximo 0.06
  - TOTAL por cuadro: media 43.38, mediana 43.38, p95 43.57, minimo 43.09, maximo 43.71, **23.05 cuadros por segundo posibles**

### CPU-2-hilos

- Apertura del interprete (primera vez): 12.58 ms
- Entrada 640x480 (JPEG de 65399 bytes, 200 mediciones):
  - Decodificar el JPEG: media 2.74, mediana 2.73, p95 2.83, minimo 2.63, maximo 4.04
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 3.60, mediana 3.57, p95 3.73, minimo 3.45, maximo 5.04
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 39.26, mediana 38.90, p95 39.17, minimo 38.64, maximo 97.53
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.02, mediana 0.01, p95 0.02, minimo 0.01, maximo 0.04
  - TOTAL por cuadro: media 45.62, mediana 45.23, p95 45.60, minimo 44.83, maximo 104.08, **21.92 cuadros por segundo posibles**
- Entrada 320x240 (JPEG de 21210 bytes, 200 mediciones):
  - Decodificar el JPEG: media 1.04, mediana 1.05, p95 1.12, minimo 0.83, maximo 1.15
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 4.09, mediana 4.10, p95 4.22, minimo 3.40, maximo 4.25
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 46.73, mediana 46.80, p95 47.19, minimo 38.70, maximo 52.69
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.01, mediana 0.01, p95 0.02, minimo 0.01, maximo 0.04
  - TOTAL por cuadro: media 51.88, mediana 51.98, p95 52.37, minimo 43.05, maximo 57.97, **19.28 cuadros por segundo posibles**

### CPU-4-hilos

- Apertura del interprete (primera vez): 15.55 ms
- Entrada 640x480 (JPEG de 65399 bytes, 200 mediciones):
  - Decodificar el JPEG: media 3.27, mediana 3.27, p95 3.36, minimo 3.14, maximo 3.51
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 4.20, mediana 4.19, p95 4.32, minimo 4.07, maximo 4.55
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 46.75, mediana 46.73, p95 47.27, minimo 46.09, maximo 52.32
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.01, mediana 0.01, p95 0.02, minimo 0.01, maximo 0.05
  - TOTAL por cuadro: media 54.23, mediana 54.23, p95 54.64, minimo 53.49, maximo 59.92, **18.44 cuadros por segundo posibles**
- Entrada 320x240 (JPEG de 21210 bytes, 200 mediciones):
  - Decodificar el JPEG: media 1.05, mediana 1.05, p95 1.12, minimo 0.96, maximo 1.32
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 4.11, mediana 4.10, p95 4.22, minimo 3.97, maximo 5.03
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 47.04, mediana 46.83, p95 48.88, minimo 46.04, maximo 52.69
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.01, mediana 0.01, p95 0.02, minimo 0.01, maximo 0.04
  - TOTAL por cuadro: media 52.21, mediana 51.96, p95 54.61, minimo 51.17, maximo 57.98, **19.15 cuadros por segundo posibles**

### GPU

- Apertura del interprete (primera vez): 1050.16 ms
- Entrada 640x480 (JPEG de 65399 bytes, 200 mediciones):
  - Decodificar el JPEG: media 5.13, mediana 5.14, p95 5.33, minimo 4.39, maximo 6.87
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 6.36, mediana 6.50, p95 6.69, minimo 4.25, maximo 7.72
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 137.44, mediana 136.33, p95 155.17, minimo 116.48, maximo 174.29
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.02, mediana 0.01, p95 0.02, minimo 0.01, maximo 0.04
  - TOTAL por cuadro: media 148.95, mediana 147.67, p95 166.92, minimo 128.04, maximo 186.07, **6.71 cuadros por segundo posibles**
- Entrada 320x240 (JPEG de 21210 bytes, 200 mediciones):
  - Decodificar el JPEG: media 1.60, mediana 1.60, p95 1.68, minimo 1.05, maximo 2.16
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 6.40, mediana 6.45, p95 6.71, minimo 4.10, maximo 9.31
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 133.49, mediana 133.44, p95 163.48, minimo 85.96, maximo 183.46
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.01, mediana 0.01, p95 0.02, minimo 0.01, maximo 0.04
  - TOTAL por cuadro: media 141.51, mediana 141.55, p95 171.64, minimo 94.68, maximo 191.48, **7.07 cuadros por segundo posibles**

### NNAPI (solo para anotar si funciona)

- Apertura del interprete (primera vez): 535.92 ms
- Entrada 640x480 (JPEG de 65399 bytes, 20 mediciones):
  - Decodificar el JPEG: media 3.10, mediana 3.10, p95 3.13, minimo 3.07, maximo 3.13
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 3.97, mediana 3.96, p95 4.00, minimo 3.93, maximo 4.06
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 55.46, mediana 55.41, p95 55.72, minimo 55.34, maximo 55.84
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.01, mediana 0.01, p95 0.01, minimo 0.01, maximo 0.01
  - TOTAL por cuadro: media 62.54, mediana 62.51, p95 62.75, minimo 62.41, maximo 62.92, **15.99 cuadros por segundo posibles**
- Entrada 320x240 (JPEG de 21210 bytes, 20 mediciones):
  - Decodificar el JPEG: media 0.95, mediana 0.95, p95 0.97, minimo 0.93, maximo 1.00
  - Preparar la imagen (reducir a 320x320 y pasar a bytes RGB): media 3.93, mediana 3.93, p95 3.97, minimo 3.86, maximo 3.98
  - Inferencia (red neuronal, con la supresion de duplicados NMS incluida en el modelo): media 55.60, mediana 55.58, p95 55.77, minimo 55.46, maximo 55.87
  - Postproceso (leer las 4 salidas y filtrar por confianza): media 0.01, mediana 0.01, p95 0.01, minimo 0.01, maximo 0.01
  - TOTAL por cuadro: media 60.49, mediana 60.48, p95 60.70, minimo 60.31, maximo 60.74, **16.53 cuadros por segundo posibles**

## Repeticion (corrida 2)

Total por cuadro, media en ms y cuadros por segundo posibles, comparado con la corrida 1:

- CPU-1-hilo, 640x480: corrida 1 = 45.40 ms (22.02 fps); corrida 2 = 45.37 ms (22.04 fps)
- CPU-1-hilo, 320x240: corrida 1 = 43.38 ms (23.05 fps); corrida 2 = 43.38 ms (23.05 fps)
- CPU-2-hilos, 640x480: corrida 1 = 45.62 ms (21.92 fps); corrida 2 = 46.44 ms (21.53 fps)
- CPU-2-hilos, 320x240: corrida 1 = 51.88 ms (19.28 fps); corrida 2 = 51.90 ms (19.27 fps)
- CPU-4-hilos, 640x480: corrida 1 = 54.23 ms (18.44 fps); corrida 2 = 54.25 ms (18.43 fps)
- CPU-4-hilos, 320x240: corrida 1 = 52.21 ms (19.15 fps); corrida 2 = 51.93 ms (19.26 fps)
- GPU, 640x480: corrida 1 = 148.95 ms (6.71 fps); corrida 2 = 148.23 ms (6.75 fps)
- GPU, 320x240: corrida 1 = 141.51 ms (7.07 fps); corrida 2 = 143.80 ms (6.95 fps)
- NNAPI, 640x480: corrida 1 = 62.54 ms (15.99 fps); corrida 2 = 62.63 ms (15.97 fps)
- NNAPI, 320x240: corrida 1 = 60.49 ms (16.53 fps); corrida 2 = 60.48 ms (16.53 fps)

Las medias de las dos corridas coinciden en menos de 1 ms en todas las configuraciones de CPU y NNAPI; la GPU varia hasta unos 2 ms entre corridas (es la menos estable). CPU-2-hilos con 640x480 da a veces 39 ms y a veces 47 ms (por eso su p95 es mas alto): con 2 hilos el sistema parece colocar el trabajo unas veces en nucleos mas rapidos y otras en mas lentos. Los datos crudos estan en `docs/datos_benchmark/`.

## Comprobacion de que el modelo funciona

- CPU-1-hilo, 640x480: 7 objetos con confianza de 0.4 o mas, todos de clase `person`; confianzas: 0.71, 0.71, 0.71, 0.65, 0.65, 0.54, 0.48.
- CPU-1-hilo, 320x240: 7 objetos con confianza de 0.4 o mas, todos de clase `person`; confianzas: 0.74, 0.72, 0.69, 0.57, 0.43, 0.43, 0.41.
- CPU-2-hilos, 640x480: 7 objetos (0.71, 0.71, 0.71, 0.65, 0.65, 0.54, 0.48).
- CPU-4-hilos, 640x480: 7 objetos (0.71, 0.71, 0.71, 0.65, 0.65, 0.54, 0.48).
- GPU, 640x480: 7 objetos (0.72, 0.72, 0.70, 0.65, 0.63, 0.54, 0.48).
- NNAPI, 640x480: 7 objetos (0.70, 0.70, 0.70, 0.65, 0.65, 0.54, 0.48).
- La foto tiene unas diez personas sentadas o de pie; no se detecto ninguna silla, banca ni mochila con confianza de 0.4 o mas. Con el umbral 0.4 solo salen personas. Todas las configuraciones detectan 7 personas con confianzas casi iguales (la GPU y NNAPI difieren unos centesimos).

## Que significan los numeros para los 300 ms

- En el telefono con CPU de 1 hilo y JPEG de 640x480: decodificar 2.71 ms + preparar 3.59 ms + inferencia 39.07 ms + postproceso 0.04 ms = 45.40 ms.
- La inferencia es casi todo (86 % del cuadro); decodificar y preparar suman unos 6.30 ms; el postproceso es despreciable.
- Con ese consumo quedan unos 255 ms para: captura de la ESP32-CAM, envio del JPEG por WiFi, la decision y el comando de vuelta al carro.
- Un cuadro cada ~45 ms permite hasta ~22 cuadros por segundo; el presupuesto de 300 ms pide solo ~3 por segundo, asi que hay margen para detectar mas de una vez por ciclo o probar un modelo mas pesado (por ejemplo EfficientDet-Lite1 o Lite2; habria que medirlo).

## Limitaciones

- Se uso una sola imagen y un solo telefono; las detecciones y la confianza dependen de la escena.
- El telefono estaba conectado a USB y con la pantalla encendida; con bateria, calor prolongado o la pantalla apagada el rendimiento puede bajar. Cada corrida completa dura unos minutos y el estado termico se mantuvo en 0 todo el tiempo.
- El modelo es cuantizado (UINT8); la GPU rinde mejor con modelos de coma flotante, asi que su resultado aqui no descarta la GPU en general. La GPU abrio sin errores, pero con esta combinacion fue mas lenta (probablemente porque la operacion de postproceso del modelo no corre en la GPU y obliga a pasar datos entre GPU y CPU; no se comprobo).
- No se midio captura, WiFi ni comando: eso falta para cerrar el presupuesto de 300 ms.

## Como repetirlo en otro telefono (OnePlus, tableta)

- Compilar e instalar: `gradlew assembleDebug` y `adb install -r app_android\app\build\outputs\apk\debug\app-debug.apk` (ver `docs/instalar_app_huawei.md`).
- Copiar la foto: crear la carpeta `/sdcard/Android/data/com.fsi.carro/files` y copiar ahi la foto como `bench_fuente.jpg` con `adb push` (si no esta, se usa una imagen sintetica sin objetos).
- Lanzar: `adb shell am start -n com.fsi.carro/.BenchmarkActivity --ez bench_auto true`. Dura unos minutos; la pantalla debe quedar encendida.
- Resultados: `adb logcat -d -s BENCH:I` o `adb pull /sdcard/Android/data/com.fsi.carro/files/bench_resultados.txt`.
- Si el telefono no es arm64, cambiar `abiFilters` en `app/build.gradle.kts`.


## Comparacion: detector de obstaculos por color de piso (en C)

Alternativa al modelo para objetos que COCO no conoce (cajas, paredes): compara el color de cada celda de 10x10 pixeles con el del piso de abajo. Detalle de como funciona en `docs/obstaculos.md`. Se midio en el mismo telefono, con la pantalla `BenchmarkActivity` (`--es bench_modo obstaculos`), con el mismo metodo que arriba: 20 pasadas de calentamiento y 200 mediciones, JPEG de 320x240 calidad 70, y la cadena completa decodificar el JPEG, convertir a bytes RGB y analizar en C.

### Conclusion

- **Es unas 30 veces mas rapido que el modelo**: un cuadro completo tarda **1.36 ms de media** (entre 1.24 y 1.60 ms segun la imagen; p95 de hasta 2.03 ms; el maximo medido fue 4.04 ms), contra **43.38 ms** del EfficientDet-Lite0 con CPU de 1 hilo y JPEG de 320x240. Solo el analisis en C tarda **0.59 ms** de media, contra 39.05 ms de la inferencia del modelo.
- Consume menos del 1 % del presupuesto de 300 ms (unos 1.36 ms): casi no quita tiempo a captura, WiFi y comando, y se puede ejecutar en cada cuadro junto con el modelo.
- En las 4 imagenes generadas el resultado fue el esperado (todas OK) en las dos corridas.
- Limites: son imagenes sinteticas y NO se probo con una foto real de pasillo ni con la camara de la ESP32-CAM (ver `docs/obstaculos.md`). Las imagenes sinteticas comprimen a JPEG de 2 a 4 KB; una foto real pesa mas y decodificarla tarda algo mas (con la foto real del benchmark del modelo, 320x240 tardaba 0.85 ms en decodificar).

### Resultados por imagen (corrida 1, en ms)

- piso-con-ruido: piso liso con ruido (no debe haber obstaculos). JPEG de 3685 bytes. Resultado correcto (cercania izquierda, centro, derecha = 0,0,0).
  - Decodificar el JPEG: media 0.59, mediana 0.55, p95 0.85, minimo 0.50, maximo 1.20
  - Convertir a bytes RGB (getPixels y empaquetado): media 0.41, mediana 0.39, p95 0.49, minimo 0.38, maximo 1.22
  - Analizar en C (llamada JNI + obstaculos_analizar): media 0.60, mediana 0.56, p95 0.75, minimo 0.55, maximo 2.36
  - TOTAL por cuadro: media 1.60, mediana 1.51, p95 2.03, minimo 1.43, maximo 3.40, **625.10 cuadros por segundo posibles**
- baldosas-caja-centro: baldosas de dos tonos y una caja roja al centro (esperado: centro, cercania 2). JPEG de 3112 bytes. Resultado correcto (cercania izquierda, centro, derecha = 0,2,0).
  - Decodificar el JPEG: media 0.54, mediana 0.51, p95 0.66, minimo 0.48, maximo 1.32
  - Convertir a bytes RGB (getPixels y empaquetado): media 0.17, mediana 0.16, p95 0.19, minimo 0.16, maximo 0.36
  - Analizar en C (llamada JNI + obstaculos_analizar): media 0.59, mediana 0.56, p95 0.67, minimo 0.55, maximo 2.33
  - TOTAL por cuadro: media 1.29, mediana 1.24, p95 1.50, minimo 1.20, maximo 3.02, **777.02 cuadros por segundo posibles**
- degradado-dos-objetos: degradado de luz, una caja a la izquierda y un circulo a la derecha (esperado: izquierda y derecha, cercania 3). JPEG de 3577 bytes. Resultado correcto (cercania izquierda, centro, derecha = 3,0,3).
  - Decodificar el JPEG: media 0.53, mediana 0.52, p95 0.63, minimo 0.49, maximo 1.09
  - Convertir a bytes RGB (getPixels y empaquetado): media 0.16, mediana 0.16, p95 0.18, minimo 0.16, maximo 0.55
  - Analizar en C (llamada JNI + obstaculos_analizar): media 0.60, mediana 0.57, p95 0.69, minimo 0.57, maximo 2.40
  - TOTAL por cuadro: media 1.30, mediana 1.25, p95 1.45, minimo 1.22, maximo 4.04, **771.99 cuadros por segundo posibles**
- pared: pared que llena toda la imagen menos la franja de abajo (esperado: las tres zonas, cercania 3). JPEG de 2419 bytes. Resultado correcto (cercania izquierda, centro, derecha = 3,3,3).
  - Decodificar el JPEG: media 0.49, mediana 0.49, p95 0.54, minimo 0.46, maximo 0.91
  - Convertir a bytes RGB (getPixels y empaquetado): media 0.16, mediana 0.16, p95 0.17, minimo 0.15, maximo 0.20
  - Analizar en C (llamada JNI + obstaculos_analizar): media 0.58, mediana 0.58, p95 0.60, minimo 0.57, maximo 0.92
  - TOTAL por cuadro: media 1.24, mediana 1.23, p95 1.29, minimo 1.19, maximo 1.66, **809.25 cuadros por segundo posibles**

### Repeticion (corrida 2)

- piso-con-ruido: total medio 1.58 ms (corrida 1: 1.60 ms); analisis en C 0.58 ms (corrida 1: 0.60 ms).
- baldosas-caja-centro: total medio 1.32 ms (corrida 1: 1.29 ms); analisis en C 0.59 ms (corrida 1: 0.59 ms).
- degradado-dos-objetos: total medio 1.28 ms (corrida 1: 1.30 ms); analisis en C 0.58 ms (corrida 1: 0.60 ms).
- pared: total medio 1.26 ms (corrida 1: 1.24 ms); analisis en C 0.59 ms (corrida 1: 0.58 ms).

Datos crudos: `docs/datos_benchmark/ELS-NX9_obstaculos_corrida1.txt` y `..._corrida2.txt`. Condiciones: telefono conectado por USB, estado termico 0, bateria a 28 C.

### Como repetirlo

- `adb shell am start -n com.fsi.carro/.BenchmarkActivity --ez bench_auto true --es bench_modo obstaculos` (o el boton "Benchmark de obstaculos" de la pantalla del benchmark). Dura unos segundos.
- Resultados: `adb logcat -d -s BENCH:I` o `adb pull /sdcard/Android/data/com.fsi.carro/files/bench_obstaculos.txt`.
- Para incluir una foto real: copiarla como `bench_pasillo.jpg` a `/sdcard/Android/data/com.fsi.carro/files/` con `adb push` antes de lanzarlo (no se sube a git).
