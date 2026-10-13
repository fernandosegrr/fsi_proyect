/*
 * camara_prueba.ino — Prueba de la cámara de la placa Freenove ESP32-WROVER (v3.0).
 *
 * GUÍA DE LECTURA
 *  1. Objetivo: ver video de la cámara por WiFi con la menor edad de cuadro posible. El sensor
 *     de esta placa es un GC0308 que NO sabe hacer JPEG, así que la cámara entrega RGB565 o
 *     YUV422 y la ESP32 comprime cada cuadro a JPEG por software.
 *  2. Tubería en dos núcleos (sección "Tubería"):
 *       - tarea_captura (núcleo 0): pide cuadros a la cámara y los deja en UNA sola casilla.
 *         Si llega un cuadro nuevo y la casilla seguía ocupada, el viejo se descarta
 *         (nunca se acumulan cuadros viejos).
 *       - tarea_stream / manejar_captura (núcleo 1): toman el cuadro más reciente, lo comprimen
 *         (frame2jpg) y lo envían.
 *     Se miden: fps de captura, ms entre cuadros, ms de compresión, ms de envío, cuadros
 *     descartados y la "edad del cuadro" (ms entre el inicio de su captura y el fin de su envío).
 *  3. Configuración activa (struct Config): reloj de la cámara (XCLK), formato (RGB565 o YUV422),
 *     tamaño (QVGA 320x240, HQVGA 240x176, QQVGA 160x120, VGA) y calidad JPEG (0 a 100, mayor =
 *     mejor). Cambiarla reinicia la cámara dentro de tarea_captura (único dueño de la cámara).
 *  4. MODO_BARRIDO = 1 prueba al arrancar todas las combinaciones (100 cuadros cada una, sin
 *     cliente) y las imprime; con 0 solo mide la configuración por defecto.
 *  5. Servidor HTTP: /  (video + números), /capture (un JPEG), /stream (MJPEG), /control?size=...
 *     y /stats (JSON). Cada petición se anota por serie: "GET <ruta> desde <ip>".
 *  6. WiFi: cliente de la red del carro con IP fija 192.168.4.2, WiFi.setSleep(false).
 *  7. Mapas de pines: el oficial de Freenove para esta placa es CAMERA_MODEL_WROVER_KIT (idéntico
 *     al del core de ESP32). Si falla se prueba AI_THINKER. Si ninguno funciona se avisa y se
 *     detiene.
 *  8. Privacidad: solo se imprimen números (tamaños, tiempos, memoria, brillo medio); nunca
 *     imágenes ni la contraseña del WiFi.
 *
 * Compilar para "ESP32 Wrover Module" (esp32:esp32:esp32wrover, con PSRAM). La tarjeta microSD
 * de la placa debe estar vacía (aquí no se usa).
 */
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "esp_camera.h"
#include "img_converters.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
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

/* ── Opciones de compilación ───────────────────────────────────────────── */

#define MODO_BARRIDO 0   /* 1 = probar todas las combinaciones al arrancar (tarda varios minutos) */
#define AUTOPRUEBA   1   /* 1 = al arrancar, la placa se pide a sí misma /stats, /capture y /stream */

/* ── Configuración ─────────────────────────────────────────────────────── */

static const IPAddress IP_FIJA(192, 168, 4, 2);
static const IPAddress PUERTA(192, 168, 4, 1);
static const IPAddress MASCARA(255, 255, 255, 0);

static const uint32_t TIEMPO_CONEXION_MS = 15000;   /* sin conectar en este tiempo: reintenta */
static const uint32_t PERIODO_ESTADO_MS = 5000;
static const int CUADROS_MEDIDOS = 100;
static const int64_t EDAD_MAX_US = 450000;          /* un cuadro que ya tiene más edad al salir de la cámara está viejo: se tira */
static const int CUADROS_DESCARTADOS = 5;           /* tras cambiar la configuración, los primeros no cuentan */

struct Config {
  int xclkMHz;        /* reloj que la ESP32 le da a la cámara */
  pixformat_t fmt;    /* PIXFORMAT_RGB565 o PIXFORMAT_YUV422 (con sensor sin JPEG) */
  framesize_t tam;    /* FRAMESIZE_QVGA, ... */
  int calidad;        /* JPEG por software: 1 a 100, mayor = mejor */
};

/* Valor por defecto: se elige con el barrido (ver docs/camara_prueba.md). */
static const Config CONFIG_DEFECTO = { 20, PIXFORMAT_RGB565, FRAMESIZE_QVGA, 50 };
static Config cfg = CONFIG_DEFECTO;

/* ── Tipos (arriba de todo: el preprocesador de Arduino pone los prototipos antes del primer código) ── */

struct MapaPines {
  const char *nombre;
  int pwdn, reset, xclk, siod, sioc;
  int y2, y3, y4, y5, y6, y7, y8, y9;
  int vsync, href, pclk;
};

/* Un cuadro ya comprimido, listo para enviar. */
struct Cuadro {
  camera_fb_t *fb;      /* solo si el sensor entrega JPEG: hay que devolverlo */
  uint8_t *jpg;         /* JPEG hecho por software: hay que liberarlo */
  const uint8_t *datos;
  size_t largo;
  int64_t t0_us;        /* cuándo empezó su captura (reloj de esp_timer) */
};

struct Acum {
  uint32_t capturados, descartados, comprimidos, enviados;
  uint32_t nPeriodo, nEdad;
  uint64_t sumPeriodoUs, sumCompUs, sumEnvioUs, sumEdadUs, sumBytes;
  uint64_t maxEdadUs, maxEnvioUs;   /* el peor cuadro del periodo */
};

struct Resumen {
  float fpsCaptura, fpsEnvio, msCaptura, msCompresion, msEnvio, edadMs, jpegMedio, edadMaxMs, envioMaxMs;
  uint32_t descartados;
};

struct Medida {
  bool ok;
  float fps, fpsMin, fpsMax, fpsCaptura, msComp, edadMs;
  uint32_t jpegMedio, descartados;
};

static const char *nombre_formato(pixformat_t f)
{
  switch (f) {
  case PIXFORMAT_RGB565: return "RGB565";
  case PIXFORMAT_YUV422: return "YUV422";
  case PIXFORMAT_JPEG:   return "JPEG";
  default:               return "otro";
  }
}

static const char *nombre_tam(framesize_t t)
{
  switch (t) {
  case FRAMESIZE_QQVGA: return "QQVGA 160x120";
  case FRAMESIZE_HQVGA: return "HQVGA 240x176";
  case FRAMESIZE_QVGA:  return "QVGA 320x240";
  case FRAMESIZE_VGA:   return "VGA 640x480";
  default:              return "otro";
  }
}

/* ── Mapas de pines de la cámara ───────────────────────────────────────── */


/* Se prueban en este orden. El primero es el oficial de Freenove para la WROVER. */
static const MapaPines MAPAS[] = {
  { "WROVER_KIT (oficial Freenove)", -1, -1, 21, 26, 27,  4, 5, 18, 19, 36, 39, 34, 35, 25, 23, 22 },
  { "AI_THINKER",                    32, -1,  0, 26, 27,  5, 18, 19, 21, 36, 39, 34, 35, 25, 23, 22 },
};
static const int NUM_MAPAS = sizeof(MAPAS) / sizeof(MAPAS[0]);

/* ── Estado compartido ─────────────────────────────────────────────────── */

static const MapaPines *mapaActivo = NULL;
static bool jpegDelSensor = false;
static const char *nombreSensor = "?";
static httpd_handle_t servidor = NULL;
static volatile bool streamActivo = false;

/* Cuántos consumidores piden cuadros ahora (un stream, un /capture, una medida). Sin demanda,
 * la tarea de captura no pide cuadros a la cámara. */
static volatile int demanda = 0;
static inline void demanda_mas() { __atomic_fetch_add(&demanda, 1, __ATOMIC_SEQ_CST); }
static inline void demanda_menos() { __atomic_fetch_sub(&demanda, 1, __ATOMIC_SEQ_CST); }

/* ── Tubería: casilla única entre captura y compresión ─────────────────── */

static camera_fb_t *casillaFb = NULL;               /* cuadro más reciente sin consumir */
static SemaphoreHandle_t candadoCasilla = NULL;     /* protege casillaFb */
static SemaphoreHandle_t hayCuadro = NULL;          /* binario: hay un cuadro en la casilla */
static SemaphoreHandle_t candadoUso = NULL;         /* quien tiene un cuadro de la cámara en la mano; el reinicio lo espera */
static SemaphoreHandle_t candadoReinicio = NULL;    /* una petición de reinicio a la vez */
static SemaphoreHandle_t reinicioHecho = NULL;      /* tarea_captura avisa que terminó el reinicio */
static volatile bool reinicioPedido = false;
static volatile bool reinicioOk = false;
static Config cfgPedida;


/* ── Medidas (acumuladores protegidos por un spinlock) ─────────────────── */


static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static Acum acum;                       /* lo que va del segundo actual */
static volatile uint32_t totalCapturados = 0, totalDescartados = 0, totalComprimidos = 0;
static volatile uint64_t totalCompUs = 0;
static const int ANILLO = 5;
static Acum anillo[ANILLO];             /* últimos 5 segundos, uno por segundo */
static uint32_t anilloMs[ANILLO];
static int anilloPos = 0;
static int anilloLleno = 0;

static void sumar_captura(int64_t periodoUs)
{
  portENTER_CRITICAL(&mux);
  acum.capturados++;
  if (periodoUs > 0 && periodoUs < 2000000) { acum.sumPeriodoUs += periodoUs; acum.nPeriodo++; }
  portEXIT_CRITICAL(&mux);
  __atomic_fetch_add(&totalCapturados, 1u, __ATOMIC_RELAXED);
}

static void sumar_descartado()
{
  portENTER_CRITICAL(&mux);
  acum.descartados++;
  portEXIT_CRITICAL(&mux);
  __atomic_fetch_add(&totalDescartados, 1u, __ATOMIC_RELAXED);
}

static void sumar_compresion(int64_t us)
{
  portENTER_CRITICAL(&mux);
  acum.comprimidos++;
  acum.sumCompUs += us;
  portEXIT_CRITICAL(&mux);
  __atomic_fetch_add(&totalComprimidos, 1u, __ATOMIC_RELAXED);
  __atomic_fetch_add(&totalCompUs, (uint64_t)us, __ATOMIC_RELAXED);
}

static void sumar_envio(int64_t envioUs, int64_t edadUs, size_t bytes)
{
  portENTER_CRITICAL(&mux);
  acum.enviados++;
  acum.sumEnvioUs += envioUs;
  acum.sumEdadUs += edadUs;
  acum.nEdad++;
  acum.sumBytes += bytes;
  if ((uint64_t)edadUs > acum.maxEdadUs) acum.maxEdadUs = edadUs;
  if ((uint64_t)envioUs > acum.maxEnvioUs) acum.maxEnvioUs = envioUs;
  portEXIT_CRITICAL(&mux);
}

/* Borra lo acumulado hasta ahora (por ejemplo las medidas de arranque), para que /stats solo cuente lo que se sirve. */
static void reiniciar_estadisticas()
{
  portENTER_CRITICAL(&mux);
  memset(&acum, 0, sizeof(acum));
  portEXIT_CRITICAL(&mux);
  memset(anillo, 0, sizeof(anillo));
  memset(anilloMs, 0, sizeof(anilloMs));
  anilloPos = 0;
  anilloLleno = 0;
}

/* Cada segundo (desde loop): guarda lo acumulado y empieza de nuevo. */
static void tomar_instantanea(uint32_t ms)
{
  Acum copia;
  portENTER_CRITICAL(&mux);
  copia = acum;
  memset(&acum, 0, sizeof(acum));
  portEXIT_CRITICAL(&mux);
  anillo[anilloPos] = copia;
  anilloMs[anilloPos] = ms;
  anilloPos = (anilloPos + 1) % ANILLO;
  if (anilloLleno < ANILLO) anilloLleno++;
}


/* Resume los últimos `n` segundos. */
static Resumen resumir(int n)
{
  Resumen r;
  memset(&r, 0, sizeof(r));
  if (n > anilloLleno) n = anilloLleno;
  if (n <= 0) return r;
  Acum t;
  memset(&t, 0, sizeof(t));
  uint32_t ms = 0;
  for (int i = 1; i <= n; i++) {
    int k = (anilloPos - i + ANILLO) % ANILLO;
    const Acum &a = anillo[k];
    t.capturados += a.capturados;  t.descartados += a.descartados;
    t.comprimidos += a.comprimidos; t.enviados += a.enviados;
    t.nPeriodo += a.nPeriodo;      t.nEdad += a.nEdad;
    t.sumPeriodoUs += a.sumPeriodoUs; t.sumCompUs += a.sumCompUs;
    t.sumEnvioUs += a.sumEnvioUs;  t.sumEdadUs += a.sumEdadUs; t.sumBytes += a.sumBytes;
    if (a.maxEdadUs > t.maxEdadUs) t.maxEdadUs = a.maxEdadUs;
    if (a.maxEnvioUs > t.maxEnvioUs) t.maxEnvioUs = a.maxEnvioUs;
    ms += anilloMs[k];
  }
  if (ms == 0) return r;
  r.fpsCaptura = t.capturados * 1000.0f / ms;
  r.fpsEnvio = t.enviados * 1000.0f / ms;
  r.msCaptura = t.nPeriodo ? t.sumPeriodoUs / 1000.0f / t.nPeriodo : 0;
  r.msCompresion = t.comprimidos ? t.sumCompUs / 1000.0f / t.comprimidos : 0;
  r.msEnvio = t.enviados ? t.sumEnvioUs / 1000.0f / t.enviados : 0;
  r.edadMs = t.nEdad ? t.sumEdadUs / 1000.0f / t.nEdad : 0;
  r.jpegMedio = t.enviados ? (float)t.sumBytes / t.enviados : 0;
  r.descartados = t.descartados;
  r.edadMaxMs = t.maxEdadUs / 1000.0f;
  r.envioMaxMs = t.maxEnvioUs / 1000.0f;
  return r;
}

/* ── Cámara ────────────────────────────────────────────────────────────── */

/* Intenta iniciar la cámara con un mapa y una configuración. Devuelve el código de esp_camera_init. */
static esp_err_t iniciar_camara(const MapaPines &m, pixformat_t formato, framesize_t tam, int xclkMHz)
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
  c.xclk_freq_hz = xclkMHz * 1000000;
  c.pixel_format = formato;
  c.frame_size = tam;
  c.jpeg_quality = 12;                   /* solo cuenta si el sensor hace JPEG */
  c.fb_count = (formato == PIXFORMAT_JPEG) ? 2 : 3;   /* uno en compresión, uno en la casilla, uno llenándose */
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;      /* siempre el cuadro más nuevo */
  return esp_camera_init(&c);
}

static void describir_sensor(const MapaPines &m)
{
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return;
  const camera_sensor_info_t *info = esp_camera_sensor_get_info(&s->id);
  nombreSensor = info ? info->name : "?";
  Serial.printf("[camara] mapa %s: sensor %s, PID=0x%02X, soporta_jpeg=%d\n",
                m.nombre, nombreSensor, s->id.PID, info ? (int)info->support_jpeg : -1);
}

/* Prueba los mapas en orden, uno solo una vez cada uno. Primero JPEG del sensor; si el sensor
 * responde pero no soporta JPEG, una vez más con la configuración por defecto (sin JPEG).
 * Devuelve true si alguno funcionó. */
static bool buscar_mapa_de_pines()
{
  for (int i = 0; i < NUM_MAPAS; i++) {
    Serial.printf("[camara] probando mapa %s ...\n", MAPAS[i].nombre);
    esp_err_t e = iniciar_camara(MAPAS[i], PIXFORMAT_JPEG, FRAMESIZE_QVGA, 20);
    if (e == ESP_OK) {
      jpegDelSensor = true;
      mapaActivo = &MAPAS[i];
      cfg.fmt = PIXFORMAT_JPEG;
      describir_sensor(MAPAS[i]);
      Serial.printf("[camara] FUNCIONO el mapa %s (JPEG del sensor)\n", MAPAS[i].nombre);
      return true;
    }
    Serial.printf("[camara] FALLO el mapa %s con JPEG: codigo 0x%X (%s)\n", MAPAS[i].nombre, (unsigned)e, esp_err_to_name(e));
    esp_camera_deinit();
    if (e != ESP_ERR_NOT_SUPPORTED) continue;

    /* El sensor contestó pero no hace JPEG: se prueba sin JPEG con el mismo mapa. */
    e = iniciar_camara(MAPAS[i], CONFIG_DEFECTO.fmt, CONFIG_DEFECTO.tam, CONFIG_DEFECTO.xclkMHz);
    if (e == ESP_OK) {
      jpegDelSensor = false;
      mapaActivo = &MAPAS[i];
      cfg = CONFIG_DEFECTO;
      describir_sensor(MAPAS[i]);
      Serial.printf("[camara] FUNCIONO el mapa %s sin JPEG de sensor (JPEG por software)\n", MAPAS[i].nombre);
      return true;
    }
    Serial.printf("[camara] FALLO el mapa %s sin JPEG: codigo 0x%X (%s)\n", MAPAS[i].nombre, (unsigned)e, esp_err_to_name(e));
    esp_camera_deinit();
  }
  return false;
}

/* Vacía la casilla devolviendo su cuadro a la cámara. */
static void vaciar_casilla()
{
  xSemaphoreTake(candadoCasilla, portMAX_DELAY);
  if (casillaFb) {
    esp_camera_fb_return(casillaFb);
    casillaFb = NULL;
  }
  xSemaphoreGive(candadoCasilla);
  xSemaphoreTake(hayCuadro, 0);
}

/* Cambia la configuración reiniciando la cámara. SOLO la llama tarea_captura. */
static bool hacer_reinicio(const Config &nueva)
{
  if (xSemaphoreTake(candadoUso, pdMS_TO_TICKS(3000)) != pdTRUE) {
    Serial.println("[camara] reinicio cancelado: un cuadro sigue en uso");
    return false;
  }
  vaciar_casilla();
  esp_camera_deinit();
  esp_err_t e = iniciar_camara(*mapaActivo, nueva.fmt, nueva.tam, nueva.xclkMHz);
  bool ok = (e == ESP_OK);
  if (ok) {
    cfg = nueva;
  } else {
    Serial.printf("[camara] ERROR al reiniciar (%d MHz, %s, %s): 0x%X (%s); vuelvo a la anterior\n",
                  nueva.xclkMHz, nombre_formato(nueva.fmt), nombre_tam(nueva.tam), (unsigned)e, esp_err_to_name(e));
    esp_camera_deinit();
    if (iniciar_camara(*mapaActivo, cfg.fmt, cfg.tam, cfg.xclkMHz) != ESP_OK) {
      Serial.println("[camara] ERROR: tampoco se pudo volver a la configuración anterior");
    }
  }
  xSemaphoreGive(candadoUso);
  return ok;
}

/* Pide el reinicio y espera a que tarea_captura lo haga. Devuelve true si salió bien. */
static bool pedir_reinicio(const Config &nueva)
{
  xSemaphoreTake(candadoReinicio, portMAX_DELAY);
  cfgPedida = nueva;
  reinicioOk = false;
  reinicioPedido = true;
  bool hecho = xSemaphoreTake(reinicioHecho, pdMS_TO_TICKS(10000)) == pdTRUE;
  bool ok = hecho && reinicioOk;
  xSemaphoreGive(candadoReinicio);
  return ok;
}

/* ── Tubería: captura (núcleo 0) ───────────────────────────────────────── */

static void tarea_captura(void *)
{
  int64_t ultimoT = 0;
  for (;;) {
    if (reinicioPedido) {
      reinicioOk = hacer_reinicio(cfgPedida);
      reinicioPedido = false;
      ultimoT = 0;
      xSemaphoreGive(reinicioHecho);
      continue;
    }
    if (demanda <= 0) {
      ultimoT = 0;
      if (casillaFb) vaciar_casilla();   /* sin nadie que lo pida, el cuadro de la casilla envejece: se devuelve */
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    int64_t t = (int64_t)fb->timestamp.tv_sec * 1000000LL + fb->timestamp.tv_usec;
    /* Al empezar a pedir cuadros después de un rato sin demanda, el primero que entrega el driver
     * puede ser de hace segundos: se tira (y los de hace más de EDAD_MAX_US al llegar). */
    if (esp_timer_get_time() - t > EDAD_MAX_US) {
      esp_camera_fb_return(fb);
      continue;
    }
    sumar_captura(ultimoT ? t - ultimoT : 0);
    ultimoT = t;

    xSemaphoreTake(candadoCasilla, portMAX_DELAY);
    if (casillaFb) {                       /* el consumidor no llegó a tiempo: el viejo se descarta */
      esp_camera_fb_return(casillaFb);
      sumar_descartado();
    }
    casillaFb = fb;
    xSemaphoreGive(candadoCasilla);
    xSemaphoreGive(hayCuadro);
  }
}

/* ── Tubería: toma, compresión y envío ─────────────────────────────────── */

/* Toma el cuadro MÁS RECIENTE de la casilla y lo deja como JPEG. Devuelve false si no llegó ninguno. */
static bool siguiente_jpeg(Cuadro &c, uint32_t esperaMs)
{
  memset(&c, 0, sizeof(c));
  if (xSemaphoreTake(hayCuadro, pdMS_TO_TICKS(esperaMs)) != pdTRUE) return false;
  if (xSemaphoreTake(candadoUso, pdMS_TO_TICKS(3000)) != pdTRUE) return false;
  xSemaphoreTake(candadoCasilla, portMAX_DELAY);
  camera_fb_t *fb = casillaFb;
  casillaFb = NULL;
  xSemaphoreGive(candadoCasilla);
  if (!fb) {
    xSemaphoreGive(candadoUso);
    return false;
  }
  c.t0_us = (int64_t)fb->timestamp.tv_sec * 1000000LL + fb->timestamp.tv_usec;
  if (fb->format == PIXFORMAT_JPEG) {      /* sensor con JPEG: se envía tal cual; el cuadro se devuelve en soltar_cuadro */
    c.fb = fb;
    c.datos = fb->buf;
    c.largo = fb->len;
    return true;
  }
  int64_t a = esp_timer_get_time();
  uint8_t *salida = NULL;
  size_t largo = 0;
  bool ok = frame2jpg(fb, (uint8_t)cfg.calidad, &salida, &largo);
  esp_camera_fb_return(fb);                /* ya se puede reutilizar el búfer de la cámara */
  sumar_compresion(esp_timer_get_time() - a);
  xSemaphoreGive(candadoUso);
  if (!ok || !salida) return false;
  c.jpg = salida;
  c.datos = salida;
  c.largo = largo;
  return true;
}

static void soltar_cuadro(Cuadro &c)
{
  if (c.fb) {
    esp_camera_fb_return(c.fb);
    xSemaphoreGive(candadoUso);
  }
  if (c.jpg) free(c.jpg);
  c.fb = NULL;
  c.jpg = NULL;
  c.datos = NULL;
}

/* ── Medida de la tubería sin cliente ──────────────────────────────────── */


/* Comprime `n` cuadros seguidos (sin enviar a nadie) y mide. La edad llega hasta el fin de la compresión. */
static Medida medir_pipeline(int n)
{
  Medida m;
  memset(&m, 0, sizeof(m));
  static int64_t marcas[CUADROS_MEDIDOS + 1];
  if (n > CUADROS_MEDIDOS) n = CUADROS_MEDIDOS;

  demanda_mas();
  for (int i = 0; i < CUADROS_DESCARTADOS; i++) {
    Cuadro c;
    if (siguiente_jpeg(c, 3000)) soltar_cuadro(c);
  }
  uint32_t cap0 = totalCapturados, des0 = totalDescartados, comp0 = totalComprimidos;
  uint64_t compUs0 = totalCompUs;
  uint64_t bytes = 0, edadUs = 0;
  int buenos = 0;
  int64_t inicio = esp_timer_get_time();
  marcas[0] = inicio;
  for (int i = 0; i < n; i++) {
    Cuadro c;
    if (!siguiente_jpeg(c, 3000)) break;
    int64_t fin = esp_timer_get_time();
    bytes += c.largo;
    edadUs += fin - c.t0_us;
    buenos++;
    marcas[buenos] = fin;
    soltar_cuadro(c);
  }
  int64_t total = esp_timer_get_time() - inicio;
  uint32_t cap1 = totalCapturados, des1 = totalDescartados, comp1 = totalComprimidos;
  uint64_t compUs1 = totalCompUs;
  demanda_menos();

  if (buenos < 10 || total <= 0) return m;
  m.ok = true;
  m.fps = buenos * 1e6f / total;
  m.fpsCaptura = (cap1 - cap0) * 1e6f / total;
  m.jpegMedio = (uint32_t)(bytes / buenos);
  m.descartados = des1 - des0;
  m.msComp = (comp1 - comp0) ? (compUs1 - compUs0) / 1000.0f / (comp1 - comp0) : 0;
  m.edadMs = edadUs / 1000.0f / buenos;
  /* Estabilidad: fps en bloques de 10 cuadros. */
  float mn = 1e9f, mx = 0;
  for (int i = 10; i <= buenos; i += 10) {
    float f = 10 * 1e6f / (marcas[i] - marcas[i - 10]);
    if (f < mn) mn = f;
    if (f > mx) mx = f;
  }
  m.fpsMin = (mn > 1e8f) ? m.fps : mn;
  m.fpsMax = (mx == 0) ? m.fps : mx;
  return m;
}

static void imprimir_medida(const char *etiqueta, const Config &c, const Medida &m)
{
  if (!m.ok) {
    Serial.printf("[%s] xclk=%d MHz %s %s q=%d: SIN CUADROS SUFICIENTES\n", etiqueta, c.xclkMHz,
                  nombre_formato(c.fmt), nombre_tam(c.tam), c.calidad);
    return;
  }
  Serial.printf("[%s] xclk=%d MHz %s %s q=%d: fps=%.1f (bloques de 10: min %.1f, max %.1f), captura=%.1f fps, JPEG medio=%u B, compresion=%.1f ms, descartados=%u, edad hasta fin de compresion=%.0f ms\n",
                etiqueta, c.xclkMHz, nombre_formato(c.fmt), nombre_tam(c.tam), c.calidad,
                m.fps, m.fpsMin, m.fpsMax, m.fpsCaptura, (unsigned)m.jpegMedio, m.msComp, (unsigned)m.descartados, m.edadMs);
}

#if MODO_BARRIDO
/* Brillo medio (0 a 255) de un cuadro YUV422 de QVGA: solo un número, sirve para saber si había buena luz. */
static int medir_luz()
{
  Config c = cfg;
  c.fmt = PIXFORMAT_YUV422;
  c.tam = FRAMESIZE_QVGA;
  if (!pedir_reinicio(c)) return -1;
  demanda_mas();
  int luz = -1;
  for (int k = 0; k < 8; k++) {                 /* los primeros cuadros tras un reinicio pueden salir oscuros */
    camera_fb_t *fb = NULL;
    if (xSemaphoreTake(hayCuadro, pdMS_TO_TICKS(3000)) != pdTRUE) break;
    xSemaphoreTake(candadoUso, pdMS_TO_TICKS(3000));
    xSemaphoreTake(candadoCasilla, portMAX_DELAY);
    fb = casillaFb;
    casillaFb = NULL;
    xSemaphoreGive(candadoCasilla);
    if (fb) {
      if (k >= 5 && fb->format == PIXFORMAT_YUV422) {
        uint32_t suma = 0, cuantos = 0;
        for (size_t i = 0; i + 1 < fb->len; i += 64) {   /* un byte Y de cada 32 píxeles */
          suma += fb->buf[i & ~1u];
          cuantos++;
        }
        if (cuantos) luz = (int)(suma / cuantos);
      }
      esp_camera_fb_return(fb);
    }
    xSemaphoreGive(candadoUso);
  }
  demanda_menos();
  return luz;
}

/* Prueba todas las combinaciones. 100 cuadros por cada una, sin cliente conectado. */
static void barrido()
{
  static const int XCLKS[] = { 10, 16, 20 };
  static const pixformat_t FORMATOS[] = { PIXFORMAT_RGB565, PIXFORMAT_YUV422 };
  static const framesize_t TAMANOS[] = { FRAMESIZE_QVGA, FRAMESIZE_HQVGA, FRAMESIZE_QQVGA };
  static const int CALIDADES[] = { 50, 70 };

  Serial.println("[barrido] INICIO (3 relojes x 2 formatos x 3 tamanos x 2 calidades, 100 cuadros cada uno)");
  Serial.printf("[barrido] luz media al inicio (Y 0-255): %d\n", medir_luz());
  for (int x = 0; x < 3; x++) {
    for (int f = 0; f < 2; f++) {
      for (int t = 0; t < 3; t++) {
        Config g = { XCLKS[x], FORMATOS[f], TAMANOS[t], 70 };
        if (!pedir_reinicio(g)) {
          Serial.printf("[barrido] xclk=%d MHz %s %s: NO INICIO\n", g.xclkMHz, nombre_formato(g.fmt), nombre_tam(g.tam));
          continue;
        }
        for (int q = 0; q < 2; q++) {
          cfg.calidad = CALIDADES[q];
          Medida m = medir_pipeline(CUADROS_MEDIDOS);
          imprimir_medida("barrido", cfg, m);
        }
      }
    }
  }
  Serial.printf("[barrido] luz media al final (Y 0-255): %d\n", medir_luz());
  Serial.println("[barrido] FIN");
}
#endif

/* ── Utilidades de red ─────────────────────────────────────────────────── */

/* IP del cliente de una petición (texto). */
static void ip_del_cliente(httpd_req_t *req, char *salida, size_t n)
{
  salida[0] = 0;
  int fd = httpd_req_to_sockfd(req);
  struct sockaddr_storage ss;
  socklen_t len = sizeof(ss);
  if (fd < 0 || getpeername(fd, (struct sockaddr *)&ss, &len) != 0) {
    snprintf(salida, n, "?");
    return;
  }
  if (ss.ss_family == AF_INET) {
    inet_ntop(AF_INET, &((struct sockaddr_in *)&ss)->sin_addr, salida, n);
  } else if (ss.ss_family == AF_INET6) {
    const uint8_t *b = ((struct sockaddr_in6 *)&ss)->sin6_addr.un.u8_addr;
    bool mapeada = true;
    for (int i = 0; i < 10; i++) if (b[i] != 0) mapeada = false;
    if (mapeada && b[10] == 0xFF && b[11] == 0xFF) snprintf(salida, n, "%u.%u.%u.%u", b[12], b[13], b[14], b[15]);
    else snprintf(salida, n, "ipv6");
  } else {
    snprintf(salida, n, "?");
  }
}

static void log_peticion(httpd_req_t *req)
{
  char ip[48];
  ip_del_cliente(req, ip, sizeof(ip));
  Serial.printf("[http] GET %s desde %s\n", req->uri, ip);
}

/* Sin retardo de Nagle y con búfer de envío pequeño: el cuadro sale ya y no se acumulan viejos en la red. */
static void ajustar_socket(int fd)
{
  int uno = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &uno, sizeof(uno));
  int sb = 2 * 1460;
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sb, sizeof(sb));
  struct timeval tv = { 1, 500000 };   /* un cliente que no recibe en 1.5 s se corta */
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static bool enviar_todo(int fd, const uint8_t *p, size_t n)
{
  size_t enviado = 0;
  int esperas = 0;
  while (enviado < n) {
    int r = send(fd, p + enviado, n - enviado, 0);
    if (r > 0) {
      enviado += r;
      continue;
    }
    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && ++esperas < 100) {
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    return false;
  }
  return true;
}

/* ── Servidor HTTP ─────────────────────────────────────────────────────── */

static const char PAGINA[] =
  "<!doctype html><html><head><meta charset='utf-8'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>Camara del carrito</title></head>"
  "<body style='margin:0;background:#111;color:#eee;font-family:sans-serif'>"
  "<div style='position:relative'>"
  "<img src='/stream' style='width:100%;display:block'>"
  "<div id='t' style='position:absolute;top:0;left:0;right:0;font:10px monospace;color:#0f0;background:#0008;padding:2px'>...</div>"
  "</div>"
  "<p style='margin:8px'>"
  "<button onclick=\"fetch('/control?size=qvga')\">QVGA</button> "
  "<button onclick=\"fetch('/control?size=hqvga')\">HQVGA</button> "
  "<button onclick=\"fetch('/control?size=qqvga')\">QQVGA</button> "
  "<button onclick=\"fetch('/control?size=vga')\">VGA</button> "
  "<a style='color:#8cf' href='/capture'>un cuadro</a> <a style='color:#8cf' href='/stats'>/stats</a></p>"
  "<script>"
  "function s(){fetch('/stats').then(function(r){return r.json()}).then(function(j){"
  "document.getElementById('t').textContent="
  "'cap '+j.fps_captura.toFixed(1)+' fps | env '+j.fps_envio.toFixed(1)+' fps | edad '+j.edad_ms.toFixed(0)+' ms'"
  "+' | comp '+j.ms_compresion.toFixed(0)+' | envio '+j.ms_envio.toFixed(0)+' | desc '+j.descartados"
  "+' | rssi '+j.rssi+' | '+j.config.tam+' q'+j.config.calidad+' '+j.config.formato+' '+j.config.xclk_mhz+'MHz';"
  "}).catch(function(){})}"
  "setInterval(s,2000);s();"
  "</script></body></html>";

static esp_err_t responder_503(httpd_req_t *req, const char *texto)
{
  httpd_resp_set_status(req, "503 Service Unavailable");
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, texto, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t manejar_indice(httpd_req_t *req)
{
  log_peticion(req);
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, PAGINA, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t manejar_captura(httpd_req_t *req)
{
  log_peticion(req);
  ajustar_socket(httpd_req_to_sockfd(req));
  demanda_mas();
  Cuadro c;
  bool ok = siguiente_jpeg(c, 2500);
  if (!ok) {
    demanda_menos();
    return httpd_resp_send_500(req);
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  int64_t a = esp_timer_get_time();
  esp_err_t r = httpd_resp_send(req, (const char *)c.datos, c.largo);
  int64_t b = esp_timer_get_time();
  if (r == ESP_OK) sumar_envio(b - a, b - c.t0_us, c.largo);
  soltar_cuadro(c);
  demanda_menos();
  return r;
}

static uint8_t *envioBuf = NULL;
static size_t envioCap = 0;

/* Tarea que manda el MJPEG a un cliente (núcleo 1), sin bloquear al servidor. */
static void tarea_stream(void *arg)
{
  httpd_req_t *req = (httpd_req_t *)arg;
  int fd = httpd_req_to_sockfd(req);
  ajustar_socket(fd);
  static const char CABECERA[] =
    "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace;boundary=cuadro\r\n"
    "Cache-Control: no-store\r\nPragma: no-cache\r\nConnection: close\r\n\r\n";
  bool ok = enviar_todo(fd, (const uint8_t *)CABECERA, sizeof(CABECERA) - 1);
  int sinCuadro = 0;

  demanda_mas();
  while (ok) {
    Cuadro c;
    if (!siguiente_jpeg(c, 1500)) {
      if (++sinCuadro > 5) break;
      continue;
    }
    sinCuadro = 0;
    size_t necesario = c.largo + 160;
    if (necesario > envioCap) {
      uint8_t *nuevo = (uint8_t *)ps_realloc(envioBuf, necesario + 4096);
      if (!nuevo) { soltar_cuadro(c); break; }
      envioBuf = nuevo;
      envioCap = necesario + 4096;
    }
    /* Un solo envío por cuadro: cabecera de la parte + JPEG + salto de línea. */
    int n = snprintf((char *)envioBuf, 160,
                     "--cuadro\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned)c.largo);
    memcpy(envioBuf + n, c.datos, c.largo);
    memcpy(envioBuf + n + c.largo, "\r\n", 2);
    size_t total = n + c.largo + 2;
    int64_t t0 = c.t0_us;
    size_t largo = c.largo;
    soltar_cuadro(c);

    int64_t a = esp_timer_get_time();
    ok = enviar_todo(fd, envioBuf, total);
    int64_t b = esp_timer_get_time();
    if (ok) sumar_envio(b - a, b - t0, largo);
  }
  demanda_menos();
  Serial.println("[http] stream terminado");
  httpd_sess_trigger_close(servidor, fd);
  httpd_req_async_handler_complete(req);
  streamActivo = false;
  vTaskDelete(NULL);
}

static esp_err_t manejar_stream(httpd_req_t *req)
{
  log_peticion(req);
  if (streamActivo) {
    return responder_503(req, "ya hay un stream activo");
  }
  httpd_req_t *copia = NULL;
  if (httpd_req_async_handler_begin(req, &copia) != ESP_OK) {
    return httpd_resp_send_500(req);
  }
  streamActivo = true;
  if (xTaskCreatePinnedToCore(tarea_stream, "stream", 10240, copia, 3, NULL, 1) != pdPASS) {
    streamActivo = false;
    httpd_req_async_handler_complete(copia);
    return ESP_FAIL;
  }
  return ESP_OK;
}

static esp_err_t manejar_control(httpd_req_t *req)
{
  log_peticion(req);
  char consulta[48];
  char valor[10];
  if (httpd_req_get_url_query_str(req, consulta, sizeof(consulta)) != ESP_OK ||
      httpd_query_key_value(consulta, "size", valor, sizeof(valor)) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "uso: /control?size=qqvga|hqvga|qvga|vga");
  }
  Config nueva = cfg;
  if (strcmp(valor, "qvga") == 0) nueva.tam = FRAMESIZE_QVGA;
  else if (strcmp(valor, "hqvga") == 0) nueva.tam = FRAMESIZE_HQVGA;
  else if (strcmp(valor, "qqvga") == 0) nueva.tam = FRAMESIZE_QQVGA;
  else if (strcmp(valor, "vga") == 0) nueva.tam = FRAMESIZE_VGA;
  else return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "size debe ser qqvga, hqvga, qvga o vga");

  if (jpegDelSensor || nueva.tam == cfg.tam || pedir_reinicio(nueva)) {
    Serial.printf("[http] tamano -> %s\n", nombre_tam(cfg.tam));
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, valor, HTTPD_RESP_USE_STRLEN);
  }
  return httpd_resp_send_500(req);
}

static esp_err_t manejar_stats(httpd_req_t *req)
{
  log_peticion(req);
  Resumen r = resumir(3);   /* últimos 3 segundos */
  char json[640];
  int n = snprintf(json, sizeof(json),
    "{\"fps_captura\":%.1f,\"fps_envio\":%.1f,\"ms_captura\":%.1f,\"ms_compresion\":%.1f,\"ms_envio\":%.1f,"
    "\"edad_ms\":%.0f,\"edad_max_ms\":%.0f,\"envio_max_ms\":%.0f,\"descartados\":%u,\"descartados_total\":%u,\"jpeg_medio_bytes\":%.0f,\"stream\":%s,"
    "\"rssi\":%d,\"heap_libre\":%u,\"psram_libre\":%u,\"uptime_s\":%u,"
    "\"config\":{\"xclk_mhz\":%d,\"formato\":\"%s\",\"tam\":\"%s\",\"calidad\":%d,\"sensor\":\"%s\"}}",
    r.fpsCaptura, r.fpsEnvio, r.msCaptura, r.msCompresion, r.msEnvio, r.edadMs, r.edadMaxMs, r.envioMaxMs,
    (unsigned)r.descartados, (unsigned)totalDescartados, r.jpegMedio, streamActivo ? "true" : "false",
    WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : 0, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(),
    (unsigned)(millis() / 1000), cfg.xclkMHz, nombre_formato(cfg.fmt), nombre_tam(cfg.tam), cfg.calidad, nombreSensor);
  if (n <= 0 || n >= (int)sizeof(json)) return httpd_resp_send_500(req);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, n);
}

static void iniciar_servidor()
{
  httpd_config_t c = HTTPD_DEFAULT_CONFIG();
  c.server_port = 80;
  c.stack_size = 10240;
  c.max_uri_handlers = 8;
  c.lru_purge_enable = true;
  c.send_wait_timeout = 2;
  if (httpd_start(&servidor, &c) != ESP_OK) {
    Serial.println("[http] ERROR: no arranco el servidor");
    return;
  }
  const httpd_uri_t rutas[] = {
    { "/",        HTTP_GET, manejar_indice,  NULL },
    { "/capture", HTTP_GET, manejar_captura, NULL },
    { "/stream",  HTTP_GET, manejar_stream,  NULL },
    { "/control", HTTP_GET, manejar_control, NULL },
    { "/stats",   HTTP_GET, manejar_stats,   NULL },
  };
  for (size_t i = 0; i < sizeof(rutas) / sizeof(rutas[0]); i++) {
    httpd_register_uri_handler(servidor, &rutas[i]);
  }
  Serial.println("[http] servidor listo en el puerto 80 (/, /capture, /stream, /control, /stats)");
}

/* ── Autoprueba: la placa se pide a sí misma las rutas (sin PC ni teléfono) ─ */

#if AUTOPRUEBA
/* Pide `ruta` a la propia IP, lee hasta `maxMs` y devuelve los bytes leídos. Solo cuenta bytes: no guarda ni mira nada. */
static size_t autopedir(const char *ruta, uint32_t maxMs, uint8_t *primeros, size_t nPrimeros, int *cuadros, int *estado)
{
  WiFiClient cli;
  *cuadros = 0;
  *estado = 0;
  if (!cli.connect(IPAddress(127, 0, 0, 1), 80, 3000)) return 0;   /* lazo local de lwIP: la propia placa */
  cli.setNoDelay(true);
  cli.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", ruta, WiFi.localIP().toString().c_str());
  size_t total = 0;
  size_t guardados = 0;
  uint32_t t0 = millis();
  static uint8_t buf[2048];
  char marca[] = "--cuadro";
  int pos = 0;
  while (millis() - t0 < maxMs && (cli.connected() || cli.available())) {
    int n = cli.read(buf, sizeof(buf));
    if (n <= 0) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
    for (int i = 0; i < n; i++) {
      if (guardados < nPrimeros) primeros[guardados++] = buf[i];
      if (buf[i] == (uint8_t)marca[pos]) { if (marca[++pos] == 0) { (*cuadros)++; pos = 0; } } else pos = (buf[i] == '-') ? 1 : 0;
    }
    total += n;
  }
  if (guardados >= 12 && memcmp(primeros, "HTTP/1.1 ", 9) == 0) *estado = atoi((const char *)primeros + 9);
  cli.stop();
  return total;
}

static void autoprueba()
{
  Serial.println("[autoprueba] la placa se pide a si misma las rutas (por 127.0.0.1; no mide el WiFi)");
  uint8_t pr[16];
  int cuadros, estado;
  size_t n = autopedir("/stats", 3000, pr, sizeof(pr), &cuadros, &estado);
  Serial.printf("[autoprueba] /stats: HTTP %d, %u bytes\n", estado, (unsigned)n);
  n = autopedir("/capture", 4000, pr, sizeof(pr), &cuadros, &estado);
  Serial.printf("[autoprueba] /capture: HTTP %d, %u bytes en total (cabeceras incluidas)\n", estado, (unsigned)n);
  n = autopedir("/control?size=qvga", 3000, pr, sizeof(pr), &cuadros, &estado);
  Serial.printf("[autoprueba] /control?size=qvga: HTTP %d\n", estado);
  uint32_t t0 = millis();
  n = autopedir("/stream", 6000, pr, sizeof(pr), &cuadros, &estado);
  uint32_t ms = millis() - t0;
  Serial.printf("[autoprueba] /stream: HTTP %d, %d cuadros en %u ms (%.1f fps), %u bytes\n",
                estado, cuadros, (unsigned)ms, cuadros * 1000.0f / ms, (unsigned)n);
  /* /stats justo después del stream: son solo números. */
  vTaskDelay(pdMS_TO_TICKS(1500));
  static uint8_t cuerpo[1000];
  memset(cuerpo, 0, sizeof(cuerpo));
  autopedir("/stats", 3000, cuerpo, sizeof(cuerpo) - 1, &cuadros, &estado);
  const char *json = strstr((const char *)cuerpo, "\r\n\r\n");
  Serial.printf("[autoprueba] /stats despues del stream: %s\n", json ? json + 4 : "(sin cuerpo)");
}

static void tarea_autoprueba(void *)
{
  vTaskDelay(pdMS_TO_TICKS(500));
  autoprueba();
  vTaskDelete(NULL);
}
#endif

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

/* Espera hasta TIEMPO_CONEXION_MS a que conecte. */
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
  Serial.printf("[sistema] CPU %u MHz, PSRAM %s, libre %u bytes; heap libre %u bytes\n",
                (unsigned)getCpuFrequencyMhz(), psramFound() ? "SI" : "NO", (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());

  candadoCasilla = xSemaphoreCreateMutex();
  candadoUso = xSemaphoreCreateMutex();
  candadoReinicio = xSemaphoreCreateMutex();
  hayCuadro = xSemaphoreCreateBinary();
  reinicioHecho = xSemaphoreCreateBinary();

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

  /* La captura corre en el núcleo 0; la compresión y el envío, en el 1. */
  xTaskCreatePinnedToCore(tarea_captura, "captura", 4096, NULL, 4, NULL, 0);

#if MODO_BARRIDO
  if (!jpegDelSensor) barrido();
  if (!pedir_reinicio(CONFIG_DEFECTO)) Serial.println("[camara] ERROR: no se pudo volver a la configuracion por defecto");
#endif

  /* Medida de la configuración activa en la propia placa, sin cliente (todavía no hay WiFi ni servidor). */
  if (!jpegDelSensor) {
    Medida m = medir_pipeline(CUADROS_MEDIDOS);
    imprimir_medida("medida", cfg, m);
  }

  conectar_wifi();
  while (!esperar_conexion()) {
    conectar_wifi();
  }
  reiniciar_estadisticas();
  iniciar_servidor();
#if AUTOPRUEBA
  /* En otra tarea: así loop() sigue tomando las estadísticas mientras corre. */
  xTaskCreatePinnedToCore(tarea_autoprueba, "autoprueba", 8192, NULL, 1, NULL, 0);
#endif
}

void loop()
{
  static uint32_t ultimoEstado = 0;
  static uint32_t ultimaRevision = 0;
  static uint32_t ultimaInstantanea = 0;

  if (mapaActivo == NULL) {
    if (millis() - ultimoEstado >= PERIODO_ESTADO_MS) {
      ultimoEstado = millis();
      Serial.println("[estado] DETENIDO: la camara no inicio con ningun mapa de pines");
    }
    delay(100);
    return;
  }

  /* Una instantánea por segundo para las estadísticas. */
  if (millis() - ultimaInstantanea >= 1000) {
    uint32_t ms = millis() - ultimaInstantanea;
    ultimaInstantanea = millis();
    tomar_instantanea(ms);
  }

  /* Si se cae el WiFi, reintenta (cada 15 s sin conexión). */
  if (millis() - ultimaRevision >= 1000) {
    ultimaRevision = millis();
    if (WiFi.status() != WL_CONNECTED && millis() - inicioIntento > TIEMPO_CONEXION_MS) {
      Serial.println("[wifi] conexion perdida: reintento");
      conectar_wifi();
    }
  }

  if (millis() - ultimoEstado >= PERIODO_ESTADO_MS) {
    ultimoEstado = millis();
    Resumen r = resumir(5);
    Serial.printf("[estado] wifi=%s ip=%s %s q%d %s %dMHz stream=%s | captura=%.1f fps (%.0f ms entre cuadros) | envio=%.1f fps, %.0f ms (max %.0f) | compresion=%.0f ms | edad=%.0f ms (max %.0f) | descartados=%u | JPEG=%.0f B | RSSI=%d dBm | heap=%u B, PSRAM=%u B\n",
                  WiFi.status() == WL_CONNECTED ? "conectado" : "SIN CONEXION",
                  WiFi.localIP().toString().c_str(), nombre_tam(cfg.tam), cfg.calidad, nombre_formato(cfg.fmt), cfg.xclkMHz,
                  streamActivo ? "si" : "no", r.fpsCaptura, r.msCaptura, r.fpsEnvio, r.msEnvio, r.envioMaxMs, r.msCompresion, r.edadMs, r.edadMaxMs,
                  (unsigned)r.descartados, r.jpegMedio, WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  }
  delay(20);
}
