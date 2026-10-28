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

Dónde está en el código: `MainActivity.kt`, `SesionCarro.kt`, `SesionVideo.kt`, `ModoSemiautonomo.kt`.
