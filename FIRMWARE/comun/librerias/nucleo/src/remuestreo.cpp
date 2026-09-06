#include "remuestreo.h"

#include <math.h>
#include <string.h>

namespace {

// Filtro triangular de Pillow (BILINEAR): soporte 1,0.
inline double filtro_triangular(double x) {
  if (x < 0.0) x = -x;
  return x < 1.0 ? 1.0 - x : 0.0;
}

// clip8 de Pillow: desplaza y satura a [0, 255].
inline uint8_t recortar8(int32_t v) {
  v >>= REMUESTREO_BITS_PRECISION;
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

}  // namespace

bool preparar_eje(EjeRemuestreo *eje, int tam_entrada, int origen, int tam_recorte,
                  int tam_salida) {
  if (eje == nullptr || tam_salida <= 0 || tam_salida > ENTRADA_MAX) return false;
  if (tam_recorte <= 0 || tam_entrada <= 0) return false;

  const double escala = (double)tam_recorte / (double)tam_salida;
  // Pillow no reduce el soporte al ampliar: filterscale = max(escala, 1).
  const double escala_filtro = escala < 1.0 ? 1.0 : escala;
  const double soporte = 1.0 * escala_filtro;  // soporte del triangular = 1,0
  const int taps = (int)ceil(soporte) * 2 + 1;
  if (taps > REMUESTREO_MAX_TAPS) return false;

  eje->tam_salida = tam_salida;
  eje->taps = taps;

  const double inv_escala_filtro = 1.0 / escala_filtro;
  for (int i = 0; i < tam_salida; ++i) {
    const double centro = (double)origen + (i + 0.5) * escala;
    int minimo = (int)(centro - soporte + 0.5);
    if (minimo < 0) minimo = 0;
    int maximo = (int)(centro + soporte + 0.5);
    if (maximo > tam_entrada) maximo = tam_entrada;
    const int largo = maximo - minimo;
    if (largo <= 0 || largo > taps) return false;

    double k[REMUESTREO_MAX_TAPS];
    double suma = 0.0;
    for (int t = 0; t < largo; ++t) {
      const double w = filtro_triangular(((double)(t + minimo) - centro + 0.5) * inv_escala_filtro);
      k[t] = w;
      suma += w;
    }

    eje->inicio[i] = (int16_t)minimo;
    eje->largo[i] = (int16_t)largo;
    int32_t *destino = &eje->peso[(size_t)i * REMUESTREO_MAX_TAPS];
    for (int t = 0; t < taps; ++t) destino[t] = 0;
    for (int t = 0; t < largo; ++t) {
      const double w = (suma != 0.0) ? k[t] / suma : 0.0;
      // normalize_coeffs_8bpc de Pillow: redondeo al entero mas proximo.
      destino[t] = (int32_t)(w < 0 ? -0.5 + w * (1 << REMUESTREO_BITS_PRECISION)
                                   : 0.5 + w * (1 << REMUESTREO_BITS_PRECISION));
    }
  }
  return true;
}

bool remuestrear_ventana(const Cuadro &cuadro, int x, int y, int ventana, int salida_px,
                         uint8_t *temporal, size_t temporal_bytes, int8_t *destino) {
  if (!cuadro.valido() || temporal == nullptr || destino == nullptr) return false;
  if (x < 0 || y < 0 || x + ventana > cuadro.ancho || y + ventana > cuadro.alto) return false;
  if ((size_t)ventana * (size_t)salida_px * 3u > temporal_bytes) return false;

  // Los coeficientes solo dependen del desplazamiento del recorte respecto del
  // borde, y con recortes alineados a la rejilla ese desplazamiento es entero:
  // basta prepararlos una vez por eje y trasladar `inicio`.
  static EjeRemuestreo eje_h;
  static EjeRemuestreo eje_v;
  static int cache_ventana = -1;
  static int cache_salida = -1;
  if (cache_ventana != ventana || cache_salida != salida_px) {
    if (!preparar_eje(&eje_h, ventana, 0, ventana, salida_px)) return false;
    if (!preparar_eje(&eje_v, ventana, 0, ventana, salida_px)) return false;
    cache_ventana = ventana;
    cache_salida = salida_px;
  }

  const int32_t redondeo = 1 << (REMUESTREO_BITS_PRECISION - 1);

  // Pasada horizontal: ventana filas x salida_px columnas, en 8 bits, igual que
  // Pillow (que tambien satura a uint8 entre las dos pasadas).
  for (int fila = 0; fila < ventana; ++fila) {
    const uint8_t *entrada = cuadro.fila(y + fila) + (size_t)x * 3;
    uint8_t *salida = temporal + (size_t)fila * salida_px * 3;
    for (int col = 0; col < salida_px; ++col) {
      const int inicio = eje_h.inicio[col];
      const int largo = eje_h.largo[col];
      const int32_t *peso = &eje_h.peso[(size_t)col * REMUESTREO_MAX_TAPS];
      int32_t r = redondeo, g = redondeo, b = redondeo;
      for (int t = 0; t < largo; ++t) {
        const uint8_t *px = entrada + (size_t)(inicio + t) * 3;
        r += peso[t] * px[0];
        g += peso[t] * px[1];
        b += peso[t] * px[2];
      }
      salida[col * 3 + 0] = recortar8(r);
      salida[col * 3 + 1] = recortar8(g);
      salida[col * 3 + 2] = recortar8(b);
    }
  }

  // Pasada vertical, escribiendo ya en int8 (valor = pixel - 128).
  for (int fila = 0; fila < salida_px; ++fila) {
    const int inicio = eje_v.inicio[fila];
    const int largo = eje_v.largo[fila];
    const int32_t *peso = &eje_v.peso[(size_t)fila * REMUESTREO_MAX_TAPS];
    int8_t *salida = destino + (size_t)fila * salida_px * 3;
    for (int col = 0; col < salida_px; ++col) {
      int32_t r = redondeo, g = redondeo, b = redondeo;
      for (int t = 0; t < largo; ++t) {
        const uint8_t *px = temporal + (size_t)(inicio + t) * salida_px * 3 + (size_t)col * 3;
        r += peso[t] * px[0];
        g += peso[t] * px[1];
        b += peso[t] * px[2];
      }
      salida[col * 3 + 0] = (int8_t)((int)recortar8(r) - 128);
      salida[col * 3 + 1] = (int8_t)((int)recortar8(g) - 128);
      salida[col * 3 + 2] = (int8_t)((int)recortar8(b) - 128);
    }
  }
  return true;
}
