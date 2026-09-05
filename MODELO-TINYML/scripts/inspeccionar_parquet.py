"""Inspeccion rapida del parquet de Pyro-SDIS: esquema, camaras, anotaciones."""

import io
import json
from collections import Counter
from pathlib import Path

import pyarrow.parquet as pq
from PIL import Image

DATOS = Path(__file__).resolve().parent.parent / "datos" / "pyro-sdis"


def main() -> None:
    archivo = sorted(DATOS.glob("*.parquet"))[0]
    pf = pq.ParquetFile(archivo)
    print(f"Archivo: {archivo.name}")
    print(f"Filas: {pf.metadata.num_rows} | grupos: {pf.metadata.num_row_groups}")
    print(f"Esquema:\n{pf.schema_arrow}\n")

    lote = next(pf.iter_batches(batch_size=200)).to_pylist()

    con_anot = sum(1 for r in lote if (r["annotations"] or "").strip())
    print(f"En las primeras {len(lote)} filas: {con_anot} con anotacion, "
          f"{len(lote) - con_anot} sin anotacion (negativos)")

    print("\nCamaras (muestra):", Counter(r["camera"] for r in lote).most_common(8))
    print("Partners (muestra):", Counter(r["partner"] for r in lote).most_common())

    for r in lote[:5]:
        img = Image.open(io.BytesIO(r["image"]["bytes"]))
        print(f"\n  {r['image_name']} | {img.size} {img.mode} | cam={r['camera']} "
              f"| fecha={r['date']}")
        print(f"  annotations={r['annotations']!r}")

    # Estadistica de tamano de bbox (normalizado) en el lote
    anchos, altos = [], []
    for r in lote:
        for linea in (r["annotations"] or "").strip().splitlines():
            partes = linea.split()
            if len(partes) == 5:
                anchos.append(float(partes[3]))
                altos.append(float(partes[4]))
    if anchos:
        anchos.sort()
        altos.sort()
        q = lambda v, p: v[int(p * (len(v) - 1))]  # noqa: E731
        print(f"\nBBox normalizado — ancho p10/p50/p90: "
              f"{q(anchos, .1):.3f} / {q(anchos, .5):.3f} / {q(anchos, .9):.3f}")
        print(f"BBox normalizado — alto  p10/p50/p90: "
              f"{q(altos, .1):.3f} / {q(altos, .5):.3f} / {q(altos, .9):.3f}")


if __name__ == "__main__":
    main()
