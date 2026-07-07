// ==========================================
// TESTE ISOLADO: MOTORES (PONTE H DRV8833)
// Com Bluetooth Serial (comandos via USB ou celular)
// ==========================================
#include <Arduino.h>
#include "BluetoothSerial.h"

// Pinos da Ponte H DRV8833
#define M1_IN1 14
#define M1_IN2 27
#define M2_IN1 26
#define M2_IN2 25

BluetoothSerial SerialBT;

String comandoSerial = "";
String comandoBT = "";

// Saida simultanea Serial + Bluetooth
void logMsg(String msg) {
  Serial.println(msg);
  SerialBT.println(msg);
}

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
void motor1Frente() { digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW); }
void motor1Tras()   { digitalWrite(M1_IN1, LOW);  digitalWrite(M1_IN2, HIGH); }
void motor1Parar()  { digitalWrite(M1_IN1, LOW);  digitalWrite(M1_IN2, LOW); }
void motor2Frente() { digitalWrite(M2_IN1, HIGH); digitalWrite(M2_IN2, LOW); }
void motor2Tras()   { digitalWrite(M2_IN1, LOW);  digitalWrite(M2_IN2, HIGH); }
void motor2Parar()  { digitalWrite(M2_IN1, LOW);  digitalWrite(M2_IN2, LOW); }

// Sequencia completa de teste
void testarMotores() {
  logMsg("\n=== TESTE DE MOTORES ===");
  logMsg(">> Ambos para FRENTE (2s)");
  motoresFrente(); delay(2000); pararMotores(); delay(500);
  logMsg(">> Ambos para TRAS (2s)");
  motoresTras(); delay(2000); pararMotores(); delay(500);
  logMsg(">> Motor 1 (esquerdo) FRENTE isolado (1.5s)");
  motor1Frente(); delay(1500); motor1Parar(); delay(500);
  logMsg(">> Motor 1 (esquerdo) TRAS isolado (1.5s)");
  motor1Tras(); delay(1500); motor1Parar(); delay(500);
  logMsg(">> Motor 2 (direito) FRENTE isolado (1.5s)");
  motor2Frente(); delay(1500); motor2Parar(); delay(500);
  logMsg(">> Motor 2 (direito) TRAS isolado (1.5s)");
  motor2Tras(); delay(1500); motor2Parar(); delay(500);
  pararMotores();
  logMsg("=== FIM DO TESTE DE MOTORES ===\n");
}

void mostrarMenu() {
  logMsg("\n========= MENU (MOTORES) =========");
  logMsg("1 ou FRENTE -> Ambos para frente (2s)");
  logMsg("2 ou TRAS   -> Ambos para tras (2s)");
  logMsg("3 ou PARAR  -> Para os motores");
  logMsg("M1F / M1R   -> Motor 1 frente/re isolado");
  logMsg("M2F / M2R   -> Motor 2 frente/re isolado");
  logMsg("MOTOR       -> Sequencia completa de teste");
  logMsg("HELP        -> Mostra este menu");
  logMsg("==================================\n");
}

void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;
  logMsg("Comando recebido: " + cmd);

  if (cmd == "1" || cmd == "FRENTE") {
    motoresFrente(); delay(2000); pararMotores();
  } else if (cmd == "2" || cmd == "TRAS") {
    motoresTras(); delay(2000); pararMotores();
  } else if (cmd == "3" || cmd == "PARAR") {
    pararMotores();
  } else if (cmd == "M1F") {
    motor1Frente(); delay(1500); motor1Parar();
  } else if (cmd == "M1R") {
    motor1Tras(); delay(1500); motor1Parar();
  } else if (cmd == "M2F") {
    motor2Frente(); delay(1500); motor2Parar();
  } else if (cmd == "M2R") {
    motor2Tras(); delay(1500); motor2Parar();
  } else if (cmd == "MOTOR" || cmd == "MOTORES") {
    testarMotores();
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
  Serial.println("  TESTE DRV8833 (MOTORES) + BLUETOOTH ");
  Serial.println("======================================");

  if (!SerialBT.begin("ESP32_Teste_Motores")) {
    Serial.println("ERRO: Bluetooth nao inicializou.");
  } else {
    Serial.println("Bluetooth Ativo! Procure por 'ESP32_Teste_Motores' no celular.");
  }

  pinMode(M1_IN1, OUTPUT); pinMode(M1_IN2, OUTPUT);
  pinMode(M2_IN1, OUTPUT); pinMode(M2_IN2, OUTPUT);
  pararMotores();
  logMsg("Drivers de motor inicializados e parados.");
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
}