# -*- coding: utf-8 -*-
"""
Modelo de energía paramétrico — Proyecto Amazonía+ (OSINFOR)
Nodo Edge AI (V1/V2/V3) + Gateway LoRaWAN. Captura por intervalos, solo de día.

Todas las cifras de consumo son ESTIMACIONES DE DISEÑO basadas en datasheets y en
literatura; deben validarse por medición en banco (fase EVT).
Ver: 03-diseno-hardware/analisis-de-potencia.md y 00-gestion/referencias.md
"""
import json, os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUT = os.path.dirname(os.path.abspath(__file__))
REC = os.path.join(OUT, "..", "_recursos")
os.makedirs(REC, exist_ok=True)

# ---------- Supuestos del sistema fotovoltaico ----------
PSH = 3.5      # horas sol pico (Loreto, conservador)
DERATE = 0.70  # pérdidas globales
DOD = 0.80     # profundidad descarga LiFePO4
EFF = 0.85     # eficiencia conversión/cableado
VBAT = 12.8    # V banco
AUT = 2.5      # días autonomía
DAYLIGHT_H = 12  # ventana diurna (humo solo visible de día)

# ---------- Parámetros por versión de nodo ----------
# P_standby (W) ; E_cap (Wh por evento captura+inferencia) ; E_tx (Wh por transmisión) ; tx/día
VERSIONS = {
 "V1 · MCU TinyML\n(ESP32-S3+OV2640)":      dict(standby=0.008, ecap=0.0008, etx=0.0003, tx=6, mode="duty"),
 "V2 · IA on-sensor\n(Himax/IMX500+ESP32)": dict(standby=0.012, ecap=0.0012, etx=0.0003, tx=6, mode="duty"),
 "V3a · NPU gated\n(RPi5+Hailo, MCU-wake)": dict(standby=0.020, ecap=0.0700, etx=0.0004, tx=6, mode="duty"),
 "V3b · NPU always-on\n(RPi5+Hailo diurno)":dict(idle_day=3.0, idle_night=0.4, mode="alwayson"),
}

# ---------- Esquemas de captura (capturas/día, solo de día) ----------
def captures_per_day(scheme):
    if scheme == "1h":   return DAYLIGHT_H / 1.0          # 12
    if scheme == "3h":   return DAYLIGHT_H / 3.0          # 4
    if scheme == "adapt":                                  # 15min en 6h de riesgo + 1h en 6h
        return (6*4) + (6*1)                               # 30
    raise ValueError(scheme)

SCHEMES = ["1h", "3h", "adapt"]
SCHEME_LBL = {"1h":"Fijo 1 h (diurno)", "3h":"Fijo 3 h (diurno)", "adapt":"Adaptativo (riesgo)"}

def node_daily_wh(p, scheme):
    if p["mode"] == "alwayson":
        return p["idle_day"]*DAYLIGHT_H + p["idle_night"]*(24-DAYLIGHT_H)
    n = captures_per_day(scheme)
    return p["standby"]*24 + n*p["ecap"] + p["tx"]*p["etx"]

# ---------- Gateway (sin Jetson: solo UG67) ----------
GW = {"UG67 típico 3.6 W": 3.6, "UG67 máx 4.8 W": 4.8}

def sizing(whd):
    bat_wh = whd*AUT/(DOD*EFF); bat_ah = bat_wh/VBAT
    wp = whd/(PSH*DERATE) + (whd*AUT)/(PSH*DERATE)/2.5
    return bat_wh, bat_ah, wp

# ================= Cálculo =================
rows = []
for vname, p in VERSIONS.items():
    for s in SCHEMES:
        whd = node_daily_wh(p, s)
        bwh, bah, wp = sizing(whd)
        rows.append(dict(elem=vname.replace("\n"," "), esquema=s, whd=round(whd,3),
                         bat_ah=round(bah,3), panel_wp=round(wp,2)))

print("="*92)
print(f"{'Versión':40s} {'Esquema':8s} {'Wh/día':>8s} {'Bat(Ah)':>8s} {'Panel(Wp)':>10s}")
print("="*92)
for r in rows:
    print(f"{r['elem']:40s} {r['esquema']:8s} {r['whd']:8.3f} {r['bat_ah']:8.3f} {r['panel_wp']:10.2f}")

print("\nGATEWAY (sin Jetson):")
for g,w in GW.items():
    whd=w*24; bwh,bah,wp=sizing(whd)
    print(f"  {g:22s} {whd:6.0f} Wh/día | bat≥{bah:5.1f} Ah | panel≥{wp:5.0f} Wp")

# ================= Gráfico 1: Wh/día por versión y esquema (log) =================
vlabels = list(VERSIONS.keys())
x = np.arange(len(vlabels)); w = 0.25
fig, ax = plt.subplots(figsize=(11,5.5))
colors = {"1h":"#2E75B6","3h":"#5B9BD5","adapt":"#ED7D31"}
for i,s in enumerate(SCHEMES):
    vals = [node_daily_wh(VERSIONS[v], s) for v in vlabels]
    bars = ax.bar(x+(i-1)*w, vals, w, label=SCHEME_LBL[s], color=colors[s])
    for b,val in zip(bars,vals):
        ax.text(b.get_x()+b.get_width()/2, val*1.05, f"{val:.2g}", ha="center", va="bottom", fontsize=7.5)
# referencia gateway
ax.axhline(3.6*24, ls="--", color="#7F7F7F", lw=1.2)
ax.text(len(vlabels)-0.5, 3.6*24*1.06, "Gateway UG67 (86 Wh/día)", ha="right", color="#7F7F7F", fontsize=8.5)
ax.set_yscale("log")
ax.set_ylabel("Consumo diario (Wh/día) — escala log")
ax.set_title("Consumo energético por versión de nodo y esquema de captura\n(solo diurno · estimación de diseño, a validar en EVT)")
ax.set_xticks(x); ax.set_xticklabels([v.replace("\n","\n") for v in vlabels], fontsize=8)
ax.legend(title="Esquema de captura", fontsize=8)
ax.grid(axis="y", ls=":", alpha=0.5)
plt.tight_layout()
p1=os.path.join(REC,"energia-por-version.png"); plt.savefig(p1, dpi=140); plt.close()

# ================= Gráfico 2: contribución sistema (1 gateway + N nodos V1) =================
fig, ax = plt.subplots(figsize=(9,5))
Ns = [1,3,5,10,20]
gw = 3.6*24
node_v1 = node_daily_wh(VERSIONS[vlabels[0]], "1h")
node_v3b = node_daily_wh(VERSIONS[vlabels[3]], "1h")
tot_v1 = [gw + n*node_v1 for n in Ns]
tot_v3 = [gw + n*node_v3b for n in Ns]
ax.plot(Ns, tot_v1, "o-", color="#2E75B6", label="Gateway + N nodos V1 (MCU TinyML)")
ax.plot(Ns, tot_v3, "s-", color="#C00000", label="Gateway + N nodos V3b (NPU always-on)")
ax.axhline(gw, ls="--", color="#7F7F7F", lw=1); ax.text(20, gw*1.04, "solo gateway", ha="right", color="#7F7F7F", fontsize=8)
ax.set_xlabel("Número de nodos en la red"); ax.set_ylabel("Consumo total del sistema (Wh/día)")
ax.set_title("Energía total del sistema según número de nodos y versión\n(el gateway domina con nodos de bajo consumo)")
ax.legend(fontsize=8.5); ax.grid(ls=":", alpha=0.5)
plt.tight_layout()
p2=os.path.join(REC,"energia-sistema-vs-nodos.png"); plt.savefig(p2, dpi=140); plt.close()

# Guardar tabla en JSON para reuso/documentación
with open(os.path.join(OUT,"resultados_energia.json"),"w",encoding="utf-8") as f:
    json.dump(rows, f, ensure_ascii=False, indent=2)
print("\nGráficos:", os.path.basename(p1), ",", os.path.basename(p2))
print("Tabla:", "resultados_energia.json")
