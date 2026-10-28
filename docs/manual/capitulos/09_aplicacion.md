# Aplicación Android

La aplicación reúne los controles de conducción, el editor de rutas y las pantallas de diagnóstico. Se comunica con el carro y con la cámara por la red local y llama al núcleo de navegación en C mediante una interfaz nativa.

## Pantallas principales

Inicio presenta el estado de conexión y accesos rápidos. Manual reúne los controles de dirección y parada. Rutas permite dibujar, simular y ejecutar trayectorias. Más da acceso a ajustes y herramientas como sensores y calibración. Las sesiones de conexión y video se comparten entre pantallas para evitar abrir un cliente independiente en cada una.

El modo manual envía órdenes mientras se mantiene pulsado un control y solicita parada al soltarlo. En una ruta, la aplicación suministra al navegador las observaciones y transmite las órdenes que este decide. Si se pierde la conexión o dejan de llegar cuadros durante el tiempo previsto, la ruta se cancela según la lógica de la aplicación.

## Uso del video y percepción

La cámara del carro y la cámara del teléfono son fuentes distintas. Algunas pantallas usan el video de la Freenove; la pantalla de visión puede usar la cámara del teléfono. Conviene comprobar cuál fuente está seleccionada antes de interpretar una detección. La cámara del carro atiende un cliente de video a la vez, y el firmware atiende un cliente WebSocket de control a la vez.

## Preparación de una ruta

En Rutas se dibujan los puntos de paso sobre la cuadrícula. La simulación muestra el resultado del modelo y permite revisar la secuencia antes de poner el carro en el piso. La ejecución real depende de la calibración guardada, de los sensores, de la imagen y de la conexión. Si la aplicación no recibe confirmación del perfil AUTO, no debe interpretarse la ruta como iniciada correctamente.

La pantalla Sensores permite consultar el estado del giroscopio y del láser, y la de calibración de giro guía las pruebas que alimentan sus parámetros. Esos procedimientos se realizan con el carro colocado y el área despejada. Cambiar de pantalla mientras hay una ruta en marcha requiere atender la indicación de la aplicación y comprobar que la ejecución se haya detenido.

## Organización de la interfaz

Inicio reúne el estado general. Desde ahí se puede comprobar si el carro y la cámara responden, y revisar indicadores disponibles antes de conducir. Un indicador de conexión describe el estado de una sesión de red; no confirma automáticamente que las ruedas estén libres o que el área sea segura.

Manual presenta los controles de movimiento, la selección de perfil y el acceso a parada. En esta pantalla el operador mantiene una orden mediante el control táctil. La repetición de mensajes es una parte del diseño de comunicación, pero el operador debe verificar visualmente que el vehículo responda como se espera.

Rutas contiene el editor y la ejecución. El editor representa el espacio mediante celdas y permite definir puntos. La simulación utiliza un modelo para representar el recorrido antes de ejecutarlo; debe interpretarse como una ayuda de planeación. Más reúne opciones de configuración y herramientas adicionales, incluidas pantallas de calibración y sensores.

## Conexión compartida

La aplicación mantiene una sesión compartida del carro y otra del video. Las pantallas que usan esos recursos se incorporan a la sesión existente en lugar de abrir cada una su propia conexión. Al salir de una pantalla hay un periodo breve de espera antes de cerrar el recurso cuando ya no lo usa ninguna otra. Este diseño reduce cortes al cambiar entre pantallas o cambiar la orientación del teléfono.

La sesión compartida no elimina la restricción del firmware de un cliente de control. Si un navegador u otra aplicación se conecta al mismo servicio, la conexión anterior puede terminar. Una pantalla que muestra reconexión indica que la aplicación está intentando recuperar el enlace; debe esperarse a confirmar el estado antes de ordenar movimiento.

## Conexión y selección de perfil

El módulo de control abre el WebSocket en la dirección configurada. Envía PING periódicamente y utiliza PONG para observar la respuesta del enlace. Mientras se mantiene presionado un control manual repite la orden de movimiento; al soltarlo envía STOP. La configuración del proyecto usa 800 ms para el sondeo y 250 ms para la repetición de movimiento, valores que no equivalen a una medición de latencia total.

La aplicación solicita un perfil y espera la confirmación correspondiente. Si una interrupción provoca que el firmware regrese a AUTO, la aplicación vuelve a solicitar el perfil deseado. No debe suponerse que la selección visible fue aceptada si falta la confirmación. Para iniciar una ruta se exige el perfil AUTO; una falla de comunicación puede impedir el inicio o cancelar la ejecución.

## Trabajo entre Android y el núcleo

La interfaz está escrita en Kotlin y el núcleo de navegación en C. JNI es el mecanismo que permite que la aplicación llame funciones nativas y reciba resultados. El puente transforma datos entre ambos lados; la lógica del navegador se conserva en el núcleo, que también puede compilarse en una computadora para pruebas.

El ciclo semiautónomo reúne la ruta, la calibración y el resultado disponible de percepción, llama al navegador y transmite los comandos que corresponden. La separación es útil para ensayar el algoritmo sin teléfono, pero la aplicación real añade sus propias condiciones de sesión, pantalla, conexión y cámara. Una prueba del núcleo no cubre automáticamente esas condiciones.

## Estados de las capturas

Las capturas muestran momentos distintos. La vista conectada ilustra un enlace activo; la vista de reconexión contiene un aviso de que el teléfono no está en la red del carro; la de ajustes muestra una sesión sin conexión. Son ejemplos de interfaz tomados en circunstancias diferentes y no una secuencia que demuestre una ruta ejecutada.

Cuando el control está disponible y falla solo el video, el canal de parada puede seguir funcionando porque es independiente. Sin embargo, no se debe iniciar o continuar una ruta que requiere percepción si no llegan cuadros confiables. En todos los casos se conserva una forma física de cortar la alimentación de los motores.

Dónde está en el código: `MainActivity.kt`, `SesionCarro.kt`, `SesionVideo.kt`, `ModoSemiautonomo.kt`.
