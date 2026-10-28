# Arquitectura general

El sistema distribuye el trabajo entre tres nodos: el controlador del carro, la cámara y el teléfono. Esta separación permite que la adquisición y compresión de video se realicen en una placa distinta de la que controla motores y sensores. La aplicación reúne la interacción con el operador y ejecuta el núcleo de navegación.

## Los tres nodos

La ESP32 DevKit del carro crea la red local y recibe las órdenes de movimiento. Controla el puente H, un circuito que permite invertir el sentido de los motores, y consulta el giroscopio y el sensor de distancia. Su dirección de red es 192.168.4.1.

La placa Freenove con cámara se conecta a esa red como cliente, con la dirección 192.168.4.2. Captura imágenes y puede transmitir video. El teléfono Android también se conecta a la red del carro: presenta los controles, recibe información y ejecuta la lógica de navegación en C mediante la aplicación.

La red se llama CarroFSI. La comunicación de control usa WebSocket, un canal bidireccional persistente; el video sigue un canal separado. Por eso una falla de video y una pérdida del enlace de control deben diagnosticarse por separado.

## Alimentación

La DevKit se alimenta por USB desde una batería portátil. La cámara Freenove usa otra batería portátil propia. Los motores reciben energía de un portapilas de cuatro pilas AA a través del driver. El negativo de las pilas, la tierra del driver y la tierra de la DevKit comparten referencia para las señales de control. La cámara se comunica por WiFi con el resto del sistema.

## Capas de seguridad

La persona conserva la responsabilidad de supervisar el espacio y poder detener el vehículo. La aplicación puede enviar una orden de parada; el firmware también vigila un temporizador de comunicación. Un sensor láser frontal puede detener el avance en el perfil AUTO cuando se cumplen las condiciones configuradas. Esa protección tiene un campo de observación limitado y puede desactivarse, por lo que no reemplaza la supervisión.

El movimiento recto se estima a partir del tiempo, no de una medición directa de distancia mediante encoders. El giroscopio ayuda a estimar el rumbo; no mide directamente el desplazamiento lateral. La corrección de pose y reincorporación de J15 cuenta con resultados de simulación, pero su validación física permanece **NO MEDIDA**.

Dónde está en el código: `firmware_carro.ino`, `config.h`, `camara_prueba.ino`, `navegador.c`.

## Responsabilidad de cada enlace

El enlace de control transporta órdenes y respuestas entre la aplicación y la DevKit. El flujo de video transporta imágenes entre el teléfono y la cámara. Ambos utilizan la misma red local, pero son servicios distintos: el estado de uno no prueba el del otro. Esta separación permite diagnosticar, por ejemplo, un carro controlable sin imagen o una imagen disponible mientras falla el control.

La comunicación de los sensores montados en el carro se realiza localmente con la DevKit. I2C comparte las líneas de datos y reloj entre dispositivos compatibles, mientras que el puente H recibe señales de mando para cada lado de los motores. Un fallo del bus puede afectar a las lecturas aunque la red inalámbrica siga funcionando.

## Qué sucede durante una ruta

El operador construye una secuencia de posiciones en la cuadrícula. El núcleo interpreta esa secuencia, determina la maniobra correspondiente y actualiza una pose estimada: posición y orientación calculadas. La aplicación traduce las decisiones del núcleo en órdenes para el carro y aporta información de los sensores y de la cámara cuando están disponibles.

La DevKit sigue siendo responsable de ejecutar las órdenes y de sus protecciones locales. La cámara, por su parte, no maneja el puente H. Esta división aclara dónde investigar un problema: una orden equivocada puede originarse en la planificación o en su traducción; una orden correcta sin movimiento puede apuntar al enlace, el firmware, la alimentación o la mecánica.

## Señales, energía y referencia eléctrica

Una señal de control indica al driver lo que debe hacer; no es la fuente de energía principal de los motores. Las pilas proporcionan esa energía a través del driver. La tierra común entre el controlador y el driver permite que el nivel de las señales tenga una referencia compartida. La conexión de tierras debe comprobarse con el sistema apagado y de acuerdo con el esquema del montaje.

La cámara tiene una batería portátil distinta y se comunica por WiFi. Que ambas placas usen USB no convierte sus baterías en una sola fuente. Tampoco debe suponerse que las baterías y el portapilas tienen el mismo estado de carga; un circuito puede funcionar mientras otro queda sin energía.

## Separación entre decisión y observación

Una imagen es una observación del entorno; una región marcada por el procesamiento es una interpretación de esa imagen. La distancia del láser es otra observación, limitada a la zona que cubre su orientación. La fusión combina información disponible según reglas del software, pero no garantiza una visión completa de todo el espacio alrededor del carro.

La pose de navegación también es una interpretación. En un tramo recto, el programa estima la distancia con el tiempo y una calibración; el giroscopio aporta información del giro. Si una rueda patina, la distancia estimada puede diferir de la real sin que el sistema conozca inmediatamente esa diferencia.

## Secuencia de fallos para razonar

Considere una ruta iniciada con video ausente. Primero hay que distinguir si el modo elegido exige cámara y qué protección se activa con esa configuración. Después se verifica la alimentación de la cámara, su conexión a la red y la recepción de imágenes. No es correcto concluir que el puente H está averiado solo porque no aparece video.

Si hay video pero la orden de avance no produce movimiento, la investigación cambia: conexión de control, estado del carro, alimentación de motores, señales al driver y mecánica son áreas distintas de comprobación. En ambos casos se detiene el carro antes de manipular cables.
