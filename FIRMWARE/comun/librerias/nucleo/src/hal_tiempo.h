// hal_tiempo.h — Reloj monotono y hora local.
#pragma once

#include <stdint.h>

struct HoraLocal {
  uint16_t anio;
  uint8_t mes;
  uint8_t dia;
  uint8_t hora;
  uint8_t minuto;
  bool valida;  // false mientras no haya referencia horaria fiable
};

uint32_t ms_ahora();
HoraLocal hora_local();

// Fija la hora (downlink de comisionado o RTC externo). Segundos UTC epoch.
void fijar_hora_utc(uint32_t epoch_s);
