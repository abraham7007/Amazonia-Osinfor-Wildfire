# Referencias y bibliografía

> Fuentes que respaldan las decisiones de diseño (cadencia de captura, payload LoRa,
> arquitectura energética). Citar con el identificador `[Rxx]` en los documentos.

## Detección temprana de incendios / humo

- **[R01]** Cal Poly — *Early Wildfire Detection System*. Cámaras PTZ que recorren 8 posiciones; ~15 s por posición, ráfaga a 1 fps, ciclo completo ~2 min. https://digitalcommons.calpoly.edu/cgi/viewcontent.cgi?article=1616&context=eesp
- **[R02]** Fernandes, Utkin, Chaves (2023). *Automatic early detection of wildfire smoke with visible-light cameras and EfficientDet*. https://journals.sagepub.com/doi/10.1177/07349041231163451
- **[R03]** *PyroNear-2025: A Real-World Benchmark for Early Wildfire Detection* (arXiv:2402.05349). https://arxiv.org/abs/2402.05349
- **[R04]** *Spatio-Temporal Data Model for Early Wildfire Detection*, Fire 9(4):175. https://doi.org/10.3390/fire9040175

## IoT + LoRa para incendios forestales

- **[R05]** *ForestProtector: An IoT Architecture Integrating Machine Vision and Deep Reinforcement Learning for Efficient Wildfire Monitoring* (arXiv:2501.09926). Nodos LoRa (TTGO LoRa32) + gateway con visión; RL para orientar la cámara. https://arxiv.org/abs/2501.09926
- **[R06]** *EcoWild: Reinforcement Learning for Energy-Aware Wildfire Detection in Remote Environments*, Sensors 25(19):6011. Muestreo adaptativo según riesgo y batería. https://doi.org/10.3390/s25196011
- **[R07]** *The Development of a Wildfire Early Warning System Using LoRa Technology*, Computers 15(2):105. https://www.mdpi.com/2073-431X/15/2/105
- **[R08]** *LoRaWAN Network for Fire Monitoring in Rural Environments*, Electronics 9(3):531. Duty-cycle 0.33 % → vida >1 año; payloads compactos. https://www.mdpi.com/2079-9292/9/3/531
- **[R09]** *Towards Smart Wildfire Prevention: Development of a LoRa-Based IoT Node for Environmental Hazard Detection*, Designs 9(4):91. https://www.mdpi.com/2411-9660/9/4/91
- **[R10]** *Forest Fire Monitoring and Energy Optimization Based on LoRa-Mesh*, Electronics 14(21):4135. https://www.mdpi.com/2079-9292/14/21/4135

## Gestión de energía / TinyML

- **[R11]** Hsu et al. *Adaptive Duty Cycling for Energy Harvesting Systems*, ISLPED 2006 (operación energéticamente neutra). https://www.microsoft.com/en-us/research/wp-content/uploads/2006/01/hsuISLPED06.pdf
- **[R12]** *An Ultra-low Power TinyML System for Real-time Visual Processing at Edge* (arXiv:2207.04663). Captura a 96×96/160×160 en escala de grises. https://arxiv.org/pdf/2207.04663
- **[R13]** *Energy-Aware Multi-Exit TinyML for Smart Zero-Energy Devices* (arXiv:2603.08047). https://arxiv.org/pdf/2603.08047

## Estándares de payload

- **[R14]** myDevices — *Cayenne Low Power Payload (LPP)*. Formato `[canal, tipo, dato]`; GPS 9 B, analógico 2 B; compatible desde 11 B. https://docs.mydevices.com/docs/lorawan/cayenne-lpp
- **[R15]** The Things Industries — *CayenneLPP payload formatter*. https://www.thethingsindustries.com/docs/integrations/payload-formatters/cayenne/


## Modelo TinyML / datasets de humo

- **[R16]** Dewangan et al. *FIgLib & SmokeyNet: Dataset and Deep Learning Model for Real-Time Wildland Fire Smoke Detection* (arXiv:2112.08598). ~24 800 imágenes de cámaras fijas (HPWREN). https://arxiv.org/pdf/2112.08598
- **[R18]** *Boreal Forest Fire: UAV-collected Wildfire Detection and Smoke Segmentation Dataset*, Scientific Data (2025). https://www.nature.com/articles/s41597-025-05634-0
- **[R19]** *Image-based Early Detection System for Wildfires* (arXiv:2211.01629). https://arxiv.org/pdf/2211.01629
- **[R20]** *Wildfire Smoke Detection System: Model Architecture, Training Mechanism, and Dataset* (arXiv:2311.10116). https://arxiv.org/pdf/2311.10116
- **[R21]** *Rethinking Temporal Models for TinyML: LSTM versus 1D-CNN in Resource-Constrained Devices* (arXiv:2603.04860). https://arxiv.org/html/2603.04860
- **[R22]** *Integrating Color and Contour Analysis with Deep Learning for Robust Fire and Smoke Detection*, PMC11991653. ~89,6 % recall, ~82,9 % precisión, ~7,5 % FPR. https://www.ncbi.nlm.nih.gov/pmc/articles/PMC11991653/

## Metodología de documentación (cómo documentar este tipo de proyectos)

- **[R23]** INCOSE. *Systems Engineering Handbook*, 5.ª ed., Wiley, 2023 (ISBN 9781119814290). https://www.incose.org/resources-publications/technical-publications/se-handbook
- **[R24]** ISO/IEC/IEEE 15288:2023 — *System life cycle processes*.
- **[R25]** ISO/IEC/IEEE 29148:2018 — *Requirements engineering*.
- **[R26]** ISO/IEC/IEEE 15289:2019 — *Content of life-cycle information items (documentation)*.
- **[R27]** NASA. *Systems Engineering Handbook* (SP-2016-6105 Rev2) — revisiones PDR/CDR. https://www.nasa.gov/seh/
- **[R28]** White, E. *Making Embedded Systems*, O'Reilly. https://www.oreilly.com/library/view/making-embedded-systems/9781098151539/
- **[R29]** Clements, P. et al. *Documenting Software Architectures: Views and Beyond*, 2.ª ed., Addison-Wesley.
- **[R30]** Starke & Hruschka — *arc42* (https://arc42.org/); Brown, S. — *C4 model* (https://c4model.com/).
- **[R31]** Nygard, M. *Documenting Architecture Decisions* (ADR), 2011. https://www.cognitect.com/blog/2011/11/15/documenting-architecture-decisions
- **[R32]** Bhatti, J. et al. *Docs for Developers*, Apress. https://docsfordevelopers.com/
- **[R33]** Google. *Technical Writing courses*. https://developers.google.com/tech-writing
- **[R34]** IEEE 1016 — *Software Design Descriptions*.
