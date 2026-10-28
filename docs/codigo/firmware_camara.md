# Recorrido de código: Firmware de la cámara (`camara_prueba`)

**Ubicación:** `camara_prueba/camara_prueba.ino` (1340 líneas), `camara_prueba/recuperacion.h` y `camara_prueba/recuperacion.c`  
**Lenguaje:** C++/Arduino en el archivo principal; C estándar (C99) en el módulo de recuperación  
**Propósito:** Adquisición de imágenes con el sensor GC0308, compresión JPEG por software aprovechando la memoria PSRAM, servidor HTTP/UDP y vigilancia automática ante fallos de red.

---

## 1. Arquitectura de dos tareas y casilla única

Debido a que el sensor GalaxyCore GC0308 entrega imágenes en formato crudo RGB565 sin comprimir, la placa debe realizar la compresión JPEG por software (`frame2jpg`). Para evitar que la compresión bloquee la captura del siguiente cuadro, el programa divide el trabajo entre los dos núcleos del ESP32:

```
 Núcleo 0: Tarea de Captura             Núcleo 1: Tarea de Servidor
 ┌─────────────────────────┐            ┌─────────────────────────┐
 │ esp_camera_fb_get()     │            │ Toma cuadro más reciente│
 │ Deposita en casilla     │──(buffer)─►│ Comprime con frame2jpg  │
 │ Si hay uno viejo, tira  │            │ Envía por HTTP /stream  │
 └─────────────────────────┘            └─────────────────────────┘
```

- **Mecanismo de casilla única:** Solo existe un buffer activo entre tareas. Si el teléfono Android o la red WiFi van lentos, la tarea de captura descarta los cuadros intermedios viejos en lugar de acumularlos en una cola. Esto asegura que el cuadro enviado por HTTP sea siempre el más fresco posible, evitando retrasos crecientes en la visualización.
- **Límite de edad (`EDAD_MAX_US` = 450 ms):** Si un cuadro tarda más de 450 ms en ser despachado, se descarta automáticamente.

---

## 2. Modos de transmisión

Implementados en `camara_prueba.ino:160-220`:

1. **Streaming HTTP Multipart (`/stream`):**  
   Sirve video continuo MJPEG delimitado por cabeceras `Content-Type: multipart/x-mixed-replace;boundary=frame`. Es el modo consumido por el visor de la aplicación móvil.
2. **Cuadro estático (`/capture`):**  
   Entrega una fotografía JPEG individual con cabeceras `Content-Type: image/jpeg`.
3. **Cuadro crudo (`/raw`):**  
   Transmite los bytes del buffer RGB565 directamente sin comprimir, útil para algoritmos de calibración que requieren fidelidad exacta de color.
4. **Matriz de Celdas UDP (`puerto 4210`):**  
   Divide el cuadro HQVGA (240×176) en una cuadrícula de 24×17 celdas de 10×10 píxeles. Promedia los canales R, G y B de cada celda y los empaqueta bajo la cabecera binaria `CEL1` (1244 bytes por paquete). Permite al teléfono detectar obstáculos con un consumo mínimo de ancho de banda.

---

## 3. Vigilante de red y recuperación (`recuperacion.c`)

El archivo `recuperacion.c` es C puro probado en PC mediante `pruebas_firmware/pruebas_camara.c`. Proporciona dos mecanismos de supervisión:

### 3.1 Vigilante de enlace WiFi
- Si la conexión con el AP del carro se pierde, inicia una política de reintentos con **esperas crecientes (retroceso exponencial):** 1 s, 2 s, 4 s, 8 s y fijándose en 10 s posteriores.
- Si transcurren **60 segundos continuos** sin lograr conectarse a la red, el módulo ordena el reinicio físico del microcontrolador mediante `esp_restart()` para restablecer el periférico de radio desde cero.
- Al recuperar la conexión, el contador de caídas se restablece a cero.

### 3.2 Vigilante del servidor de video
- Supervisa que la tarea de captura esté entregando fotogramas.
- Si hay un cliente conectado demandando video pero la antigüedad del cuadro excede los **5 segundos** o el servidor HTTP deja de responder peticiones durante 3 intentos consecutivos, fuerza un reinicio limpio del sistema.
- El algoritmo está diseñado con aritmética modular de enteros sin signo para ser totalmente inmune al desbordamiento del contador `millis()` (que ocurre cada ~49.7 días).

---

## 4. Configuración por defecto

Línea 107 de `camara_prueba.ino`:
```c
static const Config CONFIG_DEFECTO = { 20, PIXFORMAT_RGB565, FRAMESIZE_HQVGA, 50 };
```
- Frecuencia de reloj del sensor: **20 MHz**.
- Formato de píxel: **RGB565**.
- Resolución: **FRAMESIZE_HQVGA (240×176 píxeles)**.
- Calidad de compresión JPEG: **50** (balance óptimo de contraste y tasa de datos).
