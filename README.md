# Proyecto Carro FSI (LIDIA) — Vehículo con navegación asistida y visión (Versión final 1.1)

[![CI Tests](https://github.com/fernandosegrr/fsi_proyect/actions/workflows/ci.yml/badge.svg)](https://github.com/fernandosegrr/fsi_proyect/actions)
[![Versión 1.1](https://img.shields.io/badge/Versi%C3%B3n-final%201.1%20(J16)-blue.svg)](docs/estado_20261110.md)
[![Licencia MIT](https://img.shields.io/badge/Licencia-MIT-green.svg)](LICENSE)

**Institución:** Universidad de Guanajuato · División de Ingenierías · Campus Irapuato-Salamanca (DICIS)  
**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Profesor:** Dr. Horacio Rostro González  
**Integrantes del equipo:**
- Jaime Fernando Guerrero Segoviano
- Melanie Contreras Solis
- Vitto Eduardo Rangel Fortuna
- Azul Abril Cadena Campos

**Accesos rápidos del repositorio:**
- **Manual técnico completo en PDF (Versión final 1.1, 51 páginas):** [docs/manual/Manual_Proyecto_Carro_FSI.pdf](docs/manual/Manual_Proyecto_Carro_FSI.pdf)
- **Centro de documentación técnica:** [docs/README.md](docs/README.md) e [índice general](docs/index.md)
- **Artefactos listos para instalar (`dist/`):** [dist/carro_fsi_0.4-6d1b213.apk](dist/carro_fsi_0.4-6d1b213.apk), [dist/SHA256SUMS.txt](dist/SHA256SUMS.txt), [dist/LEEME.txt](dist/LEEME.txt) y [dist/FIRMWARE_LOCAL.txt](dist/FIRMWARE_LOCAL.txt)
- **Estado de validación y límites conocidos:** [docs/estado_20261110.md](docs/estado_20261110.md), [docs/limites_conocidos.md](docs/limites_conocidos.md) y [docs/entrega.md](docs/entrega.md)
- **Rama `feat/autonomo-v2` (14-nov-2026, sin fusionar; no es v1.2; SIMULADO, NO MEDIDO en el carro):** [estado](docs/estado_20261113.md), [auditoría](docs/auditoria_autonomo_v2.md), [diseño y resultados](docs/autonomo_v2_diseno.md), [protocolo de prueba física](docs/protocolo_prueba_fisica_autonomo_v2.md) y [plantilla CSV](docs/pruebas_fisicas_plantilla.csv)

---

## 1. Qué es el proyecto

**LIDIA (*Carro FSI*)** es un vehículo robótico terrestre de cuatro ruedas con tracción diferencial y rodillos tipo *mecanum*, gobernado en tiempo real por tres nodos cooperativos conectados mediante una red inalámbrica local (`CarroFSI`, 2.4 GHz):

1. **Placa de control y sensores (`ESP32 DevKit`, `192.168.4.1`):** actúa como punto de acceso WiFi, procesa los comandos entrantes mediante una cola determinista de 32 ranuras con prioridad absoluta para `STOP`/`EBRAKE`, controla los cuatro motores TT a través de un puente H doble `TC1508A` (`1 kHz`), integra un giroscopio `MPU6050` a `200 Hz` con factor de escala en partes por millón (`IMU_ESCALA` en NVS), supervisa la distancia frontal con un sensor láser `VL53L0X` (paro autónomo a `120 mm`) y ejecuta el controlador proporcional-integral de mantenimiento de rumbo en rectas (`RUMBO`) y giro cerrado (`GIRO`).
2. **Placa de cámara independiente (`Freenove ESP32-WROVER` con sensor `GC0308`, `192.168.4.2`):** alimentada por su propia *powerbank* USB independiente, captura imágenes en resolución `HQVGA` (`240×176` píxeles a `20 MHz`, `12.4 fps`, edad media de cuadro `137 ms`) mediante una arquitectura de doble núcleo con buzón de último cuadro, exponiendo flujo `MJPEG` (`:81/stream`), cuadro crudo `RGB565` (`:80/raw`) y rejilla compacta de celdas por `UDP :4210` (`1,244 bytes`).
3. **Teléfono Android (`APP:0.4-6d1b213`):** presenta la interfaz de operación y ejecuta de forma nativa vía `JNI` el núcleo matemático de navegación y visión en estándar `C99` (`nucleo_c`: planificación en cuadrícula de `20 cm`, integración de pose en décimas de grado `rumbo_x10`, guiado por punto de mira a `400 mm`, esquive en tres tramos con margen de `150 mm`, clasificador cromático `YCbCr`, inferencia `EfficientDet-Lite0` en hilo propio y fusión sincronizada bajo mutex `candado_vision`).

---

## 2. Estado de validación en la Versión final 1.1 (Cierre J16)

En estricto apego a la honestidad técnica, el repositorio separa lo validado físicamente en el carro, lo verificado en software/simulación y lo que permanece no medido:

### 2.1 VALIDADO EN EL CARRO (pruebas físicas)
- **Esquive autónomo con reincorporación (Escenario B6 — 10 de noviembre de 2026):** validado físicamente en el carro con el firmware `FW:6d1b213-20261110` y la aplicación `APP:0.4-6d1b213` ([docs/pruebas_fisicas_20261110.csv](docs/pruebas_fisicas_20261110.csv) y [docs/estado_20261110.md](docs/estado_20261110.md)):
  - **6 corridas físicas ejecutadas** (`J16_B6_r1` a `J16_B6_r6`).
  - **`0/6` toques o contactos con la caja** (`100 %` de seguridad anticolisión).
  - **`5/6` corridas autónomas completas (`TERMINADO`, `10/10` pasos)** con desviación lateral final respecto a la línea recta original de **`~0 cm` a `~10 cm` hacia la derecha** (`~10 cm`, `~10 cm`, `~5 cm`, `~0 cm` y `~0 cm`; **medidas aproximadas** observadas en pista).
  - **`1/6` corrida (`J16_B6_r5`) detenida de forma segura en estado `AYUDA`** (paso `4/10`) sin tocar la caja y con espacio físico suficiente, **sin causa identificada**.
- **Caracterización física de base (sesiones J13/J14):** balance `PWM` en perfil `MANUAL` (`252/255`) que redujo el desvío en recta de `150 cm` de `14.5 cm` a `3.0 cm`, tiempos reales de `loop()` (`~690 us` promedio) y cadencia de cámara `HQVGA` (`12.4 fps`, edad media `137 ms`).

### 2.2 VERIFICADO en software y SIMULADO en PC
- **Suites deterministas de software en PC (`100 %` en verde, `0` fallos):**
  - `make -C nucleo_c test`: **`22` programas y `1,655` casos de prueba** en `C99`.
  - `make -C pruebas_firmware test`: **`5` programas y `540` casos de prueba** + verificador de página embebida (`3/3` checks).
  - `python pruebas_firmware/probar_servidor_falso.py`: **`54` casos** de protocolo WebSocket.
  - `python scripts/probar_version.py`: **`18` casos** del verificador criptográfico de versiones.
  - `python docs/verificar_enlaces.py`: **`0` enlaces rotos** en toda la documentación.
- **Experimento Monte Carlo J16 (`SIMULADO`, [docs/precision/monte_carlo_1000.txt](docs/precision/monte_carlo_1000.txt)):** `1,000` corridas por escenario y configuración con semilla fija (`0xF51A2026`, `50,000` corridas en total):
  - Recta de `150 cm`: error mediano baja de `94 mm` (sin `RUMBO`) a **`8 mm`** (con `RUMBO`, éxito `100 %`).
  - Ruta en L (`60×60 cm`): error mediano baja de `64 mm` a **`36 mm`** (éxito `100 %`).
  - Cuadrado de `60 cm`: error mediano de cierre baja de `142 mm` a **`42 mm`** con `IMU_ESCALA` + `RUMBO` (éxito `98.4 %`).
  - Esquive `20×20 cm`: error mediano baja de `128 mm` (`4.8 %` colisiones en base J14) a **`38 mm`** con **`0.0 %` colisiones** y **`99.9 %` de éxito**.

### 2.3 NO MEDIDO en el carro físico
Permanecen como **`NO MEDIDO`** en el vehículo físico (contando únicamente con verificación en software o simulación):
- Rectas físicas de `150 cm` comparando `RUMBO:OFF` contra `RUMBO:ON` (`B5`).
- Ruta en L (`B7`) y cuadrado de `60 cm` (`B8`) con el cierre J16.
- Calibración física de vuelta completa (`360°`) en piso para `IMU_ESCALA`.
- Corte inducido de WiFi en plena marcha (`B9`) y serie aislada con cinta métrica del freno autónomo por láser (`TOF_PARO` a `120 mm`).
- Obstáculo ancho (`40×20 cm`), calibración automática (`B2`), circuito externo `GPIO 2` y consumo eléctrico en amperios.

---

## 3. Artefactos publicados en `dist/` y verificación rápida

| Artefacto | Versión interna | Tamaño | SHA-256 | Estado en el repositorio |
|---|---|---|---|---|
| [dist/carro_fsi_0.4-6d1b213.apk](dist/carro_fsi_0.4-6d1b213.apk) | `APP:0.4-6d1b213` | `14,881,402` bytes | `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F` | Publicado en `dist/` y validado en el carro (B6). |
| Firmware `ESP32 DevKit` | `FW:6d1b213-20261110` | `1,034,112` bytes | `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC` | Registrado en [dist/FIRMWARE_LOCAL.txt](dist/FIRMWARE_LOCAL.txt) (el `.bin` no se publica por contener credenciales WiFi). |

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

1. **Encender con el carro inmóvil:** coloque el chasis sobre el piso plano y nivelado sin tocarlo durante el primer segundo tras conectar la **Powerbank USB #1** (para que `CAL_IMU` calcule el sesgo del giroscopio en reposo). Después encienda la **Powerbank USB #2** de la cámara y el interruptor del portapilas `4×AA`.
2. **Conectar el teléfono:** asocie el teléfono Android a la red WiFi `CarroFSI` (desactive datos móviles o acepte mantener la conexión sin internet).
3. **Verificar enlace y calibración:** abra la aplicación `LIDIA`, confirme en la pestaña **Inicio** que el carro (`192.168.4.1`) y la cámara (`192.168.4.2`) aparezcan **Conectados**, verifique en **Más → Sensores** que `Control de rumbo en rectas` (`RUMBO`) esté activo y en **Más → Visión** pulse **Aprender piso** sobre el tramo despejado antes de ejecutar una ruta.

---

## 5. Reproducir las pruebas en PC y regenerar el manual en PDF

```powershell
# Ejecutar suites en C99 (núcleo y firmware)
make -C nucleo_c test
make -C pruebas_firmware test

# Ejecutar pruebas de protocolo, verificador de versión y enlaces
python pruebas_firmware\probar_servidor_falso.py
python scripts\probar_version.py
python docs\verificar_enlaces.py

# Regenerar las 12 figuras/láminas y el Manual Técnico v1.1 en PDF
python docs\manual\generar_manual.py
```

---

## 6. Licencia

Este proyecto se distribuye bajo la **Licencia MIT** (2026). Consulte el archivo [LICENSE](LICENSE) para el texto legal íntegro.
