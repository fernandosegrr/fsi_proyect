# Revisión de la interfaz de la app Android: parpadeos y UX (24-nov-2026)

**Estado:** rama `fix/ui-android-parpadeos` (sale de la punta de `feat/autonomo-v6`, `223d5db`; no toca esa rama ni la etiqueta `freeze/v6`). Compila (`scripts/compilar_app.ps1`: BUILD SUCCESSFUL). **Probada en un teléfono el 24-nov-2026** (Huawei ELS-NX9, Android 10) con el carro y la cámara **falsos** (`servidor_falso.py` y `camara_falsa.py` por `adb reverse`; no se encendió ni se movió ningún carro real). El dueño del teléfono pidió instalarla y se instaló con `adb install -r`, que actualiza sin desinstalar (sus datos y permisos quedaron). **No se probó con el carro real ni con la cámara real.** Lo medido está en la sección 6; lo que falta, en la 7.

## 1. Qué se pidió

«Revisa la UI/UX de la app Android: tiene errores en parpadeos y no se siente bien.» No se dijo en qué pantalla ni en qué momento, así que se buscó todo lo que **redibuja o reacomoda la pantalla sin que cambie nada de verdad**.

## 2. Método

- Lectura de `MainActivity`, `Diseno`, `SesionCarro`, `SesionVideo`, `ControlLogica` y `ModoSemiautonomo` (quién avisa y cada cuánto), `VideoCarro` y `VisorJpeg` (el video), las pantallas Inicio, Manual, Ejecutar ruta, Ajustes, Prueba final y Sensores, y los layouts de Manual (vertical y horizontal), cabecera, Inicio y Ejecutar ruta.
- Búsqueda de lo que suele causar parpadeo: `postDelayed`/`Handler` en bucle, `setText` e `invalidate` repetidos, `isChecked =` que dispara su propio oyente, `recreate()`, animaciones que se reinician, `GONE`/`VISIBLE` que empujan el contenido, bitmaps por cuadro, lecturas de disco en el hilo principal.
- La primera versión de esta revisión fue solo lectura de código y compilación. Después se instaló en el teléfono y se midió (sección 6). Esa prueba **encontró un cierre de la app que la lectura no vio** (P9), mostró que el aviso del video seguía parpadeando (P10) y encontró dos textos cortados que ya estaban así (P11 y P12). Lo que no se pudo probar sin el carro real queda dicho en cada fila.

## 3. El dato que explica casi todo: cuántas veces avisa cada cosa

Las pantallas se enteran de los cambios con `SesionCarro.escuchar()` y `SesionVideo.escuchar()`. Cada aviso rehacía la pantalla entera.

| Quién avisa | Cada cuánto | Cuándo |
|---|---|---|
| `ModoSemiautonomo.tick()` termina cada vuelta normal con `alCambiar()` | **50 ms** (20 por segundo) | con una ruta en marcha |
| Mensaje `SENS` del carro | 100 ms | perfil AUTO (Inicio, Rutas, Ejecutar ruta, Sensores); según los comentarios del código, el firmware no lo manda en el perfil rápido de Manual |
| Respuesta `PONG` | 800 ms | siempre que haya conexión |
| `SesionVideo.tick()` llama a `avisar()` aunque no cambie nada | 250 ms | siempre que el video esté abierto |
| El reloj de cada pantalla | 200 a 500 ms | mientras la pantalla se ve |

La cabecera (`actualizarCabecera()`) y la pantalla que se ve se ejecutaban con **cada uno** de esos avisos.

## 4. Hallazgos y arreglos

| # | Dónde | Qué se vería | Causa (en el código) | Arreglo | Estado |
|---|---|---|---|---|---|
| P1 | Cabecera, punto de conexión | Un «tic» seco del punto verde, en vez de respirar despacio. En Manual (perfil rápido, sin SENS) cada ~0.8 s; en AUTO el punto se vería casi fijo | El setter `PuntoConexion.nivel` cancelaba y reiniciaba la animación en **cada** asignación, aunque el nivel fuera el mismo, y la cabecera lo asigna con cada aviso. El fundido de ida dura 1.6 s: con un reinicio cada 0.8 s el brillo llegaba a ≈0.68 y volvía de golpe a 1.0 | El setter ignora un nivel igual al actual | Corregido y **medido** en Inicio con el carro falso (manda sensores 10 veces por segundo, como el perfil AUTO): con la app anterior el punto estaba **fijo**; ahora respira (sección 6). El «tic» cada ~0.8 s de Manual con el carro real (sin sensores) **no se reprodujo** |
| P2 | Manual, aviso de video | Los botones de la cruceta y los interruptores **saltaban hacia abajo** (en vertical) cuando el video fallaba o volvía; en horizontal el **video se encogía** (su alto es lo que sobra) | `manual_aviso` estaba en el flujo, entre el video y los interruptores | El aviso se movió **dentro** de `view_video_manual.xml`, encima de la parte baja del video; ya no empuja nada | Corregido y **medido**. Con la app anterior, si el video fallaba, el aviso de 9 renglones empujaba la cruceta fuera de la pantalla (captura). Con la nueva, los botones de manejo, los interruptores y el video quedan en el mismo lugar con el video bien y con el video fallando, en vertical y en horizontal. La primera versión olvidó la variante horizontal (P9) |
| P3 | Todas las pantallas con aviso en banda (global, Inicio, Manual, Rutas, Ejecutar ruta, Ajustes...) | Al aparecer o irse un aviso, **todo lo de abajo se corría de golpe** | `AvisoBanda` solo hacía un fundido de opacidad: el espacio aparecía o desaparecía en un solo cuadro | `AvisoBanda` aparece y se va como acordeón (el alto crece o baja en 150–200 ms) | Corregido. En el teléfono se ve que el aviso aparece con su alto completo; el tiempo de la animación **no se midió** (no hay grabador de pantalla) |
| P4 | `Movimiento.mostrar()` / `ocultar()` (ya sin uso) | Un aviso que aparecía justo mientras otro se iba podía **quedar oculto**, y reaparecer en el siguiente refresco (un parpadeo) | `ViewPropertyAnimator.withEndAction` también se ejecuta cuando la animación se **cancela**: el final de «ocultar» dejaba la vista en `GONE` justo después de pedir «mostrar» | Se quitaron esas dos funciones; el acordeón usa una bandera para no ejecutar el final de una animación cancelada | Corregido |
| P5 | Ejecutar ruta, durante la ruta | Tirones del video y de los botones | La pantalla se rehacía hasta 20 veces por segundo: `chip.poner()` creaba dos dibujos nuevos y volvía a medir y pintar, `AvisoBanda.mostrar()` reconstruía su fondo, cada `setText` igual redibujaba. Además `textoDetalles()` se armaba en cada refresco aunque «Detalles técnicos» estuviera plegado y llamaba a `CajaNegra.cuantas()`, que **lee la carpeta del disco en el hilo principal** (no se midió cuánto tarda en el teléfono; un acceso a disco ahí puede dar tirones) | `RefrescoAcotado` (como mucho 10 refrescos por segundo; el primero tras un rato sin avisos es inmediato); `ChipEstado.poner()`, `AvisoBanda.mostrar()` y los textos (`ponerSiCambia`) no hacen nada si no cambia nada; los detalles solo se arman con el panel abierto; `CajaNegra.cuantas()` recuerda la cuenta 2 s | Corregido y **medido** en Manual: los cuadros con una pasada de «medir y acomodar» de más de 4 ms bajaron de ~19 % a 3 % o menos (sección 6). Con una ruta en marcha no hay comparación con la app anterior |
| P6 | Cabecera | Trabajo de más en cada aviso | `window.addFlags/clearFlags(FLAG_KEEP_SCREEN_ON)` se pedía a la ventana en cada refresco, y cada llamada hace que Android vuelva a medir toda la ventana | Solo se pide cuando cambia | Corregido |
| P7 | Video (Manual, Ejecutar ruta) | Tras un tropiezo del hilo principal, varios cuadros viejos seguidos «a toda prisa» y más retraso | Cada cuadro decodificado encolaba su propio pintado en el hilo principal | `VisorJpeg` guarda en una casilla única el cuadro más nuevo y pinta solo ese (el hilo principal nunca acumula una fila de cuadros) | Corregido |
| P8 | Inicio, Manual, Ejecutar ruta, cabecera | Consumo | `RedCarro.enRedDelCarro()` hace varias llamadas al sistema y se pedía en cada refresco mientras no hay conexión | Recuerda la respuesta 1 s | Corregido |
| P9 | Manual en horizontal | **La app se cerraba** al abrir Manual de lado (por lo mismo, al girar el teléfono estando en Manual; el giro en sí no se probó) | Error mío en la primera versión de P2: moví `manual_aviso` a `view_video_manual.xml`, pero existe otra versión, `layout-land/view_video_manual.xml`, y no se la puse; `ManualFragment.onViewCreated` busca ese id y fallaba (`findViewById(...) must not be null`) | Se agregó el aviso a la versión horizontal, y un script compara los ids de todos los layouts que tienen variante (`activity_main`, `fragment_manual`, `view_video_manual`: sin diferencias) | Corregido y **medido**: el recorrido de 24 aperturas (10 pantallas en vertical, 10 en horizontal y 4 en tema claro) falló en 1 con la primera versión (Manual en horizontal; el registro de cierres tenía 3 líneas) y abrió las 24 con la final (0 líneas). Lo encontró la prueba, no la lectura |
| P10 | Manual, aviso del video | El aviso «La cámara no responde» aparecía y desaparecía **solo, cada ~4 s**, mientras la cámara siguiera caída (en la app anterior, por su layout, cada cambio además movía todo lo de abajo) | La sesión de video reintenta sola cada 3 s y en cada reintento pasa por «abriendo»; el aviso solo se mostraba en «falló». La app anterior tenía la misma lógica (`git show HEAD:…/ManualFragment.kt`) | `ManualFragment.videoFallando`: el aviso se queda hasta que el video vuelve a fluir. Texto corto (`Mensajes.sinVideoEnManual`; el largo medía 735 px y tapaba casi todo el video, el corto 455 px) y el icono de cámara apagada se esconde con el aviso (se asomaba encima del aviso más corto) | Corregido y **medido** en 22 s con la primera versión de la rama y con la final: cámara rechazada 8 cambios → 0; cámara apagada 2 → 1 (el que queda es la primera aparición). No se midió en la app anterior, que tiene la misma lógica |
| P11 | Ejecutar ruta, resumen al terminar | El chip del resultado salía **cortado** («Terminad») | Tres columnas del mismo ancho: el chip «Terminado» con su punto no cabe en un tercio. **Ya estaba así** | La columna «Resultado» se queda con el ancho de su chip y las otras dos se reparten el resto (`fragment_ejecutar.xml`) | Corregido y **visto** en el teléfono con la APK final (ruta en L contra el carro falso: «Terminado», 6 s, punto 10 de 10) |
| P12 | Cabecera, con una ruta en marcha | La frase de conexión quedaba en «Co…» al aparecer el PARO (era el N2) | La fila no alcanza: punto, frase, chip de velocidad, batería y PARO. **Ya estaba así** | El chip de velocidad (no cambia durante la ruta) se esconde mientras el PARO está en la cabecera (`MainActivity.actualizarCabecera`) | Corregido y **visto** en el teléfono con la APK final: «Conectado» completo, batería y PARO |

Qué **no** cambió en nada: el navegador (`nucleo_c/`), el firmware, la lógica de conexión y de video (`SesionCarro`, `SesionVideo`, `ControlLogica`, `ModoSemiautonomo`), los valores por defecto (la app sigue arrancando como v3 y los interruptores v5/v6 siguen siendo solo de sesión) y los tiempos de seguridad (STOP, watchdog de 1000 ms, `TOF_PARO`). Solo cambia **cuándo y cuántas veces se redibuja**, **dónde está** el aviso del video de Manual (y que su texto es más corto), el ancho de una columna del resumen de la ruta y que el chip de velocidad se esconde mientras el PARO ocupa la cabecera.

«Corregido» sin más quiere decir: cambio hecho, compilado y sin cierres en el recorrido del teléfono. Su efecto **por separado** no se midió: lo que se midió es el conjunto (sección 6).

## 5. Lo que se vio y NO se cambió

| # | Qué | Por qué no se tocó |
|---|---|---|
| N1 | El video congelado no se indica hasta que el fallo se declara. `VideoCarro` da por caída la conexión tras 3 s sin datos (`TIEMPO_LECTURA_MS`) y `hayVideo` usa los cuadros por segundo de los últimos 10 s: con la cámara trabada la última imagen queda fija hasta unos 3 s sin ningún indicio | Atenuar la imagen cuando pasa más de 1 s sin cuadro nuevo sería correcto, pero añade OTRO cambio visual que con una red mala se vería como parpadeo. Conviene decidirlo con el teléfono en la mano. Para el navegador no hay riesgo: una ruta se cancela con más de 500 ms sin cuadros |
| N2 | El PARO de la cabecera aparece y desaparece en la misma fila que el estado, el chip de velocidad y la batería: al aparecer, la frase de conexión se acorta y los chips se corren | Se vio en el teléfono («Co…») y se corrigió escondiendo el chip de velocidad (P12). Un teléfono más estrecho que este podría seguir apretado |
| N3 | El aviso global bajo la cabecera empuja el contenido (ahora suave, antes de golpe) | Ponerlo encima taparía la parte de arriba del editor de rutas, que se usa sin conexión. **Medido** en el teléfono: con el carro apagado el aviso mide 575 px (22 % del alto de la pantalla) y empuja todo lo de abajo 602 px; en Manual hay que desplazar para llegar a la cruceta (que sin carro no sirve). Falta decidir si el texto se acorta |
| N4 | Al girar el teléfono la actividad se recrea y el carro se detiene (`onPause` → `pararTodo`) | Es una medida de seguridad deliberada (comentada en `MainActivity`) |
| N5 | Cambiar el tema llama a `setDefaultNightMode` (que ya recrea la actividad) y además a `recreate()` | Android junta los dos pedidos en un solo reinicio de la actividad (razonado, no medido). Solo ocurre al cambiar el tema |
| N6 | Pantallas de ingeniería (Sensores, Calibración, Calibrar giro, Visión): se refrescan con cada aviso y arman textos largos con `text =` y `append` | Los chips y avisos ya son idempotentes (el arreglo vale para ellas), pero no son pantallas de la exposición; no se tocaron sus 2 600+ líneas sin poder probarlas |
| N7 | El estado de la cámara en Inicio puede cambiar de color si el enlace de la cámara se cae y se reabre (cada 3 s, `SES_REINTENTO_MS`) | Es información real del enlace, no un defecto de dibujo; cambiar los umbrales tocaría el código en C |
| N8 | Manual en horizontal, columna derecha (adelante, PARO, atrás): en este teléfono (1200 px de alto en horizontal) la columna necesita 745 px y caben 703, así que el botón «atrás» se **corta 42 px** abajo (17 % del botón; la flecha y casi todo el botón siguen tocables) | **No es nuevo**: el único cambio de ese layout en esta rama fue quitar el aviso del flujo (`git diff`), y con el video bien estaba oculto. Achicar los botones de manejo o hacer desplazable la columna es una decisión sobre los controles de manejo y no se tomó sin el dueño |

## 6. Lo que se midió en el teléfono (24-nov-2026)

**Cómo se hizo.** Huawei ELS-NX9 (Android 10, pantalla de 1200×2640), por cable USB y con el WiFi de la casa (el teléfono nunca estuvo en la red del carro). La app «anterior» es la que ya estaba instalada (su APK se respaldó antes de tocar nada); la «nueva», la de esta rama. Las dos contra el mismo carro falso (`servidor_falso.py --sensores`) y la misma cámara falsa (imágenes sintéticas), con ventanas de 20 s sin tocar nada. Los programas están en [`scripts/ui_telefono/`](../scripts/ui_telefono/LEEME.md). Se instaló con `adb install -r`: actualiza sin desinstalar, y los ajustes, las rutas guardadas y el permiso de la cámara quedaron como estaban.

**Cuadros dibujados en Manual** (`dumpsys gfxinfo`; una corrida de la app anterior y tres de la nueva, hechas a medida que se corregían cosas: la tercera es la APK final):

| Escena | Corrida | Cuadros lentos | Hilo principal lento (cuadros de ~1200) | Cuadro p90 / p99 (ms) | «Medir y acomodar» > 4 ms (muestra) | Batería |
|---|---|---|---|---|---|---|
| Video bien | App anterior | 28.5 % | 228 | 21 / 28 | 19.2 % | 10 % |
| Video bien | Nueva 1 (primera versión) | 14.4 % | 31 | 11 / 19 | 3.0 % | 14 % |
| Video bien | Nueva 2 (intermedia) | 9.3 % | 30 | 10 / 21 | 3.1 % | 24 % |
| Video bien | Nueva 3 (APK final) | 7.4 % | 34 | 10 / 21 | 3.3 % | 29 % |
| Video fallando | App anterior | 31.2 % | 238 | 24 / 32 | 19.2 % | 11 % |
| Video fallando | Nueva 1 (primera versión) | 10.7 % | 21 | 11 / 25 | 1.6 % | 14 % |
| Video fallando | Nueva 2 (intermedia) | 5.6 % | 2 | 10 / 12 | 0.0 % | 24 % |
| Video fallando | Nueva 3 (APK final) | 3.5 % | 1 | 10 / 11 | 0.0 % | 29 % |

«Cuadros lentos» son los que no llegaron a tiempo a la pantalla; «hilo principal lento», cuántos tardaron en el hilo de la interfaz; p90 y p99, el tiempo de cuadro bajo el cual queda el 90 % y el 99 %. La penúltima columna es una **muestra** de unos 550 cuadros (el teléfono entrega solo 10 por lectura) y cuenta los que incluyeron una pasada de «medir y acomodar» de más de 4 ms.

Lectura: con la app anterior, 19 % de los cuadros muestreados llevaban esa pasada (con el video bien y con el video fallando); con la nueva, 3.3 % o menos en las tres corridas. El ritmo de la app anterior coincide con el de los avisos de las sesiones con el carro falso (unos 10 sensores por segundo y un PONG cada 0.8 s): rehacía la pantalla con cada aviso aunque no hubiera cambiado nada, que es justo lo que evitan `RefrescoAcotado` y los textos y chips que no hacen nada si no cambian. Es coherencia entre lo medido y lo esperado, no una prueba de cada pieza por separado. **Cuidado con los «cuadros lentos»:** varían entre corridas de la misma app (la nueva con el video bien: 14.4 %, 9.3 % y 7.4 %) y la app anterior se midió con 10 a 12 % de batería y las nuevas con 14 a 30 % (el teléfono se cargaba), así que parte de esa diferencia puede ser del teléfono y no de la app. Lo que menos depende de eso es la cuenta de pasadas de «medir y acomodar», el punto de conexión y que la cruceta no se mueva.

**El punto de conexión** (en Inicio, con el carro falso que manda sensores 10 veces por segundo, como el perfil AUTO; brillo del punto en 30 capturas). Con la app anterior estaba **fijo**: de 182 a 184 sobre 255 (desviación 0.5), porque la animación se reiniciaba con cada aviso y nunca bajaba. Con la nueva **respira**: de 77 a 182 (desviación 38 en la APK final; entre 37 y 38 en las tres corridas). Es un cambio que se nota. Si se prefiere el punto fijo, es una línea en `PuntoConexion.actualizarAnimacion()`.

**La cruceta no se mueve.** Con el video fallando, la app anterior abría el aviso entre el video y los interruptores (9 renglones) y **la cruceta salía de la pantalla**: en la captura solo se asoma el borde de arriba, pegado a la barra de navegación. De esa corrida no quedó la posición medida (el volcado de la jerarquía de vistas salió vacío con la app saturada); sí la captura. Con la nueva, la posición de los 5 botones de manejo, los 2 interruptores y el video es **idéntica** con el video bien y con el video fallando (jerarquía de vistas, `dumpsys activity top`), en vertical y en horizontal.

**El aviso del video.** Con la cámara caída, el aviso aparecía y desaparecía solo (cámara rechazada: 8 cambios en 22 s; cámara apagada: 2). Ahora se queda: 0 y 1. Con el texto corto mide 455 px en vertical (el largo medía 735 y tapaba casi todo el video) y en horizontal queda encima del video sin mover los botones de manejo.

**Un cierre de la app, encontrado y corregido (P9).** La primera versión de esta rama hacía que **Manual se cerrara al abrirse en horizontal**. No lo vio la lectura del código: lo encontró el recorrido de pantallas del teléfono. Con la versión final, las 24 aperturas del recorrido abren y el registro de cierres (`logcat -b crash`) queda vacío; también durante la ruta y las demás pruebas.

**La ruta.** Una «Ruta en L» corrió completa contra el carro falso con la APK final: «Avanzando», punto 10 de 10, «Terminado» en 6 s, con la cabecera «Conectado», la batería y el PARO mientras corría, y el chip «Terminado» completo en el resumen (P11 y P12). No hay comparación de tiempos con la app anterior: en su corrida solo se llegó a la pantalla «Listo para salir». Además, la comprobación previa de la ruta se negó varias veces («Hay algo frente al carro») cuando la «caja» de la cámara falsa estaba a la vista: es la app haciendo su trabajo, pero hace que esa escena de la prueba dependa del momento (ver el `LEEME` de `scripts/ui_telefono/`).

**Lo que se vio a ojo** en las capturas de las 24 aperturas: lo de P11, P12 y N8; lo demás sin textos montados ni cortados a simple vista. No es una auditoría de cada píxel.

**Lo que NO se midió:** nada con el carro real ni con su cámara (el WiFi y el video de la placa pueden comportarse distinto); la ruta con la app anterior; que la animación del acordeón sea suave (el teléfono no trae grabador de pantalla: solo hay capturas cada segundo); girar el teléfono estando en Manual (solo se abrió ya en horizontal); otros teléfonos y tamaños de pantalla; una segunda corrida de la app anterior (hay una sola, con la batería baja).

## 7. Lo que falta comprobar con el carro real (10 minutos)

Con el carro encendido y la cámara conectada como siempre:

1. **Manual, apaga la cámara (o aléjala):** el aviso «La cámara no responde» sale **sobre la parte baja del video** y se queda mientras la cámara siga caída; la cruceta y los interruptores no se mueven. Al volver la cámara, el aviso se va sin saltos.
2. **Manual con el carro real y el punto de la cabecera:** debe respirar despacio. Según los comentarios del código, con el perfil rápido el firmware no manda sensores: es el caso del «tic» cada ~0.8 s de P1, que con el carro falso no se reprodujo.
3. **Apaga el carro y espera ~5 s en Manual o Rutas:** el aviso global debe bajar y subir **suave**, no de golpe.
4. **Ejecutar una ruta corta** (la de siempre, con el carro en el piso): el video no debe entrecortarse más que sin ruta, y la cabecera dice «Conectado» junto al PARO. Abrir y cerrar «Detalles técnicos»: se llena al abrir.
5. **Girar el teléfono en Manual** (con el carro parado): abre y no cambia nada de lugar; el carro se detiene al girar (N4, es a propósito).
6. Si algo se ve **peor**, anotar pantalla y momento y volver a `feat/autonomo-v6` (`git switch feat/autonomo-v6`, recompilar con `scripts/compilar_app.ps1` y `scripts/instalar_app_adb.py`). Un aviso que se queda a medio alto, o que no aparece, sería un defecto del acordeón (`AvisoBanda`): es lo primero que hay que mirar.

## 8. Riesgos y cómo volver atrás

- **El acordeón (`AvisoBanda`) ya se vio funcionar en el teléfono** (el aviso aparece con su alto completo), pero solo contra el carro y la cámara falsos y sin medir su animación. Si diera problemas, el arreglo de P1, P5 a P8 es independiente y se puede conservar.
- Los archivos tocados son **idénticos** entre `feat/autonomo-v3` y `feat/autonomo-v6` (se comprobó con `git diff --numstat`), así que estos commits se pueden pasar con `git cherry-pick` a cualquiera de las dos líneas si se quiere la APK de la exposición con estas mejoras. Esta revisión **no** fusionó nada ni subió nada a `main`.
- **Una regla nueva** (`docs/diseno.md`, regla 6): un layout con variante `layout-land/` se cambia en las dos, con los mismos ids. Se comprobó con un script de ids sobre `activity_main`, `fragment_manual` y `view_video_manual` (sin diferencias).
- Pruebas automáticas: `scripts/ui_telefono/` (necesita un teléfono). La lógica de `RefrescoAcotado` es corta (un periodo y un refresco pendiente) y se revisó leyéndola; no se escribió una prueba unitaria porque requeriría agregar dependencias de prueba al proyecto.

## 9. Archivos tocados

`RefrescoAcotado.kt` (nuevo), `Diseno.kt` (`ponerSiCambia`, `ChipEstado`, `PuntoConexion`, `AvisoBanda`, se quitan `Movimiento.mostrar/ocultar`), `MainActivity.kt` (también esconde el chip de velocidad mientras el PARO está en la cabecera), `InicioFragment.kt`, `ManualFragment.kt` (`videoFallando`, aviso corto), `EjecutarRutaFragment.kt`, `VisorJpeg.kt`, `Mensajes.kt` (`RedCarro`, `sinVideoEnManual`), `CajaNegra.kt` (`cuantas`), `res/layout/view_video_manual.xml`, `res/layout-land/view_video_manual.xml`, `res/layout/fragment_manual.xml`, `res/layout-land/fragment_manual.xml`, `res/layout/fragment_ejecutar.xml` (columna «Resultado»), `scripts/ui_telefono/` (nuevo) y estos documentos (`docs/diseno.md`, este archivo y el `README.md`).
