// Tiempo, memoria, energia y estado persistente en ESP32-S3.
// Nada de esto depende de la placa concreta: vale igual para el XIAO ESP32-S3
// Sense y para el NE101.
#include <Arduino.h>
#include <esp_sleep.h>
#include <time.h>

#include <string.h>

#include "config.h"
#include "estado_persistente.h"
#include "hal_energia.h"
#include "hal_tiempo.h"
#include "memoria.h"

namespace {

constexpr uint32_t kMarca = 0xA3F10001u;

// RTC_DATA_ATTR sobrevive al deep-sleep (dominio RTC lento siempre alimentado).
RTC_DATA_ATTR EstadoPersistente g_estado;
bool g_frio = false;

// Divisor del ADC de bateria. Depende de la placa: se ajusta al cerrar ADR-001.
constexpr int kPinBateria = -1;        // -1 = sin medida, se reporta SoC nominal
constexpr float kDivisorBateria = 2.0f;

// Curva LiFePO4 1S: 3,65 V lleno / 2,50 V vacio, muy plana en el medio.
uint8_t soc_desde_mv(uint16_t mv) {
  static const struct { uint16_t mv; uint8_t pct; } curva[] = {
      {3400, 100}, {3300, 90}, {3250, 70}, {3200, 40}, {3150, 20},
      {3000, 10},  {2800, 5},  {2500, 0},
  };
  if (mv >= curva[0].mv) return 100;
  for (size_t i = 1; i < sizeof(curva) / sizeof(curva[0]); ++i) {
    if (mv >= curva[i].mv) {
      const int span = curva[i - 1].mv - curva[i].mv;
      const int pct_span = curva[i - 1].pct - curva[i].pct;
      return (uint8_t)(curva[i].pct + (mv - curva[i].mv) * pct_span / span);
    }
  }
  return 0;
}

}  // namespace

void *reservar_grande(size_t bytes) {
  void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p == nullptr) p = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p;
}

void liberar_grande(void *p) { heap_caps_free(p); }

uint32_t ms_ahora() { return millis(); }

void fijar_hora_utc(uint32_t epoch_s) {
  struct timeval tv = {.tv_sec = (time_t)epoch_s, .tv_usec = 0};
  settimeofday(&tv, nullptr);
}

HoraLocal hora_local() {
  HoraLocal h = {};
  time_t ahora = time(nullptr);
  // Antes de 2020 significa que nunca se fijo la hora.
  h.valida = ahora > 1577836800;
  struct tm tm_local;
  localtime_r(&ahora, &tm_local);
  h.anio = (uint16_t)(tm_local.tm_year + 1900);
  h.mes = (uint8_t)(tm_local.tm_mon + 1);
  h.dia = (uint8_t)tm_local.tm_mday;
  h.hora = (uint8_t)tm_local.tm_hour;
  h.minuto = (uint8_t)tm_local.tm_min;
  return h;
}

EstadoEnergia leer_energia() {
  EstadoEnergia e = {};
  if (kPinBateria >= 0) {
    const uint32_t mv_adc = analogReadMilliVolts(kPinBateria);
    e.mv_bateria = (uint16_t)(mv_adc * kDivisorBateria);
    e.soc_pct = soc_desde_mv(e.mv_bateria);
    e.valido = true;
  } else {
    e.mv_bateria = 3300;
    e.soc_pct = 100;
    e.valido = false;
  }
  e.temp_c = (int8_t)temperatureRead();  // sensor interno del SoC, no ambiente
  e.humedad_pct = 0;                     // sin sensor de humedad en el BOM actual
  return e;
}

void dormir_profundo(uint32_t segundos) {
  esp_sleep_enable_timer_wakeup((uint64_t)segundos * 1000000ULL);
  esp_deep_sleep_start();  // no retorna
}

EstadoPersistente *estado() {
  if (g_estado.marca != kMarca) {
    memset(&g_estado, 0, sizeof(g_estado));
    g_estado.marca = kMarca;
    g_frio = true;
  }
  return &g_estado;
}

bool arranque_en_frio() { return g_frio; }

