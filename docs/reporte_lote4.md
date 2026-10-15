# Reporte del lote 4 (trabajos A, B, C, E, F, G, D, I y H)

Fecha: 14 oct 2026. Orden de ejecución pedido: A, B, C, E, F, G, D, I, H. Reglas: `docs/trabajos/reglas.md`. Los mensajes del lote están en `docs/trabajos/lote4_1.md` a `lote4_6.md` (el lote dice «de 7» pero solo llegaron 6 mensajes de trabajos; el séptimo fue el «EMPIEZA»).

## 1. Resumen en una tabla

| Trabajo | Qué es | Estado | Pruebas en PC | Hardware |
|---|---|---|---|---|
| A | Perfiles de velocidad MANUAL (255) y AUTO (210) con `MODO_MANUAL`/`MODO_AUTO` y confirmación | Hecho, cargado en el DevKit | 87 casos del firmware + 14 del servidor falso | **Probado**: tú confirmaste MANUAL a velocidad normal y AUTO más lento |
| B | Corredor de trayectoria (huella del carro en la imagen), apagado por defecto | Hecho | `pruebas_corredor` 59 | Pendiente (medir ancho, altura, inclinación y campo de visión) |
| C | Esquive de objetos grandes sin rendirse | Hecho (ya existía en gran parte) | `pruebas_navegador` 143 | **Probado**: «siempre que ve algo lo esquiva», con una ruta de reincorporación rara |
| E | Pisos negros y de 2 colores | Hecho | `pruebas_pisos_dificiles` 24, obstáculos 146, piso 58 | Pendiente |
| F | Modo «cualquier objeto» (modelo sin clase) | Hecho | `pruebas_cualquiera` 64 | Pendiente (pasadas con termo, caja y mochila) |
| G | Flujo del piso | Hecho, apagado por defecto | `pruebas_movimiento_piso` 54 | Pendiente |
| D | Latencia del video | Hecho en código; medidas de la cámara pendientes | — | Medida de `frame2jpg`: 54.2 ms en HQVGA; comparación de compresores pendiente |
| I | Modos de video raw y celdas por UDP | Hecho | `pruebas_celdas` 40 | Probado en el teléfono con una cámara falsa; comparación real pendiente (9 pasadas) |
| H | Comparativa de detectores (investigación) | Hecho, en la rama `investigacion/comparativa-detectores` | — | Mediciones en el teléfono; protocolo con objetos pendiente |

Total de pruebas en PC: 14 programas de `nucleo_c` (132, 28, 146, 58, 72, 66, 143, 14, 16, 59, 24, 64, 54 y 40 casos) y 87 del firmware: todos sin fallos.

## 2. Detalle por trabajo

### A — Perfiles de velocidad
- `firmware_carro`: tabla de perfiles (MANUAL 252/255, AUTO 207/210; arranque a plena potencia de 250 ms y luego velocidad de crucero), comandos `MODO_MANUAL`/`MODO_AUTO` con `ACK_MANUAL`/`ACK_AUTO`. La app espera hasta 1 s el ACK del perfil AUTO antes de una ruta.
- Calibración leída del teléfono a 210: **51.7 ms por cm (19.3 cm/s)**, frenado **1.4 cm**; a 255 eran 26.3 ms/cm y 0.9 cm. Centímetros hasta quedar detenido: 8.5 (367 ms), 9.0, 11.6 y 13.1 según el caso (`docs/latencia.md`).
- Documento: `docs/perfiles_velocidad.md`.

### B — Corredor de trayectoria
- `nucleo_c/corredor.c`: la huella del carro proyectada con perspectiva (altura de la cámara, inclinación, campo de visión, ancho del carro y margen); un obstáculo cuenta si está DENTRO o en el BORDE; si está FUERA solo avisa. Apagado por defecto; botón y campo de medidas en la pantalla de visión.

### C — Esquive de objetos grandes
- Revisión del navegador: por qué terminaba en ERROR con un objeto grande tras girar. Ahora hay reintentos (giro extra de 45° y luego 90°, máximo 2), retroceso ciego de hasta 15 cm por tiempo, calibración del retroceso (`ms_por_cm_atras`; sin calibrar usa el del avance con aviso), elección del lado con menos celdas de obstáculo y valores por defecto validados (fila mínima 440, celdas mínimas 7, umbral de color 40).
- Documentos: `docs/navegador.md` (sección «Objetos grandes»), `docs/obstaculos.md` («Valores por defecto validados»).
- **Prueba en el carro (tuya):** el esquive funciona y siempre que ve algo lo esquiva. La ruta de reincorporación se ve rara (log del 22:22:53): giro izquierda, mira, avanza de lado ~2 s, giro derecha y 8 s de correcciones cortas RIGHT/FORWARD/LEFT hasta terminar sin errores.

### E — Pisos negros y de 2 colores
- Reproducido en el PC con cuadros sintéticos. Mejoras: regla de brillo oscuro, franja de referencia ancha con apoyo mínimo, reajuste de brillo con guardas de consistencia, `piso_fijo` en rutas, botón **Aprender piso** y registro `PISO_DIAG`.
- Hallazgo honesto: el ruido por sí solo no reprodujo los falsos obstáculos en piso negro; el reflejo de luz sí (corregido).

### F — Modo «cualquier objeto»
- `nucleo_c/cualquiera.c`: filtros de caja (confianza mayor al umbral, área hasta 60 %, borde inferior en la mitad de abajo, tamaño mínimo), persistencia en 2 cuadros del modelo, y una regla de fusión donde **el color manda** (solo avisa si el color no ve nada; desmiente solo un falso positivo de color de cercanía 1). Registro `OBJ_ANY` con 6 umbrales y `scripts/analizar_obj_any.py`.
- Benchmark (60 s por configuración, enchufado): EfficientDet-Lite0 en CPU 1 hilo 43→52 ms, GPU 139 ms, NNAPI 61 ms; Lite1 79→96 / 219 / 104; Lite2 120→147 / 234 / 155. En CPU los tres modelos se degradan ~20 % en 60 s. Lite1 y Lite2 no están en el repositorio (script con hashes). Detalle en `docs/modelo_objetos.md`.

### G — Flujo del piso
- `nucleo_c/movimiento_piso.c`: bloques de 16x16 contra el desplazamiento esperado del piso (geometría de la cámara + velocidad AUTO de 193 mm/s), entrada opcional en la fusión (solo baja un falso positivo de color lejano; nunca frena), registro `FLUJO_DIAG`. Probado con un trazador de rayos.
- Límites (de la geometría): distingue una caja con textura solo a unos 30 cm o menos y no ve cajas de un solo color. Es un apoyo, no un detector de lejos (`docs/flujo_piso.md`).

### D — Latencia del video
- Registro `JPEG_MS`; comparación al arrancar de `frame2jpg` contra JPEGENC (Apache-2.0) en 4 variantes; `TCP_NODELAY` en el WebSocket del carro; cadena del video en 3 hilos con cola de 1 y `MODO_STATS`. `frame2jpg` sigue siendo el predeterminado hasta medir (regla de los 5 ms).
- Medido con la cámara cargada: compresión `frame2jpg` en HQVGA **54.2 ms**, JPEG de 2386 B, 12.4 fps, edad 135 ms.
- Tabla antes/después y pasos para el resto: `docs/latencia.md`.

### I — Modos de video raw y celdas
- Cámara: `/control?modo=jpeg|raw|celdas`, `/raw` con encabezado por cuadro, UDP 4210 con `HOLA`/`PING`, y el mismo código C del detector compilado en el firmware. App: botón de modo, vuelta automática a `jpeg` si un modo falla y `MODO_STATS`. La entrada por celdas da exactamente el mismo resultado que la imagen completa.
- Probado en el teléfono con una cámara falsa: jpeg 9.3 fps, raw 775 KB/s, celdas 10 fps con decisión en 3.3 ms; los tres fallos simulados vuelven a jpeg y avisan.
- Tabla vacía de las 9 pasadas: `docs/latencia_modos.md`.
- Sobre tu observación («según fps el mejor modo es jpeg»): en el log del teléfono solo hay datos del modo `jpeg` (12.0–12.6 fps); no hay líneas de raw ni de celdas con la cámara real, así que esa conclusión todavía no está respaldada con datos.

### H — Comparativa de detectores (rama `investigacion/comparativa-detectores`)
- SSD MobileNetV2 cuantizado (Coral) corre en **11.9 ms** por NNAPI (acelerador `liteadaptor`), frente a 82 ms en la CPU de referencia, 77 ms en GPU y 87–97 ms en CPU; EfficientDet-Lite0 no gana nada con ese acelerador; YOLO11n en ONNX Runtime tarda 55–79 ms y falla en NNAPI. **No** se afirma que `liteadaptor` sea la NPU.
- Licencia: YOLO/Ultralytics es AGPL-3.0, por eso no se suben pesos ni código. Detalle en `docs/comparativa_detectores.md` de esa rama.

## 3. Pruebas en el carro (tuyas) con datos del log

### Frenado ante el termo, con los valores por defecto (umbral 40, celdas 7)

| Pasada | Cercanía | STOP desde el cuadro | Respuesta | Retraso total | Recorrido calculado | Distancia final (tu cinta) |
|---|---|---|---|---|---|---|
| 1 | 2 | 58 ms | 30 ms | 393 ms | 9.0 cm | 30–50 cm |
| 2 | 1 | 149 ms | 126 ms | 532 ms | 11.7 cm | 30–50 cm |
| 3 | 1 | 138 ms | 228 ms | 572 ms | 12.5 cm | 30–50 cm |

Antes de quitar la configuración guardada (umbral 198, celdas 9) el retraso era 504–542 ms y el detector de color veía el obstáculo con 0–3 celdas al centro, frenando tarde por el modo «cualquier objeto».

### Hallazgos de las pruebas
1. **La configuración guardada en el teléfono anulaba los valores por defecto validados.** Se borraron `umbral_color`, `min_celdas`, `fila_minima_mil`, `aprendizaje_mil` y `grupos_piso`; con los valores por defecto 420 cuadros de piso vacío dieron 0 falsos obstáculos.
2. **El panel semiautónomo no se podía desplazar**: ahora va dentro de un `ScrollView`.
3. **Bug de la pantalla de visión** (usaba `camara` antes de crearla), encontrado y corregido con la cámara falsa.
4. El umbral de «cualquier objeto» está en 0.07 en tu teléfono (el validado es 0.15): puede dar falsas alarmas si frena sin obstáculo.

## 4. Pendiente para la prueba final (tú decidiste dejar todo junto al final)

1. Piso negro y piso de 2 colores: **Aprender piso** y pasadas (casa y universidad).
2. Ruta con la mochila a mitad de camino, con el esquive.
3. Modelo sin reconocer: termo, caja y mochila a 120 cm y piso vacío 1 minuto (`docs/modelo_objetos.md`, sección 7).
4. Las 9 pasadas de los modos de video con la caja a ~80 cm (`docs/latencia_modos.md`).
5. Opcionales: flujo del piso (`docs/flujo_piso.md`), comparación de compresores JPEG y revisar la reincorporación del esquive.

Omitidos por tu decisión: pasadas rectas de 150 cm (confirmadas a ojo) y calibración del retroceso (igual que la de ida; sin calibrar la app usa el valor del avance).

## 5. Commits (todos en `main`, el último de H en su rama)

`c11f324`, `dc980a0`, `a6fca1a`, `16850a9` (calibración a 210), `f22b148` (A), `064a24f` (B), el commit de C, `c8d8078` (E), `aa1c2a1` (F), `b10f805` (G), `db52258` (D e I, juntos porque tocan los mismos archivos), `d638ed4` (H, en la rama de investigación) y los de bitácora y del panel con scroll. Registro detallado: `docs/bitacora.md` y `docs/progreso.md`.
