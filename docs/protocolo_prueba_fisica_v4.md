# Protocolo de piso v4 · PENDIENTE

Estos ensayos requieren autorización de Fase 4 y un «listo» del operador para cada corrida. Empezar por los ensayos inmóviles. Una persona mantiene la mano en el interruptor. Usar cartón blando, piso despejado, cinta métrica y una franja marcada de exclusión. Registrar batería, versión y bandera en cada fila. PARA ante contacto, giro hacia el blanco, pérdida de control, congelamiento o batería baja. Cortar alimentación y registrar el primer fallo antes de intentar otra corrida. La recomendación actual es conservar v3 para la exposición.

## Preparación en PC

La APK normal se construye con `scripts/compilar_app.ps1`. Para la variante experimental establecer `$env:ORG_GRADLE_PROJECT_guardiaV4='true'` en la sesión de PowerShell y compilar con el mismo script. Para volver al valor normal, retirar esa variable de la sesión y recompilar. Conservar por separado ambos APK y sus SHA-256 locales. El resultado de una compilación queda en `app_android/app/build/outputs/apk/debug/app-debug.apk`.

Instalación propuesta, solo tras autorización y comprobando el dispositivo elegido con `adb devices`:

```powershell
adb -s SERIAL_DEL_TELEFONO install -r app_android/app/build/outputs/apk/debug/app-debug.apk
```

Una incompatibilidad de firma exige detenerse y conservar la instalación y sus datos. La APK normal admite el protocolo anterior. La experimental necesita el firmware del carro que anuncia `TED=1`. La cámara funciona con su firmware existente.

**Carga propuesta para una sesión de mantenimiento separada.** Identificar la DevKit en el puerto serie, con una sola placa conectada y potencia de motores desconectada. Compilar con `arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir RUTA_LOCAL_PRIVADA firmware_carro`. Conservar el binario solo localmente. Para esa sesión posterior, la operación correspondiente sería `arduino-cli upload --fqbn esp32:esp32:esp32 --port PUERTO_CONFIRMADO --input-dir RUTA_LOCAL_PRIVADA firmware_carro`. La Fase 4 descrita en este encargo excluye grabar firmware. Si la placa disponible carece de TED, limitarse a verificar la parada y a la variante normal. El PC permanece en su red habitual.

## Orden de ensayos

1. **Soporte, potencia de motores desconectada.** Con la bandera apagada comprobar versión, CAPS y SENS. Con la experimental y firmware anterior, observar el primer intento de ruta en soporte. Debe mantenerse STOP y aparecer causa 21 al faltar la respuesta verificable. Cualquier comando de movimiento, Terminado o reanudación mediante Continuar/cruceta exige PARA.
2. **TED y mediana, carro inmóvil.** Con firmware compatible, guardia encendida y potencia de motores desconectada, usar un intento de ruta para registrar la telemetría. Colocar una cartulina amplia a 30, 50 y 80 cm. Anotar distancia, validez, secuencia y tiempo entre envío y respuesta durante 5 s por posición. En TED el ID debe coincidir. La edad conservadora suma la edad en la placa y toda la ida y vuelta. Una muestra repetida debe envejecer. Al superar 400 ms debe mantenerse STOP con causa 21. Si no se puede inducir la demora en condiciones controladas, clasificar ese caso NO MEDIDO. No desconectar I2C bajo tensión.
3. **Patas y luz, aún sin tracción.** En un solo lado del corredor poner cartón vertical de 25 y 35 mm a 80, 50 y 30 cm. Repetir con una sombra y un reflejo sin objeto. Registrar detecciones por cuadro y cada causa 20. Esperar parada si persiste la evidencia cercana dentro de la huella. Un obstáculo que desaparece para ambos sensores o una sombra que provoque AYUDA deja roja esa configuración. Conservar solo telemetría numérica.
4. **Calibración física previa en zona libre.** Autoridad láser encendida, batería anotada, bandera apagada. Tres rectas de 1 m con cinta y tres giros por lado sobre marcas; medir distancia, tiempo, deriva y arco de ambas esquinas. Medir retiro del sensor y cuánto avanza el frente desde STOP hasta inmovilidad. Si no cabe la envolvente de frenado o el chasis invade el área de exclusión, terminar. No extrapolar la distancia de parada de crucero al pulso de arranque.
5. **Blanco sólido blando, espacio libre a ambos lados.** Empezar con caja de 20 × 20 cm a 1 m. Probar centrada y desplazada medio ancho del chasis. Primero usar la versión de referencia y después la experimental únicamente si los ensayos previos de esta configuración pasaron. Medir mínima separación frontal/lateral y margen entre la cola y la cara lejana al aparecer Terminado. Aceptar solo cero contactos y margen de cola positivo. Si se detiene fuera de tiempo, interrumpir antes de tocar y contar la intervención.
6. **Sombras, objeto bajo y paso estrecho.** Comparar por separado sol, sombra, reflejo y cambio de exposición en ruta libre, cinco repeticiones con la misma geometría por variante. Contar corridas con parada, esquives y AYUDA. Exigir cero esquives/AYUDA y menos paradas. Repetir con el objeto bajo para descubrir pérdida de sensibilidad. Probar objeto largo, dos sólidos sucesivos y caja junto a pared solo con el arco medido libre. Una salida del centro del carro con la esquina trasera invadiendo la pared exige PARA.
7. **Muebles, último ensayo.** Reproducir primero las patas con cartón deformable y marcar la planta del asiento. Ensayar banquito y silla por separado, en ambos desplazamientos laterales. Detener antes de que el chasis cruce la marca si falta evidencia de paso. Registrar como intervención o entrada bajo mueble, aun sin roce. La aceptación exige menos contactos y AYUDA que la referencia, sin empeorar otra métrica. Los contactos simulados actuales impiden recomendar este paso para la exposición.

## Registro de una corrida

Crear una copia privada de esta cabecera. En la caja negra de una ruta experimental, las líneas `TED` contienen ID de consulta, secuencia del eco, distancia en milímetros, validez, ángulo en décimas de grado, ida y vuelta en milisegundos y edad conservadora (saturada a 1023 ms). Medir la holgura con el chasis detenido y consignar «NO MEDIDO» donde falte un dato. La caja negra queda local. En las comparaciones separar contacto, bajo mueble y ayuda excluyentes de la bandera total de AYUDA.

```csv
fecha,corrida,version_apk,firmware_carro,ted,guardia,escena,posicion_mm,luz,bateria,distancia_cinta_mm,duracion_ms,edad_ted_ms,rtt_ms,estado,causa,contacto,bajo_mueble,ayuda_total,intervencion,esquives,paradas,holgura_min_mm,margen_cola_al_terminar_mm,caja_negra_local,observacion
```
