"""Recall por tesela a las FPR muy bajas que exige la rejilla de 32 teselas.

Con 32 teselas por captura y una sola de ellas conteniendo la columna de humo,
la relacion entre lo que mide `05_evaluar.py` y lo que ve el nodo es:

    recall_captura  ~= recall_tesela
    FPR_captura     ~= 1 - (1 - FPR_tesela) ^ 31

Para FPR_captura <= 0,10 hace falta FPR_tesela <= 0,0034; para 0,01, <= 0,00032.
Este script reporta el recall alcanzable en esos puntos, que es el numero que dice
cuanto le falta al modelo.
"""
import json, sys
from pathlib import Path
import numpy as np, tensorflow as tf
sys.path.insert(0, str(Path(__file__).parent))
RAIZ = Path(__file__).resolve().parent.parent

def predecir(ruta, X, lote=256):
    it = tf.lite.Interpreter(model_path=str(ruta))
    e = it.get_input_details()[0]
    it.resize_tensor_input(e["index"], [lote, 96, 96, 3]); it.allocate_tensors()
    e, s = it.get_input_details()[0], it.get_output_details()[0]
    out = []
    for i in range(0, len(X), lote):
        t = np.asarray(X[i:i+lote], np.float32); n = len(t)
        if n < lote: t = np.concatenate([t, np.zeros((lote-n,96,96,3), np.float32)])
        if e["dtype"] == np.int8:
            sc, z = e["quantization"]; t = np.clip(np.round(t/sc+z), -128, 127).astype(np.int8)
        it.set_tensor(e["index"], t); it.invoke()
        y = it.get_tensor(s["index"])[:n].reshape(-1)
        if s["dtype"] == np.int8:
            sc, z = s["quantization"]; y = (y.astype(np.float32)-z)*sc
        out.append(y)
    return np.concatenate(out)

modelo = sys.argv[1]
D = RAIZ/"datos"/"clasificacion-96"
res = {}
for part in ("val", "cruzado"):
    X = np.load(D/f"X_{part}.npy", mmap_mode="r"); y = np.load(D/f"y_{part}.npy")
    p = predecir(RAIZ/"modelos"/modelo/"modelo_float32.tflite", X)
    neg = np.sort(p[y == 0])[::-1]   # scores de los negativos, de mayor a menor
    pos = p[y == 1]
    fila = {}
    for fpr_obj in (0.10, 0.01, 0.0034, 0.001, 0.00032):
        k = int(fpr_obj * len(neg))
        umbral = float(neg[k]) if k < len(neg) else 1.0
        fila[fpr_obj] = {"umbral": round(umbral, 4),
                         "recall": round(float((pos >= umbral).mean()), 4)}
    res[part] = fila
    print(f"\n{part} ({len(y)} parches, {int(y.sum())} humo)")
    print(f"{'FPR tesela':>12} {'FPR captura':>12} {'umbral':>8} {'recall':>8}")
    for f, v in fila.items():
        print(f"{f:>12.5f} {1-(1-f)**31:>12.3f} {v['umbral']:>8.3f} {v['recall']:>8.3f}")
(RAIZ/"modelos"/modelo/"curva_operacion.json").write_text(json.dumps(res, indent=2))

# ---------------------------------------------------------------- tiempo hasta detectar
# REQ-SYS-008 pide latencia menor que la satelital (1 a 40 h, mas los dias que la
# nubosidad de Loreto bloquea la vista), no un recall por captura. Un incendio real
# persiste: con capturas cada 15 min, la probabilidad de haberlo detectado tras M
# capturas es 1 - (1 - recall)^M. Esta es la metrica que decide si el nodo cumple.
CADENCIA_MIN = 15
print("\n\nTiempo hasta la primera deteccion (cadencia de 15 min)")
for part in res:
    print(f"\n{part}")
    print(f"{'FPR captura':>12} {'falsas/dia':>11} {'recall':>7} " +
          " ".join(f"{m*CADENCIA_MIN:>5d}min" for m in (1, 2, 4, 8, 12)))
    for f, v in res[part].items():
        fpr_captura = 1 - (1 - f) ** 31
        r = v["recall"]
        acum = " ".join(f"{1-(1-r)**m:>8.2f}" for m in (1, 2, 4, 8, 12))
        print(f"{fpr_captura:>12.3f} {fpr_captura*48:>11.1f} {r:>7.3f} {acum}")
