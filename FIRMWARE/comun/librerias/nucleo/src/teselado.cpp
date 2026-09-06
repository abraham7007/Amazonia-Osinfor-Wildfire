#include "teselado.h"

namespace {

// Equivalente de:
//   paso = int(VENTANA * (1 - SOLAPE))
//   ejes = list(range(0, max(1, tam - VENTANA + 1), paso))
//   if ejes[-1] != tam - VENTANA: ejes.append(tam - VENTANA)
int construir_eje(int tam, int ventana, int paso, int16_t *salida, int max_salida) {
  if (tam < ventana || paso <= 0) return 0;
  const int limite = tam - ventana + 1;  // range(0, limite, paso)
  int n = 0;
  for (int v = 0; v < limite; v += paso) {
    if (n >= max_salida) return 0;
    salida[n++] = (int16_t)v;
  }
  if (n == 0) return 0;
  if (salida[n - 1] != (int16_t)(tam - ventana)) {
    if (n >= max_salida) return 0;
    salida[n++] = (int16_t)(tam - ventana);
  }
  return n;
}

}  // namespace

int construir_rejilla(int ancho, int alto, int ventana, int solape_num, int solape_den,
                      Tesela *salida, int max_salida) {
  if (salida == nullptr || max_salida <= 0 || solape_den <= 0) return 0;

  // paso = int(ventana * (1 - solape)); con solape = num/den y ventana entero,
  // la division entera coincide con el truncado de Python para valores positivos.
  const int paso = (ventana * (solape_den - solape_num)) / solape_den;

  int16_t xs[MAX_EJE], ys[MAX_EJE];
  const int nx = construir_eje(ancho, ventana, paso, xs, MAX_EJE);
  const int ny = construir_eje(alto, ventana, paso, ys, MAX_EJE);
  if (nx == 0 || ny == 0) return 0;
  if (nx * ny > max_salida) return 0;

  // Orden [(x, y) for y in ys for x in xs] — el mismo del script de evaluacion.
  int n = 0;
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      salida[n].x = xs[i];
      salida[n].y = ys[j];
      ++n;
    }
  }
  return n;
}
