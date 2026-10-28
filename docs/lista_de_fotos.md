# Lista de fotos y capturas pendientes

Este catálogo define todas las fotografías reales del carro físico y las capturas de pantalla de la app requeridas para la documentación completa y el manual.

Cuando una foto se tome y se guarde en `docs/imagenes/` con su nombre exacto, el generador del manual en PDF la incorporará automáticamente en sustitución del recuadro de aviso.

---

## 1. Fotografías físicas del carro y hardware

| ID | Archivo sugerido | Capítulo del manual | Qué debe verse y encuadre | Estado |
|---|---|---|---|---|
| `FOTO_01` | `foto_01_carro_frontal.jpg` | 3. El proyecto en una página | **Vista frontal completa del carro.** Encuadre a nivel del chasis mostrando las ruedas delanteras, el sensor láser VL53L0X al centro, la cámara Freenove apuntando al frente y la sujeción del soporte. | **PENDIENTE** |
| `FOTO_02` | `foto_02_carro_lateral.jpg` | 5. Hardware y cableado | **Vista lateral del carro.** Perfil completo del vehículo mostrando chasis TT de dos niveles, llantas, ubicación de la powerbank en el piso inferior y soporte del teléfono. | **PENDIENTE** |
| `FOTO_03` | `foto_03_carro_superior.jpg` | 5. Hardware y cableado | **Vista superior (planta).** Encuadre cenital que muestre la distribución espacial completa: ESP32 DevKit, sensor MPU6050, driver TC1508A, cables de interconexión y portapilas. | **PENDIENTE** |
| `FOTO_04` | `foto_04_electronica_abierta.jpg` | 5. Hardware y cableado | **Electrónica abierta con rótulos.** Detalle nítido de la placa ESP32 DevKit, el módulo driver TC1508A, el bus I2C común y la clema/soldadura de tierras compartidas. | **PENDIENTE** |
| `FOTO_05` | `foto_05_sensores_detalle.jpg` | 5. Hardware y cableado | **Detalle de sensores I2C.** Primer plano del giroscopio MPU6050 (GY-521) fijado sólidamente al chasis y del sensor ToF láser VL53L0X orientado al frente sin obstrucciones mecánicas. | **PENDIENTE** |
| `FOTO_06` | `foto_06_camara_freenove.jpg` | 10. Cámara ESP32 | **Placa de la cámara.** Detalle del módulo Freenove ESP32-WROVER v3.0, lente GC0308, alimentación por puerto micro-USB independiente y antena orientada adecuadamente. | **PENDIENTE** |
| `FOTO_07` | `foto_07_portapilas_driver.jpg` | 5. Hardware y cableado | **Alimentación de motores.** Portapilas con 4 pilas AA (~6 V), interruptor físico si lo hay, y cables rojo/negro ingresando a VDD y GND del driver TC1508A. | **PENDIENTE** |
| `FOTO_08` | `foto_08_powerbank_montaje.jpg` | 5. Hardware y cableado | **Powerbank y conexión USB.** Batería portátil USB de 5 V montada en el chasis, cable micro-USB al DevKit y cable independiente a la cámara. | **PENDIENTE** |
| `FOTO_09` | `foto_09_escenario_obstaculo.jpg` | 15. Resultados y validación | **Montaje de pruebas de obstáculo.** Carro sobre pista o pasillo, cinta métrica extendida en el piso y caja de cartón de 20×20 cm ubicada en la trayectoria de prueba. | **PENDIENTE** |

---

## 2. Capturas de pantalla de la aplicación Android

| ID | Archivo sugerido | Capítulo del manual | Qué debe verse y encuadre | Estado |
|---|---|---|---|---|
| `FOTO_10` | `foto_10_app_conexion.png` | 9. Aplicación Android | **Pestaña Inicio.** Pantalla principal con tarjeta de conexión IP (192.168.4.1), estado conectado, latencia de ida y vuelta (ping), y lectura de telemetría de sensores. | **PENDIENTE** |
| `FOTO_11` | `foto_11_app_manual_video.png` | 9. Aplicación Android | **Pestaña Control Manual.** Botones direccionales (Adelante, Atrás, Izquierda, Derecha, Freno de emergencia), visualizador de video en vivo (MJPEG) y selector de perfil (Auto/Manual). | **PENDIENTE** |
| `FOTO_12` | `foto_12_app_editor_rutas.png` | 8. Núcleo de navegación | **Pestaña Rutas.** Lienzo de dibujo de trayectoria con cuadrícula de 20 cm, waypoints marcados, botón de simular y botón de ejecutar ruta. | **PENDIENTE** |
| `FOTO_13` | `foto_13_app_semiautonomo.png` | 12. Operación y modos | **Modo semiautónomo en ejecución.** Vista de navegación asistida mostrando el avance sobre la ruta, detección de obstáculos y estado del navegador nativo. | **PENDIENTE** |
| `FOTO_14` | `foto_14_app_calibracion_recta.png` | 13. Calibración | **Pantalla CalibracionActivity.** Asistente de calibración de avance recto, medición con cinta métrica, desvío lateral y cálculo de factor de balance guardado en EEPROM. | **PENDIENTE** |
| `FOTO_15` | `foto_15_app_calibracion_giro.png` | 13. Calibración | **Pantalla CalibrarGiroActivity.** Prueba de giro a 90 grados, comparación entre ángulo comandado y medido por giroscopio, y factor ms/grado resultante. | **PENDIENTE** |
| `FOTO_16` | `foto_16_app_vision_deteccion.png` | 10. Cámara ESP32 | **Pantalla VisionActivity.** Vista de cámara con corredor trapezoidal de seguridad proyectado, máscara de color del piso y cuadro delimitador sobre obstáculo detectado. | **PENDIENTE** |
| `FOTO_17` | `foto_17_app_ajustes_red.png` | 11. Puesta en marcha | **Pantalla Ajustes / Red.** Parámetros de configuración: IP del carro (192.168.4.1), URL de la cámara (http://192.168.4.2/stream), tiempos de timeout y modo de transporte de video. | **PENDIENTE** |

---

## 3. Instrucciones para la actualización

1. Tome la fotografía o realice la captura de pantalla con el teléfono.
2. Guarde la imagen en la carpeta `docs/imagenes/` respetando el nombre exacto de la columna **Archivo sugerido** (por ejemplo, `docs/imagenes/foto_01_carro_frontal.jpg`).
3. En este archivo `docs/lista_de_fotos.md`, cambie la columna de **Estado** de `PENDIENTE` a `DISPONIBLE`.
4. Ejecute el script generador de manual:
   ```powershell
   python docs/manual/generar_manual.py
   ```
5. El generador incrustará la imagen en el capítulo y página correspondientes.
