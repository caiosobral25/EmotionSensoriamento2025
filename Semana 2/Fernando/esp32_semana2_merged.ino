#include "Arduino.h"
#include "LoRa_E32.h"
#include "driver/twai.h"
#include "esp_system.h"

// Definições de Hardware
#define CAN_TX_PIN 5
#define CAN_RX_PIN 6
#define LORA_RX 16
#define LORA_TX 17

// Pinos de controle do módulo E32 (ajustar se necessário)
#define LORA_AUX 4
#define LORA_M0 12
#define LORA_M1 13

// IDs Críticos do Sevcon Gen4
#define ID_RPM    0x181
#define ID_TORQUE 0x281
#define ID_SPEED  0x381

LoRa_E32 e32(&Serial2, LORA_AUX, LORA_M0, LORA_M1);

void setup() {
  // Debug serial
  Serial.begin(115200);
  delay(100);

  // Serial2 para LoRa E32
  Serial2.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  e32.begin();
  Serial.println("LoRa E32 inicializado (Serial2)");

  // Configuração do CAN (TWAI)
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver CAN instalado com sucesso!");
  } else {
    Serial.println("Falha ao instalar driver CAN.");
    return;
  }

  if (twai_start() == ESP_OK) {
    Serial.println("Barramento CAN iniciado!");
  } else {
    Serial.println("Falha ao iniciar barramento CAN.");
    return;
  }
}

void loop() {
  // Código original: recepção CAN e envio via LoRa.
  // Mantido comentado para referência até a conexão com o inversor estar disponível.
  /*
  twai_message_t message;

  if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
    // Monta payload legível
    String payload = "ID:0x" + String(message.identifier, HEX) + " DATA:";
    for (int i = 0; i < message.data_length_code; i++) {
      if (message.data[i] < 0x10) payload += "0"; // formatação hex de 2 dígitos
      payload += String(message.data[i], HEX);
      if (i < message.data_length_code - 1) payload += " ";
    }

    Serial.println(payload);

    // Envia via LoRa E32 usando a biblioteca (envio binário dos bytes da string)
    const char* buf = payload.c_str();
    size_t len = payload.length();
    ResponseStatus rs = e32.sendMessage((void*)buf, len);

    Serial.print("Status envio LoRa: ");
    Serial.println(rs.getResponseDescription());

    // Log adicional quando IDs críticos
    if (message.identifier == ID_RPM || message.identifier == ID_TORQUE || message.identifier == ID_SPEED) {
      Serial.println(">>> Mensagem crítica enviada via LoRa");
    }
  }
  */

  // Envio de dados aleatórios enquanto não há conexão com o inversor
  struct Telemetria {
    int rpm;
    int velocidade;
    float torque;
  } dados;

  // Semente pseudo-aleatória (melhora variabilidade após reset)
  randomSeed(esp_random());

  dados.rpm = random(1000, 5000);
  dados.velocidade = random(40, 120);
  dados.torque = (random(100, 350) / 10.0); // 10.0 - 35.0

  // Envia a struct binária para manter compatibilidade com o receptor Arduino UNO
  ResponseStatus rs = e32.sendMessage(&dados, sizeof(dados));

  Serial.print("Enviado Telemetria aleatoria - Status: ");
  Serial.println(rs.getResponseDescription());
  Serial.print("RPM:"); Serial.print(dados.rpm);
  Serial.print(" VEL:"); Serial.print(dados.velocidade);
  Serial.print(" TRQ:"); Serial.println(dados.torque);

  delay(2000); // intervalo entre envios
}
