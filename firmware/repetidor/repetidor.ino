#include "RoboCore_SMW_SX1276M0.h"
#include <HardwareSerial.h>

HardwareSerial LoRaSerial(2);
#define RXD2 16
#define TXD2 17
SMW_SX1276M0 lorawan(LoRaSerial);

// --- Deduplicação: guarda os últimos pacotes já retransmitidos ---
#define BUFFER_SIZE 20
struct PacoteVisto {
  int nodeId;
  int seq;
};
PacoteVisto vistos[BUFFER_SIZE];
int indiceBuffer = 0;

bool jaVisto(int nodeId, int seq) {
  for (int i = 0; i < BUFFER_SIZE; i++) {
    if (vistos[i].nodeId == nodeId && vistos[i].seq == seq) return true;
  }
  return false;
}

void marcarVisto(int nodeId, int seq) {
  vistos[indiceBuffer].nodeId = nodeId;
  vistos[indiceBuffer].seq = seq;
  indiceBuffer = (indiceBuffer + 1) % BUFFER_SIZE;
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

void setup() {
  Serial.begin(115200);
  Serial.println("========================================");
  Serial.println("   REPETIDOR - NO INTERMEDIARIO v1.0");
  Serial.println("========================================");

  LoRaSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
  lorawan.setPinReset(5);
  lorawan.reset();
  delay(3000);
  lorawan.set_JoinMode(SMW_SX1276M0_JOIN_MODE_P2P);
  delay(2000);

  randomSeed(analogRead(0)); // semente para o backoff aleatório

  Serial.println("[INFO] Repetidor pronto — aguardando pacotes");
}

void loop() {
  while (LoRaSerial.available()) {
    String raw = LoRaSerial.readStringUntil('\n');
    raw.trim();

    if (raw.indexOf("RECVB") > 0) {
      int idx = raw.lastIndexOf(':');
      if (idx > 0) {
        String hex = raw.substring(idx + 1);
        hex.trim();
        String payload = hexParaTexto(hex);

        // Formato esperado: nodeId,seq,ttl,lat,lon,sat,hdop,status
        int c1 = payload.indexOf(',');
        int c2 = payload.indexOf(',', c1 + 1);
        int c3 = payload.indexOf(',', c2 + 1);

        if (c1 < 0 || c2 < 0 || c3 < 0) {
          Serial.println("[ERRO] Payload malformado: " + payload);
          continue;
        }

        int nodeId = payload.substring(0, c1).toInt();
        int seq    = payload.substring(c1 + 1, c2).toInt();
        int ttl    = payload.substring(c2 + 1, c3).toInt();
        String resto = payload.substring(c3 + 1);

        Serial.println("[RX] no=" + String(nodeId) + " seq=" + String(seq) + " ttl=" + String(ttl));

        if (jaVisto(nodeId, seq)) {
          Serial.println("[SKIP] Ja retransmitido, descartando");
          continue;
        }
        marcarVisto(nodeId, seq);

        ttl--;
        if (ttl <= 0) {
          Serial.println("[DROP] TTL esgotado");
          continue;
        }

        String novoPayload = String(nodeId) + "," + String(seq) + "," + String(ttl) + "," + resto;

        int atraso = random(100, 500); // backoff aleatorio (ms) p/ reduzir colisao entre repetidores vizinhos
        delay(atraso);

        Serial.println("[FWD] " + novoPayload + " (backoff " + String(atraso) + "ms)");
        lorawan.sendT(1, novoPayload.c_str());
      }
    }
  }
}
