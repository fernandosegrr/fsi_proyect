# Proyecto Carro FSI (LIDIA) — Vehículo con navegación asistida y visión (v1.2.1 — autónomo v3 con correcciones del 26-nov; validación física parcial)

[![CI Tests](https://github.com/fernandosegrr/fsi_proyect/actions/workflows/ci.yml/badge.svg)](https://github.com/fernandosegrr/fsi_proyect/actions)
[![Versión 1.2.1](https://img.shields.io/badge/Versi%C3%B3n-1.2.1%20(v3%20%2B%2026--nov)-blue.svg)](docs/correcciones_fisicas_20261126.md)
[![Licencia MIT](https://img.shields.io/badge/Licencia-MIT-green.svg)](LICENSE)

## Para abrir el manual

- **Manual en PDF (v1.2.1, 137 páginas):** [docs/manual/Manual_Proyecto_Carro_FSI.pdf](docs/manual/Manual_Proyecto_Carro_FSI.pdf)
- **Guía de exposición (guion de 3 a 5 minutos, preguntas probables, qué decir si algo falla):** [docs/guia_exposicion.md](docs/guia_exposicion.md)
- **Versión y estado:** v1.2.1, autónomo v3 con correcciones del 26-nov (`FW:b396390`, `SEG=3`, `TOFP=150`, `TOF_BLOQUEO=180`), validación física parcial (25–26 nov-2026); lo histórico de v1.1 (10-nov-2026) está en [estado](docs/estado_20261110.md) y no es respaldo seguro.
- **Lo que el equipo todavía debe confirmar:** [docs/pendientes_confirmar.md](docs/pendientes_confirmar.md).

**Institución:** Universidad de Guanajuato · División de Ingenierías · Campus Irapuato-Salamanca (DICIS)  
**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Profesor:** Dr. Horacio Rostro González  
**Integrantes del equipo:**
- Jaime Fernando Guerrero Segoviano
- Melanie Contreras Solis
- Vitto Eduardo Rangel Fortuna
- Azul Abril Cadena Campos

**Accesos rápidos del repositorio:**
- **Manual técnico completo en PDF (v1.2.1, 137 páginas):** [docs/manual/Manual_Proyecto_Carro_FSI.pdf](docs/manual/Manual_Proyecto_Carro_FSI.pdf) (el manual de la v1.1 ya no está en el árbol: se conserva en la etiqueta `v1.1`, ruta `docs/manual/Manual_Proyecto_Carro_FSI.pdf`, solo de referencia)
- **Centro de documentación técnica:** [docs/README.md](docs/README.md) e [índice general](docs/index.md)
- **Artefactos de la v1.1 en `dist/` (la v1.2 no publica binarios nuevos):** [dist/carro_fsi_0.4-6d1b213.apk](dist/carro_fsi_0.4-6d1b213.apk), [dist/SHA256SUMS.txt](dist/SHA256SUMS.txt), [dist/LEEME.txt](dist/LEEME.txt) y [dist/FIRMWARE_LOCAL.txt](dist/FIRMWARE_LOCAL.txt)
- **Estado de validación y límites conocidos:** [docs/estado_20261110.md](docs/estado_20261110.md), [docs/limites_conocidos.md](docs/limites_conocidos.md) y [docs/entrega.md](docs/entrega.md)
- **Autónomo v2 (14-nov-2026; ya incluido en la v1.2 a través de v3; SIMULADO, NO MEDIDO en el carro):** [estado](docs/estado_20261113.md), [auditoría](docs/auditoria_autonomo_v2.md), [diseño y resultados](docs/autonomo_v2_diseno.md), [protocolo de prueba física](docs/protocolo_prueba_fisica_autonomo_v2.md) y [plantilla CSV](docs/pruebas_fisicas_plantilla.csv)
- **Autónomo v3 (16-nov-2026; base de la v1.2; SIMULADO, NO MEDIDO en el carro; firmware y app solo compilados):** [estado](docs/estado_20261116.md), [investigación (11 áreas, fuentes con confianza)](docs/investigacion_autonomo/00_mapa.md), [hallazgos de la revisión](docs/investigacion_autonomo/90_hallazgos.md), [límites](docs/limites_conocidos.md) y [bloque de mediciones baratas M1–M10](docs/protocolo_prueba_fisica_autonomo_v2.md#10-bloque-m--mediciones-baratas-v3).
- **Revisión de la interfaz de la app Android (24-nov-2026; integrada en la v1.2; probada en un teléfono con carro y cámara falsos, falta el carro real):** [revisión: parpadeos, causas, arreglos y cómo comprobarlos](docs/revision_ui_android.md) y [reglas anti-parpadeo del sistema de diseño](docs/diseno.md)
- **Ramas y etiquetas de archivo, NO integradas en la v1.2:** v4 (`archive/feat-autonomo-v4-a200df4`), v5-carbon (`freeze/v5-carbon`, `freeze/v5-carbon-fix`) y v6 (`freeze/v6`); por qué no se usaron está en el subsistema I del manual.

---

## 1. Qué es el proyecto

**LIDIA (*Carro FSI*)** es un vehículo robótico terrestre de cuatro ruedas con tracción diferencial y rodillos tipo *mecanum*, gobernado en tiempo real por tres nodos cooperativos conectados mediante una red inalámbrica local (`CarroFSI`, 2.4 GHz):

1. **Placa de control y sensores (`ESP32 DevKit`, `192.168.4.1`):** actúa como punto de acceso WiFi, procesa los comandos entrantes mediante una cola determinista de 32 ranuras en la que `STOP`/`EBRAKE` nunca se descartan, controla los cuatro motores TT a través de un puente H doble `TC1508A` (`1500 Hz`), integra un giroscopio `MPU6050` a `100 Hz` con factor de escala en partes por millón (`IMU_ESCALA` en NVS), supervisa la distancia frontal con un sensor láser `VL53L0X` (paro autónomo a `120 mm` en el firmware v1.1 del carro y `150 mm` en el firmware v3, solo compilado) y ejecuta el controlador proporcional-integral de mantenimiento de rumbo en rectas (`RUMBO`) y giro cerrado (`GIRO`).
2. **Placa de cámara independiente (`Freenove ESP32-WROVER` con sensor `GC0308`, `192.168.4.2`):** **comparte la misma *powerbank* USB del DevKit**, captura imágenes en resolución `HQVGA` (`240×176` píxeles a `20 MHz`, `12.4–12.5 fps`, edad media de cuadro `137 ms`) mediante una arquitectura de doble núcleo con buzón de último cuadro, exponiendo flujo `MJPEG` (`:80/stream`), cuadro crudo `RGB565` (`:80/raw`) y rejilla compacta de celdas por `UDP :4210` (`1,244 bytes`).
3. **Teléfono Android (`APP:0.4-6d1b213`):** presenta la interfaz de operación y ejecuta de forma nativa vía `JNI` el núcleo matemático de navegación y visión en estándar `C99` (`nucleo_c`: planificación en cuadrícula de `20 cm`, integración de pose en décimas de grado `rumbo_x10`, guiado por punto de mira a `400 mm`, esquive en tres tramos con margen de `150 mm`, clasificador cromático `YCbCr`, inferencia `EfficientDet-Lite0` en hilo propio y fusión sincronizada bajo mutex `candado_vision`).

---

## 2. Estado de validación (pruebas físicas del 25–26 nov-2026 + histórico v1.1)

La **v1.2.1** incorpora las correcciones físicas del 26-nov en `nucleo_c/navegador.c` y `ManualFragment.kt`, junto con las mediciones y pruebas físicas ejecutadas el 25–26 de noviembre de 2026 (`FW:b396390`, `SEG=3`, `TOFP=150`, `TOF_BLOQUEO=180`, cámara `cam-v2`, app `0.4-b396390` + correcciones). Las corridas posteriores a las correcciones se presentan como **con las correcciones del 26-nov; mismos escenarios donde ocurrió la falla; `n` pequeño**, no como validación independiente.

### 2.1 VALIDADO EN EL CARRO (pruebas físicas del 25–26 nov-2026 e histórico v1.1)
- **Pruebas físicas 25–26 nov-2026 (`FW:b396390`, `cam-v2`, app `0.4-b396390` + correcciones del 26-nov; ver [docs/correcciones_fisicas_20261126.md](docs/correcciones_fisicas_20261126.md) y capítulo 12.8 del manual):**
  - **Línea base recta 2 m (`IMU+RUMBO`, `n=3`):** `3/3` OK, `186 cm`, `28.5 cm/s`, deriva `0 / 11 / 0 cm`, `0` falsos positivos (2 corridas previas sin IMU por cable SCL roto derivaron `+43 cm`).
  - **M8 AUTO (`n=5`):** `5/5`, `140 cm` en `4.82–4.91 s`, media `28.7 cm/s`, deriva `0 cm` en `5/5`.
  - **M5 frenado láser (`TOFP=150`, `n=10`):** `10/10` sin contacto, `TOF_PARO` media `144.7 mm`, margen del chasis `8.0–10.0 cm` (media `9.1 cm`), desde la punta del láser `4.5–6.5 cm` (media `5.6 cm`).
  - **M6 bloqueo (`TOF_BLOQUEO=180`, `n=3`/distancia):** a `20` y `18 cm` la primera corrida avanzó `355–415 ms` sin tocar y frenó a `9–10 cm`, las otras bloqueo instantáneo (`6–19 ms`); a `16 cm`, `3/3` bloqueo instantáneo. Sesgo positivo del `VL53L0X` de `+40` a `+51 mm` (`+67` a `+102 mm` con tela negra mate).
  - **M1 deriva del giroscopio (`n=1`):** `0.0°` en `0, 1, 2, 5 y 10 min` quieto; `0.0°` tras `32.3 s` acumulados en AUTO (`10.0 Hz`).
  - **M3 pared lateral a 30 cm (`n=20`/ángulo):** `0°` `100 %` libre; `~20°` `85 %` libre; `~31°`, `~43°` y `~61°` `0 %` libre (detectada).
  - **Mochila negra mate (42×14 cm):** estático `20–80 cm` (`n=100`), `100/100` con eco sólido. Dinámico con `main` original (`n=3`): r1 sin tocar, r2 `AYUDA` causa 14, r3 fallo grave con contacto. Dinámico **con correcciones del 26-nov** (mismo escenario, `n=3`): `3/3` terminado, `0` contactos, `0` `AYUDA`, reincorporación a `1–2 cm` de la línea.
  - **Objetos bajos (2 y 6 cm):** láser los vio en `20/20` a `40 cm` (`aprenderPiso` rechaza a `40 cm`: `18 % > 5 %`). Dinámicos a `90 cm`: `2 cm` (`n=3`) `3/3` terminado, `0` contactos; `6 cm` (`n=3`): r1 `AYUDA` causa 2 sin contacto (antes de corrección), r2 y r3 terminados con `0` contactos (con corrección del 26-nov).
  - **Piso limpio 1.5 m (`n=5`):** `5/5` terminado, `0/5` falsos positivos, `0` paradas falsas, `0` esquives; recorrió `133–139 cm` de `150` por desgaste de pilas (recalibración `ms_por_cm_x10=355` aplicada después, sin repetirlas).
- **Histórico v1.1 (10-nov-2026, `FW:6d1b213-20261110`, `APP:0.4-6d1b213`):** esquive B6 (`6/6` sin contacto, `5/6` completas; `1/6` en `AYUDA`). **v1.1 no es respaldo seguro**: su paro a `120 mm` deja solo `1.5–3.5 cm` de margen.

### 2.2 VERIFICADO en software y SIMULADO en PC
- **Suites deterministas de software en PC (`100 %` en verde, `0` fallos):**
  - `make -C nucleo_c test`: **`24` programas y `1,901` casos de prueba** en `C99` (cifra de v1.2).
  - `make -C pruebas_firmware test`: **`5` programas y `567` casos de prueba** + verificador de página embebida (el archivo coincide con `pagina.sha256`).
  - `python probar_servidor_falso.py` (desde `pruebas_firmware/`, con `websockets` instalado): **`60` casos** de protocolo WebSocket.
  - `python scripts/probar_version.py`: **`21` casos** del verificador criptográfico de versiones.
  - `python docs/verificar_enlaces.py`: **`0` enlaces rotos** en toda la documentación.
- **Experimento Monte Carlo J16 (`SIMULADO`, [docs/precision/monte_carlo_1000.txt](docs/precision/monte_carlo_1000.txt)):** `1,000` corridas por escenario y configuración con semillas fijas (`160000 + índice del escenario × 10007 + corrida`, la misma en todas las configuraciones; corregido en v1.2: la constante `0xF51A2026` que citaba la v1.1 no existe en el código; `50,000` corridas en total):
  - Recta de `150 cm`: la mediana del error lateral final baja de `137 mm` (base J14, solo por tiempo) a **`9 mm`** con `RUMBO`; las `1,000` corridas llegan a la meta, sin toques.
  - Ruta en L (`60×60 cm`): la mediana del error de posición final baja de `145 mm` a **`31 mm`** con `RUMBO` e `IMU_ESCALA`; `1,000` de `1,000` llegan, sin toques.
  - Cuadrado de `60 cm`: la mediana del error de cierre baja de `65 mm` a **`19 mm`** con `RUMBO` e `IMU_ESCALA`; `1,000` de `1,000` llegan, sin toques.
  - Esquive `20×20 cm`: la mediana del error lateral final baja de `200 mm` (con `49` toques a la caja en `1,000` corridas, base J14) a **`16 mm`** con **`0` toques**.
  - Corregido en v1.2: las cifras que traían el README y el manual v1.1 (`94 → 8 mm`, `64 → 36 mm`, `142 → 42 mm`, `128 → 38 mm` y porcentajes de éxito) no aparecen en ese archivo; las de arriba sí salen de él (mediana `p50` de `error_mm`). Son `SIMULADO`.

### 2.3 NO MEDIDO o PENDIENTE en el carro físico
Permanecen como **`NO MEDIDO`**, **`SIMULADO`** o **`PENDIENTE`** en el vehículo físico:
- M8 en modo Manual (límite de seguridad) y prueba física de `RUMBO:ON` en modo Manual (**PENDIENTE**).
- M2 (patas reales; sigue **SIMULADO** en banco) y objeto con `0 %` de eco IR (**SIMULADO**).
- Bloques C y D (luz cambiante, ruta en L, cuadrado), M4, M7, M9 y corte inducido de WiFi en marcha (`B9`).
- Calibración física de vuelta completa (`360°`) en piso para `IMU_ESCALA`, inclinación/FOV de cámara, inclinación del láser y consumo eléctrico en amperios. En `GPIO 2` no hay nada conectado.

---

## 3. Artefactos publicados en `dist/` y verificación rápida

| Artefacto | Versión interna | Tamaño | SHA-256 | Estado en el repositorio |
|---|---|---|---|---|
| [dist/carro_fsi_0.4-6d1b213.apk](dist/carro_fsi_0.4-6d1b213.apk) | `APP:0.4-6d1b213` | `14,881,402` bytes | `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F` | Publicado en `dist/` y validado en el carro (B6). |
| Firmware `ESP32 DevKit` | `FW:6d1b213-20261110` | `1,034,112` bytes | `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC` | Registrado en [dist/FIRMWARE_LOCAL.txt](dist/FIRMWARE_LOCAL.txt) (el `.bin` no se publica por contener credenciales WiFi). |

> **Nota v1.2:** la APK de `dist/` y el firmware registrado son los de la **v1.1** (`APP:0.4-6d1b213`, `FW:6d1b213-20261110`), que son los que se validaron en el carro. La v1.2 (autónomo v3 y revisión de interfaz) **solo se compiló** (app con `scripts/compilar_app.ps1`; firmware del carro y de la cámara con `arduino-cli`): no se publicó ningún binario nuevo ni se instaló nada en el carro.

Para comprobar el hash de la APK e instalarla en un teléfono Android por `ADB` conservando la calibración guardada:

```powershell
# 1. Verificar hash y metadatos de versión del archivo APK
python scripts\verificar_version.py --solo-archivo --apk dist\carro_fsi_0.4-6d1b213.apk --revision 6d1b213

# 2. Instalar en el teléfono (conservando preferencias con -r)
adb install -r dist\carro_fsi_0.4-6d1b213.apk
```

---

## 4. Cómo conectar y operar el sistema de forma segura

> **REGLA DE PROTECCIÓN ELÉCTRICA:** El vehículo utiliza **tres fuentes de alimentación separadas**: (1) **Powerbank USB #1** para la placa `ESP32 DevKit` y sensores `I2C`, (2) **Portapilas de 4 pilas AA** conectado **exclusivamente al pin `VDD` del puente H `TC1508A`** (con tierra común `GND` unida a la `ESP32 DevKit`), y (3) **Powerbank USB #2 exclusiva para la cámara `Freenove`**. Jamás conecte el positivo de las pilas AA a la `ESP32`.

1. **Encender con el carro inmóvil:** coloque el chasis sobre el piso plano y nivelado sin tocarlo durante el primer segundo tras conectar la **Powerbank USB #1** (para que `IMU_CAL` calcule el sesgo del giroscopio en reposo). Después encienda la **Powerbank USB #2** de la cámara y el interruptor del portapilas `4×AA`.
2. **Conectar el teléfono:** asocie el teléfono Android a la red WiFi `CarroFSI` (desactive datos móviles o acepte mantener la conexión sin internet).
3. **Verificar enlace y calibración:** abra la aplicación `LIDIA`, confirme en la pestaña **Inicio** que el carro (`192.168.4.1`) y la cámara (`192.168.4.2`) aparezcan **Conectados**, verifique en **Más → Sensores** que `Control de rumbo en rectas` (`RUMBO`) esté activo y en **Más → Visión** pulse **Aprender piso** sobre el tramo despejado antes de ejecutar una ruta.

---

## 5. Reproducir las pruebas en PC y regenerar el manual en PDF

```powershell
# Ejecutar suites en C99 (núcleo y firmware)
make -C nucleo_c test
make -C pruebas_firmware test

# Ejecutar pruebas de protocolo (desde pruebas_firmware, con el entorno .venv que trae websockets), verificador de versión y enlaces
Push-Location pruebas_firmware
.venv\Scripts\python.exe probar_servidor_falso.py
Pop-Location
python scripts\probar_version.py
python docs\verificar_enlaces.py

# Regenerar el Manual Técnico v1.2 en PDF reutilizando las figuras (ver docs\manual\README.md)
python docs\manual\revisar_manual.py plantilla
python docs\manual\generar_manual.py --sin-figuras
```

---

## 6. Licencia

Este proyecto se distribuye bajo la **Licencia MIT** (2026). Consulte el archivo [LICENSE](LICENSE) para el texto legal íntegro.
[Estado de v3 pendiente de pruebas físicas](docs/estado_v3_pendiente_pruebas.md).
