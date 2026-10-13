# Fusión: detector de color + modelo de objetos

Código: `nucleo_c/fusion.c/.h` (C puro, probado en PC con `pruebas_fusion.c`), `ModeloEnHilo.kt` (el modelo en su
propio hilo), `FusionNativa.kt` (JNI) y la integración en `CamaraObstaculos.kt` y `VisionActivity.kt`.

## Para qué

- El **detector de color** (`obstaculos.c`, ~7 ms por cuadro) ve cualquier cosa que no se parezca al piso (una caja, una
  pared) pero no sabe qué es y no ve objetos casi del color del piso.
- El **modelo de objetos** (EfficientDet-Lite0 con TensorFlow Lite, CPU 1 hilo, ~45 ms) reconoce personas, mascotas, sillas,
  botellas... aunque tengan el color del piso, pero tarda más, a veces se equivoca y no conoce todo.
- La fusión los junta: **el detector manda en seguridad**; el modelo ayuda cuando el detector duda o no ve.

```
cámara ──cuadro──► hilo de la cámara:  detector de color (~7 ms) ───────────────┐
                        │  ofrecer(cuadro)  (no espera)                          ▼
                        └──────────────►  hilo "modelo-objetos" (≤ 10 por s) ─► fusion_decidir ─► zonas fusionadas ─► navegador
                                          preparar → inferir → postprocesar        ▲                (mismo formato que el detector)
                                          resultado con hora del cuadro  ─────────┘ (se ignora si tiene más de 300 ms)
```

## Reglas (en orden de importancia)

1. **El detector manda en seguridad.** Si el detector ve un obstáculo cerca (cercanía 2 o 3), la salida es esa y el carro se
   detiene **sin esperar al modelo** (`detector_solo_decide` = 1). Si el modelo dice otra cosa, está caducado o ni existe,
   da igual: la fusión **nunca baja** lo que ve el detector (probado: el modelo ve el objeto "lejos" y el detector "muy cerca"
   → se queda en muy cerca).
2. **Modelo ve un objeto EN EL CAMINO y el detector duda → sube la severidad.** "Duda" es: el detector ve algo pero menos cerca
   que el modelo, o tiene unas pocas celdas "no piso" que no llegan al mínimo (`MIN_CELDAS`). La zona sube a la cercanía del
   modelo (fuente: ambos).
3. **Modelo ve un objeto LEJANO que el detector no ve → solo avisa**, no frena.
4. **Modelo ve un objeto CERCANO (cercanía 2 o 3) que el detector no ve en absoluto → frena solo si el modelo está muy seguro**
   (confianza de 0.6 o más); si no, solo avisa. (Decisión mía: la frase pedida solo cubre el caso lejano.)
5. **Un resultado del modelo más viejo que 300 ms se ignora por completo** (`vigencia_ms`, ajustable en `ConfigFusion`). Si el
   modelo se cuelga, el detector de color sigue decidiendo solo y el carro frena igual.
6. Una caja con confianza menor que 0.4 no cuenta.

## De la caja del modelo a zona y cercanía

El modelo da una caja en milésimas del cuadro (0 a 1000, origen arriba a la izquierda).

- **Zona**: la caja está **en el camino** si cruza la zona central (333 a 667) al menos 60 milésimas del ancho (`solape_camino_mil`);
  entonces es zona CENTRO. Si no, es IZQUIERDA o DERECHA según su centro.
- **Cercanía** (1 a 3): por el **borde de abajo** de la caja, con los mismos límites del detector de color (`OBS_MEDIA_MIL` = 667
  y `OBS_CERCA_MIL` = 834): desde 834 es 3, desde 667 es 2, si no 1. Una caja **grande** (que ocupa 20 % o más del cuadro,
  `area_grande_mil` = 200) sube una cercanía (hasta 3): una persona muy cerca llena el cuadro aunque su borde de abajo no baje tanto.
- Es solo una aproximación (no mide centímetros): igual que la cercanía del detector.

## Clases de interés

Configurables con `fusion_marcar_clase` (en la app, por nombre: persona, gato, perro, botella, silla, mesa; ver
`FusionNativa.CLASES_DE_INTERES`). Las clases de interés **fuera del camino** generan un aviso (por ejemplo, una persona a un lado);
las demás clases fuera del camino se ignoran. **Cualquier clase cuenta igual cuando está en el camino**: el modelo no necesita
"conocer" el objeto para que frene.

## El modelo en su propio hilo (`ModeloEnHilo`)

- Hilo aparte ("modelo-objetos"); abre el intérprete, lo usa y lo cierra en ese mismo hilo.
- `ofrecer(cuadro)` desde el hilo de la cámara **no espera**: deja el cuadro más reciente como pendiente (reemplaza al anterior si no
  se había tomado: nunca se acumulan cuadros viejos).
- Corre **como máximo 10 veces por segundo** (`INTERVALO_MIN_MS` = 100 ms) y publica un resultado inmutable con las tres etapas medidas.
- La marca de tiempo del resultado es la del **cuadro**, no la de cuando terminó: así los 300 ms de vigencia ya incluyen lo que
  tardó el modelo.
- Si el modelo no abre o falla, queda un mensaje de error y el resto sigue con el detector de color.

## En la app

Pantalla de visión: botón «Modelo de objetos (fusión): SÍ/no» (guardado; **apagado por defecto** hasta probarlo en el teléfono) y,
cuando está encendido, un texto con **quién decidió** (detector, modelo o ambos), la severidad, si el detector frena solo, la edad del
resultado del modelo, los avisos y los **milisegundos de cada etapa**: detector (conversión + análisis), modelo (preparar + inferir +
postprocesar y cada cuánto sale un resultado) y fusión. En la imagen se dibujan en celeste las cajas del modelo con su nombre y
confianza y las zonas de abajo son las fusionadas. El modo semiautónomo usa el mismo ajuste (`ConfigVision.usarModelo`).

## Pruebas en PC (`pruebas_fusion.c`, 67 casos, todos pasan)

Configuración y clases (bits de las 96 clases); caja → zona (solapes de 47 y 67 milésimas, cajas fuera de la imagen) y cercanía (límites
834/833, 667/666, caja grande); el detector manda (sin modelo, con modelo vacío, con **contradicciones**); **modelo caducado** (300 ms vigente,
301 caducado, vigencia configurable, reloj atrasado y reloj de 32 bits que da la vuelta); el modelo sube lo que el detector duda; modelo ve algo
que el detector no ve (frena con confianza 0.9, solo avisa con 0.5 o si es lejano); clases de interés; errores; y una **línea de tiempo** con cuadros
cada 80 ms y resultados del modelo cada 120 ms (se manda PARAR en el primer cuadro en que el detector ve el obstáculo, 1040 ms, sin esperar al modelo; si el
modelo se cuelga, su último resultado caduca a los 300 ms y el detector sigue frenando solo).

## PENDIENTE HARDWARE

1. Instalar la app y encender «Modelo de objetos (fusión)» en la pantalla de visión: comprobar que no se cae y leer las etapas en pantalla.
2. Medir en el Huawei: ms del modelo (se esperan ~45 ms), cada cuánto sale un resultado (se esperan ~100 ms), si los fps del detector de color
   bajan al encender el modelo (el hilo del modelo usa otro núcleo), y el estado térmico tras varios minutos.
3. Probar con objetos reales: una persona o una silla en el camino y a un lado; una botella; un objeto del color del piso. Anotar qué clase
   sale, la cercanía que calcula la fusión y si frena a tiempo.
4. Ajustar `vigencia_ms`, `confianza_frenar_mil`, `solape_camino_mil` y `area_grande_mil` con las pruebas (están en `ConfigFusion`).

## Límites

- La cercanía del modelo sale del borde de abajo de la caja: un objeto alto y lejano con la base cortada parece cercano, y uno que no toca el piso
  (colgado) tendrá una cercanía engañosa.
- A 12 fps y con el modelo a 10 por segundo, el resultado del modelo puede tener de 0 a ~145 ms más de edad que el cuadro del detector
  (hay 300 ms de vigencia).
- El modelo se equivoca (falsos positivos con confianza baja): por eso solo frena por su cuenta con 0.6 o más y con el objeto cerca.
- Las etiquetas del modelo son las de COCO (80 clases): no conoce muchos objetos de un salón (cables, mochilas pequeñas...); para eso está el detector de color.
