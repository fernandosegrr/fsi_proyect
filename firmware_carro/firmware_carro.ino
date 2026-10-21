/*
 * firmware_carro.ino — Parte del firmware que NO puede ser C.
 *
 * POR QUÉ ESTO ES C++: las librerías WiFi, AsyncTCP, ESPAsyncWebServer y
 * AsyncWebSocket están escritas en C++ (clases, objetos, callbacks), y las
 * funciones de Arduino/ESP32 (ledcWrite, millis, Serial...) también se
 * declaran en C++. Por eso setup(), loop(), el WiFi, el servidor, el
 * WebSocket y las funciones hw_... viven aquí.
 *
 * Este archivo NO decide nada sobre los motores: solo recibe mensajes y se
 * los pasa a carro.c (carro_recibir_comando), que es C estándar, y manda de
 * vuelta lo que carro.c pide (PING → PONG). Las únicas "decisiones" que quedan
 * aquí son las que obliga la librería WebSocket (aceptar un solo cliente y
 * validar el formato del frame).
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "pagina.h"
#include <esp_task_wdt.h>
#include <Preferences.h>   /* memoria no volátil (NVS) del ESP32: ahí se guarda el balance de cada perfil */
#include <Wire.h>          /* bus I2C del giroscopio MPU6050 y del láser VL53L0X */
#include <VL53L0X.h>       /* librería de Pololu (licencia MIT), solo para el láser; el MPU6050 va con registros directos */

#include "config.h"
#include "carro.h"
#include "hw_sensores.h"
#include "sensores.h"

AsyncWebServer server(PUERTO_HTTP);
AsyncWebSocket ws(RUTA_WEBSOCKET);

/* ── Funciones hw_... : lo único que toca el ESP32 en nombre de carro.c ──── */
extern "C" void hw_motor_pwm(int pin, int valor)
{
  ledcWrite(pin, valor);
}

extern "C" uint32_t hw_millis(void)
{
  return (uint32_t)millis();
}

extern "C" void hw_keepalive(int nivel)
{
  digitalWrite(PIN_KEEPALIVE, nivel ? HIGH : LOW);
}

extern "C" void hw_registrar(const char *texto)
{
  Serial.println(texto);
}

/* Balance de cada perfil (equilibrio de los lados al ir recto): se guarda en la memoria no volátil para que sobreviva a apagar el carro. */
static Preferences memoria;

static const char *clave_balance(int perfil)
{
  return (perfil == PERFIL_MANUAL) ? "balManual" : "balAuto";
}

extern "C" int hw_balance_leer(int perfil)
{
  int valor = 0;
  if (memoria.begin("carro", true)) {   /* solo lectura */
    valor = memoria.getInt(clave_balance(perfil), 0);
    memoria.end();
  }
  return valor;
}

/* Historial de reintentos del láser en los últimos arranques (número empaquetado, ver tof.c). */
extern "C" int hw_tof_hist_leer(void)
{
  int valor = 0;
  if (memoria.begin("carro", true)) {
    valor = memoria.getInt("tofHist", 0);
    memoria.end();
  }
  return valor > 0 ? valor - 1 : -1;
}

extern "C" void hw_tof_hist_guardar(int valor)
{
  if (memoria.begin("carro", false)) {
    memoria.putInt("tofHist", valor + 1);
    memoria.end();
  }
}

/* Anticipación del giro por ángulo (4 ranuras: perfil x sentido). Se guarda ms + 1 para distinguir «0 ms» de «nada guardado». */
extern "C" int hw_imu_hist_leer(void)
{
  int valor = 0;
  if (memoria.begin("carro", true)) {
    valor = memoria.getInt("imuHist", 0);
    memoria.end();
  }
  return valor > 0 ? valor - 1 : -1;
}

extern "C" void hw_imu_hist_guardar(int valor)
{
  if (memoria.begin("carro", false)) {
    memoria.putInt("imuHist", valor + 1);
    memoria.end();
  }
}

static const char *clave_anticipacion(int ranura)
{
  static const char *claves[4] = { "antManD", "antManI", "antAutD", "antAutI" };
  return claves[ranura & 3];
}

extern "C" int hw_ant_leer(int ranura)
{
  int valor = 0;
  if (memoria.begin("carro", true)) {
    valor = memoria.getInt(clave_anticipacion(ranura), 0);
    memoria.end();
  }
  return valor > 0 ? valor - 1 : -1;
}

extern "C" void hw_ant_guardar(int ranura, int ms)
{
  if (memoria.begin("carro", false)) {
    memoria.putInt(clave_anticipacion(ranura), ms + 1);
    memoria.end();
  }
}

extern "C" void hw_ant_borrar(void)
{
  if (memoria.begin("carro", false)) {
    for (int r = 0; r < 4; r++) {
      memoria.remove(clave_anticipacion(r));
    }
    memoria.end();
  }
}

extern "C" void hw_balance_guardar(int perfil, int milesimas)
{
  if (memoria.begin("carro", false)) {
    memoria.putInt(clave_balance(perfil), milesimas);
    memoria.end();
  }
}

/* ── Sensores I2C (giroscopio MPU6050 y láser VL53L0X) ─────────────────────
 * Todo el acceso al bus ocurre desde loop() (vía sensores_actualizar) y desde setup(). Wire tiene un tiempo máximo de I2C_TIMEOUT_MS por transacción. */
static VL53L0X laser;

extern "C" uint32_t hw_micros(void)
{
  return (uint32_t)micros();
}

extern "C" void hw_demora_ms(uint32_t ms)
{
  delay(ms);
}

extern "C" int hw_i2c_leer(uint8_t dir, uint8_t reg, uint8_t *datos, int n)
{
  Wire.beginTransmission(dir);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {   /* false = reinicio repetido, sin soltar el bus */
    return 1;
  }
  if (Wire.requestFrom((int)dir, n, (int)true) != n) {
    return 2;
  }
  for (int i = 0; i < n; i++) {
    datos[i] = (uint8_t)Wire.read();
  }
  return 0;
}

extern "C" int hw_i2c_escribir(uint8_t dir, uint8_t reg, const uint8_t *datos, int n)
{
  Wire.beginTransmission(dir);
  Wire.write(reg);
  for (int i = 0; i < n; i++) {
    Wire.write(datos[i]);
  }
  return Wire.endTransmission() == 0 ? 0 : 1;
}

extern "C" int hw_i2c_presente(uint8_t dir)
{
  Wire.beginTransmission(dir);
  return Wire.endTransmission() == 0 ? 1 : 0;
}

/* Láser: init() de Pololu espera al sensor (solo en setup). Después se mide de corrido y SIN esperar: se pregunta si hay medida lista (un registro) y solo entonces se lee. */
extern "C" int hw_tof_iniciar(void)
{
  laser.setTimeout(20);
  /* UN solo intento: los reintentos (y su cuenta) los hace tof.c, que también reintenta cada TOF_REINTENTO_MS si el láser se cae. */
  if (!laser.init()) {
    return 0;
  }
  laser.setMeasurementTimingBudget((uint32_t)TOF_PERIODO_MS * 1000u);
  laser.startContinuous(0);   /* una medida detrás de otra */
  return 1;
}

extern "C" int hw_tof_listo(void)
{
  const uint8_t estado = laser.readReg(VL53L0X::RESULT_INTERRUPT_STATUS);
  if (laser.last_status != 0) {
    return -1;
  }
  return (estado & 0x07) ? 1 : 0;
}

extern "C" int hw_tof_leer(int *mm)
{
  const uint16_t valor = laser.readRangeContinuousMillimeters();   /* ya hay medida lista: no espera */
  if (laser.last_status != 0 || laser.timeoutOccurred()) {
    return 1;
  }
  *mm = (int)valor;
  return 0;
}

/* Abre el bus y dice qué contesta en las dos direcciones (para el diagnóstico de cableado). */
static void iniciar_i2c()
{
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_FRECUENCIA_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  Serial.printf("I2C: SDA=GPIO%d SCL=GPIO%d a %d Hz\n", PIN_I2C_SDA, PIN_I2C_SCL, I2C_FRECUENCIA_HZ);
  Serial.printf("I2C: MPU6050 0x%02X %s, VL53L0X 0x%02X %s\n", IMU_DIRECCION, hw_i2c_presente(IMU_DIRECCION) ? "RESPONDE" : "NO responde",
                TOF_DIRECCION, hw_i2c_presente(TOF_DIRECCION) ? "RESPONDE" : "NO responde");
  Serial.print("I2C: escaneo completo:");
  for (uint8_t dir = 1; dir < 127; dir++) {
    if (hw_i2c_presente(dir)) {
      Serial.printf(" 0x%02X", dir);
    }
  }
  Serial.println();
}

/* ── Inicialización de hardware ────────────────────────────────────────── */

/* Configura los 4 canales PWM de los motores (1500 Hz, 8 bits). */
static void hw_iniciar_pwm()
{
  ledcAttach(PIN_MOTOR_IZQ_A, PWM_FRECUENCIA_HZ, PWM_BITS);
  ledcAttach(PIN_MOTOR_IZQ_B, PWM_FRECUENCIA_HZ, PWM_BITS);
  ledcAttach(PIN_MOTOR_DER_A, PWM_FRECUENCIA_HZ, PWM_BITS);
  ledcAttach(PIN_MOTOR_DER_B, PWM_FRECUENCIA_HZ, PWM_BITS);
}

/* Watchdog de hardware: reinicia el chip si loop() deja de ejecutarse.
 *
 * El núcleo de Arduino-ESP32 3.x (IDF 5) YA inicia este vigilante (TWDT) antes de setup(); llamar a esp_task_wdt_init
 * daba el error "TWDT already initialized" y, peor, dejaba el tiempo del núcleo y no el de WATCHDOG_HW_MS. Por eso primero se
 * RECONFIGURA el que ya existe, y solo si todavía no estaba iniciado (ESP_ERR_INVALID_STATE) se inicia uno nuevo. Que esta
 * tarea (loop) ya estuviera suscrita (ESP_ERR_INVALID_ARG) tampoco es un error. */
static void iniciar_watchdog_hardware()
{
  const esp_task_wdt_config_t wdt_config = {
    .timeout_ms     = WATCHDOG_HW_MS,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  esp_err_t error = esp_task_wdt_reconfigure(&wdt_config);
  if (error == ESP_ERR_INVALID_STATE) {
    error = esp_task_wdt_init(&wdt_config);
  }
  if (error != ESP_OK) {
    Serial.printf("Watchdog de hardware: ERROR al configurar (%s)\n", esp_err_to_name(error));
  }
  error = esp_task_wdt_add(NULL);
  if (error != ESP_OK && error != ESP_ERR_INVALID_ARG) {
    Serial.printf("Watchdog de hardware: ERROR al suscribir loop() (%s)\n", esp_err_to_name(error));
  }
  Serial.printf("Watchdog de hardware: %d ms\n", WATCHDOG_HW_MS);
}

/* Crea la red WiFi propia del carro (punto de acceso). */
static void iniciar_wifi()
{
  /* Antes de crear la red se fija la IP del carro y dónde EMPIEZA el DHCP: .10 a .20, así la .2 (de la cámara) nunca se reparte. softAPConfig tiene que ir antes
   * de que se conecte nadie; el core devuelve false si el rango no cabe o se pisa con la IP del carro. */
  const IPAddress ip_ap(IP_RED_BASE_1, IP_RED_BASE_2, IP_RED_BASE_3, IP_AP_OCTETO);
  const IPAddress mascara(255, 255, 255, 0);
  const IPAddress dhcp_inicio(IP_RED_BASE_1, IP_RED_BASE_2, IP_RED_BASE_3, DHCP_INICIO_OCTETO);
  const bool dhcp_ok = WiFi.softAPConfig(ip_ap, ip_ap, mascara, dhcp_inicio);
  Serial.printf("DHCP: %s (reparte .%d a .%d; la .%d queda para la camara)\n", dhcp_ok ? "configurado" : "ERROR al configurar",
                DHCP_INICIO_OCTETO, DHCP_FIN_OCTETO, IP_CAMARA_OCTETO);

  /* Avisa a quién se le da cada dirección: sirve para comprobar que nadie recibe la .2. */
  WiFi.onEvent([](arduino_event_id_t evento, arduino_event_info_t info) {
    if (evento == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
      const IPAddress entregada(info.wifi_ap_staipassigned.ip.addr);
      Serial.printf("DHCP: entrego %s%s\n", entregada.toString().c_str(), entregada[3] == IP_CAMARA_OCTETO ? "  <-- ERROR: es la de la camara" : "");
    }
  });

  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CANAL);   /* canal fijo */
  WiFi.setSleep(false);                                 /* sin ahorro de energía del radio: menos retraso y menos tropiezos */
  WiFi.setTxPower(WIFI_POTENCIA_TX);
}

/* Configura el pin keep-alive como salida y lo deja en bajo. */
static void iniciar_keepalive_pin()
{
  pinMode(PIN_KEEPALIVE, OUTPUT);
  hw_keepalive(0);
}

/* ── Servidor HTTP ─────────────────────────────────────────────────────── */

/* GET / → envía la página de control (guardada en flash) sin caché. */
static void enviar_pagina(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response =
    request->beginResponse_P(200, "text/html", html);
  response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  response->addHeader("Pragma",  "no-cache");
  response->addHeader("Expires", "0");
  request->send(response);
}

/* Cualquier otra ruta (favicon, etc.) responde 404. */
static void responder_no_encontrado(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

/* ── WebSocket ─────────────────────────────────────────────────────────── */

static portMUX_TYPE mux_carro = portMUX_INITIALIZER_UNLOCKED;

void hw_bloquear(void)
{
  portENTER_CRITICAL(&mux_carro);
}

void hw_desbloquear(void)
{
  portEXIT_CRITICAL(&mux_carro);
}

/* Eventos del WebSocket: conexión, desconexión y mensajes de texto. */
static void al_evento_websocket(AsyncWebSocket *servidor, AsyncWebSocketClient *client,
                                AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT) {
    /* Solo un cliente a la vez: cerrar los demás. */
    for (auto &c : ws.getClients()) {
      if (c.id() != client->id()) c.close();
    }
    carro_bloquear();
    carro_conectar_cliente(client->id());
    carro_desbloquear();
    client->client()->setNoDelay(true);   /* TCP_NODELAY: el STOP y los comandos salen sin esperar a juntarse con otros datos (algoritmo de Nagle) */
    Serial.printf("WS Cliente conectado: #%u\n", client->id());

  } else if (type == WS_EVT_DISCONNECT) {
    int era_activo = 0;
    carro_bloquear();
    era_activo = carro_desconectar_cliente(client->id());
    carro_desbloquear();
    if (era_activo) {
      Serial.println("WS Desconectado — freno de emergencia activado.");
    } else {
      Serial.printf("WS Desconectado ignorado (cliente #%u no es el activo).\n", client->id());
    }

  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    /* Solo frames de texto completos y en un solo pedazo. */
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      char comando[LARGO_MAX_COMANDO];
      size_t n = (len < LARGO_MAX_COMANDO - 1) ? len : (LARGO_MAX_COMANDO - 1);
      memcpy(comando, data, n);
      comando[n] = '\0';
      int aceptado = 0;
      const char *respuesta = NULL;
      carro_bloquear();
      if (carro_cliente_activo() == 0 || client->id() == carro_cliente_activo()) {
        carro_encolar_comando(comando);
        /* PING → PONG (la app mide la latencia con el ida y vuelta) y MODO_MANUAL/MODO_AUTO → ACK_MANUAL/ACK_AUTO: la respuesta la decide carro.c. */
        respuesta = carro_respuesta(comando);
        aceptado = 1;
      }
      carro_desbloquear();
      if (aceptado && respuesta != NULL) {
        client->text(respuesta);
      }
    }
  }
}

/* ── Arranque y bucle principal ────────────────────────────────────────── */

void setup()
{
  Serial.begin(115200);
  setCpuFrequencyMhz(CPU_MHZ);
  Serial.println("CPU: 160 MHz");

  hw_iniciar_pwm();
  carro_poner_bloqueo(hw_bloquear, hw_desbloquear);
  carro_iniciar();   /* motores a 0 y estado "STOP" */

  iniciar_i2c();
  sensores_iniciar();   /* giroscopio (calibra el sesgo en loop, ~3 s con el carro quieto) y láser */

  iniciar_watchdog_hardware();

  iniciar_wifi();

  iniciar_keepalive_pin();
  Serial.print("AP Iniciado. IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, enviar_pagina);
  server.onNotFound(responder_no_encontrado);

  ws.onEvent(al_evento_websocket);
  server.addHandler(&ws);
  server.begin();
  Serial.println("Servidor listo.");
}

/* Saca los mensajes de sensores que hayan quedado en la cola de salida y los manda: al cliente WebSocket activo y/o por el puerto serie. */
static void enviar_mensajes_de_sensores()
{
  char texto[MENSAJE_SALIDA_MAX];
  int donde = 0;
  while (sensores_extraer_mensaje(texto, sizeof texto, &donde)) {
    if (donde & SENSORES_DESTINO_SERIE) {
      Serial.println(texto);
    }
    if (donde & SENSORES_DESTINO_WS) {
      carro_bloquear();
      const uint32_t id = carro_cliente_activo();
      carro_desbloquear();
      if (id != 0u) {
        ws.text(id, texto);
      }
    }
  }
}

/* Diagnóstico eléctrico del bus (comando PINES? por USB): suelta el bus y lee SDA y SCL como entradas, primero sin ayuda, luego con una resistencia hacia abajo y hacia arriba del chip.
 *   SDA/SCL = 1 sin ayuda  -> algún módulo ENCENDIDO los sube con su pull-up (bien).
 *   SDA/SCL = 0 aun con pull-up del chip -> algo los tiene pegados a GND (corto, módulo mal conectado).
 *   0 sin ayuda y 1 con pull-up del chip -> ningún módulo con energía los sube (módulos sin VCC o cable desconectado). */
static void diagnostico_pines()
{
  Wire.end();
  pinMode(PIN_I2C_SDA, INPUT);
  pinMode(PIN_I2C_SCL, INPUT);
  delay(5);
  const int sda_libre = digitalRead(PIN_I2C_SDA), scl_libre = digitalRead(PIN_I2C_SCL);
  pinMode(PIN_I2C_SDA, INPUT_PULLDOWN);
  pinMode(PIN_I2C_SCL, INPUT_PULLDOWN);
  delay(5);
  const int sda_abajo = digitalRead(PIN_I2C_SDA), scl_abajo = digitalRead(PIN_I2C_SCL);
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);
  pinMode(PIN_I2C_SCL, INPUT_PULLUP);
  delay(5);
  const int sda_arriba = digitalRead(PIN_I2C_SDA), scl_arriba = digitalRead(PIN_I2C_SCL);
  Serial.printf("PINES: libre SDA=%d SCL=%d | con resistencia hacia abajo SDA=%d SCL=%d | con resistencia hacia arriba SDA=%d SCL=%d\n",
                sda_libre, scl_libre, sda_abajo, scl_abajo, sda_arriba, scl_arriba);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_FRECUENCIA_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
}

/* Comando PRUEBAI2C? por USB: escanea el bus con SDA/SCL como están y CRUZADOS, a 100 kHz y a 400 kHz. Si algo aparece solo cruzado, los cables SDA y SCL están al revés. */
static void diagnostico_prueba_i2c()
{
  for (int cruzado = 0; cruzado < 2; cruzado++) {
    for (int velocidad = 0; velocidad < 2; velocidad++) {
      const int sda = cruzado ? PIN_I2C_SCL : PIN_I2C_SDA;
      const int scl = cruzado ? PIN_I2C_SDA : PIN_I2C_SCL;
      Wire.end();
      Wire.begin(sda, scl);
      Wire.setClock(velocidad ? 400000 : 100000);
      Wire.setTimeOut(I2C_TIMEOUT_MS);
      Serial.printf("PRUEBAI2C: SDA=GPIO%d SCL=GPIO%d %d kHz ->", sda, scl, velocidad ? 400 : 100);
      for (uint8_t dir = 1; dir < 127; dir++) {
        if (hw_i2c_presente(dir)) {
          Serial.printf(" 0x%02X", dir);
        }
      }
      Serial.println();
    }
  }
  Wire.end();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_FRECUENCIA_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
}

/* Comandos de diagnóstico por el cable USB (la PC no se conecta al WiFi del carro): líneas de texto terminadas en salto de línea. Solo los de sensores; NO mueven motores. */
static void leer_comandos_serie()
{
  static char linea[LARGO_MAX_COMANDO];
  static size_t largo = 0;
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (largo > 0) {
        linea[largo] = '\0';
        if (strcmp(linea, "PINES?") == 0) {
          diagnostico_pines();
        } else if (strcmp(linea, "PRUEBAI2C?") == 0) {
          diagnostico_prueba_i2c();
        } else {
          sensores_comando_serie(linea);
        }
      }
      largo = 0;
    } else if (largo < sizeof linea - 1) {
      linea[largo++] = c;
    } else {
      largo = 0;   /* línea demasiado larga: se descarta */
    }
  }
}

void loop()
{
  const uint32_t inicio_us = micros();

  esp_task_wdt_reset();
  ws.cleanupClients();

  sensores_actualizar();   /* único lugar de loop() donde se toca el bus I2C */
  carro_procesar_cola();
  carro_vigilar_comunicacion();
  carro_actualizar_keepalive();

  enviar_mensajes_de_sensores();
  leer_comandos_serie();

  sensores_registrar_loop_us((uint32_t)micros() - inicio_us);   /* LOOP? informa el peor caso */
}
