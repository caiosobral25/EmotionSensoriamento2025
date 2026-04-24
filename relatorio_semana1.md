# Relatório: Semana 1  
**Fernando d’Ávila**

---

## 1. Objetivos da Etapa
O objetivo desta fase foi o levantamento dos requisitos de software e o mapeamento dos identificadores (IDs) de comunicação do inversor **Sevcon Gen4 Size 8** para monitoramento dos parâmetros críticos de performance.

---

## 2. Mapeamento do Barramento CAN (Sevcon Gen4)

Conforme o planejamento, foram identificados os IDs documentados para os três parâmetros prioritários:

- **RPM do Motor:** Identificado via TPDO1 no endereço `0x181`
- **Velocidade do Veículo:** Identificado via TPDO3 no endereço `0x381`
- **Torque Real:** Identificado via TPDO2 no endereço `0x281`

### Análise de Dados
- A comunicação utiliza o protocolo **CANopen**
- Os dados de RPM e Velocidade devem ser filtrados e decodificados para processamento no **ESP32**

---

## 3. Definição da Pilha de Software

Para o controle do barramento no microcontrolador **ESP32 (Subsistema 1)**, foi definida a seguinte estrutura:

- **Biblioteca:** Driver nativo `ESP32-TWAI-CAN` (Two-Wire Automotive Interface)
- **Justificativa:**  
  Garante estabilidade e suporte nativo ao controlador CAN interno do ESP32, permitindo a leitura de frames brutos

---

## 4. Detalhamento da Pinagem e Conexões (ESP32)

A conexão entre o ESP32 e o barramento CAN exige um **transceiver externo** para converter sinais lógicos em níveis diferenciais (**CANH/CANL**).

### Especificação dos Sinais

#### CAN TX (Transmit) — GPIO 5
- **Função:** Saída de dados do ESP32 para o transceiver  
- **Fluxo:** Envio de frames de configuração ou requisições  
- **Nível Lógico:** 3.3V (CMOS)

#### CAN RX (Receive) — GPIO 4
- **Função:** Entrada de dados do transceiver para o ESP32  
- **Fluxo:** Recebe mensagens do barramento (RPM, Torque, Velocidade)  
- **Nível Lógico:** 3.3V *(ESP32 não tolera 5V)*

---

### Tabela de Pinagem — ESP32 (CAN Interface)

| Função CAN | Pino ESP32 (GPIO) | Conexão Transceiver | Direção do sinal |
|------------|-------------------|---------------------|------------------|
| CAN_TX     | GPIO 5            | TXD (D)             | Saída            |
| CAN_RX     | GPIO 4            | RXD (R)             | Entrada          |

---

### Requisitos Elétricos

- **Transceiver:**  
  Uso obrigatório de modelo compatível com 3.3V  
  Exemplo: `SN65HVD230`

- **Terminação:**  
  Resistência de **120Ω** entre CANH e CANL (já presente no protoboard)

---

## 5. Conclusão e Próximos Passos

O mapeamento de IDs e a definição de pinagem garantem que o firmware desenvolvido nas etapas **S2 e S3** seja compatível com o hardware físico.

### Próximo marco:
- Validação da comunicação **LoRa**
- Leitura bruta dos frames CAN

---
