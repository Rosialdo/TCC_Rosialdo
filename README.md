# 🐄 TCC_Rosialdo — Rastreio Bovino com LoRa e GPS
 
> **Rastreio bovino: Uma solução Offline-First com LoRa e GPS para a realidade de Roraima**  
> Universidade Federal de Roraima (UFRR) — Bacharelado em Ciência da Computação  
> Autor: Rosialdo Queivison Vidinho de Queiroz Vicente  
> Orientador: Prof. Dr. Felipe Lobo
 
---
 
## 📋 Sobre o Projeto
 
Este projeto propõe e implementa um sistema de rastreamento bovino baseado em comunicação **LoRa P2P** e **GPS**, com armazenamento local e painel de visualização, voltado para propriedades rurais com conectividade limitada ou inexistente.
 
A arquitetura é composta por dois elementos principais:
 
- **Nó Embarcado (Coleira):** IoT DevKit LoRaWAN + receptor GPS NEO-6M
- **Estação Base (Concentrador):** Raspberry Pi + IoT DevKit LoRaWAN + Node-RED + SQLite
---
 
## 🏗️ Arquitetura do Sistema
 
```
[Coleira - ESP32]
   GPS NEO-6M → lê coordenadas
   SX1276     → transmite via LoRa P2P (902-928 MHz)
        |
        | LoRa P2P
        ↓
[Estação Base - Raspberry Pi]
   SX1276     → recebe pacotes LoRa
   ESP32 Base → parseia payload e envia via HTTP POST (WiFi)
        |
        | HTTP POST (JSON)
        ↓
[Node-RED]
   → Salva no SQLite (/home/pi/rastreio.db)
   → Motor de regras: geofencing + perda de contato
   → Painel visual: worldmap (http://<ip>:1880/worldmap)
```
 
---
 
## 🛠️ Hardware Utilizado
 
| Componente | Descrição |
|---|---|
| ESP32 | Microcontrolador principal (coleira e base) |
| SMW-SX1276M0 | Módulo LoRa transceptor (Semtech SX1276) |
| GPS NEO-6M | Receptor GPS L1 (C/A) |
| Raspberry Pi Zero 2w| Concentrador local (Estação Base) |
| Painel Solar + Bateria | Alimentação autônoma da Estação Base |
 
---

## Hardware da Base

### IoT DevKit LoRaWAN 

<img src="/images/lora_base.jpg" width="80%" height="80%" alt= "IoT DevKit base">

#### `Codigo para base`

```
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
    raw.trim();### IoT DevKit LoRaWAN + GPS

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
```

### Raspbarry pi zero 2w

<img src="/images/rasp.jpg" width="80%" height="80%" alt= "Raspbarry">

### Modulo de bateria

<img src="/images/carregador.jpg" width="80%" height="80%" alt= "Bateria">

### Placa Solar

<img src="/images/painel_base.jpg" width="80%" height="80%" alt= "Placa Solar tipo C">

---

## Hardware da Coleira

### IoT DevKit LoRaWAN + GPS

<img src="/images/lora_coleira.jpg" width="80%" height="80%" alt= "IoT DevKit coleira">

#### `Codigo para coleira`

```
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

```

### Modulo de Bateria

<img src="/images/carregador.jpg" width="80%" height="80%" alt= "Bateria">
---
 
## 📡 Protocolo de Comunicação
 
- **Tecnologia:** LoRa P2P (sem stack LoRaWAN)
- **Frequência:** 902–928 MHz
- **Fator de Espalhamento (SF):** SF7–SF12 (configurável)
- **Intervalo de transmissão:** ~16 segundos (coleira)
- **Payload:** formato CSV compacto → `seq,lat,lon,sat,hdop,rssi,snr`
---
 
## 💾 Banco de Dados
 
Banco SQLite local em `/home/pi/rastreio.db` com duas tabelas:
 
### Tabela `pacotes`
```sql
CREATE TABLE pacotes (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    seq     INTEGER,
    lat     REAL,
    lon     REAL,
    sat     INTEGER,
    hdop    REAL,
    rssi    INTEGER,
    snr     INTEGER,
    ts_gw   TEXT,
    alerta  INTEGER DEFAULT 0
);
```
 
### Tabela `alertas`
```sql
CREATE TABLE alertas (
    id    INTEGER PRIMARY KEY AUTOINCREMENT,
    tipo  TEXT,        -- 'FUGA' ou 'PERDA_CONTATO'
    lat   REAL,
    lon   REAL,
    seq   INTEGER,
    ts    TEXT
);
```
 
---
 
## 🗺️ Funcionalidades Implementadas
 
### ✅ Comunicação LoRa P2P
- Coleira transmite payload a cada ~16s
- Estação Base recebe, parseia e encaminha via HTTP POST para o Node-RED
### ✅ Armazenamento Local (Offline-First)
- Todos os pacotes recebidos são persistidos no SQLite local
- Sistema continua operando sem internet
### ✅ Painel de Visualização (Worldmap)
- Mapa atualizado a cada 10 segundos com a posição da coleira
- Marcador verde = dentro do geofence | Marcador vermelho = fuga detectada
- Popup com seq, satélites, RSSI, SNR e timestamp
### ✅ Geofencing Local
- Polígono virtual (retângulo) definido na Estação Base
- Quando coordenada válida (GPS com fix) está fora do polígono → `alerta = 1`
- Registro automático na tabela `alertas` com tipo `FUGA`
### ✅ Detecção de Perda de Contato (Heartbeat)
- Monitor verifica a cada 60s o timestamp do último pacote
- Se ausência > 48s (3 intervalos) → alerta `PERDA_CONTATO` registrado
- Salva última posição conhecida e horário do último pacote
---
 
## 📂 Estrutura do Repositório
 
```
TCC_Rosialdo/
├── firmware/
│   ├── coleira/
│   │   └── coleira.ino        # Firmware do nó embarcado (ESP32 + GPS + LoRa)
│   └── base/
│       └── base.ino           # Firmware da estação base (ESP32 + LoRa + WiFi)
├── nodered/
│   └── flows.json             # Export do flow do Node-RED
├── banco/
│   └── schema.sql             # Schema do banco SQLite
├── docs/
│   ├── TCC_2.pdf              # Arquivo do TCC 2
└── README.md
```
 
---
 
## 🚀 Como Reproduzir
 
### 1. Pré-requisitos
 
- Arduino IDE com suporte ao ESP32
- Biblioteca `RoboCore_SMW_SX1276M0`
- Biblioteca `TinyGPS++`
- Raspberry Pi com Node-RED instalado
- Node-RED packages: `node-red-contrib-web-worldmap`, `node-red-node-sqlite`
### 2. Firmware da Coleira
 
1. Abre `firmware/coleira/coleira.ino` no Arduino IDE
2. Conecta o ESP32 via USB
3. Carrega o firmware
### 3. Firmware da Estação Base
 
1. Abre `firmware/base/base.ino` no Arduino IDE
2. Atualiza as credenciais WiFi e IP do Raspberry Pi:
```cpp
const char* ssid = "SUA_REDE";
const char* password = "SUA_SENHA";
const char* serverURL = "http://<IP_RASP>:1880/dados";
```
3. Carrega o firmware no ESP32 da base
### 4. Configuração do Raspberry Pi
 
```bash
# Instalar Node-RED (se não tiver)
bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)
 
# Instalar pacotes necessários
cd ~/.node-red
npm install node-red-contrib-web-worldmap
npm install node-red-node-sqlite
 
# Criar banco de dados
sqlite3 /home/pi/rastreio.db < banco/schema.sql
 
# Iniciar Node-RED
node-red-start
```
 
### 5. Importar o Flow no Node-RED
 
1. Acessa `http://<IP_RASP>:1880`
2. Menu → Import → cola o conteúdo de `nodered/flows.json`
3. Clica **Deploy**
### 6. Acessar o Painel
 
```
http://<IP_RASP>:1880/worldmap
```
 
---
 
## 📊 Perguntas de Pesquisa
 
| ID | Pergunta | Status |
|---|---|---|
| PP01 | Viabilidade da arquitetura offline-first para coleta e armazenamento contínuo | ✅ Implementado |
| PP02 | Cobertura efetiva do enlace LoRa P2P em ambiente rural (RSSI, SNR, PDR) | ⏳ Em teste |
| PP03 | Geofencing e detecção de perda de contato em modo offline-first | ✅ Implementado |
| PP04 | Desempenho temporal do ciclo (TTFF do GPS, latência até persistência local) | ⏳ Em teste |
 
---
 
## 🔭 Trabalhos Futuros
 
- Configuração do Raspberry Pi como Access Point WiFi autônomo (sem roteador externo)
- Interface web completa com histórico de trilha e exportação CSV
- Suporte a múltiplos nós simultâneos
- Estimativa de autonomia da bateria da coleira
- Testes de alcance em campo aberto (propriedade rural em Roraima)
---
 
## 📚 Referências
 

 
## 📄 Licença
 
Este projeto é desenvolvido para fins acadêmicos — UFRR, 2026.
 
---
 
> 📬 Contato: rosialdovidinho3@gmail.com  
> 🔗 Repositório: https://github.com/Rosialdo/TCC_Rosialdo.git
 
