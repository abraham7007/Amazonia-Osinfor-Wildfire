// Banco de pruebas de hardware — Amazonia+ / Piloto OSINFOR (CCNN Paoyhan).
//
// Se ejecuta desde el monitor serie: escribe el numero de una prueba y ENTER, o
// "a" para lanzarlas todas en orden. Cada prueba es independiente y dice, cuando
// falla, que revisar.
//
//   pio run -e xiao_s3 -t upload -t monitor      Nodo A · Seeed XIAO ESP32S3 Sense
//   pio run -e ne101   -t upload -t monitor      Nodo B · CamThink NeoEyes NE101
//
// El orden importa: si T01 dice que no hay PSRAM, las demas no tienen sentido.

#include <Arduino.h>

#include "placa.h"
#include "pruebas.h"

bool t01_placa();
bool t02_memoria();
bool t03_camara();
bool t04_sueno();
bool t05_inferencia();
bool t06_enlace();
bool t07_almacenamiento();
bool t08_energia();
bool t09_perifericos();

namespace {

const Prueba kPruebas[] = {
    {"T01", "Identidad de la placa (SoC, flash, PSRAM)", t01_placa},
    {"T02", "Presupuesto de memoria del nodo", t02_memoria},
    {"T03", "Camara: sensor, 1280x720 y conversion RGB888", t03_camara},
    {"T04", "Deep-sleep y persistencia de memoria RTC", t04_sueno},
    {"T05", "Modelo en la placa: cuantizacion y latencia", t05_inferencia},
    {"T06", "Enlace con el gateway UG67 (Wi-Fi + TCP a Node-RED)", t06_enlace},
    {"T07", "Tarjeta microSD", t07_almacenamiento},
    {"T08", "Bateria y testigo de consumo", t08_energia},
    {"T09", "Perifericos propios de " PLACA_NOMBRE, t09_perifericos},
};
constexpr int kN = sizeof(kPruebas) / sizeof(kPruebas[0]);

void menu() {
  Serial.println();
  Serial.println("=================================================================");
  Serial.printf("  Pruebas de hardware — %s\n", PLACA_NOMBRE);
  Serial.printf("  Nodo \"%s\"  ·  Amazonia+ / Piloto OSINFOR — CCNN Paoyhan\n", NOMBRE_NODO);
  Serial.println("=================================================================");
  for (int i = 0; i < kN; ++i) {
    Serial.printf("  %d) %s  %s\n", i + 1, kPruebas[i].codigo, kPruebas[i].titulo);
  }
  Serial.println("  a) todas en orden");
  Serial.println("  m) volver a mostrar este menu");
  Serial.println("-----------------------------------------------------------------");
  Serial.print("> ");
}

bool lanzar(int i) {
  Serial.println();
  Serial.printf("--- %s · %s ---\n", kPruebas[i].codigo, kPruebas[i].titulo);
  const uint32_t t0 = millis();
  const bool r = kPruebas[i].ejecutar();
  Serial.printf("--- %s: %s (%u ms) ---\n", kPruebas[i].codigo, r ? "CORRECTO" : "FALLO",
                (unsigned)(millis() - t0));
  return r;
}

void todas() {
  int bien = 0;
  for (int i = 0; i < kN; ++i) {
    // T04 reinicia la placa a proposito: lanzarla dentro de "todas" cortaria la
    // secuencia a la mitad, asi que se deja fuera y se pide aparte.
    if (strcmp(kPruebas[i].codigo, "T04") == 0) {
      Serial.println("\n--- T04 omitida en el modo 'todas' (reinicia la placa) ---");
      Serial.println("    lanzala por separado con la opcion 4.");
      continue;
    }
    if (lanzar(i)) ++bien;
  }
  Serial.printf("\n===== resumen: %d de %d pruebas correctas =====\n", bien, kN - 1);
}

}  // namespace

const Prueba *catalogo_pruebas(int *n) {
  if (n) *n = kN;
  return kPruebas;
}

void setup() {
  Serial.begin(115200);
  delay(2500);

  // Si la placa acaba de despertar de T04, esa prueba continua sola.
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("\n[despertar por temporizador — continuando T04]");
    lanzar(3);
  }

  menu();
}

void loop() {
  if (!Serial.available()) {
    delay(50);
    return;
  }

  String linea = Serial.readStringUntil('\n');
  linea.trim();
  if (linea.length() == 0) return;

  if (linea == "m") {
    menu();
    return;
  }
  if (linea == "a") {
    todas();
    menu();
    return;
  }

  const int n = linea.toInt();
  if (n >= 1 && n <= kN) {
    lanzar(n - 1);
  } else {
    Serial.printf("opcion no reconocida: \"%s\"\n", linea.c_str());
  }
  menu();
}
