unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  if (millis() - ultimoEnvio >= 100) {
    ultimoEnvio = millis();

    float tempo = millis() / 1000.0;
    float rpm = 3200.0 + 450.0 * sin(tempo * 0.9);
    float vel = 68.0 + 12.0 * sin(tempo * 0.7);
    float tq  = 110.0 + 18.0 * sin(tempo * 1.2);

    Serial.print("RPM=");
    Serial.print(rpm, 1);
    Serial.print(";VEL=");
    Serial.print(vel, 1);
    Serial.print(";TQ=");
    Serial.println(tq, 1);
  }
}
