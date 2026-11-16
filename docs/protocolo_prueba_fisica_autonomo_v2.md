# Protocolo de prueba física — autónomo v2 y v3

Rama `feat/autonomo-v3` (parte de `feat/autonomo-v2`; el bloque M de la sección 10 y las notas de `SEG=3` son de la v3). **Nada de esto se ha ejecutado.** Todo lo que la rama afirma está SIMULADO ([diseño](autonomo_v2_diseno.md)); este protocolo es para convertirlo en VALIDADO EN EL CARRO o en un hallazgo. Va de menor a mayor riesgo: **no pases a un bloque si el anterior no cumplió su criterio.**

Hoja de registro: [pruebas_fisicas_plantilla.csv](pruebas_fisicas_plantilla.csv). Una fila por corrida.

## 0. Reglas de la sesión

- Una mano cerca del carro en todas las corridas. Si va a tocar algo, levántalo o pulsa STOP.
- **Abortar la sesión** (no solo la corrida) si: el carro avanza sin que la app lo mande; no frena con STOP; gira más de una vuelta; el láser no frena a ~12 cm (~15 cm con `SEG=3`) de una pared en la prueba 1.3; la app se cierra dos veces.
- No se desinstala la app ni se borran sus datos. No se usa `erase_flash`. Una placa por USB a la vez. La PC no se une a la WiFi `CarroFSI`.
- No se guardan imágenes de la cámara del carro. Las fotos del resultado se toman con otro teléfono, desde arriba.

## 1. Preparación

| Paso | Qué | Criterio |
|---|---|---|
| 1.1 | 4 pilas AA nuevas en el carro; powerbanks cargadas; teléfono con más de 50 % | Anotar el voltaje de las pilas si hay multímetro |
| 1.2 | Instalar la APK conservando datos (sección 6) | La app abre y conserva rutas y calibración |
| 1.3 | Pantalla Sensores: giroscopio listo, láser vivo. Carro en perfil AUTO frente a una pared: avanzar | Frena solo a ~12 cm con el firmware actual (`TOF_PARO` 120 mm) o ~15 cm con `SEG=3` (150 mm). **Si no frena: abortar la sesión** |
| 1.4 | Piso despejado, luz estable (anotar: ventana, focos, hora). «Aprender piso» | Aprendizaje aceptado. Si lo rechaza por «muy variado», cambiar de lugar |
| 1.5 | Visión: umbral 40, celdas 7, fila mínima 44 %, detector de objetos encendido, autoridad del láser encendida | Anotar los valores reales en la hoja |

Con el firmware actual del carro (v1.1) todo el protocolo es válido. Los bloques marcados **[SEG=2]**, **[SEG=3]** y **[cam-v2]** solo aplican si autorizaste cargar el firmware nuevo (sección 7).

## 2. Bloque A — sin obstáculos (riesgo bajo)

| Prueba | Ruta | Repeticiones | Aceptación | Abortar si |
|---|---|---|---|---|
| A1 | Recta de 150 cm, piso limpio | 3 | Llega; sin paradas; error lateral ≤ 5 cm | Se detiene o esquiva sin nada delante 2 de 3 |
| A2 | Recta de 150 cm con una pared a 60 cm más allá del destino | 3 | Llega y dice Terminado sin «esquivar» la pared | Intenta rodear la pared |

## 3. Bloque B — un obstáculo (riesgo medio)

Obstáculo centrado en la línea, a 80–100 cm de la salida, con 1 m libre a cada lado y 1.5 m libres después.

| Prueba | Obstáculo | Repeticiones | Aceptación | Abortar si |
|---|---|---|---|---|
| B1 | Caja de 20 × 20 cm, recta de 150 cm | 5 | 5 de 5 sin tocar; llega; error lateral ≤ 8 cm | Toca la caja 2 veces |
| B2 | La misma caja corrida 15 cm a un lado | 3 | 3 de 3 sin tocar. Anotar si hizo el vistazo (tuerce ~12° y vuelve) | Toca la caja 2 veces |
| B3 | Banquito 35 × 45 cm con patas | 3 | Sin tocar una pata. Anotar: rodeó, pasó por debajo o pidió ayuda | Toca una pata 2 veces |
| B4 | Objeto largo (90 cm a lo largo de la ruta), recta de 250 cm | 3 | Sin tocar; no vuelve a la línea junto al objeto | Gira hacia el objeto estando a su lado |
| B5 | Pared o mueble que cierra el paso | 2 | Mide un lado y, si la vuelta cabe, el otro; pide ayuda con «Causa 16» sin acercarse | Avanza de lado hacia la pared |
| B6 | Caja de 20 × 20 cm con una pared a 30–40 cm de la línea de UN lado; del otro, 1 m libre. **Mano en STOP** | 3 | Rodea por el lado libre, o deja de girar y pide ayuda con «Causa 18» o «Causa 16». Anotar hacia qué lado giró primero | Toca la pared, o sigue girando contra ella |

En B6, lo que se quiere saber es si la cámara real ve la pared (y entonces el carro elige el otro lado) y, si no la ve, con cuánta anticipación lo detiene el láser: anota a qué distancia de la pared quedó la esquina del carro.

En B1, mirar tres cosas nuevas: (a) el carro se aparta, pasa y **avanza bastante más allá de la caja** (unos 60 cm) antes de volver; (b) antes de entrar a la línea **se queda mirando medio segundo**; (c) si retrocede, es solo por donde vino.

## 4. Bloque C — falsos positivos provocados (riesgo bajo)

Recta de 150 cm sin obstáculos.

| Prueba | Qué | Repeticiones | Aceptación |
|---|---|---|---|
| C1 | Tu sombra cruzando la ruta a 50 cm del carro | 3 | No esquiva ni pide ayuda. Anotar si se detiene un momento |
| C2 | Mancha de sol o linterna sobre el piso, a un lado de la línea | 3 | Igual. Anotar si hace el vistazo |
| C3 | Mancha de sol o linterna sobre la línea | 3 | Igual |
| C4 | «Aprender piso» con la luz encendida y correr con la luz apagada (o al revés) | 2 | Anotar qué pasa. **Esta es la prueba que más informa**: de aquí sale si hace falta fijar la exposición |

Después de cada corrida de este bloque, **guarda la caja negra** (sección 8): sus líneas `VTEL` dicen, sin imágenes, si el histograma entero se corrió (exposición), si subieron unas pocas celdas (mancha) o si la cámara cambió sus medias de color (`CAB camara_stats`, con cam-v2).

## 5. Bloque D — fallas y rutas con giros (riesgo medio–alto)

| Prueba | Qué | Aceptación | Abortar si |
|---|---|---|---|
| D1 | Recta de 200 cm: a media ruta, apagar la WiFi del teléfono | El carro se detiene en ~1 s (watchdog) | Sigue avanzando más de 2 s |
| D2 | Recta de 200 cm: tapar la cámara con la mano 2 s | Se detiene por pérdida de video y lo dice | Sigue a ciegas |
| D3 | Recta con el detector de objetos apagado en Visión | Al iniciar avisa «Percepción incompleta» | — |
| D4 | Ruta en L (150 + 100 cm), sin obstáculos | Llega; error ≤ 10 cm | — |
| D5 | Cuadrado de 100 cm, sin obstáculos | Llega; error ≤ 15 cm | — |
| D6 | Dos cajas seguidas a 80 cm una de otra, recta de 300 cm | Sin tocar ninguna | Toca una caja 2 veces |
| D7 **[SEG=2/3]** | Con el carro frente a una pared a 10 cm, mandar adelante desde la cruceta en AUTO | Los motores no dan ni un pulso | Avanza |
| D8 | Ruta en L (150 + 100 cm) con una caja de 20 × 20 cm a 50 cm de la esquina | Rodea la caja y toma el tramo siguiente; no sigue de frente más allá de la esquina | Sigue recto más de 50 cm pasada la esquina |

## 6. Compilar e instalar la APK conservando datos

No ejecutado por el asistente. En PowerShell, desde la raíz del repositorio:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/compilar_app.ps1
```

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r app_android/app/build/outputs/apk/debug/app-debug.apk
```

Si `install -r` falla por firma distinta (`INSTALL_FAILED_UPDATE_INCOMPATIBLE`): **detente. No desinstales** (se pierden rutas, calibración y el piso). Avisa y se resuelve con la misma llave de depuración de la instalación anterior.

## 7. Firmware nuevo (solo con tu autorización)

No ejecutado por el asistente. Ninguno es obligatorio para probar la rama.

- **Carro `SEG=3`** (incluye todo `SEG=2`): cierra el fallo de los comandos desconocidos, bloquea `FORWARD` contra un obstáculo, sube el freno láser a 150 mm y el bloqueo de arranque a 180 mm (antes 120 y 150) y deja de romper la racha de «cerca» con un solo «sin eco». Riesgo: bajo (pruebas en PC, compilado); es el recomendado. **Antes de cargarlo, haz las mediciones M5 y M6 con el firmware actual** para tener la base de comparación. Para volver a 120/150 sin cargar el tag: compilar con `-DTOF_PARO_MM=120 -DTOF_BLOQUEO_MM=150`.
- **Cámara `cam-v2`**: añade números de luz a `/stats` y apaga la autoprueba. Riesgo: bajo, pero no se pudo probar con el sensor real.

Procedimiento propuesto, una placa por USB a la vez, sin `erase_flash`:

```powershell
.\scripts\cargar.ps1 nuevo -SoloCompilar
```

```powershell
.\scripts\cargar.ps1 nuevo -Puerto COM6
```

```powershell
.\scripts\cargar.ps1 camara -Puerto COM8
```

Después de cargar el carro: pantalla Sensores → la línea CAPS debe traer `SEG=3,TUSO=1,TOFP=150`. Sin teléfono, desde la PC: `python scripts/verificar_version.py --puerto COM6 --solo-fw`. Repetir 1.3 antes de cualquier ruta. Para volver atrás: cargar el firmware del tag `v1.1`.

## 8. Qué registrar

**A mano, en la hoja:** distancia final al destino con cinta (a lo largo y de lado, en cm), contactos (sí/no y con qué), si pidió ayuda y el número de causa, duración, y una foto del resultado desde arriba con otro teléfono.

**Lo saca la caja negra** (un archivo por corrida; anota su nombre en la hoja):

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" pull /sdcard/Android/data/com.fsi.carro/files/cajas_negras cajas_negras
```

- cabecera con la configuración real con la que salió la ruta;
- por tick: estado, comando, pose estimada, láser, fuentes por zona, vistazos, causa;
- por cuadro: histograma de distancias al piso, brillo, celdas por zona y fila;
- cada decisión del navegador: ancho medido en el barrido, miradas, pasos extra, retrocesos;
- `TOF_PARO`, watchdog y el resultado final.

El nombre y la carpeta también aparecen en Ejecutar ruta → Avanzado.

## 9. Criterio global

- **Se puede seguir hacia v1.2** si: bloque A y B1 completos sin contacto, C1–C3 sin esquives ni ayudas, D1 y D2 detienen.
- **Vuelve al escritorio** si: algún contacto en B1; el carro gira hacia un objeto que tiene al lado; en B6 toca la pared; un falso positivo hace esquivar; la caja negra queda vacía.
- Banquito, silla, objeto bajo y la caja con pared a un lado son los casos donde el simulador ya dice que no se llega al 99 %: lo que pase ahí es información, no un fracaso de la sesión.
- **Tres cosas a vigilar que señaló la revisión independiente**: (a) si el carro hace un vistazo con el objeto ya a menos de ~40 cm y se le acerca al torcer; (b) cuánto retrocede cada vez (nunca más de 25 cm tras un paro, ni más de lo que avanzó en la diagonal) y que detrás no haya nada; (c) si junto a una pared termina más allá del destino dibujado.

## 10. Bloque M — mediciones baratas (v3)

Cada una cierra un NO MEDIDO que el simulador no puede cerrar. Ninguna necesita rutas largas; casi todas se hacen con el carro quieto o a baja velocidad. Se registran en [pruebas_fisicas_mediciones_v3_plantilla.csv](pruebas_fisicas_mediciones_v3_plantilla.csv) (una fila por lectura o repetición). Mismas reglas de la sección 0. **No hacen falta imágenes de la cámara.**

| Medición | Qué se mide y para qué | Cómo (resumen) | Qué anotar |
|---|---|---|---|
| M1 | **Deriva del giroscopio en reposo** (sesgo residual °/s). Cierra el supuesto «dentro de la zona muerta no deriva» y decide cuánto confiar en el ángulo en una ruta de 60 s | Carro quieto sobre la mesa, motores apagados, giroscopio LISTO. Leer el ángulo en Sensores a 0, 1, 2, 5 y 10 min con cronómetro. Repetir tras 30 s de recta en AUTO (sesgo «en movimiento») | Ángulo y minuto; temperatura aproximada del cuarto; pendiente °/s |
| M2 | **¿El láser ve patas reales?** Cierra la duda mayor del banquito y la silla | Carro quieto apuntando a una pata del banquito, luego a la silla, a 20, 30, 40, 50, 60 y 80 cm del frente del carro (cinta). Con la pata al centro y corrida 5 y 10 cm. 20 lecturas por posición desde Sensores | Mediana de mm, cuántas de 20 dicen «sin eco» o 1200, si el valor se va hacia la pared del fondo |
| M3 | **Láser contra una pared de lado** | Carro quieto; pared a 30 cm de la línea de lado; girar el carro a mano a 0, 20, 40 y 60° respecto de la pared (motores apagados). 20 lecturas por ángulo | mm y fracción de lecturas libres por ángulo |
| M4 | **Deriva de la exposición** (luz contra sombra) | Con `cam-v2`: caja negra con `CAB camara_stats` y líneas `VTEL`; carro quieto sobre el mismo piso con la luz del cuarto, luego en sombra y luego con una mancha de sol o linterna. Prueba del cartón sobre el cuadro 3 s | Brillo medio, medias de color y histograma que reporta la telemetría, por condición |
| M5 | **Distancia de frenado real por láser** y distancia sensor–frente (`e`) | AUTO hacia una pared desde 80 cm, 10 repeticiones, **mano en STOP**. Con cinta: distancia de la nariz del carro a la pared al quedar parado. Con el aviso `TOF_PARO:<mm>`: distancia que dijo el sensor al frenar | Nariz–pared (cm), `TOF_PARO` (mm), `e` = (mm del sensor − nariz–pared); si alguna vez toca: abortar M5 |
| M6 | **Pulso de arranque contra un obstáculo cercano** (la duda de TOF_BLOQUEO) | Pared a 20, 18 y 16 cm de la nariz, AUTO, mandar FORWARD desde la cruceta. Con `SEG=2/3` no arranca; con el firmware actual dará un pulso de 250 ms | Si arrancó, cuánto avanzó (cm) y si tocó |
| M7 | **Tiempo y ángulo de un giro corto** (también la hipótesis de NAV_GIRO_TIEMPO_MIN_GRADOS = 10) | GIRAR 4°, 10°, 20° y 45° a cada lado, 5 repeticiones, con el giroscopio encendido; luego con el giroscopio apagado en la app para los de 4° y 10°. Ángulo real con transportador pegado al piso | Ángulo mandado, ángulo informado por el firmware, ángulo real, ms hasta quedar quieto |
| M8 | **Velocidad real en AUTO y MANUAL** (¿19.3 o ~30 cm/s?) | Recta de 150 cm con cinta, 5 repeticiones por perfil, cronómetro del teléfono desde que arranca hasta que la nariz cruza la meta | Segundos y cm/s |
| M9 | **Latencia de extremo a extremo** y su cola | Si la app muestra el tiempo de PING, anotar mínimo, mediana y máximo en 60 s con el carro quieto y en 60 s con la ruta corriendo. Si no lo muestra: NO MEDIDO | ms |
| M10 | **Ritmo del láser** | Contar las líneas SENS de la caja negra en 30 s (esperado: 10 por segundo por diseño) y revisar el contador de lecturas del láser si Sensores lo muestra | lecturas por segundo |

Criterios de lectura (no de aprobación): M2 dice si el láser ve patas; si la mediana se va al fondo o más de la mitad es «sin eco», la hipótesis del simulador de «grupo más cercano» está equivocada y las cifras del banquito y la silla se deben leer con la sensibilidad `AV3_LASER_MEZCLA=1` del banco. M5 con `e` ≥ 2 cm confirma que 120 mm no alcanzaba para un arranque (H-01). M7 con 4° dando más de 10° obliga a dejar `NAV_GIRO_TIEMPO_MIN_GRADOS` donde está o subirlo.

**Orden:** M8, M1, M5 y M6 (con el firmware actual), luego M2, M3, M4, M7, M9, M10. M5 y M6 son las únicas con movimiento hacia una pared: mano en STOP y velocidad AUTO.
