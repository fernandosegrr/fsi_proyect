# Proceso de desarrollo

El proyecto evolucionó desde un control de carro basado en ESP32 hacia una arquitectura distribuida con pruebas de software y sensores. Esta sección resume las decisiones que ayudan a comprender el diseño actual.

La lógica de control se separó de las funciones de hardware para poder ejecutar pruebas con implementaciones simuladas. El núcleo de navegación se escribió en C y se integró en Android, lo que permite reutilizar la lógica en computadora y teléfono sin mantener dos algoritmos independientes.

Después se incorporaron el giroscopio y el sensor frontal de distancia. El giroscopio permite cerrar giros por ángulo; el láser agrega una condición de parada en avance. La cámara Freenove quedó en otra placa porque la captura y la compresión tienen un costo de procesamiento distinto del control de tracción.

Las pruebas físicas mostraron variaciones de movimiento y de orientación. La calibración manual redujo el desvío de una recta en las condiciones registradas. En el análisis de reincorporación posterior se identificó que la estimación proyectaba los avances con un rumbo fijo. Se cambió la integración para aprovechar el rumbo observado durante el avance y se probó la corrección en simulación. Su desempeño físico continúa pendiente.

El diseño no dispone de encoders ni de una referencia externa de posición. La elección de avance por tiempo simplifica el montaje y permite trabajar con los componentes disponibles, pero exige calibración y deja incertidumbre acumulada. Las simulaciones ayudan a explorar la lógica sin mover el carro; no reemplazan la validación de las hipótesis sobre tracción y percepción.

## Separar el control del hardware

En el prototipo inicial, la lógica de movimiento, el servidor y el acceso a pines estaban concentrados en el programa de la placa. La reorganización introdujo una interfaz de hardware para que la lógica de control pudiera invocarse con funciones reales en la ESP32 o con funciones falsas en computadora. Esto permitió ensayar estados y respuestas sin que cada prueba necesitara un vehículo energizado.

El mismo criterio se aplicó al núcleo de navegación. Las operaciones de ruta y calibración se conservaron en C; la aplicación Android las invoca mediante una interfaz nativa. Así se puede ensayar la lógica con un simulador y, a la vez, usarla en la interfaz del teléfono. Las pruebas de cada capa comprueban propiedades distintas: una suite en computadora no incluye automáticamente el comportamiento de WiFi, ruedas y piso.

## Incorporar sensores y calibrar

La navegación original dependía del tiempo para avanzar y girar. El giroscopio incorporó una observación de velocidad angular para controlar los giros por ángulo; el sensor de distancia frontal añadió una condición de parada en avance. La separación entre ambos sensores importa: el giroscopio observa orientación y el láser una distancia en su dirección de montaje.

El ajuste de balance MANUAL responde a una asimetría observada en el carro. En una recta de 150 cm el comentario de configuración registra 14.5 cm de desvío con ambos lados a 255 y 3 cm con el ajuste de 252 a la izquierda. El valor AUTO se obtuvo al escalar la relación y permanece sin medición física. La separación entre un parámetro calculado y uno medido evita atribuir a ambos la misma evidencia.

## Cámara y procesamiento local

El GC0308 entrega cuadros que requieren compresión JPEG por software. El proyecto probó combinaciones de formato, reloj y resolución para evaluar el costo de captura y compresión. La configuración actual usa HQVGA; el barrido local permitió comparar su rendimiento con QVGA bajo condiciones concretas. La decisión intercambia detalle espacial por una mayor frecuencia y menor edad de cuadro en ese ensayo.

Mantener la placa de cámara separada de la DevKit reduce la interacción directa entre el trabajo de video y el control de motores. El teléfono recibe el video y ejecuta el procesamiento necesario para la aplicación. Este reparto también introduce dependencia de dos enlaces: la disponibilidad del video y la del control deben observarse por separado.

## Diagnóstico de reincorporación

En las pruebas históricas con una caja se observaron desviaciones y esquives adicionales. La revisión posterior encontró que la estimación proyectaba un avance completo usando un rumbo fijo, aunque el giroscopio transmitía información durante el recorrido. El cambio integra incrementos con el rumbo observado cuando la lectura es válida; no convierte la distancia por tiempo en una distancia medida.

Para comparar el cambio se ejecutaron escenarios simulados con una posición virtual independiente de la estimación. La mejora del error lateral en ese modelo respalda la modificación de la lógica, pero los propios registros señalan que el escenario no reproduce exactamente las desviaciones históricas ni toda la percepción real. La pregunta sobre la mejora en el carro queda abierta hasta realizar el ensayo físico.

## Alternativas y compromisos

El diseño utiliza la red local y el teléfono para la interacción y el cálculo, sin requerir un servidor externo para el recorrido. No hay encoders ni localización externa; por ello la distancia depende de la calibración y no puede corregirse todo deslizamiento. El láser agrega una parada frontal independiente de la clasificación de imagen, pero no cubre todo el entorno y su autoridad es configurable.

El arranque a potencia plena durante un intervalo corto responde al comportamiento observado cuando los motores zumbaban sin arrancar a valores menores. Una rampa progresiva puede ser deseable en otros montajes, pero el cambio de estrategia debe evaluarse con el vehículo y sus cargas reales. Estas decisiones son compromisos del prototipo, no garantías universales de desempeño.

Dónde está en el código: `carro.c`, `trayectoria.c`, `navegador.c`, `simulador_navegador.c`.
