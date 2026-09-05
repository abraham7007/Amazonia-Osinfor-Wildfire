// T09 — Perifericos propios de cada placa.
//
// Es la prueba que hace falta que sea DISTINTA por nodo: el XIAO y el NE101 no
// tienen los mismos perifericos, y dar por bueno en uno lo probado en el otro es
// justo el error que se paga en campo.

#include <Arduino.h>

#include "placa.h"
#include "pruebas.h"

#if defined(PLACA_XIAO_S3_SENSE)

bool t09_perifericos() {
  dato("perifericos del XIAO ESP32S3 Sense");

  if (PLACA_PIN_LED >= 0) {
    pinMode(PLACA_PIN_LED, OUTPUT);
    dato("LED de usuario en GPIO %d: 3 parpadeos", PLACA_PIN_LED);
    for (int i = 0; i < 3; ++i) {
      digitalWrite(PLACA_PIN_LED, PLACA_LED_ACTIVO_BAJO ? LOW : HIGH);
      delay(250);
      digitalWrite(PLACA_PIN_LED, PLACA_LED_ACTIVO_BAJO ? HIGH : LOW);
      delay(250);
    }
    ok("si viste tres parpadeos, el LED responde");
  }

  // El XIAO Sense trae microfono PDM en la placa de expansion. No lo usa el
  // modelo de humo, pero saber si esta presente importa: es la via para una
  // segunda modalidad (motosierra, disparo) si el proyecto crece.
  dato("microfono PDM: presente en la placa de expansion Sense, sin usar por este firmware");

  dato("carga de bateria: gestionada en placa (100 mA rapida / 0,9 mA goteo)");
  dato("recuerda que el XIAO no lleva carcasa: la IP67 es la caja impresa del piloto");
  return true;
}

#elif defined(PLACA_NE101)

namespace {
// Interfaz PIR de 4 vias del NE101. El pin concreto depende de como se cableo el
// conector; se ajusta aqui tras comprobarlo con el multimetro.
constexpr int kPinPir = -1;
}  // namespace

bool t09_perifericos() {
  bool bien = true;
  dato("perifericos del NeoEyes NE101");

  // El dominio de potencia del sensor: si esto no conmuta, el ahorro en
  // deep-sleep que promete el fabricante no se materializa.
  pinMode(PLACA_PIN_ENCENDIDO_CAMARA, OUTPUT);
  digitalWrite(PLACA_PIN_ENCENDIDO_CAMARA, LOW);
  delay(200);
  digitalWrite(PLACA_PIN_ENCENDIDO_CAMARA, HIGH);
  delay(200);
  ok("dominio de potencia de la camara (GPIO %d) conmutado", PLACA_PIN_ENCENDIDO_CAMARA);
  dato("con el analizador de potencia deberia verse una caida clara al ponerlo en BAJO");

#if PLACA_TIENE_PIR
  if (kPinPir >= 0) {
    pinMode(kPinPir, INPUT);
    dato("PIR en GPIO %d: pasa la mano por delante en los proximos 10 s...", kPinPir);
    bool detecto = false;
    const uint32_t t0 = millis();
    while (millis() - t0 < 10000) {
      if (digitalRead(kPinPir) == HIGH) { detecto = true; break; }
      delay(20);
    }
    if (detecto) {
      ok("el PIR disparo");
    } else {
      fallo("el PIR no disparo en 10 s: revisa el conector de 4 vias y kPinPir");
      bien = false;
    }
  } else {
    dato("PIR sin cablear (kPinPir = -1). El nodo de humo no lo necesita:");
    dato("el disparo es por temporizador, no por movimiento. Queda como reserva.");
  }
#endif

  dato("luz de relleno y fotosensor: presentes, sin uso en el nodo de humo");
  dato("(la captura es diurna por diseno, cadencia-captura.md §4)");
  dato("carcasa IP67 de fabrica con ventana de vidrio templado: no requiere caja adicional");
  return bien;
}

#endif
