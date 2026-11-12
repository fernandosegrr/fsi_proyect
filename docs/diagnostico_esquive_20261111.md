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

## Reproducción determinista antes y después

Banco: [pruebas_esquive_confirmado.c](../nucleo_c/pruebas_esquive_confirmado.c). Pose inicial `(1000,2000,0°)`, mm, frente hacia Y decreciente; chasis 200×240 mm y sensor 120 mm delante del centro. Cámara geométrica por zonas, rayo contra rectángulos, fondo válido a 1200 mm, cuadros cada 100 ms, calibración 51,8 ms/cm, giroscopio sano y pivote nulo para aislar el defecto. Pérdida de eco inyectada como inválida; no se simula la reflectancia de patas. Recta 2000 mm salvo casos cortos de 600 mm. No reconstruye el montaje físico ni sus 13 s.

| Caso | v1.1: pose final; estado; tiempo | Rama: pose final; estado; tiempo | Posición respecto al objeto |
|---|---|---|---|
| Caja 20×20, láser sano | (994,-7,0°); TERMINADO; 20,45 s | (996,-8,0°); TERMINADO; 22,40 s | Ambos después |
| Banquito como bloque 35×45, láser sano | (991,-13,0°); TERMINADO, 2 esquives; 27,95 s | (604,664,82°); AYUDA 14, 1 esquive; 14,10 s | Rama antes de completar rebase; no declara éxito |
| Falso lateral sin objeto | (991,-11,0°); TERMINADO, 1 esquive; 20,85 s | (1000,2000,0°); AYUDA 13, 0 esquives; 1,30 s | Sin objeto |
| Falso lateral + objeto a 80 cm, ruta 60 cm | (996,1391,0°); TERMINADO; 14,90 s | (1000,2000,0°); AYUDA 13, 0 esquives; 1,30 s | Ambos antes; solo v1.1 declara éxito |
| Patas: pérdida de eco, objeto 35×45 | (1000,1556,0°); AYUDA 1; 6,30 s | (1000,1556,0°); AYUDA 13; 3,30 s | Ambos antes, sin esquive |
| Visión sin láser habilitado, objeto cercano | (992,-5,0°); TERMINADO, 1 esquive; 20,25 s | (1000,2000,0°); AYUDA 13; 1,30 s | Rama antes, sin esquive |
| Objeto próximo al final de ruta corta | (680,921,38°); AYUDA 11, 3 esquives; 20,70 s | (1000,2000,0°); AYUDA 14, 0 esquives; 0,80 s | Rama rechaza plan insuficiente |
| Objeto de 90 cm de fondo | (613,776,64°); AYUDA 1; 17,60 s | (604,664,26°); AYUDA 14; 13,30 s | Ambos sin completar rebase |
| Falso en las tres zonas | (1000,2000,0°); AYUDA 1; 4,30 s | (1000,2000,0°); AYUDA 13; 1,30 s | Sin objeto ni esquive |

Cero contactos en los nueve casos. Causa 13: falta confirmación; 14: rebase/plan insuficiente; 15: frente no libre al cerrar. Se prueban además 500/501 mm, dato inválido, duplicado, vencido, zonas alternadas, cierre ocupado/sin eco y pérdida de video.

**Falso Terminado confirmado:** en la ruta de 600 mm, v1.1 dispara con medida 765 mm, recorta el fin a 550 mm y planea solo 499 mm de paralelo. El centro termina con avance 609 mm, antes incluso de la cara frontal situada a 920 mm del origen. La cola queda en 489 mm y el borde posterior del objeto en 1370 mm: faltan 881 mm. Declara TERMINADO. La rama exige medida ≤500 mm y no inicia esa maniobra.

Con caja de 20 cm, ambas disparan a 395 mm. El plan cambia de 1199 a 1319 mm sobre la ruta y de 799 a 919 mm de paralelo, al incluir sensor-centro. El banquito tiene 450 mm de fondo, mayor que los 250 supuestos: la rama detecta su costado al volver y pide AYUDA sin declarar éxito.

Conclusión: **hipótesis parcialmente confirmada en software** (visión sin confirmación y rebase truncado). No queda demostrada la causa exacta de la prueba física ni el origen fotométrico del falso positivo. No se alteró el control de los giros reportados.

## Cambios

1. STOP visual separado del permiso de esquive: tres lecturas válidas de 1–500 mm, frescas hasta 400 ms, más cámara lateral. Persistencia visual sin distancia: STOP y AYUDA tras 1000 ms. Varios cuadros confirman ocupación, no profundidad; se adopta la política conservadora de no esquivar a ciegas.
2. Medida al confirmar con el carro parado; plan con sensor-centro, fondo y margen, sin recorte. Arco basado en distancia medida. Si no cabe, AYUDA.
3. Vuelta casi transversal, apuntando 50 mm por delante de la proyección actual; vigilancia del costado y verificación del avance planeado. Si no supera el objeto, AYUDA.
4. Cierre tras STOP, 300 ms de asentamiento y tres lecturas nuevas válidas >800 mm; sin comprobarlo en un segundo, AYUDA. Sin eco no es libre. Un error de video no se convierte en éxito.
5. Se conservan los valores de visión 40/7/44 %. La app avisa y exige Aprender piso exitoso para cada nueva ruta con cámara; elimina el segundo toque que reutilizaba el piso e invalida el aprendizaje anterior al iniciar otro intento.
6. El simulador de la app aporta rayo láser independiente y termina también en AYUDA. Distingue fondo medido de pérdida de eco. Se adaptan pruebas que exigían esquive solo visual y se mantienen esquives válidos con compensación de pivote.

## Monte Carlo contra v1.1

Solo se comparan `esquive_20x20` y `esquive_ancha` (40×20 cm del banco J16; **no es el banquito 35×45**). Mil corridas por escenario/configuración, mismas semillas `160000 + escenario*10007 + corrida`, ruido y calibración. Las columnas internas «ANTES: J15» del banco no son la referencia v1.1.

| Caso / configuración | Media lateral v1.1 → rama (mm) | p95 v1.1 → rama (mm) | Llegadas v1.1 → rama | Contactos v1.1 → rama |
|---|---:|---:|---:|---:|
| 20×20 / sin RUMBO | 33 → 31 | 78 → 79 | 1000 → 1000 | 0 → 0 |
| 20×20 / RUMBO | 17 → 16 | 40 → 37 | 1000 → 1000 | 0 → 0 |
| 20×20 / RUMBO + escala | 17 → 16 | 40 → 36 | 1000 → 1000 | 0 → 0 |
| Ancho / sin RUMBO | 33 → 31 | 79 → 77 | 1000 → 1000 | 0 → 0 |
| Ancho / RUMBO | 17 → 16 | 40 → 37 | 1000 → 1000 | 0 → 0 |
| Ancho / RUMBO + escala | 17 → 16 | 39 → 37 | 1000 → 1000 | 0 → 0 |

Con RUMBO (configuración reportada) mejoran media y p95 en ambos casos, sin perder llegadas ni aumentar contactos. Sin RUMBO, p95 de caja sube **1 mm**: queda dentro del criterio existente (+5 mm media, +10 mm o +20 % p95, sin empeorar contactos/llegadas). No se afirma mejora estricta de todas las métricas ni validación física.

Se ejecutó también v1.1 con el mismo fondo láser válido de 1200 mm de la rama: las seis filas resultan idénticas a v1.1 sin adaptación. La mejora no proviene de contar «sin eco» como libre. Registros: [v1.1](precision/esquive_v11.txt), [v1.1 con fondo pareado](precision/esquive_v11_fondo_pareado.txt), [rama](precision/esquive_corregido.txt), [poses antes](precision/esquive_poses_antes.txt) y [poses después](precision/esquive_poses_despues.txt) y [comparación comprobada](precision/esquive_comparacion.txt).

## Verificación y reproducción

Desde la raíz de la rama:

```powershell
make -C nucleo_c test
make -C pruebas_firmware test
Push-Location pruebas_firmware
.\.venv\Scripts\python.exe probar_servidor_falso.py
Pop-Location
python scripts/probar_version.py
python docs/verificar_enlaces.py
.\nucleo_c\pruebas_esquive_confirmado.exe --informe
.\nucleo_c\pruebas_precision_j16.exe 1000
```

La referencia se compiló desde fuentes explícitas de `git show bd1727b:ruta`, en temporal, sin cambiar main ni exportar archivos privados. Mismo GCC C99 y `-O2` para Monte Carlo. Las pruebas que exigían movimiento sin medida ahora comprueban STOP/AYUDA; las nuevas comprueban éxito con láser sano y rechazo seguro. Resultados de esta revisión, todos con salida 0:

| Comprobación | Resultado |
|---|---|
| `make -C nucleo_c test` | 23 programas, 1681 casos, 0 fallos; incluye 43 casos nuevos |
| `make -C pruebas_firmware test` | 540 casos, 0 fallos; página HTML idéntica (3 comprobaciones) |
| Servidor falso (desde `pruebas_firmware`) | 54 casos, 0 fallos |
| `probar_version.py` | 18 pruebas, OK |
| `verificar_enlaces.py` | 135 Markdown, 121 enlaces, 0 rotos |
| Monte Carlo de 1000 corridas | 46 comprobaciones del banco, 0 fallos; seis filas comparadas contra v1.1 dentro del criterio existente |
| APK `assembleDebug` | BUILD SUCCESSFUL; sin instalar |

No se ejecutaron sanitizadores en esta sesión. Los resultados históricos del manual conservan su fecha.

## Compilar sin instalar ni grabar

APK desde esta rama, en proceso hijo (JAVA_HOME solo en ese proceso):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/compilar_app.ps1
```

Salida local: `app_android/app/build/outputs/apk/debug/app-debug.apk`. Compilación de depuración comprobada; sin instalación ni reemplazo del entregable de `dist/`.

Firmware DevKit, **paso preparado, no ejecutado**:

```powershell
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile --fqbn esp32:esp32:esp32 --build-path "$env:TEMP\fsi-esquive-fw" firmware_carro
```

No requiere puerto ni orden de carga. Firmware sin cambios; no se publica su binario. Dependencias y configuración local deben estar preparadas. No ejecutar `cargar.ps1` en esta sesión: su preparación inspecciona archivos privados. Para regenerar manual sin alterar figuras: `python docs/manual/generar_manual.py --sin-figuras`.

## Pendiente de autorización y medición

**Banquito: NO MEDIDA; arreglo NO MEDIDO.** Primero repetir con caja 20×20 cm y registrar distancia inicial/al frenar, SENS válido/fresco, decisión de esquive, avance de rebase, pose final, contactos y estado. Después medir sensor-centro, fondo y patas del banquito, pérdida de eco y falsos positivos con iluminación actual. No atribuir éxito simulado al carro. Instalación, grabación, pruebas físicas y eventual v1.2 requieren autorización del usuario.
