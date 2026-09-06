// cuadro.h — Representacion de un cuadro de camara, independiente del driver.
#pragma once

#include <stddef.h>
#include <stdint.h>

// Un cuadro RGB888 entrelazado. `paso_fila` permite que el buffer del driver
// tenga relleno al final de cada fila sin obligar a copiarlo.
struct Cuadro {
  const uint8_t *datos = nullptr;
  int ancho = 0;
  int alto = 0;
  size_t paso_fila = 0;  // bytes entre el inicio de dos filas consecutivas

  inline const uint8_t *fila(int y) const { return datos + (size_t)y * paso_fila; }
  inline bool valido() const {
    return datos != nullptr && ancho > 0 && alto > 0 && paso_fila >= (size_t)ancho * 3;
  }
};
