#include "camara.h"

#include <Arduino.h>
#include <esp_camera.h>
#include <string.h>

#include "memoria.h"
#include "placa.h"

namespace {

camera_fb_t *g_fb = nullptr;
uint8_t *g_rgb = nullptr;
size_t g_rgb_bytes = 0;
bool g_listo = false;
const char *g_sensor = "sin iniciar";

const char *nombre_sensor(uint16_t pid) {
  switch (pid) {
    case 0x26: return "OV2640";
    case 0x36: return "OV3660";
    case 0x56: return "OV5640";
    case 0x77: return "OV7725";
    case 0x30: return "NT99141";
    case 0x9a: return "GC032A";
    default: return "desconocido";
  }
}

}  // namespace

const char *camara_sensor() { return g_sensor; }

bool camara_iniciar() {
  if (g_listo) return true;

#if PLACA_PIN_ENCENDIDO_CAMARA >= 0
  // El NE101 corta la alimentacion del sensor para ahorrar en deep-sleep. Sin
  // esto, esp_camera_init() devuelve 0x105 y parece un pinout mal puesto cuando
  // en realidad la camara esta apagada.
  pinMode(PLACA_PIN_ENCENDIDO_CAMARA, OUTPUT);
  digitalWrite(PLACA_PIN_ENCENDIDO_CAMARA, HIGH);
  delay(100);
#endif

  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_pwdn = kPines.pwdn;
  c.pin_reset = kPines.reset;
  c.pin_xclk = kPines.xclk;
  c.pin_sccb_sda = kPines.siod;
  c.pin_sccb_scl = kPines.sioc;
  c.pin_d7 = kPines.y9; c.pin_d6 = kPines.y8; c.pin_d5 = kPines.y7; c.pin_d4 = kPines.y6;
  c.pin_d3 = kPines.y5; c.pin_d2 = kPines.y4; c.pin_d1 = kPines.y3; c.pin_d0 = kPines.y2;
  c.pin_vsync = kPines.vsync;
  c.pin_href = kPines.href;
  c.pin_pclk = kPines.pclk;
  c.xclk_freq_hz = 20000000;
  // JPEG y no RGB565 directo: a 1280x720 el buffer RGB565 son 1,8 MB por cuadro
  // y el driver querria dos; en JPEG cabe holgado y se convierte una sola vez.
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_HD;
  c.jpeg_quality = 10;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&c) != ESP_OK) return false;

  sensor_t *s = esp_camera_sensor_get();
  if (s != nullptr) {
    g_sensor = nombre_sensor(s->id.PID);
    // Escena fija: sin AWB agresivo la respuesta de color es estable entre
    // capturas, que es lo que el modelo necesita para separar el humo pardo de
    // la niebla blanca.
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 0);
    s->set_gain_ctrl(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
  }

  g_rgb_bytes = (size_t)1280 * 720 * 3;
  g_rgb = (uint8_t *)reservar_grande(g_rgb_bytes);
  if (g_rgb == nullptr) {
    esp_camera_deinit();
    return false;
  }

  g_listo = true;
  return true;
}

bool camara_capturar(Cuadro *destino) {
  if (!g_listo || destino == nullptr) return false;

  // Descartar un cuadro: el primero tras encender el sensor sale con la
  // exposicion sin converger.
  camera_fb_t *descarte = esp_camera_fb_get();
  if (descarte != nullptr) esp_camera_fb_return(descarte);

  g_fb = esp_camera_fb_get();
  if (g_fb == nullptr) return false;

  if (!fmt2rgb888(g_fb->buf, g_fb->len, g_fb->format, g_rgb)) {
    esp_camera_fb_return(g_fb);
    g_fb = nullptr;
    return false;
  }

  // fmt2rgb888 entrega los bytes en orden BGR; el modelo se entreno con RGB.
  for (size_t i = 0; i < g_rgb_bytes; i += 3) {
    const uint8_t t = g_rgb[i];
    g_rgb[i] = g_rgb[i + 2];
    g_rgb[i + 2] = t;
  }

  destino->datos = g_rgb;
  destino->ancho = (int)g_fb->width;
  destino->alto = (int)g_fb->height;
  destino->paso_fila = (size_t)g_fb->width * 3;
  return true;
}

void camara_liberar(Cuadro *cuadro) {
  if (g_fb != nullptr) {
    esp_camera_fb_return(g_fb);
    g_fb = nullptr;
  }
  if (cuadro != nullptr) cuadro->datos = nullptr;
}

void camara_apagar() {
  if (!g_listo) return;
  esp_camera_deinit();
  // Cortar la alimentacion del sensor es lo que hace que el deep-sleep del
  // NE101 baje de verdad: sin esto el sensor sigue consumiendo dormido el SoC.
#if PLACA_PIN_ENCENDIDO_CAMARA >= 0
  digitalWrite(PLACA_PIN_ENCENDIDO_CAMARA, LOW);
#endif
  g_listo = false;
}
