# Firmware del carro

El firmware recibe las órdenes de la aplicación y las convierte en señales para los motores. También consulta sensores y aplica límites de seguridad. Separar la lógica de control de las funciones que acceden al hardware permite probar parte del comportamiento en una computadora.

## Control y perfiles

Las órdenes llegan por WebSocket y se incorporan a una cola; el ciclo principal procesa esa cola y actualiza los motores. La modulación por ancho de pulso, PWM, regula la potencia mediante una señal que cambia rápidamente entre encendido y apagado. La configuración usa 1500 Hz y resolución de ocho bits.

Hay perfiles MANUAL y AUTO. Los valores predeterminados son 252 a la izquierda y 255 a la derecha en MANUAL, y 207 y 210 en AUTO. Esos valores pueden cambiar por ajustes guardados. El comentario de configuración registra un desvío de 3 cm en una prueba de 150 cm con el ajuste MANUAL; el balance AUTO derivado permanece **NO MEDIDO** en el carro.

## Vigilancia y sensores

La vigilancia de comunicación está configurada para detener los motores cuando transcurren más de 1000 ms sin renovar el temporizador; un mensaje PING no lo renueva. El firmware también renueva ese temporizador para algunos mensajes que no son movimiento y para texto desconocido, por lo que esta protección no equivale a validar cada orden recibida. Un vigilante de hardware de 5000 ms busca recuperar el sistema si se bloquea el ciclo principal. Durante giros por ángulo se aplican reglas propias de duración y cancelación.

El MPU6050 aporta velocidad angular para estimar el giro. El VL53L0X mide distancia frontal; en AUTO y durante el avance, dos lecturas válidas inferiores a 120 mm activan la parada configurada. La autoridad del láser puede desactivarse y no cubre obstáculos fuera de su campo de observación.

La duración del ciclo no debe suponerse siempre inferior a 5 ms: las pruebas registraron excesos con la aplicación conectada. Las muestras son breves y no establecen por sí solas la causa.

## Secuencia de trabajo

Al iniciar, el firmware configura las salidas de motores, la red y los sensores, y parte de un estado detenido. El ciclo principal revisa los sensores y procesa las órdenes recibidas. La cola evita que el procesamiento de red y el control de motores modifiquen simultáneamente el estado de movimiento desde contextos distintos.

El perfil AUTO limita la velocidad para las rutas, mientras MANUAL conserva el máximo configurado para conducción directa. Al cambiar entre perfiles, la respuesta de confirmación permite a la aplicación comprobar que el carro aceptó el modo solicitado. Los ajustes de balance pueden guardarse y prevalecer sobre los valores iniciales.

El giroscopio estima ángulo a partir de la velocidad angular; necesita una referencia de sesgo con el carro quieto. El láser no valida por sí mismo el espacio detrás del carro. Una orden de retroceso debe considerarse con el área posterior despejada.

## Inicialización y orden de las tareas

Al encender, la placa abre la comunicación serie y registra la versión compilada. Configura el procesador, los canales PWM y el estado inicial de los motores; después prepara el bus I2C y los sensores, la vigilancia de hardware y el punto de acceso. El perfil inicial es AUTO. El servidor ofrece una página de control y un extremo WebSocket que la aplicación utiliza para intercambiar mensajes.

En el ciclo principal se atienden, entre otras tareas, la lectura de sensores, el avance de un giro por ángulo, la cola de comandos, la vigilancia de comunicación y el envío de estado. El orden tiene una finalidad: el acceso a sensores y las decisiones que cambian motores se concentran en el ciclo de control, mientras la recepción de red coloca mensajes para procesarlos. Un STOP recibido debe llegar a ese procesamiento sin quedar descartado por una ráfaga ordinaria.

La duración de una vuelta del ciclo puede consultarse mediante el diagnóstico LOOP. Un máximo aislado no describe toda la distribución de tiempos. En los ensayos históricos se observaron excesos de 5 ms con la aplicación conectada; las muestras no aislaron todas las causas. Por ello la constante de 5 ms es un objetivo de diagnóstico, no una garantía comprobada en todas las condiciones.

## Cola y prioridad de detención

La recepción WebSocket y el ciclo principal no realizan exactamente el mismo trabajo. La recepción incorpora el texto a una cola de capacidad limitada y el ciclo lo ejecuta. Si la cola se llena, la política da preferencia a conservar STOP y EBRAKE; primero puede descartar PING antiguos o mensajes que no sean de parada. Una prueba de estrés histórica de 100000 comandos registró cero STOP perdidos en ese ensayo.

El resultado anterior comprueba una propiedad de la prueba de software, no la latencia de frenado de extremo a extremo del carro real. El trayecto físico incluye recepción inalámbrica, ejecución, desactivación de señales y movimiento residual del chasis. Para comparar distancias de parada deben registrarse velocidad, superficie, alimentación y distancia medida en cada repetición.

## Perfiles, balance y arranque

Cada perfil dispone de un valor máximo y un factor de balance. El factor reduce el lado izquierdo con relación al derecho; no incrementa la potencia por encima del máximo. Para MANUAL, el ajuste predeterminado de 988 milésimas sobre 255 produce aproximadamente 252 en el lado izquierdo. Para AUTO, el ajuste de 986 sobre 210 produce 207. Esos valores son puntos de partida; la memoria no volátil puede contener ajustes guardados distintos.

Durante el arranque de un avance o retroceso, la configuración aplica potencia plena por 250 ms antes de pasar al perfil. La razón documentada es que los motores de ese montaje podían zumbar sin iniciar movimiento con una potencia menor desde reposo. El intervalo no demuestra que todas las baterías y superficies producirán la misma aceleración; la calibración debe representar el montaje usado.

El pivote utiliza un lado de tracción activo y el otro detenido. Debido a la geometría y a las diferencias entre lados, un giro izquierdo no tiene necesariamente la misma duración ni el mismo desplazamiento del centro que un giro derecho. La orientación durante los giros por ángulo se observa con el giroscopio y se informa mediante la respuesta correspondiente.

## Giro con giroscopio

El MPU6050 entrega velocidad angular. El firmware estima el ángulo integrando lecturas sucesivas después de restar el sesgo; por eso es necesario un arranque o calibración con el carro quieto. La configuración solicita 250 muestras para el cálculo de sesgo y un periodo nominal de 10 ms por muestra. El periodo configurado no prueba que cada lectura física llegue exactamente en ese intervalo.

Una orden GIRAR solicita un ángulo con signo: positivo para izquierda y negativo para derecha según la convención del proyecto. La lógica controla la maniobra, detiene el motor y espera un periodo de asentamiento para observar el resultado final. La anticipación aprendida intenta compensar el movimiento que continúa después de retirar la señal. La respuesta puede indicar éxito, tiempo agotado o error; un resultado de éxito corresponde a la lógica y tolerancias implementadas, no a una garantía de posición absoluta.

Una muestra histórica quedó contaminada cuando se sujetó el carro durante un giro: la anticipación aprendida cambió con ese resultado. Para una nueva calibración conviene registrar cada giro y evitar intervenir físicamente durante la maniobra, salvo que sea necesario detenerla. Si un resultado es inválido, debe identificarse antes de utilizarlo como evidencia de precisión.

## Lecturas de distancia y autoridad de parada

El VL53L0X mide hacia adelante mediante un principio de tiempo de vuelo de luz. El firmware consulta el sensor periódicamente y distingue mediciones válidas, ausencia de eco y fallas de comunicación. La configuración de parada en AUTO requiere dos lecturas válidas consecutivas menores de 120 mm mientras el carro avanza. Cuando esa condición se cumple, se detienen los motores y se informa el evento.

La opción de autoridad del láser puede desactivarse; una lectura disponible no implica necesariamente que esté habilitada la parada. El sensor no observa el espacio posterior ni todo el ancho del vehículo. Además, la distancia recorrida después de la orden de parada depende de la inercia, el piso y otros tiempos. El umbral configurado no es una distancia de frenado medida.

Dónde está en el código: `config.h`, `carro.c`, `firmware_carro.ino`, `sensores.c`, `imu.c`, `tof.c`, `giro.c`.
