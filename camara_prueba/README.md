# Firmware de la cámara (`camara_prueba`)

> **Nota sobre el nombre del directorio:** En los requerimientos generales del proyecto este componente se denomina a veces `firmware_camara`. En el repositorio oficial se ubica en el directorio `camara_prueba/`. Ambos nombres hacen referencia exactamente al mismo módulo.

Este directorio contiene el software para la segunda placa del vehículo: una **Freenove ESP32-WROVER v3.0** dotada de sensor de visión **GalaxyCore GC0308** y 4 MB de memoria externa PSRAM.

---

## 1. Características principales

- **Conexión a la red del carro:** Opera como estación cliente (STA) asociándose a la red WiFi generada por el DevKit, configurada con IP fija `192.168.4.2` (fuera del rango de DHCP de `.10` a `.20`).
- **Arquitectura de compresión por software:** Debido a que el sensor GC0308 no incluye compresor JPEG en hardware, la placa captura cuadros en formato crudo (RGB565) y los comprime por software utilizando la función `frame2jpg` aprovechando los 4 MB de PSRAM.
- **Resolución vigente:** La configuración por defecto es **HQVGA (240×176 píxeles)** a 20 MHz con calidad 50, proporcionando un equilibrio medido de ~12 fps y ~137 ms de edad de cuadro.
- **Tres modalidades de transmisión:**
  1. **JPEG / MJPEG (`/stream` y `/capture`):** Video continuo vía HTTP multipart para visualización fluida en la app o navegador.
  2. **RAW (`/raw`):** Transmisión de cuadro completo RGB565 sin pérdida para algoritmos que requieren colorimetría exacta.
  3. **CELDAS (UDP puerto 4210):** Agrupa la imagen en celdas de 10×10 píxeles, promediando los canales RGB. Con HQVGA genera 24×17 celdas (1244 bytes por paquete `CEL1`), permitiendo detectar obstáculos en tiempo real sin saturar el procesador con compresión JPEG.
- **Autonomía y recuperación ante fallos:** El módulo `recuperacion.c` implementa un vigilante que detecta pérdida de enlace WiFi, falta de fluidez en la captura o atascos en el servidor HTTP, forzando reinicios limpios del sistema si la anomalía persiste más de 60 segundos.

---

## 2. Mapa de archivos

| Archivo | Lenguaje | Descripción |
|---|---|---|
| `camara_prueba.ino` | C++ / Arduino | Inicialización de cámara, servidor HTTP asíncrono, transmisión MJPEG/RAW, socket UDP de celdas y bucle de despacho. |
| `recuperacion.h` / `recuperacion.c` | C estándar | Lógica de supervisión, política de reintentos exponenciales y reinicio por temporizador. Probable en PC mediante `pruebas_firmware/pruebas_camara.c`. |
| `secretos.ejemplo.h` | C | Plantilla para definir credenciales de la red WiFi a la que se enlaza la cámara. |

---

## 3. Compilación y carga

Requiere `arduino-cli` con el core `esp32:esp32` configurado para la placa `esp32:esp32:esp32wrover` o modelo compatible con PSRAM habilitada.

```powershell
# Compilación
arduino-cli compile --fqbn esp32:esp32:esp32wrover camara_prueba

# Carga por puerto serie USB
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32wrover camara_prueba
```

Para consultar la referencia técnica exhaustiva de este componente, véase [`docs/referencia/camara.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/camara.md).
