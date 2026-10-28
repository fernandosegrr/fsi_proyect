# Mantenimiento preventivo y conservación del carro FSI

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Propósito:** Guía de buenas prácticas mecánicas, eléctricas y metrológicas para prolongar la vida útil del vehículo y preservar la repetibilidad de las pruebas.  
**Fecha:** 27 de octubre de 2026  

---

## 1. Mantenimiento del sistema de alimentación

### 1.1 Cuidado de las baterías de potencia (Pilas AA)
- **Tipo de celdas recomendadas:** Utilizar pilas alcalinas estándar de 1.5 V (4 en serie = 6.0 V nominales) o acumuladores recargables de NiMH de buena calidad (1.2 V $\times$ 4 = 4.8 V nominales).
- **Prohibición de mezclas:** Nunca mezclar pilas nuevas con usadas ni de marcas o químicas distintas; la celda más débil se sobrecalentará y limitará la corriente del conjunto.
- **Prevención de sulfatación:** Retirar siempre las 4 pilas AA del portapilas al finalizar cada sesión de trabajo. Si el chasis se almacena con las pilas colocadas durante varias semanas, el riesgo de fuga química de electrolito sulfatará las terminales metálicas y arruinará el contacto eléctrico.

### 1.2 Cuidado de la batería de control (Powerbank)
- Para almacenamiento prolongado (más de 1 mes), conservar la batería entre el 40 % y el 60 % de carga (2 LEDs encendidos).
- Cargarla al 100 % el día previo a cualquier sesión de laboratorio o exposición formal.
- Desconectar los cables micro-USB de las placas DevKit y Freenove cuando el vehículo no esté en uso.

---

## 2. Mantenimiento mecánico del chasis y motores

### 2.1 Motores TT y reductoras plásticas
- Los cuatro motores amarillos incorporan engranajes internos de nylon y poliacetal. **Bajo ninguna circunstancia se deben forzar o hacer girar manualmente las ruedas a gran velocidad cuando el carro está apagado**, ya que el par invertido transmitido desde la rueda hacia el piñón del micromotor puede romper los dientes plásticos de la reductora.
- **Inspección de ejes:** Periódicamente, retirar las cuatro ruedas de hule e inspeccionar los ejes de plástico para retirar pelos, polvo o fibras de alfombra que suelen enrollarse entre el buje del motor y la llanta, provocando fricciones asimétricas.

### 2.2 Limpieza de neumáticos
- La tracción del chasis depende de la adherencia del compuesto de caucho de las llantas.
- Si las ruedas acumulan polvo, el coeficiente de fricción cae y el vehículo patina en los pivotes angulares.
- Limpiar la banda de rodadura de las cuatro ruedas con un paño ligeramente humedecido en alcohol isopropílico o agua jabonosa antes de calibrar rumbos.

### 2.3 Apriete de tornillería
- Las vibraciones generadas por la marcha continua tienden a aflojar las tuercas de los separadores de latón que sostienen los dos niveles del chasis y los soportes de la cámara.
- Revisar y apretar suavemente con destornillador de cruz la tornillería antes de cada jornada de pruebas.

---

## 3. Limpieza de sensores ópticos

1. **Lente de la cámara Freenove (GC0308):** Limpiar suavemente con una toallita de microfibra para lentes fotográficos. Huellas dactilares o grasa sobre el lente reducen el contraste de la imagen y perjudican la inferencia del modelo TensorFlow Lite.
2. **Sensor ToF láser VL53L0X:** El sensor dispone de dos ventanas ópticas diminutas (un diodo emisor láser VCSEL y un arreglo receptor SPAD). El polvo depositado sobre estas ventanas produce reflexiones parásitas internas que provocan lecturas de distancia erróneamente cortas. Limpiar con aire comprimido suave o hisopo de algodón seco.

---

## 4. Almacenamiento y transporte

- Transportar el carro en una caja rígida acolchada para evitar que los módulos sensores montados sobre espaciadores sufran golpes mecánicos o desalineaciones en su eje de puntería.
- Proteger especialmente el soporte superior del teléfono móvil para no deformar el ángulo de inclinación calibrado de la cámara.
