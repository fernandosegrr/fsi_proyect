GUÍA DE CONEXIÓN Y PARADA — J14

CONFIGURACIÓN 1: cargar firmware y app; el carro no se mueve.
1. Desconecta las cuatro pilas AA y la powerbank del carro.
2. Conecta solo el DevKit por USB a la PC; no conectes la placa de la cámara por USB.
3. Conecta el teléfono por USB a la PC y selecciona Transferir archivos.
4. Desbloquea el teléfono, quita temporalmente el PIN y el bloqueo de pantalla, y autoriza la depuración USB con «permitir siempre».
5. Une el teléfono a la red WiFi del carro; la PC no debe unirse a esa red.
6. En PowerShell ejecuta: arduino-cli board list
7. Comprueba que aparece una sola placa serie. El puerto esperado por las pruebas anteriores es COM6, aunque podría cambiar; yo comprobaré además que es el DevKit conocido antes de cargar.
8. En PowerShell ejecuta: & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" devices
9. Comprueba que el teléfono aparece como device; si dice unauthorized, acepta el permiso en el teléfono.
10. En esta configuración comprobaré la placa, guardaré el ajuste de pantalla, cargaré y verificaré el firmware, instalaré la app conservando sus datos y revisaré versiones y sensores sin mover el carro.
11. Si falta un permiso o una conexión, te indicaré qué revisar; no escribiré PIN ni contraseñas.

CONFIGURACIÓN 2: pruebas físicas; el carro se mueve.
1. Desconecta el USB del DevKit y conecta las pilas AA y la powerbank del carro.
2. Alimenta la cámara con su propia powerbank; no conectes ninguna placa por USB a la PC.
3. Coloca el carro en el piso dentro de un área despejada de 2.5 m por 1.5 m, con una cinta recta alineada con su frente.
4. Ten disponible una caja pequeña y, si tienes, una caja muy ancha.
5. Deja el teléfono conectado por USB a la PC, desbloqueado, con pantalla sin apagarse y unido a la red WiFi del carro.
6. Permanece presente durante todas las pruebas.
7. Te indicaré dónde poner el carro y las cajas antes de cada prueba y te pediré LISTO cuando necesite que los coloques.
8. Te pediré observaciones breves de orientación, distancias, desvíos y si hubo contacto con la caja; el carro quedará quieto entre pruebas.
9. Si hay un giro excesivo, falla persistente del giroscopio, pérdida prolongada de WiFi, salida del área, choque no previsto o se cumplen 90 minutos, detendré todas las pruebas.

ETAPA 3 TERMINADA. Escribe CONECTADO 1 cuando estés en la configuración 1.
