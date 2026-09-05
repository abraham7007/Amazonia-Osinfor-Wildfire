# Arquitectura de Sistema

| Estado | Versión | Fecha |
|---|---|---|
| En revisión | 0.2 | 2026-06-11 |

## Diagrama de bloques

```mermaid
flowchart TB
  subgraph CAMPO["Nodos de campo (xN)"]
    CAM["Cámara<br/>(OV5640 / IMX500 / Himax)"] --> MCU["MCU Edge AI<br/>ESP32-S3 / Vision AI"]
    MCU --> LORA["Radio LoRa<br/>(SX126x)"]
    SOLN["Panel + Batería + MPPT<br/>(nodo)"] -.alimenta.-> MCU
  end
  LORA -- "LoRaWAN" --> GW["Gateway Milesight UG67<br/>(4G / WiFi / Ethernet)"]
  subgraph ESTACION["Estación gateway"]
    GW --> AGG["(opcional) Agregador Edge AI<br/>RPi5+Hailo"]
    SOLG["Panel + Batería + MPPT<br/>(gateway)"] -.alimenta.-> GW
  end
  GW -- "4G / Internet" --> NUBE["Servidor LoRaWAN /<br/>Plataforma OSINFOR"]
```

## Familias de nodo (decisión en ADR-001)

- **Nodo A — bajo consumo:** detección on-sensor (Himax/IMX500) + ESP32-S3 + LoRa. ~0,3 W.
- **Nodo B — alto desempeño:** RPi5+Hailo con inferencia continua. ~8–10 W.

## Partición Hardware / Firmware / Software

| Capa | Responsabilidad |
|---|---|
| Hardware | Sensado, energía, radio, gabinete (ver `03-diseno-hardware/`) |
| Firmware | Captura, inferencia TinyML, gestión de energía, pila LoRaWAN (ver `04-...`) |
| Software/Backend | Servidor LoRaWAN, ingestión y visualización (fuera de alcance HW) |
