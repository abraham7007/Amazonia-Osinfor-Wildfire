// hal_energia.h — Medida de bateria/ambiente y entrada a deep-sleep.
#pragma once

#include <stdint.h>

struct EstadoEnergia {
  uint8_t soc_pct;     // estado de carga 0-100
  uint16_t mv_bateria;
  int8_t temp_c;
  uint8_t humedad_pct;
  bool valido;
};

EstadoEnergia leer_energia();

// Entra en deep-sleep durante `segundos`. No retorna: el MCU reinicia.
void dormir_profundo(uint32_t segundos);
