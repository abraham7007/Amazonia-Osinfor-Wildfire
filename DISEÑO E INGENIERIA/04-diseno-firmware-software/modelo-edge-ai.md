# Modelo Edge AI / TinyML — Detección de columnas de humo
### Dataset, entrenamiento, métricas y despliegue por versión

| Estado | Versión | Fecha | Responsable |
|---|---|---|---|
| En curso | 1.0 | 2026-09-05 | Área Técnica DSFFFS |

> **v1.0 — primera línea base entrenada.** La implementación está en
> `AMAZONIA-OSINFOR/MODELO-TINYML/`, con resultados medidos en su `README.md`.
> Tres correcciones al plan original están recogidas en la **sección 8**; léase
> antes que las secciones 1 a 5, que quedan como estaban salvo esas enmiendas.

> **Propósito.** Definir cómo se entrena y despliega el modelo de detección de **columnas de
> humo** en cada versión de nodo (V1/V2/V3), y cómo se mide su desempeño. Es insumo de la
> decisión `ADR-001` (la energía no diferencia V1/V2; el **modelo** sí).

## 1. Tarea y enfoque
- **Tarea:** detección/clasificación de **columna de humo** en imagen diurna (objetivo
  primario). El humo es el primer signo visible de día [R02][R04].
- **Condición crítica (Amazonía):** **falsos positivos** por niebla matinal, nubes bajas y
  neblina sobre dosel. Mitigación: análisis **multi-frame/temporal** — los modelos secuenciales
  mejoran el recall manteniendo precisión [R03][R16].
- **Resolución de entrada:** 96×96 o 160×160 en escala de grises para TinyML [R12].

## 2. Datasets de referencia
| Dataset | Contenido | Uso | Fuente |
|---|---|---|---|
| **PyroNear-2025** | ~50 000 imágenes, ~150 000 anotaciones, 640 incendios; imágenes y **video** | Entrenamiento/benchmark; F1 ~60 % (difícil) | [R03] |
| **FIgLib (HPWREN)** | ~24 800 imágenes de cámaras fijas en montaña | Entrenamiento; modelo de referencia SmokeyNet | [R16] |
| **Boreal Forest Fire (UAV)** | Detección + segmentación de humo desde dron | Aumentar variedad de vistas | [R18] |
| **Dataset local Amazonía** | A construir en campo (Pahoyan) | *Fine-tuning* y reducción de falsos positivos con niebla local | — |

## 3. Arquitectura por versión (despliegue)
| Versión | Modelo | Framework / cuantización | Notas |
|---|---|---|---|
| **V1 · MCU TinyML** | CNN pequeña (clasificación humo/no-humo) | TFLite-Micro **INT8 (PTQ)** con TF Model Optimization Toolkit [R19][R20] | Cabe en flash/RAM del ESP32-S3; medir latencia on-device |
| **V2 · IA on-sensor** | Detector ligero on-sensor (Himax WiseEye2 / Sony IMX500) | Cadena del fabricante (Himax/Edge Impulse · IMX500/Sony) | La IA vive en el sensor → MCU casi en sleep |
| **V3 · NPU** | Detector tipo YOLO-tiny / objeto | Compilado para **Hailo-8** | Mayor precisión; benchmark de referencia |

## 4. Entrenamiento
1. **Preparación:** unificar PyroNear+FIgLib, balancear humo/no-humo, **incluir negativos
   difíciles** (niebla/nube/amanecer) para bajar la FPR.
2. **Aumentado:** brillo, neblina sintética, recortes, rotaciones suaves.
3. **Transferencia:** backbone ligero (p. ej. MobileNet/efficient-lite) → *fine-tuning*.
4. **Cuantización:** PTQ **INT8** y medición de exactitud antes/después [R19].
5. **Temporal (opcional V2/V3):** secuencia de N frames para confirmar columna persistente [R03].

## 5. Métricas
| Métrica | Por qué | Meta inicial (a refinar) |
|---|---|---|
| **Recall (TPR)** | No perder incendios reales | ≥ 0,90 |
| **Precisión** | Limitar falsas alarmas | ≥ 0,80 |
| **F1** | Balance | ≥ 0,75 |
| **FPR (falsos positivos)** | Determinante con niebla amazónica | ≤ 0,10 |
| **Latencia on-device** | Viabilidad en MCU | medir (ms) |
| **Flash / RAM** | Cabida en el target | medir |

> Referencia de literatura: sistemas reportan ~89,6 % recall, ~82,9 % precisión y ~7,5 % FPR
> [R22]; PyroNear (modelo ligero) ~60 % F1 cruzado — la tarea presenta dificultad considerable [R03].

## 6. Validación
- **Cruzada** entre datasets (entrenar en uno, probar en otro) para medir generalización [R03].
- **En campo (Pahoyan):** medir FPR real con niebla matinal antes de fijar umbrales de alarma.
- El umbral de `confidence` que dispara la alerta LoRa se fija aquí (ver `protocolo-lorawan.md`).

## 7. Trazabilidad
REQ-SYS-001 (detección Edge AI), REQ-SYS-002 (procesamiento local), REQ-SYS-008 (latencia).
Resultados → `comparacion-versiones.md` y decisión `ADR-001`.

---

## 8. Enmiendas tras el primer entrenamiento (2026-09-05)

Implementación y cifras completas: `AMAZONIA-OSINFOR/MODELO-TINYML/README.md`.

### 8.1 El nodo debe recorrer el cuadro por teselas, no clasificar el cuadro completo

Medido sobre las anotaciones de PyroNear-2025 [R03] en cuadros de 1280×720: la
columna de humo mediana ocupa **3,2 % del ancho y 4,5 % del alto** (~41 × 32 px);
el percentil 90 no llega al 10 %. Reducir el cuadro completo a 96×96 —lectura
natural de la §1— deja la columna en **3 × 2 px**, por debajo de lo detectable.

**Enmienda a §1.** El nodo recorta ventanas de **224 × 224 px** del cuadro
original, las reduce a 96×96 e infiere sobre una rejilla: **24 teselas** sin
solape o **32 con 25 %**. La entrada al modelo sigue siendo 96×96 según §1; lo que
cambia es que hay una inferencia por tesela y no una por captura.

**Impacto en energía.** Con la cadencia diurna de 15 min de `cadencia-captura.md`
§5 son 48 capturas/día × 32 teselas ≈ **1 500 inferencias diarias**, supuesto que
`analisis-de-potencia.md` todavía no contempla. Debe medirse en EVT antes de
cerrar `ADR-001`.

### 8.2 La meta «FPR ≤ 0,10» de §5 es por tesela y no se traduce a la captura

El nodo alerta si alguna tesela supera el umbral, así que
`FPR_captura = 1 − (1 − FPR_tesela)^32`. Una FPR por tesela de 0,10 produce una
falsa alarma en el **97 % de las capturas**. Para que la captura cumpla 0,10, la
FPR por tesela debe bajar a **~0,0034**; para 0,01, a **~0,00032**.

**Enmienda a §5.** Las metas de la tabla se declaran explícitamente **por tesela**,
y se añade una meta operativa por captura: **≤ 0,5 falsas alarmas por nodo y día**
(FPR por captura ≈ 0,01), que es lo que un sistema de aviso a comunidades sostiene
sin perder credibilidad.

### 8.3 La confirmación temporal no reduce los falsos positivos; la espacial sí

§1 propone el análisis multi-frame como mitigación de la niebla. Medido sobre el
partner reservado, exigiendo FPR por captura ≤ 0,10:

| Regla | Recall |
|---|---|
| Cualquier tesela | 0,135 |
| **≥ 2 teselas en la misma captura** | **0,227** |
| 2 capturas consecutivas | 0,111 |
| 3 capturas consecutivas | 0,111 |

La confirmación espacial casi duplica el recall; la temporal no aporta. El motivo
es que los falsos positivos **no son transitorios**: bancos de niebla, nube baja
sobre la cresta y bruma en el horizonte persisten entre capturas igual que
persistiría una columna de humo. Con la cadencia de 15 min del nodo la situación
solo empeora, porque la niebla dura horas.

**Enmienda a §1 y §4.5.** El firmware implementa **confirmación espacial de K = 2
teselas**; el análisis temporal se retira como mitigación de falsos positivos.

### 8.4 La métrica que decide es el tiempo hasta detectar, no el recall por captura

REQ-SYS-008 pide latencia menor que la satelital (1 a 40 h, más los días que la
nubosidad de Loreto bloquea la vista), no un recall instantáneo. Un incendio
persiste, así que la probabilidad de haberlo detectado tras *M* capturas es
`1 − (1 − recall)^M`. Con **0,5 falsas alarmas/día por nodo**:

| Conjunto (`cnn_media` INT8) | 1 h | 2 h | 3 h |
|---|---|---|---|
| Cámaras nuevas, **misma región** | 0,78 | **0,95** | 0,99 |
| Partner reservado, **otra región** | 0,27 | 0,46 | 0,60 |

**En dominio conocido REQ-SYS-008 se cumple con holgura.** En una región nueva no.
Toda la diferencia es el ajuste al dominio local.

**Enmienda a §5.** Se añade el tiempo hasta la primera detección como métrica
principal de aceptación, con meta **≥ 0,85 de detección dentro de 2 h a ≤ 0,5
falsas alarmas/día por nodo**.

### 8.5 El dataset local de Paoyhan pasa de opcional a crítico

La tabla de §8.4 cuantifica lo que §2 anotaba como *fine-tuning* deseable: la
diferencia entre 0,95 y 0,46 de detección a dos horas es exactamente la brecha de
dominio.

Y hay evidencia de que **no se cierra mejorando el modelo**: en el partner
reservado, `cnn_media` (recall 0,074) y `mobilenetv2_035` (0,076) rinden igual,
pese a que en dominio conocido el primero es claramente superior. Cambiar de
arquitectura mueve el resultado en dominio conocido y no lo mueve fuera de él.

**Recolectar imágenes en Paoyhan es, por tanto, la actividad de mayor impacto
sobre el resultado del piloto**, por encima de cualquier mejora de arquitectura.
Basta una cámara fija capturando cada 15 min durante la estación seca para obtener
el dominio local, la niebla amazónica real y los negativos duros del sitio.

### 8.6 Nota sobre la cuantización INT8

MobileNetV2 α=0,35 pierde ~8 puntos de F1 al pasar a INT8 (0,840 → 0,758 en
validación). No es un problema de calibración: subir el conjunto representativo de
400 a 3 000 muestras no cambió nada. Es el rango dinámico de sus cuellos de botella
lineales.

**Enmienda a §3.** Para V1 se sustituye la CNN basada en MobileNet por una **CNN
separable de 148 929 parámetros sin bloques residuales lineales** (`cnn_media` en
`MODELO-TINYML/scripts/03_entrenar.py`), que gana en las dos dimensiones a la vez:

| | `mobilenetv2_035` | **`cnn_media`** |
|---|---|---|
| Parámetros | 411 489 | 148 929 |
| Tamaño INT8 | 606 KiB | **198 KiB** |
| AUC-PR validación | 0,930 | **0,963** |
| `val` INT8 — recall / precisión / F1 / FPR | 0,697 / 0,831 / 0,758 / 0,084 | **0,932 / 0,852 / 0,890 / 0,096** |
| Pérdida por cuantización (F1) | −0,082 | **−0,001** |
| Metas de §5 por tesela | no cumple | **cumple las cuatro** |

Con umbral 0,06, el INT8 de `cnn_media` cumple **las cuatro metas de §5 por
tesela** sobre cámaras nunca vistas de la misma región. El preentrenamiento en
ImageNet aporta poco aquí —las columnas de humo sobre dosel no se parecen a sus
clases— mientras que los 135 561 parches de entrenamiento sí alcanzan para
aprender la tarea desde cero; y sin bloques residuales lineales la cuantización
sale gratis, sin necesidad de QAT.

**Consecuencia para `ADR-001`:** una red de 10,7 k parámetros —la que cabría en la
SRAM interna del ESP32-S3 sin PSRAM— alcanza AUC-PR 0,58 frente a 0,96. **V1 debe
planificarse con PSRAM**; la variante sin PSRAM no resuelve la tarea.

### 8.7 `Dataset-Fire` no es utilizable para entrenar este modelo

Sus positivos son llamas en primer plano y sus negativos son paisajes sin relación
con la escena de despliegue (hojas de otoño, nieve, faros). No contiene una sola
imagen de niebla sobre bosque etiquetada como negativa, que es el caso difícil que
§1 identifica. Se entrenó con **PyroNear-SDIS** [R03], de cámaras fijas de
vigilancia. `Dataset-Fire` conserva su uso para el taller comunitario.
