# Relatório de Pinagem e Hardware (Versão MCP2551) - Semana 2 (S2)
**Projeto:** Sistema de Telemetria – Fórmula E-Motion
**Responsável:** Fernando d’Ávila[cite: 1]
**Data Limite da Sprint:** 28/04/2026[cite: 1]

## 1. Interface CAN (ESP32 <-> Transceiver MCP2551)
Atenção: O MCP2551 exige alimentação de 5V. Para proteger o ESP32, recomenda-se o uso de um divisor de tensão no pino de recepção (RX)[cite: 1].

| Função CAN | Pino ESP32 (GPIO) | Destino (MCP2551) | Observação |
| :--- | :--- | :--- | :--- |
| **VCC** | **5V / Vin** | **Pino 3 (VCC)** | Alimentação obrigatória de 5V[cite: 1]. |
| **GND** | **GND** | **Pino 2 (GND)** | Referência de sinal comum[cite: 1]. |
| **CAN TX** | **GPIO 5** | **Pino 1 (TXD)** | Saída do ESP32 para o MCP2551[cite: 1]. |
| **CAN RX** | **GPIO 4** | **Pino 4 (RXD)** | Entrada do ESP32 (Requer Divisor de Tensão)[cite: 1]. |
| **Slope (Rs)** | **GND** | **Pino 8 (Rs)** | Conectar ao GND para modo Alta Velocidade[cite: 1]. |

---

## 2. Interface LoRa (ESP32 <-> Ebyte E32-433T20D)
Conexão para transmissão sem fio dos dados coletados para a base receptora[cite: 1].

| Função LoRa | Pino ESP32 (GPIO) | Destino (Módulo E32) | Observação |
| :--- | :--- | :--- | :--- |
| **VCC** | **5V / Vin** | **VCC** | Alimentação para rádio 433 MHz[cite: 1]. |
| **GND** | **GND** | **GND** | Referência comum[cite: 1]. |
| **TXD2** | **GPIO 17** | **RXD** | Comunicação Serial (UART)[cite: 1]. |
| **RXD2** | **GPIO 16** | **TXD** | Comunicação Serial (UART)[cite: 1]. |
| **Modo M0** | **GND** | **M0** | Fixado em GND para Modo Normal[cite: 1]. |
| **Modo M1** | **GND** | **M1** | Fixado em GND para Modo Normal[cite: 1]. |

---

## 3. Conexão ao Barramento CAN do Veículo
Fiação física entre o transceptor MCP2551 e o inversor Sevcon Gen4 Size 8[cite: 1].

*   **Pino 7 (CANH):** Conectar ao barramento CAN High do carro[cite: 1].
*   **Pino 6 (CANL):** Conectar ao barramento CAN Low do carro[cite: 1].
*   **Resistor de Terminação:** Instalar resistor de **120 Ohms** em paralelo entre os pinos 6 e 7[cite: 1].

---

## 4. Notas Técnicas e Cuidados (Crítico)
*   **Proteção do RX (GPIO 4):** O pino 4 (RXD) do MCP2551 enviará 5V para o ESP32. Utilize um divisor de tensão (ex: resistores de 1kΩ e 2kΩ) para reduzir o sinal para ~3.3V[cite: 1].
*   **Nível do TX (GPIO 5):** O sinal de 3.3V do ESP32 no pino TXD geralmente é suficiente para o MCP2551 interpretar como nível lógico alto, mas deve ser validado em bancada[cite: 1].
*   **Pino 8 (Rs):** Diferente do modelo anterior, o MCP2551 exige que o pino 8 esteja aterrado para funcionar em modo de alta velocidade; se ficar solto, o chip não transmitirá dados[cite: 1].
