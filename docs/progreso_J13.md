J13 — Progreso del trabajo desatendido

Inicio: 20 de octubre de 2026 (hora de Ciudad de México). Base: c6b4fc5.
Repositorio limpio; identidad Git noreply ya configurada.

Condiciones: el dueño declara motores sin pilas ni powerbank y autoriza carga e instalación. No se enviarán movimientos al hardware. Solo se instalará con adb install -r y se cargará con cargar.ps1 nuevo conservando NVS. No se consultan imágenes de cámara ni otras apps.

Fase 1: terminada. Ocho giros alternados, calentamiento excluido del ajuste y de la adaptación de tiempos, descarte del peor válido con comprobación física del par restante. CALGIRO registra todos los pares (planeado, real, grados o sin medida) al terminar, fallar o cancelar; pantalla identifica calentamiento, descarte y pares ante error. Commit: J13 fase 1, el que contiene esta entrada.
Pruebas fase 1: núcleo 20 programas / 1545 casos (calgiro 37, antes 32), firmware 152 + 224 + 15 + 34, estrés de 100000 comandos sin STOP perdido, hash de página y servidor falso 42. App y firmware compilan. Estado: verde, sin tocar dispositivos.
Commit fase 1: b0d118b.
Fase 2: terminada. Tope 250 ms; AUTO sin historial 100 por lado, MANUAL conserva 20. Mediana de cinco, NVS, ANT? y ANT_BORRAR conservados. Asentamiento ampliado a 750 ms para medir la inercia completa (motores ya apagados). Pruebas: núcleo 1545; firmware 152+229+15+34, estrés sin STOP perdido. Inercia simulada de 200 ms aprende más de 150 y queda a ±3°. Ruido asimétrico: izquierda 10/10, derecha 9/10. Firmware compilado. Estado verde. Commit: J13 fase 2.
Commit fase 2: 7be39df.
Fase 3: terminada. Reintento del sesgo tras motivo 4 cada 5 s mientras falla, solo con motores detenidos y asentados. Reinicia el promedio sin resetear el chip ni esperar al bus. IMUR, IMUM e IMUE en CAPS; historial NVS de cinco arranques agregado a ARR; estado y motivo en Sensores. Sin sesgo, GIRAR se niega y las rutas caen al tiempo. Pruebas: núcleo 1546 (navegador +1); firmware 152+239+15+34, estrés sin STOP perdido. App y firmware compilan. Commit: J13 fase 3. Estado verde.
Commit fase 3: 5085c6d.
Fase 4: terminada. Firmware imprime FW:<git>-<aaaammdd> al arrancar y en CAPS; cargar.ps1 genera la versión en copia temporal. App 0.4 (versionCode 3) incluye hash en BuildConfig y metadatos verificables del APK; Sensores muestra ambas versiones. verificar_version.py consulta USB y APK instalado y compara con Git, opcionalmente bytes del build. URL de adb solo en memoria, aviso CÁMARA DE PRUEBA en pantallas de video, apertura normal limpia la URL antigua y usa la real. cargar.ps1 nuevo exige una sola placa serie y MAC conocida de DevKit; no usa sonda. Pruebas: núcleo 1546, firmware 152+240+15+34, hash/estrés, siete pruebas del verificador. Firmware y APK compilan; APK contiene APP:0.4-5085c6d en este build de desarrollo. Commit: J13 fase 4. Estado verde.
Fase 5: en curso (simulación; sin validar físicamente).
Fase 6: pendiente; solo tras pruebas verdes y push.
Cierre: pendiente; guardar estado del teléfono antes de tocarlo, restaurarlo al terminar y documentar estado exacto del DevKit.

Decisiones: cuatro giros por lado significan ocho mediciones totales, dos de calentamiento y seis válidas. Se conservarán y registrarán las ocho, identificando las seis usadas para el ajuste. Un par de puntos siempre define una recta: se exigirá además pendiente y frenado físicamente admisibles para aceptar el descarte.
