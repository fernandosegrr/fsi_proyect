# Hardware y conexiones

El carro reúne tracción, sensores y dos placas con funciones distintas. Este capítulo identifica las conexiones principales y las precauciones necesarias para revisarlas; los detalles de protocolo se explican más adelante.

## Componentes

El chasis tiene cuatro motores TT, dos por cada canal del controlador TC1508A. La ESP32 DevKit controla el driver y consulta dos sensores: el MPU6050, utilizado como giroscopio, y el VL53L0X, sensor láser de distancia frontal. La cámara es una Freenove ESP32-WROVER con sensor GC0308. El teléfono Android ejecuta la aplicación.

La DevKit y la cámara disponen de powerbanks independientes. Cuatro pilas AA alimentan el circuito de motores. Antes de intervenir el cableado deben retirarse las pilas y desconectarse las baterías USB.

## Conexiones de control

La DevKit utiliza GPIO 27 y 26 para las entradas del canal izquierdo del driver, y GPIO 25 y 33 para el canal derecho. Los sensores comparten el bus I2C, una conexión de datos entre circuitos, mediante SDA en GPIO 21 y SCL en GPIO 22. El firmware configura GPIO 2 para un pulso periódico de mantenimiento de la batería portátil; su conexión y eficacia físicas están **NO MEDIDAS**.

El positivo del portapilas se conecta a la alimentación del driver, no a un pin de la DevKit. El negativo del portapilas, la tierra del driver y la tierra de la DevKit deben compartir referencia para las señales de control. El consumo de los motores, incluido el de bloqueo, está **NO MEDIDO**; las capacidades nominales del driver no demuestran que las corrientes reales sean seguras.

## Revisión de conexiones

El MPU6050 y el VL53L0X comparten SDA y SCL; cada dispositivo responde a su propia dirección en el bus. Las conexiones deben estar firmes y el cableado no debe rozar las ruedas. Para revisar el montaje se desconectan las fuentes de alimentación antes de manipular los conductores; la comprobación de continuidad permite buscar cortocircuitos antes de energizar el sistema.

El driver tiene dos canales y cada canal alimenta un par de motores en paralelo. Si el sentido de un lado no corresponde a la orden, se revisa la polaridad del par y la correspondencia de entradas. No se debe inferir que el carro está seguro para operar con ruedas bloqueadas a partir de una prueba con ruedas libres.

Dónde está en el código: `config.h`, `firmware_carro.ino`, `sensores.c`.
