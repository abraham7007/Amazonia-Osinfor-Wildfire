// Backend simulado: permite compilar y probar todo el resto del firmware en el
// host, sin TFLM ni placa. Devuelve una probabilidad determinista derivada del
// contenido del parche, de modo que las pruebas del pipeline son reproducibles.
#if defined(MOTOR_SIMULADO)

#include "motor_inferencia.h"

#include <string.h>

#include "config.h"

namespace {
int8_t g_entrada[ENTRADA_PX * ENTRADA_PX * 3];
bool g_listo = false;
}  // namespace

bool motor_iniciar() {
  memset(g_entrada, 0, sizeof(g_entrada));
  g_listo = true;
  return true;
}

int8_t *motor_buffer_entrada() { return g_listo ? g_entrada : nullptr; }

int motor_inferir() {
  if (!g_listo) return -1;
  // Heuristica de juguete, no un modelo: media del canal rojo normalizada.
  int64_t suma = 0;
  for (size_t i = 0; i < sizeof(g_entrada); i += 3) suma += (int)g_entrada[i] + 128;
  const int n = ENTRADA_PX * ENTRADA_PX;
  const int media = (int)(suma / n);  // 0..255
  return media * 1000 / 255;
}

size_t motor_arena_usada() { return sizeof(g_entrada); }

#endif  // MOTOR_SIMULADO
