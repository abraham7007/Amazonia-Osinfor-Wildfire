// memoria.h — Reserva de buffers grandes.
//
// En el ESP32-S3 va a PSRAM cuando la hay (ps_malloc); en el host es malloc.
// Se aisla aqui para que el nucleo no dependa del SoC.
#pragma once

#include <stddef.h>

void *reservar_grande(size_t bytes);
void liberar_grande(void *p);
