// Etapa 06 — Una captura completa.
//
// Aqui se juntan por primera vez las etapas 03, 04 y 05: capturar, recorrer el
// cuadro, inferir sobre cada tesela y aplicar la regla de disparo. Es el ciclo
// que ejecutara el nodo cada 15 minutos, sin la parte de dormir ni la de
// transmitir.
//
// La regla de disparo esta medida, no elegida a ojo. Sobre el partner reservado
// sdis-77 —otra region, la situacion mas parecida a Paoyhan—:
//
//     K=1  umbral 0,97   recall 0,226   precision 0,894   FPR 0,098   <- la usada
//     K=2  umbral 0,62   recall 0,175   precision 0,878   FPR 0,089
//     K=3  umbral 0,32   recall 0,213   precision 0,895   FPR 0,091
//
// Para cnn_media exigir dos teselas EMPEORA el recall. (La tabla del README de
// MODELO-TINYML que recomienda K>=2 corresponde a mobilenetv2_035, no a este
// modelo.) Y la confirmacion temporal tampoco ayuda en ninguno de los dos: los
// falsos positivos son bancos de niebla, que persisten entre capturas igual que
// persistiria el humo.

#include <Arduino.h>

#include "camara.h"
#include "config.h"
#include "cuadro.h"
#include "deteccion.h"
#include "informe.h"

bool etapa_principal() {
  if (!deteccion_iniciar()) {
    fallo("no arrancaron los buffers o el motor de inferencia");
    fallo("pasa antes las etapas 02 y 05");
    return false;
  }
  ok("pipeline de deteccion listo");

  if (!camara_iniciar()) {
    fallo("la camara no arranco; pasa antes la etapa 03");
    return false;
  }

  Cuadro cuadro;
  const uint32_t t0 = micros();
  if (!camara_capturar(&cuadro)) {
    fallo("no se pudo capturar");
    camara_apagar();
    return false;
  }
  const uint32_t t_captura = micros() - t0;

  const uint32_t t1 = micros();
  const ResultadoDeteccion r = deteccion_procesar(cuadro);
  const uint32_t t_proceso = micros() - t1;
  camara_liberar(&cuadro);
  camara_apagar();

  if (r.error) {
    fallo("hubo errores durante el proceso de las teselas");
    return false;
  }

  dato("regla de disparo: K >= %u teselas con probabilidad >= %.3f",
       (unsigned)K_TESELAS_MINIMO, UMBRAL_PROB_X1000 / 1000.0);
  ok("%u teselas evaluadas, %u por encima del umbral", (unsigned)r.teselas_total,
     (unsigned)r.teselas_sobre);
  dato("probabilidad maxima %.3f en la tesela con esquina (%d,%d)", r.prob_maxima / 1000.0,
       (int)r.x_maxima, (int)r.y_maxima);

  if (r.alerta) {
    dato("");
    dato(">>> ALERTA DE HUMO <<<  confianza %u %%", (unsigned)((r.prob_maxima + 5) / 10));
    dato("");
    dato("si no hay humo delante de la camara, esto es un falso positivo:");
    dato("apuntalo con la escena, es justo el dato que falta para Paoyhan");
  } else {
    ok("sin alerta");
  }

  // El numero que bloquea ADR-001 y el dimensionado fotovoltaico.
  const double s_total = (t_captura + t_proceso) / 1e6;
  dato("");
  dato("captura        %6u ms", (unsigned)(t_captura / 1000));
  dato("remuestreo     %6u ms", (unsigned)r.ms_remuestreo);
  dato("inferencia     %6u ms", (unsigned)r.ms_inferencia);
  dato("--------------------------");
  dato("por captura    %6.2f s", s_total);
  dato("al dia (x48)   %6.1f s de CPU activa", s_total * 48.0);
  dato("");
  dato("lleva esta cifra a analisis-de-potencia.md: hoy no incluye coste de inferencia");

  return true;
}

void setup() { etapa("06", "Una captura completa: 32 teselas y regla", etapa_principal); }
void loop() { reposo(); }
