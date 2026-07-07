// ==========================================
// TESTE GERAL DO MICROMOUSE (TESTES UNIFICADOS)
// Com Bluetooth Serial ("micromouse")
// ==========================================
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include "BluetoothSerial.h"
#include "../encoder/encoder.h"

// --- Pinos MPU6500 & I2C ---
#define SDA_PIN 21
#define SCL_PIN 22
#define MPU_INT_PIN 23
#define MPU6500_ADDR 0x68
#define REG_PWR_MGMT_1   0x6B
#define REG_INT_PIN_CFG  0x37
#define REG_INT_ENABLE   0x38
#define REG_ACCEL_XOUT_H 0x3B
#define REG_WHO_AM_I     0x75

// --- Pinos VL53L0X ---
#define TOF1_XSHUT 4
#define TOF2_XSHUT 16
#define TOF3_XSHUT 17

// --- Pinos Ponte H ---
#define M1_IN1 14
#define M1_IN2 27
#define M2_IN1 26
#define M2_IN2 25

BluetoothSerial SerialBT;

// --- Variáveis de Estado ---
String comandoBT = "";
String comandoSerial = "";
bool leituraSensoresContinua = false;
unsigned long tempoAnteriorSensores = 0;
bool mpuAtivo = false;
volatile bool mpuDadosProntos = false;

bool encAtivo = false;
long lastEncEsq = 0;
long lastEncDir = 0;
unsigned long lastEncTime = 0;

// --- Sensores TOF ---
VL53L0X sensor1;
VL53L0X sensor2;
VL53L0X sensor3;
bool tof1Ok = false, tof2Ok = false, tof3Ok = false;

// Funções Utilitárias
void logMsg(String msg) {
  Serial.println(msg);
  SerialBT.println(msg);
}

// --- MPU6500 Funções ---
void IRAM_ATTR funcaoInterrupcaoMPU() {
  mpuDadosProntos = true;
}

void escreverReg(uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(reg);
  Wire.write(valor);
  Wire.endTransmission();
}

void lerMPU() {
  // if (!mpuDadosProntos) return; // Removido para forçar a leitura ignorando o pino INT físico
  mpuDadosProntos = false;

  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6500_ADDR, 14);

  if (Wire.available() >= 14) {
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();
    int16_t tempRaw = (Wire.read() << 8) | Wire.read();
    int16_t gx = (Wire.read() << 8) | Wire.read();
    int16_t gy = (Wire.read() << 8) | Wire.read();
    int16_t gz = (Wire.read() << 8) | Wire.read();

    char buf[120];
    sprintf(buf, "MPU | ACEL X:%d Y:%d Z:%d | GIRO X:%d Y:%d Z:%d", ax, ay, az, gx, gy, gz);
    logMsg(String(buf));
  }
}

// --- Motores Funções ---
void pararMotores() {
  digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN1, LOW); digitalWrite(M2_IN2, LOW);
}
void motoresFrente() {
  digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN1, HIGH); digitalWrite(M2_IN2, LOW);
}
void motoresTras() {
  digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, HIGH);
  digitalWrite(M2_IN1, LOW); digitalWrite(M2_IN2, HIGH);
}

// --- Filtro TOF (Média Móvel Exponencial - EMA) ---
float filtroS1 = 0;
float filtroS2 = 0;
float filtroS3 = 0;
const float alphaTOF = 0.3; // Fator de suavização (0.0 a 1.0). Quanto menor, mais suave (mas mais lento)

// --- TOF Funções ---
void lerTOFs() {
  char buf[80];
  // 1. Lê a distância bruta e aplica o seu balanceamento (calibração de offset)
  int r1 = tof1Ok ? sensor1.readRangeContinuousMillimeters() - 23 : -1;
  int r2 = tof2Ok ? sensor2.readRangeContinuousMillimeters() - 40 : -1;
  int r3 = tof3Ok ? sensor3.readRangeContinuousMillimeters() - 37 : -1;

  // 2. Aplica o Filtro EMA
  if (r1 != -1) filtroS1 = (filtroS1 == 0) ? r1 : (alphaTOF * r1) + ((1.0 - alphaTOF) * filtroS1);
  if (r2 != -1) filtroS2 = (filtroS2 == 0) ? r2 : (alphaTOF * r2) + ((1.0 - alphaTOF) * filtroS2);
  if (r3 != -1) filtroS3 = (filtroS3 == 0) ? r3 : (alphaTOF * r3) + ((1.0 - alphaTOF) * filtroS3);

  // 3. Imprime os valores já filtrados
  sprintf(buf, "TOF | S1:%4d mm | S2:%4d mm | S3:%4d mm", (int)filtroS1, (int)filtroS2, (int)filtroS3);
  logMsg(String(buf));
}

// --- Encoders Funções ---
void lerEncoders() {
    char buf[80];
    sprintf(buf, "ENC | Esq: %ld | Dir: %ld", encoder_esquerdo_get(), encoder_direito_get());
    logMsg(String(buf));
}

// --- Calibração MPU ---
void calibrarMPU() {
  logMsg("\n>> INICIANDO CALIBRACAO DO MPU...");
  logMsg(">> MANTENHA O ROBO COMPLETAMENTE PARADO E PLANO!");
  delay(3000);

  long sumAcelX = 0, sumAcelY = 0, sumAcelZ = 0;
  long sumGiroX = 0, sumGiroY = 0, sumGiroZ = 0;
  int numLeituras = 1000;

  for (int i = 1; i <= numLeituras; i++) {
    Wire.beginTransmission(MPU6500_ADDR);
    Wire.write(REG_ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6500_ADDR, 14);

    if (Wire.available() >= 14) {
      int16_t ax = (Wire.read() << 8) | Wire.read();
      int16_t ay = (Wire.read() << 8) | Wire.read();
      int16_t az = (Wire.read() << 8) | Wire.read();
      Wire.read(); Wire.read(); // ignora temperatura
      int16_t gx = (Wire.read() << 8) | Wire.read();
      int16_t gy = (Wire.read() << 8) | Wire.read();
      int16_t gz = (Wire.read() << 8) | Wire.read();

      sumAcelX += ax;
      sumAcelY += ay;
      sumAcelZ += az;
      sumGiroX += gx;
      sumGiroY += gy;
      sumGiroZ += gz;
    }
    
    delay(3); // Aguarda o MPU atualizar os registradores

    // Mostra o progresso e a média "parcial" a cada 200 leituras (interações)
    if (i % 200 == 0) {
      char buf[120];
      sprintf(buf, "Lendo %d/%d... Media Parcial Giro Z: %ld", i, numLeituras, (sumGiroZ / i));
      logMsg(String(buf));
    }
  }

  long mediaAcelX = sumAcelX / numLeituras;
  long mediaAcelY = sumAcelY / numLeituras;
  long mediaAcelZ = sumAcelZ / numLeituras;
  long mediaGiroX = sumGiroX / numLeituras;
  long mediaGiroY = sumGiroY / numLeituras;
  long mediaGiroZ = sumGiroZ / numLeituras;

  logMsg("\n=== RESULTADO DA CALIBRACAO ===");
  logMsg("Anote estes valores para usar como OFFSET no seu robo:");
  char buf[120];
  // Z desconta 16384 (gravidade 1G na escala de 2G)
  sprintf(buf, "OFFSET ACEL -> X: %ld | Y: %ld | Z: %ld", mediaAcelX, mediaAcelY, (mediaAcelZ - 16384));
  logMsg(String(buf));
  sprintf(buf, "OFFSET GIRO -> X: %ld | Y: %ld | Z: %ld", mediaGiroX, mediaGiroY, mediaGiroZ);
  logMsg(String(buf));
  logMsg("===============================\n");
}


// --- Menu ---
void mostrarMenu() {
  logMsg("\n========= TESTE GERAL MICROMOUSE =========");
  logMsg("MENU DE COMANDOS:");
  logMsg("TOF_ON   -> Liga leitura dos sensores de distancia (500ms)");
  logMsg("TOF_OFF  -> Desliga leitura dos sensores");
  logMsg("MPU_ON   -> Liga leitura do Giroscopio/Acelerometro");
  logMsg("MPU_OFF  -> Desliga MPU");
  logMsg("CALIBRAR -> Faz a calibracao do MPU (1000 amostras)");
  logMsg("ENC      -> Mostra contagem absoluta atual dos Encoders");
  logMsg("ENC_ON   -> Liga leitura continua de VELOCIDADE (ticks/s)");
  logMsg("ENC_OFF  -> Desliga leitura de VELOCIDADE");
  logMsg("MOTOR_F  -> Liga Motores para Frente (2s)");
  logMsg("MOTOR_T  -> Liga Motores para Tras (2s)");
  logMsg("HELP     -> Mostra este menu");
  logMsg("===========================================\n");
}

void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;
  logMsg(">> Comando: " + cmd);

  if (cmd == "TOF_ON") {
    leituraSensoresContinua = true;
  } else if (cmd == "TOF_OFF") {
    leituraSensoresContinua = false;
  } else if (cmd == "MPU_ON") {
    mpuAtivo = true;
  } else if (cmd == "MPU_OFF") {
    mpuAtivo = false;
  } else if (cmd == "CALIBRAR") {
    calibrarMPU();
  } else if (cmd == "ENC") {
    lerEncoders();
  } else if (cmd == "ENC_ON") {
    encAtivo = true;
    lastEncEsq = encoder_esquerdo_get();
    lastEncDir = encoder_direito_get();
    lastEncTime = millis();
  } else if (cmd == "ENC_OFF") {
    encAtivo = false;
  } else if (cmd == "MOTOR_F") {
    motoresFrente(); delay(2000); pararMotores();
    logMsg("Motores parados.");
  } else if (cmd == "MOTOR_T") {
    motoresTras(); delay(2000); pararMotores();
    logMsg("Motores parados.");
  } else if (cmd == "HELP" || cmd == "?") {
    mostrarMenu();
  } else {
    logMsg("Comando desconhecido. Digite HELP.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inicializa o Bluetooth
  if (!SerialBT.begin("micromouse")) {
    Serial.println("ERRO BT");
  }

  // Inicializa I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000); // MPU aguenta, TOF tbm

  // --- Setup Motores ---
  pinMode(M1_IN1, OUTPUT); pinMode(M1_IN2, OUTPUT);
  pinMode(M2_IN1, OUTPUT); pinMode(M2_IN2, OUTPUT);
  pararMotores();

  // --- Setup Encoders ---
  encoders_init();

  // --- Setup TOF ---
  pinMode(TOF1_XSHUT, OUTPUT); pinMode(TOF2_XSHUT, OUTPUT); pinMode(TOF3_XSHUT, OUTPUT);
  digitalWrite(TOF1_XSHUT, LOW); digitalWrite(TOF2_XSHUT, LOW); digitalWrite(TOF3_XSHUT, LOW);
  delay(100);
  
  digitalWrite(TOF1_XSHUT, HIGH); delay(100);
  if (sensor1.init()) { sensor1.setAddress(0x30); sensor1.startContinuous(); tof1Ok = true; }
  digitalWrite(TOF2_XSHUT, HIGH); delay(100);
  if (sensor2.init()) { sensor2.setAddress(0x31); sensor2.startContinuous(); tof2Ok = true; }
  digitalWrite(TOF3_XSHUT, HIGH); delay(100);
  if (sensor3.init()) { sensor3.setAddress(0x32); sensor3.startContinuous(); tof3Ok = true; }

  // --- Setup MPU6500 ---
  escreverReg(REG_PWR_MGMT_1, 0x00); delay(50);
  escreverReg(REG_INT_PIN_CFG, 0x00);
  escreverReg(REG_INT_ENABLE, 0x01);
  pinMode(MPU_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), funcaoInterrupcaoMPU, RISING);

  logMsg("\n--- INICIALIZACAO COMPLETA ---");
  mostrarMenu();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (comandoSerial.length() > 0) executarComando(comandoSerial);
      comandoSerial = "";
    } else {
      comandoSerial += c;
    }
  }
  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n' || c == '\r') {
      if (comandoBT.length() > 0) executarComando(comandoBT);
      comandoBT = "";
    } else {
      comandoBT += c;
    }
  }

  unsigned long tempoAtual = millis();
  if (leituraSensoresContinua && tempoAtual - tempoAnteriorSensores >= 500) {
    tempoAnteriorSensores = tempoAtual;
    lerTOFs();
  }

  if (encAtivo && tempoAtual - lastEncTime >= 500) {
    long curEsq = encoder_esquerdo_get();
    long curDir = encoder_direito_get();
    
    // Calcula a variação (ticks por segundo)
    long deltaT = tempoAtual - lastEncTime;
    long varEsq = (curEsq - lastEncEsq) * 1000 / deltaT;
    long varDir = (curDir - lastEncDir) * 1000 / deltaT;
    
    lastEncEsq = curEsq;
    lastEncDir = curDir;
    lastEncTime = tempoAtual;

    char buf[120];
    sprintf(buf, "ENC VELOCIDADE | Esq: %ld ticks/s | Dir: %ld ticks/s", varEsq, varDir);
    logMsg(String(buf));
  }

  if (mpuAtivo) {
    lerMPU();
    delay(100); // delay para nao floodar tanto a tela
  } else {
    mpuDadosProntos = false; // descarta se nao ativo
  }
}
