// Backend real: TFLite-Micro sobre el arreglo INT8 de cnn_media.
#include "motor_inferencia.h"

#include "config.h"
#include "modelo_humo_int8.h"
#include <esp_heap_caps.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

// Las ocho operaciones que usa modelo_int8.tflite. Se declaran una a una en vez
// de usar AllOpsResolver porque cada op no usada cuesta flash sin dar nada.
// Verificado con 04_cuantizar_tflite.py: CONV_2D, DEPTHWISE_CONV_2D, ADD, MUL,
// MAX_POOL_2D, MEAN, FULLY_CONNECTED, LOGISTIC.
constexpr int kNumOps = 8;

// La arena NO puede ser un array estatico. Con 320 KB reservados en DRAM, el
// enlazador falla antes de llegar a la placa:
//
//     region `dram0_0_seg' overflowed by 65656 bytes
//
// El ESP32-S3 tiene 512 KB de SRAM interna, pero el segmento de datos que
// comparten Arduino, la pila de Wi-Fi y el propio FreeRTOS deja bastante menos
// libre. Reservandola en ejecucion se toma lo que de verdad haya: primero SRAM
// interna, que es lo deseable porque TFLM la recorre en cada inferencia, y si no
// cabe, PSRAM. La etapa 02 comprueba cual de los dos casos se da en cada placa,
// y la 05 mide lo que cuesta la diferencia.
uint8_t *g_arena = nullptr;

bool g_arena_en_psram = false;
tflite::MicroInterpreter *g_interprete = nullptr;
TfLiteTensor *g_entrada = nullptr;
TfLiteTensor *g_salida = nullptr;
bool g_listo = false;

}  // namespace

bool motor_iniciar() {
  if (g_listo) return true;

  if (g_arena == nullptr) {
    g_arena = (uint8_t *)heap_caps_aligned_alloc(16, ARENA_TENSORES_BYTES,
                                                 MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (g_arena == nullptr) {
      g_arena = (uint8_t *)heap_caps_aligned_alloc(16, ARENA_TENSORES_BYTES,
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      g_arena_en_psram = true;
    }
    if (g_arena == nullptr) return false;
  }

  const tflite::Model *modelo = tflite::GetModel(modelo_humo_int8);
  if (modelo->version() != TFLITE_SCHEMA_VERSION) return false;

  static tflite::MicroMutableOpResolver<kNumOps> resolver;
  if (resolver.AddConv2D() != kTfLiteOk) return false;
  if (resolver.AddDepthwiseConv2D() != kTfLiteOk) return false;
  if (resolver.AddAdd() != kTfLiteOk) return false;
  if (resolver.AddMul() != kTfLiteOk) return false;
  if (resolver.AddMaxPool2D() != kTfLiteOk) return false;
  if (resolver.AddMean() != kTfLiteOk) return false;
  if (resolver.AddFullyConnected() != kTfLiteOk) return false;
  if (resolver.AddLogistic() != kTfLiteOk) return false;

  static tflite::MicroInterpreter interprete(modelo, resolver, g_arena, ARENA_TENSORES_BYTES);
  if (interprete.AllocateTensors() != kTfLiteOk) return false;

  g_interprete = &interprete;
  g_entrada = interprete.input(0);
  g_salida = interprete.output(0);

  // Si esto falla, el .cc no corresponde al modelo que espera el firmware.
  if (g_entrada->type != kTfLiteInt8 || g_salida->type != kTfLiteInt8) return false;
  if (g_entrada->dims->size != 4) return false;
  if (g_entrada->dims->data[1] != ENTRADA_PX || g_entrada->dims->data[2] != ENTRADA_PX ||
      g_entrada->dims->data[3] != 3) {
    return false;
  }

  g_listo = true;
  return true;
}

int8_t *motor_buffer_entrada() { return g_listo ? g_entrada->data.int8 : nullptr; }

int motor_inferir() {
  if (!g_listo) return -1;
  if (g_interprete->Invoke() != kTfLiteOk) return -1;
  // prob = (v + 128) / 256  ->  x1000 con redondeo, sin coma flotante.
  const int v = (int)g_salida->data.int8[0] + 128;  // 0..255
  return (v * 1000 + 128) / 256;
}

size_t motor_arena_usada() { return g_listo ? g_interprete->arena_used_bytes() : 0; }

bool motor_arena_en_psram() { return g_arena_en_psram; }

