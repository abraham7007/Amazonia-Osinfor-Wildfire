// Etapa 04 — Recorrer el cuadro y preparar la entrada del modelo.
//
// Esta etapa no usa el modelo todavia. Comprueba lo que va JUSTO ANTES de el, que
// es donde se esconde el error mas caro de todo el porte a TinyML: si el parche
// que llega al modelo no es el mismo tipo de parche que vio en entrenamiento,
// el nodo funcionara, transmitira, y detectara mal, sin dar ningun sintoma.
//
// Dos cosas concretas:
//
// 1. La rejilla. El nodo no clasifica el cuadro completo: la columna de humo
//    mediana mide 41x32 px sobre 1280x720, y reducir el cuadro entero a 96x96 la
//    dejaria en 3x2 px. Se recorre con una ventana de 224 px que se reduce a
//    96x96: 32 teselas con 25 % de solape.
//
// 2. El remuestreo. Los parches de entrenamiento se generaron con
//    Image.resize((96,96), BILINEAR, box=...) de Pillow. Al REDUCIR, Pillow no
//    interpola cuatro vecinos: escala el soporte del filtro por el factor de
//    reduccion (224/96 = 2,33), de modo que cada pixel de salida promedia siete
//    de entrada por eje. Un bilineal ingenuo de 2x2 descarta el 80 % de los
//    pixeles. Aqui se mide cuanto cuesta hacerlo bien en la placa.

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "camara.h"
#include "config.h"
#include "cuadro.h"
#include "informe.h"
#include "memoria.h"
#include "remuestreo.h"
#include "teselado.h"

namespace {

Tesela g_rejilla[MAX_TESELAS];
int8_t g_parche[ENTRADA_PX * ENTRADA_PX * 3];

// Vuelca un parche como arte ASCII. Es tosco, y por eso mismo util: permite ver
// en el monitor serie que la tesela contiene la escena y no ruido ni una franja
// negra, sin montar ninguna herramienta de visualizacion.
void dibujar(const int8_t *parche) {
  static const char kEscala[] = " .:-=+*#%@";
  for (int y = 0; y < ENTRADA_PX; y += 4) {
    char linea[ENTRADA_PX / 2 + 4];
    int n = 0;
    for (int x = 0; x < ENTRADA_PX; x += 2) {
      const int i = (y * ENTRADA_PX + x) * 3;
      const int luz = ((int)parche[i] + (int)parche[i + 1] + (int)parche[i + 2]) / 3 + 128;
      linea[n++] = kEscala[(luz * 9) / 255];
    }
    linea[n] = '\0';
    Serial.printf("     %s\n", linea);
  }
}

}  // namespace

bool etapa_principal() {
  bool bien = true;

  uint8_t *temporal = (uint8_t *)reservar_grande((size_t)VENTANA_PX * ENTRADA_PX * 3);
  if (temporal == nullptr) {
    fallo("sin memoria para el buffer de remuestreo (%u KB)",
          (unsigned)((size_t)VENTANA_PX * ENTRADA_PX * 3 / 1024));
    return false;
  }

  if (!camara_iniciar()) {
    fallo("la camara no arranco; pasa antes la etapa 03");
    return false;
  }

  Cuadro cuadro;
  if (!camara_capturar(&cuadro)) {
    fallo("no se pudo capturar");
    camara_apagar();
    return false;
  }

  // --- 1. Rejilla ---------------------------------------------------------
  const int n = construir_rejilla(cuadro.ancho, cuadro.alto, VENTANA_PX, SOLAPE_NUM,
                                  SOLAPE_DEN, g_rejilla, MAX_TESELAS);
  dato("cuadro %dx%d, ventana %d px, solape %d/%d", cuadro.ancho, cuadro.alto, VENTANA_PX,
       SOLAPE_NUM, SOLAPE_DEN);
  if (n == 32) {
    ok("rejilla de %d teselas, la misma que la de la evaluacion del modelo", n);
  } else {
    fallo("rejilla de %d teselas; se esperaban 32 sobre 1280x720", n);
    bien = false;
  }
  dato("primera tesela en (%d,%d), ultima en (%d,%d)", g_rejilla[0].x, g_rejilla[0].y,
       g_rejilla[n - 1].x, g_rejilla[n - 1].y);
  if (n > 0 && (g_rejilla[n - 1].x + VENTANA_PX != cuadro.ancho ||
                g_rejilla[n - 1].y + VENTANA_PX != cuadro.alto)) {
    fallo("la ultima tesela no llega al borde: quedaria una franja sin vigilar");
    bien = false;
  } else {
    ok("la rejilla cubre el cuadro hasta los bordes");
  }

  // --- 2. Remuestreo ------------------------------------------------------
  uint32_t suma_us = 0;
  int hechas = 0;
  for (int i = 0; i < n; ++i) {
    const uint32_t t0 = micros();
    const bool r = remuestrear_ventana(cuadro, g_rejilla[i].x, g_rejilla[i].y, VENTANA_PX,
                                       ENTRADA_PX, temporal,
                                       (size_t)VENTANA_PX * ENTRADA_PX * 3, g_parche);
    suma_us += micros() - t0;
    if (!r) {
      fallo("fallo el remuestreo de la tesela %d", i);
      bien = false;
      break;
    }
    ++hechas;
  }

  if (hechas == n && n > 0) {
    const uint32_t media = suma_us / n;
    ok("%d teselas remuestreadas, %u us cada una", n, (unsigned)media);
    dato("por captura: %u ms solo de remuestreo", (unsigned)(suma_us / 1000));
    dato("a 48 capturas diarias: %.1f s al dia", suma_us * 48.0 / 1e6);
  }

  // --- 3. Comprobacion visual --------------------------------------------
  // Se vuelve a remuestrear la tesela central, que es la que suele mirar al
  // horizonte, y se dibuja.
  const int centro = n / 2;
  if (n > 0 && remuestrear_ventana(cuadro, g_rejilla[centro].x, g_rejilla[centro].y,
                                   VENTANA_PX, ENTRADA_PX, temporal,
                                   (size_t)VENTANA_PX * ENTRADA_PX * 3, g_parche)) {
    dato("tesela %d, esquina (%d,%d), tal como la vera el modelo:", centro,
         g_rejilla[centro].x, g_rejilla[centro].y);
    dibujar(g_parche);
    // La entrada del modelo es int8 con punto cero -128: el byte del pixel menos
    // 128. Si esto no se cumple, el modelo recibe basura aunque todo compile.
    int fuera = 0;
    for (size_t i = 0; i < sizeof(g_parche); ++i) {
      const int v = (int)g_parche[i] + 128;
      if (v < 0 || v > 255) ++fuera;
    }
    if (fuera == 0) {
      ok("el parche esta en el rango int8 esperado (pixel - 128)");
    } else {
      fallo("%d valores fuera de rango", fuera);
      bien = false;
    }
  }

  camara_liberar(&cuadro);
  camara_apagar();
  liberar_grande(temporal);
  return bien;
}

void setup() { etapa("04", "Recorrer el cuadro y preparar la entrada", etapa_principal); }
void loop() { reposo(); }
