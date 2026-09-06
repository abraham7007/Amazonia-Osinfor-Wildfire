// estado_persistente.h — Lo que debe sobrevivir al deep-sleep.
//
// Vive en la memoria RTC lenta del ESP32-S3 (se conserva con el nucleo apagado).
#pragma once

#include <stdint.h>

#include "hal_lora.h"

struct EstadoPersistente {
  uint32_t marca;              // firma para detectar arranque en frio
  uint32_t capturas;           // total desde el ultimo arranque en frio
  uint32_t alertas;            // total de alertas transmitidas
  uint8_t alertas_esta_hora;
  uint8_t hora_ventana_alertas;
  uint8_t dia_ultimo_heartbeat;
  uint8_t dia_ultima_alerta_bateria;
  uint8_t capturas_con_deteccion;  // capturas consecutivas con al menos una tesela
  SesionLoRa sesion;
};

// Devuelve el estado; en arranque en frio lo inicializa a cero.
EstadoPersistente *estado();
bool arranque_en_frio();
