// Backend LoRa de traza: imprime la trama en hexadecimal por la salida de
// diagnostico en vez de radiarla.
//
// Es el backend ACTIVO POR DEFECTO, tambien en la placa, mientras ADR-001 no
// fije que radio lleva el nodo. Permite validar de extremo a extremo el
// pipeline y el decoder del servidor —basta copiar el hexadecimal impreso al
// payload formatter— sin esperar al hardware. Se sustituye definiendo
// USAR_LORA_RADIOLIB.
#if !defined(USAR_LORA_RADIOLIB)
#include <stdio.h>
#include <string.h>

#include "hal_lora.h"

#if defined(ARDUINO)
#include <Arduino.h>
#define TRAZA(...) Serial.printf(__VA_ARGS__)
#else
#define TRAZA(...) printf(__VA_ARGS__)
#endif

namespace {
SesionLoRa g_sesion;
}

bool lora_iniciar() {
  memset(&g_sesion, 0, sizeof(g_sesion));
  return true;
}

bool lora_restaurar(const SesionLoRa &sesion) {
  if (!sesion.valida) return false;
  g_sesion = sesion;
  return true;
}

bool lora_unir() {
  g_sesion.devaddr = 0x26011F00u;
  g_sesion.contador_subida = 0;
  g_sesion.valida = true;
  TRAZA("[lora] join OTAA de traza, devaddr=%08X\n", (unsigned)g_sesion.devaddr);
  return true;
}

bool lora_sesion_actual(SesionLoRa *destino) {
  if (destino == nullptr || !g_sesion.valida) return false;
  *destino = g_sesion;
  return true;
}

bool lora_enviar(uint8_t puerto, const uint8_t *datos, size_t n) {
  if (!g_sesion.valida || datos == nullptr) return false;
  TRAZA("[lora] uplink fport=%u fcnt=%u payload=", (unsigned)puerto,
        (unsigned)g_sesion.contador_subida);
  for (size_t i = 0; i < n; ++i) TRAZA("%02X", datos[i]);
  TRAZA("\n");
  ++g_sesion.contador_subida;
  return true;
}

void lora_dormir() {}

#endif  // !USAR_LORA_RADIOLIB
