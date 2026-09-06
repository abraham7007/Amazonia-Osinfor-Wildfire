// Etapa 01 — ¿Está viva la placa?
//
// Es la primera porque todo lo demas depende de ella. En particular, si la PSRAM
// no aparece, el nodo NO puede capturar a 1280x720 (un cuadro RGB888 son 2,7 MB)
// y no tiene sentido seguir: la geometria del modelo entrenado deja de ser
// alcanzable y habria que replantear el diseno, no el firmware.

#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_mac.h>

#include "placa.h"
#include "informe.h"

bool etapa_principal() {
  bool bien = true;

  dato("placa declarada: %s  (nodo \"%s\")", PLACA_NOMBRE, NOMBRE_NODO);

  esp_chip_info_t chip;
  esp_chip_info(&chip);
  const char *modelo = (chip.model == CHIP_ESP32S3) ? "ESP32-S3" : "NO ES UN ESP32-S3";
  dato("SoC: %s  rev %d  %d nucleo(s)  %u MHz", modelo, (int)chip.revision, (int)chip.cores,
       (unsigned)getCpuFrequencyMhz());
  if (chip.model != CHIP_ESP32S3) {
    fallo("el firmware asume ESP32-S3; esta placa no lo es");
    bien = false;
  }

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  dato("MAC Wi-Fi: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  const uint32_t flash_mb = ESP.getFlashChipSize() / (1024 * 1024);
  dato("flash: %u MB (esperado %d MB)", (unsigned)flash_mb, PLACA_FLASH_MB);
  if (flash_mb < (uint32_t)PLACA_FLASH_MB) {
    fallo("hay menos flash de la declarada: revisa el entorno de PlatformIO");
    bien = false;
  } else {
    ok("flash coherente con la placa declarada");
  }

  const size_t psram = ESP.getPsramSize();
  const size_t psram_mb = psram / (1024 * 1024);
  if (psram == 0) {
    fallo("SIN PSRAM. No se puede capturar a 1280x720 ni cargar el modelo.");
    fallo("revisa -DBOARD_HAS_PSRAM y board_build.psram_type en platformio.ini");
    bien = false;
  } else {
    dato("PSRAM: %u MB, libre %u KB", (unsigned)psram_mb,
         (unsigned)(ESP.getFreePsram() / 1024));
    if (psram_mb < (size_t)PLACA_PSRAM_MB) {
      fallo("menos PSRAM de la esperada (%d MB)", PLACA_PSRAM_MB);
      bien = false;
    } else {
      ok("PSRAM presente y del tamano esperado");
    }
  }

  dato("heap interno libre: %u KB, bloque mayor %u KB",
       (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
       (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024));

  // --- Lo propio de esta placa -------------------------------------------
  // Dar por bueno en un nodo lo comprobado en el otro es el error que se paga
  // en campo, asi que cada placa verifica aqui lo suyo.
#if defined(PLACA_XIAO_S3_SENSE)
  pinMode(PLACA_PIN_LED, OUTPUT);
  dato("LED de usuario en GPIO %d: tres parpadeos", PLACA_PIN_LED);
  for (int i = 0; i < 3; ++i) {
    digitalWrite(PLACA_PIN_LED, LOW);   // activo en bajo
    delay(200);
    digitalWrite(PLACA_PIN_LED, HIGH);
    delay(200);
  }
  dato("si viste tres parpadeos, el GPIO responde");
  dato("microfono PDM presente en la placa Sense; el nodo de humo no lo usa");
  dato("carga de bateria gestionada en placa (100 mA rapida / 0,9 mA goteo)");
  dato("esta placa va desnuda: la IP67 es la caja impresa del piloto");
#elif defined(PLACA_NE101)
  // Esta linea es la que hace que el ahorro en deep-sleep se materialice: si no
  // conmuta, el sensor sigue consumiendo con el SoC dormido y la autonomia que
  // promete el fabricante no se cumple.
  pinMode(PLACA_PIN_ENCENDIDO_CAMARA, OUTPUT);
  digitalWrite(PLACA_PIN_ENCENDIDO_CAMARA, LOW);
  delay(300);
  digitalWrite(PLACA_PIN_ENCENDIDO_CAMARA, HIGH);
  delay(300);
  digitalWrite(PLACA_PIN_ENCENDIDO_CAMARA, LOW);
  ok("dominio de potencia de la camara (GPIO %d) conmutado", PLACA_PIN_ENCENDIDO_CAMARA);
  dato("con el analizador de potencia debe verse una caida clara al ponerlo en BAJO");
  dato("interfaz PIR y entrada de alarma presentes; el nodo de humo no las necesita:");
  dato("el disparo es por temporizador, no por movimiento");
  dato("carcasa IP67 de fabrica con ventana de vidrio templado");
#endif

  return bien;
}

void setup() { etapa("01", "Identidad de la placa", etapa_principal); }
void loop() { reposo(); }
