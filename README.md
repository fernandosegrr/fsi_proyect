# Proyecto Carro FSI — Vehículo autónomo con navegación asistida

[![CI Tests](https://github.com/fernandosegrr/fsi_proyect/actions/workflows/ci.yml/badge.svg)](https://github.com/fernandosegrr/fsi_proyect/actions)
[![Estado J15](https://img.shields.io/badge/Estado-J15%20Cierre-blue.svg)](docs/reporte_J15.md)
[![Entrega FSI](https://img.shields.io/badge/Entrega-1%20Diciembre%202026-green.svg)](docs/plan_documentacion.md)

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Profesor:** Dr. Horacio Rostro González  
**Equipo:** `[PENDIENTE: nombres del equipo]`  
**Manual completo en PDF:** [`docs/manual/Manual_Proyecto_Carro_FSI.pdf`](file:///e:/Dev/fsi_proyecto/docs/manual/Manual_Proyecto_Carro_FSI.pdf)  
**Centro de documentación:** [`docs/`](file:///e:/Dev/fsi_proyecto/docs/README.md) e [`docs/index.md`](file:///e:/Dev/fsi_proyecto/docs/index.md)  

---

## 1. Qué es el proyecto en un párrafo

El carro FSI es un vehículo robótico terrestre de cuatro ruedas motrices gobernado por un microcontrolador ESP32 DevKit y supervisado desde un teléfono móvil Android mediante una red WiFi local. Integra un módulo de cámara inalámbrica independiente (Freenove ESP32-WROVER v3.0) para streaming de video continuo y detección visual de obstáculos, un giroscopio digital MPU6050 para control angular de precisión en bucle cerrado, un sensor láser ToF VL53L0X para parada de emergencia milimétrica autónoma, y una biblioteca cinemática matemática escrita en C estándar (C99) enlazada por JNI que planifica rutas sobre una cuadrícula de 20 cm, sortea obstáculos de manera reactiva y reporta telemetría bidireccional en tiempo real a través de un canal WebSocket protegido por un watchdog estricto de 1000 ms.

---

## 2. Diagrama general de arquitectura

```
                            ┌────────────────────────────────┐
                            │        Teléfono Android        │
                            │   (App FSI / Núcleo en C)      │
                            │   192.168.4.10 (cliente DHCP)  │
                            └───────▲────────────────┬───────┘
                                    │                │
                      HTTP /stream  │                │ WebSocket /ws
                      o UDP :4210   │                │ (comandos y SENS)
                                    │                │
            ┌───────────────────────┴──┐     ┌───────▼────────────────────────┐
            │ Freenove ESP32-WROVER    │     │ ESP32 DevKit (WROOM-32)        │
            │ Cámara GC0308 + PSRAM    │     │ Punto de Acceso WiFi (AP)      │
            │ 192.168.4.2 (IP estática)│     │ 192.168.4.1 · Servidor Web :80 │
            └──────────────────────────┘     └───────┬────────────┬───────────┘
                                                     │            │
                                         I2C 21/22   │            │ GPIO 27/26/25/33
                                         (400 kHz)   │            │ PWM (1500 Hz)
                                        ┌────────────┴──┐         ▼
                                        │ MPU6050 (IMU) │     TC1508A
                                        │ VL53L0X (ToF) │  (Driver puente H)
                                        └───────────────┘         │
                                                                  ▼
                                                            4 motores TT
                                                       (VDD = 4 pilas AA ~6V)
```

---

## 3. Estado actual de validación (Transparencia técnica)

En estricto apego a la honestidad científica y a las directrices de entrega, el estado del proyecto se desglosa explícitamente en tres categorías:

### 3.1 Verificado físicamente en hardware
- **Ajuste de balance en línea recta:** Con potencia 255 (Perfil MANUAL), balance 988 (252 motor izquierdo / 255 motor derecho) reduce el desvío lateral de 14.5 cm a **3.0 cm** a 150 cm de avance sobre piso plano (`firmware_carro/config.h:103-104`).
- **Rendimiento de cámara GC0308:** Con resolución HQVGA (240×176 px, calidad 50) entrega **12.4 fps** reales, 56 ms de tiempo de compresión y 137 ms de antigüedad de cuadro (`docs/camara_prueba.md`).
- **Artefactos binarios oficiales:**
  - APK Android: `dist/carro_fsi_0.4-074c9fd.apk` (**14,852,386 bytes**, SHA-256 `36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA`).
  - Imagen de firmware: `dist/firmware_carro_453ed20-20261027.bin` (**1,031,712 bytes**, SHA-256 `9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E`).
- **Pruebas físicas completadas:** Escenarios B0, B1, B3, B4, B5 y B6 superados en pista física en la jornada J14.

### 3.2 Comprobado mediante simulación en PC
- **Corrección de pose J15:** En 400 rutas simuladas del escenario B6 (esquivar una caja de 20 cm y reincorporarse a la trayectoria), la integración del rumbo instantáneo del giroscopio redujo el error lateral promedio de 13.2 cm a **1.9 cm** y el percentil 95 (p95) de 21.9 cm a **4.6 cm**, alcanzando 400 de 400 llegadas exitosas a menos de 20 cm del destino y sin contacto.
- **Batería de pruebas automatizadas:**
  - Núcleo cinemático C99: 21 ejecutables de prueba, **1598 casos evaluados, 0 fallos**.
  - Firmware y simulación de hardware: **447 casos evaluados, 0 fallos**.
  - Servidor WebSocket simulado: **46 casos evaluados, 0 fallos**.
  - Verificador de integridad de versiones: **13 casos evaluados, 0 fallos**.
- **Prueba de estrés de cola:** Inyección masiva de 100 000 comandos continuos procesados sin perder un solo comando de frenado (`STOP`).

### 3.3 Pendiente y NO MEDIDO en hardware
- **Validación física de J15:** El algoritmo de pose corregida cuenta con respaldo únicamente en simulación de PC; en el vehículo físico real se realizaron **cero corridas con esta versión** y el criterio de 5 de 6 llegadas consecutivas permanece como **NO MEDIDO**.
- **Balance del perfil AUTO en pista:** La relación 207/210 fue escalada teóricamente a partir del perfil manual; no ha sido medida con cinta métrica en el carro físico (**NO MEDIDO**).
- **Pruebas B7, B8, B9 y B2:** Suspendidas durante el cierre de J14 por decisión del dueño del proyecto.

---

## 4. Estructura del repositorio

```
fsi_proyecto/
├── app_android/            Código fuente de la aplicación Android en Kotlin y C++ (JNI)
│   ├── app/src/main/cpp/   Adaptador nativo JNI (puente.c y CMakeLists.txt)
│   └── app/src/main/java/  Vistas, sesiones persistentes, visión y navegación
├── camara_prueba/          Firmware de la cámara Freenove ESP32-WROVER v3.0 (GC0308)
├── dist/                   Artefactos oficiales compilados y hashes criptográficos SHA-256
├── docs/                   Centro de documentación técnica completa, guías y referencias
│   ├── codigo/             Recorridos pedagógicos archivo por archivo del código fuente
│   ├── figuras/            Fuentes en Python (matplotlib) y catálogo de las 12 figuras (PNG y SVG)
│   ├── imagenes/           Carpeta para fotografías físicas reales y capturas de pantalla
│   ├── manual/             Script automatizado y Manual del Proyecto consolidado en PDF
│   └── referencia/         Monografías técnicas profundas de las 8 áreas del sistema
├── firmware_carro/         Firmware del microcontrolador de tracción (ESP32 DevKit)
├── nucleo_c/               Biblioteca matemática en C99 puro (trayectorias, navegador, obstáculos)
├── pruebas_firmware/       Banco de pruebas en PC con hardware simulado y servidor mock
├── original_backup/        Copia histórica de referencia (no se toca ni versiona)
└── scripts/                Herramientas de verificación, empaquetado y comprobación de integridad
```

---

## 5. Requisitos del sistema

- **Para desarrollo y pruebas en PC:**
  - Sistema Operativo: Windows 10/11, Linux o macOS.
  - Compilador C: `gcc` compatible con estándar C99 (vía MinGW-w64 o nativo).
  - Herramienta de construcción: GNU `make` y `cmake` ($\ge 3.20$).
  - Python: versión 3.9 o superior con bibliotecas `websockets`, `matplotlib` y `reportlab`.
- **Para compilación de microcontroladores:**
  - `arduino-cli` ($\ge 0.35$) con el core `esp32:esp32` versión `3.3.12`.
  - Bibliotecas Arduino: `AsyncTCP` y `ESPAsyncWebServer`.
- **Para compilar la app Android:**
  - Android Studio Ladybug / Meerkat o herramientas SDK/NDK de línea de comandos.
  - Android SDK Platform 34 (Android 14) y NDK versión 25 o superior.
  - Java Development Kit (JDK) 17.

---

## 6. Guía paso a paso: Compilación, instalación y verificación

### 6.1 Ejecutar la suite completa de pruebas en PC (sin hardware)

No requiere placas ni teléfono; evalúa la lógica en C estándar con hardware simulado:

```powershell
# 1. Pruebas del núcleo cinemático y navegador (1598 casos)
cd nucleo_c
make test

# 2. Pruebas de firmware, watchdog y cámara (447 casos)
cd ..\pruebas_firmware
make test
```

### 6.2 Compilar y cargar el firmware del carro (ESP32 DevKit)

```powershell
# 1. Crear archivo de credenciales locales (ignorado en git)
Copy-Item firmware_carro\secretos.ejemplo.h firmware_carro\secretos.h
# Configurar WIFI_SSID y WIFI_PASSWORD en firmware_carro\secretos.h

# 2. Compilar firmware con arduino-cli
arduino-cli compile --fqbn esp32:esp32:esp32 firmware_carro

# 3. Flashear al DevKit conectado por USB (ej. puerto COM3)
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32 firmware_carro
```

### 6.3 Compilar y cargar el firmware de la cámara (Freenove ESP32-WROVER)

```powershell
# 1. Compilar para modelo WROVER con PSRAM
arduino-cli compile --fqbn esp32:esp32:esp32wrover camara_prueba

# 2. Flashear la placa de la cámara conectada por USB (ej. puerto COM4)
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32wrover camara_prueba
```

### 6.4 Instalar y verificar el APK en el teléfono Android

```powershell
# 1. Verificar la integridad criptográfica del APK oficial
python scripts\verificar_version.py --solo-apk

# 2. Instalar en el teléfono conectado por ADB
adb install -r dist\carro_fsi_0.4-074c9fd.apk
```

---

## 7. Aviso de seguridad eléctrica y límites conocidos

> **ADVERTENCIA ELÉCTRICA:** El portapilas de 4 pilas AA (~6 V) alimenta **exclusivamente el pin VDD del driver TC1508A**. Bajo ninguna circunstancia debe conectarse a los pines de alimentación de 3.3 V o 5 V del microcontrolador ESP32. La tierra (GND) del portapilas y la de la powerbank deben permanecer unidas en un punto común.

- **Límites de autonomía:** Realizar un máximo de 20 corridas o 90 minutos de prueba continuas por juego de pilas para evitar degradación de torque por caída de voltaje.
- **Autoridad de parada ToF:** El sensor láser tiene autoridad absoluta para detener el carro de manera autónoma (`TOF_PARO`) si detecta un obstáculo a menos de la distancia crítica de seguridad.

Para consultar el diagnóstico de incidencias, véase [`docs/solucion_problemas.md`](file:///e:/Dev/fsi_proyecto/docs/solucion_problemas.md) y [`docs/pendientes_documentacion.md`](file:///e:/Dev/fsi_proyecto/docs/pendientes_documentacion.md).
