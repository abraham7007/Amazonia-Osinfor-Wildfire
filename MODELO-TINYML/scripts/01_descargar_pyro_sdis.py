"""Descarga el dataset Pyro-SDIS (PyroNear) desde HuggingFace.

33 636 imagenes de camaras fijas de vigilancia: 28 103 con columna de humo
anotada (bbox YOLO) y ~5 500 negativos tomados por las MISMAS camaras
(niebla, nubes bajas, amanecer, contraluz). Licencia Apache-2.0.

Referencia [R03] de `DISENO E INGENIERIA/00-gestion/referencias.md`.
"""

import sys
from pathlib import Path

from huggingface_hub import hf_hub_download

REPO = "pyronear/pyro-sdis"
DESTINO = Path(__file__).resolve().parent.parent / "datos" / "pyro-sdis"

ARCHIVOS = [f"data/train-0000{i}-of-00006.parquet" for i in range(6)] + [
    "data/val-00000-of-00001.parquet"
]


def main() -> int:
    DESTINO.mkdir(parents=True, exist_ok=True)
    for archivo in ARCHIVOS:
        destino_local = DESTINO / Path(archivo).name
        if destino_local.exists():
            print(f"[ya existe] {destino_local.name}")
            continue
        print(f"[descargando] {archivo} ...", flush=True)
        ruta = hf_hub_download(
            repo_id=REPO,
            filename=archivo,
            repo_type="dataset",
            local_dir=str(DESTINO.parent / "_hf_cache"),
        )
        Path(ruta).replace(destino_local)
        print(f"[ok] {destino_local.name} ({destino_local.stat().st_size / 1e6:.0f} MB)")

    total = sum(p.stat().st_size for p in DESTINO.glob("*.parquet"))
    print(f"\nTotal descargado: {total / 1e9:.2f} GB en {DESTINO}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
