# Introducción

El Carro FSI es un vehículo de cuatro ruedas controlado por una aplicación Android y una placa ESP32. Permite conducirlo de forma manual o preparar una ruta en una cuadrícula para que el sistema intente seguirla con supervisión de una persona. Una cámara independiente transmite video y los sensores del carro aportan información de orientación y distancia frontal.

Este manual describe la arquitectura, las conexiones, el funcionamiento del software, la operación y la evidencia disponible. Está dirigido a quien necesita comprender, mantener o presentar el proyecto sin haber participado en su desarrollo. La descripción del comportamiento previsto no sustituye una prueba del vehículo físico.

Para comenzar, conviene leer la arquitectura y el hardware. Los capítulos de firmware, cámara, comunicación, navegación y percepción explican las responsabilidades de cada parte. La operación y las pruebas reúnen las condiciones de uso y los resultados; los anexos concentran comandos, pines y referencias al repositorio.

## Cómo interpretar la evidencia

**VERIFICADO** identifica un resultado observado en el carro u otro dispositivo físico, bajo las condiciones indicadas. **SIMULADO** identifica un resultado obtenido con un modelo de software. **NO MEDIDO** señala una característica para la que no existe una medición física suficiente. **PENDIENTE** señala una comprobación o dato necesario que todavía falta. Una simulación ayuda a revisar la lógica, pero no demuestra por sí sola el comportamiento del carro en el piso.

Dónde está en el código: `firmware_carro.ino`, `navegador.c`, `MainActivity.kt`.

## Antes de comenzar

Conviene separar tres preguntas: qué está diseñado para hacer el sistema, qué muestra una prueba de software y qué ocurrió realmente en el vehículo. El código responde sobre la implementación de una revisión concreta. Los registros de prueba describen ejecuciones concretas. Una observación actual requiere revisar el equipo y anotar sus condiciones.

El término «autónomo» se emplea aquí para el seguimiento de una ruta por software. No significa que el carro pueda circular sin vigilancia, reconocer cualquier obstáculo o garantizar que alcanzará una posición exacta. El operador debe preparar el área y mantener la capacidad de detener la marcha.

## Recorrido recomendado de lectura

Para una primera utilización, lea la arquitectura, las precauciones de hardware y el procedimiento de operación. Si aparece un problema, consulte el diagnóstico y registre lo que observa antes de cambiar varias cosas a la vez. Para modificar el software, continúe con los capítulos de firmware, protocolo, navegación, visión y aplicación.

Para evaluar los resultados, lea pruebas y límites conjuntamente. Una cifra de simulación se interpreta con el modelo y las condiciones del experimento; no se transforma en una medición física por el hecho de aparecer en una gráfica o tabla. Los anexos ayudan a localizar nombres y conceptos cuando se revisa el repositorio.

## Una orden de principio a fin

Cuando el operador pide avanzar, la aplicación prepara una orden y la envía por el canal de control. El controlador la interpreta y modifica las señales que llegan al driver. El driver conmuta la alimentación de los motores, que producen el movimiento de las ruedas. En sentido inverso, el controlador comunica información de sensores y la cámara transmite imágenes por un canal independiente.

Cada paso puede fallar por una causa diferente. Que una pantalla responda al toque no demuestra que la orden haya llegado; que una orden llegue tampoco demuestra que las ruedas se muevan. En la práctica se observan por separado la conexión, la respuesta del controlador y el resultado físico.

## Unidades y convenciones

Las distancias se expresan en centímetros cuando se habla de ruta y en las unidades indicadas por cada interfaz de sensor cuando se consulta una lectura. El tiempo aparece en milisegundos para intervalos de control y en minutos para sesiones. Un grado expresa un ángulo, no una distancia. Antes de comparar cifras, hay que comprobar que representen la misma magnitud y empleen las mismas unidades.

La palabra «estimado» significa calculado a partir de información disponible y un modelo; no implica que se haya observado la posición real con un instrumento. «Configurado» indica un valor presente en software. «Medido» requiere un procedimiento y un resultado registrados. Estas distinciones permiten explicar el proyecto sin atribuirle precisión que no se ha demostrado.
