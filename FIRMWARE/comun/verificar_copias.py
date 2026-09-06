#!/usr/bin/env python3
"""Comprueba que el codigo de cada etapa es identico en los dos nodos.

Cada proyecto tiene su propia carpeta src/ para que se pueda abrir y leer sin
saltar a ningun otro sitio. El precio es que hay dos copias de cada etapa, y dos
copias pueden divergir sin que nadie se entere: alguien arregla un fallo en el
nodo 1, se olvida del nodo 2, y a partir de ahi los dos nodos ejecutan cosas
distintas mientras el informe dice que ejecutan lo mismo.

Este script detecta eso. Todo lo que difiere entre placas vive en la libreria
`placa`, asi que el codigo de las etapas DEBE ser identico byte a byte.

    python3 comun/verificar_copias.py          informa
    python3 comun/verificar_copias.py --sincronizar NODO
                                               copia NODO sobre el otro

Devuelve 0 si todo coincide y 1 si hay diferencias, para poder encadenarlo.
"""

from __future__ import annotations

import argparse
import filecmp
import shutil
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
NODOS = ["nodo-1-xiao", "nodo-2-ne101"]


def etapas() -> list[str]:
    return sorted(p.name for p in (RAIZ / NODOS[0]).iterdir()
                  if p.is_dir() and p.name[0].isdigit())


def comparar(etapa: str) -> list[str]:
    """Devuelve la lista de ficheros que no coinciden entre los dos nodos."""
    a = RAIZ / NODOS[0] / etapa / "src"
    b = RAIZ / NODOS[1] / etapa / "src"
    if not a.is_dir() or not b.is_dir():
        return [f"falta la carpeta src/ en {etapa}"]

    nombres_a = {f.name for f in a.iterdir() if f.is_file()}
    nombres_b = {f.name for f in b.iterdir() if f.is_file()}

    problemas = [f"solo en {NODOS[0]}: {n}" for n in sorted(nombres_a - nombres_b)]
    problemas += [f"solo en {NODOS[1]}: {n}" for n in sorted(nombres_b - nombres_a)]
    problemas += [f"distinto: {n}" for n in sorted(nombres_a & nombres_b)
                  if not filecmp.cmp(a / n, b / n, shallow=False)]
    return problemas


def sincronizar(origen: str) -> int:
    destino = NODOS[1] if origen == NODOS[0] else NODOS[0]
    copiados = 0
    for etapa in etapas():
        o = RAIZ / origen / etapa / "src"
        d = RAIZ / destino / etapa / "src"
        d.mkdir(parents=True, exist_ok=True)
        for f in sorted(o.glob("*")):
            if not (d / f.name).exists() or not filecmp.cmp(f, d / f.name, shallow=False):
                shutil.copy2(f, d / f.name)
                print(f"  {etapa}/{f.name}: {origen} -> {destino}")
                copiados += 1
    print(f"\n{copiados} fichero(s) sincronizado(s)" if copiados else "\nya estaban al dia")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--sincronizar", choices=NODOS, metavar="NODO",
                   help="toma NODO como bueno y copia su codigo sobre el otro")
    args = p.parse_args()

    if args.sincronizar:
        return sincronizar(args.sincronizar)

    total = 0
    for etapa in etapas():
        problemas = comparar(etapa)
        if problemas:
            total += len(problemas)
            print(f"  {etapa}")
            for x in problemas:
                print(f"      {x}")
        else:
            print(f"  {etapa}  ok")

    if total:
        print(f"\n{total} diferencia(s). El codigo de las etapas debe ser identico en")
        print("los dos nodos: lo que cambia entre placas va en comun/librerias/placa.")
        print(f"Para igualarlos: python3 {Path(__file__).name} --sincronizar <nodo-bueno>")
        return 1

    print(f"\nlas {len(etapas())} etapas coinciden en los dos nodos")
    return 0


if __name__ == "__main__":
    sys.exit(main())
