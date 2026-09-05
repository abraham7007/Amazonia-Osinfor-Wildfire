# Registro de Decisiones de Arquitectura (ADR)

> Cada decisión relevante se registra aquí. No se borran; se marcan como *Reemplazada*.

---

## ADR-001 — Selección de la versión de nodo para el piloto (V1 / V2 / V3)

- **Estado:** 🟡 Propuesta (pendiente de aprobación, tras EVT)
- **Fecha:** 2026-06-11
- **Contexto:** El kit de validación permite construir 3 versiones; el piloto desplegará
  una. La energía **no** es el factor decisivo entre V1 y V2 (ambas ~0,2–0,3 Wh/día,
  dominadas por standby); el factor decisivo es el **desempeño del modelo TinyML** de
  detección de columnas de humo y la **tasa de falsos positivos** con niebla/nubes.
  Ver `comparacion-versiones.md` y `analisis-de-potencia.md`.
- **Opciones:**
  1. **V1 · MCU TinyML** — mínimo costo/energía; modelo más limitado.
  2. **V2 · IA on-sensor** (Himax/IMX500) — mejor balance precisión/energía; firmware simple.
  3. **V3 · NPU (RPi5+Hailo)** — máxima precisión; solo viable con disparo por evento (no always-on).
- **Decisión:** _(pendiente — recomendación técnica: **V2** como candidata principal, **V1**
  respaldo, **V3** benchmark; confirmar con medición de desempeño en EVT)._
- **Consecuencias:** Define el BOM final del nodo, la arquitectura de firmware, el modelo
  TinyML y las specs finales del sistema fotovoltaico del nodo.

---

## ADR-000 — Metodología de documentación (V-Model + NPI, docs-as-code)

- **Estado:** 🟢 Aprobada
- **Fecha:** 2026-06-11
- **Decisión:** Adoptar V-Model + fases NPI con documentación en Markdown versionable y
  export a Word/PDF para entregables formales. Ver `README.md`.
