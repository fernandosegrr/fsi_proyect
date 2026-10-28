# Solución de problemas

El diagnóstico comienza por la alimentación y las conexiones, con los motores apagados. Una indicación en la aplicación ayuda a orientar la búsqueda, pero no demuestra por sí sola el estado físico de todos los componentes.

## Red y video

Si la red del carro no aparece, comprobar que la DevKit recibe energía y que su cable USB funciona. Si la app no conecta, revisar que el teléfono esté en CarroFSI y que la dirección del carro sea 192.168.4.1. Un segundo cliente de control puede desplazar la conexión anterior.

Si hay control pero no video, comprobar la batería propia de la cámara, su conexión a la red y la dirección 192.168.4.2. La cámara atiende un cliente de video a la vez. Si el teléfono está fuera de la red, la pantalla puede indicar reconexión hasta que vuelva a conectarse.

## Movimiento y sensores

Si la aplicación indica una orden pero los motores no responden, detener la operación y revisar el interruptor del portapilas, las pilas y la tierra común entre DevKit y driver. Si el carro se desvía, comprobar ruedas y cableado y revisar el balance; el ajuste AUTO no cuenta con validación física registrada.

Si el rumbo cambia mientras el carro está inmóvil, verificar que permanezca quieto durante la calibración del giroscopio. Si el láser no responde, revisar alimentación y conexiones I2C. No continuar una ruta mientras la información necesaria para supervisarla sea poco confiable.

Dónde está en el código: `firmware_carro.ino`, `sensores.c`, `Enlace.kt`, `SesionVideo.kt`.

## Método de diagnóstico

Describa primero el síntoma: qué se esperaba, qué ocurrió y en qué modo. Registre si hay imagen, si existe conexión de control y si el problema afecta a ambos lados del carro. Anote también cambios recientes, como pilas sustituidas, cables movidos o una nueva versión de software. Una descripción concreta evita confundir síntomas de componentes distintos.

Compruebe una hipótesis por vez. Si se cambia simultáneamente la alimentación, la red y el programa, puede desaparecer el problema sin que se conozca su causa. Después de cada cambio, repita una prueba breve y anote el resultado. Si hay movimiento inesperado, detenga el carro y retire la energía antes de inspeccionar conexiones.

## No aparece la red del carro

Compruebe la fuente USB y el cable de la DevKit. Un indicador luminoso puede ayudar a ver que llega energía, pero no confirma que el programa haya iniciado correctamente ni que la red esté disponible. Si se dispone de acceso al monitor serie, revise los mensajes de arranque sin divulgar credenciales.

Si la red aparece intermitentemente, registre en qué momento se interrumpe y si coincide con movimientos o cambios de alimentación. No atribuya de inmediato el fallo a una causa concreta: una batería, un cable, el entorno inalámbrico o el propio programa requieren comprobaciones diferentes.

## La red existe, pero no hay control

Verifique que el teléfono esté realmente conectado a la red local del carro y que la aplicación use la dirección configurada. Observe si el estado cambia durante el intento de conexión. Si se conecta otro cliente de control, el comportamiento del servidor puede desplazar una sesión anterior; evite interpretar ese cambio como un fallo mecánico.

Si se obtiene respuesta a una consulta pero no se produce movimiento, la red proporciona una pista de que hay comunicación, no una garantía de que el driver y sus motores reciban energía. Pase entonces al diagnóstico de la cadena de movimiento.

## Hay control, pero falta imagen

Revise la batería portátil de la cámara, su cable y su arranque. Compruebe que la cámara se haya unido a la red del carro y que la aplicación intente acceder a su dirección. Una sola conexión de video admitida puede hacer que otra sesión compita por el servicio. Cierre las sesiones de prueba que no se utilicen antes de repetir.

Una imagen antigua no equivale a una imagen actual. Para valorar el flujo, observe si cambia al mover un objeto en un entorno seguro y revise las indicaciones de la aplicación. Si el modo de navegación depende de cámara, no continúe con una imagen ausente o evidentemente congelada.

## El carro no se mueve

Detenga la orden, apague la alimentación y revise la sujeción de ruedas y cables. Compruebe el portapilas, su interruptor, la conexión al driver y la referencia común con la DevKit. No conecte ni desconecte el cableado de motores con el sistema energizado. Una batería USB con carga no demuestra que las pilas de motores tengan carga.

Si solo responde un lado, compare las conexiones y la mecánica de ambos lados con el esquema del proyecto. Una diferencia puede encontrarse en el motor, la transmisión, el cableado o el canal del driver. Evite invertir conexiones al azar: documente la posición original y compruebe cada cambio de forma controlada.

## El carro se desvía

Revise si alguna rueda roza, si hay cables frenando el movimiento o si el piso produce adherencia desigual. Después compare el modo y la calibración usados con los del registro disponible. El ajuste MANUAL medido no equivale a una validación física de AUTO.

Para comparar dos ajustes, utilice la misma distancia de prueba, superficie y condiciones de carga; marque el punto inicial y mida el desvío al final. Repita varias veces y conserve los resultados individuales. Una sola trayectoria puede estar influida por una irregularidad puntual del piso.

## La orientación cambia con el carro quieto

Asegúrese de que el carro permanezca inmóvil durante la calibración del giroscopio y revise el estado que informa la aplicación. Si el valor cambia, anote durante cuánto tiempo y si vuelve a estabilizarse. Una lectura de orientación no mide directamente la posición lateral; no debe emplearse por sí sola para concluir cuánto se desplazó el carro.

## El sensor frontal no responde o parece inconsistente

Con la alimentación desconectada, inspeccione el cableado y las conexiones del bus I2C según el esquema. Vuelva a energizar y consulte el estado del sensor. Una respuesta ausente, una lectura fuera de rango y una distancia válida son situaciones distintas que deben registrarse por separado.

Si la lectura parece válida pero el carro no se detiene como se espera, compruebe el modo y si el uso del láser está habilitado. La disponibilidad del sensor y la autoridad para detener el avance son conceptos diferentes. Mantenga un espacio seguro y no pruebe la protección acercando personas al carro.

## La ruta no termina donde se esperaba

Guarde el trazado, la versión, la calibración y la posición de partida. Compare después la trayectoria observada con la estimada, incluyendo dónde ocurrió una evasión. El error puede acumularse en tramos rectos, giros o maniobras de esquive; el punto final por sí solo no identifica cuál fue la causa.

Si se está evaluando J15, no se debe sustituir una medición física por el resultado simulado. Registre contactos y desvío final con un procedimiento repetible y compare las corridas con el criterio indicado en el capítulo de pruebas.
