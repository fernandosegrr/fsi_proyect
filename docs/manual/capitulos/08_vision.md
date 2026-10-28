# Visión y percepción

La percepción ayuda a reconocer obstáculos que pueden interferir con una ruta. El sistema combina señales de color y de un modelo de objetos con información de distancia frontal; cada fuente tiene limitaciones propias.

## Color, objetos y corredor

El detector de color aprende una referencia del piso y analiza celdas de la imagen que se apartan de ella. Sombras, reflejos o cambios de superficie pueden afectar el resultado. El modelo de objetos usa una red neuronal para identificar objetos en imágenes; puede omitirlos o generar detecciones erróneas, por lo que no sustituye la comprobación del entorno.

El corredor es la zona proyectada de la imagen por la que se espera que pase el carro. Su forma depende de la perspectiva y de los parámetros de montaje. Permite distinguir obstáculos que invaden el camino de los que aparecen a un lado. La regla básica de fusión conserva la señal de seguridad del detector de color; otras etapas opcionales pueden modificar el resultado.

## Distancia frontal

El láser VL53L0X aporta una medición de distancia en la dirección a la que apunta. Su campo es limitado: no identifica por sí solo todos los objetos laterales, desniveles o huecos. Cuando su autoridad está activa y se cumplen las condiciones del firmware, puede ordenar una parada durante el avance en AUTO. La combinación busca aprovechar señales distintas, pero no garantiza detectar todos los obstáculos.

## De la imagen a una decisión

La referencia de piso depende de la apariencia de la superficie visible. El detector compara colores por celdas y agrupa los resultados en zonas, con confirmación temporal para evitar reaccionar a una sola observación aislada. Un objeto puede ser difícil de distinguir si su color se parece al piso; una sombra o un reflejo puede parecer un objeto. Por eso la preparación debe hacerse en el lugar de uso y con el área despejada.

El corredor representa una franja de paso proyectada por perspectiva: se ve más ancha cerca de la cámara y más estrecha hacia el horizonte. Su posición depende de la geometría y la inclinación de la cámara. Mover la cámara sin revisar esos parámetros puede hacer que una detección se interprete fuera de la zona prevista.

La regla básica de fusión da prioridad a la señal de seguridad del detector de color y permite que el modelo incremente la severidad. El láser actúa por una vía de parada separada bajo las condiciones configuradas. Las fuentes no tienen el mismo campo de visión ni la misma latencia; una coincidencia entre ellas no debe suponerse en todas las situaciones.

El aprendizaje del piso admite varios grupos de color y puede actualizarse cuando la apariencia cambia. El proyecto documenta un riesgo: una pared que llena toda la imagen, incluida la franja de referencia, puede confundirse con un cambio completo de piso y aprenderse como tal. Esta limitación exige mantener supervisión y no confiar únicamente en la clasificación por color.

Dónde está en el código: `obstaculos.c`, `fusion.c`, `corredor.c`, `DetectorTflite.kt`, `tof.c`.
