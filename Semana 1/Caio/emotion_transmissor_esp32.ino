/*
 * ============================================================
 *  E-MOTION Formula SAE — Sistema de Telemetria
 *  TRANSMISSOR — ESP32 + LoRa Ebyte E32-433T20D/T30D
 * ============================================================
 *
 *  Sem biblioteca externa. Controle direto via UART + M0/M1/AUX.
 *
 *  Modo de transmissão: Fixed Transmission Broadcasting
 *  Cada pacote é prefixado com 0xFF 0xFF 0x17, fazendo com que
 *  QUALQUER módulo no canal 0x17 (433MHz) receba os dados,
 *  independente do endereço configurado no receptor.
 *
 *  Endereço do transmissor: 0x454D ("EM" de E-Motion)
 *  Canal: 0x17 → 410 + 23 = 433 MHz
 *
 *  Pinagem:
 *  ┌─────────────────┬───────────┐
 *  │ Função          │ GPIO ESP  │
 *  ├─────────────────┼───────────┤
 *  │ E32 RXD         │ GPIO 17   │ ← Serial2 TX do ESP32
 *  │ E32 TXD         │ GPIO 16   │ ← Serial2 RX do ESP32
 *  │ E32 M0          │ GPIO 32   │
 *  │ E32 M1          │ GPIO 33   │
 *  │ E32 AUX         │ GPIO 34   │ ← INPUT apenas (obrigatório)
 *  │ E32 VCC         │ 3,3V      │
 *  │ E32 GND         │ GND       │
 *  └─────────────────┴───────────┘
 *
 *  ATENÇÃO: ESP32 opera em 3,3V — compatível direto com o E32.
 *  Não é necessário divisor de tensão em nenhum pino.
 * ============================================================
 */

#include <Arduino.h>

// ─── Pinagem ─────────────────────────────────────────────────
#define LORA_RX_PIN  16    // Serial2 RX ← E32 TXD
#define LORA_TX_PIN  17    // Serial2 TX → E32 RXD
#define M0_PIN       32    // Controle de modo
#define M1_PIN       33    // Controle de modo
#define AUX_PIN      34    // Estado do módulo (INPUT apenas)

// ─── Parâmetros da equipe E-Motion ───────────────────────────
// Endereço: 0x45='E', 0x4D='M' → "EM" de E-Motion
#define TEAM_ADDH    0x45
#define TEAM_ADDL    0x4D

// Canal 0x17 = 23 decimal → 410 + 23 = 433 MHz (padrão do módulo)
#define TEAM_CHAN     0x17

// ─── Bytes de configuração do E32 ────────────────────────────
// SPED (byte 3 do comando):
//   bits 7-6: 00  → paridade 8N1
//   bits 5-3: 011 → baud UART 9600
//   bits 2-0: 010 → air data rate 2.4kbps
//   = 0b00011010 = 0x1A
#define SPED_BYTE     0x1A

// OPTION (byte 5 do comando):
//   bit  7:   1   → Fixed Transmission (necessário para broadcasting)
//   bit  6:   1   → saídas push-pull, entrada pull-up
//   bits 5-3: 000 → wake-up 250ms
//   bit  2:   1   → FEC ligado
//   bits 1-0: 00  → potência 30dBm (1W)
//   = 0b11000100 = 0xC4
#define OPTION_BYTE   0xC4

// ─── Configurações gerais ─────────────────────────────────────
#define LORA_BAUD     9600
#define SERIAL_BAUD   115200
#define TX_INTERVAL   1000     // ms entre pacotes

// ─── Variáveis de telemetria ──────────────────────────────────
// Substituir pelos dados reais do CAN Bus ao integrar (Fernando)
int32_t g_rpm = 3500;
int32_t g_vel = 72;
int32_t g_tq  = 120;

unsigned long g_ultimoEnvio = 0;

// ─────────────────────────────────────────────────────────────
//  FUNÇÕES DE CONTROLE DO MÓDULO
// ─────────────────────────────────────────────────────────────

/**
 * @brief Aguarda o pino AUX subir para HIGH (módulo pronto).
 *        Conforme datasheet seção 5.5 e 5.6.4:
 *        AUX=LOW  → módulo ocupado (inicializando, TX ou RX)
 *        AUX=HIGH → módulo livre e pronto
 *
 *        Após AUX subir, espera 2ms adicionais (requisito datasheet).
 *
 * @param timeout_ms Tempo máximo de espera em ms (padrão 3000)
 * @return true se AUX subiu dentro do timeout, false se timeout
 */
bool aguardarAUX(uint32_t timeout_ms = 3000) {
  unsigned long inicio = millis();
  while (digitalRead(AUX_PIN) == LOW) {
    if (millis() - inicio > timeout_ms) {
      Serial.println("[AUX] TIMEOUT: AUX nao subiu para HIGH!");
      Serial.println("      Verifique: VCC=3,3V, GND, e conexao do pino AUX (GPIO34).");
      return false;
    }
    delay(1);
  }
  delay(2); // datasheet 5.6.4 nota 3: esperar 2ms apos AUX=HIGH
  return true;
}

/**
 * @brief Alterna o modo de operação do módulo E32.
 *        Modos (tabela seção 6):
 *          M0=0 M1=0 → Modo Normal   (TX/RX transparente)
 *          M0=1 M1=0 → Modo Wake-up
 *          M0=0 M1=1 → Modo Poupança
 *          M0=1 M1=1 → Modo Sleep    (configuração)
 */
void setModo(uint8_t m0, uint8_t m1) {
  digitalWrite(M0_PIN, m0);
  digitalWrite(M1_PIN, m1);
  delay(10);      // estabilização dos pinos
  aguardarAUX();  // aguarda módulo aceitar o novo modo
}

/**
 * @brief Configura o módulo E32 com os parâmetros da equipe.
 *
 *        Sequência (datasheet seção 7.5):
 *        1. Entra em modo Sleep (M0=1, M1=1)
 *        2. Envia comando C0 + 5 bytes de parâmetro
 *        3. Lê 6 bytes de confirmação do módulo
 *        4. Volta ao modo Normal (M0=0, M1=0)
 *
 *        Parâmetros gravados (persistem após desligar):
 *          Endereço: 0x454D ("EM")
 *          Canal:    0x17 (433 MHz)
 *          SPED:     0x1A (8N1, 9600 baud, 2.4kbps air)
 *          OPTION:   0xC4 (Fixed TX, 30dBm, FEC on)
 */
void configurarModulo() {
  Serial.println("[CFG] Entrando em modo Sleep para configuracao...");
  setModo(HIGH, HIGH); // Modo Sleep
  delay(100);

  // Limpa qualquer dado residual no buffer
  while (Serial2.available()) Serial2.read();

  // Monta e envia o comando de configuração (seção 7.5)
  // Formato: HEAD ADDH ADDL SPED CHAN OPTION
  uint8_t cmd[6] = {
    0xC0,        // HEAD: salvar na memória não-volátil (persiste ao desligar)
    TEAM_ADDH,   // ADDH: byte alto do endereço = 0x45 ('E')
    TEAM_ADDL,   // ADDL: byte baixo do endereço = 0x4D ('M')
    SPED_BYTE,   // SPED: 8N1 | 9600 baud | 2.4kbps air = 0x1A
    TEAM_CHAN,   // CHAN: canal 0x17 = 433MHz
    OPTION_BYTE  // OPTION: Fixed TX | push-pull | FEC | 30dBm = 0xC4
  };

  Serial2.write(cmd, 6);
  Serial2.flush();

  // Aguarda o módulo processar e responder
  delay(300);
  aguardarAUX();

  // Lê e valida a resposta (6 bytes espelhando o comando enviado)
  uint8_t resp[6] = {0};
  uint8_t n = 0;
  unsigned long t = millis();
  while (n < 6 && millis() - t < 500) {
    if (Serial2.available()) resp[n++] = Serial2.read();
    delay(1);
  }

  Serial.print("[CFG] Resposta do modulo: ");
  for (int i = 0; i < n; i++) Serial.printf("%02X ", resp[i]);
  Serial.println();

  if (n == 6 && resp[1] == TEAM_ADDH && resp[2] == TEAM_ADDL && resp[4] == TEAM_CHAN) {
    Serial.println("[CFG] OK — modulo configurado com sucesso.");
  } else {
    Serial.println("[CFG] AVISO — resposta inesperada. Verifique fiacao.");
  }

  // Volta ao modo Normal
  Serial.println("[CFG] Voltando ao modo Normal (M0=0, M1=0)...");
  setModo(LOW, LOW);
  delay(100);
  aguardarAUX();
  Serial.println("[CFG] Modo Normal ativo. Pronto para transmitir.");
}

// ─────────────────────────────────────────────────────────────
//  FUNÇÃO DE ENVIO
// ─────────────────────────────────────────────────────────────

/**
 * @brief Monta e transmite um pacote LoRa via Fixed Transmission
 *        Broadcasting conforme datasheet seção 5.2.
 *
 *        Em Fixed Transmission mode, os 3 primeiros bytes enviados
 *        ao módulo são o endereço de destino + canal:
 *          0xFF 0xFF → endereço broadcast (todos no canal recebem)
 *          0x17      → canal 433MHz
 *
 *        Os bytes seguintes são o payload, que chega ao receptor
 *        sem os 3 bytes de cabeçalho (consumidos internamente).
 *
 *        Payload máximo: 55 bytes (58 - 3 do cabeçalho).
 */
void enviarLoRa() {
  // Verifica se o módulo está pronto (AUX=HIGH)
  if (!aguardarAUX(2000)) {
    Serial.println("[TX] Modulo ocupado — pacote descartado.");
    return;
  }

  // Monta o payload (formato Cap. 6.2 do documento de planejamento)
  char payload[52];
  snprintf(payload, sizeof(payload),
           "RPM:%ld;VEL:%ld;TQ:%ld",
           (long)g_rpm, (long)g_vel, (long)g_tq);

  // Envia: cabeçalho de broadcast + payload (tudo sem pausa)
  Serial2.write((uint8_t)0xFF);    // ADDH destino: broadcast
  Serial2.write((uint8_t)0xFF);    // ADDL destino: broadcast
  Serial2.write((uint8_t)TEAM_CHAN); // Canal destino
  Serial2.print(payload);           // Dados de telemetria
  Serial2.write('\n');              // Terminador de linha para o receptor
  Serial2.flush();

  Serial.print("[TX] >> ");
  Serial.println(payload);

  // Aguarda o módulo terminar a transmissão RF antes do próximo envio
  aguardarAUX(3000);
}

// ─────────────────────────────────────────────────────────────
//  SETUP & LOOP
// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println("============================================");
  Serial.println("  E-Motion Telemetria — Transmissor ESP32  ");
  Serial.println("============================================");
  Serial.printf("  Endereco: 0x%02X%02X  (\"EM\" de E-Motion)\n", TEAM_ADDH, TEAM_ADDL);
  Serial.printf("  Canal:    0x%02X     (433 MHz)\n", TEAM_CHAN);
  Serial.printf("  Modo:     Broadcasting (0xFFFF no destino)\n");
  Serial.println("============================================");

  // Configura pinos de controle
  pinMode(M0_PIN,  OUTPUT);
  pinMode(M1_PIN,  OUTPUT);
  pinMode(AUX_PIN, INPUT);  // GPIO 34 = input-only no ESP32

  // Inicia com o módulo em modo Normal
  digitalWrite(M0_PIN, LOW);
  digitalWrite(M1_PIN, LOW);

  // Inicializa Serial2 com o E32
  Serial2.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(200);

  // Aguarda a inicialização do módulo (AUX sobe após self-check)
  Serial.print("[AUX] Aguardando inicializacao do modulo");
  for (int i = 0; i < 10; i++) {
    if (digitalRead(AUX_PIN) == HIGH) break;
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (!aguardarAUX(5000)) {
    Serial.println("[ERRO] Modulo nao respondeu. Verifique a fiacao e reinicie.");
    while (true) delay(1000);
  }
  Serial.println("[AUX] Modulo pronto.");

  // Configura os parâmetros do módulo
  configurarModulo();

  Serial.println("[SYS] Iniciando broadcasting a cada 1 segundo...");
}

void loop() {
  // ── Substitua aqui pelos dados reais do CAN Bus (Fernando) ──
  g_rpm = 3500;
  g_vel = 72;
  g_tq  = 120;
  // ────────────────────────────────────────────────────────────

  unsigned long agora = millis();
  if (agora - g_ultimoEnvio >= TX_INTERVAL) {
    g_ultimoEnvio = agora;
    enviarLoRa();
  }
}
