# Relatório de Pinagem e Hardware - Semana 2 
**Responsável:** Fernando d’Ávila


## 1. Interface CAN (ESP32 <-> Transceiver SN65HVD230)
Conexão para interface com o inversor Sevcon Gen4 para leitura de RPM, Velocidade e Torque.

| Função CAN | Pino ESP32 (GPIO) | Destino (Transceptor) | Observação |
| :--- | :--- | :--- | :--- |
| **VCC** | **3.3V** | **VCC** | Alimentação lógica do chip. |
| **GND** | **GND** | **GND** | Referência de sinal comum. |
| **CAN TX** | **GPIO 5** | **CTX (D)** | Saída de dados do controlador TWAI. |
| **CAN RX** | **GPIO 4** | **CRX (R)** | Entrada de dados para o controlador TWAI. |

---

## 2. Interface LoRa (ESP32 <-> Ebyte E32-433T20D)
Conexão para transmissão sem fio dos dados coletados para a base receptora.

| Função LoRa | Pino ESP32 (GPIO) | Destino (Módulo E32) | Observação |
| :--- | :--- | :--- | :--- |
| **VCC** | **5V / Vin** | **VCC** | Alimentação para rádio 433 MHz. |
| **GND** | **GND** | **GND** | Referência comum. |
| **TXD2** | **GPIO 17** | **RXD** | Comunicação Serial (UART). |
| **RXD2** | **GPIO 16** | **TXD** | Comunicação Serial (UART). |
| **Modo M0** | **GND** | **M0** | Fixado em GND para Modo Normal. |
| **Modo M1** | **GND** | **M1** | Fixado em GND para Modo Normal. |

---

## 3. Conexão ao Barramento CAN do Veículo
Fiação física entre o transceptor e o inversor Sevcon Gen4 Size 8.

*   **Sinal CANH:** Conectar ao barramento CAN High do carro.
*   **Sinal CANL:** Conectar ao barramento CAN Low do carro.
*   **Resistor de Terminação:** Instalar resistor de **120 Ohms** em paralelo entre CANH e CANL.
*   **Referência:** Unificar o GND da PCB com o referencial de sinal do inversor.

---

## 4. Checklist de Validação (Marcos S2)
* [ ] Hardware montado em bancada/protoboard.
* [ ] Driver TWAI inicializado com sucesso no firmware.
* [ ] Comunicação LoRa validada de forma independente.
* [ ] Recepção dos primeiros frames brutos confirmada via Serial.