# Introducción

El Carro FSI es un vehículo de cuatro ruedas controlado por una aplicación Android y una placa ESP32. Permite conducirlo de forma manual o preparar una ruta en una cuadrícula para que el sistema intente seguirla con supervisión de una persona. Una cámara independiente transmite video y los sensores del carro aportan información de orientación y distancia frontal.

Este manual describe la arquitectura, las conexiones, el funcionamiento del software, la operación y la evidencia disponible. Está dirigido a quien necesita comprender, mantener o presentar el proyecto sin haber participado en su desarrollo. La descripción del comportamiento previsto no sustituye una prueba del vehículo físico.

Para comenzar, conviene leer la arquitectura y el hardware. Los capítulos de firmware, cámara, comunicación, navegación y percepción explican las responsabilidades de cada parte. La operación y las pruebas reúnen las condiciones de uso y los resultados; los anexos concentran comandos, pines y referencias al repositorio.

## Cómo interpretar la evidencia

**VERIFICADO** identifica un resultado observado en el carro u otro dispositivo físico, bajo las condiciones indicadas. **SIMULADO** identifica un resultado obtenido con un modelo de software. **NO MEDIDO** señala una característica para la que no existe una medición física suficiente. **PENDIENTE** señala una comprobación o dato necesario que todavía falta. Una simulación ayuda a revisar la lógica, pero no demuestra por sí sola el comportamiento del carro en el piso.

Dónde está en el código: `firmware_carro.ino`, `navegador.c`, `MainActivity.kt`.
