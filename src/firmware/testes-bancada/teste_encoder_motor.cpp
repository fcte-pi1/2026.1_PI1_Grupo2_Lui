// ==========================================
// TESTE ISOLADO: ENCODERS (CALIBRACAO MANUAL)
// Motor DESLIGADO - empurre o carrinho a mao por uma distancia conhecida
// (ex.: 1 metro) e leia a contagem de ticks de cada roda.
// Com Bluetooth Serial (comandos via USB ou celular)
// ==========================================
#include <Arduino.h>
#include "BluetoothSerial.h"

// Pinos dos Encoders (ver src/firmware/pins.h)
#define ENC_ESQ_A 32
#define ENC_ESQ_B 33
#define ENC_DIR_A 18
#define ENC_DIR_B 19

// Pinos da Ponte H DRV8833 (ver src/firmware/pins.h) - mantidos em LOW
// neste teste para garantir que o motor fique parado (calibracao e manual).
#define MOT1_IN1 14
#define MOT1_IN2 27
#define MOT2_IN1 26
#define MOT2_IN2 25

#define RODA_DIAMETRO_MM 32.0f

BluetoothSerial SerialBT;

volatile long contEsq = 0;
volatile long contDir = 0;
volatile int sentidoEsq = 0;
volatile int sentidoDir = 0;

String comandoSerial = "";
String comandoBT = "";
bool leituraContinua = false;

unsigned long tempoAnterior = 0;
const long intervaloLeitura = 300;

// Saida simultanea Serial + Bluetooth
void logMsg(String msg) {
  Serial.println(msg);
  SerialBT.println(msg);
}

// Decodificacao em quadratura: dispara na borda da fase A e compara com a fase B.
//   A == B -> frente (+1)
//   A != B -> re     (-1)
void IRAM_ATTR isrEncEsq() {
  bool a = digitalRead(ENC_ESQ_A);
  bool b = digitalRead(ENC_ESQ_B);
  if (a == b) { contEsq++; sentidoEsq = 1; }
  else        { contEsq--; sentidoEsq = -1; }
}

void IRAM_ATTR isrEncDir() {
  bool a = digitalRead(ENC_DIR_A);
  bool b = digitalRead(ENC_DIR_B);
  if (a == b) { contDir++; sentidoDir = 1; }
  else        { contDir--; sentidoDir = -1; }
}

void lerEncoders() {
  noInterrupts();
  long esq = contEsq;
  long dir = contDir;
  interrupts();
  char buf[100];
  sprintf(buf, "ENC | Esq: %6ld ticks (sentido %d) | Dir: %6ld ticks (sentido %d)",
          esq, sentidoEsq, dir, sentidoDir);
  logMsg(String(buf));
}

void zerarEncoders() {
  noInterrupts();
  contEsq = 0;
  contDir = 0;
  interrupts();
  logMsg("Encoders zerados. Empurre o carrinho manualmente pela distancia combinada.");
}

// Apos ZERAR e empurrar o carrinho por uma distancia conhecida (padrao 1000 mm),
// calcula quantos mm cada tick representa em cada roda.
void calcularCalibracao(float distanciaMm) {
  long esq, dir;
  noInterrupts();
  esq = contEsq;
  dir = contDir;
  interrupts();

  if (esq == 0 || dir == 0) {
    logMsg("ERRO: contagem zerada. Rode ZERAR, empurre o carrinho e so entao rode CAL.");
    return;
  }

  float mmPorTickEsq = distanciaMm / (float)abs(esq);
  float mmPorTickDir = distanciaMm / (float)abs(dir);
  float pprEsq = (PI * RODA_DIAMETRO_MM) / mmPorTickEsq;
  float pprDir = (PI * RODA_DIAMETRO_MM) / mmPorTickDir;

  char buf[120];
  logMsg("\n=== CALCULO DE CALIBRACAO ===");
  sprintf(buf, "Distancia percorrida: %.1f mm", distanciaMm);
  logMsg(String(buf));
  sprintf(buf, "Esq -> %ld ticks | %.4f mm/tick | ~%.1f ticks/volta", esq, mmPorTickEsq, pprEsq);
  logMsg(String(buf));
  sprintf(buf, "Dir -> %ld ticks | %.4f mm/tick | ~%.1f ticks/volta", dir, mmPorTickDir, pprDir);
  logMsg(String(buf));
  logMsg("Anote estes valores (mm/tick ou ticks/volta) para ajustar ENCODER_PPR em movimento.h.");
  logMsg("===============================\n");
}

void mostrarMenu() {
  logMsg("\n======== MENU (CALIBRACAO DE ENCODERS) ========");
  logMsg("ZERAR      -> Zera a contagem antes de empurrar o carrinho");
  logMsg("LER        -> Mostra a contagem atual (Esq/Dir)");
  logMsg("START      -> Liga leitura continua (a cada 300ms)");
  logMsg("STOP       -> Para leitura continua");
  logMsg("CAL [mm]   -> Calcula mm/tick a partir da distancia percorrida");
  logMsg("              (padrao 1000mm se omitido, ex.: CAL 1000)");
  logMsg("HELP       -> Mostra este menu");
  logMsg("================================================\n");
}

void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;
  logMsg("Comando recebido: " + cmd);

  if (cmd == "ZERAR" || cmd == "RESET") {
    zerarEncoders();
  } else if (cmd == "LER" || cmd == "READ") {
    lerEncoders();
  } else if (cmd == "START") {
    leituraContinua = true;
    logMsg("Leitura continua LIGADA.");
  } else if (cmd == "STOP") {
    leituraContinua = false;
    logMsg("Leitura continua DESLIGADA.");
  } else if (cmd == "CAL" || cmd.startsWith("CAL ")) {
    float distancia = 1000.0f; // 1 metro por padrao
    int espaco = cmd.indexOf(' ');
    if (espaco != -1) {
      float valor = cmd.substring(espaco + 1).toFloat();
      if (valor > 0) distancia = valor;
    }
    calcularCalibracao(distancia);
  } else if (cmd == "HELP" || cmd == "?") {
    mostrarMenu();
  } else {
    logMsg("Comando desconhecido. Digite HELP.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n======================================");
  Serial.println("  TESTE/CALIBRACAO DE ENCODERS         ");
  Serial.println("======================================");

  if (!SerialBT.begin("ESP32_Teste_Encoder")) {
    Serial.println("ERRO: Bluetooth nao inicializou.");
  } else {
    Serial.println("Bluetooth Ativo! Procure por 'ESP32_Teste_Encoder' no celular.");
  }

  // Forca os pinos da Ponte H para LOW (motor parado). Sem isso os pinos ficam
  // flutuando e o driver pode interpretar ruido como comando de giro.
  pinMode(MOT1_IN1, OUTPUT);
  pinMode(MOT1_IN2, OUTPUT);
  pinMode(MOT2_IN1, OUTPUT);
  pinMode(MOT2_IN2, OUTPUT);
  digitalWrite(MOT1_IN1, LOW);
  digitalWrite(MOT1_IN2, LOW);
  digitalWrite(MOT2_IN1, LOW);
  digitalWrite(MOT2_IN2, LOW);

  pinMode(ENC_ESQ_A, INPUT_PULLUP);
  pinMode(ENC_ESQ_B, INPUT_PULLUP);
  pinMode(ENC_DIR_A, INPUT_PULLUP);
  pinMode(ENC_DIR_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_ESQ_A), isrEncEsq, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DIR_A), isrEncDir, CHANGE);

  logMsg("\nEncoders inicializados. Motor permanece desligado neste teste.");
  logMsg("Fluxo de calibracao: ZERAR -> empurrar o carrinho a mao -> LER (ou CAL).");
  mostrarMenu();
}

void loop() {
  // Comandos via USB
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (comandoSerial.length() > 0) executarComando(comandoSerial);
      comandoSerial = "";
    } else {
      comandoSerial += c;
    }
  }
  // Comandos via Bluetooth
  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n' || c == '\r') {
      if (comandoBT.length() > 0) executarComando(comandoBT);
      comandoBT = "";
    } else {
      comandoBT += c;
    }
  }

  // Leitura periodica sem delay
  unsigned long tempoAtual = millis();
  if (leituraContinua && tempoAtual - tempoAnterior >= intervaloLeitura) {
    tempoAnterior = tempoAtual;
    lerEncoders();
  }
}
