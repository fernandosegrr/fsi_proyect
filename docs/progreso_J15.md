J15 — Reincorporación después de esquivar

27 de octubre de 2026, hora de Ciudad de México.

Fase 1. Diagnóstico sin hardware.

Auditoría: main y origin/main en a9c71a2; árbol limpio al iniciar. v1.0 sigue en eb88151. La rama local j14-wip-20261027 (af0bfb5) queda intacta y no se publica. El disco que el encargo llama D: está montado como E:; se trabaja en E:\Dev\fsi_proyecto. La copia vieja de OneDrive solo se leyó para localizar el repositorio y no se modificó. Identidad de commits: noreply.

Leídos README, entrega, reporte y progreso J14, validación de sensores con movimiento, límites, bitácora y guía; contrastados navegador.c/.h, calibración, simulador, corredor, fusión y alimentación de sensores desde ModoSemiautonomo.kt/JNI.

Secuencia B6 conservada en docs/progreso_J14.md y reporte_J14.md:
1. 14:33, autoridad apagada: 23 s, 2 esquivas, sin toque, pose estimada (394,5,117°).
2. 14:35, autoridad apagada: 56 s, 3 esquivas, probable toque, AYUDA, punto 3/10, pose (659,780,306°).
3. 14:38, autoridad encendida: 26 s, 2 esquivas, sin toque, pose (393,-12,119°).
4. 14:40, autoridad encendida: 15 s, 1 esquiva, sin toque, pose (409,6,0°); dueño midió aproximadamente 40 cm a la derecha de la cinta. La línea está en x=400 mm: el estimador declara solo 9 mm de error lateral.
La caja estuvo aproximadamente a 100 cm en las cuatro. A 40 cm no arrancó por aprendizaje de piso. Los documentos no conservan el lado, cada ángulo, el tramo lateral ni los eventos completos de cada corrida. No hay logcat B6 en docs/logs, en registros_fsi_j12 ni entre los archivos temporales J14 encontrados. Esos datos quedan NO REGISTRADOS; no se deducen del lado del error final.

Funcionamiento comprobado:
- elegir_lado elige el lado libre con menos celdas; en empate prefiere izquierda. El primer giro es de 90°. esq_calcular_tramo busca el margen lateral NAV_ESQ_MARGEN_LATERAL_MM descontando ambos pivotes; sin pivote usa 40 cm. Luego se endereza y pasa en paralelo la cara estimada/medida, 25 cm de fondo y 10 cm de margen.
- reinc_calcular proyecta la pose sobre la polilínea y busca un vértice unos 30 cm adelante. planear_vuelta incluye los dos pivotes y su inversa. La red de seguridad compara posición estimada contra plan; no mide la cinta.
- giro_fw_empezar también cubre ESQUIVANDO y REINCORPORANDO: con giroscopio sano, de 5 a 200 grados, pide GIRAR. Menos de 5, más de 200, o fallo de firmware, usan tiempo. No es cierto que todos los giros de esquive sean temporizados.
- calcular_pose mueve cada avance por tiempo usando base_rumbo_grados constante. SENS guarda el ángulo, pero solo se aprovecha para resolver giros: no actualiza el rumbo ni la trayectoria durante una recta. La distancia tampoco tiene referencia externa.
- AYUDA se activa por lados bloqueados, máximo de 3 esquivas, reintentos agotados, geometría o frenos de coherencia. 3/10 es el progreso por puntos de la ruta; no significa tres de diez errores. Sin el diagnóstico de la corrida 2 no se conoce su causa concreta.

Causas priorizadas y cómo descartarlas:
1. Pérdida del rumbo real durante avances (defecto confirmado de la estimación; contribución exacta a B6 todavía hipótesis). B4 mide 5.5 a 20 cm por 150 cm, con derecha explícita en la primera corrida. B6-4 estima 0.9 cm frente a unos 40 cm reales. El navegador integra avances con rumbo fijo aunque recibe SENS. Descartar comparando SENS, pose integrada y cinta; un deslizamiento lateral sin rotación no se observa con giroscopio.
2. Calibración de velocidad/pivote distinta de la efectiva (hipótesis). El modelo ya incluye pivotes asimétricos; el real mide derecha 150/165 mm e izquierda 90/-90 mm a 90°. Hay que conservar esos valores y variar su error en simulación. B4 declara 150 cm en 5 s frente a los 19.3 cm/s históricos AUTO: inconsistencia que no permite afirmar velocidad/calibración efectiva de J14. Descartar leyendo parámetros y midiendo otra recta antes de pruebas con caja.
3. Reincorporación cerrada solo sobre la pose estimada (limitación confirmada). Volver a planear o hacer una segunda alineación usando el mismo error invisible no lo elimina. Además, al final hay una alternativa que admite llegar sin orientación final (explica la posibilidad de 117°/119°, no demuestra la secuencia de B6). Descartar con simulación de mundo independiente y registro de cada plan.
4. Repetición del esquive por percepción al volver (hecho en B6-1/2/3; causa perceptiva hipótesis). Se contaron 2/3/2 esquivas; el dueño observó detección lejana durante la vuelta. El código sí reintenta la reincorporación si se libera y vuelve a esquivar si persiste; AYUDA no reanuda automáticamente. Descartar con ESQUIVE_DIAG, fuente y cuadro numérico, sin imágenes reales.
5. Latencia y frenado (limitación medida, contribución hipótesis). Video aproximadamente 200–300 ms más cuadros/tick; parada total 9–14 cm AUTO. Distancia mecánica de frenado separada de la latencia: no sumarla dos veces. Variar latencia en el escenario; registrar detección, STOP/TOF_PARO y desplazamiento efectivo.
6. Giros por tiempo o datos del giroscopio perdidos (hipótesis secundaria). 28/28 giros de verificación J14 dentro de ±3° y cierre por ángulo ya implementado. No se dispone de TURN_DIAG de B6 para excluir fallback. Descartar contando por=angulo/tiempo y comprobando giro_fw_caido; no cambiar firmware sin evidencia.

Pruebas de base ejecutadas sin hardware: núcleo y firmware en verde; se guardan los registros completos temporalmente. No se ha accedido al teléfono ni a placas, ni enviado movimiento. La Autoridad del láser conserva su valor por defecto encendido en la app.

Fase 2. Reproducción y corrección sin hardware.

Se añadió pruebas_reincorporacion_j15.c al make test del núcleo. Ejecuta 400 semillas (15001 a 15400) por escenario y versión: 2400 rutas en total (B6, recta sin caja, L sin caja; antes y después). El modo antes usa sin_rumbo_medido, un control de ablación solo en C como los controles de geometría que ya existían. La posición física la mueve simulador_navegador.c; nunca se sustituye por la pose que calcula el navegador.

Modelo y supuestos explícitos:
- Recta de 2000 mm; caja de 200 x 200 mm con centro a 1000 mm del centro inicial; chasis rectangular de 200 x 240 mm. Las dimensiones son supuestos para esta simulación: falta medir la misma caja y el chasis en la prueba real. La distancia de 1 m es entre centros, no una medición exacta desde el parachoques.
- AUTO 19.3 cm/s (518 en ms_por_cm_x10), arranque supuesto de 100 ms; giros 7.4/7.8 ms por grado. Error de cierre normal aproximado por suma de 12 uniformes, media cero, sigma 1.1° izquierda/1.7° derecha y límite ±3°. El ángulo informado incluye el error físico: no se inventa un giroscopio perfecto en el estimador y uno errado en el mundo.
- Curvatura hacia la derecha: desbalance residual de 1006 a 1018 milésimas. En la recta de 150 cm equivale aproximadamente a un rumbo medio de 2° a 6°; en 2 m el error lateral antiguo llega a 27.2 cm. Se eligió derecha porque B4-1 lo especifica. No se presume conocido el lado de las otras observaciones sin signo.
- Pivotes nominales medidos: izquierda 90/-90 mm, derecha 150/165 mm a 90°, convertidos con cal_desplazamiento_de_medida (las magnitudes internas están en el marco de mitad de giro). Variación por corrida ±5 %: supuesto de sensibilidad, no dispersión medida.
- Video a 10 cuadros/s, cola de poses de 200–300 ms; proyección de la caja con sr_vista, aproximación por zonas del simulador existente (no reproduce las imágenes ni EfficientDet). Láser calculado por rayo independiente contra la caja, rango 1200 mm y autoridad central de 400 mm encendida. SENS cada 100 ms; tick de 50 ms.
- Frenado mecánico de 25–35 mm, separado de la latencia y la confirmación. Los 9–14 cm observados son la parada total desde detección visual y no se suman otra vez como deslizamiento. Con decisión del láser la latencia de parada puede ser menor.
- Contacto comprobado cada 50 ms con cuatro ejes separadores de los rectángulos; a AUTO hay menos de 1 cm entre muestras. Sin perturbaciones súbitas ni deslizamiento lateral puro: esos casos no están medidos y el giroscopio no los observa.

Línea base B6: 400/400 terminadas, una esquiva cada una, 0/400 toques (0 %), error lateral absoluto medio 132 mm, p50 135 mm, p95 219 mm, máximo 254 mm; 344/400 (86 %) terminan sin toque y a menos de 200 mm. Reproduce decenas de centímetros y el error oculto de la estimación; NO reproduce exactamente los 40 cm de B6-4 ni las esquivas múltiples de B6-1/2/3. No se ajustaron errores sin medir para forzar esa cifra. La atribución exacta de los 40 cm sigue pendiente de registros físicos.

Corrección elegida: conservar los planes de pivote y los giros GIRAR existentes, y orientar cada incremento de distancia por el rumbo SENS relativo al inicio del movimiento. Se integra cada incremento, sin rotar retroactivamente el recorrido, con residuos enteros en milésimas de mm. El frenado se proyecta con el último rumbo. La reincorporación y los planes que ya existen reciben así la desviación observada; no requieren otro planificador ni nuevas maniobras. Se reutiliza el interruptor de giroscopio de la app; apagado conserva el cálculo anterior. No se añade lazo BAL ni un segundo intento nuevo: la primera opción ya cumple el objetivo del escenario.

Se comprueban vigencia de SENS, bit IMU lista y ausencia de calibración. Lectura atrasada: se conserva el último rumbo conocido; lectura caída: se recupera la referencia sin salto al volver; salto de más de 25° en un avance: se descarta como referencia incoherente. Es una estimación, no una medición de la cinta. POSE_DIAG registra pose, rumbo inicial, validez/edad de IMU y método al cerrar cada avance, para contrastarlos con las observaciones del dueño.

Resultado B6 corregido, mismas semillas y parámetros de mundo: 400/400 terminadas con esquiva, 0/400 toques (0 %), media 19 mm, p50 17 mm, p95 46 mm, máximo 66 mm; 400/400 (100 %) sin toque y a menos de 200 mm. PASA el objetivo simulado de al menos 90 %; el objetivo físico de 5/6 aún NO MEDIDO.
Recta 200 cm sin caja: media/p50/p95 antes 162/163/270 mm, después 74/78/124 mm; 400/400 terminadas antes y después, sin falsas esquivas.
L sin caja (100 cm + derecha + 100 cm): media/p50/p95 lateral al último tramo antes 88/89/155 mm, después 26/23/60 mm; 400/400 terminadas antes y después. Estas cifras no son precisión longitudinal ni validación física.

Regresiones nuevas: 21 casos en un programa adicional (integración por incrementos, signo y cero relativo, apagado del giroscopio, IMU inválida/calibrando y recuperación, salto, lectura atrasada, cruce de 360°, cancelación, Monte Carlo y no degradación lateral de recta/L). Los 20 programas anteriores conservan sus 1577 casos. Total del núcleo: 21 programas / 1598 casos. Firmware: 447 casos, estrés de 100000 comandos por mezcla y cero STOP perdidos; servidor falso: 46. El verificador conserva sus 10 pruebas y agrega 3 para --solo-apk (13): permite comprobar la configuración 1B sin abrir puerto serie y declara explícitamente FW pendiente; no acepta confundir ese resultado con una comprobación del firmware.

No cambió firmware_carro: no se recompila ni se carga firmware en J15. FW esperado: FW:453ed20-20261027, SHA-256 histórico 9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E. La alternativa en app/núcleo usa los SENS y GIRAR ya disponibles y evita una carga innecesaria. APK de prueba compilada correctamente con Java por ruta absoluta, sin modificar PATH ni JAVA_HOME. La entrega se recompila desde el commit limpio de esta fase para que APP identifique sus fuentes; la versión y SHA finales se anotan en la fase 3. Se extendió la acción de sanitizadores a las nuevas pruebas J15.

Fase 3. Guía de conexión y parada; espera antes de hardware.

Fuentes y pruebas publicadas mediante push normal: fase 1 en 80bf494 y fase 2 en 074c9fd. GitHub Actions sanitizadores, ejecución 13 del commit 074c9fdba29789b1f21cb311dc280eba6f12d76c, terminó en success: https://github.com/fernandosegrr/fsi_proyect/actions/runs/40613481109 . Incluye la nueva prueba de reincorporación con AddressSanitizer/UndefinedBehaviorSanitizer.

Entrega compilada desde 074c9fd con fuentes limpias: APP:0.4-074c9fd, versionName 0.4, versionCode 3, 14,852,386 bytes. SHA-256: 36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA. dist/SHA256SUMS.txt incluye solamente esa APK; nombre, hash y assets/version_app.txt comprobados. El APK J14 se conserva en el historial y v1.0; dist/ contiene ahora el candidato J15, pendiente de instalación y validación física. LEEME declara ese estado. No se presenta como entrega físicamente validada.

FW sin cambios: FW:453ed20-20261027. El SHA-256 de la copia local coincide con 9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E. No se versiona el binario y no se necesita USB del DevKit. Guía en docs/guia_conexion_J15.md: configuración 1B (solo teléfono, WiFi normal) y configuración 2 (carro sin USB, pilas/powerbanks, área despejada, teléfono en CarroFSI y dueño presente), con parada y límites del encargo.

Última comprobación completa de fuentes: núcleo 21 programas/1598 casos, firmware 447, servidor falso 46 y verificador 13; cero fallos. Estrés de 2 hilos: 100000 comandos en cada mezcla, cero STOP perdidos; hash de página correcto. Compilación final de la APK correcta. No hubo fases con tres intentos fallidos ni reversiones.

Revisión previa a publicación: cero coincidencias reales de la contraseña local en archivos versionados; se descartan cuatro apariciones incidentales dentro de alfabetos hexadecimales (incluido el APK). Cero archivos mayores de 15,000,000 bytes; cero firmware .bin, secretos.h u original_backup versionados; cero correos personales en las identidades del historial. No se imprime la contraseña. v1.0 permanece intacta; v1.1 no se crea antes de validar físicamente.

Sin acceso al teléfono ni a placas durante las fases 1–3. No se cambió pantalla/stay-on/WiFi ni se envió movimiento. La app instalada y el FW real se verificarán únicamente en las etapas autorizadas. El siguiente paso exige el mensaje CONECTADO 1 del dueño; después, instalación sin movimiento y espera de EMPIEZA B. No ejecutar anticipadamente las etapas 4–6.

Fase 4. Instalación sin movimiento, autorizada por CONECTADO 1.

Configuración 1B: teléfono Huawei ELS-NX9, un solo dispositivo ADB autorizado, batería 91 % y cargando por USB. Antes de operarlo se guardaron stay_on_while_plugged_in=2 y pantalla Asleep/OFF en el archivo temporal j15_etapa4_estado.json. No se conectó ni se cargó firmware; la lectura de FW sigue pendiente para Sensores en la etapa 5.

Recompilación de 074c9fd correcta, sin modificar variables globales ni fuentes. Se seleccionó temporalmente esa revisión limpia para conservar APP:0.4-074c9fd y se volvió a main. El APK recompilado coincide exactamente con dist/: 14,852,386 bytes, SHA-256 36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA. adb install -r devolvió Success; sin desinstalar, borrar datos ni resolver diálogos. No se introdujeron PIN, contraseñas ni cuentas.

Incidente de verificación: verificar_version.py seguía leyendo cuando se restauraron los ajustes de pantalla; después ADB desapareció y la lectura no fue un ZIP completo. Ese intento no pasó. Windows seguía enumerando el Huawei y su interfaz ADB, pero adb devices quedó vacío. Tras la primera confirmación del dueño seguía vacío; reconnect offline y reinicio del servidor ADB exacto (puerto 5037) no lo recuperaron. Se pidió desconectar/reconectar el cable, desbloquear y confirmar Transferir archivos/permisos, sin reinstalar.

Tras el segundo LISTO del dueño, ADB recuperado. Se mantuvo la pantalla encendida hasta completar la lectura. scripts/verificar_version.py --solo-apk --apk dist/carro_fsi_0.4-074c9fd.apk --revision HEAD: PASA; APP:0.4-074c9fd, versionName 0.4, versionCode 3, APK instalada idéntica byte por byte al build publicado y fuentes versionadas limpias (solo este registro documental estaba pendiente). Por tanto su SHA-256 es 36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA. El verificador declaró FW PENDIENTE, como corresponde a 1B.

Cierre de etapa 4 comprobado: app cerrada con am force-stop y sin proceso; stay-on restaurado a 2 y pantalla Asleep/OFF, igual al inicio. No se cambió WiFi ni se mandó movimiento. No se leyeron ni guardaron imágenes de cámara, ni se necesitó ningún diálogo de instalación. La etapa termina y espera EMPIEZA B tras cambiar a configuración 2; la prueba física todavía NO MEDIDA.
