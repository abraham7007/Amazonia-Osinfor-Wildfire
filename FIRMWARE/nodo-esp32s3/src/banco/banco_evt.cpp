// banco_evt.cpp — Banco de medida para EVT.
//
// Responde las tres preguntas que hoy bloquean ADR-001 y el dimensionado
// fotovoltaico, y que el entrenamiento no puede contestar porque son del MCU:
//
//   1. Cuanto tarda una inferencia sobre una tesela de 96x96 en el ESP32-S3.
//   2. Cuanto tarda una captura completa (32 teselas: remuestreo + inferencia).
//   3. Cuanta arena de TFLM se usa de verdad.
//
// Con (2) y las 48 capturas diarias de cadencia-captura.md §5 sale el tiempo
// activo diario, que es lo que le falta a analisis-de-potencia.md para cerrar el
// presupuesto: hoy supone un coste de inferencia que nadie ha medido.
//
// El pin kPinTestigo se pone alto mientras dura la fase activa, para poder
// alinear en el osciloscopio o el analizador de potencia la corriente con la
// etapa que la consume. Sin ese testigo, el promedio medido no se puede repartir
// entre camara, remuestreo, inferencia y radio.
//
// Compilar con: pio run -e banco_xiao -t upload -t monitor

#if defined(BANCO_EVT)

#include <Arduino.h>

#include "config.h"
#include "cuadro.h"
#include "deteccion.h"
#include "hal_camara.h"
#include "memoria.h"
#include "motor_inferencia.h"
#include "remuestreo.h"
#include "teselado.h"

namespace {

constexpr int kPinTestigo = -1;  // GPIO libre de la placa; -1 lo desactiva
constexpr int kRepeticiones = 20;

void testigo(bool alto) {
  if (kPinTestigo >= 0) digitalWrite(kPinTestigo, alto ? HIGH : LOW);
}

void medir_inferencia_sola() {
  int8_t *entrada = motor_buffer_entrada();
  if (entrada == nullptr) {
    Serial.println("banco: motor no iniciado");
    return;
  }
  // Contenido fijo: interesa el coste, no el resultado.
  for (int i = 0; i < ENTRADA_PX * ENTRADA_PX * 3; ++i) entrada[i] = (int8_t)(i & 0x7F);

  motor_inferir();  // descartar la primera, que paga fallos de cache

  testigo(true);
  const uint32_t t0 = micros();
  for (int i = 0; i < kRepeticiones; ++i) motor_inferir();
  const uint32_t t1 = micros();
  testigo(false);

  const uint32_t us = (t1 - t0) / kRepeticiones;
  Serial.printf("inferencia_por_tesela_us=%u\n", (unsigned)us);
  Serial.printf("inferencias_por_segundo=%.1f\n", 1e6 / (double)us);
}

void medir_remuestreo_solo() {
  const size_t bytes_fuente = (size_t)VENTANA_PX * VENTANA_PX * 3;
  uint8_t *fuente = (uint8_t *)reservar_grande(bytes_fuente);
  uint8_t *temporal = (uint8_t *)reservar_grande((size_t)VENTANA_PX * ENTRADA_PX * 3);
  if (fuente == nullptr || temporal == nullptr) {
    Serial.println("banco: sin memoria para el remuestreo");
    return;
  }
  for (size_t i = 0; i < bytes_fuente; ++i) fuente[i] = (uint8_t)(i * 7);

  Cuadro c;
  c.datos = fuente;
  c.ancho = VENTANA_PX;
  c.alto = VENTANA_PX;
  c.paso_fila = (size_t)VENTANA_PX * 3;

  int8_t *destino = motor_buffer_entrada();
  remuestrear_ventana(c, 0, 0, VENTANA_PX, ENTRADA_PX, temporal,
                      (size_t)VENTANA_PX * ENTRADA_PX * 3, destino);

  testigo(true);
  const uint32_t t0 = micros();
  for (int i = 0; i < kRepeticiones; ++i) {
    remuestrear_ventana(c, 0, 0, VENTANA_PX, ENTRADA_PX, temporal,
                        (size_t)VENTANA_PX * ENTRADA_PX * 3, destino);
  }
  const uint32_t t1 = micros();
  testigo(false);

  Serial.printf("remuestreo_por_tesela_us=%u\n", (unsigned)((t1 - t0) / kRepeticiones));
  liberar_grande(fuente);
  liberar_grande(temporal);
}

void medir_captura_completa() {
  if (!camara_iniciar()) {
    Serial.println("banco: sin camara; se omite la captura completa");
    return;
  }
  Cuadro cuadro;

  testigo(true);
  const uint32_t t0 = micros();
  const bool ok = camara_capturar(&cuadro);
  const uint32_t t1 = micros();
  if (!ok) {
    testigo(false);
    Serial.println("banco: fallo la captura");
    return;
  }
  const ResultadoDeteccion r = deteccion_procesar(cuadro);
  const uint32_t t2 = micros();
  testigo(false);
  camara_liberar(&cuadro);

  Serial.printf("cuadro=%dx%d teselas=%u\n", cuadro.ancho, cuadro.alto,
                (unsigned)r.teselas_total);
  Serial.printf("captura_us=%u\n", (unsigned)(t1 - t0));
  Serial.printf("proceso_total_us=%u\n", (unsigned)(t2 - t1));
  Serial.printf("  remuestreo_ms=%u inferencia_ms=%u\n", (unsigned)r.ms_remuestreo,
                (unsigned)r.ms_inferencia);
  Serial.printf("pmax=%.3f sobre_umbral=%u alerta=%d\n", r.prob_maxima / 1000.0,
                (unsigned)r.teselas_sobre, (int)r.alerta);

  // Extrapolacion al dia, que es el numero que va a analisis-de-potencia.md.
  const double s_por_captura = (double)(t2 - t0) / 1e6;
  Serial.printf("activo_por_captura_s=%.2f\n", s_por_captura);
  Serial.printf("activo_diario_s=%.1f  (48 capturas, cadencia-captura.md §5)\n",
                s_por_captura * 48.0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(2000);
  if (kPinTestigo >= 0) pinMode(kPinTestigo, OUTPUT);

  Serial.println("\n== banco EVT — nodo de humo Amazonia+ ==");
  Serial.printf("cpu_mhz=%u psram_libre=%u heap_libre=%u\n", (unsigned)getCpuFrequencyMhz(),
                (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());

  if (!deteccion_iniciar()) {
    Serial.println("banco: no arranco el motor de inferencia (arena insuficiente?)");
    return;
  }
  Serial.printf("arena_reservada=%u arena_usada=%u\n", (unsigned)ARENA_TENSORES_BYTES,
                (unsigned)motor_arena_usada());

  medir_inferencia_sola();
  medir_remuestreo_solo();
  medir_captura_completa();
  Serial.println("== fin ==");
}

void loop() { delay(1000); }

#endif  // BANCO_EVT
