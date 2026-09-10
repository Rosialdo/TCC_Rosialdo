#include "RoboCore_SMW_SX1276M0.h"
#include <HardwareSerial.h>
#include <TinyGPS++.h>

HardwareSerial LoRaSerial(2);
#define RXD2 16
#define TXD2 17
SMW_SX1276M0 lorawan(LoRaSerial);

HardwareSerial gpsSerial(1);
#define GPS_RX 27
#define GPS_TX 26
TinyGPSPlus gps;

// --- Identificação do nó e protocolo multi-hop ---
#define NODE_ID 1              // identificador unico deste no sensor na rede
#define TTL_INICIAL 5          // numero maximo de saltos permitidos ate o Gateway

// --- Intervalo entre ciclos de transmissao ---
// Valor de campo (conforme TCC): 3-4 minutos. Para testes de bancada,
// reduza temporariamente para 15000-30000 (15-30s) e volte ao valor de
// campo antes dos ensaios reais descritos na Secao 4.2.
#define INTERVALO_TRANSMISSAO_MS 180000UL

int seq = 0;

void printSeparador() {
  Serial.println("========================================");
}

void setup() {
  Serial.begin(115200);
  printSeparador();
  Serial.println("   COLEIRA - NO SENSOR (embarcado) v2.0");
  Serial.println("   Node ID: " + String(NODE_ID) + " | TTL inicial: " + String(TTL_INICIAL));
  printSeparador();

  Serial.print("[LORA] Inicializando...");
  LoRaSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
  lorawan.setPinReset(5);
  lorawan.reset();
  delay(3000);
  lorawan.set_JoinMode(SMW_SX1276M0_JOIN_MODE_P2P);
  delay(2000);
  Serial.println(" OK");

  Serial.print("[GPS]  Inicializando...");
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println(" OK");

  printSeparador();
  Serial.println("[INFO] Sistema pronto — aguardando fix GPS");
  printSeparador();
}

void loop() {
  Serial.println();
  Serial.println("-------- CICLO #" + String(seq) + " --------");

  Serial.print("[GPS]  Buscando sinal... ");
  unsigned long inicio = millis();
  while (millis() - inicio < 5000) {
    while (gpsSerial.available())
      gps.encode(gpsSerial.read());
  }

  Serial.println("Satelites: " + String(gps.satellites.value()));

  // Formato do payload (protocolo multi-hop):
  // nodeId,seq,ttl,lat,lon,sat,hdop,status
  String payload;

  if (gps.location.isValid()) {
    float hdop = gps.hdop.hdop();
    float precisao = hdop * 2.5;

    Serial.println("[GPS]  FIX OK!");
    Serial.println("[GPS]  Lat      : " + String(gps.location.lat(), 6));
    Serial.println("[GPS]  Lon      : " + String(gps.location.lng(), 6));
    Serial.println("[GPS]  Altitude : " + String(gps.altitude.meters(), 1) + " m");
    Serial.println("[GPS]  Satelites: " + String(gps.satellites.value()));
    Serial.println("[GPS]  HDOP     : " + String(hdop, 2));
    Serial.println("[GPS]  Precisao : ~" + String(precisao, 1) + " m");

    payload = String(NODE_ID) + "," +
              String(seq++) + "," +
              String(TTL_INICIAL) + "," +
              String(gps.location.lat(), 6) + "," +
              String(gps.location.lng(), 6) + "," +
              String(gps.satellites.value()) + "," +
              String(hdop, 2) + ",OK";

  } else {
    Serial.println("[GPS]  Sem fix — enviando status NOFIX");

    payload = String(NODE_ID) + "," +
              String(seq++) + "," +
              String(TTL_INICIAL) + ",0,0," +
              String(gps.satellites.value()) + ",99.99,NOFIX";
  }

  Serial.println("[LORA] Enviando payload...");
  Serial.println("[LORA] >> " + payload);

  CommandResponse r = lorawan.sendT(1, payload.c_str());
  if (r == CommandResponse::OK)
    Serial.println("[LORA] Transmissao: SUCESSO");
  else
    Serial.println("[LORA] Transmissao: FALHOU");

  Serial.println("[INFO] Aguardando proximo ciclo (" + String(INTERVALO_TRANSMISSAO_MS / 1000) + "s)...");
  delay(INTERVALO_TRANSMISSAO_MS);
}
