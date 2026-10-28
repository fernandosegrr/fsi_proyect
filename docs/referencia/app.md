# Referencia de la aplicación Android

**Estado:** documento de referencia · **Fecha:** 27 de octubre de 2026 · **Rama:** `docs/documentacion-completa`
**Código documentado:** `app_android/` en `deae8b6`. **Fuentes:** `AndroidManifest.xml`, `app/build.gradle.kts`, `MainActivity.kt`, `ControlLogica.kt`, `Enlace.kt`, `SesionCarro.kt`, `SesionVideo.kt`, `ModoSemiautonomo.kt`, `CamaraObstaculos.kt`, `DetectorTflite.kt`, `app/src/main/cpp/CMakeLists.txt`, `docs/entrega.md` y `docs/pruebas_control.md`.

## 1. En una frase

La app es el **mando y los ojos** del carro: se conecta por WiFi al WebSocket, manda órdenes, muestra la cámara, dibuja rutas, calibra el movimiento y ejecuta el modo semiautónomo.

Está hecha con vistas clásicas de Android (Kotlin, sin Compose), funciona en un Huawei sin servicios de Google y guarda el trabajo pesado (visión, navegación, calibración) en el código C del núcleo, que se compila dentro de la app por JNI. Así el mismo código que se prueba en el PC es el que corre en el teléfono.

## 2. Cómo está organizada

- **Una actividad principal** (`MainActivity`) con barra inferior de cuatro pestañas: **Inicio**, **Manual**, **Rutas** y **Más**. Cada pestaña es un `Fragment`. Para quien no viene de Android: una **actividad** es una pantalla completa, y un **Fragment** es una parte de pantalla que se puede cambiar sin cerrar la actividad.
- **Pantallas completas aparte:** `VisionActivity` (cámara y detector), `CalibracionActivity`, `CalibrarGiroActivity`, `SensoresActivity` y `BenchmarkActivity`.
- **Una sola conexión con el carro** para toda la app (`SesionCarro`), compartida por las pantallas.
- **Un solo video de la cámara del carro** (`SesionVideo`), compartido por Inicio, Manual, Visión y Ejecutar ruta.
- **Un solo ciclo semiautónomo** (`ModoSemiautonomo`) que llama al navegador en C cada 50 ms.

La conexión y el video se abren cuando entra la primera pantalla que los usa y se cierran 1,5 s después de que sale la última. Esa espera evita cortar el video al cambiar de pestaña o al girar el teléfono (`SesionCarro.kt:4-7`; `SesionVideo.kt:5-11`; la lógica está en `nucleo_c/sesion_compartida.c`).

## 3. Las pantallas

| Pantalla | Para qué sirve | Archivo principal |
|---|---|---|
| **Inicio** | Estado del carro, conexión, sensores, batería, accesos rápidos | `InicioFragment.kt` |
| **Manual** | Botones de movimiento, perfil, video, giro por ángulo, PARO | `ManualFragment.kt` |
| **Rutas** | Dibujar/abrir una ruta, verla en la cuadrícula, ejecutarla o simularla | `RutasFragment.kt`, `EjecutarRutaFragment.kt`, `EditorRutaView.kt` |
| **Más** | Ajustes, calibración, sensores, visión, ayuda, acerca de | `MasFragment.kt` |
| **Visión** | Cámara del teléfono + detector de obstáculos con umbrales | `VisionActivity.kt` |
| **Calibración** | Pruebas guiadas de arranque, avance, frenado y giro | `CalibracionActivity.kt` |
| **Calibrar giro** | Ocho giros medidos con el giroscopio | `CalibrarGiroActivity.kt` |
| **Sensores** | Estado de MPU6050 y VL53L0X, interruptores y prueba de giro | `SensoresActivity.kt` |
| **Benchmark** | Compara CPU/GPU/NNAPI del detector de objetos | `BenchmarkActivity.kt` |

En **Manual** y **Rutas** la app mantiene la pantalla encendida mientras el carro se mueve (`MainActivity.kt:12`, `336`). El botón de **PARO** de la cabecera hace `STOP` y cancela la ruta (`SesionCarro.pararTodo`, `SesionCarro.kt:97-104`). Si hay una ruta en marcha y se intenta cambiar de pestaña, la app pide confirmación y la detiene (`MainActivity.kt:8`, `193-203`).

## 4. La conexión con el carro

`ControlLogica` es el corazón del enlace (`ControlLogica.kt`):

- Abre un WebSocket a la dirección configurada (por defecto `ws://192.168.4.1/ws`).
- Manda **`PING` cada 800 ms** para mantener viva la conexión y medir la latencia con el `PONG` (`PING_MS`, línea 43).
- Repite el comando de movimiento cada 250 ms mientras el botón está apretado. El firmware corta a los 1000 ms sin comandos reales; este heartbeat es el que mantiene el movimiento.
- Al soltar, manda `STOP` de inmediato.
- Cada 4 `PING` (3,2 s) vuelve a pedir el perfil deseado, por si el carro volvió a AUTO por el watchdog (`ControlLogica.kt:194-206`).
- Espera `ACK_MANUAL`/`ACK_AUTO` antes de dar por confirmado el perfil; las rutas no arrancan sin esa confirmación (`ControlLogica.kt:16-18`, `301-327`).
- Interpreta `SENS`, `GIRO_OK/TIMEOUT/ERROR`, `TOF_PARO`, `CAPS`, `IMU`, `TOF`, `ANT`, `ARR` y los ACK de balance.

`EnlaceOkHttp` implementa el WebSocket con OkHttp (`Enlace.kt`). Usa `readTimeout = 0` porque el carro real puede estar callado sin que la conexión esté muerta (`Enlace.kt:43-48`). Cada conexión lleva un número y los avisos de una conexión vieja se descartan (`Enlace.kt:94-101`). Si el teléfono pierde la red, la app reintenta cada 2 s (`SES_REINTENTO_MS`).

## 5. El modo semiautónomo

`ModoSemiautonomo` (`ModoSemiautonomo.kt`) es el puente entre la app y `navegador.c`:

1. Carga la ruta y la calibración en el navegador nativo.
2. Cada **50 ms** arma el resultado de obstáculos, llama a `NavegadorNativa.tick()` y manda el comando si cambió.
3. Los obstáculos pueden venir de la cámara del teléfono (`CamaraObstaculos`) o de interruptores simulados para probar sin cámara.
4. Si no llegan cuadros nuevos en **1000 ms**, cancela la ruta: sin ojos no se avanza (`TIMEOUT_CAMARA_MS`).
5. Antes de arrancar exige que el carro confirme el perfil AUTO (`PERFIL_ESPERA_MS`).
6. Si se pierde la conexión, cancela la ruta.

La app **no decide** cuándo girar ni cuánto avanzar: eso lo hace el C. Kotlin solo alimenta el navegador, ejecuta el comando y dibuja el estado. Esa separación permite probar la misma lógica en el PC.

## 6. Visión: cámara del teléfono y detector

Hay dos fuentes de imagen:

- **Cámara del teléfono** (`CamaraObstaculos.kt`): usa Camera2, elige el tamaño 4:3 más pequeño de al menos 320×240, convierte cada cuadro a RGB y llama al detector en C. Procesa en un hilo aparte y descarta cuadros viejos si llega más rápido de lo que puede.
- **Cámara del carro** (`VideoCarro.kt` + `SesionVideo.kt`): recibe JPEG, video crudo o el modo CELDAS por HTTP/UDP desde la Freenove.

El **detector de color** (`ObstaculosNativa` → `obstaculos.c`) divide la imagen en celdas de 10×10 y marca lo que no se parece al piso. El **modelo de objetos** (`DetectorTflite.kt`) usa EfficientDet-Lite0 en TensorFlow Lite, con CPU (el procesador normal), GPU (el chip gráfico) o NNAPI (la API de Android para usar los aceleradores del teléfono). La **fusión** (`FusionNativa` → `fusion.c`) combina ambos: el color manda en seguridad y el modelo solo puede subir la severidad, nunca bajarla (`fusion.h:1-20`).

El modelo está en `app/src/main/assets/efficientdet_lite0.tflite` (4.5 MB) y se puede reemplazar por otro archivo en la carpeta privada de la app (`DetectorTflite.kt:49-50`). Los resultados del benchmark en el Huawei están en `docs/benchmark_ELS-NX9.md`.

## 7. El puente JNI

`app/src/main/cpp/puente.c` es un archivo grande (≈143 KB) que traduce entre Kotlin y C. Es lo que se llama **JNI** (Java Native Interface): el mecanismo de Android para llamar código nativo escrito en C. No contiene la lógica: convierte arreglos, llama a las funciones del núcleo y devuelve resultados.

Ejemplos de la correspondencia:

| Kotlin | C (`puente.c`) | Núcleo |
|---|---|---|
| `TrayectoriaNativa.calcular` | `Java_com_fsi_carro_TrayectoriaNativa_calcular` | `trayectoria_calcular_desde_cal` |
| `ObstaculosNativa.analizarConfig` | `..._ObstaculosNativa_analizarConfig` | `obstaculos_analizar_config` |
| `NavegadorNativa.tick` | `..._NavegadorNativa_tick` | `navegador_tick` |
| `FusionNativa.decidir` | `..._FusionNativa_decidir` | `fusion_decidir` |
| `SesionNativa.entrar` | `..._SesionNativa_entrar` | `sesion_compartida_entrar` |

El `CMakeLists.txt` compila `puente.c` junto con los `.c` de `nucleo_c/`, **sin copiarlos** (`CMakeLists.txt:22-46`). El resultado es `libcarro_nativo.so`. Solo se compila para `arm64-v8a` para no inflar el APK (`build.gradle.kts:50-52`).

## 8. Compilar e instalar

Requisitos: Android Studio con SDK, NDK 30.0.16248370, CMake 4.1.2 y Java 17 (`build.gradle.kts:35-37`, `73-76`).

```powershell
cd app_android
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
.\gradlew.bat assembleDebug
```

El APK sale en `app_android/app/build/outputs/apk/debug/`. La versión se genera sola con el hash de git y el nombre `0.4` (`build.gradle.kts:15-31`): la app muestra `APP:0.4-<hash>`. La versión publicada en J15 es **APP:0.4-074c9fd**, `versionCode` 3, 14 852 386 bytes, SHA-256 `36CE38F2...9513BA` (`docs/entrega.md` §1).

Instalación (no se ejecutó en el cierre documental):

```powershell
adb install -r dist\carro_fsi_0.4-074c9fd.apk
```

La app pide **INTERNET**, **ACCESS_NETWORK_STATE**, **VIBRATE** y **CAMERA**; no usa servicios de Google Play (`AndroidManifest.xml:9-25`). `usesCleartextTraffic` está activo porque el WebSocket es `ws://` sin cifrar (`AndroidManifest.xml:30-31`).

## 9. Lo que se probó y lo que no

| Afirmación | Estado | Evidencia |
|---|---|---|
| La app compila y el APK se instala en el Huawei | VERIFICADO en J15 | `docs/entrega.md` §3 |
| La APK publicada coincide con el hash | VERIFICADO | `docs/entrega.md` §1-2 |
| El control por WebSocket responde y el watchdog frena | VERIFICADO en J13/J14; probado también con servidor falso | `docs/pruebas_control.md`, `docs/reporte_J14.md` |
| El detector de obstáculos funciona con la cámara del teléfono | VERIFICADO en pruebas de visión | `docs/vision_huawei.md`, `docs/obstaculos.md` |
| El modo semiautónomo completo esquiva en el piso real | PARCIAL: B4-B6 físicas; J15 NO MEDIDO | `docs/reporte_J14.md`, `docs/reporte_J15.md` |
| El benchmark del modelo en el Huawei | VERIFICADO con las cifras de `docs/benchmark_ELS-NX9.md` | Datos en `docs/datos_benchmark/` |
| La app funciona en otros teléfonos | NO MEDIDO | Solo se probó el Huawei ELS-NX9 |

## 10. Límites y cuidados

- **Un solo cliente WebSocket:** si otra app o el navegador se conecta, el firmware cierra el anterior. No usar la página web y la app a la vez.
- **Un solo cliente de video:** la cámara atiende a uno; la app lo comparte entre pantallas, pero no con otro dispositivo.
- **El video no es imprescindible para manejar:** el control va por otro WebSocket; si el video falla, el botón de PARO sigue funcionando.
- **La cámara del teléfono no es la del carro:** en `VisionActivity` se usa la trasera del teléfono; en el modo semiautónomo la fuente puede ser la del teléfono o la del carro, según la pantalla.
- **La app no guarda la contraseña del WiFi del carro**; la red se elige en Ajustes de Android.
- **No se deben borrar los datos de la app** durante las pruebas: ahí viven la calibración y los balances (`docs/entrega.md` §3).
- **El pin de la app** y las direcciones de red se configuran en Ajustes; no se documentan valores reales en este repositorio público.

## 11. Para seguir leyendo

- `docs/codigo/app_android.md`: recorrido por archivo y función.
- `docs/protocolo.md`: mensajes y ACK que usa la app.
- `docs/operacion.md`: cómo conectar y manejar el carro.
- `docs/calibracion_guia.md`: cómo se calibra desde la app.
- `docs/pruebas_control.md`: pruebas del control y del servidor falso.
- `docs/instalar_app_huawei.md`: instalación en el teléfono del proyecto.
