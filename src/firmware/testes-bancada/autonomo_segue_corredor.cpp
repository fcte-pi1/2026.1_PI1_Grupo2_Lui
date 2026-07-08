// ==========================================
// AUTONOMO: SEGUE CORREDOR E VIRA NAS ABERTURAS
// ==========================================
// Anda para frente, centralizado no corredor, ate a parede da frente ficar a
// LIMIAR_PAREDE_FRENTE_MM (2 cm). Ai verifica os lados: se o direito ou o
// esquerdo tiver LIMIAR_ABERTURA_LATERAL_MM (10 cm) ou mais de distancia,
// vira pra esse lado (o mais aberto, se os dois abrirem). Sem abertura em
// nenhum lado, da meia-volta (beco sem saida) e continua.
//
// Nao reimplementa nada que ja funciona — so chama:
//   girar_esquerda_90() / girar_direita_90() / girar_180()  -> movimento.cpp
//   centralizar_no_corredor() / centralizar_habilitar()     -> centralizacao.cpp
//   recuperar_centro_labirinto()                             -> movimento.cpp
//   distancia_frente_mm()/esquerda_mm()/direita_mm()         -> distanceSensor.cpp
//
// Comandos via Bluetooth "micromouse" ou Serial USB (115200):
//   INICIAR -> comeca a andar sozinho pelo labirinto
//   PARAR   -> para o robo
//   HELP    -> mostra este menu
// ==========================================
#include <Arduino.h>
#include <Wire.h>
#include "BluetoothSerial.h"
#include "../pins.h"
#include "../motors/motors.h"
#include "../movimento/movimento.h"
#include "../mpu/mpu.h"
#include "../distanceSensor/distanceSensor.h"
#include "../centralizacao/centralizacao.h"

// Mesmo limiar do sistema de emergencia (distanceSensor.h): 20mm = 2cm.
// Usar o mesmo valor garante que, quando pararmos pra virar, o sistema de
// emergencia esteja no mesmo estado que estamos verificando aqui.
#define LIMIAR_PAREDE_FRENTE_MM     LIMITE_EMERGENCIA_TOF_MM

#define LIMIAR_ABERTURA_LATERAL_MM  100   // 10 cm: acima disso, considera "abertura"

// PWM da centralizacao durante o trecho reto. Reduzido em relacao ao padrao
// (VEL_BASE_CENTRALIZADO em centralizacao.h) pra andar com mais cuidado.
// OBS: centralizar_no_corredor() documenta o parametro como "0-100 percentual",
// mas internamente ele e usado direto como PWM (constrain 0-100) — ou seja, e
// sempre um valor baixo de qualquer forma. Ajustar aqui se precisar mais/menos
// velocidade depois de testar no labirinto real.
#define VEL_FRENTE_AUTONOMO         80

BluetoothSerial SerialBT;
String comandoSerial = "";
String comandoBT = "";
bool autonomoAtivo = false;

void logMsg(String msg) {
  Serial.println(msg);
  SerialBT.println(msg);
}

void pararAutonomo() {
  autonomoAtivo = false;
  centralizar_habilitar(false);
  motors_stop_all();
  logMsg("[AUTONOMO] Parado.");
}

void iniciarAutonomo() {
  autonomoAtivo = true;
  atualizar_filtro_media();
  centralizar_habilitar(true); // zera yaw/erros acumulados pro trecho reto
  logMsg("[AUTONOMO] Iniciado! Seguindo o corredor...");
}

// Para nos motores, resolve uma eventual emergencia (recua/afasta da parede
// se preciso — sem isso o proprio giro abortaria na hora, porque
// girar_*_90() tambem verifica a emergencia a cada iteracao) e vira pro lado
// que tiver abertura. Sem abertura em nenhum lado, da meia-volta.
static void decidirEVirar() {
  motors_stop_all();
  centralizar_habilitar(false);

  recuperar_centro_labirinto(); // no-op se nao houver emergencia ativa

  atualizar_filtro_media();
  uint16_t distEsq = distancia_esquerda_mm();
  uint16_t distDir = distancia_direita_mm();
  bool abreEsq = distEsq >= LIMIAR_ABERTURA_LATERAL_MM;
  bool abreDir = distDir >= LIMIAR_ABERTURA_LATERAL_MM;

  char buf[100];
  sprintf(buf, "[AUTONOMO] Frente bloqueada | Esq:%u mm (%s) | Dir:%u mm (%s)",
          distEsq, abreEsq ? "ABERTO" : "parede",
          distDir, abreDir ? "ABERTO" : "parede");
  logMsg(String(buf));

  if (abreDir && (!abreEsq || distDir >= distEsq)) {
    logMsg("[AUTONOMO] Virando a DIREITA.");
    girar_direita_90();
  } else if (abreEsq) {
    logMsg("[AUTONOMO] Virando a ESQUERDA.");
    girar_esquerda_90();
  } else {
    logMsg("[AUTONOMO] Beco sem saida — meia-volta.");
    girar_180();
  }

  // Recalibra a centralizacao (zera yaw e erros acumulados) pro novo trecho reto
  centralizar_habilitar(true);
}

// Chamar em todo loop(): 1 "tick" da navegacao autonoma.
void autonomo_passo() {
  if (!autonomoAtivo) return;

  atualizar_filtro_media();

  if (distancia_frente_mm() <= LIMIAR_PAREDE_FRENTE_MM) {
    decidirEVirar();
    return;
  }

  centralizar_no_corredor(VEL_FRENTE_AUTONOMO);
}

void mostrarMenu() {
  logMsg("\n===== AUTONOMO: SEGUE CORREDOR =====");
  logMsg("INICIAR -> comeca a andar sozinho pelo labirinto");
  logMsg("PARAR   -> para o robo");
  logMsg("HELP    -> mostra este menu");
  logMsg("=====================================\n");
}

void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;
  logMsg(">> Comando: " + cmd);

  if (cmd == "INICIAR" || cmd == "START") {
    iniciarAutonomo();
  } else if (cmd == "PARAR" || cmd == "STOP") {
    pararAutonomo();
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

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  motors_init();
  configurarSensoresToF();

  configurarMPU();
  mpu_calibrar_offset_giro(); // robo tem que estar parado nesse momento

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

  autonomo_passo();
}
