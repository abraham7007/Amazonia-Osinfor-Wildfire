# Guía de documentación del proyecto — referencias y método

| Estado | Versión | Fecha |
|---|---|---|
| Aprobado | 1.0 | 2026-06-11 |

> Reúne los **estándares y libros de referencia** sobre cómo documentar proyectos que combinan
> hardware, firmware y sistemas, y mapea cada uno con la forma en que ya está organizada esta
> documentación. Objetivo: avanzar de forma incremental sin perder trazabilidad.

## 1. Marco adoptado y su procedencia

El método de este repositorio —**V-Model + fases NPI + docs-as-code + ADR**— no es arbitrario;
combina prácticas establecidas:

- **Ciclo de vida e ingeniería de sistemas:** INCOSE SE Handbook [R23] e ISO/IEC/IEEE 15288 [R24].
- **Qué documentos deben existir:** ISO/IEC/IEEE 15289 (ítems de información del ciclo de vida) [R26].
- **Requisitos verificables y trazables:** ISO/IEC/IEEE 29148 [R25].
- **Compuertas de revisión (PDR/CDR ≈ EVT/DVT/PVT):** prácticas de NASA/aeroespacial [R27].
- **Arquitectura y diagramas:** "Documenting Software Architectures" [R29], arc42 y C4 [R30].
- **Decisiones:** Architecture Decision Records (Nygard) [R31].
- **Firmware/embebido:** "Making Embedded Systems" [R28].
- **Escritura técnica y docs-as-code:** "Docs for Developers" [R32] y guías de Google [R33].

## 2. Referencias por disciplina

| Disciplina | Referencia | Qué aporta | Dónde lo aplicamos |
|---|---|---|---|
| Sistemas / ciclo de vida | INCOSE SE Handbook, 5.ª ed. [R23]; ISO 15288 [R24] | Procesos del ciclo de vida, V-Model | Estructura de fases 01→08 |
| Documentos requeridos | ISO 15289 [R26] | Lista de ítems de información a generar | Catálogo del tablero de seguimiento |
| Requisitos | ISO 29148 [R25] | Cómo redactar requisitos verificables y trazables | Carpeta de requisitos + matriz de trazabilidad |
| Revisiones de diseño | NASA SE Handbook [R27] | Compuertas PDR/CDR con criterios de entrada/salida | Compuertas EVT/DVT/PVT |
| Arquitectura | Clements et al. [R29]; arc42 + C4 [R30] | Vistas, plantilla y diagramas de bloques/contenedores | Carpeta de arquitectura y diagramas |
| Decisiones | ADR — Nygard [R31] | Registrar decisiones de forma ligera e inmutable | Registro de decisiones (ADR) |
| Firmware | White, "Making Embedded Systems" [R28] | Patrones de diseño, gestión de energía, OTA | Carpeta de firmware/software |
| Diseño de software | IEEE 1016 (SDD) [R34] | Estructura de la descripción de diseño | Arquitectura de firmware |
| Escritura técnica | "Docs for Developers" [R32]; Google [R33] | Cómo escribir, medir y mantener documentación | Convenciones del repositorio |

## 3. Checklist mínimo para no perder avances

Por cada avance (un cálculo, una prueba, un cambio de diseño), registrar:

1. **Qué requisito** afecta o satisface (ID `REQ-…`).
2. **Qué decisión** se tomó, si aplica → nuevo `ADR-…`.
3. **La evidencia o dato** generado (medición, cálculo, gráfico) en su carpeta de disciplina.
4. **Qué prueba** lo valida (ID `TC-…`) y su resultado.
5. **Actualizar el estado** en el tablero de seguimiento.

Si los cinco puntos quedan registrados, ningún avance se pierde y la trazabilidad
requisito → diseño → prueba se mantiene intacta.

## 4. Lectura prioritaria

1. **"Docs for Developers" [R32]** — práctica inmediata de cómo escribir y mantener la documentación.
2. **arc42 + C4 [R30]** — plantilla y diagramas de arquitectura (gratuitos en línea).
3. **INCOSE SE Handbook [R23]** (capítulos de procesos y revisiones) — marco de ciclo de vida.
4. **"Making Embedded Systems" [R28]** — referencia para la fase de firmware.

## Referencias

- [R23] INCOSE. *Systems Engineering Handbook*, 5.ª ed., Wiley, 2023 (ISBN 9781119814290). https://www.incose.org/resources-publications/technical-publications/se-handbook
- [R24] ISO/IEC/IEEE 15288:2023 — *Systems and software engineering — System life cycle processes*.
- [R25] ISO/IEC/IEEE 29148:2018 — *Requirements engineering*.
- [R26] ISO/IEC/IEEE 15289:2019 — *Content of life-cycle information items (documentation)*.
- [R27] NASA. *Systems Engineering Handbook* (SP-2016-6105 Rev2) — revisiones de diseño PDR/CDR. https://www.nasa.gov/seh/
- [R28] White, E. *Making Embedded Systems: Design Patterns for Great Software*, O'Reilly. https://www.oreilly.com/library/view/making-embedded-systems/9781098151539/
- [R29] Clements, P. et al. *Documenting Software Architectures: Views and Beyond*, 2.ª ed., Addison-Wesley.
- [R30] Starke & Hruschka — *arc42* (https://arc42.org/); Brown, S. — *C4 model* (https://c4model.com/).
- [R31] Nygard, M. *Documenting Architecture Decisions* (ADR), 2011. https://www.cognitect.com/blog/2011/11/15/documenting-architecture-decisions
- [R32] Bhatti, J. et al. *Docs for Developers: An Engineer's Field Guide to Technical Writing*, Apress. https://docsfordevelopers.com/
- [R33] Google. *Technical Writing courses*. https://developers.google.com/tech-writing
- [R34] IEEE 1016 — *Software Design Descriptions*.
