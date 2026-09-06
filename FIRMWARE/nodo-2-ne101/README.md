# Nodo 2 — CamThink NeoEyes NE101

Proyecto **Amazonía+ / Piloto OSINFOR — CCNN Paoyhan**.

| | |
|---|---|
| MCU | ESP32-S3, Xtensa LX7 doble núcleo, 240 MHz |
| PSRAM / Flash | 8 MB octal / 16 MB |
| Cámara | OV5640, hasta 2592×1944, óptica intercambiable 60°/120° |
| Almacenamiento | ranura micro-TF, **cableado sin documentar** |
| Radio | Wi-Fi 4 + BLE 5.0. **Sin LoRa** (opcionales: LTE Cat.1, Wi-Fi HaLow) |
| Alimentación | USB-C 5 V o 4× AA |
| Carcasa | **IP67 de fábrica**, ventana de vidrio templado, −20 a +50 °C |
| Etiqueta en las tramas | `NEO_CAM` |

## La escalera

Cada carpeta es un proyecto PlatformIO independiente. Se abre, se graba y
responde una pregunta. **Empieza por la 01 y baja en orden.**

```bash
cd FIRMWARE/nodo-2-ne101/01-placa
pio run -t upload -t monitor
```

No hay que elegir entorno: cada proyecto ya está fijado a esta placa, y el código
que se graba está en su propio `src/`.

> El código de cada etapa es **idéntico** al de `nodo-1-xiao`: todo lo que
> cambia entre placas vive en `../comun/librerias/placa/src/placa.h`. Si tocas una
> etapa aquí, pasa `python3 ../comun/verificar_copias.py` para no dejar los dos
> nodos ejecutando cosas distintas.
Cada firmware imprime lo que encuentra, da un veredicto y se queda esperando;
`r` + ENTER lo repite sin volver a grabar.

| | Proyecto | Responde | En esta placa |
|---|---|---|---|
| 01 | `01-placa/` | ¿Está viva? SoC, flash, **PSRAM** | conmuta el dominio de potencia de la cámara (GPIO 3) |
| 02 | `02-memoria/` | ¿Cabe todo a la vez? | igual en ambos nodos |
| 03 | `03-camara/` | ¿Captura a 1280×720? | OV5640; **el sensor hay que encenderlo antes** (GPIO 3) |
| 04 | `04-teselado/` | ¿La rejilla y el remuestreo dan lo que el modelo espera? | igual en ambos nodos |
| 05 | `05-inferencia/` | ¿El modelo da lo mismo que en el PC, y en cuánto? | igual en ambos nodos |
| 06 | `06-deteccion/` | Una captura completa: 32 teselas y regla de disparo | igual en ambos nodos |
| 07 | `07-sueno/` | ¿Despierta y conserva su estado? | igual en ambos nodos |
| 08 | `08-almacenamiento/` | ¿Puede guardar capturas? | **falla a propósito**: faltan los pines de la ranura |
| 09 | `09-energia/` | ¿Sabe cuánta batería le queda? | **falla a propósito**: falta cablear la medida |
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

**El sensor está apagado hasta que se le enciende.** El NE101 corta la
alimentación de la cámara para ahorrar en deep-sleep. Sin poner **GPIO 3** en
alto antes de `esp_camera_init()`, la cámara devuelve `0x105` y parece un pinout
mal puesto cuando en realidad no tiene corriente. La librería `camara` lo hace
sola; la etapa 01 comprueba que la línea conmuta de verdad, que es lo que hace
que el ahorro se materialice.

**Dos etapas fallan a propósito, y no es un error del firmware:**

- `08-almacenamiento` — CamThink no publica el cableado de la ranura micro-TF y
  los PDF de la compra son imágenes escaneadas. Se dejó sin fijar para que la
  etapa diga «faltan los pines» en vez de intentar un SPI inventado y que el
  fallo se confunda con una tarjeta defectuosa. Se resuelve con el multímetro o
  preguntando al fabricante, y se rellena `PLACA_SD_PIN_*` en
  [`../../comun/librerias/placa/src/placa.h`](../../comun/librerias/placa/src/placa.h).
- `09-energia` — la medida de batería sale por la cabecera de 16 pines y hay que
  cablearla. Se rellena `PLACA_PIN_BATERIA` en el mismo fichero.

El nodo funciona sin ninguna de las dos: la microSD solo hace falta para
recolectar imágenes en Paoyhan —que es lo que cerraría la brecha de dominio del
modelo— y sin medida de batería el nodo no entra en modo ahorro.

**El perfil de placa no está validado.** CamThink no publica uno para PlatformIO,
así que se usa el genérico `esp32-s3-devkitc-1` con la flash y la PSRAM que
declara su datasheet. Compila, pero confírmalo con la etapa 01 sobre la placa
real: para eso está.
