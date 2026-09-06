// deteccion.h — Pipeline de una captura: teselado -> remuestreo -> inferencia -> regla.
#pragma once

#include <stdint.h>

#include "cuadro.h"
#include "teselado.h"

struct ResultadoDeteccion {
  bool alerta = false;         // se cumplio la regla de disparo
  uint8_t teselas_sobre = 0;   // cuantas teselas superaron el umbral
  uint8_t teselas_total = 0;   // cuantas se evaluaron
  uint16_t prob_maxima = 0;    // escala 0-1000, la mayor de todas las teselas
  int16_t x_maxima = -1;       // esquina de la tesela con prob_maxima
  int16_t y_maxima = -1;
  uint32_t ms_remuestreo = 0;  // instrumentacion para EVT
  uint32_t ms_inferencia = 0;
  bool error = false;          // fallo el motor o el remuestreo en alguna tesela
};

// Reserva los buffers de trabajo y arranca el motor. Llamar una vez tras el arranque.
bool deteccion_iniciar();

// Recorre el cuadro completo y aplica la regla de disparo de config.h.
ResultadoDeteccion deteccion_procesar(const Cuadro &cuadro);

// Variante con umbral y K explicitos: la usa el banco de EVT para barrer el punto
// de operacion sin recompilar.
ResultadoDeteccion deteccion_procesar_con(const Cuadro &cuadro, uint16_t umbral_x1000,
                                          uint8_t k_minimo);
