# Modelo TinyML — detección de columnas de humo

Implementación del plan descrito en
`DISEÑO E INGENIERIA/04-diseno-firmware-software/modelo-edge-ai.md` (secciones 2 a 6).
Entrena el clasificador binario **humo / no-humo** que corre en el nodo V1
(ESP32-S3 + TFLite-Micro, INT8) y produce el valor `confidence` de la trama LoRa
definida en `protocolo-lorawan.md` §3.1.

---

## Por qué no se usó `Dataset-Fire`

El conjunto que ya estaba en el proyecto (`AMAZONIA-OSINFOR/Dataset-Fire`, 5 853
`fire` / 9 755 `not_fire`) no sirve para entrenar este modelo, aunque a primera
vista lo parezca:

- Los **positivos son llamas en primer plano** —fogatas, casas ardiendo, imágenes
  térmicas, bomberos en escena—, no columnas de humo vistas desde lejos. El nodo
  nunca verá eso: cuando la llama llene el cuadro de una cámara fija, el incendio
  ya es incontrolable, que es justamente el escenario que el proyecto quiere evitar.
- Los **negativos no tienen relación con la escena de despliegue**: hojas de otoño,
  nieve, faros, auroras boreales. Un modelo entrenado así aprende «naranja saturado
  = fuego» y da métricas de validación excelentes que no significan nada.
- **No contiene el caso difícil.** El documento de diseño identifica la niebla
  matinal y la nube baja sobre el dosel como la causa principal de falsos positivos
  (§1 de `modelo-edge-ai.md`). En `Dataset-Fire` no hay una sola imagen de niebla
  sobre bosque etiquetada como negativa.

Sigue siendo útil para el taller comunitario (ver
`fire_dataset/seleccion_taller_comunidad/`), que es para lo que se seleccionó.

## Qué se usa en su lugar

**Pyro-SDIS** (PyroNear + bomberos SDIS de Francia, Apache-2.0), la referencia
[R03] del propio documento de diseño: 33 636 imágenes de **cámaras fijas de
vigilancia** —el mismo tipo de sensor y el mismo punto de vista que tendrá el
nodo—, 28 137 con columna de humo anotada con caja y 5 499 negativos capturados
por **esas mismas cámaras**: niebla, nube baja sobre la cresta, amanecer,
contraluz. Ese es el negativo duro que el diseño pide en §4.1.

---

## Hallazgo que cambia el diseño del firmware: hay que recorrer el cuadro por teselas

Al medir las anotaciones de Pyro-SDIS sobre cuadros de 1280×720:

| Tamaño de la columna de humo | p10 | p50 | p90 |
|---|---|---|---|
| Ancho (fracción del cuadro) | 1,5 % | 3,2 % | 9,1 % |
| Alto (fracción del cuadro) | 2,3 % | 4,5 % | 9,4 % |

La columna mediana mide **unos 41 × 32 píxeles**. Reducir el cuadro completo a
96×96 —que es como suele plantearse un clasificador TinyML— la dejaría en **3 × 2
píxeles**: por debajo de lo detectable. Un modelo entrenado sobre cuadros
completos reducidos no puede funcionar para detección *temprana*, que es
precisamente el objetivo del proyecto frente al satélite.

**Consecuencia:** el nodo no clasifica el cuadro completo. Recorta una **ventana
de 224×224 px** del cuadro original, la reduce a 96×96 y ejecuta una inferencia;
repite sobre una rejilla que cubre el cuadro. Sobre 1280×720 son **24 teselas** sin
solape (6 × 4) o **32 con 25 % de solape** (8 × 4), que es lo que usa la evaluación
porque evita que una columna caiga partida entre dos teselas. Con la cadencia
diurna de 15 min ya aprobada (`cadencia-captura.md` §5) son 48 capturas al día, o
sea **~1 500 inferencias diarias**, cuyo costo energético debe medirse en EVT y
realimentar `analisis-de-potencia.md`.

### El corolario que obliga a cambiar la meta de FPR

El nodo dispara la alerta si **alguna** tesela supera el umbral. Con teselas
independientes, la falsa alarma por captura es `1 − (1 − FPR_tesela)^32`. La meta
escrita en §5 del diseño, `FPR ≤ 0,10`, es **por tesela**: llevada al cuadro
completo daría una falsa alarma en el **97 % de las capturas**. Para que el cuadro
cumpla 0,10 la FPR por tesela tiene que bajar a **~0,003**.

Por eso `06_evaluar_por_cuadro.py` mide directamente a nivel de captura y evalúa
dos reglas de confirmación: exigir **K teselas** en la misma captura (confirmación
espacial) y exigir detección en capturas consecutivas (confirmación temporal, la
mitigación que propone §1 del diseño). Ese es el número que hay que llevar a
`ADR-001`, no el de la métrica por parche.

El dataset de entrenamiento se construye con exactamente esa geometría, para que
lo que ve el modelo en entrenamiento sea lo que verá en el nodo.

---

## Dataset generado

`datos/clasificacion-96/` — parches de 96×96 RGB, uint8.

| Partición | Parches | Humo | No-humo | Cámaras |
|---|---|---|---|---|
| `train` | 135 561 | 51 526 | 84 035 | 29 |
| `val` | 24 889 | 9 284 | 15 605 | 3 |
| `cruzado` | 10 157 | 3 408 | 6 749 | 8 |

- **Positivos:** ventana centrada en cada caja anotada, con y sin jitter.
- **Negativos duros:** ventanas sin solape con ninguna caja tomadas de los *mismos
  cuadros que sí tienen humo* — mismo bosque, misma cámara, misma luz.
- **Negativos limpios:** ventanas de los cuadros sin anotación (niebla, nubes).

**La partición es por cámara, no por imagen.** Los cuadros de una misma cámara
fija son casi duplicados; repartirlos al azar entre entrenamiento y validación
inflaría las métricas sin que se note. `cruzado` reserva el partner `sdis-77`
completo (8 cámaras, otra región) para la validación cruzada que pide §6 del
documento de diseño: es la única cifra que estima de verdad cómo se comportará el
modelo en un bosque que nunca vio — el escenario de Paoyhan.

---

## Uso

```bash
uv venv --python 3.11 .venv
VIRTUAL_ENV=.venv uv pip install "tensorflow>=2.16,<2.20" "numpy<2.2" pillow \
    scikit-learn matplotlib pandas huggingface_hub pyarrow tqdm

.venv/bin/python scripts/01_descargar_pyro_sdis.py      # 3,3 GB
.venv/bin/python scripts/02_construir_dataset.py        # ~35 min, 4,0 GB
.venv/bin/python scripts/03_entrenar.py --arquitectura mobilenetv2_035
.venv/bin/python scripts/04_cuantizar_tflite.py --modelo mobilenetv2_035
.venv/bin/python scripts/05_evaluar.py --modelo mobilenetv2_035
```

| Script | Qué hace |
|---|---|
| `01_descargar_pyro_sdis.py` | Descarga los parquet de Pyro-SDIS desde HuggingFace. |
| `02_construir_dataset.py` | Recorta los parches 96×96 y los reparte por cámara. |
| `03_entrenar.py` | Transferencia + ajuste fino, con neblina sintética. |
| `04_cuantizar_tflite.py` | PTQ INT8 y arreglo C para TFLite-Micro. |
| `05_evaluar.py` | Métricas §5 y elección del umbral de operación. |
| `inspeccionar_parquet.py`, `ver_parches.py` | Verificación manual. |

### Arquitecturas

- `mobilenetv2_035` — MobileNetV2 α=0,35 @96×96 con pesos ImageNet, 411 k
  parámetros. Es el único MobileNet de Keras con pesos preentrenados a 96×96, que
  es lo que pide §4.3 del diseño. Requiere PSRAM para el *tensor arena*.
- `cnn_pequena` — CNN separable de cuatro bloques, 10,7 k parámetros. Cabe en la
  SRAM interna del ESP32-S3 sin PSRAM, pero es demasiado débil para la tarea.
- `cnn_media` — misma familia, 148 929 parámetros, ReLU6 en todas las capas y
  ninguna salida lineal ancha. Busca la precisión de MobileNet con la
  cuantización limpia de la CNN simple.

Las arquitecturas sin backbone preentrenado se entrenan a `lr = 1e-3` con
decaimiento coseno; sólo el ajuste fino sobre pesos ImageNet usa `1e-4`.

La bandera `--gris` entrena en escala de grises (replicada a tres canales) para
contrastar con la suposición de §1 del diseño; el color separa el humo pardo de la
niebla blanca, así que conviene medir cuánto cuesta descartarlo.

### Aumentado

Brillo, contraste, volteo horizontal, rotación y zoom suaves, y **neblina
sintética** (`CapaNeblina`): un velo claro de opacidad aleatoria sobre el 35 % de
los lotes, que reproduce la niebla matinal amazónica. No hay volteo vertical: el
cielo está arriba y la columna sube.

## Metas (§5 del documento de diseño)

`recall ≥ 0,90` · `precisión ≥ 0,80` · `F1 ≥ 0,75` · `FPR ≤ 0,10`

`05_evaluar.py` elige el umbral de operación como **el más bajo que aún respeta la
meta de FPR**, y reporta el resultado sobre `val` y sobre `cruzado` por separado,
tanto en float32 como en INT8, para cuantificar la pérdida por cuantización.

---

# Resultados

Se entrenaron tres arquitecturas. **La recomendada es `cnn_media`** (AUC-PR de
validación 0,963, INT8 de 198 KiB, cumple las cuatro metas de §5 por tesela);
`mobilenetv2_035` queda como referencia. Todas sus operaciones —`CONV_2D`,
`DEPTHWISE_CONV_2D`, `ADD`, `MUL`, `MEAN`, `FULLY_CONNECTED`, `LOGISTIC`— están en
el resolvedor estándar de TFLite-Micro.

| Modelo | Épocas | Mejor AUC-PR val | INT8 |
|---|---|---|---|
| `mobilenetv2_035` | 16 (4 cabeza + 12 ajuste) | 0,930 | 606 KiB |
| `cnn_pequena` | 6 (parada temprana) | 0,585 | 26,5 KiB |
| **`cnn_media`** | 11 (parada temprana) | **0,963** | **198 KiB** |

Las secciones que siguen dan primero las cifras de `mobilenetv2_035`, que fue la
línea base, y luego la comparación.

## Por tesela (`05_evaluar.py`)

| Conjunto | Modelo | Umbral | Recall | Precisión | F1 | FPR |
|---|---|---|---|---|---|---|
| `val` (3 cámaras nuevas, misma región) | float32 | 0,21 | 0,846 | 0,834 | **0,840** | 0,100 |
| `val` | int8 | 0,77 | 0,697 | 0,831 | 0,758 | 0,084 |
| `cruzado` (`sdis-77`, otra región) | float32 | 0,16 | 0,618 | 0,758 | **0,681** | 0,100 |
| `cruzado` | int8 | 0,43 | 0,530 | 0,731 | 0,614 | 0,099 |

El F1 cruzado de 0,68 coincide con lo que reporta el propio PyroNear (~0,60 F1
cruzado, citado en §5 del diseño): el resultado está en el rango esperado para
esta tarea, no por debajo.

**La cuantización cuesta caro y no es un problema de calibración.** Subir el
conjunto representativo de 400 a 3 000 muestras movió el F1 cruzado de 0,610 a
0,614: nada. La pérdida de ~8 puntos es estructural de MobileNetV2 —sus cuellos de
botella lineales tienen un rango dinámico ancho que el INT8 por capa no representa
bien—. La comparación con una CNN separable sin bloques residuales lineales lo
confirma:

| Arquitectura | Parámetros | INT8 | F1 `val` float32 | F1 `val` int8 | Pérdida |
|---|---|---|---|---|---|
| `mobilenetv2_035` | 411 489 | 606 KiB | 0,840 | 0,758 | **−0,082** |
| `cnn_pequena` | 10 716 | 26,5 KiB | 0,411 | 0,403 | −0,008 |
| **`cnn_media`** | 148 929 | **198 KiB** | **0,891** | **0,890** | **−0,001** |

La CNN separable cuantiza prácticamente sin pérdida. Con 10,7 k parámetros es
demasiado débil para la tarea, pero **con 148 929 gana en las dos cosas a la vez**.
(El F1 absoluto de `cnn_pequena` además está subestimado: se entrenó antes de
corregir la tasa de aprendizaje, con `1e-4` en vez de `1e-3`. Lo que importa ahí es
la columna de pérdida, que no depende de eso.)

### `cnn_media` es el modelo a desplegar

| | `mobilenetv2_035` | `cnn_media` |
|---|---|---|
| Tamaño INT8 | 606 KiB | **198 KiB** (3× menor) |
| `val` INT8 — recall / precisión / F1 / FPR | 0,697 / 0,831 / 0,758 / 0,084 | **0,932 / 0,852 / 0,890 / 0,096** |
| `cruzado` INT8 — F1 | 0,614 | **0,640** |
| Metas §5 por tesela en `val` | no cumple | **cumple las cuatro** |
| Pérdida por cuantización | −0,082 | −0,001 |

Con umbral 0,06 el INT8 de `cnn_media` alcanza **recall 0,932, precisión 0,852,
F1 0,890 y FPR 0,096** sobre cámaras nunca vistas de la misma región: las cuatro
metas de §5 del documento de diseño, por tesela. Es además tres veces más pequeño
y no necesita QAT.

Que una CNN separable de 149 k parámetros entrenada desde cero supere a un
MobileNetV2 preentrenado en ImageNet tiene sentido para esta tarea: las columnas de
humo sobre dosel no se parecen a las clases de ImageNet, así que el
preentrenamiento aporta poco, mientras que 135 561 parches sí alcanzan para
aprenderla directamente. Y sin bloques residuales lineales, el INT8 sale gratis.

## Por captura, que es como opera el nodo (`06_evaluar_por_cuadro.py`)

Sobre `sdis-77` (2 100 cuadros: 1 651 con humo, 449 limpios), exigiendo
FPR por captura ≤ 0,10:

| Regla de disparo | Umbral | Recall | Precisión | FPR |
|---|---|---|---|---|
| A · cualquier tesela | 0,98 | 0,135 | — | 0,089 |
| B · **≥ 2 teselas** | 0,73 | **0,227** | 0,895 | 0,098 |
| B · ≥ 3 teselas | 0,57 | 0,188 | 0,878 | 0,096 |
| C · 2 capturas consecutivas | 0,98 | 0,111 | 0,843 | 0,076 |
| C · 3 capturas consecutivas | 0,97 | 0,111 | 0,825 | 0,087 |

Sobre las cámaras de validación (mismo dominio) la mejor regla sube a recall
0,365 con FPR 0,097.

**La confirmación espacial ayuda; la temporal no.** Exigir dos teselas casi
duplica el recall a igual FPR. Exigir capturas consecutivas no mejora nada, y el
motivo es que los falsos positivos **no son transitorios**: son bancos de niebla,
nube baja sobre la cresta y bruma en el horizonte, que persisten entre capturas
igual que persistiría el humo. Esto **contradice el supuesto de §1 del documento
de diseño**, que da por hecho que el análisis multi-frame reduce la FPR; conviene
corregirlo antes de que se traslade al firmware.

## El número que decide: tiempo hasta detectar (`07_curva_operacion.py`)

Exigir `recall ≥ 0,90` **por captura** es la meta equivocada. REQ-SYS-001 y
REQ-SYS-008 piden detección temprana con latencia menor que la satelital, y un
incendio real persiste: con capturas cada 15 min, la probabilidad de haberlo
detectado tras *M* capturas es `1 − (1 − recall)^M`.

`cnn_media`, cámaras de validación (dominio conocido):

| Falsas alarmas/día por nodo | Recall por captura | 30 min | 1 h | 2 h | 3 h |
|---|---|---|---|---|---|
| 4,8 | 0,622 | 0,86 | 0,98 | 1,00 | 1,00 |
| 1,5 | 0,427 | 0,67 | 0,89 | 0,99 | 1,00 |
| **0,5** | 0,313 | 0,53 | 0,78 | **0,95** | **0,99** |

`cnn_media`, partner reservado `sdis-77` (otra región):

| Falsas alarmas/día por nodo | Recall por captura | 30 min | 1 h | 2 h | 3 h |
|---|---|---|---|---|---|
| 4,8 | 0,265 | 0,46 | 0,71 | 0,91 | 0,98 |
| 1,5 | 0,128 | 0,24 | 0,42 | 0,66 | 0,81 |
| 0,5 | 0,074 | 0,14 | 0,27 | 0,46 | 0,60 |

**Lectura.** Con media falsa alarma al día por nodo —lo máximo que un sistema de
aviso a comunidades sostiene sin perder credibilidad— el modelo detecta el **95 %
de los incendios dentro de las dos horas si el dominio es conocido**. El satélite
tarda de 1 a 40 h y la nubosidad de Loreto lo ciega durante días: en dominio
conocido, REQ-SYS-008 se cumple con holgura.

En una región nueva ese 95 % cae al 46 %. Y aquí está el dato que cierra el
argumento: en `cruzado`, `cnn_media` (0,074) y `mobilenetv2_035` (0,076) rinden
**igual**, pese a que en dominio conocido `cnn_media` es claramente superior. **La
brecha cruzada no es un problema de capacidad del modelo, es de dominio**, y por
tanto no se cierra mejorando la arquitectura. Es la justificación cuantitativa de
por qué recolectar imágenes en Paoyhan no es un refinamiento opcional sino el
trabajo que decide si el piloto funciona.

## Pendiente

Por orden de impacto sobre el resultado del piloto:

1. **Recolectar imágenes en Paoyhan.** Es lo único que cierra la brecha entre las
   dos tablas de arriba (88 % vs 47 % de detección a dos horas). Basta una cámara
   fija capturando cada 15 min durante la estación seca: da el dominio local, la
   niebla amazónica real y los negativos duros del sitio. El taller comunitario ya
   preparado (`fire_dataset/seleccion_taller_comunidad/`) encaja aquí como
   mecanismo de recolección participativa.
2. **Medir latencia y consumo por inferencia en el ESP32-S3** (EVT). Con 32
   teselas por captura y 48 capturas diarias son ~1 500 inferencias/día, un
   supuesto que `analisis-de-potencia.md` todavía no incluye. Sin ese dato no se
   cierra `ADR-001` ni el hito de las EETT fotovoltaicas.
3. **Recuperar los 8 puntos de la cuantización** con QAT o con una arquitectura
   sin cuellos de botella lineales.
4. **Corregir dos supuestos del documento de diseño**: la meta `FPR ≤ 0,10` es por
   tesela y no se traduce a la captura; y la confirmación temporal no reduce los
   falsos positivos de niebla, la espacial sí.
5. Evaluar entrada de **160×160** (permitida por §1 del diseño): con ventanas de
   224 px conserva más detalle de la columna y podría subir el recall a igual FPR.
