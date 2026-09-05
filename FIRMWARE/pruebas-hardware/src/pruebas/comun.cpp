#include <Arduino.h>
#include <stdarg.h>

#include "pruebas.h"

static void emitir(const char *marca, const char *fmt, va_list args) {
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, args);
  Serial.printf("  %s %s\n", marca, buf);
}

void ok(const char *fmt, ...) {
  va_list a; va_start(a, fmt); emitir("[ok ]", fmt, a); va_end(a);
}
void fallo(const char *fmt, ...) {
  va_list a; va_start(a, fmt); emitir("[!! ]", fmt, a); va_end(a);
}
void dato(const char *fmt, ...) {
  va_list a; va_start(a, fmt); emitir("     ", fmt, a); va_end(a);
}
