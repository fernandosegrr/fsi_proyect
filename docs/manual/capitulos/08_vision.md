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

## Formación de la cuadrícula de color

El detector divide la imagen en bloques de 10 por 10 píxeles y calcula un color promedio para cada bloque o celda. Si la imagen tiene 320 por 240 píxeles, la cuadrícula tiene 32 columnas y 24 filas. Si tiene 240 por 176, se utilizan 24 columnas y 17 filas completas; los píxeles que no completan un bloque no forman una celda adicional.

El promedio reduce el efecto de variaciones aisladas, pero también elimina detalle. Un objeto pequeño que ocupa pocas celdas puede no alcanzar el mínimo configurado para marcar una zona. Al reducir la resolución, cada celda representa una parte mayor de la escena; por ello una imagen más pequeña no equivale a la misma capacidad de detección con menos datos.

Una franja de la parte inferior y central aporta la referencia inicial de piso. El algoritmo compara el color de las demás celdas con esa referencia. Si la franja está ocupada por un objeto o no representa la superficie que se recorrerá, la referencia puede ser inadecuada. Preparar el área despejada es una condición del método, no una calibración opcional de presentación.

## Color, brillo y varios tonos

La comparación considera tanto diferencias directas de los canales rojo, verde y azul como una comparación de tono que intenta tolerar ciertos cambios de iluminación. El brillo indica cuánta intensidad total tiene el color; el tono normalizado permite comparar colores parecidos aunque uno se vea más claro. En zonas demasiado oscuras el tono puede ser poco confiable.

El piso no siempre es uniforme. Las baldosas, vetas y uniones producen colores distintos incluso cuando la superficie está libre. El modelo admite varios grupos de color para representar esa variación. Durante el aprendizaje continuo se restringe qué celdas pueden modificar la referencia, con el propósito de no incorporar como piso las celdas ya consideradas obstáculo o sus vecinas.

La restricción reduce algunos errores, pero no distingue de forma infalible un cambio de piso de una superficie que ocupa toda la imagen. Si una pared cubre también la franja de referencia, el sistema puede acabar interpretándola como un piso nuevo. La supervisión humana y la parada física siguen siendo necesarias incluso cuando el algoritmo informe una zona libre.

## Zonas y niveles de cercanía

El resultado de color resume información por zonas izquierda, centro y derecha. La cercanía va de 0 a 3 y se relaciona con la posición de las celdas detectadas dentro de la imagen. Un nivel más alto señala una observación que el algoritmo considera más próxima, pero no equivale a una distancia medida por el láser.

La perspectiva explica parte de la regla: una región próxima al vehículo suele aparecer más abajo en la imagen. No obstante, la relación depende de la altura y orientación de la cámara y de la forma del objeto. Un objeto alto o una imagen recortada pueden alterar la interpretación. Para determinar una distancia física se necesita una medición apropiada y su condición de validez.

Un obstáculo de nivel 1 puede producir solo un aviso, según la configuración del navegador. Los niveles 2 y 3 participan en reglas de detención y confirmación. La aplicación debe distinguir el dato visual de la orden final de movimiento: una zona marcada no es, por sí sola, una prueba de que el firmware haya detenido los motores.

## Modelo de objetos y vigencia

El modelo de objetos procesa imágenes en un hilo separado del detector de color. El hilo recibe el cuadro más reciente disponible y publica detecciones con una marca de tiempo. Esto evita que el detector de color espere cada inferencia, pero significa que los dos resultados pueden corresponder a momentos distintos.

La configuración de fusión descarta un resultado del modelo demasiado antiguo; el valor de referencia documentado es 300 ms. La edad se calcula respecto a la marca del cuadro, no solo desde que terminó el cálculo. Si el modelo no está disponible o su resultado caduca, la regla básica continúa utilizando el detector de color. La vigencia configurada es una regla de software, no una medición de tiempo de reacción completo del carro.

El modelo entrega cajas que delimitan regiones de la imagen y una confianza para sus detecciones. La posición de la caja y de su borde inferior ayuda a asignar zona y cercanía visual. Una confianza es una puntuación del modelo bajo sus reglas de inferencia; no es una garantía física de que el espacio esté libre o de que se reconozcan todas las clases.

## Reglas básicas de fusión

Cuando el detector de color ya señala un obstáculo cercano, la regla básica conserva esa señal sin esperar una confirmación del modelo. Si el modelo ve un objeto en el camino y el detector tiene una señal menos concluyente, la fusión puede elevar la severidad. Si el modelo detecta algo lejano que el detector no ve, puede producir un aviso. Para un objeto cercano visto solo por el modelo se requiere una confianza mayor para ordenar una parada desde esa señal.

Estas reglas se ejecutan sobre datos y umbrales configurados, y existen otras etapas opcionales de percepción que pueden modificar el resultado. La autoridad del láser corresponde a una protección distinta en el firmware. Ninguna de estas capas proporciona cobertura completa del entorno ni permite abandonar la supervisión del operador.

Dónde está en el código: `obstaculos.c`, `fusion.c`, `corredor.c`, `DetectorTflite.kt`, `tof.c`.
