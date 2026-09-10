#include "RoboCore_SMW_SX1276M0.h"
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>

HardwareSerial LoRaSerial(2);
#define RXD2 16
#define TXD2 17
SMW_SX1276M0 lorawan(LoRaSerial);

// --- Wi-Fi ---
const char* ssid = "SUA_REDE";
const char* password = "SUA_SENHA";

// --- Ubidots ---
const char* ubidotsToken = "SEU_TOKEN_UBIDOTS";
const char* deviceLabel  = "boi_01";
String serverURL = "https://industrial.api.ubidots.com/api/v1.6/devices/" + String(deviceLabel);

// TTL inicial configurado nos nos sensores (deve bater com o TTL_INICIAL
// usado no firmware da coleira). Usado apenas para calcular quantos saltos
// o pacote percorreu, como metadado de diagnostico (PP02/PP04).
#define TTL_INICIAL_CONHECIDO 5

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
  Serial.println("   NO GATEWAY v2.0");
  Serial.println("========================================");

  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());

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

        int hopCount = TTL_INICIAL_CONHECIDO - ttl.toInt();
        int gpsFix = status.startsWith("OK") ? 1 : 0;

        String json = "{";
        json += "\"position\":{\"value\":1,\"context\":{\"lat\":" + lat + ",\"lng\":" + lon + "}},";
        json += "\"rssi\":" + rssi + ",";
        json += "\"snr\":" + snr + ",";
        json += "\"hop_count\":" + String(hopCount) + ",";
        json += "\"seq\":" + seq + ",";
        json += "\"sat\":" + sat + ",";
        json += "\"hdop\":" + hdop + ",";
        json += "\"gps_fix\":" + String(gpsFix);
        json += "}";

        Serial.println("[RX] no=" + nodeId + " seq=" + seq + " ttl_recebido=" + ttl + " hops=" + String(hopCount));
        Serial.println("[HTTP] Enviando: " + json);

        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;
          http.begin(serverURL);
          http.addHeader("Content-Type", "application/json");
          http.addHeader("X-Auth-Token", ubidotsToken);
          int code = http.POST(json);
          Serial.println("[HTTP] Resposta: " + String(code));
          http.end();
        } else {
          Serial.println("[ERRO] WiFi desconectado, pacote descartado");
        }

        rssi = "0";
        snr  = "0";
      }
    }
  }
}
