"""Evalua el modelo contra las metas de la seccion 5 de `modelo-edge-ai.md`.

    recall >= 0,90 · precision >= 0,80 · F1 >= 0,75 · FPR <= 0,10

Compara Keras float y TFLite INT8 sobre dos conjuntos:
  val      camaras distintas a las de entrenamiento (mismos partners)
  cruzado  partner sdis-77 completo, nunca visto — generalizacion cruzada

Ademas elige el umbral de operacion, que es el que alimenta el campo
`confidence` de la trama LoRa (`protocolo-lorawan.md`, seccion 3.1).
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import tensorflow as tf
from tensorflow import keras

RAIZ = Path(__file__).resolve().parent.parent
DATOS = RAIZ / "datos" / "clasificacion-96"
MODELOS = RAIZ / "modelos"

METAS = {"recall": 0.90, "precision": 0.80, "f1": 0.75, "fpr": 0.10}


def predecir_tflite(ruta: Path, X, lote: int = 256) -> np.ndarray:
    interp = tf.lite.Interpreter(model_path=str(ruta))
    ent = interp.get_input_details()[0]
    sal = interp.get_output_details()[0]
    interp.resize_tensor_input(ent["index"], [lote, *ent["shape"][1:]])
    interp.allocate_tensors()
    ent = interp.get_input_details()[0]
    sal = interp.get_output_details()[0]

    esc_e, cero_e = ent["quantization"]
    esc_s, cero_s = sal["quantization"]
    salidas = []
    for i in range(0, len(X), lote):
        trozo = np.asarray(X[i:i + lote], dtype=np.float32)
        if len(trozo) < lote:  # ultimo lote: rellena y descarta despues
            relleno = np.zeros((lote - len(trozo), *trozo.shape[1:]), np.float32)
            trozo_p = np.concatenate([trozo, relleno])
        else:
            trozo_p = trozo
        if ent["dtype"] == np.int8:
            trozo_p = np.clip(np.round(trozo_p / esc_e + cero_e), -128, 127).astype(np.int8)
        interp.set_tensor(ent["index"], trozo_p)
        interp.invoke()
        y = interp.get_tensor(sal["index"])[:len(trozo)]
        if sal["dtype"] == np.int8:
            y = (y.astype(np.float32) - cero_s) * esc_s
        salidas.append(y.reshape(-1))
    return np.concatenate(salidas)


def metricas(y, p, umbral: float) -> dict:
    pred = (p >= umbral).astype(np.uint8)
    tp = int(((pred == 1) & (y == 1)).sum())
    fp = int(((pred == 1) & (y == 0)).sum())
    fn = int(((pred == 0) & (y == 1)).sum())
    tn = int(((pred == 0) & (y == 0)).sum())
    recall = tp / max(1, tp + fn)
    precision = tp / max(1, tp + fp)
    f1 = 2 * precision * recall / max(1e-9, precision + recall)
    fpr = fp / max(1, fp + tn)
    return {"umbral": round(float(umbral), 4), "tp": tp, "fp": fp, "fn": fn, "tn": tn,
            "recall": round(recall, 4), "precision": round(precision, 4),
            "f1": round(f1, 4), "fpr": round(fpr, 4),
            "exactitud": round((tp + tn) / max(1, len(y)), 4)}


def umbral_operacion(y, p, fpr_max: float = METAS["fpr"]) -> float:
    """Umbral mas bajo (mayor recall) que aun respeta la meta de FPR."""
    candidatos = np.unique(np.round(np.linspace(0.01, 0.99, 197), 4))
    viables = [(t, metricas(y, p, t)) for t in candidatos]
    ok = [(t, m) for t, m in viables if m["fpr"] <= fpr_max]
    if not ok:
        return max(viables, key=lambda tm: tm[1]["f1"])[0]
    return max(ok, key=lambda tm: tm[1]["recall"])[0]


def veredicto(m: dict) -> str:
    fallos = [k for k, meta in METAS.items()
              if (m[k] < meta if k != "fpr" else m[k] > meta)]
    return "CUMPLE" if not fallos else "NO CUMPLE: " + ", ".join(fallos)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--modelo", required=True)
    ap.add_argument("--particiones", nargs="+", default=["val", "cruzado"])
    args = ap.parse_args()

    dir_modelo = MODELOS / args.modelo
    keras_modelo = None
    informe = {"modelo": args.modelo, "metas": METAS, "resultados": {}}

    for part in args.particiones:
        X = np.load(DATOS / f"X_{part}.npy", mmap_mode="r")
        y = np.load(DATOS / f"y_{part}.npy")
        print(f"\n{'=' * 78}\n{part}: {len(y)} parches "
              f"({int(y.sum())} humo / {int((y == 0).sum())} no-humo)\n{'=' * 78}")
        informe["resultados"][part] = {}

        for etiqueta, ruta in (("float32", dir_modelo / "modelo_float32.tflite"),
                               ("int8", dir_modelo / "modelo_int8.tflite")):
            if not ruta.exists():
                continue
            p = predecir_tflite(ruta, X)
            t = umbral_operacion(y, p)
            m05 = metricas(y, p, 0.5)
            mop = metricas(y, p, t)
            informe["resultados"][part][etiqueta] = {"umbral_0.5": m05, "operacion": mop}
            print(f"\n  [{etiqueta}]")
            print(f"    umbral 0,50 -> recall {m05['recall']:.3f} · precision "
                  f"{m05['precision']:.3f} · F1 {m05['f1']:.3f} · FPR {m05['fpr']:.3f}")
            print(f"    umbral {t:.2f}  -> recall {mop['recall']:.3f} · precision "
                  f"{mop['precision']:.3f} · F1 {mop['f1']:.3f} · FPR {mop['fpr']:.3f}"
                  f"   [{veredicto(mop)}]")

    destino = dir_modelo / "evaluacion.json"
    destino.write_text(json.dumps(informe, indent=2, ensure_ascii=False))
    print(f"\nInforme en {destino}")


if __name__ == "__main__":
    main()
