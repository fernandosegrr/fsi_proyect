# Bitácora v4 · 18 de noviembre de 2026

## Fase 0 · línea base reproducida, compuertas rojas

Partición congelada en `a03d8c5`, a partir de v3 `5ac174e`. [Plan de muestreo](precision/plan_v4.md) y [calibración y límites](precision/calibracion_v4.md). Sobre el banco histórico, el ejecutable actual reprodujo 849/43/75/33 en banquito y 435/21/529/15 en silla (ok/contacto/bajo/ayuda). [Salida del histórico de v3](precision/v4/historico_v3.txt).

Salida del banco calibrado: `11760 corridas. 86/98 combinaciones con alguna compuerta roja.` Incluye v1.1, v3 y árbol actual, 40 corridas por escenario y perfil. En DESARROLLO normal/pesimista hubo 1/560 y 53/560 contactos sólidos. En RESERVADO hubo 41/600 y 82/600, incluidos 40/40 contactos con el bloque sin eco en ambos perfiles. La línea base ya incumple la meta. No se ajustaron parámetros con el reservado. [Todas las filas y causas](precision/v4/fase0.txt).

Pruebas de C y firmware en PC sin fallos, servidor falso `60 casos, 0 fallos`, versión `Ran 21 tests`, J16 `46 casos, 0 fallos`. Comparación de precisión contra v1.1: `6 comparaciones, 4 fallos`. En caja con RUMBO, media/p95 de 17/40 a 18/44 mm. [Salida de los comandos](precision/v4/verificacion0.txt). Una consulta inicial de documentos con comodín para rg falló en Windows y se repitió sobre el directorio. Se corrigió asimismo la ruta de lectura de latencia a docs/latencia_modos.md. La consulta de un espejo de UM2039 falló y no se usa como fuente.

Decisión: investigar las causas observadas en desarrollo. Los dos perfiles del láser, el oráculo de fin y la geometría quedan fijos. Las medidas de distancia de frenado y ecos en patas siguen PENDIENTES.
