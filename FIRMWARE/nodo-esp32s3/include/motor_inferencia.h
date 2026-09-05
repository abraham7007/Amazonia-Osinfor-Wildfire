// motor_inferencia.h — Frontera con TFLite-Micro.
//
// Toda la dependencia de TFLM vive detras de estas cuatro funciones. El resto del
// firmware trabaja con probabilidades en escala entera 0-1000 y se puede compilar
// y probar en el host contra el backend simulado (motor_inferencia_sim.cpp).
#pragma once

#include <stddef.h>
#include <stdint.h>

// Reserva la arena e instancia el interprete. Idempotente.
bool motor_iniciar();

// Puntero al tensor de entrada, para escribir el parche sin copia intermedia.
// El tensor es int8 de forma [1, ENTRADA_PX, ENTRADA_PX, 3].
int8_t *motor_buffer_entrada();

// Ejecuta una inferencia sobre lo que haya en el buffer de entrada.
// Devuelve la probabilidad de humo en escala 0-1000, o -1 si fallo.
//
// La salida del modelo es int8 con escala 1/256 y punto cero -128, de modo que
// prob = (salida + 128) / 256; se convierte a 0-1000 con aritmetica entera.
int motor_inferir();

// Bytes de arena efectivamente usados; util para dimensionar en EVT.
size_t motor_arena_usada();
