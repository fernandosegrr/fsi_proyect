# Partición previa a los ensayos de v4 · 18 de noviembre

Se congela antes de ejecutar el banco o cambiar el navegador. Unidad de comparación: misma geometría, semilla inicial y modelo de sensor para cada versión. Las acciones diferentes pueden producir observaciones diferentes. Las cifras son SIMULADAS.

- Referencia histórica: 23 escenas originales, semillas desde 170000, física v3 sin modificar. Monte Carlo J16 con 1000 repeticiones.
- DESARROLLO: 23 familias originales, semillas 1200000 + índice × 10007 + repetición.
- RESERVADO: semillas 3200000 + índice × 10007 + repetición. Incluye las mismas familias con nuevas posiciones y tamaños de patas, pasillos de otros anchos y además dos muebles sucesivos, mueble junto a pared y bloque sin retorno. Solo se abre al cierre de cada etapa. No se eligen parámetros a partir de sus resultados.
- En ambos conjuntos se ejecutan por separado el modelo de eco mezclado y su variante pesimista (más pérdidas y mayor señal mínima). Muestreo inicial fijado en 40 repeticiones por familia y perfil. Cero en esta muestra no certifica riesgo físico cero. Las series históricas de 1000 conservan sus denominadores originales.
- Comparar contra los núcleos 5ac174e y bd1727b en el mismo banco. Revisar todas las categorías excluyentes, además de contactos, ayudas y falsos finales incluso si se superponen, paradas, precisión y tiempos. Toda regresión queda visible. Las mejoras experimentales con alguna compuerta roja quedan apagadas.
- Variar velocidad alrededor de las rectas con cinta, latencia de entrega, cadencia de cámara, pivotes asimétricos, giro, frenado, luz, ruido y pérdida de señal. Documentar por separado los intervalos medidos y los supuestos. La prueba física se reserva para otra sesión.
