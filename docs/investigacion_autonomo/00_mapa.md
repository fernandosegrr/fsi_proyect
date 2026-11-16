# 00 — Mapa del modo autónomo: del sensor al motor

Rama `feat/autonomo-v3` (desde `feat/autonomo-v2` = `33bdb77`), 16-nov-2026. **Todo valor marcado SUPUESTO o SIMULADO no se midió en el carro.** Este mapa es el índice de lo que la Fase 1 (archivos `01_` a `11_`) verifica contra la literatura y la Fase 2 (`90_hallazgos.md`) contra el código.

## 1. Flujo de datos y de decisiones

```
 ┌──────────────── CARRO (ESP32 DevKit, 192.168.4.1) ───────────────┐      ┌────── CÁMARA (WROVER, 192.168.4.2) ──────┐
 │ VL53L0X ─I2C 400 kHz─► tof.c  (mediana de 3, "libre"=8190/65535/│      │ GC0308 HQVGA 240×176 ─► JPEG (MJPEG)      │
 │                         >1200, racha de TOF_PARO_LECTURAS=2)    │      │ + paquetes UDP de celdas de color (1244 B)│
 │ MPU6050 ─I2C──► imu.c (±500 °/s, DLPF 44 Hz, 100 Hz, sesgo,     │      │ /stats (cam-v2: medias RGB, quemadas)     │
 │                        ZUPT, IMU_ESCALA ppm) ─► giro.c / rumbo.c │      └───────────────┬──────────────────────────┘
 │ sensores.c: SENS:<mm>,<ang×10>,<flags> cada 100 ms (AUTO)        │                      │ WiFi (video ~320 ms de retraso)
 │ carro.c: cola de comandos (32), watchdog 1000 ms, __STOP_AUTO    │                      ▼
 │   por láser (solo AUTO y avanzando, 2 lecturas < 120 mm), perfiles│        ┌───────────── TELÉFONO (Huawei P40 Pro) ─────────────┐
 │   AUTO 210/207 y MANUAL 255/252, PWM LEDC 1500 Hz 8 bit          │◄──WS──►│ Kotlin: ModoSemiautonomo.kt (tick 50 ms),            │
 └────────────────────────── motores TC1508A ◄─────────────────────┘  /ws   │   ControlLogica.kt (heartbeat 250 ms, STOP inmediato)│
                                                                              │   CamaraObstaculos.kt (video/celdas → obstáculos)    │
                                                                              │   DetectorTflite.kt + ModeloEnHilo.kt (EfficientDet) │
                                                                              │ JNI puente.c ─► nucleo_c (C99):                      │
                                                                              │   obstaculos.c  clasificador de color YCbCr multi-   │
                                                                              │                 cluster, zonas, cercanía 0-3         │
                                                                              │   corredor.c    corredor de perspectiva              │
                                                                              │   fusion.c      color + detector → fuentes 1/2/3     │
                                                                              │   navegador.c   máquina de estados y decisión        │
                                                                              │   trayectoria.c / calibracion.c / calgiro.c          │
                                                                              └──────────────────────────────────────────────────────┘
```

Camino de decisión por tick (50 ms), `navegador_tick()` en `navegador.c:5780`:

1. **Entradas** (todas con sello de hora del teléfono): `navegador_aporte_sens_imu()` (láser + ángulo del giroscopio, del SENS de 100 ms), `navegador_aporte_tof_paro()` (aviso `TOF_PARO:<mm>` del firmware), `navegador_aporte_giro()` (resultado de un `GIRAR`), `navegador_aporte_modelo()` (vigencia del detector), y el cuadro de obstáculos que arma `registrar_cuadro()` (`navegador.c:2363`).
2. **Percepción por cuadro**: `obstaculos.c` (color) → `corredor.c` → `fusion.c` (color + detector) deja `ResultadoObstaculos` con tres zonas (izq., centro, der.), cercanía 0–3 por zona, celdas y `fuentes_vision` (1 color, 2 red, 3 las dos).
3. **Fusión y política** en `registrar_cuadro()`: autoridad del láser (`tof_fresco` ≤ 400 ms y válida), `laser_cerca` (≤ `OBSTACULO_LASER_MM` = 400 mm), coincidencia color+red con consenso temporal (`coinc_seguidas`), contradicciones, vistazo, precaución. `bloqueo_confirmado()` (`:2720`) decide si el frente está bloqueado.
4. **Máquina de estados** (`EstadoNav`): `SIGUIENDO` (`tick_siguiendo`) → `DETENIDO_OBSTACULO` (`tick_detenido`, decide el lado) → `MIRAR` (`tick_mirar`) → `ESQUIVANDO` (`tick_esquivando`: primer giro, paso en paralelo) → `REINCORPORANDO` (`tick_reincorporando`: diagonal, mirada, vuelta a la línea) → `SIGUIENDO` o `AYUDA` (`tick_ayuda`) / `ERROR` / `TERMINADO`.
5. **Salida**: un `ComandoNav` (STOP, ADELANTE, ATRAS, GIRAR_n, etc.) más objetivo de rumbo (`navegador_tomar_rumbo_fw`) y giro por ángulo (`navegador_tomar_giro_fw`). Kotlin lo traduce al texto del protocolo (`FORWARD`, `BACKWARD`, `GIRAR:<grados>`, `RUMBO:<n>`, `STOP`).
6. **Firmware**: `carro_recibir_comando()` (renueva watchdog solo con comandos reales), cola con STOP prioritario, `carro_actualizar()` aplica perfil y PWM; `giro.c` hace el giro en lazo cerrado con el giroscopio; `rumbo.c` el PI de rumbo al avanzar; `tof.c` + `carro.c` el freno local `__STOP_AUTO` sin pasar por el teléfono.

Capas de seguridad **independientes del teléfono**: freno láser local (`TOF_PARO_MM` 120, 2 lecturas, solo AUTO y avanzando), `TOF_BLOQUEO_MM` 150 (un `FORWARD` en AUTO no arranca), watchdog de 1000 ms, STOP/EBRAKE con prioridad en la cola, `WATCHDOG_HW_MS` 5000 (reinicio si `loop()` se cuelga). Capas del **teléfono**: silencio de SENS 1.5 s, pérdida de video 700 ms (ERROR), detector caído 2.5 s (causa 17), cordura de pose (causas 9–12).

## 2. Archivos principales

| Capa | Archivo | Funciones o estructuras clave |
|---|---|---|
| Firmware láser | `firmware_carro/tof.c`, `firmware_carro.ino` (`hw_tof_*`) | `procesar()`, `tof_actualizar()`, `tof_cerca_confirmada()`, `tof_bloqueo_confirmado()`; `laser.setMeasurementTimingBudget(33 ms)`, `startContinuous(0)` (biblioteca de Pololu) |
| Firmware IMU | `imu.c`, `giro.c`, `rumbo.c` | sesgo, ZUPT, escala, `GIRAR:<n>` con anticipación aprendida, PI de rumbo |
| Firmware protocolo | `carro.c`, `sensores.c`, `config.h` | cola, watchdog, perfiles, `SENS`, `CAPS` |
| Cámara | `camara_prueba/camara_prueba.ino` | MJPEG, UDP de celdas, `/stats` (`cam-v2`) |
| App | `ModoSemiautonomo.kt`, `ControlLogica.kt`, `CamaraObstaculos.kt`, `DetectorTflite.kt`, `NavegadorNativa.kt` | tick, heartbeat, video, detector |
| Percepción (C) | `obstaculos.c`, `corredor.c`, `fusion.c`, `mascara_vision.c`, `movimiento_piso.c`, `vision_tele.c` | clasificador de piso, zonas, fusión |
| Decisión (C) | `navegador.c` (6 024 líneas), `trayectoria.c`, `calibracion.c`, `calgiro.c` | `registrar_cuadro`, `bloqueo_confirmado`, `tick_*`, `planificar`, `reinc_calcular` |
| Simulación | `mundo_v2.c`, `simulador_navegador.c`, `pruebas_autonomo_v2.c`, `scripts/compuertas_autonomo.py` | mundo sintético, láser de cono, cámara de celdas, detector estadístico |

## 3. Constantes y umbrales críticos (lo que se verifica)

### 3.1 Láser y frenado (firmware, `config.h`)

| Constante | Valor | Qué decide |
|---|---:|---|
| `TOF_PERIODO_MS` (presupuesto de tiempo de la biblioteca) | 33 ms | frecuencia y precisión del láser |
| `TOF_SONDEO_MS` | 4 ms | sondeo de «medida lista» |
| `TOF_PARO_MM` / `TOF_PARO_LECTURAS` | 120 mm / 2 | freno local (solo AUTO, solo avanzando) |
| `TOF_BLOQUEO_MM` | 150 mm | un FORWARD en AUTO no arranca |
| `TOF_LIBRE_MM`, 8190, 65535 | 1200 mm | todo lo mayor o sin eco = «libre» para el firmware |
| `TOF_MIN_VALIDO_MM` | 8 mm | por debajo se ignora |
| `TOF_ERRORES_MAX`, `TOF_SIN_DATOS_MS` | 3, 500 ms | sensor caído |
| `I2C_FRECUENCIA_HZ`, `I2C_TIMEOUT_MS` | 400 kHz, 5 ms | bus |
| `TIEMPO_WATCHDOG_MS`, latido de la app | 1000 ms, 250 ms | corte sin enlace |
| `COLA_COMANDOS_MAX` | 32 | cola con STOP prioritario |
| `SENS_PERIODO_MS`, `LOOP_MAX_US` | 100 ms, 5000 µs | telemetría y latencia del lazo |
| Velocidades | 38 cm/s (255), 19.3 cm/s (210/207) | distancia de frenado |
| `PWM_FRECUENCIA_HZ`, `PWM_BITS`, `ARRANQUE_PLENO_MS` | 1500 Hz, 8, 250 ms | motores |

### 3.2 Giroscopio y rumbo

`IMU_PERIODO_US` 10 000, ±500 °/s (65.5 LSB), DLPF ≈ 44 Hz, `IMU_ZONA_MUERTA_MDPS` 150, sesgo 250 muestras (desv. ≤ 12 LSB), ZUPT (400 ms, 20 estables, 1/256), `IMU_ESCALA_MAX_PPM` 50 000, `GIRO_MIN/MAX_GRADOS` 5/200, anticipación 20–250 ms, `RUMBO_KP_MIL_GRADO` 10, `RUMBO_TI_MS` 1500, `RUMBO_MAX_MIL` 150, error máximo 30°.

### 3.3 Visión (núcleo C)

| Constante | Valor | Dónde |
|---|---:|---|
| Umbral de color (distancia al piso) | 40 | `obstaculos.h`/app |
| Celdas mínimas por zona | 7 | idem |
| Fila mínima | 44 % (`OBS_FILA_MINIMA_MIL` 440) | `obstaculos.h:74` |
| Cercanía 2 / 3 | 667 ‰ / 834 ‰ de la altura | `OBS_MEDIA_MIL`, `OBS_CERCA_MIL` |
| Grupos de color del piso | K = 4, aprendizaje 2 %/cuadro | `OBS_GRUPOS_DEFECTO` |
| Reaprender piso | 1000 ms todo lejos | `OBS_MS_PISO_NUEVO` |
| Celda | 10×10 px (24×17 celdas con 240×176) | `OBS_LADO_CELDA` |
| Campo, altura, inclinación de la cámara | 56°, 115 mm, 12° — **SUPUESTOS** | `NAV_CAMARA_FOV_GRADOS`, `mundo_v2.h` |
| Retraso del video | ≈ 320 ms (medido por la app, `latenciaVisualMs`) | `ModoSemiautonomo.kt` |

### 3.4 Fusión y confirmación (`navegador.h`)

`OBSTACULO_LASER_MM` 400, `NAV_TOF_FRESCO_MS` 400, `NAV_TOF_CONFIRMA_MM` 500, `NAV_TOF_LIBRE_MM` 800 con `NAV_TOF_LIBRE_LECTURAS` 3, `NAV_COINCIDENCIA_CUADROS_FRENAR` 2, `NAV_COINCIDENCIA_CUADROS_ESQUIVAR` 3 (0.8 s), `MS_CONFIRMAR_LEJOS` 150 ms, `MS_CONFIRMAR_LIBRE` 250 ms, `NAV_MODELO_CAIDO_MS` 2500, `NAV_MAX_HUECO_VIDEO_MS` 700, `NAV_FIN_HOLGURA_MM` 400, `NAV_VISION_LIBRE_CUADROS` 5.

### 3.5 Rodeo, vistazo y recuperación

Presupuestos: `NAV_MAX_ESQUIVADAS` 6 por obstáculo, `NAV_MAX_ESQUIVADAS_RUTA` 18, `NAV_MAX_REINTENTOS` 2, `NAV_GIRO_FW_FALLOS_MAX` 3, `NAV_PLAN_INTENTOS` 5, `NAV_AYUDA_MAX_MS` 180 000, `NAV_TIMEOUT_ESQUIVAR_MS` 4000, `NAV_PASAR_SIN_AVANCE_MAX` 3, `NAV_EXTENSION_RUTA_MM` 2000, `NAV_PASO_PARALELO_MAX_MM` 2500. Márgenes: `NAV_ESQ_HOLGURA_VOLVER_MM` 200, `NAV_ESCANEO_RELLENO_MM` 40, alcance útil del barrido 700 mm, `NAV_BARRIDO_DER_MM` 285 / `IZQ` 225, `NAV_PASO_VISTA_MARGEN_MM` 230. Retrocesos: `NAV_RETROCESO_CM` 15, `NAV_RETROCESO_MAX_MM` 250, `NAV_DESHACER_MAX_MM` 450. Vistazo: 10–16°, `NAV_VISTAZO_ALCANCE_MM` 650, 2 ecos, 3 cuadros, 6 celdas o raya de 3. Cordura: `NAV_LOCURA_GIRO_MAX` 150°, recorrido 2.5×, desvío 1000 mm.

### 3.6 Banderas (por defecto)

`navegador_poner_precaucion` = NUNCA; `navegador_poner_vistazo` = LEJOS; `NAV_DESHACER_GIRO_MAX` = 0 (apagado); autoridad del láser encendida; `rumbo_en_vuelta` = 0.

## 4. Línea base de la rama de partida (16-nov-2026, antes de tocar nada)

| Verificación | Resultado |
|---|---|
| `make -C nucleo_c test` | 25 ejecutables, **1 884 casos, 0 fallos** |
| `make -C pruebas_firmware test` | exit 0 (564 casos según el estado del 13-nov; sin fallos) |
| `python probar_servidor_falso.py` (desde `pruebas_firmware/`) | **60 casos, 0 fallos** |
| `python scripts/probar_version.py` | 18 pruebas, OK |
| `python docs/verificar_enlaces.py` | 144 enlaces en 139 archivos, todos válidos |
| `scripts/compuertas_autonomo.py --corridas 1000` | ver el estado `docs/precision/autonomo_v2_comparacion.txt`: 19 compuertas en rojo (se vuelve a correr para confirmar) |
| `scripts/comparar_esquives.py` (Monte Carlo heredado) | 4 de 6 en rojo por 1 mm de media |

Ver [`docs/estado_20261113.md`](../estado_20261113.md) para el detalle por escenario.
