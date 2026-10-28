/*
 * navegador.h — El "cerebro" del modo semiautónomo: sigue una ruta y esquiva obstáculos.
 *
 * GUÍA DE LECTURA
 *  1. Une dos módulos: trayectoria.c (de una ruta de celdas saca pasos AVANZAR/GIRAR con
 *     tiempos) y obstaculos.c (dice si hay algo adelante). El resultado es UN comando.
 *  2. Se usa así: navegador_iniciar() → navegador_cargar_ruta() → navegador_comenzar() y
 *     después se llama navegador_tick() cada ~50 ms; devuelve el comando que el carro debe
 *     MANTENER (los mismos 5 del protocolo real, sin parámetros).
 *  3. Estados (ver el diagrama en docs/navegador.md):
 *       ESPERANDO → SIGUIENDO → DETENIDO_OBSTACULO → ESQUIVANDO → REINCORPORANDO → SIGUIENDO
 *       ... → TERMINADO (llegó) o ERROR (no pudo seguir).
 *  4. El navegador NO tiene sensores de posición: lleva la cuenta por TIEMPO (cuánto tiempo
 *     estuvo avanzando o girando), con la calibración del carro (calibracion.h). J15: con
 *     giroscopio activo, SENS orienta cada incremento de avance; la distancia sigue estimada.
 *  5. Solo enteros, sin malloc, sin recursión; los errores son códigos de retorno.
 *  6. Regla de seguridad: en los estados ESPERANDO, TERMINADO y ERROR el comando es SIEMPRE STOP.
 *  7. PEDIR AYUDA (opcional, navegador_poner_pedir_ayuda): cuando el carro no puede esquivar (los dos lados ocupados, esquivadas o reintentos agotados) en vez de ERROR pasa a AYUDA:
 *     queda quieto y «pide que tomes el control». Mientras está en AYUDA el comando es el que la app le pone con navegador_ayuda_comando (los botones de la cruceta, soltar = STOP) y
 *     sigue calculando su posición por tiempo; no mira obstáculos (el responsable es quien maneja). Con navegador_ayuda_continuar vuelve solo a la línea de la ruta (como después de
 *     esquivar) y sigue. Apagado por defecto: sin él esos casos terminan en ERROR como siempre.
 */
#ifndef NAVEGADOR_H
#define NAVEGADOR_H

#include <stdint.h>
#include "trayectoria.h"
#include "obstaculos.h"
#include "calibracion.h"

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

/* ── Parámetros a calibrar ─────────────────────────────────────────────── */

#define NAV_TICK_MS                50    /* cada cuánto se llama navegador_tick (nominal) */
#define NAV_DT_MAX_MS            1000    /* tope del tiempo entre ticks (el firmware frena a los 1000 ms) */

/* Antirrebote EN MILISEGUNDOS (no en cuadros): así el retraso es el mismo con 8, 12 o 30 cuadros
 * por segundo. Un obstáculo en CUALQUIER zona frena el carro si su cercanía (0 a 3) es al menos
 * NAV_CERCANIA_MIN_DETENER (2): un obstáculo realmente lejano (cercanía 1, por ejemplo una escalera al fondo)
 * solo se informa, no frena. Después hay dos casos:
 *   - INMEDIATO (con UN solo cuadro): cercanía 3 en cualquier zona, o cercanía 2 en la zona central
 *     (el obstáculo ya está cerca y de frente: no hay tiempo que perder).
 *   - A CONFIRMAR (cercanía 2 en una zona de los lados): hay que verlo MS_CONFIRMAR_LEJOS milisegundos
 *     seguidos (y en al menos 2 cuadros) antes de detenerse; así un cuadro de ruido no frena el carro.
 * Para volver a avanzar, el frente debe verse libre MS_CONFIRMAR_LIBRE milisegundos seguidos. */
#define MS_CONFIRMAR_LEJOS         150
#define MS_CONFIRMAR_LIBRE         250
#define NAV_CERCANIA_MIN_DETENER     2   /* desde esta cercanía (en cualquier zona) se frena; 1 = lejos, solo se informa */
/* FRENAR DESDE LEJOS (opcional, navegador_poner_frenar_lejos_centro): con el carro a ~38 cm/s y ~0.4 s de retraso del video, el STOP llega
 * cuando ya avanzó unos 17 a 19 cm. Si el detector solo marca la cercanía 2 a ~20 cm del objeto, choca. Con esta opción un obstáculo
 * LEJANO (cercanía 1) en la zona CENTRAL también frena, pero tras 2 cuadros seguidos y NAV_MS_CONFIRMAR_CENTRO_LEJOS ms, para que un
 * cuadro de ruido no pare el carro. Los lados siguen igual (cercanía 1 a los lados solo se informa). */
#define NAV_MS_CONFIRMAR_CENTRO_LEJOS 60
#define NAV_CERCANIA_INMEDIATA       2   /* en el centro: desde esta cercanía se confirma con un cuadro */

/* Giro medido con la cámara (ver navegador_aporte_visual): el giro por tiempo se corrige con lo que la cámara ve girar.
 *   - NAV_COBERTURA_VISUAL_PCT: la cámara debe haber medido al menos este % del tiempo de giro (y NAV_MIN_MS_VISUAL ms)
 *     para fiarse de ella; si no, el giro sigue por tiempo.
 *   - NAV_GIRO_MIN_PCT: aunque la cámara diga que ya se llegó al ángulo, no se corta antes de este % del tiempo planeado
 *     (por si la cámara se equivoca).
 *   - NAV_GIRO_MAX_PCT: tiempo de RESPALDO: aunque la cámara diga que falta, el giro se corta a este % del tiempo planeado. */
#define NAV_COBERTURA_VISUAL_PCT   50
#define NAV_MIN_MS_VISUAL         200
#define NAV_GIRO_MIN_PCT           50
#define NAV_GIRO_MAX_PCT          150

#define NAV_PAUSA_MS              500    /* pausa con el carro detenido antes de decidir qué hacer */
#define NAV_MAX_HUECO_VIDEO_MS    700    /* si la cámara de obstáculos pasa más de esto sin mandar un cuadro: STOP y ERROR (no reanuda sola). Eran 500: en la prueba real del 16 oct un hueco de 503 ms (con 11 a 12 cuadros por segundo y huecos normales de 250 a 330 ms) detuvo la ruta */
#define NAV_PRIMER_CUADRO_MAX_MS 1500    /* con requiere_camara: tiempo máximo (quieto) para que llegue el primer cuadro de la ruta */
#define NAV_PAUSA_ASENTAR_MS      300    /* STOP entre avanzar y girar (y al revés): el carro termina de moverse y el giro/avance siguiente
                                          * arranca desde cero, como en la calibración. Sin esto, el giro dependía de la inercia del avance. */
#define NAV_TIMEOUT_ESQUIVAR_MS  4000    /* si en este tiempo no se puede esquivar: ERROR */

/* REINTENTOS DE ESQUIVE (ver docs/navegador.md, "Objetos grandes"). Después del giro de apartarse, el carro se queda quieto un momento y MIRA:
 * si el obstáculo SIGUE MUY CERCA al frente (cercanía NAV_CERCANIA_MUY_CERCA en la zona central), no avanza hacia él: RETROCEDE un poco
 * (por tiempo), gira del MISMO lado un ángulo extra (el 1.er reintento NAV_ANGULO_REINTENTO_1 grados, el 2.º NAV_ANGULO_REINTENTO_2) y vuelve
 * a mirar. Con NAV_MAX_REINTENTOS reintentos agotados y el frente todavía bloqueado: STOP y ERROR (NAV_MOTIVO_ESQUIVANDO, como antes). */
#define NAV_MAX_REINTENTOS          2
#define NAV_ANGULO_REINTENTO_1     45    /* grados EXTRA del primer reintento (se suman a los 90 del giro de apartarse) */
#define NAV_ANGULO_REINTENTO_2     90    /* grados EXTRA del segundo reintento */
#define NAV_CERCANIA_MUY_CERCA      3    /* con esta cercanía en el centro, "sigue muy cerca" (la misma que ya hacía ERROR al esquivar) */
#define NAV_MIRAR_MAX_ESPERA_MS   500    /* si tras la espera de arriba no llega ningún cuadro nuevo, se decide sin él (libre) */
/* RETROCESO CIEGO: el carro NO tiene cámara ni sensor atrás. Para ir hacia atrás solo sabe cuánto tiempo mandó REVERSE; si hay algo detrás
 * (una persona, una pared, un escalón) lo golpea o se cae sin enterarse. Por eso la distancia está LIMITADA a NAV_RETROCESO_CM (contando lo que
 * el carro sigue de frenado) y además a NAV_RETROCESO_MAX_MS aunque la calibración pida más. Úsalo solo con el espacio de atrás libre. */
#define NAV_RETROCESO_CM           15    /* cm que retrocede en cada reintento: NO subir sin tener sensor o cámara trasera */
#define NAV_RETROCESO_MAX_MS     2000    /* tope absoluto del tiempo de retroceso, pase lo que pase con la calibración */
#define NAV_ANGULO_ESQUIVAR        90    /* grados que gira para apartarse (múltiplo de 45) */
#define NAV_TRAMO_ESQUIVAR_CM      40    /* cuánto avanza de lado, en cm */
#define NAV_MAX_ESQUIVADAS          3    /* máximo de esquivadas en una misma ruta */

/* REINCORPORACION A LA LINEA (ver docs/navegador.md, "Volver a la línea"). Al terminar de esquivar, el carro lleva su pose estimada (x, y, rumbo) y:
 *   1. busca el punto de la línea original NAV_REINC_ADELANTE_CM cm por delante del punto donde salió de ella;
 *   2. gira hacia ese punto con el giro mínimo, avanza la distancia estimada y, al llegar, gira al rumbo original de la línea;
 *   3. si estaba a menos de NAV_REINC_DESVIO_MIN_MM de la línea, solo corrige el rumbo y sigue (no se mueve de lado).
 * Nunca avanza recto sin haberse reorientado. Entre cada movimiento hay la pausa de siempre (NAV_PAUSA_ASENTAR_MS). */
#define NAV_REINC_ADELANTE_CM      30    /* a cuántos cm por delante del punto de salida (donde ya pasó el obstáculo) está el punto de la línea al que vuelve */
#define NAV_REINC_DESVIO_MIN_MM    80    /* menos de esta desviación (8 cm) a la línea: solo corrige el rumbo */
#define NAV_REINC_GIRO_MIN_GRADOS   4    /* un giro de menos de esto no se hace (el error de la estimación es mayor) */
#define NAV_REINC_PASADO_MAX_MM   400    /* ...pero solo si no se pasó de aquí (40 cm) ni quedó a más de NAV_REINC_PASADO_LADO_MM (60 cm) de lado: una ruta que da la vuelta y termina donde empezó no cuenta */
#define NAV_REINC_PASADO_LADO_MM  600
#define NAV_REINC_PASADO_MM        30    /* si al empezar a volver el carro ya está más allá del punto final de la ruta por esto (3 cm) o más, la ruta se da por terminada:
                                          * no vuelve hacia atrás al punto final (pasa, por ejemplo, cuando el obstáculo estaba justo en el destino y se rodeó con «toma el control») */

/* DETECCION DURANTE LA ESQUIVADA (ver docs/navegador.md, "Falso obstáculo al esquivar"). Mientras el carro gira, la cámara barre la escena: lo que se ve en esos cuadros
 * (el mismo obstáculo en el borde, patas o sombras lejanas, juntas de loseta) no cuenta. Después del giro se ignoran los cuadros que todavía muestran el giro
 * (los que llegan antes de latencia_visual_ms) y luego hasta que hayan llegado NAV_ESQ_CUADROS_ESTABLES cuadros o pasado NAV_ESQ_MS_ESTABLE ms (lo que ocurra
 * primero). Bloquea un obstáculo en la zona CENTRAL con cercanía de al menos NAV_ESQ_CERCANIA_BLOQUEA y, solo mientras vuelve en diagonal a la línea y con el
 * corredor activo, también uno de un LADO con cercanía NAV_ESQ_CERCANIA_LADO que el corredor ubica en el camino (la esquina de lo que acaba de pasar), visto MS_CONFIRMAR_LEJOS ms seguidos (y en 2 cuadros). Al avanzar de lado o
 * en paralelo uno lateral no aborta (es el obstáculo que se está pasando). */
#define NAV_AYUDA_MAX_MS        180000   /* si nadie toma el control ni continúa en este tiempo (3 min): ERROR con el motivo de siempre */

#define NAV_ESQ_CUADROS_ESTABLES    2
#define NAV_ESQ_MS_ESTABLE        150
#define NAV_ESQ_CERCANIA_BLOQUEA    2
#define NAV_ESQ_CERCANIA_LADO       3

/* PASAR EL OBSTACULO ANTES DE VOLVER. Después de apartarse y enderezarse (VOLVER), el carro sigue PARALELO a la línea, a la distancia lateral a la que quedó, hasta
 * haber pasado el obstáculo, y solo entonces vuelve a la línea (con el punto de NAV_REINC_ADELANTE_CM cm de arriba). Si volviera a la línea justo al lado del obstáculo, el camino
 * de vuelta lo cruzaría o quedaría de nuevo frente a él. La distancia que avanza en paralelo, medida desde el punto de salida, es lo que había hasta la cara del obstáculo
 * (se estima con la cercanía con la que frenó: NAV_CARA_CERCANIA_x_MM son los límites de cada banda del detector) más NAV_OBSTACULO_PROFUNDO_MM (cuánto fondo se supone que
 * tiene) más NAV_PASO_MARGEN_MM. Nunca pasa de NAV_PARALELO_FIN_MM antes del final de la ruta. Si ya está más adelante que eso, no avanza. */
#define NAV_CARA_CERCANIA_3_MM    300    /* con cercanía 3 el obstáculo estaba a menos de esto */
#define NAV_CARA_CERCANIA_2_MM    500
#define NAV_CARA_CERCANIA_1_MM    800
#define NAV_OBSTACULO_PROFUNDO_MM 250    /* fondo supuesto del obstáculo (una caja de ~20 a 25 cm) */
#define NAV_PASO_MARGEN_MM        100    /* holgura al terminar de pasarlo */
#define NAV_PARALELO_FIN_MM        50    /* el tramo paralelo termina al menos esto antes del final de la ruta */
#define NAV_PARALELO_MIN_MM        50    /* un tramo paralelo más corto que esto no se hace */

/* PLAN CON PIVOTE (J8, ver docs/navegador.md, «Plan con pivote»). El carro gira de pivote: el centro se desplaza al girar (modelo de calibracion.h). Todos los giros del esquive y de la
 * vuelta a la línea se PLANEAN con ese desplazamiento, no como giros en el sitio:
 *   - giro hacia el objetivo: el ángulo α tal que, después de girar α y moverse por el pivote, el rumbo apunta al punto objetivo desde la posición nueva (iteración de punto fijo de
 *     NAV_PLAN_ITERACIONES pasos y, si no converge, barrido de 1 grado); la distancia de avance se cuenta desde la posición posterior al giro;
 *   - giro final al rumbo de la línea: el avance termina en el punto previo desde el que, tras ese giro con su desplazamiento, el centro cae sobre la línea (la inversa del modelo);
 *   - tramo de lado del esquive: el margen lateral NAV_ESQ_MARGEN_LATERAL_MM del centro a la línea cuenta lo que se mueve el centro al girar para apartarse y al enderezarse;
 *   - si no cabe (el giro de salida mueve el centro hacia adelante más de lo que hay libre, o la vuelta no tiene solución) elige el otro lado o pide ayuda;
 *   - RED DE SEGURIDAD: tras cada tramo de la vuelta compara la pose estimada con la planeada y, si pasa de NAV_PLAN_ERROR_MM, vuelve a planear desde donde está (máximo
 *     NAV_PLAN_CORRECCIONES veces; si se agotan, para y pide ayuda). */
#define NAV_PLAN_ITERACIONES        5    /* pasos máximos de la iteración de punto fijo */
#define NAV_PLAN_ERROR_MM          80    /* error de la pose estimada respecto del punto planeado a partir del cual se vuelve a planear (8 cm) */
#define NAV_PLAN_CORRECCIONES       4    /* máximo de correcciones (nuevos planes) en una vuelta a la línea */
#define NAV_PLAN_MAS_MM           200    /* si el punto de la línea no sirve, prueba NAV_PLAN_INTENTOS veces con un punto este tanto más adelante (una celda) */
#define NAV_REINC_PUNTO_PASADO_MM 100    /* al terminar la vuelta, un punto de la ruta que el carro ya dejó atrás por esto (medio cuadro) o más se salta: no se vuelve a él */
#define NAV_REINC_VERTICE_MIN_MM  150    /* el punto al que se vuelve (un centro de celda de la ruta) queda al menos esto por delante del carro */
#define NAV_PLAN_INTENTOS           5
#define NAV_ARCO_MARGEN_MM        100    /* colchón entre el desplazamiento hacia adelante del giro de salida y la cara del obstáculo */
#define NAV_ESQ_MARGEN_LATERAL_MM (NAV_TRAMO_ESQUIVAR_CM * 10)   /* distancia lateral del centro a la línea al pasar el obstáculo (la de siempre: el tramo de lado de 40 cm) */
#define NAV_ESQ_TRAMO_MIN_MM      100    /* el tramo de lado nunca es más corto que esto */

/* ARRANQUE, SONDEO Y FRENOS ANTI-LOCURA (J9, ver docs/navegador.md). El obstáculo fantasma de la prueba real (zona izquierda con 25 a 57 celdas en cercanía 3 desde el primer cuadro, con el piso despejado)
 * hacía frenar y esquivar sin motivo; y un índice de la ruta vencido hizo dar media vuelta al final. Por eso:
 *   - al arrancar NO se decide con el primer cuadro: hacen falta NAV_ARRANQUE_CUADROS cuadros seguidos iguales (o NAV_ARRANQUE_MAX_MS ms);
 *   - si el obstáculo ya estaba en esos cuadros, el carro no se ha movido y el centro está libre (cercanía NAV_SONDEO_CERC_MAX o menos, o solo en los lados), avanza NAV_SONDEO_CM cm y compara: si las celdas
 *     no crecen (NAV_SONDEO_CRECE_PCT %, al menos NAV_SONDEO_CRECE_MIN) es un «sospechoso fijo»: se ignora en esa corrida; si crecen es real y se esquiva como siempre;
 *   - la cercanía 3 frena con 2 cuadros seguidos, o con 1 si el área es de NAV_CERC3_AREA_INMEDIATA celdas o más;
 *   - frenos: un giro planeado de más de NAV_LOCURA_GIRO_MAX grados que no está en la ruta, el índice de la ruta que retrocede, una distancia recorrida de más de NAV_LOCURA_RECORRIDO_X10 / 10 veces la de la
 *     ruta, la pose a más de NAV_LOCURA_DESVIO_MM de la ruta, o más de NAV_MAX_ESQUIVADAS esquivadas: para y pide AYUDA;
 *   - el final de la ruta no deja el carro girado más de NAV_RUMBO_FINAL_TOL grados del último tramo. */
#define NAV_ARRANQUE_CUADROS        4    /* cuadros seguidos iguales antes de decidir al arrancar */
#define NAV_ARRANQUE_MAX_MS      1500    /* ...o este tiempo, lo que ocurra primero */
#define NAV_CERC3_AREA_INMEDIATA    6    /* con cercanía 3 y al menos tantas celdas, un solo cuadro frena; con menos hacen falta 2 cuadros seguidos */
#define NAV_SONDEO_CM               4    /* cuánto avanza el sondeo */
#define NAV_SONDEO_CERC_MAX         2    /* el sondeo se hace si el obstáculo es de esta cercanía o menos (o está solo en los lados, con el centro libre) */
#define NAV_SONDEO_QUIETO_MM       30    /* «el carro no se ha movido»: ha recorrido menos de esto */
#define NAV_SONDEO_CRECE_PCT       15    /* las celdas crecieron si pasaron de las de antes + este % (y al menos NAV_SONDEO_CRECE_MIN celdas) */
#define NAV_SONDEO_CRECE_MIN        3
#define NAV_SONDEO_TOPE_CELDAS     12    /* tope de celdas por zona para ignorar como sospechoso fijo */
#define NAV_SONDEO_IGNORAR_PCT    130    /* un sospechoso fijo se ignora mientras no crezca más del 30 % y su firma de celdas siga igual */
#define NAV_SONDEO_IGNORAR_MAS      0
#define NAV_SONDEO_MIRAR_MS       250    /* cuánto se miran los cuadros después de asentarse, antes de comparar */
#define NAV_SONDEO_MAX_ESPERA_MS 1500    /* espera máxima de al menos 2 cuadros tras el sondeo; si no llegan, repite sin declarar nada */
#define NAV_LOCURA_GIRO_MAX       150    /* un giro planeado de más de estos grados que no está en la ruta frena */
#define NAV_LOCURA_RECORRIDO_X10   25    /* distancia recorrida máxima: 2.5 veces la de la ruta... */
#define NAV_LOCURA_RECORRIDO_MIN_MM 600  /* ...tomando la ruta como de al menos 60 cm (con una ruta de 20 cm una sola esquivada ya pasa 2.5 veces) */
#define NAV_LOCURA_DESVIO_MM     1000    /* la pose a más de esto (1 m) de la ruta frena */
#define NAV_RUMBO_FINAL_TOL        20    /* al terminar la ruta el rumbo queda a lo más a esto del último tramo */

/* GIROSCOPIO Y SENSOR LÁSER (J11, ver docs/sensores.md). Con el giroscopio encendido (navegador_poner_giroscopio) cada giro de entre NAV_GIRO_FW_MIN_GRADOS y NAV_GIRO_FW_MAX_GRADOS se le pide al
 * firmware como GIRAR:<grados> (la app lo manda al ver navegador_tomar_giro_fw); el firmware gira hasta el ángulo MEDIDO y contesta con el ángulo REAL (navegador_aporte_giro): la pose usa ese
 * ángulo (con el desplazamiento del pivote calculado con él). Si el giro termina mal (GIRO_TIMEOUT / GIRO_ERROR) se usa el ángulo real que informó, si lo hay, y los giros siguientes de la ruta
 * vuelven al método por tiempo (con aviso SENS_DIAG). Un giro sin respuesta en la espera máxima también cae al tiempo.
 * El láser (navegador_poner_sensor_distancia) hace dos cosas: (1) TOF_PARO, el freno de emergencia del firmware, entra como obstáculo confirmado SIN información lateral: espera hasta
 * NAV_TOF_ESPERA_LADO_MS a que la cámara aporte el lado libre y si no llega esquiva hacia el lado con más espacio de la última rejilla (o a la izquierda); (2) confirma o corrige a la cámara: con
 * cercanía 2 en el centro y el láser libre (más de NAV_TOF_LIBRE_MM en NAV_TOF_LIBRE_LECTURAS lecturas) se pide UN cuadro más de la cámara antes de frenar; con el láser a menos de
 * NAV_TOF_CONFIRMA_MM se frena con un solo cuadro. Con cercanía 3 no hay confirmación que valga. La cámara nunca se ignora por más de ese cuadro. */
#define NAV_GIRO_FW_MIN_GRADOS       5    /* un giro de menos de esto no se pide al firmware (no se hace) */
#define NAV_GIRO_FW_MAX_GRADOS     200    /* el firmware no gira más de esto de una vez; uno mayor se hace por tiempo */
#define NAV_GIRO_FW_BASE_MS        500    /* tiempo máximo propio del firmware: base + por grado (igual que GIRO_TIEMPO_BASE_MS y GIRO_TIEMPO_POR_GRADO_MS de config.h) */
#define NAV_GIRO_FW_POR_GRADO_MS    15
#define NAV_GIRO_FW_ESPERA_EXTRA_MS 1500  /* margen sobre ese máximo para que llegue la respuesta; si no llega, el giro cae al método por tiempo */
#define NAV_GIRO_FW_CONSULTA_MS   1000    /* si no llega GIRO_OK ni GIRO_TIMEOUT se pregunta IMU? y se espera tanto la respuesta (si no llega se usa el último SENS fresco) */
#define NAV_GIRO_FW_YA_GIRO_PCT     80    /* con el mensaje perdido, si el giroscopio dice que ya giró al menos este % de lo pedido NO se repite el giro */
#define NAV_TOF_ESPERA_LADO_MS     700    /* tras TOF_PARO se espera tanto (como máximo) a que la cámara dé el lado libre */
#define NAV_TOF_LIBRE_MM           800    /* el láser «ve libre» si mide más de esto (o no tiene eco) */
#define NAV_TOF_LIBRE_LECTURAS       3
#define NAV_TOF_CONFIRMA_MM        500    /* el láser «confirma» el obstáculo si mide menos de esto */
#define NAV_TOF_FRESCO_MS          400    /* una lectura del láser más vieja que esto ya no sirve */
#define OBSTACULO_LASER_MM         400    /* autoridad opcional en columnas 3 a 6 de la rejilla 10x10 */

/* Resultado de un giro por ángulo (navegador_aporte_giro). */
#define NAV_GIRO_OK                  0
#define NAV_GIRO_TIMEOUT             1    /* GIRO_TIMEOUT: se acabó el tiempo del firmware (informa el ángulo real) */
#define NAV_GIRO_ERROR_CON_DATOS     2    /* GIRO_ERROR:abortado o sin_imu con el ángulo parcial */
#define NAV_GIRO_ERROR_SIN_DATOS     3    /* GIRO_ERROR:sin_imu, ocupado o angulo: el giro no empezó */

/* Diagnóstico: el navegador escribe líneas de texto (REINC_DIAG, ESQUIVE_DIAG, AYUDA_DIAG, TURN_DIAG y RUTA_DIAG) en una cola chica que la app lee con navegador_diag_siguiente()
 * y copia al log. Si la app no las lee, las más viejas se pierden (no pasa nada).
 *  - RUTA_DIAG: una línea al ENVIAR cada comando de movimiento (FORWARD, REVERSE, LEFT o RIGHT) con lo pedido y la duración planeada, y otra al TERMINARLO con la duración real y lo
 *    hecho (estimado). Sirve para separar «el programa mandó mal el comando» de «el comando fue correcto y el carro no lo hizo» (contacto eléctrico, ver docs/bitacora.md).
 *  - TURN_DIAG: una línea al terminar cada GIRO con los grados pedidos y hechos y el desplazamiento del centro que se aplicó (ver cal_desplazamiento_giro). */
#define NAV_DIAG_LINEAS            16
#define NAV_DIAG_LARGO            340
#define NAV_MAX_GIROS              16   /* cuántos giros recuerda el navegador de la ruta en curso (para la pantalla de resultado) */
#define NAV_DIAG_REPETIR_MS       500   /* una línea ESQUIVE_DIAG igual a la anterior se repite como máximo cada tanto */

/* ── Tipos ─────────────────────────────────────────────────────────────── */

typedef enum {
    NAV_ESPERANDO,
    NAV_SIGUIENDO,
    NAV_DETENIDO_OBSTACULO,
    NAV_ESQUIVANDO,
    NAV_REINCORPORANDO,
    NAV_TERMINADO,
    NAV_ERROR,
    NAV_AYUDA       /* no pudo esquivar y pide que tomes el control (solo con navegador_poner_pedir_ayuda) */
} EstadoNav;

/* Los 5 comandos del protocolo real del carro (texto sin parámetros). */
typedef enum {
    NAV_CMD_STOP,
    NAV_CMD_FORWARD,
    NAV_CMD_REVERSE,
    NAV_CMD_LEFT,
    NAV_CMD_RIGHT
} ComandoNav;

/* Por qué terminó en ERROR. */
typedef enum {
    NAV_MOTIVO_NINGUNO,
    NAV_MOTIVO_LADOS_BLOQUEADOS,   /* obstáculo al frente y ningún lado libre tras el tiempo máximo */
    NAV_MOTIVO_MAX_ESQUIVADAS,     /* ya esquivó NAV_MAX_ESQUIVADAS veces y vuelve a haber obstáculo */
    NAV_MOTIVO_ESQUIVANDO,         /* apareció un obstáculo muy cerca mientras esquivaba */
    NAV_MOTIVO_PLAN,               /* no se pudo calcular el camino de regreso a la ruta */
    NAV_MOTIVO_SIN_VIDEO           /* la cámara dejó de mandar cuadros más de NAV_MAX_HUECO_VIDEO_MS: se detuvo y NO reanuda sola */
} MotivoErrorNav;

/* Causa concreta del ERROR (campo `error_causa`; navegador_texto_error la convierte en una frase con los números). */
#define NAV_CAUSA_NINGUNA                0
#define NAV_CAUSA_LADOS                  1   /* los dos lados estaban ocupados durante NAV_TIMEOUT_ESQUIVAR_MS */
#define NAV_CAUSA_MAX_ESQUIVADAS         2   /* ya esquivó NAV_MAX_ESQUIVADAS veces y el obstáculo seguía en el camino */
#define NAV_CAUSA_REINTENTOS             3   /* se agotaron los reintentos de esquive y el obstáculo seguía en el centro */
#define NAV_CAUSA_PLAN                   4   /* no se pudo calcular el camino */
#define NAV_CAUSA_SIN_VIDEO              5   /* la cámara dejó de mandar cuadros */
#define NAV_CAUSA_ARCO                   6   /* el giro de salida mueve el centro más hacia adelante de lo que hay libre hasta el obstáculo (en los dos lados) */
#define NAV_CAUSA_CORRECCIONES           7   /* se agotaron las correcciones de la vuelta a la línea y el carro seguía lejos del punto planeado */
#define NAV_CAUSA_VUELTA                 8   /* la vuelta a la línea no tiene solución con los giros de pivote (el punto de la línea queda detrás o el plan no converge) */
#define NAV_CAUSA_GIRO_LOCO              9   /* freno anti-locura: un giro planeado de más de NAV_LOCURA_GIRO_MAX grados que no está en la ruta */
#define NAV_CAUSA_INDICE                10   /* freno anti-locura: el índice del siguiente punto de la ruta retrocedió */
#define NAV_CAUSA_RECORRIDO             11   /* freno anti-locura: recorrió más de 2.5 veces la distancia de la ruta */
#define NAV_CAUSA_LEJOS                 12   /* freno anti-locura: la pose quedó a más de 1 m de la ruta */

/* Fases de la maniobra de esquivar (campo `fase`). El camino normal es GIRAR → MIRAR → AVANZAR → VOLVER; si al mirar el frente sigue muy
 * cerca: MIRAR → RETROCEDER → ASENTAR → GIRAR (con ángulo extra) → MIRAR... hasta NAV_MAX_REINTENTOS. */
#define NAV_FASE_GIRAR        0   /* girar hacia `lado` angulo_giro grados */
#define NAV_FASE_AVANZAR      1   /* avanzar de lado NAV_TRAMO_ESQUIVAR_CM */
#define NAV_FASE_VOLVER       2   /* deshacer el giro (angulo_total) */
#define NAV_FASE_MIRAR        3   /* STOP: espera a que el carro pare y a que llegue un cuadro que lo muestre parado, y decide */
#define NAV_FASE_RETROCEDER   4   /* REVERSE NAV_RETROCESO_CM cm (ciego) */
#define NAV_FASE_ASENTAR      5   /* STOP NAV_PAUSA_ASENTAR_MS antes de girar otra vez */
#define NAV_FASE_PAUSA_PASAR  6   /* STOP NAV_PAUSA_ASENTAR_MS después de enderezarse; al terminar calcula cuánto debe avanzar en paralelo */
#define NAV_FASE_PARALELO     7   /* FORWARD paralelo a la línea hasta pasar el obstáculo (nav->paralelo_mm) */
#define NAV_FASE_PAUSA_VOLVER 8   /* STOP NAV_PAUSA_ASENTAR_MS entre AVANZAR y VOLVER (sin ella el giro hereda la inercia del avance) */
#define NAV_FASE_PAUSA_RETROCEDER 9   /* STOP NAV_PAUSA_ASENTAR_MS antes de RETROCEDER cuando venía avanzando (no pasa de FORWARD a REVERSE de golpe) */

/* Fases de REINCORPORANDO (campo `reinc_fase`): PLAN (STOP, calcula) → GIRO1 → PAUSA → AVANZAR → PAUSA → GIRO2 → PAUSA → SIGUIENDO. Las fases de giro que no hacen
 * falta (ángulo casi cero) se saltan. */
#define NAV_RF_PLAN           0   /* quieto NAV_PAUSA_ASENTAR_MS y calcula el punto de la línea y los ángulos */
#define NAV_RF_GIRO1          1   /* gira hacia el punto de la línea (giro mínimo) */
#define NAV_RF_PAUSA1         2   /* STOP */
#define NAV_RF_AVANZAR        3   /* avanza la distancia estimada hasta el punto de la línea */
#define NAV_RF_PAUSA2         4   /* STOP */
#define NAV_RF_GIRO2          5   /* gira al rumbo original de la línea */
#define NAV_RF_PAUSA3         6   /* STOP; al terminar la pausa vuelve a SIGUIENDO */

/* Códigos de retorno de las funciones de configuración. */
#define NAV_OK                 0
#define NAV_ERR_ARGUMENTO     -1   /* puntero nulo o ruta vacía */
#define NAV_ERR_RUTA          -2   /* la ruta no es válida (ver codigo_trayectoria) */
#define NAV_ERR_ESTADO        -3   /* esa función no se puede usar en el estado actual */

/* Lo que pasó en un giro (para la pantalla de resultado: «Giro pedido / Giro hecho (estimado)»). */
typedef struct {
    int lado;                  /* CAL_LADO_IZQUIERDA o CAL_LADO_DERECHA */
    int origen;                /* EstadoNav en el que se hizo: SIGUIENDO (la ruta), ESQUIVANDO, REINCORPORANDO (vuelta a la línea) o AYUDA (a mano) */
    int pedido_grados;         /* grados que pidió el programa (0 si fue a mano) */
    int hecho_grados;          /* grados que el navegador estima que giró (con la cámara si era confiable, si no por tiempo; incluye lo que sigue girando al frenar) */
    int plan_ms;               /* duración planeada del comando */
    int real_ms;               /* duración real */
    int desp_adelante_mm;      /* desplazamiento del centro durante el giro, en el marco del carro al empezar el giro */
    int desp_derecha_mm;
    int por_angulo;            /* 1 si el giro lo midió el giroscopio del firmware (hecho_grados es el ángulo REAL medido); 0 si fue por tiempo */
} NavGiro;

typedef struct {
    /* Calibración del carro (arranque, ms por cm, frenado, ms por grado de cada lado). */
    CalibracionCarro cal;

    /* Ruta original (celdas) y camino actual (pasos por tiempo). */
    Punto ruta[MAX_PUNTOS];
    int   n_puntos;
    int   indice_objetivo;       /* punto de la ruta al que se dirige (1 = el segundo) */
    Paso  pasos[MAX_PASOS];
    int   n_pasos;
    int   indice_paso;
    int   ms_paso;               /* tiempo ya ejecutado del paso actual */

    /* Estado general. */
    EstadoNav      estado;
    MotivoErrorNav motivo_error;
    int            codigo_trayectoria;   /* último código de trayectoria.c (si hubo error de ruta) */
    ComandoNav     cmd_actual;           /* el comando que se devolvió en el último tick */
    uint32_t       t_ultimo_ms;          /* hora del último tick */
    uint32_t       t_detenido_ms;        /* hora en que se detuvo por obstáculo */
    int            asentando;            /* 1 mientras espera detenido entre dos movimientos distintos */
    int            usa_camara;           /* 1 desde que llegó el primer cuadro de obstáculos en esta ruta (con la fuente de botones no hay huecos que vigilar) */
    int            requiere_camara;      /* 1 = la ruta usa la cámara: no se mueve hasta el primer cuadro y, si no llega en NAV_PRIMER_CUADRO_MAX_MS, ERROR sin video */
    uint32_t       t_ultimo_cuadro_ms;   /* hora del último cuadro de obstáculos recibido */
    uint32_t       asentar_hasta_ms;     /* hora en que termina esa espera */

    /* Posición estimada: milímetros (x hacia la derecha, y hacia abajo) y rumbo en grados
     * (0 = arriba, positivo = hacia la derecha, 0 a 359). */
    int x_mm, y_mm, rumbo_grados;
    int base_x_mm, base_y_mm, base_rumbo_grados;   /* pose al empezar el comando actual */
    int seg_ms;                                    /* tiempo bajo el comando actual */
    int giro_visual_x10;         /* grados por 10 que la cámara vio girar, en la dirección del comando de giro actual */
    int visual_cubierto_ms;      /* cuánto tiempo del giro actual cubren los cuadros confiables de la cámara */
    int frenar_lejos_centro;     /* 1 = también frena con cercanía 1 en el centro (ver NAV_MS_CONFIRMAR_CENTRO_LEJOS); 0 = como siempre */
    int solo_lejos_centro;       /* 1 mientras la racha de bloqueo actual es SOLO por esa regla (se confirma con menos tiempo que un obstáculo de lado) */
    int latencia_visual_ms;      /* retraso de la cámara: un cuadro que llega ahora muestra lo que pasó hace esto (0 = sin retraso) */

    /* Lo último que se vio del obstáculo. */
    int zona_hay[OBS_ZONAS];     /* hay obstáculo en la zona (cualquiera): lo usa elegir_lado, para no apartarse hacia un obstáculo */
    int zona_cercania[OBS_ZONAS];
    int zona_frena[OBS_ZONAS];       /* hay obstáculo en la zona Y está dentro del corredor (sin corredor = zona_hay): esto es lo que frena */
    int zona_cerc_frena[OBS_ZONAS];  /* cercanía con la que frena (la del corredor si hay; si no, la de siempre); 0 si no frena */
    int zona_en_corredor[OBS_ZONAS]; /* 1 si el corredor (activo) dice que el obstáculo de la zona está en el camino del carro; sin corredor siempre 0 */
    int frames_bloqueado;        /* cuadros seguidos con obstáculo al frente */
    int frames_libre;            /* cuadros seguidos con el frente libre */
    int frames_recibidos;        /* cuadros desde la última detención */
    uint32_t t_bloqueo_ms;       /* hora del primer cuadro de la racha con obstáculo al frente */
    uint32_t t_libre_ms;         /* hora del primer cuadro de la racha con el frente libre */
    int bloqueo_inmediato;       /* 1 si en la racha hubo un cuadro de confirmación inmediata */

    int zona_celdas[OBS_ZONAS];  /* cuántas celdas de obstáculo tenía cada zona en el último cuadro (para elegir el lado con más espacio) */

    /* Esquivar. */
    int esquivadas;
    int lado;                    /* -1 = izquierda, +1 = derecha */
    int fase;                    /* NAV_FASE_*: qué hace ahora la maniobra de esquivar */
    int ms_fase;
    int reintentos;              /* cuántos reintentos (retroceder + girar más) lleva esta esquivada */
    int angulo_giro;             /* grados que debe girar la fase GIRAR actual (90 la primera vez; 45 o 90 en los reintentos) */
    int angulo_total;            /* grados girados hacia `lado` en total desde que empezó la esquivada (para deshacerlos al final) */
    uint32_t t_mirar_ms;         /* hora en que el carro quedó quieto para mirar si el frente está libre */

    /* Detección durante la esquivada: giros, cuadros estables y racha de obstáculo central. */
    int      error_causa;            /* NAV_CAUSA_*: por qué terminó en ERROR (con motivo_error) */
    int      esq_en_giro;            /* 1 mientras la maniobra gira (los cuadros de ese momento se ignoran) */
    uint32_t esq_t_giro_ini_ms;      /* hora en que empezó el último giro de la maniobra */
    uint32_t esq_t_giro_fin_ms;      /* hora en que terminó */
    int      esq_cuadros_tras_giro;  /* cuadros que han llegado desde que terminó el giro */
    int      centro_frames;          /* cuadros seguidos (no ignorados) con obstáculo central de cercanía >= NAV_ESQ_CERCANIA_BLOQUEA */
    uint32_t centro_t_ms;            /* hora del primer cuadro de esa racha */
    int      zona_fila_min[OBS_ZONAS], zona_fila_max[OBS_ZONAS];   /* filas y columnas de las celdas de obstáculo de cada zona en el último cuadro (-1 = no se sabe) */
    int      zona_col_min[OBS_ZONAS], zona_col_max[OBS_ZONAS];
    int      zona_borde_cerca[OBS_ZONAS];                          /* 1 si durante esquive/reincorporación las columnas 0 o 9 tienen obstáculo en filas 7 a 9 */

    /* Pedir ayuda (modo «toma el control»). */
    int            pedir_ayuda;      /* 1 = en vez de ERROR por no poder esquivar, pasa a AYUDA */
    ComandoNav     ayuda_cmd;        /* el comando que el usuario manda ahora con la cruceta */
    uint32_t       ayuda_t_ini_ms;   /* hora en que pidió ayuda */
    MotivoErrorNav ayuda_motivo;     /* por qué la pidió (el motivo con el que habría dado ERROR) */
    int            ayudas;           /* cuántas veces pidió ayuda en esta ruta */
    int            desde_ayuda;      /* 1 mientras vuelve a la línea después de que el usuario tomó el control (ahí sí se da por terminada la ruta si ya pasó el final) */

    /* Dónde salió de la línea (pose estimada al empezar la esquivada). */
    int salida_x_mm, salida_y_mm, salida_rumbo;
    int salida_cercania;         /* cercanía (1 a 3) con la que frenó: dice a qué distancia estaba el obstáculo */
    int cerc_detenido_max;       /* la mayor cercanía (dentro del corredor) vista desde que se detuvo por obstáculo hasta decidir: el último cuadro puede venir vacío */
    int fin_obstaculo_mm;        /* hasta dónde (distancia a lo largo de la ruta) se supone que llega el último obstáculo que lo detuvo; -1 = ninguno */
    int paralelo_mm;             /* cuánto debe avanzar en paralelo a la línea para pasar el obstáculo (se calcula al terminar de enderezarse) */

    /* Reincorporación a la línea (REINCORPORANDO). Se calcula una vez, en la fase PLAN, y se ejecuta por fases (NAV_RF_*). */
    int      reinc_fase;
    uint32_t reinc_t_fase_ms;        /* hora en que empezó la fase actual (las pausas se miden con la hora, no con ticks) */
    int      reinc_solo_rumbo;       /* 1 = estaba a menos de NAV_REINC_DESVIO_MIN_MM de la línea: solo corrige el rumbo */
    int      reinc_obj_x_mm, reinc_obj_y_mm;   /* punto de la línea al que vuelve */
    int      reinc_rumbo_final;      /* rumbo original de la línea en ese punto */
    int      reinc_giro1, reinc_giro2;         /* grados con signo (+ derecha) del giro hacia el punto y del giro final */
    int      reinc_dist_mm;          /* distancia que avanza entre los dos giros */
    int      reinc_indice_obj;       /* punto de la ruta al que se dirige después de volver */
    int      reinc_desvio_mm;        /* a cuánto estaba de la línea al empezar a volver */
    int      reinc_antes_x_mm, reinc_antes_y_mm, reinc_antes_rumbo;   /* pose estimada al empezar a volver */
    int      volver_a_reinc;         /* 1 si lo que detuvo al carro ocurrió mientras avanzaba hacia la línea: al liberarse el frente vuelve a calcular la vuelta */
    int      reinc_pasar_primero;    /* 1 = esta vuelta solo endereza y avanza en paralelo hasta pasar el obstáculo; al terminar calcula la vuelta de verdad */
    int      reinc_plan_x_mm[3], reinc_plan_y_mm[3];   /* posición PLANEADA al terminar el giro 1, el avance y el giro 2 (la red de seguridad la compara con la estimada) */
    int      reinc_correcciones;     /* cuántas veces se volvió a planear esta vuelta (máximo NAV_PLAN_CORRECCIONES) */
    int      corr_rumbo_vertice;     /* vértice de la ruta (indice_objetivo) al que corresponde corr_rumbo_cuenta; -1 = ninguno */
    int      corr_rumbo_cuenta;      /* giros de corrección de rumbo (más de 4°) ya planeados para ese vértice (tope 2, ver navegador.c) */
    int      apunte_k_esq;           /* vértice de esquina del tramo que se está planeando apuntando al punto de parada (por el desplazamiento de un giro grande); -1 = ninguno */
    int      reinc_rumbo_importa;    /* 1 = al terminar la vuelta el rumbo debe ser el de la línea (0 cuando el punto es el final de la ruta y se llega sin giro final) */
    int      tramo_mm;               /* lo que avanza de lado esta esquivada (planeado al terminar de mirar, con el desplazamiento de los giros) */
    int      sin_red_seguridad;      /* 1 = la red de seguridad de la vuelta está apagada (solo para comparar en las pruebas; por defecto 0 = activa) */
    int      sin_arreglo_indice;     /* 1 = NO se saltan los puntos de la ruta ya pasados al terminar la vuelta (solo para reproducir en las pruebas el bug del índice vencido; por defecto 0) */
    int      reinc_rumbo_tol;        /* el rumbo al terminar la vuelta se corrige si queda a más de esto de la línea (3 por defecto; NAV_RUMBO_FINAL_TOL al final de la ruta) */

    /* Arranque, sondeo del obstáculo fijo y frenos anti-locura (J9). */
    int      arranque_ok;            /* 1 cuando ya se decidió el arranque (4 cuadros iguales o se acabó el tiempo) */
    int      arranque_iguales;       /* cuadros seguidos con la misma firma de zonas */
    int      arranque_firma;         /* firma de zonas de esos cuadros: 2 bits (cercanía con obstáculo) por zona; 0 = sin obstáculo */
    uint32_t arranque_t0_ms;         /* hora en que empezó la ruta */
    int      bloqueo_cerc3;          /* 1 si la racha tiene cercanía 3 con pocas celdas (necesita 2 cuadros seguidos) */
    int      sondeo_hecho;           /* 1 si ya se hizo (o se descartó) el sondeo de esta ruta: solo se hace uno */
    int      sondeo_fase;            /* 0 = ninguno, 1 = avanzando NAV_SONDEO_CM cm, 2 = mirando si las celdas crecen */
    int      sondeo_ms;              /* cuánto debe avanzar el sondeo */
    int      ms_sondeo;              /* cuánto lleva avanzado */
    int      sondeo_zonas;           /* bits de las zonas que tenían el obstáculo */
    int      sondeo_ref;             /* celdas de esas zonas antes de avanzar */
    long     sondeo_suma;            /* suma de las celdas vistas después (para el promedio) */
    int      sondeo_n;
    uint32_t sondeo_listo_ms;        /* desde esta hora cuentan los cuadros de después del sondeo */
    int      fijo_ignorar[OBS_ZONAS];   /* 1 = esa zona tiene un «sospechoso fijo»: se ignora mientras sus celdas no crezcan ni cambie su firma */
    int      fijo_celdas[OBS_ZONAS];
    char     fijo_hex[OBS_ZONAS][26];   /* firma hexadecimal (10 x 10) de las celdas del sospechoso fijo en cada zona */
    int      frenado_pendiente;         /* 1 = el paso actual frenó de FORWARD a STOP y su ajuste aún no se ha aplicado */
    int      sin_compensar_giro;        /* 1 = planificar() no compensa el desplazamiento del giro (solo para medir el antes/después en pruebas) */
    int      indice_max;             /* el mayor índice de punto de la ruta visto (si el índice baja de aquí, frena) */
    long     recorrido_mm;           /* distancia total que el centro ha recorrido en esta ruta (estimada) */
    int      prev_x_mm, prev_y_mm;
    long     ruta_largo_mm;          /* largo de la ruta */
    int      max_giro_ruta;          /* el mayor giro que pide la propia ruta (grados) */
    char     obs_hex[OBS_ZONAS][26]; /* de lo último que se vio: la máscara de 10 x 10 en hexadecimal de cada zona (OBS_DIAG) */
    int      obs_franja_tapada, obs_franja_pct;
    int      arco_adelante_mm, arco_libre_mm;   /* para el texto de NAV_CAUSA_ARCO: cuánto mueve el giro de salida el centro hacia adelante y cuánto había libre */

    /* Registro del comando de movimiento en curso (RUTA_DIAG) y de los giros de la ruta. */
    int cmd_pedido;              /* lo que se pidió al empezar el comando: grados (giros) o mm (avance y retroceso); 0 = no se pidió nada (a mano) */
    int cmd_plan_ms;             /* duración planeada */
    int cmd_origen;              /* EstadoNav en el que empezó */
    NavGiro giros[NAV_MAX_GIROS];
    int     n_giros;             /* giros anotados (como máximo NAV_MAX_GIROS) */
    int     giros_perdidos;      /* giros que ya no cupieron */

    /* Giroscopio y láser (J11). */
    int      usa_giroscopio;         /* 1 = los giros se piden al firmware por ángulo (configuración; por defecto 0 = por tiempo como siempre) */
    int      usa_tof;                /* 1 = se hace caso al láser: TOF_PARO y la confirmación de la cámara (configuración; por defecto 0) */
    int      autoridad_laser;        /* apagado por defecto; solo lectura válida y fresca, la cámara elige el lado */
    int      contradiccion_laser;    /* última contradicción anunciada (0 ninguna, 1 láser cerca, 2 cámara cerca) */
    int      giro_fw_caido;          /* 1 = en esta ruta el giro por ángulo falló: el resto de los giros va por tiempo */
    int      giro_fw;                /* 1 mientras el giro actual lo hace el firmware */
    int      giro_fw_estado;         /* 0 = esperando la respuesta, 1 = ya llegó el ángulo real */
    int      giro_fw_pedir;          /* 1 = falta mandar GIRAR (la app lo toma con navegador_tomar_giro_fw) */
    int      giro_fw_grados;         /* grados pedidos con la convención del navegador (+ derecha) */
    int      giro_fw_real_x10;       /* grados por 10 que giró de verdad, en la dirección del comando */
    int      giro_fw_ms;             /* lo que tardó el firmware */
    int      giro_fw_ant;            /* anticipación del firmware en ms */
    uint32_t giro_fw_t_ms;           /* hora en que se pidió */
    int      giro_fw_ang_ini_x10;    /* ángulo del giroscopio (convención del firmware, + izquierda) al empezar el giro por ángulo; solo vale si giro_fw_ang_ini_ok */
    int      giro_fw_ang_ini_ok;
    int      giro_fw_consultando;    /* 1 desde que se acabó la espera sin respuesta hasta que se resuelve con IMU? (o el último SENS) */
    int      giro_fw_consulta_pedir; /* 1 = falta mandar IMU? (la app lo toma con navegador_tomar_consulta_imu) */
    uint32_t giro_fw_consulta_t_ms;
    int      sens_angulo_x10;        /* el último ángulo que dijo SENS (+ izquierda) y la hora en que llegó */
    uint32_t sens_angulo_t_ms;
    int      sens_angulo_ok;
    /* J15: distancia por tiempo, dirección observada con SENS durante avances.
     * Se conserva el residuo en milésimas de mm para no redondear cada tick. */
    int      sin_rumbo_medido;        /* solo comparación de pruebas: comportamiento previo */
    int      avance_imu_ref_ok, avance_imu_ref_x10, avance_rumbo_ref_x10;
    int      avance_rumbo_x10, avance_mm_previo;
    long     avance_x_mil, avance_y_mil;
    int      n_giros_angulo, n_giros_tiempo;   /* cuántos giros de la ruta fueron por ángulo y cuántos por tiempo (para el resumen final) */
    int      tof_vivo;               /* 1 si el láser funciona (según SENS) */
    int      tof_mm;                 /* última lectura (1200 = libre) */
    int      tof_valida;             /* 1 si esa lectura fue una medida real */
    int      tof_libre_lecturas;     /* lecturas seguidas «libres» (más de NAV_TOF_LIBRE_MM o sin eco) */
    int      tof_cerca_lecturas;     /* lecturas seguidas con una medida real menor de NAV_TOF_CONFIRMA_MM */
    uint32_t tof_t_ms;               /* hora de la última lectura */
    int      tof_paro_activo;        /* 1 desde TOF_PARO hasta que empieza a esquivar */
    int      tof_pendiente;          /* 1 = TOF_PARO llegó mientras esquivaba o volvía a la línea: lo atiende el siguiente tick */
    int      tof_paros;              /* TOF_PARO recibidos en esta ruta */
    int      tof_dist_mm;            /* distancia al obstáculo que midió el láser al detenerse (0 = no se sabe) */
    int      racha_solo_centro2;     /* 1 si la racha de bloqueo actual es SOLO cercanía 2 en el centro */
    int      tof_racha_nota;         /* 0 nada, 1 veto anotado, 2 confirmación anotada (para escribir SENS_DIAG una vez por racha) */
    int      override_largo_mm;      /* si > 0, largo forzado de la recta en mm (ej. 1500 mm) */

    /* Cola de líneas de diagnóstico (REINC_DIAG, ESQUIVE_DIAG, TURN_DIAG, RUTA_DIAG): ver navegador_diag_siguiente. */
    char diag[NAV_DIAG_LINEAS][NAV_DIAG_LARGO];
    int  diag_escritas, diag_leidas;
    int  diag_firma;             /* resumen de la última línea ESQUIVE_DIAG (para no repetirla en cada cuadro) */
    uint32_t diag_t_ms;
} Navegador;

/* ── Funciones ─────────────────────────────────────────────────────────── */

/* Deja el navegador en ESPERANDO, sin ruta. */
void navegador_iniciar(Navegador *nav);

/* Cambia la calibración del carro (por defecto se usa la de ejemplo de calibracion.h). Solo con el
 * navegador parado (ESPERANDO, TERMINADO o ERROR). Devuelve NAV_OK, NAV_ERR_ARGUMENTO (nula o no
 * válida) o NAV_ERR_ESTADO. Los pasos de una ruta ya cargada se calculan al comenzar, así que se
 * puede cambiar justo antes de navegador_comenzar. */
int navegador_poner_calibracion(Navegador *nav, const CalibracionCarro *cal);

/* Carga la ruta (lista de celdas contiguas, ver trayectoria.h). El carro se supone en el
 * primer punto mirando hacia ORIENTACION_INICIAL. Devuelve NAV_OK o un código de error. */
int navegador_cargar_ruta(Navegador *nav, const Punto *puntos, int n_puntos);

/* Empieza a seguir la ruta (o la retoma desde la posición estimada si se canceló antes).
 * Solo desde ESPERANDO. Una ruta de un solo punto termina de inmediato. */
int navegador_comenzar(Navegador *nav, uint32_t ahora_ms);

/* Cancela: STOP inmediato y vuelve a ESPERANDO (el siguiente tick devuelve STOP). */
void navegador_cancelar(Navegador *nav);

/* Una vuelta del cerebro. `ahora_ms` es un reloj en milisegundos (puede dar la vuelta a 32 bits).
 * `obs` es el resultado del último cuadro de obstaculos_analizar, o NULL si no hay cuadro
 * nuevo (entonces no cambia nada de lo que se sabe del obstáculo). Devuelve el comando
 * que el carro debe mantener hasta el siguiente tick. */
ComandoNav navegador_tick(Navegador *nav, uint32_t ahora_ms, const ResultadoObstaculos *obs);

/* Dice cuánto se atrasa la cámara (de 0 a 1000 ms): un cuadro que llega ahora muestra lo que pasó hace tanto. Con la cámara del
 * teléfono es ~0; con la cámara del carro por WiFi, ~300 ms. Así los cuadros que todavía muestran el carro sin girar no cuentan como
 * "cubiertos" y el pedazo de giro que la cámara aún no ve se completa por tiempo (el retraso queda compensado). Devuelve NAV_OK o
 * NAV_ERR_ARGUMENTO. */
int navegador_poner_latencia_visual(Navegador *nav, int ms);

/* Activa (1) o desactiva (0) el frenado desde lejos en el centro (ver NAV_MS_CONFIRMAR_CENTRO_LEJOS). Devuelve NAV_OK o NAV_ERR_ARGUMENTO. */
int navegador_poner_frenar_lejos_centro(Navegador *nav, int activo);

/* Le avisa al navegador cuánto vio girar la cámara entre dos cuadros consecutivos (movimiento_visual.c): `giro_grados_x10` es el
 * giro del carro por 10 (POSITIVO a la derecha), `dt_ms` el tiempo entre esos dos cuadros y `confiable` 1 si la cámara estaba segura.
 * Solo cuenta mientras el carro está girando (LEFT o RIGHT) y después del retardo de arranque (los cuadros durante el arranque se
 * ignoran: el carro todavía no se mueve). El giro se corta cuando la cámara ve el ángulo pedido (con el tiempo como respaldo). */
void navegador_aporte_visual(Navegador *nav, int giro_grados_x10, int dt_ms, int confiable);

/* 1 si el retroceso de los reintentos usa una calibración medida de verdad (CAL_MEDIDO_ATRAS); 0 si usa los ms por cm del avance (sin medir). */
int navegador_retroceso_calibrado(const Navegador *nav);

/* 1 = la ruta usa la cámara de obstáculos (la app la tiene abierta): el carro no se mueve hasta el primer cuadro y, si no llega en NAV_PRIMER_CUADRO_MAX_MS,
 * ERROR sin video (antes recorría la ruta entera a ciegas). 0 = sin cámara (fuente de botones y pruebas). Devuelve NAV_OK o NAV_ERR_ARGUMENTO. */
int navegador_poner_requiere_camara(Navegador *nav, int activo);

/* Activa (1) o desactiva (0) pedir ayuda en vez de dar ERROR cuando no puede esquivar (ver arriba, punto 7). Devuelve NAV_OK o NAV_ERR_ARGUMENTO. */
int navegador_poner_pedir_ayuda(Navegador *nav, int activo);

/* Enciende (1, por defecto) o apaga (0) la red de seguridad de la vuelta a la línea: tras cada tramo compara la pose estimada con la planeada y, si pasa de NAV_PLAN_ERROR_MM, vuelve a planear.
 * Apagarla sirve para ver en las pruebas qué aporta. Devuelve NAV_OK o NAV_ERR_ARGUMENTO. */
int navegador_poner_red_seguridad(Navegador *nav, int activo);

/* En AYUDA: el comando que el usuario quiere ahora (STOP, FORWARD, REVERSE, LEFT o RIGHT). Devuelve NAV_OK, NAV_ERR_ARGUMENTO o NAV_ERR_ESTADO (no está en AYUDA). */
int navegador_ayuda_comando(Navegador *nav, ComandoNav comando);

/* En AYUDA: el usuario terminó de maniobrar. El carro se queda quieto un momento, calcula con su posición estimada cómo volver a la línea (igual que tras esquivar) y sigue la ruta
 * (las esquivadas se cuentan de nuevo desde cero). Devuelve NAV_OK o NAV_ERR_ESTADO (no está en AYUDA). */
int navegador_ayuda_continuar(Navegador *nav, uint32_t ahora_ms);

/* GIROSCOPIO (J11). poner_giroscopio / poner_sensor_distancia encienden (1) o apagan (0) cada uno (por defecto apagados: el navegador se comporta como siempre). Devuelven NAV_OK o NAV_ERR_ARGUMENTO.
 * navegador_tomar_giro_fw: si hay un giro por ángulo que todavía no se pidió al firmware devuelve 1 y deja en *grados_izq_positivo los grados con la convención del firmware (POSITIVO = izquierda)
 * para mandar GIRAR:<grados>; solo devuelve 1 una vez por giro. navegador_giro_fw_en_curso: 1 mientras el firmware hace el giro (la app NO manda LEFT ni RIGHT en ese tiempo).
 * navegador_aporte_giro: llegó la respuesta del firmware: `codigo` es NAV_GIRO_*, `real_x10_fw` los grados por 10 que giró (POSITIVO = izquierda) y `ms` lo que tardó.
 * navegador_aporte_sens: llegó un SENS del firmware (cada ~100 ms): distancia del láser, si el láser funciona y si la medida es válida (no «libre»).
 * navegador_aporte_tof_paro: el firmware frenó por algo muy cerca al frente (TOF_PARO:<mm>). */
int  navegador_poner_giroscopio(Navegador *nav, int activo);
int  navegador_poner_sensor_distancia(Navegador *nav, int activo);
int  navegador_poner_autoridad_laser(Navegador *nav, int activo);
int  navegador_tomar_giro_fw(Navegador *nav, int *grados_izq_positivo);
int  navegador_giro_fw_en_curso(const Navegador *nav);
void navegador_aporte_giro(Navegador *nav, int codigo, int real_x10_fw, int ms, int ant, uint32_t ahora_ms);
void navegador_aporte_sens(Navegador *nav, int mm, int tof_vivo, int tof_valida, int angulo_x10_fw, uint32_t ahora_ms);
/* Igual, pero con el bit IMU lista de SENS: nunca integra un ángulo de IMU caída/calibrando. */
void navegador_aporte_sens_imu(Navegador *nav, int mm, int tof_vivo, int tof_valida, int angulo_x10_fw, int imu_lista, uint32_t ahora_ms);

/* MENSAJE DE GIRO PERDIDO (J12): si pasa la espera máxima sin GIRO_OK ni GIRO_TIMEOUT, antes de repetir el giro por tiempo el navegador pide IMU? (navegador_tomar_consulta_imu devuelve 1 una vez
 * para que la app lo mande) y compara el ángulo que contesta (navegador_aporte_imu, en grados por 10, + izquierda) con el del inicio del giro: si ya giró al menos NAV_GIRO_FW_YA_GIRO_PCT % de lo pedido NO
 * repite (usa lo medido como ángulo real); si no, repite por tiempo SOLO lo que falta. Sin respuesta del IMU? en NAV_GIRO_FW_CONSULTA_MS se usa el último SENS si es reciente. */
int  navegador_tomar_consulta_imu(Navegador *nav);
void navegador_aporte_imu(Navegador *nav, int angulo_x10_fw, uint32_t ahora_ms);
void navegador_aporte_tof_paro(Navegador *nav, int mm, uint32_t ahora_ms);

/* Saca la siguiente línea de diagnóstico que el navegador escribió y la copia a `texto` (capacidad `capacidad`, termina en '\0'). Devuelve 1 si había una
 * línea nueva y 0 si no. Las líneas empiezan con REINC_DIAG, ESQUIVE_DIAG, AYUDA_DIAG, TURN_DIAG o RUTA_DIAG (solo números y palabras; nada de imágenes). La app las llama después de cada tick
 * y las manda al log; si no se leen, las más viejas se pierden. */
int navegador_diag_siguiente(Navegador *nav, char *texto, int capacidad);

/* Cuántos giros recuerda de la ruta en curso (de 0 a NAV_MAX_GIROS) y los datos del giro `indice` (0 = el primero). Devuelve NAV_OK o NAV_ERR_ARGUMENTO. */
int navegador_cuantos_giros(const Navegador *nav);
int navegador_giro_datos(const Navegador *nav, int indice, NavGiro *giro);

/* Fija el largo en mm para una recta predefinida (ej. 1500 mm para 150 cm). 0 = usa el largo de las celdas. */
void navegador_poner_largo_recta_mm(Navegador *nav, int mm);

/* Texto para la pantalla de resultado: una línea por giro, «derecha: pedido 90°, hecho ~88°», lista para mostrar bajo el título «Giro pedido / Giro hecho (estimado)».
 * Devuelve el largo escrito (0 si no hubo giros). */
int navegador_texto_giros(const Navegador *nav, char *texto, int capacidad);

/* Frase que dice la causa concreta del ERROR, con los números (por ejemplo "se agotaron los 2 reintentos, el obstáculo seguía en el centro"). Sirve para
 * mostrarla en pantalla. Devuelve el largo escrito, o 0 si no hay error. */
int navegador_texto_error(const Navegador *nav, char *texto, int capacidad);

/* Texto del comando tal como lo entiende el firmware ("FORWARD", "STOP"...). */
const char *navegador_texto_comando(ComandoNav comando);

/* Texto del estado (para mostrar en pantalla o en los registros). */
const char *navegador_texto_estado(EstadoNav estado);

#ifdef __cplusplus
}
#endif

#endif /* NAVEGADOR_H */
