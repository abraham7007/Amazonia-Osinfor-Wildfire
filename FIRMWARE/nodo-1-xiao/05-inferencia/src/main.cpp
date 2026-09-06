// Etapa 05 — El modelo corriendo en la placa.
//
// Es la prueba que decide si el nodo es viable. Responde dos cosas que solo se
// pueden medir aqui:
//
//   1. Si el INT8 del MCU da el MISMO entero que el del PC. Si no coincide, todo
//      lo medido en 05_evaluar.py deja de aplicar y el punto de operacion
//      (umbral 0,97) no significa nada.
//   2. Cuanto cuesta una inferencia. Con 32 teselas por captura y 48 capturas
//      diarias son ~1500 inferencias/dia, un coste que analisis-de-potencia.md
//      todavia no incluye y que bloquea ADR-001.

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "modelo_humo_int8.h"
#include "placa.h"
#include "informe.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

constexpr int kEntrada = 96;
constexpr size_t kArenaBytes = 320 * 1024;
constexpr int kRepeticiones = 20;

uint8_t *g_arena = nullptr;

}  // namespace

bool etapa_principal() {
  bool bien = true;

  if (g_arena == nullptr) {
    g_arena = (uint8_t *)heap_caps_aligned_alloc(16, kArenaBytes,
                                                 MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (g_arena == nullptr) {
      dato("arena no cabe en SRAM interna; se usa PSRAM (mas lenta)");
      g_arena = (uint8_t *)heap_caps_aligned_alloc(16, kArenaBytes, MALLOC_CAP_SPIRAM);
    }
  }
  if (g_arena == nullptr) {
    fallo("no se pudo reservar la arena de %u KB", (unsigned)(kArenaBytes / 1024));
    return false;
  }

  const tflite::Model *modelo = tflite::GetModel(modelo_humo_int8);
  if (modelo->version() != TFLITE_SCHEMA_VERSION) {
    fallo("el arreglo del modelo es de otra version de esquema TFLite");
    return false;
  }
  dato("modelo cnn_media INT8: %u KB en flash", (unsigned)(modelo_humo_int8_len / 1024));

  static tflite::MicroMutableOpResolver<8> resolver;
  resolver.AddConv2D();
  resolver.AddDepthwiseConv2D();
  resolver.AddAdd();
  resolver.AddMul();
  resolver.AddMaxPool2D();
  resolver.AddMean();
  resolver.AddFullyConnected();
  resolver.AddLogistic();

  static tflite::MicroInterpreter interprete(modelo, resolver, g_arena, kArenaBytes);
  if (interprete.AllocateTensors() != kTfLiteOk) {
    fallo("AllocateTensors fallo: la arena de %u KB no basta", (unsigned)(kArenaBytes / 1024));
    return false;
  }
  ok("interprete listo, arena usada %u KB de %u reservados",
     (unsigned)(interprete.arena_used_bytes() / 1024), (unsigned)(kArenaBytes / 1024));

  TfLiteTensor *ent = interprete.input(0);
  TfLiteTensor *sal = interprete.output(0);

  if (ent->type != kTfLiteInt8 || sal->type != kTfLiteInt8) {
    fallo("el modelo no es INT8 de extremo a extremo");
    bien = false;
  }
  if (ent->dims->data[1] != kEntrada || ent->dims->data[2] != kEntrada ||
      ent->dims->data[3] != 3) {
    fallo("la entrada no es 96x96x3: el .cc no corresponde al modelo esperado");
    return false;
  }
  dato("entrada: escala %.4f punto cero %d  |  salida: escala %.6f punto cero %d",
       ent->params.scale, (int)ent->params.zero_point, sal->params.scale,
       (int)sal->params.zero_point);
  if (ent->params.scale != 1.0f || ent->params.zero_point != -128) {
    fallo("la cuantizacion de entrada cambio: el preprocesado del firmware (pixel-128) ya no vale");
    bien = false;
  } else {
    ok("cuantizacion de entrada como se espera (pixel - 128, sin coma flotante)");
  }

  // Barrido de latencia con contenido fijo: interesa el coste, no el resultado.
  for (int i = 0; i < kEntrada * kEntrada * 3; ++i) ent->data.int8[i] = (int8_t)(i & 0x7F);
  interprete.Invoke();  // descartar la primera, que paga fallos de cache

  const uint32_t t0 = micros();
  for (int i = 0; i < kRepeticiones; ++i) interprete.Invoke();
  const uint32_t t1 = micros();

  const uint32_t us = (t1 - t0) / kRepeticiones;
  ok("inferencia por tesela: %u us (%.1f/s)", (unsigned)us, 1e6 / (double)us);

  // Lo que de verdad importa: el coste de una captura completa y del dia.
  const double s_captura = us * 32.0 / 1e6;
  dato("32 teselas por captura -> %.2f s de inferencia por captura", s_captura);
  dato("48 capturas/dia -> %.1f s de CPU al dia, ~%u inferencias", s_captura * 48.0,
       (unsigned)(32 * 48));
  dato("lleva este numero a analisis-de-potencia.md; hoy no incluye coste de inferencia");

  if (s_captura > 30.0) {
    fallo("mas de 30 s por captura: revisa que ESP-NN este activo y la arena en SRAM interna");
    bien = false;
  }

  return bien;
}

void setup() { etapa("05", "El modelo corriendo en la placa", etapa_principal); }
void loop() { reposo(); }
