// T04 — Deep-sleep y memoria RTC.
//
// El nodo no tiene bucle: cada despertar ejecuta una pasada y vuelve a dormir.
// Eso solo funciona si dos cosas son ciertas en ESTA placa: que el temporizador
// RTC despierta de verdad, y que lo guardado en memoria RTC sobrevive. Si la
// segunda falla, el nodo repetiria el join OTAA (o el arranque de sesion) en cada
// despertar y agotaria la bateria sin que nada parezca roto.

#include <Arduino.h>
#include <esp_sleep.h>

#include "placa.h"
#include "pruebas.h"

namespace {
constexpr uint32_t kMarca = 0x50AF1234u;
RTC_DATA_ATTR uint32_t g_marca = 0;
RTC_DATA_ATTR uint32_t g_ciclos = 0;
RTC_DATA_ATTR uint8_t g_relleno[64];
}  // namespace

bool t04_sueno() {
  const esp_sleep_wakeup_cause_t causa = esp_sleep_get_wakeup_cause();

  if (causa == ESP_SLEEP_WAKEUP_TIMER && g_marca == kMarca) {
    ++g_ciclos;
    bool intacto = true;
    for (int i = 0; i < 64; ++i) {
      if (g_relleno[i] != (uint8_t)(i * 7 + 13)) intacto = false;
    }
    ok("despertar por temporizador RTC correcto (ciclo %u)", (unsigned)g_ciclos);
    if (intacto) {
      ok("los 64 B de memoria RTC sobrevivieron intactos al deep-sleep");
    } else {
      fallo("la memoria RTC se corrompio: el nodo perderia su estado entre capturas");
      return false;
    }
    dato("tiempo hasta este punto desde el despertar: %u ms", (unsigned)millis());
    dato("prueba superada. Vuelve a lanzar T04 para dormir otros 10 s.");
    return true;
  }

  dato("preparando la prueba: se escribe un patron en memoria RTC y se duerme 10 s");
  g_marca = kMarca;
  g_ciclos = 0;
  for (int i = 0; i < 64; ++i) g_relleno[i] = (uint8_t)(i * 7 + 13);

  dato("motivo del arranque actual: %d (0 = encendido en frio)", (int)causa);
  dato("durmiendo 10 s... la placa se reiniciara y la prueba continuara sola.");
  Serial.flush();

  esp_sleep_enable_timer_wakeup(10ULL * 1000000ULL);
  esp_deep_sleep_start();
  return false;  // inalcanzable
}
