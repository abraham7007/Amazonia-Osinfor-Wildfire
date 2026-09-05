# Hardware de los nodos — referencia consolidada

Proyecto **Amazonía+ / Piloto OSINFOR — CCNN Paoyhan**.

El piloto tiene **dos nodos**, y son dos placas distintas con dos cámaras
distintas. Este documento reúne lo que hay que saber de cada una para escribir y
probar firmware, porque estaba repartido entre el informe de implementación, los
datasheets del fabricante y los PDF de la Compra 1 (que son imágenes escaneadas y
no se pueden consultar por búsqueda).

| | **Nodo A** | **Nodo B** |
|---|---|---|
| Placa | Seeed **XIAO ESP32S3 Sense** | CamThink **NeoEyes NE101** |
| Cámara | OV2640 / OV3660 (según lote) | **OV5640** 5 MP |
| Entorno PlatformIO | `xiao_s3` | `ne101` |

---

## 1. Comparativa

| | XIAO ESP32S3 Sense | NeoEyes NE101 |
|---|---|---|
| MCU | ESP32-S3R8, Xtensa LX7 doble núcleo, 240 MHz | ESP32-S3, Xtensa LX7 doble núcleo, 240 MHz |
| SRAM interna | 512 KB | 512 KB |
| PSRAM | **8 MB** | **8 MB octal** |
| Flash | 8 MB | **16 MB** |
| Cámara | OV3660 2048×1536 (compatible OV2640 / OV5640) | OV5640 hasta **2592×1944** |
| Óptica | fija, la del módulo | intercambiable: FOV 60° / 120°, enfoque cercano o lejano |
| Luz de apoyo | no | LED de relleno + fotosensor |
| Almacenamiento | microSD hasta 32 GB (FAT) | ranura micro-TF |
| Radio | Wi-Fi 4 2,4 GHz + BLE 5.0 | Wi-Fi 4 2,4 GHz + BLE 5.0 |
| Radio opcional | — | LTE Cat.1 (`-L01GL` / `-L02NA`), Wi-Fi HaLow 868/915 MHz (`-HL00` / `-HL01`) |
| **LoRa** | **no** | **no** |
| Alimentación | 5 V USB-C o batería LiPo 3,7 V | 5 V USB-C o **4× AA** |
| Carga de batería | integrada (100 mA rápida / 0,9 mA goteo) | no integrada |
| Deep sleep | ~3 mA (placa completa con expansión) | deep sleep con dominios de potencia de cámara, luz y almacenamiento controlados por firmware |
| Disparadores | GPIO / RTC | RTC, botón, **PIR**, entrada de alarma, GPIO |
| Expansión | conector B2B, 11 GPIO, 9 ADC | **cabecera de 16 pines** con UART, SPI y GPIO; conector PIR de 4 vías; alarma de 2 vías |
| Antena | conector U.FL | antena en PCB |
| Temperatura | comercial | **−20 °C a +50 °C** |
| Protección | ninguna (placa desnuda; carcasa impresa en el piloto) | **IP67 de fábrica**, ventana de vidrio templado |
| Dimensiones | 21 × 17,8 × 15 mm (con placa de expansión) | 77 × 77 × 48 mm |
| Certificación | — | CE / FCC |
| Desarrollo | Arduino / ESP-IDF / PlatformIO | firmware de referencia abierto, **ESP-IDF y Arduino** |

Datasheets en [datasheets/](datasheets/).

---

## 2. El punto que hay que resolver antes de EVT: **ninguno de los dos tiene LoRa**

`DISEÑO E INGENIERIA/04-diseno-firmware-software/protocolo-lorawan.md` §1 dice:

> Radio LoRa dedicada **integrada en la placa del nodo** (SX126x) — *Confirmado por área usuaria.*

Los datasheets de ambas placas lo contradicen. El XIAO ESP32S3 Sense lleva Wi-Fi
y BLE; el NE101 lleva Wi-Fi y BLE, y sus variantes de largo alcance son LTE Cat.1
y Wi-Fi HaLow, **no LoRa**. Ninguna de las dos puede hablar LoRaWAN con el gateway
Milesight UG67 sin añadir una radio SX126x que **no está en el BOM adjudicado**.

Y es lo que ya ocurrió en la práctica: el prototipo del informe de implementación
**no usa LoRaWAN**. Usa el UG67 como punto de acceso Wi-Fi (`OSINFOR1_GW`,
192.168.10.1) y envía un JSON por **socket TCP al puerto 8888** hacia Node-RED.
Funciona, pero es una arquitectura distinta de la documentada, con otro alcance y
otro consumo.

Hay tres caminos, y la decisión es de `ADR-001`:

| Camino | Qué implica |
|---|---|
| **A · Mantener Wi-Fi + TCP** | Es lo que ya funciona. Alcance de decenas o pocos cientos de metros hasta el gateway, no los 5 km del resultado 1 del proyecto. Sirve para el piloto de validación, no para la meta de cobertura. |
| **B · Añadir SX1262** | Recupera LoRaWAN y los 5 km. Requiere comprar la radio (Wio-SX1262 para el XIAO; módulo en la cabecera de 16 pines del NE101) y es un cambio de BOM. |
| **C · Wi-Fi HaLow en el NE101** | Sub-1 GHz, largo alcance, sin cambiar de familia de placa: es una variante de catálogo (`NE101-HL01`, 915 MHz para América). Exige un gateway HaLow, que el UG67 no es. |

Mientras no se decida, el firmware del nodo deja la capa de transporte detrás de
`hal_lora.h` y funciona con un backend de traza. Ver `FIRMWARE/README.md`.

---

## 3. Pinout de cámara — verificado

Estos son los mapeos que el informe de implementación reporta como probados en
las dos placas. Están volcados tal cual en
`FIRMWARE/nodo-esp32s3/src/hal/camara_esp32.cpp` y en las pruebas de hardware.

| Señal | XIAO ESP32S3 Sense | NeoEyes NE101 |
|---|---|---|
| `PWDN` | −1 (sin control) | −1 (sin control) |
| `RESET` | −1 (sin control) | −1 (sin control) |
| `XCLK` | 10 | **15** |
| `SIOD` (SDA) | 40 | **4** |
| `SIOC` (SCL) | 39 | **5** |
| `Y9` (D7) | 48 | **11** |
| `Y8` (D6) | 11 | **9** |
| `Y7` (D5) | 12 | **8** |
| `Y6` (D4) | 14 | **10** |
| `Y5` (D3) | 16 | 12 |
| `Y4` (D2) | 18 | 18 |
| `Y3` (D1) | 17 | 17 |
| `Y2` (D0) | 15 | **16** |
| `VSYNC` | 38 | **6** |
| `HREF` | 47 | **7** |
| `PCLK` | 13 | 13 |

**Nota sobre el NE101:** el firmware de referencia enciende `GPIO 3` en alto antes
de inicializar la cámara (`pinMode(3, OUTPUT); digitalWrite(3, HIGH);`). Es la
habilitación del dominio de potencia del sensor: sin ese paso `esp_camera_init()`
falla. La prueba `T02` lo verifica explícitamente.

---

## 4. Resolución de captura y su coste

El modelo `cnn_media` se entrenó y evaluó sobre cuadros de **1280×720**, que dan
una rejilla de **32 teselas** de 224 px con 25 % de solape. La resolución no es
un ajuste libre: cambia el número de inferencias por captura y, con él, la energía.

| Resolución | Teselas por captura | Coste relativo | Nota |
|---|---|---|---|
| 640×480 (VGA) | 12 | 0,38× | pierde alcance: la columna lejana no se resuelve |
| **1280×720 (HD)** | **32** | **1,00×** | **la geometría del entrenamiento** |
| 1600×1200 (UXGA) | 70 | 2,19× | solo NE101/OV5640; duplica con creces la energía |
| 2592×1944 (QSXGA) | 195 | 6,09× | inviable con batería |

El prototipo del informe capturaba a **QQVGA 160×120**, que es 40 veces menos
píxeles que HD. Con esa entrada una columna de humo a distancia ocupa menos de un
píxel: explica por qué el modelo de Edge Impulse quedó en F1 0,52. **No reutilizar
esa configuración de cámara.**

---

## 5. Verificación en la placa

Antes de cargar el firmware del nodo, las pruebas de
`FIRMWARE/pruebas-hardware/` comprueban en la placa real lo que este documento
afirma: presencia y tamaño de PSRAM, identificación del sensor, que `FRAMESIZE_HD`
se pueda reservar, la latencia de inferencia y la persistencia de la memoria RTC
entre deep-sleeps. Se ejecutan una por una desde el monitor serie.

---

## Fuentes

- CamThink, *NeoEyes NE101 — Ultra-Low-Power Event Camera*, datasheet oficial ([PDF local](datasheets/CamThink_NeoEyes_NE101_datasheet.pdf), [camthink.ai](https://www.camthink.ai/))
- CamThink, *NeoEyes NE101 Series — Product Information*, [wiki.camthink.ai](https://wiki.camthink.ai/docs/neoeyes-ne101-series/overview/)
- Seeed Studio, *XIAO ESP32S3 — Getting Started*, [wiki.seeedstudio.com](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) ([esquemático local](datasheets/Seeed_XIAO_ESP32S3_Sense_datasheet.pdf))
- CNX Software, *CamThink NeoEyes NE101 — a low-power ESP32-S3 Vision AI camera*, [cnx-software.com](https://www.cnx-software.com/2025/07/17/camthink-neoeyes-ne101-a-low-power-esp32-s3-vision-ai-camera-with-optional-4g-lte-and-wifi-halow-connectivity/)
- Pinouts: informe de implementación del prototipo, anexos 4 y 5 (código retirado, mapeo conservado).
