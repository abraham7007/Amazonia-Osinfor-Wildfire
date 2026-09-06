// Etapa 03 — Capturar a 1280x720.
//
// Aqui se prueba lo que mas difiere entre los dos nodos: el pinout y el sensor.
// Y se comprueba una cosa que no es negociable: que la camara entregue
// **1280x720**. No es un capricho de resolucion. El modelo se entreno y se
// evaluo sobre cuadros de ese tamano, que dan una rejilla de 32 teselas; con
// otra resolucion el nodo veria en campo algo distinto de lo que el modelo vio
// en entrenamiento, y las metricas medidas dejarian de aplicar.
//
// El prototipo anterior capturaba a QQVGA 160x120 —40 veces menos pixeles—, con
// lo que una columna de humo a distancia ocupa menos de un pixel. Es la razon
// principal de que aquel modelo se quedara en F1 0,52.

#include <Arduino.h>
#include <string.h>

#include "camara.h"
#include "cuadro.h"
#include "informe.h"
#include "placa.h"

bool etapa_principal() {
  bool bien = true;

  dato("pinout: XCLK=%d SDA=%d SCL=%d VSYNC=%d HREF=%d PCLK=%d", kPines.xclk, kPines.siod,
       kPines.sioc, kPines.vsync, kPines.href, kPines.pclk);
#if PLACA_PIN_ENCENDIDO_CAMARA >= 0
  dato("esta placa alimenta el sensor por GPIO %d; la libreria lo enciende sola",
       PLACA_PIN_ENCENDIDO_CAMARA);
#endif

  if (!camara_iniciar()) {
    fallo("la camara no arranco");
    fallo("si el error es 0x105 en el NE101, revisa el encendido del sensor");
    fallo("pinout de referencia: HARDWARE/README.md §3");
    return false;
  }
  ok("camara inicializada");

  dato("sensor detectado: %s   (esperado en esta placa: %s)", camara_sensor(),
       PLACA_SENSOR_ESP);
  if (strcmp(camara_sensor(), "desconocido") == 0) {
    fallo("sensor no reconocido: la calidad de imagen no esta caracterizada");
    bien = false;
  }

  uint32_t suma_us = 0;
  int validas = 0;
  const int n = 5;
  for (int i = 0; i < n; ++i) {
    Cuadro cuadro;
    const uint32_t t0 = micros();
    const bool capturo = camara_capturar(&cuadro);
    const uint32_t t1 = micros();
    if (!capturo) {
      fallo("captura %d fallida", i + 1);
      bien = false;
      continue;
    }
    if (cuadro.ancho != 1280 || cuadro.alto != 720) {
      fallo("el sensor entrego %dx%d en vez de 1280x720", cuadro.ancho, cuadro.alto);
      fallo("con otra geometria el modelo no ve lo que vio en entrenamiento");
      bien = false;
    }
    if (i == 0) {
      // Brillo medio: sirve para saber de un vistazo si la camara mira a algo o
      // esta tapada. Un cuadro casi negro suele ser la tapa puesta, no un fallo.
      uint64_t suma = 0;
      for (int y = 0; y < cuadro.alto; y += 8) {
        const uint8_t *fila = cuadro.fila(y);
        for (int x = 0; x < cuadro.ancho; x += 8) suma += fila[x * 3];
      }
      const int muestras = (cuadro.alto / 8) * (cuadro.ancho / 8);
      dato("primer cuadro: %dx%d RGB888, brillo medio del canal rojo %u/255", cuadro.ancho,
           cuadro.alto, (unsigned)(suma / muestras));
    }
    suma_us += t1 - t0;
    ++validas;
    camara_liberar(&cuadro);
  }

  if (validas == 0) {
    camara_apagar();
    return false;
  }

  const uint32_t media = suma_us / validas;
  ok("%d/%d capturas correctas, media %u ms (incluye JPEG -> RGB888)", validas, n,
     (unsigned)(media / 1000));
  dato("a 48 capturas diarias son %.1f s al dia solo de captura", media * 48.0 / 1e6);

  camara_apagar();
  ok("camara apagada");
  return bien;
}

void setup() { etapa("03", "Capturar a 1280x720", etapa_principal); }
void loop() { reposo(); }
