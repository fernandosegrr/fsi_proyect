# Lista de fotos y capturas del proyecto

Este catálogo define todas las fotografías reales del carro físico y las capturas de pantalla de la app evaluadas para la documentación y el manual del proyecto.

---

## 1. Fotografías físicas del carro y hardware

| ID | Archivo sugerido | Capítulo del manual | Qué debe verse y encuadre | Estado |
|---|---|---|---|---|
| `FOTO_01` | `foto_01_carro_frontal.jpg` | 4. Hardware y conexiones | **Vista frontal del carro.** Encuadre frontal mostrando ruedas Mecanum delanteras, sensor láser ToF VL53L0X al centro en la parte baja y cámara ESP32-WROVER vertical con LED verde encendido. | **DISPONIBLE** (1086×1448 px, 27-oct-2026) |
| `FOTO_02` | `foto_02_carro_lateral.jpg` | 4. Hardware y conexiones | **Vista lateral en perspectiva.** Perfil en tres cuartos mostrando chasis de acrílico de dos niveles con separadores de latón, ruedas Mecanum derechas, pack de baterías superior con cinta negra y luz roja interna. | **DISPONIBLE** (1448×1086 px, 27-oct-2026) |
| `FOTO_03` | `foto_03_carro_superior.jpg` | 4. Hardware y conexiones | **Vista superior cenital (planta).** Encuadre cenital completo mostrando las cuatro ruedas Mecanum, motores TT inferiores, powerbank trasera, portapilas central, ESP32 DevKit y sensor MPU6050. | **DISPONIBLE** (1086×1448 px, 27-oct-2026) |
| `FOTO_04` | `foto_04_carro_perspectiva.jpg` | 4. Hardware y conexiones | **Vista en perspectiva de tres cuartos.** Perspectiva exterior frontal izquierda del vehículo ensamblado exhibiendo cámara vertical, sensor ToF, sensor inercial MPU6050 y powerbank. | **DISPONIBLE** (1086×1448 px, 27-oct-2026) |
| `FOTO_05` | `foto_05_sensores_detalle.jpg` | 4. Hardware y conexiones | **Detalle de sensores I2C.** Primer plano del giroscopio MPU6050 (GY-521) fijado sólidamente al chasis y del sensor ToF láser VL53L0X orientado al frente sin obstrucciones mecánicas. | **PENDIENTE** (no incluida en esta versión) |
| `FOTO_06` | `foto_06_camara_freenove.jpg` | 6. Cámara ESP32 | **Placa de la cámara.** Detalle del módulo Freenove ESP32-WROVER v3.0, lente GC0308, alimentación por puerto micro-USB independiente y antena orientada adecuadamente. | **PENDIENTE** (no incluida en esta versión) |
| `FOTO_07` | `foto_07_portapilas_driver.jpg` | 4. Hardware y conexiones | **Alimentación de motores.** Portapilas con 4 pilas AA (~6 V), interruptor físico si lo hay, y cables rojo/negro ingresando a VDD y GND del driver TC1508A. | **PENDIENTE** (no incluida en esta versión) |
| `FOTO_08` | `foto_08_powerbank_montaje.jpg` | 4. Hardware y conexiones | **Powerbank y conexión USB.** Batería portátil USB de 5 V montada en el chasis, cable micro-USB al DevKit y cable independiente a la cámara. | **PENDIENTE** (no incluida en esta versión) |
| `FOTO_09` | `foto_09_escenario_obstaculo.jpg` | 14. Resultados y validación | **Montaje de pruebas de obstáculo.** Carro sobre pista o pasillo, cinta métrica extendida en el piso y caja de cartón de 20×20 cm ubicada en la trayectoria de prueba. | **PENDIENTE** (no incluida en esta versión) |

---

## 2. Capturas de pantalla de la aplicación Android

| ID | Archivo sugerido | Capítulo del manual | Qué debe verse y encuadre | Estado |
|---|---|---|---|---|
| `FOTO_10` | `foto_10_app_conexion.png` | 9. Aplicación Android | **Pestaña Inicio.** Pantalla principal en estado conectado mostrando tarjetas de estado ("Carro: Conectado, 14 ms, Lento" y "Cámara: Conectada, 40 ms, 12.5 fps"), botón "Volver a conectar" y accesos rápidos de ruta. | **DISPONIBLE** (1200×2640 px, 27-oct-2026) |
| `FOTO_11` | `foto_11_app_manual_video.png` | 9. Aplicación Android | **Pestaña Control Manual.** Botones direccionales (Adelante, Atrás, Izquierda, Derecha, Freno de emergencia), visualizador de video en vivo (MJPEG) y selector de perfil (Auto/Manual). | **PENDIENTE** (no incluida en esta versión) |
| `FOTO_12` | `foto_12_app_editor_rutas.png` | 9. Aplicación Android | **Pestaña Rutas.** Lienzo de dibujo con cuadrícula cartesiana de 20 cm, plantilla cuadrada de 5 waypoints y barra de herramientas de edición (deshacer, rehacer, borrar y menú). | **DISPONIBLE** (1200×2640 px, 27-oct-2026) |
| `FOTO_13` | `foto_13_app_semiautonomo.png` | 10. Visión y percepción | **Modo semiautónomo en ejecución.** Vista de navegación asistida mostrando el avance sobre la ruta, detección de obstáculos y estado del navegador nativo. | **PENDIENTE** (no incluida en esta versión) |
| `FOTO_14` | `foto_14_app_calibracion_recta.png` | 9. Aplicación Android | **Pantalla CalibracionActivity.** Asistente de calibración de avance recto y balance. | **PENDIENTE** (no incluida en esta versión) |
| `FOTO_15` | `foto_15_app_calibracion_giro.png` | 9. Aplicación Android | **Pantalla CalibrarGiroActivity.** Vista inicial de calibración del giroscopio con instrucciones de los 8 giros de prueba, botón de inicio y paro de emergencia (en reposo). | **DISPONIBLE** (1200×2640 px, 27-oct-2026) |
| `FOTO_16` | `foto_16_app_vision_deteccion.png` | 10. Visión y percepción | **Pantalla VisionActivity.** Vista de cámara con corredor trapezoidal de seguridad proyectado, máscara de color del piso y cuadro delimitador sobre obstáculo detectado. | **PENDIENTE** (no incluida en esta versión) |
| `FOTO_17` | `foto_17_app_ajustes_red.png` | 9. Aplicación Android | **Pantalla Ajustes / Red.** Campos de configuración de WebSocket (`ws://192.168.4.1/ws`) y cámara (`http://192.168.4.2/stream`) sin credenciales expuestas. | **DISPONIBLE** (1200×2640 px, 27-oct-2026) |

---

## 3. Resumen de integración

- **Total de elementos catalogados:** 17
- **Elementos DISPONIBLES en el manual:** 8 (4 fotografías del vehículo físico y 4 capturas de interfaz de la app).
- **Elementos no incluidos / PENDIENTES:** 9 (5 fotografías de hardware no provistas y 4 capturas de app omitidas por reglas de seguridad y preservación del sistema).
