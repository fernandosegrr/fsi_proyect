# 9. Aplicación Android

Este capítulo describe la arquitectura de la aplicación móvil **`LIDIA`** (`APP:0.4-6d1b213`, paquete `com.example.carro_ilib`), su organización en cuatro pestañas principales y cinco actividades de pantalla completa, el diseño de las sesiones compartidas (`SesionCarro` y `SesionVideo`) y las mejoras operativas y de seguridad incorporadas en el cierre **J16**: corte del latido `FORWARD` ante `TOF_PARO`, supervisión activa de rutas (`ModoSemiautonomo`), aviso de calibración en `Rutas` y controles en vivo para `RUMBO` y escala del giroscopio en `SensoresActivity`.

## 9.1 Estructura de pantallas y sesiones compartidas

La interfaz de usuario se hospeda en `MainActivity`, que presenta una cabecera persistente con el estado de enlace, una barra inferior de navegación con **cuatro pestañas** para la operación cotidiana y **cinco actividades de pantalla completa** dedicadas a calibración, visión e instrumentación:

| Pantalla / Componente | Clase en Kotlin | Propósito dentro de la operación |
|---|---|---|
| **Pestaña 1 — `Inicio`** | `InicioFragment.kt` | Resumen del estado del enlace (`CarroFSI`), vista rápida del flujo de cámara, lecturas actuales de sensores (`SENS`), versión verificada y acceso al asistente. |
| **Pestaña 2 — `Manual`** | `ManualFragment.kt` | Cruceta de conducción manual con latido cada `250 ms`, conmutador de perfil (`MANUAL` / `AUTO`), botones de giro rápido (`±90°`), video en vivo y botón rojo permanente `PARO`. |
| **Pestaña 3 — `Rutas`** | `RutasFragment.kt` + `EjecutarRutaFragment.kt` | Editor interactivo sobre cuadrícula de `20 cm` (`EditorRutaView`), catálogo de rutas predefinidas (`RutasPredefinidas`), simulador en pantalla, validación de calibración (J16) y panel de ejecución semiautónoma. |
| **Pestaña 4 — `Más`** | `MasFragment.kt` + `AjustesFragment.kt` | Configuración de direcciones IP (`192.168.4.1` y `192.168.4.2`), modo de cámara (`JPEG`, `RAW`, `CELDAS`), acceso a las cinco actividades completas, ayuda (`AyudaFragment`) e información de compilación (`AcercaFragment`). |
| **Actividad — `Visión`** | `VisionActivity.kt` | Diagnóstico visual del modelo de piso, superposición de la rejilla `10×10` y del corredor de perspectiva, ajuste de umbrales cromáticos y botón `Aprender piso`. |
| **Actividad — `Calibración de avance`** | `CalibracionActivity.kt` | Procedimiento guiado para medir tiempo de arranque (`arranque_ms`), pendiente (`ms_por_cm`) y deslizamiento de frenado (`frenado_mm`). |
| **Actividad — `Calibrar giro`** | `CalibrarGiroActivity.kt` | Calibración de velocidad angular por tiempo, cuerda de pivote excéntrico (`pivote_mm`) y pruebas de giro cerrado `GIRO:<grados>`. |
| **Actividad — `Sensores`** | `SensoresActivity.kt` | Monitor en vivo de `MPU6050` y `VL53L0X`, calibración de sesgo `CAL_IMU`, interruptor de **control de rumbo en rectas (`RUMBO`)** y asistente de 3 pasos para la **escala del giroscopio (`IMU_ESCALA`)** (J16). |
| **Actividad — `Benchmark`** | `BenchmarkActivity.kt` | Medición instrumentada de tiempos de inferencia del detector `TFLite` y del clasificador cromático sobre imágenes de prueba. |

[FIGURA:pantallas|Mapa de pantallas de la aplicación Android: cuatro pestañas en MainActivity, cinco actividades de pantalla completa y sesiones compartidas.]

### Por qué `SesionCarro` y `SesionVideo` son objetos únicos compartidos

Si cada pestaña o actividad abriera su propio `WebSocket` hacia `192.168.4.1` y su propia conexión `HTTP` hacia `192.168.4.2`, cambiar de la pestaña `Inicio` a `Rutas` o abrir `SensoresActivity` cerraría y reabriría los sockets, interrumpiendo la telemetría `SENS` y compitiendo por el cliente único de la cámara. Para evitarlo, la aplicación utiliza el módulo en `C99` `sesion_compartida.c` envuelto por dos *singletons* en Kotlin (`SesionCarro.kt` y `SesionVideo.kt`):

- Los fragmentos y actividades se registran como observadores al entrar en primer plano y se desregistran al salir, manteniendo una cuenta de referencias con histéresis temporal.
- Así, la transición entre pantallas conserva abiertos tanto el canal `ws://192.168.4.1/ws` como el flujo de video activo sin perder un solo paquete.

## 9.2 Interpretación de los tres estados de conexión en las capturas

En la cabecera y tarjetas de estado de la interfaz, la aplicación distingue tres situaciones operativas claramente diferenciadas para que el usuario nunca confunda una pantalla cargada en el teléfono con un vehículo realmente enlazado:

1. **Conectado (`En línea`):** el teléfono está asociado al punto de acceso `CarroFSI`, el `WebSocket` en `192.168.4.1/ws` responde `PONG` y se reciben tramas `CAPS` y `SENS`.
2. **Reconectando (`Fuera de CarroFSI` o caída temporal):** el usuario abrió la pantalla (por ejemplo, el editor de `Rutas` para planificar o simular en seco), pero el teléfono está conectado a otra red doméstica o el carro está apagado; `Enlace.kt` reintenta la conexión periódicamente y muestra el aviso de reconexión sin bloquear la edición ni el simulador local.
3. **Sin conexión activa (configuración en `Ajustes`):** pantalla de inspección o cambio de parámetros (`IP`, modo de cámara, preferencias) cuando aún no se ha iniciado sesión con el hardware.

[FIGURA:lamina_app|Tres capturas reales de la aplicación Android que ilustran los tres estados de enlace: (a) conectado en Inicio, (b) reconectando fuera de la red en Rutas y (c) sin conexión en Ajustes.]

## 9.3 Supervisión de seguridad y controles J16 en la aplicación

El cierre **J16** reforzó cuatro módulos críticos de la aplicación Android para cerrar brechas entre la interfaz, el navegador nativo y el firmware:

### 1. Corte del latido `FORWARD` en conducción manual (`ControlLogica.kt`)

En modo manual, mientras el usuario mantiene el dedo sobre el botón `Adelante`, `ControlLogica` envía `FORWARD` cada `250 ms`. Si el carro frena localmente por el sensor láser y emite `TOF_PARO:<mm>`, `ControlLogica` activa ahora la bandera interna `bloqueadoTof = true` y **suspende de inmediato el reenvío periódico de `FORWARD`** (mostrando la alerta de paro frontal), impidiendo que el latido del botón insista contra el obstáculo; la bandera se limpia en cuanto el usuario suelta el botón o comanda `STOP`, `BACKWARD`, `LEFT` o `RIGHT`.

### 2. Vigilancia triple en rutas semiautónomas (`ModoSemiautonomo.kt`)

Durante la ejecución de una ruta, `ModoSemiautonomo.kt` ejecuta un ciclo de supervisión cada `50 ms` que alimenta al núcleo en `C99` y vigila tres condiciones de seguridad:

- **Vigilancia de silencio de telemetría (`SILENCIO_SENS_MS = 1,500 ms`):** al recibir la primera trama `SENS`, registra `ultimoSensMs`. Si el navegador está en un estado de movimiento activo y transcurren más de `1,500 ms` sin recibir una nueva línea `SENS`, aborta el avance, envía `STOP` al carro, pasa la sesión a `AYUDA` y notifica `"Sin datos de sensores (1.5 s); carro detenido"`.
- **Reacción ante *watchdog* del firmware (`WD:<n>`):** si llega el mensaje `WD:<n>`, `ModoSemiautonomo` detiene inmediatamente el ciclo, envía `STOP` de confirmación y muestra `"Watchdog del carro activado: conexión interrumpida"`.
- **Respaldo ante `TOF_PARO` no absorbido:** cuando llega `TOF_PARO:<mm>`, el método `onTofParo` inyecta la distancia en `NavegadorNativa.aportarTof`. Si por cualquier razón el estado del navegador no cambia a `DETENIDO_OBSTACULO`, `ESQUIVANDO`, `AYUDA`, `TERMINADO` o `ERROR` en ese mismo instante, `ModoSemiautonomo` fuerza `STOP` y transiciona a `AYUDA`.

### 3. Validación de calibración al abrir `Rutas` (`CalibracionGuardada.kt`)

Para evitar que el operador ejecute una ruta en el piso con constantes por defecto sin haber calibrado el vehículo (o con valores corruptos en preferencias), `CalibracionGuardada.kt` incorpora el método `estadoResumen()`. Al abrir `RutasFragment`, la aplicación verifica que los parámetros guardados estén dentro de rangos físicos seguros (`ms_por_cm` en `[10, 200]`, `arranque_ms` en `[0, 1000]`, `frenado_mm` en `[0, 500]`, `ms_por_grado` en `[1, 50]`); si aún no se ha guardado una calibración medida o algún valor es inválido, muestra una advertencia visible en la parte superior del editor recomendando acudir primero a `Calibración` o activar el control `RUMBO`.

### 4. Controles de `RUMBO` y calibración de `IMU_ESCALA` en `SensoresActivity.kt`

La pantalla `SensoresActivity` añadió en el cierre **J16** dos tarjetas interactivas:

- **Control de rumbo en rectas:** muestra un interruptor (`Switch`) sincronizado con la capacidad `RUMBO` del carro (`RUMBO:ON` / `RUMBO:OFF`) y un botón de consulta (`RUMBO?`) que presenta en vivo la referencia $\theta_{\text{ref}}$, el error en grados, el ajuste `PWM` en milésimas y el sesgo aprendido (`aprendido_mil`).
- **Calibración guiada de la escala del giroscopio en 3 pasos (`IMU_ESCALA`):**
  1. **Paso 1:** el usuario alinea el carro con una marca recta en el piso (por ejemplo, la junta de una baldosa) y pulsa `1. Poner a 0°` (envía `CAL_IMU`).
  2. **Paso 2:** gira manualmente el carro **una vuelta completa exacta (`360°`)** hasta volver a coincidir con la misma marca en el piso.
  3. **Paso 3:** pulsa `3. Calcular y guardar`, que lee el ángulo reportado por `SENS`, invoca `CalGiroNativa.escala` (`calgiro_escala`), suma el ajuste a la escala previa dentro de `[-50000, +50000] ppm` y envía `IMU_ESCALA:<ppm>` para persistirlo en `NVS` (además de ofrecer un botón `Restablecer a 0 ppm`).

Dónde está en el código: `MainActivity.kt`, `SesionCarro.kt`, `SesionVideo.kt`, `ControlLogica.kt`, `ModoSemiautonomo.kt`, `CalibracionGuardada.kt`, `SensoresActivity.kt`, `RutasFragment.kt`, `VisionActivity.kt`.
