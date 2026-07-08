// ==========================================
// TESTE GERAL DO MICROMOUSE (TESTES UNIFICADOS)
// Com Bluetooth Serial ("micromouse")
// ==========================================
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include "BluetoothSerial.h"
#include "../encoder/encoder.h"
#include "../motors/motors.h"
#include "../movimento/movimento.h"
#include "../mpu/mpu.h"
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

// --- Variáveis Globais ---
BluetoothSerial SerialBT;

// --- Variáveis de Estado ---
String comandoBT = "";
String comandoSerial = "";
bool leituraSensoresContinua = false;
unsigned long tempoAnteriorSensores = 0;
bool mpuAtivo = false;
volatile bool mpuDadosProntos = false;

// --- Variaveis do Encoder ---
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

// --- Variáveis de Calibração MPU ---
long mpuOffsetAcelX = -1553;
long mpuOffsetAcelY = -26;
long mpuOffsetAcelZ = 852;
long mpuOffsetGiroX = 740;
long mpuOffsetGiroY = 412;
long mpuOffsetGiroZ = -32;

void lerMPU() {
  mpuDadosProntos = false;

  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6500_ADDR, 14);

  if (Wire.available() >= 14) {
    int16_t axRaw = (Wire.read() << 8) | Wire.read();
    int16_t ayRaw = (Wire.read() << 8) | Wire.read();
    int16_t azRaw = (Wire.read() << 8) | Wire.read();
    int16_t tempRaw = (Wire.read() << 8) | Wire.read();
    int16_t gxRaw = (Wire.read() << 8) | Wire.read();
    int16_t gyRaw = (Wire.read() << 8) | Wire.read();
    int16_t gzRaw = (Wire.read() << 8) | Wire.read();

    // Aplica os offsets de calibração
    float ax = (axRaw - mpuOffsetAcelX) / 16384.0; // Em forca G
    float ay = (ayRaw - mpuOffsetAcelY) / 16384.0; // Em forca G
    float az = (azRaw - mpuOffsetAcelZ) / 16384.0; // Em forca G

    float gx = (gxRaw - mpuOffsetGiroX) / 131.0; // Em graus/segundo
    float gy = (gyRaw - mpuOffsetGiroY) / 131.0; // Em graus/segundo
    float gz = (gzRaw - mpuOffsetGiroZ) / 131.0; // Em graus/segundo

    char buf[120];
    sprintf(buf, "MPU | ACEL(G) X:%.2f Y:%.2f Z:%.2f | GIRO(deg/s) X:%.1f Y:%.1f Z:%.1f", ax, ay, az, gx, gy, gz);
    logMsg(String(buf));
  }
}

// --- Integracao do Yaw (mesma logica do teste_giroscopio_parede) ---
float anguloZ = 0.0f;
unsigned long ultimoTempoGiro = 0;

void atualizarGiroscopio() {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(0x47); // REG_GYRO_ZOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom((uint16_t)MPU6500_ADDR, (uint8_t)2);

  if (Wire.available() >= 2) {
    int16_t gzRaw = (Wire.read() << 8) | Wire.read();
    float gz = (gzRaw - mpuOffsetGiroZ) / 131.0f;

    unsigned long agora = millis();
    float dt = (agora - ultimoTempoGiro) / 1000.0f;
    ultimoTempoGiro = agora;

    if (abs(gz) > 1.0f) {
      anguloZ += gz * dt;
    }
  }
}

// --- Dummy motor functions removidas (usando motors.h e movimento.h agora) ---

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
  int r2 = tof2Ok ? sensor2.readRangeContinuousMillimeters() - 45 : -1;
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

// --- Teste Distancia Parede ---
#define DIST_MIN_PAREDE_MM 30  // 3cm
#define TEMPO_GIRO_CORRECAO_MS 200
#define COOLDOWN_CORRECAO_MS 2000  // trava pra girar uma vez so

unsigned long ultimaCorrecao = 0;

void testeDistanciaParede() {
  // Trava: se acabou de corrigir, ignora (evita girar varias vezes seguidas)
  if (ultimaCorrecao != 0 && millis() - ultimaCorrecao < COOLDOWN_CORRECAO_MS) {
    logMsg("Correcao recente, aguardando cooldown...");
    return;
  }

  // Le os dois TOFs laterais (S1 = esquerdo, S3 = direito), com os mesmos offsets
  int distEsq = tof1Ok ? sensor1.readRangeContinuousMillimeters() - 23 : -1;
  int distDir = tof3Ok ? sensor3.readRangeContinuousMillimeters() - 37 : -1;

  char buf[80];
  sprintf(buf, "DIST PAREDE | Esq: %d mm | Dir: %d mm", distEsq, distDir);
  logMsg(String(buf));

  if (distEsq != -1 && distEsq < DIST_MIN_PAREDE_MM) {
    // Muito perto da parede esquerda -> gira pra direita pra se afastar
    logMsg("Parede esquerda muito perto! Girando pra direita...");
    girar_direita(VEL_GIRO / 2, TEMPO_GIRO_CORRECAO_MS);
    ultimaCorrecao = millis();
    logMsg("Corrigido!");
  } else if (distDir != -1 && distDir < DIST_MIN_PAREDE_MM) {
    // Muito perto da parede direita -> gira pra esquerda pra se afastar
    logMsg("Parede direita muito perto! Girando pra esquerda...");
    girar_esquerda(VEL_GIRO / 2, TEMPO_GIRO_CORRECAO_MS);
    ultimaCorrecao = millis();
    logMsg("Corrigido!");
  } else {
    logMsg("Distancia OK, nenhuma correcao necessaria.");
  }
}

// --- Teste Ajuste Parede (gira ate afastar, limitado pelo giroscopio) ---
#define TIMEOUT_AJUSTE_MS 3000     // seguranca: para de girar apos 3s
#define ANGULO_MAX_AJUSTE 45.0f    // graus: nao gira mais que isso

void testeAjusteParede() {
  int distEsq = tof1Ok ? sensor1.readRangeContinuousMillimeters() - 23 : -1;
  int distDir = tof3Ok ? sensor3.readRangeContinuousMillimeters() - 37 : -1;

  char buf[80];
  sprintf(buf, "AJUSTE PAREDE | Esq: %d mm | Dir: %d mm", distEsq, distDir);
  logMsg(String(buf));

  if (distEsq != -1 && distEsq < DIST_MIN_PAREDE_MM) {
    // Perto da parede esquerda -> gira pra direita ate passar de 30mm
    logMsg("Parede esquerda < 30mm! Girando pra direita ate afastar...");
    anguloZ = 0.0f;
    ultimoTempoGiro = millis();
    motor_esquerdo_set(VEL_GIRO / 2);
    motor_direito_set(-(VEL_GIRO / 2));
    unsigned long inicio = millis();
    while (millis() - inicio < TIMEOUT_AJUSTE_MS) {
      atualizarGiroscopio();
      if (abs(anguloZ) >= ANGULO_MAX_AJUSTE) {
        logMsg("Limite de angulo atingido!");
        break;
      }
      distEsq = sensor1.readRangeContinuousMillimeters() - 23;
      if (distEsq >= DIST_MIN_PAREDE_MM) break;
      delay(10);
    }
    motors_stop_all();
    sprintf(buf, "Ajustado! Esq: %d mm | Girou: %.1f graus", distEsq, abs(anguloZ));
    logMsg(String(buf));
  } else if (distDir != -1 && distDir < DIST_MIN_PAREDE_MM) {
    // Perto da parede direita -> gira pra esquerda ate passar de 30mm
    logMsg("Parede direita < 30mm! Girando pra esquerda ate afastar...");
    anguloZ = 0.0f;
    ultimoTempoGiro = millis();
    motor_esquerdo_set(-(VEL_GIRO / 2));
    motor_direito_set(VEL_GIRO / 2);
    unsigned long inicio = millis();
    while (millis() - inicio < TIMEOUT_AJUSTE_MS) {
      atualizarGiroscopio();
      if (abs(anguloZ) >= ANGULO_MAX_AJUSTE) {
        logMsg("Limite de angulo atingido!");
        break;
      }
      distDir = sensor3.readRangeContinuousMillimeters() - 37;
      if (distDir >= DIST_MIN_PAREDE_MM) break;
      delay(10);
    }
    motors_stop_all();
    sprintf(buf, "Ajustado! Dir: %d mm | Girou: %.1f graus", distDir, abs(anguloZ));
    logMsg(String(buf));
  } else {
    logMsg("Distancia OK, nenhum ajuste necessario.");
  }
}

// --- Menu ---
void mostrarMenu() {
  logMsg("\n========= TESTE GERAL MICROMOUSE =========");
  logMsg("MENU DE COMANDOS:");
  logMsg("TOF_ON   -> Liga leitura dos sensores de distancia (500ms)");
  logMsg("TOF_OFF  -> Desliga leitura dos sensores");
  logMsg("MPU_ON   -> Liga leitura do Giroscopio/Acelerometro");
  logMsg("MPU_OFF  -> Desliga MPU");
  logMsg("ENC_ON   -> Liga impressao de velocidade dos Encoders");
  logMsg("ENC_OFF  -> Desliga impressao de velocidade");
  logMsg("ENC_RST  -> Zera a contagem dos Encoders (Para calibracao)");
  logMsg("MOV_F    -> Mover 1 Celula Frente (via Encoder)");
  logMsg("MOV_T    -> Mover 1 Celula Tras (via Encoder)");
  logMsg("GIR_E    -> Girar 90 graus Esquerda");
  logMsg("GIR_D    -> Girar 90 graus Direita");
  logMsg("DIST_PAR -> Teste distancia parede");
  logMsg("AJU_PAR  -> Gira ate ficar a +30mm da parede lateral");
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
  } else if (cmd == "MPU_OFF") {
    mpuAtivo = false;
  } else if (cmd == "MOV_F") {
    logMsg("Movendo 1 celula pra frente...");
    mover_frente_celula();
    logMsg("Terminou!");
  } else if (cmd == "MOV_T") {
    logMsg("Movendo 1 celula pra tras...");
    mover_tras_celula();
    logMsg("Terminou!");
  } else if (cmd == "GIR_E") {
    logMsg("Girando 90 graus esquerda...");
    girar_esquerda_90();
    logMsg("Terminou!");
  } else if (cmd == "GIR_D") {
    logMsg("Girando 90 graus direita...");
    girar_direita_90();
    logMsg("Terminou!");
  } else if (cmd == "ENC") {
    lerEncoders();
  } else if (cmd == "ENC_ON") {
    encAtivo = true;
    lastEncEsq = encoder_esquerdo_get();
    lastEncDir = encoder_direito_get();
    lastEncTime = millis();
  } else if (cmd == "ENC_OFF") {
    encAtivo = false;
  } else if (cmd == "ENC_RST") {
    encoder_esquerdo_reset();
    encoder_direito_reset();
    logMsg("Encoders zerados com sucesso!");
  } else if (cmd == "DIST_PAR") {
    logMsg("Teste distancia parede...");
    testeDistanciaParede();
    logMsg("Terminou!");
  } else if (cmd == "AJU_PAR") {
    logMsg("Teste ajuste parede...");
    testeAjusteParede();
    logMsg("Terminou!");
  } else if (cmd == "HELP" || cmd == "?") {
    mostrarMenu();
  } else if (cmd == "CALIBRAR") {
    calibrarMPU();
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

  // --- Setup MPU compartilhado (mpu.cpp) ---
  // Necessario para girar_esquerda_90()/girar_direita_90() (em movimento.cpp),
  // que agora fecham a malha pelo giroscopio via mpu_atualizar_angulo()/
  // mpu_get_angulo(). Sem isso o objeto Adafruit_MPU6050 nunca e' inicializado
  // e o angulo nunca sobe, entao GIR_E/GIR_D so param pelo timeout de seguranca
  // (girando por 3s em vez de parar nos 90 graus).
  configurarMPU();
  mpu_calibrar_offset_giro();

  // --- Setup Motores (da biblioteca real) ---
  motors_init();

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
