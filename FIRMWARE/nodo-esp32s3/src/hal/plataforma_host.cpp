// Implementaciones de host para tiempo, memoria, energia y estado persistente.
// Permiten ejecutar el firmware completo en el PC.
#if defined(HAL_SIMULADA)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "estado_persistente.h"
#include "hal_energia.h"
#include "hal_tiempo.h"
#include "memoria.h"

namespace {
uint32_t g_epoch_offset = 0;
EstadoPersistente g_estado;
bool g_frio = true;
constexpr uint32_t kMarca = 0xA3F10001u;
}

void *reservar_grande(size_t bytes) { return malloc(bytes); }
void liberar_grande(void *p) { free(p); }

uint32_t ms_ahora() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

void fijar_hora_utc(uint32_t epoch_s) { g_epoch_offset = epoch_s; }

HoraLocal hora_local() {
  // NODO_EPOCH permite fijar la hora desde la linea de ordenes y recorrer el dia
  // entero sin esperar: `NODO_EPOCH=$(date -j -f '%Y-%m-%d %H:%M' '2026-09-05 12:00' +%s) ./nodo`
  if (g_epoch_offset == 0) {
    const char *env = getenv("NODO_EPOCH");
    if (env != nullptr) g_epoch_offset = (uint32_t)strtoul(env, nullptr, 10);
  }
  time_t t = (time_t)(g_epoch_offset != 0 ? g_epoch_offset : time(nullptr));
  struct tm tm_local;
  localtime_r(&t, &tm_local);
  HoraLocal h;
  h.anio = (uint16_t)(tm_local.tm_year + 1900);
  h.mes = (uint8_t)(tm_local.tm_mon + 1);
  h.dia = (uint8_t)tm_local.tm_mday;
  h.hora = (uint8_t)tm_local.tm_hour;
  h.minuto = (uint8_t)tm_local.tm_min;
  h.valida = true;
  return h;
}

EstadoEnergia leer_energia() {
  EstadoEnergia e;
  e.soc_pct = 85;
  e.mv_bateria = 3900;
  e.temp_c = 31;
  e.humedad_pct = 86;
  e.valido = true;
  return e;
}

void dormir_profundo(uint32_t segundos) {
  printf("[energia] deep-sleep %u s (simulado: fin de ejecucion)\n", (unsigned)segundos);
  exit(0);
}

EstadoPersistente *estado() {
  if (g_estado.marca != kMarca) {
    memset(&g_estado, 0, sizeof(g_estado));
    g_estado.marca = kMarca;
    g_frio = true;
  }
  return &g_estado;
}

bool arranque_en_frio() { return g_frio; }

#endif  // HAL_SIMULADA
