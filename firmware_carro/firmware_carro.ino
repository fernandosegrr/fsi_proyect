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

#include "config.h"
#include "carro.h"

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

/* Eventos del WebSocket: conexión, desconexión y mensajes de texto. */
static void al_evento_websocket(AsyncWebSocket *servidor, AsyncWebSocketClient *client,
                                AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT) {
    /* Solo un cliente a la vez: cerrar los demás. */
    for (auto &c : ws.getClients()) {
      if (c.id() != client->id()) c.close();
    }
    carro_detener();  /* estado limpio al conectar */
    carro_poner_perfil_auto();   /* y siempre en el perfil AUTO: la app pide MODO_MANUAL si lo quiere */
    Serial.printf("WS Cliente conectado: #%u\n", client->id());

  } else if (type == WS_EVT_DISCONNECT) {
    carro_detener();
    carro_poner_perfil_auto();
    Serial.println("WS Desconectado — freno de emergencia activado.");

  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    /* Solo frames de texto completos y en un solo pedazo. */
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      char comando[LARGO_MAX_COMANDO];
      size_t n = (len < LARGO_MAX_COMANDO - 1) ? len : (LARGO_MAX_COMANDO - 1);
      memcpy(comando, data, n);
      comando[n] = '\0';
      carro_recibir_comando(comando);
      /* PING → PONG (la app mide la latencia con el ida y vuelta) y MODO_MANUAL/MODO_AUTO → ACK_MANUAL/ACK_AUTO: la respuesta la decide carro.c. */
      const char *respuesta = carro_respuesta(comando);
      if (respuesta != NULL) {
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
  carro_iniciar();   /* motores a 0 y estado "STOP" */

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

void loop()
{
  esp_task_wdt_reset();
  ws.cleanupClients();

  carro_vigilar_comunicacion();
  carro_actualizar_keepalive();
}
