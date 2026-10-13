/*
 * camara_prueba.ino — Prueba de la cámara de la placa Freenove ESP32-WROVER (v3.0).
 *
 * GUÍA DE LECTURA
 *  1. setup(): (a) prueba los mapas de pines de la cámara uno por uno (una sola vez cada
 *     uno, sin bucles) y se queda con el primero que funciona; (b) mide la velocidad de la
 *     cámara a QVGA y a VGA SIN ningún cliente conectado; (c) se conecta al WiFi del carro
 *     como cliente, con IP fija; (d) arranca el servidor HTTP.
 *  2. Servidor: /  (página con el video), /capture (un JPEG), /stream (MJPEG) y
 *     /control?size=qvga|vga (cambia el tamaño).
 *  3. loop(): cada 5 s imprime por serie el estado, y revisa que siga la conexión WiFi.
 *  4. Mapas de pines: el oficial de Freenove para esta placa es CAMERA_MODEL_WROVER_KIT
 *     (Freenove_ESP32_WROVER_Board, Sketch_06.1_CameraWebServer/camera_pins.h, idéntico al del
 *     core de ESP32). Si falla se prueba AI_THINKER. Si ninguno funciona se avisa y se detiene.
 *  5. El sensor de esta placa resultó ser un GC0308 (0.3 Mpíxeles), que NO sabe hacer JPEG:
 *     la cámara se inicia en RGB565 y cada cuadro se comprime a JPEG por software en la ESP32
 *     (calidad 80 de 100), por eso los fps con JPEG son bastante menores que los de captura
 *     sola. Con RGB565 hay que reiniciar la cámara para cambiar de QVGA a VGA. Si el sensor
 *     fuera uno con JPEG (por ejemplo OV2640), el programa usa el JPEG del sensor, calidad 12.
 *  6. Privacidad: este programa solo imprime números (tamaños, tiempos, memoria); nunca imprime
 *     imágenes ni la contraseña del WiFi.
 *
 * Compilar para "ESP32 Wrover Module" (esp32:esp32:esp32wrover, con PSRAM). La tarjeta microSD
 * de la placa debe estar vacía (aquí no se usa).
 */
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#if defined(__has_include)
#  if __has_include("secretos.h")
#    include "secretos.h"
#  else
#    include "secretos.ejemplo.h"
#  endif
#else
#  include "secretos.ejemplo.h"
#endif

/* ── Configuración ─────────────────────────────────────────────────────── */

static const IPAddress IP_FIJA(192, 168, 4, 2);
static const IPAddress PUERTA(192, 168, 4, 1);
static const IPAddress MASCARA(255, 255, 255, 0);

static const uint32_t TIEMPO_CONEXION_MS = 15000;   /* sin conectar en este tiempo: reintenta */
static const uint32_t PERIODO_ESTADO_MS = 5000;
static const int CUADROS_MEDIDOS = 100;
static const int CUADROS_DESCARTADOS = 5;           /* al cambiar de tamaño, los primeros no cuentan */
static const int CALIDAD_JPEG = 12;                 /* 0 = mejor, 63 = peor */
static const int CALIDAD_JPEG_SW = 80;              /* JPEG por software: 1 a 100, mayor = mejor (aprox. equivalente; no medido) */

/* ── Mapas de pines de la cámara ───────────────────────────────────────── */

struct MapaPines {
  const char *nombre;
  int pwdn, reset, xclk, siod, sioc;
  int y2, y3, y4, y5, y6, y7, y8, y9;
  int vsync, href, pclk;
};

/* Se prueban en este orden. El primero es el oficial de Freenove para la WROVER. */
static const MapaPines MAPAS[] = {
  { "WROVER_KIT (oficial Freenove)", -1, -1, 21, 26, 27,  4, 5, 18, 19, 36, 39, 34, 35, 25, 23, 22 },
  { "AI_THINKER",                    32, -1,  0, 26, 27,  5, 18, 19, 21, 36, 39, 34, 35, 25, 23, 22 },
};
static const int NUM_MAPAS = sizeof(MAPAS) / sizeof(MAPAS[0]);

/* ── Estado compartido ─────────────────────────────────────────────────── */

static const MapaPines *mapaActivo = NULL;
static int sensorPid = -1;
static httpd_handle_t servidor = NULL;
static SemaphoreHandle_t candadoCamara = NULL;   /* un solo uso de la cámara a la vez */
static volatile bool streamActivo = false;
static const char *tamanoActual = "qvga";

/* Contadores de cuadros servidos (se reinician cada 5 s al imprimir). */
static volatile uint32_t servidosVentana = 0;
static volatile uint64_t bytesVentana = 0;
static volatile uint32_t servidosTotal = 0;

static float ultimoFpsQvga = 0, ultimoFpsVga = 0;
static uint32_t ultimoTamQvga = 0, ultimoTamVga = 0;

/* ── Cámara ────────────────────────────────────────────────────────────── */

/* Modo de captura: true = el sensor entrega JPEG; false = el sensor NO sabe hacer JPEG (por
 * ejemplo el GC0308) y se captura RGB565 y se comprime a JPEG por software en la ESP32. */
static bool jpegDelSensor = true;
static const char *nombreSensor = "?";

/* Un cuadro listo para enviar: `datos`/`largo` son siempre un JPEG. Se libera con soltar_cuadro(). */
struct Cuadro {
  camera_fb_t *fb;        /* búfer de la cámara (si hay que devolverlo) */
  uint8_t *jpg;           /* JPEG comprimido por software (si hay que liberarlo) */
  const uint8_t *datos;
  size_t largo;
};

/* Intenta iniciar la cámara con un mapa. Devuelve el código de esp_camera_init (0 = bien). */
static esp_err_t iniciar_camara(const MapaPines &m, pixformat_t formato, framesize_t tam)
{
  camera_config_t c;
  memset(&c, 0, sizeof(c));
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = m.y2;  c.pin_d1 = m.y3;  c.pin_d2 = m.y4;  c.pin_d3 = m.y5;
  c.pin_d4 = m.y6;  c.pin_d5 = m.y7;  c.pin_d6 = m.y8;  c.pin_d7 = m.y9;
  c.pin_xclk = m.xclk;
  c.pin_pclk = m.pclk;
  c.pin_vsync = m.vsync;
  c.pin_href = m.href;
  c.pin_sccb_sda = m.siod;
  c.pin_sccb_scl = m.sioc;
  c.pin_pwdn = m.pwdn;
  c.pin_reset = m.reset;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = formato;
  c.frame_size = tam;                    /* con JPEG del sensor se usa VGA (el mayor) para que los búferes alcancen en los dos tamaños */
  c.jpeg_quality = CALIDAD_JPEG;         /* solo cuenta si el sensor hace JPEG */
  c.fb_count = 2;                        /* 2 búferes (en PSRAM) */
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;      /* siempre el cuadro más nuevo */
  return esp_camera_init(&c);
}

/* Imprime qué sensor hay (nombre, PID, si soporta JPEG). Solo números y el nombre del modelo. */
static void describir_sensor(const MapaPines &m)
{
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return;
  const camera_sensor_info_t *info = esp_camera_sensor_get_info(&s->id);
  nombreSensor = info ? info->name : "?";
  sensorPid = s->id.PID;
  Serial.printf("[camara] mapa %s: sensor %s, PID=0x%02X, soporta_jpeg=%d\n",
                m.nombre, nombreSensor, s->id.PID, info ? (int)info->support_jpeg : -1);
}

/* Prueba los mapas en orden, uno solo una vez cada uno. Para cada mapa: primero JPEG del sensor;
 * si el sensor responde pero no soporta JPEG, una vez más en RGB565 (JPEG por software).
 * Devuelve true si alguno funcionó. */
static bool buscar_mapa_de_pines()
{
  for (int i = 0; i < NUM_MAPAS; i++) {
    Serial.printf("[camara] probando mapa %s ...\n", MAPAS[i].nombre);
    esp_err_t e = iniciar_camara(MAPAS[i], PIXFORMAT_JPEG, FRAMESIZE_VGA);
    if (e == ESP_OK) {
      jpegDelSensor = true;
      mapaActivo = &MAPAS[i];
      describir_sensor(MAPAS[i]);
      Serial.printf("[camara] FUNCIONO el mapa %s (JPEG del sensor)\n", MAPAS[i].nombre);
      return true;
    }
    Serial.printf("[camara] FALLO el mapa %s con JPEG: codigo 0x%X (%s)\n", MAPAS[i].nombre, (unsigned)e, esp_err_to_name(e));
    esp_camera_deinit();   /* por si quedó algo a medias; el error no importa */
    if (e != ESP_ERR_NOT_SUPPORTED) continue;

    /* El sensor contestó pero no hace JPEG: se prueba RGB565 con el mismo mapa. */
    e = iniciar_camara(MAPAS[i], PIXFORMAT_RGB565, FRAMESIZE_QVGA);
    if (e == ESP_OK) {
      jpegDelSensor = false;
      mapaActivo = &MAPAS[i];
      describir_sensor(MAPAS[i]);
      Serial.printf("[camara] FUNCIONO el mapa %s con RGB565 (el JPEG se hace por software, calidad %d de 100)\n",
                    MAPAS[i].nombre, CALIDAD_JPEG_SW);
      return true;
    }
    Serial.printf("[camara] FALLO el mapa %s con RGB565: codigo 0x%X (%s)\n", MAPAS[i].nombre, (unsigned)e, esp_err_to_name(e));
    esp_camera_deinit();
  }
  return false;
}

/* Toma un cuadro y lo deja como JPEG (del sensor o comprimido por software). */
static bool tomar_cuadro(Cuadro &c)
{
  c.fb = NULL;
  c.jpg = NULL;
  c.datos = NULL;
  c.largo = 0;
  camera_fb_t *f = esp_camera_fb_get();
  if (!f) return false;
  if (f->format == PIXFORMAT_JPEG) {
    c.fb = f;
    c.datos = f->buf;
    c.largo = f->len;
    return true;
  }
  uint8_t *salida = NULL;
  size_t largo = 0;
  bool ok = frame2jpg(f, CALIDAD_JPEG_SW, &salida, &largo);
  esp_camera_fb_return(f);
  if (!ok || !salida) return false;
  c.jpg = salida;
  c.datos = salida;
  c.largo = largo;
  return true;
}

static void soltar_cuadro(Cuadro &c)
{
  if (c.fb) esp_camera_fb_return(c.fb);
  if (c.jpg) free(c.jpg);
  c.fb = NULL;
  c.jpg = NULL;
  c.datos = NULL;
}

/* Cambia el tamaño. Con JPEG del sensor basta pedírselo al sensor; con RGB565 los búferes
 * dependen del tamaño, así que hay que reiniciar la cámara con el mapa que ya funciona. */
static bool poner_tamano(const char *nombre)
{
  framesize_t t;
  if (strcmp(nombre, "qvga") == 0) t = FRAMESIZE_QVGA;
  else if (strcmp(nombre, "vga") == 0) t = FRAMESIZE_VGA;
  else return false;

  if (jpegDelSensor) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s || s->set_framesize(s, t) != 0) return false;
  } else {
    esp_camera_deinit();
    esp_err_t e = iniciar_camara(*mapaActivo, PIXFORMAT_RGB565, t);
    if (e != ESP_OK) {
      Serial.printf("[camara] ERROR al reiniciar en %s: 0x%X (%s)\n", nombre, (unsigned)e, esp_err_to_name(e));
      return false;
    }
  }
  tamanoActual = (t == FRAMESIZE_QVGA) ? "qvga" : "vga";
  return true;
}

/* Mide CUADROS_MEDIDOS cuadros (sin cliente): primero solo la captura (fps crudos) y luego la
 * captura + JPEG que es lo que realmente se sirve. Devuelve false si la cámara dejó de dar cuadros. */
static bool medir(const char *nombre, float *fps, uint32_t *tamMedio)
{
  if (!poner_tamano(nombre)) return false;
  for (int i = 0; i < CUADROS_DESCARTADOS; i++) {
    camera_fb_t *f = esp_camera_fb_get();
    if (f) esp_camera_fb_return(f);
  }

  /* a) Solo captura, sin comprimir. */
  int crudos = 0;
  uint32_t t0 = millis();
  for (int i = 0; i < CUADROS_MEDIDOS; i++) {
    camera_fb_t *f = esp_camera_fb_get();
    if (!f) continue;
    crudos++;
    esp_camera_fb_return(f);
  }
  uint32_t msCrudo = millis() - t0;

  /* b) Captura + JPEG (lo que se sirve). */
  uint64_t bytes = 0;
  int buenos = 0;
  t0 = millis();
  for (int i = 0; i < CUADROS_MEDIDOS; i++) {
    Cuadro c;
    if (!tomar_cuadro(c)) continue;
    bytes += c.largo;
    buenos++;
    soltar_cuadro(c);
  }
  uint32_t ms = millis() - t0;
  if (buenos == 0 || ms == 0 || crudos == 0 || msCrudo == 0) return false;
  *fps = buenos * 1000.0f / ms;
  *tamMedio = (uint32_t)(bytes / buenos);
  Serial.printf("[medida] %s: captura sola %.1f fps; captura+JPEG (%s) %d cuadros en %u ms -> %.1f fps, JPEG medio %u bytes\n",
                nombre, crudos * 1000.0f / msCrudo, jpegDelSensor ? "calidad 12 del sensor" : "software calidad 80",
                buenos, (unsigned)ms, *fps, (unsigned)*tamMedio);
  return true;
}

/* ── Servidor HTTP ─────────────────────────────────────────────────────── */

static const char PAGINA[] =
  "<!doctype html><html><head><meta charset='utf-8'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>Camara del carrito</title></head>"
  "<body style='margin:0;background:#111;color:#eee;font-family:sans-serif'>"
  "<img src='/stream' style='width:100%;display:block'>"
  "<p style='margin:8px'>"
  "<button onclick=\"fetch('/control?size=qvga')\">QVGA 320x240</button> "
  "<button onclick=\"fetch('/control?size=vga')\">VGA 640x480</button> "
  "<a style='color:#8cf' href='/capture'>un cuadro</a></p></body></html>";

/* Responde "503 Service Unavailable" (el servidor del core no trae ese código en su lista). */
static esp_err_t responder_503(httpd_req_t *req, const char *texto)
{
  httpd_resp_set_status(req, "503 Service Unavailable");
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, texto, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t manejar_indice(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, PAGINA, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t manejar_captura(httpd_req_t *req)
{
  if (xSemaphoreTake(candadoCamara, pdMS_TO_TICKS(2000)) != pdTRUE) {
    return responder_503(req, "camara ocupada");
  }
  Cuadro c;
  esp_err_t r;
  if (!tomar_cuadro(c)) {
    xSemaphoreGive(candadoCamara);
    return httpd_resp_send_500(req);
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  r = httpd_resp_send(req, (const char *)c.datos, c.largo);
  if (r == ESP_OK) {
    servidosVentana++;
    servidosTotal++;
    bytesVentana += c.largo;
  }
  soltar_cuadro(c);
  xSemaphoreGive(candadoCamara);
  return r;
}

/* Tarea que manda el MJPEG a un cliente, sin bloquear al servidor (los otros pedidos siguen). */
static void tarea_stream(void *arg)
{
  httpd_req_t *req = (httpd_req_t *)arg;
  char cabecera[96];
  int fallos = 0;

  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=cuadro");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  while (true) {
    if (xSemaphoreTake(candadoCamara, pdMS_TO_TICKS(1000)) != pdTRUE) {
      if (++fallos > 20) break;
      continue;
    }
    Cuadro c;
    if (!tomar_cuadro(c)) {
      xSemaphoreGive(candadoCamara);
      if (++fallos > 50) break;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    fallos = 0;
    int n = snprintf(cabecera, sizeof(cabecera),
                     "\r\n--cuadro\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned)c.largo);
    esp_err_t r = httpd_resp_send_chunk(req, cabecera, n);
    if (r == ESP_OK) r = httpd_resp_send_chunk(req, (const char *)c.datos, c.largo);
    size_t largo = c.largo;
    soltar_cuadro(c);
    xSemaphoreGive(candadoCamara);
    if (r != ESP_OK) break;   /* el cliente se fue */
    servidosVentana++;
    servidosTotal++;
    bytesVentana += largo;
  }
  httpd_resp_send_chunk(req, NULL, 0);
  httpd_req_async_handler_complete(req);
  streamActivo = false;
  vTaskDelete(NULL);
}

static esp_err_t manejar_stream(httpd_req_t *req)
{
  if (streamActivo) {
    return responder_503(req, "ya hay un stream activo");
  }
  httpd_req_t *copia = NULL;
  if (httpd_req_async_handler_begin(req, &copia) != ESP_OK) {
    return httpd_resp_send_500(req);
  }
  streamActivo = true;
  if (xTaskCreate(tarea_stream, "stream", 10240, copia, 2, NULL) != pdPASS) {
    streamActivo = false;
    httpd_req_async_handler_complete(copia);
    return ESP_FAIL;
  }
  return ESP_OK;
}

static esp_err_t manejar_control(httpd_req_t *req)
{
  char consulta[48];
  char valor[8];
  if (httpd_req_get_url_query_str(req, consulta, sizeof(consulta)) != ESP_OK ||
      httpd_query_key_value(consulta, "size", valor, sizeof(valor)) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "uso: /control?size=qvga|vga");
  }
  if (strcmp(valor, "qvga") != 0 && strcmp(valor, "vga") != 0) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "size debe ser qvga o vga");
  }
  if (xSemaphoreTake(candadoCamara, pdMS_TO_TICKS(2000)) != pdTRUE) {
    return responder_503(req, "camara ocupada");
  }
  bool ok = poner_tamano(valor);
  xSemaphoreGive(candadoCamara);
  if (!ok) return httpd_resp_send_500(req);
  Serial.printf("[http] tamano -> %s\n", valor);
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, valor, HTTPD_RESP_USE_STRLEN);
}

static void iniciar_servidor()
{
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 80;
  cfg.stack_size = 10240;
  cfg.max_uri_handlers = 8;
  cfg.lru_purge_enable = true;
  if (httpd_start(&servidor, &cfg) != ESP_OK) {
    Serial.println("[http] ERROR: no arranco el servidor");
    return;
  }
  const httpd_uri_t rutas[] = {
    { "/",        HTTP_GET, manejar_indice,  NULL },
    { "/capture", HTTP_GET, manejar_captura, NULL },
    { "/stream",  HTTP_GET, manejar_stream,  NULL },
    { "/control", HTTP_GET, manejar_control, NULL },
  };
  for (size_t i = 0; i < sizeof(rutas) / sizeof(rutas[0]); i++) {
    httpd_register_uri_handler(servidor, &rutas[i]);
  }
  Serial.println("[http] servidor listo en el puerto 80 (/, /capture, /stream, /control)");
}

/* ── WiFi (cliente de la red del carro, IP fija) ───────────────────────── */

static uint32_t inicioIntento = 0;
static int numIntento = 0;

static void conectar_wifi()
{
  numIntento++;
  Serial.printf("[wifi] intento %d: conectando a la red '%s' con IP fija %s ...\n",
                numIntento, WIFI_SSID, IP_FIJA.toString().c_str());
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  /* Diagnóstico: ¿se ve la red del carro desde la cámara? (nombre, canal y señal; nada más) */
  int redes = WiFi.scanNetworks();
  bool vista = false;
  for (int i = 0; i < redes; i++) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      vista = true;
      Serial.printf("[wifi] escaneo: '%s' VISIBLE, canal %d, RSSI %d dBm\n", WIFI_SSID, (int)WiFi.channel(i), (int)WiFi.RSSI(i));
      break;
    }
  }
  if (!vista) Serial.printf("[wifi] escaneo: %d redes y '%s' NO se ve\n", redes < 0 ? 0 : redes, WIFI_SSID);
  WiFi.scanDelete();
  WiFi.config(IP_FIJA, PUERTA, MASCARA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);   /* sin ahorro de energía del radio: menos retraso */
  inicioIntento = millis();
}

/* Espera hasta TIEMPO_CONEXION_MS a que conecte; reintenta de inmediato si no. */
static bool esperar_conexion()
{
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - inicioIntento > TIEMPO_CONEXION_MS) {
      Serial.printf("[wifi] no conecto en %u s: reintento\n", (unsigned)(TIEMPO_CONEXION_MS / 1000));
      return false;
    }
    delay(250);
  }
  Serial.printf("[wifi] CONECTADO: IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

/* ── Arranque y ciclo ──────────────────────────────────────────────────── */

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== camara_prueba: Freenove ESP32-WROVER ===");
  Serial.printf("[sistema] PSRAM %s, libre %u bytes; heap libre %u bytes\n",
                psramFound() ? "SI" : "NO", (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());

  candadoCamara = xSemaphoreCreateMutex();
  /* Avisa por qué se desconecta el WiFi (código de razón de ESP-IDF; 201 = no se encuentra la red). */
  WiFi.onEvent([](arduino_event_id_t ev, arduino_event_info_t info) {
    if (ev == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      Serial.printf("[wifi] desconectado, razon %d\n", (int)info.wifi_sta_disconnected.reason);
    }
  });

  if (!buscar_mapa_de_pines()) {
    Serial.println("[camara] NINGUN mapa de pines funciono. Me detengo (no hay mas intentos). Revisa la placa y el cable de la camara.");
    return;   /* loop() solo avisa cada 5 s */
  }

  /* Medida de velocidad en la propia placa, sin cliente conectado (todavia no hay WiFi ni servidor). */
  bool okQ = medir("qvga", &ultimoFpsQvga, &ultimoTamQvga);
  bool okV = medir("vga", &ultimoFpsVga, &ultimoTamVga);
  if (!okQ || !okV) Serial.println("[medida] ERROR: la camara no entrego cuadros en alguna medida");
  poner_tamano("qvga");   /* se sirve en QVGA */

  conectar_wifi();
  while (!esperar_conexion()) {
    conectar_wifi();
  }
  iniciar_servidor();
}

void loop()
{
  static uint32_t ultimoEstado = 0;
  static uint32_t ultimaRevision = 0;

  if (mapaActivo == NULL) {
    if (millis() - ultimoEstado >= PERIODO_ESTADO_MS) {
      ultimoEstado = millis();
      Serial.println("[estado] DETENIDO: la camara no inicio con ningun mapa de pines");
    }
    delay(100);
    return;
  }

  /* Si se cae el WiFi, reintenta (cada 15 s sin conexion). */
  if (millis() - ultimaRevision >= 1000) {
    ultimaRevision = millis();
    if (WiFi.status() != WL_CONNECTED && millis() - inicioIntento > TIEMPO_CONEXION_MS) {
      Serial.println("[wifi] conexion perdida: reintento");
      conectar_wifi();
    }
  }

  if (millis() - ultimoEstado >= PERIODO_ESTADO_MS) {
    uint32_t ms = millis() - ultimoEstado;
    ultimoEstado = millis();
    uint32_t n = servidosVentana;
    uint64_t b = bytesVentana;
    servidosVentana = 0;
    bytesVentana = 0;
    float fpsServidos = n * 1000.0f / ms;
    uint32_t tamMedio = n ? (uint32_t)(b / n) : 0;
    Serial.printf("[estado] wifi=%s ip=%s tam=%s stream=%s | JPEG medio=%u B | fps servidos=%.1f (%u cuadros) | RSSI=%d dBm | heap libre=%u B, PSRAM libre=%u B\n",
                  WiFi.status() == WL_CONNECTED ? "conectado" : "SIN CONEXION",
                  WiFi.localIP().toString().c_str(), tamanoActual, streamActivo ? "si" : "no",
                  (unsigned)tamMedio, fpsServidos, (unsigned)n,
                  WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  }
  delay(20);
}
