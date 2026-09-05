// main.cpp — Ciclo de vida del nodo de deteccion temprana de humo.
//
// Proyecto Amazonia+ / Piloto OSINFOR — CCNN Paoyhan.
//
// El nodo no tiene bucle: cada despertar del deep-sleep ejecuta una pasada
// completa y vuelve a dormir. Todo lo que debe recordar entre pasadas vive en la
// memoria RTC (estado_persistente.h). Esta forma —y no un bucle con delay()— es
// lo que sostiene el presupuesto de 0,2 Wh/dia de analisis-de-potencia.md: fuera
// de la pasada, el SoC esta apagado.
//
//   despertar -> hora y bateria -> ?ventana diurna?
//        |                              |
//        |                              +-- no  -> dormir hasta el alba
//        |                              +-- si  -> capturar
//        v
//   camara on -> cuadro -> 32 teselas -> 32 inferencias -> regla de disparo
//        |
//        +-- alerta -> uplink FPort 10 (con limite por hora)
//        +-- toca heartbeat -> uplink FPort 11
//        v
//   camara off -> radio off -> deep-sleep con el intervalo que toque

#include <stdarg.h>
#include <stdio.h>

#include "config.h"
#include "deteccion.h"
#include "energia.h"
#include "estado_persistente.h"
#include "hal_camara.h"
#include "hal_energia.h"
#include "hal_lora.h"
#include "hal_tiempo.h"
#include "payload.h"

namespace {

void registrar(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void registrar(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

// Une o restaura la sesion LoRaWAN. El join OTAA cuesta mucho mas que un uplink,
// asi que solo se rehace si la sesion guardada en RTC no vale.
bool preparar_radio(EstadoPersistente *st) {
  if (!lora_iniciar()) return false;
  if (st->sesion.valida && lora_restaurar(st->sesion)) return true;
  if (!lora_unir()) return false;
  return lora_sesion_actual(&st->sesion);
}

void guardar_sesion(EstadoPersistente *st) { lora_sesion_actual(&st->sesion); }

// Limite de alertas por hora: evita que una escena ambigua —un banco de niebla
// que persiste toda la manana— vacie la bateria y sature al destinatario. La
// credibilidad del aviso ante la comunidad es un requisito del piloto, no un
// detalle de implementacion.
bool puede_alertar(EstadoPersistente *st, const HoraLocal &h) {
  if (st->hora_ventana_alertas != h.hora) {
    st->hora_ventana_alertas = h.hora;
    st->alertas_esta_hora = 0;
  }
  return st->alertas_esta_hora < MAX_ALERTAS_POR_HORA;
}

bool enviar_alerta(EstadoPersistente *st, TipoMensaje tipo, uint16_t prob_x1000,
                   const EstadoEnergia &e, uint8_t flags) {
  Alerta a;
  a.node_id = NODO_ID;
  a.tipo = tipo;
  a.confianza_pct = confianza_a_byte(prob_x1000);
  a.bateria_pct = e.soc_pct;
  a.temp_c = e.temp_c;
  a.flags = flags;

  uint8_t trama[PAYLOAD_ALERTA_BYTES];
  const size_t n = serializar_alerta(a, trama, sizeof(trama));
  if (n == 0) return false;
  if (!lora_enviar(FPORT_ALERTA, trama, n)) return false;

  ++st->alertas;
  ++st->alertas_esta_hora;
  guardar_sesion(st);
  return true;
}

bool enviar_heartbeat(EstadoPersistente *st, const EstadoEnergia &e) {
  Heartbeat h;
  h.node_id = NODO_ID;
  h.lat_e7 = NODO_LAT_E7;
  h.lon_e7 = NODO_LON_E7;
  h.bateria_pct = e.soc_pct;
  h.temp_c = e.temp_c;
  h.humedad_pct = e.humedad_pct;
  h.fw_ver = NODO_FW_VERSION;

  uint8_t trama[PAYLOAD_HEARTBEAT_BYTES];
  const size_t n = serializar_heartbeat(h, trama, sizeof(trama));
  if (n == 0) return false;
  if (!lora_enviar(FPORT_HEARTBEAT, trama, n)) return false;
  guardar_sesion(st);
  return true;
}

bool toca_heartbeat(const EstadoPersistente *st, const HoraLocal &h) {
  if (!h.valida) return st->capturas == 0;
  return h.hora >= HORA_HEARTBEAT && st->dia_ultimo_heartbeat != h.dia;
}

void dormir(const HoraLocal &h, uint8_t soc, Accion accion) {
  const uint32_t s = (accion == Accion::DORMIR_HASTA_ALBA) ? segundos_hasta_alba(h)
                                                           : intervalo_siguiente(h, soc);
  registrar("[nodo] durmiendo %u s\n", (unsigned)s);
  camara_apagar();
  lora_dormir();
  dormir_profundo(s);
}

// Una pasada completa. Se llama una sola vez por despertar.
void pasada() {
  EstadoPersistente *st = estado();
  const HoraLocal h = hora_local();
  const EstadoEnergia e = leer_energia();
  const Accion accion = decidir_accion(h, e.soc_pct);

  registrar("[nodo] %04u-%02u-%02u %02u:%02u  SoC=%u%%  capturas=%u\n", h.anio, h.mes, h.dia,
            h.hora, h.minuto, (unsigned)e.soc_pct, (unsigned)st->capturas);

  if (accion == Accion::DORMIR_HASTA_ALBA) {
    registrar("[nodo] fuera de la ventana diurna\n");
    dormir(h, e.soc_pct, accion);
    return;
  }

  const bool radio_ok = preparar_radio(st);
  if (!radio_ok) registrar("[nodo] AVISO: sin sesion LoRaWAN, la pasada no podra transmitir\n");

  uint8_t flags = 0;
  if (e.soc_pct < SOC_AHORRO_PCT) flags |= FLAG_MODO_AHORRO;

  // Bateria critica: no se enciende la camara. Solo telemetria, para que el
  // operador sepa que el nodo sigue vivo y por que dejo de vigilar.
  if (accion == Accion::SOLO_TELEMETRIA) {
    registrar("[nodo] bateria critica (%u%%): se omite la captura\n", (unsigned)e.soc_pct);
    if (radio_ok && st->dia_ultima_alerta_bateria != h.dia) {
      if (enviar_alerta(st, MSG_BATERIA_BAJA, 0, e, flags)) st->dia_ultima_alerta_bateria = h.dia;
    }
    dormir(h, e.soc_pct, accion);
    return;
  }

  Cuadro cuadro;
  ResultadoDeteccion r;
  if (!camara_iniciar() || !camara_capturar(&cuadro)) {
    registrar("[nodo] ERROR: fallo la captura\n");
    flags |= FLAG_ERROR_CAMARA;
    r.error = true;
  } else {
    r = deteccion_procesar(cuadro);
    camara_liberar(&cuadro);
    registrar("[nodo] teselas=%u sobre=%u pmax=%.3f (%d,%d) remuestreo=%ums inferencia=%ums\n",
              (unsigned)r.teselas_total, (unsigned)r.teselas_sobre, r.prob_maxima / 1000.0,
              (int)r.x_maxima, (int)r.y_maxima, (unsigned)r.ms_remuestreo,
              (unsigned)r.ms_inferencia);
    if (r.error) flags |= FLAG_ERROR_MODELO;
  }
  camara_apagar();

  ++st->capturas;
  st->capturas_con_deteccion = r.teselas_sobre > 0 ? (uint8_t)(st->capturas_con_deteccion + 1) : 0;

  if (r.alerta) {
    flags |= FLAG_ALARMA_ACTIVA;
    if (!radio_ok) {
      registrar("[nodo] alerta detectada pero sin radio\n");
    } else if (!puede_alertar(st, h)) {
      registrar("[nodo] alerta detectada, limite horario alcanzado (%u/h)\n",
                (unsigned)MAX_ALERTAS_POR_HORA);
    } else if (enviar_alerta(st, MSG_HUMO, r.prob_maxima, e, flags)) {
      registrar("[nodo] ALERTA DE HUMO transmitida, confianza %u%%\n",
                (unsigned)confianza_a_byte(r.prob_maxima));
    }
  }

  if (radio_ok && toca_heartbeat(st, h) && enviar_heartbeat(st, e)) {
    st->dia_ultimo_heartbeat = h.dia;
    registrar("[nodo] heartbeat diario transmitido\n");
  }

  dormir(h, e.soc_pct, accion);
}

bool arrancar() {
  registrar("\n[nodo] Amazonia+ / OSINFOR — nodo de humo, fw %u (%s)\n", (unsigned)NODO_FW_VERSION,
            arranque_en_frio() ? "arranque en frio" : "desde deep-sleep");
  if (!deteccion_iniciar()) {
    registrar("[nodo] ERROR: no se pudo iniciar el motor de inferencia\n");
    return false;
  }
  return true;
}

}  // namespace

#if defined(ARDUINO)
void setup() {
  Serial.begin(115200);
  if (!arrancar()) {
    // Sin motor no hay nodo: dormir el intervalo de ahorro y reintentar tras el
    // reinicio, en vez de quedar consumiendo en un bucle de error.
    dormir_profundo(INTERVALO_AHORRO_S);
  }
  pasada();
}
void loop() {}
#else
int main() {
  if (!arrancar()) return 1;
  pasada();
  return 0;
}
#endif
