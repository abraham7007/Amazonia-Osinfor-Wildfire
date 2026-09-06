// teselado.h — Rejilla de ventanas que recorre el cuadro.
//
// Replica exactamente `rejilla()` de MODELO-TINYML/scripts/06_evaluar_por_cuadro.py.
// Cualquier divergencia aqui cambia lo que ve el modelo respecto del entrenamiento.
#pragma once

#include <stdint.h>

// Cota del numero de posiciones por eje (1600 px / paso 168 -> 10).
#define MAX_EJE 24

struct Tesela {
  int16_t x;
  int16_t y;
};

// Escribe en `salida` las esquinas superior-izquierda de la rejilla que cubre un
// cuadro de `ancho` x `alto` con ventanas de `ventana` px y solape
// `solape_num/solape_den`. Devuelve cuantas teselas escribio (0 si no caben o si
// el cuadro es menor que la ventana).
int construir_rejilla(int ancho, int alto, int ventana, int solape_num, int solape_den,
                      Tesela *salida, int max_salida);
