# Recorrido de código: Puente JNI (`puente.c`)

**Ubicación:** `app_android/app/src/main/cpp/puente.c` (3,644 líneas)
**Lenguaje:** C con la API JNI de Android (`<jni.h>`)
**Propósito:** adaptador entre Kotlin y los módulos en C de [`nucleo_c/`](../../nucleo_c/README.md). La versión anterior de este documento describía otra API (paquetes `navegacion`, `trayectoria`, punteros `jlong`, `malloc`); lo que sigue sale del código actual. Si algo no coincide, manda el código.

---

## 1. Por qué existe

Los cálculos de navegación, visión y calibración viven en C puro dentro de `nucleo_c/`, donde se prueban en la PC. `puente.c` permite compilar esos mismos archivos para Android sin copiarlos y convierte los tipos de Java (arreglos, enteros, cadenas) a las estructuras de C y de vuelta. Kotlin no interpreta nada: casi todo regresa como enteros o como texto listo para mostrar.

---

## 2. Convenciones

- Cada función exportada sigue la regla de JNI `Java_<paquete>_<clase>_<método>`. Todas son del paquete `com.fsi.carro` (por ejemplo `Java_com_fsi_carro_NavegadorNativa_tick`).
- **No hay `malloc`** (lo dice la guía de lectura del propio archivo): todo usa arreglos de tamaño fijo.
- **El navegador es una sola instancia**: `static Navegador navegador_global`, que usa solo el hilo principal de la app. Por eso las funciones de `NavegadorNativa` no reciben un puntero.
- La parte de visión comparte configuración entre hilos y la protege con un candado (`candado_vision`, `pthread_mutex_t`).
- Los arreglos de píxeles o de celdas se leen con `GetPrimitiveArrayCritical` y se liberan con `JNI_ABORT`, porque no se modifican.

---

## 3. Funciones exportadas: 145 en 17 clases

Cuenta de funciones `Java_` por clase en `puente.c`:

- `NavegadorNativa` (36): maneja el navegador del modo semiautónomo (`navegador.c`): `cargarRuta`, `comenzar`, `cancelar`, `tick`, `estado`, `aporteVisual`, `aporteSens`, `aporteImu`, `aporteTofParo`, `aporteModelo`, `ponerCalibracion`, `ponerCeldas`, `ponerCorredor`, `ponerRumboFw`, `tomarRumboFw`, `tomarGiroFw`, `cajaNegra`, `textoEstado`, `textoError`, `textoComando`, entre otras.
- `CalibracionNativa` (28) y `CalGiroNativa` (8): calibración del carro y del giro (`calibracion.c`, `calgiro.c`).
- `RutaEditorNativa` (19): editor de rutas (`ruta_editor.c`).
- `ObstaculosNativa` (13): detector de obstáculos por color de piso (`obstaculos.c`).
- `SesionNativa` (8): sesión compartida de conexión y video (`sesion_compartida.c`).
- `SimRutaNativa` (7): simulación de rutas sin el carro (`sim_ruta.c`).
- `PruebaObstaculoNativa` (5), `VideoStatsNativa` (3), `LatenciaVideoNativa` (3): cuentas de la prueba de obstáculos, estadística del video y comparación de latencia.
- `FusionNativa` (4), `CualquieraNativa` (3), `FlujoNativa` (2), `MovimientoVisualNativa` (2): fusión de color y modelo, modo «cualquier objeto», flujo del piso y giro entre cuadros.
- `TrayectoriaNativa` (2): `calcular` recibe el arreglo plano `[columna0, fila0, columna1, fila1, ...]` y devuelve un **texto** con la lista de pasos o un mensaje de error.
- `PaqueteCeldasNativa` (1) y `CorredorNativa` (1): lectura del paquete `CEL1` y corredor de trayectoria.

---

## 4. Conexión con el sistema

- **Construcción:** el NDK compila `puente.c` con `app_android/app/src/main/cpp/CMakeLists.txt`, que crea la biblioteca `carro_nativo`.
- **Fuentes enlazadas** (de `nucleo_c/`, por ruta, sin copiar): `trayectoria.c`, `obstaculos.c`, `mascara_vision.c`, `navegador.c`, `calibracion.c`, `calgiro.c`, `fusion.c`, `movimiento_visual.c`, `video_stats.c`, `prueba_obstaculo.c`, `corredor.c`, `cualquiera.c`, `movimiento_piso.c`, `paquete_celdas.c`, `sesion_compartida.c`, `latencia_video.c`, `ruta_editor.c`, `sim_ruta.c`, `simulador_navegador.c` y `vision_tele.c`.
