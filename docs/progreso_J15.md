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
