# Flujo del piso (Trabajo G)

Código: `nucleo_c/movimiento_piso.c/.h` (C puro, solo enteros), pruebas `nucleo_c/pruebas_movimiento_piso.c` (54 casos, necesita `-lm` solo el generador de pruebas),
entrada opcional de la fusión `fusion_aplicar_flujo` (`nucleo_c/fusion.c`), app: `FlujoNativa.kt`, `CamaraObstaculos.kt`, botón «Flujo del piso» en la pantalla de visión.
**APAGADO por defecto** (`ConfigVision.flujoActivo`).

## 1. Idea

Cuando el carro avanza, un punto del piso se mueve en la imagen de una forma que se puede calcular con la geometría de la cámara (altura, inclinación y campo de
visión del corredor, `docs/obstaculos.md`, «Corredor»; velocidad del perfil AUTO 193 mm/s medidos con PWM 210). Una cosa parada sobre el piso (caja, mochila) está
más cerca de la cámara que el piso que tapa y se mueve MÁS. El flujo mide el movimiento de bloques de 16x16 entre dos cuadros y lo compara con lo esperado.

- **Esperado:** el punto del piso a `d` cm pasa a `d − s` cm (`s` = velocidad × ms entre cuadros); su fila nueva sale de `y = f (h cos − d sen) / (d cos + h sen)` y su columna se abre en la misma proporción en que crece `A(v)`. Se prueba en PC con un trazador de rayos (la escena real, no la fórmula contra sí misma).
- **Medido:** el bloque del cuadro anterior se busca en una ventana (esperado −2 … +8 px abajo, ±3 px a los lados) del cuadro nuevo, con la suma de diferencias absolutas (un píxel de cada dos), y la mejor se afina a un cuarto de píxel con una parábola.
- **Estados por bloque:** `SIN_DATO` (sin textura, o la mejor coincidencia no es única: borde recto, dibujo repetido, ruido), `PISO` (se movió como el piso), `CERCA` (se movió claramente más), `DUDOSO` (no coincide con nada razonable). **Sin textura = sin dato, nunca obstáculo.**
- **Cuándo se evalúa:** solo si el carro avanza y los cuadros están entre 40 y 300 ms de distancia (la app usa el más reciente que tenga al menos 180 ms de edad: el piso se mueve solo 1 o 2 píxeles en 100 ms).
- **Por zona:** una zona es «piso consistente» si tiene al menos 3 bloques con dato, ninguno `CERCA` y no más de la mitad dudosos.

## 2. Regla de la fusión (el color manda)

`fusion_aplicar_flujo` (después de `fusion_decidir` / `fusion_decidir_cualquiera`):
1. El color marca obstáculo con **cercanía baja (1)** en una zona que el flujo ve como **piso con textura consistente**, y nadie más (modelo) lo confirma (fuente = detector): se **baja**: la zona queda libre, queda un aviso y se cuenta en `bajados_flujo`. Con cercanía 2 o 3 el color **nunca** se baja.
2. El flujo ve algo «más cerca que el piso» donde el color no ve nada: **solo aviso, nunca frena**.
3. Sin flujo vigente (carro quieto, cuadros muy seguidos o muy separados, flujo apagado): no cambia nada.

## 3. Lo que se midió en PC (cuadros sintéticos; HECHOS, no hipótesis sobre el carro real)

| Escena | Resultado |
|---|---|
| Baldosas de 2 colores (10 cm) y damero (5 cm), 12 variantes con ruido y tiempos de 80 a 120 ms | nunca «cerca»; siempre algún bloque de piso (9 a 19 de ~90) |
| Piso liso | todo «sin dato» |
| Piso negro con ruido (20 semillas, sigma 3 a 5) | todo «sin dato»; ni «cerca» ni piso inventado |
| Caja con textura de 14 cm de ancho, cuadros separados 200 ms | «cerca» en la zona central a 20, 24 y 30 cm; **a 36 cm ya no** (el piso y la caja se mueven casi igual) |
| Caja con textura sobre piso liso / negro con ruido | «cerca» en el centro |
| Caja de un solo color (sin textura) | **el flujo no la ve** (queda sin dato; para eso está el color) |

Límites que hay que tener presentes (de la geometría, no del código):
- Con cuadros separados 100 ms el flujo de una caja a 20–30 cm apenas difiere 1 a 2 px del del piso: **no distingue**. Con ~200 ms sí, pero solo hasta ~30 cm. Es un detector de **cerca**, no de lejos.
- La mayoría de los bloques de un piso liso o de baldosas grandes quedan «sin dato». El flujo sirve para confirmar un piso con juntas o dibujo, no para ver cualquier cosa.
- Si la velocidad real no es 193 mm/s (por ejemplo con la batería baja), lo esperado se desajusta: un exceso hace falsos «cerca» (solo avisan); un defecto da «dudoso». Corrígelo con `flujoVelocidadMmS` en `ConfigVision` (hoy fija a 193).

## 4. Registro `FLUJO_DIAG`

Una línea por cuadro analizado (solo números):
`FLUJO_DIAG evaluado=1 motivo=0 dt_ms=190 avance_mm=36 piso=9 cerca=0 dudoso=1 sin_dato=80 filas=3-8 I=p3/c0/d0/s20/consistente1/cerc0 C=... D=... ms=3.2`
- `motivo`: 0 evaluado, 1 apagado, 2 el carro no avanza, 3 tiempo entre cuadros fuera de rango. Zonas I (izquierda), C (centro), D (derecha): `p` piso, `c` cerca, `d` dudoso, `s` sin dato.
- Cuando el flujo baja un falso positivo de color: `FLUJO_DIAG bajados_por_flujo=N`.

## 5. PENDIENTE HARDWARE

1. Medir el tiempo del análisis en el teléfono (campo `ms=` de `FLUJO_DIAG`; la meta es < 10 ms para no frenar el detector).
2. Encender «Flujo del piso» en la pantalla de visión, correr una ruta recta de 150 cm sobre **piso de 2 colores** sin obstáculo y revisar con `adb logcat -d -s FLUJO_DIAG`: que `cerca` sea 0 casi siempre y que `bajados_por_flujo` aparezca cuando el color marcaba obstáculo lejano.
3. Lo mismo con una **caja con dibujo** a 120 cm del arranque: ¿a qué distancia pasa a `cerca` en la zona central? (la simulación dice ~30 cm).
4. Con piso **negro** y **liso**: debe salir casi todo `sin_dato` y `cerca=0`.
5. Pegar los números en la sección 6.

## 6. Resultados con el carro

(vacío: pendiente de las pasadas del punto 5)
