// T03 — Camara: identificacion del sensor y captura.
//
// Verifica lo que el firmware del nodo da por hecho:
//   - que la camara arranca con el pinout de ESTA placa,
//   - que sensor es realmente (OV2640, OV3660 y OV5640 no tienen el mismo alcance),
//   - que FRAMESIZE_HD (1280x720) se puede reservar, porque es la geometria con la
//     que se entreno el modelo,
//   - cuanto tarda una captura, que entra en el presupuesto de energia.

#include <Arduino.h>
#include <esp_camera.h>

#include "placa.h"
#include "pruebas.h"

namespace {

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

camera_config_t configurar(framesize_t tam, pixformat_t formato) {
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
  c.pixel_format = formato;
  c.frame_size = tam;
  c.jpeg_quality = 10;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  return c;
}

}  // namespace

bool t03_camara() {
  bool bien = true;

#if PLACA_PIN_ENCENDIDO_CAMARA >= 0
  // El NE101 mantiene el sensor sin alimentar mientras duerme. Sin esto,
  // esp_camera_init() devuelve 0x105 (ESP_ERR_NOT_FOUND) y parece un pinout mal
  // puesto cuando en realidad la camara simplemente esta apagada.
  pinMode(PLACA_PIN_ENCENDIDO_CAMARA, OUTPUT);
  digitalWrite(PLACA_PIN_ENCENDIDO_CAMARA, HIGH);
  delay(100);
  dato("dominio de potencia de la camara activado (GPIO %d)", PLACA_PIN_ENCENDIDO_CAMARA);
#endif

  dato("pinout: XCLK=%d SDA=%d SCL=%d VSYNC=%d HREF=%d PCLK=%d", kPines.xclk, kPines.siod,
       kPines.sioc, kPines.vsync, kPines.href, kPines.pclk);

  camera_config_t cfg = configurar(FRAMESIZE_HD, PIXFORMAT_JPEG);
  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    fallo("esp_camera_init fallo: 0x%x", err);
    fallo("comprueba el pinout de %s en HARDWARE/README.md §3", PLACA_NOMBRE);
    return false;
  }
  ok("camara inicializada a 1280x720 JPEG");

  sensor_t *s = esp_camera_sensor_get();
  if (s == nullptr) {
    fallo("no se pudo leer el sensor");
    esp_camera_deinit();
    return false;
  }
  const char *detectado = nombre_sensor(s->id.PID);
  dato("sensor detectado: %s (PID 0x%02X)  — esperado en esta placa: %s", detectado,
       (unsigned)s->id.PID, PLACA_SENSOR_ESP);
  if (strcmp(detectado, "desconocido") == 0) {
    fallo("sensor no reconocido; la calidad de imagen no esta caracterizada");
    bien = false;
  }

  // Escena fija: sin AWB agresivo la respuesta de color es estable entre
  // capturas, que es lo que el modelo necesita para separar el humo pardo de la
  // niebla blanca.
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 0);
  s->set_gain_ctrl(s, 1);
  s->set_exposure_ctrl(s, 1);

  // Primera captura descartada: la exposicion no ha convergido.
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);

  uint32_t suma_us = 0;
  const int n = 5;
  int validas = 0;
  for (int i = 0; i < n; ++i) {
    const uint32_t t0 = micros();
    fb = esp_camera_fb_get();
    const uint32_t t1 = micros();
    if (fb == nullptr) {
      fallo("captura %d fallida", i + 1);
      bien = false;
      continue;
    }
    if ((int)fb->width != 1280 || (int)fb->height != 720) {
      fallo("el sensor entrego %ux%u en vez de 1280x720", (unsigned)fb->width,
            (unsigned)fb->height);
      bien = false;
    }
    suma_us += t1 - t0;
    ++validas;
    if (i == 0) dato("primer cuadro: %ux%u, %u bytes JPEG", (unsigned)fb->width,
                     (unsigned)fb->height, (unsigned)fb->len);
    esp_camera_fb_return(fb);
  }

  if (validas > 0) {
    const uint32_t media = suma_us / validas;
    ok("%d/%d capturas correctas, media %u ms", validas, n, (unsigned)(media / 1000));
    dato("a 48 capturas/dia son %.1f s diarios solo de captura", media * 48.0 / 1e6);
  } else {
    bien = false;
  }

  // El firmware convierte JPEG a RGB888 antes de tesela. Comprobarlo aqui evita
  // descubrir en campo que fmt2rgb888 no soporta lo que entrega este sensor.
  fb = esp_camera_fb_get();
  if (fb != nullptr) {
    uint8_t *rgb = (uint8_t *)heap_caps_malloc((size_t)1280 * 720 * 3, MALLOC_CAP_SPIRAM);
    if (rgb == nullptr) {
      fallo("sin PSRAM para el buffer RGB888");
      bien = false;
    } else {
      const uint32_t t0 = micros();
      const bool conv = fmt2rgb888(fb->buf, fb->len, fb->format, rgb);
      const uint32_t t1 = micros();
      if (conv) {
        ok("conversion JPEG -> RGB888 correcta, %u ms", (unsigned)((t1 - t0) / 1000));
      } else {
        fallo("fmt2rgb888 fallo: el firmware del nodo no podria procesar el cuadro");
        bien = false;
      }
      heap_caps_free(rgb);
    }
    esp_camera_fb_return(fb);
  }

  esp_camera_deinit();
  return bien;
}
