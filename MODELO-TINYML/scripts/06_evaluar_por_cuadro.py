"""Evalua el modelo como lo usara el nodo: recorriendo el cuadro completo por teselas.

Por que hace falta esta evaluacion aparte
------------------------------------------
`05_evaluar.py` mide el modelo por parche. Pero el nodo no clasifica un parche:
clasifica 32 por captura y dispara la alerta LoRa si alguno supera el umbral. Con
teselas independientes, la tasa de falsa alarma por CUADRO es

    FPR_cuadro = 1 - (1 - FPR_parche) ^ 32

Una FPR por parche de 0,10 —que suena aceptable y es la meta escrita en la seccion
5 de `modelo-edge-ai.md`— produce una falsa alarma en el 97 % de las capturas. Para
que el cuadro cumpla FPR <= 0,10, la FPR por parche debe bajar a ~0,003.

Este script mide directamente lo que importa: cuantos cuadros con humo se detectan
y en cuantos cuadros limpios se dispara una alerta. Ademas evalua la confirmacion
por K teselas (exigir K deteccionesen la misma captura) y la confirmacion temporal
sobre N capturas consecutivas de la misma camara, que es la mitigacion que propone
la seccion 1 del documento de diseno.

Se ejecuta sobre el partner `sdis-77`, que nunca entro en entrenamiento.
"""

from __future__ import annotations

import argparse
import io
import json
from collections import defaultdict
from pathlib import Path

import numpy as np
import pyarrow.parquet as pq
import tensorflow as tf
from PIL import Image

RAIZ = Path(__file__).resolve().parent.parent
ORIGEN = RAIZ / "datos" / "pyro-sdis"
MODELOS = RAIZ / "modelos"

VENTANA = 224
SALIDA = 96
PARTNER = "sdis-77"
SOLAPE = 0.25  # fraccion de solape entre teselas contiguas


def rejilla(ancho: int, alto: int) -> list[tuple[int, int]]:
    """Esquinas de la rejilla de teselas que cubre el cuadro, con solape."""
    paso = int(VENTANA * (1 - SOLAPE))
    xs = list(range(0, max(1, ancho - VENTANA + 1), paso))
    ys = list(range(0, max(1, alto - VENTANA + 1), paso))
    if xs[-1] != ancho - VENTANA:
        xs.append(ancho - VENTANA)
    if ys[-1] != alto - VENTANA:
        ys.append(alto - VENTANA)
    return [(x, y) for y in ys for x in xs]


def teselas(img: Image.Image, celdas) -> np.ndarray:
    return np.stack([
        np.asarray(img.resize((SALIDA, SALIDA), Image.BILINEAR,
                              box=(x, y, x + VENTANA, y + VENTANA)), dtype=np.uint8)
        for x, y in celdas
    ])


class Modelo:
    def __init__(self, ruta: Path, lote: int):
        self.interp = tf.lite.Interpreter(model_path=str(ruta))
        ent = self.interp.get_input_details()[0]
        self.interp.resize_tensor_input(ent["index"], [lote, SALIDA, SALIDA, 3])
        self.interp.allocate_tensors()
        self.ent = self.interp.get_input_details()[0]
        self.sal = self.interp.get_output_details()[0]
        self.lote = lote

    def __call__(self, x: np.ndarray) -> np.ndarray:
        n = len(x)
        if n < self.lote:
            x = np.concatenate([x, np.zeros((self.lote - n, SALIDA, SALIDA, 3), x.dtype)])
        v = x.astype(np.float32)
        if self.ent["dtype"] == np.int8:
            esc, cero = self.ent["quantization"]
            v = np.clip(np.round(v / esc + cero), -128, 127).astype(np.int8)
        self.interp.set_tensor(self.ent["index"], v)
        self.interp.invoke()
        y = self.interp.get_tensor(self.sal["index"])[:n].reshape(-1)
        if self.sal["dtype"] == np.int8:
            esc, cero = self.sal["quantization"]
            y = (y.astype(np.float32) - cero) * esc
        return y


def construir_cache(cache: Path, celdas, max_cuadros: int, camaras_pedidas=None):
    """Decodifica los cuadros del partner reservado y guarda sus teselas.

    Decodificar los JPEG es lo caro; se hace una vez y sirve para comparar varios
    modelos sobre exactamente los mismos cuadros.
    """
    cache.mkdir(parents=True, exist_ok=True)
    bruto = open(cache / "_X.raw", "wb")
    etiquetas, camaras, fechas = [], [], []

    for archivo in sorted(ORIGEN.glob("*.parquet")):
        for lote in pq.ParquetFile(archivo).iter_batches(batch_size=32):
            for fila in lote.to_pylist():
                pertenece = (fila["camera"] in camaras_pedidas if camaras_pedidas
                             else fila["partner"] == PARTNER)
                if not pertenece or len(etiquetas) >= max_cuadros:
                    continue
                img = Image.open(io.BytesIO(fila["image"]["bytes"])).convert("RGB")
                if img.size != (1280, 720):
                    img = img.resize((1280, 720))
                bruto.write(teselas(img, celdas).tobytes())
                etiquetas.append(bool((fila["annotations"] or "").strip()))
                camaras.append(fila["camera"])
                fechas.append(fila["date"])
        print(f"  {len(etiquetas)} cuadros en cache", flush=True)
        if len(etiquetas) >= max_cuadros:
            break
    bruto.close()

    n = len(etiquetas)
    forma = (n, len(celdas), SALIDA, SALIDA, 3)
    origen = np.memmap(cache / "_X.raw", dtype=np.uint8, mode="r", shape=forma)
    X = np.lib.format.open_memmap(cache / "X.npy", mode="w+", dtype=np.uint8, shape=forma)
    for i in range(0, n, 64):
        X[i:i + 64] = origen[i:i + 64]
    X.flush()
    del X, origen
    (cache / "_X.raw").unlink()

    con_humo = np.array(etiquetas)
    np.save(cache / "con_humo.npy", con_humo)
    (cache / "meta.json").write_text(json.dumps(
        {"camaras": camaras, "fechas": fechas, "celdas": celdas}, ensure_ascii=False))
    return np.load(cache / "X.npy", mmap_mode="r"), con_humo


def metricas_cuadro(con_humo: np.ndarray, alerta: np.ndarray) -> dict:
    tp = int((alerta & con_humo).sum())
    fp = int((alerta & ~con_humo).sum())
    fn = int((~alerta & con_humo).sum())
    tn = int((~alerta & ~con_humo).sum())
    recall = tp / max(1, tp + fn)
    precision = tp / max(1, tp + fp)
    return {"tp": tp, "fp": fp, "fn": fn, "tn": tn,
            "recall": round(recall, 4), "precision": round(precision, 4),
            "f1": round(2 * precision * recall / max(1e-9, precision + recall), 4),
            "fpr": round(fp / max(1, fp + tn), 4)}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--modelo", default="")
    ap.add_argument("--tflite", default="modelo_int8.tflite")
    ap.add_argument("--max-cuadros", type=int, default=1500)
    ap.add_argument("--solo-cache", action="store_true",
                    help="decodifica y guarda las teselas, sin evaluar")
    ap.add_argument("--camaras", default="",
                    help="lista separada por comas; por defecto, todo el partner "
                         f"{PARTNER}. Sirve para medir el techo con dominio conocido "
                         "usando las camaras de validacion.")
    ap.add_argument("--etiqueta-cache", default=PARTNER)
    args = ap.parse_args()
    if not args.solo_cache and not args.modelo:
        ap.error("hace falta --modelo (o --solo-cache)")

    dir_modelo = MODELOS / args.modelo
    celdas = rejilla(1280, 720)
    print(f"Rejilla: {len(celdas)} teselas de {VENTANA}px con {int(SOLAPE * 100)}% de solape")

    camaras_pedidas = set(c.strip() for c in args.camaras.split(",") if c.strip())
    cache = RAIZ / "datos" / f"teselas_{args.etiqueta_cache}_{args.max_cuadros}"
    if (cache / "X.npy").exists():
        print(f"Usando cache de teselas: {cache}")
        X = np.load(cache / "X.npy", mmap_mode="r")
        con_humo = np.load(cache / "con_humo.npy")
    else:
        X, con_humo = construir_cache(cache, celdas, args.max_cuadros, camaras_pedidas)
    print(f"{len(con_humo)} cuadros: {int(con_humo.sum())} con humo, "
          f"{int((~con_humo).sum())} limpios")
    if args.solo_cache:
        return

    modelo = Modelo(dir_modelo / args.tflite, lote=len(celdas))
    todas = np.stack([modelo(np.asarray(X[i])) for i in range(len(con_humo))])
    maxp = todas.max(axis=1)
    print(f"\n{len(con_humo)} cuadros: {int(con_humo.sum())} con humo, "
          f"{int((~con_humo).sum())} limpios\n")

    informe = {"modelo": args.modelo, "tflite": args.tflite, "partner": PARTNER,
               "teselas_por_cuadro": len(celdas), "n_cuadros": int(len(con_humo)),
               "reglas": {}}

    umbrales = np.round(np.linspace(0.05, 0.99, 95), 3)

    print("A · Regla 'cualquier tesela supera el umbral' (1 de N)")
    print(f"{'umbral':>8} {'recall':>8} {'precision':>10} {'F1':>7} {'FPR':>7}")
    mejor = None
    for t in umbrales:
        m = metricas_cuadro(con_humo, maxp >= t)
        if m["fpr"] <= 0.10 and (mejor is None or m["recall"] > mejor[1]["recall"]):
            mejor = (float(t), m)
        if int(t * 100) % 10 == 0:
            print(f"{t:>8.2f} {m['recall']:>8.3f} {m['precision']:>10.3f} "
                  f"{m['f1']:>7.3f} {m['fpr']:>7.3f}")
    informe["reglas"]["1_de_N"] = {"umbral": mejor[0], **mejor[1]} if mejor else None
    if mejor:
        print(f"  -> mejor con FPR<=0,10: umbral {mejor[0]:.2f}, "
              f"recall {mejor[1]['recall']:.3f}, FPR {mejor[1]['fpr']:.3f}")

    # Con 48 capturas diurnas al dia (cadencia de 15 min), una FPR por captura de
    # 0,10 son ~5 falsas alarmas diarias por nodo. 0,01 es ~1 cada dos dias, que es
    # lo que un sistema de alerta a comunidades puede sostener sin perder confianza.
    print("\nB · Regla 'al menos K teselas' (confirmacion espacial)")
    for k in (2, 3):
        for tope in (0.10, 0.01):
            mejor_k = None
            for t in umbrales:
                alerta = (todas >= t).sum(axis=1) >= k
                m = metricas_cuadro(con_humo, alerta)
                if m["fpr"] <= tope and (mejor_k is None or m["recall"] > mejor_k[1]["recall"]):
                    mejor_k = (float(t), m)
            clave = f"{k}_de_N_fpr{tope}"
            informe["reglas"][clave] = ({"umbral": mejor_k[0], **mejor_k[1]}
                                        if mejor_k else None)
            if mejor_k:
                print(f"  K={k}, FPR<={tope:.2f}: umbral {mejor_k[0]:.2f} -> "
                      f"recall {mejor_k[1]['recall']:.3f}, "
                      f"precision {mejor_k[1]['precision']:.3f}, "
                      f"FPR {mejor_k[1]['fpr']:.3f}")
            else:
                print(f"  K={k}, FPR<={tope:.2f}: inalcanzable")

    print("\nC · Confirmacion temporal (N capturas consecutivas de la misma camara)")
    meta = json.loads((cache / "meta.json").read_text())
    orden = sorted(range(len(con_humo)), key=lambda i: (meta["camaras"][i], meta["fechas"][i]))
    grupos: dict[str, list[int]] = defaultdict(list)
    for i in orden:
        grupos[meta["camaras"][i]].append(i)

    for n in (2, 3):
        mejor_n = None
        for t in umbrales:
            dispara = maxp >= t
            confirmado = np.zeros(len(con_humo), dtype=bool)
            for indices in grupos.values():
                for j in range(n - 1, len(indices)):
                    ventana_idx = indices[j - n + 1:j + 1]
                    confirmado[indices[j]] = dispara[ventana_idx].all()
            m = metricas_cuadro(con_humo, confirmado)
            if m["fpr"] <= 0.10 and (mejor_n is None or m["recall"] > mejor_n[1]["recall"]):
                mejor_n = (float(t), m)
        informe["reglas"][f"temporal_{n}"] = ({"umbral": mejor_n[0], **mejor_n[1]}
                                              if mejor_n else None)
        if mejor_n:
            print(f"  N={n}: umbral {mejor_n[0]:.2f} -> recall {mejor_n[1]['recall']:.3f}, "
                  f"precision {mejor_n[1]['precision']:.3f}, FPR {mejor_n[1]['fpr']:.3f}")
        else:
            print(f"  N={n}: ninguna combinacion alcanza FPR <= 0,10")

    # El nombre lleva la etiqueta del conjunto: si no, evaluar sobre las camaras de
    # validacion pisaria el informe del partner reservado.
    destino = dir_modelo / f"evaluacion_por_cuadro_{args.etiqueta_cache}.json"
    destino.write_text(json.dumps(informe, indent=2, ensure_ascii=False))
    print(f"\nInforme en {destino}")


if __name__ == "__main__":
    main()
