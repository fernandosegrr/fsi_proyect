MENSAJE 3 de 7 - PISOS NEGROS Y DE 2 COLORES. Guárdalo tal cual en docs/trabajos/lote4_3.md y responde solo "recibido 3/7". No ejecutes hasta EMPIEZA. Sigue docs/trabajos/reglas.md. Solo números, sin imágenes.

TRABAJO E. Problemas reales: (1) con piso negro el detector marca objeto aunque no haya; (2) con piso de 2 colores (visto en la universidad) marca uno de los colores como objeto. No hay piso de 2 colores en casa.

1. Reproduce en PC con cuadros sintéticos: piso negro con ruido (sigma 3 a 8), viñeteo y degradado de luz; piso de 2 colores en baldosas, franjas y damero, con color mezclado en el borde de las baldosas.
2. Hipótesis: en celdas oscuras la normalización de brillo amplifica el ruido de color; una referencia de un solo color deja el otro como obstáculo; la exposición automática cambia el brillo entre cuadros.
3. Arreglos: BRILLO_OSCURO (bajo ese brillo compara solo luminancia con diferencia mínima, sin croma); hasta K=4 grupos y un color intermedio entre dos grupos cuenta como piso (borde de baldosas); referencia de una tira inferior más ancha conservando grupos con apoyo de al menos 15 %; reaprendizaje suave si el brillo medio cambia de golpe.
4. Botón "Aprender piso" (pantalla de visión y antes de cada ruta): con el carro quieto, ~1 s, aprende los grupos de toda la zona inferior descartando los de poco apoyo. Muestra cuántos colores aprendió y su brillo medio, y avisa si el piso es muy oscuro o variado. La ruta exige haberlo aprendido (o confirmar el último). No cambia de piso a media ruta.
5. Pruebas en PC: mano, libreta, caja y mochila sobre cada piso se detectan; sombra suave y piso vacío no. Los tests de obstáculos (144) y de piso (57) siguen pasando.
6. Registro PISO_DIAG por cuadro: brillo medio, grupos, celdas marcadas y motivo.
7. PENDIENTE HARDWARE. Casa: piso de prueba con cartulinas, hojas o toallas de 2 colores en 1.5 m y otro tramo negro; aprender piso, pasar sin obstáculo (no frena) y con caja (frena). Universidad: aprender el piso real, 3 pasadas sin obstáculo y 3 con caja. Deja pasos y números a reportar.

Commit y resumen corto.
