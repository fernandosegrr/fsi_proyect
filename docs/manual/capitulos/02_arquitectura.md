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
