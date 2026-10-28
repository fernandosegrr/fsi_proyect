# Recorrido de código: Máquina de navegación (`navegador.h` y `navegador.c`)

**Ubicación:** `nucleo_c/navegador.h` (626 líneas) y `nucleo_c/navegador.c` (3860 líneas)  
**Lenguaje:** C estándar (C99)  
**Propósito:** Gestionar la máquina de estados de navegación autónoma, la estimación de la pose cinemática y la toma de decisiones para esquivar obstáculos y reincorporarse a la ruta original.

---

## 1. Responsabilidad y principios de diseño

El navegador es el «cerebro lógico» del vehículo:
- No accede a pines físicos ni sockets de red; opera recibiendo entradas sensoriales periódicas y devolviendo el comando que el carro debe ejecutar en cada instante.
- Es determinista y predecible: funciona mediante una llamada periódica denominada *tick* (`navegador_tick`, típicamente cada 50 ms).
- **Regla estricta de seguridad:** En los estados `NAV_ESPERANDO`, `NAV_TERMINADO` y `NAV_ERROR`, el comando retornado es **invariablemente `NAV_CMD_STOP`**.

---

## 2. Tipos y estructuras clave

### 2.1 Estados finitos de navegación (`EstadoNav`)
Definidos en `nucleo_c/navegador.h:222-231`:

```c
typedef enum {
    NAV_ESPERANDO,           /* En reposo, esperando orden de comenzar */
    NAV_SIGUIENDO,           /* Recorriendo los segmentos de la ruta */
    NAV_DETENIDO_OBSTACULO,  /* Freno preventivo ante detección de obstáculo */
    NAV_ESQUIVANDO,          /* Desplazándose lateralmente 40 cm para librar la caja */
    NAV_REINCORPORANDO,      /* Maniobrando hacia un punto 30 cm por delante de la salida */
    NAV_TERMINADO,           /* Ruta completada exitosamente */
    NAV_ERROR,               /* Ruta abortada (bloqueo total o fallas críticas) */
    NAV_AYUDA                /* Requiere intervención manual del operador */
} EstadoNav;
```

### 2.2 Comandos de salida (`ComandoNav`)
Definidos en `nucleo_c/navegador.h:234-240`:

```c
typedef enum {
    NAV_CMD_STOP,
    NAV_CMD_FORWARD,
    NAV_CMD_REVERSE,
    NAV_CMD_LEFT,
    NAV_CMD_RIGHT
} ComandoNav;
```

### 2.3 Estructura principal `Navegador`
Contiene el contexto completo de ejecución en `nucleo_c/navegador.h:320-505`:
- `estado`: Estado actual de la máquina de estados.
- `pose_x_mm`, `pose_y_mm`, `rumbo_mrad`: Posición $(x, y)$ en milímetros y orientación estimada en milirradianes.
- `ruta`: Arreglo de waypoints y pasos calculados a partir de la cuadrícula de 20 cm.
- `paso_actual`: Índice del tramo de avance o giro que se está ejecutando.
- `cal`: Puntero o copia de los parámetros de calibración cinemática (`ms/cm`, `frenado_mm`, balance).
- `autoridad_laser`: Bandera que activa la parada prioritaria si el láser ToF reporta colisión inminente.
- `giro_fw_pedir`: Bandera que indica a la app que debe transmitir un comando `GIRAR:<grados>` al ESP32.

---

## 3. Funciones principales

### 3.1 Inicialización y carga
- `navegador_iniciar(Navegador *nav)` (`nucleo_c/navegador.h:512`): Inicializa todos los campos en cero, limpia la cola de diagnóstico y establece el estado en `NAV_ESPERANDO`.
- `navegador_cargar_ruta(Navegador *nav, const Punto *puntos, int n_puntos)` (`navegador.h:522`): Valida y convierte la lista de coordenadas métricas en pasos de movimiento lineal y pivotes angulares.
- `navegador_comenzar(Navegador *nav, uint32_t ahora_ms)` (`navegador.h:526`): Transiciona de `NAV_ESPERANDO` a `NAV_SIGUIENDO` fijando la marca de tiempo inicial.

### 3.2 El ciclo periódico: `navegador_tick`
`nucleo_c/navegador.h:535`:
```c
ComandoNav navegador_tick(Navegador *nav, uint32_t ahora_ms, const ResultadoObstaculos *obs);
```
Se ejecuta nominalmente cada 50 ms (`NAV_TICK_MS`). En cada ciclo:
1. Comprueba si se ha alcanzado el tiempo límite del paso actual o si el paso se ha completado.
2. Evalúa si el sensor ToF o la cámara detectan un obstáculo dentro del corredor de seguridad (`obs->obstaculo_confirmado`).
3. Si hay bloqueo al frente y el carro está en `NAV_SIGUIENDO`, ordena `NAV_CMD_STOP` y transiciona a `NAV_DETENIDO_OBSTACULO`.
4. Si el obstáculo persiste tras la confirmación temporal, calcula el lado con mayor margen despejado (izquierda o derecha) y conmuta a `NAV_ESQUIVANDO`.
5. Retorna el comando correspondiente (`NAV_CMD_FORWARD`, `NAV_CMD_LEFT`, `NAV_CMD_RIGHT` o `NAV_CMD_STOP`).

### 3.3 Integración sensorial y corrección J15
- `navegador_aporte_sens(...)` y `navegador_aporte_sens_imu(...)` (`navegador.h:585-587`): Reciben la telemetría periódica del ESP32. En la jornada J15 se modificó la integración de avance: en lugar de proyectar el desplazamiento usando el ángulo estático fijado al inicio de la ruta, cada tramo recto integra los rumbos reales instantáneos reportados por el giroscopio, corrigiendo el error lateral acumulado tras las esquivadas.
- `navegador_aporte_giro(Navegador *nav, int codigo, int real_x10_fw, int ms, ...)` (`navegador.h:584`): Se invoca cuando el firmware responde `GIRO_OK` tras un comando `GIRAR:<grados>`, actualizando la orientación interna con los grados reales medidos por el hardware.
- `navegador_aporte_tof_paro(Navegador *nav, int mm, uint32_t ahora_ms)` (`navegador.h:594`): Procesa la parada autónoma emitida por el sensor láser.

---

## 4. Conexión con el resto del proyecto

- **Hacia Android (JNI):** `app/src/main/cpp/puente.c` envuelve estas funciones y expone la clase Kotlin `NavegadorNativa.kt`.
- **Hacia la orquestación móvil:** `ModoSemiautonomo.kt` ejecuta un bucle de corrutina cada 50 ms llamando a `navegador_tick()` y transmitiendo el comando resultante hacia el ESP32 mediante `SesionCarro.kt`.
- **Hacia las pruebas unitarias en PC:** `nucleo_c/pruebas_navegador.c` y `nucleo_c/pruebas_reincorporacion_j15.c` evalúan directamente este archivo sin necesidad de emuladores.
