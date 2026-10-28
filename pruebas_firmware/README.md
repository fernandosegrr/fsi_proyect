# Banco de pruebas de firmware en PC (`pruebas_firmware`)

Este directorio contiene las herramientas y programas de prueba unitaria en PC para validar exhaustivamente la lógica del firmware del carro y de la cámara sin necesidad de contar con los circuitos físicos conectados.

---

## 1. Metodología de simulación de hardware

Gracias al desacoplamiento entre la lógica en C puro y el código específico de Arduino, las funciones de acceso a hardware (`hw_motor_pwm`, `hw_millis`, `hw_keepalive`, transacciones I2C y temporizadores) son reemplazadas por implementaciones virtuales en `sensores_falsos.c` y en los propios archivos de prueba.

Esto permite:
- Avanzar o congelar el tiempo virtual en milisegundos para evaluar con precisión de un milisegundo el disparo del watchdog de comunicación.
- Inyectar fallos de bus I2C, lecturas ruidosas, desconexión de sensores o datos congelados.
- Comprobar colas saturadas mediante pruebas de estrés masivas.

---

## 2. Programas de prueba incluidos

| Programa ejecutable | Código fuente | Casos | Aspectos evaluados |
|---|---|---|---|
| `pruebas_carro` | `pruebas_carro.c` | 152 | Comandos básicos (`FORWARD`, `REVERSE`, `LEFT`, `RIGHT`, `STOP`, `EBRAKE`), tiempo exacto del watchdog a 1000 y 1001 ms, inmunidad de `STOP`, descarte selectivo de `PING`, perfiles AUTO/MANUAL y prueba de estrés con 100 000 comandos continuos. |
| `pruebas_sensores` | `pruebas_sensores.c` | 246 | Sesgo y calibración del MPU6050, tolerancia a WHO_AM_I alternativos (0x68, 0x70, 0x72), parada de emergencia por ToF VL53L0X, giros en bucle cerrado (`GIRAR:<grados>`), y serialización de telemetría `SENS`. |
| `pruebas_escalonado` | `pruebas_carro.c` (`-DARRANQUE_ESCALONADO_MS=40`) | 15 | Arranque desfasado de motores para mitigar picos de consumo de corriente en la batería. |
| `pruebas_camara` | `pruebas_camara.c` | 34 | Vigilante de red de `camara_prueba/recuperacion.c`, intervalos de reintento exponencial (1, 2, 4, 8, 10 s), reinicio automático a los 60 s y tolerancia al desbordamiento del contador `millis()`. |
| **Total de casos C** | — | **447** | **0 fallos.** |

---

## 3. Servidor y cámara simulados

- `servidor_falso.py`: Servidor WebSocket en Python que reproduce fielmente el comportamiento y respuestas del ESP32 del carro (`192.168.4.1:80/ws`). Permite operar la app Android desde la PC conectando el teléfono a la misma red local.
- `probar_servidor_falso.py`: Suite de 46 pruebas automatizadas contra `servidor_falso.py` para asegurar su estricta concordancia con el protocolo de producción.

---

## 4. Cómo ejecutar las pruebas

```powershell
# Compilación y ejecución de todas las pruebas en PC
cd pruebas_firmware
make test
```

Para consultar más detalles sobre la cobertura de pruebas del proyecto, véase [`docs/referencia/pruebas.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/pruebas.md).
