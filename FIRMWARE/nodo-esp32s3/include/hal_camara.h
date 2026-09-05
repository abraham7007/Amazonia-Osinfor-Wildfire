// hal_camara.h — Frontera con el driver de camara.
//
// El nucleo solo necesita un cuadro RGB888. Que lo entregue una OV2640 del XIAO
// ESP32-S3 Sense, una OV5640 del NE101 o un fichero de prueba es indiferente.
#pragma once

#include "cuadro.h"

bool camara_iniciar();
// Captura un cuadro. El buffer pertenece al driver hasta camara_liberar().
bool camara_capturar(Cuadro *destino);
void camara_liberar(Cuadro *cuadro);
// Corta la alimentacion del sensor antes de dormir.
void camara_apagar();
