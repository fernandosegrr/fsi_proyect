J13 — Reporte de trabajo desatendido

Estado previo a carga: fases 1 a 5 terminadas en PC, sin movimientos al hardware.
Base c6b4fc5. Commits: fase 1 b0d118b; fase 2 7be39df; fase 3 5085c6d; fase 4 e60ec35; fase 5, commit que contiene este reporte inicial.

Cambios:
- Calibración: ocho giros, calentamiento excluido, descarte controlado, pares disponibles incluso con error o cancelación.
- Anticipación: AUTO inicial 100 ms, tope 250 ms; mediana de cinco y NVS conservados. Asentamiento de 750 ms con motores apagados para medir la inercia completa.
- Giroscopio: reintenta motivo 4 detenido cada 5 s, contador IMUR e historial NVS de cinco arranques. Sin sesgo niega el giro por ángulo y la navegación cae a tiempo.
- Versiones: FW con hash/fecha, APK 0.4 con hash, ambas visibles en Sensores; script de comparación sin instalación. Cámara de prueba temporal con aviso; apertura normal vuelve a la dirección real.
- Autoridad láser: interruptor apagado por defecto, 400 mm en columnas 3..6, cámara elige lado y respalda lecturas inválidas. Solo probado en PC. Freno firmware conservado a 120 mm.

Pruebas: núcleo 20 programas / 1570 casos (antes 1540), firmware 152+241+15+34 = 442 (antes 425), servidor falso 42, verificador de versiones siete. Todas verdes, estrés 100000 comandos sin STOP perdido y hash de página correcto. APK y firmware compilan. Sanitizadores: pendientes del workflow tras push.

Fase 6: pendiente de push, compilación del HEAD, carga, instalación y verificación sin movimiento.
Versión cargada y APK instalada: aún no se han cambiado los dispositivos durante J13.
Estado final de dispositivos: pendiente del cierre. El dueño pide apagar completamente el teléfono y cortar energía del DevKit si es posible, con reset como alternativa.

Pendiente físico, en orden:
1. Arranques quietos/movidos: recuperación del sesgo y contadores.
2. Calibración con ocho giros, reposicionando el carro entre tandas; revisar pares, descarte, repetibilidad y Deshacer.
3. Anticipación: aprendizaje y diez giros por lado con cinta, incluidos primeros tras reposo.
4. LOOP con movimiento y tras recuperación; rutas en L, caja y pérdida de WiFi.
5. Autoridad láser: comparar apagado/encendido en carril central, laterales y objetos fuera del haz; mantener apagada hasta esta validación.

Fuera de alcance: J10c no se corrigió; persiste la regla que acepta una franja casi del ancho completo como segundo color de piso y puede confundir un obstáculo ancho. El modelo de objetos sigue apagado por defecto.

Al volver: consultar la sección final de este reporte y docs/progreso_J13.md antes de conectar motores. No ejecutar giros ni rutas sin colocar y despejar el carro.
