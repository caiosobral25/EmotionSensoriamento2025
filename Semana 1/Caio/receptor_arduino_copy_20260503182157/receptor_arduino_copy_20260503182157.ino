#include "Arduino.h"
#include "LoRa_E32.h"
#include <SoftwareSerial.h>

// Pinos: RX=2, TX=3. AUX=9, M0=7, M1=8
SoftwareSerial mySerial(2, 3); 
LoRa_E32 e32(&mySerial, 9, 7, 8); 

struct Telemetria {
  int rpm;
  int velocidade;
  float torque;
};

void setup() {
  Serial.begin(9600);
  e32.begin(); // Inicializa o módulo no Modo Normal (M0=0, M1=0)[cite: 1]
  
  Serial.println("Receptor Telemetria (Arduino UNO) Pronto...");
}

void loop() {
  // Verifica se há dados recebidos pelo módulo LoRa[cite: 1]
  if (e32.available()) {
    // Recebe a mensagem e faz o cast para a struct Telemetria
    ResponseStructContainer rsc = e32.receiveMessage(sizeof(Telemetria));
    struct Telemetria dados = *(Telemetria*) rsc.data;
    
    Serial.println("--- Dados Recebidos do ESP32 ---");
    Serial.print("RPM: "); Serial.println(dados.rpm);
    Serial.print("Velocidade: "); Serial.println(dados.velocidade);
    Serial.print("Torque: "); Serial.print(dados.torque); Serial.println(" Nm");
    
    rsc.close(); // Limpa o container da memória
  }
}