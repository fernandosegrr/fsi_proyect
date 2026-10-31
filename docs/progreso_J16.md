J16 — Precisión de navegación (solo software)

31 de octubre de 2026, hora de Ciudad de México. Trabajo 100 % de software: sin carro, sin cámara, sin teléfono y sin mover nada.

Estado actual: TERMINADO en software (31 oct 2026). Validación física: NO MEDIDA (plan en docs/precision/plan_pruebas.md). Reporte: docs/reporte_J16.md.

Decisiones iniciales:
- El encargo dice D:\Dev\fsi_proyecto; en esta PC no hay D:. El repositorio real está en E:\Dev\fsi_proyecto (igual que en J15). La copia de OneDrive no se toca.
- Rama j16-precision creada desde origin/main en 5d5c628 (incluye J15 y la documentación más reciente). Se le quitó el upstream automático (apuntaba a origin/main) para que un push nunca vaya a main; se publica con git push -u origin j16-precision. No se fusiona a main ni se crean tags.
- docs/manual y docs/figuras son de otro agente: no se modifican. Ante conflicto se conserva la versión de main.
- Identidad de commits: la noreply del repositorio (fernandosegrr, 223138626+fernandosegrr@users.noreply.github.com), ya configurada en el repositorio.
- dist/firmware_carro_453ed20-20261027.bin está excluido por .git/info/exclude y no se versiona.

Volver a v1.0 (cuando el dueño lo decida):
  git switch --detach v1.0      (solo mirar o compilar v1.0)
  git switch main               (volver a la rama principal)
El tag v1.0 (eb88151) no se mueve ni se borra.

Hallazgo documental (confirmado leyendo el código): la ruta en L predefinida (RutasPredefinidas.kt) va de la celda (2,6) a (2,1) y de ahí a (7,1). El último tramo está en y = 200 mm, no en y = 0. Las poses finales estimadas de B5 (y = 209, 203 y 244 mm) dan un error lateral ESTIMADO de 9, 3 y 44 mm respecto de esa línea. La cifra de "20 a 24 cm" de J14 sale de comparar contra y = 0 y no es una medición: el error físico de la L sigue NO MEDIDO (el dueño solo dijo "relativamente bien").

Comandos para reanudar:
  cd /e/Dev/fsi_proyecto
  git status -sb                      (debe decir j16-precision)
  cd nucleo_c && make test            (núcleo)
  cd ../pruebas_firmware && make test (firmware)

Bitácora:
- Lectura de navegador.c/.h, calibracion.h, simulador_navegador.c/.h, pruebas_reincorporacion_j15.c, imu.c, config.h, carro.c y reportes J14/J15. Revisiones paralelas de firmware y app/JNI (hechas; hallazgos verificados en el código antes de actuar).
- Línea base antes de tocar nada: núcleo 21 programas / 1598 casos, firmware 447 (152 + 246 + 15 + 34), sin fallos.
- Firmware: nuevo firmware_carro/rumbo.c/.h (control PI del rumbo al avanzar, apagado por defecto, RUMBO:ON/OFF/<n>/RUMBO?), integrado en carro.c; CAPS anuncia RUMBO=1; filtro giro_largo por grado en giro.c (bandera GIRO_LARGO_POR_GRADO). Nueva prueba pruebas_firmware/pruebas_rumbo.c (62 casos).
- Núcleo: objetivos de RUMBO (navegador_poner_rumbo_fw / navegador_tomar_rumbo_fw), pose medida con décimas (sin_trig_fino), recta con largo fijo arreglada (terminaba al volver de esquivar; ahora se planea por celdas con rumbo medido y nunca vuelve a puntos pasados), dt descartado tras caer a giro por tiempo entre ticks (sin_arreglo_dt_giro).
- Probado y descartado: descontar el retraso del video y el frenado en la cara del obstáculo (acortaba el paso en paralelo y empeoraba el esquive simulado). No quedó en el código.
- Simulador: modelo físico J16 opcional en simulador_navegador.c (sim_j16_activar). Error del propio simulador corregido: con latencia de comandos se filtraban LEFT/RIGHT durante un GIRAR.
- Banco nuevo: nucleo_c/pruebas_precision_j16.c (Partes A, B y Monte Carlo). make test lo corre con 150 corridas; make precision con 1000.
- Calibración del simulador (configuración J14): diferencia de empuje tras el balance de 0.3 a 2.1 %; B4 en percentiles 9/57/73/82; B6-4 en el percentil 99. Datos: docs/precision/calibracion_j14.txt.
- Hallazgo en simulación: la holgura de 100 mm al pasar el obstáculo dejaba rozar la caja en el giro final (J15 0.9 %, J14 4.9 %); con 150 mm, 0 de 1000. Adoptado (sin_margen_paso_j16).
- Monte Carlo final de 1000 corridas: docs/precision/monte_carlo_1000.txt (resumen en el reporte).
- Suites al cerrar el código: núcleo 22 programas / 1630 casos, firmware 509, servidor falso 46, verificador 18; UBSan local sin fallos.
- Commit de código d823a63. APK limpia APP:0.4-d823a63 (14,508,012 bytes, SHA-256 76DB8486...484D) en dist/; firmware FW:d823a63-20261031 compilado con cargar.ps1 nuevo -SoloCompilar, NO cargado; copia local en dist/ excluida en .git/info/exclude.
- Verificador: nuevo modo --solo-archivo (sin teléfono ni puerto serie): PASA.

Para retomar (si hiciera falta):
  cd /e/Dev/fsi_proyecto/nucleo_c && make precision          (informe de 1000 corridas)
  ./pruebas_precision_j16 depurar 8 3 0                      (una corrida del esquive con RUMBO, con registro)
  python scripts/verificar_version.py --solo-archivo --apk dist/carro_fsi_0.4-d823a63.apk
- Publicado: git push -u origin j16-precision (3c9ebcf), sin forzar; origin/main sigue en 5d5c628 y v1.0 en eb88151. GitHub Actions «sanitizadores» 41224593460: success.

