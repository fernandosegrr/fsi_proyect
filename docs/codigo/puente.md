# Recorrido de código: Puente JNI (`puente.c`)

**Ubicación:** `app_android/app/src/main/cpp/puente.c` (3467 líneas)  
**Lenguaje:** C estándar con cabeceras de la API JNI de Android (`<jni.h>`)  
**Propósito:** Servir como adaptador bidireccional entre la máquina virtual Android (Kotlin/JVM) y las bibliotecas matemáticas y algorítmicas en C99 de [`nucleo_c/`](file:///e:/Dev/fsi_proyecto/nucleo_c/README.md).

---

## 1. Justificación arquitectónica

En aplicaciones móviles de robótica con visión e inferencia concurrente, delegar el cómputo cinemático y la máquina de estados en el recolector de basura de la JVM introduce riesgos de pausas (*GC pauses*) de duración impredecible.

`puente.c` permite:
- Ejecutar la máquina de navegación a velocidad nativa sin sobrecarga de memoria gestionada.
- Compilar los mismos archivos fuente de `nucleo_c/` en Android sin mantener copias duplicadas de los algoritmos.
- Convertir tipos de datos primitivos de Java (arreglos de flotantes, enteros y cadenas) en estructuras C fuertemente tipadas (`Punto`, `CalibracionCarro`, `ResultadoObstaculos`).

---

## 2. Convenciones de enlace y manejo de punteros

Cada función expuesta sigue la convención estándar de nomenclatura de exportación JNI:
`Java_<paquete>_<clase>_<metodo>`.

Para mantener el estado del navegador a lo largo del tiempo sin recrear estructuras en cada ciclo, se utiliza el patrón de **puntero nativo opaco**:
1. Kotlin solicita inicializar el objeto (`iniciarNative()`).
2. `puente.c` reserva memoria con `malloc(sizeof(Navegador))`, inicializa la estructura con `navegador_iniciar()` y retorna el puntero convertido en un entero de 64 bits (`jlong`) hacia Kotlin.
3. En cada llamada subsecuente (`tickNative`, `aporteSensNative`), Kotlin envía ese `jlong`.
4. El puente valida que el puntero no sea nulo, realiza un *cast* de retorno a `(Navegador *)` y opera sobre la instancia.
5. Al destruir el componente, `destruirNative()` libera la memoria con `free()`.

---

## 3. Módulos y funciones clave implementadas

### 3.1 Enlace con el Navegador (`NavegadorNativa.kt`)
- `Java_com_fsi_carro_navegacion_NavegadorNativa_iniciarNative(JNIEnv *env, jobject thiz)`: Reserva e inicializa una instancia de `Navegador`.
- `Java_com_fsi_carro_navegacion_NavegadorNativa_cargarRutaNative(JNIEnv *env, jobject thiz, jlong ptr, jfloatArray xs, jfloatArray ys, jint n)`:
  - Extrae los arreglos primitivos de coordenadas métricas usando `(*env)->GetFloatArrayElements`.
  - Construye el arreglo nativo `Punto puntos[n]`.
  - Invoca `navegador_cargar_ruta()`.
  - Libera los buffers temporales con `JNI_ABORT` para evitar copias innecesarias.
- `Java_com_fsi_carro_navegacion_NavegadorNativa_tickNative(JNIEnv *env, jobject thiz, jlong ptr, jlong ahora_ms, ...)`:
  - Deserializa las banderas de obstáculos reportadas por la cámara.
  - Ejecuta `navegador_tick()`.
  - Retorna un código entero correspondiente al comando `ComandoNav` (`0 = STOP`, `1 = FORWARD`, `2 = REVERSE`, `3 = LEFT`, `4 = RIGHT`).
- `Java_com_fsi_carro_navegacion_NavegadorNativa_aporteSensNative(...)`: Traslada la telemetría `SENS` del WebSocket hacia `navegador_aporte_sens()`.

### 3.2 Enlace de Planificación de Rutas (`TrayectoriaNativa.kt`)
- `Java_com_fsi_carro_trayectoria_TrayectoriaNativa_calcularPasosNative(...)`:
  - Recibe la secuencia de waypoints dibujados por el usuario sobre la cuadrícula.
  - Llama a `trayectoria_calcular_pasos()`.
  - Construye y retorna un objeto compuesto de Java con la lista de pasos temporizados y sus orientaciones correspondientes.

### 3.3 Enlace de Calibración (`CalibracionNativa.kt`)
- Transfiere los valores de milisegundos por centímetro, balance de tracción y constantes de giro calculadas en las pantallas de calibración hacia estructuras nativas `CalibracionCarro` utilizadas por el navegador.

### 3.4 Enlace de Visión y Celdas (`VisionNativa.kt`)
- Procesa buffers de píxeles o paquetes de celdas UDP (`CEL1`) llamando a `obstaculos_analizar_celdas()` y `corredor_verificar_interseccion()`.

---

## 4. Conexión con el sistema

- **Archivo de construcción:** Compilado por el NDK de Android mediante `app_android/app/src/main/cpp/CMakeLists.txt`.
- **Bibliotecas enlazadas:** Enlaza directamente los archivos fuente ubicados en `../../../../nucleo_c/` (`navegador.c`, `calibracion.c`, `trayectoria.c`, `obstaculos.c`, `corredor.c`, etc.).
