// T07 — Tarjeta microSD.
//
// La SD no es imprescindible para alertar, pero si para lo unico que puede cerrar
// la brecha de dominio del modelo: recolectar imagenes en Paoyhan. El nodo puede
// guardar una captura cada cierto tiempo durante la estacion seca y esas imagenes
// son las que reentrenan el modelo con niebla amazonica real. Sin SD, esa via se
// pierde.

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "placa.h"
#include "pruebas.h"

bool t07_almacenamiento() {
#if !PLACA_TIENE_SD
  dato("esta placa no declara ranura de tarjeta; prueba omitida");
  return true;
#else
  if (!SD.begin()) {
    fallo("no se monto la tarjeta: comprueba que este insertada y formateada en FAT32");
    return false;
  }

  const uint8_t tipo = SD.cardType();
  const char *nombre = tipo == CARD_MMC   ? "MMC"
                       : tipo == CARD_SD  ? "SDSC"
                       : tipo == CARD_SDHC ? "SDHC"
                                            : "desconocida";
  if (tipo == CARD_NONE) {
    fallo("no hay tarjeta insertada");
    SD.end();
    return false;
  }
  ok("tarjeta %s montada, %llu MB", nombre, SD.cardSize() / (1024ULL * 1024ULL));
  dato("usado %llu MB de %llu MB", SD.usedBytes() / (1024ULL * 1024ULL),
       SD.totalBytes() / (1024ULL * 1024ULL));

  // Escritura de un bloque del tamano tipico de un JPEG HD, para medir si la
  // tarjeta aguanta el ritmo de captura sin dominar el tiempo activo.
  const size_t kBloque = 64 * 1024;
  uint8_t *buf = (uint8_t *)malloc(kBloque);
  if (buf == nullptr) {
    fallo("sin memoria para la prueba de escritura");
    SD.end();
    return false;
  }
  memset(buf, 0xA5, kBloque);

  File f = SD.open("/prueba_osinfor.bin", FILE_WRITE);
  if (!f) {
    fallo("no se pudo crear el fichero de prueba (tarjeta protegida?)");
    free(buf);
    SD.end();
    return false;
  }
  const uint32_t t0 = millis();
  size_t escritos = 0;
  for (int i = 0; i < 8; ++i) escritos += f.write(buf, kBloque);
  f.close();
  const uint32_t ms = millis() - t0;
  free(buf);

  if (escritos != kBloque * 8) {
    fallo("escritura incompleta: %u de %u bytes", (unsigned)escritos, (unsigned)(kBloque * 8));
    SD.remove("/prueba_osinfor.bin");
    SD.end();
    return false;
  }
  ok("512 KB escritos en %u ms (%.1f KB/s)", (unsigned)ms, escritos / 1024.0 / (ms / 1000.0));

  File r = SD.open("/prueba_osinfor.bin");
  const bool tam_ok = r && r.size() == kBloque * 8;
  if (r) r.close();
  if (tam_ok) {
    ok("relectura correcta");
  } else {
    fallo("el fichero releido no tiene el tamano escrito");
  }

  SD.remove("/prueba_osinfor.bin");
  SD.end();
  return tam_ok;
#endif
}
