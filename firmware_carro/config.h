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
#define COLA_COMANDOS_MAX 32

/* ── SENSORES (J11): giroscopio MPU6050 y sensor láser VL53L0X ────────────
 * Los dos módulos van en el MISMO bus I2C del DevKit, alimentados con 3V3 (cada módulo trae sus resistencias de pull-up):
 *   SDA = GPIO 21, SCL = GPIO 22.   MPU6050 (GY-521) = 0x68,  VL53L0X (VL53LXX-V2) = 0x29.
 * El código que decide (imu.c, tof.c, giro.c, sensores.c) es C puro; solo el .ino toca el bus (funciones hw_..., ver hw_sensores.h). */
#define PIN_I2C_SDA              21
#define PIN_I2C_SCL              22
#define I2C_FRECUENCIA_HZ        400000   /* 400 kHz: leer 2 bytes del giroscopio tarda ~0.2 ms */
#define I2C_TIMEOUT_MS           5        /* una transacción colgada se corta a los 5 ms: loop() nunca se queda esperando al bus */
#define IMU_DIRECCION            0x68
#define TOF_DIRECCION            0x29

/* MPU6050: el WHO_AM_I oficial es 0x68; algunos clones contestan 0x70 o 0x72 (se aceptan los tres y se registra cuál se leyó). */
#define IMU_QUIEN_1              0x68
#define IMU_QUIEN_2              0x70
#define IMU_QUIEN_3              0x72

/* Sentido del ángulo: IMU_SIGNO = 1 si girar a la IZQUIERDA (antihorario visto desde arriba) da ángulo POSITIVO con el montaje de config (eje Z hacia arriba).
 * Si en la prueba física sale al revés, se pone -1 aquí y nada más cambia (ver docs/validacion_sensores.md). */
#define IMU_SIGNO                1

/* Muestreo del giroscopio: ±500 °/s (65.5 cuentas por °/s), filtro digital de ~44 Hz, 100 muestras por segundo, solo el eje Z. */
#define IMU_PERIODO_US           10000u   /* 100 Hz */
#define IMU_DT_MAX_US            100000u  /* si loop() se atrasó más de esto entre dos muestras, no se integra el hueco entero */
#define IMU_ZONA_MUERTA_MDPS     150      /* por debajo de 0.15 °/s (ya sin el sesgo) la velocidad se toma como 0: el ruido no suma ángulo */

/* Sesgo (bias) del giroscopio: al arrancar, con el carro quieto, se promedian IMU_SESGO_MUESTRAS lecturas y se acepta el promedio solo si
 * la desviación es pequeña (el carro no se movió) y el sesgo no es absurdo; si no, se reintenta hasta IMU_SESGO_REINTENTOS veces y después el giroscopio queda en IMU_MAL. */
#define IMU_SESGO_MUESTRAS       250      /* 2.5 s a 100 Hz */
#define IMU_SESGO_REINTENTOS     3
#define IMU_SESGO_DESV_MAX_LSB   12       /* desviación estándar máxima de las lecturas (un giroscopio quieto da ~2 a 4) */
#define IMU_SESGO_MAX_LSB        1000     /* sesgo máximo aceptable (~15 °/s) */
#define IMU_CAL_QUIETO_MS        300      /* el carro debe llevar así de tiempo con los motores apagados para calibrar (que pase la vibración) */

/* Actualización lenta del sesgo con el carro detenido (ZUPT): con los motores apagados más de IMU_ZUPT_QUIETO_MS y IMU_ZUPT_ESTABLES lecturas
 * seguidas casi iguales, el sesgo se acerca 1/IMU_ZUPT_DIV de la diferencia en cada muestra. */
#define IMU_ZUPT_QUIETO_MS       400
#define IMU_ZUPT_UMBRAL_LSB      20       /* la lectura debe estar a menos de esto del sesgo */
#define IMU_ZUPT_ESTABLES        20
#define IMU_ZUPT_DIV             256

/* Fallos: IMU_ERRORES_MAX errores seguidos del bus (o lecturas locas) marcan el giroscopio como caído. Una lectura idéntica IMU_CONGELADO_MUESTRAS veces seguidas
 * (un giroscopio vivo siempre tiene algo de ruido) también. */
#define IMU_ERRORES_MAX          3
#define IMU_CONGELADO_MUESTRAS   200

/* GIRO POR ÁNGULO (comando GIRAR:<grados>): positivo = izquierda, de GIRO_MIN_GRADOS a GIRO_MAX_GRADOS. Un pivote con un solo motor; se frena cuando
 * (ángulo girado + velocidad x anticipación) llega al objetivo. La anticipación es cuánto sigue girando el carro después de frenar (inercia) en ms de velocidad actual; empieza en GIRO_ANTICIPACION_MS y,
 * si GIRO_ANTICIPACION_AUTO es 1, se corrige sola después de cada giro con lo que giró de más o de menos (por lado, tope GIRO_ANTICIPACION_MAX_MS).
 * Después de frenar se sigue integrando GIRO_ASENTAR_MS para informar el ángulo FINAL real. El giro tiene su propio tiempo máximo y NO depende del watchdog de 1 s. */
#define GIRO_MIN_GRADOS          5
#define GIRO_MAX_GRADOS          200
#define GIRO_TIEMPO_BASE_MS      500
#define GIRO_TIEMPO_POR_GRADO_MS 15
#define GIRO_ANTICIPACION_MS     20
#define GIRO_ANTICIPACION_AUTO   1
#define GIRO_ANTICIPACION_MAX_MS 150
/* La anticipación es INDEPENDIENTE por sentido (izquierda y derecha) y por perfil (AUTO y MANUAL): cuatro valores. Cada uno es la MEDIANA de las últimas GIRO_ANT_MUESTRAS lecturas
 * de «cuántos ms de anticipación habrían dado justo el ángulo pedido» (la anticipación usada más el error final dividido entre la velocidad); solo aprenden los giros de al menos
 * GIRO_ANT_MIN_GRADOS (con uno chico la velocidad no llega a su valor normal). Se guardan en la memoria no volátil cuando cambian GIRO_ANT_GUARDAR_DIF_MS o más (así no se gasta la
 * memoria) y al arrancar se leen. El comando ANT_BORRAR los vuelve a GIRO_ANTICIPACION_MS y borra lo guardado. */
#define GIRO_ANT_MUESTRAS        5
#define GIRO_ANT_MIN_GRADOS      30
#define GIRO_ANT_GUARDAR_DIF_MS  3
#define GIRO_ASENTAR_MS          200

/* SENSOR LÁSER VL53L0X: una lectura cada TOF_PERIODO_MS sin bloquear loop(). 8190, 65535, valores sobre TOF_LIBRE_MM o "sin eco" significan LIBRE (no hay nada a la vista).
 * Parada de emergencia (solo perfil AUTO, solo avanzando): TOF_PARO_LECTURAS lecturas válidas seguidas menores de TOF_PARO_MM frenan con __STOP_AUTO y avisan TOF_PARO:<mm>.
 * Tres errores seguidos, o más de TOF_SIN_DATOS_MS sin ninguna lectura, marcan el sensor como caído. */
#define TOF_PERIODO_MS           33       /* tiempo de medida del sensor (el valor por defecto de la librería de Pololu) */
#define TOF_SONDEO_MS            4        /* mientras espera la medida, pregunta si está lista cada tantos ms (no en cada vuelta, para no llenar el bus) */
#define TOF_PARO_MM              120
#define TOF_PARO_LECTURAS        2
#define TOF_LIBRE_MM             1200
#define TOF_MIN_VALIDO_MM        8        /* menos de esto es basura del sensor: se ignora (ni libre ni cerca) */
#define TOF_ERRORES_MAX          3
#define TOF_INTENTOS_INICIO      5        /* init() de la librería de Pololu falla a veces justo al encender: se reintenta (los reintentos se cuentan: CAPS y ARR?) */
#define TOF_PAUSA_INICIO_MS      100
/* Si el láser no inicia o se cae, se vuelve a intentar cada TOF_REINTENTO_MS. El intento es barato (¿contesta en 0x29?) y solo si contesta se llama a la inicialización de la librería, que sí
 * espera al sensor unas decenas de ms: por eso solo se hace con el carro detenido (TOF_REINTENTO_QUIETO_MS sin motores y sin giro). Se recuerdan los reintentos de los últimos TOF_HIST_ARRANQUES arranques. */
#define TOF_REINTENTO_MS         5000
#define TOF_REINTENTO_QUIETO_MS  300
#define TOF_HIST_ARRANQUES       5
#define TOF_SIN_DATOS_MS         500

/* Mensajes hacia la app: SENS cada SENS_PERIODO_MS mientras haya cliente y perfil AUTO. loop() no debe quedarse detenido más de LOOP_MAX_US. */
#define SENS_PERIODO_MS          100
#define LOOP_MAX_US              5000u
#define MENSAJE_SALIDA_MAX       56       /* largo máximo (con el fin de texto) de un mensaje que sale hacia la app o el puerto serie */
#define COLA_SALIDA_MAX          16

#endif /* CONFIG_H */
