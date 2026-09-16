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
