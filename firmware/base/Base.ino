#include "RoboCore_SMW_SX1276M0.h"
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>

HardwareSerial LoRaSerial(2);
#define RXD2 16
#define TXD2 17
SMW_SX1276M0 lorawan(LoRaSerial);

const char* ssid = "SUA_REDE";
const char* password = "SUA_SENHA";
const char* serverURL = "http://<IP_RASP>:1880/dados";

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
  Serial.println("Iniciando estacao base...");

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
  Serial.println("Estacao base pronta!");
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

        // Parse: seq,lat,lon,sat,hdop,status
        int c1 = dados.indexOf(',');
        int c2 = dados.indexOf(',', c1 + 1);
        int c3 = dados.indexOf(',', c2 + 1);
        int c4 = dados.indexOf(',', c3 + 1);
        int c5 = dados.indexOf(',', c4 + 1);

        String seq  = dados.substring(0, c1);
        String lat  = dados.substring(c1 + 1, c2);
        String lon  = dados.substring(c2 + 1, c3);
        String sat  = dados.substring(c3 + 1, c4);
        String hdop = dados.substring(c4 + 1, c5);

        String json = "{\"seq\":" + seq +
                      ",\"lat\":" + lat +
                      ",\"lon\":" + lon +
                      ",\"sat\":" + sat +
                      ",\"hdop\":" + hdop +
                      ",\"rssi\":" + rssi +
                      ",\"snr\":" + snr + "}";

        Serial.println("Enviando: " + json);

        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;
          http.begin(serverURL);
          http.addHeader("Content-Type", "application/json");
          int code = http.POST(json);
          Serial.println("HTTP: " + String(code));
          http.end();
        }

        rssi = "0";
        snr  = "0";
      }
    }
  }
}