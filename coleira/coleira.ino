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

int seq = 0;

void printSeparador() {
  Serial.println("========================================");
}

void setup() {
  Serial.begin(115200);
  printSeparador();
  Serial.println("   COLEIRA - NO EMBARCADO v1.0");
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

    String payload = String(seq++) + "," +
                     String(gps.location.lat(), 6) + "," +
                     String(gps.location.lng(), 6) + "," +
                     String(gps.satellites.value()) + "," +
                     String(hdop, 2) + ",OK";

    Serial.println("[LORA] Enviando payload...");
    Serial.println("[LORA] >> " + payload);

    CommandResponse r = lorawan.sendT(1, payload.c_str());
    if (r == CommandResponse::OK)
      Serial.println("[LORA] Transmissao: SUCESSO ✓");
    else
      Serial.println("[LORA] Transmissao: FALHOU ✗");

  } else {
    Serial.println("[GPS]  Sem fix — enviando status NOFIX");

    String payload = String(seq++) + ",0,0," +
                     String(gps.satellites.value()) + ",99.99,NOFIX";

    Serial.println("[LORA] >> " + payload);
    CommandResponse r = lorawan.sendT(1, payload.c_str());
    if (r == CommandResponse::OK)
      Serial.println("[LORA] Transmissao: SUCESSO ✓");
    else
      Serial.println("[LORA] Transmissao: FALHOU ✗");
  }

  Serial.println("[INFO] Aguardando proximo ciclo (10s)...");
  delay(10000);
}
