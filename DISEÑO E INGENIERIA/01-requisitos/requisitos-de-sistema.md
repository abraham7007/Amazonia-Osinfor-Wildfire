# Requisitos de Sistema

| Estado | Versión | Fecha | Responsable |
|---|---|---|---|
| En revisión | 0.2 | 2026-06-11 | Área Técnica DSFFFS |

> Derivados de las EETT del sistema Edge AI + LoRaWAN. Cada requisito es verificable y
> trazable (ver `matriz-trazabilidad.md`).

| ID | Requisito | Origen | Verificación |
|---|---|---|---|
| REQ-SYS-001 | El sistema detectará tempranamente humo/incendio mediante visión por computadora en el borde (Edge AI). | EETT 2 Objetivos | Validación en campo |
| REQ-SYS-002 | El procesamiento de eventos se realizará localmente (sin depender de la nube). | EETT 2 | Prueba de integración |
| REQ-SYS-003 | Los nodos transmitirán por LoRaWAN en zonas remotas sin red eléctrica ni celular fiable. | EETT 2 | Prueba de cobertura |
| REQ-SYS-004 | El sistema operará de forma autónoma con energía solar, con autonomía de 2–3 días sin sol. | EETT 2 Objetivos | Prueba de energía |
| REQ-SYS-005 | Operación en clima amazónico: humedad >80 %, lluvia frecuente, temperatura ≥30 °C. | EETT 2 | Ensayo ambiental |
| REQ-SYS-006 | Grado de protección IP67 en gabinetes outdoor. | EETT ítem carcasas | Ensayo IP |
| REQ-SYS-007 | Bajo mantenimiento; solución modular y replicable por nodo. | EETT 2 | Revisión de diseño |
| REQ-SYS-008 | Latencia de detección menor que soluciones satelitales. | EETT 2 | Medición |
