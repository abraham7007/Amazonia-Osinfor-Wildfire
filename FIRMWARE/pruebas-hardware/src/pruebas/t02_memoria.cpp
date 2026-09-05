// T02 — Presupuesto de memoria del nodo.
//
// Comprueba que caben a la vez las tres reservas grandes que el firmware del nodo
// necesita en todo momento. Si una no cabe, el firmware fallaria en campo tras
// horas de funcionamiento, no en el arranque: por eso se verifica aqui, a
// proposito, reservandolas todas juntas.

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "placa.h"
#include "pruebas.h"

// Los tres consumidores, con los tamanos reales del firmware del nodo.
static const size_t kCuadroRGB = (size_t)1280 * 720 * 3;      // 2,64 MB — cuadro HD en RGB888
static const size_t kTemporal = (size_t)224 * 96 * 3;         //  64 KB — pasada horizontal del remuestreo
static const size_t kArena = (size_t)320 * 1024;              // 320 KB — arena de TFLite-Micro

bool t02_memoria() {
  bool bien = true;

  void *cuadro = heap_caps_malloc(kCuadroRGB, MALLOC_CAP_SPIRAM);
  if (cuadro == nullptr) {
    fallo("no se pudo reservar el cuadro HD RGB888 (%u KB) en PSRAM",
          (unsigned)(kCuadroRGB / 1024));
    bien = false;
  } else {
    ok("cuadro HD RGB888 reservado en PSRAM (%u KB)", (unsigned)(kCuadroRGB / 1024));
  }

  void *temporal = heap_caps_malloc(kTemporal, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (temporal == nullptr) {
    fallo("no se pudo reservar el buffer de remuestreo (%u KB)", (unsigned)(kTemporal / 1024));
    bien = false;
  } else {
    ok("buffer de remuestreo reservado (%u KB)", (unsigned)(kTemporal / 1024));
  }

  // La arena se prefiere en SRAM interna: TFLM la recorre en cada inferencia y
  // la PSRAM es sensiblemente mas lenta. Con ~1500 inferencias diarias, la
  // diferencia se nota en el presupuesto energetico.
  void *arena = heap_caps_malloc(kArena, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (arena == nullptr) {
    dato("la arena de %u KB no cabe en SRAM interna; se probara en PSRAM",
         (unsigned)(kArena / 1024));
    arena = heap_caps_malloc(kArena, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (arena == nullptr) {
      fallo("la arena no cabe en ningun sitio: el modelo no podra cargarse");
      bien = false;
    } else {
      dato("arena en PSRAM: funciona, pero la inferencia sera mas lenta");
      dato("mide T05 con y sin este caso antes de cerrar el presupuesto de energia");
    }
  } else {
    ok("arena de %u KB en SRAM interna (lo deseable)", (unsigned)(kArena / 1024));
  }

  dato("con todo reservado: PSRAM libre %u KB, interna libre %u KB",
       (unsigned)(ESP.getFreePsram() / 1024),
       (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));

  heap_caps_free(cuadro);
  heap_caps_free(temporal);
  heap_caps_free(arena);
  return bien;
}
