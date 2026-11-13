# Límites conocidos de la navegación

Rama `fix/navegacion-esquive-confirmado`, revisión de autonomía del 12-nov-2026 sobre v1.1. **SIMULADO / NO MEDIDO en el carro.** No es v1.2. [Diagnóstico, poses y comparación](diagnostico_esquive_20261111.md).

## Fusión y cobertura

El láser fresco cercano (≤400 mm) confirma. Con láser libre, un positivo aislado de color o EfficientDet no detiene; con ambos coincidentes en la misma zona se frena según la banda y se puede esquivar por visión. Sin eco o con láser vencido decide la visión: coincidencia confirma, una fuente sola permite avance con precaución. El flujo óptico no se cuenta como una segunda fuente neuronal.

El haz es estrecho y las dos fuentes de cámara comparten la misma vista: pueden fallar juntas. Un objeto que no detecten ni el láser ni ambas fuentes visuales puede pasar inadvertido. Las bandas 1/2/3 son proximidad de imagen, no centímetros medidos. Los 300/500 mm usados sin láser son estimaciones, pendientes de calibración física. Reducir velocidad media con tandas 700/300 ms no reduce la velocidad instantánea durante cada impulso.

## Rebase y límites de espacio

No hay fondo fijo de 250 mm. El plan parte de distancia medida o banda visual, añade sensor-centro supuesto de 120 mm, primer tramo de 200 mm y margen de 150 mm. Si se sigue viendo el costado al volver, extiende 200 mm y aumenta el desvío lateral de 400 hacia 700 mm. Máximo seis intentos. La profundidad observada es una cota basada en pose por tiempo; no es un mapa del objeto.

La última recta puede prolongarse hasta 2 m para no recortar el rebase al punto final. Por ello el destino efectivo puede quedar más allá del que se dibujó. Cada movimiento sigue comprobando sensores, pero no existe un mapa previo de esa extensión. Una curva, un pasillo estrecho, el límite del arreglo o un objeto fuera de estos presupuestos pueden impedir un plan.

Antes de abandonar por ambos lados bloqueados se esperan 4000 ms y se intenta un retroceso de 150 mm solo si quedan 180 mm de tramo recto conocido detrás. Ese espacio se invalida al girar y se consume al retroceder. **No hay sensor trasero:** el supuesto solo cubre un entorno estático, no personas u objetos que entren por detrás. Los reintentos locales históricos de salida siguen acotados; tampoco prueban espacio trasero físico.

## Cierre y sensores

Terminado exige avance planeado cumplido y, después de STOP y 300 ms, tres lecturas nuevas válidas >800 mm sin coincidencia visual pendiente. Sin eco o láser deshabilitado, se permiten cinco cuadros nuevos sin coincidencia. Sin eco no suma lecturas láser libres; un SENS vencido tampoco habilita ese cierre. Si falta evidencia, reintenta y finalmente solicita ayuda al agotar el presupuesto. El cono libre no certifica todo el volumen del chasis.

Permanecen pérdida de video a 700 ms, cancelación de enlace/SENS en la app a 1,5 s, watchdog del firmware a 1000 ms y freno local `TOF_PARO` a 120 mm. STOP no elimina la latencia ni la inercia. Firmware sin cambios.

## Códigos de detención y AYUDA

Los códigos se conservan para compatibilidad de registros. Con AYUDA desactivada se comunica ERROR; pérdida de video siempre es ERROR. No se emite 13 por visión aislada.

| Código | Significado y condición |
|---:|---|
| 0 | Sin causa de error. |
| 1 | Ambos lados ocupados al menos 4000 ms y sin retroceso conocido disponible. |
| 2 | Agotados seis intentos de rebase, o presupuesto de extensión de la ruta. |
| 3 | Agotados los dos reintentos locales de salida, con frente aún bloqueado. |
| 4 | Planificador sin ruta válida tras sus candidatos; no se ejecuta un plan inválido. |
| 5 | Pérdida de video/sin cuadro para elegir lado tras un paro. ERROR y STOP. |
| 6 | No cabe el arco de ninguno de los lados libres y no queda retroceso conocido. |
| 7 | Agotadas cuatro correcciones de reincorporación por error de pose. |
| 8 | Agotadas las soluciones de retorno por pivotes; no hay retorno geométrico viable. |
| 9 | Giro fuera de ruta >150°: pose/plan incoherente; freno de cordura existente. |
| 10 | Índice de ruta retrocede: pose/plan incoherente; freno de cordura existente. |
| 11 | Recorrido >2,5 veces la ruta efectiva: presupuesto de recuperación agotado. |
| 12 | Pose a más de 1 m de la ruta: se perdió la localización fiable. |
| 13 | Histórico, reservado; ya no se usa para desacuerdo color/láser. |
| 14 | Rebase no cabe en la recta ni en su extensión permitida; no se trunca el plan. |
| 15 | Histórico de cierre no libre; ahora el cierre reintenta y agota 2 en vez de abandonarlo de inmediato. |

Desconexión, SENS perdido, error de visión y avisos de hardware se comunican además por los mensajes existentes de la app, no como un nuevo código 13. Los frenos de cordura no diagnostican por sí solos una batería o pieza defectuosa: requieren revisar pose/calibración/enlace. No se añadió un sensor de batería ni se inventa ese diagnóstico.

## Piso y precisión

La app mantiene Aprender piso exitoso para cada ruta; un aprendizaje fallido invalida el anterior. No detecta automáticamente toda variación de iluminación. Valores de color: 40, 7 celdas, fila mínima 44 %. Los valores reportados 240/23/44 % no permiten atribuir la causa óptica sin imágenes y máscara.

No hay encoders: la distancia se estima por tiempo; RUMBO mide orientación, no patinaje ni desplazamiento lateral. Los 120 mm sensor-centro, el modelo de chasis y el pivote requieren medición. Los resultados Monte Carlo cubren dos geometrías y sus distribuciones de ruido, no todos los objetos razonables.

## Pendiente de medición física

El banquito 35×45 del 11-nov es una observación reportada, **NO MEDIDA** con telemetría suficiente; el arreglo tampoco está medido. Primero repetir con caja de 20×20 cm, previa autorización; después banquito, patas, objeto largo y cambios de luz. Guardar ambas fuentes por zona, láser, distancia al frenar, rebase, reintentos, pose, contactos y estado final.

Los datos del [10-nov](estado_20261110.md), [CSV físico](pruebas_fisicas_20261110.csv) y [J16](reporte_J16.md) son históricos y no validan esta rama. No se movió el carro ni se instaló APK o grabó firmware. Compilación/instalación sin desinstalar: pasos preparados en el diagnóstico, no ejecutados.
