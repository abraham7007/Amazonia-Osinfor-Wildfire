# Contexto del Proyecto — Amazonía+ (Piloto OSINFOR · CCNN Paoyhan)

| | |
|---|---|
| **Título** | Validación de un Prototipo de Vigilancia Forestal mediante Edge AI y Participación Comunitaria para la Detección de Incendios en la Comunidad Nativa Paoyhan — Piloto — OSINFOR |
| **Entidad** | Organismo de Supervisión de los Recursos Forestales y de Fauna Silvestre (OSINFOR) |
| **Financiamiento** | 10 000 euros (apoyo gestionado vía COOPI); sin contrapartida |
| **Duración** | 6 meses |
| **Ubicación** | Comunidad Nativa Paoyhan, distrito de Padre Márquez, provincia de Ucayali, departamento de Loreto, Perú |
| **Contacto** | Williams Arellano Olano — warellano@osinfor.gob.pe |
| **Fuente** | Anexo 1 — Formulario de Solicitud de Apoyo (en esta misma carpeta) |

## 1. Problema

La vigilancia forestal en la Amazonía peruana depende de sistemas satelitales (FIRMS/NASA) que
detectan anomalías térmicas. Este enfoque falla para la detección **temprana**: la nubosidad
persistente de Loreto impide la detección durante días y la latencia de procesamiento (1 a 40
horas) genera una brecha temporal crítica; cuando el satélite detecta el evento, el incendio ya
suele tener una magnitud peligrosa e inasumible de suprimir.

OSINFOR integra la Red Amazónica de Manejo Integral del Fuego (RAMIF) y, desde su Laboratorio de
Innovación y Transformación Digital, impulsa un enfoque predictivo y preventivo. La intervención
se ubica en la CCNN Paoyhan (Título Habilitante 16-LOR-CON/PER-FMC-2024-001), zona de alta
prioridad por su manejo forestal y por proteger especies CITES (shihuahuaco *Dipteryx* spp.,
tahuari *Handroanthus* spp.) y amenazadas (quinilla, copaiba, lupuna, ishpingo).

## 2. Objetivos

**General:** validar un prototipo de detección temprana de incendios basado en Edge AI y LoRaWAN
en la CCNN Paoyhan, integrando un enfoque participativo con actores locales y OSINFOR.

**Específicos:**

1. Desarrollar y validar un **nodo sensor autónomo de bajo consumo** que integra un modelo de IA
   (TinyML) entrenado con datos híbridos para la detección local de humo, con conectividad LoRaWAN.
2. Ejecutar una fase de **validación en campo y transferencia de conocimiento**, capacitando al
   personal de la comunidad y representantes locales para fortalecer la respuesta temprana y la
   gobernanza forestal.

## 3. Resultados y actividades

**Resultado 1 — Nodo tecnológico validado (Edge AI + LoRaWAN):** prototipo funcional (TRL 6) con
procesamiento local, precisión del 85 % en detección de humo y transmisión autónoma de alertas a
5 km vía LoRaWAN y energía solar.

- Actividad 1.1: Entrenamiento del modelo TinyML con dataset híbrido (imágenes reales + sintéticas).
- Actividad 1.2: Ensamblaje y encapsulamiento IP67 del hardware del nodo con alimentación solar.
- Actividad 1.3: Pruebas de campo para validar la detección local y el alcance de la red LoRaWAN.

**Resultado 2 — Capacidades instaladas y modelo de escalabilidad:** 10 especialistas de OSINFOR y
la concesión capacitados, con una hoja de ruta técnica y financiera para la cobertura total del
área protegida.

- Actividad 2.1: Talleres de capacitación técnica para personal de OSINFOR, la concesión y actores locales.
- Actividad 2.2: Establecimiento de protocolos de comunicación de alertas tempranas con las comunidades.
- Actividad 2.3: Informe técnico final y hoja de ruta financiera para el escalamiento regional.

## 4. Beneficiarios

1. **OSINFOR** (beneficiario institucional): metodología validada y prototipo funcional para
   modernizar la vigilancia forestal.
2. **Población de la CCNN Paoyhan** (~2 000 habitantes) y empresas forestales.
3. **Comunidades nativas aledañas y concesiones** (~2 500 habitantes).

## 5. Componentes técnicos del sistema

- **Nodos de campo:** cámara + MCU Edge AI (ESP32-S3) con modelo TinyML de detección de humo;
  radio LoRa; alimentación solar (celda LiFePO4 + panel compacto); carcasa IP67 en árbol/poste.
- **Gateway LoRaWAN outdoor** (Milesight UG67) con backhaul 4G; alimentación solar (panel 50 W +
  batería LiFePO4 12,8 V 20 Ah).
- **Plataforma/servidor** que recibe y decodifica las alertas.

## 6. Estado de adquisiciones y documentación

- **Compra 1 (bienes):** equipos Edge AI + LoRaWAN — adquiridos.
- **Compra 2 (bienes):** kits solares + baterías — EETT cerrado (carpeta `GESTION/02-compras/02-paneles-solares-baterias`).
- **Diseño e ingeniería:** documentación técnica del nodo y gateway (carpeta `DISEÑO E INGENIERIA`).
- **Servicios (TDR):** dos servicios por definir a partir del **presupuesto detallado (Excel)** del
  proyecto. *Pendiente de cargar el archivo para identificarlos.*
