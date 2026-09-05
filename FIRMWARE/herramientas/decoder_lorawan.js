/**
 * Payload formatter del nodo de humo — Amazonia+ / Piloto OSINFOR (CCNN Paoyhan).
 *
 * Cierra el pendiente de `protocolo-lorawan.md` §6: reconstruye en el servidor el
 * registro legible a partir de la trama binaria de 6 o 14 bytes.
 *
 * Compatible con ChirpStack v4 (decodeUplink) y con The Things Stack. Se pega tal
 * cual en el "payload codec" de la aplicacion.
 *
 * La contraparte de este fichero es FIRMWARE/nodo-esp32s3/src/payload.cpp. Si se
 * cambia una, hay que cambiar la otra: la prueba de test_nucleo fija los bytes.
 */

var FPORT_ALERTA = 10;
var FPORT_HEARTBEAT = 11;

var EVENTOS = { 0: "heartbeat", 1: "humo", 2: "bateria_baja", 3: "test" };

function leerInt32BE(b, i) {
  var v = (b[i] << 24) | (b[i + 1] << 16) | (b[i + 2] << 8) | b[i + 3];
  return v | 0; // fuerza el complemento a dos: las coordenadas del sur son negativas
}

function decodificarBanderas(f) {
  return {
    alarma_activa: (f & 0x01) !== 0,
    tamper: (f & 0x02) !== 0,
    error_camara: (f & 0x04) !== 0,
    error_modelo: (f & 0x08) !== 0,
    modo_ahorro: (f & 0x10) !== 0,
  };
}

function decodificarAlerta(b) {
  if (b.length < 6) throw new Error("alerta de " + b.length + " bytes, se esperaban 6");
  var tipo = b[1];
  return {
    node_id: b[0],
    evento: EVENTOS[tipo] !== undefined ? EVENTOS[tipo] : "desconocido_" + tipo,
    confianza: b[2] / 100,
    bateria_pct: b[3],
    temp_c: b[4] - 40,
    flags: decodificarBanderas(b[5]),
  };
}

function decodificarHeartbeat(b) {
  if (b.length < 14) throw new Error("heartbeat de " + b.length + " bytes, se esperaban 14");
  return {
    node_id: b[0],
    evento: EVENTOS[b[1]] !== undefined ? EVENTOS[b[1]] : "desconocido_" + b[1],
    lat: leerInt32BE(b, 2) / 1e7,
    lon: leerInt32BE(b, 6) / 1e7,
    bateria_pct: b[10],
    temp_c: b[11] - 40,
    hr_pct: b[12],
    fw: String(b[13]),
  };
}

/** Cadena legible de §5 del documento de protocolo. */
function comoTexto(d) {
  var partes = [
    "NODE" + ("0" + d.node_id).slice(-2),
    "EVT=" + String(d.evento).toUpperCase(),
  ];
  if (d.confianza !== undefined) partes.push("CONF=" + d.confianza.toFixed(2));
  if (d.lat !== undefined) partes.push("LAT=" + d.lat.toFixed(5), "LON=" + d.lon.toFixed(5));
  partes.push("BAT=" + d.bateria_pct + "%", "T=" + d.temp_c + "C");
  if (d.hr_pct !== undefined) partes.push("HR=" + d.hr_pct + "%");
  if (d.fw !== undefined) partes.push("FW=" + d.fw);
  return partes.join(" | ");
}

function decodeUplink(input) {
  var b = input.bytes;
  var puerto = input.fPort;
  var avisos = [];
  var datos;

  try {
    if (puerto === FPORT_ALERTA) {
      datos = decodificarAlerta(b);
    } else if (puerto === FPORT_HEARTBEAT) {
      datos = decodificarHeartbeat(b);
    } else {
      return { data: {}, warnings: [], errors: ["FPort " + puerto + " no reconocido"] };
    }
  } catch (e) {
    return { data: {}, warnings: [], errors: [String(e.message || e)] };
  }

  // El nodo alerta cuando la probabilidad de la mejor tesela supera 0,97
  // (evaluacion_por_cuadro_sdis-77.json). Una confianza sensiblemente menor en un
  // FPort 10 significa que alguien cambio el umbral en config.h sin avisar.
  if (puerto === FPORT_ALERTA && datos.evento === "humo" && datos.confianza < 0.9) {
    avisos.push("confianza " + datos.confianza + " por debajo del umbral de diseno");
  }
  if (datos.bateria_pct < 20) avisos.push("bateria por debajo del 20 %");
  if (datos.flags && datos.flags.error_camara) avisos.push("el nodo reporta error de camara");
  if (datos.flags && datos.flags.error_modelo) avisos.push("el nodo reporta error de inferencia");

  datos.texto = comoTexto(datos);
  return { data: datos, warnings: avisos, errors: [] };
}

// Para poder probarlo fuera del servidor (node decoder_lorawan.js).
if (typeof module !== "undefined") {
  module.exports = { decodeUplink: decodeUplink };
}
