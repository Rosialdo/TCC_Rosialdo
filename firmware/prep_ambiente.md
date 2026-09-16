# Configuração do Ambiente de Desenvolvimento — Arduino IDE
 
Documentação do ambiente usado para desenvolver e gravar os três firmwares do
projeto (coleira, repetidor, gateway). Ambiente de referência: **Arduino IDE
1.8.19** rodando em **Linux (Pop!_OS 24.04 LTS / base Ubuntu)**, com placas IoT DevKit desenvolvidas pela RoboCore que é baseado em um ESP-32 e um lora SX1276M0.
 
---

## 1. Instalação do Arduino IDE
 
1. Baixe a versão 1.8.19 (ou superior) em [arduino.cc/en/software](https://www.arduino.cc/en/software)
2. No Linux, extraia o `.tar.xz` e execute `install.sh` dentro da pasta extraída, ou instale via gerenciador de pacotes da distribuição, se disponível
3. Abra a IDE pela primeira vez para gerar os arquivos de configuração padrão
## 2. Suporte à placa ESP32
 
O Arduino IDE não vem com suporte ao ESP32 por padrão — precisa ser adicionado manualmente:
 
1. Vá em **File → Preferences** (ou **Arquivo → Preferências**)
2. No campo **"Additional Boards Manager URLs"**, adicione:
```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
3. Vá em **Tools → Board → Boards Manager**
4. Busque por **"esp32"** e instale o pacote **"esp32 by Espressif Systems"**
5. Aguarde o download (pode demorar alguns minutos, é um pacote grande — inclui o toolchain de compilação completo)
6. Vá em **Tools → Board → ESP32 Arduino → ESP32 Dev Module** Selecione essa placa como padrão

## 3. Bibliotecas necessárias
 
| Biblioteca | Onde é usada | Como instalar |
|---|---|---|
| `RoboCore_SMW_SX1276M0` | Coleira, Repetidor, Gateway (todas usam o módulo LoRa) | Disponível direto no Library Manager: Sketch → Include Library → Manage Libraries → buscar "RoboCore_SMW_SX1276M0" → Install |
| `TinyGPS++` | Somente na Coleira (leitura do GPS NEO-6M) | Disponível direto no Library Manager: **Sketch → Include Library → Manage Libraries** → buscar "TinyGPSPlus" → Install |
| `WiFi.h` | Somente no Gateway | Já vem embutida no core do ESP32 — não precisa instalar |
| `HTTPClient.h` | Somente no Gateway | Já vem embutida no core do ESP32 — não precisa instalar |
| `HardwareSerial.h` | Todas | Já vem embutida no core do ESP32 — não precisa instalar |

## 4. Pinagem usada nos firmwares
 
Para documentação de hardware, os pinos configurados em código:
 
| Sinal | Pino | Usado em |
|---|---|---|
| LoRa RX (RXD2) | GPIO 16 | Todas as placas |
| LoRa TX (TXD2) | GPIO 17 | Todas as placas |
| LoRa Reset | GPIO 5 | Todas as placas |
| GPS RX | GPIO 27 | Somente Coleira |
| GPS TX | GPIO 26 | Somente Coleira |

# Configuração da Plataforma Ubidots — Nuvem e Dashboard
 
Documentação do ambiente de nuvem usado para armazenamento, visualização e
alertas do sistema de rastreio (Coleira → Repetidor(es) → Gateway → Ubidots).
Ambiente de referência: **Ubidots STEM (plano gratuito)**.
 
---

## 1. Criação da conta e do token
 
1. Crie uma conta em [ubidots.com](https://ubidots.com) selecionando o plano
   **STEM** (gratuito, voltado a projetos pessoais/educacionais)
2. Após o login, vá em **API Credentials** (ícone de perfil → API Credentials,
   ou **Dev Center → Credentials**)
3. Copie o **Default Token** (ou gere um novo em "+ Token") — esse valor vai
   no firmware do Gateway, na constante `ubidotsToken`
4. **Atenção:** esse token dá acesso total de escrita/leitura à conta. Não
   deve ser commitado em repositório público nem compartilhado em texto
   aberto. Se algum token vazar, revogue e gere um novo imediatamente pelo
   mesmo painel
> **Limite do plano STEM:** 4.000 *dots* por dia (cada valor de variável
> atualizado = 1 dot, somando toda a conta). Isso definiu boa parte do
> formato do payload — ver Seção 2.