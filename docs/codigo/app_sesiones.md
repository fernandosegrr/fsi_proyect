# Recorrido de código: Sesiones y comunicaciones de la app (`SesionCarro`, `SesionVideo` y `Enlace`)

**Ubicación:** `app_android/app/src/main/java/com/fsi/carro/SesionCarro.kt`, `SesionVideo.kt` y `Enlace.kt`  
**Lenguaje:** Kotlin  
**Propósito:** Gestionar la persistencia de las conexiones de red (WebSocket hacia el ESP32 y stream HTTP/UDP hacia la cámara) como objetos singleton compartidos entre todas las actividades y fragmentos de la aplicación.

---

## 1. El reto de la persistencia de sesión en Android

En el ciclo de vida convencional de Android, al cambiar de pantalla (por ejemplo, pasar de `MainActivity` a `VisionActivity` o `CalibracionActivity`), la actividad de origen se pausa o destruye. Si la conexión de red estuviera atada al ciclo de vida de la vista:
- El WebSocket se cerraría, activando el watchdog de 1000 ms y frenando el carro.
- La conexión HTTP de la cámara se reiniciaría, provocando una pantalla negra de varios segundos.

Para evitar esto, `SesionCarro` y `SesionVideo` se implementan como **objetos singleton (`object`)** independientes de las pantallas.

---

## 2. `SesionCarro.kt`: Cliente WebSocket y telemetría

`app_android/app/src/main/java/com/fsi/carro/SesionCarro.kt`:
- **Instancia del socket:** Mantiene una conexión WebSocket abierta contra `ws://192.168.4.1/ws` utilizando la implementación reactiva `Enlace.kt`.
- **Despacho con prioridad:**
  - Los comandos normales (`FORWARD`, `REVERSE`, `LEFT`, `RIGHT`) se encolan con una tasa máxima de repetición de 250 ms.
  - El comando de freno de emergencia `EBRAKE` vacía la cola local y se transmite de inmediato.
- **Cálculo de latencia (Ping/Pong):**
  - Emite periódicamente el mensaje `PING` registrando la marca temporal de salida `t_envio = System.currentTimeMillis()`.
  - Al recibir `PONG`, calcula $\text{latencia} = t_{\text{llegada}} - t_{\text{envio}}$ y expone el promedio móvil hacia la interfaz.
- **Distribución de telemetría:** Al recibir una trama `SENS:<angulo>,<distancia>,<banderas>`, la parsea y notifica a los oyentes registrados (el panel de Inicio, la pantalla de sensores y el navegador autónomo).

---

## 3. `SesionVideo.kt`: Concentrador de flujo visual

`app_android/app/src/main/java/com/fsi/carro/SesionVideo.kt`:
- **Unicidad de cliente:** La placa de la cámara (ESP32-WROVER) solo tiene capacidad para atender **un único cliente de video a la vez**. `SesionVideo` mantiene esa conexión permanente contra `http://192.168.4.2/stream`.
- **Patrón Observador multiventana:**
  - Procesa el stream multipart continuo en un hilo secundario.
  - Al completar la decodificación de un fotograma JPEG, invoca la interfaz `Oyente`:
    ```kotlin
    fun alJpeg(bytes: ByteArray, largo: Int)
    fun alCrudo(bytes: ByteArray, largo: Int, ancho: Int, alto: Int, ...)
    fun alCeldas(paquete: PaqueteCeldas)
    ```
  - Si el usuario conmuta entre la pestaña `Manual` y la pantalla `VisionActivity`, la sesión simplemente suscribe la nueva vista y desuscribe la anterior, sin cerrar ni reiniciar la conexión con la cámara.

---

## 4. `Enlace.kt`: Adaptador de bajo nivel de WebSocket

`app_android/app/src/main/java/com/fsi/carro/Enlace.kt`:
- Envuelve el socket TCP subyacente y expone callbacks limpios hacia Kotlin: `alAbrir()`, `alMensaje(texto: String)` y `alCerrar()`.
- Gestiona la reconexión transparente si detecta desconexión fortuita y garantiza que las llamadas hacia el socket no bloqueen el hilo de renderizado de la interfaz gráfica (*UI Thread*).
