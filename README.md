# Amazonía+ · Detección temprana de incendios forestales

**Piloto OSINFOR — Comunidad Nativa Paoyhan** (Padre Márquez, Ucayali, Loreto, Perú).
Universidad Nacional de Ingeniería.

Nodo autónomo de vigilancia forestal que detecta **columnas de humo** con un modelo
TinyML corriendo en el propio microcontrolador, y avisa por radio. Existe porque el
satélite no sirve para detección *temprana* en Loreto: la nubosidad lo ciega
durante días y su latencia de proceso va de 1 a 40 horas, así que cuando detecta
el evento el incendio ya es inasumible de suprimir.

---

## Qué hay aquí

| Carpeta | Contenido |
|---|---|
| [`MODELO-TINYML/`](MODELO-TINYML/) | Entrenamiento, cuantización y evaluación del clasificador humo / no-humo. **El modelo desplegable es `cnn_media`**: 198 KiB INT8. |
| [`FIRMWARE/`](FIRMWARE/) | Firmware del nodo y pruebas de hardware, en PlatformIO. |
| [`HARDWARE/`](HARDWARE/) | Referencia consolidada de las dos placas, con pinouts verificados y datasheets. |
| [`DISEÑO E INGENIERIA/`](DISEÑO%20E%20INGENIERIA/) | Documentación de ingeniería: requisitos, arquitectura, ADR, plan de pruebas. |
| [`CONTEXTO DEL PROYECTO/`](CONTEXTO%20DEL%20PROYECTO/) | Resumen del proyecto, objetivos y resultados comprometidos. |

## Por dónde empezar

1. [`CONTEXTO DEL PROYECTO/Contexto del Proyecto.md`](CONTEXTO%20DEL%20PROYECTO/Contexto%20del%20Proyecto.md) — qué es el piloto y a qué se comprometió.
2. [`MODELO-TINYML/README.md`](MODELO-TINYML/README.md) — cómo se entrenó el modelo y qué rinde de verdad.
3. [`HARDWARE/README.md`](HARDWARE/README.md) — las dos placas, sus pinouts y **el problema de la radio**.
4. [`FIRMWARE/README.md`](FIRMWARE/README.md) — el firmware del nodo y qué falta para cargarlo.

---

## Estado

**Modelo: entrenado y evaluado.** `cnn_media` cumple las cuatro metas de diseño por
tesela sobre cámaras nunca vistas de la misma región (recall 0,932 · precisión
0,852 · F1 0,890 · FPR 0,096) y **pierde 0,001 de F1 al cuantizar a INT8**, frente
a los 0,082 que perdía MobileNetV2. En una región nueva la detección a dos horas
cae del 95 % al 46 %: esa brecha es de dominio, no de arquitectura, y solo se
cierra recolectando imágenes en Paoyhan.

**Firmware: núcleo completo y probado en host.** Teselado, remuestreo, inferencia,
regla de disparo, máquina de estados y payload. El remuestreo reproduce **byte a
byte** el de Pillow con el que se construyó el dataset de entrenamiento, verificado
contra vectores generados con Pillow (0/27648 bytes de diferencia).

**Pruebas de hardware: escritas, a la espera de placa.** Nueve pruebas por nodo,
diferenciadas por placa.

**Bloqueado por `ADR-001`:** elegir la placa del piloto, y con ella el transporte.

---

## Los dos nodos

| | Nodo A | Nodo B |
|---|---|---|
| Placa | Seeed XIAO ESP32S3 Sense | CamThink NeoEyes NE101 |
| Cámara | OV2640 / OV3660 | OV5640 |
| PSRAM / Flash | 8 MB / 8 MB | 8 MB / 16 MB |
| Carcasa | impresa a medida | IP67 de fábrica |
| Entorno | `xiao_s3` | `ne101` |

## Tres cosas que conviene saber antes de tocar nada

**Ninguna de las dos placas tiene LoRa.** `protocolo-lorawan.md` §1 da por hecho una
SX126x integrada; los datasheets de ambas dicen Wi-Fi y BLE. El prototipo que ya
funciona usa el gateway UG67 como punto de acceso Wi-Fi y manda un JSON por TCP a
Node-RED. Es una arquitectura distinta de la documentada, con otro alcance.
Detalle y salidas posibles en [`HARDWARE/README.md`](HARDWARE/README.md) §2.

**El nodo no clasifica el cuadro completo.** La columna de humo mediana mide 41×32
px sobre 1280×720; reducir el cuadro entero a 96×96 la dejaría en 3×2 px. El nodo
recorre el cuadro con una ventana de 224 px que reduce a 96×96: **32 teselas por
captura**, ~1500 inferencias al día. Ese coste todavía no está en el análisis de
potencia.

**El punto de operación es K = 1 con umbral 0,97.** La tabla "Por captura" del
README de `MODELO-TINYML` corresponde a `mobilenetv2_035`; para `cnn_media` los
números son otros y la confirmación espacial no ayuda. La temporal tampoco, en
ninguno de los dos: los falsos positivos son bancos de niebla, que persisten entre
capturas igual que el humo.

---

## Nota sobre este repositorio

Contiene únicamente el trabajo técnico. Quedan fuera, a propósito, el dataset
(10 GB, se regenera con `01_descargar_pyro_sdis.py` y `02_construir_dataset.py`),
el entorno virtual, las trazas de entrenamiento y toda la documentación
administrativa del proyecto. Ver [`.gitignore`](.gitignore).

**No contiene credenciales.** Las del punto de acceso del gateway se leen de
`FIRMWARE/comun/credenciales.ini`, que no se versiona; hay una plantilla en
`credenciales.ini.ejemplo`. El historial anterior a este cambio sí las tuvo, así
que la clave que allí aparece debe considerarse comprometida y estar ya cambiada
en el equipo.
