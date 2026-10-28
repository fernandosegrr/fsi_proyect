# Núcleo de navegación en C

El núcleo transforma una ruta dibujada en la aplicación en acciones de avance y giro. Su implementación en C también puede ejecutarse en una computadora para probar la lógica con un vehículo simulado.

## Ruta, calibración y pose

La cuadrícula de la aplicación representa celdas de 20 cm. La trayectoria se descompone en tramos y cambios de dirección. Como el carro no dispone de encoders para medir el giro de las ruedas, la distancia de avance se estima a partir del tiempo y de una calibración en milisegundos por centímetro. La calibración también contempla el arranque, el frenado y los giros.

La pose es la posición y orientación estimadas. Durante un avance, el navegador integra incrementos de distancia con el rumbo observado por el giroscopio cuando la lectura es válida. El giro medido mejora la orientación estimada, pero no detecta un deslizamiento lateral sin rotación. Los errores de calibración y de tracción pueden acumularse.

## Esquive y reincorporación

Ante un obstáculo, el navegador puede detenerse, evaluar una alternativa lateral y planear el regreso a la trayectoria. Si no puede continuar, puede detenerse o solicitar ayuda según la configuración. La corrección J15 integra el rumbo observado durante el avance para actualizar la pose que usa la reincorporación.

En el escenario B6 **SIMULADO**, el error lateral medio pasó de 13.2 a 1.9 cm después de la corrección. Se terminaron 400 de 400 rutas y se registraron cero contactos en ambas versiones; con error menor de 20 cm terminaron 344 y 400, respectivamente. La validación de esa corrección en el carro físico permanece **NO MEDIDA**.

## Simulación

El simulador ejecuta la lógica del navegador sobre un modelo de movimiento y obstáculos. Permite repetir escenarios con condiciones controladas, pero no reproduce toda la percepción real, el deslizamiento, la iluminación ni la red. Un resultado simulado no demuestra que el vehículo físico siga la misma trayectoria.

## Ajuste de movimiento

La calibración relaciona una distancia conocida con el tiempo de avance observado y contempla el tiempo que tarda el carro en comenzar a desplazarse. El frenado representa la distancia recorrida después de ordenar la parada. Esos parámetros son específicos del montaje y de las condiciones del ensayo: una modificación de peso, pilas o superficie puede cambiar su efecto.

Los giros izquierdo y derecho no son necesariamente simétricos. El firmware utiliza el giroscopio para observar el ángulo y puede anticipar la detención para reducir el sobrepaso. Si el sensor no está listo o se agota el tiempo, la respuesta de giro informa el fallo. Una lectura tardía no debe presentarse como una posición absoluta medida.

## Lectura de la simulación

El simulador distingue la posición de su vehículo virtual de la posición estimada por el navegador. En el ensayo B6 la caja, el chasis, el retraso de video y la variación de giro forman parte del modelo; algunas de esas magnitudes son supuestos. Se registran llegadas, contactos y error lateral. Esas métricas no miden la precisión longitudinal ni demuestran que una ruta física quede libre de contacto.

## De una cuadrícula a movimientos

La ruta se describe mediante puntos de una cuadrícula. Una celda representa 20 cm, de modo que dos puntos separados por cinco celdas en una misma dirección representan un tramo nominal de 100 cm. El núcleo transforma cada cambio de dirección en un giro y cada tramo en un avance. La duración del avance depende de la calibración; la cantidad de celdas no indica cuánto recorrieron realmente las ruedas.

Por ejemplo, si una calibración hipotética indica 50 ms por centímetro, 100 cm corresponden a 5000 ms de movimiento calculado antes de considerar el tratamiento de arranque y frenado. La cifra es un ejemplo de cálculo, no una medición nueva. El programa contempla parámetros adicionales para el tiempo inicial y el movimiento posterior a la parada; esos valores deben corresponder al carro y al entorno utilizados.

Una ruta dibujada y una ruta ejecutada son objetos distintos. La primera contiene la intención del operador; la segunda resulta de las decisiones del navegador, de las observaciones disponibles y de cómo responde el vehículo. Si se cambia la ruta o se interrumpe la ejecución, debe comprobarse el estado de parada antes de volver a iniciar.

## Estados y transiciones

El navegador organiza su trabajo como una máquina de estados: en cada instante tiene un estado y aplica las reglas asociadas a él. En ESPERANDO existe una ruta cargada que aún no se ejecuta. En SIGUIENDO se procesa el paso actual. En DETENIDO_OBSTACULO se mantiene la parada mientras se decide si es posible continuar o esquivar.

En ESQUIVANDO se ejecutan las maniobras para apartarse; en REINCORPORANDO se calcula el regreso a la línea original. TERMINADO indica fin de ruta y ERROR indica que no pudo continuarse. En ESPERANDO, TERMINADO y ERROR el núcleo devuelve STOP. La opción AYUDA permite intervención de una persona cuando está habilitada; durante esa intervención el responsable debe supervisar el espacio y las órdenes manuales.

El estado es una descripción de la lógica, no una medición independiente del movimiento. Por ejemplo, TERMINADO indica que se alcanzó el criterio de la ruta en el modelo del navegador; no certifica por sí solo la posición física del chasis. La posición real se compara con una referencia externa, como una marca y una cinta métrica, cuando se desea medir error.

## Confirmación de obstáculos y pérdida de imagen

La cercanía visual se expresa por niveles; no es una distancia física directa en centímetros. El código trata un obstáculo muy cercano con mayor urgencia que uno lejano. Para una detección de nivel 2 en una zona lateral se requiere confirmación temporal: 150 ms y al menos dos cuadros. Para volver a avanzar se exige una condición de frente libre durante 250 ms. Estos son parámetros del software, no resultados de tiempo de reacción medidos en el conjunto físico.

La ausencia de cuadros también se considera una condición de riesgo. Para una ruta que exige cámara, el núcleo define un hueco máximo de video de 700 ms y, al superarlo, detiene la ruta y pasa a ERROR. Al empezar existe además un plazo para recibir el primer cuadro mientras el carro permanece quieto. La aplicación tiene sus propias comprobaciones; el valor de una capa no debe confundirse con una medición de latencia de toda la cadena.

## Desvío y regreso a la ruta

La configuración plantea un giro de 90 grados para apartarse y un tramo lateral nominal de 40 cm. El navegador busca superar el obstáculo antes de volver hacia la línea; el punto de reincorporación se define 30 cm por delante del punto de salida. Esos valores describen el plan del algoritmo, no las distancias necesariamente alcanzadas por el carro físico.

El regreso se calcula a partir de la pose estimada. Si el carro ha girado o patinado de una manera que el modelo no representa, el punto calculado puede diferir del lugar real. La Figura 7 ilustra una trayectoria conceptual y no representa un plano de montaje ni una medición del recorrido.

El código también limita el retroceso de los reintentos: 15 cm y un máximo de 2000 ms. El vehículo no cuenta con un sensor posterior en esa maniobra. Por eso el operador debe mantener libre la zona detrás del carro; el límite programado no detecta a una persona, una pared o un borde en esa dirección.

## Datos que conserva la calibración

El modelo de calibración incluye el tiempo de arranque, el tiempo por centímetro, el avance posterior al STOP y parámetros separados para giros izquierdo y derecho. Puede incluir retroceso y desplazamiento del centro del carro durante un pivote. Separar ambos lados permite representar la asimetría del chasis, en vez de asumir que los dos motores y las dos maniobras son idénticos.

La aplicación guía mediciones por partes. En la prueba de avance se registran distancias recorridas para distintos tiempos y repeticiones; el cálculo ajusta los parámetros a esos datos. El tiempo de arranque incluye una intervención humana cuya reacción debe considerarse. Si se usan valores de ejemplo o parámetros antiguos, el simulador y la ruta real pueden divergir aunque ambos ejecuten correctamente el mismo código.

La guía histórica de calibración afirma que el carro no tenía IMU. Esa descripción corresponde a una etapa anterior: el código actual utiliza el MPU6050. El avance continúa estimándose por tiempo, mientras que el giroscopio aporta información de giro y rumbo. Ambas afirmaciones pueden coexistir cuando se indica qué magnitud se mide y cuál se estima.

Dónde está en el código: `trayectoria.c`, `calibracion.c`, `navegador.c`, `simulador_navegador.c`, `sim_ruta.c`.
