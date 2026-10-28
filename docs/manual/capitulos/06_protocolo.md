# Protocolo de comunicación

La aplicación y el carro intercambian órdenes y estados por un WebSocket en la red local. El protocolo utiliza mensajes de texto breves; el video de la cámara sigue una vía distinta. Los comandos principales se reúnen en el anexo.

## Movimiento y parada

FORWARD y REVERSE ordenan avance y retroceso; LEFT y RIGHT ordenan pivotes. STOP y EBRAKE detienen los motores. En control manual, la aplicación repite la orden mientras el usuario mantiene pulsado el control y envía STOP al soltarlo. El vigilante detiene los motores cuando el temporizador de comunicación supera el intervalo configurado; el firmware también lo renueva ante algunos mensajes que no son órdenes de movimiento.

PING y PONG permiten comprobar el enlace, pero PING no renueva el plazo de movimiento. La cola protege las órdenes de parada frente a saturación. GIRAR solicita un giro por ángulo y devuelve un resultado como GIRO_OK, GIRO_TIMEOUT o GIRO_ERROR.

## Estado y configuración

MODO_AUTO y MODO_MANUAL seleccionan perfiles; sus respuestas confirman la selección. Los mensajes SENS comunican ángulo, distancia y banderas de estado. TOF_PARO informa una detención activada por el láser. Las consultas de sensores y balance permiten comprobar las condiciones antes de una ruta.

## Interpretación de telemetría

La trama SENS contiene tres campos separados por comas: ángulo en décimas de grado, distancia en milímetros y un entero de banderas. Por ejemplo, un ángulo de 450 representa 45 grados. Las banderas indican el estado del giroscopio, del láser y de otras condiciones del firmware; se deben interpretar junto con la versión del protocolo correspondiente.

El perfil AUTO puede emitir telemetría periódica mientras hay un cliente conectado. Una lectura inválida o atrasada no debe tratarse como una distancia segura. Las respuestas de error o de tiempo agotado de un giro indican que la operación no terminó normalmente y requieren detenerse y revisar las condiciones antes de continuar.

## Direcciones y sesiones

La DevKit crea un punto de acceso local con la dirección 192.168.4.1. La aplicación abre el canal de órdenes en `ws://192.168.4.1/ws`. La cámara se conecta a esa red con la dirección 192.168.4.2 y ofrece su video por HTTP. El teléfono se incorpora como cliente de la misma red y puede hablar con ambos nodos.

Una dirección IP identifica un dispositivo dentro de la red; una ruta como `/ws` identifica el servicio solicitado en ese dispositivo. La DevKit asigna direcciones de cliente mediante DHCP en un intervalo que deja libre la dirección fija de la cámara. Si otro equipo usa una dirección fija que coincide con la de la cámara, el resultado puede ser una conexión ambigua; la dirección debe revisarse en la configuración de ese equipo.

El firmware mantiene un cliente WebSocket activo a la vez. Si otro cliente abre el control, puede cerrar la sesión anterior. Por esa razón no conviene controlar simultáneamente desde el navegador y desde la aplicación. La sesión de video de la cámara también tiene una limitación de clientes que debe considerarse por separado.

## Orden mantenida y pulso de conexión

En conducción manual, mantener pulsado un control produce repeticiones de la orden de movimiento. Al soltarlo, la aplicación envía STOP. PING y PONG son mensajes de sondeo: ayudan a saber si hay respuesta, pero PING no renueva el temporizador que protege el movimiento. La app y el firmware realizan tareas distintas, por lo que un PONG recibido no demuestra que una orden de motor se haya ejecutado ni que el sensor vea el camino libre.

El temporizador se revisa cuando hay motores en marcha y no está activo un giro por ángulo. El valor configurado es 1000 ms y la condición de código usa una comparación de tiempo transcurrido. El firmware renueva el contador con algunos textos que no son movimiento; incluso un texto desconocido puede renovarlo. Esto limita la interpretación de la protección: no se debe afirmar que solo una orden de movimiento válida mantiene vivo el temporizador.

Los giros por ángulo tienen su propio plazo y su respuesta. Si se interrumpen con una parada o aparece una falla del giroscopio, la aplicación debe tratar la respuesta como un resultado de la maniobra y no como una confirmación de la ruta completa. El tiempo hasta que la carrocería deja de moverse también incluye la inercia después de apagar la señal PWM.

## Interpretar un ejemplo de sensores

En el mensaje `SENS:450,320,7`, el primer campo representa 45.0 grados y el segundo 320 mm. El tercero es una combinación de banderas: 7 equivale a 1 + 2 + 4, que en esta versión indica las condiciones definidas para giroscopio listo, sensor de distancia presente y distancia válida. Una bandera informa lo que el firmware declaró en ese instante; no prueba que una lectura permanezca vigente después ni que la orientación física esté calibrada contra una referencia externa.

El valor 320 mm es una distancia del sensor frontal, no el ancho de una caja ni la distancia a cualquier punto alrededor del carro. Si el objeto no intercepta el haz, o la medición es inválida, la aplicación necesita interpretar esa condición junto con la percepción visual y el estado del sensor. Un valor de respuesta debe acompañarse de su unidad y condición de validez al registrarlo.

## Consultas, confirmaciones y errores

CAPS? consulta la disponibilidad declarada de sensores. IMU? permite solicitar información del giroscopio y TOF? del sensor de distancia. IMU_CAL pide recalcular el sesgo en reposo y puede rechazarse si no se cumplen sus condiciones; IMU_CERO establece una referencia angular. TOF_USO permite habilitar o deshabilitar la autoridad de parada del láser. Un cambio de configuración debe confirmarse mediante la respuesta del carro.

Los perfiles se solicitan con MODO_AUTO y MODO_MANUAL y devuelven confirmaciones distintas. Los factores de balance se pueden consultar y modificar por perfil dentro del intervalo permitido por el firmware. La memoria no volátil puede conservar un valor previo; por eso un dato predeterminado leído en el código no basta para afirmar el valor efectivo de un dispositivo sin consultarlo.

LOOP? informa el máximo registrado del tiempo de una vuelta y el número de excesos respecto al umbral configurado. Un máximo y un contador sirven para detectar que ocurrió al menos un exceso; no muestran por sí mismos la causa ni el tiempo medio de todas las vueltas. La interpretación requiere anotar cuándo se reinició el contador y qué actividad se realizó durante la ventana observada.

Dónde está en el código: `carro.c`, `sensores.c`, `ControlLogica.kt`, `Enlace.kt`.
