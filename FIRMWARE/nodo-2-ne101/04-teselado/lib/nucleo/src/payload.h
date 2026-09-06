// payload.h — Tramas LoRaWAN del nodo (protocolo-lorawan.md §3).
//
// Codigo puro, sin dependencias del MCU: se compila y se prueba en el host.
#pragma once

#include <stddef.h>
#include <stdint.h>

#define PAYLOAD_ALERTA_BYTES     6
#define PAYLOAD_HEARTBEAT_BYTES 14

enum TipoMensaje : uint8_t {
  MSG_HEARTBEAT    = 0,
  MSG_HUMO         = 1,
  MSG_BATERIA_BAJA = 2,
  MSG_TEST         = 3,
};

// Bits de `flags` en la trama de alerta.
enum BanderaAlerta : uint8_t {
  FLAG_ALARMA_ACTIVA = 1 << 0,
  FLAG_TAMPER        = 1 << 1,
  FLAG_ERROR_CAMARA  = 1 << 2,
  FLAG_ERROR_MODELO  = 1 << 3,
  FLAG_MODO_AHORRO   = 1 << 4,
};

struct Alerta {
  uint8_t node_id;
  TipoMensaje tipo;
  uint8_t confianza_pct;  // 0-100
  uint8_t bateria_pct;    // 0-100
  int8_t temp_c;          // -40..87
  uint8_t flags;
};

struct Heartbeat {
  uint8_t node_id;
  int32_t lat_e7;
  int32_t lon_e7;
  uint8_t bateria_pct;
  int8_t temp_c;
  uint8_t humedad_pct;
  uint8_t fw_ver;
};

// Serializan a big-endian. Devuelven los bytes escritos, o 0 si el buffer no basta.
size_t serializar_alerta(const Alerta &a, uint8_t *buffer, size_t capacidad);
size_t serializar_heartbeat(const Heartbeat &h, uint8_t *buffer, size_t capacidad);

// Convierte una probabilidad en la escala entera interna (0-1000) al byte
// `confidence` 0-100 de la trama, con redondeo al entero mas proximo.
uint8_t confianza_a_byte(uint16_t prob_x1000);

// Codifica temperatura al byte con desplazamiento +40 de la trama, saturando.
uint8_t temp_a_byte(int temp_c);
