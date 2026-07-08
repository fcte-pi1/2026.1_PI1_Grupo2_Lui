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

// =====================================================================
// PARAMETROS DE TUNAGEM — todos ajustaveis em runtime via Bluetooth!
// Use os comandos CORTE, GANHO, PMIN, PMAX, MARGEM, ASSENT pra mudar.
// Use PARAMS pra ver os valores atuais. Use BENCH pra testar.
// =====================================================================

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

// --- Parametros de tunagem (ajustaveis em runtime) ---
// Corte antecipado: para o motor ANTES do alvo. A inercia carrega o resto.
// Se o robo PARA ANTES do alvo: diminua. Se PASSA DO alvo: aumente.
float corteAntecipadoGraus = 3.0f;

// Correcao proporcional: PWM = clamp(erro * ganho, pMin, pMax)
int   correcaoPwmMin  = 80;    // duty minimo pra vencer friccao estatica
int   correcaoPwmMax  = 150;   // duty maximo da correcao
float correcaoGanho   = 12.0f; // ganho proporcional

// Margem de precisao: abaixo disso, consideramos "no alvo"
float margemParadaGraus = 1.5f;

// Tempo de assentamento (ms) — espera a inercia terminar antes de medir
unsigned long assentamentoMs = 200;

// Pulso de correcao: duracao maxima de cada micro-pulso
unsigned long pulsoCorrecaoMs = 100;

// Numero maximo de pulsos de correcao (seguranca)
int maxPulsosCorrecao = 10;

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
void assentarInerciaDefault() { assentarInercia(assentamentoMs); }

// --- Calcula o PWM proporcional a distancia que falta (Fase 1) ---------------
// Retorna um valor de PWM entre ~25% e 100% do pwmGiro, conforme a zona.
int pwmProporcional(float falta) {
  if (falta > ZONA1_GRAUS)  return pwmGiro;                        // 100% cruzeiro
  if (falta > ZONA2_GRAUS)  return (int)(pwmGiro * 0.60f);         // 60%
  if (falta > ZONA3_GRAUS)  return (int)(pwmGiro * 0.40f);         // 40%
  return (int)(pwmGiro * 0.25f);                                   // 25% minimo
}

// --- Mostra todos os parametros de tunagem atuais ---
void mostrarParams() {
  char buf[80];
  logMsg("\n--- PARAMETROS DE TUNAGEM ATUAIS ---");
  sprintf(buf, "PWM cruzeiro:   %d (0-255)", pwmGiro);       logMsg(buf);
  sprintf(buf, "CORTE antecip:  %.1f graus", corteAntecipadoGraus); logMsg(buf);
  sprintf(buf, "MARGEM parada:  %.1f graus", margemParadaGraus);    logMsg(buf);
  sprintf(buf, "GANHO correcao: %.1f", correcaoGanho);              logMsg(buf);
  sprintf(buf, "PMIN correcao:  %d", correcaoPwmMin);               logMsg(buf);
  sprintf(buf, "PMAX correcao:  %d", correcaoPwmMax);               logMsg(buf);
  sprintf(buf, "ASSENT (ms):    %lu", assentamentoMs);              logMsg(buf);
  sprintf(buf, "PULSO (ms):     %lu", pulsoCorrecaoMs);             logMsg(buf);
  sprintf(buf, "MAX PULSOS:     %d", maxPulsosCorrecao);            logMsg(buf);
  logMsg("------------------------------------\n");
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
    if (falta <= corteAntecipadoGraus) break;
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
  for (int pulso = 0; pulso < maxPulsosCorrecao; pulso++) {
    assentarInerciaDefault();

    float erro = erroParaAlvo(alvo);
    float erroAbs = fabsf(erro);
    if (erroAbs <= margemParadaGraus) break;

    // PWM proporcional ao erro: pouco erro -> pouca forca
    int pwmCorrecao = (int)(erroAbs * correcaoGanho);
    pwmCorrecao = constrain(pwmCorrecao, correcaoPwmMin, correcaoPwmMax);

    bool corrigirEsq = (erro > 0.0f);
    int se = corrigirEsq ? -1 : +1;
    int sd = corrigirEsq ? +1 : -1;
    motorEsquerdoSet(se * pwmCorrecao);
    motorDireitoSet(sd * pwmCorrecao);

    unsigned long tPulso = millis();
    while (millis() - tPulso < pulsoCorrecaoMs) {
      atualizarBussola();
      if (fabsf(erroParaAlvo(alvo)) <= margemParadaGraus) break;
      delay(2);
    }
    motoresParar();
  }
  assentarInerciaDefault();

  char buf[100];
  float erroFinal = erroParaAlvo(alvo);
  sprintf(buf, "Giro OK! Alvo:%.0f | Rumo:%.1f | Erro:%.1f | %s",
          alvo, rumo_deg, erroFinal,
          (fabsf(erroFinal) <= margemParadaGraus) ? "PRECISO" : "FORA DA MARGEM");
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

// ==========================================================================
// BENCH: Teste de bancada automatizado
// Faz N giros de 90 graus (padrao: 4 = volta completa 360), coleta o erro
// de cada giro, e mostra estatisticas (media, maximo, desvio padrao).
// Uso: BENCH        -> 4 giros pra direita (padrao)
//      BENCH 8      -> 8 giros pra direita (2 voltas)
//      BENCH 4 E    -> 4 giros pra esquerda
//      BENCH 6 D    -> 6 giros pra direita
// ==========================================================================
void executarBench(int nGiros, bool paraDireita) {
  char buf[120];
  sprintf(buf, "\n=== BENCH: %d giros de 90 pra %s ===",
          nGiros, paraDireita ? "DIREITA" : "ESQUERDA");
  logMsg(String(buf));
  mostrarParams();

  // Zera a referencia antes do teste
  definirNorte();
  delay(500);

  float erros[20];  // max 20 giros por bench
  nGiros = constrain(nGiros, 1, 20);

  float somaErro = 0.0f;
  float somaErro2 = 0.0f;
  float erroMax = 0.0f;
  int   precisos = 0;

  for (int i = 0; i < nGiros; i++) {
    sprintf(buf, "\n--- Giro %d/%d ---", i + 1, nGiros);
    logMsg(String(buf));

    float alvoAntes = alvoRumo_deg;
    if (paraDireita) {
      girarDireita90();
    } else {
      girarEsquerda90();
    }

    float erro = fabsf(erroParaAlvo(alvoRumo_deg));
    erros[i] = erro;
    somaErro += erro;
    somaErro2 += erro * erro;
    if (erro > erroMax) erroMax = erro;
    if (erro <= margemParadaGraus) precisos++;

    // Pausa entre giros pra inercia assentar de vez
    delay(800);
  }

  // --- Estatisticas ---
  float media = somaErro / nGiros;
  float variancia = (somaErro2 / nGiros) - (media * media);
  float desvio = (variancia > 0.0f) ? sqrtf(variancia) : 0.0f;

  logMsg("\n========== RESULTADO DO BENCH ==========");
  logMsg("Giro | Alvo esperado | Erro (graus)");
  logMsg("-----|--------------|-------------");
  for (int i = 0; i < nGiros; i++) {
    float alvoEsp;
    if (paraDireita) {
      alvoEsp = normalizar360(-PASSO_GIRO_GRAUS * (i + 1));
    } else {
      alvoEsp = normalizar360(PASSO_GIRO_GRAUS * (i + 1));
    }
    sprintf(buf, "  %2d |    %5.0f      |   %5.1f  %s",
            i + 1, alvoEsp, erros[i],
            (erros[i] <= margemParadaGraus) ? "OK" : "FORA");
    logMsg(String(buf));
  }
  logMsg("----------------------------------------");
  sprintf(buf, "Media:  %.2f graus", media);    logMsg(buf);
  sprintf(buf, "Maximo: %.2f graus", erroMax);  logMsg(buf);
  sprintf(buf, "Desvio: %.2f graus", desvio);   logMsg(buf);
  sprintf(buf, "Precisos: %d/%d (margem: %.1f)", precisos, nGiros, margemParadaGraus);
  logMsg(String(buf));

  sprintf(buf, "Rumo final: %.1f (esperado: %.1f)",
          rumo_deg, alvoRumo_deg);
  logMsg(String(buf));
  logMsg("========================================\n");

  // Dica automatica baseada nos resultados
  if (media > 3.0f) {
    logMsg("[DICA] Erro medio alto. Tente: CORTE + alguns graus.");
  } else if (media > margemParadaGraus && desvio < 1.5f) {
    logMsg("[DICA] Erro consistente. Ajuste CORTE (erro sistematico).");
  } else if (desvio > 2.0f) {
    logMsg("[DICA] Erro muito variavel. Reduza PWM ou aumente GANHO.");
  } else {
    logMsg("[DICA] Resultados bons! Tunagem OK.");
  }
}

// --- Funcao auxiliar: extrai float de um comando (ex: "CORTE 4.5" -> 4.5) ---
float extrairFloat(String cmd, int posEspaco) {
  return cmd.substring(posEspaco + 1).toFloat();
}
int extrairInt(String cmd, int posEspaco) {
  return cmd.substring(posEspaco + 1).toInt();
}

// --- Menu ---
void mostrarMenu() {
  logMsg("\n========= BUSSOLA (GYRO MPU6500) =========");
  logMsg("--- COMANDOS BASICOS ---");
  logMsg("NORTE    -> Define o rumo atual como Norte (zera)");
  logMsg("RUMO     -> Mostra o rumo atual (0-360)");
  logMsg("GIR_D    -> Gira 90 pra Direita");
  logMsg("GIR_E    -> Gira 90 pra Esquerda");
  logMsg("BUS_ON   -> Streaming continuo do rumo");
  logMsg("BUS_OFF  -> Desliga streaming");
  logMsg("CALIBRAR -> Recalibra offset do gyro");
  logMsg("");
  logMsg("--- BANCADA (TUNAGEM) ---");
  logMsg("BENCH      -> 4 giros D (volta completa)");
  logMsg("BENCH n    -> n giros pra Direita");
  logMsg("BENCH n E  -> n giros pra Esquerda");
  logMsg("PARAMS     -> Mostra parametros atuais");
  logMsg("PWM n      -> PWM cruzeiro (0-255)");
  logMsg("CORTE n    -> Corte antecipado (graus)");
  logMsg("MARGEM n   -> Margem de precisao (graus)");
  logMsg("GANHO n    -> Ganho proporcional correcao");
  logMsg("PMIN n     -> PWM minimo da correcao");
  logMsg("PMAX n     -> PWM maximo da correcao");
  logMsg("ASSENT n   -> Tempo assentamento (ms)");
  logMsg("PULSO n    -> Duracao pulso correcao (ms)");
  logMsg("HELP       -> Este menu");
  logMsg("==========================================\n");
}

void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;
  logMsg(">> Comando: " + cmd);

  char buf[80];

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

  // --- BENCH ---
  } else if (cmd == "BENCH" || cmd.startsWith("BENCH ")) {
    int n = 4;
    bool dir = true;
    if (cmd.startsWith("BENCH ")) {
      String args = cmd.substring(6);
      args.trim();
      // BENCH n ou BENCH n E/D
      int espaco = args.indexOf(' ');
      if (espaco > 0) {
        n = args.substring(0, espaco).toInt();
        String sentido = args.substring(espaco + 1);
        sentido.trim();
        if (sentido == "E") dir = false;
      } else {
        n = args.toInt();
      }
      if (n <= 0) n = 4;
    }
    executarBench(n, dir);

  // --- PARAMS ---
  } else if (cmd == "PARAMS") {
    mostrarParams();

  // --- PWM ---
  } else if (cmd == "PWM" || cmd.startsWith("PWM ")) {
    if (cmd == "PWM") {
      sprintf(buf, "PWM do giro: %d (0-255)", pwmGiro);
      logMsg(String(buf));
    } else {
      pwmGiro = constrain(extrairInt(cmd, 3), 0, 255);
      sprintf(buf, "PWM -> %d", pwmGiro); logMsg(buf);
    }

  // --- CORTE ---
  } else if (cmd.startsWith("CORTE ")) {
    corteAntecipadoGraus = constrain(extrairFloat(cmd, 5), 0.0f, 30.0f);
    sprintf(buf, "CORTE -> %.1f graus", corteAntecipadoGraus); logMsg(buf);
  } else if (cmd == "CORTE") {
    sprintf(buf, "CORTE: %.1f graus", corteAntecipadoGraus); logMsg(buf);

  // --- MARGEM ---
  } else if (cmd.startsWith("MARGEM ")) {
    margemParadaGraus = constrain(extrairFloat(cmd, 6), 0.1f, 10.0f);
    sprintf(buf, "MARGEM -> %.1f graus", margemParadaGraus); logMsg(buf);
  } else if (cmd == "MARGEM") {
    sprintf(buf, "MARGEM: %.1f graus", margemParadaGraus); logMsg(buf);

  // --- GANHO ---
  } else if (cmd.startsWith("GANHO ")) {
    correcaoGanho = constrain(extrairFloat(cmd, 5), 1.0f, 50.0f);
    sprintf(buf, "GANHO -> %.1f", correcaoGanho); logMsg(buf);
  } else if (cmd == "GANHO") {
    sprintf(buf, "GANHO: %.1f", correcaoGanho); logMsg(buf);

  // --- PMIN ---
  } else if (cmd.startsWith("PMIN ")) {
    correcaoPwmMin = constrain(extrairInt(cmd, 4), 30, 255);
    sprintf(buf, "PMIN -> %d", correcaoPwmMin); logMsg(buf);
  } else if (cmd == "PMIN") {
    sprintf(buf, "PMIN: %d", correcaoPwmMin); logMsg(buf);

  // --- PMAX ---
  } else if (cmd.startsWith("PMAX ")) {
    correcaoPwmMax = constrain(extrairInt(cmd, 4), 50, 255);
    sprintf(buf, "PMAX -> %d", correcaoPwmMax); logMsg(buf);
  } else if (cmd == "PMAX") {
    sprintf(buf, "PMAX: %d", correcaoPwmMax); logMsg(buf);

  // --- ASSENT ---
  } else if (cmd.startsWith("ASSENT ")) {
    assentamentoMs = constrain((unsigned long)extrairInt(cmd, 6), 50UL, 2000UL);
    sprintf(buf, "ASSENT -> %lu ms", assentamentoMs); logMsg(buf);
  } else if (cmd == "ASSENT") {
    sprintf(buf, "ASSENT: %lu ms", assentamentoMs); logMsg(buf);

  // --- PULSO ---
  } else if (cmd.startsWith("PULSO ")) {
    pulsoCorrecaoMs = constrain((unsigned long)extrairInt(cmd, 5), 10UL, 500UL);
    sprintf(buf, "PULSO -> %lu ms", pulsoCorrecaoMs); logMsg(buf);
  } else if (cmd == "PULSO") {
    sprintf(buf, "PULSO: %lu ms", pulsoCorrecaoMs); logMsg(buf);

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
