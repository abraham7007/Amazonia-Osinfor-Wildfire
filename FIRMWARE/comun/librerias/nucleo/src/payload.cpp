#include "payload.h"

size_t serializar_alerta(const Alerta &a, uint8_t *buffer, size_t capacidad) {
  if (buffer == nullptr || capacidad < PAYLOAD_ALERTA_BYTES) return 0;
  buffer[0] = a.node_id;
  buffer[1] = (uint8_t)a.tipo;
  buffer[2] = a.confianza_pct > 100 ? 100 : a.confianza_pct;
  buffer[3] = a.bateria_pct > 100 ? 100 : a.bateria_pct;
  buffer[4] = temp_a_byte(a.temp_c);
  buffer[5] = a.flags;
  return PAYLOAD_ALERTA_BYTES;
}

size_t serializar_heartbeat(const Heartbeat &h, uint8_t *buffer, size_t capacidad) {
  if (buffer == nullptr || capacidad < PAYLOAD_HEARTBEAT_BYTES) return 0;
  buffer[0] = h.node_id;
  buffer[1] = (uint8_t)MSG_HEARTBEAT;
  const uint32_t lat = (uint32_t)h.lat_e7;
  const uint32_t lon = (uint32_t)h.lon_e7;
  buffer[2] = (uint8_t)(lat >> 24);
  buffer[3] = (uint8_t)(lat >> 16);
  buffer[4] = (uint8_t)(lat >> 8);
  buffer[5] = (uint8_t)(lat);
  buffer[6] = (uint8_t)(lon >> 24);
  buffer[7] = (uint8_t)(lon >> 16);
  buffer[8] = (uint8_t)(lon >> 8);
  buffer[9] = (uint8_t)(lon);
  buffer[10] = h.bateria_pct > 100 ? 100 : h.bateria_pct;
  buffer[11] = temp_a_byte(h.temp_c);
  buffer[12] = h.humedad_pct > 100 ? 100 : h.humedad_pct;
  buffer[13] = h.fw_ver;
  return PAYLOAD_HEARTBEAT_BYTES;
}

uint8_t confianza_a_byte(uint16_t prob_x1000) {
  if (prob_x1000 >= 1000) return 100;
  return (uint8_t)((prob_x1000 + 5) / 10);
}

uint8_t temp_a_byte(int temp_c) {
  if (temp_c < -40) temp_c = -40;
  if (temp_c > 87) temp_c = 87;
  return (uint8_t)(temp_c + 40);
}
