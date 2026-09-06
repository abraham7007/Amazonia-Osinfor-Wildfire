// informe.h — Salida por serie comun a todas las etapas de puesta en marcha.
//
// Cada etapa es un firmware independiente que se graba, dice lo que encontro y
// da un veredicto. Este modulo es lo unico que comparten en cuanto a formato,
// para que los resultados de las once etapas se lean igual y se puedan pegar
// tal cual en el cuaderno de campo.
#pragma once

#include <stdbool.h>

// Lineas de resultado.
void ok(const char *fmt, ...);
void fallo(const char *fmt, ...);
void dato(const char *fmt, ...);

// Cabecera, ejecucion y veredicto de la etapa. Se llama desde setup().
void etapa(const char *codigo, const char *titulo, bool (*fn)());

// Bucle de reposo: mantiene el veredicto en pantalla y repite con "r" + ENTER,
// sin volver a grabar. Se llama desde loop().
void reposo();
