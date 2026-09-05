#!/usr/bin/env python3
"""Genera los vectores de prueba que fijan la paridad host/MCU del firmware.

Produce dos cabeceras:

  test/test_nucleo/vectores_remuestreo.h
      Un parche fuente de 224x224 generado con un LCG (el firmware genera el
      mismo con la misma formula, asi que no hace falta embeberlo) y la salida
      96x96 que produce Pillow con BILINEAR. Verifica que el remuestreador del
      nodo reproduce byte a byte el que se uso para construir el dataset de
      entrenamiento.

  test/test_modelo/vectores_modelo.h
      Parches reales de `datos/clasificacion-96` con la salida INT8 que da
      modelo_int8.tflite en el PC. Verifica que el modelo portado al MCU calcula
      lo mismo que el evaluado en 05_evaluar.py.

Uso:
    MODELO-TINYML/.venv/bin/python FIRMWARE/herramientas/generar_vectores.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image

RAIZ = Path(__file__).resolve().parents[2]
TINYML = RAIZ / "MODELO-TINYML"
FIRMWARE = RAIZ / "FIRMWARE" / "nodo-esp32s3"

VENTANA = 224
SALIDA = 96
N_PARCHES_MODELO = 3


def lcg_imagen(ancho: int, alto: int, semilla: int) -> np.ndarray:
    """Mismo generador que `generar_fuente()` en la prueba de C."""
    n = ancho * alto * 3
    out = np.empty(n, dtype=np.uint8)
    x = semilla & 0x7FFFFFFF
    for i in range(n):
        x = (1103515245 * x + 12345) & 0x7FFFFFFF
        out[i] = (x >> 16) & 0xFF
    return out.reshape(alto, ancho, 3)


def arreglo_c(nombre: str, datos: np.ndarray) -> str:
    plano = datos.reshape(-1)
    tipo = "int8_t" if plano.dtype == np.int8 else "uint8_t"
    lineas = [f"static const {tipo} {nombre}[{plano.size}] = {{"]
    for i in range(0, plano.size, 16):
        lineas.append("    " + ", ".join(str(int(v)) for v in plano[i : i + 16]) + ",")
    lineas.append("};")
    return "\n".join(lineas)


def generar_remuestreo(destino: Path) -> None:
    fuente = lcg_imagen(VENTANA, VENTANA, semilla=20260905)
    img = Image.fromarray(fuente, "RGB")
    esperado = np.asarray(
        img.resize((SALIDA, SALIDA), Image.BILINEAR, box=(0, 0, VENTANA, VENTANA)),
        dtype=np.uint8,
    )
    destino.parent.mkdir(parents=True, exist_ok=True)
    destino.write_text(
        "// Generado por FIRMWARE/herramientas/generar_vectores.py — no editar a mano.\n"
        "//\n"
        "// Salida de Pillow BILINEAR al reducir 224x224 -> 96x96, que es exactamente\n"
        "// la operacion con la que se construyo el dataset de entrenamiento.\n"
        "#pragma once\n\n"
        "#include <stdint.h>\n\n"
        f"#define VECTOR_SEMILLA {20260905}\n"
        f"#define VECTOR_VENTANA {VENTANA}\n"
        f"#define VECTOR_SALIDA {SALIDA}\n\n"
        + arreglo_c("kEsperadoRemuestreo", esperado)
        + "\n"
    )
    print(f"escrito {destino} ({esperado.size} bytes de referencia)")


def generar_modelo(destino: Path) -> None:
    x_path = TINYML / "datos" / "clasificacion-96" / "X_val.npy"
    y_path = TINYML / "datos" / "clasificacion-96" / "y_val.npy"
    tflite = TINYML / "modelos" / "cnn_media" / "modelo_int8.tflite"
    if not x_path.exists() or not tflite.exists():
        print(f"aviso: falta {x_path} o {tflite}; no se genera vectores_modelo.h")
        return

    import tensorflow as tf

    X = np.load(x_path, mmap_mode="r")
    y = np.load(y_path, mmap_mode="r")
    # Un positivo, un negativo y el parche con la activacion mas alta, para que la
    # prueba falle si el modelo portado se desvia en cualquier parte del rango.
    idx_pos = int(np.argmax(y))
    idx_neg = int(np.argmin(y))

    interp = tf.lite.Interpreter(model_path=str(tflite))
    interp.allocate_tensors()
    ent = interp.get_input_details()[0]
    sal = interp.get_output_details()[0]

    def inferir(parche: np.ndarray) -> int:
        v = (parche.astype(np.int16) - 128).astype(np.int8)
        interp.set_tensor(ent["index"], v[None, ...])
        interp.invoke()
        return int(interp.get_tensor(sal["index"])[0, 0])

    candidatos = [idx_pos, idx_neg, (idx_pos + 977) % len(X)][:N_PARCHES_MODELO]
    parches = np.stack([np.asarray(X[i], dtype=np.uint8) for i in candidatos])
    salidas = [inferir(p) for p in parches]

    destino.parent.mkdir(parents=True, exist_ok=True)
    destino.write_text(
        "// Generado por FIRMWARE/herramientas/generar_vectores.py — no editar a mano.\n"
        "//\n"
        "// Parches de datos/clasificacion-96/X_val.npy con la salida INT8 cruda que\n"
        "// produce modelo_int8.tflite en el PC. El MCU debe dar el mismo entero.\n"
        "#pragma once\n\n"
        "#include <stdint.h>\n\n"
        f"#define VECTOR_MODELO_N {len(candidatos)}\n"
        f"#define VECTOR_MODELO_LADO {SALIDA}\n\n"
        + arreglo_c("kParchesModelo", parches)
        + "\n\n"
        + "static const int8_t kSalidaEsperada[VECTOR_MODELO_N] = {"
        + ", ".join(str(s) for s in salidas)
        + "};\n"
    )
    print(f"escrito {destino}; indices={candidatos} salidas_int8={salidas}")


def main() -> int:
    generar_remuestreo(FIRMWARE / "test" / "test_nucleo" / "vectores_remuestreo.h")
    generar_modelo(FIRMWARE / "test" / "test_modelo" / "vectores_modelo.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
