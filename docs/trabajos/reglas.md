MENSAJE 1 de 7 - REGLAS. Guarda este texto tal cual en docs/trabajos/reglas.md y responde solo "recibido 1/7". No hagas nada más hasta que llegue el mensaje EMPIEZA.

Contexto: carro de 4 ruedas, ESP32 DevKit (motores, TC1508A), ESP32 Freenove WROVER (cámara GC0308, IP 192.168.4.2), app Android (Huawei P40 Pro). Todo local, meta de latencia menor a 300 ms, sin encoders ni IMU ni GPS.

Datos medidos: cámara en HQVGA 240x176 (~12 fps, retraso 0.32 s medido) es el modo por defecto. QVGA se descarta porque se siente el retraso. QQVGA 160x120 es muy pequeño para detectar. El detector del teléfono tarda ~7 ms por cuadro. Los motores ya se probaron y responden bien.

Reglas:
1. original_backup/ no se toca nunca.
2. Nada de secretos en git: secretos.h sigue ignorado; sin archivos de más de 15 MB.
3. Commits con identidad noreply; push sin --force.
4. No mates procesos por búsqueda amplia; solo por PID o nombre exacto que tú lanzaste.
5. No toques Python global, PATH ni JAVA_HOME.
6. No mires ni guardes imágenes de la cámara; solo números.
7. La PC no se une al WiFi de las placas.
8. Lógica en C simple con comentarios en español muy claros; el equipo debe entenderla.
9. Cada cambio con pruebas en PC; corre todas antes de cada commit.
10. Haz por comandos todo lo posible (cargar.ps1, adb). El usuario solo conecta hardware. Lo que requiera hardware se marca PENDIENTE HARDWARE con los pasos.
11. Un commit por parte, mensaje claro en español. Actualiza docs/bitacora.md y docs/progreso.md.
12. Al terminar cada parte, resumen corto: qué quedó, qué pruebas pasan, qué falta del usuario. Sin recap de pasos.
13. No metas referencias al nombre anterior del proyecto en código ni docs nuevos (el SSID ya es CarroFSI).
