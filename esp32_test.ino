#include "driver/twai.h"

// Definição de Pinos 
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4

// Variáveis Globais para Telemetria [cite: 43, 44, 45]
int32_t rpm = 0;
int16_t torque = 0;
uint16_t velocidade = 0;

void setup() {
  Serial.begin(115200);      // Debug Serial
  Serial2.begin(9600);       // Comunicação com LoRa E32 [cite: 13, 47]

  // 1. Inicializar CAN (TWAI) [cite: 53]
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); // Sevcon padrão 500kbps
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // No início, aceita tudo para leitura bruta [cite: 31]

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    twai_start();
    Serial.println("CAN Inicializado com sucesso!");
  }
}

void loop() {
  twai_message_t message;
  
  // 2. Ler frame CAN [cite: 56]
  if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    
    // 3. Verificar ID relevante e Decodificar [cite: 57, 58, 70]
    switch (message.identifier) {
      case 0x181: // RPM (32 bits - Little Endian) 
        rpm = (int32_t)(message.data[0] | (message.data[1] << 8) | (message.data[2] << 16) | (message.data[3] << 24));
        break;

      case 0x281: // Torque (16 bits) 
        torque = (int16_t)(message.data[0] | (message.data[1] << 8));
        break;

      case 0x381: // Velocidade (16 bits) 
        velocidade = (uint16_t)(message.data[0] | (message.data[1] << 8));
        break;
    }
  }

  // 4. Montar Payload e Enviar via LoRa (Intervalo de 100ms para não saturar) [cite: 59, 60]
  static uint32_t lastTx = 0;
  if (millis() - lastTx > 100) {
    // Formato de payload definido para debug [cite: 62, 63]
    String payload = "RPM: " + String(rpm) + "; VEL: " + String(velocidade) + "; TQ: " + String(torque);
    
    Serial2.println(payload); // Transmissão via LoRa [cite: 47, 60]
    Serial.println("Enviado LoRa: " + payload);
    lastTx = millis();
  }
}
