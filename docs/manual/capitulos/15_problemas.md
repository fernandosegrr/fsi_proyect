# Solución de problemas

El diagnóstico comienza por la alimentación y las conexiones, con los motores apagados. Una indicación en la aplicación ayuda a orientar la búsqueda, pero no demuestra por sí sola el estado físico de todos los componentes.

## Red y video

Si la red del carro no aparece, comprobar que la DevKit recibe energía y que su cable USB funciona. Si la app no conecta, revisar que el teléfono esté en CarroFSI y que la dirección del carro sea 192.168.4.1. Un segundo cliente de control puede desplazar la conexión anterior.

Si hay control pero no video, comprobar la batería propia de la cámara, su conexión a la red y la dirección 192.168.4.2. La cámara atiende un cliente de video a la vez. Si el teléfono está fuera de la red, la pantalla puede indicar reconexión hasta que vuelva a conectarse.

## Movimiento y sensores

Si la aplicación indica una orden pero los motores no responden, detener la operación y revisar el interruptor del portapilas, las pilas y la tierra común entre DevKit y driver. Si el carro se desvía, comprobar ruedas y cableado y revisar el balance; el ajuste AUTO no cuenta con validación física registrada.

Si el rumbo cambia mientras el carro está inmóvil, verificar que permanezca quieto durante la calibración del giroscopio. Si el láser no responde, revisar alimentación y conexiones I2C. No continuar una ruta mientras la información necesaria para supervisarla sea poco confiable.

Dónde está en el código: `firmware_carro.ino`, `sensores.c`, `Enlace.kt`, `SesionVideo.kt`.
