// Backend de camara simulado: genera un cuadro sintetico de 1280x720 con un
// gradiente de cielo y, opcionalmente, una columna clara, para ejercitar el
// pipeline completo sin sensor.
#if defined(HAL_SIMULADA)

#include <stdlib.h>
#include <string.h>

#include "hal_camara.h"
#include "memoria.h"

namespace {
constexpr int kAncho = 1280;
constexpr int kAlto = 720;
uint8_t *g_buffer = nullptr;
}

bool camara_iniciar() {
  if (g_buffer != nullptr) return true;
  g_buffer = (uint8_t *)reservar_grande((size_t)kAncho * kAlto * 3);
  if (g_buffer == nullptr) return false;
  for (int y = 0; y < kAlto; ++y) {
    for (int x = 0; x < kAncho; ++x) {
      uint8_t *p = g_buffer + ((size_t)y * kAncho + x) * 3;
      const uint8_t cielo = (uint8_t)(120 + y * 60 / kAlto);
      p[0] = cielo;
      p[1] = (uint8_t)(cielo + 10);
      p[2] = (uint8_t)(cielo + 30);
    }
  }
  return true;
}

bool camara_capturar(Cuadro *destino) {
  if (destino == nullptr || g_buffer == nullptr) return false;
  destino->datos = g_buffer;
  destino->ancho = kAncho;
  destino->alto = kAlto;
  destino->paso_fila = (size_t)kAncho * 3;
  return true;
}

void camara_liberar(Cuadro *cuadro) {
  if (cuadro != nullptr) cuadro->datos = nullptr;
}

void camara_apagar() {}

#endif  // HAL_SIMULADA
