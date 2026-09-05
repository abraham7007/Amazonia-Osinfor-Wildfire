// T01 — Identidad de la placa.
//
// Es la primera porque todo lo demas depende de ella. En particular, si la PSRAM
// no aparece, el nodo NO puede capturar a 1280x720 (un cuadro RGB888 son 2,7 MB)
// y no tiene sentido seguir: la geometria del modelo entrenado deja de ser
// alcanzable y habria que replantear el diseno, no el firmware.

#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "placa.h"
#include "pruebas.h"

bool t01_placa() {
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

  return bien;
}
