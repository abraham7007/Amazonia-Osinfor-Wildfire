// Etapa 10 — Avisar al gateway.
//
// El prototipo del informe NO usa LoRaWAN: usa el Milesight UG67 como punto de
// acceso Wi-Fi y manda un JSON por socket TCP al 8888, donde escucha Node-RED.
// Mientras ADR-001 no decida otra cosa (ver HARDWARE/README.md §2), ese es el
// transporte real, y esta prueba comprueba que existe de extremo a extremo.
//
// Mide ademas el RSSI, que es el dato que dice si el enlace aguantara la
// distancia de despliegue: Wi-Fi 2,4 GHz en selva no llega a los 5 km del
// resultado 1 del proyecto, y conviene tener el numero antes de ir a campo.

#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>

#include "placa.h"
#include "informe.h"

// Credenciales del AP del gateway.
//
// Los valores reales NO estan aqui ni en ninguna parte del repositorio: se leen
// de comun/credenciales.ini, que no se versiona (plantilla en
// comun/credenciales.ini.ejemplo). Lo de abajo son marcadores para que la etapa
// compile aunque ese fichero no exista; el enlace fallara al no encontrar la
// red, que es exactamente lo que debe pasar.
#if !defined(GW_SSID)
#define GW_SSID "RED_SIN_CONFIGURAR"
#endif
#if !defined(GW_CLAVE)
#define GW_CLAVE "CLAVE_SIN_CONFIGURAR"
#endif
#if !defined(GW_IP)
#define GW_IP "192.168.10.1"
#endif
#if !defined(GW_PUERTO)
#define GW_PUERTO 8888
#endif

namespace {
const char *kSsid = GW_SSID;
const char *kClave = GW_CLAVE;
const char *kGatewayIp = GW_IP;
constexpr int kPuerto = GW_PUERTO;
constexpr uint32_t kEsperaMs = 20000;
}  // namespace

bool etapa_principal() {
  bool bien = true;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  dato("buscando redes...");
  const int n = WiFi.scanNetworks();
  bool visto = false;
  for (int i = 0; i < n; ++i) {
    if (WiFi.SSID(i) == kSsid) {
      visto = true;
      dato("'%s' visible, RSSI %d dBm, canal %d", kSsid, WiFi.RSSI(i), WiFi.channel(i));
      if (WiFi.RSSI(i) < -75) {
        dato("senal debil: por debajo de -75 dBm el enlace es inestable con humedad alta");
      }
    }
  }
  WiFi.scanDelete();
  if (!visto) {
    fallo("el AP '%s' no aparece: enciende el gateway UG67 o revisa su configuracion AP", kSsid);
    return false;
  }
  ok("el AP del gateway es visible");

  WiFi.begin(kSsid, kClave);
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < kEsperaMs) delay(200);
  if (WiFi.status() != WL_CONNECTED) {
    fallo("no se pudo asociar en %u s (clave incorrecta?)", (unsigned)(kEsperaMs / 1000));
    return false;
  }
  ok("asociado en %u ms, IP %s, RSSI %d dBm", (unsigned)(millis() - t0),
     WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());

  WiFiClient cliente;
  if (!cliente.connect(kGatewayIp, kPuerto)) {
    fallo("Node-RED no responde en %s:%d", kGatewayIp, kPuerto);
    fallo("revisa que el flow este desplegado y el nodo TCP-In a la escucha");
    bien = false;
  } else {
    // Trama de prueba: misma forma que la del firmware, con evento "test" para
    // que el dashboard no la confunda con una alerta real.
    char json[160];
    snprintf(json, sizeof(json),
             "{\"dispositivo\":\"%s\",\"alerta\":\"test\",\"confianza\":0.00,\"rssi\":%d}",
             NOMBRE_NODO, (int)WiFi.RSSI());
    cliente.println(json);
    cliente.stop();
    ok("trama de prueba entregada a Node-RED");
    dato("%s", json);
    dato("comprueba en el dashboard que aparece como evento de prueba, no como fuego");
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return bien;
}

void setup() { etapa("10", "Avisar al gateway", etapa_principal); }
void loop() { reposo(); }
