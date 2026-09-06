// Pruebas del nucleo portable, ejecutables en el PC: `pio test -e host`.
//
// Cubren las tres cosas que, si se rompen, el nodo sigue funcionando pero
// detectando mal — que es el fallo caro, porque no da sintomas en campo:
//   1. la rejilla de teselas deja de coincidir con la de entrenamiento,
//   2. el remuestreo deja de reproducir el de Pillow,
//   3. la trama LoRa deja de coincidir con el decoder del servidor.

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "energia.h"
#include "payload.h"
#include "remuestreo.h"
#include "teselado.h"
#include "vectores_remuestreo.h"

namespace {

int g_fallos = 0;
int g_pruebas = 0;

void comprobar(bool ok, const char *nombre) {
  ++g_pruebas;
  if (!ok) {
    ++g_fallos;
    printf("  FALLO  %s\n", nombre);
  } else {
    printf("  ok     %s\n", nombre);
  }
}

// Mismo LCG que generar_vectores.py.
void generar_fuente(uint8_t *destino, int ancho, int alto, uint32_t semilla) {
  uint32_t x = semilla & 0x7FFFFFFFu;
  const size_t n = (size_t)ancho * alto * 3;
  for (size_t i = 0; i < n; ++i) {
    x = (1103515245u * x + 12345u) & 0x7FFFFFFFu;
    destino[i] = (uint8_t)((x >> 16) & 0xFFu);
  }
}

// --- 1. Rejilla ------------------------------------------------------------
void prueba_rejilla() {
  Tesela t[MAX_TESELAS];

  // 1280x720 con ventana 224 y solape 25 % debe dar exactamente las 32 teselas
  // de 06_evaluar_por_cuadro.py, en el mismo orden.
  const int n = construir_rejilla(1280, 720, 224, 1, 4, t, MAX_TESELAS);
  comprobar(n == 32, "1280x720 -> 32 teselas");

  const int16_t xs[] = {0, 168, 336, 504, 672, 840, 1008, 1056};
  const int16_t ys[] = {0, 168, 336, 496};
  bool orden_ok = (n == 32);
  for (int j = 0; j < 4 && orden_ok; ++j) {
    for (int i = 0; i < 8; ++i) {
      const Tesela &e = t[j * 8 + i];
      if (e.x != xs[i] || e.y != ys[j]) {
        orden_ok = false;
        break;
      }
    }
  }
  comprobar(orden_ok, "esquinas y orden [(x,y) for y in ys for x in xs]");

  // La ultima columna y la ultima fila deben pegarse al borde, no quedarse cortas:
  // si no, la franja derecha del cuadro nunca se inspecciona.
  comprobar(n == 32 && t[7].x + 224 == 1280, "la ultima columna llega al borde derecho");
  comprobar(n == 32 && t[31].y + 224 == 720, "la ultima fila llega al borde inferior");

  comprobar(construir_rejilla(200, 200, 224, 1, 4, t, MAX_TESELAS) == 0,
            "cuadro menor que la ventana -> 0 teselas");
  comprobar(construir_rejilla(1600, 1200, 224, 1, 4, t, MAX_TESELAS) == 70,
            "1600x1200 -> 70 teselas (mas del doble de energia por captura)");
}

// --- 2. Remuestreo ---------------------------------------------------------
void prueba_remuestreo() {
  static uint8_t fuente[VECTOR_VENTANA * VECTOR_VENTANA * 3];
  static uint8_t temporal[VECTOR_VENTANA * VECTOR_SALIDA * 3];
  static int8_t salida[VECTOR_SALIDA * VECTOR_SALIDA * 3];

  generar_fuente(fuente, VECTOR_VENTANA, VECTOR_VENTANA, VECTOR_SEMILLA);

  Cuadro c;
  c.datos = fuente;
  c.ancho = VECTOR_VENTANA;
  c.alto = VECTOR_VENTANA;
  c.paso_fila = (size_t)VECTOR_VENTANA * 3;

  const bool ok = remuestrear_ventana(c, 0, 0, VECTOR_VENTANA, VECTOR_SALIDA, temporal,
                                      sizeof(temporal), salida);
  comprobar(ok, "remuestrear_ventana ejecuta");

  int distintos = 0;
  int peor = 0;
  for (size_t i = 0; i < sizeof(salida); ++i) {
    const int obtenido = (int)salida[i] + 128;
    const int esperado = (int)kEsperadoRemuestreo[i];
    const int d = obtenido > esperado ? obtenido - esperado : esperado - obtenido;
    if (d != 0) ++distintos;
    if (d > peor) peor = d;
  }
  printf("         %d/%zu bytes distintos de Pillow, peor desvio %d\n", distintos,
         sizeof(salida), peor);
  comprobar(distintos == 0, "salida identica byte a byte a Image.resize(BILINEAR)");
}

// --- 3. Payload ------------------------------------------------------------
void prueba_payload() {
  Alerta a;
  a.node_id = 3;
  a.tipo = MSG_HUMO;
  a.confianza_pct = 92;
  a.bateria_pct = 87;
  a.temp_c = 31;
  a.flags = FLAG_ALARMA_ACTIVA;

  uint8_t buf[16];
  memset(buf, 0xAA, sizeof(buf));
  comprobar(serializar_alerta(a, buf, sizeof(buf)) == 6, "la alerta ocupa 6 bytes");
  const uint8_t esperado[] = {0x03, 0x01, 0x5C, 0x57, 0x47, 0x01};
  comprobar(memcmp(buf, esperado, 6) == 0, "bytes de la alerta segun protocolo §3.1");
  comprobar(serializar_alerta(a, buf, 5) == 0, "rechaza un buffer corto");

  Heartbeat h;
  h.node_id = 3;
  h.lat_e7 = -61234500;
  h.lon_e7 = -751234500;
  h.bateria_pct = 87;
  h.temp_c = 31;
  h.humedad_pct = 86;
  h.fw_ver = 1;
  comprobar(serializar_heartbeat(h, buf, sizeof(buf)) == 14, "el heartbeat ocupa 14 bytes");
  comprobar(buf[1] == 0 && buf[13] == 1, "msg_type=0 y fw_ver en su sitio");
  // Latitud negativa en complemento a dos, big-endian.
  const int32_t lat = (int32_t)((uint32_t)buf[2] << 24 | (uint32_t)buf[3] << 16 |
                                (uint32_t)buf[4] << 8 | buf[5]);
  comprobar(lat == -61234500, "la latitud negativa sobrevive al viaje");

  comprobar(temp_a_byte(-50) == 0 && temp_a_byte(100) == 127, "la temperatura satura");
  comprobar(confianza_a_byte(970) == 97 && confianza_a_byte(1000) == 100,
            "la confianza pasa a porcentaje");
}

// --- 4. Politica de energia ------------------------------------------------
void prueba_energia() {
  HoraLocal h = {};
  h.valida = true;
  h.dia = 5;

  h.hora = 3;
  comprobar(decidir_accion(h, 90) == Accion::DORMIR_HASTA_ALBA, "de madrugada no captura");
  comprobar(segundos_hasta_alba(h) == 3u * 3600u, "duerme hasta las 06:00");

  h.hora = 12;
  comprobar(decidir_accion(h, 90) == Accion::CAPTURAR, "al mediodia captura");
  comprobar(intervalo_siguiente(h, 90) == INTERVALO_NORMAL_S, "alto riesgo -> 15 min");
  comprobar(intervalo_siguiente(h, 30) == INTERVALO_AHORRO_S, "bateria baja -> 1 h");
  comprobar(decidir_accion(h, 10) == Accion::SOLO_TELEMETRIA, "bateria critica -> sin camara");

  h.hora = 7;
  comprobar(intervalo_siguiente(h, 90) == INTERVALO_AHORRO_S,
            "fuera de la ventana de alto riesgo -> 1 h");

  h.hora = 19;
  comprobar(decidir_accion(h, 90) == Accion::DORMIR_HASTA_ALBA, "de noche duerme");
  comprobar(segundos_hasta_alba(h) == 11u * 3600u, "duerme hasta el alba del dia siguiente");
}

}  // namespace

int main() {
  printf("== rejilla de teselas ==\n");
  prueba_rejilla();
  printf("== remuestreo (paridad con Pillow) ==\n");
  prueba_remuestreo();
  printf("== payload LoRaWAN ==\n");
  prueba_payload();
  printf("== politica de energia ==\n");
  prueba_energia();
  printf("\n%d/%d pruebas superadas\n", g_pruebas - g_fallos, g_pruebas);
  return g_fallos == 0 ? 0 : 1;
}
