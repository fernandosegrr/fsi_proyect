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

## Inventario funcional

La DevKit proporciona el punto de acceso inalámbrico, las salidas de control y el bus de sensores. El módulo TC1508A recibe señales de la DevKit y entrega potencia a los motores. Los cuatro motores TT se agrupan por lado: dos motores izquierdos conectados a un canal y dos derechos al otro. La placa de cámara Freenove es un segundo microcontrolador con su propio sensor y memoria; el teléfono aporta la interfaz y el cálculo de navegación.

Los elementos de alimentación son dos baterías portátiles USB, una para cada placa, y un portapilas de cuatro celdas AA para el driver. Los cables USB de datos se usan para las tareas de carga de programa y diagnóstico; una conexión adecuada para alimentación no garantiza que el cable transporte datos. Para una lista de compra con marcas, capacidades y precios no hay inventario completo respaldado: esos datos permanecen **PENDIENTES**.

## Separación entre señales y potencia

Un GPIO es una entrada o salida digital del microcontrolador. No es una fuente adecuada para alimentar directamente los motores. El puente H recibe señales de mando de bajo consumo y conmuta la alimentación que procede del portapilas. La dirección de giro se decide por cuál entrada del canal recibe la señal; el valor PWM determina la fracción de tiempo activo.

El positivo de las cuatro pilas se lleva a VDD del TC1508A. El negativo de las pilas debe compartir referencia con la tierra del driver y la de la DevKit. La necesidad de esa referencia se limita a los circuitos que intercambian señales eléctricas: la cámara se comunica por WiFi y dispone de su propia alimentación. Este esquema no establece por sí mismo un detalle de cableado interno de la cámara.

La separación de alimentación busca evitar que los cambios de corriente de tracción alteren la alimentación del controlador. No constituye una medición de ruido eléctrico ni garantiza ausencia de reinicios. Para investigar un reinicio deben registrarse la condición de movimiento, la carga de las baterías y el resultado observado, en lugar de atribuirlo automáticamente a una causa.

## Identificación de los canales de motor

El canal izquierdo usa GPIO 27 para la entrada A y GPIO 26 para la B; en la configuración del proyecto el avance izquierdo utiliza la entrada B. El derecho usa GPIO 25 para la entrada A y GPIO 33 para la B; el avance derecho utiliza la A. Esos nombres describen el mapeo lógico del firmware. Los rótulos impresos en el módulo concreto deben comprobarse contra sus conexiones antes de modificar cables.

Una inversión de polaridad en un par puede producir avance en un lado y retroceso en el otro. Una rueda que no gira también puede deberse a un conector suelto o un problema mecánico; no debe corregirse cambiando valores de software sin revisar primero el montaje. La inspección se realiza con las fuentes desconectadas. Una vez revisado el cableado, la guía de conexión propone pruebas iniciales con ruedas elevadas y control de una sola etapa a la vez.

## Bus compartido de sensores

SDA es la línea de datos del bus I2C y SCL es la línea de reloj. Ambos módulos se conectan a las mismas dos señales, junto con 3.3 V y tierra. El código configura una frecuencia de 400 kHz; el MPU6050 responde en la dirección 0x68 y el VL53L0X en 0x29. Una dirección identifica al dispositivo en ese bus, no es una dirección de red WiFi.

La comunicación puede fallar aunque un LED de alimentación esté encendido. El LED demuestra que el módulo recibe alguna alimentación, pero no demuestra continuidad de SDA y SCL ni lectura válida. Un registro del proyecto describe un cable SCL flojo que dejó de responder al giroscopio. Ante un fallo se revisan alimentación, referencia común, continuidad y fijación de los conductores antes de interpretar el sensor como defectuoso.

El proyecto documenta una orientación prevista del giroscopio con el eje Z hacia arriba y la referencia frontal del módulo hacia adelante. La orientación y el signo importan porque el firmware interpreta el ángulo para decidir el sentido de giro. El láser se monta hacia el frente; su altura y ángulo condicionan qué objetos intercepta el haz. Un cambio físico de montaje requiere repetir las comprobaciones de orientación y detección.

## Corriente, temperatura y autonomía

Los dos motores de un lado comparten un canal del driver, por lo que el esfuerzo del canal depende de la corriente conjunta. El registro de conexiones cita límites nominales del componente; no contiene una caracterización suficiente del consumo real del conjunto bajo todas las cargas. El arranque, la fricción y el bloqueo pueden producir condiciones diferentes de una rueda girando sin carga.

Antes de decidir que un componente tiene margen suficiente, deben medirse las corrientes pertinentes con un instrumento y procedimiento adecuados, registrando tensión, configuración, duración y carga. La temperatura del driver y la autonomía tampoco cuentan con una medición suficiente en el repositorio. Estas magnitudes no deben sustituirse por valores típicos de otro carro.

El firmware configura GPIO 2 para emitir un pulso de 50 ms cada dos segundos. El propósito es mantener activa una batería portátil de baja carga, pero no se conoce con certeza qué carga física está conectada al pin ni si esa batería necesita el pulso. La señal programada es un dato de configuración; su efecto real permanece **NO MEDIDO**.

Dónde está en el código: `config.h`, `firmware_carro.ino`, `sensores.c`.
