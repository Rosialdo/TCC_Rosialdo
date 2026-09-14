#include "RoboCore_SMW_SX1276M0.h"
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>

HardwareSerial LoRaSerial(2);
#define RXD2 16
#define TXD2 17
SMW_SX1276M0 lorawan(LoRaSerial);

// --- Wi-Fi ---
const char* ssid = "VICENTE";
const char* password = "R$qv2020";

// --- Ubidots ---
const char* ubidotsToken = "BBUS-3GJLMpJ8iULVSGYQ8U5OViwXBEyaJd";
const char* deviceLabel  = "boi_01";
String serverURL = "https://industrial.api.ubidots.com/api/v1.6/devices/" + String(deviceLabel);

// TTL inicial configurado nos nos sensores (deve bater com o TTL_INICIAL
// usado no firmware da coleira). Usado apenas para calcular quantos saltos
// o pacote percorreu, como metadado de diagnostico (PP02/PP04).
#define TTL_INICIAL_CONHECIDO 5

// --- Deduplicacao ---
// O LoRa e broadcast: o Gateway pode ouvir tanto a transmissao original de
// um no sensor quanto a retransmissao de um repetidor. Esse buffer garante
// que cada (nodeId, seq) seja enviado a nuvem apenas uma vez.
#define BUFFER_SIZE_GW 20
struct PacoteVistoGW {
  int nodeId;
  int seq;
};
PacoteVistoGW vistosGW[BUFFER_SIZE_GW];
int indiceBufferGW = 0;

bool jaEnviado(int nodeId, int seq) {
  for (int i = 0; i < BUFFER_SIZE_GW; i++) {
    if (vistosGW[i].nodeId == nodeId && vistosGW[i].seq == seq) return true;
  }
  return false;
}

void marcarEnviado(int nodeId, int seq) {
  vistosGW[indiceBufferGW].nodeId = nodeId;
  vistosGW[indiceBufferGW].seq = seq;
  indiceBufferGW = (indiceBufferGW + 1) % BUFFER_SIZE_GW;
}

// --- Geofencing (calculado no proprio Gateway) ---
// Necessario porque o gatilho nativo de "Geofence" da Ubidots exige plano
// pago. O calculo do poligono e feito aqui.
//
// POLIGONO DE TESTE: quadrado de ~50x50m centrado na casa de referencia
// (2.805925, -60.748853), usado apenas para validar o mecanismo antes de
// definir o poligono real da propriedade.
#define NUM_VERTICES 4
float poligonoLat[NUM_VERTICES] = {2.806150, 2.806150, 2.805700, 2.805700};
float poligonoLon[NUM_VERTICES] = {-60.749078, -60.748628, -60.748628, -60.749078};

bool dentroDoPoligono(float lat, float lon) {
  bool dentro = false;
  int j = NUM_VERTICES - 1;
  for (int i = 0; i < NUM_VERTICES; i++) {
    if (((poligonoLon[i] > lon) != (poligonoLon[j] > lon)) &&
        (lat < (poligonoLat[j] - poligonoLat[i]) * (lon - poligonoLon[i]) /
               (poligonoLon[j] - poligonoLon[i]) + poligonoLat[i])) {
      dentro = !dentro;
    }
    j = i;
  }
  return dentro;
}

// Estado anterior do geofence, para so enviar um dot quando o valor MUDA
// (entrou ou saiu da cerca) em vez de a cada pacote. -1 = ainda desconhecido,
// forca o envio do primeiro valor real assim que chegar.
int ultimoGeofenceAlert = -1;

void enviarGeofenceAlert(int status) {
  if (WiFi.status() != WL_CONNECTED) return;
  String json = "{\"geofence_alert\":" + String(status) + "}";
  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Auth-Token", ubidotsToken);
  int code = http.POST(json);
  String resposta = http.getString();
  Serial.println("[HTTP] Geofence (" + String(status) + ") enviado. Resposta: " + String(code));
  if (code != 200 && code != 201) {
    Serial.println("[HTTP] Corpo da resposta (geofence): " + resposta);
  }
  http.end();
}

// --- Heartbeat / Perda de Contato (calculado no proprio Gateway) ---
// Necessario porque o gatilho nativo de "Inactivity" da Ubidots tambem
// exige plano pago. O Gateway guarda o instante do ultimo pacote recebido
// (de qualquer no ou repetidor) e verifica periodicamente se esse intervalo
// ultrapassou o limiar (3x o intervalo de transmissao, conforme o TCC).
// OBS: assume um unico no sensor ativo (NODE_ID=1). Para multiplos nos
// simultaneos, isso precisaria virar um array indexado por nodeId.
#define INTERVALO_TRANSMISSAO_NO_MS 180000UL                         // deve bater com o INTERVALO_TRANSMISSAO_MS da coleira
#define LIMIAR_PERDA_CONTATO_MS 180000UL //(3UL * INTERVALO_TRANSMISSAO_NO_MS)   // 3 intervalos
#define INTERVALO_CHECK_HEARTBEAT_MS 30000UL                         // verifica a cada 30s

unsigned long ultimoContatoMillis = 0;
unsigned long ultimoCheckHeartbeatMillis = 0;
bool primeiroPacoteRecebido = false;
bool alertaPerdaContatoAtivo = false;
String ultimaLatConhecida = "0";
String ultimaLonConhecida = "0";

void enviarHeartbeat(int status) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERRO] WiFi desconectado, nao foi possivel enviar heartbeat");
    return;
  }

  String json = "{";
  json += "\"heartbeat_alert\":" + String(status) + ",";
  json += "\"position\":{\"value\":1,\"context\":{\"lat\":" + ultimaLatConhecida + ",\"lng\":" + ultimaLonConhecida + "}}";
  json += "}";

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Auth-Token", ubidotsToken);
  int code = http.POST(json);
  String resposta = http.getString();
  Serial.println("[HTTP] Heartbeat (" + String(status) + ") enviado. Resposta: " + String(code));
  if (code != 200 && code != 201) {
    Serial.println("[HTTP] Corpo da resposta (heartbeat): " + resposta);
  }
  http.end();
}

String hexParaTexto(String hex) {
  String resultado = "";
  for (int i = 0; i < hex.length(); i += 2) {
    String byteStr = hex.substring(i, i + 2);
    char c = (char) strtol(byteStr.c_str(), NULL, 16);
    resultado += c;
  }
  return resultado;
}

String extraiValor(String texto, String chave) {
  int idx = texto.indexOf(chave);
  if (idx < 0) return "0";
  idx += chave.length();
  int fim = texto.indexOf(' ', idx);
  if (fim < 0) fim = texto.length();
  return texto.substring(idx, fim);
}

void setup() {
  Serial.begin(115200);
  Serial.println("========================================");
  Serial.println("   NO GATEWAY v2.5");
  Serial.println("========================================");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);

  Serial.print("Conectando WiFi");
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[ERRO] Falha ao conectar apos " + String(tentativas) + " tentativas.");
    Serial.println("[ERRO] Codigo de status: " + String(WiFi.status()));
  }

  LoRaSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
  lorawan.setPinReset(5);
  lorawan.reset();
  delay(3000);
  lorawan.set_JoinMode(SMW_SX1276M0_JOIN_MODE_P2P);
  delay(2000);
  Serial.println("[INFO] Gateway pronto — aguardando pacotes da cadeia de retransmissao");
}

void loop() {
  String rssi = "0", snr = "0";

  while (LoRaSerial.available()) {
    String raw = LoRaSerial.readStringUntil('\n');
    raw.trim();

    if (raw.indexOf("rssi") > 0 && raw.indexOf("snr") > 0) {
      rssi = extraiValor(raw, "rssi ");
      snr  = extraiValor(raw, "snr ");
    }

    if (raw.indexOf("RECVB") > 0) {
      int idx = raw.lastIndexOf(':');
      if (idx > 0) {
        String hex = raw.substring(idx + 1);
        hex.trim();
        String dados = hexParaTexto(hex);

        // Formato: nodeId,seq,ttl,lat,lon,sat,hdop,status
        int c1 = dados.indexOf(',');
        int c2 = dados.indexOf(',', c1 + 1);
        int c3 = dados.indexOf(',', c2 + 1);
        int c4 = dados.indexOf(',', c3 + 1);
        int c5 = dados.indexOf(',', c4 + 1);
        int c6 = dados.indexOf(',', c5 + 1);
        int c7 = dados.indexOf(',', c6 + 1);

        if (c1 < 0 || c2 < 0 || c3 < 0 || c4 < 0 || c5 < 0 || c6 < 0 || c7 < 0) {
          Serial.println("[ERRO] Payload malformado: " + dados);
          rssi = "0"; snr = "0";
          continue;
        }

        String nodeId = dados.substring(0, c1);
        String seq    = dados.substring(c1 + 1, c2);
        String ttl    = dados.substring(c2 + 1, c3);
        String lat    = dados.substring(c3 + 1, c4);
        String lon    = dados.substring(c4 + 1, c5);
        String sat    = dados.substring(c5 + 1, c6);
        String hdop   = dados.substring(c6 + 1, c7);
        String status = dados.substring(c7 + 1);

        int nodeIdInt = nodeId.toInt();
        int seqInt    = seq.toInt();
        int hopCount  = TTL_INICIAL_CONHECIDO - ttl.toInt();
        int gpsFix    = status.startsWith("OK") ? 1 : 0;

        Serial.println("[RX] no=" + nodeId + " seq=" + seq + " ttl_recebido=" + ttl + " hops=" + String(hopCount));

        // Atualiza o heartbeat a cada pacote valido recebido, mesmo que seja
        // descartado depois por deduplicacao — o que importa aqui e que o
        // radio esta vivo e o contato com o no sensor continua.
        ultimoContatoMillis = millis();
        primeiroPacoteRecebido = true;
        ultimaLatConhecida = lat;
        ultimaLonConhecida = lon;

        if (alertaPerdaContatoAtivo) {
          Serial.println("[INFO] Contato reestabelecido apos alerta de perda de contato.");
          alertaPerdaContatoAtivo = false;
          enviarHeartbeat(0);
        }

        // Descarta se esse (nodeId, seq) ja foi enviado a nuvem por outro caminho
        if (jaEnviado(nodeIdInt, seqInt)) {
          Serial.println("[SKIP] no=" + nodeId + " seq=" + seq + " ja enviado a nuvem (chegou por outro caminho, hops=" + String(hopCount) + ")");
          rssi = "0"; snr = "0";
          continue;
        }
        marcarEnviado(nodeIdInt, seqInt);

        // So calcula geofencing quando ha fix real; sem fix, lat/lon vem
        // como 0,0 (placeholder da coleira), o que seria erroneamente
        // interpretado como "fora da cerca" (0,0 fica no Oceano Atlantico).
        int geofenceAlert = 0;
        if (gpsFix == 1) {
          geofenceAlert = dentroDoPoligono(lat.toFloat(), lon.toFloat()) ? 0 : 1;
        } else {
          Serial.println("[INFO] Sem fix GPS — geofence_alert mantido em 0 (posicao nao confiavel)");
        }

        // Envia o geofence_alert como dot separado apenas quando MUDA de
        // valor (entrou ou saiu da cerca), para nao consumir a cota diaria
        // de dots da Ubidots STEM a cada pacote.
        if (geofenceAlert != ultimoGeofenceAlert) {
          enviarGeofenceAlert(geofenceAlert);
          ultimoGeofenceAlert = geofenceAlert;
        }

        // --- Payload principal reduzido a 3 dots (position, rssi, snr) ---
        // seq, sat, hdop, gps_fix e hop_count vao dentro do "context" de
        // position: continuam registrados por pacote (visiveis numa tabela
        // de dados brutos no dashboard), mas nao contam como dots extras,
        // porque contexto e metadado, nao variavel. Isso mantem o consumo
        // diario de dots bem abaixo do limite de 4.000/dia do plano STEM,
        // mesmo em operacao continua no intervalo de campo (3-4 min).
        String json = "{";
        json += "\"position\":{\"value\":1,\"context\":{";
        json += "\"lat\":" + lat + ",\"lng\":" + lon + ",";
        json += "\"seq\":" + seq + ",";
        json += "\"sat\":" + sat + ",";
        json += "\"hdop\":" + hdop + ",";
        json += "\"gps_fix\":" + String(gpsFix) + ",";
        json += "\"hop_count\":" + String(hopCount);
        json += "}},";
        json += "\"rssi\":" + rssi + ",";
        json += "\"snr\":" + snr;
        json += "}";

        Serial.println("[HTTP] Enviando: " + json);

        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;
          http.begin(serverURL);
          http.addHeader("Content-Type", "application/json");
          http.addHeader("X-Auth-Token", ubidotsToken);
          int code = http.POST(json);
          String resposta = http.getString();
          Serial.println("[HTTP] Resposta: " + String(code));
          if (code != 200 && code != 201) {
            Serial.println("[HTTP] Corpo da resposta: " + resposta);
          }
          http.end();
        } else {
          Serial.println("[ERRO] WiFi desconectado, pacote descartado");
        }

        rssi = "0";
        snr  = "0";
      }
    }
  }

  // --- Verificacao periodica de perda de contato ---
  // Roda a cada INTERVALO_CHECK_HEARTBEAT_MS, independente de ter chegado
  // pacote novo ou nao — e essa checagem periodica que detecta o silencio.
  if (millis() - ultimoCheckHeartbeatMillis > INTERVALO_CHECK_HEARTBEAT_MS) {
    ultimoCheckHeartbeatMillis = millis();

    if (primeiroPacoteRecebido && !alertaPerdaContatoAtivo) {
      if (millis() - ultimoContatoMillis > LIMIAR_PERDA_CONTATO_MS) {
        Serial.println("[ALERTA] Perda de contato detectada! Ultimo contato ha " +
                        String((millis() - ultimoContatoMillis) / 1000) + "s");
        alertaPerdaContatoAtivo = true;
        enviarHeartbeat(1);
      }
    }
  }
}