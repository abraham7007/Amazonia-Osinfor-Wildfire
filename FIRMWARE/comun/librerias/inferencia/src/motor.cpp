// Backend real: TFLite-Micro sobre el arreglo INT8 de cnn_media.
#include "motor_inferencia.h"

#include "config.h"
#include "modelo_humo_int8.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

// Las ocho operaciones que usa modelo_int8.tflite. Se declaran una a una en vez
// de usar AllOpsResolver porque cada op no usada cuesta flash sin dar nada.
// Verificado con 04_cuantizar_tflite.py: CONV_2D, DEPTHWISE_CONV_2D, ADD, MUL,
// MAX_POOL_2D, MEAN, FULLY_CONNECTED, LOGISTIC.
constexpr int kNumOps = 8;

alignas(16) uint8_t g_arena[ARENA_TENSORES_BYTES];

tflite::MicroInterpreter *g_interprete = nullptr;
TfLiteTensor *g_entrada = nullptr;
TfLiteTensor *g_salida = nullptr;
bool g_listo = false;

}  // namespace

bool motor_iniciar() {
  if (g_listo) return true;

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

  static tflite::MicroInterpreter interprete(modelo, resolver, g_arena, sizeof(g_arena));
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

