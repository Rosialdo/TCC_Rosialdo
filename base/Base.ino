#include "RoboCore_SMW_SX1276M0.h"
#include <HardwareSerial.h>

HardwareSerial LoRaSerial(2);
#define RXD2 16
#define TXD2 17
SMW_SX1276M0 lorawan(LoRaSerial);

// Converte string hex para texto legível
String hexToString(String hex) {
  String result = "";
  for (int i = 0; i < hex.length(); i += 2) {
    String byteStr = hex.substring(i, i + 2);
    char c = (char) strtol(byteStr.c_str(), NULL, 16);
    result += c;
  }
  return result;
}

void setup() {
  Serial.begin(115200);
  LoRaSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
  lorawan.setPinReset(5);
  lorawan.reset();
  delay(3000);
  lorawan.set_JoinMode(SMW_SX1276M0_JOIN_MODE_P2P);
  delay(2000);
  Serial.println("=== ESTAÇÃO BASE aguardando ===");
}

void loop() {
  if (LoRaSerial.available()) {
    String msg = LoRaSerial.readStringUntil('\n');
    msg.trim();

    // Filtra só linhas de pacote recebido
    if (msg.indexOf("RECVB") >= 0) {
      Serial.println("─────────────────────────");
      Serial.println("📦 Pacote recebido!");

      // Extrai a parte hex depois do ":"
      int colonIdx = msg.lastIndexOf(':');
      if (colonIdx >= 0) {
        String hexPayload = msg.substring(colonIdx + 1);
        hexPayload.trim();
        String payload = hexToString(hexPayload);
        Serial.println("Payload: " + payload);

        // Parseia: id,seq,lat,lng,sat,bat
        int c1 = payload.indexOf(',');
        int c2 = payload.indexOf(',', c1 + 1);
        int c3 = payload.indexOf(',', c2 + 1);
        int c4 = payload.indexOf(',', c3 + 1);
        int c5 = payload.indexOf(',', c4 + 1);

        if (c5 > 0) {
          Serial.println("ID:   " + payload.substring(0, c1));
          Serial.println("Seq:  " + payload.substring(c1+1, c2));
          Serial.println("Lat:  " + payload.substring(c2+1, c3));
          Serial.println("Lng:  " + payload.substring(c3+1, c4));
          Serial.println("Sat:  " + payload.substring(c4+1, c5));
          Serial.println("Bat:  " + payload.substring(c5+1) + "%");
        }
      }
      Serial.println("─────────────────────────");
    }
  }
}
