# Límites, riesgos y trabajo futuro

La navegación depende de una estimación de movimiento y de sensores con cobertura limitada. Los resultados deben interpretarse dentro de las condiciones en que se registraron y con supervisión física del vehículo.

La distancia recorrida se calcula por tiempo. El estado de las pilas, la superficie, la carga y el deslizamiento pueden modificar el desplazamiento real. El giroscopio observa orientación, pero no identifica directamente un deslizamiento lateral. La cámara puede verse afectada por iluminación y sombras; el láser mira en una dirección limitada.

El circuito de mantenimiento de la powerbank en GPIO 2 está configurado en software, pero su carga y eficacia están **NO MEDIDAS**. Tampoco hay medición suficiente del consumo total, la autonomía o las corrientes de bloqueo. El balance AUTO requiere una medición específica; no debe tratarse como calibración física ya validada.

El trabajo futuro prioritario es medir la corrección J15 en un entorno controlado, comparar la posición real con la pose estimada y registrar contactos y desvíos. También deben medirse las corrientes del sistema, comprobarse el circuito de GPIO 2 y caracterizarse el balance AUTO. La repetición debe conservar las condiciones y versiones para que los resultados sean comparables.

Dónde está en el código: `navegador.c`, `config.h`, `tof.c`, `obstaculos.c`.

## Alcance del entorno de operación

El sistema debe usarse en un espacio que el operador pueda observar y despejar. La cámara y el sensor frontal no sustituyen una inspección del recorrido. Objetos bajos, transparentes, estrechos o situados fuera de la zona observada pueden no generar una señal útil en las condiciones reales; su detección no debe darse por garantizada sin una prueba específica.

La iluminación puede alterar la imagen y, con ella, la clasificación de regiones. Un cambio en el color o la textura del piso también puede modificar la interpretación del entorno. La respuesta práctica es verificar el comportamiento en el entorno concreto y detener la operación cuando la información visual deja de ser confiable.

## Precisión y repetibilidad

La calibración de tiempo por distancia relaciona una orden con un desplazamiento observado bajo determinadas condiciones. Si cambian las pilas, la carga o la superficie, la misma duración puede producir otra distancia. Por ello un valor de calibración no debe interpretarse como una garantía universal de exactitud.

El giro estimado por giroscopio también depende de las condiciones de medición y del sesgo del sensor. Una calibración hecha mientras el vehículo se mueve puede contaminar la referencia. Incluso con un rumbo razonable, el carro puede desplazarse lateralmente por deslizamiento o por diferencias entre ruedas.

## Evidencia pendiente y prioridad

Para J15, el siguiente paso útil es repetir físicamente la maniobra con obstáculos, ruta y condiciones registradas, midiendo el desvío final y anotando contactos. La comparación debe identificar versión y configuración, porque una ejecución de una revisión anterior no valida una corrección posterior. Hasta disponer de esas corridas, el resultado físico continúa **NO MEDIDO**.

Para el balance AUTO se necesita una prueba física específica; el ajuste registrado para MANUAL no demuestra el comportamiento de AUTO. Para el consumo conviene distinguir alimentación de placas, motores en movimiento y situaciones de carga, siempre con instrumentación adecuada. Sin datos de corriente y capacidad utilizable no se calcula una autonomía confiable.

La función del GPIO 2 debe revisarse en el circuito real y medirse antes de afirmar que mantiene encendida una batería portátil. Una configuración en el código documenta la intención de generar una señal; no demuestra por sí sola la corriente, el efecto en la batería ni la seguridad del montaje concreto.

## Cambios futuros y regresiones

Cuando se modifique una regla de navegación, conviene repetir las pruebas de software relacionadas y comparar las salidas con las de la revisión anterior. Si el cambio altera el movimiento físico, las pruebas en computadora son una primera barrera, no la última: después se requiere una prueba controlada en el carro con una persona preparada para detenerlo.

Una mejora en una métrica no confirma automáticamente mejoras en todas las demás. Por ejemplo, reducir el error lateral simulado no mide consumo, autonomía, cobertura de cámara ni confiabilidad del enlace inalámbrico. Cada afirmación debe acompañarse de su propia evidencia.
