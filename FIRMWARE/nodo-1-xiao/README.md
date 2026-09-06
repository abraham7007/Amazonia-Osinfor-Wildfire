# Nodo 1 — Seeed XIAO ESP32S3 Sense

Proyecto **Amazonía+ / Piloto OSINFOR — CCNN Paoyhan**.

| | |
|---|---|
| MCU | ESP32-S3R8, Xtensa LX7 doble núcleo, 240 MHz |
| PSRAM / Flash | 8 MB / 8 MB |
| Cámara | OV2640 u OV3660, según lote |
| Almacenamiento | microSD en la placa Sense (CS=21, SCK=8, MISO=9, MOSI=7) |
| Radio | Wi-Fi 4 + BLE 5.0. **Sin LoRa** |
| Alimentación | USB-C 5 V o LiPo 3,7 V, con carga en placa |
| Carcasa | ninguna: la IP67 es la caja impresa del piloto |
| Etiqueta en las tramas | `XIAO_CAM` |

## La escalera

Cada carpeta es un proyecto PlatformIO independiente. Se abre, se graba y
responde una pregunta. **Empieza por la 01 y baja en orden.**

```bash
cd FIRMWARE/nodo-1-xiao/01-placa
pio run -t upload -t monitor
```

No hay que elegir entorno ni depender de nada de fuera: cada proyecto trae su
`platformio.ini`, su `src/` con el código que se graba y su `lib/` con las
librerías que usa. Puedes copiar una carpeta a otro sitio y compila igual.

> El código de cada etapa es **idéntico** al de `nodo-2-ne101`, y las
> librerías son copias de `../comun/librerias/`. Todo lo que cambia entre placas
> vive en `placa/src/placa.h`, nunca en el código de una etapa. Si tocas algo
> aquí, pasa `python3 ../comun/verificar_copias.py` para no dejar los dos nodos
> ejecutando cosas distintas.
Cada firmware imprime lo que encuentra, da un veredicto y se queda esperando;
`r` + ENTER lo repite sin volver a grabar.

| | Proyecto | Responde | En esta placa |
|---|---|---|---|
| 01 | `01-placa/` | ¿Está viva? SoC, flash, **PSRAM** | parpadea el LED de usuario (GPIO 21) |
| 02 | `02-memoria/` | ¿Cabe todo a la vez? | igual en ambos nodos |
| 03 | `03-camara/` | ¿Captura a 1280×720? | OV2640/OV3660; el sensor está siempre alimentado |
| 04 | `04-teselado/` | ¿La rejilla y el remuestreo dan lo que el modelo espera? | igual en ambos nodos |
| 05 | `05-inferencia/` | ¿El modelo da lo mismo que en el PC, y en cuánto? | igual en ambos nodos |
| 06 | `06-deteccion/` | Una captura completa: 32 teselas y regla de disparo | igual en ambos nodos |
| 07 | `07-sueno/` | ¿Despierta y conserva su estado? | igual en ambos nodos |
| 08 | `08-almacenamiento/` | ¿Puede guardar capturas? | **funciona**: pines de la placa Sense ya fijados |
| 09 | `09-energia/` | ¿Sabe cuánta batería le queda? | **funciona**: divisor en GPIO 2 |
| 10 | `10-enlace/` | ¿Llega el aviso al gateway? | igual en ambos nodos |
| 11 | `11-nodo/` | **El nodo completo**, en ciclo con deep-sleep | igual en ambos nodos |

**El orden importa.** La 01 primero siempre: si no aparece la PSRAM, el nodo no
puede capturar a 1280×720 —un cuadro RGB888 son 2,64 MB— y el problema es de
diseño, no de firmware; las demás etapas sobran. La 05 antes que la 07 porque
decide la viabilidad. La 10 al final porque necesita el gateway encendido.

### Las tres que deciden algo

**04 · teselado** es donde se esconde el error más caro del porte a TinyML. Si el
parche que llega al modelo no es el mismo tipo de parche que vio en
entrenamiento, el nodo funcionará, transmitirá y **detectará mal, sin dar ningún
síntoma**. Por eso está aparte y dibuja la tesela en el monitor.

**05 · inferencia** da el número que bloquea `ADR-001` y el dimensionado
fotovoltaico: el coste de ~1500 inferencias diarias, que `analisis-de-potencia.md`
todavía no incluye.

**06 · detección** es el primer ciclo real. Si dispara sin humo delante, ese falso
positivo es exactamente el dato que falta para Paoyhan: apúntalo con la escena.

---

## Particularidades de este nodo

**La placa va desnuda.** No tiene carcasa propia: la protección IP67 es la caja
impresa a medida del piloto. El montaje está documentado en el informe de
implementación.

**Ojo con el SPI.** La microSD ocupa cuatro GPIO y, mientras está en uso, el SPI
del XIAO no queda libre para nada más.

**No tiene radio LoRa.** La etapa 10 prueba Wi-Fi + TCP a Node-RED, que es el
transporte que el prototipo usa realmente. Ver [`../../HARDWARE/README.md`](../../HARDWARE/README.md) §2.
