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

// PWM do giro em DUTY DIRETO (0-255). Ajustavel em runtime pelo comando PWM.
// 200 e um bom compromisso entre velocidade e controle; 255 causa overshoot
// excessivo porque a inercia do robo nao freia a tempo.
#define VEL_GIRO_PWM_PADRAO 200 // duty PWM do giro no cruzeiro (0-255)

// --- Parametros da bussola ---
// Abaixo deste rate (em graus/s), consideramos o robo parado e NAO
// integramos. Ajuste para um pouco acima do ruido residual do gyro.
#define NMNI_LIMITE_DPS  1.0f
// Intervalo entre atualizacoes de rumo no modo BUSSOLA (streaming)
#define BUSSOLA_PERIODO_MS 200

// --- Parametros do giro PROPORCIONAL (3 fases) ---
#define PASSO_GIRO_GRAUS      90.0f  // cada GIR_D/GIR_E soma/subtrai isso no alvo
#define MARGEM_PARADA_GRAUS    1.5f  // margem final: abaixo disso = no alvo (graus)
#define TIMEOUT_GIRO_MS       5000   // seguranca

// Fase 1 — Desaceleracao proporcional por zonas:
// O PWM cai progressivamente conforme se aproxima do alvo.
// Zona 1: >40 graus faltando -> PWM cheio (cruzeiro)
// Zona 2: 40~20 graus        -> 60% do PWM
// Zona 3: 20~8 graus         -> 40% do PWM
// Zona 4: 8~CORTE graus      -> 25% do PWM (velocidade minima controlavel)
#define ZONA1_GRAUS           40.0f  // acima disso: cruzeiro (100%)
#define ZONA2_GRAUS           20.0f  // acima disso: 60%
#define ZONA3_GRAUS            8.0f  // acima disso: 40%
// Abaixo de ZONA3 ate o corte: 25%

// Corte antecipado: para o motor ANTES do alvo. A inercia carrega o robo
// os graus restantes. Ajuste este valor experimentalmente:
// - Se o robo PARA ANTES do alvo: diminua.
// - Se o robo PASSA DO alvo: aumente.
#define CORTE_ANTECIPADO_GRAUS 3.0f  // graus antes do alvo p/ cortar o motor

// Fase 2 — Correcao de overshoot (malha fechada proporcional):
// PWM proporcional ao erro: quanto mais longe, mais forte o pulso.
// Formula: pwm = max(PWM_MIN, min(PWM_MAX, erro * GANHO))
#define CORRECAO_PWM_MIN       80   // duty minimo pra vencer a friccao estatica
#define CORRECAO_PWM_MAX      150   // duty maximo da correcao (nao quer overshoot)
#define CORRECAO_GANHO        12.0f // ganho proporcional: pwm = erro_graus * GANHO
#define ASSENTAMENTO_MS       200   // tempo parado p/ a inercia terminar antes de medir
#define PULSO_CORRECAO_MS     100   // duracao maxima de cada pulso de correcao
#define MAX_PULSOS_CORRECAO    10   // seguranca: no. maximo de pulsos

// --- Variaveis Globais ---
BluetoothSerial SerialBT;
String comandoBT = "";
String comandoSerial = "";

// --- Estado da bussola ---
float offsetGiroZ_dps = 0.0f;          // bias do gyro em graus/s
float rumo_deg = 0.0f;                  // heading atual, normalizado 0-360
float alvoRumo_deg = 0.0f;              // alvo ACUMULADO do giro (grade de 90), 0-360
int   pwmGiro = VEL_GIRO_PWM_PADRAO;    // duty PWM do giro (0-255), ajustavel via PWM
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

// --- Motores (logica copiada de motors.cpp, sem include) ---
void motorSet(uint8_t chFwd, uint8_t chBwd, int pwm) {
  // pwm em DUTY DIRETO: -255..255 (sinal = sentido, modulo = duty)
  pwm = constrain(pwm, -255, 255);

  if (pwm > 0) {
    ledcWrite(chFwd, pwm);
    ledcWrite(chBwd, 0);
  } else if (pwm < 0) {
    ledcWrite(chFwd, 0);
    ledcWrite(chBwd, -pwm);
  } else {
    ledcWrite(chFwd, 0);
    ledcWrite(chBwd, 0);
  }
}

void motorEsquerdoSet(int pwm) {
  // Invertido via software: polaridade dos fios trocada na placa
  motorSet(CH_MOT1_IN1, CH_MOT1_IN2, -pwm);
}

void motorDireitoSet(int pwm) {
  // Invertido via software: polaridade dos fios trocada na placa
  motorSet(CH_MOT2_IN1, CH_MOT2_IN2, -pwm);
}

void motoresParar() {
  // Brake ativo: ambos os pinos em HIGH -> DRV8833 trava o motor
  // (igual ao motors_stop_all usado pelo movimento.cpp)
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
  alvoRumo_deg = 0.0f;   // zera tambem o alvo acumulado do giro
  ultimaAtualizacao_us = micros();
  logMsg("Norte definido: rumo atual = 0 graus.");
}

void mostrarRumo() {
  atualizarBussola();
  char buf[64];
  sprintf(buf, "Rumo: %.1f graus (%s)", rumo_deg, direcaoCardeal(rumo_deg).c_str());
  logMsg(String(buf));
}

// --- Quanto ainda falta girar (graus) ate o alvo, no sentido do giro -------
// paraDireita = true: nesta placa o rumo DIMINUI (horario). A "falta" comeca
// perto do passo (ex.: 90) e cai ate 0 conforme o rumo se aproxima do alvo.
float faltaParaAlvo(float alvo, bool paraDireita) {
  if (paraDireita) return normalizar360(rumo_deg - alvo);
  else             return normalizar360(alvo - rumo_deg);
}

// --- Erro SINALIZADO ate o alvo, na faixa (-180, 180] --------------------
// erro > 0: falta AUMENTAR o rumo (girar pra ESQUERDA nesta placa).
// erro < 0: passou do ponto, precisa DIMINUIR o rumo (girar pra DIREITA).
// Usado no ajuste fino: nao depende do sentido do giro original, so da posicao.
float erroParaAlvo(float alvo) {
  float e = normalizar360(alvo - rumo_deg);
  if (e > 180.0f) e -= 360.0f;
  return e;
}

// --- Espera a inercia terminar (robo assentar) mantendo o rumo atualizado --
void assentarInercia(unsigned long ms) {
  motoresParar();
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    atualizarBussola();
    delay(2);
  }
}

// --- Calcula o PWM proporcional a distancia que falta (Fase 1) ---------------
// Retorna um valor de PWM entre ~25% e 100% do pwmGiro, conforme a zona.
int pwmProporcional(float falta) {
  if (falta > ZONA1_GRAUS)  return pwmGiro;                        // 100% cruzeiro
  if (falta > ZONA2_GRAUS)  return (int)(pwmGiro * 0.60f);         // 60%
  if (falta > ZONA3_GRAUS)  return (int)(pwmGiro * 0.40f);         // 40%
  return (int)(pwmGiro * 0.25f);                                   // 25% minimo
}

// --- Gira ate a bussola bater no ALVO ABSOLUTO acumulado (alvoRumo_deg) -----
// CONTROLE PROPORCIONAL EM 3 FASES:
//
// Fase 1 (Cruzeiro + Desaceleracao proporcional):
//   O PWM diminui conforme se aproxima do alvo, em 4 zonas. Isso reduz a
//   energia cinetica gradualmente, minimizando o overshoot.
//
// Fase 1b (Corte antecipado por inercia):
//   O motor e cortado CORTE_ANTECIPADO_GRAUS ANTES do alvo. A inercia do
//   robo carrega os graus restantes. Assim a parada cai perto do alvo.
//
// Fase 2 (Ajuste fino proporcional):
//   Apos assentar, mede o erro real. Se ainda estiver fora da margem,
//   aplica pulsos curtos com PWM PROPORCIONAL ao erro (quanto menor o
//   erro, menor a forca), evitando oscilacao.
void girarParaAlvo(float alvo, bool paraDireita) {
  int sinal_esq = paraDireita ? +1 : -1;   // mesmos sentidos do movimento.cpp
  int sinal_dir = paraDireita ? -1 : +1;

  unsigned long inicio = millis();
  int pwmAtual = pwmGiro;

  motorEsquerdoSet(sinal_esq * pwmAtual);
  motorDireitoSet(sinal_dir * pwmAtual);

  // ══════════════════════════════════════════════════════════════════════
  // FASE 1: Desaceleracao proporcional por zonas + corte antecipado
  // ══════════════════════════════════════════════════════════════════════
  while (true) {
    atualizarBussola();
    float falta = faltaParaAlvo(alvo, paraDireita);

    // Corte antecipado: para o motor ANTES do alvo pra inercia completar
    if (falta <= CORTE_ANTECIPADO_GRAUS) break;
    // Passou do ponto: a falta "da a volta" (>180) -> para imediatamente
    if (falta > 180.0f) break;
    if (millis() - inicio > TIMEOUT_GIRO_MS) {
      logMsg("[GIRO] TIMEOUT na Fase 1!");
      break;
    }

    // Atualiza o PWM proporcionalmente a distancia restante
    int novoPwm = pwmProporcional(falta);
    if (novoPwm != pwmAtual) {
      pwmAtual = novoPwm;
      motorEsquerdoSet(sinal_esq * pwmAtual);
      motorDireitoSet(sinal_dir * pwmAtual);
    }
    delay(2);
  }
  motoresParar();

  // ══════════════════════════════════════════════════════════════════════
  // FASE 2: Correcao de overshoot (malha fechada PROPORCIONAL)
  // ══════════════════════════════════════════════════════════════════════
  // Apos a freada, a inercia pode ter jogado o robo alguns graus alem (ou
  // aquem). Esperamos assentar, medimos o erro real e corrigimos com
  // pulsos curtos e PWM proporcional ao erro.
  for (int pulso = 0; pulso < MAX_PULSOS_CORRECAO; pulso++) {
    assentarInercia(ASSENTAMENTO_MS);       // espera parar de vez e atualiza o rumo

    float erro = erroParaAlvo(alvo);
    float erroAbs = fabsf(erro);
    if (erroAbs <= MARGEM_PARADA_GRAUS) break;  // ja esta no alvo!

    // PWM proporcional ao erro: pouco erro -> pouca forca
    int pwmCorrecao = (int)(erroAbs * CORRECAO_GANHO);
    pwmCorrecao = constrain(pwmCorrecao, CORRECAO_PWM_MIN, CORRECAO_PWM_MAX);

    // Gira no sentido que REDUZ o erro (independe do sentido do giro original)
    bool corrigirEsq = (erro > 0.0f);       // erro>0 -> aumentar rumo -> esquerda
    int se = corrigirEsq ? -1 : +1;
    int sd = corrigirEsq ? +1 : -1;
    motorEsquerdoSet(se * pwmCorrecao);
    motorDireitoSet(sd * pwmCorrecao);

    // Pulso curto: corta assim que entrar na margem pra nao passar de novo
    unsigned long tPulso = millis();
    while (millis() - tPulso < PULSO_CORRECAO_MS) {
      atualizarBussola();
      if (fabsf(erroParaAlvo(alvo)) <= MARGEM_PARADA_GRAUS) break;
      delay(2);
    }
    motoresParar();
  }
  assentarInercia(ASSENTAMENTO_MS);         // assenta e mede o resultado final

  char buf[100];
  float erroFinal = erroParaAlvo(alvo);
  sprintf(buf, "Giro OK! Alvo:%.0f | Rumo:%.1f | Erro:%.1f | %s",
          alvo, rumo_deg, erroFinal,
          (fabsf(erroFinal) <= MARGEM_PARADA_GRAUS) ? "PRECISO" : "FORA DA MARGEM");
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
  logMsg("\n========= BUSSOLA (GYRO MPU6500) =========");
  logMsg("MENU DE COMANDOS:");
  logMsg("NORTE    -> Define o rumo atual como Norte (zera)");
  logMsg("RUMO     -> Mostra o rumo atual (0-360) e o ponto cardeal");
  logMsg("GIR_D    -> Gira 90 graus pra Direita (mede pela bussola)");
  logMsg("GIR_E    -> Gira 90 graus pra Esquerda (mede pela bussola)");
  logMsg("PWM n    -> Ajusta o PWM do giro (0-255). Ex.: PWM 200");
  logMsg("PWM      -> Mostra o PWM do giro atual");
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
  } else if (cmd == "GIR_D") {
    logMsg("Girando 90 graus pra DIREITA...");
    girarDireita90();
  } else if (cmd == "GIR_E") {
    logMsg("Girando 90 graus pra ESQUERDA...");
    girarEsquerda90();
  } else if (cmd == "PWM" || cmd.startsWith("PWM ") ||
             cmd == "VEL" || cmd.startsWith("VEL ")) {   // VEL: alias antigo
    char buf[48];
    if (cmd == "PWM" || cmd == "VEL") {
      // sem argumento: so mostra o PWM atual
      sprintf(buf, "PWM do giro: %d (0-255)", pwmGiro);
      logMsg(String(buf));
    } else {
      int v = cmd.substring(4).toInt();          // texto depois de "PWM " / "VEL "
      pwmGiro = constrain(v, 0, 255);            // duty PWM direto
      sprintf(buf, "PWM do giro ajustado para %d (0-255)", pwmGiro);
      logMsg(String(buf));
    }
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

  // --- Setup Motores (ficam parados ate um comando de giro) ---
  motoresInit();

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
