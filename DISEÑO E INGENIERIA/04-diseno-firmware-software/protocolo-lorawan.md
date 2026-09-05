# Protocolo LoRaWAN y diseño del payload
### Nodo → Gateway UG67

| Estado | Versión | Fecha |
|---|---|---|
| En revisión | 0.9 | 2026-06-11 |

## 1. Parámetros de comunicación

| Parámetro | Valor propuesto | Nota |
|---|---|---|
| Banda | US915 | Región Perú |
| Clase | **A** | Mínimo consumo; el nodo habla y luego escucha 2 ventanas. [R08] |
| Activación | OTAA | Claves seguras, sin direcciones fijas. |
| ADR | Activado para nodos estáticos | Optimiza SF/potencia. |
| Radio | LoRa dedicada **integrada en la placa del nodo** (SX126x) | Confirmado por área usuaria. |

## 2. Principio de diseño del payload

- **La trama transmitida es binaria y compacta** (no texto): los papers de monitoreo de
  incendios usan payloads de **16–64 bytes** y duty-cycle 0,33 % para superar 1 año de vida
  [R08]. El **string legible** se reconstruye en el servidor con un *decoder*.
- **Los nodos son estáticos**: la **posición GPS se envía solo en el heartbeat/comisionado**,
  no en cada alerta → ahorra bytes y energía.
- Base de codificación: **Cayenne LPP** `[canal, tipo, dato]` (GPS 9 B, analógico 2 B) [R14][R15],
  o trama binaria propia (más compacta). Se documentan ambas.

## 3. Trama propia (recomendada) — compacta

### 3.1 Alerta de evento (uplink, FPort 10) — 6 bytes
| Offset | Campo | Bytes | Codificación |
|---|---|---|---|
| 0 | `node_id` | 1 | id lógico 0–255 (DevEUI identifica el físico) |
| 1 | `msg_type` | 1 | 0=heartbeat · 1=humo · 2=batería baja · 3=test |
| 2 | `confidence` | 1 | 0–100 (% de confianza/precisión del modelo) |
| 3 | `battery` | 1 | SoC 0–100 % (o (V−9,0)·20) |
| 4 | `temp` | 1 | °C con offset +40 (−40…+87) |
| 5 | `flags` | 1 | bit0=alarma activa, bit1=tamper, bit2=error cámara… |

### 3.2 Heartbeat diario (uplink, FPort 11) — 14 bytes
| Campo | Bytes | Codificación |
|---|---|---|
| `node_id` | 1 | |
| `msg_type=0` | 1 | |
| `lat` | 4 | int32, grados ×1e-7 |
| `lon` | 4 | int32, grados ×1e-7 |
| `battery` | 1 | SoC % |
| `temp` | 1 | °C +40 |
| `humidity` | 1 | % 0–100 |
| `fw_ver` | 1 | versión firmware |

## 4. Equivalente Cayenne LPP (interoperable) [R14]

`[ch1, GPS(0x88), lat, lon, alt] [ch2, ANALOG(0x02), confianza] [ch3, ANALOG(0x02), batería] [ch4, TEMP(0x67), temp]`
— ~17 bytes; útil si se integra con plataformas que ya decodifican LPP.

## 5. String de aplicación (decodificado en el servidor)

El servidor LoRaWAN convierte la trama binaria en un registro legible/JSON, por ejemplo:

```
NODE03 | EVT=HUMO | CONF=0.92 | LAT=-6.12345 | LON=-75.12345 | BAT=87% | T=31C | HR=86% | FW=1.0
```
```json
{ "node_id": 3, "evento": "humo", "confianza": 0.92,
  "lat": -6.12345, "lon": -75.12345, "bateria_pct": 87,
  "temp_c": 31, "hr_pct": 86, "fw": "1.0", "ts": "2026-06-11T13:05:00Z" }
```

## 6. Justificación

El esquema "binario por aire + decodificación en servidor", el envío de GPS solo en
heartbeat y la transmisión por evento siguen las prácticas reportadas en sistemas LoRa de
monitoreo de incendios [R05][R07][R08][R09] y el estándar Cayenne LPP [R14][R15]. Pendiente:
definir el *decoder* (JS) en el servidor y los umbrales de `confidence` para disparar alarma.
