// camara.h — Captura de un cuadro RGB888 listo para teselar.
//
// Encapsula esp32-camera con el pinout de la placa activa (placa.h) y entrega
// siempre lo mismo: un Cuadro RGB888 de 1280x720, que es la geometria con la que
// se entreno el modelo. Quien la usa no sabe si detras hay una OV2640 del XIAO o
// una OV5640 del NE101.
#pragma once

#include "cuadro.h"

bool camara_iniciar();
bool camara_capturar(Cuadro *destino);
void camara_liberar(Cuadro *cuadro);
void camara_apagar();

// Nombre del sensor detectado ("OV5640", "OV2640"...), o "desconocido".
const char *camara_sensor();
