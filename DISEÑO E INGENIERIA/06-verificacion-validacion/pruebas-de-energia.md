# Protocolo de pruebas de energía
### Verificación del consumo real — Nodo (V1/V2/V3) y Gateway UG67

| Estado | Versión | Fecha | Responsable |
|---|---|---|---|
| Aprobado | 1.0 | 2026-06-11 | Área Técnica DSFFFS |

> **Propósito.** Medir el consumo real de cada subsistema y modo para **validar el modelo de
> energía** (`../03-diseno-hardware/analisis-de-potencia.md`) y **cerrar el dimensionamiento
> del sistema fotovoltaico**. Verifica el requisito **REQ-SYS-004** (autonomía).

## 1. Alcance
Mide V1 (MCU TinyML), V2 (IA on-sensor), V3 (NPU) y el gateway UG67, en cada **modo de
operación**: deep-sleep, despertar, captura, inferencia y transmisión LoRa.

## 2. Instrumentos
| Instrumento | Uso | Nota |
|---|---|---|
| Analizador de potencia / **Nordic PPK2** o equivalente | Corriente µA–A con registro temporal | Adecuado para perfiles de deep-sleep y picos |
| Módulo **INA226/INA219** + datalogger (o DAQ) | Medición continua de I y V en el riel | Bajo costo; registro a ≥10 Hz |
| Fuente DC programable | Alimentar a tensión fija (12,8 V / 5 V / 3,3 V) | Sustituye batería en banco |
| Multímetro true-RMS con registro | Verificación puntual | Respaldo |
| **Medidor de energía USB** (Wh) | Verificación integrada del gateway | UG67 vía PoE/DC |
| Pinza amperimétrica DC | Gateway en sitio | Campo |
| (Opcional) Piranómetro | Validar HSP local | Correlación con generación solar |

## 3. Magnitudes
Corriente promedio y pico por modo (mA), tensión de riel (V), **energía por evento** (Wh),
potencia de standby (W) y **Wh/día integrado** (medición de 24 h).

## 4. Procedimiento
### 4.1 Por modo (cada versión)
1. **Deep-sleep:** medir corriente de reposo durante ≥10 min; registrar promedio.
2. **Despertar + captura:** disparar un ciclo; registrar perfil I(t) y energía del evento.
3. **Inferencia:** aislar el tiempo/energía de inferencia (marca de tiempo por GPIO).
4. **Transmisión LoRa:** registrar pico y energía de una TX a SF y potencia objetivo.
5. **24 h integrado:** dejar el nodo operando con la cadencia objetivo (diurna) y medir Wh reales.

### 4.2 Gateway UG67
Medir consumo en idle, con tráfico LoRa y con backhaul 4G activo; integrar 24 h.

### 4.3 Condiciones
Repetir a temperatura ambiente y a ~35 °C (cámara/sol) para capturar el efecto térmico.
Tensión de batería al 100 % y al 50 % de SoC.

## 5. Criterios de aceptación
| Criterio | Meta |
|---|---|
| Desviación medido vs modelo (Wh/día) | ≤ ±25 % (si mayor, recalibrar `energy_model.py`) |
| Autonomía verificada del gateway | ≥ 2,5 días sin sol (REQ-SYS-004) |
| Recuperación de batería tras descarga | Carga completa en ≤ 3 días soleados |

## 6. Registro
Usar la plantilla **`plantilla-registro-energia.csv`** (una fila por medición). Adjuntar los
perfiles I(t) exportados del instrumento. Volcar los Wh/día medidos al modelo y recalcular el
dimensionamiento.

## 7. Casos de prueba
| ID | Qué mide | Requisito |
|---|---|---|
| TC-ENE-01 | Deep-sleep por versión | REQ-SYS-004 |
| TC-ENE-02 | Energía por evento (captura+inferencia) | REQ-SYS-004 |
| TC-ENE-03 | Energía de TX LoRa | REQ-SYS-004 |
| TC-ENE-04 | Wh/día integrado 24 h (nodo) | REQ-SYS-004 |
| TC-ENE-05 | Wh/día integrado 24 h (gateway) | REQ-SYS-004 |
| TC-ENE-06 | Autonomía y recuperación de batería | REQ-SYS-004 |

## 8. Salida
Tabla de consumos validados → actualización de `analisis-de-potencia.md` y **decisión final de
specs de panel/batería/MPPT del sistema fotovoltaico**.
