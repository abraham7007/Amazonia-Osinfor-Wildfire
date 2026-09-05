# Pruebas de hardware de los nodos

Proyecto **Amazonía+ / Piloto OSINFOR — CCNN Paoyhan**.

Verifican en la placa real lo que el firmware del nodo da por supuesto, **antes**
de cargar el firmware. Son dos nodos distintos y las pruebas lo saben: cada
entorno compila su propio pinout, su propio sensor y sus propios periféricos.

| | Entorno | Placa | Cámara |
|---|---|---|---|
| **Nodo A** | `xiao_s3` | Seeed XIAO ESP32S3 Sense | OV2640 / OV3660 |
| **Nodo B** | `ne101` | CamThink NeoEyes NE101 | OV5640 |

```bash
cd FIRMWARE/pruebas-hardware

pio run -e xiao_s3 -t upload -t monitor      # Nodo A
pio run -e ne101   -t upload -t monitor      # Nodo B
```

En el monitor serie sale un menú: escribe el número de una prueba y ENTER, o `a`
para lanzarlas todas.

---

## Las pruebas

| | Qué comprueba | Por qué está |
|---|---|---|
| **T01** | SoC, revisión, flash, **PSRAM** | Sin PSRAM el nodo no puede capturar a 1280×720 (un cuadro RGB888 son 2,64 MB). Si T01 falla, el problema es de diseño, no de firmware: las demás pruebas sobran. |
| **T02** | Que quepan a la vez cuadro HD + buffer de remuestreo + arena de 320 KB | Si una no cabe, el fallo aparece en campo tras horas, no al arrancar. Por eso se reservan las tres juntas a propósito. |
| **T03** | Cámara: init con el pinout de *esta* placa, PID del sensor, `FRAMESIZE_HD`, conversión a RGB888, latencia | El pinout y el sensor son lo que más difiere entre los dos nodos. |
| **T04** | Deep-sleep por temporizador y persistencia de memoria RTC | Si la RTC no conserva el estado, el nodo rehace la sesión en cada despertar y agota la batería sin que nada parezca roto. *(Reinicia la placa: se lanza por separado.)* |
| **T05** | El modelo en la placa: cuantización de entrada y **latencia por inferencia** | Es la prueba que decide la viabilidad. Da el número que le falta a `analisis-de-potencia.md` y que bloquea `ADR-001`. |
| **T06** | Enlace con el gateway UG67: AP visible, RSSI, asociación, TCP al 8888 de Node-RED | Es el transporte que el prototipo usa de verdad. Mide el RSSI, que dice si el enlace aguantará la distancia de despliegue. |
| **T07** | microSD: montaje, escritura de 512 KB, relectura | La SD es la vía para recolectar imágenes en Paoyhan, que es lo único que cierra la brecha de dominio del modelo. |
| **T08** | Lectura de batería y testigo de consumo en GPIO | Sin medida de carga no hay política de ahorro. El testigo permite repartir la corriente medida entre captura, remuestreo, inferencia y radio. |
| **T09** | **Distinta por nodo.** XIAO: LED, micrófono PDM, carga LiPo. NE101: conmutación del dominio de potencia de la cámara, PIR, luz de relleno, IP67 | Dar por bueno en un nodo lo probado en el otro es justo el error que se paga en campo. |

---

## Dos cosas que estas pruebas dejaron en claro

**El NE101 necesita `GPIO 3` en alto antes de `esp_camera_init()`.** Corta la
alimentación del sensor para ahorrar en deep-sleep. Sin ese paso la cámara
devuelve `0x105` y parece un pinout mal puesto cuando en realidad está apagada.
T03 lo hace explícito y T09 comprueba que la línea conmuta de verdad —que es lo
que hace que el ahorro se materialice.

**Ninguna de las dos placas tiene LoRa.** Por eso T06 prueba Wi-Fi + TCP y no
LoRaWAN: es el transporte que el prototipo usa realmente. Ver `HARDWARE/README.md`
§2 para las tres salidas posibles y lo que implica cada una.

---

## Antes de la campaña de medida de EVT

Tres constantes hay que fijar mirando la placa, y están marcadas en el código:

- `kPinTestigo` en `src/pruebas/t08_energia.cpp` — GPIO accesible para el analizador de potencia.
- `PLACA_PIN_BATERIA` en `include/placa.h` — sin fijar en el NE101, que expone la medida por la cabecera de 16 pines.
- `kPinPir` en `src/pruebas/t09_perifericos.cpp` — solo si se va a usar el PIR, que el nodo de humo no necesita.
