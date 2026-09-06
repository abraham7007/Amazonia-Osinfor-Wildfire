#include "deteccion.h"

#include "config.h"
#include "hal_tiempo.h"
#include "memoria.h"
#include "motor_inferencia.h"
#include "remuestreo.h"

namespace {

// Buffer intermedio de la pasada horizontal del remuestreo:
// VENTANA_PX filas x ENTRADA_PX columnas x 3 canales = 64 512 B para 224->96.
constexpr size_t kTemporalBytes = (size_t)VENTANA_PX * ENTRADA_PX * 3;

uint8_t *g_temporal = nullptr;
Tesela g_rejilla[MAX_TESELAS];
bool g_listo = false;

}  // namespace

bool deteccion_iniciar() {
  if (g_listo) return true;
  if (g_temporal == nullptr) {
    g_temporal = (uint8_t *)reservar_grande(kTemporalBytes);
    if (g_temporal == nullptr) return false;
  }
  if (!motor_iniciar()) return false;
  g_listo = true;
  return true;
}

ResultadoDeteccion deteccion_procesar_con(const Cuadro &cuadro, uint16_t umbral_x1000,
                                          uint8_t k_minimo) {
  ResultadoDeteccion r;
  if (!g_listo || !cuadro.valido()) {
    r.error = true;
    return r;
  }

  const int n = construir_rejilla(cuadro.ancho, cuadro.alto, VENTANA_PX, SOLAPE_NUM, SOLAPE_DEN,
                                  g_rejilla, MAX_TESELAS);
  if (n == 0) {
    r.error = true;
    return r;
  }
  r.teselas_total = (uint8_t)n;

  int8_t *entrada = motor_buffer_entrada();
  if (entrada == nullptr) {
    r.error = true;
    return r;
  }

  for (int i = 0; i < n; ++i) {
    const uint32_t t0 = ms_ahora();
    const bool ok = remuestrear_ventana(cuadro, g_rejilla[i].x, g_rejilla[i].y, VENTANA_PX,
                                        ENTRADA_PX, g_temporal, kTemporalBytes, entrada);
    const uint32_t t1 = ms_ahora();
    r.ms_remuestreo += t1 - t0;
    if (!ok) {
      r.error = true;
      continue;
    }

    const int prob = motor_inferir();
    r.ms_inferencia += ms_ahora() - t1;
    if (prob < 0) {
      r.error = true;
      continue;
    }

    if ((uint16_t)prob > r.prob_maxima) {
      r.prob_maxima = (uint16_t)prob;
      r.x_maxima = g_rejilla[i].x;
      r.y_maxima = g_rejilla[i].y;
    }
    if ((uint16_t)prob >= umbral_x1000) ++r.teselas_sobre;
  }

  r.alerta = r.teselas_sobre >= k_minimo && k_minimo > 0;
  return r;
}

ResultadoDeteccion deteccion_procesar(const Cuadro &cuadro) {
  return deteccion_procesar_con(cuadro, UMBRAL_PROB_X1000, K_TESELAS_MINIMO);
}
