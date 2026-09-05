// Comprueba que el decoder del servidor entiende exactamente las tramas que
// emite el firmware. Las de abajo son las que imprime el backend de traza.
//   node FIRMWARE/herramientas/probar_decoder.js
var { decodeUplink } = require("./decoder_lorawan.js");

function hex(s) {
  var b = [];
  for (var i = 0; i < s.length; i += 2) b.push(parseInt(s.substr(i, 2), 16));
  return b;
}

var casos = [
  { nombre: "heartbeat del firmware", fPort: 11, hex: "0100FC59A2BCD339123C55475601" },
  { nombre: "alerta de humo", fPort: 10, hex: "0301615747 01".replace(/ /g, "") },
  { nombre: "bateria baja", fPort: 10, hex: "010200124311" },
];

var fallos = 0;
for (var c of casos) {
  var r = decodeUplink({ bytes: hex(c.hex), fPort: c.fPort });
  if (r.errors.length) {
    console.log("  FALLO  " + c.nombre + ": " + r.errors.join(", "));
    fallos++;
    continue;
  }
  console.log("  ok     " + c.nombre);
  console.log("         " + r.data.texto);
  if (r.warnings.length) console.log("         avisos: " + r.warnings.join("; "));
}

// El heartbeat debe reconstruir la posicion de Paoyhan, negativa en ambos ejes.
var hb = decodeUplink({ bytes: hex("0100FC59A2BCD339123C55475601"), fPort: 11 }).data;
if (!(hb.lat < 0 && hb.lon < 0)) {
  console.log("  FALLO  la latitud/longitud negativa no sobrevive al decoder");
  fallos++;
} else {
  console.log("  ok     lat/lon negativas: " + hb.lat + ", " + hb.lon);
}

console.log(fallos === 0 ? "\ndecoder OK" : "\n" + fallos + " fallos");
process.exit(fallos === 0 ? 0 : 1);
