# Referencia técnica de archivos

Esta referencia complementa la explicación por subsistemas. Identifica archivos de implementación propios, sus responsabilidades y sus relaciones principales. Un archivo de cabecera declara una interfaz; el archivo C correspondiente implementa esa interfaz. El nombre de un archivo describe su función en esta revisión, no garantiza que todas sus funciones estén activas en todos los modos.

Los archivos de recursos gráficos, bibliotecas externas y productos compilados se explican por familia al final. No se imprimen claves ni el contenido de archivos locales de secretos. Para conocer parámetros y excepciones, consulte también los capítulos de firmware, navegación y pruebas.

## Firmware: arranque, configuración y seguridad

**firmware_carro/firmware_carro.ino.** Es el punto de entrada de Arduino de la DevKit. Conecta WiFi, servidor, bucle y operaciones reales de hardware con los módulos C; conviene seguirlo para localizar la inicialización y la ejecución periódica.

**firmware_carro/config.h.** Concentra pines, tiempos y parámetros del firmware. Un cambio aquí puede afectar conexiones, límites o comportamiento; la definición de un valor no demuestra por sí misma que se haya validado en el carro.

**firmware_carro/carro.h.** Declara la interfaz de la lógica de control y las operaciones que debe proporcionar el hardware. Permite probar decisiones de movimiento sin depender directamente de Arduino.

**firmware_carro/carro.c.** Interpreta comandos, gestiona perfiles, cola y vigilancia, y decide las órdenes para motores. Revise aquí el tratamiento de STOP y las excepciones del temporizador antes de modificar el protocolo.

**firmware_carro/hw_sensores.h.** Declara las operaciones de hardware requeridas por la capa de sensores. Separa las decisiones de los detalles concretos de acceso a la placa.

**firmware_carro/pagina.h.** Contiene la página web integrada que sirve el firmware. Es una interfaz alternativa y sus controles deben interpretarse junto con las órdenes que acepta el controlador.

**firmware_carro/pagina.sha256.** Guarda una huella de referencia de la página. Su utilidad es comprobar consistencia con la versión esperada, no verificar el estado físico del carro.

**firmware_carro/secretos.ejemplo.h.** Es una plantilla de configuración local de red. Los valores reales se mantienen fuera de la documentación pública y no deben copiarse al manual.

## Firmware: sensores y giro

**firmware_carro/imu.h.** Declara la interfaz, las estructuras y los límites del módulo que estima el ángulo a partir de la velocidad angular del MPU6050 y su sesgo. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**firmware_carro/tof.h.** Declara la interfaz, las estructuras y los límites del módulo que consulta y clasifica las lecturas de distancia del VL53L0X. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**firmware_carro/giro.h.** Declara la interfaz, las estructuras y los límites del módulo que controla una maniobra de giro usando la orientación estimada y un límite de tiempo. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**firmware_carro/sensores.h.** Declara la interfaz, las estructuras y los límites del módulo que coordina el giroscopio, el láser y el controlador de giro con la lógica del carro. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**firmware_carro/imu.c.** Implementa el módulo que estima el ángulo a partir de la velocidad angular del MPU6050 y su sesgo. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**firmware_carro/tof.c.** Implementa el módulo que consulta y clasifica las lecturas de distancia del VL53L0X. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**firmware_carro/giro.c.** Implementa el módulo que controla una maniobra de giro usando la orientación estimada y un límite de tiempo. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**firmware_carro/sensores.c.** Implementa el módulo que coordina el giroscopio, el láser y el controlador de giro con la lógica del carro. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

## Cámara y recuperación

**camara_prueba/camara_prueba.ino.** Configura la placa Freenove, la captura y los servicios de video. También coordina la red y la entrega de cuadros; es el punto de partida para investigar formatos, rendimiento y conexión.

**camara_prueba/recuperacion.h.** Declara la interfaz de las decisiones de recuperación ante fallos de cámara o conexión, separada del hardware.

**camara_prueba/recuperacion.c.** Implementa decisiones y tiempos de recuperación para que puedan comprobarse en computadora. El resultado depende del estado y de las señales que recibe desde el programa de cámara.

**camara_prueba/secretos.ejemplo.h.** Es la plantilla de configuración de la red a la que se conecta la cámara. No contiene una demostración de qué configuración está cargada actualmente en una placa.

**camara_prueba/src/JPEGENC/.** Agrupa una biblioteca externa para compresión JPEG y su licencia. Sus archivos se conservan como dependencia de terceros; no deben atribuirse como implementación original del equipo.

## Núcleo C: ruta, calibración y navegación

**nucleo_c/calibracion.h.** Declara la interfaz, las estructuras y los límites del módulo que convierte medidas y parámetros de calibración en tiempos de movimiento. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/calgiro.h.** Declara la interfaz, las estructuras y los límites del módulo que organiza la calibración de giros con lecturas del giroscopio. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/trayectoria.h.** Declara la interfaz, las estructuras y los límites del módulo que transforma puntos de cuadrícula en pasos de avance y giro. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/ruta_editor.h.** Declara la interfaz, las estructuras y los límites del módulo que mantiene la lógica de edición de puntos y operaciones sobre la ruta. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/navegador.h.** Declara la interfaz, las estructuras y los límites del módulo que ejecuta la máquina de estados de seguimiento, esquive y reincorporación. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/simulador_navegador.h.** Declara la interfaz, las estructuras y los límites del módulo que representa un carro y un entorno virtuales para ensayar el navegador. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/sim_ruta.h.** Declara la interfaz, las estructuras y los límites del módulo que ejecuta una ruta con el navegador real y el vehículo simulado para obtener su recorrido. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/calibracion.c.** Implementa el módulo que convierte medidas y parámetros de calibración en tiempos de movimiento. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/calgiro.c.** Implementa el módulo que organiza la calibración de giros con lecturas del giroscopio. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/trayectoria.c.** Implementa el módulo que transforma puntos de cuadrícula en pasos de avance y giro. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/ruta_editor.c.** Implementa el módulo que mantiene la lógica de edición de puntos y operaciones sobre la ruta. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/navegador.c.** Implementa el módulo que ejecuta la máquina de estados de seguimiento, esquive y reincorporación. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/simulador_navegador.c.** Implementa el módulo que representa un carro y un entorno virtuales para ensayar el navegador. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/sim_ruta.c.** Implementa el módulo que ejecuta una ruta con el navegador real y el vehículo simulado para obtener su recorrido. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

## Núcleo C: percepción y video

**nucleo_c/obstaculos.h.** Declara la interfaz, las estructuras y los límites del módulo que analiza color de piso y clasifica zonas de la imagen. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/corredor.h.** Declara la interfaz, las estructuras y los límites del módulo que proyecta la huella del vehículo sobre la imagen para evaluar la zona de paso. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/cualquiera.h.** Declara la interfaz, las estructuras y los límites del módulo que procesa el modo de detección de cualquier objeto del modelo. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/fusion.h.** Declara la interfaz, las estructuras y los límites del módulo que combina las señales de color y del modelo de objetos para una decisión. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/mascara_vision.h.** Declara la interfaz, las estructuras y los límites del módulo que gestiona una máscara para excluir artefactos fijos de la imagen. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/movimiento_piso.h.** Declara la interfaz, las estructuras y los límites del módulo que compara el movimiento de bloques del piso entre cuadros. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/movimiento_visual.h.** Declara la interfaz, las estructuras y los límites del módulo que estima giro visual mediante cuadros consecutivos. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/paquete_celdas.h.** Declara la interfaz, las estructuras y los límites del módulo que construye e interpreta el paquete UDP de promedios de color por celda. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/pisos_sinteticos.h.** Declara la interfaz, las estructuras y los límites del módulo que genera escenas sintéticas de piso para pruebas en computadora. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/prueba_obstaculo.h.** Declara la interfaz, las estructuras y los límites del módulo que calcula métricas de retraso y distancia en pruebas de parada por cámara. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/latencia_video.h.** Declara la interfaz, las estructuras y los límites del módulo que compara tiempos de respuesta del control con y sin video. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/video_stats.h.** Declara la interfaz, las estructuras y los límites del módulo que reúne estadísticas temporales de llegada de cuadros. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/sesion_compartida.h.** Declara la interfaz, las estructuras y los límites del módulo que gestiona el uso de un recurso compartido por varias pantallas. Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama.

**nucleo_c/obstaculos.c.** Implementa el módulo que analiza color de piso y clasifica zonas de la imagen. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/corredor.c.** Implementa el módulo que proyecta la huella del vehículo sobre la imagen para evaluar la zona de paso. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/cualquiera.c.** Implementa el módulo que procesa el modo de detección de cualquier objeto del modelo. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/fusion.c.** Implementa el módulo que combina las señales de color y del modelo de objetos para una decisión. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/mascara_vision.c.** Implementa el módulo que gestiona una máscara para excluir artefactos fijos de la imagen. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/movimiento_piso.c.** Implementa el módulo que compara el movimiento de bloques del piso entre cuadros. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/movimiento_visual.c.** Implementa el módulo que estima giro visual mediante cuadros consecutivos. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/paquete_celdas.c.** Implementa el módulo que construye e interpreta el paquete UDP de promedios de color por celda. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/pisos_sinteticos.c.** Implementa el módulo que genera escenas sintéticas de piso para pruebas en computadora. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/prueba_obstaculo.c.** Implementa el módulo que calcula métricas de retraso y distancia en pruebas de parada por cámara. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/latencia_video.c.** Implementa el módulo que compara tiempos de respuesta del control con y sin video. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/video_stats.c.** Implementa el módulo que reúne estadísticas temporales de llegada de cuadros. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

**nucleo_c/sesion_compartida.c.** Implementa el módulo que gestiona el uso de un recurso compartido por varias pantallas. Lea sus funciones junto con la cabecera homónima y sus pruebas para entender las condiciones, los cálculos y los casos límite.

## Compilación y conexión entre C y Android

**nucleo_c/Makefile.** Define objetivos de compilación y pruebas del núcleo en computadora. Es útil para conocer qué archivos se enlazan en cada ejecutable y cómo se ejecuta la suite.

**app_android/app/src/main/cpp/CMakeLists.txt.** Declara la construcción de la biblioteca nativa de Android y sus archivos de C. Si se agrega un módulo nativo, debe comprobarse su inclusión aquí.

**app_android/app/src/main/cpp/puente.c.** Implementa el puente JNI entre Kotlin y el núcleo en C. Traduce llamadas, tipos y resultados entre ambos entornos; una discrepancia de firmas puede impedir que la aplicación use el módulo nativo.

**app_android/app/build.gradle.kts.** Configura el módulo Android, sus versiones de SDK, dependencias y compilación nativa. Debe revisarse junto con CMake y la configuración general de Gradle.

**app_android/app/src/main/AndroidManifest.xml.** Declara componentes y configuración de la aplicación para Android. Es la referencia para saber qué actividades y capacidades se registran en esta revisión.

## Android: archivos Kotlin de aplicación

**AcercaFragment.kt.** Presenta información del proyecto, versión y licencias. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**Ajustes.kt.** Almacena preferencias generales de la aplicación. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**AjustesFragment.kt.** Ofrece la pantalla para consultar o modificar ajustes generales. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**AlmacenRutas.kt.** Guarda rutas con nombre, borrador e historial en el teléfono. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**AppCarro.kt.** Inicializa recursos de aplicación compartidos entre pantallas. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**AsistenteFragment.kt.** Guía la primera conexión y presenta los modos de uso. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**AyudaFragment.kt.** Muestra ayuda y permite volver al asistente. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**BenchmarkActivity.kt.** Presenta la medición de rendimiento del detector de objetos. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**BenchmarkObstaculos.kt.** Organiza la medición de rendimiento del detector por color. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**CalibracionActivity.kt.** Guía la medición de parámetros para convertir movimiento en tiempo. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**CalibracionGuardada.kt.** Persiste y recupera la calibración del vehículo. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**CalibrarGiroActivity.kt.** Dirige el procedimiento de calibración de giro y presenta su diagnóstico. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**CamaraObstaculos.kt.** Procesa imágenes para detección de obstáculos y permite utilizar la cámara del teléfono como fuente. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**ConfigVision.kt.** Administra los parámetros ajustables del procesamiento visual. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**ControlLogica.kt.** Determina qué comando de control enviar y cuándo. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**CuadriculaView.kt.** Dibuja una cuadrícula de interacción para rutas. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**DetectorTflite.kt.** Ejecuta el detector de objetos mediante TensorFlow Lite. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**Diseno.kt.** Reúne componentes y decisiones visuales reutilizables. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**EditorRutaView.kt.** Muestra la cuadrícula de edición y el recorrido simulado. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**EjecutarRutaFragment.kt.** Presenta la ejecución de ruta, el estado y el video. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**Enlace.kt.** Implementa el enlace WebSocket de control. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**EstadisticaModo.kt.** Registra estadísticas para comparar modos de video. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**Estadisticas.kt.** Resume listas de tiempos de medición. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**IlustracionCalibracion.kt.** Dibuja las ilustraciones usadas durante la calibración. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**ImagenesPrueba.kt.** Proporciona imágenes JPEG para pruebas de rendimiento. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**InfoDispositivo.kt.** Recoge datos del teléfono usados en reportes de rendimiento. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**InicioFragment.kt.** Presenta estado de carro y cámara, conexión y accesos. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**MainActivity.kt.** Actúa como armazón principal y hospeda la navegación entre pantallas. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**ManualFragment.kt.** Presenta video y controles de conducción manual. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**MasFragment.kt.** Agrupa accesos a calibración, diagnóstico, ajustes y ayuda. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**Mensajes.kt.** Centraliza avisos y mensajes de error. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**ModeloEnHilo.kt.** Ejecuta el modelo de objetos en un hilo separado del detector de color. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**ModoSemiautonomo.kt.** Coordina el ciclo del navegador nativo con los comandos que se envían al carro. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**PantallasLog.kt.** Registra eventos asociados a la apertura de pantallas. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**PruebaFinalFragment.kt.** Presenta una secuencia guiada de pruebas del carro. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**RutasFragment.kt.** Permite preparar, simular y ejecutar rutas. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**RutasPredefinidas.kt.** Define trazados preestablecidos usados en pruebas. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**SensoresActivity.kt.** Presenta el estado y diagnóstico del giroscopio y el sensor de distancia. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**SesionCarro.kt.** Comparte la conexión de control entre pantallas. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**SesionVideo.kt.** Comparte una sesión de video entre consumidores. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**VideoCarro.kt.** Recibe el video de la cámara del carro por la red. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**VisionActivity.kt.** Muestra diagnóstico visual y controles de ajuste del detector. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**VisorJpeg.kt.** Presenta cuadros JPEG de la sesión de video. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**VistaDeteccion.kt.** Dibuja la imagen y las marcas de detección. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

**VistaNumerosDeteccion.kt.** Superpone indicadores numéricos de detección sobre el video. Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro.

## Android: declaraciones para el núcleo nativo

**CalGiroNativa.kt.** Declara las llamadas de Kotlin al módulo calgiro del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**CalibracionNativa.kt.** Declara las llamadas de Kotlin al módulo calibracion del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**CorredorNativa.kt.** Declara las llamadas de Kotlin al módulo corredor del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**CualquieraNativa.kt.** Declara las llamadas de Kotlin al módulo cualquiera del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**FlujoNativa.kt.** Declara las llamadas de Kotlin al módulo movimiento_piso del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**FusionNativa.kt.** Declara las llamadas de Kotlin al módulo fusion del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**LatenciaVideoNativa.kt.** Declara las llamadas de Kotlin al módulo latencia_video del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**MovimientoVisualNativa.kt.** Declara las llamadas de Kotlin al módulo movimiento_visual del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**NavegadorNativa.kt.** Declara las llamadas de Kotlin al módulo navegador del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**ObstaculosNativa.kt.** Declara las llamadas de Kotlin al módulo obstaculos del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**PaqueteCeldasNativa.kt.** Declara las llamadas de Kotlin al módulo paquete_celdas del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**PruebaObstaculoNativa.kt.** Declara las llamadas de Kotlin al módulo prueba_obstaculo del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**RutaEditorNativa.kt.** Declara las llamadas de Kotlin al módulo ruta_editor del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**SesionNativa.kt.** Declara las llamadas de Kotlin al módulo sesion_compartida del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**SimRutaNativa.kt.** Declara las llamadas de Kotlin al módulo sim_ruta del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**TrayectoriaNativa.kt.** Declara las llamadas de Kotlin al módulo trayectoria del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

**VideoStatsNativa.kt.** Declara las llamadas de Kotlin al módulo video_stats del núcleo C. La declaración debe corresponder con el puente JNI y la interfaz C; consulte los tres puntos al cambiar parámetros o tipos.

## Pruebas, recursos y archivos auxiliares

**nucleo_c/pruebas_*.c.** Cada archivo agrupa pruebas del módulo o escenario indicado en su nombre. Por ejemplo, pruebas_navegador.c examina el navegador y pruebas_reincorporacion_j15.c el escenario de reincorporación. Son pruebas de software: no constituyen nuevas corridas físicas.

**app_android/app/src/androidTest/.** Contiene pruebas instrumentadas de Android. Su ejecución y resultados dependen del dispositivo o emulador y de la configuración empleada.

**app_android/app/src/main/res/layout/ y layout-land/.** Definen la composición visual de pantallas y sus variantes de orientación. Se relacionan con las clases de actividad, fragmento y vista que cargan esos recursos.

**app_android/app/src/main/res/values/ y values-night/.** Reúnen textos, colores, dimensiones y estilos, incluidas variantes de tema. Separar estos recursos de la lógica facilita mantener la presentación.

**app_android/app/src/main/res/drawable/ y font/.** Contienen iconos, formas y fuentes de la interfaz. Son recursos de presentación; no implementan las decisiones de navegación ni control.

**app_android/app/src/main/assets/efficientdet_lite0.tflite.** Es el archivo del modelo de objetos usado por el detector. La presencia del modelo no demuestra su exactitud para cualquier objeto o entorno.

**scripts/.** Contiene herramientas de compilación, carga, verificación y pruebas. Antes de ejecutar cada herramienta deben leerse sus argumentos y efectos: algunas interactúan con dispositivos o pueden escribir firmware.

**README.md y archivos README de subproyectos.** Describen puntos de entrada y procedimientos para el conjunto o una carpeta concreta. Cuando exista una discrepancia, compare su fecha y alcance con el código y los reportes de la revisión.

**carpeta del manual.** Contiene capítulos, generador, auditoría de afirmaciones y el PDF de este manual. La documentación explica el sistema, pero no reemplaza el código ni demuestra que una versión esté instalada en el equipo físico.

## Herramientas de apoyo, archivo por archivo

**scripts/analizar_obj_any.py.** Analiza registros de detección de cualquier objeto y calcula indicadores por umbral y tipo de pasada.

**scripts/buscar_nombre_anterior.py.** Busca referencias al nombre anterior del proyecto en los archivos incluidos en su búsqueda.

**scripts/camara_falsa.py.** Simula servicios de cámara con imágenes sintéticas para probar la aplicación sin el vehículo.

**scripts/camara_falsa_udp.c.** Simula la comunicación UDP del modo de celdas de la cámara.

**scripts/cargar.ps1.** Compila y, salvo que se limite con -SoloCompilar, puede cargar firmware; algunas identificaciones automáticas pueden sobrescribir la placa con una sonda.

**scripts/compilar_app.ps1.** Compila la aplicación de depuración y presenta un resumen de avisos y errores.

**scripts/contraste.py.** Comprueba el contraste de combinaciones seleccionadas de colores de la aplicación.

**scripts/generar_fuentes_inter.py.** Prepara versiones estáticas de la fuente Inter para los recursos de la interfaz.

**scripts/instalar_app_adb.py.** Instala la aplicación por ADB e interactúa con avisos del teléfono; modifica el dispositivo conectado.

**scripts/probar_modos_video.py.** Prueba los modos de video de la aplicación con una cámara falsa y un teléfono mediante ADB.

**scripts/probar_pantallas.py.** Abre pantallas mediante ADB y revisa registros para detectar errores de apertura.

**scripts/probar_version.py.** Comprueba los lectores de versión mediante pruebas sin USB ni teléfono.

**scripts/qa_captura.py.** Abre una pantalla en el teléfono mediante ADB y guarda una captura para inspección visual.

**scripts/verificar_pagina.py.** Compara la página integrada del firmware con las huellas de referencia.

**scripts/verificar_sensores.py.** Realiza consultas físicas de sensores por puerto serie; requiere el dispositivo y no se ejecutó para esta edición.

**scripts/verificar_version.py.** Compara las versiones disponibles del firmware y de la aplicación con Git; su opción de solo APK evita la lectura del puerto serie.

**scripts/sonda_placa/sonda_placa.ino.** Programa mínimo utilizado para identificar una placa en determinados procesos de carga. Al grabarse sustituye el programa previamente instalado.

## Pruebas del núcleo, archivo por archivo

**pruebas_calgiro.c.** Reúne casos relacionados con calibración de giro. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_calibracion.c.** Reúne casos relacionados con conversión de medidas y tiempos de calibración. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_celdas.c.** Reúne casos relacionados con codificación y lectura del paquete de celdas. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_corredor.c.** Reúne casos relacionados con geometría del corredor. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_cualquiera.c.** Reúne casos relacionados con modo de cualquier objeto. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_fusion.c.** Reúne casos relacionados con reglas de fusión de señales. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_latencia_video.c.** Reúne casos relacionados con cálculo de latencia con y sin video. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_mascara.c.** Reúne casos relacionados con máscara de visión. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_movimiento.c.** Reúne casos relacionados con estimación de movimiento visual. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_movimiento_piso.c.** Reúne casos relacionados con flujo del piso. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_navegador.c.** Reúne casos relacionados con estados y decisiones del navegador. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_obstaculos.c.** Reúne casos relacionados con detección por color del piso. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_piso.c.** Reúne casos relacionados con comportamiento del detector ante escenas de piso. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_pisos_dificiles.c.** Reúne casos relacionados con escenarios sintéticos de piso difíciles. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_prueba_obstaculo.c.** Reúne casos relacionados con cálculos de la prueba de obstáculos. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_reincorporacion_j15.c.** Reúne casos relacionados con escenarios de reincorporación J15. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_ruta_editor.c.** Reúne casos relacionados con operaciones del editor de ruta. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_sesion_compartida.c.** Reúne casos relacionados con gestión de sesiones compartidas. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_sim_ruta.c.** Reúne casos relacionados con ejecución de rutas simuladas. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_trayectoria.c.** Reúne casos relacionados con conversión de ruta en pasos. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.

**pruebas_video_stats.c.** Reúne casos relacionados con estadísticas de cuadros de video. Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva.


## Lectura guiada: del comando a los motores

En `carro.h` se distinguen dos grupos de funciones. Las operaciones con prefijo `hw_` describen servicios que la plataforma debe proporcionar, como aplicar PWM y leer el tiempo. Las operaciones con prefijo `carro_` describen la lógica del controlador. Esta frontera permite sustituir el hardware por implementaciones de prueba sin cambiar las decisiones centrales.

El recorrido comienza con `carro_encolar_comando`, continúa con `carro_procesar_cola` y llega a la interpretación de la orden. `carro_detener` representa una acción de parada y `carro_vigilar_comunicacion` comprueba el plazo de vigilancia. Para entender una incidencia hay que revisar también cómo el archivo Arduino invoca esas funciones y qué llamadas de hardware realiza: leer una sola función fuera de contexto puede ocultar condiciones de ejecución.

Al modificar el procesamiento de mensajes, compruebe prioridad de STOP, saturación de cola, cambios de perfil y semántica del watchdog. El tratamiento de PING y de otros textos no es idéntico. Una prueba en computadora ayuda a detectar regresiones de lógica; una prueba física posterior verifica la integración con el hardware.

## Lectura guiada: sensores y giro

`sensores.h` declara `sensores_iniciar`, `sensores_actualizar` y funciones para recibir consultas o informar métricas del bucle. `sensores.c` coordina módulos especializados: `imu.c` calcula orientación a partir de velocidad angular; `tof.c` gestiona las lecturas de distancia; `giro.c` controla una maniobra angular. Cada módulo tiene su propia cabecera con la interfaz y los estados necesarios.

La diferencia entre sensor disponible y lectura válida es fundamental. Un dispositivo detectado en I2C puede no producir una distancia válida en una situación concreta. Por eso la telemetría incluye información de estado además de valores. Antes de cambiar un umbral de parada, hay que revisar la validez, el perfil y la autoridad de parada del láser, además del valor numérico.

## Lectura guiada: de la ruta a la decisión

`trayectoria.h` declara variantes de `trayectoria_calcular` que convierten puntos en pasos y aceptan distintas condiciones de orientación o calibración. La salida no es una posición real medida: es una secuencia calculada a partir de la ruta y los parámetros. Si una trayectoria es incorrecta, revise primero los puntos, la orientación de partida y la calibración utilizada.

`navegador.h` declara el estado `Navegador` y operaciones como `navegador_iniciar`, `navegador_poner_calibracion`, `navegador_cargar_ruta`, `navegador_comenzar` y `navegador_cancelar`. Las funciones de aporte reciben información de sensores y giros. `navegador.c` usa esos datos y el tiempo para avanzar por los estados. En Android, `ModoSemiautonomo.kt` coordina la llamada al núcleo y el envío de la orden resultante.

Para seguir un fallo, registre entrada, estado anterior, tiempo, salida y estado siguiente. Compare después con `pruebas_navegador.c` o con una simulación adecuada. El simulador utiliza el navegador real con un entorno modelado; sus resultados no prueban por sí mismos el comportamiento del carro físico.

## Lectura guiada: imagen, color y fusión

`obstaculos.h` expone configuración, estructuras y operaciones para analizar imágenes o celdas. `obstaculos.c` implementa el modelo y el análisis. El aprendizaje utiliza un acumulador y produce un modelo de piso; por ello la escena que se presenta durante el aprendizaje importa. Si una superficie incorrecta se incorpora al modelo, una clasificación posterior puede resultar engañosa.

`fusion.h` declara la configuración de clases, la conversión de cajas a zonas y funciones de decisión como `fusion_decidir`. `fusion.c` combina el resultado del detector con las detecciones disponibles. Conviene registrar qué entradas estaban vigentes y qué opciones estaban activas; ver una caja dibujada no demuestra que se usara exactamente la misma información en una decisión anterior.

`DetectorTflite.kt` ejecuta el modelo y `ModeloEnHilo.kt` lo organiza en un hilo propio. El procesamiento de imagen y las reglas de fusión tienen ritmos y datos distintos. Al analizar latencia, identifique el instante de captura, procesamiento y decisión, y no atribuya a toda la cadena una medición de una sola etapa.

## Lectura guiada: Kotlin, JNI y C

Los archivos terminados en `Nativa.kt` declaran operaciones disponibles desde Kotlin. `puente.c` enlaza esas llamadas con las funciones del núcleo y `CMakeLists.txt` incluye los archivos C en la biblioteca nativa. Para añadir un parámetro a una operación hay que revisar coherentemente los tipos y las firmas de los tres lugares, además de quienes llamen a la función.

Una compilación correcta ayuda a detectar algunas incompatibilidades, pero no sustituye las pruebas de comportamiento. Después de un cambio en el puente, conviene comprobar también los valores límite y la gestión de errores desde la aplicación y desde las pruebas del núcleo.
