J15 — Reporte de corrección de la reincorporación a la ruta

27 de octubre de 2026, hora de Ciudad de México.

1. Estado de cierre y alcance

La corrección está implementada, probada en PC, publicada e instalada en el Huawei. Las etapas 1 a 4 terminaron. No se ejecutó ninguna corrida física de J15 ni se envió ningún movimiento. El dueño pidió este reporte y continuará las pruebas con otro modelo; por esa indicación no se ejecutan ni se repiten pruebas en este cierre.

Resultado: PASA en el escenario simulado; NO MEDIDO en el carro. El criterio físico de al menos 5 de 6 corridas sin tocar la caja y con desvío final menor de 25 cm todavía no se evaluó. No se creó v1.1. v1.0 permanece en eb88151.

Repositorio de trabajo: E:\Dev\fsi_proyecto, rama main. La unidad que el encargo llamaba D: está montada como E:. La copia antigua de OneDrive y original_backup/ no se modificaron. La rama local j14-wip-20261027 no se publicó.

2. Defecto encontrado y evidencia

En el código anterior, calcular_pose proyectaba todo un avance usando base_rumbo_grados fijo. La app sí recibía el ángulo del giroscopio por SENS, pero el navegador lo aprovechaba para resolver giros, no para observar el cambio de rumbo durante una recta. La reincorporación calculaba su regreso sobre esa pose que podía separarse del movimiento real sin enterarse.

Esto es un defecto confirmado de la estimación. Concuerda con B4, donde se midieron desvíos de 5.5 a 20 cm en 150 cm, y con B6-4: el navegador estimó x=409 mm frente a una línea en x=400 mm (0.9 cm de error), mientras el dueño observó aproximadamente 40 cm a la derecha.

No se ha demostrado que ese defecto explique por sí solo los 40 cm. No quedaron registros completos de B6 con lado elegido, cada giro, tramo lateral y SENS para reconstruir esa corrida. También siguen siendo hipótesis la diferencia entre calibración y movimiento efectivo, los errores del pivote, el deslizamiento de ruedas y las detecciones que repiten el esquive.

La geometría de los pivotes y los giros por ángulo ya existían: no faltaban por completo. Con giroscopio sano, los giros de esquive y regreso de 5 a 200 grados usan GIRAR. El mensaje 3/10 es el progreso por puntos de la ruta; no es un contador de diez errores. La causa concreta de AYUDA en B6-2 no se puede determinar sin su diagnóstico.

3. Qué cambió

- navegador.c/.h: cada incremento de distancia calculado por tiempo se orienta con el cambio de rumbo observado en SENS. Se integra el recorrido por incrementos, sin rotar retroactivamente la distancia ya recorrida; se conservan los residuos en enteros para reducir el redondeo. El frenado se proyecta con el último rumbo.
- Se comprueban vigencia y validez del IMU. Una lectura atrasada conserva el último rumbo conocido; la recuperación tras IMU caída/calibrando crea una referencia sin salto. Se descartan saltos de más de 25 grados durante un avance.
- Se conserva el interruptor de giroscopio existente. Apagado mantiene el cálculo anterior. La Autoridad del láser sigue encendida por defecto en la app.
- ModoSemiautonomo.kt, NavegadorNativa.kt y puente.c transmiten el bit IMU lista y excluyen la calibración de la integración del rumbo.
- POSE_DIAG registra al cerrar cada avance la pose estimada, el rumbo inicial, la validez/edad del IMU y el método utilizado. Se conservan TURN_DIAG, ESQUIVE_DIAG, REINC_DIAG y los eventos de sensores.
- Se agregó pruebas_reincorporacion_j15.c al make test del núcleo y a los sanitizadores de GitHub Actions. El simulador admite un láser calculado contra la caja y errores físicos de giro por lado, separados de la pose estimada.
- verificar_version.py agrega --solo-apk para comprobar una instalación sin placa serie; informa explícitamente que la lectura de FW queda pendiente.

No cambió firmware_carro. No se añadió un lazo BAL ni una maniobra adicional de alineación: la integración del rumbo, junto con los planes y correcciones existentes, alcanzó el objetivo simulado. No se reescribió el navegador completo.

4. Simulación: modelo y resultados

Se ejecutaron 400 semillas por escenario y versión: B6 con caja, recta sin caja y ruta en L sin caja, antes y después. Total: 2400 rutas. Las semillas son 15001 a 15400. El mundo simulado mueve su propia posición, independiente de la pose del navegador.

Modelo principal: recta de 200 cm, AUTO a 19.3 cm/s, pivotes nominales medidos de 90/-90 mm a la izquierda y 150/165 mm a la derecha para 90 grados, errores de giro de media cero y sigma aproximada 1.1/1.7 grados limitados a ±3 grados, y curvatura derecha equivalente a un rumbo medio aproximado de 2 a 6 grados en 150 cm.

Supuestos adicionales declarados: caja de 20 x 20 cm con centro a 1 m del centro inicial, chasis de 20 x 24 cm, arranque de 100 ms y variación del pivote de ±5 %. Esas dimensiones y dispersión no se midieron en esta sesión. Video aproximado por zonas, 10 cuadros/s y 200–300 ms de retraso; SENS cada 100 ms y tick de 50 ms. El láser se calcula por un rayo independiente contra la caja. Frenado mecánico de 25–35 mm separado de la latencia: no se suman de nuevo los 9–14 cm de parada total observada por visión.

B6 antes:
- Error lateral absoluto medio: 13.2 cm.
- p50: 13.5 cm; p95: 21.9 cm; máximo: 25.4 cm.
- Rutas terminadas: 400/400; una esquiva por corrida.
- Contactos: 0/400 (0 %).
- Terminadas sin contacto y con error menor de 20 cm: 344/400 (86 %).

B6 después:
- Error lateral absoluto medio: 1.9 cm.
- p50: 1.7 cm; p95: 4.6 cm; máximo: 6.6 cm.
- Rutas terminadas: 400/400, con esquiva.
- Contactos: 0/400 (0 %).
- Terminadas sin contacto y con error menor de 20 cm: 400/400 (100 %).

Recta sin caja, media/p50/p95 lateral: antes 16.2/16.3/27.0 cm; después 7.4/7.8/12.4 cm. Terminaron 400/400 antes y después, sin falsas esquivas.

Ruta en L sin caja, media/p50/p95 lateral al último tramo: antes 8.8/8.9/15.5 cm; después 2.6/2.3/6.0 cm. Terminaron 400/400 antes y después. Las pruebas existentes de rutas también pasaron. Estas estadísticas nuevas no miden la precisión longitudinal.

El escenario reproduce errores laterales de decenas de centímetros, pero no reproduce exactamente los 40 cm de B6-4 ni las esquivas múltiples de B6-1/2/3. No se inventaron errores adicionales para forzar ese resultado. La mejora simulada no demuestra la mejora real del carro.

5. Pruebas de software ya realizadas

Los siguientes son resultados de las ejecuciones anteriores al cierre documental. Se releyeron sus registros; no se volvieron a ejecutar por indicación del dueño.

- Núcleo: 21 programas, 1598 casos, 0 fallos. Conserva los 1577 casos anteriores y añade 21 de J15.
- Firmware: 152 + 246 + 15 + 34 = 447 casos, 0 fallos.
- Estrés de 2 hilos: 100000 comandos por mezcla, 0 STOP perdidos; comprobaciones de estado final correctas.
- Servidor falso: 46 casos, 0 fallos.
- Verificador de versiones: 13 pruebas, 0 fallos; las 10 anteriores más 3 de --solo-apk.
- Hash de la página del carro: correcto.
- Compilación Android: correcta; recompilación en la etapa 4 idéntica a la APK publicada.
- GitHub Actions sanitizadores: success en el último commit con código, 074c9fd. Incluye ThreadSanitizer y AddressSanitizer/UndefinedBehaviorSanitizer, también para J15.

Ejecución de CI: https://github.com/fernandosegrr/fsi_proyect/actions/runs/40613481109

6. Versiones y estado real de la instalación

APK instalada y verificada por lectura completa del archivo del teléfono:
APP:0.4-074c9fd; versionName 0.4; versionCode 3.
Archivo: dist/carro_fsi_0.4-074c9fd.apk.
Tamaño: 14,852,386 bytes.
SHA-256: 36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA.
adb install -r: Success, sin desinstalar ni borrar datos.
verificar_version.py --solo-apk --apk dist/carro_fsi_0.4-074c9fd.apk --revision HEAD: PASA; bytes instalados iguales a los publicados.

Firmware conservado, sin recompilar ni cargar en J15:
Versión esperada por el registro anterior: FW:453ed20-20261027.
SHA-256 de su copia local: 9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E.
La lectura actual por Sensores quedó PENDIENTE porque se usó la configuración 1B. No se presenta como verificada en el carro durante J15. El binario del firmware no se publica.

Durante el primer intento de verificación se restauró la pantalla antes de terminar la lectura y después se perdió ADB. Se recuperó al desconectar/reconectar el cable y confirmar la conexión; se completó la verificación sin reinstalar. Al cierre de la etapa 4 la app quedó cerrada por am force-stop, sin proceso, stay_on_while_plugged_in=2 y pantalla apagada, como al inicio. Batería final observada: 94 %, cargando por USB. No se cambió WiFi ni se escribieron PIN, contraseñas o cuentas. En el cierre documental no se volvió a operar el teléfono.

7. Resultados físicos y comparación con J14

J15: NO MEDIDO. Cero corridas con movimiento. No hay nuevas mediciones de contacto, desvío lateral, distancia recorrida, rumbo final, sensores ni ANT. El criterio de 5/6 no tiene resultado.

Referencia J14 B6: cuatro corridas con caja aproximadamente a 100 cm; la segunda probablemente tocó la caja y terminó en AYUDA 3/10; la cuarta evitó la caja pero quedó aproximadamente 40 cm a la derecha. Las corridas 1 y 3 hicieron dos esquivas y terminaron con rumbos estimados de 117/119 grados. Esos resultados históricos no se atribuyen a la APK nueva.

8. Límites y continuación con otro modelo

La distancia sigue calculada por tiempo, sin encoders ni referencia externa de posición. Un deslizamiento lateral sin giro no lo observa el giroscopio. Los cambios de pilas, piso, agarre y velocidad o errores del pivote pueden mantener diferencias entre la pose y el carro real. La simulación no reproduce el detector completo ni la percepción de objetos lejanos que disparó otras esquivas en J14. Se conserva también la alternativa de llegada al punto final sin una orientación final exacta cuando la geometría no permite ambas cosas.

Para continuar: verificar en Sensores el FW esperado y la APP indicada, ambos sensores, ANT y Autoridad del láser encendida; aprender el piso con el área despejada; luego hacer dos rectas de 200 cm sin caja y seis corridas de la misma recta con la caja de B6 a aproximadamente 100 cm. Registrar contacto, desvío real y lado, junto con POSE_DIAG, TURN_DIAG, ESQUIVE_DIAG, REINC_DIAG, fuente, tiempos de detección y paros por láser. Comparar la pose con la cinta, no solo con el resultado de la app.

La continuación necesita la configuración 2 de docs/guia_conexion_J15.md y la autorización de inicio correspondiente. Este reporte no inicia ninguna ruta. Se conservan las reglas de colocación/LISTO, parada, batería, 90 minutos y máximo de 20 corridas. No crear v1.1 hasta que se cumpla el criterio físico. El dueño realizará esa continuación con otro modelo; aquí se entrega únicamente el estado comprobado.

Antes de la demo: restablecer direcciones en Ajustes → Red, poner pilas nuevas, aprender el piso en el lugar y volver a poner el PIN al teléfono. La contraseña WiFi se conserva por decisión del dueño; no se modifica secretos.h ni se toca la copia antigua de OneDrive.

9. Publicación y revisión del paquete

Commits de implementación y preparación: 80bf494 (diagnóstico), 074c9fd (corrección y pruebas), f2d2693 (APK/guía) y a6e0a34 (instalación comprobada). El cierre posterior es exclusivamente documental. Se publica mediante push normal con identidad noreply.

dist/ publica solo la APK, SHA256SUMS.txt (solo esa APK), LEEME.txt y FIRMWARE_LOCAL.txt. Se conservan la exclusión de secretos.h/original_backup/ y la del firmware binario. Auditoría final de 367 archivos versionados: cero coincidencias reales de la contraseña y cuatro incidentales dentro de alfabetos hexadecimales, sin imprimir la clave; cero archivos mayores de 15,000,000 bytes y cero archivos prohibidos versionados. En los metadatos de todo el historial: cero blobs mayores de ese límite y cero identidades con correo personal.

Documentos de detalle: docs/progreso_J15.md, docs/guia_conexion_J15.md, docs/validacion_sensores.md, docs/limites_conocidos.md y docs/entrega.md.
