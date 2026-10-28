# Límites, riesgos y trabajo futuro

La navegación depende de una estimación de movimiento y de sensores con cobertura limitada. Los resultados deben interpretarse dentro de las condiciones en que se registraron y con supervisión física del vehículo.

La distancia recorrida se calcula por tiempo. El estado de las pilas, la superficie, la carga y el deslizamiento pueden modificar el desplazamiento real. El giroscopio observa orientación, pero no identifica directamente un deslizamiento lateral. La cámara puede verse afectada por iluminación y sombras; el láser mira en una dirección limitada.

El circuito de mantenimiento de la powerbank en GPIO 2 está configurado en software, pero su carga y eficacia están **NO MEDIDAS**. Tampoco hay medición suficiente del consumo total, la autonomía o las corrientes de bloqueo. El balance AUTO requiere una medición específica; no debe tratarse como calibración física ya validada.

El trabajo futuro prioritario es medir la corrección J15 en un entorno controlado, comparar la posición real con la pose estimada y registrar contactos y desvíos. También deben medirse las corrientes del sistema, comprobarse el circuito de GPIO 2 y caracterizarse el balance AUTO. La repetición debe conservar las condiciones y versiones para que los resultados sean comparables.

Dónde está en el código: `navegador.c`, `config.h`, `tof.c`, `obstaculos.c`.
