// Etapa 09 — Estado de carga y testigo de consumo.
//
// Dos cosas distintas:
//
//   a) Que el nodo pueda leer su propio estado de carga. Sin eso no puede aplicar
//      la politica de ahorro de gestion-de-energia.md ni avisar antes de morir,
//      y un nodo que se apaga sin avisar parece un fallo del sistema de alerta.
//
//   b) Un testigo en GPIO que marca las fases activas, para alinear en el
//      analizador de potencia la corriente con la etapa que la consume. Sin el,
//      el promedio medido no se puede repartir entre camara, remuestreo,
//      inferencia y radio, y el presupuesto energetico se queda en estimacion.

#include <Arduino.h>

#include "placa.h"
#include "informe.h"

namespace {
// GPIO libre para el testigo. Ajustar a un pin accesible de la placa antes de
// la campana de medida.
constexpr int kPinTestigo = -1;
}  // namespace

bool etapa_principal() {
  bool bien = true;

#if PLACA_PIN_BATERIA >= 0
  analogReadResolution(12);
  uint32_t suma = 0;
  const int n = 16;
  for (int i = 0; i < n; ++i) {
    suma += analogReadMilliVolts(PLACA_PIN_BATERIA);
    delay(10);
  }
  const uint32_t mv_adc = suma / n;
  const uint32_t mv = (uint32_t)(mv_adc * PLACA_DIVISOR_BATERIA);
  dato("ADC en GPIO %d: %u mV -> bateria %u mV (divisor %.1f)", PLACA_PIN_BATERIA,
       (unsigned)mv_adc, (unsigned)mv, PLACA_DIVISOR_BATERIA);

  if (mv < 2500 || mv > 5500) {
    fallo("lectura fuera de rango: revisa el pin y el divisor en include/placa.h");
    bien = false;
  } else {
    ok("lectura de bateria plausible");
  }
#else
  // Se declara fallo, no aviso. Un nodo que no sabe cuanta bateria le queda no
  // puede aplicar la politica de ahorro ni avisar antes de morir, y se apagaria
  // en campo sin decir nada: para quien vigila, indistinguible de una averia.
  fallo("esta placa no tiene fijado el pin de medida de bateria");
  fallo("rellena PLACA_PIN_BATERIA y PLACA_DIVISOR_BATERIA en 00-comun/placa/src/placa.h");
  dato("el NE101 expone la medida por la cabecera de 16 pines; hay que cablearla");
  dato("mientras tanto el firmware reporta SoC nominal y no entra en modo ahorro");
  bien = false;
#endif

  dato("temperatura interna del SoC: %.1f C (no es temperatura ambiente)", temperatureRead());

  if (kPinTestigo >= 0) {
    pinMode(kPinTestigo, OUTPUT);
    dato("testigo en GPIO %d: 5 pulsos de 200 ms para localizarlo en el analizador",
         kPinTestigo);
    for (int i = 0; i < 5; ++i) {
      digitalWrite(kPinTestigo, HIGH);
      delay(200);
      digitalWrite(kPinTestigo, LOW);
      delay(200);
    }
    ok("testigo ejercitado");
  } else {
    dato("testigo desactivado: fija kPinTestigo antes de la campana de medida de EVT");
  }

  return bien;
}

void setup() { etapa("09", "Estado de carga y testigo de consumo", etapa_principal); }
void loop() { reposo(); }
