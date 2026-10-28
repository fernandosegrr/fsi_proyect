# Aplicación Android (`app_android`)

Este directorio contiene el código fuente de la aplicación móvil de control, telemetría y navegación asistida del carro, desarrollada en **Kotlin** y **C++ (NDK/JNI)**.

---

## 1. Arquitectura de la app

- **Interfaz de usuario:** Basada en Vistas clásicas de Android (XML) para garantizar ligereza y compatibilidad en dispositivos como el Huawei P40 Pro (ELS-NX9).
- **Integración nativa (JNI):** El código en C de [`nucleo_c/`](file:///e:/Dev/fsi_proyecto/nucleo_c/README.md) se enlaza mediante CMake a través de `app/src/main/cpp/puente.c`, permitiendo ejecutar la máquina de estados del navegador y la cinemática a velocidad nativa sin copiar código.
- **Sesiones persistentes y compartidas:**
  - `SesionCarro.kt`: Mantiene la conexión WebSocket abierta con el ESP32 (`ws://192.168.4.1/ws`), encola comandos con prioridad para el botón de freno y calcula la latencia de ida y vuelta mediante pulsos de `PING`/`PONG`.
  - `SesionVideo.kt`: Administra el flujo de video MJPEG (`http://192.168.4.2/stream`). Como la cámara solo acepta un cliente simultáneo, esta sesión distribuye los cuadros entre pestañas y pantallas sin desconectar el stream.

---

## 2. Pantallas y navegación

1. **MainActivity:** Contenedor principal con barra de navegación inferior en cuatro pestañas:
   - **Inicio:** Estado de conexión de red, ping en tiempo real, lecturas del giroscopio y láser ToF, y versiones reportadas.
   - **Manual:** Botones de dirección con envío de comandos repetidos cada 250 ms, visor de video en vivo, botón de freno de emergencia (`EBRAKE`) y conmutador de perfil de velocidad (MANUAL / AUTO).
   - **Rutas:** Cuadrícula interactiva de 20 cm por celda donde el operador traza waypoints. Incluye simulación en pantalla antes del despacho.
   - **Más:** Menú de acceso a herramientas avanzadas y utilidades de calibración.
2. **Pantallas completas especializadas:**
   - `VisionActivity`: Transmisión de cámara en vivo con superposición del corredor trapezoidal de seguridad y detección de obstáculos mediante redes neuronales (`DetectorTflite.kt`).
   - `CalibracionActivity`: Asistente guiado de prueba de línea recta para ajustar el factor de balance entre motores y la constante de milisegundos por centímetro.
   - `CalibrarGiroActivity`: Asistente de calibración de giro a 90 grados apoyado en la telemetría del giroscopio MPU6050.
   - `SensoresActivity`: Panel detallado de telemetría de bajo nivel (`SENS`, `IMU?`, `TOF?`, `LOOP?`, `ARR?`).
   - `BenchmarkActivity`: Herramienta de medición de latencia e inferencia de modelos TensorFlow Lite en el teléfono.

---

## 3. Compilación y generación de la APK

La compilación requiere **Android Studio** o las herramientas de línea de comandos de Android (`gradlew`) junto con el **NDK**:

```powershell
# Compilación del APK en modo Release desde app_android/
cd app_android
.\gradlew assembleRelease
```

El APK de entrega oficial se encuentra precompilado y firmado en `dist/carro_fsi_0.4-074c9fd.apk` (14,852,386 bytes, SHA-256 `36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA`).

Para detalles sobre instalación en el dispositivo móvil y verificación de firmas, consulte [`docs/referencia/app.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/app.md) y [`docs/instalar_app_huawei.md`](file:///e:/Dev/fsi_proyecto/docs/instalar_app_huawei.md).
