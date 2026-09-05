# Cuaderno de campo — validación del nodo

| Estado | Versión | Fecha |
|---|---|---|
| Para uso en campo | 1.1 (versión aligerada) | 2026-09-02 |

Dos versiones con el mismo contenido, 7 páginas A4 cada una:

| Archivo | Para qué | Se genera con |
|---|---|---|
| `Cuaderno-de-campo-validacion-nodo.pdf` | Imprimir y llenar a lápiz en campo | `generar_cuaderno.py` (reportlab) |
| `Cuaderno-de-campo-validacion-nodo.docx` | Editar en Word antes de imprimir | `generar_cuaderno_docx.py` (python-docx) |

El DOCX usa tablas reales de Word con **layout fijo**, así que se pueden añadir o quitar filas y
columnas sin que se descuadre la hoja. Si se edita el DOCX a mano, el PDF deja de estar
sincronizado: conviene decidir cuál de los dos es la fuente y regenerar el otro.

## Qué contiene y qué requisito verifica

| Formato | Verifica | Copias |
|---|---|---|
| **F-01** Ficha del punto de instalación y puesta en marcha | REQ-SYS-003, REQ-SYS-006 | 1 por nodo |
| **F-02** Prueba de cobertura LoRaWAN (walk test) | REQ-SYS-003 | 2 |
| **F-03** Ensayo de detección de humo | REQ-SYS-001, REQ-SYS-008 | 3 |
| **F-04** Bitácora de alertas y falsos positivos | REQ-SYS-001 | 4 |
| **F-05** Registro de energía y ambiente | REQ-SYS-004, REQ-SYS-005 | 4 |
| **F-06** Inspección física e IP67 | REQ-SYS-006, REQ-SYS-007 | 2 por nodo |
| **F-07** Cierre de jornada | trazabilidad | 1 por día |

F-06 y F-07 comparten hoja.

### Qué se dejó fuera y por qué

La versión 1.0 pedía unos 90 datos por campaña; esta pide cerca de 55. Se quitó todo lo que no
cierra un requisito: especie y DAP del árbol, pendiente y orientación de la ladera, distancia al
claro, azimut de cada punto del walk test, altura de la columna de humo, corriente de carga
(exige instrumento), humedad relativa (exige sensor) y el desglose de la línea de vista punto por
punto. El porcentaje de pérdida del walk test ya no se calcula en campo: se anotan las tramas que
llegaron de 10 y la cuenta se hace después. La temperatura sale del heartbeat, no de un
termómetro aparte. El checklist de IP67 pasó de 16 a 8 puntos agrupando los que se revisan
juntos.

## Criterios que se contrastan en campo

- **Detección:** meta de 85 % (Resultado 1 del proyecto). Se calcula en el resumen de F-03.
- **Cobertura:** enlace útil a 5 km. Se cierra en el resultado de F-02 con el criterio PER ≤ 10 %.
- **Autonomía:** 2–3 días sin sol (REQ-SYS-004). Se cierra en el bloque final de F-05.
- **Latencia:** menor que la satelital, que va de 1 a 40 h (REQ-SYS-008). F-03 mide minutos entre
  la generación del humo y la alerta.
- **Falsos positivos:** la niebla matinal y la nube baja sobre el dosel son el riesgo principal del
  modelo según `04-diseno-firmware-software/modelo-edge-ai.md`. F-04 los registra con su causa
  probable, que es el insumo para el *fine-tuning* con dataset local.

## Coherencia con el diseño ya documentado

- Los campos de F-01 (RSSI, SNR, SF, join OTAA) y de F-04 (tipo de mensaje, confianza, batería)
  corresponden al payload definido en `protocolo-lorawan.md`: `node_id`, `msg_type`
  (0 heartbeat · 1 humo · 2 batería baja · 3 test), `confidence`, `battery`, `temp`, `flags`.
- La posición GPS se registra en papel porque, según ese mismo documento, el nodo solo la
  transmite en el heartbeat de comisionado.
- F-05 complementa el protocolo de banco `pruebas-de-energia.md`: aquel mide con instrumento en
  laboratorio, este toma lecturas reales en campo para contrastar el modelo `energy_model.py`.

## Después de la campaña

El cuaderno llenado es la evidencia primaria del informe. Digitalizar por fotografía y pasar
F-02, F-03, F-04 y F-05 a hoja de cálculo permite cerrar `validacion-en-campo.md`, que hoy sigue
en estado 🔴 en `SEGUIMIENTO.md`.

## Regenerar

```
python3 generar_cuaderno.py
```
Editar el script si hay que añadir filas, cambiar columnas o ajustar los criterios de aceptación.
