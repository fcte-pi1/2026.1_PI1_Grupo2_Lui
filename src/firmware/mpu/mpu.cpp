#include "mpu.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// ── Estado da integração do ângulo (eixo Z / yaw) ─────
static float offsetGiroZ_dps = 0.0f;      // bias do giroscópio, em graus/s
static float anguloAcumulado_deg = 0.0f;  // ângulo integrado, em graus
static unsigned long ultimaAtualizacao_us = 0;

void configurarMPU() {

  // Inicializa o MPU-6500 no endereço padrão 0x68 e atrela ao barramento Wire já existente
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("Falha ao encontrar o chip MPU-6500! Verifique as conexões.");
    while (1) {
      delay(10); // Trava o sistema caso o sensor falhe
    }
  }
  
  Serial.println("MPU-6500 inicializado com sucesso!\n");

  // Configuração das escalas (Ajustadas para dinâmica de robôs de solo)
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G); // Escala de até 4G de aceleração
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);      // Escala de até 500 graus/s de rotação
  
  // Filtro passa-baixa para remover ruído de vibração dos motores
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); 
  
  delay(100); // Pequeno tempo para o sensor estabilizar
}

void lerExibirMPU() {

  // Objetos para armazenar os eventos do sensor
  sensors_event_t a, g, temp;

  /* 
  
  a - guarda as forças lineares:

    a.acceleration.x = força linear no eixo X (m/s^2) 
    a.acceleration.y = força linear no eixo Y (m/s^2)
    a.acceleration.z = força linear no eixo Z (m/s^2)

  */

  /* 
  
  g - guarda a velocidade angular (giro) em rad/s:

    g.gyro.x = velocidade angular no eixo X (rad/s) 
    g.gyro.y = velocidade angular no eixo Y (rad/s)
    g.gyro.z = velocidade angular no eixo Z (rad/s)

    multiplicando por 57.2958 convertemos de rad/s para graus/s

  */

  // temp - temperatura do sensor 
  
  // Captura os dados mais recentes
  mpu.getEvent(&a, &g, &temp);

  // Converte a rotação Z de rad/s para graus/s
  float giroZ_graus = g.gyro.z * 57.2958; // podemos tirar isso caso seja necessário

  // Exibe a Aceleração X e Y (em m/s^2)
  Serial.print("Acel X: ");
  Serial.print(a.acceleration.x);
  Serial.print(" m/s^2 \t|\t Y: ");
  Serial.print(a.acceleration.y);
  
  // Exibe a Rotação (Velocidade Angular) no Eixo Z
  Serial.print(" m/s^2 \t|\t Giro Z (Yaw): ");
  Serial.print(giroZ_graus);
  Serial.println(" deg/s");
}

// ── Integração do Giroscópio Z (Yaw) ─────────────────

// Estado interno para integração do yaw
static float yaw_acumulado = 0.0f;
static float gyro_z_offset = 0.0f;  // Offset de calibração (bias do sensor)
static unsigned long ultimo_tempo_mpu = 0;
static bool mpu_calibrado = false;

// Zona morta: ignora leituras pequenas que são ruído do sensor
static constexpr float GYRO_DEAD_ZONE = 0.5f;  // graus/s

// Número de amostras para calibração do offset
static constexpr int CALIBRACAO_AMOSTRAS = 200;

void atualizarMPU() {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Calibração inicial: calcula o offset médio do giroscópio Z
    // O robô DEVE estar parado durante o boot para isso funcionar
    if (!mpu_calibrado) {
        static float soma_offset = 0.0f;
        static int contagem_offset = 0;

        soma_offset += g.gyro.z * 57.2958f;  // rad/s → graus/s
        contagem_offset++;

        if (contagem_offset >= CALIBRACAO_AMOSTRAS) {
            gyro_z_offset = soma_offset / (float)CALIBRACAO_AMOSTRAS;
            mpu_calibrado = true;
            ultimo_tempo_mpu = micros();
            Serial.printf("[MPU] Calibração concluída. Offset Z: %.3f deg/s\n", gyro_z_offset);
        }
        return;
    }

    unsigned long agora = micros();
    float dt = (float)(agora - ultimo_tempo_mpu) / 1000000.0f;  // Δt em segundos
    ultimo_tempo_mpu = agora;

    // Proteção contra dt absurdo (ex.: primeira chamada ou overflow)
    if (dt <= 0.0f || dt > 0.5f) return;

    // Converte para graus/s e remove o offset de calibração
    float giro_z = (g.gyro.z * 57.2958f) - gyro_z_offset;

    // Aplica zona morta para filtrar ruído quando parado
    if (fabs(giro_z) < GYRO_DEAD_ZONE) {
        giro_z = 0.0f;
    }

    // Integração trapezoidal: yaw += giro_z × dt
    yaw_acumulado += giro_z * dt;
}

float obterYaw() {
    return yaw_acumulado;
}

void resetarYaw() {
    yaw_acumulado = 0.0f;
}

// ── Calibração do offset (bias) do giroscópio em Z ────
// MEMS de giroscópio sempre tem um pequeno bias mesmo parado; sem descontar
// esse valor, a integração ao longo de um giro acumula erro (drift).
void mpu_calibrar_offset_giro() {
  Serial.println("[MPU] Calibrando offset do giroscopio (Z)... mantenha o robo parado.");
  delay(500);

  sensors_event_t a, g, temp;
  const int N_AMOSTRAS = 500;
  double soma_dps = 0.0;

  for (int i = 0; i < N_AMOSTRAS; i++) {
    mpu.getEvent(&a, &g, &temp);
    soma_dps += g.gyro.z * 57.2958;
    delay(3);
  }

  offsetGiroZ_dps = (float)(soma_dps / N_AMOSTRAS);
  Serial.print("[MPU] Offset giro Z: ");
  Serial.print(offsetGiroZ_dps);
  Serial.println(" graus/s");
}

// ── Integração do ângulo (yaw) ────────────────────────
void mpu_atualizar_angulo() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  unsigned long agora_us = micros();
  if (ultimaAtualizacao_us == 0) {
    // Primeira chamada: so registra a referencia de tempo, sem integrar
    // (senao dt viria absurdamente grande e o angulo saltaria).
    ultimaAtualizacao_us = agora_us;
    return;
  }

  float dt_s = (agora_us - ultimaAtualizacao_us) / 1000000.0f;
  ultimaAtualizacao_us = agora_us;

  float giroZ_dps = (g.gyro.z * 57.2958f) - offsetGiroZ_dps;
  anguloAcumulado_deg += giroZ_dps * dt_s;
}

void mpu_zerar_angulo() {
  anguloAcumulado_deg = 0.0f;
  ultimaAtualizacao_us = micros();
}

float mpu_get_angulo() {
  return anguloAcumulado_deg;
}