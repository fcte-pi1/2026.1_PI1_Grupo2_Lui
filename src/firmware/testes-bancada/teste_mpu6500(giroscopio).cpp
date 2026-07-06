// Rodei no arduino ide, tem bluetooth serial, verificar se precisa de alguma lib adicional, mas deve funcionar com a lib bluetooth serial do arduino ide, ESP32-MPU6500 o nome do bluetooth, conectar no bluetooth e abrir o monitor serial do arduino ide, deve aparecer os dados do giroscopio e acelerometro, no meu pc o bluetooth é na porta COM5


#include <Arduino.h>
#include <Wire.h>
#include "BluetoothSerial.h"

#define SDA_PIN 21
#define SCL_PIN 22
#define MPU_INT_PIN 23

#define MPU6500_ADDR 0x68

// Registradores do MPU6500
#define REG_PWR_MGMT_1   0x6B
#define REG_INT_PIN_CFG  0x37
#define REG_INT_ENABLE   0x38
#define REG_ACCEL_XOUT_H 0x3B
#define REG_WHO_AM_I     0x75

BluetoothSerial SerialBT;

volatile bool dadosProntos = false;

// Função executada quando a interrupção física do pino 23 dispara
void IRAM_ATTR funcaoInterrupcao() {
  dadosProntos = true;
}

// Imprime nos dois canais: USB e Bluetooth
void printAmbos(const String &msg) {
  Serial.print(msg);
  if (SerialBT.hasClient()) {
    SerialBT.print(msg);
  }
}

void printlnAmbos(const String &msg) {
  printAmbos(msg + "\n");
}

// Escreve um byte em um registrador específico do MPU6500
void escreverReg(uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(reg);
  Wire.write(valor);
  Wire.endTransmission();
}

// Lê o ID do chip para garantir que estamos falando com o hardware correto
uint8_t lerWhoAmI() {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(REG_WHO_AM_I);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6500_ADDR, 1);
  if (Wire.available()) return Wire.read();
  return 0;
}

void setup() {
  Serial.begin(115200);

  // Inicializa o Serial Bluetooth (SPP - Bluetooth Clássico)
  // O dispositivo aparecerá com este nome na busca Bluetooth
  if (!SerialBT.begin("ESP32-MPU6500")) {
    Serial.println("Erro ao iniciar o Bluetooth!");
  } else {
    Serial.println("Bluetooth iniciado. Pareie com 'ESP32-MPU6500'.");
  }

  delay(1000);

  printlnAmbos("\n=========================================");
  printlnAmbos("  TESTE DO MPU6500 ISOLADO (VIA WIRE)    ");
  printlnAmbos("=========================================");

  // Inicializa barramento I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000); // 400kHz para leitura rápida do MPU

  // Verifica a identidade do chip (MPU6500 costuma retornar 0x70)
  uint8_t id = lerWhoAmI();
  printlnAmbos("Procurando chip... WHO_AM_I retornado: 0x" + String(id, HEX));

  if (id != 0x70) {
    printlnAmbos("Aviso: Resposta WHO_AM_I inesperada. Verifique as conexões!");
  } else {
    printlnAmbos("Chip MPU6500 identificado com sucesso.");
  }

  // 1. Tira o MPU6500 do modo Sleep (Acorda o sensor)
  escreverReg(REG_PWR_MGMT_1, 0x00);
  delay(100);

  // 2. Configura o comportamento elétrico do pino físico INT
  // Latch desativado (pulso curto), pino configurado como Ativo ALTO (RISING)
  escreverReg(REG_INT_PIN_CFG, 0x00);

  // 3. Ativa a interrupção por dados prontos (Data Ready)
  escreverReg(REG_INT_ENABLE, 0x01);

  // Configura o pino 23 da ESP32 como entrada comum
  pinMode(MPU_INT_PIN, INPUT);

  // Associa a função ao pino 23 na subida de sinal (LOW para HIGH)
  attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), funcaoInterrupcao, RISING);

  printlnAmbos("Interrupção ativada no pino 23. Aguardando dados...\n");
}

void loop() {
  // O loop só processa quando o hardware gera o pulso de interrupção
  if (dadosProntos) {
    dadosProntos = false; // Reseta o sinalizador

    // Solicita os 14 registradores seguidos (Acel X, Y, Z, Temperatura, Giro X, Y, Z)
    Wire.beginTransmission(MPU6500_ADDR);
    Wire.write(REG_ACCEL_XOUT_H);
    Wire.endTransmission(false);

    // Cada leitura de eixo ocupa 2 bytes (High e Low)
    Wire.requestFrom(MPU6500_ADDR, 14);

    if (Wire.available() >= 14) {
      int16_t ax = (Wire.read() << 8) | Wire.read();
      int16_t ay = (Wire.read() << 8) | Wire.read();
      int16_t az = (Wire.read() << 8) | Wire.read();

      int16_t tempRaw = (Wire.read() << 8) | Wire.read(); // Ignorando temp no print para limpar a tela

      int16_t gx = (Wire.read() << 8) | Wire.read();
      int16_t gy = (Wire.read() << 8) | Wire.read();
      int16_t gz = (Wire.read() << 8) | Wire.read();

      // Print dos dados brutos recebidos diretamente do conversor AD do MPU
      printlnAmbos("ACEL Raw -> X: " + String(ax) +
                   " | Y: " + String(ay) +
                   " | Z: " + String(az) +
                   "  ||  GIRO Raw -> X: " + String(gx) +
                   " | Y: " + String(gy) +
                   " | Z: " + String(gz));
    }
  }
}