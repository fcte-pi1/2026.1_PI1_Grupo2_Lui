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

// --- Registrador do gyro Z (bussola) ---
#define REG_GYRO_ZOUT_H  0x47

// --- Parametros da bussola (de teste_bussola_mpu.cpp) ---
#define VEL_GIRO_PADRAO 120        // valor bruto (motor satura em 100)
#define NMNI_LIMITE_DPS  1.0f      // abaixo disso, considera parado e nao integra
// --- Parametros do giro pela bussola ---
#define PASSO_GIRO_GRAUS      90.0f
#define ZONA_FRENAGEM_GRAUS   20.0f
#define MARGEM_PARADA_GRAUS    2.0f
#define TIMEOUT_GIRO_MS       4000
// --- Correcao de overshoot (malha fechada apos a freada) ---
#define VEL_CORRECAO_GIRO       60
#define ASSENTAMENTO_MS        150
#define PULSO_CORRECAO_MS       80
#define MAX_PULSOS_CORRECAO      8

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

// --- Estado da bussola (heading relativo, so gyro Z) ---
float offsetGiroZ_dps = 0.0f;          // bias do gyro em graus/s
float rumo_deg = 0.0f;                 // heading atual, normalizado 0-360
float alvoRumo_deg = 0.0f;             // alvo ACUMULADO do giro (grade de 90), 0-360
int   velGiro = VEL_GIRO_PADRAO;       // velocidade do giro, bruta
unsigned long ultimaAtualizacao_us = 0;

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

// ==========================================
// BUSSOLA (heading relativo com gyro Z) — de teste_bussola_mpu.cpp
// Sem magnetometro: o "norte" e o rumo no momento da calibracao.
// ==========================================

// --- Leitura do gyro Z em graus/s (sem descontar bias) ---
float lerGiroZ_dps() {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(REG_GYRO_ZOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom((uint16_t)MPU6500_ADDR, (uint8_t)2);
  if (Wire.available() >= 2) {
    int16_t gzRaw = (Wire.read() << 8) | Wire.read();
    return gzRaw / 131.0f; // escala 250 dps
  }
  return 0.0f;
}

// --- Normaliza um angulo para a faixa [0, 360) ---
float normalizar360(float ang) {
  ang = fmodf(ang, 360.0f);
  if (ang < 0.0f) ang += 360.0f;
  return ang;
}

// --- Rumo -> ponto cardeal (8 direcoes) ---
String direcaoCardeal(float rumo) {
  static const char* dirs[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
  int idx = (int)((rumo + 22.5f) / 45.0f) % 8;
  return String(dirs[idx]);
}

// --- Atualiza o rumo integrando o gyro (com NMNI) ---
void atualizarBussola() {
  float giroZ_dps = lerGiroZ_dps() - offsetGiroZ_dps;

  unsigned long agora_us = micros();
  if (ultimaAtualizacao_us == 0) {
    // Primeira chamada: so registra a referencia de tempo, sem integrar
    ultimaAtualizacao_us = agora_us;
    return;
  }

  float dt_s = (agora_us - ultimaAtualizacao_us) / 1000000.0f;
  ultimaAtualizacao_us = agora_us;

  // NMNI: se o robo esta praticamente parado, nao integra (evita drift)
  if (fabsf(giroZ_dps) < NMNI_LIMITE_DPS) return;

  rumo_deg = normalizar360(rumo_deg + giroZ_dps * dt_s);
}

// --- Define o rumo atual como Norte (0 graus) ---
void definirNorte() {
  rumo_deg = 0.0f;
  alvoRumo_deg = 0.0f;   // zera tambem o alvo acumulado do giro
  ultimaAtualizacao_us = micros();
  logMsg("Norte definido: rumo atual = 0 graus.");
}

// --- Calibracao do bias do gyro Z (robo parado) ---
void calibrarOffsetGiro() {
  logMsg("[MPU] Calibrando offset do gyro (Z)... mantenha o robo parado.");
  delay(500);

  const int N_AMOSTRAS = 500;
  double soma_dps = 0.0;
  for (int i = 0; i < N_AMOSTRAS; i++) {
    soma_dps += lerGiroZ_dps();
    delay(3);
  }
  offsetGiroZ_dps = (float)(soma_dps / N_AMOSTRAS);

  char buf[60];
  sprintf(buf, "[MPU] Offset giro Z: %.3f graus/s", offsetGiroZ_dps);
  logMsg(String(buf));
}

void mostrarRumo() {
  atualizarBussola();
  char buf[64];
  sprintf(buf, "Rumo: %.1f graus (%s)", rumo_deg, direcaoCardeal(rumo_deg).c_str());
  logMsg(String(buf));
}

// --- Quanto ainda falta girar (graus) ate o alvo, no sentido do giro ---
float faltaParaAlvo(float alvo, bool paraDireita) {
  if (paraDireita) return normalizar360(rumo_deg - alvo);
  else             return normalizar360(alvo - rumo_deg);
}

// --- Erro SINALIZADO ate o alvo, na faixa (-180, 180] ---
float erroParaAlvo(float alvo) {
  float e = normalizar360(alvo - rumo_deg);
  if (e > 180.0f) e -= 360.0f;
  return e;
}

// --- Espera a inercia terminar mantendo o rumo atualizado ---
void assentarInercia(unsigned long ms) {
  motors_stop_all();
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    atualizarBussola();
    delay(2);
  }
}

// --- Gira ate a bussola bater no ALVO ABSOLUTO acumulado (alvoRumo_deg) ---
void girarParaAlvo(float alvo, bool paraDireita) {
  int sinal_esq = paraDireita ? +1 : -1;
  int sinal_dir = paraDireita ? -1 : +1;

  motor_esquerdo_set(sinal_esq * velGiro);
  motor_direito_set(sinal_dir * velGiro);

  unsigned long inicio = millis();
  bool freando = false;

  while (true) {
    atualizarBussola();
    float falta = faltaParaAlvo(alvo, paraDireita);

    // Chegou (com margem: a inercia completa o resto)
    if (falta <= MARGEM_PARADA_GRAUS) break;
    // Passou do ponto: a falta "da a volta" (>180) -> para pra nao girar de novo
    if (falta > 180.0f) break;
    if (millis() - inicio > TIMEOUT_GIRO_MS) break;

    // Perto do alvo, reduz a velocidade pela metade pra nao passar do ponto
    if (!freando && falta <= ZONA_FRENAGEM_GRAUS) {
      freando = true;
      motor_esquerdo_set(sinal_esq * (velGiro / 2));
      motor_direito_set(sinal_dir * (velGiro / 2));
    }
    delay(2);
  }
  motors_stop_all();

  // --- Fase 2: correcao de overshoot (malha fechada) ---
  for (int pulso = 0; pulso < MAX_PULSOS_CORRECAO; pulso++) {
    assentarInercia(ASSENTAMENTO_MS);

    float erro = erroParaAlvo(alvo);
    if (fabsf(erro) <= MARGEM_PARADA_GRAUS) break;

    // Gira no sentido que REDUZ o erro
    bool corrigirEsq = (erro > 0.0f);
    int se = corrigirEsq ? -1 : +1;
    int sd = corrigirEsq ? +1 : -1;
    motor_esquerdo_set(se * VEL_CORRECAO_GIRO);
    motor_direito_set(sd * VEL_CORRECAO_GIRO);

    unsigned long tPulso = millis();
    while (millis() - tPulso < PULSO_CORRECAO_MS) {
      atualizarBussola();
      if (fabsf(erroParaAlvo(alvo)) <= MARGEM_PARADA_GRAUS) break;
      delay(2);
    }
    motors_stop_all();
  }
  assentarInercia(ASSENTAMENTO_MS);

  char buf[80];
  sprintf(buf, "Giro concluido! Alvo: %.0f | Rumo agora: %.1f | Erro: %.1f",
          alvo, rumo_deg, erroParaAlvo(alvo));
  logMsg(String(buf));
}

// GIR_D: acumula -90 no alvo (direita) | GIR_E: acumula +90 (esquerda)
void girarDireita90() {
  alvoRumo_deg = normalizar360(alvoRumo_deg - PASSO_GIRO_GRAUS);
  girarParaAlvo(alvoRumo_deg, true);
}
void girarEsquerda90() {
  alvoRumo_deg = normalizar360(alvoRumo_deg + PASSO_GIRO_GRAUS);
  girarParaAlvo(alvoRumo_deg, false);
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
  logMsg("GIR_E    -> Girar 90 graus Esquerda (pela bussola, alvo acumulado)");
  logMsg("GIR_D    -> Girar 90 graus Direita (pela bussola, alvo acumulado)");
  logMsg("GIR_E_MOV-> Girar 90 graus Esquerda (versao movimento.h)");
  logMsg("GIR_D_MOV-> Girar 90 graus Direita (versao movimento.h)");
  logMsg("NORTE    -> Define o rumo atual como Norte (zera bussola)");
  logMsg("RUMO     -> Mostra o rumo atual (0-360) e o ponto cardeal");
  logMsg("CALIBRAR -> Calibra o offset do gyro Z e zera a bussola (robo parado)");
  logMsg("CALIBRAR_MPU -> Calibracao completa do MPU (acel + giro, so mostra offsets)");
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
    logMsg("Girando 90 graus pra ESQUERDA (bussola)...");
    girarEsquerda90();
  } else if (cmd == "GIR_D") {
    logMsg("Girando 90 graus pra DIREITA (bussola)...");
    girarDireita90();
  } else if (cmd == "GIR_E_MOV") {
    logMsg("Girando 90 graus esquerda (movimento.h)...");
    girar_esquerda_90();
    logMsg("Terminou!");
  } else if (cmd == "GIR_D_MOV") {
    logMsg("Girando 90 graus direita (movimento.h)...");
    girar_direita_90();
    logMsg("Terminou!");
  } else if (cmd == "NORTE") {
    definirNorte();
  } else if (cmd == "RUMO") {
    mostrarRumo();
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
  } else if (cmd == "HELP" || cmd == "?") {
    mostrarMenu();
  } else if (cmd == "CALIBRAR") {
    calibrarOffsetGiro();
    definirNorte();
  } else if (cmd == "CALIBRAR_MPU") {
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

  // --- Setup Bussola (calibra o bias do gyro Z e zera o rumo) ---
  calibrarOffsetGiro();
  definirNorte();

  logMsg("\n--- INICIALIZACAO COMPLETA ---");
  mostrarMenu();
}

void loop() {
  // Integra o rumo continuamente, pra bussola nao perder nenhum giro
  atualizarBussola();

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
