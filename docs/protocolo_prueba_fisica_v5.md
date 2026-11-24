# Protocolo de prueba fisica — autonomo v5-carbon vs v3 (A/B)

Rama `feat/autonomo-v5-carbon`. **PENDIENTE DE EJECUTAR EN EL CARRO REAL.** Todo lo afirmado en el repositorio para `v3` y `v5-carbon` es `SIMULADO` y no sustituye la validacion fisica. Este protocolo comienza con mediciones estacionarias de riesgo cero y progresa por etapas: **no avances de bloque si el anterior no cumple su criterio o dispara una regla de aborto.**

Hoja de registro: [pruebas_fisicas_v5_plantilla.csv](pruebas_fisicas_v5_plantilla.csv) (una fila por medicion o corrida). Base historica de referencia: [protocolo_prueba_fisica_autonomo_v2.md](protocolo_prueba_fisica_autonomo_v2.md).

---

## 0. Reglas inviolables de seguridad en sesion

1. **Mano lista en el interruptor / STOP** durante cada corrida en movimiento. Ante cualquier riesgo de choque contra pared, mueble o persona, levantar el chasis o pulsar **STOP**.
2. **Abortar toda la sesion de inmediato si:**
   - El carro se mueve sin orden activa desde la app;
   - No se detiene al pulsar **STOP**;
   - Gira mas de una vuelta continua sobre si mismo;
   - En la prueba estatica frontal `P0`, el freno autonomo por laser no detiene antes de tocar (~12 cm en firmware `v1.1`/`SEG=2`, o ~15 cm en `SEG=3`/`SEG=4`);
   - La app cierra sesion dos veces seguidas.
3. **Proteccion electrica y de software:** tres fuentes separadas (USB #1 para ESP32 DevKit, 4×AA solo a `VDD` del `TC1508A` con tierra comun, USB #2 para camara). No desinstalar la app ni borrar datos; no usar `erase_flash`; conectar una sola placa USB por vez; no guardar imagenes reales de la camara.

---

## 1. Preparacion de APK y firmware (NO ejecutado por el asistente)

### 1.1 Instalacion de la APK conservando datos (`adb install -r`)

Desde PowerShell en la raiz del repositorio (`E:\Dev\fsi_proyecto`), compilar e instalar actualizando sobre la app existente:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/compilar_app.ps1
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r app_android/app/build/outputs/apk/debug/app-debug.apk
```

- **Si `adb install -r` falla por firma (`INSTALL_FAILED_UPDATE_INCOMPATIBLE`):** **DETENERSE.** No desinstalar la aplicacion para no perder rutas guardadas, calibracion ni configuracion; compilar o firmar con la misma llave de depuracion previa.
- Confirmar que **Más → Sensores** conserva la calibracion previa y muestra el nuevo interruptor **`Autónomo v5-carbon`** (encendido por defecto; apagarlo reproduce `v3`). Confirmar tambien `Autoridad del laser` y `Control de rumbo en rectas` (`RUMBO`) activos.

### 1.2 Firmware del carro `SEG=4` (OPCIONAL, requiere tu autorizacion expresa)

La app `v5-carbon` es retrocompatible con el firmware actual `v1.1` / `SEG=2` / `SEG=3`. El firmware `SEG=4` solo debe grabarse si deseas validar los filtros de `< 8 mm`, estado `>= 8000`/`rstat==4`, recuperacion I2C detenido y telemetria `TOFE?` (conserva `TOF_PARO_MM=150` y `TOF_BLOQUEO_MM=180`). La camara **no requiere regrabacion**.

```powershell
# Compilar y verificar (sin grabar)
.\scripts\cargar.ps1 nuevo -SoloCompilar

# Grabar solo placa del carro por USB (una placa a la vez, nunca erase_flash)
.\scripts\cargar.ps1 nuevo -Puerto COM6
python scripts/verificar_version.py --puerto COM6 --solo-fw
```

En **Más → Sensores**, confirmar que `CAPS` incluye `SEG=4,TUSO=1,TOFP=150`.

---

## 2. Mediciones baratas por cada mejora de v5 (`M-v5.1` a `M-v5.6`)

| ID | Mejora / Duda fisica | Bandera activa | Procedimiento rapido en piso real | Que anotar en CSV |
|---|---|---|---|---|
| `M-v5.1` | Hueco `bloque_sin_eco` (`v5_masa_sin_eco`, `v5_solido`) | `v5=ON` | Carro **detenido** a 35 cm frente a objeto oscuro/mate o botella transparente sobre el piso aprendido. Mirar 10 s `SENS` (`mm`, `bs`) y caja negra `VTEL` / `OBS_DIAG`. | Mediana ToF (`8190` sin eco o mm reales) y si color + masa solida marcan centro. |
| `M-v5.2` | Filtro `< 8 mm`, estado ToF y `TOFE?` (`SEG=4`) | Opcional `SEG=4` | Tapar brevemente sensor con cartulina mate a < 5 mm o apuntar al aire limpio con carro detenido; consultar sensores (`TOFE?` en terminal serial si se prueba en banco). | Si reporta `valida=0` sin congelarse ni frenar falso. |
| `M-v5.3` | Velocidad real y pivotes (`calibracion_v5`) | Independiente | Medir con cinta 150 cm en AUTO y giros 90° izquierda vs derecha (3 repeticiones). | Velocidad real (cm/s) y radio/desplazamiento lateral izquierdo vs derecho. |
| `M-v5.4` | Regla de oro `G1` en piso real | `v5=ON` vs `OFF` | Recta 150 cm en piso limpio con luz ambiente normal y luego con sombra o reflejo controlado (sin obstaculo). | `paradas_sin_motivo`, `esquivadas`, `causa_ayuda` y lineas `VTEL`. |
| `M-v5.5` | Deteccion de atasco (`NAV_CAUSA_ATASCO=20`) | `v5=ON` | Solo con chasis ligeramente retenido a mano o contra tope acolchado frente a pared a ~30 cm en AUTO con mano en STOP (> 2 s de `FORWARD` sin variacion > 20 mm en ToF). | Tiempo hasta `AYUDA Causa 20` (esperado ~2 s) y corte de motores. |
| `M-v5.6` | Tope de 3 continuaciones tras `AYUDA` | `v5=ON` | Con ruta en `AYUDA` y carro bloqueado sin avanzar >= 20 cm, pulsar Continuar 3 veces seguidas. | Al 3er intento sin avance debe pasar a `ERROR` (`Causa 20`) sin ciclarse. |

---

## 3. Bloques progresivos de campo (`P0` a `P4`)

Ejecutar en orden creciente de riesgo tras completar `Aprender piso` en el tramo despejado (umbral `40`, min celdas `7`, fila minima `44 %`, detector encendido, autoridad del laser encendida).

| Bloque | Prueba y escenario | Banderas | Repeticiones | Criterio de aceptacion | Criterio de aborto |
|---|---|---|---|---|---|
| **P0** (Riesgo cero/bajo) | Freno estatico frontal contra pared/caja rigida desde 80 cm en AUTO (**mano en STOP**) | `v5=ON` | 3 | Detiene sin tocar (~12 cm en v1.1 o ~15 cm en `SEG=4`). | Un solo contacto o fallo de frenado → **abortar sesion**. |
| **P1** (Riesgo bajo, `G1`) | `P1.1` Recta 150 cm piso limpio; `P1.2` Sombra cruzando; `P1.3` Mancha de luz a un lado y en linea; `P1.4` Pared 60 cm tras meta | `v5=ON` | 3 c/u | 100 % llega sin esquives ni `AYUDA` en piso limpio; `P1.4` cierra `TERMINADO` sin rodear la pared. | Cualquier esquive o `AYUDA` en piso limpio (`P1.1`). |
| **P2** (Riesgo medio, `G2`/`G3`) | `P2.1` Caja `20x20` centrada; `P2.2` Caja corrida 15 cm; `P2.3` Objeto bajo (< 6 cm alto); `P2.4` Bloque oscuro/mate o acrilico sin eco (`G3`) | `v5=ON` | 3 c/u | 0 contactos en `P2.1`–`P2.3`; en `P2.4` se detiene y rodea o pide `AYUDA` segura antes de tocar. | 2 contactos en la misma prueba o avance sostenido contra objeto sin eco. |
| **P3** (Riesgo medio-alto, `G5`) | `P3.1` Pasillo simetrico (~65 cm de ancho) con caja `20x20`; `P3.2` Caja `20x20` con pared lateral a 30–40 cm de la linea (1 m libre al otro lado). **Mano en STOP** | `v5=ON` | 3 c/u | En `P3.1` rodea sin tocar; en `P3.2` rodea por lado libre o detiene con `Causa 16/18` sin tocar pared. | Contacto con pared lateral o giro forzado contra la pared. |
| **P4** (Riesgo alto, `G4`) | Banquito con patas (`35x45 cm`) o silla con patas delgadas sobre recta 200 cm | `v5=ON` | 3 | Menos contactos que `v3` o fallo seguro (`AYUDA`) antes de empujar una pata; anotar si paso por debajo, rodeo o pidio ayuda. | 2 colisiones fuertes contra pata; levantar antes de enredar chasis. |

---

## 4. Comparativa controlada **«A/B contra v3»**

Para comparar objetivamente **`v3` contra `v5-carbon`** en el carro real bajo exactamente el mismo piso, luz, carga de pilas y calibracion:

1. **Mismo hardware y sesion:** usa la **misma APK `v5-carbon`**, sin cambiar firmware ni recalibrar pivotes/IMU entre bloques. Realiza primero `Aprender piso` en el sitio y anota el voltaje de las 4 AA (evita que caiga demasiado entre A y B; alterna pares `A-B` si vas a comparar 2 o mas repeticiones por prueba).
2. **Orden estricto emparejado (alternando `v3 -> v5 -> v3 -> v5` o pares consecutivos):**
   - **A (`v3`):** Apagar el interruptor **`Autónomo v5-carbon`** en **Más → Sensores** (la caja negra registrara `v5=0`). Correr la ruta.
   - **B (`v5-carbon`):** Encender **`Autónomo v5-carbon`** en **Más → Sensores** (la caja negra registrara `v5=1`). Correr exactamente la misma ruta y posicion de obstaculo medida con cinta.
3. **Bateria minima A/B recomendada antes de la exposicion (3 pares = 6 escenarios × 2 modos):**
   - `AB1` (`P1.1`): Recta 150 cm en piso limpio (3 corridas v3 + 3 corridas v5).
   - `AB2` (`P1.2`/`P1.3`): Mancha de luz o sombra controlada (2 + 2).
   - `AB3` (`P2.1`): Caja `20x20` centrada (3 + 3).
   - `AB4` (`P2.4`): Bloque oscuro/mate o transparente sin eco (3 + 3).
   - `AB5` (`P3.1` o `P3.2`): Pasillo ~65 cm o caja con pared lateral a 35 cm (3 + 3).
   - `AB6` (`P4`): Banquito con patas (3 + 3).

### Extraccion de cajas negras tras el A/B

Cada corrida queda guardada localmente en el telefono con la bandera `v5=1` o `v5=0` en la cabecera `CAB`. Extraer todo el directorio al terminar:

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" pull /sdcard/Android/data/com.fsi.carro/files/cajas_negras cajas_negras
```

Criterio para conservar `v5-carbon` encendido frente a `v3`: **0 esquives y 0 `AYUDA` en `AB1`/`AB2`**, **0 contactos en `AB3`**, mejoria neta sin empeorar seguridad en **`AB4`** (`bloque_sin_eco`) y **`AB5`/`AB6`**. Si **`v5-carbon` dispara cualquier falso positivo en `AB1`**, apagar el interruptor **`Autónomo v5-carbon`** en **Más → Sensores** para revertir al instante a `v3` (`e37f77b`).
