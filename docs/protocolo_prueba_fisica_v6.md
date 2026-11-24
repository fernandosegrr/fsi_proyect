# Protocolo de prueba física — autónomo v6 contra v3 (A/B)

Rama `feat/autonomo-v6`. **PENDIENTE DE EJECUTAR EN EL CARRO REAL.** Todo lo que este repositorio afirma de `v3` y de `v6` es `SIMULADO` (banco de `nucleo_c`) o `NO MEDIDO`; nada de `v6` está `VALIDADO EN EL CARRO`. Solo se prueban en el carro dos versiones: **`v3` (el respaldo seguro) y `v6`**; `v5-carbon` y `v5-carbon-fix` quedan como referencia de simulación.

El protocolo empieza con mediciones quietas de riesgo cero y sube por bloques: **no pases al bloque siguiente si el anterior no cumple su criterio o dispara una regla de aborto.** Hoja de registro: [pruebas_fisicas_v6_plantilla.csv](pruebas_fisicas_v6_plantilla.csv) (una fila por medición o corrida). Antecedentes: [protocolo_prueba_fisica_v5.md](protocolo_prueba_fisica_v5.md), diseño y datos de la simulación en [autonomo_v6_diseno.md](autonomo_v6_diseno.md) y [progreso_v6.md](progreso_v6.md).

---

## 0. Reglas inviolables en sesión

1. **Mano lista en el interruptor del carro** durante cada corrida en movimiento. Ante cualquier riesgo de choque contra pared, mueble o persona: levantar el chasis o apagar. Con la cámara y el láser el carro ya frena solo, pero v6 no está validado: la mano es el freno de verdad.
2. **Abortar toda la sesión de inmediato si:**
   - el carro se mueve sin una orden activa de la app;
   - no se detiene al pulsar **STOP**;
   - gira más de una vuelta continua sobre sí mismo;
   - en la prueba estática frontal `P0` el freno por láser no detiene antes de tocar;
   - la app se cierra dos veces seguidas.
3. **Si la app pide ayuda con `Causa 18` (giro bloqueado), no pulses «Continuar»: toma el control manual.** Es un defecto que ya tenían `v3` y `v5` (la revisión independiente de v6 lo encontró): tras esa ayuda, al continuar, el frenado del centro por cámara queda ciego hasta el siguiente giro y solo quedaría el freno del láser del firmware. `v6` lo corrige (SIMULADO, prueba unitaria), pero no lo des por hecho en el carro.
4. **Protección eléctrica y de software:** tres fuentes separadas (USB #1 para la ESP32 del carro, 4×AA solo al `VDD` del `TC1508A` con tierra común, USB #2 para la cámara). No desinstalar la app ni borrar datos del teléfono; no usar `erase_flash`; una sola placa por USB a la vez. **No guardes imágenes reales de la cámara.**
5. **Privacidad:** las cajas negras y los CSV de estas pruebas se quedan en tu equipo; no se publican en el repositorio.

---

## 1. Preparación (NO la ejecuta el asistente)

### 1.1 Instalar la APK conservando datos (`adb install -r`)

Desde PowerShell, en la raíz del repositorio (`E:\Dev\fsi_proyecto`), con la rama `feat/autonomo-v6` y el teléfono por USB:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/compilar_app.ps1
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r app_android/app/build/outputs/apk/debug/app-debug.apk
```

- **Si `adb install -r` falla por la firma (`INSTALL_FAILED_UPDATE_INCOMPATIBLE`): DETENTE.** No desinstales la aplicación: se perderían rutas guardadas, calibración y configuración. Compila con la misma llave de depuración que la APK ya instalada.
- Tras instalar, abre **Más → Sensores** y comprueba:
  - Los interruptores **«Autónomo v5-carbon (referencia)»** y **«Autónomo v6 (candidato)»** aparecen **APAGADOS**. Con los dos apagados el carro se comporta como `v3`. (La clave guardada de v5 cambió a propósito: un «encendido» de una versión anterior se ignora. Si alguno aparece encendido, apágalo.)
  - La calibración previa sigue ahí; **Autoridad del láser** y **Sostener el rumbo** como los tenías.
- **Firmware: no cambia.** v6 no toca el firmware (`TOF_PARO`, watchdog de 1000 ms y STOP/EBRAKE quedan igual) y es compatible con el firmware que ya tiene el carro. Lo que haya en el carro, se queda.

### 1.2 Antes de cada sesión

1. `Aprender piso` en el sitio con el tramo despejado (umbral `40`, mínimo de celdas `7`, fila mínima `44 %`, detector encendido, autoridad del láser encendida).
2. Anota el voltaje de las 4 AA al empezar y al terminar. Si baja de forma notable entre un par A y B, cambia las pilas: la velocidad cambia y A y B dejan de ser comparables.
3. No recalibres pivotes ni IMU entre A y B.

---

## 2. Estado de la evidencia (qué está medido y qué no)

| Afirmación | Estado | Dónde |
|---|---|---|
| Con los dos interruptores apagados el navegador es `v3` exacto (`AV5_APAGADO=1` ≡ `e37f77b`, línea por línea, 16 matrices = 32 comparaciones idénticas) | `SIMULADO` | [progreso_v6.md](progreso_v6.md) |
| Con v6 apagado y v5 encendido es `v5-carbon-fix` exacto | `SIMULADO` | [progreso_v6.md](progreso_v6.md) |
| La app arranca con v5 y v6 apagados y la caja negra anota `version=` | `SIMULADO` (compilado, pruebas del núcleo) y `PENDIENTE` en el teléfono | este protocolo, §1.1 y §3 |
| v6 reduce contactos con muebles de patas finas y con objetos oscuros sin eco frente a `v3` | `SIMULADO` | [autonomo_v6_diseno.md](autonomo_v6_diseno.md) |
| v6 evita el contacto extra de `pared_al_frente` con láser pesimista | `SIMULADO` | [autonomo_v6_diseno.md](autonomo_v6_diseno.md) |
| v6 no frena ni esquiva de más que `v3` en piso limpio con ruido realista | **NO CUMPLE en la simulación** (esquives por encima de `v3`); es la pregunta principal de esta prueba | [autonomo_v6_diseno.md](autonomo_v6_diseno.md) |
| Cualquier número de contactos, esquives o paradas de `v6` en el carro | `NO MEDIDO` | esta prueba |
| Velocidad real, pivotes, distancias de frenado con `v6` | `NO MEDIDO` | esta prueba |

---

## 3. Qué interruptor en cada corrida (y cómo comprobarlo)

| Corrida | **Más → Sensores** | La caja negra debe decir |
|---|---|---|
| **A = v3** | «Autónomo v5-carbon (referencia)» **APAGADO** y «Autónomo v6 (candidato)» **APAGADO** | `v5=0 v6=0 version=v3` |
| **B = v6** | «Autónomo v5-carbon (referencia)» **APAGADO** y «Autónomo v6 (candidato)» **ENCENDIDO** (v6 ya incluye v5) | `v5=1 v6=1 version=v6` |

- No uses el interruptor de v5 en las pruebas: **solo `v3` contra `v6`**. Si por error queda encendido solo v5, la caja negra dirá `version=v5`: descarta la corrida y repítela.
- El cambio entre A y B es **un solo toque** en el interruptor de v6, con el carro quieto y fuera de una ruta en marcha. El interruptor se aplica al cargar la ruta siguiente.
- **Comprueba la versión de cada corrida** en la cabecera de la caja negra al terminar (`firmware=... v5=... v6=... version=...`) o, si tienes `adb logcat` conectado, en la línea `SENS_DIAG ruta: ... version=...` que sale al cargar la ruta. Anota en el CSV la versión que dice la caja negra, no la que creías haber puesto. **Si no es la que querías, descarta esa corrida y repítela** (no hace falta abortar la sesión si no hubo riesgo).
- Con v6 encendido el carro **no** cambia nada más de su lógica: misma ruta, mismo piso, mismo láser, mismo watchdog.

---

## 4. Orden de las corridas: del menor al mayor riesgo, igual para A y B

Se repite el **mismo orden y la misma posición medida con cinta** para A y para B. Alterna pares `A-B-A-B` (no hagas todas las A y luego todas las B) para que la pila, la luz y el piso afecten a los dos por igual.

| Bloque | Prueba y escenario | Repeticiones | Criterio de aceptación (para v6) | Criterio de aborto |
|---|---|---|---|---|
| **P0** (riesgo cero/bajo) | Freno estático frontal contra pared o caja rígida desde 80 cm, **mano en el interruptor**; `M-v6.1` y `M-v6.2` (quietas, §6) | 3 por versión | Se detiene sin tocar, igual que `v3` | Un solo contacto o fallo de frenado → **aborta la sesión** |
| **P1** (riesgo bajo; **piso limpio, la prioridad**) | `P1.1` recta de 150 cm en piso limpio con luz normal; `P1.2` sombra cruzando; `P1.3` mancha de luz a un lado y en la línea; `P1.4` reflejo de ventana o de lámpara sobre el piso; `P1.5` pared a 60 cm tras la meta | 3 por versión y por prueba | **0 esquives, 0 `AYUDA` y no más paradas que `v3`** en cada par; `P1.5` termina sin rodear la pared | **Cualquier esquive, `AYUDA` o parada de más de v6 que `v3` no tuvo en el par → deja de probar v6 (queda `v3`)** |
| **P2** (riesgo medio) | `P2.1` caja de 20×20 centrada; `P2.2` caja corrida 15 cm; `P2.3` objeto bajo (<6 cm de alto); `P2.4` objeto largo (90 cm de fondo) | 3 por versión y por prueba | 0 contactos; se detiene y rodea, o pide `AYUDA` segura antes de tocar | 2 contactos en la misma prueba |
| **P3** (riesgo medio-alto) | `P3.1` objeto oscuro y mate (tela, fieltro o cartón mate negro, 20×20 cm) sin brillo; `P3.2` mancha oscura plana en el piso (hoja negra mate pegada al piso, **que se pueda pisar**), centrada y a un lado; `P3.3` pasillo simétrico de ~65 cm con la caja de 20×20; `P3.4` caja de 20×20 con pared lateral a 30–40 cm de la línea | 3 por versión | `P3.1`: se detiene antes de tocar (y rodea o pide `AYUDA`); `P3.2`: pasa o se detiene **sin empujar nada** (es un negativo: anota si frenó o esquivó); `P3.3` y `P3.4`: rodea sin tocar o se detiene con `Causa 16/18` sin tocar pared | contacto con pared lateral o giro forzado contra la pared |
| **P4** (riesgo alto) | `P4.1` pared al frente que cierra el paso (panel de ≥1.5 m de ancho cruzado en la ruta; **mano en el interruptor**, cartón o espuma delante); `P4.2` banquito o silla de patas finas sobre una recta de 200 cm | 3 por versión | `P4.1`: **frena sin tocar y pide `AYUDA`** (nunca avanza contra la pared); `P4.2`: menos contactos que `v3` o fallo seguro (`AYUDA`) antes de empujar una pata | 2 contactos fuertes contra pared o pata; levanta el chasis antes de que se enrede |

Notas del orden:

- `P1` va **antes** de `P2` a `P4` porque es la compuerta que más pesa: un v6 que molesta en piso limpio no se compensa con menos contactos.
- `P3.2` (la mancha oscura plana) mide el negativo difícil que la simulación no puede resolver: una mancha plana oscura sobre el carril y un objeto bajo oscuro sin eco se ven casi igual para la cámara y el láser. Si v6 la esquiva o se detiene y `v3` no, anótalo: es el costo conocido de detectar objetos oscuros sin eco.
- `P4.1` prueba el cambio de v6 para el giro de regreso: tras rodear, v6 deja de girar o de avanzar en paralelo si el láser ve algo muy cerca al frente y pide ayuda. Las causas esperadas son `Causa 16` (frente ancho) o `Causa 18` (giro bloqueado).

---

## 5. Batería mínima A/B antes de la exposición (si hay poco tiempo)

Si solo cabe una sesión corta, corre estas seis en este orden (cada una: 3 corridas de `v3` y 3 de `v6`, alternadas):

1. `AB1` = `P1.1` recta de 150 cm en piso limpio.
2. `AB2` = `P1.2` o `P1.3` sombra o mancha de luz controlada.
3. `AB3` = `P2.1` caja de 20×20 centrada.
4. `AB4` = `P3.1` objeto oscuro mate sin eco.
5. `AB5` = `P3.4` caja de 20×20 con pared lateral a 35 cm.
6. `AB6` = `P4.2` banquito de patas.

Si `AB1` o `AB2` ya muestran un falso positivo de v6 que `v3` no tuvo en el mismo par, **no sigas con v6**: apaga el interruptor y quédate con `v3`.

---

## 6. Mediciones quietas de riesgo cero (opcionales, útiles para el informe)

| ID | Duda física | Procedimiento | Qué anotar |
|---|---|---|---|
| `M-v6.1` | ¿El láser ve un objeto oscuro mate? | Carro quieto a 35 cm de la caja oscura mate (misma de `P3.1`). 10 s de `SENS` (`mm`, banderas) y la caja negra (`OBS_DIAG`). | Mediana del láser (`8190` = sin eco, o los milímetros reales) y si el color marca el centro |
| `M-v6.2` | ¿Qué ve la cámara de una mancha plana oscura? | Carro quieto a 35 cm de la hoja negra mate pegada al piso (`P3.2`), centrada y luego a un lado. Mismas lecturas. | Si el color la marca como «masa» y en qué tercio |
| `M-v6.3` | ¿Velocidad real y pivotes? | Cinta de 150 cm en AUTO y giros de 90° a izquierda y derecha (3 repeticiones) | cm/s reales; desplazamiento lateral de cada pivote |

---

## 7. Reglas de decisión

- **v6 solo es candidato para la exposición si, en el carro:** en `P1` tiene 0 esquives, 0 `AYUDA` y no más paradas que `v3` en cada par; en `P2` y `P4.1` no toca nada; y en `P3.1`, `P4.2` mejora frente a `v3` sin empeorar la seguridad.
- **Un solo falso positivo de v6 en `P1` que `v3` no tuvo en el par basta para quedarse con `v3`.** Con 3 repeticiones no se puede demostrar paridad estadística; sí se puede detectar que v6 molesta.
- **Si no se corre este protocolo, el carro se queda como `v3`:** es el arranque por defecto de la app (v5 y v6 apagados).
- **Revertir en el momento:** apaga el interruptor de v6 en **Más → Sensores**; la corrida siguiente es `v3` exacto.
- Con 3 repeticiones y un piso, esto es una prueba de humo, no una validación: lo que salga se anota como `MEDIDO EN EL CARRO (n=3, un piso)`.

---

## 8. Hoja de registro y cajas negras

Copia [pruebas_fisicas_v6_plantilla.csv](pruebas_fisicas_v6_plantilla.csv) a un archivo con la fecha (por ejemplo `pruebas_fisicas_v6_2026MMDD.csv`) y llena una fila por corrida o medición. La columna `version_caja_negra` es la que dice la cabecera de la caja negra, no la que creías haber puesto.

Al terminar la sesión, extrae las cajas negras (**no la ejecuta el asistente**):

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" pull /sdcard/Android/data/com.fsi.carro/files/cajas_negras cajas_negras
```

Las cajas negras llevan solo números y palabras (ninguna imagen). Después de extraerlas, compara cada par A/B: paradas, vistazos, esquives, `AYUDA` con su causa, y contactos.

---

## 9. Qué NO se puede concluir de estas pruebas

- Que v6 «es mejor»: con n=3 por prueba y un solo piso solo se detecta una diferencia grande o un falso positivo.
- Que la simulación predice el carro: el simulador se calibró con las cajas negras del 15 de noviembre (que son de `v3`); la calibración cubre el ruido de la visión, no toda la física.
- Nada sobre pisos, luces y objetos que no estén en la hoja de registro.
