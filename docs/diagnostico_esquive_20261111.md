# Esquive confirmado: diagnóstico del 11-nov-2026

Base: main y v1.1 = `bd1727b`. Rama `fix/navegacion-esquive-confirmado`. **SOFTWARE / SIMULADO; arreglo NO MEDIDO en el carro.** No se movió el vehículo, instaló APK ni grabó firmware. No se leyó `secretos.h` ni modificó `original_backup/`. No se movió v1.1 ni se declaró v1.2.

## Fase 1: condiciones exactas de v1.1

La revisión y reproducciones de referencia se hicieron antes de editar navegación.

### Desde Kotlin hasta el esquive

`CamaraObstaculos.kt` obtiene color, modelo y movimiento, ejecuta `fusion_combinar` y calcula el corredor. `ModoSemiautonomo.kt` entrega cada cuadro nuevo una sola vez al puente JNI; conserva el número de celdas y aplica el corredor a zonas cuyo resultado no alteró la fusión. `corredor.c` trabaja sobre la rejilla, no calcula metros. SENS llega por otra vía a `navegador_aporte_sens`.

En `obstaculos.c`, una zona ocupada exige el mínimo configurado de celdas que no son piso; la última fila ocupada asigna cercanía 1/2/3 (cortes 667 y 834 por mil). **OBSTACULO 3 es cercanía**, no cantidad. `fusion.c` no tiene dato láser; conserva positivos cercanos de color aunque modelo o flujo discrepen. Los ajustes 240/23/440 por mil no demuestran la causa óptica sin cuadros y piso aprendido. Un umbral de color mayor tolera más diferencia de piso; no prueba sensibilidad excesiva.

Al avanzar, `bloqueo_confirmado` envía a `NAV_DETENIDO_OBSTACULO`:

- Cercanía 3 con al menos 6 celdas: un cuadro. Con menos celdas: dos cuadros.
- Cercanía 2 central: dos cuadros; lateral: dos cuadros y 150 ms.
- Anticipación central lejana habilitada: dos cuadros y 60 ms; Kotlin la habilita con video del carro.
- Si hay obstáculo al arrancar: espera cuatro cuadros de firma igual o 1500 ms antes de decidir. Puede existir sondeo inicial de 4 cm ante un fantasma lateral.

El láser es **opcional** para esquivar en v1.1:

- Con autoridad encendida (defecto de la app), solo actúa si está habilitado, vivo, válido, positivo y con antigüedad hasta 400 ms. Reemplaza el centro según el umbral de 400 mm y afecta laterales enteramente comprendidos en columnas centrales 3–6. Un positivo lateral fuera de ese haz sigue siendo visual. Sin lectura válida vuelve al criterio visual.
- Sin autoridad, tres lecturas cercanas (<500 mm) aceleran la confirmación central de cercanía 2; tres libres (>800 mm **o inválidas**) solo agregan un cuadro de espera. No son veto al esquive. Cercanía 3 conserva el freno visual.
- Después de 500 ms detenido y al menos un cuadro, elige lado libre y pasa a `NAV_ESQUIVANDO`, si caben arco y límites de intentos. `TOF_PARO` podía escoger última rejilla o izquierda después de 700 ms sin cuadro nuevo. Máximo tres esquives. Ambos lados bloqueados durante 4000 ms llevan a AYUDA (o ERROR si AYUDA está deshabilitada).

Tres zonas persistentemente ocupadas **no explican por sí solas la maniobra reportada**: sin lado libre el caso sintético acaba en AYUDA sin esquive. Falta telemetría de la corrida de 13 s para saber qué zona cambió.

### Distancia y recorte del rebase

Al detenerse, v1.1 guarda lectura láser fresca/válida, o cero. Planea con esa medida si existe; si falta la sustituye por 300/500/800 mm según banda visual. Añade fondo supuesto 250 mm y `NAV_PASO_MARGEN_MM = 150 mm`. El objetivo lateral es 400 mm, descontando pivotes y con mínimo de avance. Los mm se ejecutan por tiempo calibrado, no encoder.

**Se descarta que todos los tramos fueran tiempos fijos independientes del láser.** Sí se confirman la sustitución visual sin medida y el defecto de `calcular_fin_obstaculo`: recorta el objetivo al final de la recta (50 mm antes si es el último tramo). Puede quedar antes del objeto. Reincorporación y cierre no exigían láser libre. El arco, además, usaba banda visual aunque existiera medida láser.

## Revisión de autonomía del 12-nov-2026

La primera corrección (`458d5b3`) era demasiado conservadora: exigía láser para todo esquive y convertía el costado aún visible en AYUDA. **Era una regresión:** v1.1 terminaba después del banquito simulado con dos esquives; la primera corrección se detenía antes de terminar el rebase. Esa política queda reemplazada por la siguiente, a petición del usuario.

### Política implementada

- Láser vivo, válido, positivo y fresco (hasta 400 ms) a **400 mm o menos**: confirma obstáculo frontal, aunque ambas fuentes visuales estén vacías.
- Láser libre y solo color o solo red positivos: continúa, sin STOP ni AYUDA. Los positivos de color en las tres zonas siguen siendo **una sola fuente**.
- Color **y** EfficientDet coincidentes en la misma zona: confirman por visión incluso con láser libre, sin eco o vencido. No se confunde el resumen global «ambos» con coincidencia espacial. El resultado de red caduca a 300 ms; un resultado fresco vacío es desacuerdo, no fallo del sensor.
- Banda 1 permite aproximarse con precaución; bandas 2/3 frenan. Sin distancia frontal utilizable, la precaución usa tandas de avance de 700 ms y STOP de 300 ms: reduce velocidad media, no modifica PWM ni calibra una nueva velocidad de motor. Con distancia láser de aproximación entre 400 y 800 mm se conserva el avance calibrado hasta la reacción cercana. Con láser sin eco/vencido y una sola fuente también avanza con precaución.
- Siempre hay STOP antes de planear, pausa de 500 ms y un cuadro nuevo para escoger lado. El plan registra la distancia láser disponible hasta 500 mm; de faltar, usa la banda coincidente (300/500 mm, **estimaciones**, no medidas). El cierre bloqueado puede usar una lectura hasta 800 mm para reintentar.
- Se mantiene el freno independiente del firmware `TOF_PARO` a 120 mm. El navegador conserva su distancia y no lo libera por un cuadro vacío. No se cambió firmware.

### Rebase iterativo y cierre

El primer objetivo longitudinal es `s + 120 mm sensor-centro + distancia frontal + tramo exploratorio 200 mm + margen 150 mm`. **200 mm es un intervalo de exploración, no el fondo asignado al objeto. Se eliminó el supuesto de 250 mm.** Los avances siguen ejecutándose por tiempo calibrado.

Durante el regreso casi transversal (50 mm hacia delante) se revisan el láser y las tres zonas visuales. Si el costado sigue presente, STOP, registro del avance observado desde la cara frontal, otro tramo de 200 mm y desvío lateral 100 mm mayor (400 inicial, hasta 700 mm). Hay hasta seis intentos por ruta. Un giro interrumpido usa el ángulo SENS disponible para cerrar su pose; no lo da por completado ni lo calcula solo por tiempo. El fondo observado es una cota aproximada ligada a la pose estimada, no una medición completa del contorno.

El plan nunca se recorta. Si el obstáculo está junto al final, puede prolongar **solo la última recta**, hasta 2 m y dentro del arreglo de puntos. No inventa un giro de ruta nuevo ni un mapa libre de esos 2 m: continúa comprobando sensores. Una curva que no permite encajar el rodeo puede requerir AYUDA.

Para cerrar el esquive se exige haber alcanzado el avance planeado. Para cerrar la ruta se detiene y asienta 300 ms; exige tres muestras nuevas válidas **>800 mm**, sin consenso visual pendiente, o cinco cuadros nuevos sin coincidencia si el láser reporta sin eco (o está deshabilitado). Una muestra duplicada no cuenta tres veces. Un SENS vencido no se convierte en «sin eco» para cerrar. Al no cumplirlo, reintenta; agotado el presupuesto, AYUDA. La app conserva cancelación por enlace/SENS perdido (>1,5 s), video perdido y error de análisis.

Ambos lados ocupados deben persistir 4000 ms antes de abandonar. Antes intenta retroceder 150 mm si conserva al menos 180 mm del tramo recto recién recorrido. Esa evidencia se borra al girar y se consume al retroceder; **no hay sensor trasero ni garantía ante objetos que aparezcan por detrás**. El mismo retroceso permite liberar un arco de salida demasiado próximo. No se usa un retroceso desconocido para esos casos.

### Nueve grupos solicitados: antes y después

Banco [pruebas_esquive_confirmado.c](../nucleo_c/pruebas_esquive_confirmado.c), ahora conectado a `fusion.c` real. Pose inicial `(1000,2000,0°)`, mm, frente hacia Y decreciente; chasis 200×240 mm, sensor 120 mm delante, cámara geométrica, cuadros cada 100 ms, calibración 51,8 ms/cm y giroscopio sano. Sin pivotes en este banco para aislar geometría; Monte Carlo sí incluye pivotes y ruido. Patas se representan como pérdida del retorno del rayo con ocupación visual: **no es una simulación fotométrica del mueble real**.

«Antes» es `458d5b3`, la corrección rechazada. TERMINADO tras objeto exige que la **cola completa** supere su cara posterior; también se verifica contacto con el rectángulo orientado del chasis.

| Caso | Antes | Después: pose final; tiempo; esquives | Resultado físico del modelo |
|---|---|---|---|
| 1. Caja 20×20, láser sano | TERMINADO, 22,40 s | (996,-8,0°); 22,50 s; 1 | Después, cola +768 mm |
| 2. Banquito 35×45, fondo 450 mm | AYUDA 14, (604,664,82°), 14,10 s | (1001,-17,0°); 32,10 s; 3 | **TERMINADO después, sin AYUDA**, cola +527 mm |
| 3. Objeto de 90 cm de fondo | AYUDA 14, (604,664,26°), 13,30 s | (1001,-614,0°); 44,90 s; 6 | TERMINADO después, cola +674 mm |
| 4. Falso de solo color, láser libre | AYUDA 13 | (1000,-7,0°); 11,00 s; 0 | Continúa, 0 STOP intermedios |
| 5. Falso de solo detector, láser libre | TERMINADO | (1000,-7,0°); 11,00 s; 0 | Continúa, 0 STOP intermedios |
| 6. Color+red, láser libre (patas) | AYUDA 13 | (997,-17,0°); 28,00 s; 2 | TERMINADO después, cola +527 mm |
| 7a. Sin eco, visión coincidente | AYUDA 13 | (997,-17,0°); 28,40 s; 2 | TERMINADO después, cierre visual |
| 7b. Sin eco, solo color | AYUDA 13 | (1000,-7,0°); 14,20 s; 0 | Continúa con precaución, sin AYUDA |
| 8. Falso en tres zonas, solo color | AYUDA 13 | (1000,-7,0°); 11,00 s; 0 | Continúa, 0 STOP intermedios |
| 9. Ruta 600 mm, caja al final | AYUDA 14 | (989,795,0°); 18,60 s; 1 | TERMINADO después, cola +415 mm |
| Regresión adicional: falso + banquito a 800 mm, ruta 600 mm | AYUDA 13; v1.1 daba falso TERMINADO | (1001,-17,0°); 32,10 s; 3 | TERMINADO después, cola +527 mm |

**Cero contactos en los once mundos antes y después.** Las pausas de precaución del caso 7b no son esquives. Registros completos: [v1.1](precision/esquive_autonomia_poses_bd1727b.txt), [corrección anterior](precision/esquive_autonomia_poses_458d5b3.txt), [autonomía actual](precision/esquive_autonomia_poses_actual.txt). Los dos primeros usan los algoritmos y constantes de sus commits, con el mismo adaptador de entradas geométricas externas.

La hipótesis inicial queda **parcialmente confirmada en software**: en v1.1 había disparos visuales sin confirmación y un recorte del rebase. Se descarta que todos los tramos fueran tiempos fijos independientes de la medida. En el caso adicional, v1.1 disparaba a 765 mm, recortaba el fin a 550 mm y terminaba con la cola en 489 mm cuando el objeto terminaba en 1370 mm: **881 mm antes de superarlo**. Eso no identifica la causa exacta de la corrida física de 13 s.

### Monte Carlo contra v1.1, mismas semillas

1000 corridas por cada uno de los dos escenarios y tres configuraciones J16, semillas `160000 + índice*10007 + corrida` (índices 8 y 9). Referencia real `bd1727b`; no se usa la columna histórica «ANTES: J15» como sustituto de v1.1. Fondo válido a 1200 mm en ambos mundos. El informe de la primera revisión ya comprobaba que ese ajuste no alteraba los resultados de referencia. El banco interno también ejecuta sus controles generales; esta comparación evalúa exclusivamente los dos esquives pedidos.

| Escenario / configuración | Media v1.1 → actual (mm) | p95 v1.1 → actual (mm) | Llegadas antes → después | Contactos antes → después |
|---|---:|---:|---:|---:|
| 20×20 / sin RUMBO | 33 → 32 | 78 → 76 | 1000 → 1000 | 0 → 0 |
| 20×20 / RUMBO | 17 → 17 | 40 → 37 | 1000 → 1000 | 0 → 0 |
| 20×20 / RUMBO + escala | 17 → 16 | 40 → 37 | 1000 → 1000 | 0 → 0 |
| Ancho / sin RUMBO | 33 → 32 | 79 → 77 | 1000 → 1000 | 0 → 0 |
| Ancho / RUMBO | 17 → 16 | 40 → 38 | 1000 → 1000 | 0 → 0 |
| Ancho / RUMBO + escala | 17 → 16 | 39 → 38 | 1000 → 1000 | 0 → 0 |

La media/p95 del banco miden error lateral a la línea final en los esquives; no deben presentarse como precisión total de posición. El error longitudinal se conserva en los registros. «Ancho» son dos celdas, 400×200 mm; el banquito de fondo 450 mm y el objeto de 900 mm se prueban aparte, arriba. **Ninguna de las seis comparaciones empeora**, sin tolerancia de +1 mm ni porcentual. Reproducción: `python scripts/comparar_esquives.py --corridas 1000`; compila solo fuentes explícitas en temporal. [Comparación estricta](precision/esquive_autonomia_comparacion.txt), [referencia](precision/esquive_autonomia_mc_v11.txt), [actual](precision/esquive_autonomia_mc_actual.txt).

### Pruebas y alcance

Se sustituyeron las expectativas revocadas de «color manda», «sin láser siempre AYUDA» y maniobras atadas a horarios por matrices de fuentes/bandas/frescura y mundos con contacto geométrico. Se conservaron las pruebas de calibración, pivotes, corrección de rumbo, pausas, cancelación, pérdida de video y demás módulos. Las entradas sintéticas de obstáculo del simulador representan explícitamente coincidencia; los falsos positivos se inyectan por fuente usando la fusión real.

Resultados de esta revisión, comprobados al cierre del 13-nov-2026: `make -C nucleo_c test`, `make -C pruebas_firmware test`, servidor falso desde `pruebas_firmware`, `scripts/probar_version.py` y `docs/verificar_enlaces.py`, todos con salida 0. Núcleo: 1834 casos en 23 ejecutables; firmware: 540 casos y 3 verificaciones de página; servidor: 54; versión: 18; enlaces: 121 comprobados, ninguno roto. El puente JNI pasa revisión de sintaxis con Clang Android. **No se compiló APK ni firmware en esta revisión**, no se instalaron ni se ejecutaron pruebas con el carro. La compilación APK registrada en el commit anterior no valida este cambio Kotlin.

### Compilar e instalar conservando datos: pasos preparados, NO ejecutados

Desde `E:\Dev\fsi_proyecto`, en esta rama:

```powershell
git branch --show-current
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/compilar_app.ps1
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r app_android/app/build/outputs/apk/debug/app-debug.apk
```

`-r` reemplaza la aplicación y conserva datos; requiere el mismo identificador y firma que la instalada. Si Android rechaza la firma, detenerse y usar la clave original: **no desinstalar ni borrar datos**. No se incluye un comando para abrir la app o iniciar rutas. El script de compilación configura JAVA_HOME solo en su proceso hijo; no cambia Python/PATH/JAVA_HOME globales.

Firmware, solo compilación local preparada, **NO ejecutada**:

```powershell
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile --fqbn esp32:esp32:esp32 --build-path "$env:TEMP\fsi-esquive-fw" firmware_carro
```

No cargar ni publicar el binario. No mover la etiqueta v1.1 ni declarar v1.2; no hacer merge. Instalación, grabación y cualquier prueba con el carro requieren autorización posterior.

### Qué falta medir

**Todo el arreglo sigue SIMULADO / NO MEDIDO en el carro. Banquito: NO MEDIDA hasta repetir primero con caja de 20×20 cm.** Registrar distancia inicial y al frenar, SENS/estado de ambas fuentes por zona, longitud de cada rebase, reintentos, pose final, contactos y causa final. Medir sensor-centro, chasis, pivotes, distancia por tiempo y frenado con esta batería e iluminación; después repetir con banquito, patas, ausencia de eco y objeto largo. No se atribuye al carro el éxito de estos modelos ni se promete superar cualquier geometría sin espacio libre.
