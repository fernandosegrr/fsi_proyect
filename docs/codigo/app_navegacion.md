# Recorrido de código: Navegación autónoma en Android (`ModoSemiautonomo` y `NavegadorNativa`)

**Ubicación:** `app_android/app/src/main/java/com/fsi/carro/ModoSemiautonomo.kt` (515 líneas) y `NavegadorNativa.kt` (160 líneas)  
**Lenguaje:** Kotlin y corrutinas de Kotlin  
**Propósito:** Orquestar la ejecución de rutas en segundo plano, consumir la máquina de estados nativa en C y sincronizar la percepción visual con los comandos físicos hacia el carro.

---

## 1. `NavegadorNativa.kt`: El envoltorio JNI

`app_android/app/src/main/java/com/fsi/carro/NavegadorNativa.kt`:
Declara los métodos nativos (`external fun`) vinculados a `puente.c` y expone métodos de alto nivel amigables para Kotlin:

```kotlin
object NavegadorNativa {
    external fun iniciarNative(): Long
    external fun cargarRutaNative(ptr: Long, xs: FloatArray, ys: FloatArray, n: Int): Int
    external fun comenzarNative(ptr: Long, ahoraMs: Long): Int
    external fun tickNative(ptr: Long, ahoraMs: Long, obstaculoConfirmado: Boolean, ...): Int
    external fun aporteSensNative(ptr: Long, mm: Int, tofVivo: Int, tofValida: Int, anguloX10: Int, ...): Unit
    external fun aporteGiroNative(ptr: Long, codigo: Int, realX10Fw: Int, ms: Int, ...): Unit
    external fun destruirNative(ptr: Long): Unit
}
```

Mantiene el ciclo de vida seguro del puntero nativo de 64 bits (`Long`) asociándolo a cada sesión de ejecución.

---

## 2. `ModoSemiautonomo.kt`: El bucle de control en tiempo real

Es la clase controladora encargada de materializar el avance asistido:

### 2.1 El bucle de *tick* (Corrutina periódica a 50 ms)
- Al pulsar «Ejecutar Ruta», `ModoSemiautonomo` lanza un trabajo en segundo plano utilizando `CoroutineScope(Dispatchers.Default)`:
  ```kotlin
  while (activo && !terminado) {
      val ahoraMs = System.currentTimeMillis()
      val obs = camaraObstaculos.obtenerResultadoActual()
      val cmdCodigo = NavegadorNativa.tickNative(punteroNav, ahoraMs, obs.confirmado, ...)
      despacharComando(cmdCodigo)
      delay(NAV_TICK_MS) // 50 ms nominales
  }
  ```

### 2.2 Coordinación de giros angulares por hardware
Cuando la máquina nativa llega a un cambio de dirección:
1. `navegador_tomar_giro_fw` reporta que se requiere un giro por ángulo (ej. 90° a la izquierda).
2. `ModoSemiautonomo` suspende temporalmente el envío de comandos `FORWARD`.
3. Transmite el comando de texto `GIRAR:90` hacia el ESP32 a través de `SesionCarro`.
4. El carro ejecuta la maniobra controlada por el giroscopio MPU6050.
5. Al recibir la respuesta `GIRO_OK` o `GIRO_TIMEOUT` desde el WebSocket, el módulo reporta los grados reales medidos hacia `NavegadorNativa.aporteGiroNative()`.
6. La máquina de estados actualiza su orientación interna y reanuda el avance sobre el nuevo rumbo.

### 2.3 Manejo de la intervención humana (`NAV_AYUDA`)
Si el carro detecta que tanto el lado izquierdo como el derecho están bloqueados por obstáculos insalvables y se agotaron los reintentos:
- La máquina nativa entra en estado `NAV_AYUDA`.
- `ModoSemiautonomo` emite una alarma sonora y despliega en la app un banner de advertencia: *«El vehículo no puede continuar de forma autónoma. Tome el control manual»*.
- Los botones de la cruceta permiten al operador maniobrar el carro libremente.
- Al presionar **Continuar**, el navegador reincorpora el vehículo a la ruta y reanuda la marcha autónoma.
