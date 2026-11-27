# Fusión: detector de color + modelo de objetos

Código: `nucleo_c/fusion.c/.h` (C puro, probado en PC con `pruebas_fusion.c`: 86 casos, 0 fallos en la corrida del 25-nov-2026), `FusionNativa.kt` (JNI) y la integración en `CamaraObstaculos.kt`. La versión anterior de este documento describía otra regla («el detector manda» y el modelo podía frenar solo con confianza de 0.6); el código de la v3 usa **consenso por zona**, y es lo que se describe aquí. Si algo no coincide, manda el código.

## Para qué

- El **detector de color** (`obstaculos.c`, ~7 ms por cuadro) ve cualquier cosa que no se parezca al piso (una caja, una pared) pero no sabe qué es.
- El **modelo de objetos** (EfficientDet-Lite0 con TensorFlow Lite) reconoce personas, mascotas, sillas, botellas... aunque tengan el color del piso, pero tarda más y a veces se equivoca.
- La fusión pide que los dos coincidan en la misma zona antes de frenar por visión. Un falso positivo de uno solo no detiene el carro; lo que ve una sola fuente se anota como aviso.

## Reglas (en el orden en que las aplica `fusion_decidir`)

1. **El resultado del modelo debe estar vigente.** Si tiene más de `vigencia_ms` (300 ms) de edad respecto a la hora de ahora, se ignora por completo (`modelo_vigente = 0`). Una caja con confianza menor que `confianza_min_mil` (600, es decir 60 %) o con tamaño nulo tampoco cuenta.
2. **Cada caja se proyecta a las tres zonas** (izquierda, centro, derecha: tercios del ancho). La caja cuenta en una zona si la cubre al menos `solape_camino_mil` (60 milésimas del ancho). La cercanía (1 a 3) sale del borde de abajo de la caja con los mismos límites del detector (`OBS_MEDIA_MIL` = 667 y `OBS_CERCA_MIL` = 834): desde 834 es 3, desde 667 es 2, si no 1. Una caja grande (`area_grande_mil` = 200, es decir 20 % del cuadro) sube una cercanía, hasta 3.
3. **Consenso por zona.** En cada zona se guarda de dónde viene la evidencia (`fuentes_vision`: bit 1 = color, bit 2 = modelo). Solo cuando el detector de color y el modelo ven algo **en la misma zona**, esa zona queda con obstáculo, con la mayor de las dos cercanías. Esto vale también en las zonas laterales.
4. **Una fuente sola solo avisa.** Si únicamente el color o únicamente el modelo ve algo en una zona, la zona fusionada queda sin obstáculo y se anota un aviso (`aviso`, con zona y cercanía).
   > **ADVERTENCIA (actualizado con las correcciones del 26-nov-2026):** La descripción anterior indicaba que un láser libre nunca descartaba el consenso visual. Con `autoridad_laser = 1` y lectura ToF fresca (`solo_laser = nav->autoridad_laser && tof_fresco(nav, ahora_ms)` en `nucleo_c/navegador.c`), **el láser tiene autoridad en el centro**: solo `laser_cerca` confirma obstáculo en `ZONA_CENTRO`, y las reglas de visión pura (consenso de fuentes en `registrar_cuadro`, precaución visual, cercanía 3 inmediata, bordes laterales `borde_izq`/`borde_der`, extensión `costado_a_la_vista` y chequeos de entrada por color en `tick_reincorporando`) quedan inhibidas por `!solo_laser`. La cámara conserva la capacidad de disparar un vistazo lateral. Si `autoridad_laser` está apagado o el ToF no está fresco, se aplica el consenso visual descrito arriba.
5. **Severidad.** De las zonas fusionadas: cercanía 3 es `PARAR`, 2 es `FRENAR`, 1 es `AVISO`; se toma la mayor. Si no hay obstáculo pero sí un aviso, la severidad es `AVISO`.
6. **Quién decidió** (`fuente`): color, modelo o ambos, según los bits de las zonas.

Campos que quedan solo por compatibilidad: `detector_solo_decide` siempre vale 0 y `confianza_frenar_mil` ya no hace que el modelo frene solo.

## Clases de interés

Se pueden marcar con `fusion_marcar_clase` (por defecto: persona, gato, perro, botella, silla y mesa; la app las busca por nombre en las etiquetas del modelo). En el código actual sirven para configuración y presentación: **cualquier clase** con confianza suficiente puede confirmar al color.

## Otras entradas

- `fusion_decidir_cualquiera`: recibe zonas del modo «cualquier objeto» (`nucleo_c/cualquiera.c`) ya filtradas por persistencia y aplica el mismo consenso con el detector de color.
- `fusion_aplicar_flujo`: el flujo del piso (`movimiento_piso.c`) solo aporta avisos; no crea consenso ni borra uno existente.

## En la app

`ConfigVision.usarModelo` decide si además del detector de color se usa el modelo de objetos (por defecto sí, según el comentario del archivo). El modelo corre aparte del hilo de la cámara, para que el detector de color no espere por él. Los detalles del hilo y de la pantalla de visión están en el código de `CamaraObstaculos.kt` y `ConfigVision.kt`.

## Pruebas en PC

`pruebas_fusion.c` (86 casos): configuración y clases, caja a zona y a cercanía, caducidad del modelo, consenso y avisos, flujo y modo «cualquier objeto». Es una prueba **SIMULADA** de la lógica; no se midió con el modelo y objetos reales en el carro.

## PENDIENTE HARDWARE

- Medir en el teléfono los milisegundos del modelo, cada cuánto sale un resultado y el efecto sobre los fps del detector de color.
- Probar con objetos reales (una persona o una silla en el camino y a un lado, un objeto del color del piso) y anotar la clase, la cercanía y si frena a tiempo.
- Ajustar `vigencia_ms`, `confianza_min_mil`, `solape_camino_mil` y `area_grande_mil` (están en `ConfigFusion`) con esas pruebas.

## Límites

- La cercanía del modelo sale del borde de abajo de la caja: un objeto alto y lejano con la base cortada parece cercano, y uno que no toca el piso tendrá una cercanía engañosa.
- Como se exige consenso, un objeto que **solo** ve el modelo (por ejemplo, del color del piso) no frena por visión: queda como aviso y depende del láser y de las demás capas de seguridad.
- Las etiquetas del modelo son las de COCO: no conoce muchos objetos de un salón (cables, mochilas pequeñas...); para eso está el detector de color.
