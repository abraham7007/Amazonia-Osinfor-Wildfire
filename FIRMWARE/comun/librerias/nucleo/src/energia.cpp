#include "energia.h"

#include "config.h"

bool es_ventana_diurna(const HoraLocal &h) {
  if (!h.valida) return true;  // sin hora fiable se opera de forma conservadora
  return h.hora >= HORA_INICIO_DIURNA && h.hora < HORA_FIN_DIURNA;
}

Accion decidir_accion(const HoraLocal &h, uint8_t soc_pct) {
  if (!es_ventana_diurna(h)) return Accion::DORMIR_HASTA_ALBA;
  if (soc_pct < SOC_CRITICO_PCT) return Accion::SOLO_TELEMETRIA;
  return Accion::CAPTURAR;
}

uint32_t intervalo_siguiente(const HoraLocal &h, uint8_t soc_pct) {
  if (soc_pct < SOC_CRITICO_PCT) return INTERVALO_CRITICO_S;
  if (soc_pct < SOC_AHORRO_PCT) return INTERVALO_AHORRO_S;
  if (!h.valida) return INTERVALO_NORMAL_S;
  const bool alto_riesgo = h.hora >= HORA_INICIO_ALTO_RIESGO && h.hora < HORA_FIN_ALTO_RIESGO;
  return alto_riesgo ? INTERVALO_NORMAL_S : INTERVALO_AHORRO_S;
}

uint32_t segundos_hasta_alba(const HoraLocal &h) {
  if (!h.valida) return INTERVALO_AHORRO_S;
  const int32_t ahora = (int32_t)h.hora * 3600 + (int32_t)h.minuto * 60;
  const int32_t alba = (int32_t)HORA_INICIO_DIURNA * 3600;
  int32_t delta = alba - ahora;
  if (delta <= 0) delta += 24 * 3600;
  return (uint32_t)delta;
}
