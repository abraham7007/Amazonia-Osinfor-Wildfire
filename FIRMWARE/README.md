# Firmware del nodo — detección temprana de humo

Proyecto **Amazonía+ / Piloto OSINFOR — CCNN Paoyhan**.

Implementa en el microcontrolador el modelo `cnn_media` entrenado en
`MODELO-TINYML/`, y emite la trama LoRaWAN definida en
`DISEÑO E INGENIERIA/04-diseno-firmware-software/protocolo-lorawan.md`.

Cubre las versiones **V1 (MCU TinyML)** y **V2 (IA on-sensor)** de
`comparacion-versiones.md`. **No** cubre V3 (Raspberry Pi 5 + Hailo-8): esa
versión queda como referencia de precisión y no se aborda en este piloto.

---

## Estado

| Pieza | Estado |
|---|---|
| Núcleo portable (teselado, remuestreo, inferencia, regla, payload, energía) | **Completo y probado** |
| Máquina de estados y ciclo de deep-sleep | **Completa**, verificada en host |
| Plataforma ESP32-S3 (tiempo, PSRAM, RTC, deep-sleep, batería) | **Completa** salvo el pin del divisor de batería |
| Driver de cámara `esp32-camera` | Completo **salvo el pinout de la placa** (ADR-001) |
| Pila LoRaWAN | **Abstraída**: backend de traza activo; driver RadioLib esbozado |
| Decoder del servidor | **Completo y probado** |
| Banco de medida para EVT | **Completo**, a la espera de placa |

Lo único que bloquea tener firmware corriendo en hardware es **elegir la placa**
(`ADR-001`) y con ella el pinout de cámara y la radio.

---

## Cómo se ejecuta hoy, sin placa

Con `g++` a secas — **es la vía verificada**, porque en el equipo de desarrollo
todavía no hay PlatformIO instalado:

```bash
cd FIRMWARE/nodo-esp32s3

# Pruebas del núcleo (25/25, incluye la paridad byte a byte con Pillow)
g++ -std=c++17 -Iinclude -Itest/test_nucleo -DHAL_SIMULADA -DMOTOR_SIMULADO \
    test/test_nucleo/main.cpp src/teselado.cpp src/remuestreo.cpp \
    src/payload.cpp src/energia.cpp -o /tmp/test && /tmp/test

# Una pasada completa del nodo, con cámara y radio simuladas
g++ -std=c++17 -Iinclude -DHAL_SIMULADA -DMOTOR_SIMULADO \
    src/main.cpp src/teselado.cpp src/remuestreo.cpp src/payload.cpp \
    src/energia.cpp src/deteccion.cpp src/motor_inferencia_sim.cpp \
    src/hal/camara_sim.cpp src/hal/lora_sim.cpp src/hal/plataforma_host.cpp \
    -o /tmp/nodo && /tmp/nodo
```

Y el decoder del servidor:

```bash
node FIRMWARE/herramientas/probar_decoder.js
```

Con PlatformIO (`pip install platformio`), lo mismo queda en:

```bash
cd FIRMWARE/nodo-esp32s3

# Pruebas del núcleo (incluye la paridad con Pillow)
pio test -e host

# Una pasada completa del nodo, con cámara y radio simuladas
pio run -e host && .pio/build/host/program

# Recorrer el día sin esperar
NODO_EPOCH=$(date -j -f '%Y-%m-%d %H:%M' '2026-09-05 12:00' +%s) .pio/build/host/program
```

`platformio.ini` trae cinco entornos: `host`, `xiao_s3`, `ne101`, `banco_xiao` y
las pruebas. Los cuatro que compilan para ESP32-S3 **no se han podido verificar
todavía**: requieren el toolchain de Espressif y, sobre todo, la placa elegida.

---

## Lo que el firmware hereda del entrenamiento, y por qué no se puede tocar

Tres constantes de `include/config.h` **no son parámetros de diseño**: son la
geometría exacta con la que se construyó el dataset. Cambiar cualquiera de ellas
sin reentrenar hace que el modelo vea en campo algo distinto de lo que vio en
entrenamiento, y el nodo seguirá funcionando —sin dar ningún síntoma— detectando
peor.

| Constante | Valor | Fijado por |
|---|---|---|
| `VENTANA_PX` | 224 | `02_construir_dataset.py` |
| `ENTRADA_PX` | 96 | entrada del modelo |
| solape | 25 % | `06_evaluar_por_cuadro.py` |

Sobre 1280×720 dan exactamente **32 teselas** (8×4). `test_nucleo` fija esas 32
esquinas una a una.

### El remuestreo es la trampa de este porte

Los parches de entrenamiento se generaron con
`img.resize((96,96), Image.BILINEAR, box=...)`. Al **reducir**, Pillow no
interpola cuatro vecinos: escala el soporte del filtro por el factor de reducción
(224/96 = 2,33), de modo que cada píxel de salida promedia **7** de entrada por
eje. Un bilineal ingenuo de 2×2 descarta el 80 % de los píxeles y mete aliasing
justo sobre las columnas de humo tenues, que son el caso que el nodo debe
detectar.

`src/remuestreo.cpp` replica el algoritmo de Pillow **incluyendo su aritmética de
punto fijo**, y la prueba lo verifica contra vectores generados con Pillow:

```
0/27648 bytes distintos de Pillow, peor desvio 0
```

Regenerar los vectores tras cualquier cambio de modelo o geometría:

```bash
MODELO-TINYML/.venv/bin/python FIRMWARE/herramientas/generar_vectores.py
```

### Cuantización

La entrada INT8 tiene escala 1,0 y punto cero −128: basta `pixel − 128`, sin
coma flotante. La salida tiene escala 1/256 y punto cero −128, así que
`prob = (salida + 128) / 256`. El firmware trabaja con esa probabilidad en escala
entera 0–1000 y no usa `float` en el camino de decisión.

---

## Punto de operación

`UMBRAL_PROB_X1000 = 970` y `K_TESELAS_MINIMO = 1`, de
`modelos/cnn_media/evaluacion_por_cuadro_sdis-77.json` (partner reservado, otra
región: el escenario más parecido a Paoyhan).

> **Corrección respecto del README de `MODELO-TINYML`.** La tabla "Por captura"
> de ese README —la que recomienda K ≥ 2 con umbral 0,73— corresponde a
> `mobilenetv2_035`, no a `cnn_media`. Para `cnn_media` los números son otros y
> la conclusión se invierte: la confirmación espacial **no** ayuda.
>
> | Regla | Umbral | Recall | FPR |
> |---|---|---|---|
> | **K = 1** | **0,97** | **0,226** | **0,098** |
> | K = 2 | 0,62 | 0,175 | 0,089 |
> | K = 3 | 0,32 | 0,213 | 0,091 |
>
> Conviene corregirlo en `modelo-edge-ai.md` antes de que se traslade a las EETT.

Lo que sí se confirma es que la **confirmación temporal no sirve**: los falsos
positivos son bancos de niebla y nube baja, que persisten entre capturas igual
que persistiría el humo. El firmware no la implementa, y `§1` de
`modelo-edge-ai.md` —que la da por buena— debería corregirse.

El recall por captura de 0,23 no es el número que importa: un incendio persiste, y
con capturas cada 15 min la probabilidad acumulada de haberlo detectado tras *M*
capturas es `1 − (1 − recall)^M`. Es el argumento de `07_curva_operacion.py`.

---

## Estructura

```
nodo-esp32s3/
  include/config.h            Todo parámetro de operación, con su justificación
  src/teselado.cpp            Rejilla de ventanas (paridad con el script de evaluación)
  src/remuestreo.cpp          224→96 con paridad byte a byte con Pillow
  src/motor_inferencia_*.cpp  Frontera con TFLite-Micro (real | simulado)
  src/deteccion.cpp           Captura completa: 32 teselas → regla de disparo
  src/payload.cpp             Tramas de 6 y 14 bytes, sin dependencias del MCU
  src/energia.cpp             Ventana diurna, cadencia adaptativa, batería
  src/main.cpp                Máquina de estados y ciclo de deep-sleep
  src/hal/                    Cámara, radio, plataforma — un backend por destino
  src/banco/banco_evt.cpp     Banco de medida de latencia y consumo (EVT)
  lib/modelo_humo/            cnn_media INT8, 198 KiB
  test/test_nucleo/           Pruebas en host
  test/test_modelo/           Paridad del modelo PC ↔ MCU, en la placa
herramientas/
  generar_vectores.py         Regenera los vectores de paridad
  decoder_lorawan.js          Payload formatter (ChirpStack / TTS)
  probar_decoder.js           Verifica el decoder contra las tramas del firmware
```

La regla de organización: **nada específico del SoC fuera de `src/hal/`**. Por eso
el núcleo entero se compila y se prueba en el PC, y por eso cambiar de placa —o
de radio— no toca la lógica de detección.

---

## Lo que falta, por orden

1. **`ADR-001`: elegir la placa.** Desbloquea el pinout de cámara
   (`src/hal/camara_esp32.cpp`, un bloque de 12 constantes) y el pin del divisor
   de batería.
2. **Confirmar la radio.** `protocolo-lorawan.md` §1 dice "SX126x **integrada** en
   la placa del nodo, confirmado por área usuaria", pero el XIAO ESP32-S3 Sense
   no trae radio LoRa: necesita el Wio-SX1262 acoplado, que **no aparece en el
   BOM adjudicado**. Conviene aclararlo antes de EVT.
3. **Correr `banco_evt` y llevar el resultado a `analisis-de-potencia.md`.** Hoy
   ese documento no incluye ningún coste de inferencia, y son ~1 500 inferencias
   diarias. Es el dato que cierra `ADR-001` y el hito de las EETT fotovoltaicas.
4. **Correr `test_modelo` en la placa.** Si el INT8 del MCU no coincide con el del
   PC, todo lo medido en `05_evaluar.py` deja de aplicar.
5. **Fuente de hora.** La ventana diurna depende de conocer la hora local. Sin
   RTC en el BOM, hay que fijarla por downlink en el comisionado y contar con la
   deriva del oscilador interno entre despertares.
6. **OTA.** No implementada. Un nodo en el dosel a varios días de camino, sin
   actualización remota, obliga a bajarlo para cada cambio de umbral.
