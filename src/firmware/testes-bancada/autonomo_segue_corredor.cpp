// ==========================================
// AUTONOMO: SEGUE CORREDOR E VIRA NAS ABERTURAS
// ==========================================
// Anda para frente em velocidade fixa (o robo ja comeca centralizado, entao
// nao precisa corrigir durante o trecho reto) ate a parede da frente ficar a
// LIMIAR_PAREDE_FRENTE_MM (2,5 cm). Ai verifica os lados: se o direito ou o
// esquerdo tiver LIMIAR_ABERTURA_LATERAL_MM (10 cm) ou mais de distancia,
// vira pra esse lado (o mais aberto, se os dois abrirem). Sem abertura em
// nenhum lado, da meia-volta (beco sem saida). So DEPOIS de virar e que vale
// a pena centralizar (recalibra a posicao pro novo trecho reto).
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
//   STATUS  -> mostra as distancias atuais dos 3 sensores (sem se mover)
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

#define LIMIAR_PAREDE_FRENTE_MM     25    // 2,5 cm: para de andar reto e avalia o giro
#define LIMIAR_ABERTURA_LATERAL_MM  100   // 10 cm: acima disso, considera "abertura"

// PWM do trecho reto (escala 0-255, igual VEL_PADRAO/VEL_GIRO em movimento.h).
// Um pouco abaixo de VEL_PADRAO (150) pra andar com mais cuidado.
#define VEL_FRENTE_AUTONOMO         130

// Depois de virar, roda a centralizacao (PD com os sensores laterais e o
// giroscopio) por esse tempo, soh pra corrigir a entrada no novo corredor —
// durante o resto do trecho reto anda em velocidade fixa (o robo ja comeca
// centralizado, entao nao precisa ficar corrigindo o tempo todo).
#define DURACAO_CENTRALIZACAO_POS_GIRO_MS  400

#define MPU6500_ADDR 0x68

BluetoothSerial SerialBT;
String comandoSerial = "";
String comandoBT = "";
bool autonomoAtivo = false;
bool mpuOk = false;

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
  if (!mpuOk) {
    logMsg("[AUTONOMO] ERRO: MPU-6500 nao respondeu no boot — sem giroscopio nao");
    logMsg("           da pra girar nem centralizar com seguranca. Verifique a");
    logMsg("           fiacao do MPU (I2C 0x68) e reinicie a ESP32.");
    return;
  }
  autonomoAtivo = true;
  atualizar_filtro_media();
  logMsg("[AUTONOMO] Iniciado! Seguindo o corredor...");
}

// Anda reto em velocidade fixa — sem correcao, o robo ja comeca centralizado.
static void andarReto() {
  motor_esquerdo_set(VEL_FRENTE_AUTONOMO);
  motor_direito_set(VEL_FRENTE_AUTONOMO);
}

// Roda a centralizacao (PD com sensores laterais + giroscopio) por um tempo
// curto, so pra corrigir a entrada no corredor novo depois de um giro.
static void centralizarBreve() {
  centralizar_habilitar(true); // zera yaw/erros acumulados
  unsigned long inicio = millis();
  while (millis() - inicio < DURACAO_CENTRALIZACAO_POS_GIRO_MS) {
    atualizar_filtro_media();
    centralizar_no_corredor(VEL_FRENTE_AUTONOMO);
    delay(20);
  }
  centralizar_habilitar(false);
}

// Para nos motores, resolve uma eventual emergencia (recua/afasta da parede
// se preciso — sem isso o proprio giro abortaria na hora, porque
// girar_*_90() tambem verifica a emergencia a cada iteracao) e vira pro lado
// que tiver abertura. Sem abertura em nenhum lado, da meia-volta. Depois de
// virar, centraliza por um instante antes de voltar a andar reto.
static void decidirEVirar() {
  motors_stop_all();

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

  centralizarBreve();
}

// Chamar em todo loop(): 1 "tick" da navegacao autonoma.
void autonomo_passo() {
  if (!autonomoAtivo) return;

  atualizar_filtro_media();

  if (distancia_frente_mm() <= LIMIAR_PAREDE_FRENTE_MM) {
    decidirEVirar();
    return;
  }

  andarReto();
}

// Mostra as distancias atuais dos 3 sensores, sem mover o robo. Util pra
// checar se os sensores estao respondendo antes de mandar INICIAR.
void mostrarStatus() {
  atualizar_filtro_media();
  char buf[110];
  sprintf(buf, "[STATUS] Frente:%u mm | Esq:%u mm | Dir:%u mm | Autonomo:%s | Emergencia:%s",
          distancia_frente_mm(), distancia_esquerda_mm(), distancia_direita_mm(),
          autonomoAtivo ? "ATIVO" : "parado",
          emergencia_ativa() ? "SIM" : "nao");
  logMsg(String(buf));
}

void mostrarMenu() {
  logMsg("\n===== AUTONOMO: SEGUE CORREDOR =====");
  logMsg("INICIAR -> comeca a andar sozinho pelo labirinto");
  logMsg("PARAR   -> para o robo");
  logMsg("STATUS  -> mostra as distancias atuais dos sensores (sem se mover)");
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
  } else if (cmd == "STATUS") {
    mostrarStatus();
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

  // Checagem rapida (nao-bloqueante) antes de chamar configurarMPU(): essa
  // funcao trava o firmware pra sempre (while(1)) se nao achar o chip — bom
  // pro robo real (nao anda sem giroscopio), ruim pra depurar por Bluetooth,
  // porque o setup() nunca chegaria no loop() e HELP/STATUS nunca responderiam.
  // Aqui, se nao achar, so avisamos e seguimos: HELP/STATUS continuam
  // funcionando, e INICIAR fica bloqueado ate o MPU ser reconectado.
  Wire.beginTransmission(MPU6500_ADDR);
  mpuOk = (Wire.endTransmission() == 0);

  if (mpuOk) {
    configurarMPU();
    mpu_calibrar_offset_giro(); // robo tem que estar parado nesse momento
  } else {
    Serial.println("[AUTONOMO] AVISO: MPU-6500 nao respondeu no I2C (0x68).");
    Serial.println("[AUTONOMO] HELP/STATUS funcionam normalmente; INICIAR fica bloqueado.");
  }

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
