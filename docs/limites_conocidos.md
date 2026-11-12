# Límites conocidos del modo automático

Revisión de software del 11-nov-2026 en `fix/navegacion-esquive-confirmado`, basada en v1.1 (`bd1727b`). **NO MEDIDA en el carro.** La observación con un banquito de aproximadamente 35×45 cm y patas no valida este arreglo. Hay que repetir primero con caja de 20×20 cm, previa autorización. La etiqueta v1.1 no cambia; una eventual v1.2 requiere aprobación.

## 1. Confirmación frontal y pérdida del láser

La cámara puede detener el carro, pero no autoriza por sí sola un esquive. Se requieren tres lecturas láser válidas consecutivas, frescas (hasta 400 ms), de 1 a 500 mm, y cámara para elegir el lado libre. Si la visión persiste sin esa distancia, se mantiene STOP y en 1000 ms se pide AYUDA. Varios cuadros no convierten una banda visual en milímetros. Sin cámara lateral tampoco se gira a ciegas tras `TOF_PARO`.

El VL53L0X mide un haz frontal: una pata estrecha, un objeto fuera del haz o una superficie oscura/transparente pueden escapar a la medición. Una lectura inválida, sin eco, vieja o con sensor apagado **no significa libre**. Una sala sin retorno válido también puede acabar en AYUDA. La persistencia temporal no demuestra que el haz cubra todo el chasis.

## 2. Profundidad y dimensiones todavía supuestas

El rebase se ancla en la distancia láser tomada con el carro parado:

`fin requerido = avance sobre la ruta + 120 mm + distancia frontal + 250 mm + 150 mm`.

Los 120 mm (`NAV_SENSOR_FRENTE_MM`) representan el sensor delante del centro de un chasis supuesto de 240 mm; **falta medir ese desplazamiento en el carro**. Los 250 mm (`NAV_OBSTACULO_PROFUNDO_MM`) son un fondo supuesto, no medido por el láser. Los 150 mm son `NAV_PASO_MARGEN_MM`. El objetivo lateral sigue siendo 400 mm, compensado por pivotes. Si el rebase no cabe en la recta disponible, se pide AYUDA en vez de recortarlo al final de la ruta.

La reincorporación apunta casi transversalmente a la ruta, 50 mm por delante de la proyección actual, y vigila cámara/láser durante el regreso. Si sigue apareciendo el costado, se detiene en AYUDA. En el modelo de 35×45 cm y en el de 90 cm de fondo se detecta esa insuficiencia sin contacto. Esto **no garantiza** detectar todo mueble real: no se estima la profundidad, no hay mapa de contorno y unas patas pueden quedar fuera de ambos sensores.

## 3. Terminado requiere comprobación adicional

Antes de cerrar una ruta con sensores, y al completar un esquive, se detiene el carro, se comprueba que alcanzó el rebase planeado y se solicitan tres lecturas nuevas válidas mayores de 800 mm, después de 300 ms de asentamiento. Sin frente libre dentro de 1000 ms, se pide AYUDA. Un error de video sigue siendo ERROR y no se convierte en éxito por una lectura libre posterior.

Esta comprobación cubre el haz frontal; no mide separación lateral ni sustituye encoders. TERMINADO no certifica todo el volumen alrededor del vehículo.

## 4. Distancia por tiempo; rumbo por giroscopio

Los motores no tienen encoders. La distancia se ejecuta por tiempo calibrado y se integra con el rumbo SENS cuando el giroscopio está activo. RUMBO corrige orientación, pero no mide patinaje lateral, longitud recorrida ni pivote real. Batería, agarre y calibración conservan error longitudinal. El pivote por sentido y perfil es una aproximación.

Permanecen los frenos de cordura por desviación, índice y recorrido. Los giros pequeños que no admite el firmware se resuelven por tiempo. Los resultados físicos históricos se conservan en [el reporte J16](reporte_J16.md).

## 5. Piso, iluminación y falsos positivos

`OBSTACULO 3` significa banda de cercanía 3 por fila de imagen; no son tres objetos ni una distancia. `fusion.c` no recibe el láser. El color conserva prioridad para detecciones cercanas; modelo y flujo no eliminan por sí solos un positivo cercano de color. El corredor filtra celdas por geometría de imagen, no profundidad real.

Se conservan los valores por defecto de color 40, mínimo 7 celdas y fila mínima 44 %. Los valores reportados 240/23/44 % no demuestran la causa óptica: hacen falta cuadros, máscara y modelo de piso de esa iluminación. Un umbral de color mayor tolera más diferencia respecto al piso; aumentarlo a ciegas puede ocultar obstáculos.

La app exige aprendizaje de piso exitoso antes de cada ruta con cámara y avisa si falta. Un intento fallido invalida el permiso anterior. No detecta automáticamente todos los cambios de luz. Objetos del color del piso pueden seguir siendo falsos negativos; columnas extremas y patas de pocas celdas tienen cobertura limitada por el filtro espacial.

## 6. Latencia y freno físico

Video, WiFi, decodificación y tick añaden latencia; STOP no implica frenado instantáneo. Se conservan el corte de video a 700 ms y el watchdog del firmware a 1000 ms. El freno local a 120 mm reduce el riesgo pero no garantiza evitar contacto si el obstáculo aparece de repente. No se modificó firmware.

## 7. Evidencia y mediciones pendientes

- [Diagnóstico del 11-nov-2026](diagnostico_esquive_20261111.md): simulación de cada caso, pruebas y Monte Carlo pareado; ningún resultado es una medición física del arreglo.
- [Estado del 10-nov-2026](estado_20261110.md) y [CSV físico](pruebas_fisicas_20261110.csv): evidencia histórica con otra APK/FW y caja de 20×20 cm, seis corridas sin contacto, cinco completas y una AYUDA. No se transfiere esa validación a esta rama.
- Banquito 35×45 cm del 11-nov: observación reportada, **NO MEDIDA** con telemetría suficiente para atribuir el evento exacto. Duración de 13 s y giros 90→91, 90→92 y 84→85 no reconstruyen distancias.
- Pendiente, previa autorización: caja 20×20 cm, distancia al frenar, avance hasta la cara posterior, pose final y causa de AYUDA; después tamaño mayor, patas sin retorno e iluminación. Medir sensor-centro, calibración longitudinal y pivotes.
- Siguen pendientes las pruebas físicas de rectas con/sin RUMBO, L, cuadrado, IMU_ESCALA, WiFi en marcha, freno láser aislado, comparación pareada de autoridad láser, GPIO 2 y consumo. Los registros históricos citados conservan el detalle.

En esta sesión no se movió el carro, instaló APK, grabó firmware ni publicó binario.
