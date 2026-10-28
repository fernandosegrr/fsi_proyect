# Operación y seguridad física

El vehículo requiere supervisión constante y un espacio despejado. Antes de moverlo se comprueban la alimentación, la conexión, los sensores y la disponibilidad de una parada accesible.

## Preparación y conexión

Con los motores apagados, comprobar la fijación de cables, ruedas y sensores. Conectar cada placa a su propia batería portátil y encender la DevKit para que aparezca la red CarroFSI. Conectar el teléfono a esa red y abrir la aplicación. Verificar en Inicio el enlace con el carro y la cámara; consultar la pantalla de sensores con el carro quieto.

Antes de habilitar los motores, despejar la pista y comprobar que el operador puede accionar PARO y el interruptor físico del portapilas. Para una ruta, colocar el carro en la posición inicial, comprobar el rumbo y la distancia frontal, y revisar la calibración y la fuente de imagen. Simular una ruta ayuda a revisar su trazado, pero no sustituye una prueba física.

## Durante la operación

En Manual se mantiene pulsado el control de dirección y se suelta para detener. En Rutas se selecciona la trayectoria y se inicia con el área libre. Mantener una persona cerca de la parada física; si el carro no responde a la aplicación, apagar la alimentación de los motores.

El proyecto emplea como regla de ensayo un máximo de 20 corridas o 90 minutos con el mismo juego de pilas. Esa regla de operación no es una medición de autonomía. Cambios de pilas, superficie y agarre pueden invalidar una calibración previa.

## Preparar el entorno

La pista debe permitir observar el carro durante toda la maniobra y alcanzar el interruptor de motores. Se retiran objetos que puedan atrapar una rueda y se mantienen personas, cables y bordes fuera de la trayectoria. En una prueba de retroceso se deja también espacio libre detrás: el carro no mide esa zona.

Para comparar recorridos se marca un punto inicial y una línea de referencia. Una cinta métrica permite medir distancias y desvíos en el piso. La posición inicial debe ser repetible: las ruedas apoyadas, el frente alineado y la ubicación anotada. Si cambia la posición de la cámara, la iluminación o la superficie, se registra el cambio antes de comparar resultados.

Antes de energizar, se revisa que las baterías estén en condiciones adecuadas y que el portapilas de motores esté apagado. Se comprueba que cables y conectores no estén sueltos ni rocen las ruedas. Cada placa usa su propia powerbank; las cuatro pilas AA alimentan el driver de motores. Ninguna de estas comprobaciones determina por sí sola la autonomía restante.

## Encender y verificar sin movimiento

Conectar la batería de la DevKit y la de la cámara. La DevKit crea la red; la cámara debe asociarse a ella. En el teléfono, seleccionar CarroFSI desde los ajustes de red y abrir la aplicación. En Inicio se verifica el enlace con el carro y la disponibilidad de la cámara. Si no aparece la red, se revisan alimentación y cable antes de buscar fallas en la aplicación.

Con el carro inmóvil se consulta Sensores. Se revisa que el giroscopio esté listo y que la lectura de distancia sea coherente con el espacio frontal. Si el rumbo cambia con el carro quieto, puede ser necesario repetir la calibración de sesgo siguiendo la indicación de la interfaz. La referencia de cero debe establecerse con el carro en la orientación que se utilizará como inicio.

Si el video no llega, comprobar la alimentación de la cámara, la red, la dirección configurada y si otro cliente está ocupando la sesión. No se inicia una ruta que requiera cámara mientras no se reciban cuadros. El botón de parada y el interruptor físico deben estar accesibles antes de energizar los motores.

## Conducir en manual

En la pantalla Manual se selecciona el perfil deseado y se verifica su confirmación. Antes de pulsar una dirección se revisa el espacio correspondiente. El control se mantiene pulsado mientras se desea sostener la orden; al soltarlo la aplicación envía STOP. El operador observa la respuesta física, porque el movimiento puede continuar un poco después de retirar la señal por la inercia del chasis.

Si el carro no responde a la parada desde la aplicación o sale del área prevista, cortar la alimentación de motores mediante el interruptor del portapilas. Después se investiga la causa con el vehículo detenido. La parada de software, el watchdog y el láser son capas útiles, pero no sustituyen el control físico ni una zona despejada.

## Preparar y ejecutar una ruta

En Rutas se colocan los puntos sobre la cuadrícula y se comprueba que el inicio y la dirección correspondan al montaje físico. Cada celda equivale a 20 cm nominales. Simular permite comprobar si los puntos forman la trayectoria deseada en el modelo; no valida la tracción ni la detección real de un obstáculo.

Antes de ejecutar se revisan la calibración guardada, la confirmación de AUTO, el estado de sensores y la fuente de imagen. Si se va a utilizar la detección por color, la referencia del piso debe corresponder a la superficie del lugar y el área de referencia debe estar despejada. La aplicación muestra el progreso, pero la persona sigue observando el vehículo y el entorno durante toda la ruta.

Si aparece un obstáculo, el navegador puede detenerse y planear una maniobra. No debe colocarse una persona en la trayectoria para probar una detección sin un procedimiento seguro. Si el comportamiento no coincide con lo previsto, se detiene la prueba y se registra lo sucedido, incluidas las intervenciones del operador. La corrección de reincorporación J15 permanece sin validación física.

## Cierre de una sesión

Al terminar, detener la ruta y apagar el portapilas de motores. Después se desconectan las alimentaciones USB de las placas si el vehículo no se va a usar. Se registran cambios observados en la conexión, sensores, batería y comportamiento, junto con el número de corridas realizadas.

La regla de no superar 20 corridas o 90 minutos con el mismo juego de pilas busca mantener comparables las condiciones de ensayo. No equivale a una especificación medida de autonomía. Si se cambian pilas, piso o carga, se revisa la calibración antes de comparar nuevas mediciones con las anteriores.

Dónde está en el código: `ControlLogica.kt`, `ModoSemiautonomo.kt`, `carro.c`, `sensores.c`.
