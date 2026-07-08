// ==========================================
// BUSSOLA (HEADING RELATIVO) COM MPU6500 (GYRO)
// Base: teste_giro_90_mpu.cpp
//
// IMPORTANTE: o MPU6500 e um sensor de 6 eixos (gyro + acel), SEM
// magnetometro. Ele NAO consegue apontar pro norte magnetico real.
// Esta "bussola" e RELATIVA: voce define um norte (comando NORTE) e o
// rumo (0-360) e integrado a partir dali usando so o eixo Z do gyro.
//
// Tecnicas para segurar o drift (erro que acumula com o tempo):
//   1. Calibracao do bias do gyro com o robo parado.
//   2. NMNI ("No Motion No Integration"): quando o robo esta praticamente
//      parado, nao integra -> corta a maior fonte de drift em repouso.
// Mesmo assim, espere alguns graus de erro apos varios minutos.
//
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

// --- Parametros da bussola ---
// Abaixo deste rate (em graus/s), consideramos o robo parado e NAO
// integramos. Ajuste para um pouco acima do ruido residual do gyro.
#define NMNI_LIMITE_DPS  1.0f
// Intervalo entre atualizacoes de rumo no modo BUSSOLA (streaming)
#define BUSSOLA_PERIODO_MS 200

// --- Variaveis Globais ---
BluetoothSerial SerialBT;
String comandoBT = "";
String comandoSerial = "";

// --- Estado da bussola ---
float offsetGiroZ_dps = 0.0f;          // bias do gyro em graus/s
float rumo_deg = 0.0f;                  // heading atual, normalizado 0-360
unsigned long ultimaAtualizacao_us = 0;
bool bussolaStreaming = false;          // modo BUSSOLA ligado?

// --- Utilitarias ---
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

// --- Calibracao do bias (robo parado) ---
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

// --- Normaliza um angulo para a faixa [0, 360) ---
float normalizar360(float ang) {
  ang = fmodf(ang, 360.0f);
  if (ang < 0.0f) ang += 360.0f;
  return ang;
}

// --- Rumo -> ponto cardeal (8 direcoes) ---
// Convencao: rumo cresce no sentido positivo do gyro Z.
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
  ultimaAtualizacao_us = micros();
  logMsg("Norte definido: rumo atual = 0 graus.");
}

void mostrarRumo() {
  atualizarBussola();
  char buf[64];
  sprintf(buf, "Rumo: %.1f graus (%s)", rumo_deg, direcaoCardeal(rumo_deg).c_str());
  logMsg(String(buf));
}

// --- Menu ---
void mostrarMenu() {
  logMsg("\n========= BUSSOLA (GYRO MPU6500) =========");
  logMsg("MENU DE COMANDOS:");
  logMsg("NORTE    -> Define o rumo atual como Norte (zera)");
  logMsg("RUMO     -> Mostra o rumo atual (0-360) e o ponto cardeal");
  logMsg("BUS_ON   -> Liga o streaming continuo do rumo (200ms)");
  logMsg("BUS_OFF  -> Desliga o streaming continuo do rumo");
  logMsg("CALIBRAR -> Recalibra o offset do gyro (robo parado)");
  logMsg("HELP     -> Mostra este menu");
  logMsg("Obs: heading e RELATIVO (sem magnetometro) e sofre drift.");
  logMsg("==========================================\n");
}

void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;
  logMsg(">> Comando: " + cmd);

  if (cmd == "NORTE") {
    definirNorte();
  } else if (cmd == "RUMO") {
    mostrarRumo();
  } else if (cmd == "BUS_ON") {
    bussolaStreaming = true;
    logMsg("Streaming da bussola LIGADO.");
  } else if (cmd == "BUS_OFF") {
    bussolaStreaming = false;
    logMsg("Streaming da bussola DESLIGADO.");
  } else if (cmd == "CALIBRAR") {
    calibrarOffsetGiro();
    definirNorte();
  } else if (cmd == "HELP" || cmd == "?") {
    mostrarMenu();
  } else {
    logMsg("Comando desconhecido. Digite HELP.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!SerialBT.begin("micromouse")) {
    Serial.println("ERRO BT");
  }

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // --- Setup MPU6500 ---
  escreverReg(REG_PWR_MGMT_1, 0x00); // acorda o MPU
  delay(100);

  calibrarOffsetGiro();
  definirNorte();

  logMsg("\n--- INICIALIZACAO COMPLETA ---");
  mostrarMenu();
}

void loop() {
  // Integra o rumo continuamente (mesmo sem streaming), pra nao perder giro
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

  // Streaming continuo do rumo (modo BUSSOLA)
  static unsigned long ultimoPrint = 0;
  if (bussolaStreaming && (millis() - ultimoPrint >= BUSSOLA_PERIODO_MS)) {
    ultimoPrint = millis();
    char buf[64];
    sprintf(buf, "Rumo: %.1f graus (%s)", rumo_deg, direcaoCardeal(rumo_deg).c_str());
    logMsg(String(buf));
  }
}
