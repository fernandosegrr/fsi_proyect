MENSAJE 4 de 7 - MODELO DE OBJETOS SIN RECONOCER. Guárdalo tal cual en docs/trabajos/lote4_4.md y responde solo "recibido 4/7". No ejecutes hasta EMPIEZA. Sigue docs/trabajos/reglas.md. Apagado por defecto; solo números, sin imágenes.

TRABAJO F. Usar EfficientDet solo para saber que hay una caja sobre el piso, ignorando la clase.
1. Modo "cualquier objeto": toma las detecciones con confianza mayor a UMBRAL_CUALQUIERA (por defecto 0.15), ignora la clase y aplica filtros geométricos configurables: descartar cajas que cubran más del 60 % del cuadro, con el borde inferior arriba de la mitad de la imagen o muy pequeñas; aceptar solo las que tocan la zona inferior. Conviértelas a zona y cercanía (posición vertical del borde inferior) con la misma salida que el detector y compatibles con el corredor del trabajo B.
2. Persistencia: una caja cuenta si aparece en el mismo sitio (superposición mayor al 50 %) en N cuadros seguidos (PERSISTENCIA_CUADROS, por defecto 2; con cercanía alta basta 1).
3. Control del umbral en la pantalla de visión, de 0.01 a 0.50.
4. Fusión en fusion.c: el color manda en seguridad. Esta señal sube la severidad si coincide con el color y solo puede desmentir un falso positivo de color con cercanía baja. Regla escrita y probada.
5. Pruebas en PC con salidas simuladas: caja del termo con 0.20 (cuenta), caja que cubre todo con 0.43 (ignorar), caja arriba (ignorar), cajas que parpadean (no cuentan) y estables (cuentan), varias, ninguna.
6. Benchmark en el Huawei de EfficientDet-Lite0, Lite1 y Lite2 (CPU 1 hilo, GPU y NNAPI): ms y temperatura tras 60 s. Descárgalos de fuente oficial, anota licencia en docs/modelo_objetos.md, no subas archivos de más de 15 MB.
7. Registro OBJ_ANY por cuadro: cuántas cajas pasan filtros y persistencia con cada umbral 0.01, 0.05, 0.10, 0.15, 0.20 y 0.30, con zona, cercanía, ms del modelo y lo que decidió el color. Script en scripts/ que lea el log por adb y calcule por umbral y tipo de pasada: falsos obstáculos por minuto y porcentaje de pasadas donde se detecta el objeto antes de 40 cm; tabla en texto en docs/modelo_objetos.md.
8. PENDIENTE HARDWARE: pasadas sin obstáculo y con termo, caja y mochila a 3 distancias sobre piso liso, negro y de 2 colores.

Commit y resumen corto.
