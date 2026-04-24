#include <SPI.h>
#include <mcp_can.h>

const int SPI_CS_PIN = 9; // Altere para 9 se o seu CS estiver no pino 9
MCP_CAN CAN(SPI_CS_PIN);

unsigned char contador = 0;

void setup() {
  Serial.begin(115200);

  // Ajustado para MCP_8MHZ
  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    Serial.println("Erro ao inicializar MCP2515... tentando novamente");
    delay(100);
  }
  
  CAN.setMode(MCP_NORMAL);
  Serial.println("Transmissor pronto!");
}

void loop() {
  unsigned char dados[1] = {contador};

  // Envia ID 0x100
  byte status = CAN.sendMsgBuf(0x100, 0, 1, dados);

  if (status == CAN_OK) {
    Serial.print("Enviado: ");
    Serial.println(contador);
  } else {
    Serial.println("Erro no envio.");
  }

  contador++;
  delay(1000);
}
