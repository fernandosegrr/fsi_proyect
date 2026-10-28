# Auditoría de afirmaciones: avance

Esta auditoría es parcial; no equivale a una revisión completa de los capítulos ni autoriza publicar el manual. Referencias al estado de `origin/main` c6ee161; números de línea aproximados, que pueden cambiar.

## Contradicciones y resolución editorial prevista

- Alimentación: `docs/referencia/hardware.md:62` atribuye una sola powerbank a DevKit y cámara; `:99` deja incierto si la comparten. El dueño confirmó que cada placa utiliza una powerbank propia. El manual y las figuras deben expresar esa confirmación, sin inferir una tierra común de la cámara. `docs/conexiones.md:95-101` sí respalda la tierra común entre DevKit, driver y negativo de pilas.
- Resolución de cámara: `docs/camara_prueba.md:14-15` conserva una conclusión histórica que deja QVGA por defecto; `camara_prueba/camara_prueba.ino:105-107` configura HQVGA. Para el comportamiento actual se usa el código; el documento histórico sirve para las condiciones y cifras de su barrido.
- Duración del bucle: la página 63 del manual anterior afirma un ciclo inferior a 5 ms. `docs/reporte_J13_fisico.md:68-77` registra 8.431 ms en reposo con app y 6.775 ms en una prueba en piso, con excesos. No se debe presentar el límite como superado; las muestras son cortas y la causa no está demostrada.
- Corriente: la página 68 del manual anterior propone valores típicos para los motores, mientras `docs/referencia/hardware.md:106-108` indica que no se midieron corriente ni consumo total. Omitir esos valores como medición del proyecto.

## Cifras y alcance comprobados hasta ahora

- J15, `docs/reporte_J15.md:37-55`: en el escenario B6 simulado se usaron 400 semillas por versión. El error lateral medio pasó de 13.2 a 1.9 cm y el percentil 95 de 21.9 a 4.6 cm. El reporte registra 400/400 rutas terminadas y cero contactos en ambas versiones; el umbral de error menor de 20 cm se cumplió en 344 y 400 casos, respectivamente. No confundir llegadas con el criterio de error.
- J15, `docs/reporte_J15.md:9,97`: cero corridas físicas con movimiento para esa versión; el criterio físico de cinco de seis corridas sin contacto y desvío inferior a 25 cm permanece NO MEDIDO.
- Cámara, `docs/camara_prueba.md:29-32,47-48`: barrido local sin cliente, 100 cuadros por combinación. A 20 MHz y RGB565 se registraron 12.4 fps y 137 ms de edad con HQVGA, frente a 8.8 fps y 252 ms con QVGA, calidad 50. La edad indicada llega hasta el final de compresión; no es latencia hasta la pantalla del teléfono.
- Perfiles, `firmware_carro/config.h:90-127`: configuración predeterminada MANUAL 252/255 y AUTO 207/210. El comentario registra 3 cm de desvío en 150 cm para MANUAL; AUTO está explícitamente pendiente de medición. Son valores configurados, sujetos a balances guardados.
- GPIO 2, `firmware_carro/config.h:140-148` y `docs/conexiones.md:72-79`: el software configura un pulso de 50 ms cada 2 s; la carga conectada y su eficacia física son NO MEDIDAS.
- Parada por láser, `firmware_carro/config.h:223-229`: la configuración pide dos lecturas válidas inferiores a 120 mm, únicamente en perfil AUTO y durante avance. Su autoridad puede desactivarse: no se debe describir como protección incondicional.
- Vigilancia, `firmware_carro/config.h:138-143`: 1000 ms para comunicación y 5000 ms para watchdog de hardware son valores de configuración, no mediciones universales de tiempo de respuesta.
- Vigilancia, `firmware_carro/carro.c:320-335,506-524,619-625`: PING y consultas de sensores no renuevan el temporizador, pero texto desconocido y otros mensajes pueden renovarlo; el giro por ángulo tiene supervisión propia. Se corrigió el borrador que atribuía el reinicio exclusivamente a órdenes válidas de movimiento.
- Percepción, `docs/obstaculos.md:77-110`: el modelo admite varios colores y documenta que una pared que llena la imagen puede reaprenderse como piso. `docs/fusion.md:20-30` describe la prioridad de color y el aumento de severidad por el modelo.
- Calibración histórica, `docs/calibracion.md:3-4` afirma que no había IMU; `nucleo_c/navegador.h:15,187-190` y `firmware_carro/config.h:156-177` documentan el giroscopio en la implementación posterior. Se conserva la estimación de distancia por tiempo y se describe el rumbo observado por el giroscopio.

## Fuentes revisadas por capítulo

- Introducción y arquitectura: `README.md`, `firmware_carro/config.h`, `camara_prueba/camara_prueba.ino` y confirmación del dueño para las dos baterías portátiles. La figura representa conexiones WiFi, no una conexión eléctrica entre cámara y DevKit.
- Hardware: `docs/conexiones.md:15-16,95-101,111-127` y `firmware_carro/config.h:66-81,141-148,156-165`. Los pines describen la configuración, y la carga real de GPIO 2 sigue pendiente.
- Firmware: `firmware_carro/carro.c:399-429,506-524,619-625`, `config.h:90-143,223-229` y `docs/reporte_J13_fisico.md:68-77`. La cola y el watchdog son comportamientos distintos.
- Cámara: `camara_prueba/camara_prueba.ino:93-107,169-170,643,943-951` y `docs/camara_prueba.md:29-48`. La edad medida no es latencia total al teléfono.
- Protocolo: `firmware_carro/carro.c:506-524,528-560`, `firmware_carro/sensores.c` y `docs/protocolo.md`. Las consultas y respuestas dependen de la versión del firmware.
- Navegación: `nucleo_c/navegador.h:15,38-72`, `docs/calibracion.md` y `docs/reporte_J15.md:17-59`. El rumbo se observa; la distancia sigue estimada por tiempo.
- Percepción: `nucleo_c/fusion.h:1-28`, `nucleo_c/fusion.c`, `docs/fusion.md` y `docs/obstaculos.md:77-110`. La prioridad del color describe la regla básica; otras etapas pueden alterar el resultado.
- Aplicación: `app_android/app/src/main/java/com/fsi/carro/ModoSemiautonomo.kt`, `ControlLogica.kt`, `CamaraObstaculos.kt` y `docs/referencia/app.md`. Las capturas son estados diferentes, no un registro continuo de operación.
- Compilación: `app_android/app/build.gradle.kts:35-37,68-76`, `scripts/cargar.ps1`, `scripts/verificar_version.py` y `docs/referencia/compilacion.md`. Los comandos son instrucciones para uso posterior, no constancia de una carga en esta revisión.
- Operación: `docs/guia_conexion_J15.md`, `docs/operacion.md` y `docs/conexiones.md`; se reemplaza la afirmación de una sola batería portátil por la confirmación del dueño.
- Pruebas y límites: `docs/reporte_J13_fisico.md:68-86`, `docs/reporte_J14.md`, `docs/reporte_J15.md:37-59,97` y `docs/limites_conocidos.md`.
- Desarrollo y solución de problemas: `docs/bitacora.md`, `docs/historia.md`, `docs/camara_prueba.md`, `docs/reporte_J15.md` y `docs/conexiones.md`; se omiten justificaciones categóricas que no quedaron respaldadas por mediciones.

No se corrigió ningún documento fuente ni se realizaron pruebas físicas. Esta relación no equivale a un ensayo nuevo del sistema.

## Nota adicional de seguridad en compilación

`scripts/cargar.ps1:12-18` advierte que la identificación automática en los modos baseline y camara puede cargar una sonda y sobrescribir el firmware de una placa no reconocida. `-SoloCompilar` se detiene antes de esa etapa. Se documenta esta distinción para evitar describir toda detección como no destructiva.
