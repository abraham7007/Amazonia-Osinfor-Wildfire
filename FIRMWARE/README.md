# Firmware — un nodo, una carpeta, once etapas

Proyecto **Amazonía+ / Piloto OSINFOR — CCNN Paoyhan**.

No hay un firmware monolítico que probar de golpe. Hay **once etapas en escalera
por cada nodo**: cada una es un proyecto PlatformIO independiente, se graba sola
y responde una pregunta. La última integra todo y es la que se despliega.

## Empieza aquí

| Tu placa | Carpeta | Primer paso |
|---|---|---|
| **Seeed XIAO ESP32S3 Sense** | [`nodo-1-xiao/`](nodo-1-xiao/) | `cd nodo-1-xiao/01-placa && pio run -t upload -t monitor` |
| **CamThink NeoEyes NE101** | [`nodo-2-ne101/`](nodo-2-ne101/) | `cd nodo-2-ne101/01-placa && pio run -t upload -t monitor` |

Cada proyecto ya está fijado a su placa: no hay que elegir entorno ni acordarse
de ningún `-e`. Abre la carpeta de tu nodo, empieza por la `01` y baja en orden.

Las particularidades de cada placa están en su propio README:
[nodo 1](nodo-1-xiao/README.md) · [nodo 2](nodo-2-ne101/README.md).

---

## La escalera

| | Etapa | Responde | Incorpora |
|---|---|---|---|
| 01 | `01-placa` | ¿Está viva la placa? SoC, flash, **PSRAM** | — |
| 02 | `02-memoria` | ¿Cabe todo lo que el nodo necesita, a la vez? | — |
| 03 | `03-camara` | ¿Captura a 1280×720 y convierte a RGB888? | `camara` |
| 04 | `04-teselado` | ¿La rejilla y el remuestreo dan lo que el modelo espera? | `nucleo` |
| 05 | `05-inferencia` | ¿El modelo da lo mismo que en el PC, y en cuánto tiempo? | `modelo`, `inferencia` |
| 06 | `06-deteccion` | Una captura completa: 32 teselas y regla de disparo | `deteccion` |
| 07 | `07-sueno` | ¿Despierta del deep-sleep y conserva su estado? | — |
| 08 | `08-almacenamiento` | ¿Puede guardar capturas en microSD? | — |
| 09 | `09-energia` | ¿Sabe cuánta batería le queda? | — |
| 10 | `10-enlace` | ¿Llega el aviso al gateway? | — |
| 11 | `11-nodo` | **El nodo completo**, en ciclo con deep-sleep | todo |

Cada firmware imprime lo que encuentra, da un veredicto y se queda esperando:
`r` + ENTER lo repite sin volver a grabar.

---

## Dónde vive el código

```
FIRMWARE/
  nodo-1-xiao/        11 proyectos, fijados al XIAO
    01-placa/
      platformio.ini
      src/main.cpp    el código que se carga en la placa
    ...
  nodo-2-ne101/       11 proyectos, fijados al NE101
  comun/
    librerias/        ocho librerías compartidas
    verificar_copias.py
  pruebas-host/       25 pruebas unitarias en el PC, sin placa
  herramientas/       decoder del servidor y generación de vectores
```

Cada proyecto es autocontenido: tiene su `platformio.ini` y su `src/`, y ahí está
todo el código que se graba. No hay que saltar a ninguna otra carpeta para leerlo.

**El código de cada etapa es idéntico en los dos nodos**, y debe seguir siéndolo:
todo lo que cambia entre placas vive en
[`comun/librerias/placa/src/placa.h`](comun/librerias/placa/src/placa.h). El precio
de tener dos copias es que pueden divergir sin que nadie se entere —alguien
arregla un fallo en un nodo y se olvida del otro—, así que hay un verificador:

```bash
python3 FIRMWARE/comun/verificar_copias.py
# y si algo se desvió, tomando un nodo como bueno:
python3 FIRMWARE/comun/verificar_copias.py --sincronizar nodo-1-xiao
```

Conviene pasarlo antes de cada commit que toque el código de una etapa.

| Librería | Qué es |
|---|---|
| `placa` | Lo único que difiere entre los dos nodos: pinout, memoria, periféricos |
| `informe` | Formato de salida por serie, común a las once etapas |
| `nucleo` | Teselado, remuestreo, tramas, política de energía. Sin dependencias del SoC |
| `plataforma` | Implementación ESP32-S3 de lo que `nucleo` declara |
| `camara` | Captura a 1280×720 y conversión a RGB888 |
| `modelo` | `cnn_media` INT8, 198 KiB |
| `inferencia` | Frontera con TFLite-Micro |
| `deteccion` | Una captura completa: rejilla, inferencia por tesela, regla |

PlatformIO enlaza solo las librerías que cada etapa incluye de verdad: `01-placa`
compila en 4 s y no toca TFLite.

## Pruebas del núcleo en el PC

```bash
cd FIRMWARE/pruebas-host && pio test -e host
```

25 comprobaciones que no necesitan placa: la rejilla, las tramas, la política de
energía y la **paridad byte a byte del remuestreo con Pillow**, que es la
operación con la que se generaron los parches de entrenamiento.

## Herramientas

```bash
node FIRMWARE/herramientas/probar_decoder.js
```

| | |
|---|---|
| `decoder_lorawan.js` | Payload formatter para ChirpStack / The Things Stack |
| `probar_decoder.js` | Verifica el decoder contra las tramas que emite el firmware |
| `generar_vectores.py` | Regenera los vectores de paridad tras cambiar de modelo |

---

## Lo que hay que saber antes de empezar

**Ninguna de las dos placas tiene LoRa.** La etapa 10 prueba Wi-Fi + TCP a
Node-RED porque es el transporte que el prototipo usa realmente. El nodo deja el
transporte detrás de `hal_lora.h` y funciona con un backend de traza que imprime
la trama por serie, hasta que `ADR-001` decida. Ver
[`../HARDWARE/README.md`](../HARDWARE/README.md) §2.

**Dos etapas fallan a propósito en el NE101** (`08` y `09`): faltan datos de la
placa que CamThink no publica. Está explicado en
[`nodo-2-ne101/README.md`](nodo-2-ne101/README.md).

**Las credenciales del gateway no están en este repositorio.** La etapa 10 las
lee de `comun/credenciales.ini`, que no se versiona. Para configurarlas:

```bash
cp FIRMWARE/comun/credenciales.ini.ejemplo FIRMWARE/comun/credenciales.ini
# y edita ese fichero con el SSID y la clave reales
```

Sin él, la etapa 10 compila con valores de marcador y el enlace falla al no
encontrar la red, que es lo que debe pasar.
