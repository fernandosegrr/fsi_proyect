MENSAJE 3 de 7 - PARTE 2. Guárdalo en docs/trabajos/parte_2.md y responde "recibido 3/7". No ejecutes hasta EMPIEZA.

Objetivo: calibración experimental del carro (calibracion.c, CalibracionCarro), sin encoders.

Mide y guarda en el teléfono (SharedPreferences, exportable a texto):
1. ARRANQUE: retardo entre ordenar avance y que el carro realmente se mueve (ms).
2. MS_POR_CM: la app manda avance X ms (1000, 2000...), el usuario escribe cuántos cm avanzó; promedio de 3 repeticiones. Guarda también el frenado (cm extra tras STOP).
3. MS_POR_GRADO por lado: izquierda y derecha por separado, porque los pivotes de un solo motor no son simétricos. La app gira T ms, el usuario escribe los grados aproximados (con marcas de 90 grados en el piso).
4. CalibracionActivity: asistente paso a paso con textos sencillos en español: un botón por prueba, campo para el resultado, botón repetir, resumen y botón "usar estos valores".
5. calibracion.c con funciones puras probadas en PC: promedio, rechazo de datos atípicos, conversión cm->ms y grados->ms que considere arranque, frenado y lado de giro. El navegador usa estos valores en lugar de MS_POR_CM y MS_POR_GRADO fijos. Si no se calibró, usa valores por defecto con aviso en pantalla.
6. docs/calibracion.md: procedimiento para el equipo (qué medir y cómo).
7. Pruebas en PC con datos simulados con ruido de más/menos 10 por ciento.

Marca PENDIENTE HARDWARE lo que requiera el carro. Commit y resumen corto.
