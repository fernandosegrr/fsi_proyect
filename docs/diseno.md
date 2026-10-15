# Sistema de diseño de la app

Esta es la guía visual de la app del carro. La capa visual usa **Views y Material Components** (Material 3); no hay Compose. La lógica (en C y en Kotlin) no depende del diseño: si cambia un color o una tarjeta, no se toca ningún módulo del núcleo.

Principios, en orden de importancia:

1. **Calma y legibilidad.** Panel de control moderno: pocas cosas, bien separadas. Nada de aspecto de herramienta de depuración.
2. **Una sola cosa grande por pantalla.** Es lo que importa en ese momento (el estado de conexión, el video, el estado de la ruta...).
3. **Los colores significan siempre lo mismo.** Verde = bien, ámbar = aviso, rojo = paro o peligro. El acento (cian) solo marca acciones.
4. **Cada error dice qué pasó y qué hacer**, en una frase cada cosa, sin códigos crudos.
5. **Lo que mueve el carro se toca fácil**: botones de manejo de 56 dp o más y un PARO siempre a la vista.

## 1. Colores (tokens)

Los valores viven en `app_android/app/src/main/res/values/colors.xml` (tema claro) y `values-night/colors.xml` (tema oscuro, **el predeterminado**). Los dos archivos tienen los mismos nombres; el código y los estilos solo usan los nombres, nunca un hexadecimal suelto.

| Token | Claro | Oscuro | Para qué |
|---|---|---|---|
| `fondo` | `#F3F6F9` | `#0C131A` | Fondo de toda pantalla (gris azulado muy oscuro en el tema oscuro) |
| `superficie` | `#FFFFFF` | `#131C25` | Tarjetas, barra inferior, cabecera |
| `superficie_alta` | `#E9EFF4` | `#1A2531` | Botones de manejo, diálogos, indicador de la barra |
| `borde` | `#D3DCE4` | `#26323F` | Borde fino de las tarjetas (decorativo) |
| `borde_fuerte` | `#74879A` | `#5B6E80` | Bordes de botones y campos (3:1 o más) |
| `texto` | `#0F1B26` | `#E7EDF3` | Texto principal |
| `texto_suave` | `#4A5D6E` | `#9BAEC0` | Texto secundario, etiquetas |
| `acento` | `#0B7285` | `#22C5DD` | **El único color de acción** (botón primario, interruptores) |
| `acento_texto` | `#0B7285` | `#3BD0E6` | El acento cuando se usa como texto o icono |
| `sobre_acento` | `#FFFFFF` | `#03232B` | Texto sobre el botón primario |
| `ok` / `ok_fondo` | `#176B3A` / `#DDF3E4` | `#4ADE80` / `#11301E` | Verde: bien, conectado |
| `aviso` / `aviso_fondo` | `#8A5300` / `#FDEFD0` | `#FBBF24` / `#3A2C0B` | Ámbar: aviso, hay que mirar |
| `peligro` / `peligro_fondo` | `#B3261E` / `#FBE1DF` | `#FF8A8F` / `#3D1A1E` | Rojo: error o peligro |
| `neutro_fondo` | `#E3EAF0` | `#1F2B38` | Chip sin información |
| `paro` / `sobre_paro` | `#C62828` / `#FFFFFF` | `#D32F2F` / `#FFFFFF` | El botón de PARO (solo se usa para eso) |

**Tema.** Oscuro por defecto. En Ajustes se puede elegir Claro o «Sistema» (sigue al teléfono). `AppCarro` aplica el tema al abrir y `Diseno.barras()` ajusta el color de los iconos de la barra de estado.

### Contraste comprobado (mínimo 4.5:1)

`python scripts/contraste.py` lee los dos `colors.xml`, calcula el contraste WCAG de cada pareja texto/fondo y sale con error si alguna baja de **4.5:1** (3:1 para bordes y el color del PARO sobre el fondo, que son gráficos). Resultado de la última corrida: **todas las parejas pasan** en los dos temas.

| Pareja | Tema | Texto | Fondo | Contraste | Minimo | Resultado |
|---|---|---|---|---|---|---|
| Texto principal sobre el fondo | claro | `texto` #0F1B26 | `fondo` #F3F6F9 | 16.07:1 | 4.5:1 | pasa |
| Texto principal sobre el fondo | oscuro | `texto` #E7EDF3 | `fondo` #0C131A | 15.85:1 | 4.5:1 | pasa |
| Texto principal sobre una tarjeta | claro | `texto` #0F1B26 | `superficie` #FFFFFF | 17.43:1 | 4.5:1 | pasa |
| Texto principal sobre una tarjeta | oscuro | `texto` #E7EDF3 | `superficie` #131C25 | 14.59:1 | 4.5:1 | pasa |
| Texto principal sobre una tarjeta elevada | claro | `texto` #0F1B26 | `superficie_alta` #E9EFF4 | 15.03:1 | 4.5:1 | pasa |
| Texto principal sobre una tarjeta elevada | oscuro | `texto` #E7EDF3 | `superficie_alta` #1A2531 | 13.16:1 | 4.5:1 | pasa |
| Texto suave sobre el fondo | claro | `texto_suave` #4A5D6E | `fondo` #F3F6F9 | 6.28:1 | 4.5:1 | pasa |
| Texto suave sobre el fondo | oscuro | `texto_suave` #9BAEC0 | `fondo` #0C131A | 8.20:1 | 4.5:1 | pasa |
| Texto suave sobre una tarjeta | claro | `texto_suave` #4A5D6E | `superficie` #FFFFFF | 6.81:1 | 4.5:1 | pasa |
| Texto suave sobre una tarjeta | oscuro | `texto_suave` #9BAEC0 | `superficie` #131C25 | 7.55:1 | 4.5:1 | pasa |
| Texto suave sobre una tarjeta elevada | claro | `texto_suave` #4A5D6E | `superficie_alta` #E9EFF4 | 5.88:1 | 4.5:1 | pasa |
| Texto suave sobre una tarjeta elevada | oscuro | `texto_suave` #9BAEC0 | `superficie_alta` #1A2531 | 6.81:1 | 4.5:1 | pasa |
| Texto del boton primario | claro | `sobre_acento` #FFFFFF | `acento` #0B7285 | 5.59:1 | 4.5:1 | pasa |
| Texto del boton primario | oscuro | `sobre_acento` #03232B | `acento` #22C5DD | 7.89:1 | 4.5:1 | pasa |
| Acento (enlaces, iconos) sobre el fondo | claro | `acento_texto` #0B7285 | `fondo` #F3F6F9 | 5.15:1 | 4.5:1 | pasa |
| Acento (enlaces, iconos) sobre el fondo | oscuro | `acento_texto` #3BD0E6 | `fondo` #0C131A | 10.10:1 | 4.5:1 | pasa |
| Acento sobre una tarjeta | claro | `acento_texto` #0B7285 | `superficie` #FFFFFF | 5.59:1 | 4.5:1 | pasa |
| Acento sobre una tarjeta | oscuro | `acento_texto` #3BD0E6 | `superficie` #131C25 | 9.30:1 | 4.5:1 | pasa |
| Acento sobre una tarjeta elevada | claro | `acento_texto` #0B7285 | `superficie_alta` #E9EFF4 | 4.82:1 | 4.5:1 | pasa |
| Acento sobre una tarjeta elevada | oscuro | `acento_texto` #3BD0E6 | `superficie_alta` #1A2531 | 8.39:1 | 4.5:1 | pasa |
| Chip OK | claro | `ok` #176B3A | `ok_fondo` #DDF3E4 | 5.63:1 | 4.5:1 | pasa |
| Chip OK | oscuro | `ok` #4ADE80 | `ok_fondo` #11301E | 8.22:1 | 4.5:1 | pasa |
| Chip aviso | claro | `aviso` #8A5300 | `aviso_fondo` #FDEFD0 | 5.56:1 | 4.5:1 | pasa |
| Chip aviso | oscuro | `aviso` #FBBF24 | `aviso_fondo` #3A2C0B | 8.14:1 | 4.5:1 | pasa |
| Chip error | claro | `peligro` #B3261E | `peligro_fondo` #FBE1DF | 5.27:1 | 4.5:1 | pasa |
| Chip error | oscuro | `peligro` #FF8A8F | `peligro_fondo` #3D1A1E | 6.80:1 | 4.5:1 | pasa |
| OK sobre una tarjeta | claro | `ok` #176B3A | `superficie` #FFFFFF | 6.56:1 | 4.5:1 | pasa |
| OK sobre una tarjeta | oscuro | `ok` #4ADE80 | `superficie` #131C25 | 9.87:1 | 4.5:1 | pasa |
| Aviso sobre una tarjeta | claro | `aviso` #8A5300 | `superficie` #FFFFFF | 6.33:1 | 4.5:1 | pasa |
| Aviso sobre una tarjeta | oscuro | `aviso` #FBBF24 | `superficie` #131C25 | 10.31:1 | 4.5:1 | pasa |
| Peligro sobre una tarjeta | claro | `peligro` #B3261E | `superficie` #FFFFFF | 6.54:1 | 4.5:1 | pasa |
| Peligro sobre una tarjeta | oscuro | `peligro` #FF8A8F | `superficie` #131C25 | 7.61:1 | 4.5:1 | pasa |
| Texto del boton de PARO | claro | `sobre_paro` #FFFFFF | `paro` #C62828 | 5.62:1 | 4.5:1 | pasa |
| Texto del boton de PARO | oscuro | `sobre_paro` #FFFFFF | `paro` #D32F2F | 4.98:1 | 4.5:1 | pasa |
| Texto sobre el chip neutro | claro | `texto_suave` #4A5D6E | `neutro_fondo` #E3EAF0 | 5.61:1 | 4.5:1 | pasa |
| Texto sobre el chip neutro | oscuro | `texto_suave` #9BAEC0 | `neutro_fondo` #1F2B38 | 6.30:1 | 4.5:1 | pasa |
| Texto del aviso en banda (aviso) | claro | `texto` #0F1B26 | `aviso_fondo` #FDEFD0 | 15.30:1 | 4.5:1 | pasa |
| Texto del aviso en banda (aviso) | oscuro | `texto` #E7EDF3 | `aviso_fondo` #3A2C0B | 11.52:1 | 4.5:1 | pasa |
| Texto del aviso en banda (error) | claro | `texto` #0F1B26 | `peligro_fondo` #FBE1DF | 14.06:1 | 4.5:1 | pasa |
| Texto del aviso en banda (error) | oscuro | `texto` #E7EDF3 | `peligro_fondo` #3D1A1E | 13.05:1 | 4.5:1 | pasa |
| Texto de la barra inferior (activo) | claro | `acento_texto` #0B7285 | `superficie` #FFFFFF | 5.59:1 | 4.5:1 | pasa |
| Texto de la barra inferior (activo) | oscuro | `acento_texto` #3BD0E6 | `superficie` #131C25 | 9.30:1 | 4.5:1 | pasa |
| Texto de la barra inferior (inactivo) | claro | `texto_suave` #4A5D6E | `superficie` #FFFFFF | 6.81:1 | 4.5:1 | pasa |
| Texto de la barra inferior (inactivo) | oscuro | `texto_suave` #9BAEC0 | `superficie` #131C25 | 7.55:1 | 4.5:1 | pasa |
| Borde de una tarjeta sobre el fondo | claro | `borde_fuerte` #74879A | `fondo` #F3F6F9 | 3.41:1 | 3.0:1 | pasa |
| Borde de una tarjeta sobre el fondo | oscuro | `borde_fuerte` #5B6E80 | `fondo` #0C131A | 3.55:1 | 3.0:1 | pasa |
| Paro sobre el fondo | claro | `paro` #C62828 | `fondo` #F3F6F9 | 5.18:1 | 3.0:1 | pasa |
| Paro sobre el fondo | oscuro | `paro` #D32F2F | `fondo` #0C131A | 3.75:1 | 3.0:1 | pasa |

## 2. Tipografía

**Inter** (familia sans, licencia **SIL Open Font License 1.1**; el texto está en `docs/licencias/Inter-OFL-1.1.txt`). Va incluida en la app como tres archivos **estáticos** de unos 120 KB cada uno (`res/font/inter_400.ttf`, `inter_500.ttf`, `inter_600.ttf`, recortados al alfabeto latino y conservando las cifras tabulares) y tres familias `inter`, `inter_medium` e `inter_semibold`. Se generan con `scripts/generar_fuentes_inter.py` a partir del archivo variable oficial. (Se probó primero el archivo variable con `fontVariationSettings`: en el Huawei P40 Pro los pesos no se aplicaban, por eso se pasó a archivos estáticos.) Tamaños en `sp` con su altura de línea propia (`android:lineHeight`): 40 sp para el grande, 32 título, 24 subtítulo y cuerpo, 20 ayudas y 16 etiquetas.

| Estilo | Tamaño y peso | Uso | Estilo XML |
|---|---|---|---|
| **Título** | 24 sp, seminegrita | Nombre de la pantalla | `Texto.Titulo` |
| **Subtítulo** | 18 sp, medio | Título de una tarjeta | `Texto.Subtitulo` |
| **Cuerpo** | 16 sp, normal | Texto corriente | `Texto.Cuerpo` (`Texto.Suave`, 14 sp gris, para ayudas) |
| **Etiqueta** | 12 sp, medio | Rótulos, chips, barra inferior | `Texto.Etiqueta` |

Además existe `Texto.Grande` (32 sp): **la única cosa grande de la pantalla** (por ejemplo el estado de la ruta en palabras). Las **cifras tabulares** (`fontFeatureSettings="tnum"`, todas las cifras con el mismo ancho) se usan en la latencia, los fps, los cm y los tiempos con `Texto.Cifras`, para que el número no «baile» cuando cambia.

## 3. Forma y espacio

- **Cuadrícula de 8 dp.** Todos los márgenes y espacios salen de `@dimen/e0` (4, solo medio paso), `e1` (8), `e2` (16), `e3` (24), `e4` (32), `e5` (48), `e6` (64). Margen de pantalla: 16 dp.
- **Esquinas:** 16 dp en tarjetas y botones de manejo, 28 dp en los botones principales (que con 48 o 56 dp de alto quedan como píldora) y en los chips (16 dp).
- **Bordes finos** (1 dp) en vez de sombras: las tarjetas no tienen elevación. Solo la barra inferior y los diálogos se separan por contraste de fondo.
- **Tamaño de toque:** los botones que **manejan el carro** miden **56 dp o más** (la cruceta, 80 dp; el PARO de la cruceta, 88 dp); el resto, **48 dp** como mínimo.
- Una columna, márgenes generosos; en horizontal el video va al centro y los controles a los lados.

## 4. Componentes reutilizables

| Componente | Dónde está | Cómo se usa |
|---|---|---|
| **Tarjeta de estado** | `Tarjeta` (MaterialCardView) | Un título (`Texto.Subtitulo`), una o dos líneas y un chip. Las de la pantalla de Inicio: Carro y Cámara. |
| **Chip de estado** | `ChipEstado` (`Diseno.kt`) | `chip.poner(Nivel.OK, "Conectado")`. Punto de color + texto corto. Niveles: OK (verde), AVISO (ámbar), ERROR (rojo), NEUTRO (gris). No se toca. |
| **Punto de conexión** | `PuntoConexion` | Un punto de 12 dp; con nivel OK «respira» (fundido de 1.6 s). Va en la cabecera. |
| **Botón primario** | `Boton.Primario` (MaterialButton) | Relleno cian, 48 dp, píldora. **Uno por pantalla.** |
| **Botón secundario** | `Boton.Secundario` | Borde fino y texto cian. Para acciones de apoyo. |
| **Botón de texto** | `Boton.Texto` | Acciones discretas («Reintentar», «Omitir»). |
| **Botón de manejo** | `Boton.Manejo` | La cruceta: 80 dp, esquinas de 16 dp, icono grande. |
| **Botón de PARO** | `Boton.Paro` | Rojo, 56 dp o más, siempre el mismo y siempre con la palabra PARO. |
| **Cabecera** | `view_cabecera.xml` | Fija arriba: punto de conexión + estado, perfil de velocidad (chip) y batería del teléfono. El PARO rojo aparece a la derecha mientras el carro se mueve. |
| **Estado vacío** | `view_estado_vacio.xml` | Icono grande suave, una frase de qué falta y un botón con la acción. Ejemplo: «Todavía no hay rutas guardadas». |
| **Diálogo de confirmación** | `Dialogos.confirmar(...)` | Título corto, una frase y dos botones (la acción y «Cancelar»). Para borrar, limpiar o salir de una ruta. |
| **Aviso en banda** | `AvisoBanda` | `aviso.mostrar(Nivel.ERROR, "Qué pasó", "Qué hacer", "Reintentar") { ... }`. Siempre con las dos frases. |

### Mensajes de error (regla)

Todo error se escribe en dos frases: **qué pasó** («No llega el video de la cámara») y **qué hacer** («Revisa que la cámara esté encendida y toca Reintentar»). Nunca se muestra un código crudo (`-2`, `ETIMEDOUT`...); los códigos se quedan en el registro (logcat) y en `docs/`. Los textos están centralizados en `Mensajes.kt` para que sean coherentes en toda la app.

## 5. Movimiento

- Transiciones de **150 a 250 ms** (`Movimiento.RAPIDO` 150, `NORMAL` 200, `LENTO` 250): fundidos al mostrar y ocultar avisos y cambios de pantalla.
- **Respuesta al tocar:** ondulación (ripple) de Material y, en la cruceta y los botones grandes, un achique del 3 % mientras se mantiene el dedo (`Movimiento.respuestaAlTocar`).
- **Vibración corta** (18 ms) en las acciones importantes (conectar, iniciar o terminar una ruta) y dos pulsos en un obstáculo (`Haptica`); se puede apagar en Ajustes.
- **El indicador de conexión «respira»** (fundido lento entre 100 % y 35 %) solo cuando hay conexión: es la señal de que todo está vivo.
- Nada de rebotes, parpadeos ni animaciones largas.

## 6. Iconos

Un solo conjunto: **Material Symbols** (Google, **Apache 2.0**; texto en `docs/licencias/Material-Symbols-Apache-2.0.txt`), variante *outlined*, grosor 400, 24 dp, descargados como vectores en `res/drawable/ic_*.xml`. Todos se tiñen con el color del texto de su contexto.

## 7. Revisión de cada pantalla (lista obligatoria)

Al terminar cada pantalla se captura con `adb exec-out screencap` (solo desde la terminal, usando la cámara falsa con una imagen de prueba neutra; **nunca** el video real del carro) y se revisa:

1. Una sola cosa principal por pantalla.
2. Alineación a la cuadrícula de 8 dp.
3. Espaciado parejo (nada pegado, nada flotando).
4. Contraste legible (los tokens ya están comprobados; se vigila texto sobre imágenes).
5. Tamaño de toque (56 dp en manejo, 48 dp en el resto).
6. Coherencia con este documento (estilos, no valores sueltos).
7. Textos cortos y claros.
8. Estados vacíos y de error presentes.
9. Modo claro **y** oscuro.
10. Vertical **y** horizontal.

Las capturas se borran al terminar. Las decisiones que salieron de la revisión están en la sección siguiente.

## 8. Decisiones por pantalla

Cada pantalla se capturó en el teléfono (`scripts/qa_captura.py`, con la cámara y el carro falsos) y se revisó con la lista de la sección 7. Lo que salió:

### Armazón (cabecera y barra inferior)
- La cabecera mide **64 dp** (8 × 8) y no cambia de alto: el estado de conexión a la izquierda, el chip de velocidad y la batería a la derecha. El **PARO rojo de la cabecera aparece solo mientras una ruta mueve el carro** (en Manual el PARO está en la cruceta; si apareciera en la cabecera cada vez que se toca un botón, las piezas de la cabecera «saltarían»).
- En horizontal la barra inferior pasa a una **barra lateral** (riel de navegación) para no gastar alto, que es lo que falta en horizontal.
- El aviso global bajo la cabecera solo sale en las pantallas que necesitan el carro (Manual y Rutas) y tarda 5 s en aparecer, para no avisar de un error que en realidad es el primer intento de conexión.

### Inicio
- **Lo grande:** el estado en palabras («Listo para manejar», «Sin conexión»), no un número.
- Las tarjetas del carro y de la cámara llevan un chip de color y dos datos con cifras tabulares. En la tarjeta el chip dice «Reintentando» (corto); la cabecera dice «Reconectando…» (en la tarjeta se cortaba).
- El botón Conectar es el único primario (56 dp), con el icono pegado al texto. Pasa a decir «Volver a conectar» cuando ya hay conexión.

### Manual
- **Lo grande:** el video (esquinas de 16 dp, chips de retraso y cuadros encima). Cruceta de **64 dp** (manejo: 56 dp o más) con el PARO de 72 dp al centro; en una pantalla de 880 dp de alto cabe todo sin desplazar. La palabra PARO usa ancho según el texto (con ancho fijo se cortaba como «PAR»).
- En horizontal: giro a la izquierda (dos botones), video al centro, adelante y atrás con el PARO en medio a la derecha. El video ocupa el alto disponible y la imagen se ajusta dentro sin deformarse.
- El texto del retraso es de **una sola línea** (12 sp): «Rápido. El video llega ~0.3 s tarde: frena antes.».

### Rutas
- **Lo grande:** la cuadrícula (400 dp de ancho como máximo; en horizontal no crece sin límite). Leyenda «1 cuadro = 20 cm» y escala cada 40 cm dentro del borde, para no gastar márgenes.
- Las 4 herramientas (deshacer, rehacer, borrar punto, limpiar) son botones de **solo icono** de 48 dp repartidos a todo el ancho; las plantillas van en una fila que se desliza de lado.
- El **botón primario** es «Ejecutar ruta» (56 dp, uno solo); «Simular», «Guardar» y «Abrir» son secundarios. El resultado de la simulación es una tarjeta con un chip (Llegó / No llegó) y un texto de una o dos frases; los avisos van en ámbar.
- El punto 1 va en **verde** (salida) y los demás en cian; el punto elegido lleva un anillo. Cuando varios puntos caen en la misma celda (el ocho empieza y termina en el centro) se ve el último: es normal.
- El historial muestra las últimas 8 ejecuciones con un chip de color (verde TERMINADO, rojo ERROR, ámbar CANCELADO) y, si no hay ninguna, el componente de estado vacío.

### Ejecutar ruta
- **Lo grande:** el estado en palabras («Listo para empezar», «Avanzando», «Obstáculo: esquivando», «Terminado»), con el nombre de la ruta debajo y el progreso por puntos con su barra.
- El video va **más chico** (con 32 dp de margen a cada lado) para que el botón de iniciar quede a la vista sin desplazar. Mientras la ruta corre, el PARO (72 dp de alto, ancho completo) ocupa el lugar de «Iniciar ruta».
- Los detalles técnicos están **plegados** detrás de «Detalles técnicos»: la pantalla principal no parece una herramienta de depuración.

### Más, Ajustes, Ayuda, Acerca de y Prueba final
- «Más» es una lista de 6 tarjetas de 64 dp o más con icono, título, una frase y flecha. Las pantallas internas llevan una barra de título con flecha de volver y la barra inferior sigue visible.
- **Ajustes:** tarjetas por tema (Red, Velocidad en Manual, Video en Manual, Tema, Vibración) con una frase de ayuda debajo de cada una; las direcciones se validan y el error sale dentro del campo.
- **Ayuda:** 10 preguntas en tarjetas desplegables (200 ms) que dicen qué pasó y qué hacer; «Si algo falla» con 4 pasos numerados; botón para ver otra vez el asistente.
- **Prueba final:** contador grande «0 de 7 hechos» con barra de avance y una tarjeta por paso, con casilla guardada en el teléfono; el paso de la latencia con y sin video tiene el único botón primario («Medir ahora»).

### Visión y diagnóstico y Calibración
- **Visión (horizontal):** el video con esquinas redondeadas y un chip de estado de la cámara ocupa la izquierda; a la derecha las tarjetas (Detector, Guardar cuadro) y una sección **«Avanzado»** plegable (cerrada por defecto) con todos los ajustes finos agrupados por tema y el botón «Restablecer». El título se acorta a «Visión» para que no se corte.
- **Calibración (vertical):** una prueba por pantalla: «Paso X de 9» con barra de progreso, título, ilustración dibujada por código (el carro visto desde arriba, con la regla de cm, el arco de giro, la cámara...), una instrucción de máximo 3 líneas y un panel fijo abajo con la acción del paso, «Atrás» y el PARO. Solo un botón se ve primario a la vez.

### Hallazgos de la revisión (valen para toda la app)
- **Todo ScrollView lleva fondo propio (`@color/fondo`) y `overScrollMode="never"`.** Sin eso, en el Huawei el contenido se veía con un velo gris sobre el fondo (el efecto de borde del desplazamiento).
- **Las alturas de línea se fijan en cada estilo.** El tema Material deja 24 sp de altura de línea y un título de 32 sp en dos renglones se pisaba.
- Contraste revisado también en modo claro (Inicio): textos, chips y botón sin problema.
- Las capturas de pantalla solo se toman si la app está al frente (si el usuario usa otra pantalla del teléfono, el script no captura).
- **El título de una pantalla en horizontal puede quedar sin ancho** (se cortaba «Visión y diagnóstico»): usar un título corto en la barra y el nombre completo en «Más».

## 9. Licencias de lo incluido

| Recurso | Licencia | Dónde |
|---|---|---|
| Inter (tipografía) | SIL Open Font License 1.1 | `docs/licencias/Inter-OFL-1.1.txt` |
| Material Symbols (iconos) | Apache License 2.0 | `docs/licencias/Material-Symbols-Apache-2.0.txt` |
| Material Components para Android | Apache License 2.0 | dependencia de Gradle (`com.google.android.material:material:1.12.0`) |
