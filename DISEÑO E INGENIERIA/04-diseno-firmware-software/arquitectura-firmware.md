# Arquitectura de Firmware

| Estado | Fecha |
|---|---|
| Borrador | 2026-06-11 |

> **Propósito.** Estructura del firmware del nodo (tareas, máquina de estados, scheduler de energía).

## Qué documentar aquí

- Máquina de estados: sleep → wake → captura → inferencia → TX → sleep
- RTOS vs bare-metal
- Gestión de watchdog y recuperación
- Particionado de memoria y OTA
