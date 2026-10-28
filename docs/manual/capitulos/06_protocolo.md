# Protocolo de comunicación

La aplicación y el carro intercambian órdenes y estados por un WebSocket en la red local. El protocolo utiliza mensajes de texto breves; el video de la cámara sigue una vía distinta. Los comandos principales se reúnen en el anexo.

## Movimiento y parada

FORWARD y REVERSE ordenan avance y retroceso; LEFT y RIGHT ordenan pivotes. STOP y EBRAKE detienen los motores. En control manual, la aplicación repite la orden mientras el usuario mantiene pulsado el control y envía STOP al soltarlo. El vigilante detiene los motores cuando el temporizador de comunicación supera el intervalo configurado; el firmware también lo renueva ante algunos mensajes que no son órdenes de movimiento.

PING y PONG permiten comprobar el enlace, pero PING no renueva el plazo de movimiento. La cola protege las órdenes de parada frente a saturación. GIRAR solicita un giro por ángulo y devuelve un resultado como GIRO_OK, GIRO_TIMEOUT o GIRO_ERROR.

## Estado y configuración

MODO_AUTO y MODO_MANUAL seleccionan perfiles; sus respuestas confirman la selección. Los mensajes SENS comunican ángulo, distancia y banderas de estado. TOF_PARO informa una detención activada por el láser. Las consultas de sensores y balance permiten comprobar las condiciones antes de una ruta.

## Interpretación de telemetría

La trama SENS contiene tres campos separados por comas: ángulo en décimas de grado, distancia en milímetros y un entero de banderas. Por ejemplo, un ángulo de 450 representa 45 grados. Las banderas indican el estado del giroscopio, del láser y de otras condiciones del firmware; se deben interpretar junto con la versión del protocolo correspondiente.

El perfil AUTO puede emitir telemetría periódica mientras hay un cliente conectado. Una lectura inválida o atrasada no debe tratarse como una distancia segura. Las respuestas de error o de tiempo agotado de un giro indican que la operación no terminó normalmente y requieren detenerse y revisar las condiciones antes de continuar.

Dónde está en el código: `carro.c`, `sensores.c`, `ControlLogica.kt`, `Enlace.kt`.
