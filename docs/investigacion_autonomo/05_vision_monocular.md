# 05. Visión monocular para evitar obstáculos

Investigación técnica para el carro FSI (rama `feat/autonomo-v3`). Fecha de consulta de todas las fuentes externas: **2026-11-16**.
Convenciones: **[n]** remite a la sección 5. Confianza: **alta** = artículo revisado, hoja de datos o documentación oficial; **media** = resumen o fuentes secundarias coherentes; **baja** = inferencia mía, marcada **INFERENCIA**. **CÁLCULO/SIMULADO** = sale de una cuenta mía con valores SUPUESTOS (script `calculos/geometria_monocular.py`); **HECHO (proyecto)** = medido y escrito en los documentos del repositorio. Todo lo que dependa de la altura (115 mm), la inclinación (12°) o el campo (56°) de la cámara es **NO MEDIDO**.

Cómo leí las fuentes: [1] y [2] (PDF) las leí completas página por página; [3] y [5] solo las páginas indicadas; [4], [7] y [8] por resumen. Las páginas web pasaron por un resumidor automático. Lo que no abrí, lo digo en la sección 5 ("NO CONSULTADA").

> **Corrección previa que afecta a otros documentos.** El encargo, el manual (cap. 8.2) y `00_mapa.md` dicen que el clasificador usa **YCbCr** con **3 clústeres**. El código dice otra cosa: `nucleo_c/obstaculos.c` no convierte a YCbCr en ningún sitio (búsqueda de `ycbcr`, `crominanc`, `cb`, `cr`, `luma`, `cluster` en `nucleo_c` y en la app: sin resultados en el clasificador; `luma` solo aparece en el simulador `mundo_v2.c`, y `movimiento_piso.c` calcula un gris `(77R+150G+29B)/256` para el flujo). Compara en **RGB**: suma `|ΔR|+|ΔG|+|ΔB|` ≤ `UMBRAL_COLOR` (40), o, si no, el mismo **tono por croma rg normalizada** (cada canal / (R+G+B), escala 256, tolerancia `UMBRAL_CROMA` 30) con brillo `R+G+B` que no difiera más de `UMBRAL_BRILLO` (150), y un caso aparte para celdas oscuras (`BRILLO_OSCURO` 150: solo luminancia). Los grupos del piso son `OBS_MAX_GRUPOS` = **4**, no 3 (`obstaculos.h:154`). El manual también describe la franja como «filas 6 a 8, columnas 2 a 7» y `OBS_MAX_CLUSTERS = 3`; esos nombres no existen en el código leído. Esta investigación usa lo que dice el código.

---

## 1. Cómo funciona

### 1.1 La idea de los métodos de cámara única

- **Criterio de obstáculo por apariencia.** En los sistemas por apariencia, un obstáculo es lo que *difiere en apariencia del suelo*, no lo que sobresale de él [1, p.1]. Ulrich y Nourbakhsh lo apoyan en tres supuestos: (1) los obstáculos difieren del suelo en apariencia; (2) el suelo es relativamente plano; (3) no hay obstáculos colgantes [1, p.2]. Los supuestos 2 y 3 son los que permiten estimar distancia.
- **Horswill / Polly (1993, publicado en AAAI-93).** El robot Polly procesaba una imagen de **64×48** cada 66 ms (15 Hz) con una DSP TI C30 [2, p.824–825]. Reduce «detectar obstáculos» a «detectar alfombra» gracias a dos restricciones: la **restricción de textura del fondo** (la alfombra del laboratorio no tenía textura, así que cualquier región con textura es un objeto) y la **restricción de plano del suelo** (todo descansa sobre un piso plano, por tanto lo más lejano aparece más arriba en la imagen) [2, p.825]. Un detector de bordes por umbral de gradiente da, por columna, el primer punto no-piso desde abajo (`RDM(x) = min{ y | (x,y) no es piso }`), y esa altura en la imagen es la distancia [2, p.826]. El artículo de IROS 1994 (segmentación visual para evitar colisiones) **no lo pude leer**; lo que sigue de Horswill sale del de 1993 y de cómo lo describe [1].
- **Lorigo, Brooks y Grimson (1996 tesis, IROS 1997).** Según la descripción de [1, p.2] (fuente secundaria): usan las **diez filas inferiores** de la imagen como área de referencia y comparan histogramas de ventanas pequeñas con ese área; asumen que justo delante del robot no hay obstáculos; trabajan con imágenes de 64×64; los **brillos/reflejos (highlights)** son un problema particular. El texto original **no lo pude abrir** (403), así que las cifras de este párrafo son de segunda mano.
- **Ulrich y Nourbakhsh (2000).** Cuatro pasos en la versión simple: (1) filtrar la imagen de **320×260** con un gaussiano 5×5; (2) pasar a **HSI**, asignando matiz y saturación solo si la intensidad pasa un mínimo (el color es muy ruidoso con poca intensidad) y matiz solo si la saturación pasa otro mínimo (el matiz carece de sentido con poca saturación); (3) histogramar matiz e intensidad del área de referencia trapezoidal; (4) un píxel es obstáculo si el valor del histograma de matiz o el de intensidad en su bin queda por debajo de un umbral (60 y 80 píxeles) [1, p.3]. El umbral de matiz es menor que el de intensidad porque no todo píxel tiene matiz válido [1, p.3]. Sin filtrado de manchas posterior [1, p.5].
- **Área de referencia profunda.** Un área de referencia superficial (diez filas) es poco representativa más arriba en la imagen (se ve con otro ángulo) y un reflejo se clasifica como obstáculo cercano; uno profundo es mejor pero obliga a que esté libre de obstáculos [1, p.3]. Ulrich lo resuelve **con odometría**: lo que el robot recorrió en línea relativamente recta hace ~1 m se supone libre; se guardan histogramas en una cola de candidatos y pasan a una cola de referencia cuando el robot se alejó más de 1 m; se descartan si el rumbo cambió más de un umbral angular [1, p.4].

### 1.2 Espacios de color e invariancia a la iluminación

- **Modelo físico.** El color medido es una parte «de cuerpo» (depende de la geometría y de la iluminación) más otra especular; para superficies mates con luz blanca solo queda la parte de cuerpo [3, p.454–455; 5, p.2476]. Bajo ese modelo (reflectancia dicromática, iluminación blanca), el **rgb normalizado** (R/(R+G+B), G/(R+G+B), B/(R+G+B)), la **saturación** y el **matiz** son invariantes a la dirección de visión, a la geometría del objeto y a la dirección de la luz; el **matiz** (y l1l2l3) lo son además a los brillos especulares; y la exactitud de **todas** las características salvo m1m2m3 se degrada cuando cambia el **color** de la luz [3, resumen, p.453]. Es decir: la normalización por brillo cubre sombras suaves y degradados, **no** cubre reflejos especulares (rgb normalizado no figura entre los invariantes a brillos) ni un cambio de temperatura de color.
- **Constancia de color.** Estimar el color de la luz a partir de una sola imagen está sub-determinado y los métodos recurren a supuestos como «mundo gris» [5, p.2476]. La corrección de la luz solo ajusta cromaticidad, no escala el brillo [5, p.2476].
- **Sombra como «misma cromaticidad, menos brillo».** Horprasert et al. clasifican cada píxel RGB contra un fondo esperado con dos números: distorsión de brillo (escala que acerca el color esperado al observado) y distorsión de cromaticidad (distancia ortogonal a la recta que pasa por el origen y el color esperado). Sombra = cromaticidad parecida y brillo menor; resalte = cromaticidad parecida y brillo mayor; primer plano = cromaticidad distinta [4]. Es la misma lógica de la regla B del proyecto (mismo tono, distinto brillo).
- **Alternativa más fuerte.** Finlayson et al. derivan, bajo supuestos sobre luces y cámara, una representación 1D independiente de la iluminante (sin sombras) y la extienden a cromaticidad 2D [13, solo resumen]. Pide cámara calibrada: no aplica sin medir el sensor.
- **Cuál usar.** Ulrich elige HSI porque matiz y saturación son menos sensibles a la luz que la intensidad [1, p.3] y propone como mejora probar YUV, RGB normalizado y colores oponentes, o histogramas 2D [1, p.6]: ningún espacio es «el correcto»; el artículo lo deja como pregunta abierta.

### 1.3 Sombras, manchas de sol y reflejos

- **Sombras.** Polly frena por sombras si son suficientemente fuertes para disparar el detector de bordes; las sombras suelen ser difusas y no lo disparan [2, p.828]. En el detector de Ulrich, una sombra proyectada desde un cuarto vecino se clasificó mal como obstáculo, pero la sombra de un poste se ignoró y dos sombras grandes en exterior se ignoraron [1, p.5].
- **Piso brillante.** Polly no funcionó en el piso 9 del edificio (muy brillante): frenaba por el reflejo de las lámparas del techo [2, p.828]. Ulrich lista como falsos positivos clásicos del detector de bordes: pisos brillantes, fronteras entre alfombras y sombras; y falsos negativos: bordes débiles entre piso y obstáculo [1, p.1].
- **Luz con color.** Una luz amarillenta de ventana sobre el piso se resolvió bien con su método [1, p.5], pero con el modo «regular» (histogramas fijos de entrenamiento) aparecen falsos positivos si la luz difiere de la del entrenamiento [1, p.4].
- **Poca luz.** En Brown, Polly tuvo problemas por luz insuficiente para su cámara [2, p.828].

### 1.4 Aprendizaje y adaptación del modelo del piso

Ulrich define tres modos [1, p.4]: **regular** (histogramas fijos aprendidos al entrenar, buenos con iluminación estática), **adaptativo** (conjunto estático + conjunto dinámico que se actualiza durante la marcha; se combinan con OR; el dinámico conserva solo los **10 elementos más recientes** y «olvida» lo viejo) y **asistido** (solo dinámico; si cambia el piso el usuario conduce 1–2 m para que aprenda). El conjunto dinámico solo incorpora lo ya recorrido, justamente para **no aprender el obstáculo** que tiene delante [1, p.4]. El caso que él mismo no resuelve: un objeto de color parecido al piso (una pata amarilla, Fig. 3e) y la parte baja de una pared del mismo material [1, p.5].

### 1.5 Geometría del plano del suelo (proyección inversa)

- **Principio.** Con suelo plano y sin objetos colgantes, la distancia es una función monótona creciente de la altura del píxel en la imagen; es exacta en la **base** del obstáculo y se **sobreestima** para las partes altas [1, p.2; 2, p.825]. Lo más simple es usar solo el píxel obstáculo más bajo de cada columna, o agrupar y dar la distancia más corta al grupo [1, p.2]. El *inverse perspective mapping* convierte la imagen en vista de pájaro del plano del suelo y trata como obstáculo todo lo que no cumple el movimiento esperado del plano; sus autores reportan robustez a errores en los parámetros de cámara [8, resumen].
- **Fórmulas (agujero, sin distorsión).** Campo horizontal FOV y ancho W: `f = (W/2) / tan(FOV/2)` [9: α = 2·arctan(d/2f), solo lentes rectilíneas]. Cámara a altura `h`, eje inclinado `θ` hacia abajo. Un punto del piso a distancia horizontal `d` cae en la fila `v = H/2 + f·tan(atan(h/d) − θ)`, o al revés `d = h / tan(θ + atan((v − H/2)/f))`. Con `v = H/2` queda `d = h/tan θ`. Es la misma geometría que implementa `nucleo_c/corredor.c` (`corredor_distancia_de_fila`, con `A(v) = f·sen θ + (v−cy)·cos θ`). La relación «d = h / tan(ángulo)» del encargo es el caso del rayo que baja `ángulo` grados bajo la horizontal.
- **Consecuencia práctica.** La distancia no es lineal con la fila: el horizonte (`v = H/2 − f·tan θ`, d → ∞) cae en la fila 40 de 176 con los valores supuestos, la fila 50 ya corresponde a 2,7 m, la 60 a 1,3 m y la 100 a 0,43 m (CÁLCULO). Por encima del horizonte no hay piso (`corredor.c` devuelve «sin piso» si `A(v) ≤ 0`).

### 1.6 Flujo óptico y *looming*

- **τ (Lee, 1976).** Lee propuso que el frenado se controla con la razón entre el ángulo visual de un objeto y su tasa de cambio, llamada τ, que estima el tiempo hasta el choque sin conocer distancia ni velocidad [6, que cita a Lee 1976; el artículo original **no lo leí**]. Hosking et al. recuerdan que los supuestos que sostienen τ limitan su utilidad en entornos naturales [6]. La fórmula `τ ≈ θ/(dθ/dt)` y su lectura como tiempo al contacto con velocidad de acercamiento constante son conocimiento estándar que no verifiqué en una fuente abierta (marcado **INFERENCIA** en la sección 6).
- **Divergencia del campo de flujo (Nelson y Aloimonos, 1989).** Muestran que la «divergencia direccional» del campo de movimiento 2D indica obstáculos para un observador con rotación y traslación generales, y que se puede medir en secuencias reales de imágenes [7, solo resumen].
- **Relación con el proyecto.** `docs/flujo_piso.md` implementa otra variante (flujo por bloques de 16×16 contra el esperado del piso) y reporta que con cuadros a 100 ms una caja a 20–30 cm apenas difiere 1–2 px del piso. Ver la cuenta de la sección 2.3.

### 1.7 Cámara de baja resolución

La separación angular por píxel fija el objeto mínimo visible. Con 240 px y 56° (**SUPUESTO**) son 4,29 px/° (0,233°/px). Con celdas de 10×10 el detector trabaja a 0,23° × 10 = **2,3° por celda**, y exige `MIN_CELDAS` = 7 celdas «no piso» confirmadas en una zona. Lo que implica para una pata de mueble está en 2.3.

---

## 2. Parámetros y límites relevantes con cifras

### 2.1 Cifras de la literatura (para comparar)

| Dato | Cifra | Fuente |
|---|---|---|
| Resolución de Polly / paso de tiempo | 64×48 cada 66 ms (15 Hz); la base se vuelve inestable a 1,5 m/s | [2, p.824–825, 827] |
| Resolución de Lorigo (según [1]) | 64×64 | [1, p.2] (secundaria) |
| Resolución y tiempo de Ulrich | 320×260; ~200 ms por cuadro en un Pentium II a 333 MHz (hasta 10 Hz usando solo la mitad inferior) | [1, p.5] |
| Suavizado previo | Polly: filtro pasa-bajos 3×3 (sobre 64×48); Ulrich: gaussiano 5×5 | [1, p.1, p.3] |
| Umbrales de Ulrich | 60 (matiz) y 80 (intensidad), en píxeles de bin del histograma | [1, p.3] |
| Referencia de Lorigo / de Ulrich | 10 filas inferiores / unos 1 m de profundidad por odometría | [1, p.2, p.4] |
| Memoria adaptativa de Ulrich | los 10 elementos más recientes en el conjunto dinámico | [1, p.4] |
| Umbrales de sombra de Horprasert | cota inferior de brillo normalizado 0,4 en su configuración; los demás umbrales se aprenden de histogramas del fondo (las distribuciones no son gaussianas); un valor mínimo por defecto de 0,55 contra falsos positivos agrupados cuando la varianza de cromaticidad es muy pequeña | [4] |
| Cámara de Ulrich | CCD Hitachi KPD-50 con lente de iris automático, sobre una silla de ruedas | [1, p.4] |

### 2.2 Geometría del carro: fila ↔ distancia (CÁLCULO/SIMULADO, valores SUPUESTOS h = 115 mm, θ = 12°, FOV = 56°, 240×176)

`f` = 225,7 px; horizonte en la fila 40; el eje óptico toca el piso a 541 mm; `fila_minima` 44 % = fila de celdas 7 (píxel 70) con la regla `filas·440/1000` de `obstaculos.c`.

| Fila de celdas | Borde de arriba → abajo (mm) | Qué es en el proyecto |
|---|---|---|
| 7 | 881 → 654 | **primera fila de la zona de avance** (`OBS_FILA_MINIMA_MIL` 440) |
| 8–10 | 654 → 363 | cercanía 1 (lejos) |
| 11–13 | 363 → 247 | cercanía 2 (`OBS_MEDIA_MIL` 667: fila ≥ 11) |
| 14–16 | 247 → 184 | cercanía 3 (`OBS_CERCA_MIL` 834: fila ≥ 14) |

Comparación: `nucleo_c/navegador.c:2668` usa, para cercanía 3 / 2 / 1, 250 / 375 / 600 mm «de la lente a lo visto» y lo marca «SUPUESTO: cámara a 115 mm, 12 grados». Los 250 y 375 coinciden con el borde de arriba de las filas 14 y 11 (247 y 363); el 600 queda entre los bordes de las filas 7–8 (654 y 518). Es coherente, pero es el mismo supuesto usado dos veces: **no es una validación**.

Resolución de la relación (mm por fila de píxel): 30 mm/px en la fila 70, 7,5 en la 100, 4,2 en la 120, 2,7 en la 140. La cuantización no es el problema; lo es el modelo.

**Incertidumbre de la distancia de una fila** (el contacto con el piso del objeto en la fila 110, nominal 363 mm; mismo script):

| Variación | Distancia resultante |
|---|---|
| altura ±10 mm (105 / 125 mm) | 332 / 395 mm (−9 % / +9 %) |
| altura redondeada a 11 o 12 cm (`ALTURA_CAMARA_CM` es entero en `corredor.h`) | 347 / 379 mm (−4 % / +4 %) |
| inclinación ±3° (9° / 15°) | 443 / 306 mm (+22 % / −16 %) |
| campo 48° / 64° | 384 / 343 mm (+6 % / −5 %) |
| peor combinación hacia cerca (105 mm, 15°, 64°) / hacia lejos (125 mm, 9°, 48°) | 266 mm / 514 mm (−27 % / +42 %) |

La **inclinación** domina; el campo casi no importa para la distancia (sí para el ancho en píxeles). Las cifras para las filas 125 y 140 están en el script y tienen el mismo orden de variación.

**Campo según el modo (INFERENCIA, ver `06_gc0308_camara.md` §1.6).** En el driver de Espressif, con el modo de submuestreo (el valor por defecto de su Kconfig), 240×176 sale de una ventana centrada de 480×352 reducida a la mitad, es decir, el **75 % del ancho del sensor** que ve QVGA/VGA. Si el campo de 56° se supuso para QVGA, HQVGA vería unos **43,5°** (`calculos/geometria_monocular.py`); si se supuso para HQVGA, QVGA vería ~70,6°. Hay que medir el campo **en el modo que se usa**.

### 2.3 Qué ve el detector de una pata de mueble (CÁLCULO/SIMULADO, mismos supuestos, HQVGA)

Ancho en píxeles = `ancho_mm · f / d` (agujero). «Celdas» = celdas de 10×10 de la zona de avance (filas ≥ 70) con al menos 25 % de su área cubierta por la pata, la pata vertical y alta, y barriendo 20 posiciones horizontales (mínimo–máximo):

| Pata | A 300 mm | A 500 mm | A 800 mm |
|---|---|---|---|
| 25 mm | 18,8 px; 11–16 celdas | 11,3 px; 2–4 celdas | 7,1 px; 0 |
| **30 mm** | **22,6 px (2,3 celdas de ancho); 12–17 celdas** | **13,5 px (1,35 celdas); 2–4 celdas** | **8,5 px (0,85 celdas); 0** |
| 35 mm | 26,3 px; 17–22 celdas | 15,8 px; 4–6 celdas | 9,9 px; 0–1 |

- La base de la pata cae en la fila 123,6 a 300 mm, 91,8 a 500 mm y 72,9 a 800 mm; la zona de avance empieza en la 70. A 500 mm solo hay 22 filas de píxel de zona por encima del contacto, y a 800 mm solo 3.
- Con `MIN_CELDAS` = 7 por zona, una pata aislada **no cuenta a 500 mm ni a 800 mm** aunque contraste fuerte (2–4 y 0 celdas) y cuenta a 300 mm (12–17). Para 7 celdas a 500 mm haría falta un objeto de ≥ ~70 mm de ancho (7 celdas ≈ 700 px² / 22 filas ≈ 32 px = 70 mm). Esto concuerda con lo escrito en `docs/limites_conocidos.md`: «el color las ve tarde y delgadas» (SIMULADO: banquito 765 de 1000; silla 316 de 1000).
- Contraste de una celda parcialmente cubierta: una celda cubierta al 30 % aporta 0,3 × (contraste L1 con el piso); para pasar `UMBRAL_COLOR` 40 hace falta contraste L1 > 133 (**INFERENCIA** a partir de `distancia_color`, que promedia la celda).
- **Consistencia con un dato medido.** `docs/obstaculos.md` dice que, con `fila_minima` 44 %, `min_celdas` 7 y umbral 40, «la mochila se vio desde ~80 cm» (HECHO, proyecto). A 800 mm una caja ancha solo ocupa 3 filas de píxel de la zona (30 % de la fila de celdas 7): una mochila oscura y ancha (varias celdas con contraste L1 ≫ 133) sí puede dar 7 celdas; una pata no. La cuenta no contradice lo medido (no lo prueba).
- **Ampliación en 100 ms** a 193 mm/s (velocidad AUTO medida en el proyecto): 30 mm a 800 mm pasa de 8,46 a 8,67 px (+0,21), a 500 mm de 13,54 a 14,08 (+0,54), a 300 mm de 22,57 a 24,12 (+1,55). El tiempo al contacto d/v es 4,15 / 2,59 / 1,55 s. A 500 mm el cambio por cuadro es una fracción de píxel; detectar *looming* de una pata más allá de ~30 cm no es realista con este sensor (**INFERENCIA**; concuerda con `docs/flujo_piso.md`).
- Con QVGA (320 px) todo ancho en píxeles se multiplica por 1,33 si el campo es el mismo, pero las celdas siguen siendo de 10×10 px y la cuenta de celdas apenas mejora (**INFERENCIA**, no calculada).

### 2.4 Valor usado en el proyecto vs. lo que dice la fuente

| Parámetro | Valor en el proyecto (archivo) | Lo que dice la fuente |
|---|---|---|
| Espacio de color | RGB: `|ΔR|+|ΔG|+|ΔB|`, croma rg normalizada, brillo R+G+B (`obstaculos.c:54–91, 552–570`) | Ulrich: HSI; propone YUV o RGB normalizado como mejoras [1, p.3, p.6]. Horprasert: RGB con brillo/cromaticidad [4]. Gevers: rgb normalizado, S y H invariantes a geometría y dirección de la luz bajo luz blanca y reflectancia dicromática [3] |
| «YCbCr, 3 clústeres» (manual 8.2, `00_mapa.md`) | **No existe en el código**: K = 4 grupos (`OBS_MAX_GRUPOS`) | — |
| Resolución de análisis | celdas de 10×10 sobre 240×176 → 24×17 (`OBS_LADO_CELDA`) | Polly 64×48; Lorigo 64×64; Ulrich 320×260 por **píxel** [1, 2] |
| Suavizado | promedio de 100 px por celda | Polly 3×3; Ulrich gaussiano 5×5 [1] |
| Umbral de color | 40 de 765 (`UMBRAL_COLOR`) | Sin equivalente: Ulrich umbraliza bins de histograma (60/80) [1, p.3] |
| Tolerancia de brillo del mismo tono | 150 de 765 (`UMBRAL_BRILLO`) ≈ ±29 % de un piso con R+G+B = 524 | Sombra = brillo menor con cromaticidad igual, hasta una cota inferior aprendida (0,4 en su configuración) [4]. Una sombra fuerte (factor 0,4–0,6) queda **fuera** de ±29 % (**INFERENCIA**); `docs/obstaculos.md` ya lo admite |
| Brillo mínimo para usar el tono | `SUMA_MIN_CROMA` 90 (30 por canal); oscuro `BRILLO_OSCURO` 150 | Ulrich: matiz/saturación solo con intensidad y saturación sobre un mínimo (valores no dados en el texto leído) [1, p.3]; Horprasert: oscuros cerca del origen se confunden con sombra [4] |
| Referencia del piso | franja del 80 % de ancho y `OBS_REF_ALTO_MIL` 84 ‰ = 1 fila de 17 (`17·84/1000`), más «Aprender piso» (~1 s, toda la zona de avance, carro quieto) | Lorigo 10 filas; Ulrich ~1 m recorrido por odometría [1, p.2, p.4] |
| Aprendizaje | 20 ‰ por cuadro (2 %), solo avanzando sin obstáculo, `piso_fijo` en ruta | Ulrich adaptativo: 10 elementos recientes, solo zonas recorridas, estático + dinámico con OR [1, p.4] |
| Fila mínima | 44 % (fila 7 de 17) → piso desde 881 mm (borde de arriba) | Ground-plane constraint [2]; sin cifra comparable |
| `MIN_CELDAS` | 7 | Ulrich: sin filtrado de manchas [1, p.5]; Horswill: cualquier no-piso en la columna [2] |
| Cercanía | fracciones 667/834 ‰ del alto → 363 y 247 mm (SIMULADO) | Distancia monótona con la altura en la imagen, exacta en la base [1, p.2] |
| Geometría del corredor | `corredor.h`/`ConfigVision.kt`: 10 cm, 20°, 60°, carro 18 cm; el manual: 115 mm, 12°, 56°, carro 150 mm + 50 mm a cada lado | Ninguna fuente da los del carro; `docs/auditoria_autonomo_v2.md:44` ya señala el desacuerdo |
| Tiempo por cuadro | 6,85 ms por cuadro en el Huawei (HECHO, proyecto, 320×240) | Ulrich ~200 ms con 320×260 en 1999 [1, p.5] |

---

## 3. Errores típicos y modos de falla conocidos

1. **Objeto del color del piso** (Fig. 3e de Ulrich; pared del mismo material): invisible a todo método por apariencia [1, p.5]. Para este carro: `docs/obstaculos.md` («pared o piso del mismo color») y `docs/limites_conocidos.md`.
2. **Referencia contaminada.** Si el área de referencia incluye un obstáculo, se aprende como piso [1, p.3]. Polly no tiene memoria: solo frena por lo que está en el campo de visión [2, p.828].
3. **Brillos y reflejos especulares** en piso brillante: falsos obstáculos [1, p.1; 2, p.828]. El rgb normalizado no es invariante a ellos [3, resumen]. Dependen de la posición del carro: el reflejo se queda en las mismas filas, la mancha de sol baja (hipótesis ya formulada en `docs/auditoria_autonomo_v2.md:136–145`).
4. **Sombras fuertes**: falsos obstáculos en [1, Fig. 3c] y en Polly si son bastante fuertes [2, p.828].
5. **Cambio de luz o de color de luz** respecto del entrenamiento: falsos positivos del modo «regular» [1, p.4]; todas las invariantes salvo m1m2m3 se degradan si cambia el color de la luz [3].
6. **Dos o más alfombras / baldosas** con frontera: la frontera se toma por obstáculo [2, p.828]. El proyecto lo trata con K = 4 grupos, franja ancha y colores intermedios entre grupos.
7. **Objetos oscuros ≈ sombra.** Con un modelo de «misma cromaticidad, menos brillo» un objeto oscuro cerca del origen RGB parece sombra de cualquier cosa; hace falta una cota inferior de brillo [4].
8. **Distancia sobreestimada** para partes altas del objeto y para objetos colgantes (mesas) [1, p.2, p.6]. Una mesa con cubierta alta no se ve como obstáculo por debajo.
9. **Objetos más finos que la celda** (patas): ver 2.3.
10. **Poca luz**: ruido de color alto, matiz inválido [1, p.3]; Polly falló en un edificio mal iluminado [2, p.828].
11. **Parámetros de cámara mal estimados**: error de distancia de hasta −27 % / +42 % con ±10 mm, ±3° y ±8° de campo (CÁLCULO, 2.2). IPM tolera errores moderados en sus parámetros [8].
12. **Ventana de referencia ≠ lo que ve el carro avanzando**: una referencia superficial es poco representativa arriba en la imagen [1, p.3]. En el carro, «Aprender piso» con el carro quieto abarca toda la zona de avance, pero con la exposición automática de ese momento.

---

## 4. Buenas prácticas reconocidas

- **Usar la restricción de plano de suelo por columna**: el obstáculo lo da el no-piso *más bajo* de cada columna o grupo, y la distancia se lee de su base, no de su cima [1, p.2; 2, p.825–826].
- **Referencia profunda y ya recorrida**, no solo la tira de abajo: hace el modelo más representativo y evita aprender lo que está delante [1, p.3–4].
- **Aprender solo lo seguro**: el modo adaptativo de Ulrich aprende solo de lo recorrido, olvida lo viejo (10 elementos) y mantiene un conjunto estático para no perder el piso original [1, p.4].
- **Combinar el color con una segunda fuente** de naturaleza distinta (rango): Ulrich cierra su artículo proponiendo la combinación con un sensor de rango por la complementariedad, y señala que las mesas son un problema que también tienen los sensores de rango con campo estrecho [1, p.6].
- **Separar brillo y cromaticidad** y exigir brillo mínimo antes de confiar en el tono [1, p.3; 4].
- **Elegir un espacio de color con medición**, no por costumbre: el propio Ulrich deja abierta la comparación [1, p.6].
- **Medir la cámara**: altura, inclinación y campo; registrar el modo de video con el que se midió (el campo cambia con el modo, 2.2).
- **Registrar números, no imágenes**: la telemetría `VTEL` del proyecto (histograma de distancia al piso, brillo, celdas por fila) sigue esta línea.

---

## 5. Fuentes

1. Iwan Ulrich, Illah Nourbakhsh. *Appearance-Based Obstacle Detection with Monocular Color Vision.* Proc. AAAI-00 (Austin, TX), pp. 866–871, 2000. https://cdn.aaai.org/AAAI/2000/AAAI00-133.pdf — consultada 2026-11-16, **leída completa (6 páginas, como imágenes)**. «p.N» en este documento = página N del PDF (1 a 6, que son las 866 a 871 del volumen). Confianza: **alta** (artículo revisado).
2. Ian Horswill. *Polly: A Vision-Based Artificial Agent.* Proc. AAAI-93, pp. 824–829, 1993. https://cdn.aaai.org/AAAI/1993/AAAI93-123.pdf — consultada 2026-11-16, **leída completa** (páginas escaneadas, leídas como imágenes; «p.N» = página del volumen). Confianza: **alta**. (Es el artículo de 1993, no el de IROS 1994.)
3. Theo Gevers, Arnold W. M. Smeulders. *Color-based object recognition.* Pattern Recognition 32 (1999) 453–464. https://staff.fnwi.uva.nl/th.gevers/pub/GeversPR99.pdf — consultada 2026-11-16; **leí las páginas 1–3** (resumen, definiciones, modelo de reflexión); el resto (tablas de invarianza, experimentos) **NO leído**. Confianza: **alta** para lo citado.
4. T. Horprasert, D. Harwood, L. S. Davis. *A Statistical Approach for Real-time Robust Background Subtraction and Shadow Detection.* IEEE ICCV'99 FRAME-RATE Workshop, Kerkyra, 1999. https://vast.uccs.edu/~tboult/frame/Horprasert/index.html — consultada 2026-11-16; **solo la página web resumen**, no el PDF. Confianza: **media**.
5. Arjan Gijsenij, Theo Gevers, Joost van de Weijer. *Computational Color Constancy: Survey and Experiments.* IEEE Trans. Image Processing 20(9):2475–2489, 2011. https://staff.fnwi.uva.nl/th.gevers/pub/GeversTIP11.pdf — consultada 2026-11-16; **leí las páginas 1–2**. Confianza: **alta** para lo citado.
6. Simon G. Hosking, Catherine E. Davey, Mary K. Kaiser. *Visual cues for manual control of headway.* Front. Behav. Neurosci. 7:45, 2013. https://frontiersin.org/articles/10.3389/fnbeh.2013.00045/full — consultada 2026-11-16 (primeros 100 000 caracteres). La uso solo para citar cómo describe τ de Lee (1976) y sus límites. Confianza: **media** (secundaria sobre Lee).
7. Randal C. Nelson, John (Yiannis) Aloimonos. *Obstacle Avoidance Using Flow Field Divergence.* IEEE Trans. PAMI 11(10):1102–1106, 1989, DOI 10.1109/34.42840. https://www.cs.rochester.edu/users/faculty/nelson/pubs/abstracts/1989_pami.html — consultada 2026-11-16; **solo el resumen** de la página de los autores (los índices difieren en el nombre de pila del segundo autor). Confianza: **media**.
8. H. A. Mallot, H. H. Bülthoff, J. J. Little, S. Bohrer. *Inverse perspective mapping simplifies optical flow computation and obstacle detection.* Biological Cybernetics 64(3):177–185, 1991, DOI 10.1007/BF00201978. Registro: https://link.springer.com/doi/10.1007/BF00201978 — **no abrí la página**; la información (resumen e indexación) salió de resultados de búsqueda del 2026-11-16 (p. ej. https://pure.mpg.de/pubman/item/item_1794702_2). Confianza: **media**.
9. Wikipedia, *Angle of view (photography)*, fórmula α = 2·arctan(d/2f) para lentes rectilíneas. https://en.wikipedia.org/wiki/Angle_of_view_(photography) — consultada 2026-11-16. Confianza: **media**.
13. Graham D. Finlayson, Steven D. Hordley, Cheng Lu, Mark S. Drew. *On the Removal of Shadows from Images.* IEEE Trans. PAMI 28(1):59–68, 2006, DOI 10.1109/TPAMI.2006.18. https://ueaeprints.uea.ac.uk/id/eprint/3867/ — **solo el resumen**, vía resultados de búsqueda 2026-11-16. Confianza: **media**.

**NO CONSULTADAS (texto):**
- I. Horswill, *Visual collision avoidance by segmentation*, IROS 1994, pp. 902–909, DOI 10.1109/IROS.1994.407486: solo el registro bibliográfico (WebSearch) y cómo lo describe [1].
- L. Lorigo, R. Brooks, W. E. L. Grimson, *Visually-guided obstacle avoidance in unstructured environments*, IROS 1997, pp. 373–379, DOI 10.1109/IROS.1997.649086, y la tesis MIT de Lorigo (1996, 22 MB): el PDF de core.ac.uk dio 403 y la copia de scispace no devolvió texto. De este trabajo solo uso lo que cuenta [1, p.2] y el resumen de búsqueda (tres módulos independientes, probado más de 200 horas). Los tres criterios del sistema no los pude confirmar.
- D. N. Lee, *A theory of visual control of braking based on information about time-to-collision*, Perception 5(4):437–459, 1976: no abierta.
- R. Cucchiara et al., *Detecting moving objects, ghosts, and shadows in video streams*, IEEE PAMI 25(10):1337–1342, 2003: solo el registro; no confirmé que use HSV en esa versión, por eso no la uso.
- Páginas 4–10 de [3] (tablas de invarianza y experimentos) y el resto de [5] (descripción de «mundo gris»).

**Fuentes internas leídas** (no son literatura): `nucleo_c/obstaculos.c/.h`, `corredor.c/.h`, `mascara_vision.c`, `movimiento_piso.c`, `navegador.c:2668`, `docs/flujo_piso.md`, `docs/obstaculos.md`, `docs/vision_huawei.md`, `docs/modelo_objetos.md`, `docs/limites_conocidos.md`, `docs/auditoria_autonomo_v2.md`, `docs/autonomo_v2_diseno.md`, `docs/manual/capitulos/08_vision.md`, `app_android/.../ConfigVision.kt`, `cajas_negras/cn_20261115_*.txt` (sin abrir imágenes).

---

## 6. Qué implica para este carro

Cada punto: qué hacer, cómo verificarlo y qué es inferencia.

1. **Corregir el relato «YCbCr / 3 clústeres»** en el manual (8.2), `00_mapa.md` y lo que se diga en la exposición: el código compara en RGB y croma normalizada con K = 4. *Verificable:* `grep -n -i "ycbcr\|OBS_MAX_CLUSTERS" nucleo_c/*.c nucleo_c/*.h` (hoy sin resultados). Si la afirmación deseada es «YCbCr», habría que implementarlo y compararlo contra el actual en `pisos_sinteticos.c`: [1, p.6] lo deja como pregunta abierta y [3] da base teórica para normalización, pero ninguna fuente dice que YCbCr gane.
2. **Unificar la geometría supuesta.** `corredor.h` y `ConfigVision.kt` tienen 10 cm / 20° / 60° y el manual 115 mm / 12° / 56°. Además `ALTURA_CAMARA_CM` es **entero en cm**: 115 mm no se puede representar (11 o 12 cm, ∓4 % de distancia, 2.2). Pasar la altura a mm (o décimas de cm) es un cambio de interfaz pequeño y verificable con una prueba en `pruebas_corredor.c` (h = 115 mm → fila ↔ distancia como en `calculos/geometria_monocular.py`).
3. **Medir h, θ y FOV, y medirlos en el modo de video que se usa.** Procedimiento barato: tres a cuatro marcas transversales de cinta (5 cm de ancho) en el piso a 20, 30, 40, 50, 60 y 80 cm medidos desde la vertical de la lente, el carro quieto, y leer la fila del borde de cada marca en el cuadro; ajustar por mínimos cuadrados `d = h / tan(θ + atan((v − H/2)/f))` con (h, θ, f) libres (6 puntos, 3 incógnitas). Alternativa sin ver imágenes: empatar `f=` por fila de `VTEL` con la lectura `tof` de la caja negra cuando el carro se acerca a una caja (idea ya anotada como V5 en `docs/autonomo_v2_diseno.md`). Repetir en HQVGA y en QVGA: **INFERENCIA** a partir del driver de que el campo cambia 25 % entre ambos. Criterio: si la inclinación real difiere más de ±3° o la altura más de ±10 mm, las bandas 363/247 mm de arriba cambian −16 % a +22 %.
4. **Aceptar que el color por celdas no ve patas finas** y decidir con eso: a 500 mm una pata de 30 mm da 2–4 celdas contra `MIN_CELDAS` 7 (CÁLCULO). Dos caminos, ambos **INFERENCIA** hasta simularlos: (a) dejarlo al láser y al vistazo (diseño actual; el láser pasa entre las patas según `docs/limites_conocidos.md`); (b) añadir al detector una regla de **continuidad vertical anclada al suelo**: una columna de celdas «no piso» que arranque en la zona de avance (base entre las filas 7 y 16) y siga hacia arriba, aunque pase la fila mínima, cuenta como un obstáculo delgado; la restricción de plano [2] hace que cosas lejanas (cuya base queda por encima de la fila 7) no entren. Probarlo primero en simulación con una pata sintética sobre `pisos_sinteticos.c` (que ya genera piso, ruido, sombras y reflejos solo como números) y medir falsos positivos con puertas, marcos y muebles de fondo antes de tocar el firmware. Cambiar a celdas de 5×5 cuadruplica los bytes del paquete UDP (24×17×3 = 1224 B hoy; 48×34×3 ≈ 4900 B, más de un MTU): rompería el modo celdas.
5. **Sombras, sol y reflejos: medirlos antes de tocar el clasificador.** El proyecto ya tiene las pruebas C1–C4 (sombra, mancha de luz a un lado y sobre la línea, cambio de luz tras aprender) pendientes en `docs/pruebas_fisicas_20261114.csv`. Con la telemetría `VTEL` (histograma `h=`, `d=`, brillo `b=`) se distingue «todo el histograma se corre» (exposición/balance) de «unas celdas de una zona» (sombra/mancha) (`docs/manual/capitulos/08_vision.md` 8.6). Cuando se midan, evaluar con [4] una regla de sombra **asimétrica y con cota inferior de brillo** (aceptar oscurecimiento con cromaticidad parecida solo hasta cierto factor): cualquier cota baja que acepte sombras fuertes también acepta objetos oscuros del mismo tono (el propio caveat de [4]). **INFERENCIA:** hoy ±29 % de brillo deja pasar degradados y deja fuera sombras fuertes; el costo de ampliarlo es ver menos objetos oscuros.
6. **Mantener `piso_fijo` en la ruta** (coincide con la prudencia de Ulrich: aprender solo lo ya recorrido [1, p.4]). Si algún día se activa aprendizaje continuo en ruta, hacerlo con celdas de la zona que el carro ya pisó, no con el cuadro actual, y vigilar la deriva con los colores `g=` de `VTEL`. El 2 % por cuadro a ~12 fps tiene constante de tiempo de ~4 s (**INFERENCIA** por la fórmula de `aprender_del_cuadro`): una iluminación que cambie despacio la sigue, pero también un gradiente de piso a objeto si todas sus celdas quedan dentro de la mitad del umbral del grupo.
7. **El *looming* y el flujo no sirven para patas lejanas** (0,2–0,5 px por cuadro a 50–80 cm, 2.3); su lugar es confirmar cosas a < 30 cm, como ya concluye `docs/flujo_piso.md`. No invertir más ahí antes de tener los números de la sección 6 de ese documento.
8. **Resolver una discrepancia de datos.** Las cajas negras del 15-nov (`cajas_negras/cn_20261115_*.txt`) registran `VTEL c=32x24`, o sea rejilla de 320×240; la documentación dice que el modo por defecto es HQVGA (24×17). Cualquiera de los dos puede ser cierto (p. ej. un cambio de tamaño por `AUTOPRUEBA`, ya documentado), pero todas las cuentas de este documento cambian con el tamaño y con el modo. *Verificable:* añadir `config.tam` de `/stats` a la cabecera `CAB camara_stats=` de la caja negra.
9. **Lo que sigue NO MEDIDO:** altura, inclinación y campo; el campo por modo; el contraste real de una pata con el piso; la distancia a la que el detector ve realmente una pata; la sombra y la mancha de sol en el carro; el comportamiento del modelo adaptativo durante una ruta larga.
