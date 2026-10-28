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

Dónde está en el código: `camara_prueba.ino`, `recuperacion.c`, `paquete_celdas.c`.
