// ==========================================
// TESTE ISOLADO: 3 SENSORES TOF VL53L0X
// Com Bluetooth Serial (comandos via USB ou celular)
// ==========================================
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include "BluetoothSerial.h"

#define SDA_PIN 21
#define SCL_PIN 22

// Pinos XSHUT dos Sensores TOF
#define TOF1_XSHUT 4
#define TOF2_XSHUT 16
#define TOF3_XSHUT 17

VL53L0X sensor1;
VL53L0X sensor2;
VL53L0X sensor3;
BluetoothSerial SerialBT;

bool tof1Ok = false, tof2Ok = false, tof3Ok = false;
bool leituraContinua = true;

unsigned long tempoAnteriorTOF = 0;
const long intervaloTOF = 500;

String comandoSerial = "";
String comandoBT = "";

// Saida simultanea Serial + Bluetooth
void logMsg(String msg) {
  Serial.println(msg);
  SerialBT.println(msg);
}

void scanI2C() {
  logMsg("\n------ SCAN I2C ------");
  bool encontrou = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      char buf[32];
      sprintf(buf, "Dispositivo em 0x%02X", addr);
      logMsg(String(buf));
      encontrou = true;
    }
  }
  if (!encontrou) logMsg("Nenhum dispositivo encontrado.");
  logMsg("----------------------");
}

void lerSensores() {
  char buf[80];
  int d1 = tof1Ok ? sensor1.readRangeContinuousMillimeters() : -1;
  int d2 = tof2Ok ? sensor2.readRangeContinuousMillimeters() : -1;
  int d3 = tof3Ok ? sensor3.readRangeContinuousMillimeters() : -1;
  sprintf(buf, "S1 (0x30): %4d mm | S2 (0x31): %4d mm | S3 (0x32): %4d mm", d1, d2, d3);
  logMsg(String(buf));
}

void mostrarMenu() {
  logMsg("\n======== MENU (SENSORES TOF) ========");
  logMsg("START -> Liga leitura continua (a cada 500ms)");
  logMsg("STOP  -> Para leitura continua");
  logMsg("TOF   -> Faz 10 leituras rapidas");
  logMsg("SCAN  -> Scan do barramento I2C");
  logMsg("HELP  -> Mostra este menu");
  logMsg("=====================================\n");
}

void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;
  logMsg("Comando recebido: " + cmd);

  if (cmd == "START") {
    leituraContinua = true;
    logMsg("Leitura continua LIGADA.");
  } else if (cmd == "STOP") {
    leituraContinua = false;
    logMsg("Leitura continua DESLIGADA.");
  } else if (cmd == "TOF" || cmd == "SENSOR" || cmd == "SENSORES") {
    logMsg("\n=== 10 LEITURAS ===");
    for (int i = 0; i < 10; i++) {
      lerSensores();
      delay(150);
    }
    logMsg("=== FIM ===\n");
  } else if (cmd == "SCAN") {
    scanI2C();
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
  Serial.println("  TESTE 3x VL53L0X + BLUETOOTH        ");
  Serial.println("======================================");

  if (!SerialBT.begin("ESP32_Teste_TOF")) {
    Serial.println("ERRO: Bluetooth nao inicializou.");
  } else {
    Serial.println("Bluetooth Ativo! Procure por 'ESP32_Teste_TOF' no celular.");
  }

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  // Desliga todos os TOFs para reset
  pinMode(TOF1_XSHUT, OUTPUT);
  pinMode(TOF2_XSHUT, OUTPUT);
  pinMode(TOF3_XSHUT, OUTPUT);
  digitalWrite(TOF1_XSHUT, LOW);
  digitalWrite(TOF2_XSHUT, LOW);
  digitalWrite(TOF3_XSHUT, LOW);
  delay(100);
  logMsg("\nTodos os sensores TOF em RESET.");
  scanI2C();

  // Inicializacao sequencial (cada um ganha um endereco unico)
  logMsg("\nLigando Sensor 1...");
  digitalWrite(TOF1_XSHUT, HIGH);
  delay(100);
  if (sensor1.init()) {
    sensor1.setAddress(0x30);
    sensor1.startContinuous();
    tof1Ok = true;
    logMsg("Sensor 1 pronto no endereco 0x30.");
  } else {
    logMsg("ERRO: Sensor 1 nao inicializou.");
  }

  logMsg("\nLigando Sensor 2...");
  digitalWrite(TOF2_XSHUT, HIGH);
  delay(100);
  if (sensor2.init()) {
    sensor2.setAddress(0x31);
    sensor2.startContinuous();
    tof2Ok = true;
    logMsg("Sensor 2 pronto no endereco 0x31.");
  } else {
    logMsg("ERRO: Sensor 2 nao inicializou.");
  }

  logMsg("\nLigando Sensor 3...");
  digitalWrite(TOF3_XSHUT, HIGH);
  delay(100);
  if (sensor3.init()) {
    sensor3.setAddress(0x32);
    sensor3.startContinuous();
    tof3Ok = true;
    logMsg("Sensor 3 pronto no endereco 0x32.");
  } else {
    logMsg("ERRO: Sensor 3 nao inicializou.");
  }

  logMsg("\nInicializacao finalizada. Rodando...");
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
  if (leituraContinua && tempoAtual - tempoAnteriorTOF >= intervaloTOF) {
    tempoAnteriorTOF = tempoAtual;
    lerSensores();
  }
}