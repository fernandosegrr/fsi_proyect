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

Dónde está en el código: `trayectoria.c`, `calibracion.c`, `navegador.c`, `simulador_navegador.c`, `sim_ruta.c`.
