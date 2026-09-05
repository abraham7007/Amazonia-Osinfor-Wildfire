"""Construye el dataset de clasificacion 96x96 (humo / no-humo) a partir de Pyro-SDIS.

Por que por teselas y no por cuadro completo
--------------------------------------------
En Pyro-SDIS las imagenes son de 1280x720 y la columna de humo anotada ocupa una
mediana de 3,2 % del ancho y 4,5 % del alto: unos 41x32 px. Reducir el cuadro
entero a 96x96 dejaria la columna en 3x2 px — por debajo del limite de deteccion.
Por eso se recorta una VENTANA de 224x224 px del cuadro original y esa ventana se
reduce a 96x96: la columna mediana conserva ~18x14 px.

En el nodo, el firmware recorre el cuadro con esa misma rejilla de ventanas
(6x4 con solape) y ejecuta una inferencia por tesela.

Clases
------
1 = humo   : ventana centrada en una bbox anotada (con jitter).
0 = no-humo: ventanas SIN solape con ninguna bbox, tomadas de los mismos cuadros
             (negativos duros: mismo bosque, misma camara, misma luz) y de los
             cuadros sin anotacion (niebla, nubes bajas, amanecer, contraluz).

Particion
---------
Por CAMARA, no por imagen: los cuadros de una misma camara son casi duplicados y
mezclarlos entre particiones inflaria las metricas. Ademas se reserva el partner
`sdis-77` completo como conjunto de generalizacion cruzada (seccion 6 de
`modelo-edge-ai.md`).
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import random
from collections import defaultdict
from pathlib import Path

import numpy as np
import pyarrow.parquet as pq
from PIL import Image

RAIZ = Path(__file__).resolve().parent.parent
ORIGEN = RAIZ / "datos" / "pyro-sdis"
DESTINO = RAIZ / "datos" / "clasificacion-96"

VENTANA = 224          # px sobre el cuadro original (1280x720)
SALIDA = 96            # px de entrada al modelo
MARGEN_BBOX = 2.2      # la ventana cubre al menos 2,2x la bbox
CROPS_POR_BBOX = 2     # centrado + jitter
NEG_POR_CUADRO_POS = 3 # negativos duros extraidos de cuadros con humo
NEG_POR_CUADRO_NEG = 4 # negativos de cuadros sin anotacion
PARTNER_CRUZADO = "sdis-77"
SEMILLA = 42


def leer_bboxes(anotaciones: str, ancho: int, alto: int) -> list[tuple[int, int, int, int]]:
    """Convierte anotaciones YOLO normalizadas a cajas (x0, y0, x1, y1) en px."""
    cajas = []
    for linea in (anotaciones or "").strip().splitlines():
        partes = linea.split()
        if len(partes) != 5:
            continue
        xc, yc, w, h = (float(v) for v in partes[1:])
        x0 = (xc - w / 2) * ancho
        y0 = (yc - h / 2) * alto
        cajas.append((x0, y0, x0 + w * ancho, y0 + h * alto))
    return cajas


def ventana_para_bbox(caja, ancho, alto, rng, jitter: bool):
    """Ventana cuadrada que encierra la bbox con contexto, recortada al cuadro."""
    x0, y0, x1, y1 = caja
    lado = max(VENTANA, (x1 - x0) * MARGEN_BBOX, (y1 - y0) * MARGEN_BBOX)
    lado = min(lado, alto)  # no puede exceder la dimension menor del cuadro
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    if jitter:
        # Desplaza el centro sin sacar la bbox de la ventana.
        holgura_x = max(0.0, (lado - (x1 - x0)) / 2 * 0.6)
        holgura_y = max(0.0, (lado - (y1 - y0)) / 2 * 0.6)
        cx += rng.uniform(-holgura_x, holgura_x)
        cy += rng.uniform(-holgura_y, holgura_y)
    vx = min(max(cx - lado / 2, 0), ancho - lado)
    vy = min(max(cy - lado / 2, 0), alto - lado)
    return (vx, vy, vx + lado, vy + lado)


def solapa(v, cajas, umbral: float = 0.0) -> bool:
    """True si la ventana toca alguna bbox mas alla del umbral de area."""
    vx0, vy0, vx1, vy1 = v
    for x0, y0, x1, y1 in cajas:
        ix = max(0.0, min(vx1, x1) - max(vx0, x0))
        iy = max(0.0, min(vy1, y1) - max(vy0, y0))
        inter = ix * iy
        area_caja = max(1.0, (x1 - x0) * (y1 - y0))
        if inter / area_caja > umbral:
            return True
    return False


def ventanas_negativas(cajas, ancho, alto, n, rng):
    """Ventanas aleatorias que no tocan ninguna bbox."""
    salida, intentos = [], 0
    while len(salida) < n and intentos < n * 25:
        intentos += 1
        lado = VENTANA * rng.uniform(0.85, 1.6)
        lado = min(lado, alto)
        vx = rng.uniform(0, ancho - lado)
        vy = rng.uniform(0, alto - lado)
        v = (vx, vy, vx + lado, vy + lado)
        if not solapa(v, cajas):
            salida.append(v)
    return salida


def recortar(img: Image.Image, v) -> np.ndarray:
    parche = img.resize((SALIDA, SALIDA), Image.BILINEAR,
                        box=(v[0], v[1], v[2], v[3]))
    return np.asarray(parche.convert("RGB"), dtype=np.uint8)


def asignar_particion(camara: str, partner: str, mapa: dict) -> str:
    if partner == PARTNER_CRUZADO:
        return "cruzado"
    if camara not in mapa:
        # 80/20 por camara. hashlib y no hash(): hash() de str esta aleatorizado
        # por PYTHONHASHSEED y daria una particion distinta en cada ejecucion.
        digest = hashlib.md5(camara.encode()).hexdigest()
        mapa[camara] = "val" if int(digest[:8], 16) % 100 < 20 else "train"
    return mapa[camara]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limite-filas", type=int, default=0,
                    help="procesa solo N filas por archivo (prueba rapida)")
    args = ap.parse_args()

    rng = random.Random(SEMILLA)
    DESTINO.mkdir(parents=True, exist_ok=True)

    # El dataset completo ronda los 4 GB: se escribe en crudo a disco a medida que
    # se genera y solo al final se convierte a .npy, en lugar de acumularlo en RAM.
    crudos: dict[str, object] = {}
    etiquetas: dict[str, list] = defaultdict(list)
    procedencia: dict[str, list] = defaultdict(list)
    mapa_camaras: dict[str, str] = {}
    stats = defaultdict(int)

    def escribir(part: str, parche: np.ndarray, etiqueta: int, camara: str) -> None:
        if part not in crudos:
            crudos[part] = open(DESTINO / f"_{part}.raw", "wb")
        crudos[part].write(parche.tobytes())
        etiquetas[part].append(etiqueta)
        procedencia[part].append(camara)

    archivos = sorted(ORIGEN.glob("*.parquet"))
    if not archivos:
        raise SystemExit(f"No hay parquet en {ORIGEN}. Corre 01_descargar_pyro_sdis.py")

    for archivo in archivos:
        pf = pq.ParquetFile(archivo)
        vistas = 0
        print(f"\n[{archivo.name}] {pf.metadata.num_rows} filas", flush=True)
        for lote in pf.iter_batches(batch_size=64):
            for fila in lote.to_pylist():
                if args.limite_filas and vistas >= args.limite_filas:
                    break
                vistas += 1
                stats["cuadros"] += 1
                try:
                    img = Image.open(io.BytesIO(fila["image"]["bytes"])).convert("RGB")
                except Exception:
                    stats["ilegibles"] += 1
                    continue
                ancho, alto = img.size
                cajas = leer_bboxes(fila["annotations"], ancho, alto)
                part = asignar_particion(fila["camera"], fila["partner"], mapa_camaras)

                if cajas:
                    stats["cuadros_con_humo"] += 1
                    for caja in cajas:
                        for k in range(CROPS_POR_BBOX):
                            v = ventana_para_bbox(caja, ancho, alto, rng, jitter=(k > 0))
                            escribir(part, recortar(img, v), 1, fila["camera"])
                            stats["positivos"] += 1
                    for v in ventanas_negativas(cajas, ancho, alto, NEG_POR_CUADRO_POS, rng):
                        escribir(part, recortar(img, v), 0, fila["camera"])
                        stats["neg_duros"] += 1
                else:
                    stats["cuadros_sin_humo"] += 1
                    for v in ventanas_negativas([], ancho, alto, NEG_POR_CUADRO_NEG, rng):
                        escribir(part, recortar(img, v), 0, fila["camera"])
                        stats["neg_limpios"] += 1
            if args.limite_filas and vistas >= args.limite_filas:
                break
        print(f"  acumulado: {sum(len(v) for v in etiquetas.values())} parches", flush=True)

    resumen = {"ventana_px": VENTANA, "salida_px": SALIDA, "semilla": SEMILLA,
               "partner_cruzado": PARTNER_CRUZADO, "conteos": dict(stats), "particiones": {}}

    for fh in crudos.values():
        fh.close()

    for part in sorted(etiquetas):
        y = np.asarray(etiquetas[part], dtype=np.uint8)
        crudo = DESTINO / f"_{part}.raw"
        # Vuelca el .raw a .npy sin cargarlo entero: memmap de origen y destino.
        origen = np.memmap(crudo, dtype=np.uint8, mode="r",
                           shape=(len(y), SALIDA, SALIDA, 3))
        X = np.lib.format.open_memmap(DESTINO / f"X_{part}.npy", mode="w+",
                                      dtype=np.uint8, shape=origen.shape)
        for i in range(0, len(y), 2048):
            X[i:i + 2048] = origen[i:i + 2048]
        X.flush()
        del X, origen
        crudo.unlink()
        np.save(DESTINO / f"y_{part}.npy", y)
        X = np.load(DESTINO / f"X_{part}.npy", mmap_mode="r")
        (DESTINO / f"camaras_{part}.txt").write_text("\n".join(procedencia[part]))
        resumen["particiones"][part] = {
            "n": int(len(y)), "positivos": int(y.sum()), "negativos": int((y == 0).sum()),
            "camaras": len(set(procedencia[part])), "forma": list(X.shape),
        }
        print(f"{part:>8}: {len(y):6d} parches "
              f"({int(y.sum())} humo / {int((y == 0).sum())} no-humo), "
              f"{len(set(procedencia[part]))} camaras")

    (DESTINO / "resumen.json").write_text(json.dumps(resumen, indent=2, ensure_ascii=False))
    print(f"\nEscrito en {DESTINO}")


if __name__ == "__main__":
    main()
