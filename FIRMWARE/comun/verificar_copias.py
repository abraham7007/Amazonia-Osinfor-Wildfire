#!/usr/bin/env python3
"""Comprueba que las copias de codigo y librerias no se han desviado.

Cada proyecto es autocontenido: tiene su platformio.ini, su src/ y su lib/ con
las librerias que necesita. Se puede copiar a cualquier sitio y compila solo.

El precio es que hay copias: once etapas por dos nodos, y 82 copias de libreria
repartidas entre los 22 proyectos. Dos copias pueden divergir sin que nadie se
entere —alguien arregla un fallo en un proyecto y se olvida de los demas—, y a
partir de ahi los nodos ejecutan cosas distintas mientras el informe dice que
ejecutan lo mismo.

Lo mas delicado son las librerias, y en particular `placa`: ahi viven los pinout
y la memoria de cada placa. Si alguien corrige PLACA_SD_PIN_CS en un proyecto,
los otros diez de ese nodo se quedan con el valor viejo.

La referencia es comun/librerias/ para las librerias y nodo-1-xiao para el
codigo de las etapas.

    python3 comun/verificar_copias.py            informa
    python3 comun/verificar_copias.py --sincronizar
                                                 rehace las copias desde la referencia

Devuelve 0 si todo coincide y 1 si hay diferencias, para poder encadenarlo.
"""

from __future__ import annotations

import argparse
import filecmp
import shutil
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
LIBRERIAS = RAIZ / "comun" / "librerias"
NODOS = ["nodo-1-xiao", "nodo-2-ne101"]


def etapas() -> list[str]:
    return sorted(p.name for p in (RAIZ / NODOS[0]).iterdir()
                  if p.is_dir() and p.name[0].isdigit())


def proyectos() -> list[Path]:
    return [RAIZ / n / e for n in NODOS for e in etapas()]


def arboles_iguales(a: Path, b: Path) -> list[str]:
    """Ficheros que difieren entre dos arboles, comparando el contenido."""
    problemas = []
    na = {f.relative_to(a) for f in a.rglob("*") if f.is_file()}
    nb = {f.relative_to(b) for f in b.rglob("*") if f.is_file()}
    problemas += [f"falta {x}" for x in sorted(na - nb)]
    problemas += [f"sobra {x}" for x in sorted(nb - na)]
    problemas += [f"distinto {x}" for x in sorted(na & nb)
                  if not filecmp.cmp(a / x, b / x, shallow=False)]
    return problemas


def revisar_etapas() -> int:
    """El codigo de cada etapa debe ser identico en los dos nodos."""
    fallos = 0
    for etapa in etapas():
        p = arboles_iguales(RAIZ / NODOS[0] / etapa / "src",
                            RAIZ / NODOS[1] / etapa / "src")
        if p:
            fallos += len(p)
            print(f"  src {etapa}")
            for x in p:
                print(f"      {x}")
    if not fallos:
        print(f"  src: las {len(etapas())} etapas coinciden en los dos nodos")
    return fallos


def revisar_librerias() -> int:
    """Cada copia de libreria debe ser identica a la de comun/librerias/."""
    fallos = 0
    copias = 0
    for proyecto in proyectos():
        lib = proyecto / "lib"
        if not lib.is_dir():
            continue
        for copia in sorted(lib.iterdir()):
            if not copia.is_dir():
                continue
            copias += 1
            maestra = LIBRERIAS / copia.name
            if not maestra.is_dir():
                print(f"  lib {proyecto.relative_to(RAIZ)}/{copia.name}: no existe la maestra")
                fallos += 1
                continue
            p = arboles_iguales(maestra, copia)
            if p:
                fallos += len(p)
                print(f"  lib {proyecto.relative_to(RAIZ)}/{copia.name}")
                for x in p:
                    print(f"      {x}")
    if not fallos:
        print(f"  lib: las {copias} copias coinciden con comun/librerias/")
    return fallos


def sincronizar() -> int:
    """Rehace las copias desde la referencia: comun/librerias y nodo-1-xiao."""
    n = 0
    for etapa in etapas():
        origen = RAIZ / NODOS[0] / etapa / "src"
        destino = RAIZ / NODOS[1] / etapa / "src"
        destino.mkdir(parents=True, exist_ok=True)
        for f in sorted(origen.glob("*")):
            if not (destino / f.name).exists() or not filecmp.cmp(f, destino / f.name, shallow=False):
                shutil.copy2(f, destino / f.name)
                print(f"  src  {NODOS[1]}/{etapa}/{f.name}")
                n += 1

    for proyecto in proyectos():
        lib = proyecto / "lib"
        if not lib.is_dir():
            continue
        for copia in sorted(lib.iterdir()):
            maestra = LIBRERIAS / copia.name
            if maestra.is_dir() and arboles_iguales(maestra, copia):
                shutil.rmtree(copia)
                shutil.copytree(maestra, copia)
                print(f"  lib  {proyecto.relative_to(RAIZ)}/{copia.name}")
                n += 1

    print(f"\n{n} elemento(s) sincronizado(s)" if n else "\nya estaba todo al dia")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--sincronizar", action="store_true",
                   help="rehace las copias desde comun/librerias y nodo-1-xiao")
    args = p.parse_args()

    if args.sincronizar:
        return sincronizar()

    fallos = revisar_etapas() + revisar_librerias()
    if fallos:
        print(f"\n{fallos} diferencia(s).")
        print("Lo que cambia entre placas va en comun/librerias/placa, no en una copia.")
        print(f"Para rehacer las copias: python3 {Path(__file__).name} --sincronizar")
        return 1

    print("\ntodo coincide")
    return 0


if __name__ == "__main__":
    sys.exit(main())
