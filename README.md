# 🐄 TCC_Rosialdo — Rastreio Bovino com LoRa e GPS
 
> **Rastreio bovino: Uma solução Offline-First com LoRa e GPS para a realidade de Roraima**  
> Universidade Federal de Roraima (UFRR) — Bacharelado em Ciência da Computação  
> Autor: Rosialdo Queivison Vidinho de Queiroz Vicente  
> Orientador: Prof. Dr. Felipe Lobo
 
---
 
## 📋 Sobre o Projeto
 
Este projeto propõe e implementa um sistema de rastreamento bovino baseado em comunicação **LoRa P2P** e **GPS**, com armazenamento local e painel de visualização, voltado para propriedades rurais com conectividade limitada ou inexistente.
 
A arquitetura é composta por dois elementos principais:
 
- **Nó Embarcado (Coleira):** ESP32 + módulo LoRa SX1276 + receptor GPS NEO-6M
- **Estação Base (Concentrador):** Raspberry Pi + módulo LoRa SX1276 + Node-RED + SQLite
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
| Raspberry Pi | Concentrador local (Estação Base) |
| Painel Solar + Bateria | Alimentação autônoma da Estação Base |
 
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
│   ├── TCC_2.pdf              # Monografia TCC 2
│   └── TCC_2_Artigo.pdf       # Artigo do TCC
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
 
- HAXHIBEQIRI, J. et al. A survey of LoRaWAN for IoT. *Sensors*, 2018.
- MAMATNABIYEV, Z. Animal tracking system based on GPS sensor and LPWAN. *SIST*, 2022.
- KUMKHET et al. Low-cost LoRa-based monitoring for livestock. 2025.
- WELSCHER et al. Base station placement optimization for bovine tracking. 2023.
- u-blox AG. NEO-6 datasheet. 2011.
---
 
## 📄 Licença
 
Este projeto é desenvolvido para fins acadêmicos — UFRR, 2026.
 
---
 
> 📬 Contato: rosialdovidinho3@gmail.com  
> 🔗 Repositório: https://github.com/Rosialdo/TCC_Rosialdo.git
 
