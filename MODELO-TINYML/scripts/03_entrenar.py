"""Entrena el clasificador humo / no-humo 96x96 para el nodo V1 (ESP32-S3).

Sigue la seccion 4 de `DISENO E INGENIERIA/04-diseno-firmware-software/modelo-edge-ai.md`:
transferencia desde un backbone ligero, aumentado con neblina sintetica y
negativos duros, y metricas orientadas a la tasa de falsos positivos.

Uso
---
    .venv/bin/python scripts/03_entrenar.py --arquitectura mobilenetv2_035
    .venv/bin/python scripts/03_entrenar.py --arquitectura cnn_pequena --gris
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime
from pathlib import Path

import keras
import numpy as np
import tensorflow as tf
from keras import layers

RAIZ = Path(__file__).resolve().parent.parent
DATOS = RAIZ / "datos" / "clasificacion-96"
MODELOS = RAIZ / "modelos"
LADO = 96
SEMILLA = 42


# --------------------------------------------------------------------------- datos
def cargar(particion: str):
    X = np.load(DATOS / f"X_{particion}.npy", mmap_mode="r")
    y = np.load(DATOS / f"y_{particion}.npy")
    return X, y


def hacer_dataset(X, y, lote: int, entrenamiento: bool, gris: bool):
    """tf.data sobre el memmap: nunca carga las particiones enteras en RAM."""
    n = len(y)
    pasada = [0]  # `repeat()` reinvoca el generador: cuenta las pasadas para
                  # rebarajar distinto en cada epoca sin perder reproducibilidad.
    TROZO = 8     # muestras contiguas por lectura

    def generador():
        rng = np.random.default_rng(SEMILLA + pasada[0])
        pasada[0] += 1
        # Barajar indice a indice sobre los 3,7 GB del memmap obliga a traer el
        # archivo entero a cache de paginas y el proceso muere por memoria. Se
        # barajan trozos contiguos: cada lote toma 8 trozos de 8 muestras, lo que
        # mantiene la localidad de lectura y sigue mezclando ocho regiones distintas.
        trozos = np.arange(0, n, TROZO)
        if entrenamiento:
            rng.shuffle(trozos)
        por_lote = max(1, lote // TROZO)
        for i in range(0, len(trozos), por_lote):
            inicios = np.sort(trozos[i:i + por_lote])
            idx = np.concatenate([np.arange(s, min(s + TROZO, n)) for s in inicios])
            yield np.asarray(X[idx], dtype=np.uint8), y[idx].astype(np.float32)

    ds = tf.data.Dataset.from_generator(
        generador,
        output_signature=(
            tf.TensorSpec(shape=(None, LADO, LADO, 3), dtype=tf.uint8),
            tf.TensorSpec(shape=(None,), dtype=tf.float32),
        ),
    )
    if entrenamiento:
        # Buffer corto: con 64 lotes en cola son ~450 MB de mas tras el casteo.
        ds = ds.repeat().shuffle(12, seed=SEMILLA)
    if gris:
        ds = ds.map(a_gris, num_parallel_calls=tf.data.AUTOTUNE)
    # El casteo va aqui y no en una capa Lambda: una Lambda con funcion anonima
    # no se puede volver a deserializar ni exportar a TFLite.
    ds = ds.map(lambda x, y: (tf.cast(x, tf.float32), y),
                num_parallel_calls=tf.data.AUTOTUNE)
    return ds.prefetch(tf.data.AUTOTUNE)


def a_gris(x, y):
    g = tf.image.rgb_to_grayscale(x)
    return tf.cast(tf.tile(g, [1, 1, 1, 3]), tf.uint8), y


# ----------------------------------------------------------------- preprocesamiento
def bloque_aumentado():
    """Aumentado de la seccion 4.2: brillo, neblina sintetica, recortes, rotaciones suaves.

    Sin volteo vertical: el cielo siempre esta arriba y la columna de humo sube.
    """
    return keras.Sequential([
        layers.RandomFlip("horizontal", seed=SEMILLA),
        layers.RandomRotation(0.03, fill_mode="reflect", seed=SEMILLA),
        layers.RandomZoom(0.15, fill_mode="reflect", seed=SEMILLA),
        layers.RandomTranslation(0.08, 0.08, fill_mode="reflect", seed=SEMILLA),
        layers.RandomBrightness(0.25, value_range=(0.0, 255.0), seed=SEMILLA),
        layers.RandomContrast(0.25, seed=SEMILLA),
        CapaNeblina(seed=SEMILLA),
    ], name="aumentado")


@keras.saving.register_keras_serializable(package="amazonia")
class CapaNeblina(layers.Layer):
    """Neblina sintetica: mezcla la imagen con un velo claro de intensidad aleatoria.

    Reproduce la niebla matinal y la nube baja sobre el dosel, que es la causa
    principal de falsos positivos en la Amazonia (`modelo-edge-ai.md` seccion 1).
    """

    def __init__(self, intensidad_max: float = 0.45, prob: float = 0.35,
                 seed: int | None = None, **kwargs):
        super().__init__(**kwargs)
        self.intensidad_max = intensidad_max
        self.prob = prob
        self.seed = seed
        self._rng = tf.random.Generator.from_seed(seed if seed is not None else 0)

    def call(self, x, training=None):
        if not training:
            return x
        lote = tf.shape(x)[0]
        aplicar = tf.cast(self._rng.uniform([lote, 1, 1, 1]) < self.prob, x.dtype)
        alfa = self._rng.uniform([lote, 1, 1, 1], 0.0, self.intensidad_max) * aplicar
        velo = self._rng.uniform([lote, 1, 1, 1], 180.0, 255.0)
        return x * (1.0 - alfa) + velo * alfa

    def get_config(self):
        return {**super().get_config(), "intensidad_max": self.intensidad_max,
                "prob": self.prob, "seed": self.seed}


# -------------------------------------------------------------------------- modelos
def construir_mobilenet(entrada, congelar: bool):
    base = keras.applications.MobileNetV2(
        input_shape=(LADO, LADO, 3), alpha=0.35, include_top=False,
        weights="imagenet", pooling="avg",
    )
    base.trainable = not congelar
    # MobileNetV2 espera [-1, 1]; la entrada llega en [0, 255] uint8.
    x = layers.Rescaling(1 / 127.5, offset=-1.0)(entrada)
    x = base(x)
    x = layers.Dropout(0.3)(x)
    salida = layers.Dense(1, activation="sigmoid", name="humo")(x)
    return salida, base


def construir_cnn_pequena(entrada, congelar: bool):
    """Respaldo que cabe en la SRAM interna del ESP32-S3, sin PSRAM."""
    x = layers.Rescaling(1 / 127.5, offset=-1.0)(entrada)
    for filtros in (16, 32, 64, 96):
        x = layers.SeparableConv2D(filtros, 3, padding="same", use_bias=False)(x)
        x = layers.BatchNormalization()(x)
        x = layers.ReLU(6.0)(x)
        x = layers.MaxPooling2D(2)(x)
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dropout(0.3)(x)
    return layers.Dense(1, activation="sigmoid", name="humo")(x), None


def construir_cnn_media(entrada, congelar: bool):
    """CNN separable sin cuellos de botella lineales, dimensionada para competir.

    `cnn_pequena` (10,7 k parametros) cuantiza sin perdida pero es demasiado debil;
    `mobilenetv2_035` es fuerte pero pierde ~8 puntos de F1 en INT8 por el rango
    dinamico de sus bloques residuales lineales. Esta arquitectura busca el punto
    intermedio: ~200 k parametros, activacion ReLU6 en todas las capas y ninguna
    salida lineal ancha, que es la condicion que hace amable la cuantizacion.
    """
    x = layers.Rescaling(1 / 127.5, offset=-1.0)(entrada)
    x = layers.Conv2D(32, 3, strides=2, padding="same", use_bias=False)(x)
    x = layers.BatchNormalization()(x)
    x = layers.ReLU(6.0)(x)

    for filtros, repeticiones in ((64, 1), (128, 2), (192, 2), (256, 1)):
        for _ in range(repeticiones):
            x = layers.SeparableConv2D(filtros, 3, padding="same", use_bias=False)(x)
            x = layers.BatchNormalization()(x)
            x = layers.ReLU(6.0)(x)
        x = layers.MaxPooling2D(2)(x)

    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dropout(0.35)(x)
    return layers.Dense(1, activation="sigmoid", name="humo")(x), None


ARQUITECTURAS = {"mobilenetv2_035": construir_mobilenet,
                 "cnn_pequena": construir_cnn_pequena,
                 "cnn_media": construir_cnn_media}


def construir(arquitectura: str, congelar: bool):
    entrada = keras.Input(shape=(LADO, LADO, 3), dtype="float32", name="imagen")
    x = bloque_aumentado()(entrada)
    salida, base = ARQUITECTURAS[arquitectura](x, congelar)
    return keras.Model(entrada, salida, name=arquitectura), base


# --------------------------------------------------------------------------- metricas
METRICAS = [
    keras.metrics.BinaryAccuracy(name="exactitud"),
    keras.metrics.Precision(name="precision"),
    keras.metrics.Recall(name="recall"),
    keras.metrics.AUC(name="auc_pr", curve="PR"),
]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--arquitectura", default="mobilenetv2_035", choices=list(ARQUITECTURAS))
    ap.add_argument("--gris", action="store_true",
                    help="convierte a escala de grises (replicada a 3 canales)")
    ap.add_argument("--lote", type=int, default=64)
    ap.add_argument("--epocas-cabeza", type=int, default=4)
    ap.add_argument("--epocas-ajuste", type=int, default=12)
    ap.add_argument("--etiqueta", default="")
    ap.add_argument("--pasos-max", type=int, default=0,
                    help="limita los pasos por epoca (prueba rapida)")
    args = ap.parse_args()

    keras.utils.set_random_seed(SEMILLA)
    MODELOS.mkdir(exist_ok=True)
    nombre = args.etiqueta or f"{args.arquitectura}{'_gris' if args.gris else ''}"
    dir_salida = MODELOS / nombre
    dir_salida.mkdir(parents=True, exist_ok=True)

    Xtr, ytr = cargar("train")
    Xva, yva = cargar("val")
    print(f"train {Xtr.shape} ({int(ytr.sum())} humo) | val {Xva.shape} ({int(yva.sum())} humo)")

    pasos_tr = int(np.ceil(len(ytr) / args.lote))
    if args.pasos_max:
        pasos_tr = min(pasos_tr, args.pasos_max)
    ds_tr = hacer_dataset(Xtr, ytr, args.lote, True, args.gris)
    ds_va = hacer_dataset(Xva, yva, args.lote, False, args.gris)
    if args.pasos_max:
        ds_va = ds_va.take(max(1, args.pasos_max // 4))

    # Pesos de clase: el negativo es mayoritario y ademas es la clase cuyo error
    # (falso positivo) mas cuesta en campo.
    n_pos, n_neg = int(ytr.sum()), int((ytr == 0).sum())
    pesos = {0: len(ytr) / (2 * n_neg), 1: len(ytr) / (2 * n_pos)}
    print(f"pesos de clase: {pesos}")

    modelo, base = construir(args.arquitectura, congelar=True)
    modelo.summary(line_length=100)

    callbacks = [
        keras.callbacks.ModelCheckpoint(dir_salida / "mejor.keras", monitor="val_auc_pr",
                                        mode="max", save_best_only=True, verbose=1),
        keras.callbacks.EarlyStopping(monitor="val_auc_pr", mode="max", patience=4,
                                      restore_best_weights=True, verbose=1),
        keras.callbacks.CSVLogger(dir_salida / "historial.csv", append=True),
    ]

    historial = {}
    if base is not None and args.epocas_cabeza > 0:
        print("\n=== Fase 1: cabeza (backbone congelado) ===")
        modelo.compile(optimizer=keras.optimizers.Adam(1e-3),
                       loss="binary_crossentropy", metrics=METRICAS)
        h1 = modelo.fit(ds_tr, steps_per_epoch=pasos_tr, epochs=args.epocas_cabeza,
                        validation_data=ds_va, class_weight=pesos, callbacks=callbacks)
        historial["cabeza"] = {k: [float(v) for v in vs] for k, vs in h1.history.items()}
        base.trainable = True
        # Congela la mitad inferior: las primeras capas son bordes y texturas genericos.
        for capa in base.layers[:len(base.layers) // 2]:
            capa.trainable = False

    # 1e-4 es la tasa del ajuste fino sobre un backbone preentrenado; una red que
    # parte de pesos aleatorios necesita un orden de magnitud mas o no converge.
    hay_preentrenado = base is not None
    tasa = 1e-4 if hay_preentrenado else 1e-3
    print(f"\n=== Fase 2: {'ajuste fino' if hay_preentrenado else 'entrenamiento'} "
          f"(lr={tasa:g}) ===")
    programa = keras.optimizers.schedules.CosineDecay(
        tasa, decay_steps=pasos_tr * args.epocas_ajuste, alpha=0.05)
    modelo.compile(optimizer=keras.optimizers.Adam(programa),
                   loss="binary_crossentropy", metrics=METRICAS)
    h2 = modelo.fit(ds_tr, steps_per_epoch=pasos_tr, epochs=args.epocas_ajuste,
                    validation_data=ds_va, class_weight=pesos, callbacks=callbacks)
    historial["ajuste"] = {k: [float(v) for v in vs] for k, vs in h2.history.items()}

    modelo.save(dir_salida / "final.keras")
    (dir_salida / "config.json").write_text(json.dumps({
        "arquitectura": args.arquitectura, "gris": args.gris, "lado": LADO,
        "lote": args.lote, "pesos_clase": pesos, "semilla": SEMILLA,
        "parametros": int(modelo.count_params()),
        "fecha": datetime.now().isoformat(timespec="seconds"),
        "historial": historial,
    }, indent=2, ensure_ascii=False))
    print(f"\nModelo guardado en {dir_salida}")


if __name__ == "__main__":
    main()
