# Rutas: editor, simulador y ejecución

## 1. El editor (pestaña Rutas)

- **Cuadrícula** de 12 x 8 celdas de **20 cm** (240 x 160 cm), con la escala en cm en los bordes (cada 40 cm) y la leyenda «1 cuadro = 20 cm».
- **Tocar** una celda vacía agrega un punto; **tocar un punto** lo elige; **arrastrarlo** lo mueve (un arrastre entero es un solo paso de «deshacer»).
- Herramientas: **deshacer** y **rehacer** (hasta 32 pasos), **borrar el punto elegido** y **limpiar** (con confirmación).
- **Plantillas:** línea recta de 1 m, cuadrado de 40 cm, ocho (dos rombos que se tocan en el centro) y zigzag de escalones. Se ponen abajo y al centro de la cuadrícula y se pueden deshacer.
- **Resumen:** largo (cm), tiempo estimado con la **calibración guardada** (si no está completa, con valores de ejemplo y se avisa) y número de giros. Advertencia si hay un giro de 135° o más.
- **Trazo:** la ruta completa (con los pasos intermedios) en cian, puntos numerados (el 1 en verde) y la **huella del carro** (18 x 28 cm) al inicio, mirando hacia arriba.
- **Guardar y abrir:** rutas con nombre (1 a 24 letras, números, espacios, guion y guion bajo) en el teléfono; el **borrador se guarda solo** en cada cambio y vuelve al abrir la pantalla.

Toda la lógica está en C (`nucleo_c/ruta_editor.c`, 77 casos de prueba): agregar, mover, borrar, deshacer, plantillas, resumen y el formato de texto `RUTA1;12x8;c,f;c,f;...`.

Un punto nuevo lejano no necesita puntos intermedios: `re_expandir` rellena el camino celda por celda (primero en diagonal, luego recto) para que cada celda sea vecina de la anterior, que es lo que exige el navegador. La ruta completa no puede pasar de **64 celdas** (`MAX_PUNTOS`): si no cabe, el punto se rechaza con «La ruta quedaría demasiado larga para el carro».

## 2. Probar la ruta sin el carro (botón «Simular la ruta»)

Corre el **navegador real** (`navegador.c`, el mismo del carro) contra un **carro virtual** (`simulador_navegador.c`) con la calibración guardada (o valores por defecto, con aviso), y anima el recorrido sobre la cuadrícula al doble de la velocidad real: rastro verde, carro virtual con su frente, tiempo y lo que está haciendo («Avanzando», «Girando», «Obstáculo: esquivando»...). Al final muestra si **llegó**, el **tiempo**, los **comandos** mandados, las **esquivadas** y dónde terminó, con las **advertencias** (se sale de la cuadrícula, no llegó al final, sin calibración).

- **Obstáculos virtuales:** con el interruptor «Poner obstáculos virtuales» se tocan celdas para poner o quitar obstáculos (hasta 16). En cada vuelta de 50 ms se calcula qué vería la cámara: solo lo que está **adelante dentro de ±30°** y a **menos de 80 cm** de su cara; la zona sale de a qué lado cae (tercios del campo de visión) y la cercanía de la distancia a su cara (3 hasta 30 cm, 2 hasta 50 cm, 1 después). Es un modelo **aproximado**: no sustituye la prueba con el carro, pero muestra la maniobra de esquivar (girar, mirar, avanzar de lado, volver) y si la ruta tiene espacio. Cada celda es una caja de ~20 cm que la cámara virtual ve ocupando varias celdas del detector. Con una caja de 20 cm (o 40 cm de ancho) en medio de una recta con espacio a los lados, el navegador la esquiva, la pasa en paralelo y vuelve a la línea: la simulación llega. Si no llega, la pantalla dice por qué («El obstáculo bloqueó el camino y se agotaron los 2 reintentos», «No había por dónde esquivar», etc.). Ver «Simulador» en `docs/navegador.md`.
- **Ruido de giro:** «Repetir con ruido de ±20°» corre la ruta **8 veces**; en cada una un giro de 90° sale 90° más o menos hasta 20° (un número distinto por corrida, siempre el mismo para la misma semilla) y dibuja los puntos finales (verde = llegó, rojo = no). Muestra cuántas llegaron, el error medio y máximo y cuánto se abre la nube. Para el cuadrado de 40 cm: error medio ~19 cm, nube de ~47 cm.

Código: `nucleo_c/sim_ruta.c` (40 casos de prueba) y el puente `SimRutaNativa`.

## 3. Ejecutar ruta

Abre la pantalla **Ejecutar ruta** con la ruta del editor. Usa el mismo ciclo de siempre (`ModoSemiautonomo`): obstáculos de la **cámara del carro** (sesión de video compartida), perfil **AUTO** confirmado por el carro y giro por tiempo corregido con la cámara.

**Salir en 3 toques (ajustes del 15 oct).** Desde Inicio, la tarjeta «Salir rápido» tiene **«Repetir la última ruta»** y tres plantillas (**Recta 1 m**, **En L**: 60 cm adelante y 60 cm a la derecha, e **Ida y vuelta**: 60 cm, media vuelta y de regreso; también están en Rutas). Contando desde que se abre la app: **1.** elegir la ruta (abre Ejecutar ruta con la ruta cargada), **2.** «Iniciar ruta», **3.** «Confirmar y salir». Antes eran 5 o 6 toques (pestaña Rutas, plantilla, Ejecutar, Aprender piso, Iniciar y confirmar el piso).

1. **«Iniciar ruta» hace solo lo que antes eran varios pasos:** comprueba la conexión con el carro y que llegue el video (espera hasta 6 s), pide el perfil **AUTO** y espera su ACK, **aprende el piso** con el carro quieto (~1 s) y queda en «Listo para salir». Si el piso se ve **muy variado** (o falla) no sigue con un piso mal aprendido: avisa **«Despeja el frente del carro»** (o qué revisar) y se vuelve a tocar Iniciar. «Aprender piso» a mano quedó en **«Avanzado»**.
2. **«Confirmar y salir»** (el tercer toque): cuenta **3, 2, 1** y sale (el PARO cancela la cuenta; «Cancelar la salida» también). La ruta que sale queda como la última. Lo grande es el estado en palabras: «Preparando el carro…», «Avanzando», «Girando», «Obstáculo: detenido», «Obstáculo: esquivando», «Volviendo a la ruta», «Terminado», «La ruta se detuvo» o «Ruta cancelada». Debajo, el progreso por puntos con su barra.
3. **PARO:** un botón rojo grande ocupa el lugar de «Iniciar ruta» mientras corre (y también aparece en la cabecera).
4. Al terminar o cancelar: **resumen** (resultado, duración y obstáculos esquivados), vibración y una fila en el **historial** de Rutas (fecha, ruta, TERMINADO / ERROR / CANCELADO y duración). Si fue un error, un aviso en banda dice qué pasó y qué hacer.
5. Al salir de la pantalla (o de la app) la ruta se cancela con un STOP.

Aprender el piso a mano, los detalles técnicos (posición estimada, cuadros por segundo, la prueba de obstáculos `OBST_PRUEBA`) están detrás del botón «Avanzado».
