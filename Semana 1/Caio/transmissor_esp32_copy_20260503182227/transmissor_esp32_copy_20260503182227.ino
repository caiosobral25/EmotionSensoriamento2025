#include "Arduino.h"
#include "LoRa_E32.h"

// Usando a Serial2 do ESP32 (Hardware Serial)
// Pinos: AUX=4, M0=12, M1=13. RX2=16, TX2=17 (conectar ao TX/RX do LoRa)
LoRa_E32 e32(&Serial2, 4, 12, 13); 

struct Telemetria {
  int rpm;
  int velocidade;
  float torque;
} dados;

void setup() {
  Serial.begin(115200);
  
  // Configuração da Serial de Hardware do ESP32 para o LoRa
  Serial2.begin(9600, SERIAL_8N1, 16, 17); 
  e32.begin();

  Serial.println("Transmissor Telemetria (ESP32) Iniciado...");
}

void loop() {
  // Simulando a leitura de sensores
  dados.rpm = random(1000, 5000);
  dados.velocidade = random(40, 120);
  dados.torque = random(10, 35) / 1.5;

  // A biblioteca gerencia o pino AUX internamente antes de enviar
  ResponseStatus rs = e32.sendMessage(&dados, sizeof(Telemetria));
  
  Serial.print("Telemetria Enviada - Status: ");
  Serial.println(rs.getResponseDescription());
  Serial.print("RPM:"); Serial.print(dados.rpm); Serial.print(",VEL:"); Serial.print(",TRQ:"); Serial.print(dados.torque);

  
  delay(2000); // Intervalo de 2 segundos entre envios
}