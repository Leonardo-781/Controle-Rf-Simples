#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// ================= RF24 =================
#define CE_PIN   6
#define CSN_PIN  10

RF24 radio(CE_PIN, CSN_PIN);
const byte endereco[6] = "CTRL1";

// ================= PINOS =================
#define JOY1_X 0
#define JOY1_Y 1
#define JOY2_X 2
#define JOY2_Y 3
#define CHAVE_PIN 5

// ================= STRUCT =================
struct DadosControle {
  int joy1X;
  int joy1Y;
  int joy2X;
  int joy2Y;
  bool chave;
};

DadosControle dados;

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  // Aguarda porta serial abrir, mas não trava se não houver porta nativa
  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < 1500) {}
  delay(100);
  Serial.println("Boot...");

  pinMode(CHAVE_PIN, INPUT_PULLUP);

  // SPI personalizado (ESP32-C3)
  // Informe também o pino SS (CSN) para evitar conflitos no ESP32-C3
  SPI.begin(9, 8, 7, CSN_PIN); // SCK, MISO, MOSI, SS

#ifdef ESP32
  analogReadResolution(12); // Leitura de 0-4095 para mapeamento consistente
#endif

  // Inicializa RF24 com diagnósticos
  bool rfOk = radio.begin();
  if (!rfOk) {
    Serial.println("RF24 não detectado (begin falhou)");
  }

  // Checagem adicional do chip
  if (!radio.isChipConnected()) {
    Serial.println("RF24 não detectado (chip não responde)");
    Serial.print("Pinos -> CE:"); Serial.print(CE_PIN);
    Serial.print(" CSN:"); Serial.print(CSN_PIN);
    Serial.print(" MOSI:"); Serial.print(7);
    Serial.print(" MISO:"); Serial.print(8);
    Serial.print(" SCK:"); Serial.println(9);
    Serial.println("Verifique alimentação 3V3, GND comum e capacitor de desacoplamento no módulo (10-100uF).");
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(108);
  radio.setRetries(5, 15); // tentativas e atraso de reenvio
  // Desliga auto-ack para permitir teste de transmissão sem receptor
  radio.setAutoAck(false);
  radio.openWritingPipe(endereco);
  radio.stopListening();

  Serial.println("Transmissor pronto");
  // Opcional: imprime detalhes do rádio (útil para depuração)
  radio.printDetails();

  // Self-test simples sem receptor: entra em RX brevemente e lê RPD
  radio.startListening();
  delay(2);
  bool rpd = radio.testRPD();
  radio.stopListening();
  Serial.print("Self-test RPD (energia no canal): ");
  Serial.println(rpd ? "SIM" : "NAO");
}

// ================= LOOP =================
void loop() {
  // Joysticks
  dados.joy1X = analogRead(JOY1_X);
  dados.joy1Y = analogRead(JOY1_Y);
  dados.joy2X = analogRead(JOY2_X);
  dados.joy2Y = analogRead(JOY2_Y);

  // Chave seletora
  dados.chave = !digitalRead(CHAVE_PIN);

  // Limpa IRQs pendentes antes do envio
  {
    bool _tx=false, _fail=false, _rx=false;
    radio.whatHappened(_tx, _fail, _rx);
    if (_rx) radio.flush_rx();
  }

  // Envio RF
  (void)radio.write(&dados, sizeof(dados));
  bool tx_ok=false, tx_fail=false, rx_ready=false;
  radio.whatHappened(tx_ok, tx_fail, rx_ready);

  // Debug
  Serial.print("J1: ");
  Serial.print(dados.joy1X); Serial.print(",");
  Serial.print(dados.joy1Y);
  Serial.print(" | J2: ");
  Serial.print(dados.joy2X); Serial.print(",");
  Serial.print(dados.joy2Y);
  Serial.print(" | Chave: ");
  Serial.print(dados.chave);
  // Status simplificado
  Serial.print(" | TX="); Serial.print(tx_ok ? "OK" : "NOT");
  Serial.print(" | RX="); Serial.print(rx_ready ? "OK" : "NOT");
  Serial.print(" | ENVIO="); Serial.println((tx_ok && !tx_fail) ? "OK" : "NOT");
  // Detalhes (opcional)
  Serial.print("  IRQ tx_ok:"); Serial.print(tx_ok);
  Serial.print(" tx_fail:"); Serial.print(tx_fail);
  Serial.print(" rx_ready:"); Serial.println(rx_ready);

  delay(20);
}
