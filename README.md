# 🐄 TCC_Rosialdo — Rastreio Bovino com LoRa Multi-hop e GPS
 
> **Rastreio bovino: Uma solução com retransmissão LoRa multi-hop e GPS para a realidade de Roraima**
> Universidade Federal de Roraima (UFRR) — Bacharelado em Ciência da Computação
> Autor: Rosialdo Queivison Vidinho de Queiroz Vicente
> Orientador: Prof. Dr. Felipe Lobo
 
---
 
## 📋 Sobre o Projeto
 
Este projeto propõe e implementa um sistema de rastreamento bovino baseado em comunicação **LoRa P2P com retransmissão multi-hop** e **GPS**, capaz de estender a cobertura de rastreamento até um ponto com conectividade à internet (Wi-Fi ou celular), mesmo quando o local de pastagem está fora do alcance direto de rádio desse ponto — situação comum em propriedades rurais extensas de Roraima, com vegetação amazônica densa e relevo irregular.
 
A arquitetura é composta por três camadas:
 
- **Nó Sensor (Coleira):** ESP32 + GPS NEO-6M + LoRa SX1276 — coleta coordenadas e transmite via rádio
- **Cadeia de Repetidores:** nós ESP32 + LoRa SX1276 que retransmitem os pacotes salto a salto, usando flooding controlado com TTL e deduplicação
- **Nó Gateway:** ESP32 + LoRa SX1276 + Wi-Fi — recebe o pacote final da cadeia e o encaminha via HTTP para a plataforma de nuvem (Ubidots)
> ℹ️ **Nota de versão:** as primeiras versões deste projeto usavam uma arquitetura offline-first com Raspberry Pi, Node-RED e SQLite como Estação Base local. Essa abordagem foi substituída pela arquitetura multi-hop descrita aqui, que estende a cobertura por retransmissão até um ponto com internet em vez de operar totalmente offline. O histórico da versão anterior permanece no log de commits do repositório.
 
---
 
## 🏗️ Arquitetura do Sistema
 
```
[Coleira - ESP32]
   GPS NEO-6M → lê coordenadas
   SX1276     → monta payload (nodeId, seq, TTL, lat, lon, sat, hdop, status)
              → transmite via LoRa P2P (902-928 MHz)
        |
        | LoRa P2P (salto 1)
        ↓
[Repetidor 1 - ESP32 + SX1276]
   → recebe pacote, verifica duplicidade (nodeId + seq)
   → decrementa TTL, aguarda backoff aleatorio
   → retransmite (broadcast)
        |
        | LoRa P2P (salto 2..N)
        ↓
   [Repetidor 2, 3... conforme necessario]
        |
        ↓
[Gateway - ESP32 + Wi-Fi]
   SX1276  → recebe pacote final da cadeia
   ESP32   → decodifica, calcula hop_count, monta JSON
           → envia via HTTP POST (Wi-Fi) para a nuvem
        |
        | HTTPS POST (JSON)
        ↓
[Ubidots — plataforma de nuvem]
   → Armazena posicao, RSSI/SNR, hop_count, status do GPS
   → Motor de regras: geofencing + perda de contato (heartbeat)
   → Painel de visualizacao (mapa em tempo real)
```
 
---
 
## 🛠️ Hardware Utilizado
 
| Componente | Onde é usado | Descrição |
|---|---|---|
| ESP32 | Coleira, repetidores, gateway | Microcontrolador principal |
| SMW-SX1276M0 | Coleira, repetidores, gateway | Módulo LoRa transceptor (Semtech SX1276) |
| GPS NEO-6M | Coleira | Receptor GPS L1 (C/A) |
| Bateria | Coleira | Alimentação da coleira |
| Bateria + Painel Solar | Repetidores e Gateway | Alimentação autônoma dos nós fixos em campo |
 
*(o Raspberry Pi Zero 2W não faz mais parte da arquitetura — o Gateway hoje é apenas o ESP32, sem computador de placa única)*
 
---
 
## 📁 Firmware
 
O código de cada nó vive em `firmware/<nó>/`:
 
- [`firmware/coleira/coleira.ino`](firmware/coleira/coleira.ino) — nó sensor
- [`firmware/repetidor/repetidor.ino`](firmware/repetidor/repetidor.ino) — nó repetidor
- [`firmware/gateway/gateway.ino`](firmware/gateway/gateway.ino) — nó gateway
### Formato do payload (protocolo multi-hop)
 
```
nodeId,seq,ttl,lat,lon,sat,hdop,status
```
 
| Campo | Descrição |
|---|---|
| `nodeId` | Identificador do nó sensor de origem |
| `seq` | Contador de sequência (usado para deduplicação e cálculo de PDR) |
| `ttl` | Contador de saltos restantes (decrementado a cada retransmissão) |
| `lat`, `lon` | Coordenadas GPS |
| `sat`, `hdop` | Qualidade do fix GPS |
| `status` | `OK` (fix válido) ou `NOFIX` |
 
### Lógica de cada nó
 
- **Coleira:** amostra o GPS, monta o payload com `TTL_INICIAL` configurado e transmite a cada ciclo (padrão de campo: 3–4 min; reduza para 15–30s durante testes de bancada)
- **Repetidor:** recebe → descarta se já retransmitiu aquele `(nodeId, seq)` → decrementa `ttl` → descarta se `ttl = 0` → aguarda backoff aleatório (100–500ms) → retransmite (broadcast)
- **Gateway:** recebe o pacote final → calcula `hop_count = TTL_INICIAL_CONHECIDO - ttl` → monta JSON → envia por HTTP à Ubidots
---
 
## ☁️ Configuração da Nuvem (Ubidots)
 
1. Crie uma conta em [ubidots.com](https://ubidots.com) (plano educacional/gratuito)
2. Crie um **Device** (ex.: `boi_01`)
3. Crie as variáveis: `position` (contexto `lat`/`lng`), `rssi`, `snr`, `hop_count`, `seq`, `sat`, `hdop`, `gps_fix`
4. No firmware do Gateway, configure:
```cpp
   const char* ubidotsToken = "SEU_TOKEN_UBIDOTS";
   const char* deviceLabel  = "boi_01";
```
5. Monte o dashboard com um widget de mapa (variável `position`) e gráficos de linha para `rssi`/`snr`
6. Configure eventos/alertas na própria plataforma para geofencing e perda de contato (heartbeat)
---
 
## 🗺️ Funcionalidades
 
### ✅ Comunicação LoRa P2P com retransmissão multi-hop
- Coleira transmite periodicamente; repetidores encaminham salto a salto até o Gateway
- Deduplicação por `(nodeId, seq)` e controle de saltos por TTL evitam loops e retransmissões redundantes
### ✅ Gateway com envio direto à nuvem
- Sem dependência de Raspberry Pi ou Node-RED — o próprio ESP32 do Gateway envia via HTTP à Ubidots
### ⏳ Geofencing e perda de contato (nuvem)
- A ser configurado como regras de evento na Ubidots, a partir dos dados recebidos
### ⏳ Painel de visualização
- Dashboard Ubidots com mapa em tempo real, RSSI/SNR e hop_count
---
 
## 📂 Estrutura do Repositório
 
```
TCC_Rosialdo/
├── firmware/
│   ├── coleira/
│   │   └── coleira.ino
│   ├── repetidor/
│   │   └── repetidor.ino
│   └── gateway/
│       └── gateway.ino
├── docs/
│   └── TCC_2.pdf
└── README.md
```
 
*(as pastas `nodered/` e `banco/` da versão anterior foram removidas — não existe mais processamento local na Estação Base)*
 
---
 
## 🚀 Como Reproduzir
 
### 1. Pré-requisitos
 
- Arduino IDE com suporte ao ESP32
- Biblioteca `RoboCore_SMW_SX1276M0`
- Biblioteca `TinyGPS++`
- Conta na Ubidots com um device configurado (veja seção acima)
### 2. Firmware da Coleira
 
1. Abra `firmware/coleira/coleira.ino`
2. Ajuste `NODE_ID` e `TTL_INICIAL` se necessário
3. Carregue no ESP32 da coleira
### 3. Firmware do(s) Repetidor(es)
 
1. Abra `firmware/repetidor/repetidor.ino`
2. Carregue o mesmo firmware em cada ESP32 que atuará como repetidor
### 4. Firmware do Gateway
 
1. Abra `firmware/gateway/gateway.ino`
2. Atualize as credenciais de Wi-Fi e o token/device da Ubidots:
```cpp
   const char* ssid = "SUA_REDE";
   const char* password = "SUA_SENHA";
   const char* ubidotsToken = "SEU_TOKEN_UBIDOTS";
   const char* deviceLabel  = "boi_01";
```
3. Confirme que `TTL_INICIAL_CONHECIDO` bate com o `TTL_INICIAL` usado na coleira
4. Carregue no ESP32 do Gateway
### 5. Ordem de testes recomendada
 
1. Bancada, sem GPS real (coordenadas fixas) — valida o protocolo multi-hop (TTL, dedup, backoff)
2. Bancada com GPS real
3. Confirma chegada dos dados no painel Ubidots
4. Campo, curta distância, com um repetidor real
5. Campo, cenário real (ensaios da Seção 4.2 do TCC)
---
 
## 📊 Perguntas de Pesquisa
 
| ID | Pergunta | Status |
|---|---|---|
| PP01 | Viabilidade da retransmissão multi-hop para entrega confiável até o Gateway | ⏳ Em desenvolvimento |
| PP02 | Cobertura efetiva de cada enlace da cadeia (RSSI, SNR, PDR) | ⏳ Em teste |
| PP03 | Geofencing e perda de contato processados na nuvem | ⏳ Em desenvolvimento |
| PP04 | Desempenho temporal de ponta a ponta (TTFF, latência multi-hop, latência até a nuvem) | ⏳ Em teste |
 
---
 
## 🔭 Trabalhos Futuros
 
- Leitura de nível de bateria na coleira e nos repetidores
- Interface web adicional além do dashboard Ubidots (histórico de trilha, exportação CSV)
- Suporte a múltiplos nós sensores simultâneos
- Uso do recurso de Channel Activity Detection (CAD) do SX1276 para reduzir colisões entre repetidores
- Testes de alcance em campo aberto (propriedade rural em Roraima)
---
 
## 📚 Referências
 
Ver lista completa no TCC (`docs/TCC_2.pdf`).
 
## 📄 Licença
 
Este projeto é desenvolvido para fins acadêmicos — UFRR, 2026.
 
---
 
> 📬 Contato: rosialdovidinho3@gmail.com
> 🔗 Repositório: https://github.com/Rosialdo/TCC_Rosialdo.git