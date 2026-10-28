# Anexos

Los anexos reúnen términos y datos de consulta para usar junto con los capítulos anteriores. Los valores configurados no sustituyen las mediciones físicas pendientes.

## Glosario

**ESP32:** familia de microcontroladores con conectividad inalámbrica. **I2C:** bus compartido para comunicar circuitos mediante líneas de datos y reloj. **PWM:** señal cuyo ciclo de encendido regula la potencia aplicada. **Pose:** posición y orientación estimadas del vehículo. **WebSocket:** canal de comunicación bidireccional persistente. **Watchdog:** mecanismo que vigila un plazo y ejecuta una acción si no se cumple la condición esperada. **ToF:** medición de distancia basada en el tiempo de vuelo de la luz.

## Comandos principales

La Tabla 1 resume las órdenes principales. La activación de la autoridad del láser se controla con `TOF_USO`.

## Pines de la DevKit

La Tabla 2 reúne la asignación de los pines empleados por el controlador.

## Fuentes por tema

Arquitectura y hardware: `conexiones.md`, `config.h`. Firmware y protocolo: `carro.c`, `sensores.c`, `protocolo.md`. Cámara: `camara_prueba.ino`, `camara_prueba.md`. Navegación: `navegador.c`, `calibracion.c`, `reporte_J15.md`. Percepción: `obstaculos.c`, `fusion.c`, `corredor.c`. Aplicación: `MainActivity.kt`, `ModoSemiautonomo.kt`. Resultados físicos: `reporte_J13_fisico.md`, `reporte_J14.md`.

## Pendientes

Validar físicamente el esquive y la reincorporación J15; medir el balance AUTO, el consumo eléctrico y el comportamiento de GPIO 2; comprobar la versión de firmware en la placa cuando se disponga del equipo.

## Glosario ampliado: electrónica y energía

**Alimentación:** suministro de energía a un circuito. En este proyecto deben distinguirse la alimentación USB de cada placa y la de los motores mediante pilas y driver. Que una fuente funcione no garantiza el funcionamiento de las demás.

**Balance:** ajuste relativo de potencia entre los lados del carro para reducir una desviación. Su valor depende del perfil y sus resultados físicos deben evaluarse en las condiciones de prueba. Un valor guardado no equivale, por sí mismo, a un ajuste verificado en todos los pisos.

**Bus I2C:** medio compartido en el que dispositivos intercambian datos utilizando las líneas SDA y SCL. Los dispositivos deben ser compatibles con las condiciones eléctricas del bus y tener conexiones correctas. El hecho de compartir líneas facilita el cableado, pero una falla común puede afectar varias lecturas.

**Ciclo de trabajo PWM:** proporción del periodo durante la cual una señal está activa. El firmware utiliza valores para controlar la potencia aplicada por el driver. El efecto mecánico no depende solo de ese número: también intervienen la batería, el motor, la carga y la fricción.

**Driver o puente H:** etapa de potencia que permite gobernar los motores a partir de señales de control. Separa la función de decisión del microcontrolador de la conmutación de energía para los motores. Debe estar correctamente alimentado y conectado a la referencia común indicada para el montaje.

**GPIO:** pin de entrada o salida de propósito general del microcontrolador. La función concreta de un pin depende de la configuración del firmware y de su conexión física. La presencia de una definición en el código no confirma que el circuito asociado esté montado o medido.

**Tierra común:** referencia eléctrica compartida entre circuitos que intercambian señales. En el carro se indica para la DevKit, el driver y el negativo del portapilas de motores. No debe confundirse con unir indiscriminadamente todas las fuentes sin revisar el esquema.

## Glosario ampliado: comunicaciones

**Cliente y servidor:** un servidor ofrece un servicio y un cliente inicia una conexión para utilizarlo. Una misma aplicación puede ser cliente de servicios distintos, como control y video. La disponibilidad de un servicio no demuestra la disponibilidad de los demás.

**Dirección IP:** identificador de un dispositivo en una red concreta. Las direcciones descritas para la DevKit y la cámara pertenecen a la configuración del proyecto. Una dirección correcta ayuda a encontrar el servicio, pero no verifica la versión que ejecuta el dispositivo.

**Latencia:** tiempo transcurrido entre un evento y su observación o respuesta. Para interpretar una cifra hay que conocer sus puntos de inicio y fin. El tiempo de captura de la cámara no representa necesariamente toda la demora entre la escena y la pantalla del teléfono.

**Telemetría:** información que un dispositivo comunica sobre su estado o sus sensores. En el perfil correspondiente, los mensajes SENS reúnen valores de orientación, distancia y banderas. Una lectura se interpreta junto con las banderas y el estado de la conexión.

**WebSocket:** canal de comunicación bidireccional sobre una conexión persistente. Puede transportar órdenes y respuestas sin abrir una conexión nueva por cada mensaje. Una sesión activa necesita vigilancia de desconexiones y de datos atrasados.

**Watchdog:** mecanismo de vigilancia de un plazo. Si se cumple su condición de vencimiento, el firmware aplica la acción programada. Su comportamiento debe describirse con el código concreto: en este proyecto no todos los mensajes tratan el temporizador de la misma manera.

## Glosario ampliado: movimiento y percepción

**Calibración:** procedimiento para relacionar una señal o parámetro con una referencia. La calibración del tiempo de desplazamiento y la del sesgo del giroscopio resuelven problemas distintos. En ambos casos, las condiciones de la medición influyen en el resultado.

**Giroscopio:** sensor que mide velocidad angular. Integrando esa información se estima el cambio de orientación. El sesgo y otros errores pueden acumularse; el sensor no mide directamente la distancia recorrida ni la posición lateral.

**Modelo de piso:** representación usada por el procesamiento de imagen para decidir qué regiones se parecen al suelo aprendido. La interpretación puede verse afectada por cambios de iluminación, textura y composición de la escena. El modelo no identifica de forma infalible todos los objetos.

**Pose:** combinación de posición y orientación empleada para representar el estado del vehículo. En este proyecto la pose es una estimación calculada por software. Debe compararse con una referencia física si se quiere cuantificar su error real.

**Reincorporación:** maniobra destinada a regresar hacia la ruta después de apartarse de ella. En J15 existen resultados de simulación de la corrección de pose y reincorporación; la validación física de esa revisión permanece pendiente.

**Sensor ToF:** sensor que estima distancia a partir del tiempo de vuelo de la luz. Su lectura depende del alcance y de las condiciones del objeto observado. Su orientación frontal no proporciona una cobertura completa alrededor del carro.

## Glosario ampliado: pruebas

**Caso de prueba:** conjunto de condiciones, acciones y resultado esperado usado para comprobar un comportamiento. Debe poder repetirse y distinguir claramente el resultado observado del esperado.

**Percentil 95:** valor por debajo o igual al cual se encuentra aproximadamente el 95 % de una muestra ordenada. Resume la parte alta de una distribución, pero no es el máximo y no explica por sí solo cómo se recogieron los datos.

**Simulación:** ejecución con un modelo que representa parte del sistema o su entorno. Permite repetir escenarios y explorar variaciones, pero sus conclusiones están limitadas por los fenómenos que el modelo incluye.

**Trazabilidad:** capacidad de relacionar un resultado con su versión, configuración, procedimiento y condiciones. Facilita repetir la prueba y evita atribuir a una revisión un comportamiento observado en otra.

**Validación física:** comprobación en el equipo real bajo condiciones registradas. Su alcance depende del número y variedad de pruebas; una ejecución satisfactoria no demuestra funcionamiento en todas las condiciones posibles.
