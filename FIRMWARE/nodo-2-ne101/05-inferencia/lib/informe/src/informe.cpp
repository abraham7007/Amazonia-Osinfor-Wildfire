#include "informe.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

#include "placa.h"

namespace {

bool (*g_fn)() = nullptr;
const char *g_codigo = nullptr;
bool g_resultado = false;

void emitir(const char *marca, const char *fmt, va_list args) {
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, args);
  Serial.printf("  %s %s\n", marca, buf);
}

void veredicto(uint32_t ms) {
  Serial.println("-----------------------------------------------------------------");
  Serial.printf("  %s: %s   (%u ms)\n", g_codigo, g_resultado ? "CORRECTO" : "FALLO",
                (unsigned)ms);
  Serial.println("=================================================================");
  Serial.println("  'r' + ENTER para repetir");
}

}  // namespace

void ok(const char *fmt, ...) {
  va_list a; va_start(a, fmt); emitir("[ok ]", fmt, a); va_end(a);
}
void fallo(const char *fmt, ...) {
  va_list a; va_start(a, fmt); emitir("[!! ]", fmt, a); va_end(a);
}
void dato(const char *fmt, ...) {
  va_list a; va_start(a, fmt); emitir("     ", fmt, a); va_end(a);
}

void etapa(const char *codigo, const char *titulo, bool (*fn)()) {
  Serial.begin(115200);
  // Margen para que el monitor serie se enganche: sin esto la cabecera se pierde
  // y la etapa parece no haber arrancado.
  delay(2500);

  g_fn = fn;
  g_codigo = codigo;

  Serial.println();
  Serial.println("=================================================================");
  Serial.printf("  %s · %s\n", codigo, titulo);
  Serial.printf("  %s  ·  nodo \"%s\"\n", PLACA_NOMBRE, NOMBRE_NODO);
  Serial.println("  Amazonia+ / Piloto OSINFOR — CCNN Paoyhan");
  Serial.println("=================================================================");

  const uint32_t t0 = millis();
  g_resultado = fn();
  veredicto(millis() - t0);
}

void reposo() {
  if (!Serial.available()) {
    delay(100);
    return;
  }
  String linea = Serial.readStringUntil('\n');
  linea.trim();
  if (linea == "r" && g_fn != nullptr) {
    Serial.printf("\n--- repitiendo %s ---\n", g_codigo);
    const uint32_t t0 = millis();
    g_resultado = g_fn();
    veredicto(millis() - t0);
  }
}
