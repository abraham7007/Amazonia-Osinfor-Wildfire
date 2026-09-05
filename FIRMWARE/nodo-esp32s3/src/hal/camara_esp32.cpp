// Driver de camara sobre esp32-camera (OV2640 del XIAO ESP32-S3 Sense u OV5640
// del NE101).
//
// Los dos pinout estan verificados: provienen del informe de implementacion del
// prototipo, donde figuran como probados en cada placa. Ver HARDWARE/README.md §3.
//
// Nota de resolucion: el modelo se entreno y evaluo sobre cuadros de 1280x720
// (rejilla de 32 teselas). FRAMESIZE_HD da exactamente esa geometria. Con
// FRAMESIZE_UXGA (1600x1200) la rejilla pasa a 70 teselas: mas del doble de
// inferencias por captura, y por tanto mas del doble de energia. No subir la
// resolucion sin rehacer el presupuesto energetico.
#if defined(ESP32) && defined(USAR_CAMARA_ESP32)

#include <Arduino.h>
#include <esp_camera.h>

#include "hal_camara.h"
#include "memoria.h"

namespace {

// ---------------------------------------------------------------------------
// PINOUT por placa. Verificado en HARDWARE/README.md §3.
// ---------------------------------------------------------------------------
#if defined(PLACA_XIAO_S3_SENSE)
constexpr int kPinPwdn = -1, kPinReset = -1, kPinXclk = 10, kPinSiod = 40, kPinSioc = 39;
constexpr int kPinY9 = 48, kPinY8 = 11, kPinY7 = 12, kPinY6 = 14, kPinY5 = 16;
constexpr int kPinY4 = 18, kPinY3 = 17, kPinY2 = 15;
constexpr int kPinVsync = 38, kPinHref = 47, kPinPclk = 13;
// El XIAO alimenta el sensor mientras la placa lo este: no hay linea que conmutar.
constexpr int kPinEncendidoCamara = -1;
#elif defined(PLACA_NE101)
constexpr int kPinPwdn = -1, kPinReset = -1, kPinXclk = 15, kPinSiod = 4, kPinSioc = 5;
constexpr int kPinY9 = 11, kPinY8 = 9, kPinY7 = 8, kPinY6 = 10, kPinY5 = 12;
constexpr int kPinY4 = 18, kPinY3 = 17, kPinY2 = 16;
constexpr int kPinVsync = 6, kPinHref = 7, kPinPclk = 13;
// El NE101 corta la alimentacion del sensor en deep-sleep. Sin poner este GPIO en
// alto antes de esp_camera_init(), la camara no responde y el error (0x105) se
// confunde con un pinout mal puesto.
constexpr int kPinEncendidoCamara = 3;
#else
#error "Definir PLACA_XIAO_S3_SENSE o PLACA_NE101 en platformio.ini."
#endif

camera_fb_t *g_fb = nullptr;
uint8_t *g_rgb = nullptr;
size_t g_rgb_bytes = 0;
bool g_listo = false;

}  // namespace

bool camara_iniciar() {
  if (g_listo) return true;

  if (kPinEncendidoCamara >= 0) {
    pinMode(kPinEncendidoCamara, OUTPUT);
    digitalWrite(kPinEncendidoCamara, HIGH);
    delay(100);  // el sensor necesita que la alimentacion se estabilice
  }

  camera_config_t cfg = {};
  cfg.pin_pwdn = kPinPwdn;
  cfg.pin_reset = kPinReset;
  cfg.pin_xclk = kPinXclk;
  cfg.pin_sccb_sda = kPinSiod;
  cfg.pin_sccb_scl = kPinSioc;
  cfg.pin_d7 = kPinY9; cfg.pin_d6 = kPinY8; cfg.pin_d5 = kPinY7; cfg.pin_d4 = kPinY6;
  cfg.pin_d3 = kPinY5; cfg.pin_d2 = kPinY4; cfg.pin_d1 = kPinY3; cfg.pin_d0 = kPinY2;
  cfg.pin_vsync = kPinVsync;
  cfg.pin_href = kPinHref;
  cfg.pin_pclk = kPinPclk;
  cfg.xclk_freq_hz = 20000000;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  // JPEG y no RGB565 directo: a 1280x720 el buffer RGB565 son 1,8 MB por cuadro
  // y el driver necesita dos; en JPEG cabe holgado y se convierte una sola vez.
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size = FRAMESIZE_HD;  // 1280x720, la geometria del entrenamiento
  cfg.jpeg_quality = 10;          // menor = mejor calidad
  cfg.fb_count = 1;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&cfg) != ESP_OK) return false;

  sensor_t *s = esp_camera_sensor_get();
  if (s != nullptr) {
    // El nodo mira siempre la misma escena: sin AWB agresivo la respuesta de
    // color es estable entre capturas, que es lo que el modelo espera para
    // distinguir el humo pardo de la niebla blanca.
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

  // fmt2rgb888 entrega BGR en este orden de bytes; el modelo se entreno con RGB.
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
  // Cortar la alimentacion del sensor es lo que hace que el deep-sleep del NE101
  // baje de verdad: sin esto el sensor sigue consumiendo con el SoC dormido.
  if (kPinEncendidoCamara >= 0) digitalWrite(kPinEncendidoCamara, LOW);
  g_listo = false;
}

#endif  // ESP32 && USAR_CAMARA_ESP32
