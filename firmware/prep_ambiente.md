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


## 2. Estrutura de dados enviada pelo Gateway

O device criado na Ubidots (label `boi_01`) recebe as seguintes variáveis por
pacote, via HTTP POST para
`https://industrial.api.ubidots.com/api/v1.6/devices/boi_01`:

| Variável | Tipo de envio | Conta como dot? | Observação |
|---|---|---|---|
| `position` | Variável (lat/lng) + `context` | Sim (1 dot) | `seq`, `sat`, `hdop`, `gps_fix` e `hop_count` viajam dentro do `context` — não contam dot extra, pois contexto é metadado |
| `rssi` | Variável | Sim (1 dot) | Força do sinal recebido no último salto |
| `snr` | Variável | Sim (1 dot) | Relação sinal-ruído do último salto |
| `geofence_alert` | Variável | Só quando muda de valor | Calculado localmente no Gateway (ver Seção 5) |
| `heartbeat_alert` | Variável | Só quando muda de valor | Calculado localmente no Gateway (ver Seção 5) |

Payload normal: **3 dots por pacote** (contra 9 dots de uma versão anterior
que expunha cada campo como variável própria). Com o intervalo de campo de 3
minutos (Seção 4.2 do TCC), isso dá ~480 pacotes/dia × 3 = **1.440 dots/dia**,
bem dentro da cota de 4.000/dia mesmo somando os alertas.

Exemplo de payload principal enviado pelo Gateway:

```json
{
  "position": {
    "value": 1,
    "context": {
      "lat": 2.805925,
      "lng": -60.748853,
      "seq": 2013,
      "sat": 10,
      "hdop": 0.82,
      "gps_fix": 1,
      "hop_count": 0
    }
  },
  "rssi": -36,
  "snr": 29
}
```

## 3. Criação do Dashboard

1. **Data → Dashboards** → ícone de dashboard (canto superior esquerdo) →
   template **Blank** → nomeie (ex.: "Rastreio Boi 01")

2. **Widget de mapa**
   - `+` (canto superior direito) → **Map** → **+ Add marker group** →
     selecione o device `boi_01`
   - A Ubidots detecta automaticamente a variável `position` e plota o pino
     usando `lat`/`lng` do contexto

3. **Gráfico de RSSI/SNR**
   - `+` → **Line Chart** → selecione as variáveis `rssi` e `snr` juntas
     (mesmo gráfico)

4. **Tabela com os campos de diagnóstico** (`seq`, `sat`, `hdop`, `gps_fix`,
   `hop_count` — os que vivem dentro do contexto de `position`)
   - `+` → **Values Table** (não confundir com *Devices Table*)
   - `+ add column` → selecione o tipo **Context** (não *Value*)
   - Selecione o device `boi_01` e a variável `position` — só depois dessa
     seleção os campos seguintes aparecem
   - Preencha o **nome da coluna** (ex.: "seq")
   - No campo de texto livre **context key**, digite exatamente o nome do
     campo tal como está no JSON: `seq`, `sat`, `hdop`, `gps_fix` ou
     `hop_count`
   - Escolha o **tipo** (Number para todos esses campos)
   - Repita o processo — uma coluna por vez — para os 5 campos

## 4. Configuração dos Eventos (alertas por e-mail)

O plano STEM **não inclui** os gatilhos nativos de **Geofence** e
**Inactivity** (recursos pagos). Por isso, `geofence_alert` e
`heartbeat_alert` são calculados localmente no firmware do Gateway (ver
Seção 5) e enviados como variáveis simples, compatíveis com o gatilho
genérico **Value**, disponível no plano gratuito.

### Evento 1 — Fuga Detectada

1. **Data → Events** → `+` → vincule ao device `boi_01`
2. **+ add trigger** → tipo **Value** → variável `geofence_alert` →
   condição **igual a 1**
3. **+ add action** → **Email** → assunto "Fuga Detectada — Boi 01" → corpo
   simples
4. Salve o evento como "Fuga Detectada"

### Evento 2 — Perda de Contato

1. Novo evento, mesmo device `boi_01`
2. **+ add trigger** → tipo **Value** → variável `heartbeat_alert` →
   condição **igual a 1**
3. **+ add action** → **Email** → assunto "Perda de Contato — Boi 01"
4. Salve o evento como "Perda de Contato"

> **Observação:** a variável `heartbeat_alert` só passa a existir na
> Ubidots (e, portanto, só aparece na lista de seleção do evento) depois que
> o Gateway a envia pela primeira vez — o que só ocorre quando uma perda de
> contato real é detectada. Se a variável ainda não aparecer, force uma
> perda de contato de teste (desligue a coleira por um tempo maior que
> `LIMIAR_PERDA_CONTATO_MS`) antes de tentar criar o evento.

## 5. Lógica calculada no Gateway (não na nuvem)

Como o plano STEM não oferece Geofence/Inactivity nativos, o `gateway.ino`
implementa:

- **Geofencing:** algoritmo *ray-casting* (ponto-em-polígono) rodando a cada
  pacote recebido com fix de GPS válido, comparando `lat`/`lon` contra um
  polígono definido em `poligonoLat[]` / `poligonoLon[]`
- **Heartbeat / perda de contato:** checagem periódica (a cada
  `INTERVALO_CHECK_HEARTBEAT_MS`) comparando o tempo desde o último pacote
  recebido contra `LIMIAR_PERDA_CONTATO_MS` (valor de campo: 3× o intervalo
  de transmissão do nó)
- Ambos os alertas só são reenviados à nuvem **quando o valor muda de
  estado**, para não consumir a cota diária de dots a cada pacote
