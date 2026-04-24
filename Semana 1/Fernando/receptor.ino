#include <SPI.h>
#include <mcp_can.h>

const int SPI_CS_PIN = 9; // Altere para 9 se o seu CS estiver no pino 9
MCP_CAN CAN(SPI_CS_PIN);

void setup() {
  Serial.begin(115200);

  // Ajustado para MCP_8MHZ
  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    Serial.println("Erro ao inicializar MCP2515... tentando novamente");
    delay(100);
  }

  CAN.setMode(MCP_NORMAL);
  Serial.println("Receptor pronto! Aguardando...");
}

void loop() {
  long unsigned int rxId;
  unsigned char len = 0;
  unsigned char buf[8];

  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&rxId, &len, buf);

    Serial.print("Recebido do ID 0x");
    Serial.print(rxId, HEX);
    Serial.print(": ");
    Serial.println(buf[0]); // Mostra o valor do contador
  }
}
