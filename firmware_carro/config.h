/*
 * config.h — Todos los números "ajustables" del carro en un solo lugar.
 *
 * Este archivo es C puro (sin Arduino.h) para poder usarse tanto desde
 * carro.c como desde firmware_carro.ino y desde las pruebas de PC.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ── WiFi ─────────────────────────────────────────────────────────────────
 * El nombre (WIFI_SSID) y la contraseña (WIFI_PASSWORD) NO están en este
 * archivo, porque el repositorio es público. Viven en:
 *   - secretos.h          : los valores reales. Está en .gitignore y NO se
 *                           sube a GitHub. Se crea copiando secretos.ejemplo.h.
 *   - secretos.ejemplo.h  : valores de ejemplo, sí versionado. Se usa solo si
 *                           no existe secretos.h (así el proyecto compila).
 */
#if defined(__has_include)
#  if __has_include("secretos.h")
#    include "secretos.h"
#  else
#    include "secretos.ejemplo.h"
#  endif
#else
#  include "secretos.ejemplo.h"
#endif

/* El ESP32 crea su propia red; su IP es 192.168.4.1 (ver abajo las direcciones y el DHCP). Servidor HTTP en el puerto 80, WebSocket en la ruta /ws. */
#define PUERTO_HTTP    80
#define RUTA_WEBSOCKET "/ws"

/* DIRECCIONES DE LA RED DEL CARRO (192.168.4.x). El DevKit es 192.168.4.1; la cámara usa la 192.168.4.2 FIJA (no pide dirección). El servidor DHCP del DevKit
 * reparte desde la .10 (la API de Arduino-ESP32 3.x da DHCP_CUANTAS direcciones: .10 a .20), así la .2 queda libre para la cámara aunque el teléfono se conecte antes.
 * Los números son el último byte de la dirección; el .ino arma 192.168.4.<n>. */
#define IP_RED_BASE_1       192
#define IP_RED_BASE_2       168
#define IP_RED_BASE_3         4
#define IP_AP_OCTETO          1   /* el DevKit (y la puerta de enlace) */
#define IP_CAMARA_OCTETO      2   /* la cámara: IP fija, fuera del rango del DHCP */
#define DHCP_INICIO_OCTETO   10   /* primera dirección que reparte el DHCP */
#define DHCP_CUANTAS         11   /* el core reparte inicio .. inicio+10 (dato del core 3.3.12: lease.end = start + 10) */
#define DHCP_FIN_OCTETO      (DHCP_INICIO_OCTETO + DHCP_CUANTAS - 1)   /* 20 */

/* Potencia de transmisión WiFi: 13 dBm reduce picos de corriente que algunas
 * powerbanks interpretan como cortocircuito. WIFI_POWER_13dBm lo define la
 * librería de WiFi, así que solo se expande dentro del .ino. */
#define WIFI_POTENCIA_TX WIFI_POWER_13dBm

/* Canal WiFi FIJO del punto de acceso (1, 6 u 11 son los que no se traslapan). Con un canal fijo la cámara, que se conecta como
 * cliente, no se queda buscando el canal después de una caída. */
#define WIFI_CANAL 1

/* Frecuencia de CPU (MHz): 160 ahorra corriente frente a los 240 por defecto. */
#define CPU_MHZ 160

/* ── Pines de los motores (ESP32 → driver TC1508A) ────────────────────────
 * "A" y "B" son el nombre de la patita IN del driver, NO el sentido de giro.
 * Cada motor avanza con una patita distinta (cableado probado en hardware):
 *   - motor IZQUIERDO avanza con el pin B (GPIO 26) y retrocede con el A.
 *   - motor DERECHO   avanza con el pin A (GPIO 25) y retrocede con el B.
 * Regla: nunca poner PWM en A y B del mismo motor a la vez.
 */
#define PIN_MOTOR_IZQ_A 27   /* IN1 del driver */
#define PIN_MOTOR_IZQ_B 26   /* IN2 del driver */
#define PIN_MOTOR_DER_A 25   /* IN3 del driver */
#define PIN_MOTOR_DER_B 33   /* IN4 del driver */

/* ── PWM de los motores ───────────────────────────────────────────────────
 * 1500 Hz da más torque bajo carga que 5000 Hz (a costa de algo de zumbido).
 * 8 bits → valores de 0 a 255. */
#define PWM_FRECUENCIA_HZ 1500
#define PWM_BITS          8

/* ── Velocidades (0 a 255) ────────────────────────────────────────────────
 * Siempre al máximo para vencer la fricción con el peso del carro.
 * Se mantienen como dos constantes porque así estaba el original. */
#define VELOCIDAD        255   /* adelante y atrás */
#define VELOCIDAD_GIRO   255   /* motor activo en un pivote */

/* ── PERFILES DE VELOCIDAD (0 a 255) ───────────────────────────────────────
 * El carro tiene DOS perfiles. Solo cambian la potencia al AVANZAR/RETROCEDER después del arranque; los pivotes (VELOCIDAD_GIRO) y el
 * arranque a plena potencia (VELOCIDAD) son iguales en los dos.
 *   PERFIL_MANUAL: velocidad máxima, para manejar con los botones de la app. Ajuste recto VALIDADO a 255 (a 150 cm: 3 cm de desvío).
 *   PERFIL_AUTO:   velocidad limitada (210), para las rutas del modo semiautónomo y la prueba de obstáculos: a menos velocidad el carro
 *                  recorre menos mientras llega el STOP (el frenado ante un obstáculo mejora). Comprobado en el carro: arranca, va más
 *                  lento y frena bien. El ajuste recto de este perfil (207/210) es el del manual escalado y está PENDIENTE de medir.
 * Cómo se elige: la app manda MODO_MANUAL o MODO_AUTO (sin parámetros; el carro contesta ACK_MANUAL o ACK_AUTO). Si el carro se mueve al
 * cambiar de perfil, primero se detiene (STOP). Al ARRANCAR, al conectarse o reconectarse el WebSocket y tras el watchdog el perfil es
 * siempre AUTO (el más seguro): la app pide MODO_MANUAL explícitamente cuando quiere correr a máxima velocidad.
 *
 * Ajuste para ir en línea recta (cada perfil tiene el suyo): al avanzar o retroceder cada lado puede llevar una potencia distinta para
 * compensar que un lado empuje más que el otro (llantas, motores). El lado que empuja MÁS se baja: si el carro se desvía a la DERECHA,
 * baja VELOCIDAD_IZQ; si se desvía a la IZQUIERDA, baja VELOCIDAD_DER. Medido a 255 a 150 cm: con 255/255 se desviaba 14.5 cm a la derecha,
 * con 248 se pasó 16 cm a la izquierda y con 252 (más el arranque a plena potencia) quedó a 3 cm.
 *
 * ARRANQUE_PLENO_MS: al ARRANCAR avanzando o retrocediendo los dos motores van al 100 % durante este tiempo y después se aplica el ajuste
 * de lado del perfil. Con menos de 255 desde parado los motores solo zumbaban sin arrancar; con el arranque a plena potencia sí arrancan
 * (probado a 210). Con 0 no hay arranque a plena potencia. */
#define PERFIL_MANUAL                    0   /* índices de los perfiles */
#define PERFIL_AUTO                      1

/* EQUILIBRIO DE LOS LADOS (BALANCE). Cada perfil tiene un tope (VELOCIDAD_DER, el lado derecho) y un FACTOR del lado izquierdo en milésimas: la potencia izquierda es
 * tope x factor / 1000 (redondeada). El factor nunca sube la potencia por encima del tope: solo REDUCE el lado que empuja más (el izquierdo, que es el más rápido en este carro). Va
 * de BALANCE_MIN_MIL a BALANCE_MAX_MIL (1000 = los dos lados iguales). Los DEFECTO de abajo son los de siempre (252/255 y 207/210); la app los cambia con BAL_AUTO:NNN y BAL_MANUAL:NNN
 * (ver carro.h) después de la prueba de recta, y el valor queda guardado en la memoria no volátil del carro (hw_balance_guardar). Con la prueba de recta de la app salen valores
 * cerca de 900 (el izquierdo ~10 % más rápido que el derecho). PERFIL_x_VELOCIDAD_IZQ es la potencia izquierda con el factor por defecto (la usan las pruebas). */
#define BALANCE_MIN_MIL               800
#define BALANCE_MAX_MIL              1000

#define PERFIL_MANUAL_VELOCIDAD_DER    255   /* el tope del perfil */
#define PERFIL_MANUAL_BALANCE_DEFECTO  988   /* validado a 255: 252 el izquierdo, 255 el derecho */
#define PERFIL_MANUAL_VELOCIDAD_IZQ    ((PERFIL_MANUAL_VELOCIDAD_DER * PERFIL_MANUAL_BALANCE_DEFECTO + 500) / 1000)   /* 252 */
#define PERFIL_MANUAL_ARRANQUE_PLENO_MS 250u

#define PERFIL_AUTO_VELOCIDAD_DER      210
#define PERFIL_AUTO_BALANCE_DEFECTO    986   /* 210 * 252 / 255 (mismo cociente izquierdo/derecho que el validado a 255) */
#define PERFIL_AUTO_VELOCIDAD_IZQ      ((PERFIL_AUTO_VELOCIDAD_DER * PERFIL_AUTO_BALANCE_DEFECTO + 500) / 1000)   /* 207 */
#define PERFIL_AUTO_ARRANQUE_PLENO_MS  250u

/* ARRANQUE ESCALONADO (opcional, apagado por defecto = 0): si es mayor que 0, al arrancar avanzando o retrocediendo primero entra el motor IZQUIERDO y el DERECHO entra esta cantidad de
 * ms después, para repartir el pico de corriente del arranque (los dos motores a plena potencia a la vez piden mucha corriente de golpe). Un valor típico sería 30 a 60 ms. Los pivotes no
 * se afectan (usan un solo motor). Se puede probar sin tocar este archivo compilando con -DARRANQUE_ESCALONADO_MS=40 (ver pruebas_firmware/Makefile). */
#ifndef ARRANQUE_ESCALONADO_MS
#define ARRANQUE_ESCALONADO_MS 0u
#endif

/* ── Tiempos (milisegundos) ───────────────────────────────────────────────
 * Si se cambia TIEMPO_WATCHDOG_MS hay que revisar el heartbeat de pagina.h
 * (250 ms): debe seguir siendo bastante menor. */
#define TIEMPO_WATCHDOG_MS     1000u  /* sin comando real → freno automático */
#define KEEPALIVE_PERIODO_MS   2000u  /* cada cuánto se da el pulso */
#define KEEPALIVE_PULSO_MS       50u  /* cuánto dura el pulso en alto */
#define WATCHDOG_HW_MS         5000   /* reinicio del chip si loop() se cuelga */

/* ── Pin keep-alive para la powerbank ─────────────────────────────────────
 * GPIO 2: pulso corto cada 2 s para que la powerbank no apague la salida.
 * (Qué hay conectado ahí está pendiente de confirmar; ver docs.) */
#define PIN_KEEPALIVE 2

/* ── Comandos de texto ────────────────────────────────────────────────────
 * Tamaño del buffer para un comando, incluido el '\0' final: se aceptan
 * hasta LARGO_MAX_COMANDO-1 caracteres. Lo usan carro.c y el .ino. */
#define LARGO_MAX_COMANDO 32
#define COLA_COMANDOS_MAX  8

#endif /* CONFIG_H */
