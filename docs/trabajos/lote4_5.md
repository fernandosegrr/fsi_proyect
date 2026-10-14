MENSAJE 5 de 7 - FLUJO Y COMPARATIVA DE DETECTORES. Guárdalo tal cual en docs/trabajos/lote4_5.md y responde solo "recibido 5/7". No ejecutes hasta EMPIEZA. Sigue docs/trabajos/reglas.md. Ambos apagados por defecto; solo números, sin imágenes.

TRABAJO G - MOVIMIENTO DEL PISO (FLUJO).
1. movimiento_piso.c (C puro): desplazamiento entre cuadros consecutivos por bloques de 16x16, búsqueda en ventana, zona inferior. Con ALTURA_CAMARA_CM, INCLINACION_GRADOS, la velocidad del perfil AUTO y los ms entre cuadros, calcula el desplazamiento esperado de un punto del piso por fila. Un bloque con textura cuyo desplazamiento se aleja claramente del esperado (más cerca que el piso) es candidato a obstáculo. Bloque sin textura = "sin dato", nunca obstáculo. Solo evalúa si el carro avanza y los cuadros llegan a tiempo.
2. Pruebas en PC sintéticas: baldosas de 2 colores y damero desplazados según la geometría (sin obstáculo); caja con textura que se acerca (obstáculo); piso liso; piso negro con ruido.
3. En fusion.c, entrada opcional: el color manda; si el color marca obstáculo pero el flujo dice "piso con textura consistente" en esa zona y la cercanía es baja, baja la severidad. Registro FLUJO_DIAG y pasos PENDIENTE HARDWARE.

TRABAJO H - COMPARATIVA DE DETECTORES (investigación, 2 días máximo, en rama nueva, sin tocar lo que funciona). No inventes resultados; separa hechos comprobados de hipótesis; no afirmes que algo usa la NPU sin demostrarlo.
Contexto medido en el P40 Pro (Kirin 990, Android 10, sin servicios de Google): EfficientDet-Lite0 43 a 45 ms CPU 1 hilo, 149 ms GPU, 63 ms NNAPI. No repitas Lite0/1/2 (trabajo F).
1. Con el banco BenchmarkActivity mide un YOLO nano (YOLO11n o el más reciente disponible) y SSD MobileNetV2 en TFLite y, si hace falta, ONNX Runtime: tamaño, entrada, ms (CPU 1 y 4 hilos, GPU, NNAPI), memoria y temperatura tras 60 s. Registro DET_MS.
2. Licencia de cada modelo (Ultralytics es AGPL-3.0): qué implica para un repo público.
3. Huawei HiAI Foundation en el Kirin 990 sin servicios de Google: según la documentación oficial, qué SDK y conversión exige y qué operaciones soporta; prueba mínima que demuestre si algún modelo corre en la NPU.
4. Con el protocolo "cualquier objeto" (umbral 0.15) compara en las pasadas del termo, la caja y la mochila: porcentaje detectado antes de 40 cm y falsos por minuto en piso vacío.
5. Conclusión corta en docs/comparativa_detectores.md con enlaces oficiales: si algo supera a Lite0 sin pasar de ~100 ms, recomiéndalo; si no, dilo.

Un commit por trabajo y resumen corto.
