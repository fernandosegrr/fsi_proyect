# Subsistema D · App Android (Kotlin)

## D.1 Qué es y para qué sirve

La app es lo que el equipo lleva en la mano: una aplicación Android en Kotlin (`com.fsi.carro`, versión `0.4-<hash corto de git>`). Sirve para conectarse al carro y a la cámara, manejarlo con una cruceta, dibujar y simular rutas, mandarlas ejecutar y vigilar que todo siga vivo. Las cuentas de navegación y visión no las hace Kotlin: las hace el núcleo en C (subsistema E), al que la app llama por JNI, el puente entre Kotlin y C. La visión va en el subsistema F y el orden completo de las capas de seguridad en el G.

Más detalle: capítulo 9 del manual (`docs/manual/capitulos/09_aplicacion.md`); compilación en el capítulo 10 (`docs/manual/capitulos/10_compilacion.md`).

## D.2 Cómo funciona, paso a paso

```
 Pantallas          Sesiones únicas (Kotlin)            Núcleo en C
┌──────────┐      ┌─────────────────────────┐      ┌──────────────────┐
│ Inicio   │      │ SesionCarro             │ JNI  │ navegador.c      │
│ Manual   │◀─────│  ControlLogica          │◀────▶│ obstaculos.c     │
│ Rutas    │avisos│  ModoSemiautonomo       │puente│ ruta_editor.c    │
│ Más      │      │ SesionVideo             │ .c   │ sesion_compartida│
└──────────┘      │  VideoCarro, VisorJpeg  │      └──────────────────┘
                  └──────┬───────────┬──────┘
                         │           │
          ws://192.168.4.1/ws     http://192.168.4.2/stream
          (carro: DevKit)         (cámara: Freenove)
```

1. **Arranque.** `AppCarro` aplica el tema guardado (oscuro por defecto) y prepara la caja negra. `MainActivity` pone una cabecera fija (punto de conexión, estado, chip de velocidad, batería y, mientras una ruta mueve el carro, el PARO) y una barra inferior con cuatro pestañas: **Inicio**, **Manual**, **Rutas** y **Más** (en horizontal es una barra lateral). Desde Más se abren Calibración, Visión y diagnóstico, Prueba final, Ajustes, Ayuda y Acerca de.
2. **Conectar.** El botón «Conectar» de Inicio llama a `SesionCarro.conectarAhora()`. `EnlaceOkHttp` abre `ws://192.168.4.1/ws` (`DIRECCION_CARRO_DEFECTO`, editable en Ajustes). `ControlLogica` manda el perfil de velocidad, `CAPS?` y los balances guardados, y luego un PING cada 800 ms (`PING_MS`). Si la conexión cae, reintenta cada 2 s (`REINTENTO_MS`). La cámara la abre `SesionVideo` aparte: manejar no depende del video.
3. **Manual.** Mientras un botón de la cruceta está apretado, `ControlLogica` repite su comando cada 250 ms (`HEARTBEAT_MS`); al soltarlo manda STOP. El carro frena solo si pasan 1,000 ms sin comandos (`TIEMPO_WATCHDOG_MS`, `firmware_carro/config.h`). La velocidad se elige en Ajustes: «Rápido» (perfil MANUAL, 255) o «Lento» (perfil AUTO, 210). Los interruptores «Video» y «Ver detección» encienden el video y, encima, los números del detector.
4. **Rutas.** El editor es una cuadrícula de 12 x 8 celdas de 20 cm; las cuentas (agregar, mover, deshacer, plantillas, resumen) las hace `nucleo_c/ruta_editor.c`. «Simular» corre el navegador real contra un carro virtual (`simulador_navegador.c`) y anima el recorrido. También hay Guardar, Abrir y un historial de ejecuciones.
5. **Ejecutar ruta, en 3 toques** (`EjecutarRutaFragment`). (1) Elegir la ruta, desde Rutas o desde la tarjeta «Salir rápido» de Inicio. (2) «Iniciar ruta»: comprueba la conexión y que llegue video (hasta 6 s), pide el perfil AUTO y espera su ACK (1 s), y aprende el piso con el carro quieto (~1 s); si el piso se ve muy variado o se descartó más de 5 %, dice «Despeja el frente del carro» en vez de seguir. (3) «Confirmar y salir»: caduca a los 60 s, cuenta 3, 2, 1 y sale.
6. **Durante la ruta.** `ModoSemiautonomo` (en pantalla no hay un modo llamado «autónomo»: se ve Ejecutar ruta) da una vuelta cada 50 ms (`TICK_MS`): toma el último cuadro de la cámara, llama a `NavegadorNativa.tick` y manda al carro el comando que devuelve. La pantalla dice el estado en palabras («Avanzando», «Obstáculo: esquivando», «Terminado») y el progreso por puntos. El PARO rojo ocupa el lugar de «Iniciar ruta».
7. **Si el carro no puede esquivar** pasa a AYUDA en vez de rendirse: se queda quieto, el teléfono vibra y dice «Necesito ayuda: toma el control» con «Causa N: ...». Una cruceta de mantener apretado lo mueve; «Continuar la ruta» (`ayudaContinuar`) lo manda de vuelta a la línea. Si nadie lo atiende en 3 minutos (`NAV_AYUDA_MAX_MS`) termina en ERROR.
8. **Al terminar o cancelar** muestra un resumen (resultado, duración y esquivadas), lo anota en el historial (`AlmacenRutas`) y cierra la caja negra.

### Qué ve el usuario cuando algo falla

Regla de `docs/diseno.md`: todo error dice qué pasó y qué hacer, sin códigos crudos. Los textos están en `Mensajes.kt`; el aviso global bajo la cabecera tarda 5 s en salir para no alarmar con el primer intento.

| Situación | La app dice |
|---|---|
| El teléfono está en otra WiFi | «El teléfono no está en la red del carro»: conéctate a «CarroFSI» y toca Conectar |
| El carro no responde | «El carro no responde»: revisa que esté encendido y cerca |
| Se cae la conexión con una ruta en marcha | «Se perdió la conexión con el carro»: el carro se detuvo y la app intenta volver sola |
| No llega video | «La cámara no responde»: se reintenta cada 3 s y se puede seguir manejando |
| El carro no confirma el perfil AUTO | «El carro no confirmó su velocidad»: puede tener firmware viejo; la ruta no arranca |
| La cámara se calla durante la ruta | «La cámara dejó de mandar imágenes»: la ruta se detuvo por seguridad |

### El núcleo en C por JNI

`app/src/main/cpp/CMakeLists.txt` compila `puente.c` junto con 20 archivos de `nucleo_c/` (`navegador.c`, `obstaculos.c`, `fusion.c`, `sesion_compartida.c`, `ruta_editor.c`...), sin copiarlos, y produce `libcarro_nativo.so` solo para `arm64-v8a`. Cada clase `*Nativa.kt` (por ejemplo `NavegadorNativa`) carga la biblioteca con `System.loadLibrary("carro_nativo")` y declara funciones `external`. `puente.c` tiene 145 funciones JNI con nombres como `Java_com_fsi_carro_NavegadorNativa_tick`: convierten arreglos, llaman al núcleo (`navegador_tick`) y devuelven el resultado, con arreglos de tamaño fijo y sin `malloc`. Hay un solo navegador, una variable estática (`navegador_global`) que usa el hilo principal. Kotlin no decide cuándo girar ni cuánto avanzar: lo decide C (`docs/referencia/app.md`).

### Supervisión de seguridad desde la app

El orden completo entre capas va en el subsistema G. Esto es lo que vigila la app (`ControlLogica`, `ModoSemiautonomo`, `MainActivity`):

| Qué vigila | Límite | Qué hace |
|---|---|---|
| Botón soltado o conexión caída en Manual | Al instante | STOP; si cae la conexión no repite el comando: hay que soltar y apretar otra vez |
| Conexión con una ruta en marcha | Al instante | Cancela la ruta y manda STOP |
| Silencio de los mensajes `SENS` (si el firmware contestó `CAPS`) | 1,500 ms (`SILENCIO_SENS_MS`) | Cancela la ruta |
| Cuadros de la cámara | 1,000 ms (`TIMEOUT_CAMARA_MS`) | Cancela la ruta (el navegador en C ya paró a los 700 ms) |
| Aviso `WD:<n>` (el carro se frenó solo) | Al instante | Cancela la ruta |
| `TOF_PARO:<mm>` que la ruta no esperaba | Al instante | `ControlLogica` deja de repetir FORWARD; si el navegador sigue pidiendo avanzar, cancela |
| Perfil AUTO sin ACK | 1,000 ms (`PERFIL_ESPERA_MS`) | La ruta no arranca |
| App en segundo plano o cambio de pestaña con ruta | Al instante | STOP (`onPause`); con ruta en marcha, antes pregunta |

### Cajas negras en el teléfono

`CajaNegra.kt` guarda, por cada ruta, un archivo de texto en `Android/data/com.fsi.carro/files/cajas_negras/` con la configuración, el estado del navegador en cada tick, cada decisión, lo que mandó el carro y números de la visión. Solo números y palabras: nunca una imagen. Se escribe en un hilo propio, conserva 40 archivos (`MAX_ARCHIVOS`) y tiene un tope de 60,000 líneas por archivo (`MAX_LINEAS`). La carpeta `cajas_negras/` del repositorio está en `.gitignore`: los datos reales no se publican.

## D.3 Cómo lo hicimos

- **Núcleo en C por JNI.** Razones escritas: la misma fuente se compila en PC con GCC para hacer pruebas y simulaciones sin emulador ni carro (`docs/exposicion.md`, P8); es independiente del sistema operativo y sin asignación dinámica descontrolada (`nucleo_c/README.md`); y el `CMakeLists.txt` lo referencia sin copiarlo (`app_android/README.md`). Ese README también habla de «velocidad nativa»; no hay una comparación contra Kotlin (**NO MEDIDO**).
- **Vistas XML y Material, sin Compose.** Lo dice `docs/diseno.md`, y `docs/trabajos/rediseno_1.md` pidió mantenerlo para no cambiar la lógica que funciona. Sobre por qué se eligieron Views y Material en lugar de Compose: no registrado por el equipo.
- **Sesiones únicas.** La cámara atiende un solo cliente, y una pantalla que abriera su propio socket cortaría y reabriría las conexiones al cambiar de pestaña (`docs/manual_video.md`, capítulo 9). Por eso `SesionCarro` y `SesionVideo` son únicas y la decisión de abrir, compartir, cerrar (gracia de 1.5 s) y reintentar (3 s) vive en C (`sesion_compartida.c`), donde se prueba en PC.
- **Seguridad antes que comodidad.** Las rutas usan el perfil AUTO (210): ir más lento acorta la distancia de frenado, unos 8.5 cm contra 15 cm a 255 (cuenta con la calibración, no medida con cinta; `docs/latencia.md`). Cada ruta exige aprender el piso con la luz de ese momento. La app detiene el carro al pasar a segundo plano o al girar el teléfono, a propósito (`MainActivity`).
- **Caja negra.** El registro del sistema (logcat) descarta líneas y se pierde al desconectar el cable; por eso cada ruta deja su archivo (`CajaNegra.kt`).
- **Compilación.** `scripts/compilar_app.ps1`; Java 17, NDK 30.0.16248370 y CMake 4.1.2 (`build.gradle.kts`). El capítulo 10 del manual cita otro NDK, otro CMake y `libcarro_nucleo.so`: manda el código.
- **Revisión de interfaz de v1.2.** Se pidió revisar «parpadeos» (`docs/revision_ui_android.md`). La causa común: las sesiones avisan hasta 20 veces por segundo y cada aviso rehacía la pantalla. Cambió solo cuándo y cuántas veces se redibuja: `RefrescoAcotado` (como mucho 10 refrescos por segundo); chips, avisos, textos y punto de conexión que no hacen nada si no cambian; el aviso de video de Manual dentro del video (la cruceta ya no se mueve); `AvisoBanda` como acordeón; una casilla única en `VisorJpeg`. Además, el chip de velocidad se esconde mientras el PARO ocupa la cabecera y una columna del resumen ya no corta «Terminado». No cambió el navegador, el firmware, las sesiones, `ControlLogica`, `ModoSemiautonomo` ni los tiempos de seguridad. Dos reglas nuevas en `docs/diseno.md`: un layout con variante `layout-land/` se cambia en las dos, con los mismos ids (la primera versión cerró Manual en horizontal); y un aviso que depende de un estado con pasos intermedios se queda hasta que llegue el estado bueno (el del video reaparecía cada ~4 s). Entró a v1.2 con `git cherry-pick -x` (commit `6c849ce`).

## D.4 Cómo lo probamos y con qué resultado

| Qué se probó | Cómo | Resultado | Etiqueta |
|---|---|---|---|
| Pantalla de control | Contra `servidor_falso.py` por USB, Huawei ELS-NX9, 10-oct-2026 (`docs/pruebas_control.md`) | Botón mantenido: FORWARD cada 251 ms de media (15 intervalos, 248 a 254); al soltar llega STOP; PING cada 802 ms; PONG en 7.6 ms (USB, no es el WiFi). En segundo plano manda STOP y cierra el socket | **SIMULADO** (el carro no intervino) |
| Conexión con el carro real | J13, 21-oct-2026, `APP:0.4-adbdd13`, antes de v1.1 (`docs/reporte_J13_fisico.md`) | Sin pérdidas; PING de 6 a 30 ms | **VALIDADO EN EL CARRO** |
| App y firmware de v1.1 enlazados | B0, 10-nov-2026 (`docs/estado_20261110.md`) | `APP:0.4-6d1b213` y `FW:6d1b213-20261110` conectados por WiFi; Sensores muestra `CAPS` con giroscopio y láser listos | **VALIDADO EN EL CARRO** (v1.1) |
| Ejecutar ruta con esquive | B6, 10-nov-2026, v1.1, recta de 200 cm con caja a ~100 cm (`docs/estado_20261110.md`, `docs/pruebas_fisicas_20261110.csv`) | 6 de 6 sin tocar la caja, 5 de 6 completadas. La corrida 5 entró en AYUDA sin causa identificada y, tras «Continuar la ruta», venció el tope de 60 s | **VALIDADO EN EL CARRO** (v1.1) |
| App v1.2 y vigilancias de v3 | `scripts/compilar_app.ps1`: COMPILACION OK (`docs/revision_ui_android.md`) | Compila; nada se ha corrido en el carro | **SIMULADO** |
| Revisión de interfaz: recorrido de pantallas | Huawei ELS-NX9, Android 10, 24-nov-2026, `scripts/ui_telefono/`, carro y cámara falsos, rama de v6 con los mismos archivos que v3 | 24 aperturas (vertical, horizontal, tema claro): 1 cierre con la primera versión (Manual en horizontal), 0 con la final | **SIMULADO** |
| Revisión: dibujado de Manual | `dumpsys gfxinfo`, una corrida de la app anterior y tres de la nueva | Cuadros con una pasada de «medir y acomodar» de más de 4 ms: 19.2 % contra 3.3 % o menos (muestra de ~550). Los «cuadros lentos» varían entre corridas y la batería del teléfono no fue igual | **SIMULADO** |
| Revisión: punto, aviso de video y cruceta | Mismas pruebas, escenas de 20 a 22 s | Punto de conexión: fijo (desviación 0.5) contra respirando (38). Aviso de video: 8 cambios en 22 s contra 0. La cruceta no cambia de lugar | **SIMULADO** |
| Ruta en L con la APK final | Contra el carro falso | «Terminado» en 6 s, punto 10 de 10 | **SIMULADO** |
| v1.2 instalada, interfaz con carro y cámara reales, otros teléfonos, girar el teléfono en Manual, suavidad del acordeón | Sin prueba | Sin medir; la lista de 10 minutos con el carro real está en `docs/revision_ui_android.md`, sección 7 | **PENDIENTE** y **NO MEDIDO** |

## D.5 Límites conocidos

- **Nada de v3 ni de la revisión de interfaz está validado en el carro.** v3 y el firmware `SEG=3` solo están compilados o simulados, y la v1.2 compilada no se ha instalado en ningún teléfono. Lo validado en el carro es v1.1 (`docs/estado_20261110.md`).
- **«Continuar la ruta» no comprueba que la causa ya no esté.** `navegador_ayuda_continuar` borra lo que el láser había confirmado, devuelve los presupuestos de esquivadas (6 por obstáculo, 18 por ruta) y calcula la vuelta a la línea; lo que haya adelante lo vuelven a decir el láser y la cámara. No hay tope global de continuaciones, y con el láser caído (causa 19) vuelve a pedir ayuda (`docs/limites_conocidos.md`). Comportamiento deseado según el equipo: si no llegó al destino, seguir la guía hasta llegar; si ya llegó, no hacer nada. Estado: caso conocido que puede fallar, no corregido.
- **Modo Manual con `RUMBO` (corrección del 26-nov en `ManualFragment.kt`).** Sincroniza `usarGiroscopio` y `usarRumbo` al entrar a la pantalla, activa `rumbo` al avanzar con `FORWARD` y lo apaga al salir. **PENDIENTE** de validar físicamente en el carro.
- **Un teléfono probado** (Huawei ELS-NX9, Android 10); un Huawei con HarmonyOS NEXT no ejecuta APK (`docs/instalar_app_huawei.md`).
- **Un cliente por canal.** El firmware cierra el WebSocket anterior si llega otro, y la cámara atiende un solo video. El WebSocket es `ws://`, sin cifrar.
- **Un video congelado no se avisa antes de ~3 s** (`TIEMPO_LECTURA_MS`); la ruta sí se cancela a los 1,000 ms (N1 de la revisión).
- **Girar el teléfono o mandarlo a segundo plano detiene el carro**, a propósito (N4).
- **Pendientes de interfaz sin decidir.** El aviso global empuja el contenido (575 px, 22 % de la pantalla de prueba; N3) y en Manual horizontal el botón «atrás» se corta 42 px en ese teléfono (N8).
- **La app no detecta sola un cambio de luz** después de aprender el piso (`docs/limites_conocidos.md`).
- **Documentos desactualizados.** `docs/instalar_app_huawei.md` es un esqueleto. El capítulo 9 (botones ±90° en Manual) y `docs/codigo/puente.md` (otra API) se corrigieron contra el código el 25-nov-2026.

## D.6 Términos clave

- **JNI**: mecanismo de Android para que Kotlin llame funciones en C; aquí, `puente.c`.
- **`SesionCarro`**: objeto único que mantiene la conexión con el carro y guarda `ControlLogica` y `ModoSemiautonomo`.
- **`ControlLogica`**: decide qué comando mandar y cuándo (heartbeat, PING, perfiles, reintentos).
- **heartbeat**: comando repetido cada 250 ms mientras se aprieta un botón; sin él el carro frena a los 1,000 ms.
- **perfil MANUAL y AUTO**: velocidades 255 («Rápido») y 210 («Lento»).
- **`ModoSemiautonomo`**: ciclo de 50 ms que alimenta al navegador en C y manda sus comandos.
- **ACK_AUTO**: respuesta con la que el carro confirma el perfil AUTO; sin ella la ruta no arranca.
- **AYUDA**: estado en que el navegador le cede el mando a la persona.
- **PARO**: botón rojo que manda STOP y cancela la ruta (`SesionCarro.pararTodo`).
- **caja negra**: archivo de texto por ruta con números del navegador, del carro y de la visión.
- **`RefrescoAcotado`**: deja redibujar la pantalla como mucho una vez cada 100 ms.

## D.7 Preguntas probables del profesor

1. **¿Por qué usan C dentro de una app Android?** Porque el mismo código se compila en la PC y en el teléfono: así lo probamos con simulaciones de miles de rutas sin emulador ni carro, y la app solo lo llama por JNI (`docs/exposicion.md`, P8). Además no depende del sistema operativo. No medimos si es más rápido que Kotlin.
2. **¿Qué pasa si se pierde el WiFi con el carro avanzando?** El firmware frena solo si pasan 1,000 ms sin comandos, y la app cancela la ruta si se cae la conexión o si calla `SENS` por 1,500 ms; al volver, no repite el comando. El corte de WiFi en marcha no se ha probado en el carro (B9, **NO MEDIDO**): no sabemos cuánto recorre hasta frenar.
3. **¿Qué decide la app y qué decide el carro?** La app no decide cuándo girar ni cuánto avanzar: eso lo hace el navegador en C, que la app alimenta cada 50 ms con la cámara y los sensores. La app manda comandos y vigila; el carro tiene su propio watchdog y su freno por láser (subsistema G).
4. **¿Qué mejoraron en la interfaz y cómo saben que funcionó?** Quitamos redibujados innecesarios: en el teléfono, los cuadros con una pasada de «medir y acomodar» de más de 4 ms bajaron de 19.2 % a 3.3 % o menos. Fue con carro y cámara falsos y un solo teléfono (**SIMULADO**); con el carro real falta probarlo (**PENDIENTE**).
5. **¿Qué guarda la caja negra y se publica?** Solo números y palabras de cada ruta, nunca imágenes. Se queda en el teléfono y la carpeta `cajas_negras/` está en `.gitignore`: no se publica.
