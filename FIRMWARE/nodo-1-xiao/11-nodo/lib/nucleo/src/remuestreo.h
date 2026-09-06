// remuestreo.h — Reduccion de una ventana del cuadro a la entrada del modelo.
//
// Por que no vale un bilineal ingenuo
// -----------------------------------
// Los parches de entrenamiento se generaron con
//     img.resize((96, 96), Image.BILINEAR, box=(x, y, x+224, y+224))
// (02_construir_dataset.py y 06_evaluar_por_cuadro.py). Al reducir, Pillow NO
// interpola cuatro vecinos: escala el soporte del filtro triangular por el factor
// de reduccion (224/96 = 2,33), con lo que cada pixel de salida promedia 7 pixeles
// de entrada por eje. Un bilineal de 2x2 sobre una reduccion de 2,33x descarta el
// 80 % de los pixeles y produce aliasing: sobre columnas de humo tenues —que es
// justamente el caso que el nodo debe detectar— la entrada al modelo deja de
// parecerse a lo que vio en entrenamiento.
//
// Esta implementacion replica el algoritmo de Pillow (Resample.c) incluyendo su
// aritmetica de punto fijo, de modo que la salida es identica byte a byte. La
// prueba de paridad en test/ lo verifica contra vectores generados con Pillow.
#pragma once

#include <stdint.h>

#include "cuadro.h"

// Bits de la parte fraccionaria de los coeficientes (PRECISION_BITS de Pillow).
#define REMUESTREO_BITS_PRECISION 22

// Cota del lado de la entrada del modelo (96 hoy; 160 si se adopta esa variante).
#define ENTRADA_MAX 160

// Cota de taps por pixel de salida: ceil(soporte)*2 + 1 con soporte = 224/96.
#define REMUESTREO_MAX_TAPS 8

// Coeficientes de un eje, precalculados una sola vez para una geometria dada.
struct EjeRemuestreo {
  int tam_salida = 0;
  int taps = 0;
  int16_t inicio[ENTRADA_MAX];                       // primer pixel de entrada
  int16_t largo[ENTRADA_MAX];                        // cuantos taps son validos
  int32_t peso[ENTRADA_MAX * REMUESTREO_MAX_TAPS];   // punto fijo
};

// Precalcula los coeficientes para reducir un tramo de `tam_recorte` px (que
// empieza en `origen` dentro de una imagen de `tam_entrada` px) a `tam_salida` px.
bool preparar_eje(EjeRemuestreo *eje, int tam_entrada, int origen, int tam_recorte,
                  int tam_salida);

// Reduce la ventana (x, y, x+ventana, y+ventana) del cuadro a `salida_px` x
// `salida_px` y la escribe en `destino` como int8 con el desplazamiento de
// cuantizacion ya aplicado (escala 1,0, punto cero -128 => valor = pixel - 128),
// que es lo que espera la entrada de modelo_int8.tflite.
//
// `temporal` es un buffer de trabajo de al menos ventana * salida_px * 3 bytes.
bool remuestrear_ventana(const Cuadro &cuadro, int x, int y, int ventana, int salida_px,
                         uint8_t *temporal, size_t temporal_bytes, int8_t *destino);
