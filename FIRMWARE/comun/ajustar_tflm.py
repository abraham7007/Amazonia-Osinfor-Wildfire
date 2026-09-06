"""Ajusta esp-tflite-micro y esp-nn para que compilen bajo PlatformIO.

Se ejecuta antes de construir (extra_scripts) en las etapas que usan el modelo.

Hace falta porque las dos son componentes de ESP-IDF, no librerias de Arduino:
su CMakeLists elige una lista concreta de fuentes segun la arquitectura, mientras
que PlatformIO compila TODO lo que encuentra en la carpeta. El resultado es que
se intentan compilar ficheros que no corresponden a esta placa, o que dependen de
flags que solo pone el sistema de construccion de IDF.

Se escribe un library.json en cada una con el srcFilter adecuado.
"""

import json
import os

Import("env")  # noqa: F821  (lo inyecta PlatformIO)

BASE = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"))  # noqa: F821


def escribir(carpeta, nombre, excluidos):
    raiz = os.path.join(BASE, carpeta)
    if not os.path.isdir(raiz):
        return False
    manifiesto = {
        "name": nombre,
        "version": "1.0.0",
        # Al declarar un srcFilter explicito, PlatformIO deja de aplicar sus
        # exclusiones por defecto: hay que volver a apartar examples/.
        "build": {"srcFilter": ["+<*>"] + [f"-<{x}>" for x in excluidos]},
    }
    with open(os.path.join(raiz, "library.json"), "w") as f:
        json.dump(manifiesto, f, indent=2)
    return True


# ESP-NN trae una variante por arquitectura de cada nucleo de calculo. Las de
# RISC-V (esp_nn_*_riscv_pie.c) usan instrucciones PIE que el Xtensa del S3 no
# tiene, y su ensamblador no compila con este toolchain. El S3 usa las variantes
# _esp32s3 y las genericas _ansi, que si se conservan.
if escribir("esp-nn", "esp-nn", ["examples/", "*/*riscv*"]):
    print("ajustar_tflm: esp-nn sin las variantes RISC-V")

# esp-tflite-micro:
#   examples/   codigo de demostracion con su propio pinout de camara.
#   signal/     y microfrontend: procesado de audio (FFT, bancos de filtros,
#               ventanas). Un modelo de vision no usa nada de eso, y ademas
#               arrastra kissfft, que bajo PlatformIO no encuentra sus fuentes
#               ("kiss_fft.c: No such file or directory"). Se aparta tambien
#               third_party/kissfft, que solo existe para signal/. Excluirlo
#               acorta bastante la compilacion.
if escribir("esp-tflite-micro", "esp-tflite-micro",
            ["examples/", "signal/", "tensorflow/lite/experimental/",
             "third_party/kissfft/"]):
    print("ajustar_tflm: esp-tflite-micro sin examples/, signal/ ni microfrontend")
