#include "driver/twai.h"

// Definições de Hardware conforme S1 e S2
#define CAN_TX_PIN 5
#define CAN_RX_PIN 6 // Pino alterado conforme solicitação
#define LORA_RX 16
#define LORA_TX 17

// IDs Críticos do Sevcon Gen4
#define ID_RPM    0x181
#define ID_TORQUE 0x281
#define ID_SPEED  0x381

void setup() {
  // Inicializa Serial para Debug no PC
  Serial.begin(115200);
  
  // Inicializa Serial2 para o módulo LoRa E32
  // Configuração padrão de baudrate do E32 é 9600
  Serial2.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);

  // 1. Configuração do Driver TWAI (CAN)[cite: 1]
  // Importante: O MCP2551 envia 5V, use divisor de tensão no GPIO 6[cite: 1]
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); // Padrão Sevcon[cite: 1]
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // Leitura bruta para S3[cite: 1]

  // 2. Instalação e Inicialização[cite: 1]
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver CAN instalado com sucesso!");
  } else {
    Serial.println("Falha ao instalar driver! Verifique o pino GPIO 6.");
    return;
  }

  if (twai_start() == ESP_OK) {
    Serial.println("Barramento CAN iniciado!");
  } else {
    Serial.println("Falha ao iniciar barramento!");
    return;
  }
}

void loop() {
  twai_message_t message;

  // 3. Recebe frames do barramento (Leitura Bruta S3)[cite: 1]
  if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
    
    // Formata a string de saída para o LoRa[cite: 1]
    String payload = "ID:0x" + String(message.identifier, HEX) + " DATA:";
    for (int i = 0; i < message.data_length_code; i++) {
      payload += String(message.data[i], HEX) + " ";
    }

    // Exibe no Serial Monitor para debug[cite: 1]
    Serial.println(payload);

    // 4. Filtragem e Envio via LoRa[cite: 1]
    // Envia apenas os dados críticos mapeados na S1[cite: 1]
    if (message.identifier == ID_RPM || message.identifier == ID_TORQUE || message.identifier == ID_SPEED) {
      Serial2.println(payload); 
      Serial.println(">>> Enviado via LoRa");
    }
  }
}
