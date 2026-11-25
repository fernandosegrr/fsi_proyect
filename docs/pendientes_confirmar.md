# Pendientes por confirmar con el equipo (v1.2)

Generado con `python docs/manual/revisar_manual.py pendientes`. Cada fila es una pregunta que NO se pudo responder con el código ni con los documentos del repositorio y que quedó marcada en el texto como `CONFIRMAR CON EL EQUIPO`. Cuando el equipo la responda: corregir el capítulo, borrar la marca y volver a correr el comando.

Total: **43** preguntas en 11 documentos.

## Resumen por capítulo

| Capítulo | Preguntas |
|---|---|
| Subsistema A · Hardware del carro | 7 |
| Subsistema B · Firmware del carro | 4 |
| Subsistema C · Módulo de cámara | 2 |
| Subsistema D · App Android (Kotlin) | 2 |
| Subsistema E · Núcleo de navegación en C (nucleo_c) | 3 |
| Subsistema F · Visión | 4 |
| Subsistema G · Seguridad | 3 |
| Subsistema H · Simulador y pruebas | 3 |
| Subsistema I · Proceso de desarrollo | 2 |
| 12. Pruebas y resultados | 1 |
| Guía de exposición | 12 |

## Lista completa

| # | Capítulo | Archivo | Pregunta |
|---|---|---|---|
| 1 | Subsistema A · Hardware del carro | `docs/manual/capitulos/01d_subsistema_A_hardware.md` | ¿Por qué se usaron ruedas mecanum si con este driver de dos canales el carro no puede moverse de lado? |
| 2 | Subsistema A · Hardware del carro | `docs/manual/capitulos/01d_subsistema_A_hardware.md` | ¿La cámara usa una segunda powerbank (`README.md`, capítulo 2) o comparte la del DevKit (`docs/seguridad.md`, `docs/referencia/hardware.md`)? |
| 3 | Subsistema A · Hardware del carro | `docs/manual/capitulos/01d_subsistema_A_hardware.md` | ¿Qué pilas AA se usan en la demostración, alcalinas (unos 6 V) o NiMH (unos 4.8 V), y por qué cuatro? |
| 4 | Subsistema A · Hardware del carro | `docs/manual/capitulos/01d_subsistema_A_hardware.md` | ¿Se comparó 1,500 Hz con 5,000 Hz en el carro? |
| 5 | Subsistema A · Hardware del carro | `docs/manual/capitulos/01d_subsistema_A_hardware.md` | ¿A qué altura, con qué inclinación y a cuántos mm del frente del carro está montado el láser? (`docs/sensores.md` dice 6 a 8 cm y de 5 a 8° hacia arriba; el capítulo 3 dice horizontal; el simulador usa 55 mm.) |
| 6 | Subsistema A · Hardware del carro | `docs/manual/capitulos/01d_subsistema_A_hardware.md` | ¿Qué hay conectado al GPIO 2 (resistencia, LED o nada) y la powerbank realmente necesita el pulso? |
| 7 | Subsistema A · Hardware del carro | `docs/manual/capitulos/01d_subsistema_A_hardware.md` | ¿Qué marcado trae el driver (TC1508A o TC1508S) y cómo se rotulan sus entradas (IN1 a IN4, INT1 a INT4 o INA a IND)? |
| 8 | Subsistema B · Firmware del carro | `docs/manual/capitulos/01e_subsistema_B_firmware.md` | ¿hubo otra razón para crear una red propia y no usar el WiFi de la escuela, además de no depender de un router externo? |
| 9 | Subsistema B · Firmware del carro | `docs/manual/capitulos/01e_subsistema_B_firmware.md` | ¿por qué 1,000 ms para el watchdog de comandos y 5,000 ms para el de hardware? El repo solo pide que el latido de 250 ms sea mucho menor (`firmware_carro/config.h`). |
| 10 | Subsistema B · Firmware del carro | `docs/manual/capitulos/01e_subsistema_B_firmware.md` | ¿por qué el paro por láser solo actúa en perfil AUTO y avanzando, y no en MANUAL? |
| 11 | Subsistema B · Firmware del carro | `docs/manual/capitulos/01e_subsistema_B_firmware.md` | ¿qué significan las letras SEG en `SEG=2` y `SEG=3`? |
| 12 | Subsistema C · Módulo de cámara | `docs/manual/capitulos/01f_subsistema_C_camara.md` | ¿quién decidió pasar de QVGA a HQVGA por defecto y con qué prueba? |
| 13 | Subsistema C · Módulo de cámara | `docs/manual/capitulos/01f_subsistema_C_camara.md` | ¿en el carro la cámara lleva su propia powerbank o comparte la del DevKit? |
| 14 | Subsistema D · App Android (Kotlin) | `docs/manual/capitulos/01g_subsistema_D_app_android.md` | ¿por qué se eligieron Views y Material en lugar de Compose? |
| 15 | Subsistema D · App Android (Kotlin) | `docs/manual/capitulos/01g_subsistema_D_app_android.md` | ¿es este el caso conocido de «Continuar» tras AYUDA sin mover el carro, y qué debería hacer? |
| 16 | Subsistema E · Núcleo de navegación en C (nucleo_c) | `docs/manual/capitulos/01h_subsistema_E_nucleo_c.md` | ¿por qué se eligió C99 y no Kotlin o C++ para el navegador, y se probó otra opción? |
| 17 | Subsistema E · Núcleo de navegación en C (nucleo_c) | `docs/manual/capitulos/01h_subsistema_E_nucleo_c.md` | ¿por qué celdas de 20 cm (CELDA_CM) y no de otro tamaño? |
| 18 | Subsistema E · Núcleo de navegación en C (nucleo_c) | `docs/manual/capitulos/01h_subsistema_E_nucleo_c.md` | ¿el equipo comparó VFH o seguimiento de borde antes de elegir el rodeo medido, o esa comparación fue posterior? |
| 19 | Subsistema F · Visión | `docs/manual/capitulos/01i_subsistema_F_vision.md` | ¿quién entrenó exactamente ese archivo y alguien verificó su licencia Apache 2.0 en la ficha oficial? |
| 20 | Subsistema F · Visión | `docs/manual/capitulos/01i_subsistema_F_vision.md` | ¿en qué fecha y con qué versión se hizo ese ajuste en vivo? |
| 21 | Subsistema F · Visión | `docs/manual/capitulos/01i_subsistema_F_vision.md` | ¿cuáles son la altura, la inclinación y el campo de visión reales de la cámara? El simulador usa 11.5 cm, 12° y 56° (`mundo_v2.h`); el corredor, 10 cm, 20° y 60°. |
| 22 | Subsistema F · Visión | `docs/manual/capitulos/01i_subsistema_F_vision.md` | ¿en la prueba B6 estaba encendido el modelo de objetos? El documento solo dice «todo habilitado». |
| 23 | Subsistema G · Seguridad | `docs/manual/capitulos/01j_subsistema_G_seguridad.md` | ¿por qué se eligió 1,000 ms para `TIEMPO_WATCHDOG_MS`? La constante está desde el primer firmware modular (10-oct) y el repo solo la justifica después. |
| 24 | Subsistema G · Seguridad | `docs/manual/capitulos/01j_subsistema_G_seguridad.md` | ¿el freno por láser se limitó al perfil AUTO porque a velocidad MANUAL 120 mm no alcanzaría? El repo da esa cuenta, pero no dice que fuera la razón. |
| 25 | Subsistema G · Seguridad | `docs/manual/capitulos/01j_subsistema_G_seguridad.md` | ¿se grabará `SEG=3` antes del 1-dic-2026 o el carro llegará con el firmware v1.1? |
| 26 | Subsistema H · Simulador y pruebas | `docs/manual/capitulos/01k_subsistema_H_simulador_pruebas.md` | ¿por qué v3 y no v1.1 con la revisión de UI como base de la exposición, si v3 tiene 23 compuertas en rojo y no está validada en el carro? |
| 27 | Subsistema H · Simulador y pruebas | `docs/manual/capitulos/01k_subsistema_H_simulador_pruebas.md` | ¿qué es 0xC0FFEE01 y dónde se usó? |
| 28 | Subsistema H · Simulador y pruebas | `docs/manual/capitulos/01k_subsistema_H_simulador_pruebas.md` | ¿la validación de la v1.2 usará la partición DESARROLLO y RESERVADO, o se queda como método de las ramas v4 a v6? |
| 29 | Subsistema I · Proceso de desarrollo | `docs/manual/capitulos/01l_subsistema_I_proceso.md` | ¿hay una decisión escrita de no usar v5-carbon ni v5-carbon-fix, o se tomó al ver la evaluación ciega? |
| 30 | Subsistema I · Proceso de desarrollo | `docs/manual/capitulos/01l_subsistema_I_proceso.md` | ¿qué versión de la app se usó en las 7 rectas del 15-nov? La caja negra no la anota y los documentos la llaman «v3» y «versión anterior». |
| 31 | 12. Pruebas y resultados | `docs/manual/capitulos/12_pruebas.md` | las cifras de esta tabla en el manual v1.1 (`94 mm -> 8 mm`, `64 -> 36`, `142 -> 42`, `128 -> 38`, con porcentajes de éxito) no aparecen en `docs/precision/monte_carlo_1000.txt`; ¿de qué corrida o documento salieron? |
| 32 | Guía de exposición | `docs/guia_exposicion.md` | ¿Por qué ruedas mecanum si con un driver de dos canales no hay movimiento lateral? |
| 33 | Guía de exposición | `docs/guia_exposicion.md` | ¿La cámara lleva su propia powerbank o comparte la del DevKit? |
| 34 | Guía de exposición | `docs/guia_exposicion.md` | ¿Se grabará el firmware v3 (`SEG=3`) antes del 1 de diciembre o llega el v1.1? |
| 35 | Guía de exposición | `docs/guia_exposicion.md` | ¿Por qué v3 y no v1.1 con la revisión de UI como base de la exposición? |
| 36 | Guía de exposición | `docs/guia_exposicion.md` | ¿Hubo otra razón para crear una red propia además de no depender de un router externo? |
| 37 | Guía de exposición | `docs/guia_exposicion.md` | ¿Por qué C99 y no Kotlin o C++ para el navegador, y se probó otra opción? |
| 38 | Guía de exposición | `docs/guia_exposicion.md` | ¿Quién entrenó ese archivo y alguien verificó su licencia Apache 2.0 en la ficha oficial? |
| 39 | Guía de exposición | `docs/guia_exposicion.md` | ¿Quién hizo qué parte? |
| 40 | Guía de exposición | `docs/guia_exposicion.md` | ¿Qué decidió y revisó el equipo, y qué propuso o escribió una herramienta de IA? |
| 41 | Guía de exposición | `docs/guia_exposicion.md` | ¿El curso o el profesor piden declarar el uso de IA, y de qué forma? |
| 42 | Guía de exposición | `docs/guia_exposicion.md` | ¿Quién entiende y puede explicar cada parte del sistema sin ayuda? |
| 43 | Guía de exposición | `docs/guia_exposicion.md` | ¿Quién operó el carro y anotó los resultados en las pruebas físicas? |
