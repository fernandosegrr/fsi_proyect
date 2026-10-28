# Cámara Freenove

La cámara es un nodo separado que se conecta a la red creada por el carro. Su función es proporcionar imágenes o datos resumidos al teléfono, sin encomendar la compresión de video a la DevKit que controla la tracción.

## Captura y transmisión

El sensor GC0308 entrega imágenes sin compresión JPEG de hardware. La placa comprime los cuadros por software y ofrece un flujo de imágenes JPEG mediante HTTP. También existen modos para obtener un cuadro RGB565 sin comprimir y para transmitir por UDP un resumen de colores por celdas. UDP es un protocolo de envío de datagramas que no confirma por sí mismo la entrega.

La configuración predeterminada del código usa HQVGA de 240 × 176 píxeles, RGB565, reloj de 20 MHz y calidad JPEG 50. La cámara emplea la dirección 192.168.4.2 y necesita conectarse a la red del carro para que el teléfono acceda a sus servicios.

## Rendimiento y límites

En un barrido local sin cliente se midieron 100 cuadros por combinación. Con 20 MHz y RGB565, HQVGA registró 12.4 cuadros por segundo y 137 ms de edad del cuadro hasta terminar la compresión; QVGA registró 8.8 cuadros por segundo y 252 ms con calidad 50. Son resultados de ese ensayo, no una medición del retraso total hasta la pantalla del teléfono.

El firmware intenta recuperar la conexión y reiniciarse en determinadas fallas. La transmisión y la percepción siguen dependiendo de la red, la iluminación, la orientación de la cámara y el procesamiento posterior. El comportamiento de la cámara montada durante el movimiento requiere validación física.

## Servicios y gestión de cuadros

El servicio `/stream` entrega una sucesión de imágenes JPEG; `/capture` entrega una captura y `/raw` expone un cuadro RGB565. La ruta `/stats` informa datos de funcionamiento para diagnóstico. El modo de celdas resume los colores de bloques de la imagen y los transmite por el puerto UDP 4210. La aplicación utiliza la dirección de la cámara configurada en Ajustes.

La captura y la transmisión se organizan con una casilla para el cuadro más reciente. Si el consumidor no alcanza el ritmo de captura, se descartan cuadros anteriores en lugar de acumular una cola cada vez más atrasada. Esto reduce acumulación de imágenes viejas, pero no elimina la latencia de extremo a extremo.

La placa intenta reconectarse cuando pierde la red y contempla reinicios ante determinadas fallas del servicio. Una ausencia de video puede tener varias causas: alimentación, conexión WiFi, una dirección incorrecta o un cliente de video anterior. Antes de interpretar un resultado de percepción, debe comprobarse que los cuadros estén llegando y que correspondan a la fuente seleccionada.

## Placa, sensor y formatos

La Freenove ESP32-WROVER v3.0 incorpora un sensor GC0308 y memoria adicional PSRAM. Es una placa diferente de la DevKit de tracción, aunque ambas pertenezcan a la familia ESP32. Cada una tiene su propio programa, sus propias conexiones y su batería portátil. El teléfono no controla los motores mediante los pines de la cámara: envía comandos por la red a la DevKit.

El sensor del montaje entrega imágenes en formatos como RGB565; la compresión JPEG la realiza la placa por software. JPEG reduce el tamaño del cuadro a costa de pérdida de información. RGB565 representa colores con un número fijo de bits por píxel y se utiliza como entrada de procesamiento. El modo adecuado depende de si se necesita visualización, acceso al cuadro sin compresión JPEG o un resumen de color por celdas.

La resolución determina cuántos píxeles se procesan. HQVGA en esta configuración corresponde a 240 por 176 píxeles y QVGA a 320 por 240. Un cuadro más grande contiene más muestras de imagen, pero necesita más memoria y trabajo de compresión. No se puede deducir de la resolución, por sí sola, que un detector reconocerá todos los objetos.

## Dirección y conexión a la red

La cámara trabaja como cliente de la red creada por la DevKit. Su dirección fija es 192.168.4.2; la puerta de enlace configurada corresponde a la DevKit. La dirección fija permite que la aplicación conozca dónde buscar el flujo sin depender de qué dirección dinámica se entregue en cada arranque.

El servicio dinámico de direcciones de la DevKit reserva otro intervalo para clientes. Si la cámara arranca cuando la red aún no está disponible, la lógica de recuperación realiza intentos de conexión. Es posible encender las placas en orden distinto, pero el video no estará disponible hasta que ambas tengan alimentación y la cámara se haya asociado a la red.

## Un cuadro frente a un flujo continuo

Una captura aislada entrega un cuadro. Un flujo MJPEG es una sucesión de cuadros JPEG que se envían por una misma conexión; cada cuadro se puede mostrar de forma individual. Para la percepción, la edad de cada cuadro importa: si se procesa una imagen vieja, el carro puede haber cambiado de posición desde el momento de captura.

La implementación conserva el cuadro más reciente en una casilla compartida. Cuando el productor genera otro cuadro antes de que se consuma el anterior, el anterior se sustituye. La ventaja es evitar una cola creciente de imágenes antiguas; la consecuencia es que no todos los cuadros capturados llegan necesariamente al consumidor.

El código descarta cuadros cuya edad supera el límite configurado al salir de la cámara. Ese filtro no mide el retraso restante de red, decodificación o presentación en la pantalla. Si se necesita medir el retardo total, debe definirse un método que observe el mismo evento al inicio y al final de la cadena, y registrarse varias muestras en condiciones reproducibles.

## Datos por celdas

En el modo de celdas, la imagen se resume mediante promedios de color de bloques. El paquete incluye dimensiones y datos que permiten reconstruir una cuadrícula de análisis. El teléfono solicita este servicio por UDP; la cámara responde y recuerda al cliente. La transmisión usa el puerto 4210. El modo de celdas puede coexistir con la disponibilidad de JPEG en el servicio de flujo.

Este resumen reduce la cantidad de datos respecto de enviar cada píxel, pero también elimina detalle. No es equivalente a una fotografía completa ni transmite por sí mismo la identidad de un objeto. La percepción por color interpreta las celdas según el piso aprendido y sus umbrales; la comunicación por UDP, a diferencia de un flujo confiable de bytes, no garantiza por sí sola la llegada de cada paquete.

## Leer los resultados del barrido

El barrido histórico comparó combinaciones de resolución, formato, reloj y calidad. Cada combinación se evaluó con 100 cuadros y sin un cliente de video externo. A 20 MHz con RGB565, HQVGA registró 12.4 cuadros por segundo y QVGA 8.8 con la calidad indicada. En ese ensayo, la compresión media informada fue 56 ms para HQVGA y 107 ms para QVGA.

La comparación ayuda a entender la elección del valor predeterminado, pero no representa la velocidad de todas las escenas ni la latencia completa de la app. Las condiciones de luz, la complejidad de imagen, la carga del procesador y la presencia de un cliente pueden cambiar el resultado. La cifra de edad del cuadro reportada en el barrido debe conservar su definición de ensayo; no debe reemplazarse por una afirmación de tiempo hasta pantalla.

## Fallas y recuperación

La lógica de recuperación distingue pérdida de WiFi, falta de respuesta de un servicio y ausencia prolongada de cuadros cuando existe demanda. Para algunas condiciones intenta reconexión y para otras solicita reinicio. El registro de estado ayuda a investigar si hubo reinicios o reconexiones; observar video después de una recuperación no identifica por sí solo la causa inicial.

Una autoprueba local puede verificar que ciertos servicios respondan dentro de la placa, pero no reproduce el enlace inalámbrico con el teléfono. Si la autoprueba pasa y la aplicación no recibe video, aún deben revisarse la red, la dirección, la sesión de video y el cliente conectado. Si el sensor no inicializa, no se debe asumir que cada falla será recuperada automáticamente.

Dónde está en el código: `camara_prueba.ino`, `recuperacion.c`, `paquete_celdas.c`.
