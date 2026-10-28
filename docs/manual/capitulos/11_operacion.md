# Operación y seguridad física

El vehículo requiere supervisión constante y un espacio despejado. Antes de moverlo se comprueban la alimentación, la conexión, los sensores y la disponibilidad de una parada accesible.

## Preparación y conexión

Con los motores apagados, comprobar la fijación de cables, ruedas y sensores. Conectar cada placa a su propia batería portátil y encender la DevKit para que aparezca la red CarroFSI. Conectar el teléfono a esa red y abrir la aplicación. Verificar en Inicio el enlace con el carro y la cámara; consultar la pantalla de sensores con el carro quieto.

Antes de habilitar los motores, despejar la pista y comprobar que el operador puede accionar PARO y el interruptor físico del portapilas. Para una ruta, colocar el carro en la posición inicial, comprobar el rumbo y la distancia frontal, y revisar la calibración y la fuente de imagen. Simular una ruta ayuda a revisar su trazado, pero no sustituye una prueba física.

## Durante la operación

En Manual se mantiene pulsado el control de dirección y se suelta para detener. En Rutas se selecciona la trayectoria y se inicia con el área libre. Mantener una persona cerca de la parada física; si el carro no responde a la aplicación, apagar la alimentación de los motores.

El proyecto emplea como regla de ensayo un máximo de 20 corridas o 90 minutos con el mismo juego de pilas. Esa regla de operación no es una medición de autonomía. Cambios de pilas, superficie y agarre pueden invalidar una calibración previa.

Dónde está en el código: `ControlLogica.kt`, `ModoSemiautonomo.kt`, `carro.c`, `sensores.c`.
