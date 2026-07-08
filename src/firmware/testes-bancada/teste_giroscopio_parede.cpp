// ==========================================
// TESTE GIROSCOPIO PAREDE (GYRO + TOF HIBRIDO)
// Usando o teste_geral.cpp como base e adicionando
// a logica de navegacao hibrida (Sensor Fusion).
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

// --- Variáveis Globais ---
// --- Bluetooth ---
BluetoothSerial SerialBT;
String comandoBT = "";
String comandoSerial = "";

// --- Multithread Print (Core 0) ---
TaskHandle_t TaskPrintHandle;
volatile bool devePrintar = false;
volatile float p_anguloZ = 0.0f;
volatile float p_distEsq = 0.0f;
volatile float p_distDir = 0.0f;
volatile int p_pwmEsq = 0;
volatile int p_pwmDir = 0;
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

void taskPrintTelemetry(void * pvParameters) {
  for(;;) {
    if (devePrintar) {
      char buf[120];
      sprintf(buf, "YAW: %5.2f | Esq: %4.1f cm | Dir: %4.1f cm | PWM_E: %4d | PWM_D: %4d", 
              p_anguloZ, p_distEsq, p_distDir, p_pwmEsq, p_pwmDir);
      logMsg(String(buf));
      devePrintar = false;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Libera o Core 0 por 10ms
  }
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

// --- Variáveis de Integração do Yaw ---
float anguloZ = 0.0f;
unsigned long ultimoTempoGiro = 0;

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

void atualizarTOFsSilencioso() {
  int r1 = tof1Ok ? sensor1.readRangeContinuousMillimeters() - 23 : -1;
  int r2 = tof2Ok ? sensor2.readRangeContinuousMillimeters() - 45 : -1;
  int r3 = tof3Ok ? sensor3.readRangeContinuousMillimeters() - 37 : -1;

  if (r1 != -1 && r1 < 1200) filtroS1 = (filtroS1 == 0) ? r1 : (alphaTOF * r1) + ((1.0 - alphaTOF) * filtroS1);
  if (r2 != -1 && r2 < 1200) filtroS2 = (filtroS2 == 0) ? r2 : (alphaTOF * r2) + ((1.0 - alphaTOF) * filtroS2);
  if (r3 != -1 && r3 < 1200) filtroS3 = (filtroS3 == 0) ? r3 : (alphaTOF * r3) + ((1.0 - alphaTOF) * filtroS3);
}

// ==============================================================
// LÓGICA PRINCIPAL - TESTE PARADO (GYRO + TOF)
// ==============================================================
void manterCentralizadoParado() {
    float KP_GIRO = 3.5f;       
    float KP_PAREDE = 10.0f;     // Ganho mais agressivo para vencer atrito estático
    
    float CLEARANCE_MINIMO_CM = 3.0f; // So corrige se a distancia for MENOR que 3 cm
    
    anguloZ = 0.0f; 
    ultimoTempoGiro = millis();
    unsigned long ultimoUpdate = millis();
    unsigned long ultimoPrint = millis();
    
    logMsg("Iniciando teste PARADO (Gyro + TOF). Envie qualquer tecla para SAIR.");
    
    // Limpa a serial antes de entrar no loop
    while (Serial.available()) Serial.read();
    while (SerialBT.available()) SerialBT.read();
    
    // Fica no loop até o usuário enviar algo
    while (Serial.available() == 0 && SerialBT.available() == 0) {
           
        unsigned long agora = millis();
        
        // Roda PID a cada 20ms (50Hz)
        if (agora - ultimoUpdate >= 20) {
            ultimoUpdate = agora;
            
            // --- A. GESTAO DO GIROSCOPIO (Angulo) ---
            atualizarGiroscopio();
            float erroGiro = 0.0f - anguloZ; 
            float correcaoGiro = erroGiro * KP_GIRO;
            
            // --- B. GESTAO DE PAREDES (Centralizacao TOF) ---
            atualizarTOFsSilencioso();
            float distEsq = filtroS1 / 10.0f; // mm para cm
            float distDir = filtroS3 / 10.0f;
            
            float erroParede = 0.0f;
            bool esqPerigo = distEsq > 0 && distEsq < CLEARANCE_MINIMO_CM;
            bool dirPerigo = distDir > 0 && distDir < CLEARANCE_MINIMO_CM;
            
            if (esqPerigo && dirPerigo) {
                // Caso extremo (muito apertado), centraliza pela diferenca
                erroParede = distEsq - distDir;
            } else if (esqPerigo) {
                // Se invadir o limite de 3cm na esquerda, gera erro de repulsao
                erroParede = (distEsq - CLEARANCE_MINIMO_CM) * 2.0f;
            } else if (dirPerigo) {
                // Se invadir o limite de 3cm na direita, gera erro de repulsao
                erroParede = (CLEARANCE_MINIMO_CM - distDir) * 2.0f;
            } else {
                // Mais do que 3cm das paredes: NENHUMA correcao (segue reto so no Gyro)
                erroParede = 0.0f;
            }
            
            float correcaoParede = erroParede * KP_PAREDE;
            if (correcaoParede > 150.0f) correcaoParede = 150.0f;
            if (correcaoParede < -150.0f) correcaoParede = -150.0f;
            
            // --- C. COMBINANDO ---
            // Como esta PARADO (PWM Base = 0), a correcao vai diretamente girar o robo.
            // erroParede > 0 (mais perto da dir, precisa ir pra esq): freia esq, acelera dir
            int pwmEsq = 0 - (int)correcaoGiro - (int)correcaoParede;
            int pwmDir = 0 + (int)correcaoGiro + (int)correcaoParede;
            
            // Permite rodas girarem para tras para o robo rotacionar no proprio eixo
            pwmEsq = constrain(pwmEsq, -255, 255);
            pwmDir = constrain(pwmDir, -255, 255);
            
            // Zona morta para o motor nao ficar apitando com pwm mto baixo (opcional)
            if (abs(pwmEsq) < 30) pwmEsq = 0;
            if (abs(pwmDir) < 30) pwmDir = 0;
            
            motor_esquerdo_set(pwmEsq);
            motor_direito_set(pwmDir);
            // --- PRINT DAS LEITURAS EM MULTITHREAD A CADA 250ms ---
            if (agora - ultimoPrint >= 250) {
                ultimoPrint = agora;
                // Passa os valores para a Task no Core 0
                p_anguloZ = anguloZ;
                p_distEsq = distEsq;
                p_distDir = distDir;
                p_pwmEsq = pwmEsq;
                p_pwmDir = pwmDir;
                devePrintar = true; // Aciona a impressao assincrona
            }
        }
        yield();
    }
    
    motors_stop_all(); 
    logMsg("Teste interrompido. Angulo residual: " + String(anguloZ) + " graus");
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

  anguloZ = 0.0f;
  ultimoTempoGiro = millis();
}

// --- Menu ---
void mostrarMenu() {
  logMsg("\n===== TESTE GYRO + TOF (PARADO) =====");
  logMsg("TESTE    -> Inicia correcao estatica infinita (Aperte qq tecla p/ sair)");
  logMsg("INFO     -> Mostra Leitura (Angulo e Parede)");
  logMsg("CALIBRAR -> Zera Gyro");
  logMsg("STOP     -> Para motores imediatamente");
  logMsg("HELP     -> Mostra este menu");
  logMsg("============================================");
}

void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;
  logMsg(">> Comando: " + cmd);

  if (cmd == "TESTE" || cmd == "TESTE_P") {
    manterCentralizadoParado();
  } else if (cmd == "INFO") {
    atualizarGiroscopio();
    atualizarTOFsSilencioso();
    float e = filtroS1 / 10.0f;
    float d = filtroS3 / 10.0f;
    char buf[100];
    sprintf(buf, "YAW: %.2f | TOF Esq: %.1f cm | TOF Dir: %.1f cm", anguloZ, e, d);
    logMsg(String(buf));
  } else if (cmd == "CALIBRAR") {
    calibrarMPU();
  } else if (cmd == "STOP") {
    motors_stop_all();
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
  
  // --- Init Multithread Print Task no Core 0 ---
  // O loop principal do Arduino roda no Core 1. Ao jogar o print pro Core 0,
  // evitamos qualquer lag de envio do Bluetooth de afetar o PID.
  xTaskCreatePinnedToCore(
      taskPrintTelemetry,   /* Funcao da task */
      "TaskPrint",          /* Nome */
      4096,                 /* Tamanho da stack */
      NULL,                 /* Parametros */
      1,                    /* Prioridade (1 = baixa) */
      &TaskPrintHandle,     /* Handle */
      0);                   /* Nucleo (Core 0 = PRO_CPU, lida com WiFi/BT) */
  
  logMsg("Inicializacao concluida.");
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
