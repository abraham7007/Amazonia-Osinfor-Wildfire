"""Separa los negativos por procedencia para medir cuanta etiqueta ruidosa hay.

La regla de muestreo marcaba como negativa cualquier ventana sin solape con una
caja anotada. Pero el penacho es difuso: su cola y las columnas secundarias caen
fuera de la caja y quedan etiquetadas 'no-humo'. Este script mide la FPR por
separado en:

  duro   ventanas de cuadros QUE SI tienen humo (sospechosas de ruido)
  limpio ventanas de cuadros SIN ninguna anotacion (negativos de confianza)

Si la FPR del grupo limpio es mucho menor, la FPR global esta inflada por la
etiqueta y no por confusion con niebla.
"""
import io, sys
from pathlib import Path
import numpy as np, pyarrow.parquet as pq, tensorflow as tf
from PIL import Image

sys.path.insert(0, str(Path(__file__).parent))
RAIZ = Path(__file__).resolve().parent.parent
ORIGEN = RAIZ / "datos" / "pyro-sdis"
CAMARAS = {"brison-20", "courmettes-212", "serre-de-barre-108"}
VENTANA, SALIDA, SEMILLA = 224, 96, 7

import importlib.util
spec = importlib.util.spec_from_file_location("c", Path(__file__).parent/"02_construir_dataset.py")
c = importlib.util.module_from_spec(spec); spec.loader.exec_module(c)

rng = __import__("random").Random(SEMILLA)
parches, tipos = [], []
vistos = 0
for archivo in sorted(ORIGEN.glob("*.parquet")):
    for lote in pq.ParquetFile(archivo).iter_batches(batch_size=32):
        for fila in lote.to_pylist():
            if fila["camera"] not in CAMARAS or vistos >= 900:
                continue
            vistos += 1
            img = Image.open(io.BytesIO(fila["image"]["bytes"])).convert("RGB")
            an, al = img.size
            cajas = c.leer_bboxes(fila["annotations"], an, al)
            n, tipo = (3, "duro") if cajas else (4, "limpio")
            for v in c.ventanas_negativas(cajas, an, al, n, rng):
                parches.append(c.recortar(img, v)); tipos.append(tipo)
    if vistos >= 900:
        break

X = np.stack(parches); tipos = np.array(tipos)
it = tf.lite.Interpreter(model_path=str(RAIZ/"modelos"/"cnn_media"/"modelo_float32.tflite"))
e = it.get_input_details()[0]
it.resize_tensor_input(e["index"], [256, 96, 96, 3]); it.allocate_tensors()
e, s = it.get_input_details()[0], it.get_output_details()[0]
p = []
for i in range(0, len(X), 256):
    t = X[i:i+256].astype(np.float32)
    if len(t) < 256:
        t = np.concatenate([t, np.zeros((256-len(t),96,96,3), np.float32)])
    it.set_tensor(e["index"], t); it.invoke()
    p.append(it.get_tensor(s["index"]).reshape(-1))
p = np.concatenate(p)[:len(X)]

print(f"{vistos} cuadros -> {len(X)} ventanas negativas")
print(f"{'umbral':>8} {'FPR duro':>10} {'FPR limpio':>12} {'cociente':>10}")
for u in (0.06, 0.5, 0.9):
    fd = float((p[tipos=="duro"] >= u).mean())
    fl = float((p[tipos=="limpio"] >= u).mean())
    print(f"{u:>8.2f} {fd:>10.4f} {fl:>12.4f} {fd/max(fl,1e-6):>9.1f}x")
print(f"\nn duro={int((tipos=='duro').sum())}  n limpio={int((tipos=='limpio').sum())}")
