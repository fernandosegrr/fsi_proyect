TRABAJO J5 - CALIBRACION POR PARTES Y DIAGNOSTICO DE RUTA. Mismas reglas que ajustes_4.md.

1. CALIBRACION POR PARTES. Hoy el usuario siente que para calibrar una cosa debe repetir todas. Reorganiza Más → Calibración como lista de ítems independientes, cada uno con su valor actual, fecha y hora de la última calibración y un botón "Calibrar solo esto": velocidad de avance, frenado de avance, giro izquierda, giro derecha, desplazamiento al girar, ajuste fino de giro de 90° y ajuste fino de recta (balance). Cada prueba es independiente: no exige haber hecho otra antes, no borra los otros valores y guarda al terminar; una prueba cancelada no cambia nada. Cada ítem avisa si depende de las pilas ("repite si cambiaste pilas"). Opcional: "Calibrar todo, en orden".

2. DURACION DE LA PRUEBA DE GIRO. En las pruebas de giro deja elegir el giro de prueba: "Giro de 90° real" (por defecto), 180° o duración libre (500 a 1500 ms). Los valores por grado se calculan con los grados que el usuario indique que giró. No mezcles una calibración hecha a una duración con otra sin avisar.

3. RUTA QUE NO GIRO (UNA VEZ DE TRES). En la prueba real, ruta de 40 cm a la derecha con 1 giro: giró bien 2 de 3 veces; en una avanzó recto. Probablemente es contacto eléctrico intermitente (hardware); NO rediseñes el navegador. Haz solo esto: (a) simula esa ruta en C y confirma que el primer comando es el giro de 90° con el tiempo esperado; (b) agrega el log RUTA_DIAG con cada comando enviado, su duración planeada y la real, y en la pantalla de resultado "Giro pedido / Giro hecho (estimado)"; (c) al guardar una calibración, rechaza ms/° fuera de 3 a 20 y avisa; (d) si el log muestra que el comando de giro fue correcto, déjalo en docs/bitacora.md como "no software".

4. Documenta en docs/calibracion.md.
