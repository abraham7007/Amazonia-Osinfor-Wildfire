# Cadencia de captura y política de muestreo
### Recomendación con base en literatura

| Estado | Versión | Fecha | Responsable |
|---|---|---|---|
| Aprobado (recomendación) | 1.0 | 2026-06-11 | Área Técnica DSFFFS |

## 1. Problema

Definir **cada cuánto** el nodo captura una imagen y ejecuta inferencia. Es la variable que
gobierna el compromiso entre **latencia de detección** (qué tan pronto se ve la columna de
humo) y **consumo energético** (autonomía del nodo solar).

## 2. Evidencia de la literatura

| Hallazgo | Implicación para el diseño | Fuente |
|---|---|---|
| Los sistemas de cámara para detección temprana ciclan en **minutos** (≈2 min) y pueden detectar hasta **15 min antes**. | Cadencias muy largas (horas) sacrifican la ventaja de "detección temprana". | [R01], [R02] |
| El humo es el **primer signo visible y solo de día**. | Capturar **únicamente en ventana diurna** (~06:00–18:00) ahorra ~50 % sin perder capacidad. | [R02], [R04] |
| Nodos TinyML/solares emplean **ciclo de trabajo reducido**, baja resolución (96×96/160×160) y **encienden la radio solo tras detección de alta confianza**. | La captura+inferencia local es barata; la transmisión es lo caro → transmitir por evento, no continuo. | [R12], [R08] |
| El **muestreo adaptativo** (EcoWild) acorta el intervalo cuando el riesgo es alto y lo alarga cuando el riesgo o la batería bajan. | Política recomendada: intervalo variable por hora del día, condiciones y estado de batería. | [R06], [R11] |
| Con duty-cycle 0.33 % la vida del nodo supera **1 año**. | Confirma que el nodo de bajo consumo no es el limitante energético. | [R08] |

## 3. Hallazgo propio del modelo de energía

Según `03-diseno-hardware/analisis-de-potencia.md`, para las versiones **V1 (MCU TinyML)** y
**V2 (IA on-sensor)** el consumo está **dominado por el standby**, no por la captura:

| Versión | 1 h diurno | 3 h diurno | Adaptativo (~30/día) |
|---|---|---|---|
| V1 | 0,203 Wh/día | 0,197 Wh/día | 0,218 Wh/día |
| V2 | 0,304 Wh/día | 0,295 Wh/día | 0,326 Wh/día |

→ **Para V1/V2 capturar cada 1 h, cada 3 h o cada 15 min es energéticamente equivalente.**
La cadencia debe elegirse por **latencia de detección**, no por energía. Solo en **V3**
(inferencia costosa) el intervalo sí pesa (0,76 → 2,58 Wh/día) y conviene alargarlo o usar
disparo por evento.

## 4. Recomendación

1. **Captura solo diurna** (~06:00–18:00); nodo en deep-sleep de noche. [R02][R04]
2. **V1/V2 (recomendadas):** capturar a **intervalo corto (10–15 min)** en horario diurno —
   el costo energético es despreciable y reduce la latencia frente a 1–3 h. [R12]
3. **Política adaptativa** (objetivo de firmware): [R06][R11]
   - Ventana de **alto riesgo** (media mañana–tarde, estación seca, T alta / HR baja): 10–15 min.
   - Ventana de **bajo riesgo** o **batería < umbral**: alargar a 1–3 h.
   - Noche: dormir.
4. **Transmisión por evento:** enviar por LoRa solo ante detección de alta confianza + un
   **heartbeat diario** con telemetría (ver `protocolo-lorawan.md`). [R08]
5. **V3 (si se elige):** intervalo 1–3 h o disparo por pre-detector de bajo consumo; no
   mantener el NPU encendido de forma continua.

## 5. Decisión para el piloto (propuesta)

> Nodo **V2**, captura **diurna cada 15 min** con política adaptativa hacia 1 h en bajo
> riesgo/batería baja; transmisión por evento + 1 heartbeat/día. Proporciona baja latencia con autonomía suficiente. Confirmar en `ADR-001` y validar consumo real en EVT
> (`06-verificacion-validacion/pruebas-de-energia.md`).
