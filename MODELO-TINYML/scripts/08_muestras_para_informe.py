"""Extrae parches de ejemplo para el informe, con la procedencia controlada.

Los positivos salen de cajas anotadas. Los negativos NO se toman de cuadros con
humo: se toman solo de cuadros SIN ninguna anotacion, y de esos, los que el
modelo puntuo mas alto. Asi se garantiza que lo que se muestra como falso
positivo es una confusion real con niebla o nube, y no una cola de penacho que
el anotador dejo fuera de la caja (ver `09_diagnostico_negativos.py`).
"""

import base64
import io
import json
import random
import sys
from pathlib import Path

import numpy as np
import pyarrow.parquet as pq
import tensorflow as tf
from PIL import Image

sys.path.insert(0, str(Path(__file__).parent))
import importlib.util

_spec = importlib.util.spec_from_file_location(
    "constructor", Path(__file__).parent / "02_construir_dataset.py")
cons = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cons)

RAIZ = Path(__file__).resolve().parent.parent
ORIGEN = RAIZ / "datos" / "pyro-sdis"
SALIDA = RAIZ / "muestras"
DESCARTADO = RAIZ.parent / "GESTION" / "07-datos" / "dataset-fire"
CAMARAS = {"brison-20", "courmettes-212", "serre-de-barre-108"}  # nunca entrenadas
MAX_CUADROS = 1100
LADO_JPEG = 200


def puntuar(X: np.ndarray) -> np.ndarray:
    it = tf.lite.Interpreter(
        model_path=str(RAIZ / "modelos" / "cnn_media" / "modelo_float32.tflite"))
    ent = it.get_input_details()[0]
    it.resize_tensor_input(ent["index"], [256, 96, 96, 3])
    it.allocate_tensors()
    ent, sal = it.get_input_details()[0], it.get_output_details()[0]
    out = []
    for i in range(0, len(X), 256):
        t = X[i:i + 256].astype(np.float32)
        if len(t) < 256:
            t = np.concatenate([t, np.zeros((256 - len(t), 96, 96, 3), np.float32)])
        it.set_tensor(ent["index"], t)
        it.invoke()
        out.append(it.get_tensor(sal["index"]).reshape(-1))
    return np.concatenate(out)[:len(X)]


def a_jpeg(arr: np.ndarray) -> str:
    im = Image.fromarray(arr).resize((LADO_JPEG, LADO_JPEG), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, "JPEG", quality=82)
    return base64.b64encode(buf.getvalue()).decode()


def main() -> None:
    SALIDA.mkdir(exist_ok=True)
    rng = random.Random(11)

    positivos, negativos_limpios = [], []
    vistos = 0
    for archivo in sorted(ORIGEN.glob("*.parquet")):
        for lote in pq.ParquetFile(archivo).iter_batches(batch_size=32):
            for fila in lote.to_pylist():
                if fila["camera"] not in CAMARAS or vistos >= MAX_CUADROS:
                    continue
                vistos += 1
                img = Image.open(io.BytesIO(fila["image"]["bytes"])).convert("RGB")
                ancho, alto = img.size
                cajas = cons.leer_bboxes(fila["annotations"], ancho, alto)
                if cajas:
                    for caja in cajas:
                        v = cons.ventana_para_bbox(caja, ancho, alto, rng, jitter=False)
                        positivos.append(cons.recortar(img, v))
                else:
                    for v in cons.ventanas_negativas([], ancho, alto, 4, rng):
                        negativos_limpios.append(cons.recortar(img, v))
        if vistos >= MAX_CUADROS:
            break

    Xp, Xn = np.stack(positivos), np.stack(negativos_limpios)
    pp, pn = puntuar(Xp), puntuar(Xn)
    print(f"{vistos} cuadros -> {len(Xp)} positivos, {len(Xn)} negativos de cuadro limpio")

    # Positivos: se muestran los bien detectados, repartidos por el ranking para
    # que no salgan seis veces la misma escena.
    orden_p = np.argsort(-pp)[:60][::10][:6]
    # Negativos: los que mas alto puntuaron, que son los falsos positivos reales.
    orden_n = np.argsort(-pn)[:6]

    datos = {
        "positivos": [{"score": round(float(pp[i]), 3), "b64": a_jpeg(Xp[i])}
                      for i in orden_p],
        "negativos": [{"score": round(float(pn[i]), 3), "b64": a_jpeg(Xn[i])}
                      for i in orden_n],
    }

    descartadas = []
    for etiqueta, fichero in [
        ("llama en primer plano", "fire/FIRE (1018).jpg"),
        ("estructura ardiendo", "fire/FIRE (245).jpg"),
        ("negativo sin relación con la escena", "not_fire/NON_FIRE (2604).jpg"),
    ]:
        im = Image.open(DESCARTADO / fichero).convert("RGB")
        lado = min(im.size)
        izq, arr = (im.width - lado) // 2, (im.height - lado) // 2
        recorte = np.asarray(im.crop((izq, arr, izq + lado, arr + lado)))
        descartadas.append({"etiqueta": etiqueta, "b64": a_jpeg(recorte)})
    datos["descartadas"] = descartadas

    (SALIDA / "muestras.json").write_text(json.dumps(datos))
    print("positivos:", [d["score"] for d in datos["positivos"]])
    print("falsos positivos (cuadro sin humo):", [d["score"] for d in datos["negativos"]])
    peso = sum(len(d["b64"]) for g in datos.values() for d in g) / 1024
    print(f"peso base64: {peso:.0f} KiB")


if __name__ == "__main__":
    main()
