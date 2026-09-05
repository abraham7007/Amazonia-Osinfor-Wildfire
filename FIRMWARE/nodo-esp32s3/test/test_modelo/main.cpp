// Paridad del modelo entre el PC y el MCU: `pio test -e xiao_s3 -f test_modelo`.
//
// Es la prueba que atrapa el fallo mas caro y mas silencioso del porte a TinyML:
// el nodo arranca, infiere, transmite... y da otra cosa que el modelo evaluado en
// 05_evaluar.py, porque cambio el orden de canales, el desplazamiento de
// cuantizacion o la version del .cc. Sin esta prueba, la unica forma de notarlo
// es que el piloto no detecte incendios.
//
// Los vectores los genera FIRMWARE/herramientas/generar_vectores.py a partir de
// datos/clasificacion-96/X_val.npy y modelo_int8.tflite.

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "motor_inferencia.h"
#include "vectores_modelo.h"

namespace {

int g_fallos = 0;

void comprobar(bool ok, const char *nombre) {
  if (!ok) ++g_fallos;
  Serial.printf("  %s  %s\n", ok ? "ok   " : "FALLO", nombre);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n== paridad del modelo PC vs ESP32-S3 ==");

  comprobar(motor_iniciar(), "el motor arranca");
  Serial.printf("  arena usada: %u B de %u reservados\n", (unsigned)motor_arena_usada(),
                (unsigned)ARENA_TENSORES_BYTES);

  int8_t *entrada = motor_buffer_entrada();
  comprobar(entrada != nullptr, "hay buffer de entrada");
  if (entrada == nullptr) return;

  const int n_px = VECTOR_MODELO_LADO * VECTOR_MODELO_LADO * 3;
  for (int p = 0; p < VECTOR_MODELO_N; ++p) {
    // Los parches se guardan como uint8; la entrada del modelo es int8 con punto
    // cero -128, es decir el mismo byte desplazado. Este desplazamiento es
    // exactamente lo que esta prueba verifica que no se olvide.
    for (int i = 0; i < n_px; ++i) {
      entrada[i] = (int8_t)((int)kParchesModelo[p * n_px + i] - 128);
    }
    const int prob = motor_inferir();
    const int esperado_prob = (((int)kSalidaEsperada[p] + 128) * 1000 + 128) / 256;
    char nombre[80];
    snprintf(nombre, sizeof(nombre), "parche %d: prob %d/1000 (esperado %d)", p, prob,
             esperado_prob);
    comprobar(prob == esperado_prob, nombre);
  }

  Serial.printf("\n%s\n", g_fallos == 0 ? "PARIDAD OK" : "HAY DIVERGENCIAS");
}

void loop() { delay(1000); }
