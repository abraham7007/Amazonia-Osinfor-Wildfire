// hal_lora.h — Frontera con la pila LoRaWAN clase A (protocolo-lorawan.md §1).
//
// Banda US915, activacion OTAA, ADR activo. La radio concreta (SX126x integrada,
// Wio-SX1262 acoplado al XIAO, o la del NE101) se decide en ADR-001; hasta
// entonces el backend simulado imprime la trama por serie, que es exactamente lo
// que hara falta para depurar el decoder del servidor.
#pragma once

#include <stddef.h>
#include <stdint.h>

// Estado de sesion que sobrevive al deep-sleep para no repetir el join OTAA en
// cada despertar (un join cuesta mas energia que varias decenas de uplinks).
struct SesionLoRa {
  uint32_t devaddr;
  uint8_t nwk_skey[16];
  uint8_t app_skey[16];
  uint32_t contador_subida;
  bool valida;
};

bool lora_iniciar();
// Restaura una sesion previa; devuelve false si no era valida y hay que unirse.
bool lora_restaurar(const SesionLoRa &sesion);
bool lora_unir();
bool lora_sesion_actual(SesionLoRa *destino);
// Uplink no confirmado en el puerto indicado.
bool lora_enviar(uint8_t puerto, const uint8_t *datos, size_t n);
void lora_dormir();
