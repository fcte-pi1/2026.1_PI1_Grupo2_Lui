#include <Arduino.h>
#include <Wire.h>
#include <BluetoothSerial.h>

// =========================================================================
// DEFINIÇÕES E COMPONENTES (Tudo 100% contido neste arquivo)
// =========================================================================

// --- Pinos MPU6500 & I2C ---
#define SDA_PIN 21
#define SCL_PIN 22
#define MPU6500_ADDR 0x68
#define REG_PWR_MGMT_1   0x6B
#define REG_GYRO_ZOUT_H  0x47

// --- Pinos Motores (DRV8833) ---
#define PIN_MOT1_IN1 14 
#define PIN_MOT1_IN2 27 
#define PIN_MOT2_IN1 26 
#define PIN_MOT2_IN2 25 

// --- Configurações PWM Motores ---
#define MOTOR_PWM_FREQ       20000 
#define MOTOR_PWM_RESOLUTION 8     
#define CH_MOT1_IN1 0
#define CH_MOT1_IN2 1
#define CH_MOT2_IN1 2
#define CH_MOT2_IN2 3

// --- Pinos Encoders ---
#define PIN_ENC1_A 32
#define PIN_ENC1_B 33
#define PIN_ENC2_A 18
#define PIN_ENC2_B 19

// --- Constantes de Odometria ---
#define RODA_DIAMETRO_MM     32.0f
#define ENCODER_PPR          140
#define CELULA_MM            180.0f

// --- Globais ---
BluetoothSerial SerialBT;

float offsetGiroZ_dps = 0.0f;          
float anguloAcumulado_deg = 0.0f;      
unsigned long ultimaAtualizacao_us = 0;

// =========================================================================
// IMPLEMENTAÇÃO DE HARDWARE (MPU e MOTORES)
// =========================================================================

void escreverReg(uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(reg);
  Wire.write(valor);
  Wire.endTransmission();
}

float lerGiroZ_dps() {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(REG_GYRO_ZOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom((uint16_t)MPU6500_ADDR, (uint8_t)2);
  if (Wire.available() >= 2) {
    int16_t gzRaw = (Wire.read() << 8) | Wire.read();
    return gzRaw / 131.0f; 
  }
  return 0.0f;
}

void calibrarOffsetGiro() {
  SerialBT.println("[MPU] Calibrando... mantenha o robo parado.");
  delay(500);
  const int N_AMOSTRAS = 500;
  double soma_dps = 0.0;
  for (int i = 0; i < N_AMOSTRAS; i++) {
    soma_dps += lerGiroZ_dps();
    delay(3);
  }
  offsetGiroZ_dps = (float)(soma_dps / N_AMOSTRAS);
  SerialBT.println("[MPU] Calibracao concluida.");
}

void mpu_update() {
  float giroZ_dps = lerGiroZ_dps() - offsetGiroZ_dps;
  unsigned long agora_us = micros();
  if (ultimaAtualizacao_us == 0) {
    ultimaAtualizacao_us = agora_us;
    return;
  }
  float dt_s = (agora_us - ultimaAtualizacao_us) / 1000000.0f;
  ultimaAtualizacao_us = agora_us;
  anguloAcumulado_deg += giroZ_dps * dt_s;
}

float mpu_get_yaw() {
  return anguloAcumulado_deg;
}

void mpu_init() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  escreverReg(REG_PWR_MGMT_1, 0x00); 
  delay(100);
  calibrarOffsetGiro();
  ultimaAtualizacao_us = micros();
  anguloAcumulado_deg = 0.0f;
}

// =========================================================================
// IMPLEMENTAÇÃO DE HARDWARE (ENCODERS)
// =========================================================================

static volatile long enc1_count = 0;
static volatile long enc2_count = 0;

static void IRAM_ATTR isr_enc1_a() {
    bool a = digitalRead(PIN_ENC1_A);
    bool b = digitalRead(PIN_ENC1_B);
    if (a == b) { enc1_count++; } else { enc1_count--; }
}

static void IRAM_ATTR isr_enc2_a() {
    bool a = digitalRead(PIN_ENC2_A);
    bool b = digitalRead(PIN_ENC2_B);
    if (a == b) { enc2_count++; } else { enc2_count--; }
}

void encoders_init() {
    pinMode(PIN_ENC1_A, INPUT_PULLUP);
    pinMode(PIN_ENC1_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC1_A), isr_enc1_a, CHANGE);

    pinMode(PIN_ENC2_A, INPUT);
    pinMode(PIN_ENC2_B, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC2_A), isr_enc2_a, CHANGE);
}

long get_encoder_media() {
    noInterrupts();
    long e1 = enc1_count;
    long e2 = enc2_count;
    interrupts();
    return (e1 + e2) / 2;
}

void reset_encoders() {
    noInterrupts();
    enc1_count = 0;
    enc2_count = 0;
    interrupts();
}

void motores_init() {
  ledcSetup(CH_MOT1_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
  ledcAttachPin(PIN_MOT1_IN1, CH_MOT1_IN1);
  ledcSetup(CH_MOT1_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
  ledcAttachPin(PIN_MOT1_IN2, CH_MOT1_IN2);
  ledcSetup(CH_MOT2_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
  ledcAttachPin(PIN_MOT2_IN1, CH_MOT2_IN1);
  ledcSetup(CH_MOT2_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
  ledcAttachPin(PIN_MOT2_IN2, CH_MOT2_IN2);
  
  ledcWrite(CH_MOT1_IN1, 0);
  ledcWrite(CH_MOT1_IN2, 0);
  ledcWrite(CH_MOT2_IN1, 0);
  ledcWrite(CH_MOT2_IN2, 0);
}

void motorSet(uint8_t chFwd, uint8_t chBwd, int velocidade_percentual) {
  velocidade_percentual = constrain(velocidade_percentual, -100, 100);
  int pwm = map(abs(velocidade_percentual), 0, 100, 0, 255);
  
  if (velocidade_percentual > 0) {
    ledcWrite(chFwd, pwm);
    ledcWrite(chBwd, 0);
  } else if (velocidade_percentual < 0) {
    ledcWrite(chFwd, 0);
    ledcWrite(chBwd, pwm);
  } else {
    ledcWrite(chFwd, 0);
    ledcWrite(chBwd, 0);
  }
}

void motor_esquerdo_set(int velocidade) {
  // Invertido via software devido a polaridade dos fios
  motorSet(CH_MOT1_IN1, CH_MOT1_IN2, -velocidade);
}

void motor_direito_set(int velocidade) {
  // Invertido via software devido a polaridade dos fios
  motorSet(CH_MOT2_IN1, CH_MOT2_IN2, -velocidade);
}

// =========================================================================
// LÓGICA DE CONTROLE DE MOVIMENTO (INSPIRADA NO PID DO GRUPO 1)
// =========================================================================

float kp_giro = 3.5;
float ki_giro = 0.01;
float kd_giro = 0.5;

float kp_reta = 2.0;

const int VEL_BASE_FRENTE = 100;
const int VEL_BASE_RE = -100;

void girar(float graus_alvo) {
    SerialBT.printf("Iniciando giro de %.1f graus...\n", graus_alvo);
    
    float erro_anterior = 0;
    float integral = 0;
    
    float yaw_inicial = mpu_get_yaw();
    float yaw_alvo = yaw_inicial + graus_alvo;
    
    unsigned long inicio_giro = millis();
    while (true) {
        mpu_update();
        float yaw_atual = mpu_get_yaw();
        
        float erro = yaw_alvo - yaw_atual;
        
        // Parada suave (menor que 2 graus)
        if (abs(erro) < 2.0) {
            motor_esquerdo_set(0);
            motor_direito_set(0);
            SerialBT.println("Giro concluido!");
            break;
        }
        
        // Timeout de seguranca
        if (millis() - inicio_giro > 3000) {
            motor_esquerdo_set(0);
            motor_direito_set(0);
            SerialBT.println("Timeout no giro!");
            break;
        }
        
        // PID
        integral += erro;
        float derivativo = erro - erro_anterior;
        float correcao = (kp_giro * erro) + (ki_giro * integral) + (kd_giro * derivativo);
        erro_anterior = erro;
        
        if (correcao > 150) correcao = 150;
        if (correcao < -150) correcao = -150;
        
        if (correcao > 0 && correcao < 40) correcao = 40;
        if (correcao < 0 && correcao > -40) correcao = -40;
        
        motor_esquerdo_set(-correcao);
        motor_direito_set(correcao);
        
        delay(10);
    }
}

void andar_distancia_mm(int velocidade_base, float distancia_mm) {
    SerialBT.printf("Andando %.1f mm (vel: %d)...\n", distancia_mm, velocidade_base);
    
    reset_encoders();
    float yaw_alvo = mpu_get_yaw();
    
    // Calcula alvo em pulsos
    float circunferencia = RODA_DIAMETRO_MM * 3.14159f;
    float pulsos_por_mm = ENCODER_PPR / circunferencia;
    long pulsos_alvo = abs(distancia_mm * pulsos_por_mm);
    
    unsigned long inicio = millis();
    while (true) {
        long pulsos_atuais = abs(get_encoder_media());
        if (pulsos_atuais >= pulsos_alvo) {
            break;
        }
        
        // Timeout de seguranca (5 seg)
        if (millis() - inicio > 5000) {
            SerialBT.println("Timeout no movimento!");
            break;
        }

        mpu_update();
        float yaw_atual = mpu_get_yaw();
        float erro = yaw_alvo - yaw_atual;
        
        float correcao = kp_reta * erro;
        
        int vel_esq = velocidade_base - correcao;
        int vel_dir = velocidade_base + correcao;
        
        motor_esquerdo_set(vel_esq);
        motor_direito_set(vel_dir);
        
        delay(10);
    }
    
    motor_esquerdo_set(0);
    motor_direito_set(0);
    SerialBT.println("Movimento concluido!");
}

void navAndarUmaCelula(bool re = false) {
    int vel = re ? VEL_BASE_RE : VEL_BASE_FRENTE;
    andar_distancia_mm(vel, CELULA_MM);
}

// =========================================================================
// SETUP E LOOP PRINCIPAL
// =========================================================================

void mostrarMenu() {
    SerialBT.println("=========================================");
    SerialBT.println("SISTEMA HIBRIDO PRONTO (Odometria + MPU)");
    SerialBT.println("Comandos Bluetooth:");
    SerialBT.println(" 'w' - Andar frente (1 celula - 180mm)");
    SerialBT.println(" 's' - Andar tras (1 celula - 180mm)");
    SerialBT.println(" 'a' - Girar esquerda");
    SerialBT.println(" 'd' - Girar direita");
    SerialBT.println(" 'q' - Parar motores");
    SerialBT.println(" 'h' - Mostrar este menu");
    SerialBT.println("=========================================");
}

void setup() {
    Serial.begin(115200);
    SerialBT.begin("MICROMOUSE_G2_HIBRIDO"); 
    
    Serial.println("Iniciando componentes...");
    
    motores_init();
    mpu_init();
    encoders_init();
    
    mostrarMenu();
}

void loop() {
    mpu_update();
    
    if (SerialBT.available()) {
        char cmd = SerialBT.read();
        
        switch (cmd) {
            case 'w':
                navAndarUmaCelula(false);
                break;
            case 's':
                navAndarUmaCelula(true); 
                break;
            case 'a':
                girar(90.0); 
                break;
            case 'd':
                girar(-90.0); 
                break;
            case 'q':
                motor_esquerdo_set(0);
                motor_direito_set(0);
                SerialBT.println("PARADA DE EMERGENCIA");
                break;
            case 'h':
                mostrarMenu();
                break;
        }
    }
    
    delay(10);
}
