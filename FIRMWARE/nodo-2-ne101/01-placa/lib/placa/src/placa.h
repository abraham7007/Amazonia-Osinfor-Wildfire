// placa.h — Todo lo que difiere entre los dos nodos, en un solo sitio.
//
// Nodo A: Seeed XIAO ESP32S3 Sense   (-DPLACA_XIAO_S3_SENSE)
// Nodo B: CamThink NeoEyes NE101     (-DPLACA_NE101)
//
// Los pinout provienen del informe de implementacion del prototipo (anexos 4 y 5),
// donde estan reportados como probados en cada placa. Ver HARDWARE/README.md §3.
#pragma once

#include <stdint.h>

struct PinesCamara {
  int pwdn, reset, xclk, siod, sioc;
  int y9, y8, y7, y6, y5, y4, y3, y2;
  int vsync, href, pclk;
};

#if defined(PLACA_XIAO_S3_SENSE)

#define PLACA_NOMBRE       "Seeed XIAO ESP32S3 Sense"
#define PLACA_SENSOR_ESP   "OV2640 u OV3660"
#define PLACA_FLASH_MB     8
#define PLACA_PSRAM_MB     8
// El XIAO no tiene linea de habilitacion de la camara: el sensor esta siempre
// alimentado mientras la placa lo este.
#define PLACA_PIN_ENCENDIDO_CAMARA  (-1)
#define PLACA_TIENE_PIR    0

// microSD en la placa de expansion Sense. Ocupa 4 GPIO y, mientras esta en uso,
// el SPI del XIAO no queda libre para nada mas.
#define PLACA_TIENE_SD     1
#define PLACA_SD_PIN_CS    21
#define PLACA_SD_PIN_SCK    8
#define PLACA_SD_PIN_MISO   9
#define PLACA_SD_PIN_MOSI   7

#define PLACA_PIN_LED      21   // LED de usuario, activo en BAJO
#define PLACA_LED_ACTIVO_BAJO 1
// Divisor de bateria del XIAO Sense: 1/2 sobre GPIO 2 (A1).
#define PLACA_PIN_BATERIA  2
#define PLACA_DIVISOR_BATERIA 2.0f

static const PinesCamara kPines = {
    /*pwdn*/ -1, /*reset*/ -1, /*xclk*/ 10, /*siod*/ 40, /*sioc*/ 39,
    /*y9*/ 48, /*y8*/ 11, /*y7*/ 12, /*y6*/ 14, /*y5*/ 16,
    /*y4*/ 18, /*y3*/ 17, /*y2*/ 15,
    /*vsync*/ 38, /*href*/ 47, /*pclk*/ 13};

#elif defined(PLACA_NE101)

#define PLACA_NOMBRE       "CamThink NeoEyes NE101"
#define PLACA_SENSOR_ESP   "OV5640"
#define PLACA_FLASH_MB     16
#define PLACA_PSRAM_MB     8
// El NE101 apaga el dominio de potencia del sensor para ahorrar en deep-sleep.
// Sin poner GPIO 3 en alto antes de esp_camera_init(), la camara no responde.
#define PLACA_PIN_ENCENDIDO_CAMARA  3
#define PLACA_TIENE_PIR    1

// El NE101 tiene ranura micro-TF, pero CamThink no publica su cableado y los
// PDF de la compra son imagenes escaneadas. Se deja SIN FIJAR a proposito: es
// preferible que la etapa 08 diga "faltan los pines" a que intente un SPI
// inventado y el fallo se interprete como tarjeta defectuosa.
// Se resuelve con el multimetro sobre la placa, o preguntando al fabricante.
#define PLACA_TIENE_SD     1
#define PLACA_SD_PIN_CS    (-1)
#define PLACA_SD_PIN_SCK   (-1)
#define PLACA_SD_PIN_MISO  (-1)
#define PLACA_SD_PIN_MOSI  (-1)

#define PLACA_PIN_LED      (-1)   // el NE101 usa LED de relleno, no LED de usuario
#define PLACA_LED_ACTIVO_BAJO 0
#define PLACA_PIN_BATERIA  (-1)   // medida por la cabecera de 16 pines, sin fijar
#define PLACA_DIVISOR_BATERIA 1.0f

static const PinesCamara kPines = {
    /*pwdn*/ -1, /*reset*/ -1, /*xclk*/ 15, /*siod*/ 4, /*sioc*/ 5,
    /*y9*/ 11, /*y8*/ 9, /*y7*/ 8, /*y6*/ 10, /*y5*/ 12,
    /*y4*/ 18, /*y3*/ 17, /*y2*/ 16,
    /*vsync*/ 6, /*href*/ 7, /*pclk*/ 13};

#else
#error "Definir PLACA_XIAO_S3_SENSE o PLACA_NE101 (lo hace platformio.ini)."
#endif

#if !defined(NOMBRE_NODO)
#define NOMBRE_NODO "NODO"
#endif
