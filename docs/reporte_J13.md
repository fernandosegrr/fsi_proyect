J13 — Reporte de trabajo desatendido

Estado final, 21 de octubre de 2026, hora de Ciudad de México: fases 1 a 5 terminadas y publicadas; firmware cargado y verificado sin movimiento. APK nueva pendiente por bloqueo de PIN. Teléfono apagado. DevKit detenido en reset verificado; el USB sigue suministrando tensión.
Base c6b4fc5. Commits: fase 1 b0d118b; fase 2 7be39df; fase 3 5085c6d; fase 4 e60ec35; fase 5 4233201. El cierre posterior solo cambia documentación.

Cambios:
- Calibración: ocho giros, calentamiento excluido, descarte controlado, pares disponibles incluso con error o cancelación.
- Anticipación: AUTO inicial 100 ms, tope 250 ms; mediana de cinco y NVS conservados. Asentamiento de 750 ms con motores apagados para medir la inercia completa.
- Giroscopio: reintenta motivo 4 detenido cada 5 s, contador IMUR e historial NVS de cinco arranques. Sin sesgo niega el giro por ángulo y la navegación cae a tiempo.
- Versiones: FW con hash/fecha, APK 0.4 con hash, ambas visibles en Sensores; script de comparación sin instalación. Cámara de prueba temporal con aviso; apertura normal vuelve a la dirección real.
- Autoridad láser: interruptor apagado por defecto, 400 mm en columnas 3..6, cámara elige lado y respalda lecturas inválidas. Solo probado en PC. Freno firmware conservado a 120 mm.

Pruebas: núcleo 20 programas / 1570 casos (antes 1540), firmware 152+241+15+34 = 442 (antes 425), servidor falso 42, verificador de versiones siete. Todas verdes, estrés 100000 comandos sin STOP perdido y hash de página correcto. APK y firmware compilan. Sanitizadores de GitHub Actions: success, ejecución 39548476207 (https://github.com/fernandosegrr/fsi_proyect/actions/runs/39548476207).

Pruebas nuevas: calentamiento +15°, atípico intermedio, datos buenos e inconsistentes; tope/NVS y aprendizaje con inercia de 200 ms; arranque movido/quieto, bloqueo del reintento con motores activos, recuperación e historial; respaldo a tiempo sin sesgo; FW en CAPS y lectores de versiones; autoridad apagada/encendida, 400/401 mm, caído/inválido/vencido, bordes de columnas 2 a 7 y rejilla de 32 columnas, elección lateral por cámara y protección del freno anterior de 120 mm.

Fase 6 y versiones, 21 oct, 07:28 a 07:38:
- Revisión de producto 4233201, publicada antes de cargar. FW:4233201-20261021 compilado y cargado al DevKit conocido, única placa serie, COM6. Se usó cargar.ps1 nuevo a 921600. No hubo borrado completo de flash; verify-flash pasó para bootloader, particiones, boot_app0 y aplicación. ANT se conservó exactamente en 121,150,20,20 antes y después.
- APK compilada: APP:0.4-4233201, versionName 0.4, versionCode 3. Archivo listo: app_android/app/build/outputs/apk/debug/app-debug.apk. El hash del APK compilado coincide con el del FW y la revisión cargada. Esto no implica que esté instalada.
- Se intentó instalar solo con adb install -r. El teléfono mostró un bloqueo por PIN; no hubo botón de instalación accesible. No se escribió ningún código, contraseña ni cuenta. Se canceló el cliente de instalación por el PID exacto 17368. Quedó instalada la app anterior 0.3, versionCode 2, sin hash verificable de J13. No se desinstaló ni se borraron sus datos. El límite de cinco toques no se agotó porque la excepción expresa de no resolver PIN impide seguir.
- Verificación por USB a las 07:35:11: CAPS:IMU=1,TOF=1,TOFR=1,IMUR=0,IMUM=0,IMUE=2,FW:4233201-20261021. Giroscopio listo, sesgo -0.48 grados/s; láser vivo y lectura válida de 253 mm (antes 252; no había una referencia medida). ARR:5,4,11101,1,3,0,00000,0. Los reintentos TOF se anotaron durante reinicios de diagnóstico/carga; no son cinco encendidos normales con powerbank. La prueba final de reset añadió después otro arranque.
- LOOP:12858,1, máximo 12.858 ms y un exceso de 5 ms. Antes de cargar: 2.126 ms, cero excesos. Causa no determinada; pendiente aislarlo en reposo antes de medirlo en marcha. No se afirma que el límite de 5 ms haya pasado en esta verificación.
- La pantalla Sensores de la app nueva, sus versiones visibles y la comparación completa de FW/APK instalado con verificar_version.py quedaron pendientes por el PIN. No se tomaron capturas; no se vio ni guardó video. No se mandó ningún comando de movimiento al hardware.

Cierre y alimentación:
- Teléfono: se guardó antes de tocarlo stay_on_while_plugged_in=2 y pantalla encendida. Al cerrar se ejecutó am force-stop com.fsi.carro, se restauró y comprobó el valor 2, se retiró el archivo temporal de UI y se envió reboot -p. La orden fue aceptada; después adb devices no muestra el teléfono. Se cerró el servidor adb. No se dejaron wake-locks nuevos.
- No se pudo cortar físicamente la alimentación del DevKit. Está en el puerto 2 del concentrador raíz de Windows; uhubctl no está instalado y su documentación declara que la conmutación no funciona en Windows con el controlador habitual (https://github.com/mvp/uhubctl). Se intentó deshabilitar únicamente USB\\VID_10C4&PID_EA60\\0001 con pnputil: Acceso denegado. No se alteró el concentrador ni otros dispositivos. Deshabilitar/reiniciar un dispositivo tampoco demuestra por sí mismo que VBUS quede apagado; referencia de Microsoft: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/usbioctl/ni-usbioctl-ioctl_usb_hub_cycle_port.
- Alternativa autorizada: DevKit detenido en reset por RTS afirmada (bajo físico), con DTR desactivada. Se verificó silencio al consultar CAPS durante tres segundos; al soltar RTS arrancó FW:4233201-20261021; al afirmarla de nuevo hubo otros tres segundos sin respuesta. El helper conserva la línea afirmada. No se afirma que los 5 V estén cortados.
- Proceso de reset: Python PID 19532, lanzador PID 14244. Script: C:\Users\guerr\AppData\Local\Temp\j13_mantener_reset.py. Estado: C:\Users\guerr\AppData\Local\Temp\j13_reset_estado.json. Es el único helper que se deja en segundo plano; Gradle y servidores de prueba/adb detenidos.
- Para reanimar: ejecutar Stop-Process -Id 19532,14244 -ErrorAction SilentlyContinue y desconectar/reconectar USB. Cerrar el helper antes de consultar o cargar COM6. La forma de eliminar por completo la alimentación que aquí no se pudo conmutar es desconectar físicamente el USB.

Pendiente, en orden:
1. Encender y desbloquear el teléfono; instalar la APK 0.4 con adb install -r y comprobar FW/APK y pantalla Sensores. Mantener Autoridad del láser apagada.
2. Repetir LOOP en reposo y aislar el pico de 12.858 ms. Arranques quietos/movidos para recuperación del sesgo y contadores.
3. Calibración con ocho giros, reposicionando el carro entre tandas; revisar pares, descarte, repetibilidad y Deshacer.
4. Anticipación: aprendizaje y diez giros por lado con cinta, incluidos primeros tras reposo. No hace falta borrar NVS para empezar.
5. LOOP con movimiento y tras recuperación; rutas en L, caja y pérdida de WiFi.
6. Autoridad láser: comparar apagado/encendido en carril central, laterales y objetos fuera del haz; mantener apagada hasta esta validación.

Fuera de alcance: J10c no se corrigió; persiste la regla que acepta una franja casi del ancho completo como segundo color de piso y puede confundir un obstáculo ancho. El modelo de objetos sigue apagado por defecto.

Al volver: liberar el reset y reconectar USB, encender/desbloquear el teléfono e instalar primero la APK nueva. Consultar docs/progreso_J13.md y la sección J13 sin movimiento de docs/validacion_sensores.md antes de conectar motores. No ejecutar giros ni rutas sin colocar y despejar el carro. No usar el comportamiento nuevo de la app hasta instalarla.
