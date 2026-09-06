// config.h — Parametros de operacion del nodo de deteccion temprana de humo.
//
// Proyecto Amazonia+ / Piloto OSINFOR — CCNN Paoyhan.
//
// TODO valor que provenga de la documentacion de diseno o del entrenamiento del
// modelo vive aqui, con la referencia al documento que lo justifica. Si cambia el
// modelo, se reconstruye este archivo y el arreglo de lib/modelo_humo/.
#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// Identidad del nodo
// ---------------------------------------------------------------------------
#define NODO_ID          1u     // id logico 0-255 (protocolo-lorawan.md §3.1)
#define NODO_FW_VERSION  1u     // se transmite en el heartbeat

// Posicion de instalacion, grados x 1e7 (se envia solo en el heartbeat:
// los nodos son estaticos, protocolo-lorawan.md §2).
#define NODO_LAT_E7  (-61234500)
#define NODO_LON_E7  (-751234500)

// ---------------------------------------------------------------------------
// Geometria de inferencia
//
// El nodo NO clasifica el cuadro completo. La columna de humo mediana mide
// 41x32 px sobre 1280x720; reducir el cuadro entero a 96x96 la dejaria en 3x2 px.
// Se recorre el cuadro con una ventana de 224 px que se reduce a 96x96.
// Estos tres valores DEBEN coincidir con MODELO-TINYML/scripts/02_construir_dataset.py
// y 06_evaluar_por_cuadro.py, o el modelo vera en campo algo distinto de lo que
// vio en entrenamiento.
// ---------------------------------------------------------------------------
#define VENTANA_PX        224   // lado de la ventana sobre el cuadro original
#define ENTRADA_PX         96   // lado de la entrada del modelo
#define SOLAPE_NUM          1   // solape entre teselas contiguas = 1/4 = 25 %
#define SOLAPE_DEN          4
#define MAX_TESELAS        96   // cota de la rejilla (1280x720 -> 32; 1600x1200 -> 70)

// ---------------------------------------------------------------------------
// Regla de disparo
//
// Medida sobre el partner reservado sdis-77 (otra region, nunca visto en
// entrenamiento), que es el escenario mas parecido a Paoyhan.
// Ver modelos/cnn_media/evaluacion_por_cuadro_sdis-77.json:
//
//   K=1  umbral 0,97  recall 0,226  precision 0,894  FPR 0,098   <- por defecto
//   K=2  umbral 0,62  recall 0,175  precision 0,878  FPR 0,089
//   K=3  umbral 0,32  recall 0,213  precision 0,895  FPR 0,091
//
// Para cnn_media la confirmacion espacial NO ayuda (a diferencia de
// mobilenetv2_035, que es de donde salen las cifras de la tabla "Por captura"
// del README de MODELO-TINYML). La confirmacion temporal tampoco: los falsos
// positivos son bancos de niebla, que persisten entre capturas igual que el humo.
//
// El umbral se expresa en la escala entera de la salida INT8 para no usar coma
// flotante en el camino de decision: prob = (salida_int8 + 128) / 256.
// ---------------------------------------------------------------------------
#define UMBRAL_PROB_X1000   970u   // 0,970
#define K_TESELAS_MINIMO      1u   // teselas que deben superar el umbral

// ---------------------------------------------------------------------------
// Cadencia y ventana diurna (cadencia-captura.md §4)
//
// El humo es el primer signo visible y solo de dia. De noche el nodo duerme.
// Para V1/V2 el consumo lo domina el standby: capturar cada 15 min o cada 3 h es
// energeticamente equivalente, asi que la cadencia se elige por latencia.
// ---------------------------------------------------------------------------
#define HORA_INICIO_DIURNA    6u   // hora local de la primera captura
#define HORA_FIN_DIURNA      18u   // hora local de la ultima captura
#define INTERVALO_NORMAL_S      (15u * 60u)   // alto riesgo / bateria sana
#define INTERVALO_AHORRO_S      (60u * 60u)   // bateria baja o bajo riesgo
#define INTERVALO_CRITICO_S    (180u * 60u)   // bateria critica: solo telemetria

// Ventana de alto riesgo dentro del dia (media manana a tarde).
#define HORA_INICIO_ALTO_RIESGO  9u
#define HORA_FIN_ALTO_RIESGO    17u

// ---------------------------------------------------------------------------
// Politica de bateria (gestion-de-energia.md)
// ---------------------------------------------------------------------------
#define SOC_AHORRO_PCT   35u   // por debajo: alargar intervalo
#define SOC_CRITICO_PCT  15u   // por debajo: no capturar, solo heartbeat
#define SOC_ALERTA_BAJA  20u   // por debajo: emitir msg_type=2 una vez al dia

// ---------------------------------------------------------------------------
// Politica de transmision (protocolo-lorawan.md §2)
// ---------------------------------------------------------------------------
#define MAX_ALERTAS_POR_HORA   2u
#define FPORT_ALERTA          10u
#define FPORT_HEARTBEAT       11u
#define HORA_HEARTBEAT         7u   // una vez al dia, tras amanecer

// ---------------------------------------------------------------------------
// Arena de TFLite-Micro.
//
// La activacion mayor de cnn_media es de 147 456 B (1x48x48x64). Con las de
// trabajo y los descriptores, 320 KiB da margen holgado y cabe en la SRAM
// interna del ESP32-S3 si hiciera falta prescindir de PSRAM.
// ---------------------------------------------------------------------------
#define ARENA_TENSORES_BYTES  (320u * 1024u)
