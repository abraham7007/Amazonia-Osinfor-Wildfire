// energia.h — Politica de cadencia y ahorro (cadencia-captura.md §4, gestion-de-energia.md).
//
// Funciones puras: reciben hora y estado de carga, devuelven que hacer. Asi la
// politica se prueba en el host sin placa ni reloj real.
#pragma once

#include <stdint.h>

#include "hal_tiempo.h"

enum class Accion : uint8_t {
  CAPTURAR,          // ventana diurna, bateria suficiente
  SOLO_TELEMETRIA,   // bateria critica: no se enciende la camara
  DORMIR_HASTA_ALBA, // fuera de la ventana diurna
};

Accion decidir_accion(const HoraLocal &h, uint8_t soc_pct);

// Segundos hasta el proximo despertar segun hora, riesgo y estado de carga.
uint32_t intervalo_siguiente(const HoraLocal &h, uint8_t soc_pct);

// Segundos hasta HORA_INICIO_DIURNA del dia siguiente (o de hoy si aun no llego).
uint32_t segundos_hasta_alba(const HoraLocal &h);

// true si la hora cae dentro de la ventana diurna de captura.
bool es_ventana_diurna(const HoraLocal &h);
