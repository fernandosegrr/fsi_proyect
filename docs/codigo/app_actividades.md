# Recorrido de código: Actividades y pantallas de la app (`MainActivity`, `VisionActivity` y utilidades)

**Ubicación:** `app_android/app/src/main/java/com/fsi/carro/MainActivity.kt`, `VisionActivity.kt`, `CalibracionActivity.kt`, `CalibrarGiroActivity.kt`, `SensoresActivity.kt`  
**Lenguaje:** Kotlin  
**Propósito:** Estructura de navegación de la interfaz de usuario, renderizado de video y asistentes guiados de calibración y diagnóstico.

---

## 1. `MainActivity.kt`: El contenedor principal

`MainActivity` es el punto de entrada de la aplicación y gestiona el menú de navegación inferior estructurado en cuatro pestañas principales:

1. **Pestaña Inicio (`InicioFragment`):**
   - Tarjeta de conexión rápida: campo de dirección IP (`192.168.4.1`) y botón de enlace.
   - Panel de salud de red: indicador de estado (Desconectado / Conectando / En línea), latencia de ping en tiempo real y versiones de software (`FW:453ed20...` y `APP:0.4-074c9fd`).
   - Resumen sensorial: lecturas vivas de rumbo angular y distancia del láser ToF.
2. **Pestaña Control Manual (`ManualFragment`):**
   - Interfaz táctil de teleoperación: cruceta direccional (Adelante, Atrás, Giro Izquierda y Giro Derecha).
   - Botón de parada de emergencia (`EBRAKE`): pintado en rojo prominente para detención instantánea.
   - Visor de video en directo: superficie gráfica optimizada que renderiza los cuadros MJPEG de `SesionVideo`.
   - Conmutador de perfil: botones para alternar entre perfil `MANUAL` (potencia 255) y perfil `AUTO` (potencia 210).
3. **Pestaña Rutas (`RutasFragment`):**
   - Lienzo táctil interactivo (`CuadriculaView` / `EditorRutaView`): grilla métrica de 20 cm donde el usuario traza waypoints con el dedo.
   - Herramientas de edición: limpiar ruta, deshacer último punto y cargar circuitos predefinidos.
   - Controles de despacho: botón **Simular** (anima la cinemática estimada en pantalla) y botón **Ejecutar** (inicia el modo semiautónomo asistido).
4. **Pestaña Más (`MasFragment`):**
   - Menú de accesos directos hacia las pantallas especializadas de pantalla completa, ajustes de red y documentación del proyecto.

---

## 2. `VisionActivity.kt`: Diagnóstico visual y superposiciones

Diseñada para evaluar la visión computacional en tiempo real sin interferencias:
- **Visor central:** Despliega el stream continuo de la cámara.
- **Capa vectorial de superposición (*Canvas Overlay*):**
  - Dibuja el trapecio del corredor de seguridad en color verde translúcido.
  - Pinta la máscara binaria de segmentación del piso destacando en color rojo las áreas que no coinciden con el suelo aprendido.
  - Dibuja los cuadros delimitadores (*bounding boxes*) de las detecciones de la red neuronal TensorFlow Lite con etiqueta de clase y porcentaje de certeza.
- **Herramientas de calibración óptica:**
  - Botón de muestreo cromático: captura el color promedio del área inferior para fijar la referencia del piso del aula.
  - Deslizadores en pantalla para ajustar la tolerancia cromática y la inclinación angular calculada del soporte.

---

## 3. Pantallas de calibración y metrología

### 3.1 `CalibracionActivity.kt` (Línea recta y avance)
- Asistente paso a paso que guía al operador para medir el avance del carro sobre una cinta métrica.
- Ejecuta una corrida corta (ej. 1000 ms) y una corrida larga (ej. 2500 ms).
- Solicita al usuario ingresar los centímetros reales medidos y calcula la constante `ms/cm` y el arrastre inercial `frenado_mm`.
- Permite ingresar el desvío lateral en centímetros y ajusta automáticamente el factor de balance izquierdo, enviando `BAL_AUTO:NNN` o `BAL_MANUAL:NNN` para guardarlo en la memoria del microcontrolador.

### 3.2 `CalibrarGiroActivity.kt` (Pivotes y rumbos)
- Asistente de calibración de rotación que ejecuta giros de 90° físicos hacia ambos lados.
- Compara el ángulo comandado contra la lectura del giroscopio MPU6050 y ajusta las constantes de milisegundos por grado para maniobras en bucle abierto.

---

## 4. `SensoresActivity.kt`: Telemetría de bajo nivel

Panel de control para supervisión de ingeniería:
- Visualizador numérico del ángulo acumulado de la IMU y su sesgo interno.
- Botones de acción directa: **Calibrar IMU** (`IMU_CAL`) y **Poner Cero** (`IMU_CERO`).
- Monitor de tiempo de ciclo del firmware: consulta el comando `LOOP?` e indica el peor tiempo de ejecución registrado en microsegundos.
- Interruptor de autoridad de parada del sensor ToF (`TOF_USO:<0|1>`).
