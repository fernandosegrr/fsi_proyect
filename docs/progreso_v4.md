# Bitácora v4 · 18 de noviembre de 2026

## Fase 0 · línea base reproducida, compuertas rojas

Partición congelada en `a03d8c5`, a partir de v3 `5ac174e`. [Plan de muestreo](precision/plan_v4.md) y [calibración y límites](precision/calibracion_v4.md). Sobre el banco histórico, el ejecutable actual reprodujo 849/43/75/33 en banquito y 435/21/529/15 en silla (ok/contacto/bajo/ayuda). [Salida del histórico de v3](precision/v4/historico_v3.txt).

Salida del banco calibrado: `11760 corridas. 86/98 combinaciones con alguna compuerta roja.` Incluye v1.1, v3 y árbol actual, 40 corridas por escenario y perfil. En DESARROLLO normal/pesimista hubo 1/560 y 53/560 contactos sólidos. En RESERVADO hubo 41/600 y 82/600, incluidos 40/40 contactos con el bloque sin eco en ambos perfiles. La línea base ya incumple la meta. No se ajustaron parámetros con el reservado. [Todas las filas y causas](precision/v4/fase0.txt).

Pruebas de C y firmware en PC sin fallos, servidor falso `60 casos, 0 fallos`, versión `Ran 21 tests`, J16 `46 casos, 0 fallos`. Comparación de precisión contra v1.1: `6 comparaciones, 4 fallos`. En caja con RUMBO, media/p95 de 17/40 a 18/44 mm. [Salida de los comandos](precision/v4/verificacion0.txt). Una consulta inicial de documentos con comodín para rg falló en Windows y se repitió sobre el directorio. Se corrigió asimismo la ruta de lectura de latencia a docs/latencia_modos.md. La consulta de un espejo de UM2039 falló y no se usa como fuente.

Decisión: investigar las causas observadas en desarrollo. Los dos perfiles del láser, el oráculo de fin y la geometría quedan fijos. Las medidas de distancia de frenado y ecos en patas siguen PENDIENTES.

## Fase 1 · diagnóstico y diseño

Se reprodujeron con semilla los contactos de caja lateral y banquito, la entrada bajo una silla y dos ayudas ante el arco de pivote. En los registros físicos se pudo atribuir un aborto a la pérdida de video. Falta evidencia para asignar las causas de las patas al carro. [Diseño y verificación física por idea](autonomo_v4_diseno.md).

Decisión de implementación: una etapa de parada por percepción incierta con una sola bandera experimental, que usa la máscara lateral existente y la edad del filtro de distancia cuando el firmware puede proporcionarla. Las capacidades antiguas deben seguir funcionando. Se posponen la recuperación automática contra pared y los cambios de exposición. Un primer filtro de las trazas falló por un conducto mal formado en PowerShell; la extracción corregida se guardó en el informe de trazas.

Salida repetida: `11760 corridas. 86/98 combinaciones con alguna compuerta roja.` El banco histórico completo, con las cuatro versiones a 1000 corridas por escena, volvió a dar 23 fallos; el resultado histórico de fase 0 con solo la actual tenía 12 porque omitía las comparaciones entre versiones. [Salida del banco histórico](precision/v4/historico_completo.txt), [banco calibrado de esta fase](precision/v4/fase1.txt) y [verificación](precision/v4/verificacion1.txt).
