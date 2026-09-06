# Pruebas del núcleo en el PC

25 comprobaciones que no necesitan placa. Cubren lo que, si se rompe, deja el
nodo funcionando pero **detectando mal** —el fallo caro, porque no da síntomas
en campo—.

```bash
cd FIRMWARE/pruebas-host
pio test -e host
```

| Bloque | Qué fija |
|---|---|
| Rejilla de teselas | Que 1280×720 dé exactamente las 32 esquinas de la evaluación del modelo, en el mismo orden, y que la última llegue al borde |
| **Remuestreo** | Que la reducción 224→96 sea **idéntica byte a byte** a `Image.resize(BILINEAR)` de Pillow, con el que se generaron los parches de entrenamiento |
| Payload LoRaWAN | Que los 6 y 14 bytes coincidan con el decoder del servidor, incluidas las coordenadas negativas |
| Política de energía | Ventana diurna, cadencia adaptativa y umbrales de batería |

El remuestreo es el que justifica todo esto. Al **reducir**, Pillow no interpola
cuatro vecinos: escala el soporte del filtro por el factor de reducción
(224/96 = 2,33), de modo que cada píxel de salida promedia siete de entrada por
eje. Un bilineal ingenuo de 2×2 descartaría el 80 % de los píxeles y metería
aliasing justo sobre las columnas de humo tenues, que son el caso que el nodo
debe detectar. La prueba compara contra vectores generados con Pillow:

```
0/27648 bytes distintos de Pillow, peor desvio 0
```

Regenerar los vectores tras cambiar de modelo o de geometría:

```bash
MODELO-TINYML/.venv/bin/python FIRMWARE/herramientas/generar_vectores.py
```

También se puede compilar sin PlatformIO:

```bash
g++ -std=c++17 -I../comun/librerias/nucleo/src -Itest/test_nucleo \
    test/test_nucleo/main.cpp ../comun/librerias/nucleo/src/{teselado,remuestreo,payload,energia}.cpp \
    -o /tmp/test && /tmp/test
```
