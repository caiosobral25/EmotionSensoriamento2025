/*
 * ============================================================
 *  E-MOTION Formula SAE — Sistema de Telemetria
 *  RECEPTOR — Arduino Uno + LoRa Ebyte E32-433T20D/T30D
 * ============================================================
 *
 *  Sem biblioteca externa. Controle direto via SoftwareSerial
 *  + M0/M1/AUX conforme datasheet do E32.
 *
 *  Modo de recepção: Fixed Transmission, endereço 0xFFFF
 *  Qualquer Arduino com esta configuração e no canal 0x17
 *  recebe automaticamente o broadcasting do carro.
 *
 *  Pinagem e nível de tensão:
 *  ┌──────────────┬────────────┬─────────────────────────────┐
 *  │ Função       │ Pino Uno   │ Observação                  │
 *  ├──────────────┼────────────┼─────────────────────────────┤
 *  │ E32 RXD      │ D11 (SWTX) │ ⚠ Divisor 5V→3,3V (veja ①) │
 *  │ E32 TXD      │ D10 (SWRX) │ Direto (3,3V → Arduino OK) │
 *  │ E32 M0       │ D6         │ ⚠ Divisor 5V→3,3V (veja ①) │
 *  │ E32 M1       │ D7         │ ⚠ Divisor 5V→3,3V (veja ①) │
 *  │ E32 AUX      │ D5         │ Direto (3,3V → Arduino OK) │
 *  │ E32 VCC      │ 3,3V Uno   │ NUNCA ligar ao 5V          │
 *  │ E32 GND      │ GND        │ GND compartilhado           │
 *  └──────────────┴────────────┴─────────────────────────────┘
 *
 *  ① Divisor de tensão 5V → 3,3V para cada pino marcado:
 *
 *    Arduino pino ──[ 10kΩ ]──┬── E32 pino
 *                             │
 *                          [ 20kΩ ]
 *                             │
 *                            GND
 *
 *    Tensão no E32 = 5V × 20k / (10k + 20k) = 3,33V ✓
 *
 *  Saída para o PC: Serial USB 9600 baud.
 *  Formato impresso: RPM:3500;VEL:72;TQ:120
 * ============================================================
 */

#include <SoftwareSerial.h>

// ─── Pinagem ─────────────────────────────────────────────────
#define LORA_SW_RX  10   // ← E32 TXD (sem divisor)
#define LORA_SW_TX  11   // → E32 RXD (com divisor 5V→3,3V)
#define M0_PIN       6   // → E32 M0  (com divisor 5V→3,3V)
#define M1_PIN       7   // → E32 M1  (com divisor 5V→3,3V)
#define AUX_PIN      5   // ← E32 AUX (sem divisor)

// ─── Parâmetros do receptor ───────────────────────────────────
// Endereço 0xFFFF = receptor universal (recebe qualquer
// broadcast no canal, independente do endereço do transmissor)
#define RX_ADDH      0xFF
#define RX_ADDL      0xFF

// Canal DEVE ser idêntico ao do transmissor
#define TEAM_CHAN     0x17  // 410 + 23 = 433 MHz

// ─── Bytes de configuração ────────────────────────────────────
// SPED: 8N1 | 9600 baud UART | 2.4kbps air = 0x1A
// DEVE ser igual ao do transmissor (especialmente o air data rate)
#define SPED_BYTE     0x1A

// OPTION: Fixed TX | push-pull | 250ms | FEC on | 30dBm = 0xC4
// DEVE ser igual ao do transmissor
#define OPTION_BYTE   0xC4

// ─── SoftwareSerial ───────────────────────────────────────────
SoftwareSerial loraSerial(LORA_SW_RX, LORA_SW_TX);

// ─── Buffer de recepção ───────────────────────────────────────
char    bufRx[64];
uint8_t idxRx = 0;

// ─────────────────────────────────────────────────────────────
//  FUNÇÕES DE CONTROLE DO MÓDULO
// ─────────────────────────────────────────────────────────────

/**
 * @brief Aguarda AUX=HIGH (módulo pronto) com timeout.
 *        Após AUX subir, espera 2ms conforme datasheet 5.6.4.
 */
bool aguardarAUX(uint16_t timeout_ms = 3000) {
  unsigned long inicio = millis();
  while (digitalRead(AUX_PIN) == LOW) {
    if ((unsigned long)(millis() - inicio) > timeout_ms) {
      Serial.println("[AUX] TIMEOUT: AUX nao subiu!");
      Serial.println("      Verifique: VCC=3,3V, GND, pino AUX no D5.");
      return false;
    }
    delay(1);
  }
  delay(2); // requisito datasheet: 2ms após AUX=HIGH
  return true;
}

/**
 * @brief Alterna o modo do módulo e aguarda estabilização.
 */
void setModo(uint8_t m0, uint8_t m1) {
  digitalWrite(M0_PIN, m0);
  digitalWrite(M1_PIN, m1);
  delay(10);
  aguardarAUX();
}

/**
 * @brief Configura o módulo E32 com os parâmetros do receptor.
 *
 *        Endereço 0xFFFF → recebe broadcasting de qualquer
 *        transmissor no canal 0x17, conforme datasheet seção 5.4
 *        ("Monitor address").
 *
 *        IMPORTANTE: air data rate (bits 2-0 do SPED) e canal
 *        DEVEM ser idênticos aos do transmissor. Se forem
 *        diferentes, nenhum pacote será recebido.
 */
void configurarModulo() {
  Serial.println("[CFG] Entrando em modo Sleep para configuracao...");
  setModo(HIGH, HIGH); // Modo Sleep (M0=1, M1=1)
  delay(100);

  // Limpa buffer residual
  while (loraSerial.available()) loraSerial.read();

  // Comando de configuração (datasheet seção 7.5)
  uint8_t cmd[6] = {
    0xC0,        // HEAD: salva na memória não-volátil
    RX_ADDH,     // ADDH: 0xFF — endereço broadcast/monitor
    RX_ADDL,     // ADDL: 0xFF
    SPED_BYTE,   // SPED: 0x1A (8N1, 9600 baud, 2.4kbps) — IGUAL ao TX
    TEAM_CHAN,   // CHAN: 0x17 (433MHz) — IGUAL ao TX
    OPTION_BYTE  // OPTION: 0xC4 (Fixed TX, FEC, 30dBm) — IGUAL ao TX
  };

  // Envia cada byte individualmente (SoftwareSerial mais estável assim)
  for (int i = 0; i < 6; i++) {
    loraSerial.write(cmd[i]);
    delay(5);
  }

  // Aguarda o módulo processar
  delay(300);
  aguardarAUX();

  // Descarta a resposta de confirmação (6 bytes)
  delay(100);
  while (loraSerial.available()) loraSerial.read();

  // Volta ao modo Normal
  Serial.println("[CFG] Voltando ao modo Normal...");
  setModo(LOW, LOW);
  delay(100);
  aguardarAUX();

  Serial.println("[CFG] OK — Receptor configurado:");
  Serial.println("[CFG]   Endereco: 0xFFFF (recebe todos os broadcasts)");
  Serial.printf ("[CFG]   Canal:    0x%02X (433MHz)\n", TEAM_CHAN);
  Serial.println("[CFG]   Air rate: 2.4kbps (igual ao transmissor)");
}

// ─────────────────────────────────────────────────────────────
//  SETUP & LOOP
// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("==========================================");
  Serial.println("  E-Motion Telemetria — Receptor Arduino  ");
  Serial.println("==========================================");
  Serial.println("  Recebe broadcasting do carro (0x454D)  ");
  Serial.println("  Canal: 0x17 (433MHz)                   ");
  Serial.println("==========================================");

  // Configura pinos
  pinMode(M0_PIN,  OUTPUT);
  pinMode(M1_PIN,  OUTPUT);
  pinMode(AUX_PIN, INPUT);

  // Inicia em modo Normal
  digitalWrite(M0_PIN, LOW);
  digitalWrite(M1_PIN, LOW);

  loraSerial.begin(9600);
  delay(200);

  // Aguarda a inicialização do módulo
  Serial.print("[AUX] Aguardando inicializacao do modulo");
  for (int i = 0; i < 15; i++) {
    if (digitalRead(AUX_PIN) == HIGH) break;
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (!aguardarAUX(5000)) {
    Serial.println("[ERRO] Modulo nao respondeu. Verifique a fiacao.");
    Serial.println("       Lembre: VCC=3,3V, divisores de tensao nos");
    Serial.println("       pinos D11 (SW TX), D6 (M0) e D7 (M1).");
    while (true) delay(1000);
  }
  Serial.println("[AUX] Modulo pronto.");

  // Configura os parâmetros do módulo
  configurarModulo();

  Serial.println("[SYS] Aguardando pacotes do carro...");
  Serial.println("--------------------------------------------");
}

void loop() {
  // ── Heartbeat: confirma que o Arduino está vivo ──────────
  static unsigned long ultimoHB = 0;
  if (millis() - ultimoHB >= 5000) {
    ultimoHB = millis();
    Serial.print("[HB] Aguardando... AUX=");
    Serial.print(digitalRead(AUX_PIN) ? "HIGH" : "LOW");
    Serial.print("  buf=");
    Serial.println(loraSerial.available());
  }

  // ── Recepção byte a byte ──────────────────────────────────
  // O E32 em Fixed TX mode entrega APENAS o payload ao Arduino
  // (o cabeçalho 0xFF 0xFF 0x17 é consumido internamente pelo módulo).
  // O '\n' enviado pelo ESP32 delimita o fim do pacote.
  while (loraSerial.available() > 0) {
    char c = (char)loraSerial.read();

    if (c == '\n') {
      // Fim do pacote — imprime o payload completo
      if (idxRx > 0) {
        bufRx[idxRx] = '\0';
        // Imprime para o PC via Serial USB (software de telemetria lê aqui)
        Serial.println(bufRx);
        idxRx = 0;
      }
    } else if (c != '\r') {
      // Acumula byte no buffer (ignora \r)
      if (idxRx < sizeof(bufRx) - 1) {
        bufRx[idxRx++] = c;
      }
    }
  }
}
