#include "driver/twai.h"

// Definições de Hardware conforme S1
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
#define LORA_RX 16
#define LORA_TX 17

void setup() {
  // Inicializa Serial para Debug no PC
  Serial.begin(115200);
  
  // Inicializa Serial2 para o módulo LoRa E32
  Serial2.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);

  // 1. Configuração do Driver TWAI (CAN)
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); // Velocidade padrão Sevcon
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // Aceita todos os IDs para leitura bruta

  // 2. Instalação e Inicialização
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver CAN instalado!");
  } else {
    Serial.println("Falha ao instalar driver!");
    return;
  }

  if (twai_start() == ESP_OK) {
    Serial.println("Barramento CAN iniciado com sucesso!");
  } else {
    Serial.println("Falha ao iniciar barramento!");
    return;
  }
}

void loop() {
  twai_message_t message;

  // 3. Recebe frames do barramento (Leitura Bruta)
  if (twai_receive(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    
    // Formata a mensagem para o Monitor Serial e para o LoRa
    String output = "ID: 0x" + String(message.identifier, HEX) + " | Data: ";
    for (int i = 0; i < message.data_length_code; i++) {
      output += String(message.data[i], HEX) + " ";
    }

    // Envia para o PC (Debug)
    Serial.println(output);

    // 4. Envia via LoRa para a unidade receptora
    // Filtragem básica para os IDs críticos definidos na S1
    if (message.identifier == 0x181 || message.identifier == 0x281 || message.identifier == 0x381) {
      Serial2.println(output); 
    }
  } else {
    Serial.println("Aguardando frames do Sevcon...");
  }
}
