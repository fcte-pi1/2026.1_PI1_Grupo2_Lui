// ==========================================
// TESTE GIRO 90 GRAUS COM MPU6500 (GYRO)
// Base: teste_geral2.cpp
// Abordagem do Hilmer (navigation_controller / mpu.cpp):
//   1. Calibra o bias do gyro com o robo parado
//   2. Zera o angulo antes da manobra
//   3. Integra o yaw com dt em micros (guarda na 1a leitura)
//   4. Gira ate atingir o angulo alvo (com margem pra inercia)
// Com Bluetooth Serial ("micromouse")
// ==========================================
#include <Arduino.h>
#include <Wire.h>
#include "BluetoothSerial.h"

// --- Pinos MPU6500 & I2C ---
#define SDA_PIN 21
#define SCL_PIN 22
#define MPU6500_ADDR 0x68
#define REG_PWR_MGMT_1   0x6B
#define REG_GYRO_ZOUT_H  0x47

// --- Motores (DRV8833) — mesma configuracao de motors.cpp/pins.h ---
#define PIN_MOT1_IN1 14 // AIN1: Motor esquerdo (M1)
#define PIN_MOT1_IN2 27 // AIN2: Motor esquerdo (M1)
#define PIN_MOT2_IN1 26 // BIN1: Motor direito (M2)
#define PIN_MOT2_IN2 25 // BIN2: Motor direito (M2)

#define MOTOR_PWM_FREQ       20000 // 20 kHz — acima da faixa audivel
#define MOTOR_PWM_RESOLUTION 8     // 8 bits -> duty 0-255

#define CH_MOT1_IN1 0
#define CH_MOT1_IN2 1
#define CH_MOT2_IN1 2
#define CH_MOT2_IN2 3

#define VEL_GIRO 120 // mesma constante do movimento.h

// --- Parametros do giro ---
#define ANGULO_ALVO 90.0f       // graus
#define MARGEM_PARADA 3.0f      // para um pouco antes: a inercia completa o giro
#define TIMEOUT_GIRO_MS 3000    // seguranca

// --- Variáveis Globais ---
BluetoothSerial SerialBT;
String comandoBT = "";
String comandoSerial = "";

// --- Estado da integracao do yaw (abordagem do mpu.cpp) ---
float offsetGiroZ_dps = 0.0f;          // bias do gyro em graus/s
float anguloAcumulado_deg = 0.0f;      // yaw integrado em graus
unsigned long ultimaAtualizacao_us = 0;

// Funções Utilitárias
void logMsg(String msg) {
  Serial.println(msg);
  SerialBT.println(msg);
}

void escreverReg(uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(reg);
  Wire.write(valor);
  Wire.endTransmission();
}

// --- Motores (logica copiada de motors.cpp, sem include) ---
void motorSet(uint8_t chFwd, uint8_t chBwd, int velocidade_percentual) {
  // Mesma escala de motors.cpp: -100..100 (%) mapeado pra 0-255 de PWM
  velocidade_percentual = constrain(velocidade_percentual, -100, 100);
  int pwm = map(abs(velocidade_percentual), 0, 100, 0, 255);
  int velocidade = velocidade_percentual;

  if (velocidade > 0) {
    ledcWrite(chFwd, pwm);
    ledcWrite(chBwd, 0);
  } else if (velocidade < 0) {
    ledcWrite(chFwd, 0);
    ledcWrite(chBwd, pwm);
  } else {
    ledcWrite(chFwd, 0);
    ledcWrite(chBwd, 0);
  }
}

void motorEsquerdoSet(int velocidade) {
  // Invertido via software: polaridade dos fios trocada na placa
  motorSet(CH_MOT1_IN1, CH_MOT1_IN2, -velocidade);
}

void motorDireitoSet(int velocidade) {
  // Invertido via software: polaridade dos fios trocada na placa
  motorSet(CH_MOT2_IN1, CH_MOT2_IN2, -velocidade);
}

void motoresParar() {
  // Brake ativo: ambos os pinos em HIGH -> DRV8833 trava o motor
  ledcWrite(CH_MOT1_IN1, 255);
  ledcWrite(CH_MOT1_IN2, 255);
  ledcWrite(CH_MOT2_IN1, 255);
  ledcWrite(CH_MOT2_IN2, 255);
}

void motoresInit() {
  ledcSetup(CH_MOT1_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
  ledcAttachPin(PIN_MOT1_IN1, CH_MOT1_IN1);
  ledcSetup(CH_MOT1_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
  ledcAttachPin(PIN_MOT1_IN2, CH_MOT1_IN2);
  ledcSetup(CH_MOT2_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
  ledcAttachPin(PIN_MOT2_IN1, CH_MOT2_IN1);
  ledcSetup(CH_MOT2_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
  ledcAttachPin(PIN_MOT2_IN2, CH_MOT2_IN2);
  motoresParar();
}

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

// --- Calibracao do bias (igual mpu_calibrar_offset_giro) ---
// MEMS sempre tem um pequeno bias mesmo parado; sem descontar,
// a integracao acumula erro (drift).
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

// --- Integracao do yaw (igual mpu_atualizar_angulo) ---
void atualizarAngulo() {
  float giroZ_dps = lerGiroZ_dps() - offsetGiroZ_dps;

  unsigned long agora_us = micros();
  if (ultimaAtualizacao_us == 0) {
    // Primeira chamada: so registra a referencia de tempo, sem integrar
    ultimaAtualizacao_us = agora_us;
    return;
  }

  float dt_s = (agora_us - ultimaAtualizacao_us) / 1000000.0f;
  ultimaAtualizacao_us = agora_us;

  anguloAcumulado_deg += giroZ_dps * dt_s;
}

void zerarAngulo() {
  anguloAcumulado_deg = 0.0f;
  ultimaAtualizacao_us = micros();
}

// --- Giro de 90 graus controlado pelo gyro ---
// paraDireita = true: horario (angulo negativo); false: anti-horario
void girar90ComGyro(bool paraDireita) {
  char buf[80];
  logMsg(paraDireita ? "Girando 90 graus pra DIREITA (gyro)..."
                     : "Girando 90 graus pra ESQUERDA (gyro)...");

  zerarAngulo();

  int vel = VEL_GIRO;
  motorEsquerdoSet(paraDireita ? vel : -vel);
  motorDireitoSet(paraDireita ? -vel : vel);

  unsigned long inicio = millis();
  while (millis() - inicio < TIMEOUT_GIRO_MS) {
    atualizarAngulo();
    // Para com margem: a inercia completa o restante do giro
    if (abs(anguloAcumulado_deg) >= (ANGULO_ALVO - MARGEM_PARADA)) break;
    delayMicroseconds(500);
  }
  motoresParar();

  // Deixa o robo assentar e mede o angulo final real
  delay(300);
  for (int i = 0; i < 50; i++) { atualizarAngulo(); delay(2); }

  sprintf(buf, "Giro concluido! Angulo final: %.1f graus", anguloAcumulado_deg);
  logMsg(String(buf));
}

// --- Menu ---
void mostrarMenu() {
  logMsg("\n===== TESTE GIRO 90 GRAUS (MPU6500) =====");
  logMsg("GIR_D    -> Gira 90 graus pra Direita (gyro)");
  logMsg("GIR_E    -> Gira 90 graus pra Esquerda (gyro)");
  logMsg("ANG      -> Mostra o angulo acumulado atual");
  logMsg("ZERAR    -> Zera o angulo acumulado");
  logMsg("CALIBRAR -> Recalibra o offset do gyro (robo parado)");
  logMsg("STOP     -> Para motores imediatamente");
  logMsg("HELP     -> Mostra este menu");
  logMsg("=========================================\n");
}

void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;
  logMsg(">> Comando: " + cmd);

  if (cmd == "GIR_D") {
    girar90ComGyro(true);
  } else if (cmd == "GIR_E") {
    girar90ComGyro(false);
  } else if (cmd == "ANG") {
    atualizarAngulo();
    char buf[60];
    sprintf(buf, "Angulo acumulado: %.2f graus", anguloAcumulado_deg);
    logMsg(String(buf));
  } else if (cmd == "ZERAR") {
    zerarAngulo();
    logMsg("Angulo zerado.");
  } else if (cmd == "CALIBRAR") {
    calibrarOffsetGiro();
    zerarAngulo();
  } else if (cmd == "STOP") {
    motoresParar();
    logMsg("Parado.");
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
  Wire.setClock(400000);

  // --- Setup Motores ---
  motoresInit();

  // --- Setup MPU6500 ---
  escreverReg(REG_PWR_MGMT_1, 0x00); // acorda o MPU
  delay(100);

  // --- Calibracao do gyro na inicializacao (abordagem do Hilmer) ---
  calibrarOffsetGiro();
  zerarAngulo();

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
}
