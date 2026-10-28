# Pendientes y observaciones de la documentación

**Fecha de recopilación:** 27 de octubre de 2026  
**Rama:** `docs/documentacion-completa`  
**Regla de preservación:** No se modifica el código fuente ni los documentos históricos; cada contradicción o aspecto pendiente se anota aquí con su archivo y línea.

---

## 1. Contradicciones detectadas en la documentación previa

| Archivo y línea | Texto contradictorio | Situación real comprobada en código | Acción documental tomada |
|---|---|---|---|
| `docs/limites_conocidos.md:3` | Indica que el vehículo opera *«sin encoders, IMU, lidar»*. | **Obsoleto.** En la jornada J11 se integraron un giroscopio MPU6050 (I2C 0x68) y un sensor láser de distancia VL53L0X (I2C 0x29). | Se mantiene el documento histórico original intacto y se aclara en los nuevos documentos técnicos (`hardware.md`, `firmware.md`, `nucleo.md` y manual) que el carro sí cuenta con giroscopio y láser ToF. |
| `docs/camara_prueba.md:120` | Concluye que *«se queda QVGA como valor por defecto»*. | **Contradicción con código vigente.** En `camara_prueba/camara_prueba.ino:107`, la configuración por defecto es `CONFIG_DEFECTO = { 20, PIXFORMAT_RGB565, FRAMESIZE_HQVGA, 50 }` (resolución HQVGA 240×176). | Se documenta HQVGA como valor vigente según el código de producción. Se anota en la referencia técnica de la cámara (`docs/referencia/camara.md:100`). |
| Múltiples documentos históricos en `docs/` | Se refieren al proyecto como *«WALL-E»*. | **Cambio de denominación.** El nombre formal del proyecto es **«el carro»** o **«carro FSI»**. | Se preservan los reportes y notas históricas sin alterar; toda la documentación vigente, READMEs y manual utilizan «el carro» o «carro FSI». |

---

## 2. Decisiones de autoría y gobernanza pendientes

| Asunto | Descripción del pendiente | Acción requerida por el equipo |
|---|---|---|
| **Integrantes del equipo** | Se incorporaron los 4 integrantes del equipo (Jaime Fernando Guerrero Segoviano, Melanie Contreras Solis, Vitto Eduardo Rangel Fortuna, Azul Abril Cadena Campos) en README, portada del manual y documentación. | Resuelto el 27 de octubre de 2026. |
| **Licencia del repositorio** | Se incorporó la licencia formal MIT con titular Jose Ruben Guerrero Esparza (año 2026) en archivo `LICENSE` y referencia en README. | Resuelto el 27 de octubre de 2026. |

---

## 3. Aspectos físicos y de hardware pendientes de confirmar

| Archivo de referencia | Parámetro o elemento | Duda técnica | Impacto |
|---|---|---|---|
| `firmware_carro/config.h:148` | `PIN_KEEPALIVE 2` | Señal de mantenimiento en GPIO 2 configurada en firmware para emitir un pulso periódico. | No verificado físicamente (polaridad lógica y circuito no determinados). |
| `firmware_carro/config.h:96, 126` | `PERFIL_AUTO_BALANCE_DEFECTO` | El balance de motores para el perfil AUTO (207 motor izquierdo, 210 motor derecho) se calculó escalando el balance validado de 255. Está **NO MEDIDO** en pista física. | El carro podría presentar una ligera deriva lateral acumulada al avanzar a baja velocidad en modo autónomo. |
| Chasis y consumo | Corriente total pico del sistema | El consumo con los cuatro motores bloqueados y la transmisión WiFi simultánea no se ha medido con osciloscopio o multímetro (**NO MEDIDO**). | Recomendación de usar pilas alcalinas nuevas de buena calidad para evitar caídas de tensión en el driver. |

---

## 4. Estado de fotografías y capturas de pantalla

De las 17 imágenes catalogadas en `docs/lista_de_fotos.md`, 8 imágenes están **DISPONIBLES** (4 fotos del vehículo físico y 4 capturas de la app Android) y 9 permanecen **no incluidas en esta versión**:
- 4 fotografías del carro disponibles: `FOTO_01` (frontal), `FOTO_02` (lateral en tres cuartos), `FOTO_03` (superior cenital) y `FOTO_04` (perspectiva de tres cuartos), procesadas a JPEG sin metadatos EXIF.
- 4 capturas de pantalla de la app Android disponibles: `FOTO_10` (inicio conectada), `FOTO_12` (editor de rutas con cuadrícula), `FOTO_15` (calibración de giro en reposo) y `FOTO_17` (ajustes de red).
- 5 fotografías de hardware pendientes (`FOTO_05` detalle sensores, `FOTO_06` cámara Freenove, `FOTO_07` portapilas, `FOTO_08` powerbank, `FOTO_09` escenario de pruebas).
- 4 capturas de app no incluidas: `FOTO_11` (control manual con video), `FOTO_13` (semiautónomo en ejecución), `FOTO_14` (calibración de avance; descartada por capturar el panel de control del sistema) y `FOTO_16` (visión/detección).



---

## 5. Código fuente: comentarios o documentación interna mejorable

| Archivo y línea | Hallazgo | Sugerencia para futura versión de código |
|---|---|---|
| `camara_prueba/camara_prueba.ino:105` | El comentario explicativo de `CONFIG_DEFECTO` detalla la justificación de HQVGA, pero no menciona que contradice la sección histórica de `docs/camara_prueba.md`. | Agregar una nota cruzada en el comentario del encabezado indicando la versión de firmware donde se adoptó HQVGA. |
| `firmware_carro/carro.c:389` | `carro_encolar_comando()` no documenta explícitamente en el cuerpo de la función el criterio de descarte selectivo de comandos `PING` cuando la cola se satura. | El mecanismo está probado exhaustivamente en `pruebas_carro.c`, pero convendría añadir un comentario explicativo directo en `carro.c`. |
