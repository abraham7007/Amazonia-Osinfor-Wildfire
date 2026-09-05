// pruebas.h — Registro de pruebas de hardware.
#pragma once

#include <stdbool.h>

struct Prueba {
  const char *codigo;
  const char *titulo;
  bool (*ejecutar)();
};

// Devuelve el catalogo y su tamano. Las pruebas específicas de una placa se
// registran solo cuando esa placa esta compilada.
const Prueba *catalogo_pruebas(int *n);

// Utilidades compartidas de informe.
void ok(const char *fmt, ...);
void fallo(const char *fmt, ...);
void dato(const char *fmt, ...);
