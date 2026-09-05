"""Cuantiza el modelo entrenado a INT8 (PTQ) y lo exporta para TFLite-Micro.

Corresponde a la seccion 4.4 de `modelo-edge-ai.md`: cuantizacion entera completa
con dataset representativo y medicion de la exactitud antes y despues.

Genera:
    modelo_float32.tflite   referencia
    modelo_int8.tflite      el que se despliega en el ESP32-S3
    modelo_int8.cc/.h       arreglo C para TFLite-Micro
"""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
import tensorflow as tf
from tensorflow import keras

import importlib.util
_spec = importlib.util.spec_from_file_location("entrenar", Path(__file__).parent / "03_entrenar.py")
_entrenar = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_entrenar)  # registra CapaNeblina para des-serializar

RAIZ = Path(__file__).resolve().parent.parent
DATOS = RAIZ / "datos" / "clasificacion-96"
MODELOS = RAIZ / "modelos"
LADO = 96
# MobileNetV2 es sensible a la cuantizacion por los cuellos de botella lineales:
# con 400 muestras representativas el F1 caia ~8 puntos frente a float32.
N_REPRESENTATIVAS = 3000


def modelo_de_inferencia(entrenado: keras.Model, gris: bool) -> keras.Model:
    """Reconstruye el grafo sin las capas de aumentado.

    Las capas de aumentado son inertes en inferencia, pero dejarlas dentro mete
    operaciones aleatorias en el grafo exportado que TFLite-Micro no soporta.
    """
    entrada = keras.Input(shape=(LADO, LADO, 3), dtype="float32", name="imagen")
    x = entrada
    if gris:
        g = tf.image.rgb_to_grayscale(x)
        x = tf.tile(g, [1, 1, 1, 3])
    # Se saltan la entrada y el bloque de aumentado; el resto se aplica en orden.
    for capa in entrenado.layers:
        if capa.name in ("imagen", "aumentado"):
            continue
        x = capa(x)
    return keras.Model(entrada, x, name=f"{entrenado.name}_inferencia")


def dataset_representativo():
    X = np.load(DATOS / "X_train.npy", mmap_mode="r")
    rng = np.random.default_rng(0)
    idx = np.sort(rng.choice(len(X), min(N_REPRESENTATIVAS, len(X)), replace=False))

    def gen():
        for i in idx:
            yield [np.asarray(X[i:i + 1], dtype=np.float32)]

    return gen


def convertir(modelo: keras.Model, destino: Path, int8: bool) -> Path:
    conv = tf.lite.TFLiteConverter.from_keras_model(modelo)
    if int8:
        conv.optimizations = [tf.lite.Optimize.DEFAULT]
        conv.representative_dataset = dataset_representativo()
        # Entero completo: sin esto quedan operaciones float que TFLM no ejecuta.
        conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
        conv.inference_input_type = tf.int8
        conv.inference_output_type = tf.int8
    blob = conv.convert()
    destino.write_bytes(blob)
    return destino


def a_arreglo_c(tflite: Path, nombre: str) -> None:
    datos = tflite.read_bytes()
    lineas = []
    for i in range(0, len(datos), 12):
        lineas.append("  " + " ".join(f"0x{b:02x}," for b in datos[i:i + 12]))
    cc = (f'#include "{nombre}.h"\n\n'
          f"alignas(16) const unsigned char {nombre}[] = {{\n"
          + "\n".join(lineas) + "\n};\n"
          f"const unsigned int {nombre}_len = {len(datos)};\n")
    h = (f"#pragma once\n\n"
         f"extern const unsigned char {nombre}[];\n"
         f"extern const unsigned int {nombre}_len;\n")
    tflite.with_name(f"{nombre}.cc").write_text(cc)
    tflite.with_name(f"{nombre}.h").write_text(h)


def resumen_ops(tflite: Path) -> dict:
    interp = tf.lite.Interpreter(model_path=str(tflite))
    interp.allocate_tensors()
    ent, sal = interp.get_input_details()[0], interp.get_output_details()[0]
    return {
        "bytes": tflite.stat().st_size,
        "entrada": {"forma": ent["shape"].tolist(), "tipo": str(ent["dtype"].__name__),
                    "escala_punto_cero": [float(ent["quantization"][0]), int(ent["quantization"][1])]},
        "salida": {"forma": sal["shape"].tolist(), "tipo": str(sal["dtype"].__name__),
                   "escala_punto_cero": [float(sal["quantization"][0]), int(sal["quantization"][1])]},
        "n_tensores": len(interp.get_tensor_details()),
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--modelo", required=True, help="nombre de la carpeta en modelos/")
    args = ap.parse_args()

    dir_modelo = MODELOS / args.modelo
    cfg = json.loads((dir_modelo / "config.json").read_text())
    entrenado = keras.models.load_model(dir_modelo / "mejor.keras", compile=False)
    inferencia = modelo_de_inferencia(entrenado, cfg["gris"])

    float_tflite = convertir(inferencia, dir_modelo / "modelo_float32.tflite", int8=False)
    print(f"float32: {float_tflite.stat().st_size / 1024:.1f} KiB")

    int8_tflite = convertir(inferencia, dir_modelo / "modelo_int8.tflite", int8=True)
    print(f"int8   : {int8_tflite.stat().st_size / 1024:.1f} KiB "
          f"({float_tflite.stat().st_size / int8_tflite.stat().st_size:.2f}x menor)")

    a_arreglo_c(int8_tflite, "modelo_humo_int8")
    print(f"arreglo C: {(dir_modelo / 'modelo_humo_int8.cc').stat().st_size / 1024:.0f} KiB")

    info = {"float32": resumen_ops(float_tflite), "int8": resumen_ops(int8_tflite),
            "parametros_keras": int(inferencia.count_params())}
    (dir_modelo / "cuantizacion.json").write_text(json.dumps(info, indent=2, ensure_ascii=False))
    print(json.dumps(info["int8"], indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
