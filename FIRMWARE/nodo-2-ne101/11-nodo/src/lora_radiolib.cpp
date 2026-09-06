// Driver LoRaWAN real sobre RadioLib.
//
// PENDIENTE DE ADR-001: falta elegir la radio. Las dos opciones sobre la mesa:
//
//   - Wio-SX1262 acoplado al XIAO ESP32-S3 (el XIAO Sense no trae radio propia,
//     pese a lo que da por hecho protocolo-lorawan.md §1 al decir "SX126x
//     integrada en la placa del nodo": conviene confirmarlo con el area usuaria
//     antes de cerrar el BOM).
//   - La radio del NE101, si su variante la incluye.
//
// Lo unico que cambia entre ambas son los pines del bloque de abajo y la clase
// de RadioLib; el resto del firmware no se entera, porque habla por hal_lora.h.
//
// Activar con -DUSAR_LORA_RADIOLIB y anadir a lib_deps:  jgromes/RadioLib
#if defined(USAR_LORA_RADIOLIB)

#include <RadioLib.h>

#include "hal_lora.h"

namespace {

// --- Pines de la radio (rellenar segun la placa) ---------------------------
constexpr int kPinNss = -1;
constexpr int kPinDio1 = -1;
constexpr int kPinReset = -1;
constexpr int kPinBusy = -1;

// --- Credenciales OTAA -----------------------------------------------------
// NO se dejan en el repositorio: se inyectan por -D desde un fichero local no
// versionado, o se leen de NVS en el comisionado.
#if !defined(LORA_JOIN_EUI) || !defined(LORA_DEV_EUI)
#error "Definir LORA_JOIN_EUI, LORA_DEV_EUI, LORA_APP_KEY y LORA_NWK_KEY fuera del repo."
#endif

SX1262 g_radio = new Module(kPinNss, kPinDio1, kPinReset, kPinBusy);
// US915 (Peru), subbanda 2, que es la que usan los gateways de 8 canales como el
// Milesight UG67 del proyecto.
LoRaWANNode g_nodo(&g_radio, &US915, 1);
bool g_unido = false;

}  // namespace

bool lora_iniciar() { return g_radio.begin() == RADIOLIB_ERR_NONE; }

bool lora_restaurar(const SesionLoRa &sesion) {
  if (!sesion.valida) return false;
  // RadioLib persiste sesion y nonces en su propio buffer; aqui se rehidrata
  // desde la memoria RTC para no repetir el join en cada despertar.
  // TODO: mapear SesionLoRa a LoRaWANNode::setBufferSession() al fijar la radio.
  return false;
}

bool lora_unir() {
  const int estado = g_nodo.activateOTAA();
  g_unido = (estado == RADIOLIB_LORAWAN_NEW_SESSION || estado == RADIOLIB_LORAWAN_SESSION_RESTORED);
  return g_unido;
}

bool lora_sesion_actual(SesionLoRa *destino) {
  if (destino == nullptr || !g_unido) return false;
  destino->valida = true;
  return true;
}

bool lora_enviar(uint8_t puerto, const uint8_t *datos, size_t n) {
  if (!g_unido) return false;
  return g_nodo.sendReceive((uint8_t *)datos, n, puerto) >= RADIOLIB_ERR_NONE;
}

void lora_dormir() { g_radio.sleep(); }

#endif  // USAR_LORA_RADIOLIB
