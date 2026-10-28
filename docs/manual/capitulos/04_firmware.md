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

Dónde está en el código: `config.h`, `carro.c`, `firmware_carro.ino`, `sensores.c`, `imu.c`, `tof.c`, `giro.c`.
