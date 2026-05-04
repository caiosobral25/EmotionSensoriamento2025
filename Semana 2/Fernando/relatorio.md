# Relatório de Pinagem e Hardware (Versão MCP2551 - GPIO 6) - Semana 2 (S2)
**Projeto:** Sistema de Telemetria – Fórmula E-Motion
**Responsável:** Fernando d’Ávila 


## 1. Interface CAN (ESP32 <-> Transceiver MCP2551)
Configuração de hardware para leitura dos IDs críticos (0x181, 0x281, 0x381) do inversor Sevcon Gen4 .

| Função CAN | Pino ESP32 (GPIO) | Destino (MCP2551) | Observação |
| :--- | :--- | :--- | :--- |
| **VCC** | **5V / Vin** | **Pino 3 (VCC)** | O MCP2551 exige 5V para operação . |
| **GND** | **GND** | **Pino 2 (GND)** | Referência comum de sinal . |
| **CAN TX** | **GPIO 5** | **Pino 1 (TXD)** | Saída do controlador TWAI para o transceptor . |
| **CAN RX** | **GPIO 6** | **Pino 4 (RXD)** | Entrada para o ESP32 (Necessário Divisor de Tensão) . |
| **Slope (Rs)** | **GND** | **Pino 8 (Rs)** | Aterrado para garantir modo de Alta Velocidade . |

---

## 2. Interface LoRa (ESP32 <-> Ebyte E32-433T20D)
Módulo responsável pela transmissão sem fio dos parâmetros de RPM, Velocidade e Torque .

| Função LoRa | Pino ESP32 (GPIO) | Destino (Módulo E32) | Observação |
| :--- | :--- | :--- | :--- |
| **VCC** | **5V / Vin** | **VCC** | Alimentação do rádio (433 MHz) . |
| **GND** | **GND** | **GND** | Referência comum . |
| **TXD2** | **GPIO 17** | **RXD** | Comunicação Serial UART . |
| **RXD2** | **GPIO 16** | **TXD** | Comunicação Serial UART . |
| **Modo M0** | **GND** | **M0** | Configurado para Modo de Operação Normal . |
| **Modo M1** | **GND** | **M1** | Configurado para Modo de Operação Normal . |

---

## 3. Conexão Física ao Barramento do Carro
Interligação entre a PCB embarcada e o barramento do inversor Sevcon Gen4 Size 8 .

*   **Pino 7 (CANH):** Conectado à linha CAN High do veículo .
*   **Pino 6 (CANL):** Conectado à linha CAN Low do veículo .
*   **Resistor de Terminação:** Manter resistor de **120 Ohms** em paralelo entre os pinos 6 e 7 .

---

## 4. Avisos Técnicos de Segurança
*   **Proteção de Tensão (Crítico):** Como o MCP2551 envia sinais de 5V no pino RXD e o ESP32 opera em 3.3V, um divisor de tensão é obrigatório no **GPIO 6** para evitar danos permanentes ao chip .
*   **Conflito de Boot:** O uso do GPIO 6 pode causar falha na execução do firmware, pois este pino é compartilhado com a Flash interna do ESP32. Recomenda-se validação imediata em bancada .
*   **Referência GND:** O GND da unidade embarcada deve ser unificado ao GND do inversor para estabilidade na leitura dos frames brutos .
