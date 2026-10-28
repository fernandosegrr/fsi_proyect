# Referencia de pruebas

**Estado:** documento de referencia · **Fecha:** 27 de octubre de 2026 · **Rama:** `docs/documentacion-completa`
**Fuentes:** `nucleo_c/Makefile`, `pruebas_firmware/Makefile`, `README.md`, `docs/reporte_J15.md`, `docs/reporte_J14.md`, `docs/pruebas_control.md`, `docs/validacion_sensores.md` y `.github/workflows/sanitizadores.yml`.

## 1. Qué se prueba y qué no

El proyecto separa tres tipos de prueba:

1. **Pruebas de PC (software puro):** compilan la lógica real y la ejecutan con hardware falso o datos sintéticos. Son rápidas, repetibles y no necesitan el carro.
2. **Pruebas con servidor falso:** simulan al carro por WebSocket para probar la app y el protocolo sin la placa.
3. **Pruebas físicas:** se hacen con el carro real, en el piso, con personas y obstáculos. Son las únicas que demuestran el comportamiento físico, y son las que menos se pueden repetir.

Este documento describe las dos primeras. Las físicas se resumen al final, con su estado real: **J15 no tuvo ninguna corrida** (`docs/reporte_J15.md` §7).

## 2. Números registrados

| Suite | Programas | Casos | Fallos | Dónde |
|---|---|---|---|---|
| Núcleo en C | 21 | 1598 | 0 | `nucleo_c/` |
| Firmware del carro | 4 ejecutables | 447 | 0 | `pruebas_firmware/` |
| Servidor falso | 1 | 46 | 0 | `pruebas_firmware/servidor_falso.py` |
| Verificador de versiones | 1 | 13 | 0 | `scripts/verificar_version.py` |
| Sanitizadores | CI | — | success | GitHub Actions, run 40613481109 |

Los 447 del firmware se reparten en **152** de `pruebas_carro`, **246** de `pruebas_sensores`, **15** de `pruebas_escalonado` y **34** de `pruebas_camara` (`docs/reporte_J15.md` §5). Los 1598 del núcleo conservan los 1577 anteriores y suman 21 de J15 (`pruebas_reincorporacion_j15.c`).

**Estos números son del último registro, no de una ejecución nueva.** No se repitieron durante el cierre documental por indicación del dueño. Cualquier persona que los repita debe anotar fecha, comando y salida.

## 3. Pruebas del núcleo

Se ejecutan con:

```bash
cd nucleo_c
make test
```

La lista completa está en `nucleo_c/Makefile:7` y la secuencia de ejecución en `72-93`. Cada programa imprime `OK` o `FALLO` por caso y un resumen al final.

| Programa | Qué comprueba |
|---|---|
| `pruebas_calibracion` | Promedios, medianas, atípicos, ajuste de recta, conversión cm↔ms y validación |
| `pruebas_calgiro` | Ocho giros guiados, seguridad y resultado de la calibración con giroscopio |
| `pruebas_trayectoria` | Rutas válidas, saltos, puntos repetidos, demasiados puntos y pasos |
| `pruebas_obstaculos` | Detección por color, franja de referencia, zonas, cercanía y errores |
| `pruebas_piso` | Casos de piso con distintos colores, sombras y ruido |
| `pruebas_fusion` | Fusión detector/modelo y regla «el color manda» |
| `pruebas_movimiento` | Giro visual entre cuadros, confianza y signo |
| `pruebas_navegador` | Estados, avance, giros, esquive, reincorporación y seguridad (345 casos en J11) |
| `pruebas_video_stats` | fps, huecos y ventana de 10 s |
| `pruebas_prueba_obstaculo` | Cuentas de la prueba de obstáculos |
| `pruebas_corredor` | Geometría del corredor y cajas dentro/fuera |
| `pruebas_pisos_dificiles` | Pisos negros, baldosas, franjas y cambios de exposición |
| `pruebas_cualquiera` | Modo «cualquier objeto»: filtros y persistencia |
| `pruebas_movimiento_piso` | Flujo del piso contra lo esperado |
| `pruebas_celdas` | Paquete UDP de celdas: armar, leer y rechazar |
| `pruebas_sesion_compartida` | Entradas, salidas, gracia y reapertura |
| `pruebas_latencia_video` | Comparación con y sin video |
| `pruebas_ruta_editor` | Vértices, deshacer, rehacer, plantillas y texto |
| `pruebas_mascara` | Artefactos fijos, límites y zonas que dependen del lugar |
| `pruebas_sim_ruta` | Rutas simuladas con obstáculos |
| `pruebas_reincorporacion_j15` | 21 casos de la corrección de pose de J15 |

El `Makefile` compila con `-Werror`: una advertencia rompe la compilación. Eso obliga a mantener el código limpio y evita que un aviso se vuelva un fallo silencioso.

## 4. Pruebas del firmware

Se ejecutan con:

```bash
cd pruebas_firmware
make test
```

Compilan `carro.c`, `imu.c`, `tof.c`, `giro.c`, `sensores.c` y `recuperacion.c` **sin modificarlos**, con hardware falso (`pruebas_firmware/Makefile:9-27`).

| Ejecutable | Qué comprueba |
|---|---|
| `pruebas_carro` (152) | Comandos, motores, perfiles, balance, cola, watchdog, clientes, keep-alive, DHCP y página |
| `pruebas_sensores` (246) | MPU6050, VL53L0X, giro por ángulo, parada por láser, mensajes y fallos |
| `pruebas_escalonado` (15) | La misma lógica con `ARRANQUE_ESCALONADO_MS=40` |
| `pruebas_camara` (34) | Decisiones de recuperación de la cámara, incluida la vuelta de `millis()` |
| `verificar_pagina.py` | Hash de la página web del carro |

El hardware falso (`sensores_falsos.c`) permite simular el MPU6050 y el VL53L0X, provocar errores de bus, congelar lecturas y controlar el tiempo. Así se prueban caminos que con el hardware real serían difíciles de reproducir.

## 5. Estrés y concurrencia

Se ejecutó una **prueba de estrés** (mandar muchísimos comandos seguidos para ver si algo se rompe) de **100 000 comandos** con dos **hilos** (dos tareas que corren a la vez dentro del mismo programa), mezclando movimientos, `STOP`, `PING`, giros y consultas. El resultado registrado es **0 STOP perdidos** y estado final correcto (`docs/reporte_J15.md` §5). Es la prueba que respalda la regla de la cola: cuando está llena, se descarta un `PING` o un comando viejo, nunca un `STOP` (`carro.c:399-429`).

Los **sanitizadores** son herramientas que añaden comprobaciones extra al programa para detectar errores de memoria o de concurrencia que, de otro modo, pasarían desapercibidos. Corren en GitHub Actions sobre el último commit con código, `074c9fd`:

- **ThreadSanitizer** sobre el estrés con giros.
- **AddressSanitizer + UndefinedBehaviorSanitizer** sobre sensores, carro y navegador.

El resultado fue `success` (run 40613481109). En esta PC Windows ThreadSanitizer no está disponible; por eso se usa Linux en CI.

## 6. Servidor falso

`pruebas_firmware/servidor_falso.py` imita al carro por WebSocket: acepta los comandos, responde `PONG`, `ACK_*`, `SENS`, `GIRO_*` y `TOF_PARO`, y permite probar la app sin la placa (`docs/pruebas_control.md`). Tiene **46 casos** registrados.

Sirve para:

- comprobar que la app conecta, manda y recibe;
- probar el watchdog y la reconexión;
- simular sensores y fallos sin gastar batería ni mover el carro;
- repetir un caso tantas veces como haga falta.

**No sirve** para validar la física, la cámara real ni el WiFi real. Un caso que pasa con el servidor falso puede fallar con el carro si el tiempo o la red son distintos.

## 7. Verificador de versiones

`scripts/verificar_version.py` tiene **13 pruebas** registradas. Comprueba:

- que el hash del firmware leído por el puerto serie corresponda a un commit;
- que la APK contenga el `version_app.txt` correcto;
- que los bytes instalados en el teléfono sean idénticos a los publicados;
- que los commits posteriores solo toquen documentación, entrega o el propio verificador.

El caso de J15 con `--solo-apk` dio **PASA** y dejó el firmware como pendiente (`docs/entrega.md` §3).

## 8. Pruebas físicas: lo que sí se hizo

Las pruebas físicas de J13/J14 están en `docs/reporte_J13_fisico.md` y `docs/reporte_J14.md`. Las principales:

- **B0-B3:** conexión, mandos, watchdog, giros y rectas.
- **B4:** rectas de 150 cm con desvíos de 5.5 a 20 cm.
- **B5:** ajuste de balance y recta.
- **B6:** cuatro corridas con una caja a ~100 cm. La segunda probablemente tocó la caja; la cuarta la evitó pero quedó ~40 cm a la derecha. Las corridas 1 y 3 hicieron dos esquivas y terminaron con rumbos estimados de 117/119°.
- **B7, B8, B9 y B2:** no se realizaron por decisión del dueño (`README.md`, J14).

Esos resultados son **históricos** y no se atribuyen a la APK nueva de J15.

## 9. J15: lo que no se midió

- **Cero corridas físicas.** No hay mediciones nuevas de contacto, desvío, rumbo, sensores ni anticipación.
- El criterio de **5 de 6 corridas sin tocar la caja y con desvío final menor de 25 cm** queda **NO MEDIDO**.
- La simulación B6 dio 400/400 sin contacto, error lateral medio 1.9 cm y p95 4.6 cm, pero es **SIMULADO** (`docs/reporte_J15.md` §4).
- No se creó v1.1; v1.0 sigue en `eb88151`.

## 10. Lo que ninguna prueba de PC cubre

- El deslizamiento real de las ruedas.
- La vibración del chasis y el ruido del giroscopio en movimiento.
- La luz del lugar, el color real del piso y los reflejos.
- La latencia real del WiFi y del video en el teléfono.
- La corriente y la temperatura del driver.
- El comportamiento de la batería al caer su voltaje.
- La sujeción de la cámara y su orientación exacta.

Por eso, cuando alguien pregunte «¿está probado?», la respuesta correcta es: **la lógica está probada en PC; el comportamiento físico tiene pruebas parciales de J13/J14 y J15 quedó pendiente.**

## 11. Cómo repetir las pruebas sin romper nada

1. Las pruebas de PC no tocan hardware: se pueden correr en cualquier momento.
2. Antes de una prueba física, revisar `docs/guia_conexion_J15.md` y `docs/seguridad.md`.
3. No operar el carro sin autorización del dueño.
4. Anotar fecha, comando, versión de la app, versión del firmware y resultado.
5. Si algo falla, no «arreglarlo» en la documentación: anotarlo en `docs/pendientes_documentacion.md` con archivo y línea.

## 12. Para seguir leyendo

- `docs/codigo/pruebas_firmware.md`: cómo están hechas las pruebas y qué simula el hardware falso.
- `docs/pruebas_control.md`: pruebas de la app con servidor falso.
- `docs/validacion_sensores.md`: pruebas físicas de los sensores.
- `docs/reporte_J14.md` y `docs/reporte_J15.md`: resultados y límites.
