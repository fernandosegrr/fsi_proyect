# Guía de operación y rutinas del carro FSI

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Propósito:** Manual de procedimientos prácticos para el encendido, puesta a punto, teleoperación y ejecución de rutas autónomas.  
**Fecha de vigencia:** 27 de octubre de 2026  

---

## 1. Reglas operativas fundamentales

Para garantizar la integridad del hardware y la reproducibilidad de los resultados, se deben respetar estrictamente las siguientes cuatro reglas:

1. **Regla de los 90 minutos y 20 corridas:**  
   Los motores TT alimentados con pilas AA sufren caídas perceptibles de velocidad y torque a medida que la batería se descarga. **No se deben realizar más de 20 corridas continuas o superar 90 minutos de pruebas con el mismo juego de pilas**. Pasado este umbral, los factores de calibración de avance y giro dejan de ser válidos.
2. **Procedimiento «Colocación y LISTO»:**  
   Antes de iniciar cualquier recorrido autónomo, el carro debe alinearse manualmente sobre la línea de partida con las cuatro ruedas en contacto plano con el suelo. Una vez posicionado, se debe verificar en la pantalla de la app que el giroscopio reporte un rumbo estable de 0.0° (`IMU_CERO`) y que el sensor ToF marque el frente despejado antes de presionar el comando de inicio.
3. **Mecanismos de parada inmediata:**  
   El operador debe mantener siempre el pulgar listo sobre el botón rojo de **Freno de Emergencia (`EBRAKE`)** en la pantalla del teléfono. Si el vehículo no responde a la app, el operador debe apagar físicamente el interruptor del portapilas de motores.
4. **Mantenimiento del piso de prueba:**  
   La tracción depende enteramente de la fricción entre los neumáticos de hule y la superficie. La pista debe estar libre de polvo, grasa o cables sueltos que puedan enredarse en los ejes de los motores.

---

## 2. Procedimiento de encendido y enlace inicial

### Paso 1: Verificación de fuentes de energía
- Confirmar que la powerbank de 5 V cuente con al menos 75 % de carga (3 o 4 LEDs indicadores encendidos).
- Instalar 4 pilas alcalinas tipo AA nuevas o recién recargadas en el portapilas de potencia.
- Mantener el interruptor de los motores en posición **APAGADO (OFF)** durante el arranque inicial.

### Paso 2: Conexión de la electrónica
- Conectar el cable USB de la powerbank al puerto micro-USB de la placa ESP32 DevKit. Se encenderá el LED rojo de alimentación de la placa.
- Conectar el segundo cable USB de la powerbank a la placa de la cámara Freenove ESP32-WROVER.
- Esperar aproximadamente 5 segundos a que ambas placas inicialicen sus periféricos y la red WiFi esté lista.

### Paso 3: Emparejamiento inalámbrico del teléfono
1. En el teléfono móvil Android, abrir Ajustes de WiFi.
2. Localizar y seleccionar la red inalámbrica emitida por el carro (nombre configurado en `firmware_carro/secretos.h`).
3. Ingresar la contraseña de red predefinida.
4. Esperar a que el teléfono obtenga dirección IP automática por DHCP (habitualmente `192.168.4.10`).

### Paso 4: Inicio y conexión de la app
1. Abrir la aplicación **Carro FSI** en el dispositivo móvil.
2. En la pestaña **Inicio**, verificar que la dirección de destino esté configurada en `192.168.4.1`.
3. Presionar el botón **Conectar**.
4. Confirmar que el indicador pase a color verde («Conectado») y que el panel de telemetría comience a reflejar lecturas de latencia de ping (habitualmente entre 15 y 45 ms), ángulo del giroscopio y distancia del láser ToF.
5. Encender el interruptor de las pilas de motores (**ON**). El vehículo está listo para operar.

---

## 3. Modos de operación

### 3.1 Modo Manual (Teleoperación)
Ubicado en la pestaña **Manual** de la aplicación móvil.

- **Controles de dirección:** Botones para Adelante, Atrás, Pivote Izquierda y Pivote Derecha. Al mantener presionado un botón, la app transmite ráfagas de comando cada 250 ms. Al soltarlo, se emite un `STOP`.
- **Selector de Perfil:**
  - **MANUAL:** Tracción a máxima potencia (255 derecho / 252 izquierdo). Diseñado para desplazamientos ágiles y superar pequeños desniveles.
  - **AUTO:** Tracción regulada a 210 de potencia (207 izquierdo). Recomendado para maniobras precisas en espacios reducidos.
- **Transmisión de video:** El centro de la pantalla muestra en tiempo real el flujo MJPEG proveniente de la cámara (`http://192.168.4.2/stream`).

### 3.2 Modo Semiautónomo (Navegación por Rutas)
Ubicado en la pestaña **Rutas**.

1. **Trazado en cuadrícula:** La pantalla presenta una retícula métrica donde cada celda representa un área física de 20×20 cm.
2. **Definición de waypoints:** Con el dedo, el usuario pulsa sobre las celdas para marcar la trayectoria deseada (punto de inicio, cambios de dirección y destino final).
3. **Simulación previa:** Pulsar el botón **Simular** para visualizar una recreación cinemática del recorrido estimado según los parámetros de calibración vigentes.
4. **Ejecución:** Al pulsar **Ejecutar Ruta**, el navegador nativo en C (`navegador.c`) toma el control:
   - El vehículo avanza por segmentos temporizados calculados con la constante de milisegundos por centímetro.
   - En cada cambio de dirección, ejecuta pivotes guiados por el giroscopio.
   - Si la cámara o el láser ToF identifican una caja u obstáculo en el corredor de seguridad, el carro frena, evalúa el espacio lateral disponible, se desvía 40 cm hacia el lado libre, supera la caja y busca un punto de reincorporación 30 cm por delante de la obstrucción.

---

## 4. Lista de comprobación antes de una demostración o examen

Antes de la presentación final ante el jurado o profesor, ejecutar meticulosamente esta lista de preparación:

- [ ] **Ajustes de red restablecidos:** En la app, acceder a `Ajustes → Red` y verificar que las IPs coincidan con `192.168.4.1` (carro) y `192.168.4.2` (cámara).
- [ ] **Pilas nuevas instaladas:** Colocar un juego de 4 pilas AA alcalinas sin uso previo para asegurar que los giros y avances mantengan el torque calibrado.
- [ ] **Aprender el color del piso del aula:** En la pantalla `VisionActivity`, apuntar la cámara al piso del lugar de la demostración y presionar el botón de muestreo cromático para que el detector reconozca la superficie del recinto.
- [ ] **Prueba de avance recto de 1 metro:** Ejecutar un avance de verificación en línea recta para confirmar que el carro no presenta desviaciones excesivas por balance.
- [ ] **Aseguramiento del teléfono:** Desactivar el bloqueo automático de pantalla del móvil para evitar que se apague en mitad de una ruta autónoma.
