# Pruebas y resultados

Los resultados se interpretan según el entorno en el que se obtuvieron. Una prueba en computadora comprueba una parte de la lógica; una prueba física incluye además el vehículo, el piso y las condiciones de operación.

## Pruebas físicas registradas

El registro del ajuste MANUAL describe una recta de 150 cm y un desvío de 3 cm con los valores 252 y 255. En pruebas posteriores se observaron diferencias de rumbo y comportamiento durante esquives. En una de cuatro corridas históricas con caja se observó que el carro la evitó pero terminó aproximadamente 40 cm a la derecha; otra probablemente tocó la caja. Estos resultados corresponden a la versión y condiciones de esas pruebas, no a la corrección posterior.

En el registro del ciclo del firmware se observaron máximos de 8.431 ms en reposo con la app conectada y de 6.775 ms en una prueba en piso, ambos con excesos del umbral de 5 ms. La muestra es reducida y la causa no quedó demostrada.

## Software y simulación

El reporte J15 registra 1598 casos del núcleo y 447 de firmware sin fallos en las ejecuciones documentadas. También registra una prueba de estrés de 100000 comandos sin pérdida de STOP. Esos resultados son registros históricos de software; no son mediciones físicas del vehículo.

El escenario de esquive **SIMULADO** usó 400 semillas por versión. Tras la corrección, el error lateral medio fue 1.9 cm y el percentil 95 fue 4.6 cm; antes fueron 13.2 y 21.9 cm. El modelo usa supuestos y no reproduce todos los efectos del piso ni toda la percepción real.

## Pendientes físicos

La corrección J15 no tuvo corridas físicas con movimiento. El criterio de cinco de seis corridas sin contacto y con desvío final menor de 25 cm está **NO MEDIDO**. También permanecen sin medir el balance AUTO, el consumo eléctrico y la eficacia del circuito asociado a GPIO 2.

## Qué demuestra cada clase de ensayo

Una prueba unitaria ejecuta una parte del programa con entradas conocidas y compara la salida con una expectativa. Puede comprobar, por ejemplo, cómo se transforma una ruta o cómo se responde a un mensaje. Cuando el hardware se reemplaza por funciones falsas, la prueba puede reproducir estados y tiempos controlados, pero no incluye el comportamiento eléctrico de la placa.

Una prueba con servidor falso sustituye al carro en el canal de comunicación. Es útil para verificar que la aplicación envíe mensajes, interprete respuestas y maneje una desconexión. No demuestra que una red real tenga la misma latencia, cobertura o pérdida de paquetes, ni que los motores ejecuten el movimiento esperado.

Una simulación de ruta agrega un modelo de vehículo y de obstáculos. Permite repetir una misma geometría con variaciones de parámetros y observar estados, comandos y posiciones virtuales. Su alcance depende de las simplificaciones del modelo: si no representa reflejos, deslizamiento o una falla de alimentación, no puede utilizarse para descartar esos problemas en el carro real.

Una prueba física observa el sistema completo, pero también depende de las condiciones del ensayo. Una sola corrida no caracteriza todas las superficies, baterías y obstáculos. Para que otra persona pueda interpretar un resultado se necesita registrar fecha, versiones, modo, estado de sensores, condiciones de pista, mediciones y cualquier interrupción.

## Lectura de las cifras de simulación

El escenario B6 del reporte posterior a la corrección usa una ruta recta con una caja modelada y 400 semillas por versión. La semilla permite repetir las variaciones introducidas por el simulador. Se informaron, entre otras métricas, error lateral medio, percentil 95, contactos y rutas terminadas.

El error lateral medio se calcula promediando los errores laterales absolutos de las corridas. El percentil 95 es un valor que permite describir la parte alta de esa distribución: en términos generales, 95 % de las observaciones quedan en o por debajo del percentil calculado. Ninguna de estas dos cifras mide por sí sola el error de avance longitudinal.

Antes de la modificación, la media fue 13.2 cm y el percentil 95 fue 21.9 cm; después, 1.9 y 4.6 cm. Ambos conjuntos registraron 400 rutas terminadas y cero contactos en ese modelo. Con error inferior a 20 cm, terminaron 344 de 400 antes y 400 de 400 después. Diferenciar estos resultados evita confundir una llegada al destino con el cumplimiento de un umbral de precisión.

El criterio físico pendiente usa una definición distinta: al menos cinco de seis corridas sin contacto y con desvío final menor de 25 cm. No puede concluirse que ese criterio se cumple a partir del resultado de 400 simulaciones con un umbral de 20 cm. Se necesitan las corridas físicas y sus registros para responderlo.

## Condiciones de las pruebas físicas anteriores

El reporte histórico de cuatro corridas con una caja sitúa el obstáculo aproximadamente a un metro. Describe una posible colisión en una corrida y una llegada aproximadamente 40 cm a la derecha en otra. En otras corridas se produjeron esquives adicionales. El reporte no conserva todos los datos necesarios para reconstruir cada fase; por eso no atribuye de forma demostrada el resultado a una única causa.

La comparación con versiones posteriores requiere conservar esa limitación. El defecto de pose detectado en la revisión es una explicación técnica plausible de parte de la discrepancia, pero no está demostrado que explique por sí solo toda la desviación observada. Diferencias de giro, calibración, deslizamiento y detección también pueden intervenir.

## Cómo registrar una repetición futura

Antes de una prueba se identifica la versión de la aplicación y, cuando sea posible, la versión leída de la placa. Se registra si la autoridad del láser está habilitada, la fuente de imagen, la calibración, el estado de las baterías y la geometría de la pista. El operador debe conocer el procedimiento de parada y mantener libre el área de maniobra.

Para cada corrida se anotan el inicio, la ruta, la ubicación del obstáculo, si hubo contacto y el desvío final medido con una referencia externa. También conviene conservar los registros de pose y de giro con sus tiempos. Si la corrida se aborta o se toca el carro, se anota esa intervención: excluirla silenciosamente puede distorsionar las conclusiones.

Al resumir, se informa el número total de intentos, cuántos se completaron y qué criterio se usó para considerarlos exitosos. Las unidades se mantienen junto a cada resultado. Si faltan versiones, condiciones o datos de una corrida, el informe debe señalarlo en lugar de completar los huecos con suposiciones.

Dónde está en el código: `pruebas_reincorporacion_j15.c`, `simulador_navegador.c`, `config.h`.

## Preparar una ficha de ensayo

Antes de cada prueba, asigne un identificador y anote fecha, persona responsable, revisión del software y dispositivo utilizado. Indique también el modo de conducción, el estado de carga conocido, la superficie, el trazado, el tipo de obstáculo y la iluminación cuando intervenga la cámara. Si algún dato no se conoce, regístrelo como desconocido en lugar de estimarlo sin evidencia.

Describa qué se quiere medir y cómo se hará. Por ejemplo, para desvío lateral es necesario definir una línea de referencia, un punto inicial, un punto final y la regla para medir perpendicularmente el desplazamiento. También hay que indicar la resolución del instrumento utilizado y si el carro terminó la maniobra o tuvo que detenerse.

Una ficha debe separar el resultado observado de la interpretación. «Se detuvo antes de llegar a la marca» describe una observación. «El sensor frenó correctamente» es una hipótesis que requiere comprobar la lectura, la configuración y el evento de parada. Si hay un registro de mensajes, consérvelo junto a la ficha, retirando datos sensibles.

## Ensayo propuesto de movimiento recto

Seleccione una zona despejada y trace una referencia recta sobre el piso. Marque el punto de partida y el objetivo, coloque el carro con una orientación inicial comparable y defina por adelantado quién supervisa la parada. Registre el modo, la calibración y las condiciones de alimentación. Realice la prueba a una distancia apropiada para el espacio disponible.

Al finalizar, mida la distancia longitudinal y el desvío lateral respecto de la referencia. Anote si hubo intervención humana, contacto, patinamiento visible o interrupción. Si se repite, vuelva a colocar el vehículo en la misma posición y orientación. No mezcle resultados de superficies o calibraciones diferentes sin indicarlo.

Para interpretar los datos, compare los resultados individuales además de cualquier promedio. Un promedio pequeño puede ocultar corridas con desvíos grandes en sentidos opuestos. Este ensayo caracteriza solo la configuración y condiciones efectivamente probadas; no sustituye una evaluación de esquive.

## Ensayo propuesto de giro

Prepare una marca de orientación inicial y una referencia angular en una zona libre. Con el carro inmóvil, complete la preparación del giroscopio según la operación del sistema. Solicite un giro admitido y observe tanto la respuesta comunicada como el ángulo final medido externamente, si se dispone de un método de medición.

Registre el sentido del giro, el ángulo solicitado, la respuesta y cualquier intervención. Una respuesta de finalización indica el resultado de la lógica de control, mientras que la comparación con una referencia externa permite estimar el error físico. Si aparece un tiempo agotado o una falla de sensor, no se contabiliza como giro preciso completado.

Las repeticiones ayudan a identificar dispersión, pero no prueban todos los ángulos y condiciones. Si se cambian las ruedas, el piso o la alimentación, esas condiciones deben anotarse en una serie distinta.

## Ensayo propuesto de video y control

Con el vehículo inmovilizado de forma segura, compruebe por separado la conexión de control y la recepción de video. Registre el estado inicial y observe qué indica la aplicación cuando se interrumpe cada enlace por separado. Para comprobar si una imagen se actualiza, cambie un objeto visible frente a la cámara y confirme que el cambio aparece.

Esta comprobación permite detectar que un canal responde o deja de responder en el escenario ensayado. No mide la latencia completa salvo que se utilice un procedimiento con una referencia temporal y puntos de inicio y fin definidos. Tampoco demuestra la cobertura inalámbrica en otras ubicaciones.

## Ensayo propuesto de parada frontal

El ensayo de protección frontal requiere una pista controlada, supervisión directa y una forma de detener el carro. Antes de comenzar, confirme el modo y que el uso del láser esté habilitado. Coloque un obstáculo apropiado y mida su distancia con una referencia independiente, sin colocar personas en la trayectoria.

Registre la lectura del sensor cuando sea posible, el mensaje de parada y la posición final. Si la distancia reportada no es válida o el sistema se comporta de forma inesperada, detenga la prueba. Una respuesta positiva en una geometría concreta no demuestra detección de cualquier material, ángulo o altura.

## Análisis de una serie de resultados

Conserve todas las corridas, incluidas las fallidas y las interrumpidas, e indique por qué se excluye alguna de un cálculo. Para una distancia medida, informe la unidad, el número de resultados y una medida de dispersión o los valores individuales. Si se calcula una proporción, indique claramente el numerador y el denominador.

Por ejemplo, cinco éxitos entre seis intentos se expresan como 5/6, no como una garantía de éxito futuro. Un criterio puede exigir simultáneamente ausencia de contacto y desvío menor que un límite: si falta una de las mediciones, no corresponde contar la corrida como cumplimiento demostrado.

## Comparación entre revisiones

Para comparar una corrección con una versión anterior, mantenga el procedimiento y las condiciones lo más constantes posible. Si cambia la posición del obstáculo, la superficie o el estado de carga, anótelo como diferencia. Cuando sea viable, alterne el orden de las versiones para reducir el efecto de cambios durante la sesión, y registre cualquier excepción.

La comparación de J15 disponible en el repositorio corresponde a simulación con las semillas y el modelo descritos en el reporte. Una comparación física futura deberá identificar sus propias condiciones y no atribuir automáticamente los valores simulados al vehículo real.
